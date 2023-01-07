// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CommandLineFlags.Add;
import org.chromium.base.test.util.CommandLineFlags.Remove;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.infobar.AutofillSaveCardInfoBar;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.infobars.InfoBar;
import org.chromium.components.infobars.InfoBarLayout;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessageStateHandler;
import org.chromium.components.messages.MessagesTestHelper;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ButtonCompat;

import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Integration tests for the Autofill Upstream and Expiration Date Fix Flow.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "enable-features=AutofillUpstream"})
public class AutofillUpstreamTest {
    private static final String TEST_SERVER_DIR = "components/test/data/autofill";
    private static final String TEST_FORM_URL = "/credit_card_upload_form_address_and_cc.html";
    private static final String SAVE_BUTTON_LABEL = "Save";
    private static final String CONTINUE_BUTTON_LABEL = "Continue";
    private static final String MESSAGE_UI_PARAMS =
            "enable-features=" + ChromeFeatureList.MESSAGES_FOR_ANDROID_SAVE_CARD + "<Study";
    private static final String MESSAGE_UI_TRIAL = "force-fieldtrials=Study/Group";
    private static final String MESSAGE_UI_TRIAL_PARAMS =
            "force-fieldtrial-params=Study.Group:save_card_message_use_followup_button_text/false/save_card_message_use_gpay_icon/true";
    private static final String INFOBAR_UI_PARAMS =
            "disable-features=" + ChromeFeatureList.MESSAGES_FOR_ANDROID_SAVE_CARD;

    @Rule
    public SyncTestRule mActivityTestRule = new SyncTestRule();

    private EmbeddedTestServer mServer;

    @Before
    public void setUp() {
        mActivityTestRule.setUpAccountAndEnableSyncForTesting();
        mServer = new EmbeddedTestServer();
        mServer.initializeNative(InstrumentationRegistry.getContext(),
                EmbeddedTestServer.ServerHTTPSSetting.USE_HTTP);
        mServer.addDefaultHandlers(TEST_SERVER_DIR);
        mServer.start();
    }

    @After
    public void tearDown() {
        mServer.stopAndDestroyServer();
    }

    private void assertInfoBarPrimaryButtonLabel(String buttonLabel) {
        InfoBarLayout view = (InfoBarLayout) getAutofillSaveCardInfoBar().getView();
        ButtonCompat primaryButton = view.getPrimaryButton();
        Assert.assertEquals(buttonLabel, primaryButton.getText().toString());
    }

    private void assertMessagePrimaryButtonLabel(String buttonLabel) throws ExecutionException {
        MessageDispatcher dispatcher = getMessageDispatcher();
        MessageStateHandler handler =
                MessagesTestHelper.getEnqueuedMessages(dispatcher, MessageIdentifier.SAVE_CARD)
                        .get(0);
        PropertyModel model = MessagesTestHelper.getCurrentMessage(handler);
        Assert.assertEquals(buttonLabel, model.get(MessageBannerProperties.PRIMARY_BUTTON_TEXT));
    }

    private void waitForSaveCardInfoBar() {
        CriteriaHelper.pollUiThread(
                ()
                        -> hasAutofillSaveCardInfobar(mActivityTestRule.getInfoBars()),
                "Autofill Save Card Infobar view was never added.");
    }

    private void waitForSaveCardMessage() throws ExecutionException {
        MessageDispatcher dispatcher = getMessageDispatcher();
        CriteriaHelper.pollUiThread(
                ()
                        -> !MessagesTestHelper
                                    .getEnqueuedMessages(dispatcher, MessageIdentifier.SAVE_CARD)
                                    .isEmpty());
    }

