// Copyright 2019 The Chromium Authors. All rights reserved.
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
 * A payment integration test for updateWith().
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestUpdateWithTest implements MainActivityStartCallback {
    // Disable animations to reduce flakiness.
    @ClassRule
    public static DisableAnimationsTestRule sNoAnimationsRule = new DisableAnimationsTestRule();

    @Rule
    public PaymentRequestTestRule mRule =
            new PaymentRequestTestRule("payment_request_update_with_test.html", this);

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
     * A merchant that calls updateWith() with {} will not cause timeouts in UI.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testUpdateWithEmpty() throws Throwable {
        mRule.triggerUIAndWait("updateWithEmpty", mRule.getReadyToPay());
        mRule.clickInOrderSummaryAndWait(mRule.getReadyToPay());
        Assert.assertEquals("USD $5.00", mRule.getOrderSummaryTotal());
        Assert.assertEquals("$2.00", mRule.getLineItemAmount(0));
        Assert.assertEquals("$3.00", mRule.getLineItemAmount(1));
        Assert.assertEquals("$0.00", mRule.getLineItemAmount(2));
        Assert.assertEquals("USD $5.00", mRule.getOrderSummaryTotal());
        Assert.assertEquals("$2.00", mRule.getLineItemAmount(0));
        Assert.assertEquals("$3.00", mRule.getLineItemAmount(1));
        Assert.assertEquals("$0.00", mRule.getLineItemAmount(2));
        mRule.clickAndWait(R.id.button_primary, mRule.getReadyForUnmaskInput());
        mRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mRule.getReadyToUnmask());
        mRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mRule.getDismissed());
        mRule.expectResultContains(new String[] {"freeShipping"});
    }

    /** A merchant that calls updateWith() with total will not cause timeouts in UI. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testUpdateWithTotal() throws Throwable {
        mRule.triggerUIAndWait("updateWithTotal", mRule.getReadyToPay());
        mRule.clickInOrderSummaryAndWait(mRule.getReadyToPay());
        Assert.assertEquals("USD $5.00", mRule.getOrderSummaryTotal());
        Assert.assertEquals("$2.00", mRule.getLineItemAmount(0));
        Assert.assertEquals("$3.00", mRule.getLineItemAmount(1));
        Assert.assertEquals("$0.00", mRule.getLineItemAmount(2));
        mRule.clickInShippingAddressAndWait(R.id.payments_section, mRule.getReadyToPay());
        mRule.clickOnShippingAddressSuggestionOptionAndWait(1, mRule.getReadyToPay());
        mRule.clickInOrderSummaryAndWait(mRule.getReadyToPay());
        Assert.assertEquals("USD $10.00", mRule.getOrderSummaryTotal());
        Assert.assertEquals("$2.00", mRule.getLineItemAmount(0));
        Assert.assertEquals("$3.00", mRule.getLineItemAmount(1));
        Assert.assertEquals("$0.00", mRule.getLineItemAmount(2));
        mRule.clickAndWait(R.id.button_primary, mRule.getReadyForUnmaskInput());
        mRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mRule.getReadyToUnmask());
        mRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mRule.getDismissed());
        mRule.expectResultContains(new String[] {"freeShipping"});
    }

    /** A merchant that calls updateWith() with displayItems will not cause timeouts in UI. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testUpdateWithDisplayItems() throws Throwable {
        mRule.triggerUIAndWait("updateWithDisplayItems", mRule.getReadyToPay());
        mRule.clickInOrderSummaryAndWait(mRule.getReadyToPay());
        Assert.assertEquals("USD $5.00", mRule.getOrderSummaryTotal());
        Assert.assertEquals("$2.00", mRule.getLineItemAmount(0));
        Assert.assertEquals("$3.00", mRule.getLineItemAmount(1));
        Assert.assertEquals("$0.00", mRule.getLineItemAmount(2));
        mRule.clickInShippingAddressAndWait(R.id.payments_section, mRule.getReadyToPay());
        mRule.clickOnShippingAddressSuggestionOptionAndWait(1, mRule.getReadyToPay());
        mRule.clickInOrderSummaryAndWait(mRule.getReadyToPay());
        Assert.assertEquals("USD $5.00", mRule.getOrderSummaryTotal());
        Assert.assertEquals("$3.00", mRule.getLineItemAmount(0));
        Assert.assertEquals("$2.00", mRule.getLineItemAmount(1));
        Assert.assertEquals("$0.00", mRule.getLineItemAmount(2));
        mRule.clickAndWait(R.id.button_primary, mRule.getReadyForUnmaskInput());
        mRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mRule.getReadyToUnmask());
        mRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mRule.getDismissed());
        mRule.expectResultContains(new String[] {"freeShipping"});
    }

    /** A merchant that calls updateWith() with shipping options will not cause timeouts in UI. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testUpdateWithShippingOptions() throws Throwable {
        mRule.triggerUIAndWait("updateWithShippingOptions", mRule.getReadyToPay());
        mRule.clickInOrderSummaryAndWait(mRule.getReadyToPay());
        Assert.assertEquals("USD $5.00", mRule.getOrderSummaryTotal());
        Assert.assertEquals("$2.00", mRule.getLineItemAmount(0));
        Assert.assertEquals("$3.00", mRule.getLineItemAmount(1));
        Assert.assertEquals("$0.00", mRule.getLineItemAmount(2));
        mRule.clickInShippingAddressAndWait(R.id.payments_section, mRule.getReadyToPay());
        mRule.clickOnShippingAddressSuggestionOptionAndWait(1, mRule.getReadyToPay());
        mRule.clickInOrderSummaryAndWait(mRule.getReadyToPay());
        Assert.assertEquals("USD $5.00", mRule.getOrderSummaryTotal());
        Assert.assertEquals("$2.00", mRule.getLineItemAmount(0));
        Assert.assertEquals("$3.00", mRule.getLineItemAmount(1));
        Assert.assertEquals("$0.00", mRule.getLineItemAmount(2));
        mRule.clickAndWait(R.id.button_primary, mRule.getReadyForUnmaskInput());
        mRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mRule.getReadyToUnmask());
        mRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mRule.getDismissed());
        mRule.expectResultContains(new String[] {"updatedShipping"});
    }

    /** A merchant that calls updateWith() with modifiers will not cause timeouts in UI. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testUpdateWithModifiers() throws Throwable {
        mRule.triggerUIAndWait("updateWithModifiers", mRule.getReadyToPay());
        mRule.clickInOrderSummaryAndWait(mRule.getReadyToPay());
        Assert.assertEquals("USD $5.00", mRule.getOrderSummaryTotal());
        Assert.assertEquals("$2.00", mRule.getLineItemAmount(0));
        Assert.assertEquals("$3.00", mRule.getLineItemAmount(1));
        Assert.assertEquals("$0.00", mRule.getLineItemAmount(2));
        mRule.clickInShippingAddressAndWait(R.id.payments_section, mRule.getReadyToPay());
        mRule.clickOnShippingAddressSuggestionOptionAndWait(1, mRule.getReadyToPay());
        mRule.clickInOrderSummaryAndWait(mRule.getReadyToPay());
        Assert.assertEquals("USD $4.00", mRule.getOrderSummaryTotal());
        Assert.assertEquals("$2.00", mRule.getLineItemAmount(0));
        Assert.assertEquals("$3.00", mRule.getLineItemAmount(1));
        Assert.assertEquals("-$1.00", mRule.getLineItemAmount(2));
        mRule.clickAndWait(R.id.button_primary, mRule.getReadyForUnmaskInput());
        mRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mRule.getReadyToUnmask());
        mRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mRule.getDismissed());
        mRule.expectResultContains(new String[] {"freeShipping"});
    }

    /**
     * Show the shipping address validation error message even if the merchant provided some
     * shipping options.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testUpdateWithError() throws Throwable {
        mRule.triggerUIAndWait("updateWithError", mRule.getReadyToPay());
        mRule.clickInShippingAddressAndWait(R.id.payments_section, mRule.getReadyToPay());
        mRule.clickOnShippingAddressSuggestionOptionAndWait(1, mRule.getReadyForInput());
        CharSequence actualString = mRule.getShippingAddressOptionRowAtIndex(0).getLabelText();
        Assert.assertEquals("This is an error for a browsertest", actualString);
    }
}
