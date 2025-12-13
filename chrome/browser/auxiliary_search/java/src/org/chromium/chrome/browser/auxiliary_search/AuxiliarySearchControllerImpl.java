// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import android.content.Context;
import android.graphics.Bitmap;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.TimeUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchMetrics.RequestStatus;
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
@NullMarked
public class AuxiliarySearchControllerImpl
        implements AuxiliarySearchController,
                AuxiliarySearchConfigManager.ShareTabsWithOsStateListener {
    // 3 minutes in milliseconds.
    @VisibleForTesting static final long TIME_RANGE_MS = 3 * TimeUtils.MILLISECONDS_PER_MINUTE;
    private static final int ZERO_STATE_FAVICON_NUMBER = 5;

    protected final @AuxiliarySearchHostType int mHostType;
    protected final AuxiliarySearchProvider mAuxiliarySearchProvider;
    protected final Profile mProfile;

    private final Context mContext;
    private final FaviconHelper mFaviconHelper;
    private final AuxiliarySearchDonor mDonor;
    private final int mZeroStateFaviconNumber;
    private final int mDefaultFaviconSize;

    protected CallbackController mCallbackController = new CallbackController();

    private @Nullable ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private boolean mHasDeletingTask;
    private int mTaskFinishedCount;

    @VisibleForTesting
    public AuxiliarySearchControllerImpl(
            Context context,
            Profile profile,
            AuxiliarySearchProvider auxiliarySearchProvider,
            AuxiliarySearchDonor auxiliarySearchDonor,
            FaviconHelper faviconHelper,
            @AuxiliarySearchHostType int hostType) {
        mContext = context;
        mProfile = profile;
        mAuxiliarySearchProvider = auxiliarySearchProvider;
        mDonor = auxiliarySearchDonor;
        mFaviconHelper = faviconHelper;
        mHostType = hostType;

        mZeroStateFaviconNumber = ZERO_STATE_FAVICON_NUMBER;
        mDefaultFaviconSize = AuxiliarySearchUtils.getFaviconSize(mContext.getResources());

        AuxiliarySearchConfigManager.getInstance().addListener(this);
    }

    /**
     * @param context The application context.
     * @param profile The profile in use.
     * @param tabModelSelector The instance of {@link TabModelSelector}.
     * @param hostType The type of host who creates and owns the controller instance.
     */
    public AuxiliarySearchControllerImpl(
            Context context,
            Profile profile,
            @Nullable TabModelSelector tabModelSelector,
            @AuxiliarySearchHostType int hostType) {
        this(
                context,
                profile,
                new AuxiliarySearchProvider(context, profile, tabModelSelector, hostType),
                AuxiliarySearchDonor.getInstance(),
                new FaviconHelper(),
                hostType);
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
    @SuppressWarnings("NullAway")
    public void destroy(@Nullable ActivityLifecycleDispatcher lifecycleDispatcher) {
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
    public void donateCustomTabs(GURL url, long beginTime) {
        long startTime = TimeUtils.uptimeMillis();
        mAuxiliarySearchProvider.getCustomTabsAsync(
                // A backward time adjustment is required due to the history visit's timestamp being
                // earlier than that of the TabImpl's last visit timestamp.
                url,
                beginTime - TIME_RANGE_MS,
                mCallbackController.makeCancelable(
                        (entries) -> onNonSensitiveCustomTabsAvailable(entries, startTime)));
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
        tryDonateTabsImpl(startTime);
    }

    protected void tryDonateTabsImpl(long startTime) {
        mAuxiliarySearchProvider.getTabsSearchableDataProtoAsync(
                mCallbackController.makeCancelable(
                        (tabs) -> onNonSensitiveTabsAvailable(tabs, startTime)));
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

        tabs.sort(AuxiliarySearchProvider.sComparator);

        onNonSensitiveDataAvailable(tabs, startTimeMs, /* onDonationCompleteRunnable= */ null);
    }

    @VisibleForTesting
    <T> void onNonSensitiveCustomTabsAvailable(@Nullable List<T> entries, long startTimeMs) {
        AuxiliarySearchMetrics.recordQueryCustomTabTime(TimeUtils.uptimeMillis() - startTimeMs);

        if (entries == null || entries.isEmpty()) {
            AuxiliarySearchMetrics.recordCustomTabFetchResultsCount(0);
            return;
        }

        AuxiliarySearchMetrics.recordCustomTabFetchResultsCount(entries.size());
        onNonSensitiveDataAvailable(entries, startTimeMs, /* onDonationCompleteRunnable= */ null);
    }

    @VisibleForTesting
    <T> void onNonSensitiveDataAvailable(
            List<T> entries, long startTimeMs, @Nullable Runnable onDonationCompleteRunnable) {
        int[] counts = new int[AuxiliarySearchEntryType.MAX_VALUE + 1];
        Callback<Boolean> onDonationCompleteCallback =
                (success) -> {
                    // Only records total donate counts when all data's meta data are donated.
                    AuxiliarySearchMetrics.recordDonationCount(counts);
                    recordDonationTimeAndResults(startTimeMs, success);
                    if (onDonationCompleteRunnable != null) {
                        onDonationCompleteRunnable.run();
                    }
                };
        Callback<Boolean> onFaviconDonationCompleteCallback =
                (success) -> {
                    recordDonationTimeAndResults(startTimeMs, success);
                };

        // Donates the list of entries without favicons.
        mDonor.donateEntries(entries, counts, onDonationCompleteCallback);

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
                                mDonor.donateEntries(
                                        entryToFaviconMap, onFaviconDonationCompleteCallback);
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
            mAuxiliarySearchProvider.scheduleBackgroundTask(TimeUtils.uptimeMillis());
        }
    }

    private void recordDonationTimeAndResults(long startTimeMs, boolean success) {
        AuxiliarySearchMetrics.recordDonateTime(TimeUtils.uptimeMillis() - startTimeMs);
        AuxiliarySearchMetrics.recordDonationRequestStatus(
                success ? RequestStatus.SUCCESSFUL : RequestStatus.UNSUCCESSFUL);
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
}
