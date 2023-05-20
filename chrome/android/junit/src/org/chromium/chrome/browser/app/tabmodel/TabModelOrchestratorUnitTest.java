// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager.TabModelStartupInfo;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabPersistentStoreObserver;

/** Tests for {@link TabModelOrchestrator} */
@RunWith(BaseRobolectricTestRunner.class)
public class TabModelOrchestratorUnitTest {
    @Mock
    private ObservableSupplierImpl<TabModelStartupInfo> mMockTabModelStartupInfoSupplier;
    @Mock
    private TabPersistentStore mMockTabPersistentStore;

    private TabModelOrchestrator mTabModelOrchestrator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mTabModelOrchestrator = new TabModelOrchestrator() {};
    }

    @Test
    @SmallTest
    @Feature({"TabStripPerformance"})
    public void testTabModelStartupInfo() {
        // Prepare TabModelOrchestrator.
        mTabModelOrchestrator.setTabPersistentStoreForTesting(mMockTabPersistentStore);

        // Prepare observer.
        ArgumentCaptor<TabPersistentStoreObserver> observerCaptor =
                ArgumentCaptor.forClass(TabPersistentStoreObserver.class);
        mTabModelOrchestrator.wireSelectorAndStore();
        mTabModelOrchestrator.setStartupInfoObservableSupplier(mMockTabModelStartupInfoSupplier);
        verify(mMockTabPersistentStore).addObserver(observerCaptor.capture());

        // Send test tab model info.
        TabPersistentStoreObserver observer = observerCaptor.getValue();
        observer.onDetailsRead(0, 0, "0", false, false, true);
        observer.onDetailsRead(1, 1, "1", false, true, true);
        observer.onDetailsRead(2, 2, "2", false, false, false);
        observer.onDetailsRead(3, 3, "3", false, false, false);
        observer.onDetailsRead(4, 4, "4", true, false, false);
        observer.onInitialized(5);

        // Verify that the {@link TabModelStartupInfo} is as expected.
        ArgumentCaptor<TabModelStartupInfo> startupInfoCaptor =
                ArgumentCaptor.forClass(TabModelStartupInfo.class);
        verify(mMockTabModelStartupInfoSupplier).set(startupInfoCaptor.capture());
        TabModelStartupInfo startupInfo = startupInfoCaptor.getValue();

        assertEquals("Unexpected standard tab count.", 3, startupInfo.standardCount);
        assertEquals("Unexpected incognito tab count", 2, startupInfo.incognitoCount);
        assertEquals("Unexpected standard active tab index", 4, startupInfo.standardActiveIndex);
        assertEquals("Unexpected incognito active tab index", 1, startupInfo.incognitoActiveIndex);
    }
}
