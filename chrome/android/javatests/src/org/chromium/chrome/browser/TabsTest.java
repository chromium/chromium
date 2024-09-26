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
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.content.pm.ActivityInfo;
import android.graphics.Point;
import android.os.SystemClock;
import android.util.DisplayMetrics;
import android.view.View;

import androidx.test.espresso.Espresso;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.hub.HubFieldTrial;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabpersistence.TabStateDirectory;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.components.javascript_dialogs.JavascriptTabModalDialog;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.test.util.UiUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.io.File;
import java.util.Locale;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/** General Tab tests. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(
        reason =
                "https://crbug.com/1347598: Side effects are causing flakes in CI and failures"
                        + " locally. Unbatched to isolate flakes before batching again.")
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

    private boolean mNotifyChangedCalled;

    private static final long WAIT_RESIZE_TIMEOUT_MS = 3000;

    private static final String INITIAL_SIZE_TEST_URL =
            UrlUtils.encodeHtmlDataUri(
                    "<html><head><meta name=\"viewport\" content=\"width=device-width\">"
                            + "<script>"
                            + "  document.writeln(window.innerWidth + ',' + window.innerHeight);"
                            + "</script></head>"
                            + "<body>"
                            + "</body></html>");

    private static final String RESIZE_TEST_URL =
            UrlUtils.encodeHtmlDataUri(
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
        CompositorAnimationHandler.setTestingMode(true);
    }

    @After
    public void tearDown() {
        sActivityTestRule
                .getActivity()
                .setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
    }

    private String getUrl(String filePath) {
        return sActivityTestRule.getTestServer().getURL(filePath);
    }

    /** Verify that spawning a popup from a background tab in a different model works properly. */
    @Test
    @LargeTest
    @Feature({"Navigation"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @CommandLineFlags.Add(ContentSwitches.DISABLE_POPUP_BLOCKING)
    public void testSpawnPopupOnBackgroundTab() {
        sActivityTestRule.loadUrl(getUrl(TEST_FILE_PATH));
        final Tab tab = sActivityTestRule.getActivity().getActivityTab();

        sActivityTestRule.newIncognitoTabFromMenu();

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        tab.getWebContents()
                                .evaluateJavaScriptForTests(
                                        "(function() {"
                                                + "  window.open('www.google.com');"
                                                + "})()",
                                        null));

        CriteriaHelper.pollUiThread(
                () -> {
                    int tabCount =
                            sActivityTestRule
                                    .getActivity()
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
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        tab.getWebContents()
                                .evaluateJavaScriptForTests(
                                        "(function() {" + "  alert('hi');" + "})()", null));

        final AtomicReference<JavascriptTabModalDialog> dialog = new AtomicReference<>();

        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    dialog.set(getCurrentAlertDialog());
                    Criteria.checkThat(dialog.get(), Matchers.notNullValue());
                });

        onView(withId(R.id.positive_button)).perform(click());

        dialog.set(null);

        CriteriaHelper.pollInstrumentationThread(
                () -> Criteria.checkThat(getCurrentAlertDialog(), Matchers.nullValue()));

        Assert.assertTrue(
                "Incognito model was not selected",
                sActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
    }

    /** Verify New Tab Open and Close Event not from the context menu. */
    @Test
    @LargeTest
    @Feature({"Android-TabSwitcher"})
    @Restriction(DeviceFormFactor.PHONE)
    public void testOpenAndCloseNewTabButton() {
        sActivityTestRule.loadUrl(getUrl(TEST_FILE_PATH));
        Tab tab0 =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return sActivityTestRule.getActivity().getCurrentTabModel().getTabAt(0);
                        });
        Assert.assertEquals("Data file for TabsTest", ChromeTabUtils.getTitleOnUiThread(tab0));
        final int originalTabCount =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return sActivityTestRule.getActivity().getCurrentTabModel().getCount();
                        });
        onViewWaiting(withId(R.id.tab_switcher_button))
                .check(matches(isDisplayed()))
                .perform(click());
        LayoutTestUtils.waitForLayout(
                sActivityTestRule.getActivity().getLayoutManager(), LayoutType.TAB_SWITCHER);

        int newTabButtonId =
                HubFieldTrial.usesFloatActionButton()
                        ? R.id.host_action_button
                        : R.id.toolbar_action_button;
        onViewWaiting(withId(newTabButtonId)).check(matches(isDisplayed())).perform(click());
        LayoutTestUtils.waitForLayout(
                sActivityTestRule.getActivity().getLayoutManager(), LayoutType.BROWSING);

        int currentTabCount =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return sActivityTestRule.getActivity().getCurrentTabModel().getCount();
                        });
        Assert.assertEquals(
                "The tab count should increase by one", originalTabCount + 1, currentTabCount);

        CriteriaHelper.pollUiThread(
                () -> {
                    Tab tab1 = sActivityTestRule.getActivity().getCurrentTabModel().getTabAt(1);
                    String title = tab1.getTitle().toLowerCase(Locale.US);
                    String expectedTitle = "new tab";
                    Criteria.checkThat(title, Matchers.startsWith(expectedTitle));
                });

        ChromeTabUtils.closeCurrentTab(
                InstrumentationRegistry.getInstrumentation(), sActivityTestRule.getActivity());
        currentTabCount =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return sActivityTestRule.getActivity().getCurrentTabModel().getCount();
                        });
        Assert.assertEquals(
                "The tab count should be same as original", originalTabCount, currentTabCount);
    }

    private void assertWaitForKeyboardStatus(final boolean show) {
        CriteriaHelper.pollUiThread(
                () -> {
                    boolean isKeyboardShowing =
                            sActivityTestRule
                                    .getKeyboardDelegate()
                                    .isKeyboardShowing(
                                            sActivityTestRule.getActivity(),
                                            sActivityTestRule.getActivity().getTabsView());
                    Criteria.checkThat(isKeyboardShowing, Matchers.is(show));
                });
    }

    /**
     * Verify that opening a new tab, switching to an existing tab and closing current tab hide
     * keyboard.
     */
    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.TABLET)
    @Feature({"Android-TabSwitcher"})
    @DisableIf.Device(DeviceFormFactor.TABLET) // crbug.com/353910783
    public void testHideKeyboard() throws Exception {
        // Open a new tab(The 1st tab) and click node.
        sActivityTestRule.loadUrlInNewTab(getUrl(TEST_FILE_PATH), false);
        Assert.assertEquals(
                "Failed to click node.",
                true,
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

    /** Verify that opening a new window hides keyboard. */
    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @DisabledTest(message = "https://crbug.com/329064612")
    public void testHideKeyboardWhenOpeningWindow() throws Exception {
        // Open a new tab and click an editable node.
        ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(),
                sActivityTestRule.getActivity(),
                getUrl(TEST_FILE_PATH),
                false);
        Assert.assertEquals(
                "Failed to click textarea.",
                true,
                DOMUtils.clickNode(sActivityTestRule.getWebContents(), "textarea"));
        assertWaitForKeyboardStatus(true);

        // Click the button to open a new window.
        Assert.assertEquals(
                "Failed to click button.",
                true,
                DOMUtils.clickNode(sActivityTestRule.getWebContents(), "button"));
        assertWaitForKeyboardStatus(false);
    }

    private void assertWaitForSelectedText(final String text) {
        CriteriaHelper.pollUiThread(
                () -> {
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
        TouchCommon.performDrag(
                sActivityTestRule.getActivity(),
                dragStartX,
                dragEndX,
                dragStartY,
                dragEndY,
                stepCount,
                250);
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
    @Restriction(DeviceFormFactor.PHONE)
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
        String innerText =
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                                webContents, "document.body.innerText")
                        .replace("\"", "");

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

    /** Enters the tab switcher without animation. */
    private void showOverviewWithNoAnimation() {
        LayoutTestUtils.startShowingAndWaitForLayout(
                sActivityTestRule.getActivity().getLayoutManager(), LayoutType.TAB_SWITCHER, false);
    }

    /** Exits the tab switcher without animation. */
    private void hideOverviewWithNoAnimation() {
        LayoutTestUtils.startShowingAndWaitForLayout(
                sActivityTestRule.getActivity().getLayoutManager(), LayoutType.BROWSING, false);
    }

    /** Test that we can safely close a tab during a fling (http://b/issue?id=5364043) */
    @Test
    @SmallTest
    @Feature({"Android-TabSwitcher"})
    public void testCloseTabDuringFling() {
        sActivityTestRule.loadUrlInNewTab(
                getUrl("/chrome/test/data/android/tabstest/text_page.html"));
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            WebContents webContents = sActivityTestRule.getWebContents();
                            webContents
                                    .getEventForwarder()
                                    .startFling(SystemClock.uptimeMillis(), 0, -2000, false, true);
                        });
        ChromeTabUtils.closeCurrentTab(
                InstrumentationRegistry.getInstrumentation(), sActivityTestRule.getActivity());
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE)
    @DisabledTest(message = "https://crbug.com/1347598")
    public void testQuickSwitchBetweenTabAndSwitcherMode() {
        final String[] urls = {
            getUrl("/chrome/test/data/android/navigate/one.html"),
            getUrl("/chrome/test/data/android/navigate/two.html"),
            getUrl("/chrome/test/data/android/navigate/three.html")
        };

        for (String url : urls) {
            sActivityTestRule.loadUrlInNewTab(url, false);
        }

        final int lastUrlIndex = urls.length - 1;
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();

        View button = sActivityTestRule.getActivity().findViewById(R.id.tab_switcher_button);
        Assert.assertNotNull("Could not find 'tab_switcher_button'", button);

        for (int i = 0; i < 15; i++) {
            // Wait for UI to show so the back press will apply to the switcher not the tab.
            TabUiTestHelper.enterTabSwitcher(cta);

            // Switch back to the tab view from the tab-switcher mode.
            Espresso.pressBack();

            Assert.assertEquals(
                    "URL mismatch after switching back to the tab from tab-switch mode",
                    urls[lastUrlIndex],
                    ChromeTabUtils.getUrlStringOnUiThread(
                            sActivityTestRule.getActivity().getActivityTab()));
        }
    }

    /** Open an incognito tab from menu and verify its property. */
    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    public void testOpenIncognitoTab() {
        sActivityTestRule.newIncognitoTabFromMenu();

        Assert.assertTrue(
                "Current Tab should be an incognito tab.",
                sActivityTestRule.getActivity().getActivityTab().isIncognito());
    }

    /** Test that orientation changes cause the live tab reflow. */
    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testOrientationChangeCausesLiveTabReflowInNormalView()
            throws InterruptedException, TimeoutException {
        sActivityTestRule
                .getActivity()
                .setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), sActivityTestRule.getActivity());
        sActivityTestRule.loadUrl(RESIZE_TEST_URL);
        final WebContents webContents = sActivityTestRule.getWebContents();

        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                sActivityTestRule.getWebContents(), "resizeHappened = false;");
        sActivityTestRule
                .getActivity()
                .setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        Assert.assertEquals(
                "onresize event wasn't received by the tab (normal view)",
                "true",
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        webContents,
                        "resizeHappened",
                        WAIT_RESIZE_TIMEOUT_MS,
                        TimeUnit.MILLISECONDS));
    }

    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    public void testLastClosedUndoableTabGetsHidden() {
        final TabModel model =
                sActivityTestRule.getActivity().getTabModelSelector().getCurrentModel();
        final Tab tab = TabModelUtils.getCurrentTab(model);

        Assert.assertEquals("Too many tabs at startup", 1, model.getCount());

        ThreadUtils.runOnUiThreadBlocking(
                (Runnable) () -> model.closeTabs(TabClosureParams.closeTab(tab).build()));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(
                            "Tab close is not undoable", model.isClosurePending(tab.getId()));
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
            return ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        return mView.hasFocus();
                    });
        }
    }

    // Regression test for https://crbug.com/1394372.
    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"Android-TabSwitcher"})
    public void testRequestFocusOnCloseTab() throws Exception {
        final View urlBar = sActivityTestRule.getActivity().findViewById(R.id.url_bar);
        final TabModel model =
                sActivityTestRule.getActivity().getTabModelSelector().getCurrentModel();
        final Tab oldTab = TabModelUtils.getCurrentTab(model);

        Assert.assertNotNull("Tab should have a view", oldTab.getView());

        final FocusListener focusListener = new FocusListener(oldTab.getView());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    oldTab.getView().setOnFocusChangeListener(focusListener);
                });
        Assert.assertEquals(
                "oldTab should not have been focused.", 0, focusListener.getTimesFocused());
        Assert.assertEquals(
                "oldTab should not have been unfocused.", 0, focusListener.getTimesUnfocused());
        Assert.assertTrue("oldTab should have focus.", focusListener.hasFocus());

        final Tab newTab =
                ChromeTabUtils.fullyLoadUrlInNewTab(
                        InstrumentationRegistry.getInstrumentation(),
                        sActivityTestRule.getActivity(),
                        "about:blank",
                        false);

        Assert.assertEquals(
                "oldTab should not have been focused.", 0, focusListener.getTimesFocused());
        Assert.assertEquals(
                "oldTab should have been unfocused.", 1, focusListener.getTimesUnfocused());
        Assert.assertFalse("oldTab should not have focus", focusListener.hasFocus());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    model.closeTabs(TabClosureParams.closeTab(newTab).build());
                });

        Assert.assertEquals("oldTab should have been focused.", 1, focusListener.getTimesFocused());
        Assert.assertEquals(
                "oldTab should not have been unfocused again.",
                1,
                focusListener.getTimesUnfocused());
        Assert.assertTrue("oldTab should have focus.", focusListener.hasFocus());

        // Focus on the URL bar.
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> urlBar.requestFocus());
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());

        Assert.assertEquals(
                "oldTab should not have been focused again.", 1, focusListener.getTimesFocused());
        Assert.assertEquals(
                "oldTab should have been unfocused by url bar.",
                2,
                focusListener.getTimesUnfocused());
        Assert.assertFalse("oldTab should not have focus.", focusListener.hasFocus());
        CriteriaHelper.pollUiThread(
                () -> {
                    boolean keyboardVisible =
                            sActivityTestRule
                                    .getKeyboardDelegate()
                                    .isKeyboardShowing(sActivityTestRule.getActivity(), urlBar);
                    Criteria.checkThat(keyboardVisible, Matchers.is(true));
                });

        // Check refocus doesn't happen again on the closure being finalized.
        ThreadUtils.runOnUiThreadBlocking(() -> model.commitAllTabClosures());

        Assert.assertEquals(
                "oldTab should not have been focused again after committing tab closures.",
                1,
                focusListener.getTimesFocused());
        Assert.assertEquals(
                "oldTab should not have been unfocused again after committing tab closures.",
                2,
                focusListener.getTimesUnfocused());
        Assert.assertFalse("oldTab should remain unfocused.", focusListener.hasFocus());

        CriteriaHelper.pollUiThread(
                () -> {
                    boolean keyboardVisible =
                            sActivityTestRule
                                    .getKeyboardDelegate()
                                    .isKeyboardShowing(sActivityTestRule.getActivity(), urlBar);
                    Criteria.checkThat(keyboardVisible, Matchers.is(true));
                });

        // Ensure the keyboard is hidden so we are in a clean-slate for next test.
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> urlBar.clearFocus());
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(() -> oldTab.getView().requestFocus());
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());

        CriteriaHelper.pollUiThread(
                () -> {
                    boolean keyboardVisible =
                            sActivityTestRule
                                    .getKeyboardDelegate()
                                    .isKeyboardShowing(sActivityTestRule.getActivity(), urlBar);
                    Criteria.checkThat(keyboardVisible, Matchers.is(false));
                });
    }

    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"Android-TabSwitcher"})
    public void testRequestFocusOnSwitchTab() {
        final TabModel model =
                sActivityTestRule.getActivity().getTabModelSelector().getCurrentModel();
        final Tab oldTab = TabModelUtils.getCurrentTab(model);

        Assert.assertNotNull("Tab should have a view", oldTab.getView());

        final FocusListener oldTabFocusListener = new FocusListener(oldTab.getView());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    oldTab.getView().setOnFocusChangeListener(oldTabFocusListener);
                });
        Assert.assertEquals(
                "oldTab should not have been focused.", 0, oldTabFocusListener.getTimesFocused());
        Assert.assertEquals(
                "oldTab should not have been unfocused.",
                0,
                oldTabFocusListener.getTimesUnfocused());
        Assert.assertTrue("oldTab should have focus.", oldTabFocusListener.hasFocus());

        final Tab newTab =
                ChromeTabUtils.fullyLoadUrlInNewTab(
                        InstrumentationRegistry.getInstrumentation(),
                        sActivityTestRule.getActivity(),
                        "about:blank",
                        false);
        final FocusListener newTabFocusListener = new FocusListener(newTab.getView());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    newTab.getView().setOnFocusChangeListener(newTabFocusListener);
                });
        Assert.assertEquals(
                "newTab should not have been focused.", 0, newTabFocusListener.getTimesFocused());
        Assert.assertEquals(
                "newTab should not have been unfocused.",
                0,
                newTabFocusListener.getTimesUnfocused());
        Assert.assertTrue("newTab should have focus.", newTabFocusListener.hasFocus());
        Assert.assertEquals(
                "oldTab should not have been focused.", 0, oldTabFocusListener.getTimesFocused());
        Assert.assertEquals(
                "oldTab should have been unfocused.", 1, oldTabFocusListener.getTimesUnfocused());
        Assert.assertFalse("oldTab should not have focus.", oldTabFocusListener.hasFocus());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    model.setIndex(model.indexOf(oldTab), TabSelectionType.FROM_USER);
                });

        Assert.assertEquals(
                "newTab should not have been focused.", 0, newTabFocusListener.getTimesFocused());
        Assert.assertEquals(
                "newTab should have been unfocused.", 1, newTabFocusListener.getTimesUnfocused());
        Assert.assertFalse("newTab should not have focus.", newTabFocusListener.hasFocus());
        Assert.assertEquals(
                "oldTab should have been focused.", 1, oldTabFocusListener.getTimesFocused());
        Assert.assertEquals(
                "oldTab should not have been unfocused again.",
                1,
                oldTabFocusListener.getTimesUnfocused());
        Assert.assertTrue("oldTab should have focus.", oldTabFocusListener.hasFocus());
    }

    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    public void testLastClosedTabTriggersNotifyChangedCall() {
        final TabModel model =
                sActivityTestRule.getActivity().getTabModelSelector().getCurrentModel();
        final Tab tab = TabModelUtils.getCurrentTab(model);
        final TabModelSelector selector = sActivityTestRule.getActivity().getTabModelSelector();
        mNotifyChangedCalled = false;

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.addObserver(
                            new TabModelSelectorObserver() {
                                @Override
                                public void onChange() {
                                    mNotifyChangedCalled = true;
                                }
                            });
                });

        Assert.assertEquals("Too many tabs at startup", 1, model.getCount());

        ThreadUtils.runOnUiThreadBlocking(
                (Runnable) () -> model.closeTabs(TabClosureParams.closeTab(tab).build()));

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

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    @SuppressWarnings("unused") // Avoid GC of observer
                    WebContentsObserver observer =
                            new WebContentsObserver(tab.getWebContents()) {
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

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertNull("Tab still has a web contents", tab.getWebContents());
                    Assert.assertFalse("Tab was not destroyed", tab.isInitialized());
                });

        webContentsDestroyed.waitForOnly();
    }

    @Test
    @MediumTest
    @Feature({"Android-TabSwitcher"})
    @DisableFeatures(ChromeFeatureList.ANDROID_TAB_DECLUTTER)
    @DisabledTest(message = "https://crbug.com/361535551")
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
        File normalTabFile =
                new File(
                        tabStateDir,
                        TabStateFileManager.getTabStateFilename(
                                normalModel.getTabAt(normalModel.getCount() - 1).getId(),
                                false,
                                false));
        File incognitoTabFile =
                new File(
                        tabStateDir,
                        TabStateFileManager.getTabStateFilename(
                                incognitoModel.getTabAt(0).getId(),
                                true,
                                /* isFlatBuffer= */ false));

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

    @Test
    @MediumTest
    public void testTabModelSelectorCloseTabInUndoableState() {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), sActivityTestRule.getActivity());
        TabModelSelectorImpl selector =
                (TabModelSelectorImpl) sActivityTestRule.getActivity().getTabModelSelector();
        Tab tab = sActivityTestRule.getActivity().getActivityTab();

        // Start undoable tab closure.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertFalse(tab.isClosing());
                    Assert.assertFalse(tab.isDestroyed());

                    selector.getModel(/* incognito= */ false)
                            .closeTabs(TabClosureParams.closeTab(tab).allowUndo(true).build());
                    Assert.assertTrue(tab.isClosing());
                    Assert.assertFalse(tab.isDestroyed());
                });

        // Later something calls `TabModelSelector#closeTab`.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(tab.isClosing());
                    Assert.assertFalse(tab.isDestroyed());

                    // Prior to fixing crbug.com/40067160 this would assert as the tab could not be
                    // found in
                    // any model as it was in the undoable tab closure state.
                    selector.closeTab(tab);
                    Assert.assertTrue(tab.isClosing());
                    Assert.assertTrue(tab.isDestroyed());
                });
    }

    private void assertFileExists(final File fileToCheck, final boolean expected) {
        CriteriaHelper.pollInstrumentationThread(
                () -> Criteria.checkThat(fileToCheck.exists(), Matchers.is(expected)));
    }

    private JavascriptTabModalDialog getCurrentAlertDialog() {
        return (JavascriptTabModalDialog)
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            PropertyModel dialogModel =
                                    sActivityTestRule
                                            .getActivity()
                                            .getModalDialogManager()
                                            .getCurrentDialogForTest();
                            return dialogModel != null
                                    ? dialogModel.get(ModalDialogProperties.CONTROLLER)
                                    : null;
                        });
    }
}
