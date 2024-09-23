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
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ActionDelegate;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;

/** Unit tests for {@link TabListEditorRestoreAllArchivedTabsAction}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabListEditorRestoreAllArchivedTabsActionUnitTest {
    @Mock private TabGroupModelFilter mTabModelFilter;
    @Mock private SelectionDelegate<Integer> mSelectionDelegate;
    @Mock private ActionDelegate mDelegate;
    @Mock private Profile mProfile;

    @Mock private ArchivedTabsDialogCoordinator.ArchiveDelegate mArchiveDelegate;

    private MockTabModel mTabModel;
    private TabListEditorRestoreAllArchivedTabsAction mAction;
    private Activity mActivity;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).get();
        mAction =
                (TabListEditorRestoreAllArchivedTabsAction)
                        TabListEditorRestoreAllArchivedTabsAction.createAction(
                                mActivity, mArchiveDelegate);
        mTabModel = spy(new MockTabModel(mProfile, null));
        when(mTabModelFilter.getTabModel()).thenReturn(mTabModel);
        mAction.configure(() -> mTabModelFilter, mSelectionDelegate, mDelegate, false);
    }

    @Test
    @SmallTest
    public void testInherentActionProperties() {
        Assert.assertEquals(
                R.id.tab_list_editor_restore_all_archived_tabs_menu_item,
                mAction.getPropertyModel().get(TabListEditorActionProperties.MENU_ITEM_ID));
        Assert.assertEquals(
                R.string.archived_tabs_dialog_restore_all_action,
                mAction.getPropertyModel().get(TabListEditorActionProperties.TITLE_RESOURCE_ID));
        Assert.assertEquals(
                false,
                mAction.getPropertyModel().get(TabListEditorActionProperties.TITLE_IS_PLURAL));
        Assert.assertNull(
                mAction.getPropertyModel()
                        .get(TabListEditorActionProperties.CONTENT_DESCRIPTION_RESOURCE_ID));
        Assert.assertNull(mAction.getPropertyModel().get(TabListEditorActionProperties.ICON));
    }

    @Test
    @SmallTest
    public void testPerformAction() {
        mAction.performAction(null);
        verify(mArchiveDelegate).restoreAllArchivedTabs();
    }
}
