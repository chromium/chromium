// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.hasToString;
import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertNotNull;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.FopSelectorProperties.SCREEN_ITEMS;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.BANK_ACCOUNT;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.CONTINUE_BUTTON;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SCREEN;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SCREEN_VIEW_MODEL;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SequenceScreen.ERROR_SCREEN;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SequenceScreen.FOP_SELECTOR;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SequenceScreen.PROGRESS_SCREEN;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.VISIBLE_STATE;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.VisibleState.HIDDEN;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.VisibleState.SHOWN;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.VisibleState.SWAPPING_SCREEN;

import android.view.View;
import android.view.ViewGroup;
import android.widget.ProgressBar;
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
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.ButtonCompat;

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
    private FacilitatedPaymentsPaymentMethodsMediator mMediator;
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
        mMediator = new FacilitatedPaymentsPaymentMethodsMediator();
        runOnUiThreadBlocking(
                () -> {
                    mModel =
                            new PropertyModel.Builder(
                                            FacilitatedPaymentsPaymentMethodsProperties.ALL_KEYS)
                                    .with(VISIBLE_STATE, HIDDEN)
                                    .with(DISMISS_HANDLER, (Integer unused) -> {})
                                    .build();
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
    public void testViewCanBeShownUsingTheModel() {
        // Confirm that the bottom sheet is not open.
        assertThat(mBottomSheetController.isSheetOpen(), is(false));

        runOnUiThreadBlocking(
                () -> {
                    mModel.set(SCREEN, FOP_SELECTOR);
                    mModel.get(SCREEN_VIEW_MODEL)
                            .get(SCREEN_ITEMS)
                            .add(
                                    new ListItem(
                                            BANK_ACCOUNT, createBankAccountModel(BANK_ACCOUNT_1)));
                    runOnUiThreadBlocking(() -> mModel.set(VISIBLE_STATE, SHOWN));
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        // Verify that the bottom sheet is opened, and shows the view.
        assertThat(mBottomSheetController.isSheetOpen(), is(true));
        assertThat(mView.getContentView().isShown(), is(true));
    }

    @Test
    @MediumTest
    public void testViewCanBeHiddenUsingTheModel() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(SCREEN, FOP_SELECTOR);
                    mModel.get(SCREEN_VIEW_MODEL)
                            .get(SCREEN_ITEMS)
                            .add(
                                    new ListItem(
                                            BANK_ACCOUNT, createBankAccountModel(BANK_ACCOUNT_1)));
                    runOnUiThreadBlocking(() -> mModel.set(VISIBLE_STATE, SHOWN));
                });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        // Confirm that the bottom sheet is opened, and shows the view.
        assertThat(mBottomSheetController.isSheetOpen(), is(true));
        assertThat(mView.getContentView().isShown(), is(true));

        runOnUiThreadBlocking(
                () -> {
                    mModel.set(VISIBLE_STATE, HIDDEN);
                });

        BottomSheetTestSupport.waitForState(mBottomSheetController, SheetState.HIDDEN);

        // Verify that the view is hidden, and the bottom sheet is closed.
        assertThat(mView.getContentView().isShown(), is(false));
        assertThat(mBottomSheetController.isSheetOpen(), is(false));
    }

    @Test
    @MediumTest
    public void testBankAccountShown() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(SCREEN, FOP_SELECTOR);
                    mModel.get(SCREEN_VIEW_MODEL)
                            .get(SCREEN_ITEMS)
                            .add(
                                    new ListItem(
                                            BANK_ACCOUNT, createBankAccountModel(BANK_ACCOUNT_1)));
                    mModel.get(SCREEN_VIEW_MODEL)
                            .get(SCREEN_ITEMS)
                            .add(
                                    new ListItem(
                                            BANK_ACCOUNT, createBankAccountModel(BANK_ACCOUNT_2)));
                    mModel.set(VISIBLE_STATE, SHOWN);
                });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        assertThat(getBankAccounts().getChildCount(), is(2));

        String expectedBankAccountSummary1 = String.format("Pix  •  %s ••••%s", "Checking", "1111");
        assertThat(getBankAccountNameAt(0).getText(), is("bankName1"));
        assertThat(getBankAccountSummaryAt(0).getText(), is(expectedBankAccountSummary1));
        assertThat(getBankAccountAdditionalInfoAt(0).getText(), is("Limit per Pix R$ 500"));

        String expectedBankAccountSummary2 = String.format("Pix  •  %s ••••%s", "Savings", "2222");
        assertThat(getBankAccountNameAt(1).getText(), is("bankName2"));
        assertThat(getBankAccountSummaryAt(1).getText(), is(expectedBankAccountSummary2));
        assertThat(getBankAccountAdditionalInfoAt(1).getText(), is("Limit per Pix R$ 500"));
    }

    @Test
    @MediumTest
    public void testDescriptionLine() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(SCREEN, FOP_SELECTOR);
                    mModel.get(SCREEN_VIEW_MODEL)
                            .get(SCREEN_ITEMS)
                            .add(mMediator.buildAdditionalInfo());
                    mModel.set(VISIBLE_STATE, SHOWN);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        TextView descriptionLine1 = mView.getContentView().findViewById(R.id.description_line);
        assertThat(
                descriptionLine1.getText(),
                hasToString(
                        containsString("To turn off Pix in Chrome, go to your payment settings")));
    }

    @Test
    @MediumTest
    public void testContinueButtonText() {
        runOnUiThreadBlocking(
                () -> {
                    PropertyModel bankAccountModel = createBankAccountModel(BANK_ACCOUNT_1);
                    mModel.set(SCREEN, FOP_SELECTOR);
                    mModel.get(SCREEN_VIEW_MODEL)
                            .get(SCREEN_ITEMS)
                            .add(new ListItem(BANK_ACCOUNT, bankAccountModel));
                    mModel.get(SCREEN_VIEW_MODEL)
                            .get(SCREEN_ITEMS)
                            .add(new ListItem(CONTINUE_BUTTON, bankAccountModel));
                    mModel.set(VISIBLE_STATE, SHOWN);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        TextView buttonText =
                mView.getContentView()
                        .findViewById(R.id.facilitated_payments_continue_button_title);
        assertThat(buttonText.getText(), is("Continue"));
    }

    @Test
    @MediumTest
    public void testProgressScreenShown() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(SCREEN, PROGRESS_SCREEN);
                    mModel.set(VISIBLE_STATE, SHOWN);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        // Verify that the {@link ProgressBar} is shown.
        assertThat(
                containsViewOfClass((ViewGroup) mView.getContentView(), ProgressBar.class),
                is(true));
    }

    @Test
    @MediumTest
    public void testErrorScreenShown() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(SCREEN, ERROR_SCREEN);
                    mModel.set(VISIBLE_STATE, SHOWN);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        // Verify that the error screen is shown.
        assertThat(
                containsViewWithId((ViewGroup) mView.getContentView(), R.id.error_screen),
                is(true));
    }

    @Test
    @MediumTest
    public void testErrorScreenContents() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(SCREEN, ERROR_SCREEN);
                    mModel.set(VISIBLE_STATE, SHOWN);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        TextView title = mView.getContentView().findViewById(R.id.title);
        assertThat(title.getText(), is("Something went wrong"));
        TextView description = mView.getContentView().findViewById(R.id.description);
        assertThat(
                description.getText(),
                is(
                        "Your transaction didn’t go through. No funds were withdrawn from your"
                                + " account."));
        ButtonCompat primaryButton = mView.getContentView().findViewById(R.id.primary_button);
        assertThat(primaryButton.getText(), is("OK"));
    }

    @Test
    @MediumTest
    public void testFopSelectorToProgressScreenSwapUpdatesView() {
        // Show the FOP selector.
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(SCREEN, FOP_SELECTOR);
                    mModel.get(SCREEN_VIEW_MODEL)
                            .get(SCREEN_ITEMS)
                            .add(
                                    new ListItem(
                                            BANK_ACCOUNT, createBankAccountModel(BANK_ACCOUNT_1)));
                    mModel.set(VISIBLE_STATE, SHOWN);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        // Confirm the FOP selector is shown.
        assertThat(mView.getContentView().isShown(), is(true));
        assertNotNull(mView.getContentView().findViewById(R.id.sheet_item_list));

        // Show the progress screen.
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(VISIBLE_STATE, SWAPPING_SCREEN);
                    mModel.set(SCREEN, PROGRESS_SCREEN);
                    mModel.set(VISIBLE_STATE, SHOWN);
                });

        // Verify that the progress screen is shown.
        assertThat(mView.getContentView().isShown(), is(true));
        assertThat(
                containsViewOfClass((ViewGroup) mView.getContentView(), ProgressBar.class),
                is(true));
    }

    @Test
    @MediumTest
    public void testProgressScreenToErrorScreenSwapUpdatesView() {
        // Show the progress screen.
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(SCREEN, PROGRESS_SCREEN);
                    mModel.set(VISIBLE_STATE, SHOWN);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        // Confirm the progress screen is shown.
        assertThat(mView.getContentView().isShown(), is(true));
        assertThat(
                containsViewOfClass((ViewGroup) mView.getContentView(), ProgressBar.class),
                is(true));

        // Show the error screen.
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(VISIBLE_STATE, SWAPPING_SCREEN);
                    mModel.set(SCREEN, ERROR_SCREEN);
                    mModel.set(VISIBLE_STATE, SHOWN);
                });

        // Verify that the error screen is shown.
        assertThat(mView.getContentView().isShown(), is(true));
        assertThat(
                containsViewWithId((ViewGroup) mView.getContentView(), R.id.error_screen),
                is(true));
    }

    private PropertyModel createBankAccountModel(BankAccount bankAccount) {
        return mMediator.createBankAccountModel(mActivityTestRule.getActivity(), bankAccount);
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

    private TextView getBankAccountAdditionalInfoAt(int index) {
        return getBankAccounts().getChildAt(index).findViewById(R.id.bank_account_additional_info);
    }

    private static boolean containsViewOfClass(ViewGroup parent, Class<?> clazz) {
        for (int i = 0; i < parent.getChildCount(); i++) {
            View child = parent.getChildAt(i);
            if (clazz.isInstance(child)) {
                return true;
            }
            if (child instanceof ViewGroup) {
                if (containsViewOfClass((ViewGroup) child, clazz)) {
                    return true;
                }
            }
        }
        return false;
    }

    private static boolean containsViewWithId(ViewGroup parent, int id) {
        for (int i = 0; i < parent.getChildCount(); i++) {
            View child = parent.getChildAt(i);
            if (child.getId() == id) {
                return true;
            }
            if (child instanceof ViewGroup) {
                if (containsViewWithId((ViewGroup) child, id)) {
                    return true;
                }
            }
        }
        return false;
    }
}
