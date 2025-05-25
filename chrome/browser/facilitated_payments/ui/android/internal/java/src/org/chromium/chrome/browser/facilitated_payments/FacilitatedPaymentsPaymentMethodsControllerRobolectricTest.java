// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.contains;
import static org.hamcrest.Matchers.containsInAnyOrder;
import static org.hamcrest.Matchers.hasSize;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.AdditionalInfoProperties.SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.BANK_NAME;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.ON_BANK_ACCOUNT_CLICK_ACTION;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ErrorScreenProperties.PRIMARY_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.EwalletProperties.EWALLET_NAME;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.EwalletProperties.ON_EWALLET_CLICK_ACTION;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.FopSelectorProperties.SCREEN_ITEMS;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.HeaderProperties.DESCRIPTION_ID;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.HeaderProperties.SECURITY_CHECK_DRAWABLE_ID;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.HeaderProperties.TITLE;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.ADDITIONAL_INFO;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.BANK_ACCOUNT;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.CONTINUE_BUTTON;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.EWALLET;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.FOOTER;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.HEADER;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.PixAccountLinkingPromptProperties.ACCEPT_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.PixAccountLinkingPromptProperties.DECLINE_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SCREEN;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SCREEN_VIEW_MODEL;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SURVIVES_NAVIGATION;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SequenceScreen.ERROR_SCREEN;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SequenceScreen.FOP_SELECTOR;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SequenceScreen.PIX_ACCOUNT_LINKING_PROMPT;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SequenceScreen.PROGRESS_SCREEN;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SequenceScreen.UNINITIALIZED;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.UI_EVENT_LISTENER;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.VISIBLE_STATE;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.VisibleState.HIDDEN;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.VisibleState.SHOWN;

import android.app.Activity;
import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.autofill.AutofillImageFetcher;
import org.chromium.chrome.browser.autofill.AutofillImageFetcherFactory;
import org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.FooterProperties;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.autofill.payments.AccountType;
import org.chromium.components.autofill.payments.BankAccount;
import org.chromium.components.autofill.payments.Ewallet;
import org.chromium.components.autofill.payments.PaymentInstrument;
import org.chromium.components.autofill.payments.PaymentRail;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.facilitated_payments.core.ui_utils.FopSelectorAction;
import org.chromium.components.facilitated_payments.core.ui_utils.UiEvent;
import org.chromium.components.payments.ui.InputProtector;
import org.chromium.components.payments.ui.test_support.FakeClock;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;
import java.util.List;
import java.util.Optional;
import java.util.stream.Collectors;
import java.util.stream.StreamSupport;

/**
 * Tests for {@link FacilitatedPaymentsPaymentMethodsCoordinator} and {@link
 * FacilitatedPaymentsPaymentMethodsMediator}
 */
@RunWith(BaseRobolectricTestRunner.class)
public class FacilitatedPaymentsPaymentMethodsControllerRobolectricTest {
    private static final BankAccount BANK_ACCOUNT_1 =
            new BankAccount.Builder()
                    .setPaymentInstrument(
                            new PaymentInstrument.Builder()
                                    .setInstrumentId(100)
                                    .setNickname("nickname1")
                                    .setSupportedPaymentRails(new int[] {PaymentRail.PIX})
                                    .build())
                    .setBankName("bankName1")
                    .setAccountNumberSuffix("1111")
                    .setAccountType(AccountType.CHECKING)
                    .build();
    private static final BankAccount BANK_ACCOUNT_2 =
            new BankAccount.Builder()
                    .setPaymentInstrument(
                            new PaymentInstrument.Builder()
                                    .setInstrumentId(200)
                                    .setNickname("nickname2")
                                    .setSupportedPaymentRails(new int[] {PaymentRail.PIX})
                                    .build())
                    .setBankName("bankName2")
                    .setAccountNumberSuffix("2222")
                    .setAccountType(AccountType.SAVINGS)
                    .build();
    private static final Ewallet EWALLET_1 =
            new Ewallet.Builder()
                    .setEwalletName("eWalletName1")
                    .setAccountDisplayName("account display name 1")
                    .setPaymentInstrument(
                            new PaymentInstrument.Builder()
                                    .setInstrumentId(100)
                                    .setNickname("nickname 3")
                                    .setSupportedPaymentRails(new int[] {2})
                                    .setIsFidoEnrolled(true)
                                    .build())
                    .build();
    private static final Ewallet EWALLET_2 =
            new Ewallet.Builder()
                    .setEwalletName("eWalletName2")
                    .setAccountDisplayName("account display name 2")
                    .setPaymentInstrument(
                            new PaymentInstrument.Builder()
                                    .setInstrumentId(101)
                                    .setNickname("nickname 4")
                                    .setSupportedPaymentRails(new int[] {2})
                                    .setIsFidoEnrolled(true)
                                    .build())
                    .build();
    private static final Ewallet EWALLET_3 =
            new Ewallet.Builder()
                    .setEwalletName("eWalletName2")
                    .setAccountDisplayName("account display name 3")
                    .setPaymentInstrument(
                            new PaymentInstrument.Builder()
                                    .setInstrumentId(123)
                                    .setNickname("nickname 5")
                                    .setSupportedPaymentRails(new int[] {2})
                                    .setIsFidoEnrolled(false)
                                    .build())
                    .build();
    private static final Ewallet EWALLET_4 =
            new Ewallet.Builder()
                    .setEwalletName("eWalletName3")
                    .setAccountDisplayName("account display name 4")
                    .setPaymentInstrument(
                            new PaymentInstrument.Builder()
                                    .setInstrumentId(312)
                                    .setNickname("nickname 6")
                                    .setSupportedPaymentRails(new int[] {2})
                                    .setIsFidoEnrolled(false)
                                    .build())
                    .build();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private FacilitatedPaymentsPaymentMethodsComponent.Delegate mDelegateMock;
    @Mock private AutofillImageFetcher mAutofillImageFetcher;
    @Mock private Profile mProfile;

