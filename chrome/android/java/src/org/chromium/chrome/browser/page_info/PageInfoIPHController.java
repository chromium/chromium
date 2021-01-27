// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.view.View;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;

/**
 * Controller to manage when an IPH bubble for PageInfo is shown.
 */
public class PageInfoIPHController {
    private final UserEducationHelper mUserEducationHelper;
    View mStatusView;

    /**
     * Constructor
     * @param activity The activity.
     * @param statusView The status view in the omnibox. Used as anchor for IPH bubble.
     */
    public PageInfoIPHController(Activity activity, View statusView) {
        mUserEducationHelper = new UserEducationHelper(activity,
                new Handler(Looper.getMainLooper()), TrackerFactory::getTrackerForProfile);

        mStatusView = statusView;
    }

    /**
     * Called when a permission prompt was shown.
     * @param contentSettings The content settings of the prompt.
     */
    public void onPermissionDialogShown(int[] contentSettings) {
        Tracker tracker = TrackerFactory.getTrackerForProfile(Profile.getLastUsedRegularProfile());
        tracker.notifyEvent(EventConstants.PERMISSION_REQUEST_SHOWN);

        if (contentSettings == null || contentSettings.length == 0) return;
        mUserEducationHelper.requestShowIPH(new IPHCommandBuilder(
                mStatusView.getContext().getResources(), FeatureConstants.PAGE_INFO_FEATURE,
                R.string.page_info_iph, R.string.page_info_iph)
                                                    .setAnchorView(mStatusView)
                                                    .setShouldHighlight(true)
                                                    .build());
    }
}
