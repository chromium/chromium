// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
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
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.concurrent.TimeoutException;

/**
 * Integration tests verifying that form resubmission dialogs are correctly displayed and handled.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class RepostFormWarningTest {
    // Active tab.

    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private Tab mTab;
    // Callback helper that manages waiting for pageloads to finish.
    private TestCallbackHelperContainer mCallbackHelper;

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        mTab = sActivityTestRule.getActivity().getActivityTab();
        mCallbackHelper = new TestCallbackHelperContainer(mTab.getWebContents());
        mTestServer = sActivityTestRule.getTestServer();
    }

    /** Verifies that the form resubmission warning is not displayed upon first POST navigation. */
    @Test
    @MediumTest
    @Feature({"Navigation"})
    public void testFormFirstNavigation() throws Throwable {
        // Load the url posting data for the first time.
        postNavigation();
        mCallbackHelper.getOnPageFinishedHelper().waitForCallback(0);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // Verify that the form resubmission warning was not shown.
        waitForNoReportFormWarningDialog();
    }

    /** Verifies that confirming the form reload performs the reload. */
    @Test
    @MediumTest
    @Feature({"Navigation"})
    public void testFormResubmissionContinue() throws Throwable {
        // Load the url posting data for the first time.
        postNavigation();
        mCallbackHelper.getOnPageFinishedHelper().waitForCallback(0);

        // Trigger a reload and wait for the warning to be displayed.
        reload();
        PropertyModel dialog = waitForRepostFormWarningDialog();

        // Click "Continue" and verify that the page is reloaded.
        clickButton(dialog, ModalDialogProperties.ButtonType.POSITIVE);
        mCallbackHelper.getOnPageFinishedHelper().waitForCallback(1);

        // Verify that the reference to the dialog in RepostFormWarningDialog was cleared.
        waitForNoReportFormWarningDialog();
    }

    /**
     * Verifies that cancelling the form reload prevents it from happening. Currently the test waits
     * after the "Cancel" button is clicked to verify that the load was not triggered, which blocks
     * for CallbackHelper's default timeout upon each execution.
     */
    @Test
    @SmallTest
    @Feature({"Navigation"})
    public void testFormResubmissionCancel() throws Throwable {
        // Load the url posting data for the first time.
        postNavigation();
        mCallbackHelper.getOnPageFinishedHelper().waitForCallback(0);

        // Trigger a reload and wait for the warning to be displayed.
        reload();
        PropertyModel dialog = waitForRepostFormWarningDialog();

        // Click "Cancel" and verify that the page is not reloaded.
        clickButton(dialog, ModalDialogProperties.ButtonType.NEGATIVE);
        boolean timedOut = false;
        try {
            mCallbackHelper.getOnPageFinishedHelper().waitForCallback(1);
        } catch (TimeoutException ex) {
            timedOut = true;
        }
        Assert.assertTrue("Page was reloaded despite selecting Cancel.", timedOut);

        // Verify that the reference to the dialog in RepostFormWarningDialog was cleared.
        waitForNoReportFormWarningDialog();
    }

    /** Verifies that destroying the Tab dismisses the form resubmission dialog. */
    @Test
    @SmallTest
    @Feature({"Navigation"})
    public void testFormResubmissionTabDestroyed() throws Throwable {
        // Load the url posting data for the first time.
        postNavigation();
        mCallbackHelper.getOnPageFinishedHelper().waitForCallback(0);

        // Trigger a reload and wait for the warning to be displayed.
        reload();
        waitForRepostFormWarningDialog();

        ThreadUtils.runOnUiThreadBlocking(
                (Runnable)
                        () ->
                                sActivityTestRule
                                        .getActivity()
                                        .getCurrentTabModel()
                                        .closeTabs(
                                                TabClosureParams.closeTab(mTab)
                                                        .allowUndo(false)
                                                        .build()));

        waitForNoReportFormWarningDialog();
    }

    private PropertyModel getCurrentModalDialog() {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        sActivityTestRule
                                .getActivity()
                                .getModalDialogManager()
                                .getCurrentDialogForTest());
    }

    private void waitForNoReportFormWarningDialog() {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Form resubmission dialog not dismissed correctly",
                            getCurrentModalDialog(),
                            Matchers.nullValue());
                });
    }

    private PropertyModel waitForRepostFormWarningDialog() {
        CriteriaHelper.pollUiThread(
                () -> {
                    PropertyModel dialogModel = getCurrentModalDialog();
                    Criteria.checkThat(
                            "No modal dialog shown", dialogModel, Matchers.notNullValue());
                    Criteria.checkThat(
                            "Modal dialog is not a HTTP post dialog",
                            dialogModel.get(ModalDialogProperties.TITLE),
                            Matchers.is(
                                    sActivityTestRule
                                            .getActivity()
                                            .getString(R.string.http_post_warning_title)));
                });
        return getCurrentModalDialog();
    }

    /** Performs a POST navigation in mTab. */
    private void postNavigation() {
        final String url = "/chrome/test/data/android/test.html";
        final byte[] postData = new byte[] {42};

        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () ->
                                mTab.loadUrl(
                                        LoadUrlParams.createLoadHttpPostParams(
                                                mTestServer.getURL(url), postData)));
    }

    /** Reloads mTab. */
    private void reload() {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> mTab.reload());
    }

    /** Clicks the given button in the given dialog. */
    private void clickButton(final PropertyModel dialog, final @ButtonType int type) {
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> dialog.get(ModalDialogProperties.CONTROLLER).onClick(dialog, type));
    }
}
