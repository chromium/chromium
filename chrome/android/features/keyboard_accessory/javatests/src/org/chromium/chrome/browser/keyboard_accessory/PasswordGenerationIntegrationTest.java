// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.core.AllOf.allOf;

import static org.chromium.chrome.R.id.password_generation_dialog;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.selectTabAtPosition;

import android.view.Window;
import android.widget.TextView;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.IntegrationTest;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.modaldialog.AppModalPresenter;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.widget.ButtonCompat;

import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Integration tests for password generation.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
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
    private static final String PASSWORD_ATTRIBUTE_NAME = "password_creation_field";
    private static final String ELIGIBLE_FOR_GENERATION = "1";
    private static final String INFOBAR_MESSAGE = "Password saved";

    private final ManualFillingTestHelper mHelper = new ManualFillingTestHelper(mSyncTestRule);

    @Before
    public void setUp() throws InterruptedException {
        mSyncTestRule.setUpTestAccountAndSignIn();
        mHelper.loadTestPage(FORM_URL, false);
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
    }

    @Test
    @IntegrationTest
    @DisabledTest(message = "crbug.com/1010540")
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
    }

    @Test
    @IntegrationTest
    @DisabledTest(message = "crbug.com/1010344")
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
        assertPasswordText(PASSWORD_NODE_ID, generatedPassword);
        clickNode(SUBMIT_NODE_ID);
        mHelper.waitForViewOnActivityRoot(withId(R.id.infobar_message))
                .check(matches(withText(INFOBAR_MESSAGE)));
    }

    @Test
    @IntegrationTest
    @DisabledTest(message = "crbug.com/1010540")
    public void testManualGenerationUsePassword() throws InterruptedException, TimeoutException {
        waitForGenerationLabel();
        focusField(PASSWORD_NODE_ID_MANUAL);
        mHelper.waitForKeyboardAccessoryToBeShown();
        toggleAccessorySheet();
        pressManualGenerationSuggestion();
        waitForGenerationDialog();
        String generatedPassword = getTextFromTextView(R.id.generated_password);
        onView(withId(R.id.positive_button)).perform(click());
        assertPasswordText(PASSWORD_NODE_ID_MANUAL, generatedPassword);
        clickNode(SUBMIT_NODE_ID_MANUAL);
        mHelper.waitForViewOnActivityRoot(withId(R.id.infobar_message))
                .check(matches(withText(INFOBAR_MESSAGE)));
    }

    public void pressManualGenerationSuggestion() {
        onView(allOf(isDescendantOfA(withId(R.id.passwords_sheet)),
                       withText(R.string.password_generation_accessory_button)))
                .perform(click());
    }

    public void toggleAccessorySheet() {
        mHelper.waitForViewOnActivityRoot(withId(R.id.tabs)).perform(selectTabAtPosition(0));
    }

    public void focusField(String node) throws TimeoutException, InterruptedException {
        DOMUtils.focusNode(mHelper.getWebContents(), node);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mHelper.getWebContents().scrollFocusedEditableNodeIntoView(); });
    }

    public void clickNode(String node) throws InterruptedException, TimeoutException {
        DOMUtils.clickNode(mHelper.getWebContents(), node);
    }

    private void assertPasswordTextEmpty(String passwordNode)
            throws InterruptedException, TimeoutException {
        assertPasswordText(passwordNode, "");
    }

    private void assertPasswordText(String passwordNode, String text)
            throws InterruptedException, TimeoutException {
        Assert.assertEquals(mHelper.getFieldText(passwordNode), text);
    }

    private void waitForGenerationLabel() {
        CriteriaHelper.pollInstrumentationThread(() -> {
            String attribute = mHelper.getAttribute(PASSWORD_NODE_ID, PASSWORD_ATTRIBUTE_NAME);
            return attribute.equals(ELIGIBLE_FOR_GENERATION);
        });
    }

    private void waitForGenerationDialog() {
        waitForModalDialogPresenter();
        Window window = ((AppModalPresenter) mSyncTestRule.getActivity()
                                 .getModalDialogManager()
                                 .getCurrentPresenterForTest())
                                .getWindow();
        mHelper.waitForViewOnRoot(
                window.getDecorView().getRootView(), withId(password_generation_dialog));
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

    public static String getTextFromTextView(int id) {
        AtomicReference<String> textRef = new AtomicReference<>();
        onView(withId(id))
                .check((view, error) -> textRef.set(((TextView) view).getText().toString()));
        return textRef.get();
    }
}
