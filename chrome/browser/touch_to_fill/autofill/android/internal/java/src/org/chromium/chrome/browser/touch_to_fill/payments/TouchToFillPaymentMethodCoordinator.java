// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import static org.chromium.chrome.browser.autofill.AutofillUiUtils.getCardIcon;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.CREDIT_CARD;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.FILL_BUTTON;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.FOOTER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.HEADER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.IBAN;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType.TERMS_LABEL;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.VISIBLE;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill.PersonalDataManager.Iban;
import org.chromium.chrome.browser.touch_to_fill.common.BottomSheetFocusHelper;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.ImageSize;
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

    @Override
    public void initialize(
            Context context,
            PersonalDataManager personalDataManager,
            BottomSheetController sheetController,
            Delegate delegate,
            BottomSheetFocusHelper bottomSheetFocusHelper) {
                mTouchToFillPaymentMethodModel = createModel(mMediator);
        mCardImageFunction =
                (metaData) ->
                        getCardIcon(
                                context,
                                personalDataManager,
                                metaData.artUrl,
                                metaData.iconId,
                                ImageSize.LARGE,
                                /* showCustomIcon= */ true);
        mMediator.initialize(delegate, mTouchToFillPaymentMethodModel, bottomSheetFocusHelper);
        setUpModelChangeProcessors(
                mTouchToFillPaymentMethodModel,
                new TouchToFillPaymentMethodView(context, sheetController));
    }

    @Override
    public void showSheet(
            List<CreditCard> cards,
            List<AutofillSuggestion> suggestions,
            boolean shouldShowScanCreditCard) {
        assert mCardImageFunction != null : "Attempting to call showSheet before initialize.";
        mMediator.showSheet(cards, suggestions, shouldShowScanCreditCard, mCardImageFunction);
    }

    @Override
    public void showSheet(List<Iban> ibans) {
        mMediator.showSheet(ibans);
    }

    @Override
    public void hideSheet() {
        mMediator.hideSheet();
    }

    /**
     * Connects the given model with the given view using Model Change Processors.
     * @param model A {@link PropertyModel} built with {@link TouchToFillPaymentMethodProperties}.
     * @param view A {@link TouchToFillPaymentMethodView}.
     */
    @VisibleForTesting
    static void setUpModelChangeProcessors(PropertyModel model, TouchToFillPaymentMethodView view) {
        PropertyModelChangeProcessor.create(
                model, view, TouchToFillPaymentMethodViewBinder::bindTouchToFillPaymentMethodView);
    }

    static void setUpCardItems(PropertyModel model, TouchToFillPaymentMethodView view) {
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
                HEADER,
                TouchToFillPaymentMethodViewBinder::createHeaderItemView,
                TouchToFillPaymentMethodViewBinder::bindHeaderView);
        adapter.registerType(
                FILL_BUTTON,
                TouchToFillPaymentMethodViewBinder::createFillButtonView,
                TouchToFillPaymentMethodViewBinder::bindFillButtonView);
        adapter.registerType(
                FOOTER,
                TouchToFillPaymentMethodViewBinder::createFooterItemView,
                TouchToFillPaymentMethodViewBinder::bindFooterView);
        adapter.registerType(
                TERMS_LABEL,
                TouchToFillPaymentMethodViewBinder::createTermsLabelView,
                TouchToFillPaymentMethodViewBinder::bindTermsLabelView);
        view.setSheetItemListAdapter(adapter);
    }

    PropertyModel createModel(TouchToFillPaymentMethodMediator mediator) {
        return new PropertyModel.Builder(TouchToFillPaymentMethodProperties.ALL_KEYS)
                .with(VISIBLE, false)
                .with(SHEET_ITEMS, new ModelList())
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
