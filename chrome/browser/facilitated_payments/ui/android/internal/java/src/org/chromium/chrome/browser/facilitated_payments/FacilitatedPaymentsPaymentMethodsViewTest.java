// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.hasToString;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.hamcrest.Matchers.nullValue;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.FopSelectorProperties.SCREEN_ITEMS;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.BANK_ACCOUNT;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.CONTINUE_BUTTON;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.EWALLET;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.PAYMENT_APP;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SCREEN;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SCREEN_VIEW_MODEL;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SURVIVES_NAVIGATION;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SequenceScreen.ERROR_SCREEN;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SequenceScreen.FOP_SELECTOR;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SequenceScreen.PIX_ACCOUNT_LINKING_PROMPT;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SequenceScreen.PROGRESS_SCREEN;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.UI_EVENT_LISTENER;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.VISIBLE_STATE;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.VisibleState.HIDDEN;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.VisibleState.SHOWN;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.VisibleState.SWAPPING_SCREEN;

import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.autofill.payments.AccountType;
import org.chromium.components.autofill.payments.BankAccount;
import org.chromium.components.autofill.payments.Ewallet;
import org.chromium.components.autofill.payments.PaymentInstrument;
import org.chromium.components.autofill.payments.PaymentRail;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.ButtonCompat;

import java.util.List;

