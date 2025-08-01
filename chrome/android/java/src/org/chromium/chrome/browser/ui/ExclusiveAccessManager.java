// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import org.jni_zero.NativeMethods;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
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
public class ExclusiveAccessManager implements Destroyable {
    private long mExclusiveAccessManagerAndroidNativePointer;

    public ExclusiveAccessManager(
            FullscreenManager fullscreenManager, ActivityTabProvider activityTabProvider) {
        mExclusiveAccessManagerAndroidNativePointer =
                ExclusiveAccessManagerJni.get().init(this, fullscreenManager, activityTabProvider);
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
                        options.showStatusBar);
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

    /** Cleanup function which should be called when owning object is destroyed. */
    @Override
    public void destroy() {
        if (mExclusiveAccessManagerAndroidNativePointer != 0) {
            ExclusiveAccessManagerJni.get().destroy(mExclusiveAccessManagerAndroidNativePointer);
            mExclusiveAccessManagerAndroidNativePointer = 0;
        }
    }

    @NativeMethods
    public interface Natives {
        long init(
                ExclusiveAccessManager caller,
                FullscreenManager fullscreenManager,
                ActivityTabProvider activityTabProvider);

        void enterFullscreenModeForTab(
                long nativeExclusiveAccessManagerAndroid,
                long requestingFrame,
                boolean showNavigationBar,
                boolean showStatusBar);

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

        void destroy(long nativeExclusiveAccessManagerAndroid);
    }
}
