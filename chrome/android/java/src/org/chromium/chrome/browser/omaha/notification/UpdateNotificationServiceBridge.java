// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha.notification;

import static org.chromium.chrome.browser.omaha.UpdateConfigs.getUpdateNotificationTextBody;
import static org.chromium.chrome.browser.omaha.UpdateConfigs.getUpdateNotificationTitle;
import static org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateState.INLINE_UPDATE_AVAILABLE;
import static org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateState.UPDATE_AVAILABLE;

import android.content.Intent;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateState;

/**
 * Class supports to build and to send update notification per certain duration if new Chrome
 * version is available. It listens to {@link UpdateStatusProvider}, and coordinates with the
 * backend notification scheduling system.
 */
@JNINamespace("updates")
public class UpdateNotificationServiceBridge implements UpdateNotificationController, Destroyable {
    private final Callback<UpdateStatusProvider.UpdateStatus> mObserver = status -> {
        mUpdateStatus = status;
        processUpdateStatus();
    };

    private ActivityLifecycleDispatcher mActivityLifecycle;
    private @Nullable UpdateStatusProvider.UpdateStatus mUpdateStatus;
    private static final String TAG = "cr_UpdateNotif";

    /**
     * @param lifecycleDispatcher Lifecycle of an Activity the notification will be shown in.
     */
    public UpdateNotificationServiceBridge(ActivityLifecycleDispatcher lifecycleDispatcher) {
        mActivityLifecycle = lifecycleDispatcher;
        UpdateStatusProvider.getInstance().addObserver(mObserver);
        mActivityLifecycle.register(this);
    }

    // UpdateNotificationController implementation.
    @Override
    public void onNewIntent(Intent intent) {
        processUpdateStatus();
    }

    // Destroyable implementation.
    @Override
    public void destroy() {
        UpdateStatusProvider.getInstance().removeObserver(mObserver);
        mActivityLifecycle.unregister(this);
        mActivityLifecycle = null;
    }

    private void processUpdateStatus() {
        if (mUpdateStatus == null) return;
        switch (mUpdateStatus.updateState) {
            case UPDATE_AVAILABLE: // Intentional fallthrough.
            case INLINE_UPDATE_AVAILABLE: // Intentional fallthrough.
                boolean shouldShowImmediately = mUpdateStatus.updateState == INLINE_UPDATE_AVAILABLE
                        || ChromeFeatureList.isEnabled(
                                ChromeFeatureList.UPDATE_NOTIFICATION_IMMEDIATE_SHOW_OPTION);
                UpdateNotificationServiceBridgeJni.get().schedule(getUpdateNotificationTitle(),
                        getUpdateNotificationTextBody(), mUpdateStatus.updateState,
                        shouldShowImmediately);

                break;
            default:
                break;
        }
    }

    @NativeMethods
    interface Natives {
        /**
         * Schedule a notification through scheduling system.
         * @param title The title string of notification context.
         * @param message The body string of notification context.
         * @param state An enum of {@link UpdateState} pulled from UpdateStatusProvider.
         * @param shouldShowImmediately A flag to show notification right away if it is true.
         */
        void schedule(String title, String message, @UpdateState int state,
                boolean shouldShowImmediately);
    }

    /**
     * Launches Chrome activity depends on {@link UpdateState}.
     * @param state An enum value of {@link UpdateState} stored in native side schedule service.
     * */
    @CalledByNative
    private static void launchChromeActivity(@UpdateState int state) {
        try {
            UpdateUtils.onUpdateAvailable(ContextUtils.getApplicationContext(), state);
        } catch (IllegalArgumentException e) {
            Log.e(TAG, "Failed to start activity in background.", e);
        }
    }
}
