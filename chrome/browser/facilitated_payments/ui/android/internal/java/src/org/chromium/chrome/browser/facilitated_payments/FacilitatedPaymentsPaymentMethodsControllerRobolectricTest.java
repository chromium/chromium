// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.contains;
import static org.hamcrest.Matchers.hasSize;
import static org.hamcrest.Matchers.is;
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
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ErrorScreenProperties.PRIMARY_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.EwalletProperties.EWALLET_NAME;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.EwalletProperties.ON_EWALLET_CLICK_ACTION;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.FopSelectorProperties.SCREEN_ITEMS;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.ADDITIONAL_INFO;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.BANK_ACCOUNT;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.CONTINUE_BUTTON;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.EWALLET;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.FOOTER;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.HEADER;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SCREEN;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SCREEN_VIEW_MODEL;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SequenceScreen.ERROR_SCREEN;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SequenceScreen.FOP_SELECTOR;
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
import org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.FooterProperties;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.autofill.payments.AccountType;
import org.chromium.components.autofill.payments.BankAccount;
import org.chromium.components.autofill.payments.Ewallet;
import org.chromium.components.autofill.payments.PaymentInstrument;
import org.chromium.components.autofill.payments.PaymentRail;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.facilitated_payments.core.ui_utils.UiEvent;
import org.chromium.components.payments.InputProtector;
import org.chromium.components.payments.test_support.FakeClock;
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
                    .setEwalletName("ewallet name 1")
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
                    .setEwalletName("ewallet name 2")
                    .setAccountDisplayName("account display name 2")
                    .setPaymentInstrument(
                            new PaymentInstrument.Builder()
                                    .setInstrumentId(100)
                                    .setNickname("nickname 4")
                                    .setSupportedPaymentRails(new int[] {2})
                                    .setIsFidoEnrolled(true)
                                    .build())
                    .build();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    private FacilitatedPaymentsPaymentMethodsCoordinator mCoordinator;
    private PropertyModel mFacilitatedPaymentsPaymentMethodsModel;
    private FakeClock mClock = new FakeClock();
    Context mContext;

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private FacilitatedPaymentsPaymentMethodsComponent.Delegate mDelegateMock;
    @Mock private Profile mProfile;

    public FacilitatedPaymentsPaymentMethodsControllerRobolectricTest() {
        mCoordinator = new FacilitatedPaymentsPaymentMethodsCoordinator();
        mContext = Robolectric.buildActivity(Activity.class).get();
    }

    @Before
    public void setUp() {
        Mockito.when(
                        mBottomSheetController.requestShowContent(
                                any(BottomSheetContent.class), anyBoolean()))
                .thenReturn(true);
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
        assertNotNull(mFacilitatedPaymentsPaymentMethodsModel.get(DISMISS_HANDLER));
        assertNotNull(mFacilitatedPaymentsPaymentMethodsModel.get(UI_EVENT_LISTENER));
    }

    @Test
    public void testCreatesModelForFopSelectorScreen_BankAccountFopSelector() {
        mCoordinator.showSheet(List.of(BANK_ACCOUNT_1));

        // Verify that the bottom sheet model is updated to show the FOP selector.
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(VISIBLE_STATE), is(SHOWN));
        assertThat(mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN), is(FOP_SELECTOR));
        assertNotNull(mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN_VIEW_MODEL));
        // Verify the FOP selector screen model contains the required properties.
        assertTrue(
                mFacilitatedPaymentsPaymentMethodsModel
                        .get(SCREEN_VIEW_MODEL)
                        .containsKey(SCREEN_ITEMS));
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
    }

    @Test
    public void testBankAccountsShown() {
        mCoordinator.showSheet(List.of(BANK_ACCOUNT_1, BANK_ACCOUNT_2));

        // Verify the screen contents set in the model when 2 bank accounts exist.
        ModelList itemList =
                mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN_VIEW_MODEL).get(SCREEN_ITEMS);
        assertThat(itemList.size(), is(5));
        assertEquals(itemList.get(0).type, HEADER);
        assertEquals(itemList.get(1).type, BANK_ACCOUNT);
        assertEquals(itemList.get(2).type, BANK_ACCOUNT);
        assertEquals(itemList.get(3).type, ADDITIONAL_INFO);
        assertEquals(itemList.get(4).type, FOOTER);
    }

    @Test
    public void testEwalletsShown() {
        mCoordinator.showSheetForEwallet(List.of(EWALLET_1, EWALLET_2));

        // Verify the screen contents set in the model when 2 eWallets exist.
        // TODO(crbug.com/40280186): Modify the assertions when other items of eWallet FOP selector
        // are implemented.
        ModelList itemList =
                mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN_VIEW_MODEL).get(SCREEN_ITEMS);
        assertThat(itemList.size(), is(2));
        assertEquals(itemList.get(0).type, EWALLET);
        assertEquals(itemList.get(1).type, EWALLET);
    }

    @Test
    public void testSingleBankAccountShown() {
        mCoordinator.showSheet(List.of(BANK_ACCOUNT_1));

        // Verify the screen contents set in the model when only 1 bank account exists.
        ModelList itemList =
                mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN_VIEW_MODEL).get(SCREEN_ITEMS);
        assertThat(itemList.size(), is(5));
        assertEquals(itemList.get(0).type, HEADER);
        assertEquals(itemList.get(1).type, BANK_ACCOUNT);
        assertEquals(itemList.get(2).type, ADDITIONAL_INFO);
        assertEquals(itemList.get(3).type, CONTINUE_BUTTON);
        assertEquals(itemList.get(4).type, FOOTER);
    }

    @Test
    public void testSingleEwalletShown() {
        mCoordinator.showSheetForEwallet(List.of(EWALLET_1));

        // Verify the screen contents set in the model when only 1 eWallet account exists.
        // TODO(crbug.com/40280186): Modify the assertions when other items of eWallet FOP selector
        // are implemented.
        ModelList itemList =
                mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN_VIEW_MODEL).get(SCREEN_ITEMS);
        assertThat(itemList.size(), is(2));
        assertEquals(itemList.get(0).type, EWALLET);
        assertEquals(itemList.get(1).type, CONTINUE_BUTTON);
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
    public void testOnDismissedIsCalled() {
        mFacilitatedPaymentsPaymentMethodsModel
                .get(DISMISS_HANDLER)
                .onResult(StateChangeReason.SWIPE);

        verify(mDelegateMock).onDismissed();
    }

    @Test
    public void testShowFopSelector_SuccessfullyShown_UiEventRelayed() {
        mCoordinator.showSheet(List.of(BANK_ACCOUNT_1));

        verify(mDelegateMock).onUiEvent(UiEvent.NEW_SCREEN_SHOWN);
    }

    @Test
    public void testShowFopSelector_FailedToShow_UiEventRelayed() {
        Mockito.when(
                        mBottomSheetController.requestShowContent(
                                any(BottomSheetContent.class), anyBoolean()))
                .thenReturn(false);

        mCoordinator.showSheet(List.of(BANK_ACCOUNT_1));

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
    public void testShowFinancialAccountsManagementSettings() {
        mCoordinator.showSheet(List.of(BANK_ACCOUNT_1, BANK_ACCOUNT_2));

        // The additional info is the last item of the screen items list right now.
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

        verify(mDelegateMock).showFinancialAccountsManagementSettings(mContext);
    }

    @Test
    public void testShowsContinueButtonWhenOneBankAccount() {
        mCoordinator.showSheet(List.of(BANK_ACCOUNT_1));

        ModelList itemList =
                mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN_VIEW_MODEL).get(SCREEN_ITEMS);
        assertEquals(getModelsOfType(itemList, CONTINUE_BUTTON).size(), 1);
    }

    @Test
    public void testShowsContinueButtonWhenOneEwallet() {
        mCoordinator.showSheetForEwallet(List.of(EWALLET_1));

        ModelList itemList =
                mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN_VIEW_MODEL).get(SCREEN_ITEMS);
        assertEquals(getModelsOfType(itemList, CONTINUE_BUTTON).size(), 1);
    }

    @Test
    public void testNoContinueButtonWhenManyBankAccounts() {
        mCoordinator.showSheet(List.of(BANK_ACCOUNT_1, BANK_ACCOUNT_2));

        ModelList itemList =
                mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN_VIEW_MODEL).get(SCREEN_ITEMS);
        assertEquals(getModelsOfType(itemList, CONTINUE_BUTTON).size(), 0);
    }

    @Test
    public void testNoContinueButtonWhenManyEwallets() {
        mCoordinator.showSheetForEwallet(List.of(EWALLET_1, EWALLET_2));

        ModelList itemList =
                mFacilitatedPaymentsPaymentMethodsModel.get(SCREEN_VIEW_MODEL).get(SCREEN_ITEMS);
        assertEquals(getModelsOfType(itemList, CONTINUE_BUTTON).size(), 0);
    }

    @Test
    public void testContinueButtonClickForBankAccount() {
        mCoordinator.showSheet(List.of(BANK_ACCOUNT_1));
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
    public void testShowManagePaymentMethodsSettingsOnFooter() {
        mCoordinator.showSheet(List.of(BANK_ACCOUNT_1));

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

        verify(mDelegateMock).showManagePaymentMethodsSettings(mContext);
    }

    @Test
    public void testCallbackIsCalledWhenBankAccountIsSelected() {
        mCoordinator.showSheet(List.of(BANK_ACCOUNT_1));
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
        mCoordinator.showSheet(List.of(BANK_ACCOUNT_1));
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
                mFacilitatedPaymentsPaymentMethodsModel
                        .get(SCREEN_VIEW_MODEL)
                        .getAllProperties()
                        .size(),
                0);
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
    public void testFopSelectorToProgressScreenSwapUpdatesModel() {
        // Show the FOP selector.
        mCoordinator.showSheet(List.of(BANK_ACCOUNT_1));

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
                mFacilitatedPaymentsPaymentMethodsModel
                        .get(SCREEN_VIEW_MODEL)
                        .getAllProperties()
                        .size(),
                0);

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
        mCoordinator.showSheet(List.of(BANK_ACCOUNT_1));

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
