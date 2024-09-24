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
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.containsString;

import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.selectTabWithDescription;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.whenDisplayed;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.ChromeWindow;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.keyboard_accessory.FakeKeyboard;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.button_group_component.KeyboardAccessoryButtonGroupView;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.TimeoutException;

/** Integration tests for credit card accessory views. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class CreditCardAccessoryIntegrationTest {
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
                "/chrome/test/data/autofill/autofill_creditcard_form.html",
                false,
                false,
                keyboardDelegate);
        CreditCard card = new CreditCard();
        card.setName("Kirby Puckett");
        card.setNumber("4111111111111111");
        card.setMonth("03");
        card.setYear("2034");

        new AutofillTestHelper().setCreditCard(card);
        DOMUtils.waitForNonZeroNodeBounds(mHelper.getWebContents(), "CREDIT_CARD_NAME_FULL");
    }

    @Test
    @SmallTest
    public void testCreditCardSheetAvailable_whenManualFallbackEnabled() {
        mHelper.loadTestPage(false);

        CriteriaHelper.pollUiThread(
                () -> {
                    return mHelper.getOrCreateCreditCardAccessorySheet() != null;
                },
                "Credit Card sheet should be bound to accessory sheet.");
    }

    @Test
    @SmallTest
    @DisableIf.Device(DeviceFormFactor.TABLET) // https://crbug.com/1182626
    public void testDisplaysEmptyStateMessageWithoutSavedCards() throws TimeoutException {
        mHelper.loadTestPage(false);

        // Focus the field to bring up the accessory.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();

        // Click the tab to show the sheet and hide the keyboard.
        whenDisplayed(withId(R.id.bar_items_view))
                .perform(
                        scrollTo(isAssignableFrom(KeyboardAccessoryButtonGroupView.class)),
                        actionOnItem(
                                isAssignableFrom(KeyboardAccessoryButtonGroupView.class),
                                selectTabWithDescription(
                                        R.string.credit_card_accessory_sheet_toggle)));

        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withId(R.id.credit_card_sheet));
        onView(withText(containsString("No saved payment methods"))).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1392789, https://crbug.com/1182626")
    public void testFillsSuggestionOnClick() throws TimeoutException {
        loadTestPage(FakeKeyboard::new);
        mHelper.clickNodeAndShowKeyboard("CREDIT_CARD_NAME_FULL", 1);
        DOMUtils.focusNode(mActivityTestRule.getWebContents(), "CREDIT_CARD_NAME_FULL");
        mHelper.waitForKeyboardAccessoryToBeShown(true);

        // Scroll to last element and click the first icon:
        whenDisplayed(withId(R.id.bar_items_view))
                .perform(
                        scrollTo(isAssignableFrom(KeyboardAccessoryButtonGroupView.class)),
                        actionOnItem(
                                isAssignableFrom(KeyboardAccessoryButtonGroupView.class),
                                selectTabWithDescription(
                                        R.string.credit_card_accessory_sheet_toggle)));

        // Wait for the sheet to come up and be stable.
        whenDisplayed(withId(R.id.credit_card_sheet));

        // Click a suggestion.
        whenDisplayed(withId(R.id.cc_number)).perform(click());

        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    return mHelper.getFieldText("CREDIT_CARD_NAME_FULL").equals("4111111111111111");
                });
    }
}
