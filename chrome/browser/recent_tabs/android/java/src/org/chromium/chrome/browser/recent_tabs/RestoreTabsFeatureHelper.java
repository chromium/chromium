// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;

import org.chromium.base.Callback;
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionWindow;
import org.chromium.chrome.browser.recent_tabs.RestoreTabsMetricsHelper.RestoreTabsOnFREPromoShowResult;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.function.Supplier;

/** A class of helper methods that assist in the restore tabs workflow. */
@NullMarked
public class RestoreTabsFeatureHelper {
    private static final long RESTORE_TABS_RECENCY_THRESHOLD_MS = TimeUnit.DAYS.toMillis(7);
    private @Nullable RestoreTabsController mController;
    private @Nullable RestoreTabsControllerDelegate mDelegate;
    private @Nullable RestoreTabsControllerDelegate mDelegateForTesting;
    private @Nullable ForeignSessionHelper mForeignSessionHelper;

    public RestoreTabsFeatureHelper() {}

    public void destroy() {
        if (mForeignSessionHelper != null) {
            mForeignSessionHelper.destroy();
            mForeignSessionHelper = null;
        }

        if (mController != null) {
            mController.destroy();
            mController = null;
        }

        if (mDelegate != null) {
            mDelegate = null;
        }
    }

    /** Check the criteria for displaying the restore tabs promo. */
    public void maybeShowPromo(
            Activity activity,
            Profile profile,
            TabCreatorManager tabCreatorManager,
            BottomSheetController bottomSheetController,
            Supplier<Integer> gtsTabListModelSizeSupplier,
            Callback<Integer> scrollGTSToRestoredTabsCallback,
            Supplier<ModalDialogManager> modalDialogManagerSupplier) {
        if (profile == null || profile.isOffTheRecord()) {
            RestoreTabsMetricsHelper.recordPromoShowResultHistogram(
                    RestoreTabsOnFREPromoShowResult.NULL_PROFILE);
            return;
        }
        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        updateTrackerIfFirstStartIsRecent(profile, tracker);
        if (!tracker.wouldTriggerHelpUi(FeatureConstants.RESTORE_TABS_ON_FRE_FEATURE)) {
            RestoreTabsMetricsHelper.recordPromoShowResultHistogram(
                    RestoreTabsOnFREPromoShowResult.NOT_ELIGIBLE);
            return;
        }
        mForeignSessionHelper = new ForeignSessionHelper(profile);
        if (!mForeignSessionHelper.isTabSyncEnabled()) {
            destroy();

            RestoreTabsMetricsHelper.recordPromoShowResultHistogram(
                    RestoreTabsOnFREPromoShowResult.TAB_SYNC_DISABLED);
            return;
        }
        // Trigger a session sync in the event one has not occurred. This is asynchronous so it may
        // not finish by the time we attempt to show. However, we need to show immediately to avoid
        // showing up after the GTS is already visible to avoid potential misclicks. Triggering
        // this does mean next time the GTS becomes visible we are more likely to have tabs
        // available.
        mForeignSessionHelper.triggerSessionSync();

        // Determines whether the promo is to be shown for the first or second time.
        // To determine if it is the first time that the promo is being triggered, the logic checks
        // if the promo has ever triggered. Since wouldTriggerHelpUi indicates that the promo
        // will be shown if the shouldTriggerHelpUi is called, it is assumed that it will show,
        // hence setting the showCount to 1. If it has already triggered and the same criteria is
        // fulfilled, it can be assumed this will be the second time the promo shows. Note that this
        // logic only works for the 2 count max for promo showing. The hasEverTriggered call must be
        // before the shouldTriggerHelpUi call, otherwise it will always return true.
        int showCount =
                tracker.hasEverTriggered(FeatureConstants.RESTORE_TABS_ON_FRE_FEATURE, false)
                        ? 2
                        : 1;
        RestoreTabsMetricsHelper.setPromoShownCount(showCount);

        // The difference between wouldTriggerHelpUi and shouldTriggerHelpUi is that the latter
        // increments an internal trigger count if it returns true, which means that if it is called
        // successfully, IPH must show. Alternatively, the former lets the logic know if the promo
        // is expected to show, which can help determine if it is being shown for the first or
        // second time.
        List<ForeignSession> sessions = mForeignSessionHelper.getMobileAndTabletForeignSessions();
        if (hasValidSyncedDevices(sessions)
                && tracker.shouldTriggerHelpUi(FeatureConstants.RESTORE_TABS_ON_FRE_FEATURE)) {
            createDelegate(
                    activity,
                    profile,
                    tabCreatorManager,
                    bottomSheetController,
                    gtsTabListModelSizeSupplier,
                    scrollGTSToRestoredTabsCallback,
                    modalDialogManagerSupplier);
            mDelegate.showPromo(sessions);
            RestoreTabsMetricsHelper.recordPromoShowResultHistogram(
                    RestoreTabsOnFREPromoShowResult.SHOWN);
            RestoreTabsMetricsHelper.recordSyncedDevicesCountHistogram(sessions.size());
        } else {
            destroy();

            // This metric covers the situations where:
            // * no tabs are synced.
            // * synced tabs haven't finished syncing.
            RestoreTabsMetricsHelper.recordPromoShowResultHistogram(
                    RestoreTabsOnFREPromoShowResult.NO_SYNCED_TABS);
        }
    }

