// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
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
import static org.chromium.chrome.browser.keyboard_accessory.tab_layout_component.KeyboardAccessoryTabTestHelper.isKeyboardAccessoryTabLayout;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.ui.test.util.UiDisableIf;

import java.util.concurrent.TimeoutException;

/**
 * Integration tests for password accessory views.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PasswordAccessoryIntegrationTest {
    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    private final ManualFillingTestHelper mHelper = new ManualFillingTestHelper(mActivityTestRule);

    @After
    public void tearDown() {
        mHelper.clear();
    }

    @Test
    @SmallTest
    public void testPasswordSheetIsAvailable() {
        mHelper.loadTestPage(false);

        CriteriaHelper.pollUiThread(() -> {
            return mHelper.getOrCreatePasswordAccessorySheet() != null;
        }, " Password Sheet should be bound to accessory sheet.");
    }

    @Test
    @SmallTest
    public void testPasswordSheetDisplaysProvidedItems() throws TimeoutException {
        mHelper.loadTestPage(false);
        mHelper.cacheCredentials("mayapark@gmail.com", "SomeHiddenPassword");

        // Focus the field to bring up the accessory.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();
        whenDisplayed(isKeyboardAccessoryTabLayout()).perform(selectTabAtPosition(0));

        // Check that the provided elements are there.
        whenDisplayed(withText("mayapark@gmail.com"));
        whenDisplayed(withText("SomeHiddenPassword")).check(matches(isTransformed()));
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.RECOVER_FROM_NEVER_SAVE_ANDROID,
            ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY})
    public void
    testPasswordSheetDisplaysOptions() throws TimeoutException {
        mHelper.loadTestPage(false);
        // Marking the origin as denylisted shows only a very minimal accessory.
        mHelper.cacheCredentials(new String[0], new String[0], true);

        // Focus the field to bring up the accessory.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();
        whenDisplayed(isKeyboardAccessoryTabLayout()).perform(selectTabAtPosition(0));

        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withId(R.id.passwords_sheet)).perform(scrollToLastElement());
        onView(withText(containsString("Manage password"))).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @DisableIf.Device(type = {UiDisableIf.TABLET}) // https://crbug.com/1111770
    public void testFillsPasswordOnTap() throws TimeoutException {
        mHelper.loadTestPage(false);
        mHelper.cacheCredentials("mpark@abc.com", "ShorterPassword");

        // Focus the field to bring up the accessory.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();
        whenDisplayed(isKeyboardAccessoryTabLayout()).perform(selectTabAtPosition(0));

        // Click the suggestion.
        whenDisplayed(withText("ShorterPassword")).perform(click());

        // The callback should have triggered and set the reference to the selected Item.
        CriteriaHelper.pollInstrumentationThread(
                () -> mHelper.getPasswordText().equals("ShorterPassword"));
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug/1365613")
    @EnableFeatures({ChromeFeatureList.RECOVER_FROM_NEVER_SAVE_ANDROID,
            ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY})
    public void
    testDisplaysEmptyStateMessageWithoutSavedPasswords() throws TimeoutException {
        mHelper.loadTestPage(false);
        // Mark the origin as denylisted to have a reason to show the accessory in the first place.
        mHelper.cacheCredentials(new String[0], new String[0], true);

        // Focus the field to bring up the accessory.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();

        // Click the tab to show the sheet and hide the keyboard.
        whenDisplayed(isKeyboardAccessoryTabLayout()).perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withId(R.id.passwords_sheet));
        onView(withText(containsString("No saved passwords"))).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.RECOVER_FROM_NEVER_SAVE_ANDROID,
            ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY})
    public void
    testEnablesUndenylistingToggle() throws TimeoutException, InterruptedException {
        mHelper.loadTestPage(false);
        mHelper.cacheCredentials(new String[0], new String[0], true);

        // Focus the field to bring up the accessory.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();
        whenDisplayed(isKeyboardAccessoryTabLayout()).perform(selectTabAtPosition(0));

        whenDisplayed(withId(R.id.option_toggle_switch)).check(matches(isNotChecked()));
        onView(withId(R.id.option_toggle_subtitle)).check(matches(withText(R.string.text_off)));

        whenDisplayed(withId(R.id.option_toggle_switch)).perform(click());
        onView(withId(R.id.option_toggle_switch)).check(matches(isChecked()));
        onView(withId(R.id.option_toggle_subtitle)).check(matches(withText(R.string.text_on)));
    }
}
