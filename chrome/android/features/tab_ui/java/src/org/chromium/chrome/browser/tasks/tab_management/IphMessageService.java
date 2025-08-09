// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.annotation.SuppressLint;
import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_ui.TabSwitcherIphController;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.modelutil.PropertyModel;

/** One of the concrete {@link MessageService} that only serves {@link MessageType.IPH}. */
@NullMarked
public class IphMessageService extends MessageService {
    private static boolean sSkipIphInTests = true;

    private final TabSwitcherIphController mIphController;
    private Tracker mTracker;

    private final Callback<Boolean> mInitializedCallback =
            (result) -> {
                if (wouldTriggerIph()) {
                    assert mTracker.isInitialized();
                    sendAvailabilityNotification(this::buildModel);
                }
            };

    /** This is the data type that this MessageService is serving to its Observer. */
    static class IphMessageData {
        private final MessageCardView.ActionProvider mAcceptActionProvider;
        private final MessageCardView.ActionProvider mDismissActionProvider;

        IphMessageData(
                MessageCardView.ActionProvider acceptActionProvider,
                MessageCardView.ActionProvider dismissActionProvider) {
            mAcceptActionProvider = acceptActionProvider;
            mDismissActionProvider = dismissActionProvider;
        }

        /**
         * @return The {@link MessageCardView.ActionProvider} for the associated IPH.
         */
        MessageCardView.ActionProvider getAcceptActionProvider() {
            return mAcceptActionProvider;
        }

        /**
         * @return The {@link MessageCardView.ServiceDismissActionProvider} for the associated IPH.
         */
        MessageCardView.ActionProvider getDismissActionProvider() {
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
        mTracker.shouldTriggerHelpUi(FeatureConstants.TAB_GROUPS_DRAG_AND_DROP_FEATURE);
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
                mTracker.wouldTriggerHelpUi(FeatureConstants.TAB_GROUPS_DRAG_AND_DROP_FEATURE);
        boolean skipForTests = BuildConfig.IS_FOR_TEST && sSkipIphInTests;
        return wouldTriggerIph && !skipForTests;
    }

    /** Sets whether to skip the IPH for testing. By default the IPH is skipped. */
    static void setSkipIphInTestsForTesting(boolean skipIphInTests) {
        boolean oldValue = sSkipIphInTests;
        sSkipIphInTests = skipIphInTests;
        ResettersForTesting.register(() -> sSkipIphInTests = oldValue);
    }

    private PropertyModel buildModel(
            Context context, MessageCardView.ServiceDismissActionProvider serviceActionProvider) {
        return IphMessageCardViewModel.create(
                context, serviceActionProvider, new IphMessageData(this::review, this::dismiss));
    }
}
