// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.test.util.Criteria.checkThat;
import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;
import static org.chromium.base.test.util.Matchers.containsString;
import static org.chromium.base.test.util.Matchers.is;
import static org.chromium.chrome.browser.autofill.AutofillTestHelper.createCreditCard;
import static org.chromium.chrome.test.R.id.first_line_label;
import static org.chromium.chrome.test.R.id.main_text;
import static org.chromium.chrome.test.R.id.minor_text;
import static org.chromium.chrome.test.R.id.sheet_item_list;

import android.view.View;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_manager.PasswordManagerTestUtilsBridge;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.payments.InputProtector;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestInputMethodManagerWrapper;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;
import org.chromium.ui.widget.ButtonCompat;

import java.util.concurrent.TimeoutException;

/** Integration tests for the payments bottom sheet. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "show-autofill-signatures"})
public class TouchToFillCreditCardTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    private static final String FORM_URL =
            "/chrome/test/data/autofill/autofill_creditcard_form.html";
    private static final String CREDIT_CARD_NAME_FIELD_ID = "CREDIT_CARD_NAME_FULL";
    private static final String CREDIT_CARD_NUMBER_FIELD_ID = "CREDIT_CARD_NUMBER";
    private static final String CREDIT_CARD_EXP_MONTH_FIELD_ID = "CREDIT_CARD_EXP_MONTH";
    private static final String CREDIT_CARD_EXP_YEAR_FIELD_ID = "CREDIT_CARD_EXP_4_DIGIT_YEAR";

    private static final String CARD_NAME = "Visa";
    private static final String CARD_NUMBER = "4111111111111111";
    private static final String CARD_EXP_YEAR = "2050";
    private static final String CARD_EXP_2_DIGIT_YEAR = "50";
    private static final String CARD_EXP_MONTH = "05";
    private static final String MASKED_NUMBER = "• • • • 1111";
    private static final String NETWORK_NAME = "visa";
    private static final String CARD_NAME_FOR_AUTOFILL_DISPLAY = "Visa";
    private static final CreditCard VISA =
            createCreditCard(
                    CARD_NAME,
                    CARD_NUMBER,
                    CARD_EXP_MONTH,
                    CARD_EXP_YEAR,
                    /* isLocal= */ true,
                    CARD_NAME_FOR_AUTOFILL_DISPLAY,
                    MASKED_NUMBER,
                    0,
                    NETWORK_NAME);

    private BottomSheetController mBottomSheetController;
    private WebContents mWebContents;
    private EmbeddedTestServer mServer;
    TestInputMethodManagerWrapper mInputMethodWrapper;

    @Before
    public void setup() throws TimeoutException {
        mServer =
                EmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getInstrumentation().getContext(),
                        ServerCertificate.CERT_OK);
        mActivityTestRule.startMainActivityWithURL(mServer.getURL(FORM_URL));
        PasswordManagerTestUtilsBridge.disableServerPredictions();
        new AutofillTestHelper().setCreditCard(VISA);

        runOnUiThreadBlocking(
                () -> {
                    mBottomSheetController =
                            BottomSheetControllerProvider.from(
                                    mActivityTestRule.getActivity().getWindowAndroid());
                });
        mWebContents = mActivityTestRule.getWebContents();

        ImeAdapter imeAdapter = WebContentsUtils.getImeAdapter(mWebContents);
        mInputMethodWrapper = TestInputMethodManagerWrapper.create(imeAdapter);
        imeAdapter.setInputMethodManagerWrapper(mInputMethodWrapper);
    }

    @Test
    @MediumTest
    public void testSelectingLocalCard() throws TimeoutException {
        // Focus the field to bring up the touch to fill for credit cards.
        DOMUtils.clickNode(mWebContents, CREDIT_CARD_NUMBER_FIELD_ID);
        // Wait for TTF.
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        // Check that keyboard is not displayed.
        pollUiThread(
                () -> {
                    checkThat(mInputMethodWrapper.getShowSoftInputCounter(), is(0));
                });

        // Wait minimum amount of time for input protector.
        mFakeTimeTestRule.advanceMillis(InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD);

        // The item with the index 1 in the recycler view is supposed to be the credit card.
        // Click on it to simulate user selection.
        runOnUiThreadBlocking(
                () -> {
                    View creditCardSuggestionItemLayout = getItemsList().getChildAt(1);
                    verifyCardSuggestionIsCorrectlyDisplayed(creditCardSuggestionItemLayout);
                    // Check that continue button is present
                    Assert.assertTrue(getItemsList().getChildAt(2) instanceof ButtonCompat);

                    creditCardSuggestionItemLayout.performClick();
                });
        // Wait until the bottom sheet is closed
        BottomSheetTestSupport.waitForState(mBottomSheetController, SheetState.HIDDEN);

        // Check that the form is filled with the card credentials
        checkThat(DOMUtils.getNodeValue(mWebContents, CREDIT_CARD_NAME_FIELD_ID), is(CARD_NAME));
        checkThat(
                DOMUtils.getNodeValue(mWebContents, CREDIT_CARD_NUMBER_FIELD_ID), is(CARD_NUMBER));
        checkThat(
                DOMUtils.getNodeValue(mWebContents, CREDIT_CARD_EXP_YEAR_FIELD_ID),
                is(CARD_EXP_YEAR));
        checkThat(
                DOMUtils.getNodeValue(mWebContents, CREDIT_CARD_EXP_MONTH_FIELD_ID),
                is(CARD_EXP_MONTH));
    }

    private RecyclerView getItemsList() {
        return mActivityTestRule.getActivity().findViewById(sheet_item_list);
    }

    private void verifyCardSuggestionIsCorrectlyDisplayed(View cardSuggestionItemLayout) {
        TextView mainTextLayout = cardSuggestionItemLayout.findViewById(main_text);
        TextView minorTextLayout = cardSuggestionItemLayout.findViewById(minor_text);
        TextView cardDescLayout = cardSuggestionItemLayout.findViewById(first_line_label);
        // Check that suggestion main text with the card name is displayed
        checkThat(mainTextLayout.getText().toString(), is(CARD_NAME));
        // Check that suggestion minor text with the last four digits of the card are displayed
        checkThat(
                minorTextLayout.getText().toString(),
                containsString(CARD_NUMBER.substring(CARD_NUMBER.length() - 4)));
        // Check that the expiration month and year are present in the card description
        checkThat(cardDescLayout.getText().toString(), containsString(CARD_EXP_MONTH));
        checkThat(cardDescLayout.getText().toString(), containsString(CARD_EXP_2_DIGIT_YEAR));
    }
}
