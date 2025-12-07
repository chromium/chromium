// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import static org.chromium.chrome.browser.autofill.AutofillUiUtils.getCardIcon;
import static org.chromium.chrome.browser.autofill.AutofillUiUtils.getValuableIcon;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BACK_PRESS_HANDLER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CURRENT_SCREEN;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.FOCUSED_VIEW_ID_FOR_ACCESSIBILITY;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.ALL_LOYALTY_CARDS;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.BNPL;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.BNPL_ISSUER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.BNPL_SELECTION_PROGRESS_HEADER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.BNPL_SELECTION_PROGRESS_TERMS;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.BNPL_TOS_TEXT;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.CREDIT_CARD;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.ERROR_DESCRIPTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.FILL_BUTTON;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.FOOTER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.HEADER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.IBAN;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.LOYALTY_CARD;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.PROGRESS_ICON;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.TERMS_LABEL;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.TEXT_BUTTON;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.TOS_FOOTER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.WALLET_SETTINGS_BUTTON;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ScreenId.HOME_SCREEN;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.VISIBLE;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.autofill.AutofillImageFetcher;
import org.chromium.chrome.browser.autofill.PersonalDataManager.Iban;
import org.chromium.chrome.browser.touch_to_fill.common.BottomSheetFocusHelper;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.ImageSize;
import org.chromium.components.autofill.LoyaltyCard;
import org.chromium.components.autofill.payments.BnplIssuerContext;
import org.chromium.components.autofill.payments.BnplIssuerTosDetail;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.List;
import java.util.function.Function;

/**
 * Implements the TouchToFillPaymentMethodComponent. It uses a bottom sheet to let the user select a
 * credit card to be filled into the focused form.
 */
public class TouchToFillPaymentMethodCoordinator implements TouchToFillPaymentMethodComponent {
    private final TouchToFillPaymentMethodMediator mMediator = new TouchToFillPaymentMethodMediator();
    private PropertyModel mTouchToFillPaymentMethodModel;
    private Function<TouchToFillPaymentMethodProperties.CardImageMetaData, Drawable>
            mCardImageFunction;
    private Function<LoyaltyCard, Drawable> mValuableImageFunction;

    @Override
    public void initialize(
            Context context,
            AutofillImageFetcher imageFetcher,
            BottomSheetController sheetController,
            Delegate delegate,
            BottomSheetFocusHelper bottomSheetFocusHelper) {
        mTouchToFillPaymentMethodModel = createModel(mMediator);
        mCardImageFunction =
                (metaData) ->
                        getCardIcon(
                                context,
                                imageFetcher,
                                metaData.artUrl,
                                metaData.iconId,
                                ImageSize.LARGE,
                                /* showCustomIcon= */ true);
        mValuableImageFunction =
                (loyaltyCard) ->
                        getValuableIcon(
                                context,
                                imageFetcher,
                                loyaltyCard.getProgramLogo(),
                                ImageSize.LARGE,
                                loyaltyCard.getMerchantName());
        mMediator.initialize(
                context, delegate, mTouchToFillPaymentMethodModel, bottomSheetFocusHelper);
        setUpModelChangeProcessors(
                mTouchToFillPaymentMethodModel,
                new TouchToFillPaymentMethodView(context, sheetController));
    }

    @Override
    public void showPaymentMethods(
            List<AutofillSuggestion> suggestions, boolean shouldShowScanCreditCard) {
        assert mCardImageFunction != null : "Attempting to call showCreditCards before initialize.";
        mMediator.showPaymentMethods(suggestions, shouldShowScanCreditCard, mCardImageFunction);
    }

    @Override
    public void showIbans(List<Iban> ibans) {
        mMediator.showIbans(ibans);
    }

    @Override
    public void showLoyaltyCards(
            List<LoyaltyCard> affiliatedLoyaltyCards,
            List<LoyaltyCard> allLoyaltyCards,
            boolean firstTimeUsage) {
        mMediator.showLoyaltyCards(
                affiliatedLoyaltyCards, allLoyaltyCards, mValuableImageFunction, firstTimeUsage);
    }

    @Override
    public void onPurchaseAmountExtracted(
            List<BnplIssuerContext> bnplIssuerContexts,
            Long extractedAmount,
            boolean isAmountSupportedByAnyIssuer) {
        mMediator.onPurchaseAmountExtracted(
                bnplIssuerContexts, extractedAmount, isAmountSupportedByAnyIssuer);
    }

    @Override
    public void showProgressScreen() {
        mMediator.showProgressScreen();
    }

    @Override
    public void showBnplIssuers(List<BnplIssuerContext> bnplIssuerContexts) {
        mMediator.showBnplIssuers(bnplIssuerContexts);
    }

    @Override
    public void showErrorScreen(String title, String description) {
        mMediator.showErrorScreen(title, description);
    }

    @Override
    public void showBnplIssuerTos(BnplIssuerTosDetail bnplIssuerTosDetail) {
        mMediator.showBnplIssuerTos(bnplIssuerTosDetail);
    }

    @Override
    public void hideSheet() {
        mMediator.hideSheet();
    }

    @Override
    public void setVisible(boolean visible) {
        mMediator.setVisible(visible);
    }

