// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.contrib.RecyclerViewActions.scrollToPosition;
import static android.support.test.espresso.matcher.ViewMatchers.assertThat;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withChild;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.notNullValue;

import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingTestHelper.selectTabAtPosition;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingTestHelper.whenDisplayed;

import android.support.test.espresso.Espresso;
import android.support.test.espresso.matcher.BoundedMatcher;
import android.support.test.filters.SmallTest;
import android.text.method.PasswordTransformationMethod;
import android.view.View;
import android.widget.TextView;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Item;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;

import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;
/**
 * Integration tests for password accessory views. This integration test currently stops testing at
 * the bridge - ideally, there should be an easy way to add a temporary account with temporary
 * passwords.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PasswordAccessoryIntegrationTest {
    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    private final ManualFillingTestHelper mHelper = new ManualFillingTestHelper(mActivityTestRule);

    @Before
    public void setUp() throws Exception {
        // TODO(crbug.com/894428) - fix this suite to use the embedded test server instead of
        // data urls.
        Features.getInstance().enable(ChromeFeatureList.AUTOFILL_ALLOW_NON_HTTP_ACTIVATION);
    }

    @After
    public void tearDown() {
        mHelper.clear();
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.PASSWORDS_KEYBOARD_ACCESSORY})
    public void testPasswordSheetIsAvailable() throws InterruptedException {
        mHelper.loadTestPage(false);

        Assert.assertNotNull("Password Sheet should be bound to accessory sheet.",
                mActivityTestRule.getActivity()
                        .getManualFillingController()
                        .getMediatorForTesting()
                        .getPasswordAccessorySheet());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.EXPERIMENTAL_UI})
    public void testPasswordSheetIsAvailableInExperimentalUi() throws InterruptedException {
        mHelper.loadTestPage(false);

        Assert.assertNotNull("Password Sheet should be bound to accessory sheet.",
                mActivityTestRule.getActivity()
                        .getManualFillingController()
                        .getMediatorForTesting()
                        .getPasswordAccessorySheet());
    }

    @Test
    @SmallTest
    @DisableFeatures(
            {ChromeFeatureList.EXPERIMENTAL_UI, ChromeFeatureList.PASSWORDS_KEYBOARD_ACCESSORY})
    public void
    testPasswordSheetUnavailableWithoutFeature() throws InterruptedException {
        mHelper.loadTestPage(false);

        Assert.assertNull("Password Sheet should not have been created.",
                mActivityTestRule.getActivity()
                        .getManualFillingController()
                        .getMediatorForTesting()
                        .getPasswordAccessorySheet());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.PASSWORDS_KEYBOARD_ACCESSORY})
    @FlakyTest(message = "https://crbug.com/854326")
    public void testPasswordSheetDisplaysProvidedItems()
            throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);
        provideItems(new Item[] {Item.createLabel("Passwords", "Description_Passwords"),
                createSuggestion("mayapark@gmail.com", (item) -> {}),
                createPassword("SomeHiddenPassword"),
                createSuggestion("mayaelisabethmercedesgreenepark@googlemail.com", (item) -> {}),
                createPassword("ExtremelyLongPasswordThatUsesQuiteSomeSpaceInTheSheet")});

        // Focus the field to bring up the accessory.
        mHelper.clickPasswordField();
        mHelper.waitForKeyboard();
        whenDisplayed(withId(R.id.tabs)).perform(selectTabAtPosition(0));

        // Check that the provided elements are there.
        whenDisplayed(withText("Passwords"));
        whenDisplayed(withText("mayapark@gmail.com"));
        whenDisplayed(withText("SomeHiddenPassword")).check(matches(isTransformed()));
        // TODO(fhorschig): Figure out whether the long name should be cropped or wrapped.
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.PASSWORDS_KEYBOARD_ACCESSORY})
    @FlakyTest(message = "https://crbug.com/854326")
    public void testPasswordSheetDisplaysNoPasswordsMessageAndOptions()
            throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);
        final AtomicReference<Item> clicked = new AtomicReference<>();
        provideItems(new Item[] {
                Item.createLabel("No saved passwords for abc.com", "Description_Passwords"),
                Item.createDivider(),
                Item.createOption(
                        "Suggest strong password...", "Description_Generate", clicked::set),
                Item.createOption("Manage passwords...", "Description_Manage", (item) -> {})});

        // Focus the field to bring up the accessory.
        mHelper.clickPasswordField();
        mHelper.waitForKeyboard();
        whenDisplayed(withId(R.id.tabs)).perform(selectTabAtPosition(0));

        // Scroll down and click the suggestion.
        whenDisplayed(withChild(withText("Suggest strong password...")))
                .perform(scrollToPosition(2));
        whenDisplayed(withText("Suggest strong password...")).perform(click());

        // The callback should have triggered and set the reference to the selected Item.
        assertThat(clicked.get(), notNullValue());
        assertThat(clicked.get().getCaption(), equalTo("Suggest strong password..."));
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.PASSWORDS_KEYBOARD_ACCESSORY})
    @FlakyTest(message = "crbug.com/855617")
    public void testPasswordSheetTriggersCallback() throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);
        final AtomicReference<Item> clicked = new AtomicReference<>();
        provideItems(new Item[] {
                Item.createLabel("Passwords", "Description_Passwords"),
                createSuggestion("mpark@abc.com", null), createPassword("ShorterPassword"),
                createSuggestion("mayap@xyz.com", null), createPassword("PWD"),
                createSuggestion("park@googlemail.com", null), createPassword("P@$$W0rt"),
                createSuggestion("mayapark@gmail.com", clicked::set),
                createPassword("SomeHiddenLongPassword"),
        });

        // Focus the field to bring up the accessory.
        mHelper.clickPasswordField();
        mHelper.waitForKeyboard();
        whenDisplayed(withId(R.id.tabs)).perform(selectTabAtPosition(0));

        // Scroll down and click the suggestion.
        whenDisplayed(withChild(withText("Passwords"))).perform(scrollToPosition(7));
        whenDisplayed(withText("mayapark@gmail.com")).perform(click());

        // The callback should have triggered and set the reference to the selected Item.
        assertThat(clicked.get(), notNullValue());
        assertThat(clicked.get().getCaption(), equalTo("mayapark@gmail.com"));
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.PASSWORDS_KEYBOARD_ACCESSORY})
    @FlakyTest(message = "crbug.com/855617")
    public void testDisplaysEmptyStateMessageWithoutSavedPasswords()
            throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);

        // Focus the field to bring up the accessory.
        mHelper.clickPasswordField();
        mHelper.waitForKeyboard();

        // Click the tab to show the sheet and hide the keyboard.
        whenDisplayed(withId(R.id.tabs)).perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withId(R.id.keyboard_accessory_sheet));
        onView(withText(containsString("No saved passwords"))).check(matches(isDisplayed()));

        Espresso.pressBack();

        mHelper.waitToBeHidden(withId(R.id.keyboard_accessory_sheet));
        mHelper.waitToBeHidden(withId(R.id.keyboard_accessory));
    }

    /**
     * Matches any {@link TextView} which applies a {@link PasswordTransformationMethod}.
     * @return The matcher checking the transformation method.
     */
    private static Matcher<View> isTransformed() {
        return new BoundedMatcher<View, TextView>(TextView.class) {
            @Override
            public boolean matchesSafely(TextView textView) {
                return textView.getTransformationMethod() instanceof PasswordTransformationMethod;
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("is a transformed password.");
            }
        };
    }

    /**
     * Creates a provider, binds it to the accessory sheet like a bridge would do and notifies about
     * the given |items|.
     * @param items The items to be provided to the password accessory sheet.
     */
    private void provideItems(Item[] items) {
        KeyboardAccessoryData.PropertyProvider<Item> itemProvider =
                new KeyboardAccessoryData.PropertyProvider<>();
        mActivityTestRule.getActivity()
                .getManualFillingController()
                .getMediatorForTesting()
                .getPasswordAccessorySheet()
                .registerItemProvider(itemProvider);
        itemProvider.notifyObservers(items);
    }

    private static Item createSuggestion(String caption, Callback<Item> callback) {
        return Item.createSuggestion(caption, "Description_" + caption, false, callback, null);
    }

    private static Item createPassword(String caption) {
        return Item.createSuggestion(caption, "Description_" + caption, true, null, null);
    }
}
