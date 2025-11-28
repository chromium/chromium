// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import android.content.Context;
import android.os.Bundle;

import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;

/**
 * Exclusive Access Manager class and the Exclusive Access Context interface are used for
 * synchronization of Pointer Lock, Keyboard Lock and Fullscreen features. The main responsibilities
 * of EAM are to monitor which features are currently in use and when the features exit criteria are
 * met (e.g. ESC key is pressed). The synchronization is done in the context of a single Activity.
 * This class is the Java counterpart of ExclusiveAccessManagerAndroid responsible for combining the
 * different exclusive access modes (like fullscreen and pointer lock). The Java class is
 * responsible for native object creation and destruction.
 */
@NullMarked
public class ExclusiveAccessManager
        implements Destroyable, DesktopWindowStateManager.AppHeaderObserver {
    private static final String LATEST_FULLSCREEN_OPTIONS = "latest_fullscreen_options";
    private long mExclusiveAccessManagerAndroidNativePointer;
    private final FullscreenManager mFullscreenManager;
    private @MonotonicNonNull DesktopWindowStateManager mDesktopWindowStateManager;
    @Nullable private TabModelSelector mTabModelSelector;
    private final TabModelObserver mTabModelObserver;
    private final ObservableSupplierImpl<Boolean> mExclusiveAccessState =
            new ObservableSupplierImpl<>(false);
    private boolean mPendingFullscreen;
    @Nullable private FullscreenOptions mLatestFullscreenOptions;

    public ExclusiveAccessManager(
            FullscreenManager fullscreenManager,
            @Nullable DesktopWindowStateManager desktopWindowStateManager) {
        mFullscreenManager = fullscreenManager;
        if (desktopWindowStateManager != null) {
            mDesktopWindowStateManager = desktopWindowStateManager;
            mDesktopWindowStateManager.addObserver(this);
        }
        mFullscreenManager.setFullscreenManagerDelegate(
                new FullscreenManager.FullscreenManagerDelegate() {
                    @Override
                    public void onExitFullscreen(@Nullable Tab tab) {
                        if (mExclusiveAccessManagerAndroidNativePointer == 0) return;
                        mLatestFullscreenOptions = null;
                        if (tab == null) {
                            ExclusiveAccessManagerJni.get()
                                    .exitExclusiveAccess(
                                            mExclusiveAccessManagerAndroidNativePointer);
                        } else {
                            deactivateTab(tab);
                        }
                    }
                });
        mTabModelObserver =
                new TabModelObserver() {
                    @Override
                    public void willCloseTab(Tab tab, boolean didCloseAlone) {
                        if (mExclusiveAccessManagerAndroidNativePointer == 0) return;
                        mLatestFullscreenOptions = null;
                        ExclusiveAccessManagerJni.get()
                                .onTabClosing(
                                        mExclusiveAccessManagerAndroidNativePointer,
                                        tab.getWebContents());
                    }
                };
        mFullscreenManager
                .getPersistentFullscreenModeSupplier()
                .addObserver(
                        new Callback<Boolean>() {
                            @Override
                            public void onResult(Boolean result) {
                                // Exclusive Access Manager always follows the fullscreen state. We
                                // subscribe to the FS state supplier in case when the fullscreen is
                                // delayed. Thanks to that EAM stat supplier will update on all FS
                                // enter and exit events.
                                // Exiting fullscreen should unlock all other locks.
                                mExclusiveAccessState.set(result);
                            }
                        });
    }

    public void initialize(
            @Nullable TabModelSelector modelSelector,
            Context context,
            ActivityTabProvider activityTabProvider) {
        assert modelSelector != null;
        mTabModelSelector = modelSelector;
        mExclusiveAccessManagerAndroidNativePointer =
                ExclusiveAccessManagerJni.get()
                        .init(this, context, mFullscreenManager, activityTabProvider);

        for (TabModel model : modelSelector.getModels()) {
            model.addObserver(mTabModelObserver);
        }

        if (mPendingFullscreen) {
            Tab tab = mTabModelSelector.getCurrentTab();
            // There always should be an active tab and fullscreen options  available if
            // mPendingFullscreen is on
            assert tab != null;
            assert mLatestFullscreenOptions != null;
            final @Nullable WebContents webContents = tab.getWebContents();
            // Right now we only allow non native pages to go fullscreen via this API. This is
            // caused by the native side which always requires WebContents to be present. This
            // conditions is always true on a desktop platforms.
            if (webContents != null) {
                ExclusiveAccessManagerJni.get()
                        .forceActiveTab(mExclusiveAccessManagerAndroidNativePointer, tab);
                ExclusiveAccessManagerJni.get()
                        .enterFullscreenModeForTab(
                                mExclusiveAccessManagerAndroidNativePointer,
                                webContents.getMainFrame(),
                                mLatestFullscreenOptions.showNavigationBar,
                                mLatestFullscreenOptions.showStatusBar,
                                mLatestFullscreenOptions.displayId);
            }
            mPendingFullscreen = false;
        }
    }

    /**
     * This function should be called when the state of fullscreen is known but it cannot be entered
     * yet as native side is not initialized. The state will be used during EAM initialization.
     *
     * @param savedInstanceState fullscreen options which were used to enter full screen before
     *     Activity recreation
     */
    public void setFullscreenPendingState(@Nullable Bundle savedInstanceState) {
        if (savedInstanceState == null) {
            return;
        }
        FullscreenOptions options = savedInstanceState.getParcelable(LATEST_FULLSCREEN_OPTIONS);
        if (options == null) {
            return;
        }
        mPendingFullscreen = true;
        mLatestFullscreenOptions = options;
    }

    public ObservableSupplier<Boolean> getExclusiveAccessStateSupplier() {
        return mExclusiveAccessState;
    }

    private void deactivateTab(Tab tab) {
        if (mExclusiveAccessManagerAndroidNativePointer == 0) return;
        mLatestFullscreenOptions = null;
        ExclusiveAccessManagerJni.get()
                .onTabDeactivated(
                        mExclusiveAccessManagerAndroidNativePointer, tab.getWebContents());
        ExclusiveAccessManagerJni.get()
                .onTabDetachedFromView(
                        mExclusiveAccessManagerAndroidNativePointer, tab.getWebContents());
    }

    /**
     * EAM frontend for WebContentsDelegate to enter fullscreen
     *
     * @param renderFrameHost the render frame host requesting fullscreen
     * @param options should the bars be hidden
     */
    public void enterFullscreenModeForTab(
            RenderFrameHost renderFrameHost, FullscreenOptions options) {
        if (mExclusiveAccessManagerAndroidNativePointer == 0) return;
        if (mLatestFullscreenOptions != null && mLatestFullscreenOptions.equals(options)) return;
        mLatestFullscreenOptions = options;
        ExclusiveAccessManagerJni.get()
                .enterFullscreenModeForTab(
                        mExclusiveAccessManagerAndroidNativePointer,
                        renderFrameHost,
                        options.showNavigationBar,
                        options.showStatusBar,
                        options.displayId);
    }

    /**
     * EAM frontend for WebContentsDelegate to exit fullscreen
     *
     * @param webContents exit requester
     */
    public void exitFullscreenModeForTab(@Nullable WebContents webContents) {
        if (mExclusiveAccessManagerAndroidNativePointer == 0) return;
        mLatestFullscreenOptions = null;
        ExclusiveAccessManagerJni.get()
                .exitFullscreenModeForTab(mExclusiveAccessManagerAndroidNativePointer, webContents);
    }

    /** Force exit of all exclusive access controllers */
    public void exitExclusiveAccess() {
        if (mExclusiveAccessManagerAndroidNativePointer == 0) return;
        mLatestFullscreenOptions = null;
        ExclusiveAccessManagerJni.get()
                .exitExclusiveAccess(mExclusiveAccessManagerAndroidNativePointer);
        // Marking state in case of enabled locks without fullscreen
        mExclusiveAccessState.set(false);
    }

    /** Checks if any of the fullscreen, keyboard lock or pointer lock is on */
    public boolean hasExclusiveAccess() {
        if (mExclusiveAccessManagerAndroidNativePointer == 0) return false;

        return ExclusiveAccessManagerJni.get()
                .hasExclusiveAccess(mExclusiveAccessManagerAndroidNativePointer);
    }

    /**
     * EAM frontend for WebContentsDelegate to check if web contents is in fullscreen
     *
     * @param webContents the requester of check
     * @return is currently in fullscreen
     */
    public boolean isFullscreenForTabOrPending(@Nullable WebContents webContents) {
        // This check takes care after Renderers which wish to be in the fullscreen
        if (mPendingFullscreen) {
            return true;
        }
        if (webContents == null || mExclusiveAccessManagerAndroidNativePointer == 0) {
            return false;
        }
        return ExclusiveAccessManagerJni.get()
                .isFullscreenForTabOrPending(
                        mExclusiveAccessManagerAndroidNativePointer, webContents);
    }

    /**
     * EAM frontend for WebContentsDelegate to pre handle the key press
     *
     * @param nativeKeyEvent the native pointer to key event, not having temporary Java object due
     *     to performance
     * @return true if the key was handled
     */
    public boolean preHandleKeyboardEvent(long nativeKeyEvent) {
        if (mExclusiveAccessManagerAndroidNativePointer == 0) return false;

        boolean handled =
                ExclusiveAccessManagerJni.get()
                        .preHandleKeyboardEvent(
                                mExclusiveAccessManagerAndroidNativePointer, nativeKeyEvent);
        if (handled) {
            // Marking state when escape key was consumed in case of no changes in the fullscreen
            mExclusiveAccessState.set(false);
        }
        return handled;
    }

    /**
     * EAM frontend for WebContentsDelegate to request keyboard lock
     *
     * @param webContents the WebContents that requested keyboard load
     * @param escKeyLocked whether the escape key is to be locked.
     */
    public void requestKeyboardLock(WebContents webContents, boolean escKeyLocked) {
        if (mExclusiveAccessManagerAndroidNativePointer == 0) return;

        ExclusiveAccessManagerJni.get()
                .requestKeyboardLock(
                        mExclusiveAccessManagerAndroidNativePointer, webContents, escKeyLocked);
        // Checking internal state of the controllers in case lock was not set
        mExclusiveAccessState.set(hasExclusiveAccess());
    }

    /**
     * EAM frontend for WebContentsDelegate to cancel keyboard lock
     *
     * @param webContents the WebContents cancelling keyboard lock
     */
    public void cancelKeyboardLockRequest(WebContents webContents) {
        if (mExclusiveAccessManagerAndroidNativePointer == 0) return;

        ExclusiveAccessManagerJni.get()
                .cancelKeyboardLockRequest(
                        mExclusiveAccessManagerAndroidNativePointer, webContents);
        // Checking internal state of the controllers in case other locks are on
        mExclusiveAccessState.set(hasExclusiveAccess());
    }

    /**
     * EAM frontend for ActivityRecreationController to check if keyboard lock is active
     *
     * @return true if native side is initialized and keyboard lock is active
     */
    public boolean isKeyboardLocked() {
        if (mExclusiveAccessManagerAndroidNativePointer == 0) return false;
        return ExclusiveAccessManagerJni.get()
                .isKeyboardLocked(mExclusiveAccessManagerAndroidNativePointer);
    }

    /**
     * EAM frontend for WebContentsDelegate to request pointer lock
     *
     * @param webContents the WebContents that requested keyboard load
     * @param userGesture whether the lock was triggered by the user gesture
     * @param lastUnlockedByTarget whether the last lock was removed by site
     */
    public void requestPointerLock(
            WebContents webContents, boolean userGesture, boolean lastUnlockedByTarget) {
        if (mExclusiveAccessManagerAndroidNativePointer == 0) return;

        ExclusiveAccessManagerJni.get()
                .requestPointerLock(
                        mExclusiveAccessManagerAndroidNativePointer,
                        webContents,
                        userGesture,
                        lastUnlockedByTarget);
        // Checking internal state of the controllers in case lock was not set
        mExclusiveAccessState.set(hasExclusiveAccess());
    }

    /** EAM frontend for WebContentsDelegate to inform that pointer lock was lost */
    public void lostPointerLock() {
        if (mExclusiveAccessManagerAndroidNativePointer == 0) return;
        ExclusiveAccessManagerJni.get()
                .lostPointerLock(mExclusiveAccessManagerAndroidNativePointer);
        // Checking internal state of the controllers in case other locks are on
        mExclusiveAccessState.set(hasExclusiveAccess());
    }

    /**
     * EAM frontend for ActivityRecreationController to check if pointer lock is active
     *
     * @return true if native side is initialized and pointer lock is active
     */
    public boolean isPointerLocked() {
        if (mExclusiveAccessManagerAndroidNativePointer == 0) return false;
        return ExclusiveAccessManagerJni.get()
                .isPointerLocked(mExclusiveAccessManagerAndroidNativePointer);
    }

    /**
     * Saves Exclusive Access Manager state to bundle that will be used to restore the EAM state
     * after the activity is recreated. This is expected to be invoked in {@code
     * RootUICoordinator#onSaveInstanceState(Bundle)}.
     *
     * @param outState The {@link Bundle} that is used to save state information.
     */
    public void saveFullscreenState(@Nullable Bundle outState) {
        if (outState == null) return;
        if (mLatestFullscreenOptions == null) return;
        outState.putParcelable(LATEST_FULLSCREEN_OPTIONS, mLatestFullscreenOptions);
    }

    @Override
    public void onDesktopWindowingModeChanged(boolean isInDesktopWindow) {
        if (mExclusiveAccessManagerAndroidNativePointer == 0) return;

        if (isInDesktopWindow && mFullscreenManager.getPersistentFullscreenMode()) {
            mLatestFullscreenOptions = null;
            // Exiting when fullscreen should be lost
            ExclusiveAccessManagerJni.get()
                    .exitExclusiveAccess(mExclusiveAccessManagerAndroidNativePointer);
        }
    }

    /** Cleanup function which should be called when owning object is destroyed. */
    @Override
    public void destroy() {
        if (mExclusiveAccessManagerAndroidNativePointer != 0) {
            ExclusiveAccessManagerJni.get().destroy(mExclusiveAccessManagerAndroidNativePointer);
            mExclusiveAccessManagerAndroidNativePointer = 0;
        }
        if (mDesktopWindowStateManager != null) mDesktopWindowStateManager.removeObserver(this);
        if (mTabModelSelector != null) {
            for (TabModel model : mTabModelSelector.getModels()) {
                model.removeObserver(mTabModelObserver);
            }
        }
    }

    @NativeMethods
    public interface Natives {
        long init(
                ExclusiveAccessManager caller,
                Context context,
                FullscreenManager fullscreenManager,
                ActivityTabProvider activityTabProvider);

        void enterFullscreenModeForTab(
                long nativeExclusiveAccessManagerAndroid,
                RenderFrameHost renderFrameHost,
                boolean showNavigationBar,
                boolean showStatusBar,
                long displayId);

        void exitFullscreenModeForTab(
                long nativeExclusiveAccessManagerAndroid, @Nullable WebContents webContents);

        boolean isFullscreenForTabOrPending(
                long nativeExclusiveAccessManagerAndroid, WebContents webContents);

        boolean preHandleKeyboardEvent(
                long nativeExclusiveAccessManagerAndroid, long nativeKeyEvent);

        void requestKeyboardLock(
                long nativeExclusiveAccessManagerAndroid,
                WebContents webContents,
                boolean escKeyLocked);

        void cancelKeyboardLockRequest(
                long nativeExclusiveAccessManagerAndroid, WebContents webContents);

        boolean isKeyboardLocked(long nativeExclusiveAccessManagerAndroid);

        void requestPointerLock(
                long nativeExclusiveAccessManagerAndroid,
                WebContents webContents,
                boolean userGesture,
                boolean lastUnlockedByTarget);

        void lostPointerLock(long nativeExclusiveAccessManagerAndroid);

        boolean isPointerLocked(long nativeExclusiveAccessManagerAndroid);

        void exitExclusiveAccess(long nativeExclusiveAccessManagerAndroid);

        boolean hasExclusiveAccess(long nativeExclusiveAccessManagerAndroid);

        void onTabDeactivated(
                long nativeExclusiveAccessManagerAndroid, @Nullable WebContents webContents);

        void onTabDetachedFromView(
                long nativeExclusiveAccessManagerAndroid, @Nullable WebContents webContents);

        void onTabClosing(
                long nativeExclusiveAccessManagerAndroid, @Nullable WebContents webContents);

        void forceActiveTab(long nativeExclusiveAccessManagerAndroid, Tab webContents);

        void destroy(long nativeExclusiveAccessManagerAndroid);
    }
}
