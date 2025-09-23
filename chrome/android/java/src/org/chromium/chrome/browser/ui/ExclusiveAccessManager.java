// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import android.app.Activity;

import org.jni_zero.NativeMethods;

import org.chromium.base.lifetime.Destroyable;
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
    }

    public void initialize(TabModelSelector modelSelector) {
        mTabModelSelector = modelSelector;
        for (TabModel model : modelSelector.getModels()) {
            model.addObserver(mTabModelObserver);
        }
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
     * @param requestingFrame the native pointer to the frame object
     * @param options should the bars be hidden
     */
    public void enterFullscreenModeForTab(long requestingFrame, FullscreenOptions options) {
        ExclusiveAccessManagerJni.get()
                .enterFullscreenModeForTab(
                        mExclusiveAccessManagerAndroidNativePointer,
                        requestingFrame,
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
        return ExclusiveAccessManagerJni.get()
                .preHandleKeyboardEvent(
                        mExclusiveAccessManagerAndroidNativePointer, nativeKeyEvent);
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
    }

    public void requestPointerLock(
            WebContents webContents, boolean userGesture, boolean lastUnlockedByTarget) {
        ExclusiveAccessManagerJni.get()
                .requestPointerLock(
                        mExclusiveAccessManagerAndroidNativePointer,
                        webContents,
                        userGesture,
                        lastUnlockedByTarget);
    }

    public void lostPointerLock() {
        ExclusiveAccessManagerJni.get()
                .lostPointerLock(mExclusiveAccessManagerAndroidNativePointer);
    }

    @Override
    public void onDesktopWindowingModeChanged(boolean isInDesktopWindow) {
        if (isInDesktopWindow && mFullscreenManager.getPersistentFullscreenMode()) {
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
                long requestingFrame,
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

        void onTabDeactivated(
                long nativeExclusiveAccessManagerAndroid, @Nullable WebContents webContents);

        void onTabDetachedFromView(
                long nativeExclusiveAccessManagerAndroid, @Nullable WebContents webContents);

        void onTabClosing(
                long nativeExclusiveAccessManagerAndroid, @Nullable WebContents webContents);

        void destroy(long nativeExclusiveAccessManagerAndroid);
    }
}
