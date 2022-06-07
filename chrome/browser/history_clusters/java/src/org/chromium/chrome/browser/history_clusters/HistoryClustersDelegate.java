// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import android.content.Intent;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.url.GURL;

/**
 * Provider of functionality that the HistoryClusters component can't or shouldn't implement
 * internally.
 */
public interface HistoryClustersDelegate {
    /** Returns whether the HistoryClusters UI is running in its own activity. */
    boolean isSeparateActivity();

    /**
     * Returns the currently selected tab, if any. {@code null} when not running in a separate
     * activity.
     */
    @Nullable
    Tab getTab();

    /** Returns an intent that opens the history activity. */
    @Nullable
    Intent getHistoryActivityIntent();

    /** Returns an intent that opens the given url in the correct main browsing activity. */
    @Nullable
    Intent getOpenUrlIntent(GURL gurl);

    /** Returns a toggle view that swaps between the Journeys UI and the "normal" History UI. */
    @Nullable
    ViewGroup getToggleView(ViewGroup parent);
}
