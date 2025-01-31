// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupShareNoticeBottomSheetCoordinator.TabGroupShareNoticeBottomSheetCoordinatorDelegate;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Mediator class for the Shared Tab Group Notice Bottom Sheet. This mediator contains the logic for
 * bottom sheet user interactions.
 */
public class TabGroupShareNoticeBottomSheetMediator {
    private final BottomSheetController mBottomSheetController;
    private final TabGroupShareNoticeBottomSheetCoordinatorDelegate mDelegate;
    private final PropertyModel mModel;
    private final Tracker mTracker;

    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetClosed(@StateChangeReason int reason) {
                    markHasReadNotice();
                    mBottomSheetController.removeObserver(mBottomSheetObserver);
                }

                @Override
                public void onSheetStateChanged(int newState, int reason) {
                    if (newState != BottomSheetController.SheetState.HIDDEN) return;
                    onSheetClosed(reason);
                }
            };

    /**
     * @param bottomSheetController The controller to use for showing or hiding the content.
     * @param delegate For handling view layer interactions.
     * @param profile The current user profile.
     */
    TabGroupShareNoticeBottomSheetMediator(
            BottomSheetController bottomSheetController,
            TabGroupShareNoticeBottomSheetCoordinatorDelegate delegate,
            Profile profile) {
        this(bottomSheetController, delegate, TrackerFactory.getTrackerForProfile(profile));
    }

    /**
     * @param bottomSheetController The controller to use for showing or hiding the content.
     * @param delegate For handling view layer interactions.
     * @param tracker Tracker to manage feature engagement.
     */
    @VisibleForTesting
    TabGroupShareNoticeBottomSheetMediator(
            BottomSheetController bottomSheetController,
            TabGroupShareNoticeBottomSheetCoordinatorDelegate delegate,
            Tracker tracker) {
        mBottomSheetController = bottomSheetController;
        mDelegate = delegate;
        mTracker = tracker;

        mModel =
                new PropertyModel.Builder(TabGroupShareNoticeBottomSheetProperties.ALL_KEYS)
                        .with(
                                TabGroupShareNoticeBottomSheetProperties.COMPLETION_HANDLER,
                                () -> hide(StateChangeReason.INTERACTION_COMPLETE))
                        .build();
    }

    /**
     * Requests to show the bottom sheet content. Will not show if the user has already accepted the
     * notice.
     */
    void requestShowContent() {
        if (!shouldDisplayNotice() || !mDelegate.requestShowContent()) return;
        mBottomSheetController.addObserver(mBottomSheetObserver);
    }

    /** Hides the bottom sheet. */
    void hide(@StateChangeReason int hideReason) {
        mDelegate.hide(hideReason);
    }

    /** Marks the notice as read. */
    @VisibleForTesting
    void markHasReadNotice() {
        mTracker.notifyEvent("tab_group_share_notice_dismissed");
        mTracker.dismissed(FeatureConstants.TAB_GROUP_SHARE_NOTICE_FEATURE);
    }

    /** Returns the model for the bottom sheet. */
    PropertyModel getModel() {
        return mModel;
    }

    /** Returns whether the user has read the notice. */
    private boolean shouldDisplayNotice() {
        return mTracker.shouldTriggerHelpUi(FeatureConstants.TAB_GROUP_SHARE_NOTICE_FEATURE);
    }
}
