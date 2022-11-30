// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Implements the TouchToFillCreditCardComponent. It uses a bottom sheet to let the user select a
 * credit card to be filled into the focused form.
 */
public class TouchToFillCreditCardCoordinator implements TouchToFillCreditCardComponent {
    private final TouchToFillCreditCardMediator mMediator = new TouchToFillCreditCardMediator();

    @Override
    public void initialize(Context context, BottomSheetController sheetController,
            TouchToFillCreditCardComponent.Delegate delegate) {
        PropertyModel model = new PropertyModel.Builder(TouchToFillCreditCardProperties.ALL_KEYS)
                                      .with(TouchToFillCreditCardProperties.VISIBLE, false)
                                      .with(TouchToFillCreditCardProperties.DISMISS_HANDLER,
                                              mMediator::onDismissed)
                                      .build();

        mMediator.initialize(delegate, model);
        setUpModelChangeProcessors(model, new TouchToFillCreditCardView(context, sheetController));
    }

    @Override
    public void showSheet() {
        mMediator.showSheet();
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
}
