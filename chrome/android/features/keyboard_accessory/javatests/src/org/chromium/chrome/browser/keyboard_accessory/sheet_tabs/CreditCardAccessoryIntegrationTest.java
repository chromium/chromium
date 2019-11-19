// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.contrib.RecyclerViewActions.actionOnItem;
import static android.support.test.espresso.contrib.RecyclerViewActions.scrollTo;
import static android.support.test.espresso.matcher.ViewMatchers.isAssignableFrom;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withContentDescription;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.not;

import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.selectTabAtPosition;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.whenDisplayed;
import static org.chromium.chrome.browser.keyboard_accessory.tab_layout_component.KeyboardAccessoryTabTestHelper.isKeyboardAccessoryTabLayout;

import android.os.Build;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.widget.TextView;

import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeWindow;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.keyboard_accessory.FakeKeyboard;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.DOMUtils;

import java.util.concurrent.TimeoutException;

/**
 * Integration tests for credit card accessory views.
 */

@RunWith(ChromeJUnit4ClassRunner.class)
@DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.LOLLIPOP, message = "crbug.com/958631")
@RetryOnFailure
@EnableFeatures({ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY})
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
        mHelper.loadTestPage("/chrome/test/data/autofill/autofill_creditcard_form.html", false,
                false, keyboardDelegate);
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
    @EnableFeatures({ChromeFeatureList.AUTOFILL_MANUAL_FALLBACK_ANDROID})
    public void testCreditCardSheetAvailable() {
        mHelper.loadTestPage(false);

        CriteriaHelper.pollUiThread(() -> {
            return mHelper.getOrCreateCreditCardAccessorySheet() != null;
        }, "Credit Card sheet should be bound to accessory sheet.");
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.AUTOFILL_MANUAL_FALLBACK_ANDROID})
    public void testCreditCardSheetUnavailableWithoutFeature() {
        mHelper.loadTestPage(false);

        Assert.assertNull("Credit Card sheet should not have been created.",
                mHelper.getOrCreateCreditCardAccessorySheet());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_MANUAL_FALLBACK_ANDROID})
    public void testDisplaysEmptyStateMessageWithoutSavedCards() throws TimeoutException {
        mHelper.loadTestPage(false);

        // Focus the field to bring up the accessory.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();

        // Click the tab to show the sheet and hide the keyboard.
        whenDisplayed(allOf(withContentDescription(R.string.credit_card_accessory_sheet_toggle),
                              not(isAssignableFrom(TextView.class))))
                .perform(click());
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withId(R.id.credit_card_sheet));
        onView(withText(containsString("No saved payment methods"))).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_MANUAL_FALLBACK_ANDROID})
    public void testFillsSuggestionOnClick() throws TimeoutException {
        loadTestPage(FakeKeyboard::new);
        mHelper.clickNodeAndShowKeyboard("CREDIT_CARD_NAME_FULL");
        DOMUtils.focusNode(mActivityTestRule.getWebContents(), "CREDIT_CARD_NAME_FULL");
        mHelper.waitForKeyboardAccessoryToBeShown(true);

        // Scroll to last element and click the second icon:
        whenDisplayed(withId(R.id.bar_items_view))
                .perform(scrollTo(isKeyboardAccessoryTabLayout()),
                        actionOnItem(isKeyboardAccessoryTabLayout(), selectTabAtPosition(1)));

        // Wait for the sheet to come up and be stable.
        whenDisplayed(withId(R.id.credit_card_sheet));

        // Click a suggestion.
        whenDisplayed(withId(R.id.cc_number)).perform(click());

        CriteriaHelper.pollInstrumentationThread(() -> {
            return mHelper.getFieldText("CREDIT_CARD_NAME_FULL").equals("4111111111111111");
        });
    }
}
