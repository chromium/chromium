// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.user_education.IPHCommand;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;

/**
 * Controller to manage when and how we show home button in-product-help messages on the tab
 * switcher when start surface is enabled to users.
 */
public class StartSurfaceHomeButtonIPHController {
    private final UserEducationHelper mUserEducationHelper;
    private final IPHCommand mIPHCommand;
    private boolean mIsShowingIPH;

    public StartSurfaceHomeButtonIPHController(
            UserEducationHelper userEducationHelper, View homeButtonView) {
        mUserEducationHelper = userEducationHelper;
        mIPHCommand = new IPHCommandBuilder(homeButtonView.getResources(),
                FeatureConstants.START_SURFACE_TAB_SWITCHER_HOME_BUTTON_FEATURE,
                org.chromium.chrome.R.string.iph_ntp_with_feed_text,
                org.chromium.chrome.R.string.iph_ntp_with_feed_accessibility_text)
                              .setAnchorView(homeButtonView)
                              .setHighlightParams(new HighlightParams(HighlightShape.CIRCLE))
                              .setDismissOnTouch(true)
                              .setOnShowCallback(() -> mIsShowingIPH = true)
                              .setOnDismissCallback(() -> mIsShowingIPH = false)
                              .setAutoDismissTimeout(10 * 1000)
                              .build();
    }

    public void maybeShowIPH() {
        if (!mIsShowingIPH) {
            mUserEducationHelper.requestShowIPH(mIPHCommand);
        }
    }

    /**
     * Record the home button has been clicked when IPH is showing.
     */
    public void onHomeButtonClicked() {
        if (mIsShowingIPH) {
            Tracker tracker =
                    TrackerFactory.getTrackerForProfile(Profile.getLastUsedRegularProfile());
            tracker.notifyEvent(EventConstants.START_SURFACE_TAB_SWITCHER_HOME_BUTTON_CLICKED);
        }
    }

    @VisibleForTesting
    boolean isShowingHomeButtonIPHForTesting() {
        return mIsShowingIPH;
    }

    @VisibleForTesting
    void setIsShowingIPHForTesting(boolean isShowing) {
        mIsShowingIPH = isShowing;
    }

    @VisibleForTesting
    IPHCommand getIPHCommand() {
        return mIPHCommand;
    }
}
