// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;

import java.util.List;

/** Unit tests for {@link TabClosureParams}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabClosureParamsUnitTest {
    private static final Token TAB_GROUP_ID = new Token(4378L, 73489L);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModel mTabModel;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private Runnable mUndoRunnable;

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
        assertNull("Undo runnable should be null", params.undoRunnable);
        assertFalse("Should not be a tab group", params.isTabGroup);
    }

    @Test
    public void testCloseTabParams_NonDefaults() {
        TabClosureParams params =
                TabClosureParams.closeTab(mTab1)
                        .recommendedNextTab(mTab2)
                        .uponExit(true)
                        .allowUndo(false)
                        .withUndoRunnable(mUndoRunnable)
                        .build();

        assertEquals("Tabs should be mTab1", List.of(mTab1), params.tabs);
        assertFalse("Should not be all tabs", params.isAllTabs);
        assertEquals("Recommended next tab should be mTab2", mTab2, params.recommendedNextTab);
        assertTrue("Should be upon exit", params.uponExit);
        assertFalse("Should not allow undo", params.allowUndo);
        assertFalse("Should not hide tab groups", params.hideTabGroups);
        assertTrue("Should save to tab restore service", params.saveToTabRestoreService);
        assertEquals("Should be TabCloseType.SINGLE", TabCloseType.SINGLE, params.tabCloseType);
        assertEquals("Undo runnable should be set", mUndoRunnable, params.undoRunnable);
        assertFalse("Should not be a tab group", params.isTabGroup);
    }

    @Test
    public void testCloseTabParams_UnsupportedSetters() {
        TabClosureParams.Builder builder = TabClosureParams.closeTab(mTab1);

        assertThrows(AssertionError.class, () -> builder.hideTabGroups(true));
        assertThrows(AssertionError.class, () -> builder.saveToTabRestoreService(false));
    }

    @Test
    public void testCloseTabParams_UnsupportedSettersAcceptDefaults() {
        TabClosureParams params =
                TabClosureParams.closeTab(mTab1)
                        .hideTabGroups(false)
                        .saveToTabRestoreService(true)
                        .build();

        assertEquals(
                "Writing the default of an unsupported field should change nothing",
                TabClosureParams.closeTab(mTab1).build(),
                params);
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
        assertNull("Undo runnable should be null", params.undoRunnable);
        assertFalse("Should not be a tab group", params.isTabGroup);
        assertTrue("Should allow unload handlers", params.allowUnloadHandlers);
    }

    @Test
    public void testCloseTabsParams_NonDefaults() {
        List<Tab> tabs = List.of(mTab1, mTab2);
        TabClosureParams params =
                TabClosureParams.closeTabs(tabs)
                        .allowUndo(false)
                        .hideTabGroups(true)
                        .saveToTabRestoreService(false)
                        .withUndoRunnable(mUndoRunnable)
                        .allowUnloadHandlers(false)
                        .build();

        assertEquals("Tabs should be mTab1, mTab2", tabs, params.tabs);
        assertFalse("Should not be all tabs", params.isAllTabs);
        assertNull("Recommended next tab should be null", params.recommendedNextTab);
        assertFalse("Should not be upon exit", params.uponExit);
        assertFalse("Should not allow undo", params.allowUndo);
        assertTrue("Should hide tab groups", params.hideTabGroups);
        assertFalse("Should not save to tab restore service", params.saveToTabRestoreService);
        assertEquals("Should be TabCloseType.MULTIPLE", TabCloseType.MULTIPLE, params.tabCloseType);
        assertEquals("Undo runnable should be set", mUndoRunnable, params.undoRunnable);
        assertFalse("Should not be a tab group", params.isTabGroup);
        assertFalse("Should not allow unload handlers", params.allowUnloadHandlers);
    }

    @Test
    public void testCloseTabsParams_UnsupportedSetters() {
        TabClosureParams.Builder builder = TabClosureParams.closeTabs(List.of(mTab1, mTab2));

        assertThrows(AssertionError.class, () -> builder.recommendedNextTab(mTab2));
        assertThrows(AssertionError.class, () -> builder.uponExit(true));
    }

    @Test
    public void testCloseTabsParams_UnsupportedSettersAcceptDefaults() {
        List<Tab> tabs = List.of(mTab1, mTab2);
        TabClosureParams params =
                TabClosureParams.closeTabs(tabs).recommendedNextTab(null).uponExit(false).build();

        assertEquals(
                "Writing the default of an unsupported field should change nothing",
                TabClosureParams.closeTabs(tabs).build(),
                params);
    }

    @Test
    public void testCloseTabsParams_TabGroup() {
        List<Tab> tabs = List.of(mTab1, mTab2);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(tabs);
        TabClosureParams params =
                TabClosureParams.forCloseTabGroup(mTabModel, TAB_GROUP_ID).build();

        assertEquals("Tabs should be mTab1, mTab2", tabs, params.tabs);
        assertFalse("Should not be all tabs", params.isAllTabs);
        assertNull("Recommended next tab should be null", params.recommendedNextTab);
        assertFalse("Should not be upon exit", params.uponExit);
        assertTrue("Should allow undo", params.allowUndo);
        assertFalse("Should not hide tab groups", params.hideTabGroups);
        assertTrue("Should save to tab restore service", params.saveToTabRestoreService);
        assertEquals("Should be TabCloseType.MULTIPLE", TabCloseType.MULTIPLE, params.tabCloseType);
        assertNull("Undo runnable should be null", params.undoRunnable);
        assertTrue("Should be a tab group", params.isTabGroup);
        assertTrue("Should allow unload handlers", params.allowUnloadHandlers);
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
        assertNull("Undo runnable should be null", params.undoRunnable);
        assertFalse("Should not be a tab group", params.isTabGroup);
        assertTrue("Should allow unload handlers", params.allowUnloadHandlers);
    }

    @Test
    public void testCloseAllTabsParams_NonDefaults() {
        TabClosureParams params =
                TabClosureParams.closeAllTabs()
                        .uponExit(true)
                        .hideTabGroups(true)
                        .withUndoRunnable(mUndoRunnable)
                        .allowUnloadHandlers(false)
                        .build();

        assertNull("Tabs should be null", params.tabs);
        assertTrue("Should be all tabs", params.isAllTabs);
        assertNull("Recommended next tab should be null", params.recommendedNextTab);
        assertTrue("Should be upon exit", params.uponExit);
        assertTrue("Should allow undo", params.allowUndo);
        assertTrue("Should hide tab groups", params.hideTabGroups);
        assertTrue("Should save to tab restore service", params.saveToTabRestoreService);
        assertEquals("Should be TabCloseType.ALL", TabCloseType.ALL, params.tabCloseType);
        assertEquals("Undo runnable should be set", mUndoRunnable, params.undoRunnable);
        assertFalse("Should not be a tab group", params.isTabGroup);
        assertFalse("Should not allow unload handlers", params.allowUnloadHandlers);
    }

    @Test
    public void testCloseAllTabsParams_UnsupportedSetters() {
        TabClosureParams.Builder builder = TabClosureParams.closeAllTabs();

        assertThrows(AssertionError.class, () -> builder.recommendedNextTab(mTab1));
    }

    @Test
    public void testToBuilder_CloseTab_CarriesOverEveryField() {
        TabClosureParams params =
                TabClosureParams.closeTab(mTab1)
                        .recommendedNextTab(mTab2)
                        .uponExit(true)
                        .allowUndo(false)
                        .tabClosingSource(TabClosingSource.TABLET_TAB_STRIP)
                        .withUndoRunnable(mUndoRunnable)
                        .allowUnloadHandlers(false)
                        .build();

        assertEquals("Copy should equal the original", params, params.toBuilder().build());
    }

    @Test
    public void testToBuilder_CloseTabs_CarriesOverEveryField() {
        TabClosureParams params =
                TabClosureParams.closeTabs(List.of(mTab1, mTab2))
                        .allowUndo(false)
                        .hideTabGroups(true)
                        .saveToTabRestoreService(false)
                        .tabClosingSource(TabClosingSource.TABLET_TAB_STRIP)
                        .withUndoRunnable(mUndoRunnable)
                        .allowUnloadHandlers(false)
                        .build();

        assertEquals("Copy should equal the original", params, params.toBuilder().build());
    }

    @Test
    public void testToBuilder_CloseAllTabs_CarriesOverEveryField() {
        TabClosureParams params =
                TabClosureParams.closeAllTabs()
                        .uponExit(true)
                        .allowUndo(false)
                        .hideTabGroups(true)
                        .saveToTabRestoreService(false)
                        .tabClosingSource(TabClosingSource.TABLET_TAB_STRIP)
                        .withUndoRunnable(mUndoRunnable)
                        .allowUnloadHandlers(false)
                        .build();

        assertEquals("Copy should equal the original", params, params.toBuilder().build());
    }

    @Test
    public void testToBuilder_PreservesIsTabGroup() {
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(List.of(mTab1, mTab2));
        TabClosureParams params =
                TabClosureParams.forCloseTabGroup(mTabModel, TAB_GROUP_ID).build();

        TabClosureParams copy = params.toBuilder().build();

        assertTrue("Copy should still be a tab group closure", copy.isTabGroup);
        assertEquals("Copy should equal the original", params, copy);
    }

    @Test
    public void testToBuilder_ReplacesTabs() {
        TabClosureParams params =
                TabClosureParams.closeTabs(List.of(mTab1, mTab2)).hideTabGroups(true).build();

        TabClosureParams copy = params.toBuilder(List.of(mTab1)).build();

        assertEquals("Tabs should be replaced", List.of(mTab1), copy.tabs);
        assertTrue("Every other field should carry over", copy.hideTabGroups);
    }

    @Test
    public void testToBuilder_UnsupportedReplacements() {
        TabClosureParams allTabsParams = TabClosureParams.closeAllTabs().build();
        TabClosureParams singleTabParams = TabClosureParams.closeTab(mTab1).build();
        TabClosureParams multipleTabsParams =
                TabClosureParams.closeTabs(List.of(mTab1, mTab2)).build();
        List<Tab> oneTab = List.of(mTab1);
        List<Tab> twoTabs = List.of(mTab1, mTab2);
        List<Tab> noTabs = List.of();

        assertThrows(AssertionError.class, () -> allTabsParams.toBuilder(oneTab));
        assertThrows(AssertionError.class, () -> singleTabParams.toBuilder(twoTabs));
        assertThrows(AssertionError.class, () -> multipleTabsParams.toBuilder(noTabs));
    }

    @Test
    public void testToPartialClosureBuilder_CarriesOverSupportedFields() {
        TabClosureParams params =
                TabClosureParams.closeAllTabs()
                        .uponExit(true)
                        .allowUndo(false)
                        .hideTabGroups(true)
                        .saveToTabRestoreService(false)
                        .tabClosingSource(TabClosingSource.TABLET_TAB_STRIP)
                        .withUndoRunnable(mUndoRunnable)
                        .allowUnloadHandlers(false)
                        .build();

        TabClosureParams partialParams = params.toPartialClosureBuilder(List.of(mTab1)).build();

        assertEquals(
                "Should be TabCloseType.MULTIPLE",
                TabCloseType.MULTIPLE,
                partialParams.tabCloseType);
        assertFalse("Should no longer be all tabs", partialParams.isAllTabs);
        assertEquals("Tabs should be the surviving tabs", List.of(mTab1), partialParams.tabs);
        assertFalse("Should not allow undo", partialParams.allowUndo);
        assertTrue("Should hide tab groups", partialParams.hideTabGroups);
        assertFalse("Should not save to tab restore service", partialParams.saveToTabRestoreService);
        assertEquals(
                "Tab closing source should carry over",
                TabClosingSource.TABLET_TAB_STRIP,
                partialParams.tabClosingSource);
        assertEquals("Undo runnable should carry over", mUndoRunnable, partialParams.undoRunnable);
        assertFalse("Should not allow unload handlers", partialParams.allowUnloadHandlers);
        assertFalse(
                "uponExit is unsupported by a multi-tab closure and must be dropped",
                partialParams.uponExit);
    }

    @Test
    public void testToPartialClosureBuilder_RequiresAllTabsClosure() {
        TabClosureParams params = TabClosureParams.closeTabs(List.of(mTab1, mTab2)).build();
        List<Tab> oneTab = List.of(mTab1);

        assertThrows(AssertionError.class, () -> params.toPartialClosureBuilder(oneTab));
    }

    @Test
    public void testToPartialClosureBuilder_RequiresSurvivingTabs() {
        TabClosureParams params = TabClosureParams.closeAllTabs().build();
        List<Tab> noTabs = List.of();

        assertThrows(AssertionError.class, () -> params.toPartialClosureBuilder(noTabs));
    }

    @Test
    public void testTabClosureParams_Equality() {
        TabClosureParams tab1Params = TabClosureParams.closeTab(mTab1).build();
        TabClosureParams tab1ParamsDuplicate = TabClosureParams.closeTab(mTab1).build();
        TabClosureParams tab2Params = TabClosureParams.closeTab(mTab2).build();

        assertEqualsAndHashCodeWork(tab1Params, tab1ParamsDuplicate, tab2Params);
    }

    @Test
    public void testTabClosureParams_Equality_AllowUnloadHandlers() {
        TabClosureParams params = TabClosureParams.closeTab(mTab1).build();
        TabClosureParams duplicateParams = TabClosureParams.closeTab(mTab1).build();
        TabClosureParams differentParams =
                TabClosureParams.closeTab(mTab1).allowUnloadHandlers(false).build();

        assertEqualsAndHashCodeWork(params, duplicateParams, differentParams);
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
