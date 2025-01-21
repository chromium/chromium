// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.content.res.Resources;
import android.graphics.Rect;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.StringRes;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.user_education.IphCommand;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Controls the display of IPH on tablet tab strip. The IPH will appear directly below the center of
 * the anchor rect area.
 */
public class TabStripIphController {
    /** An enum representing the type of IPH. */
    @IntDef({
        IphType.TAB_GROUP_SYNC,
        IphType.GROUP_TITLE_NOTIFICATION_BUBBLE,
        IphType.TAB_NOTIFICATION_BUBBLE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface IphType {
        /** Indicates the IPH is triggered for syncing a tab group. */
        int TAB_GROUP_SYNC = 0;

        /**
         * Indicates the IPH is triggered for displaying a notification bubble on the group title.
         */
        int GROUP_TITLE_NOTIFICATION_BUBBLE = 1;

        /** Indicates the IPH is triggered for displaying a notification bubble on a tab. */
        int TAB_NOTIFICATION_BUBBLE = 2;
    }

    private final Resources mResources;
    private final UserEducationHelper mUserEducationHelper;
    private final Tracker mTracker;

    /**
     * Constructs the controller.
     *
     * @param resources The {@link Resources}.
     * @param userEducationHelper The {@link UserEducationHelper} for showing iph.
     * @param tracker The tracker to tracker whether we should show the iph.
     */
    public TabStripIphController(
            Resources resources, UserEducationHelper userEducationHelper, Tracker tracker) {
        mResources = resources;
        mUserEducationHelper = userEducationHelper;
        mTracker = tracker;
    }

    public void maybeShowIphOnTabStrip(
            StripLayoutGroupTitle groupTitle,
            View toolbarContainerView,
            @IphType int iphType,
            float tabStripHeight) {
        // Return early when the IPH triggering criteria is not satisfied.
        if (mTracker == null && !mTracker.wouldTriggerHelpUi(getIphFeature(iphType))) {
            return;
        }

        Rect anchorRect =
                calculateAnchorRect(toolbarContainerView, groupTitle, iphType, tabStripHeight);
        IphCommand iphCommand =
                new IphCommandBuilder(
                                mResources,
                                getIphFeature(iphType),
                                getIphString(iphType),
                                getIphString(iphType))
                        .setAnchorView(toolbarContainerView)
                        .setAnchorRect(anchorRect)
                        .setDismissOnTouch(true)
                        .build();
        mUserEducationHelper.requestShowIph(iphCommand);
    }

    /** Dismisses any currently visible IPH text bubble. */
    public void dismissTextBubble() {
        if (mUserEducationHelper != null) {
            mUserEducationHelper.dismissTextBubble();
        }
    }

    private Rect calculateAnchorRect(
            View toolbarContainerView,
            StripLayoutGroupTitle groupTitle,
            @IphType int iphType,
            float tabStripHeight) {
        float dpToPx = mResources.getDisplayMetrics().density;
        int[] toolbarCoordinates = new int[2];
        toolbarContainerView.getLocationInWindow(toolbarCoordinates);
        float xOffset = 0f;
        float yOffset = toolbarCoordinates[1];

        // Get default anchor rect for IPH.
        Rect anchorRect = new Rect();
        groupTitle.getPaddedBoundsPx(anchorRect);

        switch (iphType) {
            case IphType.TAB_GROUP_SYNC:
                // Adjust the bottom boundary to match the tab strip's lower edge.
                anchorRect.bottom = (int) (tabStripHeight * dpToPx);
                break;
            case IphType.GROUP_TITLE_NOTIFICATION_BUBBLE:
                // TODO(crbug.com/348728701): implement.
                break;
            case IphType.TAB_NOTIFICATION_BUBBLE:
                // TODO(crbug.com/348728701): implement
                break;
            default:
                throw new IllegalArgumentException("Invalid IPH type");
        }
        anchorRect.offset((int) xOffset, (int) yOffset);
        return anchorRect;
    }

    private @FeatureConstants String getIphFeature(@IphType int iphType) {
        switch (iphType) {
            case IphType.TAB_GROUP_SYNC:
                return FeatureConstants.TAB_GROUP_SYNC_ON_STRIP_FEATURE;
            case IphType.GROUP_TITLE_NOTIFICATION_BUBBLE: // Fallthrough.
            case IphType.TAB_NOTIFICATION_BUBBLE:
                return FeatureConstants.TAB_GROUP_SHARE_NOTIFICATION_BUBBLE_ON_STRIP_FEATURE;
            default:
                throw new IllegalArgumentException("Invalid IPH type");
        }
    }

    private @StringRes int getIphString(@IphType int iphType) {
        switch (iphType) {
            case IphType.TAB_GROUP_SYNC:
                return R.string.newly_synced_tab_group_iph;
            case IphType.GROUP_TITLE_NOTIFICATION_BUBBLE:
            case IphType.TAB_NOTIFICATION_BUBBLE:
                // TODO(crbug.com/348728701): Implement notification bubble iph string.
                return -1;
            default:
                throw new IllegalArgumentException("Invalid IPH type");
        }
    }
}
