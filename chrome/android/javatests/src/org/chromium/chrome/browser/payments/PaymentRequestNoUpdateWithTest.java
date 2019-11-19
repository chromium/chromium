// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.CardType;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.MainActivityStartCallback;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DisableAnimationsTestRule;
import org.chromium.ui.modaldialog.ModalDialogProperties;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that either does not listen to update callbacks, does
 * not call updateWith(), or does not uses promises. These behaviors are all OK and should not cause
 * timeouts.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestNoUpdateWithTest implements MainActivityStartCallback {
    // Disable animations to reduce flakiness.
    @ClassRule
    public static DisableAnimationsTestRule sNoAnimationsRule = new DisableAnimationsTestRule();

    @Rule
    public PaymentRequestTestRule mRule =
            new PaymentRequestTestRule("payment_request_no_update_with_test.html", this);

    @Override
    public void onMainActivityStarted() throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        helper.setProfile(new AutofillProfile("" /* guid */, "https://www.example.com" /* origin */,
                "Lisa Simpson", "Acme Inc.", "123 Main", "California", "Los Angeles", "", "90210",
                "", "US", "555 123-4567", "lisa@simpson.com", ""));
        String billingAddressId = helper.setProfile(
                new AutofillProfile("" /* guid */, "https://www.example.com" /* origin */,
                        "Maggie Simpson", "Acme Inc.", "123 Main", "California", "Los Angeles", "",
                        "90210", "", "Uzbekistan", "555 123-4567", "maggie@simpson.com", ""));
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.visa_card,
                CardType.UNKNOWN, billingAddressId, "" /* serverId */));
    }

    /**
     * A merchant that does not listen to shipping address update events will not cause timeouts in
     * UI.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNoEventListener() throws Throwable {
        mRule.triggerUIAndWait("buyWithoutListeners", mRule.getReadyForInput());
        mRule.clickInShippingAddressAndWait(R.id.payments_section, mRule.getReadyForInput());
        mRule.clickOnShippingAddressSuggestionOptionAndWait(1, mRule.getReadyForInput());
        mRule.clickAndWait(R.id.button_primary, mRule.getReadyForUnmaskInput());
        mRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mRule.getReadyToUnmask());
        mRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mRule.getDismissed());
        mRule.expectResultContains(new String[] {"freeShipping"});
    }

    /**
     * A merchant that listens to shipping address update events, but does not call updateWith() on
     * the event, will not cause timeouts in UI.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNoUpdateWith() throws Throwable {
        mRule.triggerUIAndWait("buyWithoutCallingUpdateWith", mRule.getReadyForInput());
        mRule.clickInShippingAddressAndWait(R.id.payments_section, mRule.getReadyForInput());
        mRule.clickOnShippingAddressSuggestionOptionAndWait(1, mRule.getReadyForInput());
        mRule.clickAndWait(R.id.button_primary, mRule.getReadyForUnmaskInput());
        mRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mRule.getReadyToUnmask());
        mRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mRule.getDismissed());
        mRule.expectResultContains(new String[] {"freeShipping"});
    }

    /** A merchant that calls updateWith() without using promises will not cause timeouts in UI. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNoPromises() throws Throwable {
        mRule.triggerUIAndWait("buyWithoutPromises", mRule.getReadyForInput());
        Assert.assertEquals("USD $5.00", mRule.getOrderSummaryTotal());
        mRule.clickInShippingAddressAndWait(R.id.payments_section, mRule.getReadyForInput());
        mRule.clickOnShippingAddressSuggestionOptionAndWait(1, mRule.getReadyForInput());
        Assert.assertEquals("USD $10.00", mRule.getOrderSummaryTotal());
        mRule.clickAndWait(R.id.button_primary, mRule.getReadyForUnmaskInput());
        mRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mRule.getReadyToUnmask());
        mRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mRule.getDismissed());
        mRule.expectResultContains(new String[] {"updatedShipping"});
    }
}