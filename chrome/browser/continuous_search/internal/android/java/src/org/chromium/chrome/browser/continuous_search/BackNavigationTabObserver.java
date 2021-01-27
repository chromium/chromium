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
    private GURL mLastSrpUrl;
    private GURL mLastVisitedUrl;
    private int mBackNavigationCount;

    public BackNavigationTabObserver(Tab tab) {
        tab.addObserver(this);
        mLastSrpUrl = null;
        mLastVisitedUrl = null;
        mBackNavigationCount = 0;
    }

    @Override
    public void onPageLoadFinished(Tab tab, GURL url) {
        // Ignore page reloads
        if (url.equals(mLastVisitedUrl)) return;

        if (tab.getWebContents() == null) return;
        NavigationHistory history =
                tab.getWebContents().getNavigationController().getNavigationHistory();
        // Some tests don't mock Tab completely and leave NavigationHistory with no entries.
        // This check here is to prevent crashing in that scenario.
        if (history.getEntryCount() == 0) return;

        NavigationEntry entry = history.getEntryAtIndex(history.getCurrentEntryIndex());
        if (SearchUrlHelper.isSrpUrl(entry.getUrl())) {
            if (entry.getUrl().equals(mLastSrpUrl)) {
                // Treat re-navigation to the last seen SRP as a back navigation.
                mBackNavigationCount++;
            } else {
                // Encountered a new SRP session. Record the previous session and start a new one.
                recordMetricAndClearCache();
                mLastSrpUrl = entry.getUrl();
            }
            // A page opened through SRP has google.com as its referrer URL. Treat other cases as
            // navigating away from the SRP session.
        } else if (!SearchUrlHelper.isGoogleDomainUrl(entry.getReferrerUrl())) {
            recordMetricAndClearCache();
        }
        mLastVisitedUrl = entry.getUrl();
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

    private void recordMetricAndClearCache() {
        if (mLastSrpUrl != null) {
            RecordHistogram.recordCount100Histogram(
                    "Browser.ContinuousSearch.BackNavigationToSrp", mBackNavigationCount);
        }
        mLastSrpUrl = null;
        mBackNavigationCount = 0;
    }
}
