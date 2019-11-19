// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.graphics.Rect;
import android.text.TextUtils;
import android.view.View;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.widget.textbubble.TextBubble;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.feature_engagement.TriggerState;
import org.chromium.ui.widget.RectProvider;

/**
 * Helper class for displaying In-Product Help UI for Contextual Search.
 */
public class ContextualSearchIPH {
    private View mParentView;
    private ContextualSearchPanel mSearchPanel;
    private TextBubble mHelpBubble;
    private RectProvider mRectProvider;
    private String mFeatureName;
    private boolean mIsShowing;

    /**
     * Constructs the helper class.
     */
    ContextualSearchIPH() {}

    /**
     * @param searchPanel The instance of {@link ContextualSearchPanel}.
     */
    void setSearchPanel(ContextualSearchPanel searchPanel) {
        mSearchPanel = searchPanel;
    }

    /**
     * @param parentView The parent view that the {@link TextBubble} will be attached to.
     */
    public void setParentView(View parentView) {
        mParentView = parentView;
    }

    /**
     * Called after the Contextual Search panel's animation is finished.
     * @param wasActivatedByTap Whether Contextual Search was activated by tapping.
     * @param profile The {@link Profile} used for {@link TrackerFactory}.
     */
    void onPanelFinishedShowing(boolean wasActivatedByTap, Profile profile) {
        if (!wasActivatedByTap) {
            maybeShow(FeatureConstants.CONTEXTUAL_SEARCH_PROMOTE_TAP_FEATURE, profile);
            maybeShow(FeatureConstants.CONTEXTUAL_SEARCH_WEB_SEARCH_FEATURE, profile);
        }
    }

    /**
     * Called after entity data is received.
     * @param wasActivatedByTap Whether Contextual Search was activated by tapping.
     * @param profile The {@link Profile} used for {@link TrackerFactory}.
     */
    void onEntityDataReceived(boolean wasActivatedByTap, Profile profile) {
        if (wasActivatedByTap) {
            maybeShow(FeatureConstants.CONTEXTUAL_SEARCH_PROMOTE_PANEL_OPEN_FEATURE, profile);
        }
    }

    /**
     * Shows the appropriate In-Product Help UI if the conditions are met.
     * @param featureName Name of the feature in IPH, look at {@link FeatureConstants}.
     * @param profile The {@link Profile} used for {@link TrackerFactory}.
     */
    private void maybeShow(String featureName, Profile profile) {
        if (mIsShowing) return;

        if (mSearchPanel == null || mParentView == null || profile == null) return;

        mFeatureName = featureName;
        maybeShowBubbleAbovePanel(profile);
    }

    /**
     * Shows a help bubble above the Contextual Search panel if the In-Product Help conditions are
     * met.
     * @param profile The {@link Profile} used for {@link TrackerFactory}.
     */
    private void maybeShowBubbleAbovePanel(Profile profile) {
        if (!mSearchPanel.isShowing()) return;

        final Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        if (!tracker.shouldTriggerHelpUI(mFeatureName)) return;

        int stringId = 0;
        switch (mFeatureName) {
            case FeatureConstants.CONTEXTUAL_SEARCH_WEB_SEARCH_FEATURE:
                stringId = R.string.contextual_search_iph_search_result;
                break;
            case FeatureConstants.CONTEXTUAL_SEARCH_PROMOTE_PANEL_OPEN_FEATURE:
                stringId = R.string.contextual_search_iph_entity;
                break;
            case FeatureConstants.CONTEXTUAL_SEARCH_PROMOTE_TAP_FEATURE:
                stringId = R.string.contextual_search_iph_tap;
        }

        assert stringId != 0;
        assert mHelpBubble == null;
        mRectProvider = new RectProvider(getHelpBubbleAnchorRect());
        mHelpBubble = new TextBubble(
                mParentView.getContext(), mParentView, stringId, stringId, mRectProvider);

        mHelpBubble.setDismissOnTouchInteraction(true);
        mHelpBubble.addOnDismissListener(() -> {
            tracker.dismissed(mFeatureName);
            mIsShowing = false;
            mHelpBubble = null;
        });

        mHelpBubble.show();
        mIsShowing = true;
    }

    /**
     * Updates the position of the help bubble if it is showing.
     */
    void updateBubblePosition() {
        if (!mIsShowing || mHelpBubble == null || !mHelpBubble.isShowing()) return;

        mRectProvider.setRect(getHelpBubbleAnchorRect());
    }

    /**
     * @return A {@link Rect} object that represents the appropriate anchor for {@link TextBubble}.
     */
    private Rect getHelpBubbleAnchorRect() {
        Rect anchorRect = mSearchPanel.getPanelRect();
        int yInsetPx = mParentView.getResources().getDimensionPixelOffset(
                R.dimen.contextual_search_bubble_y_inset);
        anchorRect.top -= yInsetPx;
        return anchorRect;
    }

    /**
     * Dismisses the In-Product Help UI.
     */
    void dismiss() {
        if (!mIsShowing || TextUtils.isEmpty(mFeatureName)) return;

        mHelpBubble.dismiss();

        mIsShowing = false;
    }

    /**
     * Notifies the Feature Engagement backend and logs UMA metrics.
     * @param profile The current {@link Profile}.
     * @param wasSearchContentViewSeen Whether the Contextual Search panel was opened.
     * @param wasActivatedByTap Whether the Contextual Search was activating by tapping.
     * @param wasContextualCardsDataShown Whether entity cards were received.
     */
    public static void doSearchFinishedNotifications(Profile profile,
            boolean wasSearchContentViewSeen, boolean wasActivatedByTap,
            boolean wasContextualCardsDataShown) {
        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        if (wasSearchContentViewSeen) {
            tracker.notifyEvent(EventConstants.CONTEXTUAL_SEARCH_PANEL_OPENED);
            tracker.notifyEvent(wasActivatedByTap
                            ? EventConstants.CONTEXTUAL_SEARCH_PANEL_OPENED_AFTER_TAP
                            : EventConstants.CONTEXTUAL_SEARCH_PANEL_OPENED_AFTER_LONGPRESS);

            // Log whether IPH for opening the panel has been shown before.
            ContextualSearchUma.logPanelOpenedIPH(
                    tracker.getTriggerState(
                            FeatureConstants.CONTEXTUAL_SEARCH_PROMOTE_PANEL_OPEN_FEATURE)
                    == TriggerState.HAS_BEEN_DISPLAYED);

            // Log whether IPH for Contextual Search web search has been shown before.
            ContextualSearchUma.logContextualSearchIPH(
                    tracker.getTriggerState(FeatureConstants.CONTEXTUAL_SEARCH_WEB_SEARCH_FEATURE)
                    == TriggerState.HAS_BEEN_DISPLAYED);
        }
        if (wasContextualCardsDataShown) {
            tracker.notifyEvent(EventConstants.CONTEXTUAL_SEARCH_PANEL_OPENED_FOR_ENTITY);
        }
    }
}
