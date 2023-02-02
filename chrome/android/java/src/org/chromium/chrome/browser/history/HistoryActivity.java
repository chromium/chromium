// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.os.Bundle;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.BackPressHelper;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.SnackbarActivity;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.history_clusters.HistoryClustersConstants;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Activity for displaying the browsing history manager.
 */
public class HistoryActivity extends SnackbarActivity {
    private HistoryManager mHistoryManager;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        boolean isIncognito = IntentUtils.safeGetBooleanExtra(
                getIntent(), IntentHandler.EXTRA_INCOGNITO_MODE, false);
        boolean showHistoryClustersImmediately = IntentUtils.safeGetBooleanExtra(
                getIntent(), HistoryClustersConstants.EXTRA_SHOW_HISTORY_CLUSTERS, false);
        String historyClustersQuery = IntentUtils.safeGetStringExtra(
                getIntent(), HistoryClustersConstants.EXTRA_HISTORY_CLUSTERS_QUERY);
        mHistoryManager = new HistoryManager(this, true, getSnackbarManager(), isIncognito,
                /* Supplier<Tab>= */ null, showHistoryClustersImmediately, historyClustersQuery,
                new BrowsingHistoryBridge(Profile.getLastUsedRegularProfile()));
        setContentView(mHistoryManager.getView());
        if (BackPressManager.isSecondaryActivityEnabled()) {
            BackPressHelper.create(this, getOnBackPressedDispatcher(), mHistoryManager);
        } else {
            BackPressHelper.create(
                    this, getOnBackPressedDispatcher(), mHistoryManager::onBackPressed);
        }
    }

    @Override
    protected void onDestroy() {
        mHistoryManager.onDestroyed();
        mHistoryManager = null;
        super.onDestroy();
    }

    @VisibleForTesting
    HistoryManager getHistoryManagerForTests() {
        return mHistoryManager;
    }
}
