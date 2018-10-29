// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import android.content.DialogInterface;
import android.support.test.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.util.InfoBarTestAnimationListener;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/**
 * TestRule for permissions UI testing on Android.
 *
 * This class allows for easy testing of permissions infobar and dialog prompts. Writing a test
 * simply requires a HTML file containing JavaScript methods which trigger a permission prompt. The
 * methods should update the page's title with <prefix>: <count>, where <count> is the number of
 * updates expected (usually 1, although some APIs like Geolocation's watchPosition may trigger
 * callbacks repeatedly).
 *
 * Subclasses may then call runAllowTest to start a test server, navigate to the provided HTML page,
 * and run the JavaScript method. The permission will be granted, and the test will verify that the
 * page title is updated as expected.
 *
 * runAllowTest has several parameters to specify the conditions of the test, including whether
 * a persistence toggle is expected, whether it should be explicitly toggled, whether to trigger the
 * JS call with a gesture, and whether an infobar or a dialog is expected.
 */
public class PermissionTestRule extends ChromeActivityTestRule<ChromeActivity> {
    public static final String MODAL_FLAG = ChromeFeatureList.MODAL_PERMISSION_PROMPTS;
    public static final String TOGGLE_FLAG = "DisplayPersistenceToggleInPermissionPrompts";
    public static final String MODAL_TOGGLE_FLAG = MODAL_FLAG + "," + TOGGLE_FLAG;
    public static final String PERMISSION_REQUEST_MANAGER_FLAG = "UseGroupedPermissionInfobars";

    private InfoBarTestAnimationListener mListener;
    private EmbeddedTestServer mTestServer;

    /**
     * Waits till a JavaScript callback which updates the page title is called the specified number
     * of times. The page title is expected to be of the form <prefix>: <count>.
     */
    public static class PermissionUpdateWaiter extends EmptyTabObserver {
        private CallbackHelper mCallbackHelper;
        private String mPrefix;
        private int mExpectedCount;
        private final ChromeActivity mActivity;

        public PermissionUpdateWaiter(String prefix, ChromeActivity activity) {
            mCallbackHelper = new CallbackHelper();
            mPrefix = prefix;
            mActivity = activity;
        }

        @Override
        public void onTitleUpdated(Tab tab) {
            String expectedTitle = mPrefix + mExpectedCount;
            if (mActivity.getActivityTab().getTitle().equals(expectedTitle)) {
                mCallbackHelper.notifyCalled();
            }
        }

        public void waitForNumUpdates(int numUpdates) throws Exception {
            mExpectedCount = numUpdates;
            mCallbackHelper.waitForCallback(0);
        }
    }

    @Override
    public Statement apply(final Statement base, Description desc) {
        return super.apply(new Statement() {
            @Override
            public void evaluate() throws Throwable {
                ruleSetUp();
                base.evaluate();
                ruleTearDown();
            }
        }, desc);
    }

    /**
     * Criteria class to detect whether the permission dialog is shown.
     */
    protected static class DialogShownCriteria extends Criteria {
        private PermissionDialogView mDialog;
        private boolean mExpectDialog;

        public DialogShownCriteria(String error, boolean expectDialog) {
            super(error);
            mExpectDialog = expectDialog;
        }

        public PermissionDialogView getDialog() {
            return mDialog;
        }

        @Override
        public boolean isSatisfied() {
            try {
                return ThreadUtils.runOnUiThreadBlocking(new Callable<Boolean>() {
                    @Override
                    public Boolean call() {
                        mDialog = PermissionDialogController.getInstance()
                                          .getCurrentDialogForTesting();
                        return (mDialog != null) == mExpectDialog;
                    }
                });
            } catch (ExecutionException e) {
                return false;
            }
        }
    }

    public PermissionTestRule() {
        super(ChromeActivity.class);
    }

    private void ruleSetUp() throws Throwable {
        // TODO(https://crbug.com/867446): Refactor to use EmbeddedTestServerRule.
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    /**
     * Starts an activity and listens for info-bars appearing/disappearing.
     */
    void setUpActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
        mListener = new InfoBarTestAnimationListener();
        getInfoBarContainer().addAnimationListener(mListener);
    }

