// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import android.content.res.Resources;
import android.graphics.Rect;
import android.view.View;

import org.chromium.chrome.browser.user_education.IPHCommand;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.widget.ViewRectProvider;

/**
 * Controls showing IPH for Tab Group Sync on tablet tab strip, it will directly appear below the
 * group title of the last synced tab group.
 */
public class TabGroupSyncIphController {

    private final Resources mResources;
    private final UserEducationHelper mUserEducationHelper;
    private final int mStringId;
    private final Tracker mTracker;

    /**
     * Constructs the controller.
     *
     * @param resources The {@link Resources}.
     * @param userEducationHelper The {@link UserEducationHelper} for showing iph.
     * @param stringId The string id for the iph text bubble.
     * @param tracker The tracker to tracker whether we should show the iph.
     */
    public TabGroupSyncIphController(
            Resources resources,
            UserEducationHelper userEducationHelper,
            int stringId,
            Tracker tracker) {
        mResources = resources;
        mUserEducationHelper = userEducationHelper;
        mStringId = stringId;
        mTracker = tracker;
    }

    public void maybeShowIphOnTabStrip(
            View view, float left, float top, float right, float bottom) {
        // Return early when the IPH triggering criteria is not satisfied.
        if (mTracker == null) {
            return;
        }
        if (!mTracker.wouldTriggerHelpUI(FeatureConstants.TAB_GROUP_SYNC_ON_STRIP_FEATURE)) {
            return;
        }
        IPHCommand iphCommand =
                new IPHCommandBuilder(
                                mResources,
                                FeatureConstants.TAB_GROUP_SYNC_ON_STRIP_FEATURE,
                                mStringId,
                                mStringId)
                        .setAnchorView(view)
                        .setInsetRect(new Rect((int) left, (int) top, (int) right, (int) bottom))
                        .setViewRectProvider(new ViewRectProvider(view))
                        .setDismissOnTouch(true)
                        .build();
        mUserEducationHelper.requestShowIPH(iphCommand);
    }

    public void dismissTextBubble() {
        if (mUserEducationHelper != null) {
            mUserEducationHelper.dismissTextBubble();
        }
    }
}
