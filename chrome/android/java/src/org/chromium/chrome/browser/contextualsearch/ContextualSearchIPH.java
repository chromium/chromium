// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.graphics.Point;
import android.graphics.Rect;
import android.text.TextUtils;
import android.view.View;
import android.widget.PopupWindow.OnDismissListener;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanelInterface;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.browser_ui.widget.textbubble.TextBubble;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.feature_engagement.TriggerState;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.RectProvider;

/**
 * Helper class for displaying In-Product Help UI for Contextual Search.
 */
public class ContextualSearchIPH {
    private static final int FLOATING_BUBBLE_SPACING_FACTOR = 10;
    private View mParentView;
    private ContextualSearchPanelInterface mSearchPanel;
    private TextBubble mHelpBubble;
    private RectProvider mRectProvider;
    private String mFeatureName;
    private boolean mIsShowing;
    private boolean mIsShowingInPanel;
    private boolean mDidShow;
    private boolean mIsPositionedByPanel;
    private boolean mHasUserEverEngaged;
    private Point mFloatingBubbleAnchorPoint;
    private OnDismissListener mDismissListener;
    private boolean mDidUserOptIn;

    /**
     * Constructs the helper class.
     */
    ContextualSearchIPH() {}

    /**
     * @param searchPanel The instance of {@link ContextualSearchPanel}.
     */
    void setSearchPanel(ContextualSearchPanelInterface searchPanel) {
        mSearchPanel = searchPanel;
    }

    /**
     * @param parentView The parent view that the {@link TextBubble} will be attached to.
     */
    void setParentView(View parentView) {
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
        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        tracker.notifyEvent(EventConstants.CONTEXTUAL_SEARCH_ENTITY_RESULT);
        if (wasActivatedByTap) {
            maybeShow(FeatureConstants.CONTEXTUAL_SEARCH_PROMOTE_PANEL_OPEN_FEATURE, profile);
        }
    }

    /**
     * Called when the Search Panel is shown.
     * @param wasActivatedByTap Whether Contextual Search was activated by tapping.
     * @param profile The {@link Profile} used for {@link TrackerFactory}.
     */
    void onSearchPanelShown(boolean wasActivatedByTap, Profile profile) {
        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        tracker.notifyEvent(wasActivatedByTap
                        ? EventConstants.CONTEXTUAL_SEARCH_TRIGGERED_BY_TAP
                        : EventConstants.CONTEXTUAL_SEARCH_TRIGGERED_BY_LONGPRESS);

        // Log whether IPH for tapping has been shown before.
        if (wasActivatedByTap) {
            ContextualSearchUma.logTapIPH(
                    tracker.getTriggerState(FeatureConstants.CONTEXTUAL_SEARCH_PROMOTE_TAP_FEATURE)
                    == TriggerState.HAS_BEEN_DISPLAYED);
        }
    }

    /**
     * Should be called after the user taps but a tap will not trigger due to longpress activation.
     * @param profile The active user profile.
     * @param bubbleAnchorPoint The point where the bubble arrow should be positioned.
     * @param hasUserEverEngaged Whether the user has ever engaged Contextual Search by opening
     *        the panel.
     * @param dismissListener An {@link OnDismissListener} to call when the bubble is dismissed.
     */
    void onNonTriggeringTap(Profile profile, Point bubbleAnchorPoint, boolean hasUserEverEngaged,
            OnDismissListener dismissListener) {
        mFloatingBubbleAnchorPoint = bubbleAnchorPoint;
        mHasUserEverEngaged = hasUserEverEngaged;
        mDismissListener = dismissListener;
        maybeShow(FeatureConstants.CONTEXTUAL_SEARCH_TAPPED_BUT_SHOULD_LONGPRESS_FEATURE, profile,
                false);
    }

    /**
     * Should be called when the panel is shown and a Translation is needed but the user has
     * not yet Opted-in.
     * @param profile The {@link Profile} used for {@link TrackerFactory}.
     */
    void onTranslationNeeded(Profile profile) {
        maybeShow(FeatureConstants.CONTEXTUAL_SEARCH_TRANSLATION_ENABLE_FEATURE, profile);
    }

    /**
     * Shows the appropriate In-Product Help UI if the conditions are met.
     * @param featureName Name of the feature in IPH, look at {@link FeatureConstants}.
     * @param profile The {@link Profile} used for {@link TrackerFactory}.
     */
    private void maybeShow(String featureName, Profile profile) {
        maybeShow(featureName, profile, true);
    }

