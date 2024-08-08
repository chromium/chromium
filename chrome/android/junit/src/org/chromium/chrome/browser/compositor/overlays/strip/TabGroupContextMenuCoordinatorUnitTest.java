// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupOverflowMenuCoordinator.OnItemClickedCallback;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupVisualDataTextInputLayout;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.listmenu.BasicListMenu.ListMenuItemType;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

/** Unit tests for {@link TabGroupContextMenuCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({
    ChromeFeatureList.TAB_STRIP_GROUP_CONTEXT_MENU,
    ChromeFeatureList.TAB_GROUP_PARITY_ANDROID
})
public class TabGroupContextMenuCoordinatorUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private Activity mActivity;
    private TabGroupContextMenuCoordinator mTabGroupContextMenuCoordinator;
    @Mock private View mMenuView;
    @Mock private OnItemClickedCallback mOnItemClickedCallback;
    @Mock private Supplier<TabModel> mTabModelSupplier;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabModel mTabModel;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        LayoutInflater inflater = LayoutInflater.from(mActivity);
        mMenuView = inflater.inflate(R.layout.tab_strip_group_menu_layout, null);
        mTabGroupContextMenuCoordinator =
                new TabGroupContextMenuCoordinator(
                        mOnItemClickedCallback,
                        mTabModelSupplier,
                        mTabGroupModelFilter,
                        /* shouldShowDeleteGroup= */ true);
    }

    @Test
    public void testListMenuItems() {
        ModelList modelList = new ModelList();
        mTabGroupContextMenuCoordinator.buildMenuActionItems(
                modelList,
                /* isIncognito= */ false,
                /* shouldShowDeleteGroup= */ true,
                /* hasCollaborationData= */ false);

        // Assert: verify number of items in the model list.
        assertEquals("Number of items in the list menu is incorrect", 6, modelList.size());

        // Assert: verify item type or id.
        verifyNormalListItems(modelList);
        assertEquals(ListMenuItemType.DIVIDER, modelList.get(4).type);
        assertEquals(
                R.id.delete_tab, modelList.get(5).model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    public void testListMenuItems_Incognito() {
        ModelList modelList = new ModelList();
        mTabGroupContextMenuCoordinator.buildMenuActionItems(
                modelList,
                /* isIncognito= */ true,
                /* shouldShowDeleteGroup= */ false,
                /* hasCollaborationData= */ false);

        // Assert: verify number of items in the model list.
        assertEquals("Number of items in the list menu is incorrect", 4, modelList.size());

        // Assert: verify divider or menu item id.
        verifyNormalListItems(modelList);
    }

    @Test
    public void testCustomMenuItems() {
        mTabGroupContextMenuCoordinator.buildCustomView(mMenuView, /* isIncognito= */ false);

        // Verify text input layout.
        TabGroupVisualDataTextInputLayout tabGroupTextInputLayout =
                mTabGroupContextMenuCoordinator.getTabGroupTextInputLayoutForTesting();
        assertNotNull(tabGroupTextInputLayout);

        // Verify color picker.
        ColorPickerCoordinator colorPickerCoordinator =
                mTabGroupContextMenuCoordinator.getColorPickerCoordinatorForTesting();
        assertNotNull(colorPickerCoordinator);
    }

    private void verifyNormalListItems(ModelList modelList) {
        assertEquals(ListMenuItemType.DIVIDER, modelList.get(0).type);
        assertEquals(
                R.id.open_new_tab_in_group,
                modelList.get(1).model.get(ListMenuItemProperties.MENU_ITEM_ID));
        assertEquals(
                R.id.ungroup_tab, modelList.get(2).model.get(ListMenuItemProperties.MENU_ITEM_ID));
        assertEquals(
                R.id.close_tab, modelList.get(3).model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }
}
