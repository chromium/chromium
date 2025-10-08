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
import org.chromium.chrome.browser.tasks.tab_management.MessageCardView.ServiceDismissActionProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.function.Supplier;

/** One of the concrete {@link MessageService} that only serves {@link MessageType.IPH}. */
@NullMarked
public class IphMessageService extends MessageService<@MessageType Integer, @UiType Integer> {
    private static boolean sSkipIphInTests = true;

    private final TabSwitcherIphController mIphController;
    private final Supplier<Profile> mProfileSupplier;
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
        private final boolean mIsIncognito;

        IphMessageData(
                MessageCardView.ActionProvider acceptActionProvider,
                MessageCardView.ActionProvider dismissActionProvider,
                boolean isIncognito) {
            mAcceptActionProvider = acceptActionProvider;
            mDismissActionProvider = dismissActionProvider;
            mIsIncognito = isIncognito;
        }

        /**
         * @return The {@link MessageCardView.ActionProvider} for the associated IPH.
         */
        MessageCardView.ActionProvider getAcceptActionProvider() {
            return mAcceptActionProvider;
        }

        /**
         * @return The {@link ServiceDismissActionProvider} for the associated IPH.
         */
        MessageCardView.ActionProvider getDismissActionProvider() {
            return mDismissActionProvider;
        }

        /** Returns whether the message card is to be themed for incognito */
        boolean isIncognito() {
            return mIsIncognito;
        }
    }

    IphMessageService(Supplier<Profile> profileSupplier, TabSwitcherIphController controller) {
        super(
                MessageType.IPH,
                UiType.IPH_MESSAGE,
                R.layout.tab_grid_message_card_item,
                MessageCardViewBinder::bind);
        mIphController = controller;
        mTracker = TrackerFactory.getTrackerForProfile(profileSupplier.get());
        mProfileSupplier = profileSupplier;
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
    public void addObserver(MessageObserver<@MessageType Integer> observer) {
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
            Context context,
            ServiceDismissActionProvider<@MessageType Integer> serviceActionProvider) {
        boolean isIncognito = mProfileSupplier.get().isIncognitoBranded();
        return IphMessageCardViewModel.create(
                context,
                serviceActionProvider,
                new IphMessageData(this::review, this::dismiss, isIncognito));
    }
}
