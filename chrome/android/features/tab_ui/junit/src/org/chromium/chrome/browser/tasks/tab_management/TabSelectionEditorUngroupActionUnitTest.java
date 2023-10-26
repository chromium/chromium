// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.ActionDelegate;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.ActionObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.ButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.IconPosition;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.ShowMode;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;

import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

/** Unit tests for {@link TabSelectionEditorUngroupAction}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabSelectionEditorUngroupActionUnitTest {
    @Rule public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private SelectionDelegate<Integer> mSelectionDelegate;
    @Mock private TabModelFilterProvider mTabModelFilterProvider;
    @Mock private TabGroupModelFilter mGroupFilter;
    @Mock private ActionDelegate mDelegate;
    @Mock private Profile mProfile;
    private MockTabModel mTabModel;
    private TabSelectionEditorAction mAction;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mAction =
                TabSelectionEditorUngroupAction.createAction(
                        RuntimeEnvironment.application,
                        ShowMode.MENU_ONLY,
                        ButtonType.TEXT,
                        IconPosition.START);
        mTabModel = spy(new MockTabModel(mProfile, null));
        when(mTabModelFilterProvider.getCurrentTabModelFilter()).thenReturn(mGroupFilter);
        when(mTabModelSelector.getTabModelFilterProvider()).thenReturn(mTabModelFilterProvider);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        mAction.configure(mTabModelSelector, mSelectionDelegate, mDelegate, false);
    }

    @Test
    @SmallTest
    public void testInherentActionProperties() {
        Assert.assertEquals(
                R.id.tab_selection_editor_ungroup_menu_item,
                mAction.getPropertyModel().get(TabSelectionEditorActionProperties.MENU_ITEM_ID));
        Assert.assertEquals(
                R.plurals.tab_selection_editor_ungroup_tabs,
                mAction.getPropertyModel()
                        .get(TabSelectionEditorActionProperties.TITLE_RESOURCE_ID));
        Assert.assertEquals(
                true,
                mAction.getPropertyModel().get(TabSelectionEditorActionProperties.TITLE_IS_PLURAL));
        Assert.assertEquals(
                R.plurals.accessibility_tab_selection_editor_ungroup_tabs,
                mAction.getPropertyModel()
                        .get(TabSelectionEditorActionProperties.CONTENT_DESCRIPTION_RESOURCE_ID)
                        .intValue());
        Assert.assertNotNull(
                mAction.getPropertyModel().get(TabSelectionEditorActionProperties.ICON));
    }

    @Test
    @SmallTest
    public void testUngroupActionDisabled() {
        List<Integer> tabIds = new ArrayList<>();
        mAction.onSelectionStateChange(tabIds);
        Assert.assertEquals(
                false, mAction.getPropertyModel().get(TabSelectionEditorActionProperties.ENABLED));
        Assert.assertEquals(
                0, mAction.getPropertyModel().get(TabSelectionEditorActionProperties.ITEM_COUNT));
    }

    @Test
    @SmallTest
    public void testUngroupActionWithTabs() throws Exception {
        List<Integer> tabIds = new ArrayList<>();
        tabIds.add(5);
        tabIds.add(3);
        tabIds.add(7);
        for (int id : tabIds) {
            mTabModel.addTab(id);
        }
        Set<Integer> tabIdsSet = new LinkedHashSet<>(tabIds);
        when(mSelectionDelegate.getSelectedItems()).thenReturn(tabIdsSet);

        mAction.onSelectionStateChange(tabIds);
        Assert.assertEquals(
                true, mAction.getPropertyModel().get(TabSelectionEditorActionProperties.ENABLED));
        Assert.assertEquals(
                3, mAction.getPropertyModel().get(TabSelectionEditorActionProperties.ITEM_COUNT));

        final CallbackHelper helper = new CallbackHelper();
        ActionObserver observer =
                new ActionObserver() {
                    @Override
                    public void preProcessSelectedTabs(List<Tab> tabs) {
                        helper.notifyCalled();
                    }
                };
        mAction.addActionObserver(observer);

        Assert.assertTrue(mAction.perform());
        for (int id : tabIds) {
            verify(mGroupFilter).moveTabOutOfGroup(id);
        }
        verify(mDelegate).hideByAction();

        helper.waitForFirst();
        mAction.removeActionObserver(observer);

        Assert.assertTrue(mAction.perform());
        for (int id : tabIds) {
            verify(mGroupFilter, times(2)).moveTabOutOfGroup(id);
        }
        verify(mDelegate, times(2)).hideByAction();
        Assert.assertEquals(1, helper.getCallCount());
    }
}
