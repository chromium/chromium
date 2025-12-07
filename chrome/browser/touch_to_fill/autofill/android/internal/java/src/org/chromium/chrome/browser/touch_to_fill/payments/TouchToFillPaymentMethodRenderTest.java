// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.test.util.ApplicationTestUtils.finishActivity;
import static org.chromium.chrome.browser.autofill.AutofillTestHelper.createCreditCard;
import static org.chromium.chrome.browser.autofill.AutofillTestHelper.createCreditCardSuggestion;
import static org.chromium.chrome.browser.autofill.AutofillTestHelper.createVirtualCreditCard;
import static org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils.tearDownNightModeAfterChromeActivityDestroyed;
import static org.chromium.ui.base.LocalizationUtils.setRtlForTesting;

import android.view.View;
import android.view.ViewGroup;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.AutofillUiUtils;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill.PersonalDataManager.Iban;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.touch_to_fill.common.BottomSheetFocusHelper;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.LoyaltyCard;
import org.chromium.components.autofill.SuggestionType;
import org.chromium.components.autofill.payments.BnplIssuerContext;
import org.chromium.components.autofill.payments.BnplIssuerTosDetail;
import org.chromium.components.autofill.payments.LegalMessageLine;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.ui.test.util.RenderTestRule.Component;
import org.chromium.url.GURL;

