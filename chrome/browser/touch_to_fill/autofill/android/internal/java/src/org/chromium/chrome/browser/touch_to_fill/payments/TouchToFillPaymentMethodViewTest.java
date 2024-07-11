// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;
import static org.chromium.chrome.browser.autofill.AutofillTestHelper.createClickActionWithFlags;
import static org.chromium.chrome.browser.autofill.AutofillTestHelper.createCreditCard;
import static org.chromium.chrome.browser.autofill.AutofillTestHelper.createLocalCreditCard;
import static org.chromium.chrome.browser.autofill.AutofillTestHelper.createVirtualCreditCard;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.CREDIT_CARD;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.FILL_BUTTON;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.IBAN;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.VISIBLE;

import android.view.MotionEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.ImageView;
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
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill.PersonalDataManager.Iban;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.touch_to_fill.common.FillableItemCollectionInfo;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Tests for {@link TouchToFillPaymentMethodView} */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "The methods of ChromeAccessibilityUtil don't seem to work with batching.")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TouchToFillPaymentMethodViewTest {
    private static final CreditCard VISA =
            createCreditCard(
                    "Visa",
                    "4111111111111111",
                    "5",
                    "2050",
                    true,
                    "Visa",
                    "• • • • 1111",
                    0,
                    "visa");
    private static final CreditCard NICKNAMED_VISA =
            createCreditCard(
                    "Visa",
                    "4111111111111111",
                    "5",
                    "2050",
                    true,
                    "Best Card",
                    "• • • • 1111",
                    0,
                    "visa");
    private static final CreditCard MASTER_CARD =
            createLocalCreditCard("MasterCard", "5555555555554444", "8", "2050");
    private static final CreditCard VIRTUAL_CARD =
            createVirtualCreditCard(
                    /* name= */ "Mojo Jojo",
                    /* number= */ "4111111111111111",
                    /* month= */ "4",
                    /* year= */ "2090",
                    /* network= */ "Visa",
                    /* iconId= */ 0,
                    /* cardNameForAutofillDisplay= */ "Visa",
                    /* obfuscatedLastFourDigits= */ "1111");
    private static final CreditCard LONG_CARD_NAME_CARD =
            createCreditCard(
                    "MJ",
                    "4111111111111111",
                    "5",
                    "2050",
                    false,
                    "How much wood would a woodchuck chuck if a woodchuck could chuck wood",
                    "• • • • 1111",
                    0,
                    "visa");

    private static final Iban LOCAL_IBAN =
            Iban.createLocal(
                    /* guid= */ "000000111111",
                    /* label= */ "CH56 **** **** **** *800 9",
                    /* nickname= */ "My brother's IBAN",
                    /* value= */ "CH5604835012345678009");

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Mock private Callback<Integer> mDismissCallback;
    @Mock private FillableItemCollectionInfo mItemCollectionInfo;

    private BottomSheetController mBottomSheetController;
    private BottomSheetTestSupport mSheetTestSupport;
    private TouchToFillPaymentMethodView mTouchToFillPaymentMethodView;
    private PropertyModel mTouchToFillPaymentMethodModel;

    @Before
    public void setupTest() throws InterruptedException {
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
                    mTouchToFillPaymentMethodModel =
                            new PropertyModel.Builder(TouchToFillPaymentMethodProperties.ALL_KEYS)
                                    .with(VISIBLE, false)
                                    .with(SHEET_ITEMS, new ModelList())
                                    .with(DISMISS_HANDLER, mDismissCallback)
                                    .build();
                    mTouchToFillPaymentMethodView =
                            new TouchToFillPaymentMethodView(
                                    mActivityTestRule.getActivity(), mBottomSheetController);
                    PropertyModelChangeProcessor.create(
                            mTouchToFillPaymentMethodModel,
                            mTouchToFillPaymentMethodView,
                            TouchToFillPaymentMethodViewBinder::bindTouchToFillPaymentMethodView);
                });
    }

    @Test
    @MediumTest
    public void testVisibilityChangedByModel() {
        runOnUiThreadBlocking(
                () -> {
                    mTouchToFillPaymentMethodModel
                            .get(SHEET_ITEMS)
                            .add(
                                    new ListItem(
                                            CREDIT_CARD,
                                            createCardModel(VISA, mItemCollectionInfo)));
                });
        // After setting the visibility to true, the view should exist and be visible.
        runOnUiThreadBlocking(() -> mTouchToFillPaymentMethodModel.set(VISIBLE, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        assertThat(mTouchToFillPaymentMethodView.getContentView().isShown(), is(true));

        // After hiding the view, the view should still exist but be invisible.
        runOnUiThreadBlocking(() -> mTouchToFillPaymentMethodModel.set(VISIBLE, false));
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HIDDEN);
        assertThat(mTouchToFillPaymentMethodView.getContentView().isShown(), is(false));
    }

    @Test
    @MediumTest
    public void testCredentialsChangedByModel() {
        runOnUiThreadBlocking(
                () -> {
                    mTouchToFillPaymentMethodModel
                            .get(SHEET_ITEMS)
                            .add(
                                    new ListItem(
                                            CREDIT_CARD,
                                            createCardModel(VISA, mItemCollectionInfo)));
                    mTouchToFillPaymentMethodModel.set(VISIBLE, true);
                    mTouchToFillPaymentMethodModel
                            .get(SHEET_ITEMS)
                            .add(
                                    new ListItem(
                                            CREDIT_CARD,
                                            createCardModel(MASTER_CARD, mItemCollectionInfo)));
                    mTouchToFillPaymentMethodModel
                            .get(SHEET_ITEMS)
                            .add(
                                    new ListItem(
                                            CREDIT_CARD,
                                            createCardModel(VIRTUAL_CARD, mItemCollectionInfo)));
                });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        assertThat(getCreditCards().getChildCount(), is(3));

        assertThat(getCreditCardNameAt(0).getText(), is(VISA.getCardNameForAutofillDisplay()));
        assertThat(getCreditCardNumberAt(0).getText(), is(VISA.getObfuscatedLastFourDigits()));
        assertThat(
                getCreditCardExpirationAt(0).getText(),
                is(VISA.getFormattedExpirationDate(ContextUtils.getApplicationContext())));

        assertThat(
                getCreditCardNameAt(1).getText(), is(MASTER_CARD.getCardNameForAutofillDisplay()));
        assertThat(
                getCreditCardNumberAt(1).getText(), is(MASTER_CARD.getObfuscatedLastFourDigits()));
        assertThat(
                getCreditCardExpirationAt(1).getText(),
                is(MASTER_CARD.getFormattedExpirationDate(ContextUtils.getApplicationContext())));

        assertThat(
                getCreditCardNameAt(2).getText(), is(VIRTUAL_CARD.getCardNameForAutofillDisplay()));
        assertThat(
                getCreditCardNumberAt(2).getText(), is(VIRTUAL_CARD.getObfuscatedLastFourDigits()));
        assertThat(getCreditCardExpirationAt(2).getText(), is(getVirtualCardLabel()));
    }

    @Test
    @MediumTest
    public void testSheetStartsInFullHeightForAccessibility() {
        // Enabling the accessibility settings.
        runOnUiThreadBlocking(
                () -> {
                    AccessibilityState.setIsTouchExplorationEnabledForTesting(true);
                });

        runOnUiThreadBlocking(
                () -> {
                    mTouchToFillPaymentMethodModel
                            .get(SHEET_ITEMS)
                            .add(
                                    new ListItem(
                                            CREDIT_CARD,
                                            createCardModel(VISA, mItemCollectionInfo)));
                    mTouchToFillPaymentMethodModel.set(VISIBLE, true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        // The sheet should be expanded to full height.
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);

        // Disabling the accessibility settings.
        runOnUiThreadBlocking(
                () -> {
                    AccessibilityState.setIsTouchExplorationEnabledForTesting(false);
                });
    }

    @Test
    @MediumTest
    public void testSheetStartsInHalfHeightForAccessibilityDisabled() {
        runOnUiThreadBlocking(
                () -> {
                    mTouchToFillPaymentMethodModel
                            .get(SHEET_ITEMS)
                            .add(
                                    new ListItem(
                                            CREDIT_CARD,
                                            createCardModel(VISA, mItemCollectionInfo)));
                    mTouchToFillPaymentMethodModel.set(VISIBLE, true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        // The sheet should be expanded to half height if possible, the half height state is
        // disabled on small screens.
        @BottomSheetController.SheetState
        int desiredState =
                mBottomSheetController.isSmallScreen()
                        ? BottomSheetController.SheetState.FULL
                        : BottomSheetController.SheetState.HALF;
        pollUiThread(() -> getBottomSheetState() == desiredState);
    }

    @Test
    @MediumTest
    public void testSheetScrollabilityDependsOnState() {
        runOnUiThreadBlocking(
                () -> {
                    mTouchToFillPaymentMethodModel
                            .get(SHEET_ITEMS)
                            .add(
                                    new ListItem(
                                            CREDIT_CARD,
                                            createCardModel(VISA, mItemCollectionInfo)));
                    mTouchToFillPaymentMethodModel.set(VISIBLE, true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        // The sheet should be expanded to the half height and scrolling suppressed, unless
        // the half height state is disabled due to the device having too small a screen.
        RecyclerView recyclerView = mTouchToFillPaymentMethodView.getSheetItemListView();
        assertEquals(!mBottomSheetController.isSmallScreen(), recyclerView.isLayoutSuppressed());

        // Expand the sheet to the full height and scrolling .
        runOnUiThreadBlocking(
                () ->
                        mSheetTestSupport.setSheetState(
                                BottomSheetController.SheetState.FULL, false));
        BottomSheetTestSupport.waitForState(
                mBottomSheetController, BottomSheetController.SheetState.FULL);

        assertFalse(recyclerView.isLayoutSuppressed());
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SECURITY_TOUCH_EVENT_FILTERING_ANDROID})
    public void testCreditCardViewProcessesClicksThroughObscuredSurfaces() {
        Runnable actionCallback = mock(Runnable.class);
        runOnUiThreadBlocking(
                () -> {
                    PropertyModel cardModel =
                            createCardModel(
                                    NICKNAMED_VISA,
                                    mItemCollectionInfo,
                                    actionCallback,
                                    /* isAcceptable= */ true);
                    mTouchToFillPaymentMethodModel
                            .get(SHEET_ITEMS)
                            .add(new ListItem(CREDIT_CARD, cardModel));
                    mTouchToFillPaymentMethodModel
                            .get(SHEET_ITEMS)
                            .add(new ListItem(FILL_BUTTON, cardModel));
                    mTouchToFillPaymentMethodModel.set(VISIBLE, true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        onView(withText(NICKNAMED_VISA.getCardNameForAutofillDisplay()))
                .perform(createClickActionWithFlags(MotionEvent.FLAG_WINDOW_IS_OBSCURED));
        waitForEvent(actionCallback).run();
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SECURITY_TOUCH_EVENT_FILTERING_ANDROID})
    public void testAcceptButtonProcessesClicksThroughObscuredSurfaces() {
        Runnable actionCallback = mock(Runnable.class);
        runOnUiThreadBlocking(
                () -> {
                    PropertyModel cardModel =
                            createCardModel(
                                    NICKNAMED_VISA,
                                    mItemCollectionInfo,
                                    actionCallback,
                                    /* isAcceptable= */ true);
                    mTouchToFillPaymentMethodModel
                            .get(SHEET_ITEMS)
                            .add(new ListItem(CREDIT_CARD, cardModel));
                    mTouchToFillPaymentMethodModel
                            .get(SHEET_ITEMS)
                            .add(new ListItem(FILL_BUTTON, cardModel));
                    mTouchToFillPaymentMethodModel.set(VISIBLE, true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        onView(
                        withText(
                                mActivityTestRule
                                        .getActivity()
                                        .getString(
                                                R.string.autofill_payment_method_continue_button)))
                .perform(createClickActionWithFlags(MotionEvent.FLAG_WINDOW_IS_OBSCURED));
        waitForEvent(actionCallback).run();
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SECURITY_TOUCH_EVENT_FILTERING_ANDROID})
    public void testCreditCardViewFiltersClicks() {
        runOnUiThreadBlocking(
                () -> {
                    PropertyModel cardModel =
                            createCardModel(
                                    NICKNAMED_VISA,
                                    mItemCollectionInfo,
                                    /* actionCallback= */ () -> fail(),
                                    /* isAcceptable= */ true);
                    mTouchToFillPaymentMethodModel
                            .get(SHEET_ITEMS)
                            .add(new ListItem(CREDIT_CARD, cardModel));
                    mTouchToFillPaymentMethodModel
                            .get(SHEET_ITEMS)
                            .add(new ListItem(FILL_BUTTON, cardModel));
                    mTouchToFillPaymentMethodModel.set(VISIBLE, true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        // Make sure touch events are ignored if something is drawn on top the the bottom sheet.
        onView(withText(NICKNAMED_VISA.getCardNameForAutofillDisplay()))
                .perform(createClickActionWithFlags(MotionEvent.FLAG_WINDOW_IS_OBSCURED));
        onView(withText(NICKNAMED_VISA.getCardNameForAutofillDisplay()))
                .perform(createClickActionWithFlags(MotionEvent.FLAG_WINDOW_IS_PARTIALLY_OBSCURED));
        onView(
                        withText(
                                mActivityTestRule
                                        .getActivity()
                                        .getString(R.string.autofill_payment_method_continue_button)))
                .perform(createClickActionWithFlags(MotionEvent.FLAG_WINDOW_IS_OBSCURED));
        onView(
                        withText(
                                mActivityTestRule
                                        .getActivity()
                                        .getString(R.string.autofill_payment_method_continue_button)))
                .perform(createClickActionWithFlags(MotionEvent.FLAG_WINDOW_IS_PARTIALLY_OBSCURED));
    }

    @Test
    @MediumTest
    public void testCardNameContentLabelForNicknamedCardContainsANetworkName() {
        runOnUiThreadBlocking(
                () -> {
                    mTouchToFillPaymentMethodModel
                            .get(SHEET_ITEMS)
                            .add(
                                    new ListItem(
                                            CREDIT_CARD,
                                            createCardModel(NICKNAMED_VISA, mItemCollectionInfo)));
                    mTouchToFillPaymentMethodModel.set(VISIBLE, true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        TextView cardName =
                mTouchToFillPaymentMethodView.getContentView().findViewById(R.id.card_name);
        assertTrue(cardName.getContentDescription().toString().equals("Best Card visa"));
    }

    @Test
    @MediumTest
    public void testCardNameContentDescriptionIsNotSetForCardWithNoNickname() {
        runOnUiThreadBlocking(
                () -> {
                    mTouchToFillPaymentMethodModel
                            .get(SHEET_ITEMS)
                            .add(
                                    new ListItem(
                                            CREDIT_CARD,
                                            createCardModel(VISA, mItemCollectionInfo)));
                    mTouchToFillPaymentMethodModel.set(VISIBLE, true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        TextView cardName =
                mTouchToFillPaymentMethodView.getContentView().findViewById(R.id.card_name);
        assertEquals(cardName.getContentDescription(), null);
    }

    @Test
    @MediumTest
    public void testDescriptionLineContentDescriptionOfCreditCard() {
        runOnUiThreadBlocking(
                () -> {
                    mTouchToFillPaymentMethodModel
                            .get(SHEET_ITEMS)
                            .add(
                                    new ListItem(
                                            CREDIT_CARD,
                                            createCardModel(
                                                    VISA, new FillableItemCollectionInfo(1, 1))));
                    mTouchToFillPaymentMethodModel.set(VISIBLE, true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        TextView descriptionLine =
                mTouchToFillPaymentMethodView.getContentView().findViewById(R.id.description_line_2);
        AccessibilityNodeInfo info = AccessibilityNodeInfo.obtain();
        descriptionLine.onInitializeAccessibilityNodeInfo(info);
        assertEquals(
                descriptionLine
                        .getContext()
                        .getString(
                                R.string.autofill_payment_method_a11y_item_collection_info,
                                descriptionLine.getText(),
                                1,
                                1),
                info.getContentDescription());
    }

    @Test
    @MediumTest
    public void testDescriptionLineContentDescriptionOfVirtualCard() {
        runOnUiThreadBlocking(
                () -> {
                    mTouchToFillPaymentMethodModel
                            .get(SHEET_ITEMS)
                            .add(
                                    new ListItem(
                                            CREDIT_CARD,
                                            createCardModel(
                                                    VIRTUAL_CARD,
                                                    new FillableItemCollectionInfo(1, 1))));
                    mTouchToFillPaymentMethodModel.set(VISIBLE, true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        TextView descriptionLine =
                mTouchToFillPaymentMethodView.getContentView().findViewById(R.id.description_line_2);
        AccessibilityNodeInfo info = AccessibilityNodeInfo.obtain();
        descriptionLine.onInitializeAccessibilityNodeInfo(info);
        assertEquals(
                descriptionLine
                        .getContext()
                        .getString(
                                R.string.autofill_payment_method_a11y_item_collection_info,
                                descriptionLine.getText(),
                                1,
                                1),
                info.getContentDescription());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/333128685")
    public void testCardNameTooLong_cardNameTruncated_lastFourDigitsAlwaysShown() {
        runOnUiThreadBlocking(
                () -> {
                    mTouchToFillPaymentMethodModel
                            .get(SHEET_ITEMS)
                            .add(
                                    new ListItem(
                                            CREDIT_CARD,
                                            createCardModel(
                                                    LONG_CARD_NAME_CARD, mItemCollectionInfo)));
                    mTouchToFillPaymentMethodModel.set(VISIBLE, true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        TextView cardName =
                mTouchToFillPaymentMethodView.getContentView().findViewById(R.id.card_name);
        TextView cardNumber =
                mTouchToFillPaymentMethodView.getContentView().findViewById(R.id.card_number);
        assertTrue(
                cardName.getLayout().getEllipsisCount(cardName.getLayout().getLineCount() - 1) > 0);
        assertThat(
                cardNumber.getLayout().getText().toString(),
                is(LONG_CARD_NAME_CARD.getObfuscatedLastFourDigits()));
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SECURITY_TOUCH_EVENT_FILTERING_ANDROID})
    public void testIbanViewProcessesTouchEvents() {
        Runnable actionCallback = mock(Runnable.class);
        runOnUiThreadBlocking(
                () -> {
                    PropertyModel ibanModel = createIbanModel(LOCAL_IBAN, actionCallback);
                    mTouchToFillPaymentMethodModel
                            .get(SHEET_ITEMS)
                            .add(new ListItem(IBAN, ibanModel));
                    mTouchToFillPaymentMethodModel
                            .get(SHEET_ITEMS)
                            .add(new ListItem(FILL_BUTTON, ibanModel));
                    mTouchToFillPaymentMethodModel.set(VISIBLE, true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        onView(withText(LOCAL_IBAN.getLabel()))
                .perform(createClickActionWithFlags(MotionEvent.FLAG_WINDOW_IS_OBSCURED));
        waitForEvent(actionCallback).run();
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SECURITY_TOUCH_EVENT_FILTERING_ANDROID})
    public void testIbanAcceptButtonProcessesTouchEvents() {
        Runnable actionCallback = mock(Runnable.class);
        runOnUiThreadBlocking(
                () -> {
                    PropertyModel ibanModel = createIbanModel(LOCAL_IBAN, actionCallback);
                    mTouchToFillPaymentMethodModel
                            .get(SHEET_ITEMS)
                            .add(new ListItem(IBAN, ibanModel));
                    mTouchToFillPaymentMethodModel
                            .get(SHEET_ITEMS)
                            .add(new ListItem(FILL_BUTTON, ibanModel));
                    mTouchToFillPaymentMethodModel.set(VISIBLE, true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        onView(
                        withText(
                                mActivityTestRule
                                        .getActivity()
                                        .getString(
                                                R.string.autofill_payment_method_continue_button)))
                .perform(createClickActionWithFlags(MotionEvent.FLAG_WINDOW_IS_OBSCURED));
        waitForEvent(actionCallback).run();
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SECURITY_TOUCH_EVENT_FILTERING_ANDROID})
    public void testIbanViewFiltersTouchEvents() {
        runOnUiThreadBlocking(
                () -> {
                    PropertyModel ibanModel = createIbanModel(LOCAL_IBAN, () -> fail());
                    mTouchToFillPaymentMethodModel
                            .get(SHEET_ITEMS)
                            .add(new ListItem(IBAN, ibanModel));
                    mTouchToFillPaymentMethodModel
                            .get(SHEET_ITEMS)
                            .add(new ListItem(FILL_BUTTON, ibanModel));
                    mTouchToFillPaymentMethodModel.set(VISIBLE, true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        // Make sure touch events are ignored if something is drawn on top the the bottom sheet.
        onView(withText(LOCAL_IBAN.getLabel()))
                .perform(createClickActionWithFlags(MotionEvent.FLAG_WINDOW_IS_OBSCURED));
        onView(withText(LOCAL_IBAN.getLabel()))
                .perform(createClickActionWithFlags(MotionEvent.FLAG_WINDOW_IS_PARTIALLY_OBSCURED));
        onView(
                        withText(
                                mActivityTestRule
                                        .getActivity()
                                        .getString(R.string.autofill_payment_method_continue_button)))
                .perform(createClickActionWithFlags(MotionEvent.FLAG_WINDOW_IS_OBSCURED));
        onView(
                        withText(
                                mActivityTestRule
                                        .getActivity()
                                        .getString(R.string.autofill_payment_method_continue_button)))
                .perform(createClickActionWithFlags(MotionEvent.FLAG_WINDOW_IS_PARTIALLY_OBSCURED));
    }

    @Test
    @MediumTest
    public void testIbanValueAndNicknameForIban() {
        runOnUiThreadBlocking(
                () -> {
                    mTouchToFillPaymentMethodModel
                            .get(SHEET_ITEMS)
                            .add(new ListItem(IBAN, createIbanModel(LOCAL_IBAN)));
                    mTouchToFillPaymentMethodModel.set(VISIBLE, true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        TextView ibanValue =
                mTouchToFillPaymentMethodView.getContentView().findViewById(R.id.iban_value);
        assertThat(ibanValue.getLayout().getText().toString(), is(LOCAL_IBAN.getLabel()));
        TextView ibanNickname =
                mTouchToFillPaymentMethodView.getContentView().findViewById(R.id.iban_nickname);
        assertThat(ibanNickname.getLayout().getText().toString(), is(LOCAL_IBAN.getNickname()));
    }

    @Test
    @MediumTest
    public void testNonAcceptableVirtualCardSuggestion() {
        runOnUiThreadBlocking(
                () -> {
                    mTouchToFillPaymentMethodModel
                            .get(SHEET_ITEMS)
                            .add(
                                    new ListItem(
                                            CREDIT_CARD,
                                            createCardModel(
                                                    VIRTUAL_CARD,
                                                    new FillableItemCollectionInfo(1, 1),
                                                    () -> {},
                                                    /* isAcceptable= */ false)));
                    mTouchToFillPaymentMethodModel.set(VISIBLE, true);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        ImageView icon = mTouchToFillPaymentMethodView.getContentView().findViewById(R.id.favicon);
        assertThat(icon.getAlpha(), is(0.38f));
        assertThat(getCreditCards().getChildAt(0).isEnabled(), is(false));
    }

    private RecyclerView getCreditCards() {
        return mTouchToFillPaymentMethodView.getContentView().findViewById(R.id.sheet_item_list);
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

    private static PropertyModel createCardModel(
            CreditCard card, FillableItemCollectionInfo collectionInfo) {
        return createCardModel(card, collectionInfo, () -> {}, /* isAcceptable= */ true);
    }

    private static PropertyModel createCardModel(
            CreditCard card,
            FillableItemCollectionInfo collectionInfo,
            Runnable actionCallback,
            boolean isAcceptable) {
        PropertyModel.Builder creditCardModelBuilder =
                new PropertyModel.Builder(
                                TouchToFillPaymentMethodProperties.CreditCardProperties
                                        .NON_TRANSFORMING_CREDIT_CARD_KEYS)
                        .with(
                                TouchToFillPaymentMethodProperties.CreditCardProperties.CARD_NAME,
                                card.getCardNameForAutofillDisplay())
                        .with(
                                TouchToFillPaymentMethodProperties.CreditCardProperties.CARD_NUMBER,
                                card.getObfuscatedLastFourDigits())
                        .with(
                                TouchToFillPaymentMethodProperties.CreditCardProperties
                                        .ITEM_COLLECTION_INFO,
                                collectionInfo)
                        .with(
                                TouchToFillPaymentMethodProperties.CreditCardProperties
                                        .ON_CREDIT_CARD_CLICK_ACTION,
                                actionCallback)
                        .with(
                                TouchToFillPaymentMethodProperties.CreditCardProperties
                                        .IS_ACCEPTABLE,
                                isAcceptable);
        if (!card.getBasicCardIssuerNetwork()
                .equals(card.getCardNameForAutofillDisplay().toLowerCase())) {
            creditCardModelBuilder.with(
                    TouchToFillPaymentMethodProperties.CreditCardProperties.NETWORK_NAME,
                    card.getBasicCardIssuerNetwork());
        }
        if (card.getIsVirtual()) {
            creditCardModelBuilder.with(
                    TouchToFillPaymentMethodProperties.CreditCardProperties.VIRTUAL_CARD_LABEL,
                    getVirtualCardLabel());
        } else {
            creditCardModelBuilder.with(
                    TouchToFillPaymentMethodProperties.CreditCardProperties.CARD_EXPIRATION,
                    card.getFormattedExpirationDate(ContextUtils.getApplicationContext()));
        }
        return creditCardModelBuilder.build();
    }

    private static PropertyModel createIbanModel(Iban iban) {
        return createIbanModel(iban, () -> {});
    }

    private static PropertyModel createIbanModel(Iban iban, Runnable actionCallback) {
        PropertyModel.Builder ibanModelBuilder =
                new PropertyModel.Builder(
                                TouchToFillPaymentMethodProperties.IbanProperties
                                        .NON_TRANSFORMING_IBAN_KEYS)
                        .with(
                                TouchToFillPaymentMethodProperties.IbanProperties.IBAN_VALUE,
                                iban.getLabel())
                        .with(
                                TouchToFillPaymentMethodProperties.IbanProperties.IBAN_NICKNAME,
                                iban.getNickname())
                        .with(
                                TouchToFillPaymentMethodProperties.IbanProperties.ON_IBAN_CLICK_ACTION,
                                actionCallback);
        return ibanModelBuilder.build();
    }

    private static String getVirtualCardLabel() {
        return ContextUtils.getApplicationContext()
                .getString(R.string.autofill_virtual_card_number_switch_label);
    }

    private static <T> T waitForEvent(T mock) {
        return verify(
                mock,
                timeout(ScalableTimeout.scaleTimeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL)));
    }
}
