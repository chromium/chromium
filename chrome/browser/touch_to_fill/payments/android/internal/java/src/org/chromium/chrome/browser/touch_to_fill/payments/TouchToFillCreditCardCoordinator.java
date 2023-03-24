// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.ItemType.CREDIT_CARD;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.ItemType.FILL_BUTTON;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.ItemType.FOOTER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.ItemType.HEADER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.VISIBLE;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.touch_to_fill.common.BottomSheetFocusHelper;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/**
 * Implements the TouchToFillCreditCardComponent. It uses a bottom sheet to let the user select a
 * credit card to be filled into the focused form.
 */
public class TouchToFillCreditCardCoordinator implements TouchToFillCreditCardComponent {
    private final TouchToFillCreditCardMediator mMediator = new TouchToFillCreditCardMediator();
    private PropertyModel mTouchToFillCreditCardModel;

    @Override
    public void initialize(Context context, BottomSheetController sheetController,
            TouchToFillCreditCardComponent.Delegate delegate,
            BottomSheetFocusHelper bottomSheetFocusHelper) {
        mTouchToFillCreditCardModel = createModel(mMediator);
        mMediator.initialize(
                context, delegate, mTouchToFillCreditCardModel, bottomSheetFocusHelper);
        setUpModelChangeProcessors(mTouchToFillCreditCardModel,
                new TouchToFillCreditCardView(context, sheetController));
    }

    @Override
    public void showSheet(CreditCard[] cards, boolean shouldShowScanCreditCard) {
        mMediator.showSheet(cards, shouldShowScanCreditCard);
    }

    @Override
    public void hideSheet() {
        mMediator.hideSheet();
    }

    /**
     * Connects the given model with the given view using Model Change Processors.
     * @param model A {@link PropertyModel} built with {@link TouchToFillCreditCardProperties}.
     * @param view A {@link TouchToFillCreditCardView}.
     */
    @VisibleForTesting
    static void setUpModelChangeProcessors(PropertyModel model, TouchToFillCreditCardView view) {
        PropertyModelChangeProcessor.create(
                model, view, TouchToFillCreditCardViewBinder::bindTouchToFillCreditCardView);
    }

    static void setUpCardItems(PropertyModel model, TouchToFillCreditCardView view) {
        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(model.get(SHEET_ITEMS));
        adapter.registerType(CREDIT_CARD, TouchToFillCreditCardViewBinder::createCardItemView,
                TouchToFillCreditCardViewBinder::bindCardItemView);
        adapter.registerType(HEADER, TouchToFillCreditCardViewBinder::createHeaderItemView,
                TouchToFillCreditCardViewBinder::bindHeaderView);
        adapter.registerType(FILL_BUTTON, TouchToFillCreditCardViewBinder::createFillButtonView,
                TouchToFillCreditCardViewBinder::bindFillButtonView);
        adapter.registerType(FOOTER, TouchToFillCreditCardViewBinder::createFooterItemView,
                TouchToFillCreditCardViewBinder::bindFooterView);
        view.setSheetItemListAdapter(adapter);
    }

    PropertyModel createModel(TouchToFillCreditCardMediator mediator) {
        return new PropertyModel.Builder(TouchToFillCreditCardProperties.ALL_KEYS)
                .with(VISIBLE, false)
                .with(SHEET_ITEMS, new ModelList())
                .with(DISMISS_HANDLER, mediator::onDismissed)
                .build();
    }

    @VisibleForTesting
    PropertyModel getModelForTesting() {
        return mTouchToFillCreditCardModel;
    }
}
