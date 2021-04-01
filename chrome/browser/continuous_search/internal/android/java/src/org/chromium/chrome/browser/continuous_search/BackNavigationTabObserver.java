// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/**
 * A tab observer to record the number of back navigations to SRP
 */
public class BackNavigationTabObserver extends EmptyTabObserver {
    private GURL mLastVisitedUrl;
    private String mLastSrpUrlQuery;
    private @PageCategory int mResultCategory;
    private int mBackNavigationCount;
    private boolean mClickedOnResultLink;

    public BackNavigationTabObserver(Tab tab) {
        tab.addObserver(this);
        mLastVisitedUrl = null;
        mLastSrpUrlQuery = null;
        mBackNavigationCount = 0;
        mClickedOnResultLink = false;
    }

    @Override
    public void onPageLoadFinished(Tab tab, GURL url) {
        // Ignore page reloads
        if (url.equals(mLastVisitedUrl)) return;

        mLastVisitedUrl = url;

        String query = SearchUrlHelper.getQueryIfValidSrpUrl(url);
        if (query != null) {
            @PageCategory
            int resultCategory = SearchUrlHelper.getSrpPageCategoryFromUrl(url);
            if (query.equals(mLastSrpUrlQuery) && resultCategory == mResultCategory) {
                // Treat re-navigation to the last seen SRP as a back navigation.
                mBackNavigationCount++;
            } else {
                // Encountered a new SRP session. Record the previous session and start a new one.
                recordMetricAndClearCache();
                mLastSrpUrlQuery = query;
                mResultCategory = resultCategory;
            }
        } else {
            checkIfSrpResultClicked(tab);
        }
    }

    @Override
    public void onContentChanged(Tab tab) {
        if (tab.isNativePage()) {
            recordMetricAndClearCache();
        }
    }

    @Override
    public void onHidden(Tab tab, int reason) {
        if (reason == TabHidingType.ACTIVITY_HIDDEN) {
            recordMetricAndClearCache();
        }
    }

    @Override
    public void onDestroyed(Tab tab) {
        recordMetricAndClearCache();
        tab.removeObserver(this);
    }

    @Override
    public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
        // Intentionally do nothing to prevent automatic observer removal on detachment.
    }

    public void checkIfSrpResultClicked(Tab tab) {
        if (tab.getWebContents() == null) return;
        NavigationHistory history =
                tab.getWebContents().getNavigationController().getNavigationHistory();
        // Some tests don't mock Tab completely and leave NavigationHistory with no entries.
        // This check here is to prevent crashing in that scenario.
        if (history.getEntryCount() == 0) return;

        NavigationEntry entry = history.getEntryAtIndex(history.getCurrentEntryIndex());
        // A page opened through SRP has google.com as its referrer URL.
        if (SearchUrlHelper.isGoogleDomainUrl(entry.getReferrerUrl())) {
            mClickedOnResultLink = true;
        }
    }

    private void recordMetricAndClearCache() {
        // Record if seen a SRP or it was not abandoned (clicked on at least one result)
        if (mLastSrpUrlQuery != null && mClickedOnResultLink) {
            RecordHistogram.recordCount100Histogram("Browser.ContinuousSearch.BackNavigationToSrp"
                            + SearchUrlHelper.getHistogramSuffixForPageCategory(mResultCategory),
                    mBackNavigationCount);
        }
        mLastSrpUrlQuery = null;
        mBackNavigationCount = 0;
        mClickedOnResultLink = false;
    }
}
