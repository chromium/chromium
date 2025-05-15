// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.sAndroidAppIntegrationMultiDataSourceHistoryContentTtlHours;

import android.content.Context;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.TimeUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/**
 * A controller which donates Tabs of all windows, CCTs and top sites for the auxiliary search. One
 * controller is shared among multiple ChromeTabbedActivity(s), and it only donates once when
 * multiple ChromeTabbedActivity(s) are going to the background.
 */
@NullMarked
public class AuxiliarySearchMultiDataControllerImpl extends AuxiliarySearchControllerImpl
        implements AuxiliarySearchTopSiteProviderBridge.Observer {
    private final long mHistoryTtlMillis;

    // Whether this controller is observing most visited sites.
    private boolean mIsObservingTopSites;
    private long mTopSiteLastFetchTimestamp;

    // A flag to prevent donating multiple times when Chrome is on paused with multiple windows.
    // Only donating when mExpectDonating equals true, and sets it false once a donation starts.
    // This flag is set to true in onResumeWithNative() and after a donation is completed.
    private boolean mExpectDonating;

    // A set of ActivityLifecycleDispatcher that this controller tracks.
    private final Set<ActivityLifecycleDispatcher> mActivityLifecycleDispatcherSet;

    // It is null when the controller doesn't observe top sites changes.
    private @Nullable AuxiliarySearchTopSiteProviderBridge mAuxiliarySearchTopSiteProviderBridge;
    private @Nullable List<AuxiliarySearchDataEntry> mCurrentSiteSuggestionEntries;

    /**
     * @param context The application context.
     * @param profile The profile in use.
     * @param hostType The type of host who creates and owns the controller instance.
     */
    public AuxiliarySearchMultiDataControllerImpl(
            Context context, Profile profile, @AuxiliarySearchHostType int hostType) {
        this(
                context,
                profile,
                new AuxiliarySearchProvider(
                        context, profile, /* tabModelSelector= */ null, hostType),
                AuxiliarySearchDonor.getInstance(),
                new FaviconHelper(),
                hostType,
                new AuxiliarySearchTopSiteProviderBridge(profile));
    }

    @VisibleForTesting
    public AuxiliarySearchMultiDataControllerImpl(
            Context context,
            Profile profile,
            AuxiliarySearchProvider auxiliarySearchProvider,
            AuxiliarySearchDonor auxiliarySearchDonor,
            FaviconHelper faviconHelper,
            @AuxiliarySearchHostType int hostType,
            AuxiliarySearchTopSiteProviderBridge auxiliarySearchTopSiteProviderBridge) {
        super(
                context,
                profile,
                auxiliarySearchProvider,
                auxiliarySearchDonor,
                faviconHelper,
                hostType);

        mAuxiliarySearchTopSiteProviderBridge = auxiliarySearchTopSiteProviderBridge;
        mExpectDonating = true;
        mHistoryTtlMillis =
                TimeUnit.HOURS.toMillis(
                        sAndroidAppIntegrationMultiDataSourceHistoryContentTtlHours.getValue());
        mActivityLifecycleDispatcherSet = new HashSet<>();
    }

    @Override
    public void onResumeWithNative() {
        super.onResumeWithNative();
        mExpectDonating = true;
    }

    @Override
    protected void tryDonateTabsImpl(long startTime) {
        // This function can be called multiple times when multiple ChromeTabbedActivity are
        // onPause(). However, we only want to donate once since mAuxiliarySearchMultiDataProvider
        // checks all open TabModels, and CCTs and MVTs.
        if (!mExpectDonating) return;

        mExpectDonating = false;
        mAuxiliarySearchProvider.getHistorySearchableDataProtoAsync(
                mCallbackController.makeCancelable(
                        (entries) ->
                                onNonSensitiveHistoryDataAvailable(
                                        entries, startTime, this::onDonationComplete)));
    }

    private void onDonationComplete() {
        // Setting mExpectDonating to true after metadata entries are donated unblocks the donation
        // process. This is crucial if multiple windows are closed or backgrounded sequentially
        // without onResumeWithNative() being called in the interim.
        mExpectDonating = true;
    }

    // AuxiliarySearchController implementations.
    @Override
    public void register(ActivityLifecycleDispatcher lifecycleDispatcher) {
        if (lifecycleDispatcher == null) return;

        lifecycleDispatcher.register(this);
        mActivityLifecycleDispatcherSet.add(lifecycleDispatcher);
    }

    @Override
    public void onDeferredStartup() {
        if (mHostType == AuxiliarySearchHostType.CTA && !mIsObservingTopSites) {
            mIsObservingTopSites = true;
            if (mAuxiliarySearchTopSiteProviderBridge == null) {
                mAuxiliarySearchTopSiteProviderBridge =
                        new AuxiliarySearchTopSiteProviderBridge(mProfile);
            }
            mAuxiliarySearchTopSiteProviderBridge.setObserver(this);
        }
    }

    @Override
    public void destroy(@Nullable ActivityLifecycleDispatcher lifecycleDispatcher) {
        if (lifecycleDispatcher != null) {
            lifecycleDispatcher.unregister(this);
            mActivityLifecycleDispatcherSet.remove(lifecycleDispatcher);
        }

        if (mActivityLifecycleDispatcherSet.isEmpty()) {
            if (mIsObservingTopSites) {
                assumeNonNull(mAuxiliarySearchTopSiteProviderBridge);
                mAuxiliarySearchTopSiteProviderBridge.destroy();
                mAuxiliarySearchTopSiteProviderBridge = null;
                mIsObservingTopSites = false;
            }
        }
    }

    // AuxiliarySearchProvider.Observer implementations.
    @Override
    public void onSiteSuggestionsAvailable(@Nullable List<AuxiliarySearchDataEntry> entries) {
        mCurrentSiteSuggestionEntries = entries;
        mTopSiteLastFetchTimestamp = TimeUtils.uptimeMillis();
    }

    @Override
    public void onIconMadeAvailable(GURL siteUrl) {}

    /**
     * Called when a list of up to 100 non sensitive entries is available.
     *
     * @param entries A list of non sensitive entries.
     * @param startTimeMs The starting time to query the data.
     * @param onDonationCompleteRunnable The callback to be called when the donation of metadata is
     *     completed.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public void onNonSensitiveHistoryDataAvailable(
            @Nullable List<AuxiliarySearchDataEntry> entries,
            long startTimeMs,
            Runnable onDonationCompleteRunnable) {
        AuxiliarySearchMetrics.recordQueryHistoryDataTime(TimeUtils.uptimeMillis() - startTimeMs);

        List<AuxiliarySearchDataEntry> donationList = getMergedList(entries);
        if (donationList == null || donationList.isEmpty()) {
            onDonationCompleteRunnable.run();
            return;
        }

        onNonSensitiveDataAvailable(donationList, startTimeMs, onDonationCompleteRunnable);
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

        // It is possible that mCurrentSiteSuggestionEntries has duplicated URLs which are also in
        // the historyEntryList, filters out from the mCurrentSiteSuggestionEntries now.
        Set<GURL> urlSet = new HashSet<>();
        for (var entry : historyEntryList) {
            urlSet.add(entry.url);
        }
        List<AuxiliarySearchDataEntry> refinedSiteSuggestionEntries = new ArrayList<>();
        for (var entry : mCurrentSiteSuggestionEntries) {
            if (!urlSet.contains(entry.url)) {
                refinedSiteSuggestionEntries.add(entry);
            }
        }

        // Adds the most visited site suggestion with the highest score as the first one in
        // tht list to donate. This allows to include at least one most visited site
        // suggestion in the first five entries to fetch icons.
        if (refinedSiteSuggestionEntries.size() >= 1) {
            donationList.add(refinedSiteSuggestionEntries.get(0));
        }
        // Adds the Tabs and Custom Tabs.
        donationList.addAll(historyEntryList);
        // Adds the remaining most visited sites suggestions.
        for (int i = 1; i < refinedSiteSuggestionEntries.size(); i++) {
            donationList.add(refinedSiteSuggestionEntries.get(i));
        }

        urlSet.clear();
        return donationList;
    }

    void setExpectDonatingForTesting(boolean expectDonating) {
        mExpectDonating = expectDonating;
    }

    boolean getExpectDonatingForTesting() {
        return mExpectDonating;
    }

    @Nullable
    AuxiliarySearchTopSiteProviderBridge getAuxiliarySearchTopSiteProviderBridgeForTesting() {
        return mAuxiliarySearchTopSiteProviderBridge;
    }
}