    private void ruleTearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
    }

    protected void setUpUrl(final String url) throws InterruptedException {
        loadUrl(getURL(url));
    }

    public String getURL(String url) {
        return mTestServer.getURL(url);
    }

    public String getOrigin() {
        return mTestServer.getURL("/");
    }
    /**
     * Simulates clicking a button on an PermissionDialogView.
     */
    private void clickButton(final PermissionDialogView dialog, final int button) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                dialog.getButton(button).performClick();
            }
        });
    }

    /**
     * Runs a permission prompt test that grants the permission and expects the page title to be
     * updated in response.
     * @param updateWaiter  The update waiter to wait for callbacks. Should be added as an observer
     *                      to the current tab prior to calling this method.
     * @param javascript    The JS function to run in the current tab to execute the test and update
     *                      the page title.
     * @param nUpdates      How many updates of the page title to wait for.
     * @param withGeature   True if we require a user gesture to trigger the prompt.
     * @param isDialog      True if we are expecting a permission dialog, false for an infobar.
     * @throws Exception
     */
    public void runAllowTest(PermissionUpdateWaiter updateWaiter, final String url,
            String javascript, int nUpdates, boolean withGesture, boolean isDialog)
            throws Exception {
        setUpUrl(url);
        if (withGesture) {
            runJavaScriptCodeInCurrentTabWithGesture(javascript);
        } else {
            runJavaScriptCodeInCurrentTab(javascript);
        }
        replyToPromptAndWaitForUpdates(updateWaiter, true, nUpdates, isDialog);
    }

    /**
     * Runs a permission prompt test that grants the permission and expects the page title to be
     * updated in response.
     * @param updateWaiter  The update waiter to wait for callbacks. Should be added as an observer
     *                      to the current tab prior to calling this method.
     * @param javascript    The JS function to run in the current tab to execute the test and update
     *                      the page title.
     * @param nUpdates      How many updates of the page title to wait for.
     * @param withGesture   True if we require a user gesture to trigger the prompt.
     * @param isDialog      True if we are expecting a permission dialog, false for an infobar.
     * @throws Exception
     */
    public void runDenyTest(PermissionUpdateWaiter updateWaiter, final String url,
            String javascript, int nUpdates, boolean withGesture, boolean isDialog)
            throws Exception {
        setUpUrl(url);
        if (withGesture) {
            runJavaScriptCodeInCurrentTabWithGesture(javascript);
        } else {
            runJavaScriptCodeInCurrentTab(javascript);
        }
        replyToPromptAndWaitForUpdates(updateWaiter, false, nUpdates, isDialog);
    }

    private void replyToPromptAndWaitForUpdates(PermissionUpdateWaiter updateWaiter, boolean allow,
            int nUpdates, boolean isDialog) throws Exception {
        if (isDialog) {
            DialogShownCriteria criteria = new DialogShownCriteria("Dialog not shown", true);
            CriteriaHelper.pollUiThread(criteria);
            replyToDialogAndWaitForUpdates(updateWaiter, criteria.getDialog(), nUpdates, allow);
        } else {
            replyToInfoBarAndWaitForUpdates(updateWaiter, nUpdates, allow);
        }
    }

    private void runJavaScriptCodeInCurrentTabWithGesture(String javascript)
            throws InterruptedException, java.util.concurrent.TimeoutException {
        runJavaScriptCodeInCurrentTab("functionToRun = '" + javascript + "'");
        TouchCommon.singleClickView(getActivity().getActivityTab().getView());
    }

    /**
     * Replies to an infobar permission prompt and waits for a provided number
     * of updates to the page title in response.
     */
    private void replyToInfoBarAndWaitForUpdates(
            PermissionUpdateWaiter updateWaiter, int nUpdates, boolean allow) throws Exception {
        mListener.addInfoBarAnimationFinished("InfoBar not added.");
        InfoBar infobar = getInfoBars().get(0);
        Assert.assertNotNull(infobar);

        if (allow) {
            Assert.assertTrue("Allow button wasn't found", InfoBarUtil.clickPrimaryButton(infobar));
        } else {
            Assert.assertTrue(
                    "Block button wasn't found", InfoBarUtil.clickSecondaryButton(infobar));
        }
        updateWaiter.waitForNumUpdates(nUpdates);
    }

    /**
     * Replies to a dialog permission prompt and waits for a provided number of
     * updates to the page title in response.
     */
    private void replyToDialogAndWaitForUpdates(PermissionUpdateWaiter updateWaiter,
            PermissionDialogView dialog, int nUpdates, boolean allow) throws Exception {
        if (allow) {
            clickButton(dialog, DialogInterface.BUTTON_POSITIVE);
        } else {
            clickButton(dialog, DialogInterface.BUTTON_NEGATIVE);
        }
        updateWaiter.waitForNumUpdates(nUpdates);
    }
}