/** Instrumentation tests for {@link FacilitatedPaymentsPaymentMethodsView}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisableFeatures({ChromeFeatureList.FACILITATED_PAYMENTS_ENABLE_A2A_PAYMENT})
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
    private static final String PAYMENT_APP_1_NAME = "Payment App";
    private static final ResolveInfo PAYMENT_APP_1 = createPaymentApp(PAYMENT_APP_1_NAME);
    private static final String PAYMENT_APP_2_NAME = "Another Payment App";
    private static final ResolveInfo PAYMENT_APP_2 = createPaymentApp(PAYMENT_APP_2_NAME);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Mock private FacilitatedPaymentsPaymentMethodsComponent.Delegate mDelegateMock;

    private BottomSheetController mBottomSheetController;
    private BottomSheetTestSupport mSheetTestSupport;
    private FacilitatedPaymentsPaymentMethodsMediator mMediator;
    private FacilitatedPaymentsPaymentMethodsView mView;
    private PropertyModel mModel;
    private WebPageStation mPage;

    @Before
    public void setUp() {
        mPage = mActivityTestRule.startOnBlankPage();
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
                                    .with(UI_EVENT_LISTENER, (Integer unused) -> {})
                                    .build();
                    mView =
                            new FacilitatedPaymentsPaymentMethodsView(
                                    mActivityTestRule.getActivity(), mBottomSheetController);
                    mMediator.initialize(
                            ContextUtils.getApplicationContext(),
                            mModel,
                            mDelegateMock,
                            mActivityTestRule.getProfile(false));
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

        assertThat(getSheetItems().getChildCount(), is(2));

        assertThat(getBankAccountNameAt(0).getText(), is("bankName1"));
        assertThat(getBankAccountPaymentRailAt(0).getText(), is("Pix  •  "));
        assertThat(getBankAccountTypeAt(0).getText(), is("Checking"));
        assertThat(getBankAccountNumberAt(0).getText(), is("••••1111"));
        assertThat(getBankAccountAdditionalInfoAt(0).getText(), is("Limit per Pix R$ 500"));

        assertThat(getBankAccountNameAt(1).getText(), is("bankName2"));
        assertThat(getBankAccountPaymentRailAt(1).getText(), is("Pix  •  "));
        assertThat(getBankAccountTypeAt(1).getText(), is("Savings"));
        assertThat(getBankAccountNumberAt(1).getText(), is("••••2222"));
        assertThat(getBankAccountAdditionalInfoAt(1).getText(), is("Limit per Pix R$ 500"));
    }

    @Test
    @MediumTest
    public void testEwalletShown() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(SCREEN, FOP_SELECTOR);
                    mModel.get(SCREEN_VIEW_MODEL)
                            .get(SCREEN_ITEMS)
                            .add(new ListItem(EWALLET, createEwalletModel(EWALLET_1)));
                    mModel.get(SCREEN_VIEW_MODEL)
                            .get(SCREEN_ITEMS)
                            .add(new ListItem(EWALLET, createEwalletModel(EWALLET_2)));
                    mModel.set(VISIBLE_STATE, SHOWN);
                });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        assertThat(getSheetItems().getChildCount(), is(2));

        assertThat(getEwalletNameAt(0).getText(), is("eWalletName1"));
        assertThat(getAccountDisplayNameAt(0).getText(), is("account display name 1"));

        assertThat(getEwalletNameAt(1).getText(), is("eWalletName2"));
        assertThat(getAccountDisplayNameAt(1).getText(), is("account display name 2"));
    }

    @Test
    @MediumTest
    public void testPaymentAppShown() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(SCREEN, FOP_SELECTOR);
                    mModel.get(SCREEN_VIEW_MODEL)
                            .get(SCREEN_ITEMS)
                            .add(new ListItem(PAYMENT_APP, createPaymentAppModel(PAYMENT_APP_1)));
                    mModel.get(SCREEN_VIEW_MODEL)
                            .get(SCREEN_ITEMS)
                            .add(new ListItem(PAYMENT_APP, createPaymentAppModel(PAYMENT_APP_2)));
                    mModel.set(VISIBLE_STATE, SHOWN);
                });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        assertThat(getSheetItems().getChildCount(), is(2));
        assertThat(getPaymentAppNameAt(0).getText(), is(PAYMENT_APP_1_NAME));
        assertThat(getPaymentAppNameAt(1).getText(), is(PAYMENT_APP_2_NAME));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.FACILITATED_PAYMENTS_ENABLE_A2A_PAYMENT})
    public void testEwalletAndPaymentAppShown() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(SCREEN, FOP_SELECTOR);
                    mModel.get(SCREEN_VIEW_MODEL)
                            .get(SCREEN_ITEMS)
                            .add(new ListItem(EWALLET, createEwalletModel(EWALLET_1)));
                    mModel.get(SCREEN_VIEW_MODEL)
                            .get(SCREEN_ITEMS)
                            .add(new ListItem(PAYMENT_APP, createPaymentAppModel(PAYMENT_APP_1)));
                    mModel.set(VISIBLE_STATE, SHOWN);
                });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        assertThat(getSheetItems().getChildCount(), is(2));
        assertThat(getEwalletNameAt(0).getText(), is("eWalletName1"));
        assertThat(getAccountDisplayNameAt(0).getText(), is("account display name 1"));
        assertThat(getPaymentAppNameAt(1).getText(), is(PAYMENT_APP_1_NAME));
    }

    // This test checks that the header security image and header description are not shown and
    // header product icon is present to user when eWallet and payment app both are available.
    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.FACILITATED_PAYMENTS_ENABLE_A2A_PAYMENT})
    public void testPaymentAppHeaderWhenBothEwalletAndPaymentAppAvailable() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(SCREEN, FOP_SELECTOR);
                    mModel.get(SCREEN_VIEW_MODEL)
                            .get(SCREEN_ITEMS)
                            .add(
                                    mMediator.buildPaymentLinkHeader(
                                            mActivityTestRule.getActivity(),
                                            List.of(EWALLET_3),
                                            List.of(PAYMENT_APP_1)));
                    mModel.set(VISIBLE_STATE, SHOWN);
                });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        assertThat(getSheetItems().getChildCount(), is(1));
        ImageView headerSecurityCheckImage = getHeaderSecurityCheckImageAt(0);
        TextView headerDescription = getHeaderDescriptionAt(0);
        ImageView headerProductIcon = getHeaderProductIconAt(0);

        assertThat(headerProductIcon.getContentDescription(), is("Google Pay"));
        assertThat(headerSecurityCheckImage.getVisibility(), is(View.GONE));
        assertThat(headerDescription.getVisibility(), is(View.GONE));
    }

    // This test checks that the header product icon, header security image and header description
    // are not shown to user when only payment app is available.
    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.FACILITATED_PAYMENTS_ENABLE_A2A_PAYMENT})
    public void
            testPaymentAppProductionIconSecurtityCheckAndDescriptionNotVisibleWhenOnlyPaymentAppAvailable() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(SCREEN, FOP_SELECTOR);
                    mModel.get(SCREEN_VIEW_MODEL)
                            .get(SCREEN_ITEMS)
                            .add(
                                    mMediator.buildPaymentLinkHeader(
                                            mActivityTestRule.getActivity(),
                                            List.of(),
                                            List.of(PAYMENT_APP_1)));
                    mModel.set(VISIBLE_STATE, SHOWN);
                });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        assertThat(getSheetItems().getChildCount(), is(1));
        ImageView headerSecurityCheckImage = getHeaderSecurityCheckImageAt(0);
        TextView headerDescription = getHeaderDescriptionAt(0);
        ImageView headerProductIcon = getHeaderProductIconAt(0);

        assertThat(headerProductIcon.getContentDescription(), nullValue());
        assertThat(headerSecurityCheckImage.getVisibility(), is(View.GONE));
        assertThat(headerDescription.getVisibility(), is(View.GONE));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.FACILITATED_PAYMENTS_ENABLE_A2A_PAYMENT})
    public void
            testPaymentAppHeaderProductIconContentDescriptionWhenEwalletAndPaymentAppAvailable() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(SCREEN, FOP_SELECTOR);
                    mModel.get(SCREEN_VIEW_MODEL)
                            .get(SCREEN_ITEMS)
                            .add(
                                    mMediator.buildPaymentLinkHeader(
                                            mActivityTestRule.getActivity(),
                                            List.of(EWALLET_1),
                                            List.of(PAYMENT_APP_1)));
                    mModel.set(VISIBLE_STATE, SHOWN);
                });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        assertThat(getSheetItems().getChildCount(), is(1));
        ImageView headerProductIcon = getHeaderProductIconAt(0);

        assertThat(headerProductIcon.getContentDescription(), is("Google Pay"));
    }

    @Test
    @MediumTest
    public void testPixHeaderProductIconContentDescription() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(SCREEN, FOP_SELECTOR);
                    mModel.get(SCREEN_VIEW_MODEL)
                            .get(SCREEN_ITEMS)
                            .add(mMediator.buildPixHeader(mActivityTestRule.getActivity()));
                    mModel.set(VISIBLE_STATE, SHOWN);
                });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        assertThat(getSheetItems().getChildCount(), is(1));
        ImageView headerProductIcon = getHeaderProductIconAt(0);

        assertThat(headerProductIcon.getContentDescription(), is("Google Pay, Pix"));
    }

    @Test
    @MediumTest
    public void testPixHeaderFirstTimeCheckNotVisible() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(SCREEN, FOP_SELECTOR);
                    mModel.get(SCREEN_VIEW_MODEL)
                            .get(SCREEN_ITEMS)
                            .add(mMediator.buildPixHeader(mActivityTestRule.getActivity()));
                    mModel.set(VISIBLE_STATE, SHOWN);
                });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        assertThat(getSheetItems().getChildCount(), is(1));
        ImageView headerSecurityCheckImage = getHeaderSecurityCheckImageAt(0);
        TextView headerDescription = getHeaderDescriptionAt(0);

        assertThat(headerSecurityCheckImage.getVisibility(), is(View.GONE));
        assertThat(headerDescription.getVisibility(), is(View.VISIBLE));
    }

    @Test
    @MediumTest
    public void testEwalletHeaderProductIconContentDescription() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(SCREEN, FOP_SELECTOR);
                    mModel.get(SCREEN_VIEW_MODEL)
                            .get(SCREEN_ITEMS)
                            .add(
                                    mMediator.buildPaymentLinkHeader(
                                            mActivityTestRule.getActivity(),
                                            List.of(EWALLET_1),
                                            List.of()));
                    mModel.set(VISIBLE_STATE, SHOWN);
                });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        assertThat(getSheetItems().getChildCount(), is(1));
        ImageView headerProductIcon = getHeaderProductIconAt(0);

        assertThat(headerProductIcon.getContentDescription(), is("Google Pay"));
    }

    @Test
    @MediumTest
    public void testEwalletHeaderFirstTimeCheckVisible() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(SCREEN, FOP_SELECTOR);
                    mModel.get(SCREEN_VIEW_MODEL)
                            .get(SCREEN_ITEMS)
                            .add(
                                    mMediator.buildPaymentLinkHeader(
                                            mActivityTestRule.getActivity(),
                                            List.of(EWALLET_3),
                                            List.of()));
                    mModel.set(VISIBLE_STATE, SHOWN);
                });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        assertThat(getSheetItems().getChildCount(), is(1));
        ImageView headerSecurityCheckImage = getHeaderSecurityCheckImageAt(0);
        TextView headerDescription = getHeaderDescriptionAt(0);

        assertThat(headerSecurityCheckImage.getVisibility(), is(View.VISIBLE));
        assertThat(headerDescription.getVisibility(), is(View.VISIBLE));
    }

    @Test
    @MediumTest
    public void testEwalletHeaderFirstTimeCheckNotVisible() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(SCREEN, FOP_SELECTOR);
                    mModel.get(SCREEN_VIEW_MODEL)
                            .get(SCREEN_ITEMS)
                            .add(
                                    mMediator.buildPaymentLinkHeader(
                                            mActivityTestRule.getActivity(),
                                            List.of(EWALLET_3, EWALLET_4),
                                            List.of()));
                    mModel.set(VISIBLE_STATE, SHOWN);
                });

        assertThat(getSheetItems().getChildCount(), is(1));
        ImageView headerSecurityCheckImage = getHeaderSecurityCheckImageAt(0);
        TextView headerDescription = getHeaderDescriptionAt(0);

        assertThat(headerSecurityCheckImage.getVisibility(), is(View.GONE));
        assertThat(headerDescription.getVisibility(), is(View.GONE));
    }

    @Test
    @MediumTest
    public void testEwalletAndPaymentAppHeaderProductIconAndTitleMargin() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(SCREEN, FOP_SELECTOR);
                    mModel.get(SCREEN_VIEW_MODEL)
                            .get(SCREEN_ITEMS)
                            .add(
                                    mMediator.buildPaymentLinkHeader(
                                            mActivityTestRule.getActivity(),
                                            List.of(EWALLET_1),
                                            List.of(PAYMENT_APP_1)));
                    mModel.set(VISIBLE_STATE, SHOWN);
                });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        assertThat(getSheetItems().getChildCount(), is(1));
        ImageView headerProductIcon = getHeaderProductIconAt(0);
        TextView headerTitle = getHeaderTitleAt(0);
        ViewGroup.MarginLayoutParams params =
                (ViewGroup.MarginLayoutParams) headerTitle.getLayoutParams();

        assertThat(headerProductIcon.getVisibility(), is(View.VISIBLE));
        assertThat(params.topMargin, is(not(0)));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.FACILITATED_PAYMENTS_ENABLE_A2A_PAYMENT})
    public void testPaymentAppHeaderProductIconAndTitleMargin() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(SCREEN, FOP_SELECTOR);
                    mModel.get(SCREEN_VIEW_MODEL)
                            .get(SCREEN_ITEMS)
                            .add(
                                    mMediator.buildPaymentLinkHeader(
                                            mActivityTestRule.getActivity(),
                                            List.of(),
                                            List.of(PAYMENT_APP_1)));
                    mModel.set(VISIBLE_STATE, SHOWN);
                });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        assertThat(getSheetItems().getChildCount(), is(1));
        ImageView headerProductIcon = getHeaderProductIconAt(0);
        TextView headerTitle = getHeaderTitleAt(0);
        ViewGroup.MarginLayoutParams params =
                (ViewGroup.MarginLayoutParams) headerTitle.getLayoutParams();

        assertThat(headerProductIcon.getVisibility(), is(View.GONE));
        assertThat(params.topMargin, is(not(0)));
    }

    @Test
    @MediumTest
    public void testPixDescriptionLine() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(SCREEN, FOP_SELECTOR);
                    mModel.get(SCREEN_VIEW_MODEL)
                            .get(SCREEN_ITEMS)
                            .add(mMediator.buildPixAdditionalInfo());
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
    public void testEwalletDescriptionLine() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(SCREEN, FOP_SELECTOR);
                    mModel.get(SCREEN_VIEW_MODEL)
                            .get(SCREEN_ITEMS)
                            .add(
                                    mMediator.buildPaymentLinkAdditionalInfo(
                                            List.of(EWALLET_1), List.of()));
                    mModel.set(VISIBLE_STATE, SHOWN);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        TextView descriptionLine1 = mView.getContentView().findViewById(R.id.description_line);
        assertThat(
                descriptionLine1.getText(),
                hasToString(
                        containsString(
                                "Your saved auto-pay method may be used for this payment. To turn"
                                        + " off eWallets in Chrome, go to your payment settings")));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.FACILITATED_PAYMENTS_ENABLE_A2A_PAYMENT})
    public void testEwalletAndPaymentAppDescriptionLine() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(SCREEN, FOP_SELECTOR);
                    mModel.get(SCREEN_VIEW_MODEL)
                            .get(SCREEN_ITEMS)
                            .add(
                                    mMediator.buildPaymentLinkAdditionalInfo(
                                            List.of(EWALLET_1), List.of(PAYMENT_APP_1)));
                    mModel.set(VISIBLE_STATE, SHOWN);
                });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        TextView descriptionLine1 = mView.getContentView().findViewById(R.id.description_line);
        assertThat(
                descriptionLine1.getText(),
                hasToString(
                        containsString(
                                "Your saved auto-pay method may be used for this payment. To turn"
                                    + " off eWallets or payment app in Chrome, go to your payment"
                                    + " settings")));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.FACILITATED_PAYMENTS_ENABLE_A2A_PAYMENT})
    public void testPaymentAppDescriptionLineShown() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(SCREEN, FOP_SELECTOR);
                    mModel.get(SCREEN_VIEW_MODEL)
                            .get(SCREEN_ITEMS)
                            .add(
                                    mMediator.buildPaymentLinkAdditionalInfo(
                                            List.of(), List.of(PAYMENT_APP_1)));
                    mModel.set(VISIBLE_STATE, SHOWN);
                });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        TextView descriptionLine1 = mView.getContentView().findViewById(R.id.description_line);

        assertThat(
                descriptionLine1.getText(),
                hasToString(
                        containsString(
                                "To turn off payment app in Chrome, go to your payment settings")));
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
        assertNotNull(getSheetItems());

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

    @Test
    @MediumTest
    public void testPixAccountLinkingPromptShown() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(SCREEN, PIX_ACCOUNT_LINKING_PROMPT);
                    mModel.set(VISIBLE_STATE, SHOWN);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        // Verify that the Pix account linking prompt is shown.
        assertThat(
                containsViewWithId(
                        (ViewGroup) mView.getContentView(), R.id.pix_account_linking_prompt),
                is(true));
    }

    @Test
    @MediumTest
    public void testPixAccountLinkingPromptContents() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(SCREEN, PIX_ACCOUNT_LINKING_PROMPT);
                    mModel.set(VISIBLE_STATE, SHOWN);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        ImageView productIcon = mView.getContentView().findViewById(R.id.product_icon);
        assertNotNull(productIcon);

        TextView title = mView.getContentView().findViewById(R.id.title);
        assertThat(title.getText(), is("Pay with Pix directly in Chrome next time"));

        TextView valuePropMessage1 = mView.getContentView().findViewById(R.id.value_prop_message_1);
        assertThat(valuePropMessage1.getText(), is("Enable Pix by linking your account quickly"));
        assertNotNull(valuePropMessage1.getCompoundDrawablesRelative()[0]);
        TextView valuePropMessage2 = mView.getContentView().findViewById(R.id.value_prop_message_2);
        assertThat(valuePropMessage2.getText(), is("Pay in Chrome without using your bank app"));
        assertNotNull(valuePropMessage2.getCompoundDrawablesRelative()[0]);
        TextView valuePropMessage3 = mView.getContentView().findViewById(R.id.value_prop_message_3);
        assertThat(valuePropMessage3.getText(), is("Encryption protects your personal info"));
        assertNotNull(valuePropMessage3.getCompoundDrawablesRelative()[0]);

        ButtonCompat acceptButton = mView.getContentView().findViewById(R.id.accept_button);
        assertThat(acceptButton.getText(), is("Enable Pix in Wallet"));
        ButtonCompat declineButton = mView.getContentView().findViewById(R.id.decline_button);
        assertThat(declineButton.getText(), is("No thanks"));
    }

    @Test
    @MediumTest
    public void testViewLifecycleCanBeManipulatedByTheModel() {
        // Verify that the view's initial state does not survive page navigations (does not have a
        // custom lifecycle).
        assertThat(mView.hasCustomLifecycle(), is(false));

        runOnUiThreadBlocking(
                () -> {
                    mModel.set(SURVIVES_NAVIGATION, true);
                });

        assertThat(mView.hasCustomLifecycle(), is(true));

        runOnUiThreadBlocking(
                () -> {
                    mModel.set(SURVIVES_NAVIGATION, false);
                });

        assertThat(mView.hasCustomLifecycle(), is(false));
    }

    private RecyclerView getSheetItems() {
        return mView.getContentView().findViewById(R.id.sheet_item_list);
    }

    private PropertyModel createBankAccountModel(BankAccount bankAccount) {
        return mMediator.createBankAccountModel(mActivityTestRule.getActivity(), bankAccount);
    }

    private TextView getBankAccountNameAt(int index) {
        return getSheetItems().getChildAt(index).findViewById(R.id.bank_name);
    }

    private TextView getBankAccountPaymentRailAt(int index) {
        return getSheetItems().getChildAt(index).findViewById(R.id.bank_account_payment_rail);
    }

    private TextView getBankAccountTypeAt(int index) {
        return getSheetItems().getChildAt(index).findViewById(R.id.bank_account_type);
    }

    private TextView getBankAccountNumberAt(int index) {
        return getSheetItems().getChildAt(index).findViewById(R.id.bank_account_number);
    }

    private TextView getBankAccountAdditionalInfoAt(int index) {
        return getSheetItems().getChildAt(index).findViewById(R.id.bank_account_additional_info);
    }

    private PropertyModel createEwalletModel(Ewallet eWallet) {
        return mMediator.createEwalletModel(mActivityTestRule.getActivity(), eWallet);
    }

    private TextView getEwalletNameAt(int index) {
        return getSheetItems().getChildAt(index).findViewById(R.id.ewallet_name);
    }

    private PropertyModel createPaymentAppModel(ResolveInfo app) {
        return mMediator.createPaymentAppModel(mActivityTestRule.getActivity(), app);
    }

    private TextView getPaymentAppNameAt(int index) {
        return getSheetItems().getChildAt(index).findViewById(R.id.payment_app_name);
    }

    private TextView getAccountDisplayNameAt(int index) {
        return getSheetItems().getChildAt(index).findViewById(R.id.account_display_name);
    }

    private ImageView getHeaderProductIconAt(int index) {
        return getSheetItems().getChildAt(index).findViewById(R.id.branding_icon);
    }

    private ImageView getHeaderSecurityCheckImageAt(int index) {
        return getSheetItems().getChildAt(index).findViewById(R.id.security_check_illustration);
    }

    private TextView getHeaderDescriptionAt(int index) {
        return getSheetItems().getChildAt(index).findViewById(R.id.description_text);
    }

    private TextView getHeaderTitleAt(int index) {
        return getSheetItems().getChildAt(index).findViewById(R.id.sheet_title);
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

    private static ResolveInfo createPaymentApp(String appLabel) {
        ActivityInfo activityInfo = new ActivityInfo();
        activityInfo.packageName = "some.payment.app";
        activityInfo.name = "RandomActivity";

        ResolveInfo resolveInfo = mock(ResolveInfo.class);
        resolveInfo.activityInfo = activityInfo;
        when(resolveInfo.loadLabel(any(PackageManager.class))).thenReturn(appLabel);
        when(resolveInfo.loadIcon(any(PackageManager.class))).thenReturn(mock(Drawable.class));
        return resolveInfo;
    }
}
