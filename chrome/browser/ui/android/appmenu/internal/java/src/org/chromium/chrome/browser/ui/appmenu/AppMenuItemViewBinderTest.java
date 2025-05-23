// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import static org.junit.Assert.assertThrows;

import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.os.SystemClock;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.MediumTest;

import com.google.android.material.button.MaterialButton;

import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler.AppMenuItemType;
import org.chromium.chrome.browser.ui.appmenu.test.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;
import org.chromium.components.browser_ui.util.motion.MotionEventTestUtils;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.widget.ChromeImageView;

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
        public void onItemClick(PropertyModel model, @Nullable MotionEventInfo triggeringMotion) {
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
    static final int MENU_ID8 = 800;
    static final String TITLE_1 = "Menu Item One";
    static final String TITLE_2 = "Menu Item Two";
    static final String TITLE_3 = "Menu Item Three";
    static final String TITLE_4 = "Menu Item Four";
    static final String TITLE_5 = "Menu Item Five";
    static final String TITLE_6 = "Menu Item Six";
    static final String TITLE_7 = "Menu Item Seven";
    static final String TITLE_8 = "Menu Item Eight";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
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
        mClickHandler = new TestClickHandler();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivity = sActivityTestRule.getActivity();
                    mMenuList = new ModelListAdapter.ModelList();
                    mModelListAdapter = new ModelListAdapter(mMenuList);

                    AppMenuHandlerImpl.registerDefaultViewBinders(mModelListAdapter, true);
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
            int titleMenuId,
            String title,
            @Nullable Drawable menuIcon,
            int buttonMenuId,
            String buttonTitle,
            boolean checkable,
            boolean checked) {
        PropertyModel titleModel =
                new PropertyModel.Builder(AppMenuItemProperties.ALL_KEYS)
                        .with(AppMenuItemProperties.MENU_ITEM_ID, titleMenuId)
                        .with(AppMenuItemProperties.TITLE, title)
                        .with(AppMenuItemProperties.ICON, menuIcon)
                        .build();

        PropertyModel buttonModel =
                new PropertyModel.Builder(AppMenuItemProperties.ALL_ICON_KEYS)
                        .with(AppMenuItemProperties.MENU_ITEM_ID, buttonMenuId)
                        .with(AppMenuItemProperties.TITLE, buttonTitle)
                        .with(AppMenuItemProperties.CHECKABLE, checkable)
                        .with(AppMenuItemProperties.CHECKED, checked)
                        .build();

        ModelListAdapter.ModelList subList = new ModelListAdapter.ModelList();
        subList.add(new ModelListAdapter.ListItem(0, buttonModel));

        titleModel.set(AppMenuItemProperties.ADDITIONAL_ICONS, subList);
        mMenuList.add(new ModelListAdapter.ListItem(AppMenuItemType.TITLE_BUTTON, titleModel));

        return titleModel;
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
        int menutype = AppMenuItemType.BUTTON_ROW;
        createIconMenuItem(subList, subId1, titleCondensed1, icon1);
        createIconMenuItem(subList, subId2, titleCondensed2, icon2);
        createIconMenuItem(subList, subId3, titleCondensed3, icon3);
        if (subId4 != View.NO_ID) {
            createIconMenuItem(subList, subId4, titleCondensed4, icon4);
            if (subId5 != View.NO_ID) {
                createIconMenuItem(subList, subId5, titleCondensed5, icon5);
            }
        }

        model.set(AppMenuItemProperties.ADDITIONAL_ICONS, subList);
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

    @Test
    @UiThreadTest
    @MediumTest
    public void testStandardMenuItem_WithMenuTitle() {
        createStandardMenuItem(MENU_ID1, TITLE_1);

        ViewGroup parentView = mActivity.findViewById(android.R.id.content);
        View view = mModelListAdapter.getView(0, null, parentView);
        TextView titleView = view.findViewById(R.id.menu_item_text);
        ChromeImageView itemIcon = view.findViewById(R.id.menu_item_icon);

        Assert.assertEquals("Incorrect title text for item 1", TITLE_1, titleView.getText());
        Assert.assertNull("Should not have icon for item 1", itemIcon.getDrawable());
    }

    @Test
    @UiThreadTest
    @MediumTest
    public void testStandardMenuItem_WithMenuIcon() {
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
    public void testStandardMenuItem_WithClickHandler_OnClickListener() throws TimeoutException {
        PropertyModel standardModel = createStandardMenuItem(MENU_ID1, TITLE_1);
        standardModel.set(AppMenuItemProperties.CLICK_HANDLER, mClickHandler);

        ViewGroup parentView = mActivity.findViewById(android.R.id.content);
        View view =
                mModelListAdapter.getView(/* position= */ 0, /* convertView= */ null, parentView);
        view.performClick();
        mClickHandler.onClickCallback.waitForCallback(/* currentCallCount= */ 0);

        Assert.assertEquals(
                "Incorrect clicked item id",
                MENU_ID1,
                mClickHandler.lastClickedModel.get(AppMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    @UiThreadTest
    @MediumTest
    public void testStandardMenuItem_WithClickHandler_OnPeripheralClickListener_TouchScreenClick() {
        PropertyModel standardModel = createStandardMenuItem(MENU_ID1, TITLE_1);
        standardModel.set(AppMenuItemProperties.CLICK_HANDLER, mClickHandler);

        ViewGroup parentView = mActivity.findViewById(android.R.id.content);
        View view =
                mModelListAdapter.getView(/* position= */ 0, /* convertView= */ null, parentView);
        simulateClickWithMotionEvents(
                view, InputDevice.SOURCE_TOUCHSCREEN, MotionEvent.TOOL_TYPE_FINGER);

        // Simulated touch screen motion events should not trigger OnPeripheralClickListener.
        // As the motion events are simulated using dispatchTouchEvent(), the OnClickListener will
        // not be triggered either.
        // Therefore, the onClickCallback should not be called.
        assertThrows(
                TimeoutException.class,
                () -> mClickHandler.onClickCallback.waitForCallback(/* currentCallCount= */ 0));
    }

    @Test
    @UiThreadTest
    @MediumTest
    public void testStandardMenuItem_WithClickHandler_OnPeripheralClickListener_PeripheralClick()
            throws TimeoutException {
        PropertyModel standardModel = createStandardMenuItem(MENU_ID1, TITLE_1);
        standardModel.set(AppMenuItemProperties.CLICK_HANDLER, mClickHandler);

        ViewGroup parentView = mActivity.findViewById(android.R.id.content);
        View view =
                mModelListAdapter.getView(/* position= */ 0, /* convertView= */ null, parentView);
        simulateClickWithMotionEvents(view, InputDevice.SOURCE_MOUSE, MotionEvent.TOOL_TYPE_MOUSE);

        // Simulated mouse motion events should trigger OnPeripheralClickListener.
        mClickHandler.onClickCallback.waitForCallback(/* currentCallCount= */ 0);
        Assert.assertEquals(
                "Incorrect clicked item id",
                MENU_ID1,
                mClickHandler.lastClickedModel.get(AppMenuItemProperties.MENU_ITEM_ID));
    }

    /**
     * Simulates a click event using {@link View#dispatchTouchEvent(MotionEvent)}.
     *
     * <p>This will not trigger {@link View.OnClickListener}.
     *
     * @param view the View to receive the click.
     * @param motionSource see {@link MotionEvent#getSource()}.
     * @param motionToolType see {@link MotionEvent#getToolType(int)}.
     */
    private static void simulateClickWithMotionEvents(
            View view, int motionSource, int motionToolType) {
        long downTime = SystemClock.uptimeMillis();
        view.dispatchTouchEvent(
                MotionEventTestUtils.createMotionEvent(
                        downTime,
                        /* eventTime= */ downTime,
                        MotionEvent.ACTION_DOWN,
                        /* x= */ 0,
                        /* y= */ 0,
                        motionSource,
                        motionToolType));
        view.dispatchTouchEvent(
                MotionEventTestUtils.createMotionEvent(
                        downTime,
                        /* eventTime= */ downTime + 50,
                        MotionEvent.ACTION_UP,
                        /* x= */ 0,
                        /* y= */ 0,
                        motionSource,
                        motionToolType));
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
        createTitleMenuItem(MENU_ID2, TITLE_2, null, MENU_ID3, TITLE_3, true, true);
        createTitleMenuItem(MENU_ID5, TITLE_5, null, MENU_ID6, TITLE_6, true, false);

        Assert.assertEquals(
                "Wrong item view type",
                AppMenuItemType.TITLE_BUTTON,
                mModelListAdapter.getItemViewType(0));

        ViewGroup parentView = mActivity.findViewById(android.R.id.content);
        View view1 = mModelListAdapter.getView(0, null, parentView);
        TextView titleView = view1.findViewById(R.id.menu_item_text);

        Assert.assertEquals("Incorrect title text for item 1", TITLE_2, titleView.getText());

        ImageView iconView = view1.findViewById(R.id.menu_item_icon);
        Assert.assertNotNull(iconView);
        Assert.assertNotEquals(View.VISIBLE, iconView.getVisibility());

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
        createTitleMenuItem(MENU_ID2, TITLE_2, icon, MENU_ID3, TITLE_3, true, true);
        createTitleMenuItem(MENU_ID5, TITLE_5, icon, MENU_ID6, TITLE_6, true, false);

        Assert.assertEquals(
                "Wrong item view type",
                AppMenuItemType.TITLE_BUTTON,
                mModelListAdapter.getItemViewType(0));

        ViewGroup parentView = mActivity.findViewById(android.R.id.content);
        View view1 = mModelListAdapter.getView(0, null, parentView);
        Assert.assertNotNull(
                "Should have icon for item 1", view1.findViewById(R.id.menu_item_icon));

        View view2 = mModelListAdapter.getView(1, view1, parentView);
        Assert.assertEquals("Convert view should have been re-used", view1, view2);
    }

    @Test
    @UiThreadTest
    @MediumTest
    public void testConvertView_Reused_IconRow_SameButtonCount() {
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
    public void testConvertView_Reused_IconRow_IncreasingButtonCount() {
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
        Assert.assertEquals(3, countVisibleChildren((ViewGroup) view1));

        View view2 = mModelListAdapter.getView(1, view1, parentView);
        Assert.assertEquals(4, countVisibleChildren((ViewGroup) view2));
        Assert.assertEquals("Convert view should be re-used", view1, view2);
    }

    @Test
    @UiThreadTest
    @MediumTest
    public void testConvertView_Reused_IconRow_DecreasingButtonCount() {
        Drawable icon =
                AppCompatResources.getDrawable(
                        mActivity,
                        org.chromium.chrome.browser.ui.appmenu.test.R.drawable
                                .test_ic_vintage_filter);
        createIconRowMenuItem(
                1, MENU_ID4, TITLE_4, icon, MENU_ID5, TITLE_5, icon, MENU_ID6, TITLE_6, icon,
                MENU_ID7, TITLE_7, icon, MENU_ID8, TITLE_8, icon);
        createIconRowMenuItem(
                2,
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

        ViewGroup parentView = mActivity.findViewById(android.R.id.content);
        View view1 = mModelListAdapter.getView(0, null, parentView);
        Assert.assertEquals(5, countVisibleChildren((ViewGroup) view1));

        View view2 = mModelListAdapter.getView(1, view1, parentView);
        Assert.assertEquals(3, countVisibleChildren((ViewGroup) view2));
        Assert.assertEquals("Convert view should be re-used", view1, view2);
    }

    private static int countVisibleChildren(ViewGroup view) {
        int visibleChildCount = 0;
        for (int i = 0; i < view.getChildCount(); i++) {
            if (view.getChildAt(i).getVisibility() == View.VISIBLE) visibleChildCount++;
        }
        return visibleChildCount;
    }

    @Test
    @UiThreadTest
    @MediumTest
    public void testConvertView_NotReused() {
        createStandardMenuItem(MENU_ID1, TITLE_1);
        createTitleMenuItem(MENU_ID3, TITLE_3, null, MENU_ID4, TITLE_4, true, true);

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
    public void testTitleMenuItem_Checkbox() {
        createTitleMenuItem(MENU_ID2, TITLE_2, null, MENU_ID3, TITLE_3, true, true);

        ViewGroup parentView = mActivity.findViewById(android.R.id.content);
        View view = mModelListAdapter.getView(0, null, parentView);
        AppMenuItemIcon checkbox = view.findViewById(R.id.checkbox);

        Assert.assertTrue("Checkbox should be checked", checkbox.isChecked());
    }

    @Test
    @UiThreadTest
    @MediumTest
    public void testTitleMenuItem_ToggleCheckbox() {
        createTitleMenuItem(MENU_ID2, TITLE_2, null, MENU_ID3, TITLE_3, true, false);

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
        MaterialButton button = view.findViewById(R.id.button_one);
        Assert.assertEquals(
                "Incorrect content description for icon 1",
                TITLE_1,
                button.getContentDescription());
        Assert.assertNotNull("Should have an icon for icon 1", button.getIcon());

        button = view.findViewById(R.id.button_two);
        Assert.assertEquals(
                "Incorrect content description for icon 2",
                TITLE_2,
                button.getContentDescription());
        Assert.assertNotNull("Should have an icon for icon 2", button.getIcon());

        button = view.findViewById(R.id.button_three);
        Assert.assertEquals(
                "Incorrect content description for icon 3",
                TITLE_3,
                button.getContentDescription());
        Assert.assertNotNull("Should have an icon for icon 3", button.getIcon());

        button = view.findViewById(R.id.button_four);
        Assert.assertEquals(
                "Incorrect content description for icon 4",
                TITLE_4,
                button.getContentDescription());
        Assert.assertNotNull("Should have an icon for icon 4", button.getIcon());

        button = view.findViewById(R.id.button_five);
        Assert.assertEquals(
                "Incorrect content description for icon 5",
                TITLE_5,
                button.getContentDescription());
        Assert.assertNotNull("Should have an icon for icon 5", button.getIcon());
    }
}
