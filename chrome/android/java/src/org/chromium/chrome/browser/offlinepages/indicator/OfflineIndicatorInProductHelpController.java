// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.indicator;

import android.app.Activity;
import android.os.Handler;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.OfflineContentAvailabilityStatusProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.status_indicator.StatusIndicatorCoordinator;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.FeatureConstants;

/**
 * Class that controls the showing of in-product help for the offline indicator. The in-product help
 * is shown when the "show" animation finishes.
 */
public class OfflineIndicatorInProductHelpController
        implements StatusIndicatorCoordinator.StatusIndicatorObserver {
    private final Activity mActivity;
    private final ToolbarManager mToolbarManager;
    private final AppMenuHandler mAppMenuHandler;
    private final Handler mHandler = new Handler();
    private final UserEducationHelper mUserEducationHelper;
    private final StatusIndicatorCoordinator mCoordinator;

    public OfflineIndicatorInProductHelpController(
            final Activity activity,
            Profile profile,
            final ToolbarManager toolbarManager,
            final AppMenuHandler appMenuHandler,
            final StatusIndicatorCoordinator coordinator) {
        mActivity = activity;
        mToolbarManager = toolbarManager;
        mAppMenuHandler = appMenuHandler;
        mUserEducationHelper = new UserEducationHelper(mActivity, profile, mHandler);

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

        if (!mToolbarManager.getMenuButtonView().isShown()) {
            // Don't show the IPH if the menu button isn't visible (e.g. if the user has scrolled
            // down to the point where the toolbar has been hidden). If we did show it here, it
            // would just be a floating text bubble pointing at an empty spot where the menu button
            // would have been.
            return;
        }

        // TODO(sclittle): Currently, this works fine since the offline indicator is the only
        // StatusIndicator at the moment, but if different StatusIndicators are added in the
        // future, then it will be important to make sure that Chrome only shows this IPH for the
        // offline indicator, and not for other StatusIndicators.
        mUserEducationHelper.requestShowIPH(
                new IPHCommandBuilder(
                                mActivity.getResources(),
                                FeatureConstants.DOWNLOAD_INDICATOR_FEATURE,
                                R.string.iph_download_indicator_text,
                                R.string.iph_download_home_accessibility_text)
                        .setAnchorView(mToolbarManager.getMenuButtonView())
                        .setOnShowCallback(this::turnOnHighlightForDownloadsMenuItem)
                        .setOnDismissCallback(this::turnOffHighlightForDownloadsMenuItem)
                        .build());
    }

    private void turnOnHighlightForDownloadsMenuItem() {
        if (mAppMenuHandler == null) return;
        mAppMenuHandler.setMenuHighlight(R.id.downloads_menu_id);
    }

    private void turnOffHighlightForDownloadsMenuItem() {
        if (mAppMenuHandler == null) return;
        mAppMenuHandler.clearMenuHighlight();
    }
}