    private void onMessagePrimaryButtonClicked() throws ExecutionException {
        MessageDispatcher dispatcher = getMessageDispatcher();
        MessageStateHandler handler =
                MessagesTestHelper.getEnqueuedMessages(dispatcher, MessageIdentifier.SAVE_CARD)
                        .get(0);
        PropertyModel model = MessagesTestHelper.getCurrentMessage(handler);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.get(MessageBannerProperties.ON_PRIMARY_ACTION).get();
            dispatcher.dismissMessage(model, DismissReason.PRIMARY_ACTION);
        });
    }

    private MessageDispatcher getMessageDispatcher() throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> MessageDispatcherProvider.from(
                                mActivityTestRule.getActivity().getWindowAndroid()));
    }

    private boolean hasAutofillSaveCardInfobar(List<InfoBar> infobars) {
        return (infobars != null && infobars.size() == 1
                && infobars.get(0) instanceof AutofillSaveCardInfoBar);
    }

    private AutofillSaveCardInfoBar getAutofillSaveCardInfoBar() {
        List<InfoBar> infobars = mActivityTestRule.getInfoBars();
        if (hasAutofillSaveCardInfobar(infobars)) {
            return (AutofillSaveCardInfoBar) infobars.get(0);
        }
        Assert.fail("Save card infobar not found");
        return null;
    }

    private PropertyModel getPropertyModelForDialog() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> mActivityTestRule.getActivity()
                                   .getModalDialogManager()
                                   .getCurrentDialogForTest());
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Add({INFOBAR_UI_PARAMS})
    @Remove({MESSAGE_UI_PARAMS, MESSAGE_UI_TRIAL, MESSAGE_UI_TRIAL_PARAMS})
    public void testSaveCardInfoBarWithAllFieldsFilled() throws TimeoutException {
        mActivityTestRule.loadUrl(mServer.getURL(TEST_FORM_URL));
        final WebContents webContents = mActivityTestRule.getActivity().getCurrentWebContents();

        DOMUtils.clickNode(webContents, "fill_form");
        DOMUtils.clickNode(webContents, "submit");
        waitForSaveCardInfoBar();
        assertInfoBarPrimaryButtonLabel(SAVE_BUTTON_LABEL);
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Add({MESSAGE_UI_PARAMS, MESSAGE_UI_TRIAL, MESSAGE_UI_TRIAL_PARAMS})
    @Remove({INFOBAR_UI_PARAMS})
    public void testSaveCardMessageWithAllFieldsFilled()
            throws TimeoutException, ExecutionException {
        mActivityTestRule.loadUrl(mServer.getURL(TEST_FORM_URL));
        final WebContents webContents = mActivityTestRule.getActivity().getCurrentWebContents();

        DOMUtils.clickNode(webContents, "fill_form");
        DOMUtils.clickNode(webContents, "submit");
        waitForSaveCardMessage();
        // For message UI, primary button label is always "Continue" in order to
        // trigger a dialog with legal terms if card is uploaded.
        assertMessagePrimaryButtonLabel(CONTINUE_BUTTON_LABEL);

        onMessagePrimaryButtonClicked();
        PropertyModel fixflowPromptPropertyModel = getPropertyModelForDialog();

        // Verify that dialog is not null.
        Assert.assertNotNull(fixflowPromptPropertyModel);
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Add({INFOBAR_UI_PARAMS})
    @Remove({MESSAGE_UI_PARAMS, MESSAGE_UI_TRIAL, MESSAGE_UI_TRIAL_PARAMS})
    public void testSaveCardInfoBarWithEmptyMonth() throws TimeoutException {
        mActivityTestRule.loadUrl(mServer.getURL(TEST_FORM_URL));
        final WebContents webContents = mActivityTestRule.getActivity().getCurrentWebContents();

        DOMUtils.clickNode(webContents, "fill_form");
        // Clear the month field.
        DOMUtils.clickNode(webContents, "clear_month");
        DOMUtils.clickNode(webContents, "submit");
        waitForSaveCardInfoBar();

        assertInfoBarPrimaryButtonLabel(CONTINUE_BUTTON_LABEL);
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Add({MESSAGE_UI_PARAMS, MESSAGE_UI_TRIAL, MESSAGE_UI_TRIAL_PARAMS})
    @Remove({INFOBAR_UI_PARAMS})
    public void testSaveCardMessageWithEmptyMonth() throws TimeoutException, ExecutionException {
        mActivityTestRule.loadUrl(mServer.getURL(TEST_FORM_URL));
        final WebContents webContents = mActivityTestRule.getActivity().getCurrentWebContents();

        DOMUtils.clickNode(webContents, "fill_form");
        // Clear the month field.
        DOMUtils.clickNode(webContents, "clear_month");
        DOMUtils.clickNode(webContents, "submit");
        waitForSaveCardMessage();

        assertMessagePrimaryButtonLabel(CONTINUE_BUTTON_LABEL);
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Add({INFOBAR_UI_PARAMS})
    @Remove({MESSAGE_UI_PARAMS, MESSAGE_UI_TRIAL, MESSAGE_UI_TRIAL_PARAMS})
    public void testSaveCardInfoBarWithEmptyYear() throws TimeoutException {
        mActivityTestRule.loadUrl(mServer.getURL(TEST_FORM_URL));
        final WebContents webContents = mActivityTestRule.getActivity().getCurrentWebContents();

        DOMUtils.clickNode(webContents, "fill_form");
        // Clear the year field.
        DOMUtils.clickNode(webContents, "clear_year");
        DOMUtils.clickNode(webContents, "submit");
        waitForSaveCardInfoBar();

        assertInfoBarPrimaryButtonLabel(CONTINUE_BUTTON_LABEL);
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Add({MESSAGE_UI_PARAMS, MESSAGE_UI_TRIAL, MESSAGE_UI_TRIAL_PARAMS})
    @Remove({INFOBAR_UI_PARAMS})
    public void testSaveCardMessageWithEmptyYear() throws TimeoutException, ExecutionException {
        mActivityTestRule.loadUrl(mServer.getURL(TEST_FORM_URL));
        final WebContents webContents = mActivityTestRule.getActivity().getCurrentWebContents();

        DOMUtils.clickNode(webContents, "fill_form");
        // Clear the year field.
        DOMUtils.clickNode(webContents, "clear_year");
        DOMUtils.clickNode(webContents, "submit");
        waitForSaveCardMessage();

        assertMessagePrimaryButtonLabel(CONTINUE_BUTTON_LABEL);
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Add({INFOBAR_UI_PARAMS})
    @Remove({MESSAGE_UI_PARAMS, MESSAGE_UI_TRIAL, MESSAGE_UI_TRIAL_PARAMS})
    public void testSaveCardInfoBarWithEmptyMonthAndYear() throws TimeoutException {
        mActivityTestRule.loadUrl(mServer.getURL(TEST_FORM_URL));
        final WebContents webContents = mActivityTestRule.getActivity().getCurrentWebContents();

        DOMUtils.clickNode(webContents, "fill_form");
        // Clear the month and year field.
        DOMUtils.clickNode(webContents, "clear_expiration_date");
        DOMUtils.clickNode(webContents, "submit");
        waitForSaveCardInfoBar();

        assertInfoBarPrimaryButtonLabel(CONTINUE_BUTTON_LABEL);
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Add({MESSAGE_UI_PARAMS, MESSAGE_UI_TRIAL, MESSAGE_UI_TRIAL_PARAMS})
    @Remove({INFOBAR_UI_PARAMS})
    public void testSaveCardMessageWithEmptyMonthAndYear()
            throws TimeoutException, ExecutionException {
        mActivityTestRule.loadUrl(mServer.getURL(TEST_FORM_URL));
        final WebContents webContents = mActivityTestRule.getActivity().getCurrentWebContents();

        DOMUtils.clickNode(webContents, "fill_form");
        // Clear the month and year field.
        DOMUtils.clickNode(webContents, "clear_expiration_date");
        DOMUtils.clickNode(webContents, "submit");
        waitForSaveCardMessage();

        assertMessagePrimaryButtonLabel(CONTINUE_BUTTON_LABEL);
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Add({INFOBAR_UI_PARAMS})
    @Remove({MESSAGE_UI_PARAMS, MESSAGE_UI_TRIAL, MESSAGE_UI_TRIAL_PARAMS})
    public void testSaveCardInfoBarContinueButton_EmptyExpDate_launchesExpDateFixFlow()
            throws TimeoutException {
        mActivityTestRule.loadUrl(mServer.getURL(TEST_FORM_URL));
        final WebContents webContents = mActivityTestRule.getActivity().getCurrentWebContents();

        DOMUtils.clickNode(webContents, "fill_form");
        // Clear the month and year field.
        DOMUtils.clickNode(webContents, "clear_expiration_date");
        DOMUtils.clickNode(webContents, "submit");
        waitForSaveCardInfoBar();
        // Click on the continue button.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> getAutofillSaveCardInfoBar().onButtonClicked(true));
        PropertyModel fixflowPromptPropertyModel = getPropertyModelForDialog();

        // Verify that dialog is not null.
        Assert.assertNotNull(fixflowPromptPropertyModel);
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Add({MESSAGE_UI_PARAMS, MESSAGE_UI_TRIAL, MESSAGE_UI_TRIAL_PARAMS})
    @Remove({INFOBAR_UI_PARAMS})
    public void testSaveCardMessageContinueButton_EmptyExpDate_launchesExpDateFixFlow()
            throws TimeoutException, ExecutionException {
        mActivityTestRule.loadUrl(mServer.getURL(TEST_FORM_URL));
        final WebContents webContents = mActivityTestRule.getActivity().getCurrentWebContents();

        DOMUtils.clickNode(webContents, "fill_form");
        // Clear the month and year field.
        DOMUtils.clickNode(webContents, "clear_expiration_date");
        DOMUtils.clickNode(webContents, "submit");
        waitForSaveCardMessage();
        // Click on the continue button.
        onMessagePrimaryButtonClicked();
        PropertyModel fixflowPromptPropertyModel = getPropertyModelForDialog();

        // Verify that dialog is not null.
        Assert.assertNotNull(fixflowPromptPropertyModel);
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Add({INFOBAR_UI_PARAMS})
    @Remove({MESSAGE_UI_PARAMS, MESSAGE_UI_TRIAL, MESSAGE_UI_TRIAL_PARAMS})
    public void testSaveCardInfoBarWithEmptyName() throws TimeoutException {
        mActivityTestRule.loadUrl(mServer.getURL(TEST_FORM_URL));
        final WebContents webContents = mActivityTestRule.getActivity().getCurrentWebContents();

        DOMUtils.clickNode(webContents, "fill_form");
        // Clear the name field.
        DOMUtils.clickNode(webContents, "clear_name");
        DOMUtils.clickNode(webContents, "submit");
        waitForSaveCardInfoBar();

        assertInfoBarPrimaryButtonLabel(CONTINUE_BUTTON_LABEL);
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Add({MESSAGE_UI_PARAMS, MESSAGE_UI_TRIAL, MESSAGE_UI_TRIAL_PARAMS})
    @Remove({INFOBAR_UI_PARAMS})
    public void testSaveCardMessageWithEmptyName() throws TimeoutException, ExecutionException {
        mActivityTestRule.loadUrl(mServer.getURL(TEST_FORM_URL));
        final WebContents webContents = mActivityTestRule.getActivity().getCurrentWebContents();

        DOMUtils.clickNode(webContents, "fill_form");
        // Clear the name field.
        DOMUtils.clickNode(webContents, "clear_name");
        DOMUtils.clickNode(webContents, "submit");
        waitForSaveCardMessage();

        assertMessagePrimaryButtonLabel(CONTINUE_BUTTON_LABEL);
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Add({INFOBAR_UI_PARAMS})
    @Remove({MESSAGE_UI_PARAMS, MESSAGE_UI_TRIAL, MESSAGE_UI_TRIAL_PARAMS})
    public void testSaveCardInfoBarContinueButton_EmptyName_launchesNameFixFlow()
            throws TimeoutException {
        mActivityTestRule.loadUrl(mServer.getURL(TEST_FORM_URL));
        final WebContents webContents = mActivityTestRule.getActivity().getCurrentWebContents();

        DOMUtils.clickNode(webContents, "fill_form");
        // Clear the name field.
        DOMUtils.clickNode(webContents, "clear_name");
        DOMUtils.clickNode(webContents, "submit");
        waitForSaveCardInfoBar();
        // Click on the continue button.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> getAutofillSaveCardInfoBar().onButtonClicked(true));
        PropertyModel fixflowPromptPropertyModel = getPropertyModelForDialog();

        // Verify that dialog is not null.
        Assert.assertNotNull(fixflowPromptPropertyModel);
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Add({MESSAGE_UI_PARAMS, MESSAGE_UI_TRIAL, MESSAGE_UI_TRIAL_PARAMS})
    @Remove({INFOBAR_UI_PARAMS})
    public void testSaveCardMessageContinueButton_EmptyName_launchesNameFixFlow()
            throws TimeoutException, ExecutionException {
        mActivityTestRule.loadUrl(mServer.getURL(TEST_FORM_URL));
        final WebContents webContents = mActivityTestRule.getActivity().getCurrentWebContents();

        DOMUtils.clickNode(webContents, "fill_form");
        // Clear the name field.
        DOMUtils.clickNode(webContents, "clear_name");
        DOMUtils.clickNode(webContents, "submit");
        waitForSaveCardMessage();
        // Click on the continue button.
        onMessagePrimaryButtonClicked();
        PropertyModel fixflowPromptPropertyModel = getPropertyModelForDialog();

        // Verify that dialog is not null.
        Assert.assertNotNull(fixflowPromptPropertyModel);
    }
}
