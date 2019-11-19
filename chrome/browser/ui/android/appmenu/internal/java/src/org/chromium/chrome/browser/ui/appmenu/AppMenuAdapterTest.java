// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import android.graphics.drawable.Drawable;
import android.support.test.filters.MediumTest;
import android.support.v7.content.res.AppCompatResources;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.SubMenu;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.ui.appmenu.test.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DummyUiActivityTestCase;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Tests for {@link AppMenuAdapter}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class AppMenuAdapterTest extends DummyUiActivityTestCase {
    static class TestClickHandler implements AppMenuAdapter.OnClickHandler {
        public CallbackHelper onClickCallback = new CallbackHelper();
        public MenuItem lastClickedItem;

        public CallbackHelper onLongClickCallback = new CallbackHelper();
        public MenuItem lastLongClickedItem;

        @Override
        public void onItemClick(MenuItem menuItem) {
            onClickCallback.notifyCalled();
            lastClickedItem = menuItem;
        }

        @Override
        public boolean onItemLongClick(MenuItem menuItem, View view) {
            onLongClickCallback.notifyCalled();
            lastLongClickedItem = menuItem;
            return true;
        }
    }

    static final String TITLE_1 = "Menu Item One";
    static final String TITLE_2 = "Menu Item Two";
    static final String TITLE_3 = "Menu Item Three";
    static final String TITLE_4 = "Menu Item Four";
    static final String TITLE_5 = "Menu Item Five";

    private TestClickHandler mClickHandler;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        MockitoAnnotations.initMocks(this);
        mClickHandler = new TestClickHandler();
    }

    @Test
    @MediumTest
    public void testStandardMenuItem() throws ExecutionException, TimeoutException {
        int itemId = 1234;
        List<MenuItem> items = new ArrayList<>();
        items.add(buildMenuItem(itemId, TITLE_1));

        AppMenuAdapter adapter = new AppMenuAdapter(
                mClickHandler, items, getActivity().getLayoutInflater(), 0, null);
        Assert.assertEquals("Wrong item view type", AppMenuAdapter.MenuItemType.STANDARD,
                adapter.getItemViewType(0));

        ViewGroup parentView = getActivity().findViewById(android.R.id.content);
        View view1 = adapter.getView(0, null, parentView);
        TextView titleView1 = view1.findViewById(R.id.menu_item_text);

        Assert.assertEquals("Incorrect title text for item 1", TITLE_1, titleView1.getText());

        TestThreadUtils.runOnUiThreadBlocking(() -> view1.performClick());
        mClickHandler.onClickCallback.waitForCallback(0);
        Assert.assertEquals(
                "Incorrect clicked item id", itemId, mClickHandler.lastClickedItem.getItemId());
    }

    @Test
    @MediumTest
    public void testConvertView_Reused_StandardMenuItem() {
        List<MenuItem> items = new ArrayList<>();
        items.add(buildMenuItem(1, TITLE_1));
        items.add(buildMenuItem(2, TITLE_2));

        AppMenuAdapter adapter = new AppMenuAdapter(
                mClickHandler, items, getActivity().getLayoutInflater(), 0, null);

        ViewGroup parentView = getActivity().findViewById(android.R.id.content);
        View view1 = adapter.getView(0, null, parentView);
        TextView titleView = view1.findViewById(R.id.menu_item_text);

        Assert.assertEquals("Incorrect title text for item 1", TITLE_1, titleView.getText());

        View view2 = adapter.getView(1, view1, parentView);
        Assert.assertEquals("Convert view should have been re-used", view1, view2);
        Assert.assertEquals("Title should have been updated", TITLE_2, titleView.getText());
    }

    @Test
    @MediumTest
    public void testConvertView_Reused_TitleMenuItem() {
        List<MenuItem> items = new ArrayList<>();
        items.add(buildTitleMenuItem(1, 2, TITLE_1, 3, TITLE_2));
        items.add(buildTitleMenuItem(4, 5, TITLE_3, 6, TITLE_4));

        AppMenuAdapter adapter = new AppMenuAdapter(
                mClickHandler, items, getActivity().getLayoutInflater(), 0, null);

        Assert.assertEquals("Wrong item view type", AppMenuAdapter.MenuItemType.TITLE_BUTTON,
                adapter.getItemViewType(0));

        ViewGroup parentView = getActivity().findViewById(android.R.id.content);
        View view1 = adapter.getView(0, null, parentView);
        TextView titleView = view1.findViewById(R.id.title);

        Assert.assertEquals("Incorrect title text for item 1", TITLE_1, titleView.getText());

        View view2 = adapter.getView(1, view1, parentView);
        Assert.assertEquals("Convert view should have been re-used", view1, view2);
        Assert.assertEquals("Title should have been updated", TITLE_3, titleView.getText());
    }

    @Test
    @MediumTest
    public void testConvertView_Reused_IconRow() {
        Drawable icon =
                AppCompatResources.getDrawable(getActivity(), R.drawable.test_ic_vintage_filter);
        List<MenuItem> items = new ArrayList<>();
        items.add(buildIconRow(1, 2, TITLE_1, icon, 3, TITLE_2, icon, 4, "", icon, 0, null, null, 0,
                null, null, true));
        items.add(buildIconRow(5, 6, TITLE_3, icon, 7, TITLE_4, icon, 8, "", icon, 0, null, null, 0,
                null, null, true));

        AppMenuAdapter adapter = new AppMenuAdapter(
                mClickHandler, items, getActivity().getLayoutInflater(), 0, null);

        ViewGroup parentView = getActivity().findViewById(android.R.id.content);
        View view1 = adapter.getView(0, null, parentView);
        View buttonOne = view1.findViewById(R.id.button_one);

        Assert.assertEquals("Incorrect content description for item 1", TITLE_1,
                buttonOne.getContentDescription());

        View view2 = adapter.getView(1, view1, parentView);
        Assert.assertEquals("Convert view should have been re-used", view1, view2);
        Assert.assertEquals("Content description should have been updated", TITLE_3,
                buttonOne.getContentDescription());
    }

    @Test
    @MediumTest
    public void testConvertView_NotReused() {
        List<MenuItem> items = new ArrayList<>();
        items.add(buildMenuItem(1, TITLE_1));
        items.add(buildTitleMenuItem(2, 3, TITLE_2, 4, TITLE_3));

        AppMenuAdapter adapter = new AppMenuAdapter(
                mClickHandler, items, getActivity().getLayoutInflater(), 0, null);

        Assert.assertEquals("Wrong item view type for item 1", AppMenuAdapter.MenuItemType.STANDARD,
                adapter.getItemViewType(0));
        Assert.assertEquals("Wrong item view type for item 2",
                AppMenuAdapter.MenuItemType.TITLE_BUTTON, adapter.getItemViewType(1));

        ViewGroup parentView = getActivity().findViewById(android.R.id.content);
        View view1 = adapter.getView(0, null, parentView);
        TextView titleView = view1.findViewById(R.id.menu_item_text);

        Assert.assertEquals("Incorrect title text for item 1", TITLE_1, titleView.getText());

        View view2 = adapter.getView(1, view1, parentView);
        Assert.assertNotEquals("Standard view should not have been re-used", view1, view2);
        Assert.assertEquals(
                "Title for view 1 should have not have been updated", TITLE_1, titleView.getText());

        View view3 = adapter.getView(0, view2, parentView);
        Assert.assertNotEquals("Title button view should not have been re-used", view2, view3);
    }

    @Test
    @MediumTest
    public void testConvertView_NotReused_IconRow() {
        Drawable icon =
                AppCompatResources.getDrawable(getActivity(), R.drawable.test_ic_vintage_filter);
        List<MenuItem> items = new ArrayList<>();
        items.add(buildIconRow(1, 2, TITLE_1, icon, 3, TITLE_2, icon, 4, "", icon, 0, null, null, 0,
                null, null, true));
        items.add(buildIconRow(5, 6, TITLE_3, icon, 7, TITLE_4, icon, 8, "", icon, 9, TITLE_5, icon,
                0, null, null, true));

        AppMenuAdapter adapter = new AppMenuAdapter(
                mClickHandler, items, getActivity().getLayoutInflater(), 0, null);

        ViewGroup parentView = getActivity().findViewById(android.R.id.content);
        View view1 = adapter.getView(0, null, parentView);
        View view2 = adapter.getView(1, view1, parentView);
        Assert.assertNotEquals("Convert view should not have been re-used", view1, view2);
    }

    @Test
    @MediumTest
    public void testCustomViewBinders() {
        // Set-up custom binders.
        CustomViewBinderOne customBinder1 = new CustomViewBinderOne();
        CustomViewBinderTwo customBinder2 = new CustomViewBinderTwo();
        List<CustomViewBinder> customViewBinders = new ArrayList<>();
        customViewBinders.add(customBinder1);
        customViewBinders.add(customBinder2);

        Assert.assertEquals(3, AppMenuAdapter.getCustomViewTypeCount(customViewBinders));

        // Set-up menu items.
        List<MenuItem> items = new ArrayList<>();
        items.add(buildMenuItem(0, TITLE_1));
        items.add(buildMenuItem(customBinder1.supportedId1, TITLE_2));
        items.add(buildMenuItem(customBinder1.supportedId2, TITLE_3));
        items.add(buildMenuItem(customBinder1.supportedId3, TITLE_4));
        items.add(buildMenuItem(customBinder2.supportedId1, TITLE_5));

        AppMenuAdapter adapter = new AppMenuAdapter(
                mClickHandler, items, getActivity().getLayoutInflater(), 0, customViewBinders);
        Map<CustomViewBinder, Integer> offsetMap = adapter.getViewTypeOffsetMapForTests();

        Assert.assertEquals("Incorrect view type offset for binder 1",
                AppMenuAdapter.MenuItemType.NUM_ENTRIES, (int) offsetMap.get(customBinder1));
        Assert.assertEquals("Incorrect view type offset for binder 2",
                AppMenuAdapter.MenuItemType.NUM_ENTRIES + CustomViewBinderOne.VIEW_TYPE_COUNT,
                (int) offsetMap.get(customBinder2));

        // Check that the item view types are correct.
        Assert.assertEquals("Wrong item view type for item 1", AppMenuAdapter.MenuItemType.STANDARD,
                adapter.getItemViewType(0));
        Assert.assertEquals("Wrong item view type for item 2",
                CustomViewBinderOne.VIEW_TYPE_1 + AppMenuAdapter.MenuItemType.NUM_ENTRIES,
                adapter.getItemViewType(1));
        Assert.assertEquals("Wrong item view type for item 3",
                CustomViewBinderOne.VIEW_TYPE_1 + AppMenuAdapter.MenuItemType.NUM_ENTRIES,
                adapter.getItemViewType(2));
        Assert.assertEquals("Wrong item view type for item 4",
                CustomViewBinderOne.VIEW_TYPE_2 + AppMenuAdapter.MenuItemType.NUM_ENTRIES,
                adapter.getItemViewType(3));
        Assert.assertEquals("Wrong item view type for item 5",
                CustomViewBinderTwo.VIEW_TYPE_1 + AppMenuAdapter.MenuItemType.NUM_ENTRIES
                        + CustomViewBinderOne.VIEW_TYPE_COUNT,
                adapter.getItemViewType(4));

        // Check that custom binders and convert views are used as expected.
        ViewGroup parentView = getActivity().findViewById(android.R.id.content);
        View view1 = adapter.getView(0, null, parentView);
        Assert.assertEquals("Binder1 incorrectly called", 0,
                customBinder1.getViewItemCallbackHelper.getCallCount());
        Assert.assertEquals("Binder2 incorrectly called", 0,
                customBinder2.getViewItemCallbackHelper.getCallCount());

        View view2 = adapter.getView(1, view1, parentView);
        Assert.assertEquals(
                "Binder1 not called", 1, customBinder1.getViewItemCallbackHelper.getCallCount());
        Assert.assertEquals("Binder2 incorrectly called", 0,
                customBinder2.getViewItemCallbackHelper.getCallCount());
        Assert.assertNotEquals("Convert view should not have been re-used", view1, view2);
        Assert.assertNotNull("Views created with binder1 should have an enter animation.",
                view2.getTag(R.id.menu_item_enter_anim_id));

        View view3 = adapter.getView(2, view2, parentView);
        Assert.assertEquals(
                "Binder1 not called", 2, customBinder1.getViewItemCallbackHelper.getCallCount());
        Assert.assertEquals("Binder2 incorrectly called", 0,
                customBinder2.getViewItemCallbackHelper.getCallCount());
        Assert.assertEquals("Convert view should have been re-used", view2, view3);

        View view4 = adapter.getView(3, view2, parentView);
        Assert.assertEquals(
                "Binder1 not called", 3, customBinder1.getViewItemCallbackHelper.getCallCount());
        Assert.assertEquals("Binder2 incorrectly called", 0,
                customBinder2.getViewItemCallbackHelper.getCallCount());
        Assert.assertNotEquals("Convert view should not have been re-used", view2, view4);

        View view5 = adapter.getView(4, view2, parentView);
        Assert.assertEquals("Binder1 incorrectly called", 3,
                customBinder1.getViewItemCallbackHelper.getCallCount());
        Assert.assertEquals(
                "Binder2 not called", 1, customBinder2.getViewItemCallbackHelper.getCallCount());
        Assert.assertNotEquals("Convert view should not have been re-used", view2, view5);
        Assert.assertNull("Views created with binder2 should not have an enter animation",
                view5.getTag(R.id.menu_item_enter_anim_id));
    }

    @Test
    @MediumTest
    public void testTitleMenuItem_ToggleCheckbox() {
        List<MenuItem> items = new ArrayList<>();
        items.add(buildTitleMenuItem(1, 2, TITLE_1, 3, TITLE_2));

        AppMenuAdapter adapter = new AppMenuAdapter(
                mClickHandler, items, getActivity().getLayoutInflater(), 0, null);

        ViewGroup parentView = getActivity().findViewById(android.R.id.content);
        View view = adapter.getView(0, null, parentView);
        AppMenuItemIcon checkbox = view.findViewById(R.id.checkbox);

        Assert.assertFalse("Checkbox should be unchecked", checkbox.isChecked());

        TestThreadUtils.runOnUiThreadBlocking(() -> checkbox.toggle());
        Assert.assertTrue("Checkbox should be checked", checkbox.isChecked());

        TestThreadUtils.runOnUiThreadBlocking(() -> checkbox.toggle());
        Assert.assertFalse("Checkbox should be unchecked again", checkbox.isChecked());
    }

    static MenuItem buildMenuItem(int id, CharSequence title) {
        return buildMenuItem(id, title, true);
    }

    static MenuItem buildMenuItem(int id, CharSequence title, boolean enabled) {
        MenuItem item = Mockito.mock(MenuItem.class);
        Mockito.when(item.getItemId()).thenReturn(id);
        Mockito.when(item.getTitle()).thenReturn(title);
        Mockito.when(item.isEnabled()).thenReturn(enabled);
        return item;
    }

    static MenuItem buildMenuItem(int id, CharSequence title, boolean enabled, Drawable icon) {
        MenuItem item = buildMenuItem(id, title, enabled);
        Mockito.when(item.getIcon()).thenReturn(icon);
        return item;
    }

    static MenuItem buildTitleMenuItem(
            int id, int subId1, CharSequence title1, int subId2, CharSequence title2) {
        return buildTitleMenuItem(id, subId1, title1, subId2, title2, null, false, false, true);
    }

    static MenuItem buildTitleMenuItem(int id, int subId1, CharSequence title1, int subId2,
            CharSequence title2, @Nullable Drawable icon, boolean checkable, boolean checked,
            boolean enabled) {
        MenuItem item = Mockito.mock(MenuItem.class);
        SubMenu subMenu = Mockito.mock(SubMenu.class);
        Mockito.when(item.getItemId()).thenReturn(id);
        Mockito.when(item.hasSubMenu()).thenReturn(true);
        Mockito.when(item.getSubMenu()).thenReturn(subMenu);

        Mockito.when(subMenu.size()).thenReturn(2);
        MenuItem title = buildMenuItem(subId1, title1, enabled);
        MenuItem subItem = buildMenuItem(subId2, title2, enabled);

        Mockito.when(subMenu.getItem(0)).thenReturn(title);
        Mockito.when(subMenu.getItem(1)).thenReturn(subItem);

        if (icon != null) {
            assert !checkable : "Title button only supports icon or checkbox";
            Mockito.when(subItem.isCheckable()).thenReturn(false);
            Mockito.when(subItem.getIcon()).thenReturn(icon);
            Mockito.when(subItem.isChecked()).thenReturn(checked);
            Mockito.when(subItem.isVisible()).thenReturn(true);
        }

        if (checkable) {
            assert icon == null : "Title button only supports icon or checkbox";
            Mockito.when(subItem.isCheckable()).thenReturn(true);
            Mockito.when(subItem.isChecked()).thenReturn(checked);
            Mockito.when(subItem.isVisible()).thenReturn(true);
        }

        return item;
    }

    static MenuItem buildIconMenuItem(int id, CharSequence titleCondensed, boolean enabled) {
        MenuItem item = Mockito.mock(MenuItem.class);
        Mockito.when(item.getItemId()).thenReturn(id);
        Mockito.when(item.getTitleCondensed()).thenReturn(titleCondensed);
        Mockito.when(item.isEnabled()).thenReturn(enabled);
        return item;
    }

    static MenuItem buildIconRow(int id, int subId1, CharSequence title1, Drawable icon1,
            int subId2, CharSequence title2, Drawable icon2, int subId3, CharSequence title3,
            Drawable icon3, int subId4, CharSequence title4, @Nullable Drawable icon4, int subId5,
            CharSequence title5, @Nullable Drawable icon5, boolean enabled) {
        MenuItem item = Mockito.mock(MenuItem.class);
        SubMenu subMenu = Mockito.mock(SubMenu.class);
        Mockito.when(item.getItemId()).thenReturn(id);
        Mockito.when(item.hasSubMenu()).thenReturn(true);
        Mockito.when(item.getSubMenu()).thenReturn(subMenu);

        int numSubMenus = 3;
        if (subId4 != 0) {
            numSubMenus++;
            if (subId5 != 0) numSubMenus++;
        }
        Mockito.when(subMenu.size()).thenReturn(numSubMenus);

        MenuItem subMenuItem1 = buildIconMenuItem(subId1, title1, enabled);
        Mockito.when(subMenuItem1.getIcon()).thenReturn(icon1);
        Mockito.when(subMenuItem1.isVisible()).thenReturn(true);
        Mockito.when(subMenu.getItem(0)).thenReturn(subMenuItem1);

        MenuItem subMenuItem2 = buildIconMenuItem(subId2, title2, enabled);
        Mockito.when(subMenuItem2.getIcon()).thenReturn(icon2);
        Mockito.when(subMenuItem2.isVisible()).thenReturn(true);
        Mockito.when(subMenu.getItem(1)).thenReturn(subMenuItem2);

        MenuItem subMenuItem3 = buildIconMenuItem(subId3, title3, enabled);
        Mockito.when(subMenuItem3.getIcon()).thenReturn(icon3);
        Mockito.when(subMenuItem3.isVisible()).thenReturn(true);
        Mockito.when(subMenu.getItem(2)).thenReturn(subMenuItem3);

        if (subId4 != 0) {
            MenuItem subMenuItem4 = buildIconMenuItem(subId4, title4, enabled);
            Mockito.when(subMenuItem4.getIcon()).thenReturn(icon4);
            Mockito.when(subMenuItem4.isVisible()).thenReturn(true);
            Mockito.when(subMenu.getItem(3)).thenReturn(subMenuItem4);

            if (subId5 != 0) {
                MenuItem subMenuItem5 = buildIconMenuItem(subId5, title5, enabled);
                Mockito.when(subMenuItem5.getIcon()).thenReturn(icon5);
                Mockito.when(subMenuItem5.isVisible()).thenReturn(true);
                Mockito.when(subMenu.getItem(4)).thenReturn(subMenuItem5);
            }
        }

        return item;
    }

    private static class CustomViewBinderOne implements CustomViewBinder {
        public static final int VIEW_TYPE_1 = 0;
        public static final int VIEW_TYPE_2 = 1;
        public static final int VIEW_TYPE_COUNT = 2;

        public int supportedId1;
        public int supportedId2;
        public int supportedId3;

        public CallbackHelper getViewItemCallbackHelper = new CallbackHelper();

        public CustomViewBinderOne() {
            supportedId1 = View.generateViewId();
            supportedId2 = View.generateViewId();
            supportedId3 = View.generateViewId();
        }

        @Override
        public int getViewTypeCount() {
            return VIEW_TYPE_COUNT;
        }

        @Override
        public int getItemViewType(int id) {
            if (id == supportedId1 || id == supportedId2) {
                return VIEW_TYPE_1;
            } else if (id == supportedId3) {
                return VIEW_TYPE_2;
            } else {
                return NOT_HANDLED;
            }
        }

        @Override
        public View getView(MenuItem item, @Nullable View convertView, ViewGroup parent,
                LayoutInflater inflater) {
            int itemId = item.getItemId();
            Assert.assertTrue("getView called for incorrect item",
                    itemId == supportedId1 || itemId == supportedId2 || itemId == supportedId3);

            getViewItemCallbackHelper.notifyCalled();

            return convertView != null ? convertView : new View(parent.getContext());
        }

        @Override
        public boolean supportsEnterAnimation(int id) {
            return true;
        }
    }

    private static class CustomViewBinderTwo implements CustomViewBinder {
        public static final int VIEW_TYPE_1 = 0;
        public static final int VIEW_TYPE_COUNT = 1;

        public int supportedId1;
        public CallbackHelper getViewItemCallbackHelper = new CallbackHelper();

        public CustomViewBinderTwo() {
            supportedId1 = View.generateViewId();
        }

        @Override
        public int getViewTypeCount() {
            return VIEW_TYPE_COUNT;
        }

        @Override
        public int getItemViewType(int id) {
            return id == supportedId1 ? VIEW_TYPE_1 : NOT_HANDLED;
        }

        @Override
        public View getView(MenuItem item, @Nullable View convertView, ViewGroup parent,
                LayoutInflater inflater) {
            int itemId = item.getItemId();
            Assert.assertTrue("getView called for incorrect item", itemId == supportedId1);

            getViewItemCallbackHelper.notifyCalled();

            return convertView != null ? convertView : new View(parent.getContext());
        }

        @Override
        public boolean supportsEnterAnimation(int id) {
            return false;
        }
    }
}