    /**
     * Shows the appropriate In-Product Help UI if the conditions are met.
     * @param featureName Name of the feature in IPH, look at {@link FeatureConstants}.
     * @param profile The {@link Profile} used for {@link TrackerFactory}.
     * @param isPositionedByPanel Whether the bubble positioning should be based on the
     *        panel position instead of floating somewhere on the base page.
     */
    private void maybeShow(String featureName, Profile profile, boolean isPositionedByPanel) {
        mIsPositionedByPanel = isPositionedByPanel;
        if (mIsShowing || profile == null || mParentView == null
                || mIsPositionedByPanel && mSearchPanel == null) {
            return;
        }

        mFeatureName = featureName;
        maybeShowFeaturedBubble(profile);
    }

    /**
     * Shows a help bubble if the In-Product Help conditions are met.
     * Private state members are used to determine which message to show in the bubble
     * and how to position it.
     * @param profile The {@link Profile} used for {@link TrackerFactory}.
     */
    private void maybeShowFeaturedBubble(Profile profile) {
        if (mIsPositionedByPanel && !mSearchPanel.isShowing()) return;

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
                break;
            case FeatureConstants.CONTEXTUAL_SEARCH_TAPPED_BUT_SHOULD_LONGPRESS_FEATURE:
                // TODO(donnd): put the engaged user variant behind a separate fieldtrial parameter
                // so we can control it or collapse it later.
                if (mHasUserEverEngaged) {
                    stringId = R.string.contextual_search_iph_touch_and_hold_engaged;
                } else {
                    stringId = R.string.contextual_search_iph_touch_and_hold;
                }
                break;
            case FeatureConstants.CONTEXTUAL_SEARCH_TRANSLATION_ENABLE_FEATURE:
                stringId = R.string.contextual_search_iph_enable;
                break;
        }

        assert stringId != 0;
        assert mHelpBubble == null;
        mRectProvider = new RectProvider(getHelpBubbleAnchorRect());
        mHelpBubble = new TextBubble(mParentView.getContext(), mParentView, stringId, stringId,
                mRectProvider, ChromeAccessibilityUtil.get().isAccessibilityEnabled());

        // Set the dismiss logic.
        mHelpBubble.setDismissOnTouchInteraction(true);
        mHelpBubble.addOnDismissListener(() -> {
            tracker.dismissed(mFeatureName);
            mIsShowing = false;
            mHelpBubble = null;
        });
        if (mDismissListener != null) {
            mHelpBubble.addOnDismissListener(mDismissListener);
            mDismissListener = null;
        }