    private final Context mContext;
    private final FacilitatedPaymentsPaymentMethodsCoordinator mCoordinator;
    private final FakeClock mClock = new FakeClock();
    private PropertyModel mFacilitatedPaymentsPaymentMethodsModel;

    public FacilitatedPaymentsPaymentMethodsControllerRobolectricTest() {
        mContext = Robolectric.buildActivity(Activity.class).get();
        mCoordinator = new FacilitatedPaymentsPaymentMethodsCoordinator();
    }

    @Before
    public void setUp() {
        Mockito.when(
                        mBottomSheetController.requestShowContent(
                                any(BottomSheetContent.class), anyBoolean()))
                .thenReturn(true);
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        AutofillImageFetcherFactory.setInstanceForTesting(mAutofillImageFetcher);
        mCoordinator.initialize(mContext, mBottomSheetController, mDelegateMock, mProfile);
        mFacilitatedPaymentsPaymentMethodsModel = mCoordinator.getModelForTesting();
        mCoordinator
                .getMediatorForTesting()
                .setInputProtectorForTesting(new InputProtector(mClock));
    }

    @Test
    public void testCreatesValidDefaultPropertyModel() {
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(VISIBLE_STATE), is(HIDDEN));
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN), is(UNINITIALIZED));
        assertNull(mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN_VIEW_MODEL));
        assertNotNull(mFacilitatedPaymentsPaymentMethodsModel.get(UI_EVENT_LISTENER));
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(SURVIVES_NAVIGATION), is(false));
    }

    @Test
    public void testCreatesModelForFopSelectorScreen_BankAccountFopSelector() {
        mCoordinator.showSheetForPix(List.of(BANK_ACCOUNT_1));

        // Verify that the bottom sheet model is updated to show the FOP selector.
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(VISIBLE_STATE), is(SHOWN));
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN), is(FOP_SELECTOR));
        assertNotNull(mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN_VIEW_MODEL));
        // Verify the FOP selector screen model contains the required properties.
        assertTrue(
                mFacilitatedPaymentsPaymentMethodsModel
                        .get(SCREEN_VIEW_MODEL)
                        .containsKey(SCREEN_ITEMS));
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(SURVIVES_NAVIGATION), is(false));
    }

    @Test
    public void testCreatesModelForFopSelectorScreen_EwalletFopSelector() {
        mCoordinator.showSheetForEwallet(List.of(EWALLET_1));

        // Verify that the bottom sheet model is updated to show the FOP selector.
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(VISIBLE_STATE), is(SHOWN));
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN), is(FOP_SELECTOR));
        assertNotNull(mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN_VIEW_MODEL));
        // Verify the FOP selector screen model contains the required properties.
        assertTrue(
                mFacilitatedPaymentsPaymentMethodsModel
                        .get(SCREEN_VIEW_MODEL)
                        .containsKey(SCREEN_ITEMS));
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(SURVIVES_NAVIGATION), is(false));
    }

    @Test
    public void testBankAccountsShown() {
        mCoordinator.showSheetForPix(List.of(BANK_ACCOUNT_1, BANK_ACCOUNT_2));

        // Verify the screen contents set in the model when 2 bank accounts exist.
        ModelList itemList =
                mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN_VIEW_MODEL).get(SCREEN_ITEMS);
        assertThat(itemList.size(), is(5));
        assertEquals(HEADER, itemList.get(0).type);
        assertEquals(BANK_ACCOUNT, itemList.get(1).type);
        assertEquals(BANK_ACCOUNT, itemList.get(2).type);
        assertEquals(ADDITIONAL_INFO, itemList.get(3).type);
        assertEquals(FOOTER, itemList.get(4).type);
    }

    @Test
    public void testEwalletsShown() {
        mCoordinator.showSheetForEwallet(List.of(EWALLET_1, EWALLET_2));

        // Verify the screen contents set in the model when 2 eWallets exist.
        ModelList itemList =
                mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN_VIEW_MODEL).get(SCREEN_ITEMS);
        assertThat(itemList.size(), is(5));
        assertEquals(HEADER, itemList.get(0).type);
        assertEquals(EWALLET, itemList.get(1).type);
        assertEquals(EWALLET, itemList.get(2).type);
        assertEquals(ADDITIONAL_INFO, itemList.get(3).type);
        assertEquals(FOOTER, itemList.get(4).type);
    }

    @Test
    public void testSingleBankAccountShown() {
        mCoordinator.showSheetForPix(List.of(BANK_ACCOUNT_1));

        // Verify the screen contents set in the model when only 1 bank account exists.
        ModelList itemList =
                mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN_VIEW_MODEL).get(SCREEN_ITEMS);
        assertThat(itemList.size(), is(5));
        assertEquals(HEADER, itemList.get(0).type);
        assertEquals(BANK_ACCOUNT, itemList.get(1).type);
        assertEquals(ADDITIONAL_INFO, itemList.get(2).type);
        assertEquals(CONTINUE_BUTTON, itemList.get(3).type);
        assertEquals(FOOTER, itemList.get(4).type);
    }

    @Test
    public void testSingleEwalletShown() {
        mCoordinator.showSheetForEwallet(List.of(EWALLET_1));

        // Verify the screen contents set in the model when only 1 eWallet account exists.
        ModelList itemList =
                mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN_VIEW_MODEL).get(SCREEN_ITEMS);
        assertThat(itemList.size(), is(5));
        assertEquals(HEADER, itemList.get(0).type);
        assertEquals(EWALLET, itemList.get(1).type);
        assertEquals(ADDITIONAL_INFO, itemList.get(2).type);
        assertEquals(CONTINUE_BUTTON, itemList.get(3).type);
        assertEquals(FOOTER, itemList.get(4).type);
    }

    @Test
    public void testSingleFidoUnenrolledEwalletFirstTimeHeaderUsed() {
        mCoordinator.showSheetForEwallet(List.of(EWALLET_3));

        // Verify the header model contains security check UI elements when only 1 eWallet account
        // exists and it is not Fido enrolled.
        ModelList itemList =
                mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN_VIEW_MODEL).get(SCREEN_ITEMS);
        PropertyModel header = itemList.get(0).model;

        assertThat(header.get(SECURITY_CHECK_DRAWABLE_ID), is(not(0)));
        assertThat(header.get(DESCRIPTION_ID), is(not(0)));
    }

    @Test
    public void testMultipleEwalletsFirstTimeHeaderNotUsed() {
        mCoordinator.showSheetForEwallet(List.of(EWALLET_3, EWALLET_4));

        // Verify the header model doesn't contain security check UI elements when multiple eWallet
        // accounts exist.
        // Note: It does not matter if the accounts are Fido enrolled.
        ModelList itemList =
                mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN_VIEW_MODEL).get(SCREEN_ITEMS);
        PropertyModel header = itemList.get(0).model;

        assertThat(header.get(SECURITY_CHECK_DRAWABLE_ID), is(0));
        assertThat(header.get(DESCRIPTION_ID), is(0));
    }

    @Test
    public void testSingleFidoEnrolledEwalletFirstTimeHeaderNotUsed() {
        mCoordinator.showSheetForEwallet(List.of(EWALLET_1));

        // Verify the header model doesn't contain security check UI elements when only 1 eWallet
        // account exists and it is not Fido enrolled.
        ModelList itemList =
                mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN_VIEW_MODEL).get(SCREEN_ITEMS);
        PropertyModel header = itemList.get(0).model;

        assertThat(header.get(SECURITY_CHECK_DRAWABLE_ID), is(0));
        assertThat(header.get(DESCRIPTION_ID), is(0));
    }

    @Test
    public void testEwalletGenericHeaderTitleUsed() {
        mCoordinator.showSheetForEwallet(List.of(EWALLET_1, EWALLET_3));

        // Verify the header model uses the generic title when multiple providers are displayed.
        ModelList itemList =
                mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN_VIEW_MODEL).get(SCREEN_ITEMS);
        PropertyModel header = itemList.get(0).model;

        assertThat(header.get(TITLE), is("Pay without switching apps"));
    }

    @Test
    public void testEwalletSpecificHeaderTitleUsed() {
        mCoordinator.showSheetForEwallet(List.of(EWALLET_2, EWALLET_3));

        // Verify the header model uses the provider specific title when all eWallets use the same
        // provider.
        ModelList itemList =
                mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN_VIEW_MODEL).get(SCREEN_ITEMS);
        PropertyModel header = itemList.get(0).model;

        assertThat(header.get(TITLE), is("Pay with eWalletName2 without switching apps"));
    }

    @Test
    public void testUiEventsAreForwardedToDelegate() {
        for (int uiEvent :
                Arrays.asList(
                        UiEvent.NEW_SCREEN_SHOWN,
                        UiEvent.SCREEN_CLOSED_NOT_BY_USER,
                        UiEvent.SCREEN_CLOSED_BY_USER)) {
            mFacilitatedPaymentsPaymentMethodsModel.get(UI_EVENT_LISTENER).onResult(uiEvent);

            verify(mDelegateMock).onUiEvent(uiEvent);
        }
    }

    @Test
    public void testShowFopSelector_SuccessfullyShown_UiEventRelayed() {
        mCoordinator.showSheetForPix(List.of(BANK_ACCOUNT_1));

        verify(mDelegateMock).onUiEvent(UiEvent.NEW_SCREEN_SHOWN);
    }

    @Test
    public void testShowFopSelector_FailedToShow_UiEventRelayed() {
        Mockito.when(
                        mBottomSheetController.requestShowContent(
                                any(BottomSheetContent.class), anyBoolean()))
                .thenReturn(false);

        mCoordinator.showSheetForPix(List.of(BANK_ACCOUNT_1));

        verify(mDelegateMock).onUiEvent(UiEvent.SCREEN_CLOSED_NOT_BY_USER);
    }

    @Test
    public void testShowErrorScreen_SuccessfullyShown_UiEventRelayed() {
        mCoordinator.showErrorScreen();

        verify(mDelegateMock).onUiEvent(UiEvent.NEW_SCREEN_SHOWN);
    }

    @Test
    public void testShowErrorScreen_FailedToShow_UiEventRelayed() {
        Mockito.when(
                        mBottomSheetController.requestShowContent(
                                any(BottomSheetContent.class), anyBoolean()))
                .thenReturn(false);

        mCoordinator.showErrorScreen();

        verify(mDelegateMock).onUiEvent(UiEvent.SCREEN_CLOSED_NOT_BY_USER);
    }

    @Test
    public void testPixShowFinancialAccountsManagementSettings() {
        mCoordinator.showSheetForPix(List.of(BANK_ACCOUNT_1, BANK_ACCOUNT_2));

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                FacilitatedPaymentsPaymentMethodsMediator
                                        .PIX_FOP_SELECTOR_USER_ACTION_HISTOGRAM,
                                FopSelectorAction.TURN_OFF_PAYMENT_PROMPT_LINK_CLICKED)
                        .build();

        // The additional info is the second to last item of the screen items list right now.
        int lastItemPos =
                mFacilitatedPaymentsPaymentMethodsModel
                                .get(SCREEN_VIEW_MODEL)
                                .get(SCREEN_ITEMS)
                                .size()
                        - 2;
        mFacilitatedPaymentsPaymentMethodsModel
                .get(SCREEN_VIEW_MODEL)
                .get(SCREEN_ITEMS)
                .get(lastItemPos)
                .model
                .get(SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK)
                .run();

        histogramWatcher.assertExpected();
        verify(mDelegateMock).showFinancialAccountsManagementSettings(mContext);
    }

    @Test
    public void testSingleFidoUnenrolledEwalletShowFinancialAccountsManagementSettings() {
        mCoordinator.showSheetForEwallet(List.of(EWALLET_3));

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                FacilitatedPaymentsPaymentMethodsMediator
                                                .EWALLET_FOP_SELECTOR_USER_ACTION_HISTOGRAM
                                        + "SingleUnboundEwallet",
                                FopSelectorAction.TURN_OFF_PAYMENT_PROMPT_LINK_CLICKED)
                        .build();

        // The additional info is the third to last item of the screen items list right now.
        int lastItemPos =
                mFacilitatedPaymentsPaymentMethodsModel
                                .get(SCREEN_VIEW_MODEL)
                                .get(SCREEN_ITEMS)
                                .size()
                        - 3;
        mFacilitatedPaymentsPaymentMethodsModel
                .get(SCREEN_VIEW_MODEL)
                .get(SCREEN_ITEMS)
                .get(lastItemPos)
                .model
                .get(SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK)
                .run();

        histogramWatcher.assertExpected();
        verify(mDelegateMock).showFinancialAccountsManagementSettings(mContext);
    }

    @Test
    public void testSingleFidoEnrolledEwalletShowFinancialAccountsManagementSettings() {
        mCoordinator.showSheetForEwallet(List.of(EWALLET_2));

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                FacilitatedPaymentsPaymentMethodsMediator
                                                .EWALLET_FOP_SELECTOR_USER_ACTION_HISTOGRAM
                                        + "SingleBoundEwallet",
                                FopSelectorAction.TURN_OFF_PAYMENT_PROMPT_LINK_CLICKED)
                        .build();

        // The additional info is the third to last item of the screen items list right now.
        int lastItemPos =
                mFacilitatedPaymentsPaymentMethodsModel
                                .get(SCREEN_VIEW_MODEL)
                                .get(SCREEN_ITEMS)
                                .size()
                        - 3;
        mFacilitatedPaymentsPaymentMethodsModel
                .get(SCREEN_VIEW_MODEL)
                .get(SCREEN_ITEMS)
                .get(lastItemPos)
                .model
                .get(SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK)
                .run();

        histogramWatcher.assertExpected();
        verify(mDelegateMock).showFinancialAccountsManagementSettings(mContext);
    }

    @Test
    public void testMultipleEwalletsShowFinancialAccountsManagementSettings() {
        mCoordinator.showSheetForEwallet(List.of(EWALLET_3, EWALLET_4));

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                FacilitatedPaymentsPaymentMethodsMediator
                                                .EWALLET_FOP_SELECTOR_USER_ACTION_HISTOGRAM
                                        + "MultipleEwallets",
                                FopSelectorAction.TURN_OFF_PAYMENT_PROMPT_LINK_CLICKED)
                        .build();

        // The additional info is the second to last item of the screen items list right now.
        int lastItemPos =
                mFacilitatedPaymentsPaymentMethodsModel
                                .get(SCREEN_VIEW_MODEL)
                                .get(SCREEN_ITEMS)
                                .size()
                        - 2;
        mFacilitatedPaymentsPaymentMethodsModel
                .get(SCREEN_VIEW_MODEL)
                .get(SCREEN_ITEMS)
                .get(lastItemPos)
                .model
                .get(SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK)
                .run();

        histogramWatcher.assertExpected();
        verify(mDelegateMock).showFinancialAccountsManagementSettings(mContext);
    }

    @Test
    public void testShowsContinueButtonWhenOneBankAccount() {
        mCoordinator.showSheetForPix(List.of(BANK_ACCOUNT_1));

        ModelList itemList =
                mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN_VIEW_MODEL).get(SCREEN_ITEMS);
        assertEquals(1, getModelsOfType(itemList, CONTINUE_BUTTON).size());
    }

    @Test
    public void testShowsContinueButtonWhenOneEwallet() {
        mCoordinator.showSheetForEwallet(List.of(EWALLET_1));

        ModelList itemList =
                mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN_VIEW_MODEL).get(SCREEN_ITEMS);
        assertEquals(1, getModelsOfType(itemList, CONTINUE_BUTTON).size());
    }

    @Test
    public void testNoContinueButtonWhenManyBankAccounts() {
        mCoordinator.showSheetForPix(List.of(BANK_ACCOUNT_1, BANK_ACCOUNT_2));

        ModelList itemList =
                mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN_VIEW_MODEL).get(SCREEN_ITEMS);
        assertEquals(0, getModelsOfType(itemList, CONTINUE_BUTTON).size());
    }

    @Test
    public void testNoContinueButtonWhenManyEwallets() {
        mCoordinator.showSheetForEwallet(List.of(EWALLET_1, EWALLET_2));

        ModelList itemList =
                mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN_VIEW_MODEL).get(SCREEN_ITEMS);
        assertEquals(0, getModelsOfType(itemList, CONTINUE_BUTTON).size());
    }

    @Test
    public void testContinueButtonClickForBankAccount() {
        mCoordinator.showSheetForPix(List.of(BANK_ACCOUNT_1));
        ModelList itemList =
                mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN_VIEW_MODEL).get(SCREEN_ITEMS);

        mClock.advanceCurrentTimeMillis(InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD);
        getModelsOfType(itemList, CONTINUE_BUTTON).get(0).get(ON_BANK_ACCOUNT_CLICK_ACTION).run();

        verify(mDelegateMock).onBankAccountSelected(BANK_ACCOUNT_1.getInstrumentId());
    }

    @Test
    public void testContinueButtonClickForEwallet() {
        mCoordinator.showSheetForEwallet(List.of(EWALLET_1));
        ModelList itemList =
                mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN_VIEW_MODEL).get(SCREEN_ITEMS);

        mClock.advanceCurrentTimeMillis(InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD);
        getModelsOfType(itemList, CONTINUE_BUTTON).get(0).get(ON_EWALLET_CLICK_ACTION).run();

        verify(mDelegateMock).onEwalletSelected(EWALLET_1.getInstrumentId());
    }

    @Test
    public void testPixShowManagePaymentMethodsSettingsOnFooter() {
        mCoordinator.showSheetForPix(List.of(BANK_ACCOUNT_1));

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                FacilitatedPaymentsPaymentMethodsMediator
                                        .PIX_FOP_SELECTOR_USER_ACTION_HISTOGRAM,
                                FopSelectorAction.MANAGE_PAYMENT_METHODS_OPTION_SELECTED)
                        .build();

        int lastItemPos =
                mFacilitatedPaymentsPaymentMethodsModel
                                .get(SCREEN_VIEW_MODEL)
                                .get(SCREEN_ITEMS)
                                .size()
                        - 1;
        mFacilitatedPaymentsPaymentMethodsModel
                .get(SCREEN_VIEW_MODEL)
                .get(SCREEN_ITEMS)
                .get(lastItemPos)
                .model
                .get(FooterProperties.SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK)
                .run();

        histogramWatcher.assertExpected();
        verify(mDelegateMock).showManagePaymentMethodsSettings(mContext);
    }

    @Test
    public void testSingleFidoUnenrolledEwalletShowManagePaymentMethodsSettingsOnFooter() {
        mCoordinator.showSheetForEwallet(List.of(EWALLET_4));

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                FacilitatedPaymentsPaymentMethodsMediator
                                                .EWALLET_FOP_SELECTOR_USER_ACTION_HISTOGRAM
                                        + "SingleUnboundEwallet",
                                FopSelectorAction.MANAGE_PAYMENT_METHODS_OPTION_SELECTED)
                        .build();

        int lastItemPos =
                mFacilitatedPaymentsPaymentMethodsModel
                                .get(SCREEN_VIEW_MODEL)
                                .get(SCREEN_ITEMS)
                                .size()
                        - 1;
        mFacilitatedPaymentsPaymentMethodsModel
                .get(SCREEN_VIEW_MODEL)
                .get(SCREEN_ITEMS)
                .get(lastItemPos)
                .model
                .get(FooterProperties.SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK)
                .run();

        histogramWatcher.assertExpected();
        verify(mDelegateMock).showManagePaymentMethodsSettings(mContext);
    }

    @Test
    public void testSingleFidoEnrolledEwalletShowManagePaymentMethodsSettingsOnFooter() {
        mCoordinator.showSheetForEwallet(List.of(EWALLET_1));

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                FacilitatedPaymentsPaymentMethodsMediator
                                                .EWALLET_FOP_SELECTOR_USER_ACTION_HISTOGRAM
                                        + "SingleBoundEwallet",
                                FopSelectorAction.MANAGE_PAYMENT_METHODS_OPTION_SELECTED)
                        .build();

        int lastItemPos =
                mFacilitatedPaymentsPaymentMethodsModel
                                .get(SCREEN_VIEW_MODEL)
                                .get(SCREEN_ITEMS)
                                .size()
                        - 1;
        mFacilitatedPaymentsPaymentMethodsModel
                .get(SCREEN_VIEW_MODEL)
                .get(SCREEN_ITEMS)
                .get(lastItemPos)
                .model
                .get(FooterProperties.SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK)
                .run();

        histogramWatcher.assertExpected();
        verify(mDelegateMock).showManagePaymentMethodsSettings(mContext);
    }

    @Test
    public void testMultipleEwalletsShowManagePaymentMethodsSettingsOnFooter() {
        mCoordinator.showSheetForEwallet(List.of(EWALLET_3, EWALLET_2));

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                FacilitatedPaymentsPaymentMethodsMediator
                                                .EWALLET_FOP_SELECTOR_USER_ACTION_HISTOGRAM
                                        + "MultipleEwallets",
                                FopSelectorAction.MANAGE_PAYMENT_METHODS_OPTION_SELECTED)
                        .build();

        int lastItemPos =
                mFacilitatedPaymentsPaymentMethodsModel
                                .get(SCREEN_VIEW_MODEL)
                                .get(SCREEN_ITEMS)
                                .size()
                        - 1;
        mFacilitatedPaymentsPaymentMethodsModel
                .get(SCREEN_VIEW_MODEL)
                .get(SCREEN_ITEMS)
                .get(lastItemPos)
                .model
                .get(FooterProperties.SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK)
                .run();

        histogramWatcher.assertExpected();
        verify(mDelegateMock).showManagePaymentMethodsSettings(mContext);
    }

    @Test
    public void testCallbackIsCalledWhenBankAccountIsSelected() {
        mCoordinator.showSheetForPix(List.of(BANK_ACCOUNT_1));
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(VISIBLE_STATE), is(SHOWN));

        Optional<PropertyModel> bankAccountModel =
                getBankAccountModelByBankName(
                        mFacilitatedPaymentsPaymentMethodsModel
                                .get(SCREEN_VIEW_MODEL)
                                .get(SCREEN_ITEMS),
                        BANK_ACCOUNT_1);
        assertNotNull(bankAccountModel.get().get(ON_BANK_ACCOUNT_CLICK_ACTION));

        mClock.advanceCurrentTimeMillis(InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD);
        bankAccountModel.get().get(ON_BANK_ACCOUNT_CLICK_ACTION).run();
        verify(mDelegateMock).onBankAccountSelected(BANK_ACCOUNT_1.getInstrumentId());
    }

    @Test
    public void testCallbackIsCalledWhenEwalletIsSelected() {
        mCoordinator.showSheetForEwallet(List.of(EWALLET_1));
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(VISIBLE_STATE), is(SHOWN));

        Optional<PropertyModel> eWalletModel =
                getEwalletModelByEwalletName(
                        mFacilitatedPaymentsPaymentMethodsModel
                                .get(SCREEN_VIEW_MODEL)
                                .get(SCREEN_ITEMS),
                        EWALLET_1);
        assertNotNull(eWalletModel.get().get(ON_EWALLET_CLICK_ACTION));

        mClock.advanceCurrentTimeMillis(InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD);
        eWalletModel.get().get(ON_EWALLET_CLICK_ACTION).run();
        verify(mDelegateMock).onEwalletSelected(EWALLET_1.getInstrumentId());
    }

    @Test
    public void testNoCallbackForSelectedBankAccountBeforeInputTime() {
        mCoordinator.showSheetForPix(List.of(BANK_ACCOUNT_1));
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(VISIBLE_STATE), is(SHOWN));

        Optional<PropertyModel> bankAccountModel =
                getBankAccountModelByBankName(
                        mFacilitatedPaymentsPaymentMethodsModel
                                .get(SCREEN_VIEW_MODEL)
                                .get(SCREEN_ITEMS),
                        BANK_ACCOUNT_1);
        assertNotNull(bankAccountModel.get().get(ON_BANK_ACCOUNT_CLICK_ACTION));

        // Clicking after an interval less than the threshold should be a no-op.
        mClock.advanceCurrentTimeMillis(
                InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD - 100);
        bankAccountModel.get().get(ON_BANK_ACCOUNT_CLICK_ACTION).run();
        verify(mDelegateMock, times(0)).onBankAccountSelected(BANK_ACCOUNT_1.getInstrumentId());

        // Clicking after the threshold should work.
        mClock.advanceCurrentTimeMillis(200);
        bankAccountModel.get().get(ON_BANK_ACCOUNT_CLICK_ACTION).run();
        verify(mDelegateMock, times(1)).onBankAccountSelected(BANK_ACCOUNT_1.getInstrumentId());
    }

    @Test
    public void testNoCallbackForSelectedEwalletBeforeInputTime() {
        mCoordinator.showSheetForEwallet(List.of(EWALLET_1));
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(VISIBLE_STATE), is(SHOWN));

        Optional<PropertyModel> eWalletModel =
                getEwalletModelByEwalletName(
                        mFacilitatedPaymentsPaymentMethodsModel
                                .get(SCREEN_VIEW_MODEL)
                                .get(SCREEN_ITEMS),
                        EWALLET_1);
        assertNotNull(eWalletModel.get().get(ON_EWALLET_CLICK_ACTION));

        // Clicking after an interval less than the threshold should be a no-op.
        mClock.advanceCurrentTimeMillis(
                InputProtector.POTENTIALLY_UNINTENDED_INPUT_THRESHOLD - 100);
        eWalletModel.get().get(ON_EWALLET_CLICK_ACTION).run();
        verify(mDelegateMock, times(0)).onEwalletSelected(EWALLET_1.getInstrumentId());

        // Clicking after the threshold should work.
        mClock.advanceCurrentTimeMillis(200);
        eWalletModel.get().get(ON_EWALLET_CLICK_ACTION).run();
        verify(mDelegateMock, times(1)).onEwalletSelected(EWALLET_1.getInstrumentId());
    }

    @Test
    public void testCreatesModelForProgressScreen() {
        mCoordinator.showProgressScreen();

        // Verify that the bottom sheet model is updated to show the progress screen.
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(VISIBLE_STATE), is(SHOWN));
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN), is(PROGRESS_SCREEN));
        assertNotNull(mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN_VIEW_MODEL));
        // Progress screen doesn't have any view properties.
        assertEquals(
                0,
                mFacilitatedPaymentsPaymentMethodsModel
                        .get(SCREEN_VIEW_MODEL)
                        .getAllProperties()
                        .size());
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(SURVIVES_NAVIGATION), is(false));
    }

    @Test
    public void testCreatesModelForErrorScreen() {
        mCoordinator.showErrorScreen();

        // Verify that the bottom sheet model is updated to show the error screen.
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(VISIBLE_STATE), is(SHOWN));
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN), is(ERROR_SCREEN));
        assertNotNull(mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN_VIEW_MODEL));
        // Verify error screen view properties.
        List<PropertyKey> propertyKeys =
                (List<PropertyKey>)
                        mFacilitatedPaymentsPaymentMethodsModel
                                .get(SCREEN_VIEW_MODEL)
                                .getAllProperties();
        assertThat(propertyKeys, hasSize(1));
        assertThat(propertyKeys, contains(PRIMARY_BUTTON_CALLBACK));
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(SURVIVES_NAVIGATION), is(false));
    }

    @Test
    public void testClickingErrorScreenPrimaryButtonDismissesView() {
        mCoordinator.showErrorScreen();

        // Simulate clicking the primary button.
        mFacilitatedPaymentsPaymentMethodsModel
                .get(SCREEN_VIEW_MODEL)
                .get(PRIMARY_BUTTON_CALLBACK)
                .onClick(null);

        // Verify that the bottom sheet model reflects dismissed state.
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN), is(UNINITIALIZED));
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(VISIBLE_STATE), is(HIDDEN));
    }

    @Test
    public void testCreatesModelForPixAccountLinkingPrompt() {
        mCoordinator.showPixAccountLinkingPrompt();

        // Verify that the bottom sheet model is updated to show the PIX account linking screen.
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(VISIBLE_STATE), is(SHOWN));
        assertThat(
                mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN),
                is(PIX_ACCOUNT_LINKING_PROMPT));
        assertNotNull(mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN_VIEW_MODEL));
        // Verify screen view properties.
        List<PropertyKey> propertyKeys =
                (List<PropertyKey>)
                        mFacilitatedPaymentsPaymentMethodsModel
                                .get(SCREEN_VIEW_MODEL)
                                .getAllProperties();
        assertThat(propertyKeys, hasSize(2));
        assertThat(
                propertyKeys, containsInAnyOrder(ACCEPT_BUTTON_CALLBACK, DECLINE_BUTTON_CALLBACK));
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(SURVIVES_NAVIGATION), is(true));
    }

    @Test
    public void testAcceptingPixAccountLinkingPromptInformsDelegate() {
        mCoordinator.showPixAccountLinkingPrompt();

        // Simulate clicking the accept button.
        mFacilitatedPaymentsPaymentMethodsModel
                .get(SCREEN_VIEW_MODEL)
                .get(ACCEPT_BUTTON_CALLBACK)
                .onClick(null);

        verify(mDelegateMock).onPixAccountLinkingPromptAccepted();
    }

    @Test
    public void testDecliningPixAccountLinkingPromptInformsDelegate() {
        mCoordinator.showPixAccountLinkingPrompt();

        // Simulate clicking the accept button.
        mFacilitatedPaymentsPaymentMethodsModel
                .get(SCREEN_VIEW_MODEL)
                .get(DECLINE_BUTTON_CALLBACK)
                .onClick(null);

        verify(mDelegateMock).onPixAccountLinkingPromptDeclined();
    }

    @Test
    public void testFopSelectorToProgressScreenSwapUpdatesModel() {
        // Show the FOP selector.
        mCoordinator.showSheetForPix(List.of(BANK_ACCOUNT_1));

        // Confirm the FOP selector is shown.
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(VISIBLE_STATE), is(SHOWN));
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN), is(FOP_SELECTOR));

        // The bottom sheet is now open.
        Mockito.when(mBottomSheetController.isSheetOpen()).thenReturn(true);
        // Show the progress screen. The FOP selector is still being shown.
        mCoordinator.showProgressScreen();

        // Verify that the bottom sheet model is updated to show the progress screen.
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(VISIBLE_STATE), is(SHOWN));
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN), is(PROGRESS_SCREEN));
        assertNotNull(mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN_VIEW_MODEL));
        // Progress screen doesn't have any view properties.
        assertEquals(
                0,
                mFacilitatedPaymentsPaymentMethodsModel
                        .get(SCREEN_VIEW_MODEL)
                        .getAllProperties()
                        .size());

        // Verify that the UI event is relayed to the delegate. New screen shown event should be
        // triggered twice, once for each screen.
        verify(mDelegateMock, times(2)).onUiEvent(UiEvent.NEW_SCREEN_SHOWN);
    }

    @Test
    public void testProgressScreenToErrorScreenSwapUpdatesModel() {
        // Show the progress screen.
        mCoordinator.showProgressScreen();

        // Confirm the progress screen is shown.
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(VISIBLE_STATE), is(SHOWN));
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN), is(PROGRESS_SCREEN));

        // The bottom sheet is now open.
        Mockito.when(mBottomSheetController.isSheetOpen()).thenReturn(true);
        // Show the error screen. The progress screen is still being shown.
        mCoordinator.showErrorScreen();

        // Verify that the bottom sheet model is updated to show the error screen.
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(VISIBLE_STATE), is(SHOWN));
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN), is(ERROR_SCREEN));
        assertNotNull(mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN_VIEW_MODEL));
        // Verify error screen view properties.
        List<PropertyKey> propertyKeys =
                (List<PropertyKey>)
                        mFacilitatedPaymentsPaymentMethodsModel
                                .get(SCREEN_VIEW_MODEL)
                                .getAllProperties();
        assertThat(propertyKeys, hasSize(1));
        assertThat(propertyKeys, contains(PRIMARY_BUTTON_CALLBACK));

        // Verify that the UI event is relayed to the delegate. New screen shown event should be
        // triggered twice, once for each screen.
        verify(mDelegateMock, times(2)).onUiEvent(UiEvent.NEW_SCREEN_SHOWN);
    }

    @Test
    public void testDismiss() {
        // Show the FOP selector.
        mCoordinator.showSheetForPix(List.of(BANK_ACCOUNT_1));

        // Confirm the FOP selector is shown.
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(VISIBLE_STATE), is(SHOWN));
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN), is(FOP_SELECTOR));

        // Close the bottom sheet.
        mCoordinator.dismiss();

        // Verify that the bottom sheet model is updated for dismissal.
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN), is(UNINITIALIZED));
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(VISIBLE_STATE), is(HIDDEN));

        // Verify that the bottom sheet closing is triggered. The bottom sheet is initialized in the
        // hidden state which triggers hideContent. The second call is from the dismissal.
        verify(mBottomSheetController, times(2)).hideContent(any(), eq(true));
    }

    private static List<PropertyModel> getModelsOfType(ModelList items, int type) {
        return StreamSupport.stream(items.spliterator(), false)
                .filter(item -> item.type == type)
                .map(item -> item.model)
                .collect(Collectors.toList());
    }

    private static Optional<PropertyModel> getBankAccountModelByBankName(
            ModelList items, BankAccount bankAccount) {
        return StreamSupport.stream(items.spliterator(), false)
                .filter(
                        item ->
                                item.type == BANK_ACCOUNT
                                        && item.model
                                                .get(BANK_NAME)
                                                .equals(bankAccount.getBankName()))
                .findFirst()
                .map(item -> item.model);
    }

    private static Optional<PropertyModel> getEwalletModelByEwalletName(
            ModelList items, Ewallet eWallet) {
        return StreamSupport.stream(items.spliterator(), false)
                .filter(
                        item ->
                                item.type == EWALLET
                                        && item.model
                                                .get(EWALLET_NAME)
                                                .equals(eWallet.getEwalletName()))
                .findFirst()
                .map(item -> item.model);
    }
}
