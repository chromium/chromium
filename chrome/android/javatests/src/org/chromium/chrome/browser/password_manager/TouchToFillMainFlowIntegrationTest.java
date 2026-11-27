// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

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
import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.SimpleConditions;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.ui.BottomSheetFacility;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.content_public.browser.test.transit.HtmlElement;
import org.chromium.content_public.browser.test.transit.WebPageFacility;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.url.GURL;

/** Integration tests that check the main flow using Touch to Fill component. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(
        reason =
                "TODO(crbug.com/40232561): add resetting logic for"
                        + "FakePasswordStoreAndroidBackend to allow batching")
@CommandLineFlags.Add("show-autofill-signatures")
public class TouchToFillMainFlowIntegrationTest {
    private static final String FORM_URL = "/chrome/test/data/password/simple_password.html";
    private static final String PASSWORD_ATTRIBUTE_NAME = "pm_parser_annotation";
    private static final String PASSWORD_NODE_ID = "password_field";
    private static final String TEST_ACCOUNT_NAME = "Test user";
    private static final String TEST_ACCOUNT_PASSWORD = "S3cr3t";
    private EmbeddedTestServer mTestServer;
    private BottomSheetController mBottomSheetController;
    private PasswordStoreBridge mPasswordStoreBridge;

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public SigninTestRule mSigninTestRule = new SigninTestRule();

    private WebPageStation mStartingPage;

    private static class PasswordFormFacility extends WebPageFacility {
        public final HtmlElement annotatedPasswordField =
                declareHtmlElement("#" + PASSWORD_NODE_ID + "[" + PASSWORD_ATTRIBUTE_NAME + "]");
    }

    // TODO(crbug.com/428032441): Drop the sheet state condition once BottomSheetFacility waits
    // for the sheet to settle.
    private static class TouchToFillPasswordFacility extends BottomSheetFacility<WebPageStation> {
        private final BottomSheetController mController;

        TouchToFillPasswordFacility(BottomSheetController controller, String username) {
            mController = controller;
            ViewElement<RecyclerView> credentialList =
                    declareDescendantView(RecyclerView.class, withId(R.id.sheet_item_list));
            declareView(
                    credentialList.descendant(
                            TextView.class, withId(R.id.username), withText(username)));
        }

        @Override
        public void declareExtraElements() {
            declareEnterCondition(
                    SimpleConditions.uiThreadCondition(
                            "Bottom sheet is open",
                            () -> {
                                @SheetState int state = mController.getSheetState();
                                return Condition.whether(
                                        state == SheetState.HALF || state == SheetState.FULL,
                                        "sheetState=%d (expected HALF=%d or FULL=%d)",
                                        state,
                                        SheetState.HALF,
                                        SheetState.FULL);
                            }));
        }
    }

    public TouchToFillMainFlowIntegrationTest() {
        // This test suite relies on the real password store. However, that can only store
        // passwords if the device it runs on has the required min GMS Core version.
        // To ensure the tests don't depend on the device configuration, set up a fake GMS
        // Core version instead.
        PasswordManagerTestHelper.setUpPwmRequiredMinGmsVersion();
    }

    @Before
    public void setUp() {
        mStartingPage = mActivityTestRule.startOnBlankPage();
        PasswordManagerTestHelper.setAccountForPasswordStore(TestAccounts.ACCOUNT1.getEmail());
        PasswordManagerTestUtilsBridge.disableServerPredictions();
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        mTestServer =
                EmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getInstrumentation().getContext(),
                        ServerCertificate.CERT_OK);

        runOnUiThreadBlocking(
                () -> {
                    mBottomSheetController =
                            BottomSheetControllerProvider.from(
                                    mStartingPage.getActivity().getWindowAndroid());
                });

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

    // TODO(crbug.com/462636368): Turn on the flag after blink bug is fixed.
    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.AUTOFILL_ANDROID_KEYBOARD_ACCESSORY_DYNAMIC_POSITIONING)
    @DisableIf.Device(DeviceFormFactor.DESKTOP_FREEFORM) // crbug.com/511288618
    public void testClickingSuggestionPopulatesForm() {
        // Fill the password store.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPasswordStoreBridge.insertPasswordCredential(
                            new PasswordStoreCredential(
                                    new GURL(mTestServer.getURL("/")),
                                    TEST_ACCOUNT_NAME,
                                    TEST_ACCOUNT_PASSWORD));
                });

        // Load the page and wait for autofill to parse the form and label the password field.
        PasswordFormFacility passwordFormFacility = new PasswordFormFacility();
        mStartingPage =
                mStartingPage.loadPageProgrammatically(
                        mTestServer.getURL(FORM_URL),
                        WebPageStation.newBuilder().withFacility(passwordFormFacility));

        // Focus the field to bring up the TouchToFillSuggestions and wait for TTF to show the
        // credential.
        passwordFormFacility
                .annotatedPasswordField
                .clickTo()
                .enterFacility(
                        new TouchToFillPasswordFacility(mBottomSheetController, TEST_ACCOUNT_NAME));
    }
}
