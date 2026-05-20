// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.base.TimeUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.content_public.browser.WebContents;

/**
 * This class is the Java counterpart of ExclusiveAccessContextAndroid responsible for forwarding
 * WebContents calls to the system. The native class is responsible for Java object creation and
 * destruction.
 */
@NullMarked
public class ExclusiveAccessContext implements Destroyable {
    private static final String TAG = "ExclusiveAccessCtx";

    private final Context mContext;
    private final FullscreenManager mFullscreenManager;
    final ActivityTabProvider.ActivityTabTabObserver mActiveTabObserver;
    @Nullable private Tab mActiveTab;
    private long mNativeExclusiveAccessContextAndroid;
    private long mLastTouchTime;

    /**
     * Throttles touch events to avoid excessive JNI calls. This interval (1 min) is chosen to be
     * significantly smaller than the native snooze timer (15 mins). By notifying the native side
     * periodically during active use, we ensure the security notice stays 'snoozed' and does not
     * interrupt the user (e.g. while gaming). The notice only reappears if the user is inactive for
     * the full 15-minute snooze period and then returns.
     */
    private static final long TOUCH_EVENT_THROTTLE_MS = 60000; // 1 minute

    @CalledByNative
    public static ExclusiveAccessContext create(
            long nativeExclusiveAccessContextAndroid,
            Context context,
            FullscreenManager fullscreenManager,
            ActivityTabProvider activityTabProvider) {
        return new ExclusiveAccessContext(
                nativeExclusiveAccessContextAndroid,
                context,
                fullscreenManager,
                activityTabProvider);
    }

    public ExclusiveAccessContext(
            long nativeExclusiveAccessContextAndroid,
            Context context,
            FullscreenManager fullscreenManager,
            ActivityTabProvider activityTabProvider) {
        mNativeExclusiveAccessContextAndroid = nativeExclusiveAccessContextAndroid;
        mContext = context;
        mFullscreenManager = fullscreenManager;
        mActiveTabObserver =
                new ActivityTabProvider.ActivityTabTabObserver(
                        activityTabProvider, /* shouldTrigger= */ true) {
                    @Override
                    protected void onObservingDifferentTab(@Nullable Tab tab) {
                        if (mActiveTab == null || tab == null) {
                            Log.i(TAG, "onObservingDifferentTab is new tab null? " + (tab == null));
                        }

                        mActiveTab = tab;
                    }

                    @Override
                    public void onTouchDown() {
                        // Notify native of user input to reset the exclusive access bubble snooze
                        // timer. This mirrors desktop behavior where any interaction re-snoozes
                        // the security notice.
                        if (mNativeExclusiveAccessContextAndroid != 0) {
                            long currentTime = TimeUtils.uptimeMillis();
                            if (mLastTouchTime == 0
                                    || currentTime - mLastTouchTime >= TOUCH_EVENT_THROTTLE_MS) {
                                mLastTouchTime = currentTime;
                                ExclusiveAccessContextJni.get()
                                        .onExclusiveAccessUserInput(
                                                mNativeExclusiveAccessContextAndroid);
                            }
                        }
                    }
                };
    }

    Context getAppContext() {
        return mContext;
    }

    /**
     * @return The SnackbarManager for this context, or null if the context does not support
     *     snackbars (e.g. application context, or activity that doesn't implement
     *     SnackbarManageable).
     */
    public @Nullable SnackbarManager getSnackbarManager() {
        if (mContext instanceof SnackbarManager.SnackbarManageable manageable) {
            return manageable.getSnackbarManager();
        }
        return null;
    }

    @Override
    @CalledByNative
    public void destroy() {
        // Explicitly null the native pointer to prevent any stray callbacks from attempting to
        // use it after the native object has been destroyed.
        mNativeExclusiveAccessContextAndroid = 0;
        mActiveTabObserver.destroy();
    }

    @CalledByNative
    public @Nullable Profile getProfile() {
        if (mActiveTab == null) {
            Log.e(TAG, "mActiveTab is null in getProfile");
            return null;
        }

        return mActiveTab.getProfile();
    }

    @CalledByNative
    boolean isFullscreen() {
        return mFullscreenManager.getPersistentFullscreenMode();
    }

    @CalledByNative
    public void enterFullscreenModeForTab(
            long displayId, boolean prefersNavigationBar, boolean prefersStatusBar) {
        if (mActiveTab != null) {
            FullscreenOptions options =
                    new FullscreenOptions(prefersNavigationBar, prefersStatusBar, displayId);
            mFullscreenManager.onEnterFullscreen(mActiveTab, options);
        }
    }

    @CalledByNative
    public @Nullable WebContents getWebContentsForExclusiveAccess() {
        return mActiveTab != null ? mActiveTab.getWebContents() : null;
    }

    @CalledByNative
    public void exitFullscreenModeForTab() {
        if (mActiveTab != null) {
            mFullscreenManager.onExitFullscreen(mActiveTab);
        }
    }

    @CalledByNative
    public void forceActiveTab(Tab tab) {
        mActiveTab = tab;
    }

    @NativeMethods
    interface Natives {
        void onExclusiveAccessUserInput(long nativeExclusiveAccessContextAndroid);
    }
}
