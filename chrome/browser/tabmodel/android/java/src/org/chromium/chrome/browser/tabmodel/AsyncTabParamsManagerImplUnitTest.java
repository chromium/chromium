// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;

/** Unit tests for {@link AsyncTabParamsManagerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
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

    @Test
    public void testCloseAllIncognitoTabs_IncognitoWebContents() {
        AsyncTabParams incognitoWebContentsParams =
                createMockAsyncTabParamsWithWebContents(/* isIncognito= */ true);
        AsyncTabParams regularParams = mRegularParams;

        mManager.add(/* tabId= */ 1, incognitoWebContentsParams);
        mManager.add(/* tabId= */ 2, regularParams);

        assertTrue(IncognitoTabHostUtils.doIncognitoTabsExist());
        IncognitoTabHostUtils.closeAllIncognitoTabs();

        assertFalse(IncognitoTabHostUtils.doIncognitoTabsExist());
        verify(incognitoWebContentsParams).destroy();
        verify(regularParams, never()).destroy();

        assertNull(mManager.remove(/* tabId= */ 1));
        assertNotNull(mManager.remove(/* tabId= */ 2));
    }

    @Test
    public void testCloseAllIncognitoTabs_RegularWebContents() {
        AsyncTabParams regularWebContentsParams =
                createMockAsyncTabParamsWithWebContents(/* isIncognito= */ false);

        mManager.add(/* tabId= */ 1, regularWebContentsParams);

        assertFalse(IncognitoTabHostUtils.doIncognitoTabsExist());
        IncognitoTabHostUtils.closeAllIncognitoTabs();

        verify(regularWebContentsParams, never()).destroy();
        assertNotNull(mManager.remove(/* tabId= */ 1));
    }

    @Test
    public void testCloseAllIncognitoTabs_MultipleIncognitoAndRegularTabs() {
        AsyncTabParams incognitoParams1 =
                createMockAsyncTabParams(
                        /* isIncognitoBranded= */ true, /* isOffTheRecord= */ true);
        AsyncTabParams incognitoParams2 =
                createMockAsyncTabParams(
                        /* isIncognitoBranded= */ true, /* isOffTheRecord= */ true);
        AsyncTabParams regularParams1 =
                createMockAsyncTabParams(
                        /* isIncognitoBranded= */ false, /* isOffTheRecord= */ false);
        AsyncTabParams incognitoWebContentsParams =
                createMockAsyncTabParamsWithWebContents(/* isIncognito= */ true);
        AsyncTabParams incognitoParams3 =
                createMockAsyncTabParams(
                        /* isIncognitoBranded= */ false, /* isOffTheRecord= */ true);
        AsyncTabParams nullTabParams = createMockAsyncTabParamsWithNullTab();
        AsyncTabParams regularWebContentsParams =
                createMockAsyncTabParamsWithWebContents(/* isIncognito= */ false);
        AsyncTabParams incognitoParams4 =
                createMockAsyncTabParams(
                        /* isIncognitoBranded= */ true, /* isOffTheRecord= */ true);
        AsyncTabParams regularParams2 =
                createMockAsyncTabParams(
                        /* isIncognitoBranded= */ false, /* isOffTheRecord= */ false);

        mManager.add(/* tabId= */ 1, incognitoParams1);
        mManager.add(/* tabId= */ 2, incognitoParams2);
        mManager.add(/* tabId= */ 3, regularParams1);
        mManager.add(/* tabId= */ 4, incognitoWebContentsParams);
        mManager.add(/* tabId= */ 5, incognitoParams3);
        mManager.add(/* tabId= */ 6, nullTabParams);
        mManager.add(/* tabId= */ 7, regularWebContentsParams);
        mManager.add(/* tabId= */ 8, incognitoParams4);
        mManager.add(/* tabId= */ 9, regularParams2);

        assertTrue(IncognitoTabHostUtils.doIncognitoTabsExist());
        IncognitoTabHostUtils.closeAllIncognitoTabs();

        assertFalse(IncognitoTabHostUtils.doIncognitoTabsExist());
        verify(incognitoParams1).destroy();
        verify(incognitoParams2).destroy();
        verify(incognitoWebContentsParams).destroy();
        verify(incognitoParams3).destroy();
        verify(incognitoParams4).destroy();
        verify(regularParams1, never()).destroy();
        verify(nullTabParams, never()).destroy();
        verify(regularWebContentsParams, never()).destroy();
        verify(regularParams2, never()).destroy();

        assertNull(mManager.remove(/* tabId= */ 1));
        assertNull(mManager.remove(/* tabId= */ 2));
        assertNotNull(mManager.remove(/* tabId= */ 3));
        assertNull(mManager.remove(/* tabId= */ 4));
        assertNull(mManager.remove(/* tabId= */ 5));
        assertNotNull(mManager.remove(/* tabId= */ 6));
        assertNotNull(mManager.remove(/* tabId= */ 7));
        assertNull(mManager.remove(/* tabId= */ 8));
        assertNotNull(mManager.remove(/* tabId= */ 9));
    }

    @Test
    public void testCloseAllIncognitoTabs_WithPriorRemovals() {
        AsyncTabParams incognitoParams1 =
                createMockAsyncTabParams(
                        /* isIncognitoBranded= */ true, /* isOffTheRecord= */ true);
        AsyncTabParams regularParams1 =
                createMockAsyncTabParams(
                        /* isIncognitoBranded= */ false, /* isOffTheRecord= */ false);
        AsyncTabParams incognitoWebContentsParams =
                createMockAsyncTabParamsWithWebContents(/* isIncognito= */ true);
        AsyncTabParams regularParams2 =
                createMockAsyncTabParams(
                        /* isIncognitoBranded= */ false, /* isOffTheRecord= */ false);
        AsyncTabParams incognitoParams3 =
                createMockAsyncTabParams(
                        /* isIncognitoBranded= */ false, /* isOffTheRecord= */ true);

        mManager.add(/* tabId= */ 10, incognitoParams1);
        mManager.add(/* tabId= */ 20, regularParams1);
        mManager.add(/* tabId= */ 30, incognitoWebContentsParams);
        mManager.add(/* tabId= */ 40, regularParams2);
        mManager.add(/* tabId= */ 50, incognitoParams3);

        // Remove elements prior to closeAllIncognitoTabs to leave deleted markers in SparseArray.
        assertEquals(/* expected= */ regularParams1, mManager.remove(/* tabId= */ 20));
        assertEquals(/* expected= */ incognitoWebContentsParams, mManager.remove(/* tabId= */ 30));

        assertTrue(IncognitoTabHostUtils.doIncognitoTabsExist());
        IncognitoTabHostUtils.closeAllIncognitoTabs();

        assertFalse(IncognitoTabHostUtils.doIncognitoTabsExist());
        verify(incognitoParams1).destroy();
        verify(incognitoWebContentsParams, never()).destroy();
        verify(incognitoParams3).destroy();
        verify(regularParams1, never()).destroy();
        verify(regularParams2, never()).destroy();

        assertNull(mManager.remove(/* tabId= */ 10));
        assertNull(mManager.remove(/* tabId= */ 20));
        assertNull(mManager.remove(/* tabId= */ 30));
        assertNotNull(mManager.remove(/* tabId= */ 40));
        assertNull(mManager.remove(/* tabId= */ 50));
    }

    private AsyncTabParams createMockAsyncTabParams(
            boolean isIncognitoBranded, boolean isOffTheRecord) {
        AsyncTabParams params = mock(AsyncTabParams.class);
        Tab tab = mock(Tab.class);
        when(params.getTabToReparent()).thenReturn(tab);
        when(tab.isIncognitoBranded()).thenReturn(isIncognitoBranded);
        when(tab.isOffTheRecord()).thenReturn(isOffTheRecord);
        return params;
    }

    private AsyncTabParams createMockAsyncTabParamsWithWebContents(boolean isIncognito) {
        AsyncTabParams params = mock(AsyncTabParams.class);
        WebContents webContents = mock(WebContents.class);
        when(params.getTabToReparent()).thenReturn(null);
        when(params.getWebContents()).thenReturn(webContents);
        when(webContents.isIncognito()).thenReturn(isIncognito);
        return params;
    }

    private AsyncTabParams createMockAsyncTabParamsWithNullTab() {
        AsyncTabParams params = mock(AsyncTabParams.class);
        when(params.getTabToReparent()).thenReturn(null);
        when(params.getWebContents()).thenReturn(null);
        return params;
    }
}
