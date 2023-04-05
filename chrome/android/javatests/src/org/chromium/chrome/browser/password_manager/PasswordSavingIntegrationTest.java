// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.chromium.base.test.util.Matchers.is;
import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Matchers;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.messages.MessagesTestHelper;
import org.chromium.components.signin.AccountUtils;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestInputMethodManagerWrapper;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

/**
 * Integration test for the whole saving/updating password workflow.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "show-autofill-signatures"})
@EnableFeatures({ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID})
public class PasswordSavingIntegrationTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();
    @Rule
    public SigninTestRule mSigninTestRule = new SigninTestRule();

    private static final String SIGNIN_FORM_URL = "/chrome/test/data/password/simple_password.html";
    private static final String CHANGE_PASSWORD_FORM_URL =
            "/chrome/test/data/password/simple_change_password_form.html";
    private static final String DONE_URL = "/chrome/test/data/password/done.html";
    private static final String USERNAME_FIELD_ID = "username_field";
    private static final String PASSWORD_NODE_ID = "password_field";
    private static final String OLD_PASSWORD_NODE_ID = "chg_password_field";
    private static final String NEW_PASSWORD_NODE_ID = "chg_new_password_1";
    private static final String NEW_PASSWORD_REPEAT_NODE_ID = "chg_new_password_2";
    private static final String USERNAME_TEXT = "username";
    private static final String PASSWORD_TEXT = "password";
    private static final String NEW_PASSWORD_TEXT = "new password";
    private static final String SUBMIT_BUTTON_ID = "input_submit_button";
    private static final String CHANGE_PASSWORD_BUTTON_ID = "chg_submit_button";
    private static final String PASSWORD_MANAGER_ANNOTATION = "pm_parser_annotation";

    private PasswordStoreBridge mPasswordStoreBridge;
    private BottomSheetController mBottomSheetController;
    private WebContents mWebContents;

    @Before
    public void setup() throws Exception {
        PasswordStoreAndroidBackendFactory.setFactoryInstanceForTesting(
                new FakePasswordStoreAndroidBackendFactoryImpl());
        runOnUiThreadBlocking(() -> {
            ((FakePasswordStoreAndroidBackend) PasswordStoreAndroidBackendFactory.getInstance()
                            .createBackend())
                    .setSyncingAccount(
                            AccountUtils.createAccountFromName(SigninTestRule.TEST_ACCOUNT_EMAIL));
        });
        PasswordSyncControllerDelegateFactory.setFactoryInstanceForTesting(
                new FakePasswordSyncControllerDelegateFactoryImpl());

        mActivityTestRule.startMainActivityOnBlankPage();
        PasswordManagerTestUtilsBridge.disableServerPredictions();
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();

        runOnUiThreadBlocking(() -> {
            mBottomSheetController = BottomSheetControllerProvider.from(
                    mActivityTestRule.getActivity().getWindowAndroid());
            mPasswordStoreBridge = new PasswordStoreBridge();
        });

        mWebContents = mActivityTestRule.getWebContents();
    }

    @After
    public void tearDown() {
        runOnUiThreadBlocking(() -> { mPasswordStoreBridge.clearAllPasswords(); });
        mSigninTestRule.tearDownRule();
    }

    @Test
    @MediumTest
    public void testSavingNewPassword() throws InterruptedException, TimeoutException {
        mActivityTestRule.loadUrl(mActivityTestRule.getTestServer().getURL(SIGNIN_FORM_URL));

        enterInput(mWebContents, USERNAME_FIELD_ID, USERNAME_TEXT);
        enterInput(mWebContents, PASSWORD_NODE_ID, PASSWORD_TEXT);
        waitForPmParserAnnotation(mWebContents, PASSWORD_NODE_ID);

        DOMUtils.clickNodeWithJavaScript(mWebContents, SUBMIT_BUTTON_ID);
        ChromeTabUtils.waitForTabPageLoaded(mActivityTestRule.getActivity().getActivityTab(),
                mActivityTestRule.getTestServer().getURL(DONE_URL));
        waitForMessageShown();

        clickSaveUpdateButtonOnMessage();
        CriteriaHelper.pollUiThread(() -> {
            PasswordStoreCredential[] credentials = mPasswordStoreBridge.getAllCredentials();
            Criteria.checkThat("Should have added one password.", credentials.length, is(1));
            Criteria.checkThat(credentials[0].getUsername(), is(USERNAME_TEXT));
            Criteria.checkThat(credentials[0].getPassword(), is(PASSWORD_TEXT));
        });
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1359305")
    public void testUpdatingPassword() throws InterruptedException, TimeoutException {
        // Store the test credential.
        PasswordStoreCredential testCredential = new PasswordStoreCredential(
                new GURL(mActivityTestRule.getTestServer().getURL(CHANGE_PASSWORD_FORM_URL)),
                USERNAME_TEXT, PASSWORD_TEXT);
        runOnUiThreadBlocking(
                () -> { mPasswordStoreBridge.insertPasswordCredential(testCredential); });

        mActivityTestRule.loadUrl(
                mActivityTestRule.getTestServer().getURL(CHANGE_PASSWORD_FORM_URL));

        // Wait for autofill to parse the form and label the password field.
        waitForPmParserAnnotation(mWebContents, OLD_PASSWORD_NODE_ID);

        // Focus the field to bring up the TouchToFillSuggestions.
        DOMUtils.clickNode(mWebContents, OLD_PASSWORD_NODE_ID);

        // Wait for TTF to show up.
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        // Click the TTF button that inserts the credential in the form.
        ButtonCompat continueButton = (ButtonCompat) getCredentials().getChildAt(2);
        runOnUiThreadBlocking(() -> { continueButton.performClick(); });

        // Wait till TTF closes.
        BottomSheetTestSupport.waitForState(mBottomSheetController, SheetState.HIDDEN);

        // Enter the new password.
        enterInput(mWebContents, NEW_PASSWORD_NODE_ID, NEW_PASSWORD_TEXT);
        enterInput(mWebContents, NEW_PASSWORD_REPEAT_NODE_ID, NEW_PASSWORD_TEXT);

        // Submit the form and wait for the success page to load.
        DOMUtils.clickNodeWithJavaScript(mWebContents, CHANGE_PASSWORD_BUTTON_ID);
        ChromeTabUtils.waitForTabPageLoaded(mActivityTestRule.getActivity().getActivityTab(),
                mActivityTestRule.getTestServer().getURL(DONE_URL));

        // Wait for the update message to show and confirm the update.
        waitForMessageShown();
        clickSaveUpdateButtonOnMessage();

        // Check that the credential was updated.
        CriteriaHelper.pollUiThread(() -> {
            PasswordStoreCredential[] credentials = mPasswordStoreBridge.getAllCredentials();
            Criteria.checkThat("Should have contained one password.", credentials.length, is(1));
            Criteria.checkThat(credentials[0].getUsername(), is(USERNAME_TEXT));
            Criteria.checkThat(credentials[0].getPassword(), is(NEW_PASSWORD_TEXT));
        });
    }

    private void enterInput(WebContents webContents, String nodeId, String input)
            throws TimeoutException {
        ImeAdapter imeAdapter = WebContentsUtils.getImeAdapter(webContents);
        TestInputMethodManagerWrapper inputMethodManagerWrapper =
                TestInputMethodManagerWrapper.create(imeAdapter);
        imeAdapter.setInputMethodManagerWrapper(inputMethodManagerWrapper);

        DOMUtils.clickNode(webContents, nodeId);
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(inputMethodManagerWrapper.getShowSoftInputCounter(), Matchers.is(1));
        });

        imeAdapter.setComposingTextForTest(input, input.length());
    }

    private void clickSaveUpdateButtonOnMessage() {
        runOnUiThreadBlocking(() -> {
            TextView button =
                    mActivityTestRule.getActivity().findViewById(R.id.message_primary_button);
            button.performClick();
        });
    }

    private void waitForMessageShown() {
        WindowAndroid window = mActivityTestRule.getActivity().getWindowAndroid();
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat("Message is not enqueued.",
                    MessagesTestHelper.getMessageCount(window), Matchers.is(1));
        });
    }

    private void waitForPmParserAnnotation(WebContents webContents, String nodeID) {
        CriteriaHelper.pollInstrumentationThread(() -> {
            String attribute = DOMUtils.getNodeAttribute(
                    PASSWORD_MANAGER_ANNOTATION, webContents, nodeID, String.class);
            return attribute != null;
        });
    }

    private RecyclerView getCredentials() {
        return mActivityTestRule.getActivity().findViewById(R.id.sheet_item_list);
    }
}
