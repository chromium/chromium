// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabPersistentStoreObserver;

/** Tests for {@link TabModelOrchestrator} */
@RunWith(BaseRobolectricTestRunner.class)
public class TabModelOrchestratorUnitTest {
    @Mock private ObservableSupplierImpl<TabModelStartupInfo> mMockTabModelStartupInfoSupplier;
    @Mock private TabModel mMockTabModel;
    @Mock private TabModelSelectorBase mMockTabModelSelectorBase;
    @Mock private TabPersistentStore mMockTabPersistentStore;

    private TabModelOrchestrator mTabModelOrchestrator;
    private ArgumentCaptor<TabPersistentStoreObserver> mObserverCaptor;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        when(mMockTabModelSelectorBase.getModel(anyBoolean())).thenReturn(mMockTabModel);

        mTabModelOrchestrator =
                new TabModelOrchestrator() {
                    @Override
                    public TabModelSelectorBase getTabModelSelector() {
                        return mMockTabModelSelectorBase;
                    }
                };
        mTabModelOrchestrator.setTabPersistentStoreForTesting(mMockTabPersistentStore);

        mObserverCaptor = ArgumentCaptor.forClass(TabPersistentStoreObserver.class);
        mTabModelOrchestrator.wireSelectorAndStore();
        mTabModelOrchestrator.setStartupInfoObservableSupplier(mMockTabModelStartupInfoSupplier);
        verify(mMockTabPersistentStore).addObserver(mObserverCaptor.capture());
    }

    @Test
    @SmallTest
    @Feature({"TabStripPerformance"})
    public void testTabModelStartupInfo() {
        // Send test tab model info.
        int numIncognitoTabs = 2;
        int numStandardTabs = 3;
        int incognitoIndex = 1;
        int standardIndex = 2;
        boolean fromMerge = false;
        readTabState(numStandardTabs, numIncognitoTabs, standardIndex, incognitoIndex, fromMerge);

        // Verify that the {@link TabModelStartupInfo} is as expected.
        ArgumentCaptor<TabModelStartupInfo> startupInfoCaptor =
                ArgumentCaptor.forClass(TabModelStartupInfo.class);
        verify(mMockTabModelStartupInfoSupplier).set(startupInfoCaptor.capture());
        TabModelStartupInfo startupInfo = startupInfoCaptor.getValue();

        assertEquals("Unexpected standard tab count.", numStandardTabs, startupInfo.standardCount);
        assertEquals(
                "Unexpected incognito tab count.", numIncognitoTabs, startupInfo.incognitoCount);
        assertEquals(
                "Unexpected standard active tab index.",
                standardIndex,
                startupInfo.standardActiveIndex);
        assertEquals(
                "Unexpected incognito active tab index.",
                incognitoIndex,
                startupInfo.incognitoActiveIndex);
    }

    @Test
    @SmallTest
    @Feature({"TabStripPerformance"})
    public void testTabModelStartupInfo_FromMerge() {
        // Send test tab model info.
        int numIncognitoTabs = 2;
        int numStandardTabs = 3;
        int incognitoIndex = 1;
        int standardIndex = 2;
        boolean fromMerge = true;
        readTabState(numStandardTabs, numIncognitoTabs, standardIndex, incognitoIndex, fromMerge);

        // Verify that the {@link TabModelStartupInfo} is as expected.
        ArgumentCaptor<TabModelStartupInfo> startupInfoCaptor =
                ArgumentCaptor.forClass(TabModelStartupInfo.class);
        verify(mMockTabModelStartupInfoSupplier).set(startupInfoCaptor.capture());
        TabModelStartupInfo startupInfo = startupInfoCaptor.getValue();

        assertEquals("Unexpected standard tab count.", numStandardTabs, startupInfo.standardCount);
        assertEquals(
                "Unexpected incognito tab count.", numIncognitoTabs, startupInfo.incognitoCount);
        assertEquals(
                "Merged tab states shouldn't set the standard active tab index.",
                TabModel.INVALID_TAB_INDEX,
                startupInfo.standardActiveIndex);
        assertEquals(
                "Merged tab states shouldn't set the incognito active tab index.",
                TabModel.INVALID_TAB_INDEX,
                startupInfo.incognitoActiveIndex);
    }

    @Test
    @SmallTest
    @Feature({"TabStripPerformance"})
    public void testTabModelStartupInfo_IgnoreIncognito() {
        mTabModelOrchestrator.loadState(true, null);

        // Send test tab model info.
        int numIncognitoTabs = 2;
        int numStandardTabs = 3;
        int incognitoIndex = 1;
        int standardIndex = 2;
        boolean fromMerge = false;
        readTabState(numStandardTabs, numIncognitoTabs, standardIndex, incognitoIndex, fromMerge);

        // Verify that the {@link TabModelStartupInfo} is as expected.
        ArgumentCaptor<TabModelStartupInfo> startupInfoCaptor =
                ArgumentCaptor.forClass(TabModelStartupInfo.class);
        verify(mMockTabModelStartupInfoSupplier).set(startupInfoCaptor.capture());
        TabModelStartupInfo startupInfo = startupInfoCaptor.getValue();

        assertEquals("Unexpected standard tab count.", numStandardTabs, startupInfo.standardCount);
        assertEquals("Unexpected incognito tab count.", 0, startupInfo.incognitoCount);
        assertEquals(
                "Unexpected standard active tab index.",
                standardIndex,
                startupInfo.standardActiveIndex);
        assertEquals(
                "Unexpected incognito active tab index.",
                TabModel.INVALID_TAB_INDEX,
                startupInfo.incognitoActiveIndex);
    }

    private void readTabState(
            int numStandardTabs,
            int numIncognitoTabs,
            int standardIndex,
            int incognitoIndex,
            boolean fromMerge) {
        TabPersistentStoreObserver observer = mObserverCaptor.getValue();
        int index = 0;

        for (int i = 0; i < numIncognitoTabs; i++) {
            observer.onDetailsRead(
                    index, index, "Tab " + index, false, i == incognitoIndex, true, fromMerge);
            index++;
        }

        for (int i = 0; i < numStandardTabs; i++) {
            observer.onDetailsRead(
                    index, index, "Tab " + index, i == standardIndex, false, false, fromMerge);
            index++;
        }

        observer.onInitialized(numIncognitoTabs + numStandardTabs);
        mTabModelOrchestrator.restoreTabs(false);
    }
}
