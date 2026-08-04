// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;

/** Unit tests for {@link AsyncTabParamsManagerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AsyncTabParamsManagerImplUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mIncognitoBrandedTab;
    @Mock private Tab mOffTheRecordTab;
    @Mock private Tab mRegularTab;
    @Mock private AsyncTabParams mIncognitoBrandedParams;
    @Mock private AsyncTabParams mOffTheRecordParams;
    @Mock private AsyncTabParams mRegularParams;
    @Mock private AsyncTabParams mNullTabParams;

    private AsyncTabParamsManager mManager;

    @Before
    public void setUp() {
        IncognitoTabHostRegistry.getInstance().getHosts().clear();
        mManager = AsyncTabParamsManagerFactory.createAsyncTabParamsManager();
        when(mIncognitoBrandedParams.getTabToReparent()).thenReturn(mIncognitoBrandedTab);
        when(mOffTheRecordParams.getTabToReparent()).thenReturn(mOffTheRecordTab);
        when(mRegularParams.getTabToReparent()).thenReturn(mRegularTab);
        when(mNullTabParams.getTabToReparent()).thenReturn(null);

        when(mIncognitoBrandedTab.isIncognitoBranded()).thenReturn(true);
        when(mIncognitoBrandedTab.isOffTheRecord()).thenReturn(true);

        when(mOffTheRecordTab.isIncognitoBranded()).thenReturn(false);
        when(mOffTheRecordTab.isOffTheRecord()).thenReturn(true);

        when(mRegularTab.isIncognitoBranded()).thenReturn(false);
        when(mRegularTab.isOffTheRecord()).thenReturn(false);
    }

    @After
    public void tearDown() {
        IncognitoTabHostRegistry.getInstance().getHosts().clear();
    }

    @Test
    public void testCloseAllIncognitoTabs_IncognitoBranded() {
        mManager.add(/* tabId= */ 1, mIncognitoBrandedParams);
        mManager.add(/* tabId= */ 2, mRegularParams);
        mManager.add(/* tabId= */ 3, mNullTabParams);

        assertTrue(IncognitoTabHostUtils.doIncognitoTabsExist());
        IncognitoTabHostUtils.closeAllIncognitoTabs();

        assertFalse(IncognitoTabHostUtils.doIncognitoTabsExist());
        verify(mIncognitoBrandedParams).destroy();
        verify(mRegularParams, never()).destroy();
        verify(mNullTabParams, never()).destroy();

        assertNull(mManager.remove(/* tabId= */ 1));
        assertNotNull(mManager.remove(/* tabId= */ 2));
        assertNotNull(mManager.remove(/* tabId= */ 3));
    }

    @Test
    public void testCloseAllIncognitoTabs_OffTheRecord() {
        mManager.add(/* tabId= */ 1, mOffTheRecordParams);
        mManager.add(/* tabId= */ 2, mRegularParams);

        assertTrue(IncognitoTabHostUtils.doIncognitoTabsExist());
        IncognitoTabHostUtils.closeAllIncognitoTabs();

        assertFalse(IncognitoTabHostUtils.doIncognitoTabsExist());
        verify(mOffTheRecordParams).destroy();
        verify(mRegularParams, never()).destroy();

        assertNull(mManager.remove(/* tabId= */ 1));
        assertNotNull(mManager.remove(/* tabId= */ 2));
    }

    @Test
    public void testHasParamsWithTabToReparent() {
        assertFalse(mManager.hasParamsWithTabToReparent());
        assertFalse(mManager.hasParamsWithTabToReparent(/* tabId= */ 1));

        mManager.add(/* tabId= */ 1, mNullTabParams);
        assertFalse(mManager.hasParamsWithTabToReparent());
        assertFalse(mManager.hasParamsWithTabToReparent(/* tabId= */ 1));

        mManager.add(/* tabId= */ 2, mRegularParams);
        assertTrue(mManager.hasParamsWithTabToReparent());
        assertTrue(mManager.hasParamsWithTabToReparent(/* tabId= */ 2));
        assertFalse(mManager.hasParamsWithTabToReparent(/* tabId= */ 1));
    }

    @Test
    public void testAddAndRemove() {
        assertFalse(mManager.hasParamsForTabId(/* tabId= */ 1));
        mManager.add(/* tabId= */ 1, mRegularParams);
        assertTrue(mManager.hasParamsForTabId(/* tabId= */ 1));
        assertEquals(/* expected= */ 1, mManager.getAsyncTabParams().size());

        assertEquals(/* expected= */ mRegularParams, mManager.remove(/* tabId= */ 1));
        assertFalse(mManager.hasParamsForTabId(/* tabId= */ 1));
        assertEquals(/* expected= */ 0, mManager.getAsyncTabParams().size());
    }
}
