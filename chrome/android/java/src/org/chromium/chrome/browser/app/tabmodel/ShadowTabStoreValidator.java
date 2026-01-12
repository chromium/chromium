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
    private final TabPersistentStore mAuthoritativeStore;
    private final TabPersistentStore mShadowStore;
    private final TabModel mTabModel;
    private final AccumulatingTabCreator mShadowTabCreator;
    private final StoreMetricsObserver mAuthoritativeObserver;
    private final StoreMetricsObserver mShadowObserver;

    // TODO(crbug.com/475181628): Remove once we've added histogram support for TabModelOrchestrator
    // subclasses.
    private final boolean mRecordMetrics;

    /**
     * @param authoritativeStore The primary store whose timing is used as the baseline.
     * @param shadowStore The alternative store being compared against the authoritative one.
     * @param tabModel The {@link TabModel} associated with the authoritative store.
     * @param shadowTabCreator The {@link AccumulatingTabCreator} used by the shadow store.
     * @param recordMetrics If set to true, we will record metrics.
     */
    public ShadowTabStoreValidator(
            TabPersistentStore authoritativeStore,
            TabPersistentStore shadowStore,
            TabModel tabModel,
            AccumulatingTabCreator shadowTabCreator,
            boolean recordMetrics) {
        mRecordMetrics = recordMetrics;
        mAuthoritativeStore = authoritativeStore;
        mShadowStore = shadowStore;
        mTabModel = tabModel;
        mShadowTabCreator = shadowTabCreator;

        mAuthoritativeObserver = new StoreMetricsObserver(this);
        mShadowObserver = new StoreMetricsObserver(this);

        authoritativeStore.addObserver(mAuthoritativeObserver);
        shadowStore.addObserver(mShadowObserver);
    }

    private void onStateLoaded() {
        if (mAuthoritativeObserver.isLoaded() && mShadowObserver.isLoaded()) {
            onBothStateLoaded();
        }
    }

    private void onBothStateLoaded() {
        if (mRecordMetrics) recordDiffMetrics();

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
        int tabCountDelta =
                mTabModel.getCount() - mShadowTabCreator.createFrozenTabArgumentsList.size();
        if (tabCountDelta > 0) {
            RecordHistogram.recordCount1000Histogram(
                    "Tabs.TabStateStore.TabCountDelta.AuthoritativeHigher", tabCountDelta);
        } else if (tabCountDelta < 0) {
            RecordHistogram.recordCount1000Histogram(
                    "Tabs.TabStateStore.TabCountDelta.ShadowHigher", -tabCountDelta);
        }

        for (CreateFrozenTabArguments arguments : mShadowTabCreator.createFrozenTabArgumentsList) {
            Tab tab = mTabModel.getTabById(arguments.id);
            if (tab == null || arguments.state.contentsState == null) continue;

            String authUrl = tab.getUrl().getSpec();
            String shadowUrl = arguments.state.contentsState.getVirtualUrlFromState();

            if (!TextUtils.equals(authUrl, shadowUrl)) {
                long timeDelta = tab.getTimestampMillis() - arguments.state.timestampMillis;
                if (timeDelta > 0) {
                    RecordHistogram.recordTimesHistogram(
                            "Tabs.TabStateStore.TimeDeltaOnMismatch.AuthoritativeNewer", timeDelta);
                } else if (timeDelta < 0) {
                    RecordHistogram.recordTimesHistogram(
                            "Tabs.TabStateStore.TimeDeltaOnMismatch.ShadowNewer", -timeDelta);
                }
            }
        }
    }

    public static class StoreMetricsObserver implements TabPersistentStoreObserver {
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
