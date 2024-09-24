// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.jsdialog;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isFocusable;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.content.pm.ActivityInfo;

import androidx.test.espresso.Espresso;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.components.javascript_dialogs.JavascriptTabModalDialog;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/** Test suite for displaying and functioning of tab modal JavaScript alert, confirm and prompt. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(JavascriptAppModalDialogTest.JAVASCRIPT_DIALOG_BATCH_NAME)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class JavascriptTabModalDialogTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, true);

    private static final String EMPTY_PAGE =
            UrlUtils.encodeHtmlDataUri(
                    "<html><title>Modal Dialog Test</title><p>Testcase.</p></title></html>");
    private static final String OTHER_PAGE =
            UrlUtils.encodeHtmlDataUri(
                    "<html><title>Modal Dialog Test</title><p>Testcase. Other"
                            + " tab.</p></title></html>");

    private ChromeTabbedActivity mActivity;

    @Before
    public void setUp() {
        sActivityTestRule.loadUrl(EMPTY_PAGE);
        mActivity = sActivityTestRule.getActivity();
    }

    /**
     * Verifies modal alert-dialog appearance and that JavaScript execution is able to continue
     * after dismissal.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Main"})
    public void testAlertModalDialog() throws TimeoutException, ExecutionException {
        final OnEvaluateJavaScriptResultHelper scriptEvent =
                executeJavaScriptAndWaitForDialog("alert('Hello Android!');");

        JavascriptTabModalDialog jsDialog = getCurrentDialog();
        Assert.assertNotNull("No dialog showing.", jsDialog);

        onView(withText(R.string.ok)).perform(click());
        Assert.assertTrue(
                "JavaScript execution should continue after closing prompt.",
                scriptEvent.waitUntilHasValue());
    }

    /** Verifies that clicking on a button twice doesn't crash. */
    @Test
    @MediumTest
    @Feature({"Browser", "Main"})
    public void testAlertModalDialogWithTwoClicks() throws TimeoutException, ExecutionException {
        OnEvaluateJavaScriptResultHelper scriptEvent =
                executeJavaScriptAndWaitForDialog("alert('Hello Android');");
        JavascriptTabModalDialog jsDialog = getCurrentDialog();
        Assert.assertNotNull("No dialog showing.", jsDialog);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel model =
                            mActivity.getModalDialogManager().getCurrentDialogForTest();
                    jsDialog.onClick(model, ModalDialogProperties.ButtonType.POSITIVE);
                    jsDialog.onClick(model, ModalDialogProperties.ButtonType.POSITIVE);
                });

        Assert.assertTrue(
                "JavaScript execution should continue after closing prompt.",
                scriptEvent.waitUntilHasValue());
    }

    /**
     * Verifies that modal confirm-dialogs display, two buttons are visible and the return value of
     * [Ok] equals true, [Cancel] equals false.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Main"})
    public void testConfirmModalDialog() throws TimeoutException, ExecutionException {
        OnEvaluateJavaScriptResultHelper scriptEvent =
                executeJavaScriptAndWaitForDialog("confirm('Android');");

        JavascriptTabModalDialog jsDialog = getCurrentDialog();
        Assert.assertNotNull("No dialog showing.", jsDialog);

        onView(withText(R.string.ok)).check(matches(isDisplayed()));
        onView(withText(R.string.cancel)).check(matches(isDisplayed()));

        onView(withText(R.string.ok)).perform(click());
        Assert.assertTrue(
                "JavaScript execution should continue after closing dialog.",
                scriptEvent.waitUntilHasValue());

        String resultString = scriptEvent.getJsonResultAndClear();
        Assert.assertEquals("Invalid return value.", "true", resultString);

        // Try again, pressing cancel this time.
        scriptEvent = executeJavaScriptAndWaitForDialog("confirm('Android');");
        jsDialog = getCurrentDialog();
        Assert.assertNotNull("No dialog showing.", jsDialog);

        onView(withText(R.string.cancel)).perform(click());
        Assert.assertTrue(
                "JavaScript execution should continue after closing dialog.",
                scriptEvent.waitUntilHasValue());

        resultString = scriptEvent.getJsonResultAndClear();
        Assert.assertEquals("Invalid return value.", "false", resultString);
    }

    /** Verifies that modal prompt-dialogs display and the result is returned. */
    @Test
    @MediumTest
    @Feature({"Browser", "Main"})
    public void testPromptModalDialog() throws TimeoutException, ExecutionException {
        final String promptText = "Hello Android!";
        final OnEvaluateJavaScriptResultHelper scriptEvent =
                executeJavaScriptAndWaitForDialog("prompt('Android', 'default');");

        final JavascriptTabModalDialog jsDialog = getCurrentDialog();
        Assert.assertNotNull("No dialog showing.", jsDialog);

        // Set the text in the prompt field of the dialog.
        onView(withId(R.id.js_modal_dialog_prompt)).perform(replaceText(promptText));

        onView(withText(R.string.ok)).perform(click());
        Assert.assertTrue(
                "JavaScript execution should continue after closing prompt.",
                scriptEvent.waitUntilHasValue());

        String resultString = scriptEvent.getJsonResultAndClear();
        Assert.assertEquals("Invalid return value.", '"' + promptText + '"', resultString);
    }

    /**
     * Verifies that message content in a dialog is only focusable if the message itself is long
     * enough to require scrolling.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Main"})
    public void testAlertModalDialogMessageFocus() throws TimeoutException, ExecutionException {
        assertScrollViewFocusabilityInAlertDialog("alert('Short message!');", false);

        // Test on landscape mode so that the message is long enough to make scroll view scrollable
        // on a large-screen device.
        mActivity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        assertScrollViewFocusabilityInAlertDialog(
                "alert(new Array(200).join('Long message!'));", true);

        // Reset to portrait mode.
        mActivity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
    }

    private void assertScrollViewFocusabilityInAlertDialog(
            final String jsAlertScript, final boolean expectedFocusability)
            throws TimeoutException, ExecutionException {
        final OnEvaluateJavaScriptResultHelper scriptEvent =
                executeJavaScriptAndWaitForDialog(jsAlertScript);

        final JavascriptTabModalDialog jsDialog = getCurrentDialog();
        Assert.assertNotNull("No dialog showing.", jsDialog);

        onView(withId(R.id.modal_dialog_title_scroll_view))
                .check(matches(expectedFocusability ? isFocusable() : not(isFocusable())));

        onView(withText(R.string.ok)).perform(click());
        Assert.assertTrue(
                "JavaScript execution should continue after closing prompt.",
                scriptEvent.waitUntilHasValue());
    }

    /**
     * Displays a dialog and closes the tab in the background before attempting to accept the
     * dialog. Verifies that the dialog is dismissed when the tab is closed.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Main"})
    public void testDialogDismissedAfterClosingTab() {
        executeJavaScriptAndWaitForDialog("alert('Android')");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivity
                            .getCurrentTabModel()
                            .closeTabs(
                                    TabClosureParams.closeTab(mActivity.getActivityTab())
                                            .allowUndo(false)
                                            .build());
                });

        // Closing the tab should have dismissed the dialog.
        checkDialogShowing("The dialog should have been dismissed when its tab was closed.", false);
    }

    /**
     * Displays a dialog and goes to tab switcher in the before attempting to accept or cancel the
     * dialog. Verifies that the dialog is dismissed.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Main"})
    @Restriction(DeviceFormFactor.PHONE)
    public void testDialogDismissedAfterToggleOverview() {
        executeJavaScriptAndWaitForDialog("alert('Android')");

        onViewWaiting(withId(R.id.tab_switcher_button)).perform(click());

        // Entering tab switcher should have dismissed the dialog.
        checkDialogShowing(
                "The dialog should have been dismissed when switching to overview mode.", false);
    }

    /**
     * Displays a dialog and loads a new URL before attempting to accept or cancel the dialog.
     * Verifies that the dialog is dismissed.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Main"})
    public void testDialogDismissedAfterUrlUpdated() {
        executeJavaScriptAndWaitForDialog("alert('Android')");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivity
                            .getActivityTab()
                            .loadUrl(new LoadUrlParams(OTHER_PAGE, PageTransition.LINK));
                });

        // Loading a different URL should have dismissed the dialog.
        checkDialogShowing(
                "The dialog should have been dismissed when a new url is loaded.", false);
    }

    /**
     * Displays a dialog and performs back press before attempting to accept or cancel the dialog.
     * Verifies that the dialog is dismissed.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Main"})
    public void testDialogDismissedAfterBackPressed() {
        executeJavaScriptAndWaitForDialog("alert('Android')");

        Espresso.pressBack();

        // Performing back press should have dismissed the dialog.
        checkDialogShowing("The dialog should have been dismissed after back press.", false);
    }

    /**
     * Asynchronously executes the given code for spawning a dialog and waits for the dialog to be
     * visible.
     */
    private OnEvaluateJavaScriptResultHelper executeJavaScriptAndWaitForDialog(String script) {
        return executeJavaScriptAndWaitForDialog(new OnEvaluateJavaScriptResultHelper(), script);
    }

    /**
     * Given a JavaScript evaluation helper, asynchronously executes the given code for spawning a
     * dialog and waits for the dialog to be visible.
     */
    private OnEvaluateJavaScriptResultHelper executeJavaScriptAndWaitForDialog(
            final OnEvaluateJavaScriptResultHelper helper, String script) {
        helper.evaluateJavaScriptForTests(mActivity.getCurrentWebContents(), script);
        checkDialogShowing("Could not spawn or locate a modal dialog.", true);
        return helper;
    }

    /**
     * Returns the current JavaScript modal dialog showing or null if no such dialog is currently
     * showing.
     */
    private JavascriptTabModalDialog getCurrentDialog() throws ExecutionException {
        return (JavascriptTabModalDialog)
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            PropertyModel model =
                                    mActivity.getModalDialogManager().getCurrentDialogForTest();
                            return model != null
                                    ? model.get(ModalDialogProperties.CONTROLLER)
                                    : null;
                        });
    }

    /** Check whether dialog is showing as expected. */
    private void checkDialogShowing(final String errorMessage, final boolean shouldBeShown) {
        CriteriaHelper.pollUiThread(
                () -> {
                    final boolean isShown = mActivity.getModalDialogManager().isShowing();
                    Criteria.checkThat(errorMessage, isShown, is(shouldBeShown));
                });
    }
}
