// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import android.app.Activity;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler.AppMenuItemType;
import org.chromium.chrome.browser.ui.appmenu.test.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.widget.ChromeImageView;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/** Tests for {@link AppMenuItemViewBinder}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class AppMenuItemViewBinderTest {
    static class TestClickHandler implements AppMenuClickHandler {
        public CallbackHelper onClickCallback = new CallbackHelper();
        public PropertyModel lastClickedModel;

        public CallbackHelper onLongClickCallback = new CallbackHelper();
        public PropertyModel lastLongClickedModel;

        @Override
        public void onItemClick(PropertyModel model) {
            onClickCallback.notifyCalled();
            lastClickedModel = model;
        }

        @Override
        public boolean onItemLongClick(PropertyModel model, View view) {
            onLongClickCallback.notifyCalled();
            lastLongClickedModel = model;
            return true;
        }
    }

    private static class CustomViewBinderOne implements CustomViewBinder {
        public static final int VIEW_TYPE_1 = 0;
        public static final int VIEW_TYPE_2 = 1;
        public static final int VIEW_TYPE_COUNT = 2;

        public int supportedId1;
        public int supportedId2;
        public int supportedId3;

        public int lastBindId;

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
        public int getLayoutId(int viewType) {
            return CustomViewBinder.NOT_HANDLED;
        }

        @Override
        public void bind(PropertyModel model, View view, PropertyKey key) {
            if (key == AppMenuItemProperties.MENU_ITEM_ID) {
                lastBindId = model.get(AppMenuItemProperties.MENU_ITEM_ID);
                getViewItemCallbackHelper.notifyCalled();
            }
        }

        @Override
        public boolean supportsEnterAnimation(int id) {
            return true;
        }

        @Override
        public int getPixelHeight(Context context) {
            return 0;
        }
    }

    private static class CustomViewBinderTwo implements CustomViewBinder {
        public static final int VIEW_TYPE_1 = 0;
        public static final int VIEW_TYPE_COUNT = 1;

        public int supportedId1;

        public int lastBindId;

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
        public int getLayoutId(int viewType) {
            return CustomViewBinder.NOT_HANDLED;
        }

        @Override
        public void bind(PropertyModel model, View view, PropertyKey key) {
            if (key == AppMenuItemProperties.MENU_ITEM_ID) {
                lastBindId = model.get(AppMenuItemProperties.MENU_ITEM_ID);
                getViewItemCallbackHelper.notifyCalled();
            }
        }

        @Override
        public boolean supportsEnterAnimation(int id) {
            return false;
        }

        @Override
        public int getPixelHeight(Context context) {
            return 0;
        }
    }

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    static final int MENU_ID1 = 100;
    static final int MENU_ID2 = 200;
    static final int MENU_ID3 = 300;
    static final int MENU_ID4 = 400;
    static final int MENU_ID5 = 500;
    static final int MENU_ID6 = 600;
    static final int MENU_ID7 = 700;
    static final String TITLE_1 = "Menu Item One";
    static final String TITLE_2 = "Menu Item Two";
    static final String TITLE_3 = "Menu Item Three";
    static final String TITLE_4 = "Menu Item Four";
    static final String TITLE_5 = "Menu Item Five";
    static final String TITLE_6 = "Menu Item Six";
    static final String TITLE_7 = "Menu Item Seven";

    private Activity mActivity;
    private ModelListAdapter.ModelList mMenuList;
    private ModelListAdapter mModelListAdapter;

    private TestClickHandler mClickHandler;

    @BeforeClass
    public static void setupSuite() {
        sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUpTest() throws Exception {
        MockitoAnnotations.initMocks(this);
        mClickHandler = new TestClickHandler();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivity = sActivityTestRule.getActivity();
                    mMenuList = new ModelListAdapter.ModelList();
                    mModelListAdapter = new ModelListAdapter(mMenuList);

                    mModelListAdapter.registerType(
                            AppMenuItemType.STANDARD,
                            new LayoutViewBuilder(R.layout.menu_item_start_with_icon),
                            AppMenuItemViewBinder::bindStandardItem);
                    mModelListAdapter.registerType(
                            AppMenuItemType.TITLE_BUTTON,
                            new LayoutViewBuilder(R.layout.title_button_menu_item),
                            AppMenuItemViewBinder::bindTitleButtonItem);
                    mModelListAdapter.registerType(
                            AppMenuItemType.THREE_BUTTON_ROW,
                            new LayoutViewBuilder(R.layout.icon_row_menu_item),
                            AppMenuItemViewBinder::bindIconRowItem);
                    mModelListAdapter.registerType(
                            AppMenuItemType.FOUR_BUTTON_ROW,
                            new LayoutViewBuilder(R.layout.icon_row_menu_item),
                            AppMenuItemViewBinder::bindIconRowItem);
                    mModelListAdapter.registerType(
                            AppMenuItemType.FIVE_BUTTON_ROW,
                            new LayoutViewBuilder(R.layout.icon_row_menu_item),
                            AppMenuItemViewBinder::bindIconRowItem);
                });
    }

    private PropertyModel createStandardMenuItem(int menuId, String title) {
        PropertyModel model =
                new PropertyModel.Builder(AppMenuItemProperties.ALL_KEYS)
                        .with(AppMenuItemProperties.MENU_ITEM_ID, menuId)
                        .with(AppMenuItemProperties.TITLE, title)
                        .build();
        mMenuList.add(new ModelListAdapter.ListItem(AppMenuItemType.STANDARD, model));

        return model;
    }

    private PropertyModel createTitleMenuItem(
            int mainMenuId,
            int titleMenuId,
            String title,
            @Nullable Drawable menuIcon,
            int buttonMenuId,
            String buttonTitle,
            boolean checkable,
            boolean checked) {
        PropertyModel model =
                new PropertyModel.Builder(AppMenuItemProperties.ALL_KEYS)
                        .with(AppMenuItemProperties.MENU_ITEM_ID, mainMenuId)
                        .build();

        ModelListAdapter.ModelList subList = new ModelListAdapter.ModelList();
        PropertyModel titleModel =
                new PropertyModel.Builder(AppMenuItemProperties.ALL_KEYS)
                        .with(AppMenuItemProperties.MENU_ITEM_ID, titleMenuId)
                        .with(AppMenuItemProperties.TITLE, title)
                        .build();
        if (menuIcon != null) {
            titleModel.set(AppMenuItemProperties.ICON, menuIcon);
        }
        PropertyModel buttonModel =
                new PropertyModel.Builder(AppMenuItemProperties.ALL_KEYS)
                        .with(AppMenuItemProperties.MENU_ITEM_ID, buttonMenuId)
                        .with(AppMenuItemProperties.TITLE, buttonTitle)
                        .with(AppMenuItemProperties.CHECKABLE, checkable)
                        .with(AppMenuItemProperties.CHECKED, checked)
                        .build();
        subList.add(new ModelListAdapter.ListItem(0, titleModel));
        subList.add(new ModelListAdapter.ListItem(0, buttonModel));

        model.set(AppMenuItemProperties.SUBMENU, subList);
        mMenuList.add(new ModelListAdapter.ListItem(AppMenuItemType.TITLE_BUTTON, model));

        return model;
    }

    private PropertyModel createIconRowMenuItem(
            int menuId,
            int subId1,
            String titleCondensed1,
            Drawable icon1,
            int subId2,
            String titleCondensed2,
            Drawable icon2,
            int subId3,
            String titleCondensed3,
            Drawable icon3,
            int subId4,
            @Nullable String titleCondensed4,
            @Nullable Drawable icon4,
            int subId5,
            @Nullable String titleCondensed5,
            @Nullable Drawable icon5) {
        PropertyModel model =
                new PropertyModel.Builder(AppMenuItemProperties.ALL_KEYS)
                        .with(AppMenuItemProperties.MENU_ITEM_ID, menuId)
                        .build();

        ModelListAdapter.ModelList subList = new ModelListAdapter.ModelList();
        int menutype = AppMenuItemType.THREE_BUTTON_ROW;
        createIconMenuItem(subList, subId1, titleCondensed1, icon1);
        createIconMenuItem(subList, subId2, titleCondensed2, icon2);
        createIconMenuItem(subList, subId3, titleCondensed3, icon3);
        if (subId4 != View.NO_ID) {
            createIconMenuItem(subList, subId4, titleCondensed4, icon4);
            menutype = AppMenuItemType.FOUR_BUTTON_ROW;
            if (subId5 != View.NO_ID) {
                createIconMenuItem(subList, subId5, titleCondensed5, icon5);
                menutype = AppMenuItemType.FIVE_BUTTON_ROW;
            }
        }

        model.set(AppMenuItemProperties.SUBMENU, subList);
        mMenuList.add(new ModelListAdapter.ListItem(menutype, model));

        return model;
    }

    private void createIconMenuItem(
            ModelListAdapter.ModelList list, int id, String titleCondensed, Drawable icon) {
        PropertyModel model =
                new PropertyModel.Builder(AppMenuItemProperties.ALL_KEYS)
                        .with(AppMenuItemProperties.MENU_ITEM_ID, id)
                        .with(AppMenuItemProperties.TITLE_CONDENSED, titleCondensed)
                        .with(AppMenuItemProperties.ICON, icon)
                        .build();
        list.add(new ModelListAdapter.ListItem(0, model));
    }

    private PropertyModel createCustomMenuItem(
            int menuId, int offset, CustomViewBinder customBinder) {
        PropertyModel model =
                new PropertyModel.Builder(AppMenuItemProperties.ALL_KEYS)
                        .with(AppMenuItemProperties.MENU_ITEM_ID, menuId)
                        .build();
        mMenuList.add(
                new ModelListAdapter.ListItem(
                        offset + customBinder.getItemViewType(menuId), model));

        return model;
    }

    @Test
    @UiThreadTest
    @MediumTest
    public void testStandardMenuItem() throws ExecutionException, TimeoutException {
        PropertyModel standardModel = createStandardMenuItem(MENU_ID1, TITLE_1);

        ViewGroup parentView = mActivity.findViewById(android.R.id.content);
        View view = mModelListAdapter.getView(0, null, parentView);
        TextView titleView = view.findViewById(R.id.menu_item_text);
        ChromeImageView itemIcon = view.findViewById(R.id.menu_item_icon);

        Assert.assertEquals("Incorrect title text for item 1", TITLE_1, titleView.getText());
        Assert.assertNull("Should not have icon for item 1", itemIcon.getDrawable());

        standardModel.set(AppMenuItemProperties.CLICK_HANDLER, mClickHandler);
        view.performClick();
        mClickHandler.onClickCallback.waitForCallback(0);
        Assert.assertEquals(
                "Incorrect clicked item id",
                MENU_ID1,
                mClickHandler.lastClickedModel.get(AppMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    @UiThreadTest
    @MediumTest
    public void testStandardMenuItem_WithMenuIcon() throws ExecutionException, TimeoutException {
        PropertyModel standardModel = createStandardMenuItem(MENU_ID1, TITLE_1);

        ViewGroup parentView = mActivity.findViewById(android.R.id.content);
        View view = mModelListAdapter.getView(0, null, parentView);
        ChromeImageView itemIcon = view.findViewById(R.id.menu_item_icon);

        standardModel.set(
                AppMenuItemProperties.ICON,
                AppCompatResources.getDrawable(
                        mActivity,
                        org.chromium.chrome.browser.ui.appmenu.test.R.drawable
                                .test_ic_vintage_filter));
        Assert.assertNotNull("Should have icon for item 1", itemIcon.getDrawable());
    }

    @Test
    @UiThreadTest
    @MediumTest
    public void testConvertView_Reused_StandardMenuItem() throws TimeoutException {
        PropertyModel standardModel1 = createStandardMenuItem(MENU_ID1, TITLE_1);
        standardModel1.set(AppMenuItemProperties.CLICK_HANDLER, mClickHandler);
        PropertyModel standardModel2 = createStandardMenuItem(MENU_ID2, TITLE_2);
        standardModel2.set(AppMenuItemProperties.CLICK_HANDLER, mClickHandler);

        ViewGroup parentView = mActivity.findViewById(android.R.id.content);
        View view1 = mModelListAdapter.getView(0, null, parentView);
        TextView titleView = view1.findViewById(R.id.menu_item_text);

        Assert.assertEquals("Incorrect title text for item 1", TITLE_1, titleView.getText());

        View view2 = mModelListAdapter.getView(1, view1, parentView);
        Assert.assertEquals("Convert view should have been re-used", view1, view2);
        Assert.assertEquals("Title should have been updated", TITLE_2, titleView.getText());

        view2.performClick();
        mClickHandler.onClickCallback.waitForCallback(0);
        Assert.assertEquals(
                "Incorrect clicked item id",
                MENU_ID2,
                mClickHandler.lastClickedModel.get(AppMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    @UiThreadTest
    @MediumTest
    public void testConvertView_Reused_TitleMenuItem() {
        createTitleMenuItem(MENU_ID1, MENU_ID2, TITLE_2, null, MENU_ID3, TITLE_3, true, true);
        createTitleMenuItem(MENU_ID4, MENU_ID5, TITLE_5, null, MENU_ID6, TITLE_6, true, false);

        Assert.assertEquals(
                "Wrong item view type",
                AppMenuItemType.TITLE_BUTTON,
                mModelListAdapter.getItemViewType(0));

        ViewGroup parentView = mActivity.findViewById(android.R.id.content);
        View view1 = mModelListAdapter.getView(0, null, parentView);
        TextViewWithCompoundDrawables titleView =
                (TextViewWithCompoundDrawables) view1.findViewById(R.id.title);

        Assert.assertEquals("Incorrect title text for item 1", TITLE_2, titleView.getText());

        Assert.assertNull(
                "Should not have icon for item 1", view1.findViewById(R.id.menu_item_icon));

        View view2 = mModelListAdapter.getView(1, view1, parentView);
        Assert.assertEquals("Convert view should have been re-used", view1, view2);
        Assert.assertEquals("Title should have been updated", TITLE_5, titleView.getText());
    }

    @Test
    @UiThreadTest
    @MediumTest
    public void testConvertView_Reused_TitleMenuItem_WithMenuIcon() {
        Drawable icon =
                AppCompatResources.getDrawable(
                        mActivity,
                        org.chromium.chrome.browser.ui.appmenu.test.R.drawable
                                .test_ic_vintage_filter);
        createTitleMenuItem(MENU_ID1, MENU_ID2, TITLE_2, icon, MENU_ID3, TITLE_3, true, true);
        createTitleMenuItem(MENU_ID4, MENU_ID5, TITLE_5, icon, MENU_ID6, TITLE_6, true, false);

        Assert.assertEquals(
                "Wrong item view type",
                AppMenuItemType.TITLE_BUTTON,
                mModelListAdapter.getItemViewType(0));

        ViewGroup parentView = mActivity.findViewById(android.R.id.content);
        View view1 = mModelListAdapter.getView(0, null, parentView);
        TextViewWithCompoundDrawables titleView = view1.findViewById(R.id.title);
        Drawable[] drawables = titleView.getCompoundDrawablesRelative();
        Assert.assertNotNull("Should have icon for item 1", drawables[0]);
    }

    @Test
    @UiThreadTest
    @MediumTest
    public void testConvertView_Reused_IconRow() {
        Drawable icon =
                AppCompatResources.getDrawable(
                        mActivity,
                        org.chromium.chrome.browser.ui.appmenu.test.R.drawable
                                .test_ic_vintage_filter);
        createIconRowMenuItem(
                1,
                MENU_ID1,
                TITLE_1,
                icon,
                MENU_ID2,
                TITLE_2,
                icon,
                MENU_ID3,
                TITLE_3,
                icon,
                View.NO_ID,
                null,
                null,
                View.NO_ID,
                null,
                null);
        createIconRowMenuItem(
                1,
                MENU_ID4,
                TITLE_4,
                icon,
                MENU_ID5,
                TITLE_5,
                icon,
                MENU_ID6,
                TITLE_6,
                icon,
                View.NO_ID,
                null,
                null,
                View.NO_ID,
                null,
                null);

        ViewGroup parentView = mActivity.findViewById(android.R.id.content);
        View view1 = mModelListAdapter.getView(0, null, parentView);
        View buttonOne = view1.findViewById(R.id.button_one);

        Assert.assertEquals(
                "Incorrect content description for item 1",
                TITLE_1,
                buttonOne.getContentDescription());

        View view2 = mModelListAdapter.getView(1, view1, parentView);
        Assert.assertEquals("Convert view should have been re-used", view1, view2);
        Assert.assertEquals(
                "Content description should have been updated",
                TITLE_4,
                buttonOne.getContentDescription());
    }

    @Test
    @UiThreadTest
    @MediumTest
    public void testConvertView_NotReused() {
        createStandardMenuItem(MENU_ID1, TITLE_1);
        createTitleMenuItem(MENU_ID2, MENU_ID3, TITLE_3, null, MENU_ID4, TITLE_4, true, true);

        Assert.assertEquals(
                "Wrong item view type for item 1",
                AppMenuItemType.STANDARD,
                mModelListAdapter.getItemViewType(0));
        Assert.assertEquals(
                "Wrong item view type for item 2",
                AppMenuItemType.TITLE_BUTTON,
                mModelListAdapter.getItemViewType(1));

        ViewGroup parentView = mActivity.findViewById(android.R.id.content);
        View view1 = mModelListAdapter.getView(0, null, parentView);
        TextView titleView = view1.findViewById(R.id.menu_item_text);

        Assert.assertEquals("Incorrect title text for item 1", TITLE_1, titleView.getText());

        View view2 = mModelListAdapter.getView(1, view1, parentView);
        Assert.assertNotEquals("Standard view should not have been re-used", view1, view2);
        Assert.assertEquals(
                "Title for view 1 should have not have been updated", TITLE_1, titleView.getText());

        View view3 = mModelListAdapter.getView(0, view2, parentView);
        Assert.assertNotEquals("Title button view should not have been re-used", view2, view3);
    }

    @Test
    @UiThreadTest
    @MediumTest
    public void testConvertView_NotReused_IconRow() {
        Drawable icon =
                AppCompatResources.getDrawable(
                        mActivity,
                        org.chromium.chrome.browser.ui.appmenu.test.R.drawable
                                .test_ic_vintage_filter);
        createIconRowMenuItem(
                1,
                MENU_ID1,
                TITLE_1,
                icon,
                MENU_ID2,
                TITLE_2,
                icon,
                MENU_ID3,
                TITLE_3,
                icon,
                View.NO_ID,
                null,
                null,
                View.NO_ID,
                null,
                null);
        createIconRowMenuItem(
                2,
                MENU_ID4,
                TITLE_4,
                icon,
                MENU_ID5,
                TITLE_5,
                icon,
                MENU_ID6,
                TITLE_6,
                icon,
                MENU_ID7,
                TITLE_7,
                icon,
                View.NO_ID,
                null,
                null);

        ViewGroup parentView = mActivity.findViewById(android.R.id.content);
        View view1 = mModelListAdapter.getView(0, null, parentView);
        View view2 = mModelListAdapter.getView(1, view1, parentView);
        Assert.assertNotEquals("Convert view should not have been re-used", view1, view2);
    }

    @Test
    @UiThreadTest
    @MediumTest
    public void testCustomViewBinders() {
        CustomViewBinderOne customBinder1 = new CustomViewBinderOne();
        CustomViewBinderTwo customBinder2 = new CustomViewBinderTwo();
        mModelListAdapter.registerType(
                AppMenuItemType.NUM_ENTRIES,
                new LayoutViewBuilder(R.layout.menu_item_start_with_icon),
                customBinder1);
        mModelListAdapter.registerType(
                AppMenuItemType.NUM_ENTRIES + 1,
                new LayoutViewBuilder(R.layout.menu_item_start_with_icon),
                customBinder1);
        mModelListAdapter.registerType(
                AppMenuItemType.NUM_ENTRIES + customBinder1.getViewTypeCount(),
                new LayoutViewBuilder(R.layout.menu_item_start_with_icon),
                customBinder2);

        createStandardMenuItem(MENU_ID1, TITLE_1);
        createCustomMenuItem(
                customBinder1.supportedId1, AppMenuItemType.NUM_ENTRIES, customBinder1);
        createCustomMenuItem(
                customBinder1.supportedId2, AppMenuItemType.NUM_ENTRIES, customBinder1);
        createCustomMenuItem(
                customBinder1.supportedId3, AppMenuItemType.NUM_ENTRIES, customBinder1);
        createCustomMenuItem(
                customBinder2.supportedId1,
                AppMenuItemType.NUM_ENTRIES + customBinder1.getViewTypeCount(),
                customBinder2);

        ViewGroup parentView = mActivity.findViewById(android.R.id.content);
        View view = mModelListAdapter.getView(0, null, parentView);
        TextView titleView = view.findViewById(R.id.menu_item_text);
        Assert.assertEquals("Incorrect title text for item 1", TITLE_1, titleView.getText());

        view = mModelListAdapter.getView(1, null, parentView);
        Assert.assertEquals(
                "Binder1 not called", 1, customBinder1.getViewItemCallbackHelper.getCallCount());
        Assert.assertEquals(
                "Wrong ID is called", customBinder1.lastBindId, customBinder1.supportedId1);

        view = mModelListAdapter.getView(2, null, parentView);
        Assert.assertEquals(
                "Binder1 not called", 2, customBinder1.getViewItemCallbackHelper.getCallCount());
        Assert.assertEquals(
                "Wrong ID is called", customBinder1.lastBindId, customBinder1.supportedId2);

        view = mModelListAdapter.getView(3, null, parentView);
        Assert.assertEquals(
                "Binder1 not called", 3, customBinder1.getViewItemCallbackHelper.getCallCount());
        Assert.assertEquals(
                "Wrong ID is called", customBinder1.lastBindId, customBinder1.supportedId3);

        view = mModelListAdapter.getView(4, null, parentView);
        Assert.assertEquals(
                "Binder2 not called", 1, customBinder2.getViewItemCallbackHelper.getCallCount());
        Assert.assertEquals(
                "Wrong ID is called", customBinder2.lastBindId, customBinder2.supportedId1);
    }

    @Test
    @UiThreadTest
    @MediumTest
    public void testTitleMenuItem_Checkbox() {
        createTitleMenuItem(MENU_ID1, MENU_ID2, TITLE_2, null, MENU_ID3, TITLE_3, true, true);

        ViewGroup parentView = mActivity.findViewById(android.R.id.content);
        View view = mModelListAdapter.getView(0, null, parentView);
        AppMenuItemIcon checkbox = view.findViewById(R.id.checkbox);

        Assert.assertTrue("Checkbox should be checked", checkbox.isChecked());
    }

    @Test
    @UiThreadTest
    @MediumTest
    public void testTitleMenuItem_ToggleCheckbox() {
        createTitleMenuItem(MENU_ID1, MENU_ID2, TITLE_2, null, MENU_ID3, TITLE_3, true, false);

        ViewGroup parentView = mActivity.findViewById(android.R.id.content);
        View view = mModelListAdapter.getView(0, null, parentView);
        AppMenuItemIcon checkbox = view.findViewById(R.id.checkbox);

        Assert.assertFalse("Checkbox should be unchecked", checkbox.isChecked());

        checkbox.toggle();
        Assert.assertTrue("Checkbox should be checked", checkbox.isChecked());

        checkbox.toggle();
        Assert.assertFalse("Checkbox should be unchecked again", checkbox.isChecked());
    }

    @Test
    @UiThreadTest
    @MediumTest
    public void testIconRowViewBinders() {
        Drawable icon =
                AppCompatResources.getDrawable(
                        mActivity,
                        org.chromium.chrome.browser.ui.appmenu.test.R.drawable
                                .test_ic_vintage_filter);
        createIconRowMenuItem(
                1, MENU_ID1, TITLE_1, icon, MENU_ID2, TITLE_2, icon, MENU_ID3, TITLE_3, icon,
                MENU_ID4, TITLE_4, icon, MENU_ID5, TITLE_5, icon);

        ViewGroup parentView = mActivity.findViewById(android.R.id.content);
        View view = mModelListAdapter.getView(0, null, parentView);
        ImageButton button = view.findViewById(R.id.button_one);
        Assert.assertEquals(
                "Incorrect content description for icon 1",
                TITLE_1,
                button.getContentDescription());
        Assert.assertNotNull("Should have an icon for icon 1", button.getDrawable());

        button = view.findViewById(R.id.button_two);
        Assert.assertEquals(
                "Incorrect content description for icon 2",
                TITLE_2,
                button.getContentDescription());
        Assert.assertNotNull("Should have an icon for icon 2", button.getDrawable());

        button = view.findViewById(R.id.button_three);
        Assert.assertEquals(
                "Incorrect content description for icon 3",
                TITLE_3,
                button.getContentDescription());
        Assert.assertNotNull("Should have an icon for icon 3", button.getDrawable());

        button = view.findViewById(R.id.button_four);
        Assert.assertEquals(
                "Incorrect content description for icon 4",
                TITLE_4,
                button.getContentDescription());
        Assert.assertNotNull("Should have an icon for icon 4", button.getDrawable());

        button = view.findViewById(R.id.button_five);
        Assert.assertEquals(
                "Incorrect content description for icon 5",
                TITLE_5,
                button.getContentDescription());
        Assert.assertNotNull("Should have an icon for icon 5", button.getDrawable());
    }
}
