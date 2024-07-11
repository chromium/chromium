// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.ListView;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler.AppMenuItemType;
import org.chromium.chrome.browser.ui.appmenu.test.R;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;

/** Render tests for {@link AppMenuItemViewBinder}. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
public class AppMenuItemViewBinderRenderTest {
    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams =
            Arrays.asList(
                    new ParameterSet().value(false, true).name("LiteMode_MenuItemEnabled"),
                    new ParameterSet().value(false, false).name("LiteMode_MenuItemDisabled"),
                    new ParameterSet().value(true, true).name("NightMode_MenuItemEnabled"),
                    new ParameterSet().value(true, false).name("NightMode_MenuItemDisabled"));

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_MOBILE_APP_MENU)
                    .setRevision(1)
                    .build();

    private static Activity sActivity;
    private static ListView sListView;
    private static View sContentView;

    static final int MENU_ID1 = 100;
    static final int MENU_ID2 = 200;
    static final int MENU_ID3 = 300;
    static final int MENU_ID4 = 400;
    static final int MENU_ID5 = 500;
    static final String TITLE_1 = "Menu Item One";
    static final String TITLE_2 = "Menu Item Two";
    static final String TITLE_3 = "Menu Item Three";
    static final String TITLE_4 = "Menu Item Four";
    static final String TITLE_5 = "Menu Item Five";

    private ModelListAdapter.ModelList mMenuList;
    private ModelListAdapter mModelListAdapter;
    private boolean mMenuItemEnabled;

    public AppMenuItemViewBinderRenderTest(boolean nightMode, boolean menuItemEnabled) {
        mMenuItemEnabled = menuItemEnabled;
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightMode);
        mRenderTestRule.setNightModeEnabled(nightMode);
        mRenderTestRule.setVariantPrefix(menuItemEnabled ? "MenuItemEnabled" : "MenuItemDisabled");
    }

    @Before
    public void setUpTest() throws Exception {
        sActivityTestRule.launchActivity(null);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sActivity = sActivityTestRule.getActivity();
                    mMenuList = new ModelListAdapter.ModelList();
                    mModelListAdapter = new ModelListAdapter(mMenuList);

                    sActivity.setContentView(R.layout.app_menu_layout);
                    sContentView = sActivity.findViewById(android.R.id.content);
                    sListView = sContentView.findViewById(R.id.app_menu_list);
                    sListView.setAdapter(mModelListAdapter);

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

    @After
    public void tearDownTest() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mMenuList.clear();
                });
    }

    @AfterClass
    public static void afterClass() {
        NightModeTestUtils.tearDownNightModeForBlankUiTestActivity();
    }

    private PropertyModel createStandardMenuItem(
            int menuId, String title, boolean enabled, @Nullable Drawable icon) {
        PropertyModel model =
                new PropertyModel.Builder(AppMenuItemProperties.ALL_KEYS)
                        .with(AppMenuItemProperties.MENU_ITEM_ID, menuId)
                        .with(AppMenuItemProperties.TITLE, title)
                        .with(AppMenuItemProperties.ENABLED, enabled)
                        .build();
        if (icon != null) {
            model.set(AppMenuItemProperties.ICON, icon);
        }
        mMenuList.add(new ModelListAdapter.ListItem(AppMenuItemType.STANDARD, model));

        return model;
    }

    private PropertyModel createTitleMenuItem(
            int mainMenuId,
            int titleMenuId,
            String title,
            boolean enabled,
            @Nullable Drawable menuIcon,
            int buttonMenuId,
            String buttonTitle,
            boolean checkable,
            boolean checked,
            boolean buttonEnabled,
            @Nullable Drawable buttonIcon) {
        PropertyModel model =
                new PropertyModel.Builder(AppMenuItemProperties.ALL_KEYS)
                        .with(AppMenuItemProperties.MENU_ITEM_ID, mainMenuId)
                        .build();

        ModelListAdapter.ModelList subList = new ModelListAdapter.ModelList();
        PropertyModel titleModel =
                new PropertyModel.Builder(AppMenuItemProperties.ALL_KEYS)
                        .with(AppMenuItemProperties.MENU_ITEM_ID, titleMenuId)
                        .with(AppMenuItemProperties.TITLE, title)
                        .with(AppMenuItemProperties.ENABLED, enabled)
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
                        .with(AppMenuItemProperties.ENABLED, buttonEnabled)
                        .build();
        if (buttonIcon != null) {
            buttonModel.set(AppMenuItemProperties.ICON, buttonIcon);
        }
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
            @Nullable Drawable icon5,
            boolean enabled) {
        PropertyModel model =
                new PropertyModel.Builder(AppMenuItemProperties.ALL_KEYS)
                        .with(AppMenuItemProperties.MENU_ITEM_ID, menuId)
                        .build();

        ModelListAdapter.ModelList subList = new ModelListAdapter.ModelList();
        int menutype = AppMenuItemType.THREE_BUTTON_ROW;
        createIconMenuItem(subList, subId1, titleCondensed1, icon1, enabled);
        createIconMenuItem(subList, subId2, titleCondensed2, icon2, enabled);
        createIconMenuItem(subList, subId3, titleCondensed3, icon3, enabled);
        if (subId4 != View.NO_ID) {
            createIconMenuItem(subList, subId4, titleCondensed4, icon4, enabled);
            menutype = AppMenuItemType.FOUR_BUTTON_ROW;
            if (subId5 != View.NO_ID) {
                createIconMenuItem(subList, subId5, titleCondensed5, icon5, enabled);
                menutype = AppMenuItemType.FIVE_BUTTON_ROW;
            }
        }

        model.set(AppMenuItemProperties.SUBMENU, subList);
        mMenuList.add(new ModelListAdapter.ListItem(menutype, model));

        return model;
    }

    private void createIconMenuItem(
            ModelListAdapter.ModelList list,
            int id,
            String titleCondensed,
            Drawable icon,
            boolean enabled) {
        PropertyModel model =
                new PropertyModel.Builder(AppMenuItemProperties.ALL_KEYS)
                        .with(AppMenuItemProperties.MENU_ITEM_ID, id)
                        .with(AppMenuItemProperties.TITLE_CONDENSED, titleCondensed)
                        .with(AppMenuItemProperties.ICON, icon)
                        .with(AppMenuItemProperties.ENABLED, enabled)
                        .build();
        list.add(new ModelListAdapter.ListItem(0, model));
    }

    private void waitListViewToBeDrawn() {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(sListView.getChildAt(0), Matchers.notNullValue());
                });
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testStandardMenuItem() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    createStandardMenuItem(MENU_ID1, TITLE_1, mMenuItemEnabled, null);
                });
        waitListViewToBeDrawn();
        mRenderTestRule.render(sContentView, "standard");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testStandardMenuItem_Icon() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Drawable icon =
                            AppCompatResources.getDrawable(
                                    sActivity,
                                    org.chromium.chrome.browser.ui.appmenu.test.R.drawable
                                            .test_ic_vintage_filter);
                    createStandardMenuItem(MENU_ID1, TITLE_1, mMenuItemEnabled, icon);
                });
        waitListViewToBeDrawn();
        mRenderTestRule.render(sContentView, "standard_with_icon");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testTitleButtonMenuItem_Icon() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Drawable buttonIcon =
                            AppCompatResources.getDrawable(
                                    sActivity,
                                    org.chromium.chrome.browser.ui.appmenu.test.R.drawable
                                            .test_ic_arrow_forward_black_24dp);
                    createTitleMenuItem(
                            MENU_ID1,
                            MENU_ID2,
                            TITLE_2,
                            mMenuItemEnabled,
                            null,
                            MENU_ID3,
                            TITLE_3,
                            false,
                            false,
                            mMenuItemEnabled,
                            buttonIcon);
                });
        waitListViewToBeDrawn();
        mRenderTestRule.render(sContentView, "title_button_icon");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testTitleButtonMenuItem_Checkbox_Checked() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    createTitleMenuItem(
                            MENU_ID1,
                            MENU_ID2,
                            TITLE_2,
                            mMenuItemEnabled,
                            null,
                            MENU_ID3,
                            TITLE_3,
                            true,
                            true,
                            mMenuItemEnabled,
                            null);
                });
        waitListViewToBeDrawn();
        mRenderTestRule.render(sContentView, "title_button_checkbox_checked");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testTitleButtonMenuItem_Checkbox_Unchecked() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    createTitleMenuItem(
                            MENU_ID1,
                            MENU_ID2,
                            TITLE_2,
                            mMenuItemEnabled,
                            null,
                            MENU_ID3,
                            TITLE_3,
                            true,
                            false,
                            mMenuItemEnabled,
                            null);
                });
        waitListViewToBeDrawn();
        mRenderTestRule.render(sContentView, "title_button_checkbox_unchecked");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testTitleButtonMenuItem_Checkbox_Unchecked_IconBeforeItem() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Drawable icon =
                            AppCompatResources.getDrawable(
                                    sActivity,
                                    org.chromium.chrome.browser.ui.appmenu.test.R.drawable
                                            .test_ic_vintage_filter);
                    createTitleMenuItem(
                            MENU_ID1,
                            MENU_ID2,
                            TITLE_2,
                            mMenuItemEnabled,
                            icon,
                            MENU_ID3,
                            TITLE_3,
                            true,
                            false,
                            mMenuItemEnabled,
                            null);
                });
        waitListViewToBeDrawn();
        mRenderTestRule.render(sContentView, "title_button_checkbox_unchecked_icon_before_item");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testIconRow_ThreeIcons() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Drawable icon1 =
                            AppCompatResources.getDrawable(
                                    sActivity,
                                    org.chromium.chrome.browser.ui.appmenu.test.R.drawable
                                            .test_ic_arrow_forward_black_24dp);
                    Drawable icon2 =
                            AppCompatResources.getDrawable(
                                    sActivity,
                                    org.chromium.chrome.browser.ui.appmenu.test.R.drawable
                                            .test_ic_star_border_black_24dp);
                    Drawable icon3 =
                            AppCompatResources.getDrawable(
                                    sActivity,
                                    org.chromium.chrome.browser.ui.appmenu.test.R.drawable
                                            .test_ic_arrow_downward_black_24dp);
                    createIconRowMenuItem(
                            1,
                            MENU_ID1,
                            TITLE_1,
                            icon1,
                            MENU_ID2,
                            TITLE_2,
                            icon2,
                            MENU_ID3,
                            TITLE_3,
                            icon3,
                            View.NO_ID,
                            null,
                            null,
                            View.NO_ID,
                            null,
                            null,
                            mMenuItemEnabled);
                });
        waitListViewToBeDrawn();
        mRenderTestRule.render(sContentView, "iconrow_three_icons");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testIconRow_FourIcons() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Drawable icon1 =
                            AppCompatResources.getDrawable(
                                    sActivity,
                                    org.chromium.chrome.browser.ui.appmenu.test.R.drawable
                                            .test_ic_arrow_forward_black_24dp);
                    Drawable icon2 =
                            AppCompatResources.getDrawable(
                                    sActivity,
                                    org.chromium.chrome.browser.ui.appmenu.test.R.drawable
                                            .test_ic_star_border_black_24dp);
                    Drawable icon3 =
                            AppCompatResources.getDrawable(
                                    sActivity,
                                    org.chromium.chrome.browser.ui.appmenu.test.R.drawable
                                            .test_ic_arrow_downward_black_24dp);
                    Drawable icon4 =
                            AppCompatResources.getDrawable(
                                    sActivity,
                                    org.chromium.chrome.browser.ui.appmenu.test.R.drawable
                                            .test_ic_info_outline_black_24dp);
                    createIconRowMenuItem(
                            1,
                            MENU_ID1,
                            TITLE_1,
                            icon1,
                            MENU_ID2,
                            TITLE_2,
                            icon2,
                            MENU_ID3,
                            TITLE_3,
                            icon3,
                            MENU_ID4,
                            TITLE_4,
                            icon4,
                            View.NO_ID,
                            null,
                            null,
                            mMenuItemEnabled);
                });
        waitListViewToBeDrawn();
        mRenderTestRule.render(sContentView, "iconrow_four_icons");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testIconRow_FiveIcons() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Drawable icon1 =
                            AppCompatResources.getDrawable(
                                    sActivity,
                                    org.chromium.chrome.browser.ui.appmenu.test.R.drawable
                                            .test_ic_arrow_forward_black_24dp);
                    Drawable icon2 =
                            AppCompatResources.getDrawable(
                                    sActivity,
                                    org.chromium.chrome.browser.ui.appmenu.test.R.drawable
                                            .test_ic_star_border_black_24dp);
                    Drawable icon3 =
                            AppCompatResources.getDrawable(
                                    sActivity,
                                    org.chromium.chrome.browser.ui.appmenu.test.R.drawable
                                            .test_ic_arrow_downward_black_24dp);
                    Drawable icon4 =
                            AppCompatResources.getDrawable(
                                    sActivity,
                                    org.chromium.chrome.browser.ui.appmenu.test.R.drawable
                                            .test_ic_info_outline_black_24dp);
                    Drawable icon5 =
                            AppCompatResources.getDrawable(
                                    sActivity,
                                    org.chromium.chrome.browser.ui.appmenu.test.R.drawable
                                            .test_ic_refresh_black_24dp);
                    createIconRowMenuItem(
                            1,
                            MENU_ID1,
                            TITLE_1,
                            icon1,
                            MENU_ID2,
                            TITLE_2,
                            icon2,
                            MENU_ID3,
                            TITLE_3,
                            icon3,
                            MENU_ID4,
                            TITLE_4,
                            icon4,
                            MENU_ID5,
                            TITLE_5,
                            icon5,
                            mMenuItemEnabled);
                });
        waitListViewToBeDrawn();
        mRenderTestRule.render(sContentView, "iconrow_five_icons");
    }
}
