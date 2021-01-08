// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.chrome.test.util.ViewUtils.createMotionEvent;

import android.content.pm.ActivityInfo;
import android.graphics.Point;
import android.os.Build.VERSION_CODES;
import android.os.Debug;
import android.os.SystemClock;
import android.support.test.InstrumentationRegistry;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.View;

import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChromePhone;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.layouts.SceneChangeObserver;
import org.chromium.chrome.browser.compositor.layouts.StaticLayout;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.phone.StackLayout;
import org.chromium.chrome.browser.compositor.layouts.phone.stack.Stack;
import org.chromium.chrome.browser.compositor.layouts.phone.stack.StackTab;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabStateFileManager;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabpersistence.TabStateDirectory;
import org.chromium.chrome.browser.toolbar.top.ToggleTabStackButton;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.OverviewModeBehaviorWatcher;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.ScrollDirection;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.SwipeHandler;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.javascript_dialogs.JavascriptTabModalDialog;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.test.util.UiUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.UiRestriction;
import org.chromium.url.GURL;

import java.io.File;
import java.util.Locale;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/**
 * General Tab tests.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TabsTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String TEST_FILE_PATH =
            "/chrome/test/data/android/tabstest/tabs_test.html";
    private static final String TEST_PAGE_FILE_PATH = "/chrome/test/data/google/google.html";

    private EmbeddedTestServer mTestServer;

    private float mPxToDp = 1.0f;
    private float mTabsViewHeightDp;
    private float mTabsViewWidthDp;

    private boolean mNotifyChangedCalled;

    private static final int SWIPE_TO_RIGHT_DIRECTION = 1;
    private static final int SWIPE_TO_LEFT_DIRECTION = -1;

    private static final long WAIT_RESIZE_TIMEOUT_MS = 3000;

    private static final int STRESSFUL_TAB_COUNT = 100;

    private static final String INITIAL_SIZE_TEST_URL = UrlUtils.encodeHtmlDataUri(
            "<html><head><meta name=\"viewport\" content=\"width=device-width\">"
            + "<script>"
            + "  document.writeln(window.innerWidth + ',' + window.innerHeight);"
            + "</script></head>"
            + "<body>"
            + "</body></html>");

    private static final String RESIZE_TEST_URL = UrlUtils.encodeHtmlDataUri(
            "<html><head><script>"
            + "  var resizeHappened = false;"
            + "  function onResize() {"
            + "    resizeHappened = true;"
            + "    document.getElementById('test').textContent ="
            + "       window.innerWidth + 'x' + window.innerHeight;"
            + "  }"
            + "</script></head>"
            + "<body onresize=\"onResize()\">"
            + "  <div id=\"test\">No resize event has been received yet.</div>"
            + "</body></html>");

    @Before
    public void setUp() throws InterruptedException {
        float dpToPx = InstrumentationRegistry.getInstrumentation()
                               .getContext()
                               .getResources()
                               .getDisplayMetrics()
                               .density;
        mPxToDp = 1.0f / dpToPx;

        // Exclude the tests that can launch directly to a page other than the NTP.
        if (mActivityTestRule.getName().equals("testOpenAndCloseNewTabButton")
                || mActivityTestRule.getName().equals("testSwitchToTabThatDoesNotHaveThumbnail")
                || mActivityTestRule.getName().equals("testCloseTabPortrait")
                || mActivityTestRule.getName().equals("testCloseTabLandscape")
                || mActivityTestRule.getName().equals("testTabsAreDestroyedOnModelDestruction")
                || mActivityTestRule.getName().equals("testIncognitoTabsNotRestoredAfterSwipe")) {
            return;
        }
        mActivityTestRule.startMainActivityOnBlankPage();
        CompositorAnimationHandler.setTestingMode(true);
    }

    @After
    public void tearDown() {
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
        if (mTestServer != null) {
            mTestServer.stopAndDestroyServer();
        }
    }

    /**
     * Verify that spawning a popup from a background tab in a different model works properly.
     */
    @Test
    @LargeTest
    @Feature({"Navigation"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @CommandLineFlags.Add(ContentSwitches.DISABLE_POPUP_BLOCKING)
    public void testSpawnPopupOnBackgroundTab() {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_FILE_PATH));
        final Tab tab = mActivityTestRule.getActivity().getActivityTab();

        mActivityTestRule.newIncognitoTabFromMenu();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> tab.getWebContents().evaluateJavaScriptForTests("(function() {"
                                        + "  window.open('www.google.com');"
                                        + "})()",
                                null));

        CriteriaHelper.pollUiThread(() -> {
            int tabCount = mActivityTestRule.getActivity()
                                   .getTabModelSelector()
                                   .getModel(false)
                                   .getCount();
            Criteria.checkThat(tabCount, Matchers.is(2));
        });
    }

    @Test
    @MediumTest
    public void testAlertDialogDoesNotChangeActiveModel() {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mActivityTestRule.newIncognitoTabFromMenu();
        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_FILE_PATH));
        final Tab tab = mActivityTestRule.getActivity().getActivityTab();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> tab.getWebContents().evaluateJavaScriptForTests("(function() {"
                                        + "  alert('hi');"
                                        + "})()",
                                null));

        final AtomicReference<JavascriptTabModalDialog> dialog = new AtomicReference<>();

        CriteriaHelper.pollInstrumentationThread(() -> {
            dialog.set(getCurrentAlertDialog());
            Criteria.checkThat(dialog.get(), Matchers.notNullValue());
        });

        onView(withId(R.id.positive_button)).perform(click());

        dialog.set(null);

        CriteriaHelper.pollInstrumentationThread(
                () -> Criteria.checkThat(getCurrentAlertDialog(), Matchers.nullValue()));

        Assert.assertTrue("Incognito model was not selected",
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
    }

    /**
     * Verify New Tab Open and Close Event not from the context menu.
     * @LargeTest
     * @Feature({"Android-TabSwitcher"})
     * @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
     */
    @Test
    @DisabledTest
    public void testOpenAndCloseNewTabButton() {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mActivityTestRule.startMainActivityWithURL(mTestServer.getURL(TEST_FILE_PATH));
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            String title =
                    mActivityTestRule.getActivity().getCurrentTabModel().getTabAt(0).getTitle();
            Assert.assertEquals("Data file for TabsTest", title);
        });
        final int tabCount = mActivityTestRule.getActivity().getCurrentTabModel().getCount();
        OverviewModeBehaviorWatcher overviewModeWatcher = new OverviewModeBehaviorWatcher(
                mActivityTestRule.getActivity().getLayoutManager(), true, false);
        View tabSwitcherButton =
                mActivityTestRule.getActivity().findViewById(R.id.tab_switcher_button);
        Assert.assertNotNull("'tab_switcher_button' view is not found", tabSwitcherButton);
        TouchCommon.singleClickView(tabSwitcherButton);
        overviewModeWatcher.waitForBehavior();
        overviewModeWatcher = new OverviewModeBehaviorWatcher(
                mActivityTestRule.getActivity().getLayoutManager(), false, true);
        View newTabButton = mActivityTestRule.getActivity().findViewById(R.id.new_tab_button);
        Assert.assertNotNull("'new_tab_button' view is not found", newTabButton);
        TouchCommon.singleClickView(newTabButton);
        overviewModeWatcher.waitForBehavior();

        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> Assert.assertEquals("The tab count is wrong", tabCount + 1,
                                mActivityTestRule.getActivity().getCurrentTabModel().getCount()));

        CriteriaHelper.pollUiThread(() -> {
            Tab tab = mActivityTestRule.getActivity().getCurrentTabModel().getTabAt(1);
            String title = tab.getTitle().toLowerCase(Locale.US);
            String expectedTitle = "new tab";
            Criteria.checkThat(title, Matchers.startsWith(expectedTitle));
        });

        ChromeTabUtils.closeCurrentTab(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> Assert.assertEquals(tabCount,
                                mActivityTestRule.getActivity().getCurrentTabModel().getCount()));
    }

    private void assertWaitForKeyboardStatus(final boolean show) {
        CriteriaHelper.pollUiThread(() -> {
            boolean isKeyboardShowing = mActivityTestRule.getKeyboardDelegate().isKeyboardShowing(
                    mActivityTestRule.getActivity(), mActivityTestRule.getActivity().getTabsView());
            Criteria.checkThat(isKeyboardShowing, Matchers.is(show));
        });
    }

    /**
     * Verify that opening a new tab, switching to an existing tab and closing current tab hide
     * keyboard.
     */
    @Test
    @LargeTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"Android-TabSwitcher"})
    public void testHideKeyboard() throws Exception {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());

        // Open a new tab(The 1st tab) and click node.
        ChromeTabUtils.fullyLoadUrlInNewTab(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(), mTestServer.getURL(TEST_FILE_PATH), false);
        Assert.assertEquals("Failed to click node.", true,
                DOMUtils.clickNode(mActivityTestRule.getWebContents(), "input_text"));
        assertWaitForKeyboardStatus(true);

        // Open a new tab(the 2nd tab).
        ChromeTabUtils.fullyLoadUrlInNewTab(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(), mTestServer.getURL(TEST_FILE_PATH), false);
        assertWaitForKeyboardStatus(false);

        // Click node in the 2nd tab.
        DOMUtils.clickNode(mActivityTestRule.getWebContents(), "input_text");
        assertWaitForKeyboardStatus(true);

        // Switch to the 1st tab.
        ChromeTabUtils.switchTabInCurrentTabModel(mActivityTestRule.getActivity(), 1);
        assertWaitForKeyboardStatus(false);

        // Click node in the 1st tab.
        DOMUtils.clickNode(mActivityTestRule.getWebContents(), "input_text");
        assertWaitForKeyboardStatus(true);

        // Close current tab(the 1st tab).
        ChromeTabUtils.closeCurrentTab(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        assertWaitForKeyboardStatus(false);
    }

    /**
     * Verify that opening a new window hides keyboard.
     */
    @DisabledTest(message = "crbug.com/766735")
    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    public void testHideKeyboardWhenOpeningWindow() throws Exception {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        // Open a new tab and click an editable node.
        ChromeTabUtils.fullyLoadUrlInNewTab(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(), mTestServer.getURL(TEST_FILE_PATH), false);
        Assert.assertEquals("Failed to click textarea.", true,
                DOMUtils.clickNode(mActivityTestRule.getWebContents(), "textarea"));
        assertWaitForKeyboardStatus(true);

        // Click the button to open a new window.
        Assert.assertEquals("Failed to click button.", true,
                DOMUtils.clickNode(mActivityTestRule.getWebContents(), "button"));
        assertWaitForKeyboardStatus(false);
    }

    private void assertWaitForSelectedText(final String text) {
        CriteriaHelper.pollUiThread(() -> {
            WebContents webContents = mActivityTestRule.getWebContents();
            SelectionPopupController controller =
                    SelectionPopupController.fromWebContents(webContents);
            final String actualText = controller.getSelectedText();
            Criteria.checkThat(actualText, Matchers.is(text));
        });
    }

    /**
     * Generate a fling sequence from the given start/end X,Y percentages, for the given steps.
     * Works in either landscape or portrait orientation.
     */
    private void fling(float startX, float startY, float endX, float endY, int stepCount) {
        Point size = new Point();
        mActivityTestRule.getActivity().getWindowManager().getDefaultDisplay().getSize(size);
        float dragStartX = size.x * startX;
        float dragEndX = size.x * endX;
        float dragStartY = size.y * startY;
        float dragEndY = size.y * endY;
        TouchCommon.performDrag(mActivityTestRule.getActivity(), dragStartX, dragEndX, dragStartY,
                dragEndY, stepCount, 250);
    }

    private void scrollDown() {
        fling(0.f, 0.9f, 0.f, 0.1f, 100);
    }

    /**
     * Verify that the selection is collapsed when switching to the tab-switcher mode then switching
     * back. https://crbug.com/697756
     */
    @Test
    @MediumTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @Feature({"Android-TabSwitcher"})
    public void testTabSwitcherCollapseSelection() throws Exception {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mActivityTestRule.loadUrlInNewTab(mTestServer.getURL(TEST_FILE_PATH), false);
        DOMUtils.longPressNode(mActivityTestRule.getWebContents(), "textarea");
        assertWaitForSelectedText("helloworld");

        // Switch to tab-switcher mode, switch back, and scroll page.
        showOverviewWithNoAnimation();
        hideOverviewWithNoAnimation();
        scrollDown();
        assertWaitForSelectedText("");
    }

    /**
     * Verify that opening a new tab and navigating immediately sets a size on the newly created
     * renderer. https://crbug.com/434477.
     * @throws TimeoutException
     */
    @Test
    @SmallTest
    public void testNewTabSetsContentViewSize() throws TimeoutException {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // Make sure we're on the NTP
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(tab);

        mActivityTestRule.loadUrl(INITIAL_SIZE_TEST_URL);

        final WebContents webContents = tab.getWebContents();
        String innerText = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                webContents, "document.body.innerText").replace("\"", "");

        DisplayMetrics metrics = mActivityTestRule.getActivity().getResources().getDisplayMetrics();

        // For non-integer pixel ratios like the N7v1 (1.333...), the layout system will actually
        // ceil the width.
        int expectedWidth = (int) Math.ceil(metrics.widthPixels / metrics.density);

        String[] nums = innerText.split(",");
        Assert.assertTrue(nums.length == 2);
        int innerWidth = Integer.parseInt(nums[0]);
        int innerHeight = Integer.parseInt(nums[1]);

        Assert.assertEquals(expectedWidth, innerWidth);

        // Height can be affected by browser controls so just make sure it's non-0.
        Assert.assertTrue("innerHeight was not set by page load time", innerHeight > 0);
    }

    /**
     * A Runnable to simulate a click on given coordinates.
     */
    public static class SimulateClickOnMainThread implements Runnable {
        private final LayoutManagerChrome mLayoutManager;
        private final float mX;
        private final float mY;

        public SimulateClickOnMainThread(LayoutManagerChrome layoutManager, float x, float y) {
            mLayoutManager = layoutManager;
            mX = x;
            mY = y;
        }

        @Override
        public void run() {
            mLayoutManager.simulateClick(mX, mY);
        }
    }

    /**
     * A Runnable to simulate a swipe with specific coordinates and distance.
     */
    public static class SimulateTabSwipeOnMainThread implements Runnable {
        private final LayoutManagerChrome mLayoutManager;
        private final float mX;
        private final float mY;
        private final float mDeltaX;
        private final float mDeltaY;

        public SimulateTabSwipeOnMainThread(LayoutManagerChrome layoutManager, float x, float y,
                float dX, float dY) {
            mLayoutManager = layoutManager;
            mX = x;
            mY = y;
            mDeltaX = dX;
            mDeltaY = dY;
        }

        @Override
        public void run() {
            mLayoutManager.simulateDrag(mX, mY, mDeltaX, mDeltaY);
        }
    }

    /**
     * Verify that the provided click position closes a tab.
     * TODO(yuezhanggg@): The hard-coded coordinates are not a good way to verify the position of
     * the closing button. Should be replaced by render tests.
     */
    private void checkCloseTabAtPosition(final float x, final float y) {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        int initialTabCount = cta.getCurrentTabModel().getCount();
        ChromeTabUtils.fullyLoadUrlInNewTab(InstrumentationRegistry.getInstrumentation(), cta,
                UrlConstants.CHROME_BLANK_URL, false);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { cta.getLayoutManager().showOverview(false); });

        Assert.assertTrue("Expected: " + (initialTabCount + 1)
                        + " tab Got: " + cta.getCurrentTabModel().getCount(),
                (initialTabCount + 1) == cta.getCurrentTabModel().getCount());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        final LayoutManagerChrome layoutManager = updateTabsViewSize();
        StackLayout layout = (StackLayout) layoutManager.getOverviewLayout();
        Stack stack = layout.getTabStackAtIndex(0);
        Assert.assertTrue("Position is not in the active area of the close button",
                stack.checkCloseHitTestOnLayoutTab(x, y, stack.getTabs()[0].getLayoutTab()));
        ChromeTabUtils.closeTabWithAction(InstrumentationRegistry.getInstrumentation(), cta,
                ()
                        -> InstrumentationRegistry.getInstrumentation().runOnMainSync(
                                new SimulateClickOnMainThread(layoutManager, x, y)));
        Assert.assertTrue(
                "Expected: " + initialTabCount + " tab Got: " + cta.getCurrentTabModel().getCount(),
                initialTabCount == cta.getCurrentTabModel().getCount());
    }

    /**
     * Verify close button works in the TabSwitcher in portrait mode.
     * This code does not handle properly different screen densities.
     */
    @Test
    @LargeTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"Android-TabSwitcher"})
    @DisableIf.Build(sdk_is_greater_than = VERSION_CODES.O_MR1,
            message = "Very flaky on android-pie-x86-rel https://crbug.com/1135837")
    public void
    testTabSwitcherPortraitCloseButton() {
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        int portraitWidth = Math.min(
                mActivityTestRule.getActivity().getResources().getDisplayMetrics().widthPixels,
                mActivityTestRule.getActivity().getResources().getDisplayMetrics().heightPixels);
        // Hard-coded coordinates of the close button on the top right of the screen.
        // If the coordinates need to be updated, the easiest is to take a screenshot and measure.
        // Note that starting from the right of the screen should cover any screen size.
        checkCloseTabAtPosition(portraitWidth * mPxToDp - 32, 70);
    }

    /**
     * Verify close button works in the TabSwitcher in landscape mode.
     * This code does not handle properly different screen densities.
     */
    @Test
    @LargeTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @DisableIf.Build(message = "Flaky on Android P, see https://crbug.com/1164443",
            sdk_is_greater_than = VERSION_CODES.O_MR1, sdk_is_less_than = VERSION_CODES.Q)
    @Feature({"Android-TabSwitcher"})
    public void
    testTabSwitcherLandscapeCloseButton() {
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        // Hard-coded coordinates of the close button on the bottom left of the screen.
        // If the coordinates need to be updated, the easiest is to take a screenshot and measure.
        checkCloseTabAtPosition(74 * mPxToDp, 216 * mPxToDp);
    }

    /**
     * Verify that we can open a large number of tabs without running out of
     * memory. This test waits for the NTP to load before opening the next one.
     * This is a LargeTest but because we're doing it "slowly", we need to further scale
     * the timeout for adb am instrument and the various events.
     */
    /*
     * @EnormousTest
     * @TimeoutScale(10)
     * @Feature({"Android-TabSwitcher"})
     * Bug crbug.com/166208
     */
    @Test
    @DisabledTest
    public void testOpenManyTabsSlowly() {
        int startCount = mActivityTestRule.getActivity().getCurrentTabModel().getCount();
        for (int i = 1; i <= STRESSFUL_TAB_COUNT; ++i) {
            ChromeTabUtils.newTabFromMenu(
                    InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
            InstrumentationRegistry.getInstrumentation().waitForIdleSync();
            Assert.assertEquals(startCount + i,
                    mActivityTestRule.getActivity().getCurrentTabModel().getCount());
        }
    }

    /**
     * Verify that we can open a large number of tabs without running out of
     * memory. This test hammers the "new tab" button quickly to stress the app.
     *
     * @LargeTest
     * @TimeoutScale(10)
     * @Feature({"Android-TabSwitcher"})
     *
     */
    @Test
    @FlakyTest
    public void testOpenManyTabsQuickly() {
        int startCount = mActivityTestRule.getActivity().getCurrentTabModel().getCount();
        for (int i = 1; i <= STRESSFUL_TAB_COUNT; ++i) {
            MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                    mActivityTestRule.getActivity(), R.id.new_tab_menu_id);
            Assert.assertEquals(startCount + i,
                    mActivityTestRule.getActivity().getCurrentTabModel().getCount());
        }
    }

    /**
     * Verify that we can open a burst of new tabs, even when there are already
     * a large number of tabs open.
     * Bug: crbug.com/180718
     * @EnormousTest
     * @TimeoutScale(30)
     * @Feature({"Navigation"})
     */
    @Test
    @FlakyTest
    public void testOpenManyTabsInBursts() throws TimeoutException {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        final int burstSize = 5;
        final String url = mTestServer.getURL(TEST_PAGE_FILE_PATH);
        final int startCount = mActivityTestRule.getActivity().getCurrentTabModel().getCount();
        for (int tabCount = startCount; tabCount < STRESSFUL_TAB_COUNT; tabCount += burstSize)  {
            loadUrlInManyNewTabs(url, burstSize);
            Assert.assertEquals(tabCount + burstSize,
                    mActivityTestRule.getActivity().getCurrentTabModel().getCount());
        }
    }

    /**
     * Verify opening 10 tabs at once and that each tab loads when selected.
     */
    /*
     * @EnormousTest
     * @TimeoutScale(30)
     * @Feature({"Navigation"})
     */
    @Test
    @FlakyTest(message = "crbug.com/223110")
    public void testOpenManyTabsAtOnce10() throws TimeoutException {
        openAndVerifyManyTestTabs(10);
    }

    /**
     * Verify that we can open a large number of tabs all at once and that each
     * tab loads when selected.
     */
    private void openAndVerifyManyTestTabs(final int num) throws TimeoutException {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        final String url = mTestServer.getURL(TEST_PAGE_FILE_PATH);
        int startCount = mActivityTestRule.getActivity().getCurrentTabModel().getCount();
        loadUrlInManyNewTabs(url, num);
        Assert.assertEquals(
                startCount + num, mActivityTestRule.getActivity().getCurrentTabModel().getCount());
    }

    class ClickOptionButtonOnMainThread implements Runnable {
        @Override
        public void run() {
            // This is equivalent to clickById(R.id.tab_switcher_button) but does not rely on the
            // event pipeline.
            ToggleTabStackButton button =
                    mActivityTestRule.getActivity().findViewById(R.id.tab_switcher_button);
            Assert.assertNotNull("Could not find view R.id.tab_switcher_button", button);
            button.onClick(button);
        }
    }

    /** Enters the tab switcher without animation.*/
    private void showOverviewWithNoAnimation() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getLayoutManager().showOverview(false));
    }

    /** Exits the tab switcher without animation. */
    private void hideOverviewWithNoAnimation() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getLayoutManager().hideOverview(false));
    }

    /**
     * Opens tabs to populate the model to a given count.
     * @param targetTabCount The desired number of tabs in the model.
     * @param waitToLoad     Whether the tabs need to be fully loaded.
     * @return               The new number of tabs in the model.
     */
    private int openTabs(final int targetTabCount, boolean waitToLoad) {
        final ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        Callable<Integer> countOnUi = () -> {
            return activity.getCurrentTabModel().getCount();
        };
        int tabCount = TestThreadUtils.runOnUiThreadBlockingNoException(countOnUi);
        while (tabCount < targetTabCount) {
            ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(), activity);
            tabCount++;
            Assert.assertEquals("The tab count is wrong", tabCount,
                    (int) TestThreadUtils.runOnUiThreadBlockingNoException(countOnUi));
            if (waitToLoad) {
                CriteriaHelper.pollUiThread(() -> {
                    return !TabModelUtils.getCurrentTab(activity.getCurrentTabModel()).isLoading();
                });
            }
        }
        return tabCount;
    }

    /**
     * Verifies that when more than 9 tabs are open only at most 8 are drawn. Basically it verifies
     * that the tab culling mechanism works properly.
     */
    /*
       @LargeTest
       @Feature({"Android-TabSwitcher"})
    */
    @Test
    @DisabledTest(message = "crbug.com/156746")
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testTabsCulling() {
        // Open one more tabs than maxTabsDrawn.
        final int maxTabsDrawn = 8;
        int tabCount = openTabs(maxTabsDrawn + 1, false);
        showOverviewWithNoAnimation();

        // Check counts.
        LayoutManagerChromePhone layoutManager =
                (LayoutManagerChromePhone) mActivityTestRule.getActivity().getLayoutManager();
        int drawnCount = layoutManager.getOverviewLayout().getLayoutTabsToRender().length;
        int drawnExpected = Math.min(tabCount, maxTabsDrawn);
        Assert.assertEquals("The number of drawn tab is wrong", drawnExpected, drawnCount);
    }

    /**
     * Checks the stacked tabs in the stack are visible.
     */
    private void checkTabsStacking() {
        final int count = mActivityTestRule.getActivity().getCurrentTabModel().getCount();
        Assert.assertEquals(
                "The number of tab in the stack should match the number of tabs in the model",
                count, getLayoutTabInStackCount(false));

        Assert.assertTrue("The selected tab should always be visible",
                stackTabIsVisible(
                        false, mActivityTestRule.getActivity().getCurrentTabModel().index()));
        for (int i = 0; i < Stack.MAX_NUMBER_OF_STACKED_TABS_TOP && i < count; i++) {
            Assert.assertTrue("The stacked tab " + i + " from the top should always be visible",
                    stackTabIsVisible(false, i));
        }
        for (int i = 0; i < Stack.MAX_NUMBER_OF_STACKED_TABS_BOTTOM && i < count; i++) {
            Assert.assertTrue("The stacked tab " + i + " from the bottom should always be visible",
                    stackTabIsVisible(false, count - 1 - i));
        }
    }

    /**
     * Verifies that the tab are actually stacking at the bottom and top of the screen.
     */
    /**
     * @LargeTest
     * @Feature({"Android-TabSwitcher"})
     */
    @Test
    @FlakyTest(message = "crbug.com/170179")
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testTabsStacking() {
        final int count = openTabs(12, false);

        // Selecting the first tab to scroll all the way to the top.
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> TabModelUtils.setIndex(
                                mActivityTestRule.getActivity().getCurrentTabModel(), 0));
        showOverviewWithNoAnimation();
        checkTabsStacking();

        // Selecting the last tab to scroll all the way to the bottom.
        hideOverviewWithNoAnimation();
        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> TabModelUtils.setIndex(
                                mActivityTestRule.getActivity().getCurrentTabModel(), count - 1));
        showOverviewWithNoAnimation();
        checkTabsStacking();
    }

    /**
     * @return A stable read of allocated size (native + dalvik) after gc.
     */
    private long getStableAllocatedSize() {
        // Measure the equivalent of allocated size native + dalvik in:
        // adb shell dumpsys meminfo | grep chrome -A 20
        int maxTries = 8;
        int tries = 0;
        long threshold = 512; // bytes
        long lastAllocatedSize = Long.MAX_VALUE;
        long currentAllocatedSize = 0;
        while (tries < maxTries && Math.abs(currentAllocatedSize - lastAllocatedSize) > threshold) {
            System.gc();
            try {
                Thread.sleep(1000 + tries * 500); // Memory measurement is not an exact science...
                lastAllocatedSize = currentAllocatedSize;
                currentAllocatedSize = Debug.getNativeHeapAllocatedSize()
                        + Runtime.getRuntime().totalMemory();
                //Log.w("MEMORY_MEASURE", "[" + tries + "/" + maxTries + "]" +
                //        "currentAllocatedSize " + currentAllocatedSize);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
            tries++;
        }
        Assert.assertTrue("Could not have a stable read on native allocated size even after "
                        + tries + " gc.",
                tries < maxTries);
        return currentAllocatedSize;
    }

    /**
     * Verify that switching back and forth to the tabswitcher does not leak memory.
     */
    /**
     * @LargeTest
     * @Feature({"Android-TabSwitcher"})
     */
    @Test
    @FlakyTest(message = "crbug.com/303319")
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testTabSwitcherMemoryLeak() {
        openTabs(4, true);

        int maxTries = 10;
        int tries = 0;
        long threshold = 1024; // bytes
        long lastAllocatedSize = 0;
        long currentAllocatedSize = 2 * threshold;
        while (tries < maxTries && (lastAllocatedSize + threshold) < currentAllocatedSize) {
            showOverviewWithNoAnimation();

            lastAllocatedSize = currentAllocatedSize;
            currentAllocatedSize = getStableAllocatedSize();
            //Log.w("MEMORY_TEST", "[" + tries + "/" + maxTries + "]" +
            //        "currentAllocatedSize " + currentAllocatedSize);

            hideOverviewWithNoAnimation();
            tries++;
        }

        Assert.assertTrue(
                "Native heap allocated size keeps increasing even after " + tries + " iterations",
                tries < maxTries);
    }

    /**
     * Verify that switching back and forth stay stable. This test last for at least 8 seconds.
     */
    @Test
    @LargeTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @Feature({"Android-TabSwitcher"})
    public void testTabSwitcherStability() throws InterruptedException {
        openTabs(8, true);

        // This is about as fast as you can ever click.
        final long fastestUserInput = 20; // ms
        for (int i = 0; i < 200; i++) {
            // Show overview
            InstrumentationRegistry.getInstrumentation().runOnMainSync(
                    new ClickOptionButtonOnMainThread());
            Thread.sleep(fastestUserInput);

            // hide overview
            InstrumentationRegistry.getInstrumentation().runOnMainSync(
                    new ClickOptionButtonOnMainThread());
            Thread.sleep(fastestUserInput);
        }
    }

    @Test
    @LargeTest
    @Feature({"Android-TabSwitcher"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testTabSelectionPortrait() {
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        checkTabSelection(2, 0, false);

        // Ensure all tabs following the selected tab are off the screen when the animation is
        // complete.
        final int count = getLayoutTabInStackCount(false);
        for (int i = 1; i < count; i++) {
            float y = getLayoutTabInStackXY(false, i)[1];
            Assert.assertTrue(
                    String.format(Locale.US,
                            "Tab %d's final draw Y, %f, should exceed the view height, %f.", i, y,
                            mTabsViewHeightDp),
                    y >= mTabsViewHeightDp);
        }
    }

    /**
     * @LargeTest
     * @Feature({"Android-TabSwitcher"})
     */
    @Test
    @FlakyTest(message = "crbug.com/170179")
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testTabSelectionLandscape() {
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        checkTabSelection(2, 0, true);

        // Ensure all tabs following the selected tab are off the screen when the animation is
        // complete.
        final int count = getLayoutTabInStackCount(false);
        for (int i = 1; i < count; i++) {
            float x = getLayoutTabInStackXY(false, i)[0];
            Assert.assertTrue(
                    String.format(Locale.US,
                            "Tab %d's final draw X, %f, should exceed the view width, %f.", i, x,
                            mTabsViewWidthDp),
                    x >= mTabsViewWidthDp);
        }
    }

    /**
     * Verify that we don't crash and show the overview mode after closing the last tab.
     */
    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @Feature({"Android-TabSwitcher"})
    public void testCloseLastTabFromMain() {
        OverviewModeBehaviorWatcher overviewModeWatcher = new OverviewModeBehaviorWatcher(
                mActivityTestRule.getActivity().getLayoutManager(), true, false);
        ChromeTabUtils.closeCurrentTab(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        overviewModeWatcher.waitForBehavior();
    }

    private LayoutManagerChrome updateTabsViewSize() {
        View tabsView = mActivityTestRule.getActivity().getTabsView();
        mTabsViewHeightDp = tabsView.getHeight() * mPxToDp;
        mTabsViewWidthDp = tabsView.getWidth() * mPxToDp;
        return mActivityTestRule.getActivity().getLayoutManager();
    }

    private Stack getStack(final LayoutManagerChrome layoutManager, boolean isIncognito) {
        Assert.assertTrue(
                "getStack must be executed on the ui thread", ThreadUtils.runningOnUiThread());
        LayoutManagerChromePhone layoutManagerPhone = (LayoutManagerChromePhone) layoutManager;
        StackLayout layout = (StackLayout) layoutManagerPhone.getOverviewLayout();
        return (layout).getTabStackAtIndex(
                isIncognito ? StackLayout.INCOGNITO_STACK_INDEX : StackLayout.NORMAL_STACK_INDEX);
    }

    private int getLayoutTabInStackCount(final boolean isIncognito) {
        final LayoutManagerChrome layoutManager = updateTabsViewSize();
        final int[] count = new int[1];
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Stack stack = getStack(layoutManager, isIncognito);
            count[0] = stack.getTabs().length;
        });
        return count[0];
    }

    private boolean stackTabIsVisible(final boolean isIncognito, final int index) {
        final LayoutManagerChrome layoutManager = updateTabsViewSize();
        final boolean[] isVisible = new boolean[1];
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Stack stack = getStack(layoutManager, isIncognito);
            isVisible[0] = (stack.getTabs())[index].getLayoutTab().isVisible();
        });
        return isVisible[0];
    }

    private float[] getLayoutTabInStackXY(final boolean isIncognito, final int index) {
        final LayoutManagerChrome layoutManager = updateTabsViewSize();
        final float[] xy = new float[2];
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Stack stack = getStack(layoutManager, isIncognito);
            xy[0] = (stack.getTabs())[index].getLayoutTab().getX();
            xy[1] = (stack.getTabs())[index].getLayoutTab().getY();
        });
        return xy;
    }

    private float[] getStackTabClickTarget(final int tabIndexToSelect, final boolean isIncognito,
            final boolean isLandscape) {
        final LayoutManagerChrome layoutManager = updateTabsViewSize();
        final float[] target = new float[2];
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Stack stack = getStack(layoutManager, isIncognito);
            StackTab[] tabs = stack.getTabs();
            // The position of the click is expressed from the top left corner of the content.
            // The aim is to find an offset that is inside the content but not on the close
            // button.  For this, we calculate the center of the visible tab area.
            LayoutTab layoutTab = tabs[tabIndexToSelect].getLayoutTab();
            LayoutTab nextLayoutTab = (tabIndexToSelect + 1) < tabs.length
                    ? tabs[tabIndexToSelect + 1].getLayoutTab() : null;

            float tabOffsetX = layoutTab.getX();
            float tabOffsetY = layoutTab.getY();
            float tabRightX;
            float tabBottomY;
            if (isLandscape) {
                tabRightX = nextLayoutTab != null
                        ? nextLayoutTab.getX()
                        : tabOffsetX + layoutTab.getScaledContentWidth();
                tabBottomY = tabOffsetY + layoutTab.getScaledContentHeight();
            } else {
                tabRightX = tabOffsetX + layoutTab.getScaledContentWidth();
                tabBottomY = nextLayoutTab != null
                        ? nextLayoutTab.getY()
                        : tabOffsetY + layoutTab.getScaledContentHeight();
            }
            tabRightX = Math.min(tabRightX, mTabsViewWidthDp);
            tabBottomY = Math.min(tabBottomY, mTabsViewHeightDp);

            target[0] = (tabOffsetX + tabRightX) / 2.0f;
            target[1] = (tabOffsetY + tabBottomY) / 2.0f;
        });
        return target;
    }

    private void checkTabSelection(
            int additionalTabsToOpen, int tabIndexToSelect, boolean isLandscape) {
        for (int i = 0; i < additionalTabsToOpen; i++) {
            ChromeTabUtils.newTabFromMenu(
                    InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        }
        Assert.assertEquals("Number of open tabs does not match", additionalTabsToOpen + 1,
                mActivityTestRule.getActivity().getCurrentTabModel().getCount());
        showOverviewWithNoAnimation();

        float[] coordinates = getStackTabClickTarget(tabIndexToSelect, false, isLandscape);
        float clickX = coordinates[0];
        float clickY = coordinates[1];

        OverviewModeBehaviorWatcher overviewModeWatcher = new OverviewModeBehaviorWatcher(
                mActivityTestRule.getActivity().getLayoutManager(), false, true);

        final LayoutManagerChrome layoutManager = updateTabsViewSize();
        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                new SimulateClickOnMainThread(layoutManager, (int) clickX, (int) clickY));
        overviewModeWatcher.waitForBehavior();

        // Make sure we did not accidentally close a tab.
        Assert.assertEquals("Number of open tabs does not match", additionalTabsToOpen + 1,
                mActivityTestRule.getActivity().getCurrentTabModel().getCount());
    }

    public void swipeToCloseTab(final int tabIndexToClose, final boolean isLandscape,
            final boolean isIncognito, final int swipeDirection) {
        final LayoutManagerChrome layoutManager = updateTabsViewSize();
        float[] coordinates = getStackTabClickTarget(tabIndexToClose, isIncognito, isLandscape);
        final float clickX = coordinates[0];
        final float clickY = coordinates[1];
        Log.v("ChromeTest", String.format("clickX %f clickY %f", clickX, clickY));

        ChromeTabUtils.closeTabWithAction(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(), () -> {
                    if (isLandscape) {
                        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                                new SimulateTabSwipeOnMainThread(layoutManager, clickX, clickY, 0,
                                        swipeDirection * mTabsViewWidthDp));
                    } else {
                        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                                new SimulateTabSwipeOnMainThread(layoutManager, clickX, clickY,
                                        swipeDirection * mTabsViewHeightDp, 0));
                    }
                });

        CriteriaHelper.pollUiThread(() -> {
            return !mActivityTestRule.getActivity()
                            .getLayoutManager()
                            .getActiveLayout()
                            .isLayoutAnimating();
        }, "Did not finish animation");
    }

    private void swipeToCloseNTabs(
            int number, boolean isLandscape, boolean isIncognito, int swipeDirection) {
        for (int i = number - 1; i >= 0; i--) {
            swipeToCloseTab(i, isLandscape, isIncognito, swipeDirection);
        }
    }

    /**
     * Test closing few tabs by swiping them in Overview portrait mode.
     */
    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"Android-TabSwitcher", "Main"})
    public void testCloseTabPortrait() {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mActivityTestRule.startMainActivityWithURL(
                mTestServer.getURL("/chrome/test/data/android/test.html"));

        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);

        int tabCount = mActivityTestRule.getActivity().getCurrentTabModel().getCount();
        ChromeTabUtils.newTabsFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity(), 3);
        Assert.assertEquals("wrong count after new tabs", tabCount + 3,
                mActivityTestRule.getActivity().getCurrentTabModel().getCount());

        showOverviewWithNoAnimation();
        swipeToCloseNTabs(3, false, false, SWIPE_TO_LEFT_DIRECTION);

        Assert.assertEquals("Wrong tab counts after closing a few of them", tabCount,
                mActivityTestRule.getActivity().getCurrentTabModel().getCount());
    }

    /**
     * Test closing few tabs by swiping them in Overview landscape mode.
     */
    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher", "Main"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testCloseTabLandscape() {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mActivityTestRule.startMainActivityWithURL(
                mTestServer.getURL("/chrome/test/data/android/test.html"));

        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);

        int tabCount = mActivityTestRule.getActivity().getCurrentTabModel().getCount();
        ChromeTabUtils.newTabsFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity(), 3);
        Assert.assertEquals("wrong count after new tabs", tabCount + 3,
                mActivityTestRule.getActivity().getCurrentTabModel().getCount());

        showOverviewWithNoAnimation();
        swipeToCloseTab(0, true, false, SWIPE_TO_LEFT_DIRECTION);
        swipeToCloseTab(0, true, false, SWIPE_TO_LEFT_DIRECTION);
        swipeToCloseTab(0, true, false, SWIPE_TO_LEFT_DIRECTION);

        Assert.assertEquals("Wrong tab counts after closing a few of them", tabCount,
                mActivityTestRule.getActivity().getCurrentTabModel().getCount());
    }

    /**
     * Test close Incognito tab by swiping in Overview Portrait mode.
     */
    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testCloseIncognitoTabPortrait() throws InterruptedException {
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        mActivityTestRule.newIncognitoTabsFromMenu(2);

        showOverviewWithNoAnimation();
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        swipeToCloseNTabs(2, false, true, SWIPE_TO_LEFT_DIRECTION);
    }

    /**
     * Test close 5 Incognito tabs by swiping in Overview Portrait mode.
     */
    @Test
    @Feature({"Android-TabSwitcher"})
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testCloseFiveIncognitoTabPortrait() throws InterruptedException {
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        mActivityTestRule.newIncognitoTabsFromMenu(5);

        showOverviewWithNoAnimation();
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        swipeToCloseNTabs(5, false, true, SWIPE_TO_LEFT_DIRECTION);
    }

    /**
     * Simple swipe gesture should not close tabs when two Tabstacks are open in Overview mode.
     * Test in Portrait Mode.
     */
    @Test
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    public void testSwitchTabStackWithoutClosingTabsInPortrait() {
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        LayoutManagerChrome layoutManager = updateTabsViewSize();
        SimulateTabSwipeOnMainThread swipeToIncognito =
                new SimulateTabSwipeOnMainThread(layoutManager, mTabsViewWidthDp - 20,
                        mTabsViewHeightDp / 2, SWIPE_TO_LEFT_DIRECTION * mTabsViewWidthDp, 0);
        SimulateTabSwipeOnMainThread swipeToNormal = new SimulateTabSwipeOnMainThread(layoutManager,
                20, mTabsViewHeightDp / 2, SWIPE_TO_RIGHT_DIRECTION * mTabsViewWidthDp, 0);
        testSwitchTabStackWithoutClosingTabs(swipeToIncognito, swipeToNormal);
    }

    /**
     * Simple swipe gesture should not close tabs when two Tabstacks are open in Overview mode.
     * Test in Landscape Mode.
     */
    @Test
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    public void testSwitchTabStackWithoutClosingTabsInLandscape() {
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        LayoutManagerChrome layoutManager = updateTabsViewSize();
        SimulateTabSwipeOnMainThread swipeToIncognito =
                new SimulateTabSwipeOnMainThread(layoutManager, mTabsViewWidthDp / 2,
                        mTabsViewHeightDp - 20, 0, SWIPE_TO_LEFT_DIRECTION * mTabsViewWidthDp);
        SimulateTabSwipeOnMainThread swipeToNormal = new SimulateTabSwipeOnMainThread(layoutManager,
                mTabsViewWidthDp / 2, 20, 0, SWIPE_TO_RIGHT_DIRECTION * mTabsViewWidthDp);
        testSwitchTabStackWithoutClosingTabs(swipeToIncognito, swipeToNormal);
    }

    private void testSwitchTabStackWithoutClosingTabs(SimulateTabSwipeOnMainThread swipeToIncognito,
            SimulateTabSwipeOnMainThread swipeToNormal) {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        final TabModelSelector tabModelSelector = cta.getTabModelSelector();
        final StackLayout layout = (StackLayout) cta.getLayoutManager().getOverviewLayout();

        mActivityTestRule.newIncognitoTabFromMenu();
        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(), cta);
        showOverviewWithNoAnimation();
        final int normalTabCount = getLayoutTabInStackCount(false);
        final int incognitoTabCount = getLayoutTabInStackCount(true);
        Assert.assertEquals(2, normalTabCount);
        Assert.assertEquals(1, incognitoTabCount);

        // Swipe to switch between normal and incognito tab model for 20 times.
        boolean shouldSwipeToIncognito = true;
        for (int i = 0; i < 20; i++) {
            SimulateTabSwipeOnMainThread swipe =
                    shouldSwipeToIncognito ? swipeToIncognito : swipeToNormal;
            int tabCount = shouldSwipeToIncognito ? incognitoTabCount : normalTabCount;
            Assert.assertNotEquals("Tab model has not been changed before swipe",
                    shouldSwipeToIncognito, tabModelSelector.isIncognitoSelected());
            Assert.assertNotEquals("Tab count has not been changed before swipe", tabCount,
                    tabModelSelector.getCurrentModel().getCount());
            // Swipe to switch tab model.
            TestThreadUtils.runOnUiThreadBlocking(swipe);
            CriteriaHelper.pollUiThread(() -> !layout.isLayoutAnimating());
            Assert.assertEquals("Tab model should be changed by swipe.", shouldSwipeToIncognito,
                    tabModelSelector.isIncognitoSelected());
            Assert.assertEquals("Tab count should be changed by swipe.", tabCount,
                    tabModelSelector.getCurrentModel().getCount());
            // Flip the swipe direction.
            shouldSwipeToIncognito = !shouldSwipeToIncognito;
        }
    }

    /**
     * Test close Incognito tab by swiping in Overview Landscape mode.
     */
    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testCloseIncognitoTabLandscape() throws InterruptedException {
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        mActivityTestRule.newIncognitoTabFromMenu();

        showOverviewWithNoAnimation();
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        swipeToCloseTab(0, true, true, SWIPE_TO_LEFT_DIRECTION);
    }

    /**
     * Test close 5 Incognito tabs by swiping in Overview Landscape mode.
     */
    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testCloseFiveIncognitoTabLandscape() throws InterruptedException {
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        mActivityTestRule.newIncognitoTabsFromMenu(5);

        showOverviewWithNoAnimation();
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        swipeToCloseNTabs(5, true, true, SWIPE_TO_LEFT_DIRECTION);
    }

    /**
     * Test that we can safely close a tab during a fling (http://b/issue?id=5364043)
     */
    @Test
    @SmallTest
    @Feature({"Android-TabSwitcher"})
    public void testCloseTabDuringFling() {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mActivityTestRule.loadUrlInNewTab(
                mTestServer.getURL("/chrome/test/data/android/tabstest/text_page.html"));
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            WebContents webContents = mActivityTestRule.getWebContents();
            webContents.getEventForwarder().startFling(
                    SystemClock.uptimeMillis(), 0, -2000, false, true);
        });
        ChromeTabUtils.closeCurrentTab(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
    }

    /**
     * Flaky on instrumentation-yakju-clankium-ics. See https://crbug.com/431296.
     * @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
     * @MediumTest
     * @Feature({"Android-TabSwitcher"})
     */
    @Test
    @FlakyTest
    public void testQuickSwitchBetweenTabAndSwitcherMode() {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        final String[] urls = {
                mTestServer.getURL("/chrome/test/data/android/navigate/one.html"),
                mTestServer.getURL("/chrome/test/data/android/navigate/two.html"),
                mTestServer.getURL("/chrome/test/data/android/navigate/three.html")};

        for (String url : urls) {
            mActivityTestRule.loadUrlInNewTab(url);
        }

        int lastUrlIndex = urls.length - 1;

        View button = mActivityTestRule.getActivity().findViewById(R.id.tab_switcher_button);
        Assert.assertNotNull("Could not find 'tab_switcher_button'", button);

        for (int i = 0; i < 15; i++) {
            TouchCommon.singleClickView(button);
            // Switch back to the tab view from the tab-switcher mode.
            TouchCommon.singleClickView(button);

            Assert.assertEquals("URL mismatch after switching back to the tab from tab-switch mode",
                    urls[lastUrlIndex],
                    mActivityTestRule.getActivity().getActivityTab().getUrlString());
        }
    }

    /**
     * Open an incognito tab from menu and verify its property.
     */
    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    public void testOpenIncognitoTab() {
        mActivityTestRule.newIncognitoTabFromMenu();

        Assert.assertTrue("Current Tab should be an incognito tab.",
                mActivityTestRule.getActivity().getActivityTab().isIncognito());
    }

    /**
     * Test NewTab button on the browser toolbar.
     * Restricted to phones due crbug.com/429671.
     */
    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testNewTabButton() throws ExecutionException, InterruptedException {
        int initialTabCount = mActivityTestRule.getActivity().getCurrentTabModel().getCount();
        showOverviewWithNoAnimation();

        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity()
                        .findViewById(R.id.new_tab_button)
                        .performClick());

        int newTabCount = mActivityTestRule.getActivity().getCurrentTabModel().getCount();
        Assert.assertEquals("Tab count is expected to increment by 1 after clicking new tab button",
                initialTabCount + 1, newTabCount);
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        CriteriaHelper.pollInstrumentationThread(
                () -> !mActivityTestRule.getActivity().isInOverviewMode());
    }

    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testToolbarSwipeOnlyTab() throws TimeoutException {
        initToolbarSwipeTest(false, 0, false);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.RIGHT, 0, false);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.LEFT, 0, false);
    }

    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testToolbarSwipePrevTab() throws TimeoutException {
        initToolbarSwipeTest(true, 1, false);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.RIGHT, 0, true);
    }

    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testToolbarSwipeNextTab() throws TimeoutException {
        initToolbarSwipeTest(true, 0, false);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.LEFT, 1, true);
    }

    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testToolbarSwipePrevTabNone() throws TimeoutException {
        initToolbarSwipeTest(true, 0, false);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.RIGHT, 0, false);
    }

    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testToolbarSwipeNextTabNone() throws TimeoutException {
        initToolbarSwipeTest(true, 1, false);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.LEFT, 1, false);
    }

    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testToolbarSwipeNextThenPrevTab() throws TimeoutException {
        initToolbarSwipeTest(true, 0, false);

        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.LEFT, 1, true);

        final TabModel tabModel =
                mActivityTestRule.getActivity().getTabModelSelector().getModel(false);
        Assert.assertEquals("Incorrect tab index after first swipe.", 1, tabModel.index());

        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.RIGHT, 0, true);
    }

    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testToolbarSwipeNextThenPrevTabIncognito() throws TimeoutException {
        initToolbarSwipeTest(true, 0, true);

        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.LEFT, 1, true);

        final TabModel tabModel =
                mActivityTestRule.getActivity().getTabModelSelector().getModel(true);
        Assert.assertEquals("Incorrect tab index after first swipe.", 1, tabModel.index());

        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.RIGHT, 0, true);
    }

    /**
     * Initialize a test for the toolbar swipe behavior.
     * @param useTwoTabs Whether the test should use two tabs. One tab is used if {@code false}.
     * @param selectedTab The tab index in the current model to have selected after the tabs are
     *                    loaded.
     * @param incognito Whether the test should run on incognito tabs.
     */
    private void initToolbarSwipeTest(boolean useTwoTabs, int selectedTab, boolean incognito) {
        if (incognito) {
            // If incognito, there is no default tab, so open a new one and switch to it.
            mActivityTestRule.loadUrlInNewTab(generateSolidColorUrl("#00ff00"), true);
            mActivityTestRule.getActivity().getTabModelSelector().selectModel(true);
        } else {
            // If not incognito, use the tab the test started on.
            mActivityTestRule.loadUrl(generateSolidColorUrl("#00ff00"));
        }

        if (useTwoTabs) {
            mActivityTestRule.loadUrlInNewTab(generateSolidColorUrl("#0000ff"), incognito);
        }

        ChromeTabUtils.switchTabInCurrentTabModel(mActivityTestRule.getActivity(), selectedTab);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        final TabModelSelector tabModelSelector =
                mActivityTestRule.getActivity().getTabModelSelector();
        final TabModel tabModel = tabModelSelector.getModel(incognito);

        Assert.assertEquals("Incorrect model selected.", incognito,
                tabModelSelector.getCurrentModel().isIncognito());
        Assert.assertEquals("Incorrect starting index.", selectedTab, tabModel.index());
        Assert.assertEquals("Incorrect tab count.", useTwoTabs ? 2 : 1, tabModel.getCount());
    }

    private void runToolbarSideSwipeTestOnCurrentModel(@ScrollDirection int direction,
            int finalIndex, boolean expectsSelection) throws TimeoutException {
        final CallbackHelper selectCallback = new CallbackHelper();
        final ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        final int id = activity.getCurrentTabModel().getTabAt(finalIndex).getId();
        final TabModelSelectorTabModelObserver observer =
                new TabModelSelectorTabModelObserver(activity.getTabModelSelector()) {
                    @Override
                    public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                        if (tab.getId() == id) selectCallback.notifyCalled();
                    }
                };

        int tabSelectedCallCount = selectCallback.getCallCount();

        // Listen for changes in the layout to indicate the swipe has completed.
        final CallbackHelper staticLayoutCallbackHelper = new CallbackHelper();
        activity.getCompositorViewHolder().getLayoutManager().addSceneChangeObserver(
                new SceneChangeObserver() {
                    @Override
                    public void onTabSelectionHinted(int tabId) {}

                    @Override
                    public void onSceneChange(Layout layout) {
                        if (layout instanceof StaticLayout) {
                            staticLayoutCallbackHelper.notifyCalled();
                        }
                    }
                });

        int callLayoutChangeCount = staticLayoutCallbackHelper.getCallCount();
        performToolbarSideSwipe(direction);
        staticLayoutCallbackHelper.waitForCallback(callLayoutChangeCount, 1);

        if (expectsSelection) selectCallback.waitForCallback(tabSelectedCallCount, 1);
        TestThreadUtils.runOnUiThreadBlocking(() -> observer.destroy());

        Assert.assertEquals("Index after toolbar side swipe is incorrect", finalIndex,
                activity.getCurrentTabModel().index());
    }

    private void performToolbarSideSwipe(@ScrollDirection int direction) {
        Assert.assertTrue("Unexpected direction for side swipe " + direction,
                direction == ScrollDirection.LEFT || direction == ScrollDirection.RIGHT);
        final View toolbar = mActivityTestRule.getActivity().findViewById(R.id.toolbar);

        int[] toolbarPos = new int[2];
        toolbar.getLocationOnScreen(toolbarPos);
        final int width = toolbar.getWidth();
        final int height = toolbar.getHeight();

        final int fromX = toolbarPos[0] + width / 2;
        final int toX = toolbarPos[0] + (direction == ScrollDirection.LEFT ? 0 : width);
        final int y = toolbarPos[1] + height / 2;
        final int stepCount = 25;
        final long duration = 500;

        View toolbarRoot = mActivityTestRule.getActivity()
                                   .getRootUiCoordinatorForTesting()
                                   .getToolbarManager()
                                   .getContainerViewForTesting();
        TouchCommon.performDrag(toolbarRoot, fromX, toX, y, y, stepCount, duration);
    }

    /**
     * Test that swipes and tab transitions are not causing URL bar to be focused.
     */
    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"Android-TabSwitcher"})
    public void testOSKIsNotShownDuringSwipe() throws InterruptedException {
        final View urlBar = mActivityTestRule.getActivity().findViewById(R.id.url_bar);
        final LayoutManagerChrome layoutManager = updateTabsViewSize();
        final SwipeHandler swipeHandler = layoutManager.getToolbarSwipeHandler();

        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> urlBar.requestFocus());
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());

        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> urlBar.clearFocus());
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());

        Assert.assertFalse("Keyboard somehow got shown",
                mActivityTestRule.getKeyboardDelegate().isKeyboardShowing(
                        mActivityTestRule.getActivity(), urlBar));

        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            swipeHandler.onSwipeStarted(ScrollDirection.RIGHT, createMotionEvent(0, 0));
            float swipeXChange = mTabsViewWidthDp / 2.f;
            swipeHandler.onSwipeUpdated(createMotionEvent(swipeXChange / mPxToDp, 0.f),
                    swipeXChange / mPxToDp, 0.f, swipeXChange / mPxToDp, 0.f);
        });

        CriteriaHelper.pollUiThread(() -> {
            return !mActivityTestRule.getActivity()
                            .getLayoutManager()
                            .getActiveLayout()
                            .shouldDisplayContentOverlay();
        });

        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            Assert.assertFalse("Keyboard should be hidden while swiping",
                    mActivityTestRule.getKeyboardDelegate().isKeyboardShowing(
                            mActivityTestRule.getActivity(), urlBar));
            swipeHandler.onSwipeFinished();
        });

        CriteriaHelper.pollUiThread(() -> {
            LayoutManagerImpl driver = mActivityTestRule.getActivity().getLayoutManager();
            return driver.getActiveLayout().shouldDisplayContentOverlay();
        }, "Layout not requesting Tab Android view be attached");

        Assert.assertFalse("Keyboard should not be shown",
                mActivityTestRule.getKeyboardDelegate().isKeyboardShowing(
                        mActivityTestRule.getActivity(), urlBar));
    }

    /**
     * Test that orientation changes cause the live tab reflow.
     */
    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testOrientationChangeCausesLiveTabReflowInNormalView()
            throws InterruptedException, TimeoutException {
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        mActivityTestRule.loadUrl(RESIZE_TEST_URL);
        final WebContents webContents = mActivityTestRule.getWebContents();

        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                mActivityTestRule.getWebContents(), "resizeHappened = false;");
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        Assert.assertEquals("onresize event wasn't received by the tab (normal view)", "true",
                JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents, "resizeHappened",
                        WAIT_RESIZE_TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    /**
     * Test that orientation changes cause the live tab reflow.
     */
    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testOrientationChangeCausesLiveTabReflowInTabSwitcher()
            throws InterruptedException, TimeoutException {
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        mActivityTestRule.loadUrl(RESIZE_TEST_URL);

        showOverviewWithNoAnimation();
        final WebContents webContents = mActivityTestRule.getWebContents();
        JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents, "resizeHappened = false;");
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        Assert.assertEquals(
                "onresize event wasn't received by the live tab (tabswitcher, to Landscape)",
                "true",
                JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents, "resizeHappened",
                        WAIT_RESIZE_TIMEOUT_MS, TimeUnit.MILLISECONDS));

        JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents, "resizeHappened = false;");
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        Assert.assertEquals(
                "onresize event wasn't received by the live tab (tabswitcher, to Portrait)", "true",
                JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents, "resizeHappened",
                        WAIT_RESIZE_TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    public void testLastClosedUndoableTabGetsHidden() {
        final TabModel model =
                mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel();
        final Tab tab = TabModelUtils.getCurrentTab(model);

        Assert.assertEquals("Too many tabs at startup", 1, model.getCount());

        TestThreadUtils.runOnUiThreadBlocking(
                (Runnable) () -> model.closeTab(tab, false, false, true));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue("Tab close is not undoable", model.isClosurePending(tab.getId()));
            Assert.assertTrue("Tab was not hidden", tab.isHidden());
        });
    }

    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    public void testLastClosedTabTriggersNotifyChangedCall() {
        final TabModel model =
                mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel();
        final Tab tab = TabModelUtils.getCurrentTab(model);
        final TabModelSelector selector = mActivityTestRule.getActivity().getTabModelSelector();
        mNotifyChangedCalled = false;

        selector.addObserver(new EmptyTabModelSelectorObserver() {
            @Override
            public void onChange() {
                mNotifyChangedCalled = true;
            }
        });

        Assert.assertEquals("Too many tabs at startup", 1, model.getCount());

        TestThreadUtils.runOnUiThreadBlocking(
                (Runnable) () -> model.closeTab(tab, false, false, true));

        Assert.assertTrue("notifyChanged() was not called", mNotifyChangedCalled);
    }

    // Flaky: http://crbug.com/901986
    @Test
    @DisabledTest
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    public void testTabsAreDestroyedOnModelDestruction() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        final TabModelSelectorImpl selector =
                (TabModelSelectorImpl) mActivityTestRule.getActivity().getTabModelSelector();
        final Tab tab = mActivityTestRule.getActivity().getActivityTab();

        final AtomicBoolean webContentsDestroyCalled = new AtomicBoolean();

        TestThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                @SuppressWarnings("unused") // Avoid GC of observer
                WebContentsObserver observer = new WebContentsObserver(tab.getWebContents()) {
                            @Override
                            public void destroy() {
                                super.destroy();
                                webContentsDestroyCalled.set(true);
                            }
                        };

                Assert.assertNotNull("No initial tab at startup", tab);
                Assert.assertNotNull("Tab does not have a web contents", tab.getWebContents());
                Assert.assertTrue("Tab is destroyed", tab.isInitialized());

                selector.destroy();

                Assert.assertNull("Tab still has a web contents", tab.getWebContents());
                Assert.assertFalse("Tab was not destroyed", tab.isInitialized());
            }
        });

        Assert.assertTrue(
                "WebContentsObserver was never destroyed", webContentsDestroyCalled.get());
    }

    // Flaky even with RetryOnFailure: http://crbug.com/649429
    @Test
    @DisabledTest
    //    @MediumTest
    //    @Feature({"Android-TabSwitcher"})
    public void testIncognitoTabsNotRestoredAfterSwipe() throws Exception {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mActivityTestRule.startMainActivityWithURL(mTestServer.getURL(TEST_PAGE_FILE_PATH));

        mActivityTestRule.newIncognitoTabFromMenu();
        // Tab states are not saved for empty NTP tabs, so navigate to any page to trigger a file
        // to be saved.
        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_PAGE_FILE_PATH));

        File tabStateDir = TabStateDirectory.getOrCreateTabbedModeStateDirectory();
        TabModel normalModel =
                mActivityTestRule.getActivity().getTabModelSelector().getModel(false);
        TabModel incognitoModel =
                mActivityTestRule.getActivity().getTabModelSelector().getModel(true);
        File normalTabFile = new File(tabStateDir,
                TabStateFileManager.getTabStateFilename(
                        normalModel.getTabAt(normalModel.getCount() - 1).getId(), false));
        File incognitoTabFile = new File(tabStateDir,
                TabStateFileManager.getTabStateFilename(incognitoModel.getTabAt(0).getId(), true));

        assertFileExists(normalTabFile, true);
        assertFileExists(incognitoTabFile, true);

        // Although we're destroying the activity, the Application will still live on since its in
        // the same process as this test.
        ApplicationTestUtils.finishActivity(mActivityTestRule.getActivity());

        // Activity will be started without a savedInstanceState.
        mActivityTestRule.startMainActivityOnBlankPage();
        assertFileExists(normalTabFile, true);
        assertFileExists(incognitoTabFile, false);
    }

    /**
     * Generate a URL that shows a web page with a solid color. This makes visual debugging easier.
     * @param htmlColor The HTML/CSS color the page should display.
     * @return A URL that shows the solid color when loaded.
     */
    private static String generateSolidColorUrl(String htmlColor) {
        return UrlUtils.encodeHtmlDataUri("<html><head><style>"
                + "  body { background-color: " + htmlColor + ";}"
                + "</style></head>"
                + "<body></body></html>");
    }

    private void assertFileExists(final File fileToCheck, final boolean expected) {
        CriteriaHelper.pollInstrumentationThread(
                () -> Criteria.checkThat(fileToCheck.exists(), Matchers.is(expected)));
    }

    /**
     * Load a url in multiple new tabs in parallel. Each {@link Tab} will pretend to be
     * created from a link.
     *
     * @param url The url of the page to load.
     * @param numTabs The number of tabs to open.
     */
    private void loadUrlInManyNewTabs(final String url, final int numTabs) throws TimeoutException {
        final CallbackHelper[] pageLoadedCallbacks = new CallbackHelper[numTabs];
        final int[] tabIds = new int[numTabs];
        for (int i = 0; i < numTabs; ++i) {
            final int index = i;
            InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
                @Override
                public void run() {
                    Tab currentTab =
                            mActivityTestRule.getActivity().getCurrentTabCreator().launchUrl(
                                    url, TabLaunchType.FROM_LINK);
                    final CallbackHelper pageLoadCallback = new CallbackHelper();
                    pageLoadedCallbacks[index] = pageLoadCallback;
                    currentTab.addObserver(new EmptyTabObserver() {
                        @Override
                        public void onPageLoadFinished(Tab tab, GURL url) {
                            pageLoadCallback.notifyCalled();
                            tab.removeObserver(this);
                        }
                    });
                    tabIds[index] = currentTab.getId();
                }
            });
        }
        //  When opening many tabs some may be frozen due to memory pressure and won't send
        //  PAGE_LOAD_FINISHED events. Iterate over the newly opened tabs and wait for each to load.
        for (int i = 0; i < numTabs; ++i) {
            final TabModel tabModel = mActivityTestRule.getActivity().getCurrentTabModel();
            final Tab tab = TabModelUtils.getTabById(tabModel, tabIds[i]);
            InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
                @Override
                public void run() {
                    TabModelUtils.setIndex(tabModel, tabModel.indexOf(tab));
                }
            });
            pageLoadedCallbacks[i].waitForCallback(0);
        }
    }

    private JavascriptTabModalDialog getCurrentAlertDialog() {
        return (JavascriptTabModalDialog) TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            PropertyModel dialogModel = mActivityTestRule.getActivity()
                                                .getModalDialogManager()
                                                .getCurrentDialogForTest();
            return dialogModel != null ? dialogModel.get(ModalDialogProperties.CONTROLLER) : null;
        });
    }
}