        maybeSetPreferredOrientation();
        mHelpBubble.show();
        mIsShowing = true;
        mIsShowingInPanel = false;
        mDidShow = true;
    }

    /**
     * Updates the position of the help bubble if it is showing.
     */
    void updateBubblePosition() {
        if (!mIsShowing || mIsShowingInPanel || mHelpBubble == null || !mHelpBubble.isShowing()) {
            return;
        }
        mRectProvider.setRect(getHelpBubbleAnchorRect());
    }

    /** Returns whether to show the In-Panel-Help and start tracking that IPH. */
    boolean startShowingInPanelHelp(Profile profile) {
        boolean shouldShow = TrackerFactory.getTrackerForProfile(profile).shouldTriggerHelpUI(
                FeatureConstants.CONTEXTUAL_SEARCH_IN_PANEL_HELP_FEATURE);
        if (shouldShow) {
            // Start tracking this alternative to the bubble being shown.
            mFeatureName = FeatureConstants.CONTEXTUAL_SEARCH_IN_PANEL_HELP_FEATURE;
        }
        mIsShowing = shouldShow;
        mIsShowingInPanel = mIsShowing;
        return shouldShow;
    }

    /**
     * @return A {@link Rect} object that represents the appropriate anchor for {@link TextBubble}.
     */
    private Rect getHelpBubbleAnchorRect() {
        int yInsetPx = mParentView.getResources().getDimensionPixelOffset(
                R.dimen.contextual_search_bubble_y_inset);
        if (!mIsPositionedByPanel) {
            // Position the bubble to point to an adjusted tap location, since there's no panel,
            // just a selected word.  It would be better to point to the rectangle of the selected
            // word, but that's not easy to get.
            int adjustFactor = shouldPositionBubbleBelowArrow() ? -1 : 1;
            int yAdjust = FLOATING_BUBBLE_SPACING_FACTOR * yInsetPx * adjustFactor;
            return new Rect(mFloatingBubbleAnchorPoint.x, mFloatingBubbleAnchorPoint.y + yAdjust,
                    mFloatingBubbleAnchorPoint.x, mFloatingBubbleAnchorPoint.y + yAdjust);
        }

        Rect anchorRect = mSearchPanel.getPanelRect();
        anchorRect.top -= yInsetPx;
        return anchorRect;
    }

    /** Overrides the preferred orientation if the bubble is not anchored to the panel. */
    private void maybeSetPreferredOrientation() {
        if (mIsPositionedByPanel) return;

        mHelpBubble.setPreferredVerticalOrientation(shouldPositionBubbleBelowArrow()
                        ? AnchoredPopupWindow.VerticalOrientation.BELOW
                        : AnchoredPopupWindow.VerticalOrientation.ABOVE);
    }

    /** @return whether the bubble should be positioned below it's arrow pointer. */
    private boolean shouldPositionBubbleBelowArrow() {
        // The bubble looks best when above the arrow, so we use that for most of the screen,
        // but needs to appear below the arrow near the top.
        return mFloatingBubbleAnchorPoint.y < mParentView.getHeight() / 3;
    }

    /**
     * Notifies that the search has completed so we can dismiss the In-Product Help UI, etc.
     * @param profile The current user profile.
     */
    void onCloseContextualSearch(Profile profile) {
        recordOptedInOutcome();
        if (!mIsShowing || TextUtils.isEmpty(mFeatureName)) return;

        if (mIsShowingInPanel) {
            assert mHelpBubble == null;
            dismissInPanelHelp(profile);
        } else {
            mHelpBubble.dismiss();
        }

        mIsShowing = false;
    }

    /**
     * @return whether the bubble is currently showing for the tap-where-longpress-needed promo.
     */
    boolean isShowingForTappedButShouldLongpress() {
        return mIsShowing
                && FeatureConstants.CONTEXTUAL_SEARCH_TAPPED_BUT_SHOULD_LONGPRESS_FEATURE.equals(
                        mFeatureName);
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

        // Log whether a Translation Opt-in suggestion IPH was ever shown.
        ContextualSearchUma.logTranslationsOptInIPHShown(
                tracker.getTriggerState(
                        FeatureConstants.CONTEXTUAL_SEARCH_TRANSLATION_ENABLE_FEATURE)
                == TriggerState.HAS_BEEN_DISPLAYED);
    }

    /**
     * Notifies the Feature Engagement backend and logs UMA metrics when the user Opted-in.
     * @param profile The current user {@link Profile}.
     */
    void doUserOptedInNotifications(Profile profile) {
        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        tracker.notifyEvent(EventConstants.CONTEXTUAL_SEARCH_ENABLED_OPT_IN);
        mDidUserOptIn = true;
    }

    /**
     * Handles notification that the OK button was pressed on the In-Panel-Help view.
     * @param profile The current user profile.
     */
    void onPanelHelpOkClicked(Profile profile) {
        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        tracker.notifyEvent(EventConstants.CONTEXTUAL_SEARCH_ACKNOWLEDGED_IN_PANEL_HELP);
        // Although tracker logs a user action too, it's OK to duplicate it with a Contextual
        // Search specific user action for ease of discovery in the analysis tool.
        ContextualSearchUma.logInPanelHelpAcknowledged();
    }

    /**
     * Dismisses In-Panel-Help from the tracker so it knows that the help is no longer shown.
     * @param profile The current user profile.
     */
    private void dismissInPanelHelp(Profile profile) {
        if (EventConstants.CONTEXTUAL_SEARCH_ACKNOWLEDGED_IN_PANEL_HELP.equals(mFeatureName)) {
            Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
            tracker.dismissed(mFeatureName);
            mFeatureName = null;
        }
    }

    /**
     * Records UMA metrics indicated whether the user Opted-in.
     */
    private void recordOptedInOutcome() {
        // If we showed the suggestion to Opt-in for Translations, Log whether the user did or not.
        if (mDidShow
                && mFeatureName.equals(
                        FeatureConstants.CONTEXTUAL_SEARCH_TRANSLATION_ENABLE_FEATURE)) {
            ContextualSearchUma.logTranslationsOptInIPHWorked(mDidUserOptIn);
        }
        mDidShow = false;
        mDidUserOptIn = false;
    }
}
