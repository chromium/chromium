// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.contrib.RecyclerViewActions.actionOnItem;
import static androidx.test.espresso.contrib.RecyclerViewActions.scrollTo;
import static androidx.test.espresso.matcher.ViewMatchers.isAssignableFrom;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.not;

import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.selectTabWithDescription;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.whenDisplayed;

import android.widget.TextView;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.ChromeWindow;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.keyboard_accessory.FakeKeyboard;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.button_group_component.KeyboardAccessoryButtonGroupView;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.content_public.browser.test.util.DOMUtils;

import java.util.concurrent.TimeoutException;

/** Integration tests for address accessory views. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AddressAccessoryIntegrationTest {
    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    private final ManualFillingTestHelper mHelper = new ManualFillingTestHelper(mActivityTestRule);

    @After
    public void tearDown() {
        mHelper.clear();
    }

    private void loadTestPage(ChromeWindow.KeyboardVisibilityDelegateFactory keyboardDelegate)
            throws TimeoutException {
        mHelper.loadTestPage(
                "/chrome/test/data/autofill/autofill_test_form.html",
                false,
                false,
                keyboardDelegate);
        new AutofillTestHelper()
                .setProfile(
                        AutofillProfile.builder()
                                .setFullName("Marcus McSpartangregor")
                                .setCompanyName("Acme Inc")
                                .setStreetAddress("1 Main\nApt A")
                                .setRegion("CA")
                                .setLocality("San Francisco")
                                .setPostalCode("94102")
                                .setCountryCode("US")
                                .setPhoneNumber("(415) 999-0000")
                                .setEmailAddress("marc@acme-mail.inc")
                                .setLanguageCode("en")
                                .build());
        DOMUtils.waitForNonZeroNodeBounds(mHelper.getWebContents(), "NAME_FIRST");
    }

    @Test
    @SmallTest
    public void testAddressSheetIsAvailable() {
        mHelper.loadTestPage(false);

        CriteriaHelper.pollUiThread(
                () -> {
                    return mHelper.getOrCreateAddressAccessorySheet() != null;
                },
                "Address sheet should be bound to accessory sheet.");
    }

    @Test
    @SmallTest
    public void testDisplaysEmptyStateMessageWithoutSavedAddresses() throws TimeoutException {
        mHelper.loadTestPage(false);

        // Focus the field to bring up the accessory.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();

        // Click the tab to show the sheet and hide the keyboard.
        whenDisplayed(
                        allOf(
                                withContentDescription(R.string.address_accessory_sheet_toggle),
                                not(isAssignableFrom(TextView.class))))
                .perform(click());
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withId(R.id.addresses_sheet));
        onView(withText(containsString("No saved addresses"))).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testFillsSuggestionOnClick() throws TimeoutException {
        loadTestPage(FakeKeyboard::new);
        mHelper.clickNodeAndShowKeyboard("NAME_FIRST", 1);
        mHelper.waitForKeyboardAccessoryToBeShown(true);

        // Scroll to last element and click the second icon:
        whenDisplayed(withId(R.id.bar_items_view))
                .perform(
                        scrollTo(isAssignableFrom(KeyboardAccessoryButtonGroupView.class)),
                        actionOnItem(
                                isAssignableFrom(KeyboardAccessoryButtonGroupView.class),
                                selectTabWithDescription(R.string.address_accessory_sheet_toggle)));

        // Wait for the sheet to come up and be stable.
        whenDisplayed(withId(R.id.addresses_sheet));

        // Click a suggestion.
        whenDisplayed(withText("Marcus McSpartangregor")).perform(click());

        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    return mHelper.getFieldText("NAME_FIRST").equals("Marcus McSpartangregor");
                });
    }
}
