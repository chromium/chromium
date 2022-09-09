// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.chromium.base.test.util.Matchers.is;
import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;
import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlockingNoException;

import android.view.View;
import android.view.Window;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.tabs.TabLayout;
import com.google.android.material.tabs.TabLayout.Tab;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.IntegrationTest;
import org.chromium.base.test.util.Matchers;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.password_manager.FakePasswordStoreAndroidBackend;
import org.chromium.chrome.browser.password_manager.FakePasswordStoreAndroidBackendFactoryImpl;
import org.chromium.chrome.browser.password_manager.FakePasswordSyncControllerDelegateFactoryImpl;
import org.chromium.chrome.browser.password_manager.PasswordStoreAndroidBackendFactory;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge;
import org.chromium.chrome.browser.password_manager.PasswordStoreCredential;
import org.chromium.chrome.browser.password_manager.PasswordSyncControllerDelegateFactory;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.messages.MessagesTestHelper;
import org.chromium.components.signin.AccountUtils;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.widget.ButtonCompat;

import java.util.ArrayList;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Integration tests for password generation.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "TODO(crbug.com/1346583): add resetting logic for"
                + "FakePasswordStoreAndroidBackend to allow batching")
@EnableFeatures({ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "show-autofill-signatures"})
public class PasswordGenerationIntegrationTest {
    @Rule
    public SyncTestRule mSyncTestRule = new SyncTestRule();

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

    private ManualFillingTestHelper mHelper;
    private PasswordStoreBridge mPasswordStoreBridge;
    private ChromeTabbedActivity mActivity;
    private RecyclerView mKeyboardAccessoryBarItems;
    private TextView mGeneratedPasswordTextView;

    @Before
    public void setUp() throws InterruptedException {
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

        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        ManualFillingTestHelper.disableServerPredictions();

        runOnUiThreadBlocking(() -> { mPasswordStoreBridge = new PasswordStoreBridge(); });

        mHelper = new ManualFillingTestHelper(mSyncTestRule);
        mHelper.loadTestPage(FORM_URL, false);
        mActivity = mSyncTestRule.getActivity();
    }

    @After
    public void tearDown() {
        mHelper.clear();
    }

