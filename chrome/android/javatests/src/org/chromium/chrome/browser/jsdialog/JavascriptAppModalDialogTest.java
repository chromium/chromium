// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.jsdialog;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isChecked;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags.Add;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.components.javascript_dialogs.JavascriptAppModalDialog;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.test.util.WebContentsUtils;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Test suite for displaying and functioning of app modal JavaScript onbeforeunload dialogs.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(JavascriptAppModalDialogTest.JAVASCRIPT_DIALOG_BATCH_NAME)
@Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class JavascriptAppModalDialogTest {
    public static final String JAVASCRIPT_DIALOG_BATCH_NAME = "javascript_dialog";

    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, true);

    private static final String TAG = "JSAppModalDialogTest";
    private static final String EMPTY_PAGE = UrlUtils.encodeHtmlDataUri(
            "<html><title>Modal Dialog Test</title><p>Testcase.</p></title></html>");
    private static final String BEFORE_UNLOAD_URL = UrlUtils.encodeHtmlDataUri("<html>"
            + "<head><script>window.onbeforeunload=function() {"
            + "return 'Are you sure?';"
            + "};</script></head></html>");

    @Before
    public void setUp() {
        sActivityTestRule.loadUrl(EMPTY_PAGE);
    }

    /**
     * Verifies beforeunload dialogs are shown and they block/allow navigation
     * as appropriate.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1295498")
    @Feature({"Browser", "Main"})
    public void testBeforeUnloadDialog() throws TimeoutException, ExecutionException {
        sActivityTestRule.loadUrl(BEFORE_UNLOAD_URL);
        // JavaScript onbeforeunload dialogs require a user gesture.
        tapViewAndWait();
        executeJavaScriptAndWaitForDialog("history.back();");

        // Click cancel and verify that the url is the same.
        JavascriptAppModalDialog jsDialog = getCurrentDialog();
        Assert.assertNotNull("No dialog showing.", jsDialog);
        onView(withText(R.string.cancel)).perform(click());

        Assert.assertEquals(BEFORE_UNLOAD_URL,
                sActivityTestRule.getActivity()
                        .getCurrentWebContents()
                        .getLastCommittedUrl()
                        .getSpec());
        executeJavaScriptAndWaitForDialog("history.back();");

        jsDialog = getCurrentDialog();
        Assert.assertNotNull("No dialog showing.", jsDialog);

        // Click leave and verify that the url is changed.
        final TestCallbackHelperContainer.OnPageFinishedHelper onPageLoaded =
                getActiveTabTestCallbackHelperContainer().getOnPageFinishedHelper();
        int callCount = onPageLoaded.getCallCount();
        onView(withText(R.string.leave)).perform(click());
        onPageLoaded.waitForCallback(callCount);
        Assert.assertEquals(EMPTY_PAGE,
                sActivityTestRule.getActivity()
                        .getCurrentWebContents()
                        .getLastCommittedUrl()
                        .getSpec());
    }

    /**
     * Verifies behavior when the tab that has an onBeforeUnload handler has no history stack
     * (pressing back should still show the dialog).
     *
     * Regression test for https://crbug.com/1055540
     */
    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1237639")
    @Feature({"Browser", "Main"})
    public void testBeforeUnloadDialogWithNoHistory() throws TimeoutException, ExecutionException {
        ChromeTabbedActivity activity = sActivityTestRule.getActivity();
        TabUiTestHelper.verifyTabModelTabCount(activity, 1, 0);
        sActivityTestRule.loadUrlInNewTab(BEFORE_UNLOAD_URL);
        TabUiTestHelper.verifyTabModelTabCount(activity, 2, 0);
        // JavaScript onbeforeunload dialogs require a user gesture.
        tapViewAndWait();
        TestThreadUtils.runOnUiThreadBlocking(() -> { activity.onBackPressed(); });
        assertJavascriptAppModalDialogShownState(true);

        // Click leave and verify that the tab is closed.
        JavascriptAppModalDialog jsDialog = getCurrentDialog();
        Assert.assertNotNull("No dialog showing.", jsDialog);
        onView(withText(R.string.leave)).perform(click());
        TabUiTestHelper.verifyTabModelTabCount(activity, 1, 0);
    }

    /**
     * Verifies that when showing a beforeunload dialogs as a result of a page
     * reload, the correct UI strings are used.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1295498")
    @Feature({"Browser", "Main"})
    public void testBeforeUnloadOnReloadDialog() throws TimeoutException, ExecutionException {
        sActivityTestRule.loadUrl(BEFORE_UNLOAD_URL);
        // JavaScript onbeforeunload dialogs require a user gesture.
        tapViewAndWait();
        executeJavaScriptAndWaitForDialog("window.location.reload();");

        JavascriptAppModalDialog jsDialog = getCurrentDialog();
        Assert.assertNotNull("No dialog showing.", jsDialog);

        onView(withText(R.string.cancel)).check(matches(isDisplayed()));
        onView(withText(R.string.reload)).check(matches(isDisplayed()));
    }

    /**
     * Verifies that repeated dialogs give the option to disable dialogs
     * altogether and then that disabling them works.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Main"})
    @DisabledTest(message = "https://crbug.com/1299944")
    public void testDisableRepeatedDialogs() throws TimeoutException, ExecutionException {
        sActivityTestRule.loadUrl(BEFORE_UNLOAD_URL);
        // JavaScript onbeforeunload dialogs require a user gesture.
        tapViewAndWait();
        executeJavaScriptAndWaitForDialog("history.back();");

        // Show a dialog once.
        JavascriptAppModalDialog jsDialog = getCurrentDialog();
        Assert.assertNotNull("No dialog showing.", jsDialog);
        onView(withText(R.string.cancel)).perform(click());
        Assert.assertEquals(BEFORE_UNLOAD_URL,
                sActivityTestRule.getActivity()
                        .getCurrentWebContents()
                        .getLastCommittedUrl()
                        .getSpec());

        // Show it again, it should have the option to suppress subsequent dialogs.
        OnEvaluateJavaScriptResultHelper resultHelper =
                executeJavaScriptAndWaitForDialog("history.back();");
        jsDialog = getCurrentDialog();
        Assert.assertNotNull("No dialog showing.", jsDialog);
        onView(withId(R.id.suppress_js_modal_dialogs))
                .check(matches(isDisplayed()))
                .perform(click())
                .check(matches(isChecked()));
        onView(withText(R.string.cancel)).perform(click());
        Assert.assertEquals(BEFORE_UNLOAD_URL,
                sActivityTestRule.getActivity()
                        .getCurrentWebContents()
                        .getLastCommittedUrl()
                        .getSpec());

        // Try showing a dialog again and verify it is not shown.
        resultHelper.evaluateJavaScriptForTests(
                sActivityTestRule.getWebContents(), "history.back();");
        jsDialog = getCurrentDialog();
        Assert.assertNull("Dialog should not be showing.", jsDialog);
    }

    /**
     * Displays a dialog and closes the tab in the background before attempting
     * to accept the dialog. Verifies that the dialog is dismissed when the tab
     * is closed.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Main"})
    public void testDialogDismissedAfterClosingTab() throws TimeoutException {
        sActivityTestRule.loadUrl(BEFORE_UNLOAD_URL);
        // JavaScript onbeforeunload dialogs require a user gesture.
        tapViewAndWait();
        executeJavaScriptAndWaitForDialog("history.back();");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeTabbedActivity activity = sActivityTestRule.getActivity();
            activity.getCurrentTabModel().closeTab(activity.getActivityTab());
        });

        // Closing the tab should have dismissed the dialog.
        assertJavascriptAppModalDialogShownState(false);
    }

    /**
     * Taps on a view and waits for a callback.
     */
    private void tapViewAndWait() throws TimeoutException {
        final TapGestureStateListener tapGestureStateListener = new TapGestureStateListener();
        int callCount = tapGestureStateListener.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            WebContentsUtils.getGestureListenerManager(sActivityTestRule.getWebContents())
                    .addListener(tapGestureStateListener);
        });
        TouchCommon.singleClickView(sActivityTestRule.getActivity().getActivityTab().getView());
        tapGestureStateListener.waitForTap(callCount);
    }

    /**
     * Asynchronously executes the given code for spawning a dialog and waits
     * for the dialog to be visible.
     */
    private OnEvaluateJavaScriptResultHelper executeJavaScriptAndWaitForDialog(String script) {
        return executeJavaScriptAndWaitForDialog(new OnEvaluateJavaScriptResultHelper(), script);
    }

    /**
     * Given a JavaScript evaluation helper, asynchronously executes the given
     * code for spawning a dialog and waits for the dialog to be visible.
     */
    private OnEvaluateJavaScriptResultHelper executeJavaScriptAndWaitForDialog(
            final OnEvaluateJavaScriptResultHelper helper, String script) {
        helper.evaluateJavaScriptForTests(
                sActivityTestRule.getActivity().getCurrentWebContents(), script);
        assertJavascriptAppModalDialogShownState(true);
        return helper;
    }

    /**
     * Returns the current JavaScript modal dialog showing or null if no such dialog is currently
     * showing.
     */
    private JavascriptAppModalDialog getCurrentDialog() throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(
                () -> JavascriptAppModalDialog.getCurrentDialogForTest());
    }

    private static class TapGestureStateListener extends GestureStateListener {
        private CallbackHelper mCallbackHelper = new CallbackHelper();

        public int getCallCount() {
            return mCallbackHelper.getCallCount();
        }

        public void waitForTap(int currentCallCount) throws TimeoutException {
            mCallbackHelper.waitForCallback(currentCallCount);
        }

        @Override
        public void onSingleTap(boolean consumed) {
            mCallbackHelper.notifyCalled();
        }
    }

    private void assertJavascriptAppModalDialogShownState(boolean shouldBeShown) {
        CriteriaHelper.pollUiThread(() -> {
            JavascriptAppModalDialog dialog = JavascriptAppModalDialog.getCurrentDialogForTest();
            if (shouldBeShown) {
                Criteria.checkThat("Could not spawn or locate a modal dialog.", dialog,
                        Matchers.notNullValue());
            } else {
                Criteria.checkThat("No dialog should be shown.", dialog, Matchers.nullValue());
            }
        });
    }

    private TestCallbackHelperContainer getActiveTabTestCallbackHelperContainer() {
        return new TestCallbackHelperContainer(sActivityTestRule.getWebContents());
    }
}
