// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import android.text.TextUtils;
import android.util.SparseArray;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tabmodel.AccumulatingTabCreator;
import org.chromium.chrome.browser.tabmodel.AccumulatingTabCreator.CreateFrozenTabArguments;
import org.chromium.chrome.browser.tabmodel.PersistentStoreMigrationManager;
import org.chromium.chrome.browser.tabmodel.PersistentStoreMigrationManager.StoreType;
import org.chromium.chrome.browser.tabmodel.RecordingTabCreator;
import org.chromium.chrome.browser.tabmodel.RecordingTabCreator.TabCreationData;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabPersistentStoreObserver;

import java.util.List;

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
    private final RecordingTabCreator mAuthoritativeTabCreator;
    private final AccumulatingTabCreator mShadowTabCreator;
    private final PersistentStoreMigrationManager mPersistentStoreMigrationManager;
    private final StoreMetricsObserver mAuthoritativeObserver;
    private final StoreMetricsObserver mShadowObserver;
    private final String mSuffix;
    private final boolean mShadowStoreCaughtUp;

    /**
     * @param authoritativeStore The primary store whose timing is used as the baseline.
     * @param shadowStore The alternative store being compared against the authoritative one.
     * @param authoritativeTabCreator The {@link RecordingTabCreator} used by the authoritative
     *     store.
     * @param shadowTabCreator The {@link AccumulatingTabCreator} used by the shadow store.
     * @param persistentStoreMigrationManager The {@link PersistentStoreMigrationManager} for
     *     migration.
     * @param orchestratorTag The type of tab model orchestrator this validator is for.
     */
    public ShadowTabStoreValidator(
            TabPersistentStore authoritativeStore,
            TabPersistentStore shadowStore,
            RecordingTabCreator authoritativeTabCreator,
            AccumulatingTabCreator shadowTabCreator,
            PersistentStoreMigrationManager persistentStoreMigrationManager,
            String orchestratorTag) {
        mAuthoritativeStore = authoritativeStore;
        mShadowStore = shadowStore;
        mAuthoritativeTabCreator = authoritativeTabCreator;
        mShadowTabCreator = shadowTabCreator;
        mPersistentStoreMigrationManager = persistentStoreMigrationManager;
        mSuffix = "." + orchestratorTag;

        mAuthoritativeObserver = new StoreMetricsObserver(this);
        mShadowObserver = new StoreMetricsObserver(this);

        authoritativeStore.addObserver(mAuthoritativeObserver);
        shadowStore.addObserver(mShadowObserver);

        // Retrieve shadow store catch up state prior to any clearing operation.
        mShadowStoreCaughtUp = mPersistentStoreMigrationManager.isShadowStoreCaughtUp();

        if (!isTabStateStoreShadowing()) {
            shadowTabCreator.stopRecording();
            authoritativeTabCreator.stopRecording();
        }
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

        mAuthoritativeTabCreator.getFrozenTabCreationData().clear();
        mAuthoritativeTabCreator.getNewTabCreationData().clear();

        mAuthoritativeStore.removeObserver(mAuthoritativeObserver);
        mShadowStore.removeObserver(mShadowObserver);
    }

    private void recordDiffMetrics() {
        if (!mShadowStoreCaughtUp || !isTabStateStoreShadowing()) return;

        List<TabCreationData> authoritativeFrozenData =
                mAuthoritativeTabCreator.getFrozenTabCreationData();

        List<TabCreationData> authoritativeNewTabData =
                mAuthoritativeTabCreator.getNewTabCreationData();

        int tabCountDelta =
                (authoritativeNewTabData.size() + authoritativeFrozenData.size())
                        - (mShadowTabCreator.createFrozenTabArgumentsList.size()
                                + mShadowTabCreator.createNewTabArgumentsList.size());

        if (tabCountDelta > 0) {
            recordCountHistogram(
                    "Tabs.TabStateStore.TabCountDelta.AuthoritativeHigher", tabCountDelta);
        } else if (tabCountDelta < 0) {
            recordCountHistogram("Tabs.TabStateStore.TabCountDelta.ShadowHigher", -tabCountDelta);
        } else {
            recordEqualTabCountHistogram();
        }

        SparseArray<TabCreationData> authoritativeDataMap =
                new SparseArray<>(authoritativeFrozenData.size());
        for (TabCreationData data : authoritativeFrozenData) {
            authoritativeDataMap.put(data.id, data);
        }

        for (CreateFrozenTabArguments arguments : mShadowTabCreator.createFrozenTabArgumentsList) {
            TabCreationData authoritativeData = authoritativeDataMap.get(arguments.id);
            if (authoritativeData == null || arguments.state.url == null) continue;

            String authUrl = authoritativeData.url;
            String shadowUrl = arguments.state.url.getSpec();

            if (!TextUtils.equals(authUrl, shadowUrl)) {
                long timeDelta =
                        authoritativeData.timestampMillis - arguments.state.timestampMillis;
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

    private boolean isTabStateStoreShadowing() {
        return mAuthoritativeStore.getStoreType() == StoreType.LEGACY
                && mShadowStore.getStoreType() == StoreType.TAB_STATE_STORE;
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
