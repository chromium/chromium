// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.content.res.Resources;
import android.graphics.Rect;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.user_education.IphCommand;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.LocalizationUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Controls the display of IPH on tablet tab strip. The IPH will appear directly below the center of
 * the anchor rect area.
 */
@NullMarked
public class TabStripIphController {
    /** An enum representing the type of IPH. */
    @IntDef({
        IphType.TAB_GROUP_SYNC,
        IphType.GROUP_TITLE_NOTIFICATION_BUBBLE,
        IphType.TAB_NOTIFICATION_BUBBLE,
        IphType.TAB_TEARING_XR
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

        /** Indicates the IPH is triggered for tab tearing on XR. */
        int TAB_TEARING_XR = 3;
    }

    private static final int IPH_AUTO_DISMISS_WAIT_TIME_MS = 5 * 1000;

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

    /**
     * Calculates the anchor rect and display an In-Product Help (IPH) on the tab strip.
     *
     * @param groupTitle The group title or its related tab where the IPH should be shown on.
     * @param tab The tab to show the IPH on. Pass in {@code null} if the IPH is not tied to a
     *     particular tab. param iphType The type of the IPH to be shown.
     * @param toolbarContainerView Used to get the anchor view for the IPH.
     * @param iphType The type of IPH to display.
     * @param tabStripHeight The height of the tab strip, used to calculate the anchor rect.
     * @param enableSnoozeMode Whether to enable snooze mode on the IPH.
     */
    public void showIphOnTabStrip(
            @Nullable StripLayoutGroupTitle groupTitle,
            @Nullable StripLayoutTab tab,
            View toolbarContainerView,
            @IphType int iphType,
            float tabStripHeight,
            boolean enableSnoozeMode) {
        Rect anchorRect =
                calculateAnchorRect(toolbarContainerView, groupTitle, tab, iphType, tabStripHeight);
        IphCommand iphCommand =
                new IphCommandBuilder(
                                mResources,
                                getIphFeature(iphType),
                                getIphString(iphType),
                                getIphString(iphType))
                        .setAnchorView(toolbarContainerView)
                        .setAnchorRect(anchorRect)
                        .setDismissOnTouch(true)
                        .setAutoDismissTimeout(IPH_AUTO_DISMISS_WAIT_TIME_MS)
                        .setEnableSnoozeMode(enableSnoozeMode)
                        .build();
        mUserEducationHelper.requestShowIph(iphCommand);
    }

    /** Dismisses any currently visible IPH text bubble. */
    public void dismissTextBubble() {
        if (mUserEducationHelper != null) {
            mUserEducationHelper.dismissTextBubble();
        }
    }

    /**
     * Determines whether the IPH would be triggered. IPH wo
     *
     * @param iphType The type of IPH.
     * @return {@code true} if the IPH bubble for the given type would be triggered.
     */
    public boolean wouldTriggerIph(@IphType int iphType) {
        return mTracker != null && mTracker.wouldTriggerHelpUi(getIphFeature(iphType));
    }

    /**
     * @param toolbarContainerView The view where the IPH will be shown on.
     * @param groupTitle The group title or its related tab where the IPH should be shown on.
     * @param tab The tab to show the IPH on. Pass in {@code null} if the IPH is not tied to a
     *     particular tab.
     * @param iphType The type of the IPH to be shown.
     * @return the anchor area where the IPH should be positioned underneath.
     */
    private Rect calculateAnchorRect(
            View toolbarContainerView,
            @Nullable StripLayoutGroupTitle groupTitle,
            @Nullable StripLayoutTab tab,
            @IphType int iphType,
            float tabStripHeight) {
        assert groupTitle != null || tab != null : "Either groupTitle or tab should be non-null.";

        float dpToPx = mResources.getDisplayMetrics().density;
        boolean isRtl = LocalizationUtils.isLayoutRtl();
        int[] toolbarCoordinates = new int[2];
        toolbarContainerView.getLocationInWindow(toolbarCoordinates);
        float xOffset = 0f;
        float yOffset = toolbarCoordinates[1];

        // Get default anchor rect for IPH.
        Rect anchorRect = new Rect();
        if (groupTitle != null) {
            groupTitle.getPaddedBoundsPx(anchorRect);
        } else {
            assert tab != null;
            tab.getAnchorRect(anchorRect);
        }

        switch (iphType) {
            case IphType.TAB_GROUP_SYNC:
                // Adjust the bottom boundary to match the tab strip's lower edge.
                anchorRect.bottom = (int) (tabStripHeight * dpToPx);
                break;
            case IphType.GROUP_TITLE_NOTIFICATION_BUBBLE:
                assert groupTitle != null;
                anchorRect.left = (int) (groupTitle.getBubbleDrawX() * dpToPx);
                anchorRect.right =
                        (int) ((groupTitle.getBubbleDrawX() + groupTitle.getBubbleSize()) * dpToPx);
                break;
            case IphType.TAB_TEARING_XR: // fallthrough
            case IphType.TAB_NOTIFICATION_BUBBLE:
                assert tab != null;
                float left =
                        isRtl
                                ? -tab.getFaviconPadding() - tab.getFaviconSize()
                                : tab.getFaviconPadding();
                float right =
                        isRtl
                                ? -tab.getFaviconPadding()
                                : tab.getFaviconPadding() + tab.getFaviconSize();
                xOffset = (isRtl ? tab.getDrawX() + tab.getWidth() : tab.getDrawX()) * dpToPx;
                anchorRect.left = (int) (left * dpToPx);
                anchorRect.right = (int) (right * dpToPx);
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
            case IphType.TAB_TEARING_XR:
                return FeatureConstants.IPH_TAB_TEARING_XR;
            default:
                throw new IllegalArgumentException("Invalid IPH type");
        }
    }

    private @StringRes int getIphString(@IphType int iphType) {
        switch (iphType) {
            case IphType.TAB_GROUP_SYNC:
                return R.string.newly_synced_tab_group_iph;
            case IphType.GROUP_TITLE_NOTIFICATION_BUBBLE: // Fallthrough.
            case IphType.TAB_NOTIFICATION_BUBBLE:
                return R.string.tab_group_share_notification_bubble_iph;
            case IphType.TAB_TEARING_XR:
                return R.string.iph_tab_tearing_xr;
            default:
                throw new IllegalArgumentException("Invalid IPH type");
        }
    }
}
