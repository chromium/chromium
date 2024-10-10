// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ActionDelegate;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link TabListEditorRestoreArchivedTabsAction}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabListEditorRestoreArchivedTabsActionUnitTest {
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private SelectionDelegate<Integer> mSelectionDelegate;
    @Mock private ActionDelegate mDelegate;
    @Mock private Profile mProfile;

    @Mock private ArchivedTabsDialogCoordinator.ArchiveDelegate mArchiveDelegate;

    private MockTabModel mTabModel;
    private TabListEditorRestoreArchivedTabsAction mAction;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mAction =
                (TabListEditorRestoreArchivedTabsAction)
                        TabListEditorRestoreArchivedTabsAction.createAction(mArchiveDelegate);
        mTabModel = spy(new MockTabModel(mProfile, null));
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        mAction.configure(() -> mTabGroupModelFilter, mSelectionDelegate, mDelegate, false);
    }

    @Test
    @SmallTest
    public void testInherentActionProperties() {
        Assert.assertEquals(
                R.id.tab_list_editor_restore_archived_tabs_menu_item,
                mAction.getPropertyModel().get(TabListEditorActionProperties.MENU_ITEM_ID));
        Assert.assertEquals(
                R.plurals.archived_tabs_dialog_restore_action,
                mAction.getPropertyModel().get(TabListEditorActionProperties.TITLE_RESOURCE_ID));
        Assert.assertEquals(
                true,
                mAction.getPropertyModel().get(TabListEditorActionProperties.TITLE_IS_PLURAL));
        Assert.assertNull(
                mAction.getPropertyModel()
                        .get(TabListEditorActionProperties.CONTENT_DESCRIPTION_RESOURCE_ID));
        Assert.assertNull(mAction.getPropertyModel().get(TabListEditorActionProperties.ICON));
    }

    @Test
    @SmallTest
    public void testPerformAction() {
        List<Tab> tabs = new ArrayList<>();
        mAction.performAction(tabs);
        verify(mArchiveDelegate).restoreArchivedTabs(tabs);
    }
}
