// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import android.app.Activity;

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
    private long mExclusiveAccessManagerAndroidNativePointer;
    private final FullscreenManager mFullscreenManager;
    private @MonotonicNonNull DesktopWindowStateManager mDesktopWindowStateManager;
    @Nullable private TabModelSelector mTabModelSelector;
    private final TabModelObserver mTabModelObserver;
    private final ObservableSupplierImpl<Boolean> mExclusiveAccessState =
            new ObservableSupplierImpl<>(false);

    public ExclusiveAccessManager(
            Activity activity,
            FullscreenManager fullscreenManager,
            ActivityTabProvider activityTabProvider,
            @Nullable DesktopWindowStateManager desktopWindowStateManager) {
        mFullscreenManager = fullscreenManager;
        mExclusiveAccessManagerAndroidNativePointer =
                ExclusiveAccessManagerJni.get()
                        .init(this, activity, fullscreenManager, activityTabProvider);
        if (desktopWindowStateManager != null) {
            mDesktopWindowStateManager = desktopWindowStateManager;
            mDesktopWindowStateManager.addObserver(this);
        }
        mFullscreenManager.setFullscreenManagerDelegate(
                new FullscreenManager.FullscreenManagerDelegate() {
                    @Override
                    public void onExitFullscreen(@Nullable Tab tab) {
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

    public void initialize(TabModelSelector modelSelector) {
        mTabModelSelector = modelSelector;
        for (TabModel model : modelSelector.getModels()) {
            model.addObserver(mTabModelObserver);
        }
    }

    public ObservableSupplier<Boolean> getExclusiveAccessStateSupplier() {
        return mExclusiveAccessState;
    }

    private void deactivateTab(Tab tab) {
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
        ExclusiveAccessManagerJni.get()
                .exitFullscreenModeForTab(mExclusiveAccessManagerAndroidNativePointer, webContents);
    }

    /** Force exit of all exclusive access controllers */
    public void exitExclusiveAccess() {
        ExclusiveAccessManagerJni.get()
                .exitExclusiveAccess(mExclusiveAccessManagerAndroidNativePointer);
        // Marking state in case of enabled locks without fullscreen
        mExclusiveAccessState.set(false);
    }

    /** Checks if any of the fullscreen, keyboard lock or pointer lock is on */
    public boolean hasExclusiveAccess() {
        return ExclusiveAccessManagerJni.get()
                .hasExclusiveAccess(mExclusiveAccessManagerAndroidNativePointer);
    }

    /**
     * EAM frontend for WebContentsDelegate to check if web contents is in fullscreen
     *
     * @param webContents the requester of check
     * @return is currently in fullscreen
     */
    public boolean isFullscreenForTabOrPending(WebContents webContents) {
        if (webContents == null) {
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
        ExclusiveAccessManagerJni.get()
                .cancelKeyboardLockRequest(
                        mExclusiveAccessManagerAndroidNativePointer, webContents);
        // Checking internal state of the controllers in case other locks are on
        mExclusiveAccessState.set(hasExclusiveAccess());
    }

    public void requestPointerLock(
            WebContents webContents, boolean userGesture, boolean lastUnlockedByTarget) {
        ExclusiveAccessManagerJni.get()
                .requestPointerLock(
                        mExclusiveAccessManagerAndroidNativePointer,
                        webContents,
                        userGesture,
                        lastUnlockedByTarget);
        // Checking internal state of the controllers in case lock was not set
        mExclusiveAccessState.set(hasExclusiveAccess());
    }

    public void lostPointerLock() {
        ExclusiveAccessManagerJni.get()
                .lostPointerLock(mExclusiveAccessManagerAndroidNativePointer);
        // Checking internal state of the controllers in case other locks are on
        mExclusiveAccessState.set(hasExclusiveAccess());
    }

    @Override
    public void onDesktopWindowingModeChanged(boolean isInDesktopWindow) {
        if (isInDesktopWindow && mFullscreenManager.getPersistentFullscreenMode()) {
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
                Activity activity,
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

        void requestPointerLock(
                long nativeExclusiveAccessManagerAndroid,
                WebContents webContents,
                boolean userGesture,
                boolean lastUnlockedByTarget);

        void lostPointerLock(long nativeExclusiveAccessManagerAndroid);

        void exitExclusiveAccess(long nativeExclusiveAccessManagerAndroid);

        boolean hasExclusiveAccess(long nativeExclusiveAccessManagerAndroid);

        void onTabDeactivated(
                long nativeExclusiveAccessManagerAndroid, @Nullable WebContents webContents);

        void onTabDetachedFromView(
                long nativeExclusiveAccessManagerAndroid, @Nullable WebContents webContents);

        void onTabClosing(
                long nativeExclusiveAccessManagerAndroid, @Nullable WebContents webContents);

        void destroy(long nativeExclusiveAccessManagerAndroid);
    }
}
