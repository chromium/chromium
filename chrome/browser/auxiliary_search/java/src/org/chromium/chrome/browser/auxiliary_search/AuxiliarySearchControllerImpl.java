// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.sAndroidAppIntegrationWithFaviconScheduleDelayTimeMs;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.sAndroidAppIntegrationWithFaviconZeroStateFaviconNumber;

import android.content.Context;
import android.graphics.Bitmap;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.TimeUtils;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchEntry;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchMetrics.RequestStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** The Controller to handle the communication between Chrome and {@link AuxiliarySearchDonor}. */
public class AuxiliarySearchControllerImpl
        implements AuxiliarySearchController,
                AuxiliarySearchConfigManager.ShareTabsWithOsStateListener {
    private static final String TAG = "AuxiliarySearch";
    private final @NonNull Context mContext;
    private final @NonNull Profile mProfile;
    private final @NonNull FaviconHelper mFaviconHelper;
    private final @NonNull AuxiliarySearchProvider mAuxiliarySearchProvider;
    private final @NonNull AuxiliarySearchDonor mDonor;
    private final boolean mIsFaviconEnabled;
    private final int mZeroStateFaviconNumber;
    private final int mDefaultFaviconSize;

    private @NonNull ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private boolean mHasDeletingTask;
    private int mTaskFinishedCount;
    private boolean mSharedTabsWithOsState;

    @VisibleForTesting
    public AuxiliarySearchControllerImpl(
            @NonNull Context context,
            @NonNull Profile profile,
            @NonNull AuxiliarySearchProvider auxiliarySearchProvider,
            @NonNull AuxiliarySearchDonor auxiliarySearchDonor,
            @NonNull FaviconHelper faviconHelper) {
        mContext = context;
        mProfile = profile;
        mAuxiliarySearchProvider = auxiliarySearchProvider;
        mDonor = auxiliarySearchDonor;
        mFaviconHelper = faviconHelper;
        mIsFaviconEnabled = ChromeFeatureList.sAndroidAppIntegrationWithFavicon.isEnabled();

        mZeroStateFaviconNumber =
                sAndroidAppIntegrationWithFaviconZeroStateFaviconNumber.getValue();
        mDefaultFaviconSize = AuxiliarySearchUtils.getFaviconSize(mContext.getResources());

        mSharedTabsWithOsState = AuxiliarySearchUtils.isShareTabsWithOsEnabled();
        if (mSharedTabsWithOsState) {
            initDonor();
        }
        AuxiliarySearchConfigManager.getInstance().addListener(this);
    }

    /**
     * @param context The application context.
     * @param profile The profile in use.
     * @param tabModelSelector The instance of {@link TabModelSelector}.
     */
    public AuxiliarySearchControllerImpl(
            @NonNull Context context,
            @NonNull Profile profile,
            @Nullable TabModelSelector tabModelSelector) {
        this(
                context,
                profile,
                new AuxiliarySearchProvider(context, profile, tabModelSelector),
                new AuxiliarySearchDonor(context),
                new FaviconHelper());
    }

    // AuxiliarySearchController implementations.

    @Override
    public void register(ActivityLifecycleDispatcher lifecycleDispatcher) {
        if (lifecycleDispatcher == null) return;
        mActivityLifecycleDispatcher = lifecycleDispatcher;
        mActivityLifecycleDispatcher.register(this);
    }

    @Override
    public void onResumeWithNative() {
        if (!mSharedTabsWithOsState) return;

        deleteAllTabs();
    }

    @Override
    public void onPauseWithNative() {
        tryDonateTabs();
    }

    @Override
    public void destroy() {
        AuxiliarySearchConfigManager.getInstance().removeListener(this);

        if (mActivityLifecycleDispatcher != null) {
            mActivityLifecycleDispatcher.unregister(this);
            mActivityLifecycleDispatcher = null;
        }
        if (mDonor != null) {
            mDonor.destroy();
        }
        mFaviconHelper.destroy();
    }

    @Override
    public void onBackgroundTaskStart(
            @NonNull List<AuxiliarySearchEntry> tabs,
            @NonNull Map<Integer, Bitmap> tabIdToFaviconMap,
            @NonNull Callback<Boolean> callback,
            long startTimeMillis) {
        assert mSharedTabsWithOsState;

        mDonor.donateFavicons(
                tabs,
                tabIdToFaviconMap,
                (success) -> {
                    callback.onResult(success);
                    AuxiliarySearchMetrics.recordScheduledDonateTime(
                            TimeUtils.uptimeMillis() - startTimeMillis);
                });
    }

    // AuxiliarySearchConfigManager.ShareTabsWithOsStateListener implementations.
    @Override
    public void onConfigChanged(boolean enabled) {
        if (mSharedTabsWithOsState == enabled) return;

        mSharedTabsWithOsState = enabled;
        AuxiliarySearchUtils.setSharedTabsWithOs(enabled);
        if (enabled) {
            // Initializes the session now.
            initDonor();
        } else {
            // When disabled, remove all shared Tabs and closes the session.
            deleteAllTabs();
            mDonor.destroy();
        }
    }

    private void tryDonateTabs() {
        if (mHasDeletingTask || !mSharedTabsWithOsState) return;

        long startTime = TimeUtils.uptimeMillis();
        mAuxiliarySearchProvider.getTabsSearchableDataProtoAsync(
                (tabs) -> onNonSensitiveTabsAvailable(tabs, startTime));
    }

    /**
     * Called when a list of up to 100 non sensitive Tabs is available.
     *
     * @param tabs A list of non sensitive Tabs.
     * @param startTimeMs The starting time to query the tab list.
     */
    @VisibleForTesting
    public void onNonSensitiveTabsAvailable(@NonNull List<Tab> tabs, long startTimeMs) {
        AuxiliarySearchMetrics.recordQueryTabTime(TimeUtils.uptimeMillis() - startTimeMs);

        if (tabs.isEmpty()) return;

        if (mIsFaviconEnabled) {
            tabs.sort(AuxiliarySearchProvider.sComparator);
        }

        Callback<Boolean> onDonationCompleteCallback =
                (success) -> {
                    AuxiliarySearchMetrics.recordDonateTime(TimeUtils.uptimeMillis() - startTimeMs);
                    AuxiliarySearchMetrics.recordDonationRequestStatus(
                            success ? RequestStatus.SUCCESSFUL : RequestStatus.UNSUCCESSFUL);
                };

        // Donates the list of tabs without favicons.
        mDonor.donateTabs(tabs, onDonationCompleteCallback);

        if (!mIsFaviconEnabled) {
            return;
        }

        mTaskFinishedCount = 0;
        Map<Tab, Bitmap> tabToFaviconMap = new HashMap<>();
        int zeroStateFaviconFetchedNumber =
                mIsFaviconEnabled ? Math.min(tabs.size(), mZeroStateFaviconNumber) : 0;

        long faviconStartTimeMs = TimeUtils.uptimeMillis();
        // When donating favicon is enabled, Chrome only donates the favicons of the most
        // recently visited tabs in the first round.
        for (int i = 0; i < zeroStateFaviconFetchedNumber; i++) {
            Tab tab = tabs.get(i);

            mFaviconHelper.getLocalFaviconImageForURL(
                    mProfile,
                    tab.getUrl(),
                    mDefaultFaviconSize,
                    (image, url) -> {
                        mTaskFinishedCount++;
                        if (image != null) {
                            tabToFaviconMap.put(tab, image);
                        }

                        // Once all favicon fetching is completed, donates all tabs with favicons if
                        // exists.
                        if (mTaskFinishedCount == zeroStateFaviconFetchedNumber) {
                            AuxiliarySearchMetrics.recordFaviconFirstDonationCount(
                                    tabToFaviconMap.size());
                            AuxiliarySearchMetrics.recordQueryFaviconTime(
                                    TimeUtils.uptimeMillis() - faviconStartTimeMs);

                            if (!tabToFaviconMap.isEmpty()) {
                                mDonor.donateTabs(tabToFaviconMap, onDonationCompleteCallback);
                            }
                        }
                    });
        }

        int remainingFaviconFetchCount = tabs.size() - zeroStateFaviconFetchedNumber;
        if (mIsFaviconEnabled && remainingFaviconFetchCount > 0) {

            // Saves the metadata of tabs in a local file.
            mAuxiliarySearchProvider.saveTabMetadataToFile(
                    AuxiliarySearchUtils.getTabDonateFile(mContext),
                    tabs,
                    zeroStateFaviconFetchedNumber,
                    remainingFaviconFetchCount);

            // Schedules a background task to donate favicons of the remaining tabs.
            mAuxiliarySearchProvider.scheduleBackgroundTask(
                    sAndroidAppIntegrationWithFaviconScheduleDelayTimeMs.getValue(),
                    TimeUtils.uptimeMillis());
        }
    }

    private void deleteAllTabs() {
        long startTimeMs = TimeUtils.uptimeMillis();

        mHasDeletingTask = true;
        mDonor.deleteAllTabs(
                (success) -> {
                    mHasDeletingTask = false;
                    AuxiliarySearchMetrics.recordDeleteTime(
                            TimeUtils.uptimeMillis() - startTimeMs, AuxiliarySearchDataType.TAB);
                    AuxiliarySearchMetrics.recordDeletionRequestStatus(
                            success ? RequestStatus.SUCCESSFUL : RequestStatus.UNSUCCESSFUL,
                            AuxiliarySearchDataType.TAB);
                });
    }

    private void initDonor() {
        mDonor.createSessionAndInit();
    }
}
