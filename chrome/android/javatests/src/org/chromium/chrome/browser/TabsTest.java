// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.ui.test.util.ViewUtils.createMotionEvent;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.content.pm.ActivityInfo;
import android.graphics.Point;
import android.os.SystemClock;
import android.support.test.InstrumentationRegistry;
import android.util.DisplayMetrics;
import android.view.View;

import androidx.test.espresso.Espresso;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Manual;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.layouts.SceneChangeObserver;
import org.chromium.chrome.browser.compositor.layouts.StaticLayout;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabpersistence.TabStateDirectory;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.ScrollDirection;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.SwipeHandler;
import org.chromium.components.javascript_dialogs.JavascriptTabModalDialog;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.test.util.UiUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.UiRestriction;
import org.chromium.url.GURL;

import java.io.File;
import java.util.Locale;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * General Tab tests.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class TabsTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private static final String TEST_FILE_PATH =
            "/chrome/test/data/android/tabstest/tabs_test.html";
    private static final String TEST_PAGE_FILE_PATH = "/chrome/test/data/google/google.html";

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

        CompositorAnimationHandler.setTestingMode(true);
    }

    @After
    public void tearDown() {
        sActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
    }

    private String getUrl(String filePath) {
        return sActivityTestRule.getTestServer().getURL(filePath);
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
        sActivityTestRule.loadUrl(getUrl(TEST_FILE_PATH));
        final Tab tab = sActivityTestRule.getActivity().getActivityTab();

        sActivityTestRule.newIncognitoTabFromMenu();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> tab.getWebContents().evaluateJavaScriptForTests("(function() {"
                                        + "  window.open('www.google.com');"
                                        + "})()",
                                null));

        CriteriaHelper.pollUiThread(() -> {
            int tabCount = sActivityTestRule.getActivity()
                                   .getTabModelSelector()
                                   .getModel(false)
                                   .getCount();
            Criteria.checkThat(tabCount, Matchers.is(2));
        });
    }

    @Test
    @MediumTest
    public void testAlertDialogDoesNotChangeActiveModel() {
        sActivityTestRule.newIncognitoTabFromMenu();
        sActivityTestRule.loadUrl(getUrl(TEST_FILE_PATH));
        final Tab tab = sActivityTestRule.getActivity().getActivityTab();
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
                sActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
    }

    /**
     * Verify New Tab Open and Close Event not from the context menu.
     */
    @Test
    @LargeTest
    @Feature({"Android-TabSwitcher"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @DisabledTest(message = "https://crbug.com/1347598")
    public void testOpenAndCloseNewTabButton() {
        sActivityTestRule.loadUrl(getUrl(TEST_FILE_PATH));
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            String title =
                    sActivityTestRule.getActivity().getCurrentTabModel().getTabAt(0).getTitle();
            Assert.assertEquals("Data file for TabsTest", title);
        });
        final int tabCount = sActivityTestRule.getActivity().getCurrentTabModel().getCount();
        View tabSwitcherButton =
                sActivityTestRule.getActivity().findViewById(R.id.tab_switcher_button);
        Assert.assertNotNull("'tab_switcher_button' view is not found", tabSwitcherButton);
        TouchCommon.singleClickView(tabSwitcherButton);
        LayoutTestUtils.waitForLayout(
                sActivityTestRule.getActivity().getLayoutManager(), LayoutType.TAB_SWITCHER);

        View newTabButton = sActivityTestRule.getActivity().findViewById(R.id.new_tab_button);
        Assert.assertNotNull("'new_tab_button' view is not found", newTabButton);
        TouchCommon.singleClickView(newTabButton);
        LayoutTestUtils.waitForLayout(
                sActivityTestRule.getActivity().getLayoutManager(), LayoutType.BROWSING);

        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                ()
                        -> Assert.assertEquals("The tab count is wrong", tabCount + 1,
                                sActivityTestRule.getActivity().getCurrentTabModel().getCount()));

        CriteriaHelper.pollUiThread(() -> {
            Tab tab = sActivityTestRule.getActivity().getCurrentTabModel().getTabAt(1);
            String title = tab.getTitle().toLowerCase(Locale.US);
            String expectedTitle = "new tab";
            Criteria.checkThat(title, Matchers.startsWith(expectedTitle));
        });

        ChromeTabUtils.closeCurrentTab(
                InstrumentationRegistry.getInstrumentation(), sActivityTestRule.getActivity());
        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                ()
                        -> Assert.assertEquals(tabCount,
                                sActivityTestRule.getActivity().getCurrentTabModel().getCount()));
    }

    private void assertWaitForKeyboardStatus(final boolean show) {
        CriteriaHelper.pollUiThread(() -> {
            boolean isKeyboardShowing = sActivityTestRule.getKeyboardDelegate().isKeyboardShowing(
                    sActivityTestRule.getActivity(), sActivityTestRule.getActivity().getTabsView());
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
        // Open a new tab(The 1st tab) and click node.
        sActivityTestRule.loadUrlInNewTab(getUrl(TEST_FILE_PATH), false);
        Assert.assertEquals("Failed to click node.", true,
                DOMUtils.clickNode(sActivityTestRule.getWebContents(), "input_text"));
        assertWaitForKeyboardStatus(true);

        // Open a new tab(the 2nd tab).
        sActivityTestRule.loadUrlInNewTab(getUrl(TEST_FILE_PATH), false);
        assertWaitForKeyboardStatus(false);

        // Click node in the 2nd tab.
        DOMUtils.clickNode(sActivityTestRule.getWebContents(), "input_text");
        assertWaitForKeyboardStatus(true);

        // Switch to the 1st tab.
        ChromeTabUtils.switchTabInCurrentTabModel(sActivityTestRule.getActivity(), 1);
        assertWaitForKeyboardStatus(false);

        // Click node in the 1st tab.
        DOMUtils.clickNode(sActivityTestRule.getWebContents(), "input_text");
        assertWaitForKeyboardStatus(true);

        // Close current tab(the 1st tab).
        ChromeTabUtils.closeCurrentTab(
                InstrumentationRegistry.getInstrumentation(), sActivityTestRule.getActivity());
        assertWaitForKeyboardStatus(false);
    }

    /**
     * Verify that opening a new window hides keyboard.
     */
    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    public void testHideKeyboardWhenOpeningWindow() throws Exception {
        // Open a new tab and click an editable node.
        ChromeTabUtils.fullyLoadUrlInNewTab(InstrumentationRegistry.getInstrumentation(),
                sActivityTestRule.getActivity(), getUrl(TEST_FILE_PATH), false);
        Assert.assertEquals("Failed to click textarea.", true,
                DOMUtils.clickNode(sActivityTestRule.getWebContents(), "textarea"));
        assertWaitForKeyboardStatus(true);

        // Click the button to open a new window.
        Assert.assertEquals("Failed to click button.", true,
                DOMUtils.clickNode(sActivityTestRule.getWebContents(), "button"));
        assertWaitForKeyboardStatus(false);
    }

    private void assertWaitForSelectedText(final String text) {
        CriteriaHelper.pollUiThread(() -> {
            WebContents webContents = sActivityTestRule.getWebContents();
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
        sActivityTestRule.getActivity().getWindowManager().getDefaultDisplay().getSize(size);
        float dragStartX = size.x * startX;
        float dragEndX = size.x * endX;
        float dragStartY = size.y * startY;
        float dragEndY = size.y * endY;
        TouchCommon.performDrag(sActivityTestRule.getActivity(), dragStartX, dragEndX, dragStartY,
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
        sActivityTestRule.loadUrlInNewTab(getUrl(TEST_FILE_PATH), false);
        DOMUtils.longPressNode(sActivityTestRule.getWebContents(), "textarea");
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
                InstrumentationRegistry.getInstrumentation(), sActivityTestRule.getActivity());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // Make sure we're on the NTP
        Tab tab = sActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(tab);

        sActivityTestRule.loadUrl(INITIAL_SIZE_TEST_URL);

        final WebContents webContents = tab.getWebContents();
        String innerText = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                webContents, "document.body.innerText").replace("\"", "");

        DisplayMetrics metrics = sActivityTestRule.getActivity().getResources().getDisplayMetrics();

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
     * Verify that we can open a large number of tabs without running out of
     * memory. This test waits for the NTP to load before opening the next one.
     * This is a LargeTest but because we're doing it "slowly", we need to further scale
     * the timeout for adb am instrument and the various events.
     */
    @Test
    @Manual(message = "Slow test")
    @Feature({"Android-TabSwitcher"})
    public void testOpenManyTabsSlowly() {
        int startCount = sActivityTestRule.getActivity().getCurrentTabModel().getCount();
        for (int i = 1; i <= STRESSFUL_TAB_COUNT; ++i) {
            ChromeTabUtils.newTabFromMenu(
                    InstrumentationRegistry.getInstrumentation(), sActivityTestRule.getActivity());
            InstrumentationRegistry.getInstrumentation().waitForIdleSync();
            Assert.assertEquals(startCount + i,
                    sActivityTestRule.getActivity().getCurrentTabModel().getCount());
        }
    }

    /**
     * Verify that we can open a large number of tabs without running out of
     * memory. This test hammers the "new tab" button quickly to stress the app.
     */
    @Test
    @Manual(message = "Slow test")
    @Feature({"Android-TabSwitcher"})
    public void testOpenManyTabsQuickly() {
        int startCount = sActivityTestRule.getActivity().getCurrentTabModel().getCount();
        for (int i = 1; i <= STRESSFUL_TAB_COUNT; ++i) {
            MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                    sActivityTestRule.getActivity(), R.id.new_tab_menu_id);
            Assert.assertEquals(startCount + i,
                    sActivityTestRule.getActivity().getCurrentTabModel().getCount());
        }
    }

    /**
     * Verify that we can open a burst of new tabs, even when there are already
     * a large number of tabs open.
     */
    @Test
    @Manual(message = "Slow test")
    @Feature({"Navigation"})
    public void testOpenManyTabsInBursts() throws TimeoutException {
        final int burstSize = 5;
        final String url = getUrl(TEST_PAGE_FILE_PATH);
        final int startCount = sActivityTestRule.getActivity().getCurrentTabModel().getCount();
        for (int tabCount = startCount; tabCount < STRESSFUL_TAB_COUNT; tabCount += burstSize)  {
            loadUrlInManyNewTabs(url, burstSize);
            Assert.assertEquals(tabCount + burstSize,
                    sActivityTestRule.getActivity().getCurrentTabModel().getCount());
        }
    }

    /**
     * Verify opening 10 tabs at once and that each tab loads when selected.
     */
    @Test
    @Manual(message = "Slow test")
    @Feature({"Navigation"})
    public void testOpenManyTabsAtOnce10() throws TimeoutException {
        openAndVerifyManyTestTabs(10);
    }

    /**
     * Verify that we can open a large number of tabs all at once and that each
     * tab loads when selected.
     */
    private void openAndVerifyManyTestTabs(final int num) throws TimeoutException {
        final String url = getUrl(TEST_PAGE_FILE_PATH);
        int startCount = sActivityTestRule.getActivity().getCurrentTabModel().getCount();
        loadUrlInManyNewTabs(url, num);
        Assert.assertEquals(
                startCount + num, sActivityTestRule.getActivity().getCurrentTabModel().getCount());
    }

    /** Enters the tab switcher without animation.*/
    private void showOverviewWithNoAnimation() {
        LayoutTestUtils.startShowingAndWaitForLayout(
                sActivityTestRule.getActivity().getLayoutManager(), LayoutType.TAB_SWITCHER, false);
    }

    /** Exits the tab switcher without animation. */
    private void hideOverviewWithNoAnimation() {
        LayoutTestUtils.startShowingAndWaitForLayout(
                sActivityTestRule.getActivity().getLayoutManager(), LayoutType.BROWSING, false);
    }

    /**
     * Opens tabs to populate the model to a given count.
     * @param targetTabCount The desired number of tabs in the model.
     * @param waitToLoad     Whether the tabs need to be fully loaded.
     * @return               The new number of tabs in the model.
     */
    private int openTabs(final int targetTabCount, boolean waitToLoad) {
        final ChromeTabbedActivity activity = sActivityTestRule.getActivity();
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

    private LayoutManagerChrome updateTabsViewSize() {
        View tabsView = sActivityTestRule.getActivity().getTabsView();
        mTabsViewHeightDp = tabsView.getHeight() * mPxToDp;
        mTabsViewWidthDp = tabsView.getWidth() * mPxToDp;
        return sActivityTestRule.getActivity().getLayoutManager();
    }

    /**
     * Test that we can safely close a tab during a fling (http://b/issue?id=5364043)
     */
    @Test
    @SmallTest
    @Feature({"Android-TabSwitcher"})
    public void testCloseTabDuringFling() {
        sActivityTestRule.loadUrlInNewTab(
                getUrl("/chrome/test/data/android/tabstest/text_page.html"));
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            WebContents webContents = sActivityTestRule.getWebContents();
            webContents.getEventForwarder().startFling(
                    SystemClock.uptimeMillis(), 0, -2000, false, true);
        });
        ChromeTabUtils.closeCurrentTab(
                InstrumentationRegistry.getInstrumentation(), sActivityTestRule.getActivity());
    }

    @Test
    @MediumTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @DisabledTest(message = "https://crbug.com/1347598")
    public void testQuickSwitchBetweenTabAndSwitcherMode() {
        final String[] urls = {getUrl("/chrome/test/data/android/navigate/one.html"),
                getUrl("/chrome/test/data/android/navigate/two.html"),
                getUrl("/chrome/test/data/android/navigate/three.html")};

        for (String url : urls) {
            sActivityTestRule.loadUrlInNewTab(url, false);
        }

        final int lastUrlIndex = urls.length - 1;

        View button = sActivityTestRule.getActivity().findViewById(R.id.tab_switcher_button);
        Assert.assertNotNull("Could not find 'tab_switcher_button'", button);

        for (int i = 0; i < 15; i++) {
            TouchCommon.singleClickView(button);

            // Wait for UI to show so the back press will apply to the switcher not the tab.
            onViewWaiting(withId(org.chromium.chrome.test.R.id.tab_switcher_toolbar))
                    .check(matches(isDisplayed()));

            // Switch back to the tab view from the tab-switcher mode.
            Espresso.pressBack();

            Assert.assertEquals("URL mismatch after switching back to the tab from tab-switch mode",
                    urls[lastUrlIndex],
                    ChromeTabUtils.getUrlStringOnUiThread(
                            sActivityTestRule.getActivity().getActivityTab()));
        }
    }

    /**
     * Open an incognito tab from menu and verify its property.
     */
    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    public void testOpenIncognitoTab() {
        sActivityTestRule.newIncognitoTabFromMenu();

        Assert.assertTrue("Current Tab should be an incognito tab.",
                sActivityTestRule.getActivity().getActivityTab().isIncognito());
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
                sActivityTestRule.getActivity().getTabModelSelector().getModel(false);
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
                sActivityTestRule.getActivity().getTabModelSelector().getModel(true);
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
            sActivityTestRule.loadUrlInNewTab(generateSolidColorUrl("#00ff00"), true);
            sActivityTestRule.getActivity().getTabModelSelector().selectModel(true);
        } else {
            // If not incognito, use the tab the test started on.
            sActivityTestRule.loadUrl(generateSolidColorUrl("#00ff00"));
        }

        if (useTwoTabs) {
            sActivityTestRule.loadUrlInNewTab(generateSolidColorUrl("#0000ff"), incognito);
        }

        ChromeTabUtils.switchTabInCurrentTabModel(sActivityTestRule.getActivity(), selectedTab);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        final TabModelSelector tabModelSelector =
                sActivityTestRule.getActivity().getTabModelSelector();
        final TabModel tabModel = tabModelSelector.getModel(incognito);

        Assert.assertEquals("Incorrect model selected.", incognito,
                tabModelSelector.getCurrentModel().isIncognito());
        Assert.assertEquals("Incorrect starting index.", selectedTab, tabModel.index());
        Assert.assertEquals("Incorrect tab count.", useTwoTabs ? 2 : 1, tabModel.getCount());
    }

    private void runToolbarSideSwipeTestOnCurrentModel(@ScrollDirection int direction,
            int finalIndex, boolean expectsSelection) throws TimeoutException {
        final CallbackHelper selectCallback = new CallbackHelper();
        final ChromeTabbedActivity activity = sActivityTestRule.getActivity();
        final int id = activity.getCurrentTabModel().getTabAt(finalIndex).getId();
        final TabModelSelectorTabModelObserver observer =
                TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
                    return new TabModelSelectorTabModelObserver(activity.getTabModelSelector()) {
                        @Override
                        public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                            if (tab.getId() == id) selectCallback.notifyCalled();
                        }
                    };
                });

        int tabSelectedCallCount = selectCallback.getCallCount();

        // Listen for changes in the layout to indicate the swipe has completed.
        final CallbackHelper staticLayoutCallbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            activity.getCompositorViewHolderForTesting().getLayoutManager().addSceneChangeObserver(
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
        final View toolbar = sActivityTestRule.getActivity().findViewById(R.id.toolbar);

        int[] toolbarPos = new int[2];
        toolbar.getLocationOnScreen(toolbarPos);
        final int width = toolbar.getWidth();
        final int height = toolbar.getHeight();

        final int fromX = toolbarPos[0] + width / 2;
        final int toX = toolbarPos[0] + (direction == ScrollDirection.LEFT ? 0 : width);
        final int y = toolbarPos[1] + height / 2;
        final int stepCount = 25;
        final long duration = 500;

        View toolbarRoot = sActivityTestRule.getActivity()
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
        final View urlBar = sActivityTestRule.getActivity().findViewById(R.id.url_bar);
        final LayoutManagerChrome layoutManager = updateTabsViewSize();
        final SwipeHandler swipeHandler = layoutManager.getToolbarSwipeHandler();

        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> urlBar.requestFocus());
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());

        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> urlBar.clearFocus());
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), sActivityTestRule.getActivity());
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());

        Assert.assertFalse("Keyboard somehow got shown",
                sActivityTestRule.getKeyboardDelegate().isKeyboardShowing(
                        sActivityTestRule.getActivity(), urlBar));

        PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, () -> {
            swipeHandler.onSwipeStarted(ScrollDirection.RIGHT, createMotionEvent(0, 0));
            float swipeXChange = mTabsViewWidthDp / 2.f;
            swipeHandler.onSwipeUpdated(createMotionEvent(swipeXChange / mPxToDp, 0.f),
                    swipeXChange / mPxToDp, 0.f, swipeXChange / mPxToDp, 0.f);
        });

        CriteriaHelper.pollUiThread(() -> {
            return !sActivityTestRule.getActivity()
                            .getLayoutManager()
                            .getActiveLayout()
                            .shouldDisplayContentOverlay();
        });

        PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, () -> {
            Assert.assertFalse("Keyboard should be hidden while swiping",
                    sActivityTestRule.getKeyboardDelegate().isKeyboardShowing(
                            sActivityTestRule.getActivity(), urlBar));
            swipeHandler.onSwipeFinished();
        });

        CriteriaHelper.pollUiThread(() -> {
            LayoutManagerImpl driver = sActivityTestRule.getActivity().getLayoutManager();
            return driver.getActiveLayout().shouldDisplayContentOverlay();
        }, "Layout not requesting Tab Android view be attached");

        Assert.assertFalse("Keyboard should not be shown",
                sActivityTestRule.getKeyboardDelegate().isKeyboardShowing(
                        sActivityTestRule.getActivity(), urlBar));
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
        sActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), sActivityTestRule.getActivity());
        sActivityTestRule.loadUrl(RESIZE_TEST_URL);
        final WebContents webContents = sActivityTestRule.getWebContents();

        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                sActivityTestRule.getWebContents(), "resizeHappened = false;");
        sActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        Assert.assertEquals("onresize event wasn't received by the tab (normal view)", "true",
                JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents, "resizeHappened",
                        WAIT_RESIZE_TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @DisabledTest(message = "https://crbug.com/1424109")
    public void testLastClosedUndoableTabGetsHidden() {
        final TabModel model =
                sActivityTestRule.getActivity().getTabModelSelector().getCurrentModel();
        final Tab tab = TabModelUtils.getCurrentTab(model);

        Assert.assertEquals("Too many tabs at startup", 1, model.getCount());

        TestThreadUtils.runOnUiThreadBlocking(
                (Runnable) () -> model.closeTab(tab, false, false, true));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue("Tab close is not undoable", model.isClosurePending(tab.getId()));
            Assert.assertTrue("Tab was not hidden", tab.isHidden());
        });
    }

    private static class FocusListener implements View.OnFocusChangeListener {
        private View mView;
        private int mTimesFocused;
        private int mTimesUnfocused;

        FocusListener(View view) {
            mView = view;
        }

        @Override
        public void onFocusChange(View v, boolean hasFocus) {
            if (v != mView) return;

            if (hasFocus) {
                mTimesFocused++;
            } else {
                mTimesUnfocused++;
            }
        }

        int getTimesFocused() {
            return mTimesFocused;
        }

        int getTimesUnfocused() {
            return mTimesUnfocused;
        }

        boolean hasFocus() {
            return TestThreadUtils.runOnUiThreadBlockingNoException(
                    () -> { return mView.hasFocus(); });
        }
    }

    // Regression test for https://crbug.com/1394372.
    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"Android-TabSwitcher"})
    public void testRequestFocusOnCloseTab() throws Exception {
        final View urlBar = sActivityTestRule.getActivity().findViewById(R.id.url_bar);
        final TabModel model =
                sActivityTestRule.getActivity().getTabModelSelector().getCurrentModel();
        final Tab oldTab = TabModelUtils.getCurrentTab(model);

        Assert.assertNotNull("Tab should have a view", oldTab.getView());

        final FocusListener focusListener = new FocusListener(oldTab.getView());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { oldTab.getView().setOnFocusChangeListener(focusListener); });
        Assert.assertEquals(
                "oldTab should not have been focused.", 0, focusListener.getTimesFocused());
        Assert.assertEquals(
                "oldTab should not have been unfocused.", 0, focusListener.getTimesUnfocused());
        Assert.assertTrue("oldTab should have focus.", focusListener.hasFocus());

        final Tab newTab =
                ChromeTabUtils.fullyLoadUrlInNewTab(InstrumentationRegistry.getInstrumentation(),
                        sActivityTestRule.getActivity(), "about:blank", false);

        Assert.assertEquals(
                "oldTab should not have been focused.", 0, focusListener.getTimesFocused());
        Assert.assertEquals(
                "oldTab should have been unfocused.", 1, focusListener.getTimesUnfocused());
        Assert.assertFalse("oldTab should not have focus", focusListener.hasFocus());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { model.closeTab(newTab, false, false, true); });

        Assert.assertEquals("oldTab should have been focused.", 1, focusListener.getTimesFocused());
        Assert.assertEquals("oldTab should not have been unfocused again.", 1,
                focusListener.getTimesUnfocused());
        Assert.assertTrue("oldTab should have focus.", focusListener.hasFocus());

        // Focus on the URL bar.
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> urlBar.requestFocus());
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());

        Assert.assertEquals(
                "oldTab should not have been focused again.", 1, focusListener.getTimesFocused());
        Assert.assertEquals("oldTab should have been unfocused by url bar.", 2,
                focusListener.getTimesUnfocused());
        Assert.assertFalse("oldTab should not have focus.", focusListener.hasFocus());
        Assert.assertTrue("Keyboard should show",
                sActivityTestRule.getKeyboardDelegate().isKeyboardShowing(
                        sActivityTestRule.getActivity(), urlBar));

        // Check refocus doesn't happen again on the closure being finalized.
        TestThreadUtils.runOnUiThreadBlocking(() -> model.commitAllTabClosures());

        Assert.assertEquals(
                "oldTab should not have been focused again after committing tab closures.", 1,
                focusListener.getTimesFocused());
        Assert.assertEquals(
                "oldTab should not have been unfocused again after committing tab closures.", 2,
                focusListener.getTimesUnfocused());
        Assert.assertFalse("oldTab should remain unfocused.", focusListener.hasFocus());

        Assert.assertTrue("Keyboard should show",
                sActivityTestRule.getKeyboardDelegate().isKeyboardShowing(
                        sActivityTestRule.getActivity(), urlBar));

        // Ensure the keyboard is hidden so we are in a clean-slate for next test.
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> urlBar.clearFocus());
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> oldTab.getView().requestFocus());
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());

        Assert.assertFalse("Keyboard should no longer show",
                sActivityTestRule.getKeyboardDelegate().isKeyboardShowing(
                        sActivityTestRule.getActivity(), urlBar));
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"Android-TabSwitcher"})
    public void testRequestFocusOnSwitchTab() {
        final TabModel model =
                sActivityTestRule.getActivity().getTabModelSelector().getCurrentModel();
        final Tab oldTab = TabModelUtils.getCurrentTab(model);

        Assert.assertNotNull("Tab should have a view", oldTab.getView());

        final FocusListener oldTabFocusListener = new FocusListener(oldTab.getView());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { oldTab.getView().setOnFocusChangeListener(oldTabFocusListener); });
        Assert.assertEquals(
                "oldTab should not have been focused.", 0, oldTabFocusListener.getTimesFocused());
        Assert.assertEquals("oldTab should not have been unfocused.", 0,
                oldTabFocusListener.getTimesUnfocused());
        Assert.assertTrue("oldTab should have focus.", oldTabFocusListener.hasFocus());

        final Tab newTab =
                ChromeTabUtils.fullyLoadUrlInNewTab(InstrumentationRegistry.getInstrumentation(),
                        sActivityTestRule.getActivity(), "about:blank", false);
        final FocusListener newTabFocusListener = new FocusListener(newTab.getView());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { newTab.getView().setOnFocusChangeListener(newTabFocusListener); });
        Assert.assertEquals(
                "newTab should not have been focused.", 0, newTabFocusListener.getTimesFocused());
        Assert.assertEquals("newTab should not have been unfocused.", 0,
                newTabFocusListener.getTimesUnfocused());
        Assert.assertTrue("newTab should have focus.", newTabFocusListener.hasFocus());
        Assert.assertEquals(
                "oldTab should not have been focused.", 0, oldTabFocusListener.getTimesFocused());
        Assert.assertEquals(
                "oldTab should have been unfocused.", 1, oldTabFocusListener.getTimesUnfocused());
        Assert.assertFalse("oldTab should not have focus.", oldTabFocusListener.hasFocus());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.setIndex(model.indexOf(oldTab), TabSelectionType.FROM_USER, false);
        });

        Assert.assertEquals(
                "newTab should not have been focused.", 0, newTabFocusListener.getTimesFocused());
        Assert.assertEquals(
                "newTab should have been unfocused.", 1, newTabFocusListener.getTimesUnfocused());
        Assert.assertFalse("newTab should not have focus.", newTabFocusListener.hasFocus());
        Assert.assertEquals(
                "oldTab should have been focused.", 1, oldTabFocusListener.getTimesFocused());
        Assert.assertEquals("oldTab should not have been unfocused again.", 1,
                oldTabFocusListener.getTimesUnfocused());
        Assert.assertTrue("oldTab should have focus.", oldTabFocusListener.hasFocus());
    }

    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @DisabledTest(message = "https://crbug.com/1347598")
    public void testLastClosedTabTriggersNotifyChangedCall() {
        final TabModel model =
                sActivityTestRule.getActivity().getTabModelSelector().getCurrentModel();
        final Tab tab = TabModelUtils.getCurrentTab(model);
        final TabModelSelector selector = sActivityTestRule.getActivity().getTabModelSelector();
        mNotifyChangedCalled = false;

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            selector.addObserver(new TabModelSelectorObserver() {
                @Override
                public void onChange() {
                    mNotifyChangedCalled = true;
                }
            });
        });

        Assert.assertEquals("Too many tabs at startup", 1, model.getCount());

        TestThreadUtils.runOnUiThreadBlocking(
                (Runnable) () -> model.closeTab(tab, false, false, true));

        Assert.assertTrue("notifyChanged() was not called", mNotifyChangedCalled);
    }

    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    public void testTabsAreDestroyedOnModelDestruction() throws Exception {
        final TabModelSelectorImpl selector =
                (TabModelSelectorImpl) sActivityTestRule.getActivity().getTabModelSelector();
        final Tab tab = sActivityTestRule.getActivity().getActivityTab();

        final CallbackHelper webContentsDestroyed = new CallbackHelper();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            @SuppressWarnings("unused") // Avoid GC of observer
            WebContentsObserver observer = new WebContentsObserver(tab.getWebContents()) {
                @Override
                public void destroy() {
                    super.destroy();
                    webContentsDestroyed.notifyCalled();
                }
            };

            Assert.assertNotNull("No initial tab at startup", tab);
            Assert.assertNotNull("Tab does not have a web contents", tab.getWebContents());
            Assert.assertTrue("Tab is destroyed", tab.isInitialized());
        });

        ApplicationTestUtils.finishActivity(sActivityTestRule.getActivity());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertNull("Tab still has a web contents", tab.getWebContents());
            Assert.assertFalse("Tab was not destroyed", tab.isInitialized());
        });

        webContentsDestroyed.waitForFirst();
    }

    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    public void testIncognitoTabsNotRestoredAfterSwipe() throws Exception {
        sActivityTestRule.loadUrl(getUrl(TEST_PAGE_FILE_PATH));

        sActivityTestRule.newIncognitoTabFromMenu();
        // Tab states are not saved for empty NTP tabs, so navigate to any page to trigger a file
        // to be saved.
        sActivityTestRule.loadUrl(getUrl(TEST_PAGE_FILE_PATH));

        File tabStateDir = TabStateDirectory.getOrCreateTabbedModeStateDirectory();
        TabModel normalModel =
                sActivityTestRule.getActivity().getTabModelSelector().getModel(false);
        TabModel incognitoModel =
                sActivityTestRule.getActivity().getTabModelSelector().getModel(true);
        File normalTabFile = new File(tabStateDir,
                TabStateFileManager.getTabStateFilename(
                        normalModel.getTabAt(normalModel.getCount() - 1).getId(), false));
        File incognitoTabFile = new File(tabStateDir,
                TabStateFileManager.getTabStateFilename(incognitoModel.getTabAt(0).getId(), true));

        assertFileExists(normalTabFile, true);
        assertFileExists(incognitoTabFile, true);

        // Although we're destroying the activity, the Application will still live on since its in
        // the same process as this test.
        ApplicationTestUtils.finishActivity(sActivityTestRule.getActivity());

        // Activity will be started without a savedInstanceState.
        sActivityTestRule.startMainActivityOnBlankPage();
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
                            sActivityTestRule.getActivity().getCurrentTabCreator().launchUrl(
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
            final TabModel tabModel = sActivityTestRule.getActivity().getCurrentTabModel();
            final Tab tab = TabModelUtils.getTabById(tabModel, tabIds[i]);
            InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
                @Override
                public void run() {
                    TabModelUtils.setIndex(tabModel, tabModel.indexOf(tab), false);
                }
            });
            pageLoadedCallbacks[i].waitForCallback(0);
        }
    }

    private JavascriptTabModalDialog getCurrentAlertDialog() {
        return (JavascriptTabModalDialog) TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            PropertyModel dialogModel = sActivityTestRule.getActivity()
                                                .getModalDialogManager()
                                                .getCurrentDialogForTest();
            return dialogModel != null ? dialogModel.get(ModalDialogProperties.CONTROLLER) : null;
        });
    }

    private String getTabBodyText(Tab tab) {
        try {
            return JavaScriptUtils.executeJavaScriptAndWaitForResult(
                    tab.getWebContents(), "document.body.innerText");
        } catch (Exception ex) {
            assert false : "Unexpected Exception";
        }
        return null;
    }
}
