// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for the Shared Tab Group Notice Bottom Sheet. */
@NullMarked
public class TabGroupShareNoticeBottomSheetCoordinator {

    private @MonotonicNonNull TabGroupShareNoticeBottomSheetView mView;

    interface TabGroupShareNoticeBottomSheetCoordinatorDelegate {
        /** Requests to show the bottom sheet content. */
        boolean requestShowContent();

        /** Hides the bottom sheet. */
        void hide(@StateChangeReason int hideReason);

        /** To be run on closing the bottom sheet. */
        void onSheetClosed();
    }

    private final BottomSheetController mBottomSheetController;
    private final Context mContext;
    private final Tracker mTracker;

    /**
     * @param bottomSheetController The controller for the bottom sheet.
     * @param context The {@link Context} to attach the bottom sheet to.
     * @param profile The current user profile.
     */
    public TabGroupShareNoticeBottomSheetCoordinator(
            BottomSheetController bottomSheetController, Context context, Profile profile) {
        mBottomSheetController = bottomSheetController;
        mContext = context;
        mTracker = TrackerFactory.getTrackerForProfile(profile);
    }

    /** Initializes the delegate. */
    @VisibleForTesting
    TabGroupShareNoticeBottomSheetCoordinatorDelegate initDelegate() {
        assumeNonNull(mView);
        return new TabGroupShareNoticeBottomSheetCoordinatorDelegate() {
            @Override
            public boolean requestShowContent() {
                return mBottomSheetController.requestShowContent(mView, /* animate= */ true);
            }

            @Override
            public void hide(@StateChangeReason int hideReason) {
                mBottomSheetController.hideContent(mView, /* animate= */ true, hideReason);
            }

            @Override
            public void onSheetClosed() {
                mTracker.notifyEvent("tab_group_share_notice_dismissed");
                mTracker.dismissed(FeatureConstants.TAB_GROUP_SHARE_NOTICE_FEATURE);
            }
        };
    }

    /** Requests to show the bottom sheet content. */
    public void requestShowContent() {
        if (!shouldDisplayNotice()) return;
        mView = new TabGroupShareNoticeBottomSheetView(mContext);

        TabGroupShareNoticeBottomSheetCoordinatorDelegate delegate = initDelegate();
        TabGroupShareNoticeBottomSheetMediator mMediator =
                new TabGroupShareNoticeBottomSheetMediator(mBottomSheetController, delegate);

        PropertyModelChangeProcessor.create(
                mMediator.getModel(), mView, TabGroupShareNoticeBottomSheetViewBinder::bind);
        mMediator.requestShowContent();
    }

    /** Returns whether the user has read the notice. */
    private boolean shouldDisplayNotice() {
        return mTracker.shouldTriggerHelpUi(FeatureConstants.TAB_GROUP_SHARE_NOTICE_FEATURE);
    }
}
