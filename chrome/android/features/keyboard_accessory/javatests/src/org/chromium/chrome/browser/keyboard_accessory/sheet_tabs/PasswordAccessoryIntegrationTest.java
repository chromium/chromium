// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isAssignableFrom;
import static androidx.test.espresso.matcher.ViewMatchers.isChecked;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isNotChecked;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.containsString;

import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.isTransformed;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.scrollToLastElement;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.selectTabAtPosition;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.whenDisplayed;

import android.os.Looper;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.button_group_component.KeyboardAccessoryButtonGroupView;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge;
import org.chromium.chrome.browser.password_manager.PasswordStoreCredential;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

/** Integration tests for password accessory views. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "show-autofill-signatures"})
public class PasswordAccessoryIntegrationTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private EmbeddedTestServer mTestServer;
    private PasswordStoreBridge mPasswordStoreBridge;
    private final ManualFillingTestHelper mHelper = new ManualFillingTestHelper(mActivityTestRule);

    @After
    public void tearDown() {
        mHelper.clear();
    }

    @Test
    @SmallTest
    public void testPasswordSheetIsAvailable() {
        mHelper.loadTestPage(false);

        CriteriaHelper.pollUiThread(
                () -> {
                    return mHelper.getOrCreatePasswordAccessorySheet() != null;
                },
                " Password Sheet should be bound to accessory sheet.");
    }

    @Test
    @MediumTest
    public void testPasswordSheetDisplaysProvidedItems() throws TimeoutException {
        preparePasswordBridge();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPasswordStoreBridge.insertPasswordCredential(
                            new PasswordStoreCredential(
                                    new GURL(mTestServer.getURL("/")),
                                    "mayapark@gmail.com",
                                    "SomeHiddenPassword"));
                });
        mActivityTestRule.loadUrl(
                mTestServer.getURL("/chrome/test/data/password/password_form.html"));
        mHelper.focusPasswordField(false);
        mHelper.waitForKeyboardAccessoryToBeShown();
        mHelper.waitForKeyboardToShow();
        whenDisplayed(isAssignableFrom(KeyboardAccessoryButtonGroupView.class))
                .perform(selectTabAtPosition(0));

        // Check that the provided elements are there.
        whenDisplayed(withText("mayapark@gmail.com"));
        whenDisplayed(withText("SomeHiddenPassword")).check(matches(isTransformed()));
    }

    private void preparePasswordBridge() {
        Looper.prepare();
        mActivityTestRule.startMainActivityOnBlankPage();
        mTestServer = mActivityTestRule.getTestServer();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPasswordStoreBridge =
                            new PasswordStoreBridge(mActivityTestRule.getProfile(false));
                });
    }

    @Test
    @SmallTest
    public void testPasswordSheetDisplaysOptions() throws TimeoutException {
        mHelper.loadTestPage(false);
        // Marking the origin as denylisted shows only a very minimal accessory.
        mHelper.cacheCredentials(new String[0], new String[0], true);

        // Focus the field to bring up the accessory.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();
        whenDisplayed(isAssignableFrom(KeyboardAccessoryButtonGroupView.class))
                .perform(selectTabAtPosition(0));

        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withId(R.id.passwords_sheet)).perform(scrollToLastElement());
        onView(withText(containsString("Manage password"))).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @DisableIf.Device(DeviceFormFactor.TABLET) // https://crbug.com/1111770
    @DisableFeatures({ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_LOCAL_PWD_MIGRATION_WARNING})
    public void testFillsPasswordOnTap() throws TimeoutException {
        preparePasswordBridge();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPasswordStoreBridge.insertPasswordCredential(
                            new PasswordStoreCredential(
                                    new GURL(mTestServer.getURL("/")),
                                    "mpark@abc.com",
                                    "ShorterPassword"));
                });
        mHelper.loadUrl("/chrome/test/data/password/password_form.html");
        mHelper.focusPasswordField(false);
        mHelper.waitForKeyboardAccessoryToBeShown();
        mHelper.waitForKeyboardToShow();
        whenDisplayed(isAssignableFrom(KeyboardAccessoryButtonGroupView.class))
                .perform(selectTabAtPosition(0));

        // Click the suggestion.
        whenDisplayed(withText("ShorterPassword")).perform(click());
        // The callback should have triggered and set the reference to the selected Item.
        CriteriaHelper.pollInstrumentationThread(
                () -> mHelper.getPasswordText().equals("ShorterPassword"));
    }

    @Test
    @SmallTest
    public void testDisplaysEmptyStateMessageWithoutSavedPasswords() throws TimeoutException {
        mHelper.loadTestPage(false);
        // Mark the origin as denylisted to have a reason to show the accessory in the first place.
        mHelper.cacheCredentials(new String[0], new String[0], true);

        // Focus the field to bring up the accessory.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();

        // Click the tab to show the sheet and hide the keyboard.
        whenDisplayed(isAssignableFrom(KeyboardAccessoryButtonGroupView.class))
                .perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withId(R.id.passwords_sheet));
        onView(withText(containsString("No saved passwords"))).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1503085")
    public void testEnablesUndenylistingToggle() throws TimeoutException, InterruptedException {
        preparePasswordBridge();
        String url = mTestServer.getURL("/chrome/test/data/password/password_form.html");
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPasswordStoreBridge.blocklistForTesting(url);
                });
        mActivityTestRule.loadUrl(url);
        mHelper.focusPasswordField(false);
        mHelper.waitForKeyboardAccessoryToBeShown();
        mHelper.waitForKeyboardToShow();
        whenDisplayed(isAssignableFrom(KeyboardAccessoryButtonGroupView.class))
                .perform(selectTabAtPosition(0));

        whenDisplayed(withId(R.id.option_toggle_switch)).check(matches(isNotChecked()));
        onView(withId(R.id.option_toggle_subtitle)).check(matches(withText(R.string.text_off)));

        whenDisplayed(withId(R.id.option_toggle_switch)).perform(click());
        onView(withId(R.id.option_toggle_switch)).check(matches(isChecked()));
        onView(withId(R.id.option_toggle_subtitle)).check(matches(withText(R.string.text_on)));
    }
}
