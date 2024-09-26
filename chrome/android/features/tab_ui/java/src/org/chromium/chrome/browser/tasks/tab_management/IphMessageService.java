// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.annotation.SuppressLint;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_ui.TabSwitcherIphController;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;

/** One of the concrete {@link MessageService} that only serves {@link MessageType.IPH}. */
public class IphMessageService extends MessageService {
    private static boolean sSkipIphInTests = true;

    private final TabSwitcherIphController mIphController;
    private Tracker mTracker;

    private Callback<Boolean> mInitializedCallback =
            (result) -> {
                if (wouldTriggerIph()) {
                    assert mTracker.isInitialized();
                    sendAvailabilityNotification(
                            new IphMessageData(this::review, (int messageType) -> dismiss()));
                }
            };

    /** This is the data type that this MessageService is serving to its Observer. */
    static class IphMessageData implements MessageData {
        private final MessageCardView.ReviewActionProvider mReviewActionProvider;
        private final MessageCardView.DismissActionProvider mDismissActionProvider;

        IphMessageData(
                MessageCardView.ReviewActionProvider reviewActionProvider,
                MessageCardView.DismissActionProvider dismissActionProvider) {
            mReviewActionProvider = reviewActionProvider;
            mDismissActionProvider = dismissActionProvider;
        }

        /**
         * @return The {@link MessageCardView.ReviewActionProvider} for the associated IPH.
         */
        MessageCardView.ReviewActionProvider getReviewActionProvider() {
            return mReviewActionProvider;
        }

        /**
         * @return The {@link MessageCardView.DismissActionProvider} for the associated IPH.
         */
        MessageCardView.DismissActionProvider getDismissActionProvider() {
            return mDismissActionProvider;
        }
    }

    IphMessageService(Profile profile, TabSwitcherIphController controller) {
        super(MessageType.IPH);
        mIphController = controller;
        mTracker = TrackerFactory.getTrackerForProfile(profile);
    }

    @VisibleForTesting
    protected void review() {
        mIphController.showIph();
    }

    @SuppressLint("CheckResult")
    @VisibleForTesting
    protected void dismiss() {
        mTracker.shouldTriggerHelpUI(FeatureConstants.TAB_GROUPS_DRAG_AND_DROP_FEATURE);
        mTracker.dismissed(FeatureConstants.TAB_GROUPS_DRAG_AND_DROP_FEATURE);
    }

    @Override
    public void addObserver(MessageObserver observer) {
        super.addObserver(observer);
        if (mTracker.isInitialized()) {
            mInitializedCallback.onResult(true);
        } else {
            mTracker.addOnInitializedCallback(mInitializedCallback);
        }
    }

    protected Callback<Boolean> getInitializedCallbackForTesting() {
        return mInitializedCallback;
    }

    /**
     * Whether to trigger the IPH taking into account testing configuration. By default the IPH is
     * not shown in tests.
     */
    private boolean wouldTriggerIph() {
        boolean wouldTriggerIph =
                mTracker.wouldTriggerHelpUI(FeatureConstants.TAB_GROUPS_DRAG_AND_DROP_FEATURE);
        boolean skipForTests = BuildConfig.IS_FOR_TEST && sSkipIphInTests;
        return wouldTriggerIph && !skipForTests;
    }

    /** Sets whether to skip the IPH for testing. By default the IPH is skipped. */
    static void setSkipIphInTestsForTesting(boolean skipIphInTests) {
        boolean oldValue = sSkipIphInTests;
        sSkipIphInTests = skipIphInTests;
        ResettersForTesting.register(() -> sSkipIphInTests = oldValue);
    }
}
