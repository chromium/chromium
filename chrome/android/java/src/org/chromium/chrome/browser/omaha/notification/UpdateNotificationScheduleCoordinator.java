// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha.notification;

import android.content.Intent;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider;

/**
 * Class supports to build and to send update notification per certain duration if new Chrome
 * version is available. It listens to {@link UpdateStatusProvider}, and coordinates with the
 * backend notification scheduling system.
 */
public class UpdateNotificationScheduleCoordinator
        implements UpdateNotificationController, Destroyable {
    private final Callback<UpdateStatusProvider.UpdateStatus> mObserver = status -> {
        mUpdateStatus = status;
        // TODO(hesen): process UpdateStatus.
    };

    private ChromeActivity mActivity;
    private @Nullable UpdateStatusProvider.UpdateStatus mUpdateStatus;

    /**
     * @param activity A {@link ChromeActivity} instance the notification will be shown in.
     */
    public UpdateNotificationScheduleCoordinator(ChromeActivity activity) {
        mActivity = activity;
        UpdateStatusProvider.getInstance().addObserver(mObserver);
        mActivity.getLifecycleDispatcher().register(this);
    }

    // UpdateNotificationController implementation.
    @Override
    public void onNewIntent(Intent intent) {
        // TODO(hesen): process UpdateStatus.
        return;
    }

    // Destroyable implementation.
    @Override
    public void destroy() {
        UpdateStatusProvider.getInstance().removeObserver(mObserver);
        mActivity.getLifecycleDispatcher().unregister(this);
        mActivity = null;
    }
}
