// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;

import java.util.List;

/** Unit tests for {@link TabClosureParams}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabClosureParamsUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock Tab mTab1;
    @Mock Tab mTab2;
    @Mock Tab mTab3;

    @Test
    public void testCloseTabParams_Defaults() {
        TabClosureParams params = TabClosureParams.closeTab(mTab1).build();

        assertEquals("Tabs should be mTab1", List.of(mTab1), params.tabs);
        assertFalse("Should not be all tabs", params.isAllTabs);
        assertNull("Recommended next tab should be null", params.recommendedNextTab);
        assertFalse("Should not be upon exit", params.uponExit);
        assertTrue("Should allow undo", params.allowUndo);
        assertFalse("Should not hide tab groups", params.hideTabGroups);
        assertTrue("Should save to tab restore service", params.saveToTabRestoreService);
        assertEquals("Should be TabCloseType.SINGLE", TabCloseType.SINGLE, params.tabCloseType);
    }

    @Test
    public void testCloseTabParams_NonDefaults() {
        TabClosureParams params =
                TabClosureParams.closeTab(mTab1)
                        .recommendedNextTab(mTab2)
                        .uponExit(true)
                        .allowUndo(false)
                        .build();

        assertEquals("Tabs should be mTab1", List.of(mTab1), params.tabs);
        assertFalse("Should not be all tabs", params.isAllTabs);
        assertEquals("Recommended next tab should be mTab2", mTab2, params.recommendedNextTab);
        assertTrue("Should be upon exit", params.uponExit);
        assertFalse("Should not allow undo", params.allowUndo);
        assertFalse("Should not hide tab groups", params.hideTabGroups);
        assertTrue("Should save to tab restore service", params.saveToTabRestoreService);
        assertEquals("Should be TabCloseType.SINGLE", TabCloseType.SINGLE, params.tabCloseType);
    }

    @Test
    public void testCloseTabsParams_Defaults() {
        List<Tab> tabs = List.of(mTab1, mTab2);
        TabClosureParams params = TabClosureParams.closeTabs(tabs).build();

        assertEquals("Tabs should be mTab1, mTab2", tabs, params.tabs);
        assertFalse("Should not be all tabs", params.isAllTabs);
        assertNull("Recommended next tab should be null", params.recommendedNextTab);
        assertFalse("Should not be upon exit", params.uponExit);
        assertTrue("Should allow undo", params.allowUndo);
        assertFalse("Should not hide tab groups", params.hideTabGroups);
        assertTrue("Should save to tab restore service", params.saveToTabRestoreService);
        assertEquals("Should be TabCloseType.MULTIPLE", TabCloseType.MULTIPLE, params.tabCloseType);
    }

    @Test
    public void testCloseTabsParams_NonDefaults() {
        List<Tab> tabs = List.of(mTab1, mTab2);
        TabClosureParams params =
                TabClosureParams.closeTabs(tabs)
                        .allowUndo(false)
                        .hideTabGroups(true)
                        .saveToTabRestoreService(false)
                        .build();

        assertEquals("Tabs should be mTab1, mTab2", tabs, params.tabs);
        assertFalse("Should not be all tabs", params.isAllTabs);
        assertNull("Recommended next tab should be null", params.recommendedNextTab);
        assertFalse("Should not be upon exit", params.uponExit);
        assertFalse("Should not allow undo", params.allowUndo);
        assertTrue("Should hide tab groups", params.hideTabGroups);
        assertFalse("Should not save to tab restore service", params.saveToTabRestoreService);
        assertEquals("Should be TabCloseType.MULTIPLE", TabCloseType.MULTIPLE, params.tabCloseType);
    }

    @Test
    public void testCloseAllTabsParams_Defaults() {
        TabClosureParams params = TabClosureParams.closeAllTabs().build();

        assertNull("Tabs should be null", params.tabs);
        assertTrue("Should be all tabs", params.isAllTabs);
        assertNull("Recommended next tab should be null", params.recommendedNextTab);
        assertFalse("Should not be upon exit", params.uponExit);
        assertTrue("Should allow undo", params.allowUndo);
        assertFalse("Should not hide tab groups", params.hideTabGroups);
        assertTrue("Should save to tab restore service", params.saveToTabRestoreService);
        assertEquals("Should be TabCloseType.ALL", TabCloseType.ALL, params.tabCloseType);
    }

    @Test
    public void testCloseAllTabsParams_NonDefaults() {
        TabClosureParams params =
                TabClosureParams.closeAllTabs().uponExit(true).hideTabGroups(true).build();

        assertNull("Tabs should be null", params.tabs);
        assertTrue("Should be all tabs", params.isAllTabs);
        assertNull("Recommended next tab should be null", params.recommendedNextTab);
        assertTrue("Should be upon exit", params.uponExit);
        assertTrue("Should allow undo", params.allowUndo);
        assertTrue("Should hide tab groups", params.hideTabGroups);
        assertTrue("Should save to tab restore service", params.saveToTabRestoreService);
        assertEquals("Should be TabCloseType.ALL", TabCloseType.ALL, params.tabCloseType);
    }

    @Test
    public void testTabClosureParams_Equality() {
        TabClosureParams tab1Params = TabClosureParams.closeTab(mTab1).build();
        TabClosureParams tab1ParamsDuplicate = TabClosureParams.closeTab(mTab1).build();
        TabClosureParams tab2Params = TabClosureParams.closeTab(mTab2).build();

        assertEqualsAndHashCodeWork(tab1Params, tab1ParamsDuplicate, tab2Params);
    }

    private void assertEqualsAndHashCodeWork(
            TabClosureParams params,
            TabClosureParams duplicateParams,
            TabClosureParams differentParams) {
        assertTrue("params should equal itself", params.equals(params));
        assertTrue("duplicateParams should equal itself", duplicateParams.equals(duplicateParams));
        assertTrue("differentParams should equal itself", differentParams.equals(differentParams));
        assertTrue("params should equal duplicateParams", params.equals(duplicateParams));
        assertFalse("params should not equal differentParams", params.equals(differentParams));
        assertFalse("params should not equal null", params.equals(null));
        assertEquals(
                "params should have the same hash code as duplicateParams",
                params.hashCode(),
                duplicateParams.hashCode());
        assertNotEquals(
                "params should not have the same hash code as differentParams",
                params.hashCode(),
                differentParams.hashCode());
    }
}
