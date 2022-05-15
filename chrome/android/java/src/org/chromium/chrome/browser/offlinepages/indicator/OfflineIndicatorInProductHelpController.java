// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.indicator;

import android.app.Activity;
import android.os.Handler;

import org.chromium.chrome.browser.download.OfflineContentAvailabilityStatusProvider;
import org.chromium.chrome.browser.status_indicator.StatusIndicatorCoordinator;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.user_education.UserEducationHelper;

/**
 * Class that controls the showing of in-product help for the offline indicator. The in-product help
 * is shown when the "show" animation finishes.
 */
public class OfflineIndicatorInProductHelpController
        implements StatusIndicatorCoordinator.StatusIndicatorObserver {
    private final Activity mActivity;
    private final ToolbarManager mToolbarManager;
    private final Handler mHandler = new Handler();
    private final UserEducationHelper mUserEducationHelper;
    private final StatusIndicatorCoordinator mCoordinator;

    public OfflineIndicatorInProductHelpController(final Activity activity,
            final ToolbarManager toolbarManager,
            final StatusIndicatorCoordinator coordinator) {
        mActivity = activity;
        mToolbarManager = toolbarManager;
        mUserEducationHelper = new UserEducationHelper(mActivity, mHandler);

        assert coordinator != null;
        mCoordinator = coordinator;
        mCoordinator.addObserver(this);
    }

    public void destroy() {
        mCoordinator.removeObserver(this);
    }

    @Override
    public void onStatusIndicatorShowAnimationEnd() {
        if (!OfflineContentAvailabilityStatusProvider.getInstance()
                        .isPersistentContentAvailable()) {
            // Don't show the IPH if Download Home would be empty.
            return;
        }
    }

    private void turnOnHighlightForDownloadsMenuItem() {
    }

    private void turnOffHighlightForDownloadsMenuItem() {
    }
}
