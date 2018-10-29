// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.metrics;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.download.home.filter.FilterCoordinator;
import org.chromium.chrome.browser.download.home.filter.Filters.FilterType;
import org.chromium.chrome.browser.download.ui.DownloadFilter;

/**
 * Helper class to log filter changes as the occur.
 *
 * Note that there are some caveats to when this method is notified of changes (e.g. when forcefully
 * setting the selected tab from an external source this may not get called).  These can be
 * addressed as a follow up.
 */
public class FilterChangeLogger implements FilterCoordinator.Observer {
    // FilterCoordinator.Observer implementation.
    @Override
    public void onFilterChanged(@FilterType int selectedTab) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.DownloadManager.Filter", selectedTab, DownloadFilter.Type.NUM_ENTRIES);
    }
}