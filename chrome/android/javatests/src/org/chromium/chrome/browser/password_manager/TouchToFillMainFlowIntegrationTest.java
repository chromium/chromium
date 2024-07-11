// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

/** Integration tests that check the main flow using Touch to Fill component. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(
        reason =
                "TODO(crbug.com/40232561): add resetting logic for"
                        + "FakePasswordStoreAndroidBackend to allow batching")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "show-autofill-signatures"})
public class TouchToFillMainFlowIntegrationTest {
    private static final String FORM_URL = "/chrome/test/data/password/simple_password.html";
    private static final String PASSWORD_ATTRIBUTE_NAME = "pm_parser_annotation";
    private static final String PASSWORD_NODE_ID = "password_field";
    private static final String TEST_ACCOUNT_EMAIL = "test@gmail.com";
    private static final String TEST_ACCOUNT_NAME = "Test user";
    private static final String TEST_ACCOUNT_PASSWORD = "S3cr3t";
    private EmbeddedTestServer mTestServer;
    private BottomSheetController mBottomSheetController;
    private WebContents mWebContents;
    private PasswordStoreBridge mPasswordStoreBridge;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public SigninTestRule mSigninTestRule = new SigninTestRule();

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
        PasswordManagerTestHelper.setAccountForPasswordStore(SigninTestRule.TEST_ACCOUNT_EMAIL);
        PasswordManagerTestUtilsBridge.disableServerPredictions();
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();

        mTestServer =
                EmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getInstrumentation().getContext(),
                        ServerCertificate.CERT_OK);

        runOnUiThreadBlocking(
                () -> {
                    mBottomSheetController =
                            BottomSheetControllerProvider.from(
                                    mActivityTestRule.getActivity().getWindowAndroid());
                });

        mWebContents = mActivityTestRule.getWebContents();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPasswordStoreBridge =
                            new PasswordStoreBridge(mActivityTestRule.getProfile(false));
                });
    }

    @After
    public void tearDown() {
        mSigninTestRule.tearDownRule();
    }

    @Test
    @MediumTest
    public void testClickingSuggestionPopulatesForm()
            throws TimeoutException, InterruptedException {
        // Fill the password store.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPasswordStoreBridge.insertPasswordCredential(
                            new PasswordStoreCredential(
                                    new GURL(mTestServer.getURL("/")),
                                    TEST_ACCOUNT_NAME,
                                    TEST_ACCOUNT_PASSWORD));
                });

        mActivityTestRule.loadUrl(mTestServer.getURL(FORM_URL));

        // Wait for autofill to parse the form and label the password field.
        waitForPasswordElementLabel();

        // Focus the field to bring up the TouchToFillSuggestions.
        focusField(PASSWORD_NODE_ID);

        // Wait for TTF.
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        // Check that the credential is there.
        CriteriaHelper.pollUiThread(() -> getCredentials().getChildAt(1) != null);

        // Check if the correct credential is shown.
        CriteriaHelper.pollUiThread(
                () -> {
                    LinearLayout credentialItemLayout =
                            (LinearLayout) getCredentials().getChildAt(1);
                    TextView usernameTextView = credentialItemLayout.findViewById(R.id.username);
                    return TEST_ACCOUNT_NAME.equals(usernameTextView.getText());
                });
    }

    private RecyclerView getCredentials() {
        return mActivityTestRule.getActivity().findViewById(R.id.sheet_item_list);
    }

    private void focusField(String node) throws TimeoutException, InterruptedException {
        // DOMUtils.clickNode could be flaky.
        DOMUtils.clickNode(mWebContents, node);
    }

    private void waitForPasswordElementLabel() {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    String attribute =
                            DOMUtils.getNodeAttribute(
                                    PASSWORD_ATTRIBUTE_NAME,
                                    mWebContents,
                                    PASSWORD_NODE_ID,
                                    String.class);
                    return attribute != null;
                });
    }
}
