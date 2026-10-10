// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.graphics.Rect;
import android.widget.ListView;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.DeviceInfo;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils.ExpandOnHoverToggleEntryPoint;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.base.DeviceInput;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.RectProvider;

/** Unit tests for {@link VerticalTabCollapseButtonContextMenuCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class VerticalTabCollapseButtonContextMenuCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Runnable mOnMenuDismissed;

    private Activity mActivity;
    private VerticalTabCollapseButtonContextMenuCoordinator mCoordinator;
    private RectProvider mRectProvider;

    @Before
    public void setUp() {
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ANDROID_VERTICAL_TABS, "expand_on_hover", true);
        DeviceInfo.setIsDesktopForTesting(true);
        DeviceInput.setSupportsPrecisionPointerForTesting(true);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mRectProvider = new RectProvider(new Rect(10, 10, 50, 50));
        mCoordinator =
                new VerticalTabCollapseButtonContextMenuCoordinator(mActivity, mOnMenuDismissed);
    }

    @After
    public void tearDown() {
        mCoordinator.destroy();
        VerticalTabUtils.resetSharedPrefsForTesting();
    }

    @Test
    public void testShowMenu_ExpandOnHoverOn_ShowsCheckedItem() {
        mCoordinator.showMenu(mRectProvider, /* isIncognito= */ false);

        assertTrue(mCoordinator.isMenuShowing());
        PropertyModel itemModel = getOnlyItemModel();
        assertEquals(
                R.id.toggle_expand_tabs_on_hover_menu_id,
                itemModel.get(ListMenuItemProperties.MENU_ITEM_ID));
        assertEquals(R.string.expand_tabs_on_hover, itemModel.get(ListMenuItemProperties.TITLE_ID));
        assertTrue(itemModel.get(ListMenuItemProperties.CHECKABLE));
        assertTrue(itemModel.get(ListMenuItemProperties.CHECKED));
        assertEquals(
                R.drawable.material_ic_check_24dp,
                itemModel.get(ListMenuItemProperties.END_ICON_ID));
    }

    @Test
    public void testShowMenu_ExpandOnHoverOff_ShowsUncheckedItem() {
        VerticalTabUtils.setExpandOnHoverEnabledInSharedPref(
                false, ExpandOnHoverToggleEntryPoint.SETTINGS);

        mCoordinator.showMenu(mRectProvider, /* isIncognito= */ false);

        PropertyModel itemModel = getOnlyItemModel();
        assertEquals(R.string.expand_tabs_on_hover, itemModel.get(ListMenuItemProperties.TITLE_ID));
        assertTrue(itemModel.get(ListMenuItemProperties.CHECKABLE));
        assertFalse(itemModel.get(ListMenuItemProperties.CHECKED));
        assertEquals(
                android.R.color.transparent, itemModel.get(ListMenuItemProperties.END_ICON_ID));
    }

    @Test
    public void testSelectItem_TogglesSettingAndDismisses() {
        mCoordinator.showMenu(mRectProvider, /* isIncognito= */ false);
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.VerticalTabs.ExpandOnHoverToggle.Disable",
                        ExpandOnHoverToggleEntryPoint.COLLAPSE_BUTTON_CONTEXT_MENU);

        clickOnlyItem();

        assertFalse(VerticalTabUtils.isExpandOnHoverEnabled());
        histogramWatcher.assertExpected();
        assertFalse(mCoordinator.isMenuShowing());
        verify(mOnMenuDismissed).run();

        // Selecting the item again turns the setting back on.
        mCoordinator.showMenu(mRectProvider, /* isIncognito= */ false);
        clickOnlyItem();
        assertTrue(VerticalTabUtils.isExpandOnHoverEnabled());
    }

    @Test
    public void testDismiss() {
        mCoordinator.showMenu(mRectProvider, /* isIncognito= */ false);
        assertTrue(mCoordinator.isMenuShowing());

        mCoordinator.dismiss();

        assertFalse(mCoordinator.isMenuShowing());
        verify(mOnMenuDismissed).run();
    }

    private ListView getListView() {
        assertNotNull(mCoordinator.getPopupWindowForTesting());
        return mCoordinator
                .getPopupWindowForTesting()
                .getContentView()
                .findViewById(R.id.tab_group_action_menu_list);
    }

    private PropertyModel getOnlyItemModel() {
        ListView listView = getListView();
        assertEquals(1, listView.getAdapter().getCount());
        return ((ListItem) listView.getAdapter().getItem(0)).model;
    }

    private void clickOnlyItem() {
        mCoordinator.getListMenuDelegate().onItemSelected(getOnlyItemModel(), getListView());
    }
}
