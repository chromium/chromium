// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.sAndroidAppIntegrationMultiDataSourceHistoryContentTtlHours;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.sAndroidAppIntegrationWithFaviconScheduleDelayTimeMs;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.sAndroidAppIntegrationWithFaviconZeroStateFaviconNumber;

import android.content.Context;
import android.graphics.Bitmap;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.TimeUtils;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchMetrics.RequestStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.TimeUnit;

/** The Controller to handle the communication between Chrome and {@link AuxiliarySearchDonor}. */
public class AuxiliarySearchControllerImpl
        implements AuxiliarySearchController,
                AuxiliarySearchConfigManager.ShareTabsWithOsStateListener,
                AuxiliarySearchProvider.Observer {
    private static final String TAG = "AuxiliarySearch";
    private final Context mContext;
    private final Profile mProfile;
    private final FaviconHelper mFaviconHelper;
    private final AuxiliarySearchProvider mAuxiliarySearchProvider;
    private final AuxiliarySearchDonor mDonor;
    private final boolean mIsFaviconEnabled;
    private final boolean mSupportMultiDataSource;
    private final int mZeroStateFaviconNumber;
    private final int mDefaultFaviconSize;
    private final long mHistoryTtlMillis;

    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private boolean mHasDeletingTask;
    private int mTaskFinishedCount;
    private boolean mIsObserving;
    private CallbackController mCallbackController = new CallbackController();
    private long mTopSiteLastFetchTimestamp;
    @Nullable private List<AuxiliarySearchDataEntry> mCurrentSiteSuggestionEntries;

    @VisibleForTesting
    public AuxiliarySearchControllerImpl(
            Context context,
            Profile profile,
            AuxiliarySearchProvider auxiliarySearchProvider,
            AuxiliarySearchDonor auxiliarySearchDonor,
            FaviconHelper faviconHelper) {
        mContext = context;
        mProfile = profile;
        mAuxiliarySearchProvider = auxiliarySearchProvider;
        mDonor = auxiliarySearchDonor;
        mFaviconHelper = faviconHelper;
        mIsFaviconEnabled = ChromeFeatureList.sAndroidAppIntegrationWithFavicon.isEnabled();
        mSupportMultiDataSource =
                AuxiliarySearchControllerFactory.getInstance().isMultiDataTypeEnabledOnDevice();

        mZeroStateFaviconNumber =
                sAndroidAppIntegrationWithFaviconZeroStateFaviconNumber.getValue();
        mDefaultFaviconSize = AuxiliarySearchUtils.getFaviconSize(mContext.getResources());
        mHistoryTtlMillis =
                TimeUnit.HOURS.toMillis(
                        sAndroidAppIntegrationMultiDataSourceHistoryContentTtlHours.getValue());

        AuxiliarySearchConfigManager.getInstance().addListener(this);
    }

    /**
     * @param context The application context.
     * @param profile The profile in use.
     * @param tabModelSelector The instance of {@link TabModelSelector}.
     */
    public AuxiliarySearchControllerImpl(
            Context context, Profile profile, @Nullable TabModelSelector tabModelSelector) {
        this(
                context,
                profile,
                new AuxiliarySearchProvider(context, profile, tabModelSelector),
                AuxiliarySearchDonor.getInstance(),
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
        deleteAllTabs();
    }

    @Override
    public void onPauseWithNative() {
        tryDonateTabs();
    }

    @Override
    public void destroy() {
        if (mCallbackController == null) return;

        mCallbackController.destroy();
        mCallbackController = null;
        AuxiliarySearchConfigManager.getInstance().removeListener(this);
        if (mIsObserving) {
            mAuxiliarySearchProvider.setObserver(null);
            mIsObserving = false;
        }

        if (mActivityLifecycleDispatcher != null) {
            mActivityLifecycleDispatcher.unregister(this);
            mActivityLifecycleDispatcher = null;
        }

        mFaviconHelper.destroy();
    }

    @Override
    public <T> void onBackgroundTaskStart(
            List<T> entries,
            Map<T, Bitmap> entryToFaviconMap,
            Callback<Boolean> callback,
            long startTimeMillis) {
        if (!mDonor.canDonate()) return;

        // mDonor will cache the donation list if the initialization of the donor is in progress.
        mDonor.donateFavicons(
                entries,
                entryToFaviconMap,
                (success) -> {
                    callback.onResult(success);
                    AuxiliarySearchMetrics.recordScheduledDonateTime(
                            TimeUtils.uptimeMillis() - startTimeMillis);
                });
    }

    @Override
    public void onDeferredStartup() {
        if (mSupportMultiDataSource && !mIsObserving) {
            mIsObserving = true;
            mAuxiliarySearchProvider.setObserver(this);
        }
    }

    // AuxiliarySearchConfigManager.ShareTabsWithOsStateListener implementations.
    @Override
    public void onConfigChanged(boolean enabled) {
        long startTimeMs = TimeUtils.uptimeMillis();
        mDonor.onConfigChanged(enabled, (success) -> onAllTabDeleted(success, startTimeMs));
    }

    private void tryDonateTabs() {
        if (mHasDeletingTask || !mDonor.canDonate()) return;

        long startTime = TimeUtils.uptimeMillis();
        if (mSupportMultiDataSource) {
            mAuxiliarySearchProvider.getHistorySearchableDataProtoAsync(
                    mCallbackController.makeCancelable(
                            (entries) -> onNonSensitiveHistoryDataAvailable(entries, startTime)));
        } else {
            mAuxiliarySearchProvider.getTabsSearchableDataProtoAsync(
                    mCallbackController.makeCancelable(
                            (tabs) -> onNonSensitiveTabsAvailable(tabs, startTime)));
        }
    }

    /**
     * Called when a list of up to 100 non sensitive Tabs is available.
     *
     * @param tabs A list of non sensitive Tabs.
     * @param startTimeMs The starting time to query the tab list.
     */
    @VisibleForTesting
    public void onNonSensitiveTabsAvailable(@Nullable List<Tab> tabs, long startTimeMs) {
        AuxiliarySearchMetrics.recordQueryTabTime(TimeUtils.uptimeMillis() - startTimeMs);

        if (tabs == null || tabs.isEmpty()) return;

        if (mIsFaviconEnabled) {
            tabs.sort(AuxiliarySearchProvider.sComparator);
        }

        onNonSensitiveDataAvailable(tabs, startTimeMs);
    }

    @VisibleForTesting
    <T> void onNonSensitiveDataAvailable(List<T> entries, long startTimeMs) {
        int[] counts = new int[AuxiliarySearchEntryType.MAX_VALUE + 1];
        Callback<Boolean> onDonationCompleteCallback =
                (success) -> {
                    AuxiliarySearchMetrics.recordDonationCount(counts);
                    AuxiliarySearchMetrics.recordDonateTime(TimeUtils.uptimeMillis() - startTimeMs);
                    AuxiliarySearchMetrics.recordDonationRequestStatus(
                            success ? RequestStatus.SUCCESSFUL : RequestStatus.UNSUCCESSFUL);
                };

        // Donates the list of entries without favicons.
        mDonor.donateEntries(entries, counts, onDonationCompleteCallback);

        if (!mIsFaviconEnabled) {
            return;
        }

        mTaskFinishedCount = 0;
        Map<T, Bitmap> entryToFaviconMap = new HashMap<>();
        int zeroStateFaviconFetchedNumber = Math.min(entries.size(), mZeroStateFaviconNumber);

        long faviconStartTimeMs = TimeUtils.uptimeMillis();
        int metaDataVersion = AuxiliarySearchUtils.getMetadataVersion(entries.get(0));

        // When donating favicon is enabled, Chrome only donates the favicons of the most
        // recently visited tabs in the first round.
        for (int i = 0; i < zeroStateFaviconFetchedNumber; i++) {
            T entry = entries.get(i);

            GURL entryUrl;
            if (entry instanceof Tab tab) {
                entryUrl = tab.getUrl();
            } else {
                entryUrl = ((AuxiliarySearchDataEntry) entry).url;
            }
            mFaviconHelper.getLocalFaviconImageForURL(
                    mProfile,
                    entryUrl,
                    mDefaultFaviconSize,
                    (image, url) -> {
                        mTaskFinishedCount++;
                        if (image != null) {
                            entryToFaviconMap.put(entry, image);
                        }

                        // Once all favicon fetching is completed, donates all entries with favicons
                        // if exists.
                        if (mTaskFinishedCount == zeroStateFaviconFetchedNumber) {
                            AuxiliarySearchMetrics.recordFaviconFirstDonationCount(
                                    entryToFaviconMap.size());
                            AuxiliarySearchMetrics.recordQueryFaviconTime(
                                    TimeUtils.uptimeMillis() - faviconStartTimeMs);

                            if (!entryToFaviconMap.isEmpty()) {
                                mDonor.donateEntries(entryToFaviconMap, onDonationCompleteCallback);
                            }
                        }
                    });
        }

        int remainingFaviconFetchCount = entries.size() - zeroStateFaviconFetchedNumber;
        if (remainingFaviconFetchCount > 0) {

            // Saves the metadata of entries in a local file.
            mAuxiliarySearchProvider.saveTabMetadataToFile(
                    AuxiliarySearchUtils.getTabDonateFile(mContext),
                    metaDataVersion,
                    entries,
                    zeroStateFaviconFetchedNumber,
                    remainingFaviconFetchCount);

            // Schedules a background task to donate favicons of the remaining entries.
            mAuxiliarySearchProvider.scheduleBackgroundTask(
                    sAndroidAppIntegrationWithFaviconScheduleDelayTimeMs.getValue(),
                    TimeUtils.uptimeMillis());
        }
    }

    /**
     * Called when a list of up to 100 non sensitive entries is available.
     *
     * @param entries A list of non sensitive entries.
     * @param startTimeMs The starting time to query the data.
     */
    @VisibleForTesting
    public void onNonSensitiveHistoryDataAvailable(
            @Nullable List<AuxiliarySearchDataEntry> entries, long startTimeMs) {
        AuxiliarySearchMetrics.recordQueryHistoryDataTime(TimeUtils.uptimeMillis() - startTimeMs);

        List<AuxiliarySearchDataEntry> donationList = getMergedList(entries);
        if (donationList == null || donationList.isEmpty()) return;

        onNonSensitiveDataAvailable(donationList, startTimeMs);
    }

    /** Merges the fetched list of Tabs and CCTs with list of the most visited sites together. */
    @VisibleForTesting
    @Nullable
    List<AuxiliarySearchDataEntry> getMergedList(
            @Nullable List<AuxiliarySearchDataEntry> historyEntryList) {
        if (historyEntryList == null && mCurrentSiteSuggestionEntries == null) return null;

        if (mCurrentSiteSuggestionEntries == null || mCurrentSiteSuggestionEntries.isEmpty()) {
            return historyEntryList;
        }

        // Don't donate most visited sites if they were calculated 24 hours ago.
        long topSiteExpirationDuration =
                TimeUtils.uptimeMillis() - mTopSiteLastFetchTimestamp - mHistoryTtlMillis;
        if (topSiteExpirationDuration > 0) {
            AuxiliarySearchMetrics.recordTopSiteExpirationDuration(topSiteExpirationDuration);
            return historyEntryList;
        }

        List<AuxiliarySearchDataEntry> donationList = new ArrayList<>();
        if (historyEntryList == null || historyEntryList.isEmpty()) {
            donationList.addAll(mCurrentSiteSuggestionEntries);
            return donationList;
        }

        // Adds the most visited site suggestion with the highest score as the first one in
        // tht list to donate. This allows to include at least one most visited site
        // suggestion in the first five entries to fetch icons.
        donationList.add(mCurrentSiteSuggestionEntries.get(0));
        // Adds the Tabs and Custom Tabs.
        donationList.addAll(historyEntryList);
        // Adds the remaining most visited sites suggestions.
        for (int i = 1; i < mCurrentSiteSuggestionEntries.size(); i++) {
            donationList.add(mCurrentSiteSuggestionEntries.get(i));
        }
        return donationList;
    }

    private void deleteAllTabs() {
        long startTimeMs = TimeUtils.uptimeMillis();

        mHasDeletingTask = true;
        if (!mDonor.deleteAll(
                (success) -> {
                    onAllTabDeleted(success, startTimeMs);
                })) {
            mHasDeletingTask = false;
        }
    }

    private void onAllTabDeleted(boolean success, long startTimeMs) {
        mHasDeletingTask = false;
        AuxiliarySearchMetrics.recordDeleteTime(
                TimeUtils.uptimeMillis() - startTimeMs, AuxiliarySearchDataType.TAB);
        AuxiliarySearchMetrics.recordDeletionRequestStatus(
                success ? RequestStatus.SUCCESSFUL : RequestStatus.UNSUCCESSFUL,
                AuxiliarySearchDataType.TAB);
    }

    public boolean getHasDeletingTaskForTesting() {
        return mHasDeletingTask;
    }

    // AuxiliarySearchProvider.Observer implementations.
    @Override
    public void onSiteSuggestionsAvailable(@Nullable List<AuxiliarySearchDataEntry> entries) {
        mCurrentSiteSuggestionEntries = entries;
        mTopSiteLastFetchTimestamp = TimeUtils.uptimeMillis();
    }

    @Override
    public void onIconMadeAvailable(GURL siteUrl) {}
}
