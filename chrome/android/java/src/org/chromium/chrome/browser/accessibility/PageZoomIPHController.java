// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.view.View;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.FeatureConstants;

/** Controller for In-Product Help for the Page Zoom feature. */
public class PageZoomIPHController {
    private final AppMenuHandler mAppMenuHandler;
    private final View mToolbarMenuButton;
    private final UserEducationHelper mUserEducationHelper;

    public PageZoomIPHController(
            Activity mActivity,
            Profile profile,
            AppMenuHandler mAppMenuHandler,
            View mToolbarMenuButton) {
        this(
                mAppMenuHandler,
                mToolbarMenuButton,
                new UserEducationHelper(mActivity, profile, new Handler(Looper.getMainLooper())));
    }

    protected PageZoomIPHController(
            AppMenuHandler mAppMenuHandler,
            View mToolbarMenuButton,
            UserEducationHelper mUserEducationHelper) {
        this.mAppMenuHandler = mAppMenuHandler;
        this.mToolbarMenuButton = mToolbarMenuButton;
        this.mUserEducationHelper = mUserEducationHelper;
    }

    /**
     * Make a request to the |UserEducationHelper| to show the In-Product Help text bubble for
     * the Page Zoom feature. When shown, the Zoom option in the main menu will be highlighted,
     * and the Tracker for the current Profile will be notified.
     */
    public void showColdStartIPH() {
        mUserEducationHelper.requestShowIPH(
                new IPHCommandBuilder(
                                mToolbarMenuButton.getContext().getResources(),
                                FeatureConstants.PAGE_ZOOM_FEATURE,
                                R.string.page_zoom_iph_message,
                                R.string.page_zoom_iph_message)
                        .setAnchorView(mToolbarMenuButton)
                        .setOnShowCallback(() -> turnOnHighlightForMenuItem(R.id.page_zoom_id))
                        .setOnDismissCallback(this::turnOffHighlightForMenuItem)
                        .build());
    }

    private void turnOnHighlightForMenuItem(int highlightMenuItemId) {
        mAppMenuHandler.setMenuHighlight(highlightMenuItemId);
    }

    private void turnOffHighlightForMenuItem() {
        mAppMenuHandler.clearMenuHighlight();
    }
}
