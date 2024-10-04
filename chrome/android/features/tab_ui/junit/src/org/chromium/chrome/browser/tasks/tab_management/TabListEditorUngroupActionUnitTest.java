// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationManager.ConfirmationResult;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ActionDelegate;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ActionObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.IconPosition;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ShowMode;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

/** Unit tests for {@link TabListEditorUngroupAction}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabListEditorUngroupActionUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SelectionDelegate<Integer> mSelectionDelegate;
    @Mock private TabGroupModelFilter mGroupFilter;
    @Mock private ActionDelegate mDelegate;
    @Mock private Profile mProfile;
    @Mock private ActionConfirmationManager mActionConfirmationManager;

    @Captor private ArgumentCaptor<Callback<Integer>> mConfirmationResultCaptor;

    private MockTabModel mTabModel;
    private TabListEditorAction mAction;

    @Before
    public void setUp() {
        mAction =
                TabListEditorUngroupAction.createAction(
                        RuntimeEnvironment.application,
                        ShowMode.MENU_ONLY,
                        ButtonType.TEXT,
                        IconPosition.START,
                        mActionConfirmationManager);
        mTabModel = spy(new MockTabModel(mProfile, null));
        when(mGroupFilter.getTabModel()).thenReturn(mTabModel);
        mAction.configure(() -> mGroupFilter, mSelectionDelegate, mDelegate, false);
    }

    @Test
    @SmallTest
    public void testInherentActionProperties() {
        Assert.assertEquals(
                R.id.tab_list_editor_ungroup_menu_item,
                mAction.getPropertyModel().get(TabListEditorActionProperties.MENU_ITEM_ID));
        Assert.assertEquals(
                R.plurals.tab_selection_editor_ungroup_tabs,
                mAction.getPropertyModel()
                        .get(TabListEditorActionProperties.TITLE_RESOURCE_ID));
        Assert.assertEquals(
                true,
                mAction.getPropertyModel().get(TabListEditorActionProperties.TITLE_IS_PLURAL));
        Assert.assertEquals(
                R.plurals.accessibility_tab_selection_editor_ungroup_tabs,
                mAction.getPropertyModel()
                        .get(TabListEditorActionProperties.CONTENT_DESCRIPTION_RESOURCE_ID)
                        .intValue());
        Assert.assertNotNull(
                mAction.getPropertyModel().get(TabListEditorActionProperties.ICON));
    }

    @Test
    @SmallTest
    public void testUngroupActionDisabled() {
        List<Integer> tabIds = new ArrayList<>();
        mAction.onSelectionStateChange(tabIds);
        Assert.assertEquals(
                false, mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        Assert.assertEquals(
                0, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));
    }

    @Test
    @SmallTest
    public void testUngroupActionWithTabs() throws Exception {
        List<Integer> tabIds = Arrays.asList(5, 3, 7);
        List<Tab> tabs = new ArrayList<>();
        for (int id : tabIds) {
            tabs.add(mTabModel.addTab(id));
        }
        when(mGroupFilter.getRelatedTabList(anyInt())).thenReturn(tabs);
        Set<Integer> tabIdsSet = new LinkedHashSet<>(tabIds);
        when(mSelectionDelegate.getSelectedItems()).thenReturn(tabIdsSet);

        mAction.onSelectionStateChange(tabIds);
        Assert.assertEquals(
                true, mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        Assert.assertEquals(
                3, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));

        final CallbackHelper helper = new CallbackHelper();
        ActionObserver observer =
                new ActionObserver() {
                    @Override
                    public void preProcessSelectedTabs(List<Tab> tabs) {
                        helper.notifyCalled();
                    }
                };
        mAction.addActionObserver(observer);

        assertTrue(mAction.perform());
        verify(mActionConfirmationManager)
                .processUngroupTabAttempt(any(), mConfirmationResultCaptor.capture());
        mConfirmationResultCaptor.getValue().onResult(ConfirmationResult.CONFIRMATION_POSITIVE);
        for (int id : tabIds) {
            verify(mGroupFilter).moveTabOutOfGroupInDirection(id, /* trailing= */ true);
        }
        verify(mDelegate).hideByAction();

        helper.waitForOnly();
        mAction.removeActionObserver(observer);

        assertTrue(mAction.perform());
        verify(mActionConfirmationManager, times(2))
                .processUngroupTabAttempt(any(), mConfirmationResultCaptor.capture());
        mConfirmationResultCaptor.getValue().onResult(ConfirmationResult.CONFIRMATION_POSITIVE);
        for (int id : tabIds) {
            verify(mGroupFilter, times(2)).moveTabOutOfGroupInDirection(id, /* trailing= */ true);
        }
        verify(mDelegate, times(2)).hideByAction();
        Assert.assertEquals(1, helper.getCallCount());
    }

    @Test
    @SmallTest
    public void testPerformAction_ImmediateContinue() {
        List<Integer> tabIds = Arrays.asList(5, 3, 7);
        List<Tab> tabs = new ArrayList<>();
        for (int id : tabIds) {
            tabs.add(mTabModel.addTab(id));
        }
        when(mGroupFilter.getRelatedTabList(anyInt())).thenReturn(tabs);

        assertTrue(mAction.performAction(tabs));
        verify(mActionConfirmationManager)
                .processUngroupTabAttempt(any(), mConfirmationResultCaptor.capture());
        mConfirmationResultCaptor.getValue().onResult(ConfirmationResult.IMMEDIATE_CONTINUE);

        for (int id : tabIds) {
            verify(mGroupFilter).moveTabOutOfGroupInDirection(id, /* trailing= */ true);
        }
    }

    @Test
    @SmallTest
    public void testPerformAction_ConfirmationNegative() {
        List<Integer> tabIds = Arrays.asList(5, 3, 7);
        List<Tab> tabs = new ArrayList<>();
        for (int id : tabIds) {
            tabs.add(mTabModel.addTab(id));
        }
        when(mGroupFilter.getRelatedTabList(anyInt())).thenReturn(tabs);

        assertTrue(mAction.performAction(tabs));
        verify(mActionConfirmationManager)
                .processUngroupTabAttempt(any(), mConfirmationResultCaptor.capture());
        mConfirmationResultCaptor.getValue().onResult(ConfirmationResult.CONFIRMATION_NEGATIVE);

        verify(mGroupFilter, never()).moveTabOutOfGroupInDirection(anyInt(), anyBoolean());
    }

    @Test
    @SmallTest
    public void testPerformAction_PartialGroup() {
        List<Integer> tabIds = Arrays.asList(5, 3, 7);
        List<Tab> tabs = new ArrayList<>();
        for (int id : tabIds) {
            tabs.add(mTabModel.addTab(id));
        }
        when(mGroupFilter.getRelatedTabList(anyInt())).thenReturn(tabs);

        List<Tab> tabsToRemove = Arrays.asList(mTabModel.getTabAt(0), mTabModel.getTabAt(1));
        assertTrue(mAction.performAction(tabsToRemove));
        verify(mActionConfirmationManager, never()).processUngroupTabAttempt(any(), any());

        for (Tab tab : tabsToRemove) {
            verify(mGroupFilter).moveTabOutOfGroupInDirection(tab.getId(), /* trailing= */ true);
        }
    }
}