    /**
     * Connects the given model with the given view using Model Change Processors.
     *
     * @param model A {@link PropertyModel} built with {@link TouchToFillPaymentMethodProperties}.
     * @param view A {@link TouchToFillPaymentMethodView}.
     */
    @VisibleForTesting
    static void setUpModelChangeProcessors(PropertyModel model, TouchToFillPaymentMethodView view) {
        PropertyModelChangeProcessor.create(
                model, view, TouchToFillPaymentMethodViewBinder::bindTouchToFillPaymentMethodView);
    }

    static void setUpSheetItems(PropertyModel model, TouchToFillPaymentMethodView view) {
        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(model.get(SHEET_ITEMS));
        adapter.registerType(
                CREDIT_CARD,
                TouchToFillPaymentMethodViewBinder::createCardItemView,
                TouchToFillPaymentMethodViewBinder::bindCardItemView);
        adapter.registerType(
                IBAN,
                TouchToFillPaymentMethodViewBinder::createIbanItemView,
                TouchToFillPaymentMethodViewBinder::bindIbanItemView);
        adapter.registerType(
                LOYALTY_CARD,
                TouchToFillPaymentMethodViewBinder::createLoyaltyCardItemView,
                TouchToFillPaymentMethodViewBinder::bindLoyaltyCardItemView);
        adapter.registerType(
                ALL_LOYALTY_CARDS,
                TouchToFillPaymentMethodViewBinder::createAllLoyaltyCardsItemView,
                TouchToFillPaymentMethodViewBinder::bindAllLoyaltyCardsItemView);
        adapter.registerType(
                HEADER,
                TouchToFillPaymentMethodViewBinder::createHeaderItemView,
                TouchToFillPaymentMethodViewBinder::bindHeaderView);
        adapter.registerType(
                FILL_BUTTON,
                TouchToFillPaymentMethodViewBinder::createFillButtonView,
                TouchToFillPaymentMethodViewBinder::bindButtonView);
        adapter.registerType(
                WALLET_SETTINGS_BUTTON,
                TouchToFillPaymentMethodViewBinder::createWalletSettingsButtonView,
                TouchToFillPaymentMethodViewBinder::bindButtonView);
        adapter.registerType(
                FOOTER,
                TouchToFillPaymentMethodViewBinder::createFooterItemView,
                TouchToFillPaymentMethodViewBinder::bindFooterView);
        adapter.registerType(
                TERMS_LABEL,
                TouchToFillPaymentMethodViewBinder::createTermsLabelView,
                TouchToFillPaymentMethodViewBinder::bindTermsLabelView);
        adapter.registerType(
                BNPL,
                TouchToFillPaymentMethodViewBinder::createBnplItemView,
                TouchToFillPaymentMethodViewBinder::bindBnplItemView);
        adapter.registerType(
                PROGRESS_ICON,
                TouchToFillPaymentMethodViewBinder::createProgressIconView,
                TouchToFillPaymentMethodViewBinder::bindProgressIconView);
        adapter.registerType(
                BNPL_SELECTION_PROGRESS_HEADER,
                TouchToFillPaymentMethodViewBinder::createBnplSelectionProgressHeaderItemView,
                TouchToFillPaymentMethodViewBinder::bindBnplSelectionProgressHeaderView);
        adapter.registerType(
                BNPL_ISSUER,
                TouchToFillPaymentMethodViewBinder::createBnplIssuerItemView,
                TouchToFillPaymentMethodViewBinder::bindBnplIssuerItemView);
        adapter.registerType(
                ERROR_DESCRIPTION,
                TouchToFillPaymentMethodViewBinder::createErrorDescriptionView,
                TouchToFillPaymentMethodViewBinder::bindErrorDescriptionView);
        adapter.registerType(
                BNPL_TOS_TEXT,
                TouchToFillPaymentMethodViewBinder::createBnplIssuerTosItemView,
                TouchToFillPaymentMethodViewBinder::bindBnplIssuerTosItemView);
        adapter.registerType(
                BNPL_SELECTION_PROGRESS_TERMS,
                TouchToFillPaymentMethodViewBinder::createBnplSelectionProgressTermsItemView,
                TouchToFillPaymentMethodViewBinder::bindBnplSelectionProgressTermsView);
        adapter.registerType(
                TOS_FOOTER,
                TouchToFillPaymentMethodViewBinder::createLegalMessageItemView,
                TouchToFillPaymentMethodViewBinder::bindLegalMessageItemView);
        adapter.registerType(
                TEXT_BUTTON,
                TouchToFillPaymentMethodViewBinder::createTextButtonView,
                TouchToFillPaymentMethodViewBinder::bindButtonView);
        view.setSheetItemListAdapter(adapter);
    }

    PropertyModel createModel(TouchToFillPaymentMethodMediator mediator) {
        return new PropertyModel.Builder(TouchToFillPaymentMethodProperties.ALL_KEYS)
                .with(VISIBLE, false)
                .with(CURRENT_SCREEN, HOME_SCREEN)
                .with(FOCUSED_VIEW_ID_FOR_ACCESSIBILITY, 0)
                .with(SHEET_ITEMS, new ModelList())
                .with(BACK_PRESS_HANDLER, mediator::onBackButtonPressed)
                .with(DISMISS_HANDLER, mediator::onDismissed)
                .build();
    }

    PropertyModel getModelForTesting() {
        return mTouchToFillPaymentMethodModel;
    }

    TouchToFillPaymentMethodMediator getMediatorForTesting() {
        return mMediator;
    }
}