import java.io.IOException;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 * These tests render screenshots of touch to fill for credit cards/IBANs sheet and compare them to
 * a gold standard.
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
// TODO(crbug.com/344662597): Failing when batched, batch this again.
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TouchToFillPaymentMethodRenderTest {
    @ParameterAnnotations.ClassParameter
    private static final List<ParameterSet> sClassParams =
            Arrays.asList(
                    new ParameterSet().value(false, false).name("Default"),
                    new ParameterSet().value(false, true).name("RTL"),
                    new ParameterSet().value(true, false).name("NightMode"));

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(15)
                    .setBugComponent(Component.UI_BROWSER_AUTOFILL)
                    .build();

    @Mock private TouchToFillPaymentMethodComponent.Delegate mDelegateMock;
    @Mock private BottomSheetFocusHelper mBottomSheetFocusHelper;

    private static final CreditCard VISA =
            createCreditCard(
                    "Visa",
                    "4111111111111111",
                    "05",
                    "2100",
                    true,
                    "Visa",
                    "• • • • 1111",
                    R.drawable.visa_metadata_card,
                    "visa");
    private static final CreditCard MASTERCARD =
            createCreditCard(
                    "MasterCard",
                    "5555555555554444",
                    "08",
                    "2100",
                    true,
                    "Mastercard",
                    "• • • • 4444",
                    R.drawable.mc_metadata_card,
                    "mastercard");
    private static final CreditCard SERVER_MASTERCARD =
            createCreditCard(
                    "MasterCard",
                    "5454545454545454",
                    "11",
                    "2100",
                    false,
                    "MasterCard-GPay",
                    "• • • • 5454",
                    R.drawable.mc_metadata_card,
                    "mastercard");
    private static final CreditCard DISCOVER =
            createCreditCard(
                    "Discover",
                    "6011111111111117",
                    "09",
                    "2100",
                    true,
                    "Discover",
                    "• • • • 1117",
                    R.drawable.discover_metadata_card,
                    "discover");
    private static final CreditCard AMERICAN_EXPRESS =
            createCreditCard(
                    "American Express",
                    "378282246310005",
                    "10",
                    "2100",
                    true,
                    "American Express",
                    "• • • • 0005",
                    R.drawable.amex_metadata_card,
                    "american express");
    private static final CreditCard MASTERCARD_VIRTUAL_CARD =
            createVirtualCreditCard(
                    /* name= */ "MasterCard-GPay",
                    /* number= */ "5454545454545454",
                    /* month= */ "11",
                    /* year= */ "2100",
                    /* network= */ "Mastercard",
                    /* iconId= */ R.drawable.mc_metadata_card,
                    /* cardNameForAutofillDisplay= */ "MasterCard-GPay",
                    /* obfuscatedLastFourDigits= */ "• • • • 5454");
    private static final CreditCard LONG_CARD_NAME_CARD =
            createCreditCard(
                    /* name= */ "MJ",
                    /* number= */ "4111111111111111",
                    /* month= */ "5",
                    /* year= */ "2050",
                    /* isLocal= */ false,
                    /* nameForAutofillDisplay= */ "How much wood would a woodchuck chuck if a"
                            + " woodchuck could chuck wood",
                    /* obfuscatedLastFourDigits= */ "• • • • 1111",
                    /* iconId= */ 0,
                    /* network= */ "visa");
    private static final Iban LOCAL_IBAN =
            Iban.createLocal(
                    /* guid= */ "000000111111",
                    /* label= */ "CH56 •••• •••• •••• •800 9",
                    /* nickname= */ "My brother's IBAN",
                    /* value= */ "CH5604835012345678009");

    private static final Iban LOCAL_IBAN_NO_NICKNAME =
            Iban.createLocal(
                    /* guid= */ "000000222222",
                    /* label= */ "FR76 •••• •••• •••• •••• •••0 189",
                    /* nickname= */ "",
                    /* value= */ "FR7630006000011234567890189");

    private static final LoyaltyCard CVS_LOYALTY_CARD =
            new LoyaltyCard(
                    /* loyaltyCardId= */ "cvs",
                    /* merchantName= */ "CVS Pharmacy",
                    /* programName= */ "Loyalty program",
                    /* programLogo= */ new GURL("https://site.com/icon.png"),
                    /* loyaltyCardNumber= */ "1234",
                    /* merchantDomains= */ Collections.emptyList());
    private static final LoyaltyCard DB_LOYALTY_CARD =
            new LoyaltyCard(
                    /* loyaltyCardId= */ "db",
                    /* merchantName= */ "Deutsche Bahn",
                    /* programName= */ "Loyalty program",
                    /* programLogo= */ new GURL("https://db.com/icon.png"),
                    /* loyaltyCardNumber= */ "4321",
                    /* merchantDomains= */ Collections.emptyList());

    private static final AutofillSuggestion VISA_SUGGESTION =
            createCreditCardSuggestion(
                    VISA.getCardNameForAutofillDisplay(),
                    VISA.getObfuscatedLastFourDigits(),
                    VISA.getFormattedExpirationDate(ContextUtils.getApplicationContext()),
                    /* secondarySubLabel= */ "",
                    /* labelContentDescription= */ "",
                    /* suggestionType= */ SuggestionType.CREDIT_CARD_ENTRY,
                    /* customIconUrl= */ new GURL(""),
                    VISA.getIssuerIconDrawableId(),
                    /* applyDeactivatedStyle= */ false,
                    /* shouldDisplayTermsAvailable= */ false,
                    VISA.getGUID(),
                    VISA.getIsLocal());
    private static final AutofillSuggestion VISA_SUGGESTION_WITH_CARD_BENEFITS =
            createCreditCardSuggestion(
                    VISA.getCardNameForAutofillDisplay(),
                    VISA.getObfuscatedLastFourDigits(),
                    /* subLabel= */ "2% cashback on travel",
                    VISA.getFormattedExpirationDate(ContextUtils.getApplicationContext()),
                    /* labelContentDescription= */ "",
                    /* suggestionType= */ SuggestionType.CREDIT_CARD_ENTRY,
                    /* customIconUrl= */ new GURL("http://www.example.com"),
                    VISA.getIssuerIconDrawableId(),
                    /* applyDeactivatedStyle= */ false,
                    /* shouldDisplayTermsAvailable= */ true,
                    VISA.getGUID(),
                    VISA.getIsLocal());
    private static final AutofillSuggestion MASTERCARD_SUGGESTION =
            createCreditCardSuggestion(
                    MASTERCARD.getCardNameForAutofillDisplay(),
                    MASTERCARD.getObfuscatedLastFourDigits(),
                    MASTERCARD.getFormattedExpirationDate(ContextUtils.getApplicationContext()),
                    /* secondarySubLabel= */ "",
                    /* labelContentDescription= */ "",
                    /* suggestionType= */ SuggestionType.CREDIT_CARD_ENTRY,
                    /* customIconUrl= */ new GURL("http://www.example.com"),
                    MASTERCARD.getIssuerIconDrawableId(),
                    /* applyDeactivatedStyle= */ false,
                    /* shouldDisplayTermsAvailable= */ false,
                    MASTERCARD.getGUID(),
                    MASTERCARD.getIsLocal());
    private static final AutofillSuggestion SERVER_MASTERCARD_SUGGESTION =
            createCreditCardSuggestion(
                    SERVER_MASTERCARD.getCardNameForAutofillDisplay(),
                    SERVER_MASTERCARD.getObfuscatedLastFourDigits(),
                    SERVER_MASTERCARD.getFormattedExpirationDate(
                            ContextUtils.getApplicationContext()),
                    /* secondarySubLabel= */ "",
                    /* labelContentDescription= */ "",
                    /* suggestionType= */ SuggestionType.CREDIT_CARD_ENTRY,
                    /* customIconUrl= */ new GURL("http://www.example.com"),
                    SERVER_MASTERCARD.getIssuerIconDrawableId(),
                    /* applyDeactivatedStyle= */ false,
                    /* shouldDisplayTermsAvailable= */ false,
                    SERVER_MASTERCARD.getGUID(),
                    SERVER_MASTERCARD.getIsLocal());
    private static final AutofillSuggestion DISCOVER_SUGGESTION =
            createCreditCardSuggestion(
                    DISCOVER.getCardNameForAutofillDisplay(),
                    DISCOVER.getObfuscatedLastFourDigits(),
                    DISCOVER.getFormattedExpirationDate(ContextUtils.getApplicationContext()),
                    /* secondarySubLabel= */ "",
                    /* labelContentDescription= */ "",
                    /* suggestionType= */ SuggestionType.CREDIT_CARD_ENTRY,
                    /* customIconUrl= */ new GURL("http://www.example.com"),
                    DISCOVER.getIssuerIconDrawableId(),
                    /* applyDeactivatedStyle= */ false,
                    /* shouldDisplayTermsAvailable= */ false,
                    DISCOVER.getGUID(),
                    DISCOVER.getIsLocal());
    private static final AutofillSuggestion AMERICAN_EXPRESS_SUGGESTION =
            createCreditCardSuggestion(
                    AMERICAN_EXPRESS.getCardNameForAutofillDisplay(),
                    AMERICAN_EXPRESS.getObfuscatedLastFourDigits(),
                    AMERICAN_EXPRESS.getFormattedExpirationDate(
                            ContextUtils.getApplicationContext()),
                    /* secondarySubLabel= */ "",
                    /* labelContentDescription= */ "",
                    /* suggestionType= */ SuggestionType.CREDIT_CARD_ENTRY,
                    /* customIconUrl= */ new GURL("http://www.example.com"),
                    AMERICAN_EXPRESS.getIssuerIconDrawableId(),
                    /* applyDeactivatedStyle= */ false,
                    /* shouldDisplayTermsAvailable= */ false,
                    AMERICAN_EXPRESS.getGUID(),
                    AMERICAN_EXPRESS.getIsLocal());
    private static final AutofillSuggestion ACCEPTABLE_MASTERCARD_VIRTUAL_CARD_SUGGESTION =
            createCreditCardSuggestion(
                    MASTERCARD_VIRTUAL_CARD.getCardNameForAutofillDisplay(),
                    MASTERCARD_VIRTUAL_CARD.getObfuscatedLastFourDigits(),
                    /* subLabel= */ "Virtual card",
                    /* secondarySubLabel= */ "",
                    /* labelContentDescription= */ "",
                    /* suggestionType= */ SuggestionType.VIRTUAL_CREDIT_CARD_ENTRY,
                    /* customIconUrl= */ new GURL(AutofillUiUtils.CAPITAL_ONE_ICON_URL),
                    MASTERCARD_VIRTUAL_CARD.getIssuerIconDrawableId(),
                    /* applyDeactivatedStyle= */ false,
                    /* shouldDisplayTermsAvailable= */ false,
                    MASTERCARD_VIRTUAL_CARD.getGUID(),
                    MASTERCARD_VIRTUAL_CARD.getIsLocal());
    private static final AutofillSuggestion NON_ACCEPTABLE_MASTERCARD_VIRTUAL_CARD_SUGGESTION =
            createCreditCardSuggestion(
                    MASTERCARD_VIRTUAL_CARD.getCardNameForAutofillDisplay(),
                    MASTERCARD_VIRTUAL_CARD.getObfuscatedLastFourDigits(),
                    /* subLabel= */ "Merchant doesn't accept this virtual card",
                    /* secondarySubLabel= */ "",
                    /* labelContentDescription= */ "",
                    /* suggestionType= */ SuggestionType.VIRTUAL_CREDIT_CARD_ENTRY,
                    /* customIconUrl= */ new GURL("http://www.example.com"),
                    MASTERCARD_VIRTUAL_CARD.getIssuerIconDrawableId(),
                    /* applyDeactivatedStyle= */ true,
                    /* shouldDisplayTermsAvailable= */ false,
                    MASTERCARD_VIRTUAL_CARD.getGUID(),
                    MASTERCARD_VIRTUAL_CARD.getIsLocal());
    private static final AutofillSuggestion MASTERCARD_VIRTUAL_CARD_SUGGESTION_WITH_CARD_BENEFITS =
            createCreditCardSuggestion(
                    MASTERCARD_VIRTUAL_CARD.getCardNameForAutofillDisplay(),
                    MASTERCARD_VIRTUAL_CARD.getObfuscatedLastFourDigits(),
                    /* subLabel= */ "2% cashback on travel",
                    /* secondarySubLabel= */ "Virtual card",
                    /* labelContentDescription= */ "",
                    /* suggestionType= */ SuggestionType.VIRTUAL_CREDIT_CARD_ENTRY,
                    /* customIconUrl= */ new GURL("http://www.example.com"),
                    MASTERCARD_VIRTUAL_CARD.getIssuerIconDrawableId(),
                    /* applyDeactivatedStyle= */ false,
                    /* shouldDisplayTermsAvailable= */ true,
                    MASTERCARD_VIRTUAL_CARD.getGUID(),
                    MASTERCARD_VIRTUAL_CARD.getIsLocal());
    private static final AutofillSuggestion LONG_CARD_NAME_CARD_SUGGESTION =
            createCreditCardSuggestion(
                    LONG_CARD_NAME_CARD.getCardNameForAutofillDisplay(),
                    LONG_CARD_NAME_CARD.getObfuscatedLastFourDigits(),
                    LONG_CARD_NAME_CARD.getFormattedExpirationDate(
                            ContextUtils.getApplicationContext()),
                    /* secondarySubLabel= */ "",
                    /* labelContentDescription= */ "",
                    /* suggestionType= */ SuggestionType.CREDIT_CARD_ENTRY,
                    /* customIconUrl= */ new GURL("http://www.example.com"),
                    LONG_CARD_NAME_CARD.getIssuerIconDrawableId(),
                    /* applyDeactivatedStyle= */ false,
                    /* shouldDisplayTermsAvailable= */ false,
                    LONG_CARD_NAME_CARD.getGUID(),
                    LONG_CARD_NAME_CARD.getIsLocal());
    private static final AutofillSuggestion BNPL_SUGGESTION =
            createCreditCardSuggestion(
                    /* label= */ "Pay later options",
                    /* secondaryLabel= */ "",
                    /* subLabel= */ "Available for purchases over $35",
                    /* secondarySubLabel= */ "",
                    /* labelContentDescription= */ "",
                    /* suggestionType= */ SuggestionType.BNPL_ENTRY,
                    /* customIconUrl= */ new GURL(""),
                    /* iconId= */ R.drawable.bnpl_icon_generic,
                    /* applyDeactivatedStyle= */ false,
                    /* shouldDisplayTermsAvailable= */ false,
                    /* guid= */ "",
                    /* isLocalPaymentsMethod= */ false);
    private static final AutofillSuggestion DEACTIVATED_BNPL_SUGGESTION =
            createCreditCardSuggestion(
                    /* label= */ "Pay later options",
                    /* secondaryLabel= */ "",
                    /* subLabel= */ "Available for purchases over $35",
                    /* secondarySubLabel= */ "",
                    /* labelContentDescription= */ "",
                    /* suggestionType= */ SuggestionType.BNPL_ENTRY,
                    /* customIconUrl= */ new GURL(""),
                    /* iconId= */ R.drawable.bnpl_icon_generic,
                    /* applyDeactivatedStyle= */ true,
                    /* shouldDisplayTermsAvailable= */ false,
                    /* guid= */ "",
                    /* isLocalPaymentsMethod= */ false);
    private static final BnplIssuerContext BNPL_ISSUER_CONTEXT_AFFIRM_LINKED =
            new BnplIssuerContext(
                    /* iconId= */ R.drawable.affirm_linked,
                    /* issuerId= */ "affirm",
                    /* displayName= */ "Affirm",
                    /* selectionText= */ "Monthly or 4 installments",
                    /* isLinked= */ true,
                    /* isEligible= */ true);
    private static final BnplIssuerContext BNPL_ISSUER_CONTEXT_AFFIRM_UNLINKED =
            new BnplIssuerContext(
                    /* iconId= */ R.drawable.affirm_unlinked,
                    /* issuerId= */ "affirm",
                    /* displayName= */ "Affirm",
                    /* selectionText= */ "Monthly or 4 installments",
                    /* isLinked= */ false,
                    /* isEligible= */ true);
    private static final BnplIssuerContext BNPL_ISSUER_CONTEXT_KLARNA_LINKED =
            new BnplIssuerContext(
                    /* iconId= */ R.drawable.klarna_linked,
                    /* issuerId= */ "klarna",
                    /* displayName= */ "Klarna",
                    /* selectionText= */ "Pay in low monthly installments",
                    /* isLinked= */ true,
                    /* isEligible= */ true);
    private static final BnplIssuerContext BNPL_ISSUER_CONTEXT_KLARNA_UNLINKED =
            new BnplIssuerContext(
                    /* iconId= */ R.drawable.klarna_unlinked,
                    /* issuerId= */ "klarna",
                    /* displayName= */ "Klarna",
                    /* selectionText= */ "Pay in low monthly installments",
                    /* isLinked= */ false,
                    /* isEligible= */ true);
    private static final BnplIssuerContext BNPL_ISSUER_CONTEXT_ZIP_LINKED =
            new BnplIssuerContext(
                    /* iconId= */ R.drawable.zip_linked,
                    /* issuerId= */ "zip",
                    /* displayName= */ "Zip",
                    /* selectionText= */ "Pay in easy installments",
                    /* isLinked= */ true,
                    /* isEligible= */ true);
    private static final BnplIssuerContext BNPL_ISSUER_CONTEXT_ZIP_UNLINKED =
            new BnplIssuerContext(
                    /* iconId= */ R.drawable.zip_unlinked,
                    /* issuerId= */ "zip",
                    /* displayName= */ "Zip",
                    /* selectionText= */ "Pay in easy installments",
                    /* isLinked= */ false,
                    /* isEligible= */ true);
    private static final BnplIssuerContext
            BNPL_ISSUER_CONTEXT_INELIGIBLE_NOT_SUPPORTED_BY_MERCHANT =
                    new BnplIssuerContext(
                            /* iconId= */ R.drawable.affirm_linked,
                            /* issuerId= */ "affirm",
                            /* displayName= */ "Affirm",
                            /* selectionText= */ "Not supported by merchant",
                            /* isLinked= */ true,
                            /* isEligible= */ false);
    private static final BnplIssuerContext BNPL_ISSUER_CONTEXT_INELIGIBLE_CHECKOUT_AMOUNT_TOO_LOW =
            new BnplIssuerContext(
                    /* iconId= */ R.drawable.klarna_linked,
                    /* issuerId= */ "klarna",
                    /* displayName= */ "Klarna",
                    /* selectionText= */ "Purchase must be over $50.00",
                    /* isLinked= */ true,
                    /* isEligible= */ false);
    private static final BnplIssuerContext BNPL_ISSUER_CONTEXT_INELIGIBLE_CHECKOUT_AMOUNT_TOO_HIGH =
            new BnplIssuerContext(
                    /* iconId= */ R.drawable.zip_unlinked,
                    /* issuerId= */ "zip",
                    /* displayName= */ "Zip",
                    /* selectionText= */ "Purchase must be under $10,000.00",
                    /* isLinked= */ false,
                    /* isEligible= */ false);
    private static final BnplIssuerTosDetail BNPL_ISSUER_TOS_DETAIL =
            new BnplIssuerTosDetail(
                    /* issuerId= */ "affirm",
                    /* headerIconDrawableId= */ R.drawable.bnpl_icon_generic,
                    /* headerIconDarkDrawableId= */ R.drawable.bnpl_icon_generic,
                    /* isLinkedIssuer= */ true,
                    /* issuerName= */ "Affirm",
                    /* legalMessageLines= */ Arrays.asList(
                            new LegalMessageLine("Affirm legal message line")));

    private BottomSheetController mBottomSheetController;
    private TouchToFillPaymentMethodCoordinator mCoordinator;
    private WebPageStation mPage;

    public TouchToFillPaymentMethodRenderTest(boolean nightModeEnabled, boolean useRtlLayout) {
        setRtlForTesting(useRtlLayout);
        ChromeNightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
        mRenderTestRule.setVariantPrefix(useRtlLayout ? "RTL" : "LTR");
    }

    @Before
    public void setUp() throws InterruptedException {
        mPage = mActivityTestRule.startOnBlankPage();
        mActivityTestRule.waitForActivityCompletelyLoaded();
        mBottomSheetController =
                mActivityTestRule
                        .getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getBottomSheetController();
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator = new TouchToFillPaymentMethodCoordinator();
                    mCoordinator.initialize(
                            mActivityTestRule.getActivity(),
                            AutofillTestHelper.getAutofillImageFetcherForLastUsedProfile(),
                            mBottomSheetController,
                            mDelegateMock,
                            mBottomSheetFocusHelper);
                });
    }

    @After
    public void tearDown() {
        setRtlForTesting(false);
        try {
            finishActivity(mActivityTestRule.getActivity());
        } catch (Exception e) {
            // Activity was already closed (e.g. due to last test tearing down the suite).
        }
        runOnUiThreadBlocking(() -> tearDownNightModeAfterChromeActivityDestroyed());
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @RequiresRestart("crbug.com/344665938")
    public void testShowsOneCard() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.showPaymentMethods(
                            List.of(VISA_SUGGESTION), /* shouldShowScanCreditCard= */ true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        View bottomSheetView = mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet);
        mRenderTestRule.render(bottomSheetView, "touch_to_fill_credit_card_sheet_one_card");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsOneCardHalfState() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.showPaymentMethods(
                            List.of(VISA_SUGGESTION), /* shouldShowScanCreditCard= */ true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        ViewGroup bottomSheetParentView =
                (ViewGroup)
                        mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet).getParent();
        mRenderTestRule.render(
                bottomSheetParentView, "touch_to_fill_credit_card_sheet_one_card_half_state");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsTwoCards() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.showPaymentMethods(
                            List.of(VISA_SUGGESTION, MASTERCARD_SUGGESTION),
                            /* shouldShowScanCreditCard= */ true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        View bottomSheetView = mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet);
        mRenderTestRule.render(bottomSheetView, "touch_to_fill_credit_card_sheet_two_cards");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsTwoCardsHalfState() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.showPaymentMethods(
                            List.of(VISA_SUGGESTION, MASTERCARD_SUGGESTION),
                            /* shouldShowScanCreditCard= */ true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        ViewGroup bottomSheetParentView =
                (ViewGroup)
                        mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet).getParent();
        mRenderTestRule.render(
                bottomSheetParentView, "touch_to_fill_credit_card_sheet_two_cards_half_state");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsThreeCards() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.showPaymentMethods(
                            List.of(VISA_SUGGESTION, MASTERCARD_SUGGESTION, DISCOVER_SUGGESTION),
                            /* shouldShowScanCreditCard= */ true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        View bottomSheetView = mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet);
        mRenderTestRule.render(bottomSheetView, "touch_to_fill_credit_card_sheet_three_cards");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsThreeCardsHalfState() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.showPaymentMethods(
                            List.of(VISA_SUGGESTION, MASTERCARD_SUGGESTION, DISCOVER_SUGGESTION),
                            /* shouldShowScanCreditCard= */ true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        ViewGroup bottomSheetParentView =
                (ViewGroup)
                        mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet).getParent();
        mRenderTestRule.render(
                bottomSheetParentView, "touch_to_fill_credit_card_sheet_three_cards_half_state");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsFourCards() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.showPaymentMethods(
                            List.of(
                                    VISA_SUGGESTION,
                                    MASTERCARD_SUGGESTION,
                                    DISCOVER_SUGGESTION,
                                    AMERICAN_EXPRESS_SUGGESTION),
                            /* shouldShowScanCreditCard= */ true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        View bottomSheetView = mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet);
        mRenderTestRule.render(bottomSheetView, "touch_to_fill_credit_card_sheet_four_cards");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsFourCardsHalfState() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.showPaymentMethods(
                            List.of(
                                    VISA_SUGGESTION,
                                    MASTERCARD_SUGGESTION,
                                    DISCOVER_SUGGESTION,
                                    AMERICAN_EXPRESS_SUGGESTION),
                            /* shouldShowScanCreditCard= */ true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        ViewGroup bottomSheetParentView =
                (ViewGroup)
                        mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet).getParent();
        mRenderTestRule.render(
                bottomSheetParentView, "touch_to_fill_credit_card_sheet_four_cards_half_state");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsLocalAndServerAndVirtualCards() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.showPaymentMethods(
                            List.of(
                                    VISA_SUGGESTION,
                                    ACCEPTABLE_MASTERCARD_VIRTUAL_CARD_SUGGESTION,
                                    SERVER_MASTERCARD_SUGGESTION),
                            /* shouldShowScanCreditCard= */ true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        View bottomSheetView = mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet);
        mRenderTestRule.render(
                bottomSheetView,
                "touch_to_fill_credit_card_sheet_shows_local_and_server_and_virtual_cards");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsLocalAndServerAndNonAcceptableVirtualCards() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.showPaymentMethods(
                            List.of(
                                    VISA_SUGGESTION,
                                    NON_ACCEPTABLE_MASTERCARD_VIRTUAL_CARD_SUGGESTION,
                                    SERVER_MASTERCARD_SUGGESTION),
                            /* shouldShowScanCreditCard= */ true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        View bottomSheetView = mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet);
        mRenderTestRule.render(
                bottomSheetView,
                "touch_to_fill_credit_card_sheet_shows_local_and_server_and_non_acceptable_virtual_cards");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsServerAndVirtualCardsWithCardBenefits() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.showPaymentMethods(
                            List.of(
                                    VISA_SUGGESTION_WITH_CARD_BENEFITS,
                                    MASTERCARD_VIRTUAL_CARD_SUGGESTION_WITH_CARD_BENEFITS),
                            /* shouldShowScanCreditCard= */ true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        View bottomSheetView = mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet);
        mRenderTestRule.render(
                bottomSheetView,
                "touch_to_fill_credit_card_sheet_shows_real_and_virtual_cards_with_card_benefits");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsServerCardWithLongName() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.showPaymentMethods(
                            List.of(LONG_CARD_NAME_CARD_SUGGESTION),
                            /* shouldShowScanCreditCard= */ true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        View bottomSheetView = mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet);
        mRenderTestRule.render(
                bottomSheetView, "touch_to_fill_credit_card_sheet_server_card_with_long_name");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsBnplSuggestion() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.showPaymentMethods(
                            List.of(VISA_SUGGESTION, BNPL_SUGGESTION),
                            /* shouldShowScanCreditCard= */ true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        View bottomSheetView = mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet);
        mRenderTestRule.render(bottomSheetView, "touch_to_fill_credit_card_sheet_bnpl_suggestion");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsDeactivatedBnplSuggestion() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.showPaymentMethods(
                            List.of(VISA_SUGGESTION, DEACTIVATED_BNPL_SUGGESTION),
                            /* shouldShowScanCreditCard= */ true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        View bottomSheetView = mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet);
        mRenderTestRule.render(
                bottomSheetView, "touch_to_fill_credit_card_sheet_deactivated_bnpl_suggestion");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsBnplProgressScreen() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.showProgressScreen();
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        View bottomSheetView = mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet);
        mRenderTestRule.render(bottomSheetView, "touch_to_fill_bnpl_progress_screen");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsBnplIssuerSelectionScreenWithLinkedIssuers() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.showBnplIssuers(
                            List.of(
                                    BNPL_ISSUER_CONTEXT_AFFIRM_LINKED,
                                    BNPL_ISSUER_CONTEXT_KLARNA_LINKED,
                                    BNPL_ISSUER_CONTEXT_ZIP_LINKED));
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        View bottomSheetView = mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet);
        mRenderTestRule.render(
                bottomSheetView, "touch_to_fill_bnpl_issuer_selection_screen_with_linked_issuers");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsBnplIssuerSelectionScreenWithUnlinkedIssuers() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.showBnplIssuers(
                            List.of(
                                    BNPL_ISSUER_CONTEXT_AFFIRM_UNLINKED,
                                    BNPL_ISSUER_CONTEXT_KLARNA_UNLINKED,
                                    BNPL_ISSUER_CONTEXT_ZIP_UNLINKED));
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        View bottomSheetView = mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet);
        mRenderTestRule.render(
                bottomSheetView,
                "touch_to_fill_bnpl_issuer_selection_screen_with_unlinked_issuers");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsBnplIssuerTosScreen() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.showBnplIssuerTos(BNPL_ISSUER_TOS_DETAIL);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        View bottomSheetView = mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet);
        mRenderTestRule.render(bottomSheetView, "touch_to_fill_bnpl_issuer_tos_screen");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsErrorScreen() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.showErrorScreen(
                            /* title= */ "Something went wrong",
                            /* description= */ "Pay later is unavailable at this time. Try again or"
                                    + " choose another payment method.");
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        View bottomSheetView = mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet);
        mRenderTestRule.render(bottomSheetView, "touch_to_fill_error_screen");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @RequiresRestart("crbug.com/344665938")
    public void testScanNewCardButtonIsHidden() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.showPaymentMethods(
                            List.of(VISA_SUGGESTION), /* shouldShowScanCreditCard= */ false);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        View bottomSheetView = mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet);
        mRenderTestRule.render(
                bottomSheetView, "touch_to_fill_credit_card_sheet_scan_credit_card_hidden");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsOneIban() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.showIbans(List.of(LOCAL_IBAN));
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        View bottomSheetView = mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet);
        mRenderTestRule.render(bottomSheetView, "touch_to_fill_iban_sheet_one_iban");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsOneIbanHalfState() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.showIbans(List.of(LOCAL_IBAN));
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        ViewGroup bottomSheetParentView =
                (ViewGroup)
                        mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet).getParent();
        mRenderTestRule.render(
                bottomSheetParentView, "touch_to_fill_iban_sheet_one_iban_half_state");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsTwoIbans() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.showIbans(List.of(LOCAL_IBAN, LOCAL_IBAN_NO_NICKNAME));
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        View bottomSheetView = mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet);
        mRenderTestRule.render(bottomSheetView, "touch_to_fill_iban_sheet_two_ibans");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsTwoIbansHalfState() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.showIbans(List.of(LOCAL_IBAN, LOCAL_IBAN_NO_NICKNAME));
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        ViewGroup bottomSheetParentView =
                (ViewGroup)
                        mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet).getParent();
        mRenderTestRule.render(
                bottomSheetParentView, "touch_to_fill_iban_sheet_two_ibans_half_state");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsOneLoyaltyCard() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.showLoyaltyCards(
                            List.of(CVS_LOYALTY_CARD),
                            List.of(CVS_LOYALTY_CARD, DB_LOYALTY_CARD),
                            /* firstTimeUsage= */ false);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        ViewGroup bottomSheetParentView =
                (ViewGroup)
                        mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet).getParent();
        mRenderTestRule.render(
                bottomSheetParentView, "touch_to_fill_loyalty_card_sheet_one_loyalty_card");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @DisabledTest(message = "crbug.com/428186413")
    public void testShowsAllLoyaltyCardsScreen() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.showLoyaltyCards(
                            List.of(CVS_LOYALTY_CARD),
                            List.of(CVS_LOYALTY_CARD, DB_LOYALTY_CARD),
                            /* firstTimeUsage= */ false);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        onView(withId(R.id.all_loyalty_cards_item_title)).perform(click());

        ViewGroup bottomSheetParentView =
                (ViewGroup)
                        mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet).getParent();
        mRenderTestRule.render(
                bottomSheetParentView, "touch_to_fill_loyalty_card_all_loyalty_cards_screen");
    }
}
