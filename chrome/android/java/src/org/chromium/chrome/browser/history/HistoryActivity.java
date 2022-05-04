// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.os.Bundle;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.BackPressHelper;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.SnackbarActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.history_clusters.HistoryClustersCoordinator;
import org.chromium.chrome.browser.history_clusters.HistoryClustersIntent;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Activity for displaying the browsing history manager.
 */
public class HistoryActivity extends SnackbarActivity {
    private HistoryManager mHistoryManager;
    private @Nullable HistoryClustersCoordinator mHistoryClustersCoordinator;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        boolean isIncognito = IntentUtils.safeGetBooleanExtra(
                getIntent(), IntentHandler.EXTRA_INCOGNITO_MODE, false);

        // TODO(https://crbug.com/1303171): Move history clusters feature flag and view switching
        // logic to HistoryManager in order to support the tablet UI; HistoryActivity is currently
        // only for phones.
        boolean historyClustersEnabled =
                ChromeFeatureList.isEnabled(ChromeFeatureList.HISTORY_JOURNEYS);
        boolean showHistoryClusters = false;
        if (historyClustersEnabled) {
            mHistoryClustersCoordinator = new HistoryClustersCoordinator(
                    Profile.getLastUsedRegularProfile(), this, null, null);
            showHistoryClusters = IntentUtils.safeGetBooleanExtra(
                    getIntent(), HistoryClustersIntent.EXTRA_SHOW_HISTORY_CLUSTERS, false);
            String query = IntentUtils.safeGetStringExtra(
                    getIntent(), HistoryClustersIntent.EXTRA_HISTORY_CLUSTERS_QUERY);
            if (!TextUtils.isEmpty(query)) {
                mHistoryClustersCoordinator.setQuery(query);
            }
        }

        mHistoryManager = new HistoryManager(
                this, true, getSnackbarManager(), isIncognito, /* Supplier<Tab>= */ null);
        BackPressHelper.create(this, getOnBackPressedDispatcher(), mHistoryManager::onBackPressed);

        View contentView = showHistoryClusters
                ? mHistoryClustersCoordinator.getActivityContentView()
                : mHistoryManager.getView();
        setContentView(contentView);
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
