// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withTagValue;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.is;

import androidx.annotation.IdRes;
import androidx.annotation.IntDef;

import org.hamcrest.Matchers;
import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.InfoBarTestAnimationListener;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.components.browser_ui.modaldialog.ModalDialogTestUtils;
import org.chromium.components.browser_ui.modaldialog.ModalDialogView;
import org.chromium.components.infobars.InfoBar;
import org.chromium.components.permissions.PermissionDialogController;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.test.util.ViewUtils;

/**
 * TestRule for permissions UI testing on Android.
 *
 * <p>This class allows for easy testing of permissions infobar and dialog prompts. Writing a test
 * simply requires a HTML file containing JavaScript methods which trigger a permission prompt. The
 * methods should update the page's title with <prefix>: <count>, where <count> is the number of
 * updates expected (usually 1, although some APIs like Geolocation's watchPosition may trigger
 * callbacks repeatedly).
 *
 * <p>Subclasses may then call runAllowTest to start a test server, navigate to the provided HTML
 * page, and run the JavaScript method. The permission will be granted, and the test will verify
 * that the page title is updated as expected.
 *
 * <p>runAllowTest has several parameters to specify the conditions of the test, including whether a
 * persistence toggle is expected, whether it should be explicitly toggled, whether to trigger the
 * JS call with a gesture, and whether an infobar or a dialog is expected.
 */
public class PermissionTestRule extends ChromeTabbedActivityTestRule {
    private InfoBarTestAnimationListener mListener;

    @IntDef({
        PromptDecision.ALLOW,
        PromptDecision.ALLOW_ONCE,
        PromptDecision.DENY,
        PromptDecision.NONE
    })
    public @interface PromptDecision {
        int ALLOW = 0;
        int ALLOW_ONCE = 1;
        int DENY = 2;
        int NONE = 3;
    }

    /**
     * Waits till a JavaScript callback which updates the page title is called the specified number
     * of times. The page title is expected to be of the form <prefix>: <count>.
     */
    public static class PermissionUpdateWaiter extends EmptyTabObserver {
        private CallbackHelper mCallbackHelper;
        private String mPrefix;
        private String mExpectedTitle;
        private final ChromeActivity mActivity;

        public PermissionUpdateWaiter(String prefix, ChromeActivity activity) {
            mCallbackHelper = new CallbackHelper();
            mPrefix = prefix;
            mActivity = activity;
        }

        @Override
        public void onTitleUpdated(Tab tab) {
            if (ChromeTabUtils.getTitleOnUiThread(mActivity.getActivityTab())
                    .equals(mExpectedTitle)) {
                mCallbackHelper.notifyCalled();
            }
        }

        /**
         * Wait for the page title to reach the expected number of updates. The page is expected to
         * update the title like so: `prefix` + numUpdates. In essence this waits for the page to
         * update the title to match, and does not actually count page title updates.
         *
         * @param numUpdates The number that should be after the prefix for the wait to be over. `0`
         *     to only wait for the prefix.
         */
        public void waitForNumUpdates(int numUpdates) throws Exception {
            int callbackCountBefore = mCallbackHelper.getCallCount();

            // Update might have already happened, check before waiting for title udpdates.
            mExpectedTitle = mPrefix;
            if (numUpdates != 0) mExpectedTitle += numUpdates;
            if (ChromeTabUtils.getTitleOnUiThread(mActivity.getActivityTab())
                    .equals(mExpectedTitle)) {
                return;
            }

            mCallbackHelper.waitForCallback(callbackCountBefore);
        }
    }

    public PermissionTestRule() {
        this(false);
    }

    public PermissionTestRule(boolean useHttpsServer) {
        getEmbeddedTestServerRule().setServerUsesHttps(useHttpsServer);
    }

    @Override
    protected void before() throws Throwable {
        super.before();
        ModalDialogView.disableButtonTapProtectionForTesting();
    }

