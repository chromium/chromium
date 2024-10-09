// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import android.content.Context;
import android.graphics.Bitmap;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchBookmarkGroup;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchEntry;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchTabGroup;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.components.cached_flags.IntCachedFieldTrialParameter;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** This class provides information for the auxiliary search. */
public class AuxiliarySearchProvider {
    /** The callback interface to get results from fetching a favicon. * */
    public interface FaviconImageFetchedCallback {
        /** This method will be called when the result favicon is ready. */
        void onFaviconAvailable(Bitmap image, AuxiliarySearchEntry entry);
    }

    private static final int kNumTabsToSend = 100;

    /* Only donate the recent 7 days accessed tabs.*/
    @VisibleForTesting static final String TAB_AGE_HOURS_PARAM = "tabs_max_hours";
    @VisibleForTesting static final int DEFAULT_TAB_AGE_HOURS = 168;
    @VisibleForTesting static final int DEFAULT_FAVICON_NUMBER = 5;

    /**
     * A comparator to sort Tabs with timestamp descending, i.e., the most recent tab comes first.
     */
    @VisibleForTesting
    static Comparator<Tab> sComparator =
            (tab1, tab2) -> {
                long delta = tab1.getTimestampMillis() - tab2.getTimestampMillis();
                return (int) -Math.signum((float) delta);
            };

    private static final String MAX_FAVICON_NUMBER_PARAM = "max_favicon_number";
    public static final IntCachedFieldTrialParameter MAX_FAVICON_NUMBER =
            ChromeFeatureList.newIntCachedFieldTrialParameter(
                    ChromeFeatureList.ANDROID_APP_INTEGRATION_WITH_FAVICON,
                    MAX_FAVICON_NUMBER_PARAM,
                    DEFAULT_FAVICON_NUMBER);

    private final Profile mProfile;
    private final AuxiliarySearchBridge mAuxiliarySearchBridge;
    private final TabModelSelector mTabModelSelector;
    private final @NonNull FaviconHelper mFaviconHelper;
    private final int mDefaultFaviconSize;
    private final boolean mIsFaviconEnabled;
    private final int mMaxFaviconNumber;
    private Long mTabMaxAgeMillis;

    public AuxiliarySearchProvider(
            @NonNull Context context,
            @NonNull Profile profile,
            @NonNull TabModelSelector tabModelSelector) {
        mProfile = profile;
        mAuxiliarySearchBridge = new AuxiliarySearchBridge(mProfile);
        mTabModelSelector = tabModelSelector;
        mTabMaxAgeMillis = getTabsMaxAgeMs();
        mFaviconHelper = new FaviconHelper();
        mDefaultFaviconSize =
                context.getResources().getDimensionPixelSize(R.dimen.tab_grid_favicon_size);
        mIsFaviconEnabled = ChromeFeatureList.sAndroidAppIntegrationWithFavicon.isEnabled();
        mMaxFaviconNumber = MAX_FAVICON_NUMBER.getValue();
    }

    /**
     * @return AuxiliarySearchGroup for bookmarks.
     */
    public AuxiliarySearchBookmarkGroup getBookmarksSearchableDataProto() {
        return mAuxiliarySearchBridge.getBookmarksSearchableData();
    }

    /**
     * @param callback {@link Callback} to pass back the AuxiliarySearchGroup for {@link Tab}s with
     *     favicons.
     * @param faviconImageFetchedCallback The callback to be called when the fetching of favicon is
     *     complete.
     */
    public void getTabsSearchableDataProtoWithFaviconAsync(
            @NonNull Callback<AuxiliarySearchTabGroup> callback,
            @Nullable FaviconImageFetchedCallback faviconImageFetchedCallback) {
        long minAccessTime = System.currentTimeMillis() - mTabMaxAgeMillis;
        List<Tab> listTab = getTabsByMinimalAccessTime(minAccessTime);

        mAuxiliarySearchBridge.getNonSensitiveTabs(
                listTab,
                tabs -> {
                    onNonSensitiveTabsAvailable(
                            mFaviconHelper, callback, faviconImageFetchedCallback, tabs);
                });
    }

    @VisibleForTesting
    void onNonSensitiveTabsAvailable(
            @NonNull FaviconHelper faviconHelper,
            @NonNull Callback<AuxiliarySearchTabGroup> callback,
            @Nullable FaviconImageFetchedCallback faviconImageFetchedCallback,
            @NonNull List<Tab> tabs) {
        var tabGroupBuilder = AuxiliarySearchTabGroup.newBuilder();

        int count = 0;
        if (mIsFaviconEnabled) {
            tabs.sort(sComparator);
        }

        for (Tab tab : tabs) {
            AuxiliarySearchEntry entry = tabToAuxiliarySearchEntry(tab);
            if (entry != null) {
                tabGroupBuilder.addTab(entry);
                if (!mIsFaviconEnabled || count >= mMaxFaviconNumber) continue;

                count++;
                faviconHelper.getLocalFaviconImageForURL(
                        mProfile,
                        tab.getUrl(),
                        mDefaultFaviconSize,
                        (image, url) -> {
                            if (faviconImageFetchedCallback != null) {
                                faviconImageFetchedCallback.onFaviconAvailable(image, entry);
                            }
                        });
            }
        }
        callback.onResult(tabGroupBuilder.build());
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static @Nullable AuxiliarySearchEntry tabToAuxiliarySearchEntry(@Nullable Tab tab) {
        if (tab == null) {
            return null;
        }

        String title = tab.getTitle();
        GURL url = tab.getUrl();
        if (TextUtils.isEmpty(title) || url == null || !url.isValid()) return null;

        var tabBuilder = AuxiliarySearchEntry.newBuilder().setTitle(title).setUrl(url.getSpec());
        final long lastAccessTime = tab.getTimestampMillis();
        if (lastAccessTime != Tab.INVALID_TIMESTAMP) {
            tabBuilder.setLastAccessTimestamp(lastAccessTime);
        }

        return tabBuilder.build();
    }

    /**
     * @param minAccessTime specifies the earliest access time for a tab to be included in the
     *     returned list.
     * @return List of {@link Tab} which is accessed after 'minAccessTime'.
     */
    @VisibleForTesting
    @NonNull
    List<Tab> getTabsByMinimalAccessTime(long minAccessTime) {
        TabList allTabs = mTabModelSelector.getModel(false).getComprehensiveModel();
        List<Tab> recentAccessedTabs = new ArrayList<>();

        for (int i = 0; i < allTabs.getCount(); i++) {
            Tab tab = allTabs.getTabAt(i);
            if (tab.getTimestampMillis() >= minAccessTime) {
                recentAccessedTabs.add(allTabs.getTabAt(i));
            }
        }

        return recentAccessedTabs;
    }

    /** Returns the donated tab's max age in MS. */
    @VisibleForTesting
    long getTabsMaxAgeMs() {
        int configuredTabMaxAgeHrs =
                ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                        ChromeFeatureList.ANDROID_APP_INTEGRATION, TAB_AGE_HOURS_PARAM, 0);
        if (configuredTabMaxAgeHrs == 0) configuredTabMaxAgeHrs = DEFAULT_TAB_AGE_HOURS;
        return TimeUnit.HOURS.toMillis(configuredTabMaxAgeHrs);
    }
}
