// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.jsdialog;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isChecked;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Log;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags.Add;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
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
@RetryOnFailure
@Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class JavascriptAppModalDialogTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final String TAG = "JSAppModalDialogTest";
    private static final String EMPTY_PAGE = UrlUtils.encodeHtmlDataUri(
            "<html><title>Modal Dialog Test</title><p>Testcase.</p></title></html>");
    private static final String BEFORE_UNLOAD_URL = UrlUtils.encodeHtmlDataUri("<html>"
            + "<head><script>window.onbeforeunload=function() {"
            + "return 'Are you sure?';"
            + "};</script></head></html>");

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityWithURL(EMPTY_PAGE);
    }

    /**
     * Verifies beforeunload dialogs are shown and they block/allow navigation
     * as appropriate.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Main"})
    public void testBeforeUnloadDialog() throws TimeoutException, ExecutionException {
        mActivityTestRule.loadUrl(BEFORE_UNLOAD_URL);
        // JavaScript onbeforeunload dialogs require a user gesture.
        tapViewAndWait();
        executeJavaScriptAndWaitForDialog("history.back();");

        // Click cancel and verify that the url is the same.
        JavascriptAppModalDialog jsDialog = getCurrentDialog();
        Assert.assertNotNull("No dialog showing.", jsDialog);
        onView(withText(R.string.cancel)).perform(click());

        Assert.assertEquals(BEFORE_UNLOAD_URL,
                mActivityTestRule.getActivity().getCurrentWebContents().getLastCommittedUrl());
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
                mActivityTestRule.getActivity().getCurrentWebContents().getLastCommittedUrl());
    }

    /**
     * Verifies that when showing a beforeunload dialogs as a result of a page
     * reload, the correct UI strings are used.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Main"})
    public void testBeforeUnloadOnReloadDialog() throws TimeoutException, ExecutionException {
        mActivityTestRule.loadUrl(BEFORE_UNLOAD_URL);
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
    public void testDisableRepeatedDialogs() throws TimeoutException, ExecutionException {
        mActivityTestRule.loadUrl(BEFORE_UNLOAD_URL);
        // JavaScript onbeforeunload dialogs require a user gesture.
        tapViewAndWait();
        executeJavaScriptAndWaitForDialog("history.back();");

        // Show a dialog once.
        JavascriptAppModalDialog jsDialog = getCurrentDialog();
        Assert.assertNotNull("No dialog showing.", jsDialog);
        onView(withText(R.string.cancel)).perform(click());
        Assert.assertEquals(BEFORE_UNLOAD_URL,
                mActivityTestRule.getActivity().getCurrentWebContents().getLastCommittedUrl());

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
                mActivityTestRule.getActivity().getCurrentWebContents().getLastCommittedUrl());

        // Try showing a dialog again and verify it is not shown.
        resultHelper.evaluateJavaScriptForTests(
                mActivityTestRule.getWebContents(), "history.back();");
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
        mActivityTestRule.loadUrl(BEFORE_UNLOAD_URL);
        // JavaScript onbeforeunload dialogs require a user gesture.
        tapViewAndWait();
        executeJavaScriptAndWaitForDialog("history.back();");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeActivity activity = mActivityTestRule.getActivity();
            activity.getCurrentTabModel().closeTab(activity.getActivityTab());
        });

        // Closing the tab should have dismissed the dialog.
        CriteriaHelper.pollInstrumentationThread(new JavascriptAppModalDialogShownCriteria(
                "The dialog should have been dismissed when its tab was closed.", false));
    }

    /**
     * Taps on a view and waits for a callback.
     */
    private void tapViewAndWait() throws TimeoutException {
        final TapGestureStateListener tapGestureStateListener = new TapGestureStateListener();
        int callCount = tapGestureStateListener.getCallCount();
        WebContentsUtils.getGestureListenerManager(mActivityTestRule.getWebContents())
                .addListener(tapGestureStateListener);

        TouchCommon.singleClickView(mActivityTestRule.getActivity().getActivityTab().getView());
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
                mActivityTestRule.getActivity().getCurrentWebContents(), script);
        CriteriaHelper.pollInstrumentationThread(new JavascriptAppModalDialogShownCriteria(
                "Could not spawn or locate a modal dialog.", true));
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

    private static class TapGestureStateListener implements GestureStateListener {
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

    private static class JavascriptAppModalDialogShownCriteria extends Criteria {
        private final boolean mShouldBeShown;

        public JavascriptAppModalDialogShownCriteria(String error, boolean shouldBeShown) {
            super(error);
            mShouldBeShown = shouldBeShown;
        }

        @Override
        public boolean isSatisfied() {
            try {
                return TestThreadUtils.runOnUiThreadBlocking(() -> {
                    final boolean isShown =
                            JavascriptAppModalDialog.getCurrentDialogForTest() != null;
                    return mShouldBeShown == isShown;
                });
            } catch (ExecutionException e) {
                Log.e(TAG, "Failed to getCurrentDialog", e);
                return false;
            }
        }
    }

    private TestCallbackHelperContainer getActiveTabTestCallbackHelperContainer() {
        return new TestCallbackHelperContainer(mActivityTestRule.getWebContents());
    }
}