    /** Starts an activity and listens for info-bars appearing/disappearing. */
    public void setUpActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
        mListener = new InfoBarTestAnimationListener();
        ThreadUtils.runOnUiThreadBlocking(
                () -> getInfoBarContainer().addAnimationListener(mListener));
    }

    /**
     * Navigates to a relative URL in relation to the embedded server host directly without going
     * through the UrlBar. This bypasses the page preloading mechanism of the UrlBar.
     *
     * @param relativeUrl The relative URL for which an absolute URL will be computed and loaded in
     *     the current tab.
     */
    public void setUpUrl(final String relativeUrl) {
        loadUrl(getURL(relativeUrl));
    }

    /**
     * Navigates to a relative URL in relation to the specified host directly without going through
     * the UrlBar. This bypasses the page preloading mechanism of the UrlBar.
     *
     * @param relativeUrl The relative URL for which an absolute URL will be computed and loaded in
     *     the current tab.
     * @param hostName The host name which should be used.
     */
    public void setupUrlWithHostName(String hostName, String relativeUrl) {
        loadUrl(getURLWithHostName(hostName, relativeUrl));
    }

    public String getURL(String url) {
        return getTestServer().getURL(url);
    }

    public String getOrigin() {
        return getTestServer().getURL("/");
    }

    public String getURLWithHostName(String hostName, String url) {
        return getTestServer().getURLWithHostName(hostName, url);
    }

    /**
     * Runs a permission prompt test that grants the permission and expects the page title to be
     * updated in response.
     *
     * @param updateWaiter The update waiter to wait for callbacks. Should be added as an observer
     *     to the current tab prior to calling this method.
     * @param javascript The JS function to run in the current tab to execute the test and update
     *     the page title.
     * @param nUpdates How many updates of the page title to wait for.
     * @param withGesture True if we require a user gesture to trigger the prompt.
     * @param isDialog True if we are expecting a permission dialog, false for an infobar.
     */
    public void runAllowTest(
            PermissionUpdateWaiter updateWaiter,
            final String url,
            String javascript,
            int nUpdates,
            boolean withGesture,
            boolean isDialog)
            throws Exception {
        setUpUrl(url);
        if (withGesture) {
            runJavaScriptCodeInCurrentTabWithGesture(javascript);
        } else {
            runJavaScriptCodeInCurrentTab(javascript);
        }
        replyToPromptAndWaitForUpdates(updateWaiter, PromptDecision.ALLOW, nUpdates, isDialog);
    }

    /**
     * Runs a permission prompt test that does not grant the permission and expects the page title
     * to be updated in response.
     *
     * @param updateWaiter The update waiter to wait for callbacks. Should be added as an observer
     *     to the current tab prior to calling this method.
     * @param javascript The JS function to run in the current tab to execute the test and update
     *     the page title.
     * @param nUpdates How many updates of the page title to wait for.
     * @param withGesture True if we require a user gesture to trigger the prompt.
     * @param isDialog True if we are expecting a permission dialog, false for an infobar.
     */
    public void runDenyTest(
            PermissionUpdateWaiter updateWaiter,
            final String url,
            String javascript,
            int nUpdates,
            boolean withGesture,
            boolean isDialog)
            throws Exception {
        setUpUrl(url);
        if (withGesture) {
            runJavaScriptCodeInCurrentTabWithGesture(javascript);
        } else {
            runJavaScriptCodeInCurrentTab(javascript);
        }
        replyToPromptAndWaitForUpdates(updateWaiter, PromptDecision.DENY, nUpdates, isDialog);
    }

    /**
     * Runs a permission prompt test that expects no prompt to be displayed because the permission
     * is already granted/blocked and expects the page title to be updated.
     *
     * @param updateWaiter The update waiter to wait for callbacks. Should be added as an observer
     *     to the current tab prior to calling this method.
     * @param javascript The JS function to run in the current tab to execute the test and update
     *     the page title.
     * @param nUpdates How many updates of the page title to wait for.
     * @param withGesture True if we require a user gesture.
     * @param isDialog True if we are testing a permission dialog, false for an infobar.
     */
    public void runNoPromptTest(
            PermissionUpdateWaiter updateWaiter,
            final String url,
            String javascript,
            int nUpdates,
            boolean withGesture,
            boolean isDialog)
            throws Exception {
        setUpUrl(url);
        if (withGesture) {
            runJavaScriptCodeInCurrentTabWithGesture(javascript);
        } else {
            runJavaScriptCodeInCurrentTab(javascript);
        }
        waitForUpdatesAndAssertNoPrompt(updateWaiter, nUpdates, isDialog);
    }

    private void replyToPromptAndWaitForUpdates(
            PermissionUpdateWaiter updateWaiter,
            @PromptDecision int decision,
            int nUpdates,
            boolean isDialog)
            throws Exception {
        if (isDialog) {
            waitForDialogShownState(true);
            replyToDialogAndWaitForUpdates(updateWaiter, nUpdates, decision);
        } else {
            replyToInfoBarAndWaitForUpdates(updateWaiter, nUpdates, decision);
        }
    }

    private void waitForUpdatesAndAssertNoPrompt(
            PermissionUpdateWaiter updateWaiter, int nUpdates, boolean isDialog) throws Exception {
        updateWaiter.waitForNumUpdates(nUpdates);

        if (isDialog) {
            Assert.assertFalse(
                    "Modal permission prompt shown when none expected",
                    PermissionDialogController.getInstance().isDialogShownForTest());
        } else {
            Assert.assertEquals(
                    "Permission infobar shown when none expected", getInfoBars().size(), 0);
        }
    }

    public void runJavaScriptCodeInCurrentTabWithGesture(String javascript)
            throws java.util.concurrent.TimeoutException {
        runJavaScriptCodeInCurrentTab("functionToRun = '" + javascript + "'");
        TouchCommon.singleClickView(getActivity().getActivityTab().getView());
    }

    /**
     * Replies to an infobar permission prompt and waits for a provided number of updates to the
     * page title in response.
     */
    private void replyToInfoBarAndWaitForUpdates(
            PermissionUpdateWaiter updateWaiter, int nUpdates, @PromptDecision int decison)
            throws Exception {
        mListener.addInfoBarAnimationFinished("InfoBar not added.");
        InfoBar infobar = getInfoBars().get(0);
        Assert.assertNotNull(infobar);

        switch (decison) {
            case PromptDecision.ALLOW -> Assert.assertTrue(
                    "Allow button wasn't found", InfoBarUtil.clickPrimaryButton(infobar));
            case PromptDecision.ALLOW_ONCE -> throw new AssertionError(
                    "Allowing once is not supported on infobars.");
            case PromptDecision.DENY -> Assert.assertTrue(
                    "Block button wasn't found", InfoBarUtil.clickSecondaryButton(infobar));
        }
        updateWaiter.waitForNumUpdates(nUpdates);
    }

    /**
     * Replies to a dialog permission prompt and waits for a provided number of updates to the page
     * title in response.
     */
    private void replyToDialogAndWaitForUpdates(
            PermissionUpdateWaiter updateWaiter,
            int nUpdates,
            final @PermissionTestRule.PromptDecision int decison)
            throws Exception {
        replyToDialog(decison, getActivity());
        updateWaiter.waitForNumUpdates(nUpdates);
    }

    /** Verify the shown state of the dialog. */
    protected void waitForDialogShownState(boolean expectedShowState) {
        waitForDialogShownState(getActivity(), expectedShowState);
    }

    /** Utility functions to support permissions testing in other contexts. */
    public static void replyToDialog(
            final @PermissionTestRule.PromptDecision int decision, ChromeActivity activity) {
        // Wait for button view to appear in view hierarchy. If the browser controls are not visible
        // then ModalDialogPresenter will first trigger animation for showing browser controls and
        // only then add modal dialog view into the container.
        @IdRes
        int buttonId =
                switch (decision) {
                    case PromptDecision.ALLOW -> ModalDialogProperties.ButtonType.POSITIVE;
                    case PromptDecision.ALLOW_ONCE -> ModalDialogProperties.ButtonType
                            .POSITIVE_EPHEMERAL;
                    case PromptDecision.DENY -> ModalDialogProperties.ButtonType.NEGATIVE;
                    default -> throw new IllegalStateException("Unexpected value: " + decision);
                };

        ViewUtils.onViewWaiting(
                        allOf(
                                withTagValue(is(ModalDialogView.getTagForButtonType(buttonId))),
                                isDisplayed()))
                .perform(click());
    }

    /** Wait for the permission dialog to be in the expected shown state. */
    public static void waitForDialogShownState(ChromeActivity activity, boolean expectedShowState) {
        ModalDialogManager dialogManager =
                ThreadUtils.runOnUiThreadBlocking(activity::getModalDialogManager);
        CriteriaHelper.pollUiThread(
                () -> {
                    boolean isDialogShownForTest =
                            PermissionDialogController.getInstance().isDialogShownForTest();
                    if (isDialogShownForTest) {
                        ModalDialogTestUtils.checkCurrentPresenter(
                                dialogManager, ModalDialogType.TAB);
                    }
                    Criteria.checkThat(isDialogShownForTest, Matchers.is(expectedShowState));
                });
    }

    /** Wait for the permission dialog to be shown. */
    public static void waitForDialog(ChromeActivity activity) {
        waitForDialogShownState(activity, true);
    }
}