    @Test
    @IntegrationTest
    public void testAutomaticGenerationCancel() throws InterruptedException, TimeoutException {
        waitForGenerationLabel();
        focusField(PASSWORD_NODE_ID);
        mHelper.waitForKeyboardAccessoryToBeShown(true);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ButtonCompat suggestStrongPassword =
                    (ButtonCompat) mHelper.getFirstAccessorySuggestion();
            Assert.assertNotNull(suggestStrongPassword);
            suggestStrongPassword.performClick();
        });
        waitForGenerationDialog();
        onView(withId(R.id.negative_button)).perform(click());
        assertPasswordTextEmpty(PASSWORD_NODE_ID);
        assertNoInfobarsAreShown();
        CriteriaHelper.pollUiThread(() -> {
            PasswordStoreCredential[] credentials = mPasswordStoreBridge.getAllCredentials();
            Criteria.checkThat("Should have added no passwords.", credentials.length, is(0));
        });
    }

    @Test
    @IntegrationTest
    @DisabledTest(message = "crbug.com/1353701")
    public void testManualGenerationCancel() throws InterruptedException, TimeoutException {
        waitForGenerationLabel();
        focusField(PASSWORD_NODE_ID_MANUAL);
        mHelper.waitForKeyboardAccessoryToBeShown();
        toggleAccessorySheet();
        pressManualGenerationSuggestion();
        waitForGenerationDialog();
        onView(withId(R.id.negative_button)).perform(click());
        assertPasswordTextEmpty(PASSWORD_NODE_ID_MANUAL);
        assertNoInfobarsAreShown();
        CriteriaHelper.pollUiThread(() -> {
            PasswordStoreCredential[] credentials = mPasswordStoreBridge.getAllCredentials();
            Criteria.checkThat("Should have added no passwords.", credentials.length, is(0));
        });
    }

    @Test
    @IntegrationTest
    public void testAutomaticGenerationUsePassword() throws InterruptedException, TimeoutException {
        waitForGenerationLabel();
        focusField(PASSWORD_NODE_ID);
        mHelper.waitForKeyboardAccessoryToBeShown(true);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ButtonCompat suggestStrongPassword =
                    (ButtonCompat) mHelper.getFirstAccessorySuggestion();
            Assert.assertNotNull(suggestStrongPassword);
            suggestStrongPassword.performClick();
        });
        waitForGenerationDialog();
        String generatedPassword = getTextFromTextView(R.id.generated_password);
        onView(withId(R.id.positive_button)).perform(click());
        CriteriaHelper.pollInstrumentationThread(
                () -> !mHelper.getFieldText(PASSWORD_NODE_ID).isEmpty());
        assertPasswordText(PASSWORD_NODE_ID, generatedPassword);
        clickNode(SUBMIT_NODE_ID);
        ChromeTabUtils.waitForTabPageLoaded(mSyncTestRule.getActivity().getActivityTab(),
                mHelper.getOrCreateTestServer().getURL(DONE_URL));
        waitForMessageShown();
        CriteriaHelper.pollUiThread(() -> {
            PasswordStoreCredential[] credentials = mPasswordStoreBridge.getAllCredentials();
            Criteria.checkThat("Should have added one password.", credentials.length, is(1));
            Criteria.checkThat(credentials[0].getUsername(), is(USERNAME_TEXT));
        });
    }

    @Test
    @IntegrationTest
    @DisabledTest(message = "This test is flaky.")
    public void testManualGenerationUsePassword() throws InterruptedException, TimeoutException {
        waitForGenerationLabel();
        focusField(PASSWORD_NODE_ID_MANUAL);
        mHelper.waitForKeyboardAccessoryToBeShown();
        toggleAccessorySheet();
        pressManualGenerationSuggestion();
        waitForGenerationDialog();
        String generatedPassword = getTextFromTextView(R.id.generated_password);
        onView(withId(R.id.positive_button)).perform(click());
        CriteriaHelper.pollInstrumentationThread(
                () -> !mHelper.getFieldText(PASSWORD_NODE_ID_MANUAL).isEmpty());
        assertPasswordText(PASSWORD_NODE_ID_MANUAL, generatedPassword);
        clickNode(SUBMIT_NODE_ID_MANUAL);
        ChromeTabUtils.waitForTabPageLoaded(mSyncTestRule.getActivity().getActivityTab(),
                mHelper.getOrCreateTestServer().getURL(DONE_URL));
        waitForMessageShown();
        CriteriaHelper.pollUiThread(() -> {
            PasswordStoreCredential[] credentials = mPasswordStoreBridge.getAllCredentials();
            Criteria.checkThat("Should have added one password.", credentials.length, is(1));
            Criteria.checkThat(credentials[0].getUsername(), is(USERNAME_TEXT));
        });
    }

    private void pressManualGenerationSuggestion() {
        CriteriaHelper.pollUiThread(
                () -> { return mActivity.findViewById(R.id.passwords_sheet) != null; });
        ArrayList<View> selectedViews = new ArrayList();
        (mActivity.findViewById(R.id.passwords_sheet))
                .findViewsWithText(selectedViews,
                        mActivity.getResources().getString(
                                R.string.password_generation_accessory_button),
                        View.FIND_VIEWS_WITH_TEXT);
        View generationButton = selectedViews.get(0);
        runOnUiThreadBlockingNoException(generationButton::callOnClick);
    }

    private void toggleAccessorySheet() {
        CriteriaHelper.pollInstrumentationThread(() -> {
            mKeyboardAccessoryBarItems = (RecyclerView) mActivity.findViewById(R.id.bar_items_view);
            return mKeyboardAccessoryBarItems != null;
        });
        TabLayout keyboardAccessoryView =
                (TabLayout) mKeyboardAccessoryBarItems.findViewHolderForLayoutPosition(0).itemView;
        Tab tab = keyboardAccessoryView.getTabAt(0);
        runOnUiThreadBlocking(tab::select);
    }

    private void focusField(String node) throws TimeoutException, InterruptedException {
        DOMUtils.focusNode(mHelper.getWebContents(), node);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mHelper.getWebContents().scrollFocusedEditableNodeIntoView(); });
    }

    private void clickNode(String node) throws InterruptedException, TimeoutException {
        DOMUtils.clickNodeWithJavaScript(mHelper.getWebContents(), node);
    }

    private void assertPasswordTextEmpty(String passwordNode)
            throws InterruptedException, TimeoutException {
        assertPasswordText(passwordNode, "");
    }

    private void assertPasswordText(String passwordNode, String text)
            throws InterruptedException, TimeoutException {
        Assert.assertEquals(text, mHelper.getFieldText(passwordNode));
    }

    private void waitForGenerationLabel() {
        CriteriaHelper.pollInstrumentationThread(() -> {
            String attribute = mHelper.getAttribute(PASSWORD_NODE_ID, PASSWORD_ATTRIBUTE_NAME);
            return attribute.equals(ELIGIBLE_FOR_GENERATION);
        });
    }

    private void waitForGenerationDialog() {
        waitForModalDialogPresenter();
        ModalDialogManager manager = TestThreadUtils.runOnUiThreadBlockingNoException(
                mSyncTestRule.getActivity()::getModalDialogManager);
        CriteriaHelper.pollUiThread(() -> {
            Window window = ((AppModalPresenter) manager.getCurrentPresenterForTest()).getWindow();
            mGeneratedPasswordTextView =
                    window.getDecorView().getRootView().findViewById(R.id.generated_password);
            return mGeneratedPasswordTextView != null;
        });
    }

    private void waitForModalDialogPresenter() {
        CriteriaHelper.pollUiThread(()
                                            -> mSyncTestRule.getActivity()
                                                       .getModalDialogManager()
                                                       .getCurrentPresenterForTest()
                        != null);
    }

    private void assertNoInfobarsAreShown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(InfoBarContainer.from(mSyncTestRule.getActivity().getActivityTab())
                                       .hasInfoBars());
        });
    }

    private static String getTextFromTextView(int id) {
        AtomicReference<String> textRef = new AtomicReference<>();
        onView(withId(id))
                .check((view, error) -> textRef.set(((TextView) view).getText().toString()));
        return textRef.get();
    }

    private void waitForMessageShown() {
        WindowAndroid window = mSyncTestRule.getActivity().getWindowAndroid();
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat("Message is not enqueued.",
                    MessagesTestHelper.getMessageCount(window), Matchers.is(1));
        });
    }
}
