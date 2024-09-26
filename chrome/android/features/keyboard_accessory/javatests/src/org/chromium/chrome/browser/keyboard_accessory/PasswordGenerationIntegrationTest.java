// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.test.util.Matchers.is;
import static org.chromium.chrome.browser.touch_to_fill.password_generation.TouchToFillPasswordGenerationTestHelper.acceptPasswordInGenerationBottomSheet;
import static org.chromium.chrome.browser.touch_to_fill.password_generation.TouchToFillPasswordGenerationTestHelper.rejectPasswordInGenerationBottomSheet;

import android.view.View;
import android.widget.Button;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.IntegrationTest;
import org.chromium.base.test.util.Matchers;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.keyboard_accessory.button_group_component.KeyboardAccessoryButtonGroupView;
import org.chromium.chrome.browser.password_manager.PasswordManagerTestHelper;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge;
import org.chromium.chrome.browser.password_manager.PasswordStoreCredential;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.messages.MessagesTestHelper;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.test.util.GmsCoreVersionRestriction;
import org.chromium.ui.widget.ChromeImageButton;

import java.util.ArrayList;
import java.util.concurrent.TimeoutException;

/** Integration tests for password generation. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(
        reason =
                "TODO(crbug.com/40232561): add resetting logic for"
                        + "FakePasswordStoreAndroidBackend to allow batching")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "show-autofill-signatures"})
public class PasswordGenerationIntegrationTest {
    /**
     * The number of buttons currently available in the keyboard accessory bar. The offered options
     * are: passwords, addresses and payments.
     */
    public static final int KEYBOARD_ACCESSORY_BAR_ITEM_COUNT = 3;

    @Rule public SyncTestRule mSyncTestRule = new SyncTestRule();

    private static final String PASSWORD_NODE_ID = "password_field";
    private static final String PASSWORD_NODE_ID_MANUAL = "password_field_manual";
    private static final String SUBMIT_NODE_ID = "input_submit_button";
    private static final String SUBMIT_NODE_ID_MANUAL = "input_submit_button_manual";
    private static final String FORM_URL =
            "/chrome/test/data/password/filled_simple_signup_form.html";
    private static final String DONE_URL = "/chrome/test/data/password/done.html";
    private static final String PASSWORD_ATTRIBUTE_NAME = "password_creation_field";
    private static final String ELIGIBLE_FOR_GENERATION = "1";
    private static final String USERNAME_TEXT = "username";

    private EmbeddedTestServer mTestServer;
    private ManualFillingTestHelper mHelper;
    private PasswordStoreBridge mPasswordStoreBridge;
    private ChromeTabbedActivity mActivity;
    private RecyclerView mKeyboardAccessoryBarItems;
    private BottomSheetController mBottomSheetController;

    @Before
    public void setUp() throws InterruptedException {
        PasswordManagerTestHelper.setAccountForPasswordStore(SigninTestRule.TEST_ACCOUNT_EMAIL);

        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        ManualFillingTestHelper.disableServerPredictions();

        runOnUiThreadBlocking(
                () -> {
                    mPasswordStoreBridge = new PasswordStoreBridge(mSyncTestRule.getProfile(false));
                    mBottomSheetController =
                            BottomSheetControllerProvider.from(
                                    mSyncTestRule.getActivity().getWindowAndroid());
                });

        mTestServer =
                EmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getInstrumentation().getContext(),
                        ServerCertificate.CERT_OK);
        mSyncTestRule.loadUrl(mTestServer.getURL(FORM_URL));
        mHelper = new ManualFillingTestHelper(mSyncTestRule);
        mHelper.updateWebContentsDependentState();
        mActivity = mSyncTestRule.getActivity();
    }

    @After
    public void tearDown() {
        mHelper.clear();
    }

    @Test
    @IntegrationTest
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_22W30)
    public void testAutomaticGenerationCancel() throws InterruptedException, TimeoutException {
        waitForGenerationLabel();
        focusField(PASSWORD_NODE_ID);
        dismissBottomSheet();
        // Focus again, because the sheet steals the focus from web contents.
        focusField(PASSWORD_NODE_ID);
        mHelper.waitForKeyboardAccessoryToBeShown(true);
        clickSuggestPasswordInItemsBar();
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        rejectPasswordInGenerationBottomSheet(mActivity);
        assertPasswordTextEmpty(PASSWORD_NODE_ID);
        assertNoInfobarsAreShown();
        CriteriaHelper.pollUiThread(
                () -> {
                    PasswordStoreCredential[] credentials =
                            mPasswordStoreBridge.getAllCredentials();
                    Criteria.checkThat(
                            "Should have added no passwords.", credentials.length, is(0));
                });
    }

    @Test
    @IntegrationTest
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_22W30)
    public void testManualGenerationCancel() throws InterruptedException, TimeoutException {
        waitForGenerationLabel();
        focusField(PASSWORD_NODE_ID_MANUAL);
        mHelper.waitForKeyboardAccessoryToBeShown();
        toggleAccessorySheet();
        pressManualGenerationSuggestion();
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        rejectPasswordInGenerationBottomSheet(mActivity);
        assertPasswordTextEmpty(PASSWORD_NODE_ID_MANUAL);
        assertNoInfobarsAreShown();
        CriteriaHelper.pollUiThread(
                () -> {
                    PasswordStoreCredential[] credentials =
                            mPasswordStoreBridge.getAllCredentials();
                    Criteria.checkThat(
                            "Should have added no passwords.", credentials.length, is(0));
                });
    }

    @Test
    @IntegrationTest
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_22W30)
    public void testAutomaticGenerationUsePassword() throws InterruptedException, TimeoutException {
        waitForGenerationLabel();
        focusField(PASSWORD_NODE_ID);
        dismissBottomSheet();
        // Focus again, because the sheet steals the focus from web contents.
        focusField(PASSWORD_NODE_ID);
        mHelper.waitForKeyboardAccessoryToBeShown(true);
        clickSuggestPasswordInItemsBar();
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        String generatedPassword = acceptPasswordInGenerationBottomSheet(mActivity);
        CriteriaHelper.pollInstrumentationThread(
                () -> !mHelper.getFieldText(PASSWORD_NODE_ID).isEmpty());
        assertPasswordText(PASSWORD_NODE_ID, generatedPassword);
        clickNode(SUBMIT_NODE_ID);
        ChromeTabUtils.waitForTabPageLoaded(
                mSyncTestRule.getActivity().getActivityTab(), mTestServer.getURL(DONE_URL));
        waitForMessageShown();
        CriteriaHelper.pollUiThread(
                () -> {
                    PasswordStoreCredential[] credentials =
                            mPasswordStoreBridge.getAllCredentials();
                    Criteria.checkThat(
                            "Should have added one password.", credentials.length, is(1));
                    Criteria.checkThat(credentials[0].getUsername(), is(USERNAME_TEXT));
                });
    }

    @Test
    @IntegrationTest
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_22W30)
    @DisabledTest(message = "Flakey/Failing, see crbug.com/358643071")
    public void testManualGenerationUsePassword() throws InterruptedException, TimeoutException {
        waitForGenerationLabel();
        focusField(PASSWORD_NODE_ID_MANUAL);
        mHelper.waitForKeyboardAccessoryToBeShown();
        toggleAccessorySheet();
        pressManualGenerationSuggestion();
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        String generatedPassword = acceptPasswordInGenerationBottomSheet(mActivity);
        CriteriaHelper.pollInstrumentationThread(
                () -> !mHelper.getFieldText(PASSWORD_NODE_ID_MANUAL).isEmpty());
        assertPasswordText(PASSWORD_NODE_ID_MANUAL, generatedPassword);
        clickNode(SUBMIT_NODE_ID_MANUAL);
        ChromeTabUtils.waitForTabPageLoaded(
                mSyncTestRule.getActivity().getActivityTab(), mTestServer.getURL(DONE_URL));
        waitForMessageShown();
        CriteriaHelper.pollUiThread(
                () -> {
                    PasswordStoreCredential[] credentials =
                            mPasswordStoreBridge.getAllCredentials();
                    Criteria.checkThat(
                            "Should have added one password.", credentials.length, is(1));
                    Criteria.checkThat(credentials[0].getUsername(), is(USERNAME_TEXT));
                });
    }

    private void pressManualGenerationSuggestion() {
        CriteriaHelper.pollUiThread(
                () -> {
                    return mActivity.findViewById(R.id.passwords_sheet) != null;
                });
        ArrayList<View> selectedViews = new ArrayList();
        mActivity
                .findViewById(R.id.passwords_sheet)
                .findViewsWithText(
                        selectedViews,
                        mActivity.getString(R.string.password_generation_accessory_button),
                        View.FIND_VIEWS_WITH_TEXT);
        View generationButton = selectedViews.get(0);
        runOnUiThreadBlocking(generationButton::callOnClick);
    }

    private void toggleAccessorySheet() {
        CriteriaHelper.pollUiThread(
                () -> {
                    mKeyboardAccessoryBarItems = mActivity.findViewById(R.id.bar_items_view);
                    return mKeyboardAccessoryBarItems != null;
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    return mKeyboardAccessoryBarItems.findViewHolderForLayoutPosition(0) != null;
                });
        KeyboardAccessoryButtonGroupView keyboardAccessoryView =
                (KeyboardAccessoryButtonGroupView)
                        mKeyboardAccessoryBarItems.findViewHolderForLayoutPosition(0).itemView;
        CriteriaHelper.pollUiThread(
                () -> {
                    return keyboardAccessoryView.getButtons().size()
                            == KEYBOARD_ACCESSORY_BAR_ITEM_COUNT;
                });
        ArrayList<ChromeImageButton> buttons = keyboardAccessoryView.getButtons();
        ChromeImageButton keyButton = buttons.get(0);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        runOnUiThreadBlocking(
                () -> {
                    keyButton.callOnClick();
                });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    private void clickSuggestPasswordInItemsBar() {
        CriteriaHelper.pollUiThread(
                () -> {
                    mKeyboardAccessoryBarItems = mActivity.findViewById(R.id.bar_items_view);
                    Button button =
                            (Button)
                                    mKeyboardAccessoryBarItems.findViewHolderForLayoutPosition(0)
                                            .itemView;
                    Assert.assertEquals(
                            mActivity.getString(R.string.password_generation_accessory_button),
                            button.getText());
                    button.performClick();
                });
    }

    private void focusField(String node) throws TimeoutException {
        DOMUtils.clickNode(mHelper.getWebContents(), node);
    }

    private void clickNode(String node) throws InterruptedException, TimeoutException {
        DOMUtils.clickNodeWithJavaScript(mHelper.getWebContents(), node);
    }

    private void assertPasswordTextEmpty(String passwordNode)
            throws InterruptedException, TimeoutException {
        assertPasswordText(passwordNode, "");
    }

    private void assertPasswordText(String passwordNode, String text) throws TimeoutException {
        Assert.assertEquals(text, mHelper.getFieldText(passwordNode));
    }

    private void waitForGenerationLabel() {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    String attribute =
                            mHelper.getAttribute(PASSWORD_NODE_ID, PASSWORD_ATTRIBUTE_NAME);
                    return ELIGIBLE_FOR_GENERATION.equals(attribute);
                });
    }

    private void assertNoInfobarsAreShown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertFalse(
                            InfoBarContainer.from(mSyncTestRule.getActivity().getActivityTab())
                                    .hasInfoBars());
                });
    }

    private void waitForMessageShown() {
        WindowAndroid window = mSyncTestRule.getActivity().getWindowAndroid();
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Message is not enqueued.",
                            MessagesTestHelper.getMessageCount(window),
                            Matchers.is(1));
                });
    }

    private void dismissBottomSheet() {
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mBottomSheetController.hideContent(
                                mBottomSheetController.getCurrentSheetContent(),
                                false,
                                StateChangeReason.BACK_PRESS));
        BottomSheetTestSupport.waitForState(mBottomSheetController, SheetState.HIDDEN);
    }
}
