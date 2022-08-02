// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.chromium.base.test.util.Matchers.is;

import android.widget.TextView;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Matchers;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.messages.MessagesTestHelper;
import org.chromium.components.messages.R;
import org.chromium.components.signin.AccountUtils;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestInputMethodManagerWrapper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.ui.base.WindowAndroid;

import java.util.concurrent.TimeoutException;

/**
 * Integration test for the whole saving/updating password workflow.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "TODO(crbug.com/1346583): add resetting logic for"
                + "FakePasswordStoreAndroidBackend to allow batching")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "show-autofill-signatures"})
@EnableFeatures({ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID})
public class PasswordSavingIntegrationTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();
    @Rule
    public SigninTestRule mSigninTestRule = new SigninTestRule();

    private static final String FORM_URL = "/chrome/test/data/password/simple_password.html";
    private static final String DONE_URL = "/chrome/test/data/password/done.html";
    private static final String USERNAME_FIELD_ID = "username_field";
    private static final String PASSWORD_NODE_ID = "password_field";
    private static final String USERNAME_TEXT = "username";
    private static final String PASSWORD_TEXT = "password";
    private static final String SUBMIT_BUTTON_ID = "input_submit_button";
    private static final String PASSWORD_MANAGER_ANNOTATION = "pm_parser_annotation";

    private PasswordStoreBridge mPasswordStoreBridge;

    @Before
    public void setup() throws Exception {
        PasswordStoreAndroidBackendFactory.setFactoryInstanceForTesting(
                new FakePasswordStoreAndroidBackendFactoryImpl());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
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

        mActivityTestRule.loadUrl(mActivityTestRule.getTestServer().getURL(FORM_URL));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mPasswordStoreBridge = new PasswordStoreBridge(); });
    }

    @After
    public void tearDown() {
        mSigninTestRule.tearDownRule();
    }

    @Test
    @MediumTest
    public void testSavingNewPassword() throws InterruptedException, TimeoutException {
        WebContents webContents = mActivityTestRule.getWebContents();
        enterInput(webContents, USERNAME_FIELD_ID, USERNAME_TEXT);
        enterInput(webContents, PASSWORD_NODE_ID, PASSWORD_TEXT);
        waitForPmParserAnnotation(webContents);

        DOMUtils.clickNodeWithJavaScript(webContents, SUBMIT_BUTTON_ID);
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
        TestThreadUtils.runOnUiThreadBlocking(() -> {
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

    private void waitForPmParserAnnotation(WebContents webContents) {
        CriteriaHelper.pollInstrumentationThread(() -> {
            String attribute = DOMUtils.getNodeAttribute(
                    PASSWORD_MANAGER_ANNOTATION, webContents, PASSWORD_NODE_ID, String.class);
            return attribute != null;
        });
    }
}
