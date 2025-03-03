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
    private final boolean mSupportMultiDataSource;
    private final int mZeroStateFaviconNumber;
    private final int mDefaultFaviconSize;

    private @NonNull ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private boolean mHasDeletingTask;
    private int mTaskFinishedCount;
    private CallbackController mCallbackController = new CallbackController();

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
        mSupportMultiDataSource =
                ChromeFeatureList.sAndroidAppIntegrationMultiDataSource.isEnabled();

        mZeroStateFaviconNumber =
                sAndroidAppIntegrationWithFaviconZeroStateFaviconNumber.getValue();
        mDefaultFaviconSize = AuxiliarySearchUtils.getFaviconSize(mContext.getResources());

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

        if (mActivityLifecycleDispatcher != null) {
            mActivityLifecycleDispatcher.unregister(this);
            mActivityLifecycleDispatcher = null;
        }

        mFaviconHelper.destroy();
    }

    @Override
    public <T> void onBackgroundTaskStart(
            @NonNull List<T> entries,
            @NonNull Map<T, Bitmap> entryToFaviconMap,
            @NonNull Callback<Boolean> callback,
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
        Callback<Boolean> onDonationCompleteCallback =
                (success) -> {
                    AuxiliarySearchMetrics.recordDonateTime(TimeUtils.uptimeMillis() - startTimeMs);
                    AuxiliarySearchMetrics.recordDonationRequestStatus(
                            success ? RequestStatus.SUCCESSFUL : RequestStatus.UNSUCCESSFUL);
                };

        // Donates the list of entries without favicons.
        mDonor.donateEntries(entries, onDonationCompleteCallback);

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
        AuxiliarySearchMetrics.recordQueryTabTime(TimeUtils.uptimeMillis() - startTimeMs);

        if (entries == null || entries.isEmpty()) return;

        onNonSensitiveDataAvailable(entries, startTimeMs);
    }

    private void deleteAllTabs() {
        long startTimeMs = TimeUtils.uptimeMillis();

        mHasDeletingTask = true;
        if (!mDonor.deleteAllTabs(
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
}
