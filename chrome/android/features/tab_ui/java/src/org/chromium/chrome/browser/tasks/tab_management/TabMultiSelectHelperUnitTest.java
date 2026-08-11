// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.KeyEvent;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;

import java.util.Set;

/** Unit tests for {@link TabMultiSelectHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabMultiSelectHelperUnitTest {
    @Mock private TabModel mTabModel;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private Tab mTab3;
    @Mock private Callback<Integer> mSelectTabCallback;

    private static final int TAB1_ID = 11;
    private static final int TAB2_ID = 22;
    private static final int TAB3_ID = 33;
    private static final Token GROUP_ID = new Token(1L, 2L);

    private TabMultiSelectHelper mHelper;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);

        when(mTab1.getId()).thenReturn(TAB1_ID);
        when(mTab2.getId()).thenReturn(TAB2_ID);
        when(mTab3.getId()).thenReturn(TAB3_ID);

        when(mTabModel.getCount()).thenReturn(3);

        when(mTabModel.getTabById(TAB1_ID)).thenReturn(mTab1);
        when(mTabModel.getTabById(TAB2_ID)).thenReturn(mTab2);
        when(mTabModel.getTabById(TAB3_ID)).thenReturn(mTab3);

        when(mTabModel.indexOf(mTab1)).thenReturn(0);
        when(mTabModel.indexOf(mTab2)).thenReturn(1);
        when(mTabModel.indexOf(mTab3)).thenReturn(2);

        when(mTabModel.getTabAt(0)).thenReturn(mTab1);
        when(mTabModel.getTabAt(1)).thenReturn(mTab2);
        when(mTabModel.getTabAt(2)).thenReturn(mTab3);

        mHelper = new TabMultiSelectHelper(() -> mTabModel, mSelectTabCallback);
    }

    @Test
    public void testHandleCtrlClick_FirstClick() {
        when(mTabModel.getMultiSelectedTabsCount()).thenReturn(0);
        when(mTabModel.isTabMultiSelected(TAB2_ID)).thenReturn(false);

        mHelper.handleTabClick(TAB2_ID, KeyEvent.META_CTRL_ON);

        verify(mSelectTabCallback).onResult(TAB2_ID);
        verify(mTabModel).setTabsMultiSelected(Set.of(TAB2_ID, TAB1_ID), /* isSelected= */ true);
    }

    @Test
    public void testHandleCtrlClick_FirstClick_OnCurrentTab_DoesNotCrash() {
        when(mTabModel.getMultiSelectedTabsCount()).thenReturn(0);
        when(mTabModel.isTabMultiSelected(TAB1_ID)).thenReturn(false);

        mHelper.handleTabClick(TAB1_ID, KeyEvent.META_CTRL_ON);

        verify(mSelectTabCallback).onResult(TAB1_ID);
        verify(mTabModel).setTabsMultiSelected(Set.of(TAB1_ID), /* isSelected= */ true);
    }

    @Test
    public void testHandleCtrlClick_FirstClick_UpdatesCount() {
        when(mTabModel.getMultiSelectedTabsCount()).thenReturn(0);
        when(mTabModel.isTabMultiSelected(TAB2_ID)).thenReturn(false);

        mHelper.handleTabClick(TAB2_ID, KeyEvent.META_CTRL_ON);

        verify(mSelectTabCallback).onResult(TAB2_ID);
        verify(mTabModel).setTabsMultiSelected(Set.of(TAB2_ID, TAB1_ID), /* isSelected= */ true);
        verify(mTabModel, never()).clearMultiSelection(anyBoolean());
    }

    @Test
    public void testHandleCtrlClick_ToggleOff() {
        when(mTabModel.getMultiSelectedTabsCount()).thenReturn(2);
        when(mTabModel.isTabMultiSelected(TAB2_ID)).thenReturn(true);

        mHelper.handleTabClick(TAB2_ID, KeyEvent.META_CTRL_ON);

        verify(mTabModel).setTabsMultiSelected(Set.of(TAB2_ID), /* isSelected= */ false);
        verify(mTabModel, never()).clearMultiSelection(anyBoolean());
    }

    @Test
    public void testHandleCtrlClick_ToggleOff_ActiveTab_SwitchesToAdjacentSelectedTab() {
        when(mTabModel.getMultiSelectedTabsCount()).thenReturn(2);
        when(mTabModel.isTabMultiSelected(TAB1_ID)).thenReturn(true);
        when(mTabModel.isTabMultiSelected(TAB2_ID)).thenReturn(true);
        when(mTabModel.index()).thenReturn(1);
        when(mTabModel.getTabAt(1)).thenReturn(mTab2);

        mHelper.handleTabClick(TAB2_ID, KeyEvent.META_CTRL_ON);

        verify(mSelectTabCallback).onResult(TAB1_ID);
        verify(mTabModel).setTabsMultiSelected(Set.of(TAB2_ID), /* isSelected= */ false);
        verify(mTabModel, never()).clearMultiSelection(anyBoolean());
    }

    @Test
    public void testHandleCtrlClick_ToggleOff_DoesNotClearWhenCountDrops() {
        when(mTabModel.getMultiSelectedTabsCount()).thenReturn(1);
        when(mTabModel.isTabMultiSelected(TAB2_ID)).thenReturn(true);

        mHelper.handleTabClick(TAB2_ID, KeyEvent.META_CTRL_ON);

        verify(mTabModel).setTabsMultiSelected(Set.of(TAB2_ID), /* isSelected= */ false);
        verify(mTabModel, never()).clearMultiSelection(anyBoolean());
    }

    @Test
    public void testHandleShiftClick_Destructive() {
        mHelper.setAnchorTabIdForTesting(TAB1_ID);
        mHelper.handleTabClick(TAB3_ID, KeyEvent.META_SHIFT_ON);

        verify(mSelectTabCallback).onResult(TAB3_ID);
        verify(mTabModel).clearMultiSelection(/* notifyObservers= */ false);
        verify(mTabModel)
                .setTabsMultiSelected(Set.of(TAB1_ID, TAB2_ID, TAB3_ID), /* isSelected= */ true);
    }

    @Test
    public void testHandleShiftClick_NonDestructive() {
        mHelper.setAnchorTabIdForTesting(TAB1_ID);
        mHelper.handleTabClick(TAB3_ID, KeyEvent.META_SHIFT_ON | KeyEvent.META_CTRL_ON);

        verify(mSelectTabCallback).onResult(TAB3_ID);
        verify(mTabModel, never()).clearMultiSelection(anyBoolean());
        verify(mTabModel)
                .setTabsMultiSelected(Set.of(TAB1_ID, TAB2_ID, TAB3_ID), /* isSelected= */ true);
    }

    @Test
    public void testHandleShiftClick_BackwardRange() {
        mHelper.setAnchorTabIdForTesting(TAB3_ID);
        mHelper.handleTabClick(TAB1_ID, KeyEvent.META_SHIFT_ON);

        verify(mSelectTabCallback).onResult(TAB1_ID);
        verify(mTabModel).clearMultiSelection(/* notifyObservers= */ false);
        verify(mTabModel)
                .setTabsMultiSelected(Set.of(TAB1_ID, TAB2_ID, TAB3_ID), /* isSelected= */ true);
    }

    @Test
    public void testHandleShiftClick_AutoExpandsGroup() {
        when(mTab2.getTabGroupId()).thenReturn(GROUP_ID);
        when(mTabModel.getTabGroupCollapsed(GROUP_ID)).thenReturn(true);

        mHelper.setAnchorTabIdForTesting(TAB1_ID);
        mHelper.handleTabClick(TAB3_ID, KeyEvent.META_SHIFT_ON);

        verify(mSelectTabCallback).onResult(TAB3_ID);
        verify(mTabModel)
                .setTabGroupCollapsed(GROUP_ID, /* isCollapsed= */ false, /* animate= */ true);
        verify(mTabModel).clearMultiSelection(/* notifyObservers= */ false);
        verify(mTabModel)
                .setTabsMultiSelected(Set.of(TAB1_ID, TAB2_ID, TAB3_ID), /* isSelected= */ true);
    }

    @Test
    public void testHandleShiftClick_ExpandsGroupOnlyOnceForMultipleTabsInSameGroup() {
        when(mTab2.getTabGroupId()).thenReturn(GROUP_ID);
        when(mTab3.getTabGroupId()).thenReturn(GROUP_ID);

        mHelper.setAnchorTabIdForTesting(TAB1_ID);
        mHelper.handleTabClick(TAB3_ID, KeyEvent.META_SHIFT_ON);

        verify(mSelectTabCallback).onResult(TAB3_ID);
        verify(mTabModel)
                .setTabGroupCollapsed(GROUP_ID, /* isCollapsed= */ false, /* animate= */ true);
        verify(mTabModel).clearMultiSelection(/* notifyObservers= */ false);
        verify(mTabModel)
                .setTabsMultiSelected(Set.of(TAB1_ID, TAB2_ID, TAB3_ID), /* isSelected= */ true);
    }

    @Test
    public void testHandleShiftClick_NullTabInIndex_SkipsAndContinues() {
        when(mTabModel.getTabAt(1)).thenReturn(null);

        mHelper.setAnchorTabIdForTesting(TAB1_ID);
        mHelper.handleTabClick(TAB3_ID, KeyEvent.META_SHIFT_ON);

        verify(mSelectTabCallback).onResult(TAB3_ID);
        verify(mTabModel).clearMultiSelection(/* notifyObservers= */ false);
        verify(mTabModel).setTabsMultiSelected(Set.of(TAB1_ID, TAB3_ID), /* isSelected= */ true);
    }

    @Test
    public void testClearMultiSelection() {
        mHelper.setAnchorTabIdForTesting(TAB1_ID);
        mHelper.clearMultiSelection(/* clearAnchor= */ true, /* notifyObservers= */ true);

        assertEquals(Tab.INVALID_TAB_ID, mHelper.getAnchorTabIdForTesting());
        verify(mTabModel).clearMultiSelection(/* notifyObservers= */ true);
    }

    @Test
    public void testClearMultiSelection_PreserveAnchor_WithoutNotifyObservers() {
        mHelper.setAnchorTabIdForTesting(TAB1_ID);
        mHelper.clearMultiSelection(/* clearAnchor= */ false, /* notifyObservers= */ false);

        assertEquals(TAB1_ID, mHelper.getAnchorTabIdForTesting());
        verify(mTabModel).clearMultiSelection(/* notifyObservers= */ false);
    }

    @Test
    public void testHandleTabClick_NormalClick_ClearsSelection() {
        mHelper.setAnchorTabIdForTesting(TAB1_ID);

        assertFalse(mHelper.handleTabClick(TAB2_ID, 0));
        assertEquals(Tab.INVALID_TAB_ID, mHelper.getAnchorTabIdForTesting());
        verify(mTabModel).clearMultiSelection(/* notifyObservers= */ true);
    }

    @Test
    public void testHandleTabClick_CtrlClick_TogglesSelectionAndResetsAnchor() {
        when(mTabModel.getMultiSelectedTabsCount()).thenReturn(0);
        mHelper.setAnchorTabIdForTesting(TAB1_ID);

        assertTrue(mHelper.handleTabClick(TAB2_ID, KeyEvent.META_CTRL_ON));
        assertEquals(Tab.INVALID_TAB_ID, mHelper.getAnchorTabIdForTesting());

        verify(mSelectTabCallback).onResult(TAB2_ID);
        verify(mTabModel).setTabsMultiSelected(Set.of(TAB2_ID, TAB1_ID), /* isSelected= */ true);
    }

    @Test
    public void testHandleTabClick_ShiftClick_DestructiveRange() {
        mHelper.setAnchorTabIdForTesting(TAB1_ID);

        assertTrue(mHelper.handleTabClick(TAB3_ID, KeyEvent.META_SHIFT_ON));
        assertEquals(TAB1_ID, mHelper.getAnchorTabIdForTesting());

        verify(mSelectTabCallback).onResult(TAB3_ID);
        verify(mTabModel).clearMultiSelection(/* notifyObservers= */ false);
        verify(mTabModel)
                .setTabsMultiSelected(Set.of(TAB1_ID, TAB2_ID, TAB3_ID), /* isSelected= */ true);
    }

    @Test
    public void testHandleTabClick_CtrlShiftClick_AdditiveRange() {
        mHelper.setAnchorTabIdForTesting(TAB1_ID);

        assertTrue(mHelper.handleTabClick(TAB3_ID, KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON));
        assertEquals(TAB1_ID, mHelper.getAnchorTabIdForTesting());

        verify(mSelectTabCallback).onResult(TAB3_ID);
        verify(mTabModel, never()).clearMultiSelection(anyBoolean());
        verify(mTabModel)
                .setTabsMultiSelected(Set.of(TAB1_ID, TAB2_ID, TAB3_ID), /* isSelected= */ true);
    }

    @Test
    public void testMultiselectKeyboardFocusedItem_Select() {
        when(mTabModel.isTabMultiSelected(TAB2_ID)).thenReturn(false);

        mHelper.multiselectKeyboardFocusedItem(TAB2_ID);

        verify(mTabModel).setTabsMultiSelected(Set.of(TAB2_ID, TAB1_ID), /* isSelected= */ true);
    }

    @Test
    public void testMultiselectKeyboardFocusedItem_Unselect() {
        when(mTabModel.isTabMultiSelected(TAB2_ID)).thenReturn(true);

        mHelper.multiselectKeyboardFocusedItem(TAB2_ID);

        verify(mTabModel).setTabsMultiSelected(Set.of(TAB2_ID), /* isSelected= */ false);
    }

    @Test
    public void testHasMultipleTabsSelected() {
        assertFalse(TabMultiSelectHelper.hasMultipleTabsSelected(null));

        when(mTabModel.getMultiSelectedTabsCount()).thenReturn(0);
        assertFalse(TabMultiSelectHelper.hasMultipleTabsSelected(mTabModel));

        when(mTabModel.getMultiSelectedTabsCount()).thenReturn(1);
        assertFalse(TabMultiSelectHelper.hasMultipleTabsSelected(mTabModel));

        when(mTabModel.getMultiSelectedTabsCount()).thenReturn(2);
        assertTrue(TabMultiSelectHelper.hasMultipleTabsSelected(mTabModel));

        when(mTabModel.getMultiSelectedTabsCount()).thenReturn(5);
        assertTrue(TabMultiSelectHelper.hasMultipleTabsSelected(mTabModel));
    }
}
