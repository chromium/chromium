// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ActionDelegate;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link TabListEditorCloseArchivedTabsAction}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabListEditorCloseArchivedTabsActionUnitTest {
    @Mock private TabGroupModelFilter mTabModelFilter;
    @Mock private SelectionDelegate<Integer> mSelectionDelegate;
    @Mock private ActionDelegate mDelegate;
    @Mock private Profile mProfile;

    @Mock private ArchivedTabsDialogCoordinator.ArchiveDelegate mArchiveDelegate;

    private MockTabModel mTabModel;
    private TabListEditorCloseArchivedTabsAction mAction;
    private Activity mActivity;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).get();
        mAction =
                (TabListEditorCloseArchivedTabsAction)
                        TabListEditorCloseArchivedTabsAction.createAction(
                                mActivity, mArchiveDelegate);
        mTabModel = spy(new MockTabModel(mProfile, null));
        when(mTabModelFilter.getTabModel()).thenReturn(mTabModel);
        mAction.configure(() -> mTabModelFilter, mSelectionDelegate, mDelegate, false);
    }

    @Test
    @SmallTest
    public void testInherentActionProperties() {
        Assert.assertEquals(
                R.id.tab_list_editor_close_archived_tabs_menu_item,
                mAction.getPropertyModel().get(TabListEditorActionProperties.MENU_ITEM_ID));
        Assert.assertEquals(
                R.plurals.tab_selection_editor_close_tabs,
                mAction.getPropertyModel().get(TabListEditorActionProperties.TITLE_RESOURCE_ID));
        Assert.assertEquals(
                true,
                mAction.getPropertyModel().get(TabListEditorActionProperties.TITLE_IS_PLURAL));
        Assert.assertEquals(
                R.plurals.accessibility_tab_selection_editor_close_tabs,
                mAction.getPropertyModel()
                        .get(TabListEditorActionProperties.CONTENT_DESCRIPTION_RESOURCE_ID)
                        .intValue());
        Assert.assertNull(mAction.getPropertyModel().get(TabListEditorActionProperties.ICON));
    }

    @Test
    @SmallTest
    public void testPerformAction() {
        List<Tab> tabs = new ArrayList<>();
        mAction.performAction(tabs);
        verify(mArchiveDelegate).closeArchivedTabs(tabs);
    }
}
