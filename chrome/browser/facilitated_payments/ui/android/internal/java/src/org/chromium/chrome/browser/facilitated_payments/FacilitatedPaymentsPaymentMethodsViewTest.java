// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.is;

import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.BANK_ACCOUNT;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.VISIBLE;
import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.autofill.payments.AccountType;
import org.chromium.components.autofill.payments.BankAccount;
import org.chromium.components.autofill.payments.PaymentInstrument;
import org.chromium.components.autofill.payments.PaymentRail;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Instrumentation tests for {@link FacilitatedPaymentsPaymentMethodsView}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public final class FacilitatedPaymentsPaymentMethodsViewTest {
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

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private BottomSheetController mBottomSheetController;
    private BottomSheetTestSupport mSheetTestSupport;
    private FacilitatedPaymentsPaymentMethodsView mView;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.startMainActivityOnBlankPage();
        mBottomSheetController =
                mActivityTestRule
                        .getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getBottomSheetController();
        mSheetTestSupport = new BottomSheetTestSupport(mBottomSheetController);
        runOnUiThreadBlocking(
                () -> {
                    mModel = createFacilitatedPaymentsPaymentMethodsModel();
                    mView =
                            new FacilitatedPaymentsPaymentMethodsView(
                                    mActivityTestRule.getActivity(), mBottomSheetController);
                    PropertyModelChangeProcessor.create(
                            mModel,
                            mView,
                            FacilitatedPaymentsPaymentMethodsViewBinder
                                    ::bindFacilitatedPaymentsPaymentMethodsView);
                });
    }

    @Test
    @MediumTest
    public void testVisibilityChangedByModel() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.get(SHEET_ITEMS)
                            .add(
                                    new ListItem(
                                            BANK_ACCOUNT, createBankAccountModel(BANK_ACCOUNT_1)));
                });
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        assertThat(mView.getContentView().isShown(), is(true));
    }

    @Test
    @MediumTest
    public void testBankAccountShown() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.get(SHEET_ITEMS)
                            .add(
                                    new ListItem(
                                            BANK_ACCOUNT, createBankAccountModel(BANK_ACCOUNT_1)));
                    mModel.set(VISIBLE, true);
                    mModel.get(SHEET_ITEMS)
                            .add(
                                    new ListItem(
                                            BANK_ACCOUNT, createBankAccountModel(BANK_ACCOUNT_2)));
                });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        assertThat(getBankAccounts().getChildCount(), is(2));

        String expectedBankAccountSummary1 = String.format("Pix  •  %s ••••%s", "Checking", "1111");
        assertThat(getBankAccountNameAt(0).getText(), is("bankName1"));
        assertThat(getBankAccountSummaryAt(0).getText(), is(expectedBankAccountSummary1));

        String expectedBankAccountSummary2 = String.format("Pix  •  %s ••••%s", "Savings", "2222");
        assertThat(getBankAccountNameAt(1).getText(), is("bankName2"));
        assertThat(getBankAccountSummaryAt(1).getText(), is(expectedBankAccountSummary2));
    }

    @Test
    @MediumTest
    public void testDescriptionLine1() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.get(SHEET_ITEMS)
                            .add(FacilitatedPaymentsPaymentMethodsMediator.buildAdditionalInfo());
                    mModel.set(VISIBLE, true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        TextView descriptionLine1 = mView.getContentView().findViewById(R.id.description_line_1);
        assertThat(
                descriptionLine1.getText(),
                is("Transactions that exceed your balance will not be processed"));
    }

    private PropertyModel createFacilitatedPaymentsPaymentMethodsModel() {
        return new PropertyModel.Builder(FacilitatedPaymentsPaymentMethodsProperties.ALL_KEYS)
                .with(VISIBLE, false)
                .with(SHEET_ITEMS, new ModelList())
                .build();
    }

    private PropertyModel createBankAccountModel(BankAccount bankAccount) {
        return FacilitatedPaymentsPaymentMethodsMediator.createBankAccountModel(
                mActivityTestRule.getActivity(), bankAccount);
    }

    private RecyclerView getBankAccounts() {
        return mView.getContentView().findViewById(R.id.sheet_item_list);
    }

    private TextView getBankAccountNameAt(int index) {
        return getBankAccounts().getChildAt(index).findViewById(R.id.bank_name);
    }

    private TextView getBankAccountSummaryAt(int index) {
        return getBankAccounts().getChildAt(index).findViewById(R.id.bank_account_summary);
    }
}
