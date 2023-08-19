// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import static org.mockito.ArgumentMatchers.eq;

import android.graphics.Canvas;
import android.graphics.Rect;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.LifecycleObserver;
import org.chromium.chrome.browser.ui.appmenu.test.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighterTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;
import org.chromium.ui.test.util.UiDisableIf;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Integration tests the app menu popup. Covers AppMenuCoordinatorImpl and public interface for
 * AppMenuHandlerImpl.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class AppMenuTest extends BlankUiTestActivityTestCase {
    private AppMenuCoordinatorImpl mAppMenuCoordinator;
    private AppMenuHandlerImpl mAppMenuHandler;
    private TestAppMenuPropertiesDelegate mPropertiesDelegate;
    private TestAppMenuDelegate mDelegate;
    private TestAppMenuObserver mMenuObserver;
    private TestActivityLifecycleDispatcher mLifecycleDispatcher;
    private TestMenuButtonDelegate mTestMenuButtonDelegate;

    @Mock
    private Canvas mCanvas;
    // Tell R8 not to break the ability to mock the class.
    @Mock
    private AppMenu mUnused;

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        BlankUiTestActivity.setTestLayout(R.layout.test_app_menu_activity_layout);
    }

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        mCanvas = Mockito.mock(Canvas.class);
        TestThreadUtils.runOnUiThreadBlocking(this::setUpTestOnUiThread);
        mLifecycleDispatcher.observerRegisteredCallbackHelper.waitForCallback(0);
    }

    @AfterClass
    public static void tearDownAfterActivityDestroyed() {}

    private void setUpTestOnUiThread() {
        mLifecycleDispatcher = new TestActivityLifecycleDispatcher();
        mDelegate = new TestAppMenuDelegate();
        mTestMenuButtonDelegate = new TestMenuButtonDelegate();
        mAppMenuCoordinator = new AppMenuCoordinatorImpl(getActivity(), mLifecycleDispatcher,
                mTestMenuButtonDelegate, mDelegate, getActivity().getWindow().getDecorView(),
                getActivity().findViewById(R.id.menu_anchor_stub), this::getAppRect);
        mAppMenuHandler = mAppMenuCoordinator.getAppMenuHandlerImplForTesting();
        mMenuObserver = new TestAppMenuObserver();
        mAppMenuCoordinator.getAppMenuHandler().addObserver(mMenuObserver);
        mPropertiesDelegate =
                (TestAppMenuPropertiesDelegate) mAppMenuCoordinator.getAppMenuPropertiesDelegate();
    }

    private Rect getAppRect() {
        Rect appRect = new Rect();
        getActivity().getWindow().getDecorView().getWindowVisibleDisplayFrame(appRect);
        return appRect;
    }

    @Test
    @MediumTest
    public void testShowHideAppMenu() throws TimeoutException {
        showMenuAndAssert();

        TestThreadUtils.runOnUiThreadBlocking(() -> mAppMenuHandler.hideAppMenu());
        mMenuObserver.menuHiddenCallback.waitForCallback(0);

        Assert.assertEquals("Incorrect number of calls to #onMenuDismissed after hide", 1,
                mPropertiesDelegate.menuDismissedCallback.getCallCount());

        TestThreadUtils.runOnUiThreadBlocking(() -> mAppMenuCoordinator.destroy());
        Assert.assertEquals("Incorrect number of calls to #onMenuDismissed after destroy", 1,
                mPropertiesDelegate.menuDismissedCallback.getCallCount());
    }

    @Test
    @MediumTest
    public void testHideAppMenuMultiple() throws TimeoutException {
        showMenuAndAssert();

        TestThreadUtils.runOnUiThreadBlocking(() -> mAppMenuHandler.getAppMenu().dismiss());
        mMenuObserver.menuHiddenCallback.waitForCallback(0);

        Assert.assertEquals("Incorrect number of calls to #onMenuDismissed after first call", 1,
                mPropertiesDelegate.menuDismissedCallback.getCallCount());

        TestThreadUtils.runOnUiThreadBlocking(() -> mAppMenuHandler.getAppMenu().dismiss());
        Assert.assertEquals("Incorrect number of calls to #onMenuDismissed after second call", 1,
                mPropertiesDelegate.menuDismissedCallback.getCallCount());
    }

    @Test
    @MediumTest
    public void testShowAppMenu_AnchorTop() throws TimeoutException {
        AppMenuCoordinatorImpl.setHasPermanentMenuKeyForTesting(false);
        showMenuAndAssert();

        View topAnchor = getActivity().findViewById(R.id.top_button);
        Rect viewRect = getViewLocationRect(topAnchor);
        Rect popupRect = getPopupLocationRect();

        // Check that top right corner of app menu aligns with the top right corner of the anchor.
        int alignmentSlop = viewRect.bottom - viewRect.top;
        Assert.assertEquals("Popup should overlap top anchor. Anchor rect: " + viewRect
                        + ", popup rect: " + popupRect,
                viewRect.top, popupRect.top, alignmentSlop);
        Assert.assertTrue("Popup should overlap top anchor. Anchor rect: " + viewRect
                        + ", popup rect: " + popupRect,
                viewRect.top <= popupRect.top);
        Assert.assertEquals("Popup should be aligned with right of anchor. Anchor rect: " + viewRect
                        + ", popup rect: " + popupRect,
                viewRect.right, popupRect.right);
    }

    @Test
    @MediumTest
    public void testShowAppMenu_PermanentButton() throws TimeoutException {
        AppMenuCoordinatorImpl.setHasPermanentMenuKeyForTesting(true);
        showMenuAndAssert();

        View anchorStub = getActivity().findViewById(R.id.menu_anchor_stub);
        Rect viewRect = getViewLocationRect(anchorStub);
        Rect popupRect = getPopupLocationRect();

        // Check a basic alignment property. Full coverage checked in unit tests.
        Assert.assertNotEquals("Popup should be offset from right of anchor."
                        + "Anchor rect: " + viewRect + ", popup rect: " + popupRect,
                viewRect.right, popupRect.right);
    }

    @Test
    @MediumTest
    public void testShowDestroyAppMenu() throws TimeoutException {
        showMenuAndAssert();

        TestThreadUtils.runOnUiThreadBlocking(() -> mAppMenuCoordinator.destroy());

        Assert.assertEquals("Incorrect number of calls to #onMenuDismissed after destroy", 1,
                mPropertiesDelegate.menuDismissedCallback.getCallCount());
    }

    @Test
    @MediumTest
    public void testClickMenuItem() throws TimeoutException {
        showMenuAndAssert();

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> AppMenuTestSupport.callOnItemClick(
                                mAppMenuCoordinator, R.id.menu_item_three));

        mDelegate.itemSelectedCallbackHelper.waitForCallback(0);
        Assert.assertEquals("Incorrect id for last selected item.", R.id.menu_item_three,
                mDelegate.lastSelectedItemId);
    }

    @Test
    @MediumTest
    public void testClickMenuItem_Disabled() throws TimeoutException {
        showMenuAndAssert();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> AppMenuTestSupport.callOnItemClick(mAppMenuCoordinator, R.id.menu_item_two));

        Assert.assertEquals("Item selected callback should not have been called.", 0,
                mDelegate.itemSelectedCallbackHelper.getCallCount());
    }

    @Test
    @MediumTest
    public void testClickMenuItem_UsingPosition() throws TimeoutException {
        showMenuAndAssert();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mAppMenuHandler.getAppMenu().onItemClick(null, null, 0, 0));

        mDelegate.itemSelectedCallbackHelper.waitForCallback(0);
        Assert.assertEquals("Incorrect id for last selected item.", R.id.menu_item_one,
                mDelegate.lastSelectedItemId);
    }

    @Test
    @MediumTest
    public void testLongClickMenuItem_Title() throws TimeoutException {
        mPropertiesDelegate.enableAppIconRow = true;
        showMenuAndAssert();
        AppMenu spiedMenu = Mockito.spy(mAppMenuHandler.getAppMenu());

        View dummyView = new View(getActivity());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            spiedMenu.onItemLongClick(
                    mAppMenuHandler.getAppMenu().getMenuItemPropertyModel(R.id.icon_one),
                    dummyView);
        });

        Mockito.verify(spiedMenu, Mockito.times(1)).showToastForItem("Icon One", dummyView);
    }

    @Test
    @MediumTest
    public void testLongClickMenuItem_TitleCondensed() throws TimeoutException {
        mPropertiesDelegate.enableAppIconRow = true;
        showMenuAndAssert();
        AppMenu spiedMenu = Mockito.spy(mAppMenuHandler.getAppMenu());

        View dummyView = new View(getActivity());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            spiedMenu.onItemLongClick(
                    mAppMenuHandler.getAppMenu().getMenuItemPropertyModel(R.id.icon_two),
                    dummyView);
        });

        Mockito.verify(spiedMenu, Mockito.times(1)).showToastForItem("2", dummyView);
    }

    @Test
    @MediumTest
    public void testLongClickMenuItem_Disabled() throws TimeoutException {
        mPropertiesDelegate.enableAppIconRow = true;
        showMenuAndAssert();
        AppMenu spiedMenu = Mockito.spy(mAppMenuHandler.getAppMenu());

        View dummyView = new View(getActivity());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            spiedMenu.onItemLongClick(
                    mAppMenuHandler.getAppMenu().getMenuItemPropertyModel(R.id.icon_three),
                    dummyView);
        });

        Mockito.verify(spiedMenu, Mockito.times(0))
                .showToastForItem(Mockito.any(CharSequence.class), Mockito.any(View.class));
    }

    @Test
    @MediumTest
    public void testAppMenuBlockers() throws TimeoutException {
        Assert.assertTrue("App menu should be allowed to show, no blockers registered",
                AppMenuTestSupport.shouldShowAppMenu(mAppMenuCoordinator));

        AppMenuBlocker blocker1 = () -> false;
        AppMenuBlocker blocker2 = () -> true;

        mAppMenuCoordinator.registerAppMenuBlocker(blocker1);
        mAppMenuCoordinator.registerAppMenuBlocker(blocker2);
        Assert.assertFalse("App menu should not be allowed to show, both blockers registered",
                AppMenuTestSupport.shouldShowAppMenu(mAppMenuCoordinator));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mAppMenuCoordinator.showAppMenuForKeyboardEvent());
        Assert.assertFalse(
                "App menu should not have been shown.", mAppMenuHandler.isAppMenuShowing());

        mAppMenuCoordinator.unregisterAppMenuBlocker(blocker1);
        Assert.assertTrue("App menu should be allowed to show, only blocker2 registered",
                AppMenuTestSupport.shouldShowAppMenu(mAppMenuCoordinator));
        showMenuAndAssert();
    }

    @Test
    @MediumTest
    public void testSetMenuHighlight_StandardItem() throws TimeoutException {
        Assert.assertFalse(mMenuObserver.menuHighlighting);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mAppMenuHandler.setMenuHighlight(R.id.menu_item_one));
        mMenuObserver.menuHighlightChangedCallback.waitForCallback(0);
        Assert.assertTrue(mMenuObserver.menuHighlighting);

        showMenuAndAssert();

        View itemView = getViewAtPosition(0);
        checkHighlightOn(itemView);

        TestThreadUtils.runOnUiThreadBlocking(() -> mAppMenuHandler.clearMenuHighlight());
        mMenuObserver.menuHighlightChangedCallback.waitForCallback(1);
        Assert.assertFalse(mMenuObserver.menuHighlighting);
    }

    @Test
    @MediumTest
    public void testSetMenuHighlight_ChipItem() throws TimeoutException {
        mPropertiesDelegate.footerResourceId = R.layout.test_menu_footer;
        Assert.assertFalse(mMenuObserver.menuHighlighting);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mAppMenuHandler.setMenuHighlight(R.id.menu_footer_chip_view));
        mMenuObserver.menuHighlightChangedCallback.waitForCallback(0);
        Assert.assertTrue(mMenuObserver.menuHighlighting);

        showMenuAndAssert();
        mPropertiesDelegate.footerInflatedCallback.waitForCallback(0);

        ChipView chipView =
                (ChipView) mAppMenuHandler.getAppMenu().getListView().getRootView().findViewById(
                        R.id.menu_footer_chip_view);
        checkHighlightOn(chipView);

        ViewHighlighterTestUtils.drawPulseDrawable(chipView, mCanvas);
        Mockito.verify(mCanvas).drawRoundRect(Mockito.any(), eq((float) chipView.getCornerRadius()),
                eq((float) chipView.getCornerRadius()), Mockito.any());

        TestThreadUtils.runOnUiThreadBlocking(() -> mAppMenuHandler.clearMenuHighlight());
        mMenuObserver.menuHighlightChangedCallback.waitForCallback(1);
        Assert.assertFalse(mMenuObserver.menuHighlighting);
    }

    @Test
    @MediumTest
    public void testSetMenuHighlight_Icon() throws TimeoutException {
        mPropertiesDelegate.enableAppIconRow = true;

        Assert.assertFalse(mMenuObserver.menuHighlighting);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mAppMenuHandler.setMenuHighlight(R.id.icon_one));
        mMenuObserver.menuHighlightChangedCallback.waitForCallback(0);
        Assert.assertTrue(mMenuObserver.menuHighlighting);

        showMenuAndAssert();

        View itemView = ((LinearLayout) getViewAtPosition(3)).getChildAt(0);
        checkHighlightOn(itemView);

        TestThreadUtils.runOnUiThreadBlocking(() -> mAppMenuHandler.clearMenuHighlight());
        mMenuObserver.menuHighlightChangedCallback.waitForCallback(1);
        Assert.assertFalse(mMenuObserver.menuHighlighting);
    }

    @Test
    @MediumTest
    public void testMenuItemContentChanged() throws TimeoutException {
        showMenuAndAssert();
        View itemView = getViewAtPosition(1);
        Assert.assertEquals("Menu item text incorrect", "Menu Item Two",
                ((TextView) itemView.findViewById(R.id.menu_item_text)).getText());

        String newText = "Test!";
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mAppMenuHandler.getAppMenu()
                    .getMenuItemPropertyModel(R.id.menu_item_two)
                    .set(AppMenuItemProperties.TITLE, newText);
            mAppMenuHandler.menuItemContentChanged(R.id.menu_item_two);
        });

        itemView = getViewAtPosition(1);
        Assert.assertEquals("Menu item text incorrect", newText,
                ((TextView) itemView.findViewById(R.id.menu_item_text)).getText());
    }

    @Test
    @MediumTest
    public void testHeaderFooter() throws TimeoutException {
        mPropertiesDelegate.headerResourceId = R.layout.test_menu_header;
        mPropertiesDelegate.footerResourceId = R.layout.test_menu_footer;
        showMenuAndAssert();

        mPropertiesDelegate.headerInflatedCallback.waitForCallback(0);
        mPropertiesDelegate.footerInflatedCallback.waitForCallback(0);

        Assert.assertEquals("Incorrect number of header views", 1,
                mAppMenuHandler.getAppMenu().getListView().getHeaderViewsCount());
        Assert.assertNotNull("Footer stub not inflated.",
                mAppMenuHandler.getAppMenu().getPopup().getContentView().findViewById(
                        R.id.app_menu_footer));
    }

    @Test
    @MediumTest
    public void testAppMenuHiddenOnStopWithNative() throws TimeoutException {
        showMenuAndAssert();
        TestThreadUtils.runOnUiThreadBlocking(() -> mAppMenuHandler.onStopWithNative());
        Assert.assertFalse(mAppMenuHandler.isAppMenuShowing());
    }

    @Test
    @MediumTest
    public void testAppMenuHiddenOnConfigurationChange() throws TimeoutException {
        showMenuAndAssert();
        TestThreadUtils.runOnUiThreadBlocking(() -> mAppMenuHandler.onConfigurationChanged(null));
        Assert.assertFalse(mAppMenuHandler.isAppMenuShowing());
    }

    @Test
    @MediumTest
    public void testAppMenuKeyEvent_HiddenOnHardwareButtonPress() throws Exception {
        showMenuAndAssert();

        AppMenu appMenu = mAppMenuHandler.getAppMenu();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            KeyEvent down = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_MENU);
            KeyEvent up = new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_MENU);
            appMenu.onKey(appMenu.getListView(), KeyEvent.KEYCODE_MENU, down);
            appMenu.onKey(appMenu.getListView(), KeyEvent.KEYCODE_MENU, up);
        });
        mMenuObserver.menuHiddenCallback.waitForCallback(0);
    }

    @Test
    @MediumTest
    public void testAppMenuKeyEvent_IgnoreUnrelatedKeyCode() throws Exception {
        showMenuAndAssert();

        AppMenu appMenu = mAppMenuHandler.getAppMenu();
        KeyEvent unrelated = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_BOOKMARK);
        Assert.assertFalse("#onKeyEvent should return false for unrelated codes",
                appMenu.onKey(null, KeyEvent.KEYCODE_BOOKMARK, unrelated));
    }

    @Test
    @MediumTest
    public void testAppMenuKeyEvent_IgnoreUnrelatedKeyEvent() throws Exception {
        showMenuAndAssert();

        AppMenu appMenu = mAppMenuHandler.getAppMenu();
        KeyEvent unrelated = new KeyEvent(KeyEvent.ACTION_MULTIPLE, KeyEvent.KEYCODE_MENU);
        Assert.assertFalse("#onKeyEvent should return false for unrelated events",
                appMenu.onKey(null, KeyEvent.KEYCODE_MENU, unrelated));
    }

    @Test
    @MediumTest
    public void testAppMenuKeyEvent_IgnoreEventsWhenHidden() throws Exception {
        // Show app menu to initialize, then hide.
        showMenuAndAssert();
        TestThreadUtils.runOnUiThreadBlocking(() -> mAppMenuHandler.hideAppMenu());
        mMenuObserver.menuHiddenCallback.waitForCallback(0);

        AppMenu appMenu = mAppMenuHandler.getAppMenu();
        Assert.assertNull("ListView should be null.", appMenu.getListView());
        KeyEvent down = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_MENU);
        Assert.assertFalse("#onKeyEvent should return false when app menu hidden",
                appMenu.onKey(null, KeyEvent.KEYCODE_MENU, null));
    }

    @Test
    @MediumTest
    public void testAppMenuButtonHelper_DownUp() throws Exception {
        AppMenuButtonHelperImpl buttonHelper =
                (AppMenuButtonHelperImpl) mAppMenuHandler.createAppMenuButtonHelper();

        Assert.assertFalse("View should start unpressed",
                mTestMenuButtonDelegate.getMenuButtonView().isPressed());
        Assert.assertFalse("App menu should be not be active", buttonHelper.isAppMenuActive());

        MotionEvent downMotionEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        sendMotionEventToButtonHelper(
                buttonHelper, mTestMenuButtonDelegate.getMenuButtonView(), downMotionEvent);

        waitForMenuToShow(0);
        Assert.assertTrue("Menu should be showing", mAppMenuHandler.isAppMenuShowing());
        Assert.assertTrue(
                "View should be pressed", mTestMenuButtonDelegate.getMenuButtonView().isPressed());
        Assert.assertTrue("App menu should be active", buttonHelper.isAppMenuActive());

        MotionEvent upMotionEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, 0, 0, 0);
        sendMotionEventToButtonHelper(
                buttonHelper, mTestMenuButtonDelegate.getMenuButtonView(), upMotionEvent);

        Assert.assertFalse("View should no longer be pressed",
                mTestMenuButtonDelegate.getMenuButtonView().isPressed());
        Assert.assertTrue("App menu should still be active", buttonHelper.isAppMenuActive());
    }

    @Test
    @MediumTest
    public void testAppMenuButtonHelper_DownCancel() throws Exception {
        AppMenuButtonHelperImpl buttonHelper =
                (AppMenuButtonHelperImpl) mAppMenuHandler.createAppMenuButtonHelper();
        Assert.assertFalse("View should start unpressed",
                mTestMenuButtonDelegate.getMenuButtonView().isPressed());

        MotionEvent downMotionEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        sendMotionEventToButtonHelper(
                buttonHelper, mTestMenuButtonDelegate.getMenuButtonView(), downMotionEvent);

        waitForMenuToShow(0);
        Assert.assertTrue("Menu should be showing", mAppMenuHandler.isAppMenuShowing());

        Assert.assertTrue(
                "View should be pressed", mTestMenuButtonDelegate.getMenuButtonView().isPressed());

        MotionEvent cancelMotionEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_CANCEL, 0, 0, 0);
        sendMotionEventToButtonHelper(
                buttonHelper, mTestMenuButtonDelegate.getMenuButtonView(), cancelMotionEvent);

        Assert.assertFalse("View should no longer be pressed",
                mTestMenuButtonDelegate.getMenuButtonView().isPressed());
    }

    @Test
    @MediumTest
    public void testAppMenuButtonHelper_ClickRunnable() throws Exception {
        Assert.assertFalse("View should start unpressed",
                mTestMenuButtonDelegate.getMenuButtonView().isPressed());

        AppMenuButtonHelperImpl buttonHelper =
                (AppMenuButtonHelperImpl) mAppMenuHandler.createAppMenuButtonHelper();
        CallbackHelper clickCallbackHelper = new CallbackHelper();
        Runnable clickRunnable = () -> clickCallbackHelper.notifyCalled();
        buttonHelper.setOnClickRunnable(clickRunnable);

        MotionEvent downMotionEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        sendMotionEventToButtonHelper(
                buttonHelper, mTestMenuButtonDelegate.getMenuButtonView(), downMotionEvent);

        clickCallbackHelper.waitForCallback(0);
        waitForMenuToShow(0);
    }

    @Test
    @MediumTest
    public void testAppMenuButtonHelper_ShowTwice() throws Exception {
        AppMenuButtonHelperImpl buttonHelper =
                (AppMenuButtonHelperImpl) mAppMenuHandler.createAppMenuButtonHelper();

        CallbackHelper showCallbackHelper = new CallbackHelper();
        Runnable showListener = () -> showCallbackHelper.notifyCalled();
        buttonHelper.setOnAppMenuShownListener(showListener);

        MotionEvent downMotionEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        sendMotionEventToButtonHelper(
                buttonHelper, mTestMenuButtonDelegate.getMenuButtonView(), downMotionEvent);

        waitForMenuToShow(0);
        Assert.assertTrue("Menu should be showing", mAppMenuHandler.isAppMenuShowing());
        Assert.assertEquals(
                "Runnable should have been called once", 1, showCallbackHelper.getCallCount());

        sendMotionEventToButtonHelper(
                buttonHelper, mTestMenuButtonDelegate.getMenuButtonView(), downMotionEvent);

        Assert.assertEquals("Runnable should still only have been called once", 1,
                showCallbackHelper.getCallCount());
    }

    @Test
    @MediumTest
    public void testAppMenuButtonHelper_ShowBlocked() throws Exception {
        AppMenuButtonHelperImpl buttonHelper =
                (AppMenuButtonHelperImpl) mAppMenuHandler.createAppMenuButtonHelper();
        AppMenuBlocker blocker1 = () -> false;
        mAppMenuCoordinator.registerAppMenuBlocker(blocker1);

        CallbackHelper showCallbackHelper = new CallbackHelper();
        Runnable showListener = () -> showCallbackHelper.notifyCalled();
        buttonHelper.setOnAppMenuShownListener(showListener);

        MotionEvent downMotionEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        sendMotionEventToButtonHelper(
                buttonHelper, mTestMenuButtonDelegate.getMenuButtonView(), downMotionEvent);

        Assert.assertEquals(
                "Runnable should not have been called once", 0, showCallbackHelper.getCallCount());
    }

    @Test
    @MediumTest
    public void testAppMenuButtonHelper_AccessibilityActions() throws Exception {
        AppMenuButtonHelperImpl buttonHelper =
                (AppMenuButtonHelperImpl) mAppMenuHandler.createAppMenuButtonHelper();

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> buttonHelper.performAccessibilityAction(
                                mTestMenuButtonDelegate.getMenuButtonView(),
                                AccessibilityNodeInfo.ACTION_CLICK, null));

        waitForMenuToShow(0);
        Assert.assertTrue("Menu should be showing", mAppMenuHandler.isAppMenuShowing());

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> buttonHelper.performAccessibilityAction(
                                mTestMenuButtonDelegate.getMenuButtonView(),
                                AccessibilityNodeInfo.ACTION_CLICK, null));

        mMenuObserver.menuHiddenCallback.waitForCallback(0);
        Assert.assertFalse("Menu should be hidden", mAppMenuHandler.isAppMenuShowing());
    }

    @Test
    @MediumTest
    public void testAppMenuButtonHelper_showEnterKeyPress() throws Exception {
        AppMenuButtonHelperImpl buttonHelper =
                (AppMenuButtonHelperImpl) mAppMenuHandler.createAppMenuButtonHelper();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> buttonHelper.onEnterKeyPress(mTestMenuButtonDelegate.getMenuButtonView()));

        waitForMenuToShow(0);
        Assert.assertTrue("Menu should be showing", mAppMenuHandler.isAppMenuShowing());
    }

    @Test
    @MediumTest
    @DisableIf.Device(type = {UiDisableIf.TABLET})
    @DisabledTest(message = "crbug.com/1186468")
    public void testDragHelper_ClickItem() throws Exception {
        AppMenuButtonHelperImpl buttonHelper =
                (AppMenuButtonHelperImpl) mAppMenuHandler.createAppMenuButtonHelper();

        Assert.assertFalse("View should start unpressed",
                mTestMenuButtonDelegate.getMenuButtonView().isPressed());
        Assert.assertFalse("App menu should be not be active", buttonHelper.isAppMenuActive());

        MotionEvent downMotionEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        sendMotionEventToButtonHelper(
                buttonHelper, mTestMenuButtonDelegate.getMenuButtonView(), downMotionEvent);

        waitForMenuToShow(0);
        CriteriaHelper.pollUiThread(
                () -> mAppMenuHandler.getAppMenuDragHelper().isReadyForMenuItemAction());

        Rect firstItemScreenRect = getVisibleScreenRectAtPosition(0);
        int eventX = firstItemScreenRect.left + (firstItemScreenRect.width() / 2);
        int eventY = firstItemScreenRect.top + (firstItemScreenRect.height() / 2);

        MotionEvent dragMotionEvent =
                MotionEvent.obtain(0, 100, MotionEvent.ACTION_MOVE, eventX, eventY, 0);
        sendMotionEventToButtonHelper(
                buttonHelper, mTestMenuButtonDelegate.getMenuButtonView(), dragMotionEvent);

        MotionEvent upMotionEvent =
                MotionEvent.obtain(0, 150, MotionEvent.ACTION_UP, eventX, eventY, 0);
        sendMotionEventToButtonHelper(
                buttonHelper, mTestMenuButtonDelegate.getMenuButtonView(), upMotionEvent);
        mDelegate.itemSelectedCallbackHelper.waitForCallback(
                "itemRect: " + firstItemScreenRect + " eventX: " + eventX + " eventY: " + eventY,
                0);
        Assert.assertEquals("Incorrect id for last selected item.", R.id.menu_item_one,
                mDelegate.lastSelectedItemId);
    }

    @Test
    @SmallTest
    public void testCalculateHeightForItems_enoughSpace() throws Exception {
        showMenuAndAssert();

        List<Integer> menuItemIds = new ArrayList<Integer>();
        List<Integer> heightList = new ArrayList<Integer>();
        createMenuItem(menuItemIds, heightList, 0 /* id */, 10 /* height */);
        createMenuItem(menuItemIds, heightList, 1 /* id */, 10 /* height */);
        createMenuItem(menuItemIds, heightList, 2 /* id */, 10 /* height */);

        int height = mAppMenuHandler.getAppMenu().calculateHeightForItems(menuItemIds, heightList,
                -1 /* groupDividerResourceId */, 35 /* availableScreenSpace */);
        Assert.assertEquals(30, height);
    }

    @Test
    @SmallTest
    public void testCalculateHeightForItems_notEnoughSpaceForOneItem() throws Exception {
        showMenuAndAssert();

        List<Integer> menuItemIds = new ArrayList<Integer>();
        List<Integer> heightList = new ArrayList<Integer>();
        createMenuItem(menuItemIds, heightList, 0 /* id */, 10 /* height */);
        createMenuItem(menuItemIds, heightList, 1 /* id */, 10 /* height */);
        createMenuItem(menuItemIds, heightList, 2 /* id */, 10 /* height */);

        int height = mAppMenuHandler.getAppMenu().calculateHeightForItems(menuItemIds, heightList,
                -1 /* groupDividerResourceId */, 26 /* availableScreenSpace */);
        // The space only can fit the 1st and 2nd items and the partial 3rd item.
        Assert.assertEquals(25, height);
    }

    @Test
    @SmallTest
    public void testCalculateHeightForItems_notEnoughSpaceForTwoItem() throws Exception {
        showMenuAndAssert();

        List<Integer> menuItemIds = new ArrayList<Integer>();
        List<Integer> heightList = new ArrayList<Integer>();
        createMenuItem(menuItemIds, heightList, 0 /* id */, 10 /* height */);
        createMenuItem(menuItemIds, heightList, 1 /* id */, 10 /* height */);
        createMenuItem(menuItemIds, heightList, 2 /* id */, 10 /* height */);

        int height = mAppMenuHandler.getAppMenu().calculateHeightForItems(menuItemIds, heightList,
                -1 /* groupDividerResourceId */, 24 /* availableScreenSpace */);
        // The space only can fit the full 1st item, the full 2nd items and the partial 3rd item.
        // The space for the 3rd item is 4, but since the menu is small enough, we show the maximum
        // available height instead of switching to the partial 3rd item.
        Assert.assertEquals(24, height);
    }

    @Test
    @SmallTest
    public void testCalculateHeightForItems_notEnoughSpaceForThreeItem() throws Exception {
        showMenuAndAssert();

        List<Integer> menuItemIds = new ArrayList<Integer>();
        List<Integer> heightList = new ArrayList<Integer>();
        createMenuItem(menuItemIds, heightList, 0 /* id */, 10 /* height */);
        createMenuItem(menuItemIds, heightList, 1 /* id */, 10 /* height */);
        createMenuItem(menuItemIds, heightList, 2 /* id */, 10 /* height */);
        createMenuItem(menuItemIds, heightList, 3 /* id */, 10 /* height */);

        int height = mAppMenuHandler.getAppMenu().calculateHeightForItems(menuItemIds, heightList,
                -1 /* groupDividerResourceId */, 34 /* availableScreenSpace */);
        // The space only can fit the full 1st item, the full 2nd item, the full 3rd item, and the
        // partial 4th item. But the space for 4th item is 4, which is not enough to show partial
        // 3rd item(5 = LAST_ITEM_SHOW_FRACTION * 10), we show the partial 3rd item instead.
        Assert.assertEquals(25, height);
    }

    @Test
    @SmallTest
    public void testCalculateHeightForItems_notEnoughSpaceForDivider() throws Exception {
        showMenuAndAssert();

        List<Integer> menuItemIds = new ArrayList<Integer>();
        List<Integer> heightList = new ArrayList<Integer>();
        createMenuItem(menuItemIds, heightList, 0 /* id */, 10 /* height */);
        createMenuItem(menuItemIds, heightList, 1 /* id */, 10 /* height */);
        createMenuItem(menuItemIds, heightList, 2 /* id */, 10 /* height */);
        createMenuItem(menuItemIds, heightList, 3 /* id */, 10 /* height */);
        createMenuItem(menuItemIds, heightList, 4 /* id */, 10 /* height */);

        int height = mAppMenuHandler.getAppMenu().calculateHeightForItems(menuItemIds, heightList,
                3 /* groupDividerResourceId */, 36 /* availableScreenSpace */);
        // The space only can fit the 1st, 2nd, 3rd, and partial 4th item. But the 4th item is a
        // divider line, so we show only the partial 3rd item.
        Assert.assertEquals(25, height);
    }

    @Test
    @SmallTest
    public void testCalculateHeightForItems_showPartialDivider() throws Exception {
        showMenuAndAssert();

        List<Integer> menuItemIds = new ArrayList<Integer>();
        List<Integer> heightList = new ArrayList<Integer>();
        createMenuItem(menuItemIds, heightList, 0 /* id */, 10 /* height */);
        createMenuItem(menuItemIds, heightList, 1 /* id */, 10 /* height */);
        createMenuItem(menuItemIds, heightList, 2 /* id */, 10 /* height */);
        createMenuItem(menuItemIds, heightList, 3 /* id */, 10 /* height */);

        int height = mAppMenuHandler.getAppMenu().calculateHeightForItems(menuItemIds, heightList,
                2 /* groupDividerResourceId */, 26 /* availableScreenSpace */);
        // The space only can fit the 1st, 2nd and the partial 3rd item. The third item
        // is a divider line, and the menu is small enough that we still want to use all available
        // space.
        Assert.assertEquals(26, height);
    }

    @Test
    @SmallTest
    public void testCalculateHeightForItems_notEnoughSpaceForItemShowPartialDivider()
            throws Exception {
        showMenuAndAssert();

        List<Integer> menuItemIds = new ArrayList<Integer>();
        List<Integer> heightList = new ArrayList<Integer>();
        createMenuItem(menuItemIds, heightList, 0 /* id */, 10 /* height */);
        createMenuItem(menuItemIds, heightList, 1 /* id */, 10 /* height */);
        createMenuItem(menuItemIds, heightList, 2 /* id */, 10 /* height */);
        createMenuItem(menuItemIds, heightList, 3 /* id */, 10 /* height */);

        int height = mAppMenuHandler.getAppMenu().calculateHeightForItems(menuItemIds, heightList,
                2 /* groupDividerResourceId */, 34 /* availableScreenSpace */);
        // The space only can fit the full 1st, 2nd and 3rd item and the partial 4th item.
        // But the space for 4th item is 4, which is not enough to show partial 4th item(5 =
        // LAST_ITEM_SHOW_FRACTION * 10), so we should show the partial 3rd item instead. The third
        // item is a divider line, and the menu is small enough that we still want to use all
        // available space.
        Assert.assertEquals(34, height);
    }

    @Test
    @SmallTest
    public void testCalculateHeightForItems_minimalHight() throws Exception {
        showMenuAndAssert();

        List<Integer> menuItemIds = new ArrayList<Integer>();
        List<Integer> heightList = new ArrayList<Integer>();
        createMenuItem(menuItemIds, heightList, 0 /* id */, 10 /* height */);
        createMenuItem(menuItemIds, heightList, 1 /* id */, 10 /* height */);
        createMenuItem(menuItemIds, heightList, 2 /* id */, 10 /* height */);

        int height = mAppMenuHandler.getAppMenu().calculateHeightForItems(menuItemIds, heightList,
                -1 /* groupDividerResourceId */, 4 /* availableScreenSpace */);
        // The space is not enough for any item, but we still show 1 and half items at least.
        Assert.assertEquals(15, height);
    }

    @Test
    @SmallTest
    public void testCalculateHeightForItems_minimalHight_notEnoughSpaceForDivider()
            throws Exception {
        showMenuAndAssert();

        List<Integer> menuItemIds = new ArrayList<Integer>();
        List<Integer> heightList = new ArrayList<Integer>();
        createMenuItem(menuItemIds, heightList, 0 /* id */, 10 /* height */);
        createMenuItem(menuItemIds, heightList, 1 /* id */, 10 /* height */);
        createMenuItem(menuItemIds, heightList, 2 /* id */, 10 /* height */);

        int height = mAppMenuHandler.getAppMenu().calculateHeightForItems(menuItemIds, heightList,
                1 /* groupDividerResourceId */, 6 /* availableScreenSpace */);
        // The space is not enough for any item, but we still show 1 and half items at least.
        Assert.assertEquals(15, height);
    }

    @Test
    @SmallTest
    public void testCalculateHeightForItems_nagativeSpaceForZeroItems() throws Exception {
        showMenuAndAssert();

        List<Integer> menuItemIds = new ArrayList<Integer>();
        List<Integer> heightList = new ArrayList<Integer>();

        int height = mAppMenuHandler.getAppMenu().calculateHeightForItems(menuItemIds, heightList,
                1 /* groupDividerResourceId */, -1 /* availableScreenSpace */);
        // Make sure there are no crashes.
        Assert.assertEquals(0, height);
    }

    private void createMenuItem(
            List<Integer> menuItemIds, List<Integer> heightList, int id, int height) {
        menuItemIds.add(id);
        heightList.add(height);
    }

    private void showMenuAndAssert() throws TimeoutException {
        int currentCallCount = mMenuObserver.menuShownCallback.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mAppMenuCoordinator.showAppMenuForKeyboardEvent());
        waitForMenuToShow(currentCallCount);
    }

    private void waitForMenuToShow(int currentCallCount) throws TimeoutException {
        mMenuObserver.menuShownCallback.waitForCallback(currentCallCount);
        Assert.assertTrue("Menu should be showing", mAppMenuHandler.isAppMenuShowing());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mAppMenuHandler.getAppMenu().finishAnimationsForTests());
    }

    private class TestActivityLifecycleDispatcher implements ActivityLifecycleDispatcher {
        public CallbackHelper observerRegisteredCallbackHelper = new CallbackHelper();
        @Override
        public void register(LifecycleObserver observer) {
            observerRegisteredCallbackHelper.notifyCalled();
        }

        @Override
        public void unregister(LifecycleObserver observer) {}

        @Override
        public int getCurrentActivityState() {
            return 0;
        }

        @Override
        public boolean isNativeInitializationFinished() {
            return false;
        }

        @Override
        public boolean isActivityFinishingOrDestroyed() {
            return false;
        }
    }

    private class TestMenuButtonDelegate implements MenuButtonDelegate {
        @Nullable
        @Override
        public View getMenuButtonView() {
            return getActivity().findViewById(R.id.top_button);
        }
    }

    private View getViewAtPosition(int index) {
        // Wait for the view to be available. This is necessary when the menu is first shown.
        CriteriaHelper.pollUiThread(
                ()
                        -> AppMenuTestSupport.getListView(mAppMenuCoordinator).getChildAt(index)
                        != null);
        return AppMenuTestSupport.getListView(mAppMenuCoordinator).getChildAt(index);
    }

    private Rect getPopupLocationRect() {
        View contentView = mAppMenuHandler.getAppMenu().getPopup().getContentView();

        Rect popupRect = new Rect();
        int[] popupLocation = new int[2];
        contentView.getLocationOnScreen(popupLocation);
        popupRect.left = popupLocation[0];
        popupRect.top = popupLocation[1];
        popupRect.right = popupLocation[0] + contentView.getWidth();
        popupRect.bottom = popupLocation[1] + contentView.getHeight();
        return popupRect;
    }

    private Rect getViewLocationRect(View anchor) {
        Rect viewRect = new Rect();
        int[] viewLocation = new int[2];
        anchor.getLocationOnScreen(viewLocation);
        viewRect.left = viewLocation[0];
        viewRect.top = viewLocation[1];
        viewRect.right = viewRect.left + anchor.getWidth();
        viewRect.bottom = viewRect.top + anchor.getHeight();
        return viewRect;
    }

    private Rect getVisibleScreenRectAtPosition(int position) throws ExecutionException {
        View view = getViewAtPosition(position);
        return TestThreadUtils.runOnUiThreadBlocking(
                () -> mAppMenuHandler.getAppMenuDragHelper().getScreenVisibleRect(view));
    }

    private void sendMotionEventToButtonHelper(AppMenuButtonHelperImpl helper, View view,
            MotionEvent event) throws ExecutionException {
        TestThreadUtils.runOnUiThreadBlocking(() -> helper.onTouch(view, event));
    }

    private void checkHighlightOn(View view) {
        Assert.assertTrue(ViewHighlighterTestUtils.checkHighlightOn(view));
    }
}