    @EnsuresNonNull("mDelegate")
    private void createDelegate(
            Activity activity,
            Profile profile,
            TabCreatorManager tabCreatorManager,
            BottomSheetController bottomSheetController,
            Supplier<Integer> gtsTabListModelSizeSupplier,
            Callback<Integer> scrollGTSToRestoredTabsCallback,
            Supplier<ModalDialogManager> modalDialogManagerSupplier) {
        mDelegate =
                (mDelegateForTesting != null)
                        ? mDelegateForTesting
                        : new RestoreTabsControllerDelegate() {
                            private boolean mWasDismissed;

                            @Override
                            public void showPromo(List<ForeignSession> sessions) {
                                mController =
                                        RestoreTabsControllerFactory.createInstance(
                                                activity,
                                                profile,
                                                tabCreatorManager,
                                                bottomSheetController,
                                                modalDialogManagerSupplier);
                                mController.showHomeScreen(
                                        assumeNonNull(mForeignSessionHelper),
                                        sessions,
                                        assumeNonNull(mDelegate));
                            }

                            @Override
                            public void onDismissed() {
                                assert !mWasDismissed : "Promo should only be dismissed once.";
                                mWasDismissed = true;

                                TrackerFactory.getTrackerForProfile(profile)
                                        .dismissed(FeatureConstants.RESTORE_TABS_ON_FRE_FEATURE);

                                destroy();
                            }

                            @Override
                            public int getGTSTabListModelSize() {
                                return gtsTabListModelSizeSupplier.get();
                            }

                            @Override
                            public void scrollGTSToRestoredTabs(int tabListModelSize) {
                                scrollGTSToRestoredTabsCallback.onResult(tabListModelSize);
                            }
                        };
    }

    private boolean hasValidSyncedDevices(List<ForeignSession> sessions) {
        for (ForeignSession session : sessions) {
            for (ForeignSessionWindow window : session.windows) {
                if (window.tabs.size() != 0) {
                    return true;
                }
            }
        }
        return false;
    }

    void setRestoreTabsControllerDelegateForTesting(RestoreTabsControllerDelegate delegate) {
        mDelegateForTesting = delegate;
    }

    void updateTrackerIfFirstStartIsRecent(Profile profile, Tracker tracker) {
        if (ChromeFeatureList.sAndroidShowRestoreTabsPromoOnFreBypassedKillSwitch.isEnabled()) {
            long firstInitialized =
                    ChromeSharedPreferences.getInstance()
                            .readLong(ChromePreferenceKeys.FIRST_CTA_START_TIMESTAMP, -1L);
            if (firstInitialized == -1L || isTimestampRecent(firstInitialized)) {
                tracker.notifyEvent(EventConstants.RESTORE_TABS_ON_FIRST_RUN_SHOW_PROMO);
            }
        }
    }

    boolean isTimestampRecent(long timestamp) {
        long threshold = System.currentTimeMillis() - RESTORE_TABS_RECENCY_THRESHOLD_MS;
        return timestamp > threshold;
    }
}
