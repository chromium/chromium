// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import android.text.TextUtils;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tabmodel.AccumulatingTabCreator;
import org.chromium.chrome.browser.tabmodel.AccumulatingTabCreator.CreateFrozenTabArguments;
import org.chromium.chrome.browser.tabmodel.PersistentStoreMigrationManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabPersistentStoreObserver;

/**
 * Verifies the integrity of a shadow {@link TabPersistentStore} against an authoritative one.
 *
 * <p>Waits for both stores to load, records consistency metrics (e.g. tab count deltas, state
 * mismatches), and then cleans up the shadow store's resources.
 */
@NullMarked
public class ShadowTabStoreValidator {
    // LINT.IfChange(TabModelOrchestratorType)
    public static final String TABBED_TAG = "Tabbed";
    public static final String HEADLESS_TAG = "Headless";
    public static final String CUSTOM_TAG = "Custom";
    public static final String ARCHIVED_TAG = "Archived";
    // LINT.ThenChange(//tools/metrics/histograms/metadata/tab/histograms.xml:TabModelOrchestratorType)

    private final TabPersistentStore mAuthoritativeStore;
    private final TabPersistentStore mShadowStore;
    private final TabModel mTabModel;
    private final AccumulatingTabCreator mShadowTabCreator;
    private final PersistentStoreMigrationManager mPersistentStoreMigrationManager;
    private final StoreMetricsObserver mAuthoritativeObserver;
    private final StoreMetricsObserver mShadowObserver;
    private final String mSuffix;
    private final boolean mShadowStoreCaughtUp;

    /**
     * @param authoritativeStore The primary store whose timing is used as the baseline.
     * @param shadowStore The alternative store being compared against the authoritative one.
     * @param tabModel The {@link TabModel} associated with the authoritative store.
     * @param shadowTabCreator The {@link AccumulatingTabCreator} used by the shadow store.
     * @param persistentStoreMigrationManager The {@link PersistentStoreMigrationManager} for
     *     migration.
     * @param orchestratorTag The type of tab model orchestrator this validator is for.
     */
    public ShadowTabStoreValidator(
            TabPersistentStore authoritativeStore,
            TabPersistentStore shadowStore,
            TabModel tabModel,
            AccumulatingTabCreator shadowTabCreator,
            PersistentStoreMigrationManager persistentStoreMigrationManager,
            String orchestratorTag) {
        mAuthoritativeStore = authoritativeStore;
        mShadowStore = shadowStore;
        mTabModel = tabModel;
        mShadowTabCreator = shadowTabCreator;
        mPersistentStoreMigrationManager = persistentStoreMigrationManager;
        mSuffix = "." + orchestratorTag;

        mAuthoritativeObserver = new StoreMetricsObserver(this);
        mShadowObserver = new StoreMetricsObserver(this);

        authoritativeStore.addObserver(mAuthoritativeObserver);
        shadowStore.addObserver(mShadowObserver);

        // Retrieve shadow store catch up state prior to any clearing operation.
        mShadowStoreCaughtUp = mPersistentStoreMigrationManager.isShadowStoreCaughtUp();
    }

    private void onStateLoaded() {
        if (mAuthoritativeObserver.isLoaded() && mShadowObserver.isLoaded()) {
            onBothStateLoaded();
        }
    }

    private void onBothStateLoaded() {
        recordDiffMetrics();

        for (CreateFrozenTabArguments arguments : mShadowTabCreator.createFrozenTabArgumentsList) {
            WebContentsState webContentsState = arguments.state.contentsState;
            if (webContentsState != null) {
                webContentsState.destroy();
            }
        }
        mShadowTabCreator.createNewTabArgumentsList.clear();
        mShadowTabCreator.createFrozenTabArgumentsList.clear();

        mAuthoritativeStore.removeObserver(mAuthoritativeObserver);
        mShadowStore.removeObserver(mShadowObserver);
    }

    private void recordDiffMetrics() {
        if (!mShadowStoreCaughtUp) return;

        int tabCountDelta =
                mTabModel.getCount() - mShadowTabCreator.createFrozenTabArgumentsList.size();
        if (tabCountDelta > 0) {
            recordCountHistogram(
                    "Tabs.TabStateStore.TabCountDelta.AuthoritativeHigher", tabCountDelta);
        } else if (tabCountDelta < 0) {
            recordCountHistogram("Tabs.TabStateStore.TabCountDelta.ShadowHigher", -tabCountDelta);
        } else {
            recordEqualTabCountHistogram();
        }

        for (CreateFrozenTabArguments arguments : mShadowTabCreator.createFrozenTabArgumentsList) {
            Tab tab = mTabModel.getTabById(arguments.id);
            if (tab == null || arguments.state.url == null) continue;

            String authUrl = tab.getUrl().getSpec();
            String shadowUrl = arguments.state.url.getSpec();

            if (!TextUtils.equals(authUrl, shadowUrl)) {
                long timeDelta = tab.getTimestampMillis() - arguments.state.timestampMillis;
                if (timeDelta > 0) {
                    recordTimesHistogram(
                            "Tabs.TabStateStore.TimeDeltaOnMismatch.AuthoritativeNewer", timeDelta);
                } else if (timeDelta < 0) {
                    recordTimesHistogram(
                            "Tabs.TabStateStore.TimeDeltaOnMismatch.ShadowNewer", -timeDelta);
                }
            }
        }
    }

    private void recordCountHistogram(String histogramStr, int tabCountDelta) {
        RecordHistogram.recordCount1000Histogram(histogramStr + mSuffix, tabCountDelta);
    }

    private void recordEqualTabCountHistogram() {
        RecordHistogram.recordBooleanHistogram(
                "Tabs.TabStateStore.TabCountDelta.Equal" + mSuffix, true);
    }

    private void recordTimesHistogram(String histogramStr, long timeDelta) {
        RecordHistogram.recordTimesHistogram(histogramStr + mSuffix, timeDelta);
    }

    private static class StoreMetricsObserver implements TabPersistentStoreObserver {
        private final ShadowTabStoreValidator mTracker;
        private boolean mLoaded;

        public StoreMetricsObserver(ShadowTabStoreValidator tracker) {
            mTracker = tracker;
        }

        @Override
        public void onStateLoaded() {
            mLoaded = true;
            mTracker.onStateLoaded();
        }

        public boolean isLoaded() {
            return mLoaded;
        }
    }
}
