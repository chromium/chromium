// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.verify;

import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;
import static org.chromium.chrome.browser.autofill.AutofillTestHelper.createLocalCreditCard;
import static org.chromium.chrome.browser.autofill.AutofillTestHelper.createVirtualCreditCard;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.ItemType.CREDIT_CARD;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.VISIBLE;
import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.touch_to_fill.common.BottomSheetFocusHelper;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Tests for {@link TouchToFillCreditCardView} */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "The methods of ChromeAccessibilityUtil don't seem to work with batching.")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TouchToFillCreditCardViewTest {
    private static final CreditCard VISA =
            createLocalCreditCard("Visa", "4111111111111111", "5", "2050");
    private static final CreditCard MASTER_CARD =
            createLocalCreditCard("MasterCard", "5555555555554444", "8", "2050");
    private static final CreditCard VIRTUAL_CARD = createVirtualCreditCard(/* name= */ "Mojo Jojo",
            /* number= */ "4111111111111111", /* month= */ "4", /* year= */ "2090",
            /* network= */ "Visa", /* iconId= */ 0, /* cardNameForAutofillDisplay= */ "Visa",
            /* obfuscatedLastFourDigits= */ "1111");

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Mock
    private TouchToFillCreditCardComponent.Delegate mDelegateMock;
    @Mock
    private Callback<Integer> mDismissCallback;
    @Mock
    private BottomSheetFocusHelper mBottomSheetFocusHelper;

    private BottomSheetController mBottomSheetController;
    private BottomSheetTestSupport mSheetSupport;
    private TouchToFillCreditCardCoordinator mCoordinator;
    private TouchToFillCreditCardView mTouchToFillCreditCardView;
    private PropertyModel mTouchToFillCreditCardModel;

    @Before
    public void setupTest() throws InterruptedException {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.startMainActivityOnBlankPage();
        mBottomSheetController = mActivityTestRule.getActivity()
                                         .getRootUiCoordinatorForTesting()
                                         .getBottomSheetController();
        mSheetSupport = new BottomSheetTestSupport(mBottomSheetController);
        runOnUiThreadBlocking(() -> {
            mCoordinator = new TouchToFillCreditCardCoordinator();
            mCoordinator.initialize(mActivityTestRule.getActivity(), mBottomSheetController,
                    mDelegateMock, mBottomSheetFocusHelper);
            mTouchToFillCreditCardModel =
                    new PropertyModel.Builder(TouchToFillCreditCardProperties.ALL_KEYS)
                            .with(VISIBLE, false)
                            .with(SHEET_ITEMS, new ModelList())
                            .with(DISMISS_HANDLER, mDismissCallback)
                            .build();
            mTouchToFillCreditCardView = new TouchToFillCreditCardView(
                    mActivityTestRule.getActivity(), mBottomSheetController);
            PropertyModelChangeProcessor.create(mTouchToFillCreditCardModel,
                    mTouchToFillCreditCardView,
                    TouchToFillCreditCardViewBinder::bindTouchToFillCreditCardView);
        });
    }

    @Test
    @MediumTest
    public void testVisibilityChangedByModel() {
        runOnUiThreadBlocking(() -> {
            mTouchToFillCreditCardModel.get(SHEET_ITEMS)
                    .add(new ListItem(CREDIT_CARD, createCardModel(VISA)));
        });
        // After setting the visibility to true, the view should exist and be visible.
        runOnUiThreadBlocking(() -> mTouchToFillCreditCardModel.set(VISIBLE, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        assertThat(mTouchToFillCreditCardView.getContentView().isShown(), is(true));

        // After hiding the view, the view should still exist but be invisible.
        runOnUiThreadBlocking(() -> mTouchToFillCreditCardModel.set(VISIBLE, false));
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HIDDEN);
        assertThat(mTouchToFillCreditCardView.getContentView().isShown(), is(false));
    }

    @Test
    @MediumTest
    public void testCredentialsChangedByModel() {
        runOnUiThreadBlocking(() -> {
            mTouchToFillCreditCardModel.get(SHEET_ITEMS)
                    .add(new ListItem(CREDIT_CARD, createCardModel(VISA)));
            mTouchToFillCreditCardModel.set(VISIBLE, true);
            mTouchToFillCreditCardModel.get(SHEET_ITEMS)
                    .add(new ListItem(CREDIT_CARD, createCardModel(MASTER_CARD)));
            mTouchToFillCreditCardModel.get(SHEET_ITEMS)
                    .add(new ListItem(CREDIT_CARD, createCardModel(VIRTUAL_CARD)));
        });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        assertThat(getCreditCards().getChildCount(), is(3));

        assertThat(getCreditCardNameAt(0).getText(), is(VISA.getCardNameForAutofillDisplay()));
        assertThat(getCreditCardNumberAt(0).getText(), is(VISA.getObfuscatedLastFourDigits()));
        assertThat(getCreditCardExpirationAt(0).getText(), is(createExpirationDateString(VISA)));

        assertThat(
                getCreditCardNameAt(1).getText(), is(MASTER_CARD.getCardNameForAutofillDisplay()));
        assertThat(
                getCreditCardNumberAt(1).getText(), is(MASTER_CARD.getObfuscatedLastFourDigits()));
        assertThat(getCreditCardExpirationAt(1).getText(),
                is(createExpirationDateString(MASTER_CARD)));

        assertThat(
                getCreditCardNameAt(2).getText(), is(VIRTUAL_CARD.getCardNameForAutofillDisplay()));
        assertThat(
                getCreditCardNumberAt(2).getText(), is(VIRTUAL_CARD.getObfuscatedLastFourDigits()));
        assertThat(getCreditCardExpirationAt(2).getText(), is(getVirtualCardLabel()));
    }

    @Test
    @MediumTest
    public void testScanNewCardButtonIsHidden() {
        runOnUiThreadBlocking(
                () -> mCoordinator.showSheet(new CreditCard[] {VISA, MASTER_CARD}, false));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        runOnUiThreadBlocking(() -> { mSheetSupport.setSheetState(SheetState.FULL, false); });

        onView(withId(R.id.scan_new_card)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testScanNewCardClick() {
        runOnUiThreadBlocking(
                () -> mCoordinator.showSheet(new CreditCard[] {VISA, MASTER_CARD}, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        runOnUiThreadBlocking(() -> { mSheetSupport.setSheetState(SheetState.FULL, false); });

        onView(withId(R.id.scan_new_card)).perform(click());

        verify(mDelegateMock).scanCreditCard();
    }

    @Test
    @MediumTest
    public void testManagePaymentMethodsClick() {
        runOnUiThreadBlocking(
                () -> mCoordinator.showSheet(new CreditCard[] {VISA, MASTER_CARD}, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        runOnUiThreadBlocking(() -> mSheetSupport.setSheetState(SheetState.FULL, false));

        onView(withId(R.id.manage_payment_methods)).perform(click());

        verify(mDelegateMock).showCreditCardSettings();
    }

    @Test
    @MediumTest
    public void testContinueButtonClick() {
        runOnUiThreadBlocking(() -> mCoordinator.showSheet(new CreditCard[] {VISA}, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        runOnUiThreadBlocking(() -> mSheetSupport.setSheetState(SheetState.FULL, false));

        onView(withId(R.id.touch_to_fill_button_title)).perform(click());

        verify(mDelegateMock).suggestionSelected(VISA.getGUID(), VISA.getIsVirtual());
    }

    @Test
    @MediumTest
    public void testSheetStartsInFullHeightForAccessibility() {
        // Enabling the accessibility settings.
        runOnUiThreadBlocking(() -> {
            ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(true);
            ChromeAccessibilityUtil.get().setTouchExplorationEnabledForTesting(true);
        });

        runOnUiThreadBlocking(() -> mCoordinator.showSheet(new CreditCard[] {VISA}, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        // The sheet should be expanded to full height.
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);

        // Disabling the accessibility settings.
        runOnUiThreadBlocking(() -> {
            ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(false);
            ChromeAccessibilityUtil.get().setTouchExplorationEnabledForTesting(false);
        });
    }

    @Test
    @MediumTest
    public void testSheetStartsInHalfHeightForAccessibilityDisabled() {
        runOnUiThreadBlocking(() -> mCoordinator.showSheet(new CreditCard[] {VISA}, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        // The sheet should be expanded to half height.
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HALF);
    }

    private RecyclerView getCreditCards() {
        return mTouchToFillCreditCardView.getContentView().findViewById(R.id.sheet_item_list);
    }

    private TextView getCreditCardNameAt(int index) {
        return getCreditCards().getChildAt(index).findViewById(R.id.card_name);
    }

    private TextView getCreditCardNumberAt(int index) {
        return getCreditCards().getChildAt(index).findViewById(R.id.card_number);
    }

    private TextView getCreditCardExpirationAt(int index) {
        return getCreditCards().getChildAt(index).findViewById(R.id.description_line_2);
    }

    private @SheetState int getBottomSheetState() {
        return mBottomSheetController.getSheetState();
    }

    private static PropertyModel createCardModel(CreditCard card) {
        PropertyModel.Builder creditCardModelBuilder =
                new PropertyModel
                        .Builder(TouchToFillCreditCardProperties.CreditCardProperties.ALL_KEYS)
                        .with(TouchToFillCreditCardProperties.CreditCardProperties.CARD_NAME,
                                card.getCardNameForAutofillDisplay())
                        .with(TouchToFillCreditCardProperties.CreditCardProperties.CARD_NUMBER,
                                card.getObfuscatedLastFourDigits());
        if (card.getIsVirtual()) {
            creditCardModelBuilder.with(
                    TouchToFillCreditCardProperties.CreditCardProperties.VIRTUAL_CARD_LABEL,
                    getVirtualCardLabel());
        } else {
            creditCardModelBuilder.with(
                    TouchToFillCreditCardProperties.CreditCardProperties.CARD_EXPIRATION,
                    createExpirationDateString(card));
        }
        return creditCardModelBuilder.build();
    }

    private static String createExpirationDateString(CreditCard card) {
        return ContextUtils.getApplicationContext()
                .getString(R.string.autofill_credit_card_two_line_label_from_card_number)
                .replace("$1",
                        card.getFormattedExpirationDate(ContextUtils.getApplicationContext()));
    }

    private static String getVirtualCardLabel() {
        return ContextUtils.getApplicationContext().getString(
                R.string.autofill_virtual_card_number_switch_label);
    }
}
