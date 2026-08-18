// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.AccumulatingTabCreator;
import org.chromium.chrome.browser.tabmodel.PersistentStoreMigrationManager;
import org.chromium.chrome.browser.tabmodel.PersistentStoreMigrationManager.StoreType;
import org.chromium.chrome.browser.tabmodel.RecordingTabCreator;
import org.chromium.chrome.browser.tabmodel.RecordingTabCreator.TabCreationData;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabPersistentStoreObserver;

/** Unit tests for {@link ShadowTabStoreValidator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ShadowTabStoreValidatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private TabPersistentStore mAuthoritativeStore;
    @Mock private TabPersistentStore mShadowStore;
    @Mock private PersistentStoreMigrationManager mPersistentStoreMigrationManager;

    @Captor private ArgumentCaptor<TabPersistentStoreObserver> mAuthoritativeObserverCaptor;
    @Captor private ArgumentCaptor<TabPersistentStoreObserver> mShadowObserverCaptor;

    private RecordingTabCreator mAuthoritativeTabCreator;
    private AccumulatingTabCreator mShadowTabCreator;

    @Before
    public void setUp() {
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mAuthoritativeStore.getStoreType()).thenReturn(StoreType.LEGACY);
        when(mShadowStore.getStoreType()).thenReturn(StoreType.TAB_STATE_STORE);

        mAuthoritativeTabCreator = new RecordingTabCreator();
        mShadowTabCreator = new AccumulatingTabCreator();
    }

    @Test
    public void testRecordDiffMetrics_SuppressesMetrics_WhenShadowStoreNotCaughtUp() {
        // Initially caught up at construction time.
        when(mPersistentStoreMigrationManager.isShadowStoreCaughtUp()).thenReturn(true);

        new ShadowTabStoreValidator(
                mProfile,
                mAuthoritativeStore,
                mShadowStore,
                mAuthoritativeTabCreator,
                mShadowTabCreator,
                mPersistentStoreMigrationManager,
                "window_1",
                ShadowTabStoreValidator.TABBED_TAG);

        verify(mAuthoritativeStore).addObserver(mAuthoritativeObserverCaptor.capture());
        verify(mShadowStore).addObserver(mShadowObserverCaptor.capture());

        // Simulate shadow store being razed during asynchronous loading,
        // causing migration manager to report that the shadow store is no longer caught up.
        when(mPersistentStoreMigrationManager.isShadowStoreCaughtUp()).thenReturn(false);

        // Authoritative store has 1 tab, shadow store has 0 tabs.
        TabCreationData tabData =
                new TabCreationData(1, "https://www.google.com", 1000L, false, null);
        mAuthoritativeTabCreator.getFrozenTabCreationData().add(tabData);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                "Tabs.TabStateStore.TabCountDelta.AuthoritativeHigher."
                                        + ShadowTabStoreValidator.TABBED_TAG)
                        .build();

        // Trigger completion for both stores.
        mAuthoritativeObserverCaptor.getValue().onStateLoaded();
        mShadowObserverCaptor.getValue().onStateLoaded();

        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordDiffMetrics_RecordsMetrics_WhenShadowStoreCaughtUp() {
        when(mPersistentStoreMigrationManager.isShadowStoreCaughtUp()).thenReturn(true);

        new ShadowTabStoreValidator(
                mProfile,
                mAuthoritativeStore,
                mShadowStore,
                mAuthoritativeTabCreator,
                mShadowTabCreator,
                mPersistentStoreMigrationManager,
                "window_1",
                ShadowTabStoreValidator.TABBED_TAG);

        verify(mAuthoritativeStore).addObserver(mAuthoritativeObserverCaptor.capture());
        verify(mShadowStore).addObserver(mShadowObserverCaptor.capture());

        TabCreationData tabData =
                new TabCreationData(1, "https://www.google.com", 1000L, false, null);
        mAuthoritativeTabCreator.getFrozenTabCreationData().add(tabData);

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.TabStateStore.TabCountDelta.AuthoritativeHigher."
                                + ShadowTabStoreValidator.TABBED_TAG,
                        1);

        mAuthoritativeObserverCaptor.getValue().onStateLoaded();
        mShadowObserverCaptor.getValue().onStateLoaded();

        histogramWatcher.assertExpected();
    }
}
