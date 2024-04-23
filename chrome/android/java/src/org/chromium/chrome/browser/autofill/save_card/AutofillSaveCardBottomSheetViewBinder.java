// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.save_card;

import android.text.method.LinkMovementMethod;
import android.view.View;

import org.chromium.chrome.browser.autofill.AutofillUiUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/*package*/ class AutofillSaveCardBottomSheetViewBinder {
    static void bind(
            PropertyModel model, AutofillSaveCardBottomSheetView view, PropertyKey propertyKey) {
        if (AutofillSaveCardBottomSheetProperties.TITLE == propertyKey) {
            view.mTitle.setText(model.get(AutofillSaveCardBottomSheetProperties.TITLE));
        } else if (AutofillSaveCardBottomSheetProperties.DESCRIPTION == propertyKey) {
            view.mDescription.setText(model.get(AutofillSaveCardBottomSheetProperties.DESCRIPTION));
        } else if (AutofillSaveCardBottomSheetProperties.LOGO_ICON == propertyKey) {
            Integer iconID = model.get(AutofillSaveCardBottomSheetProperties.LOGO_ICON);
            if (iconID == 0) {
                view.mLogoIcon.setVisibility(View.GONE);
            } else {
                view.mLogoIcon.setImageResource(iconID);
                view.mLogoIcon.setVisibility(View.VISIBLE);
            }
        } else if (AutofillSaveCardBottomSheetProperties.CARD_DESCRIPTION == propertyKey) {
            view.mCardView.setContentDescription(
                    model.get(AutofillSaveCardBottomSheetProperties.CARD_DESCRIPTION));
        } else if (AutofillSaveCardBottomSheetProperties.CARD_ICON == propertyKey) {
            view.mCardIcon.setImageResource(
                    model.get(AutofillSaveCardBottomSheetProperties.CARD_ICON));
        } else if (AutofillSaveCardBottomSheetProperties.CARD_LABEL == propertyKey) {
            view.mCardLabel.setText(model.get(AutofillSaveCardBottomSheetProperties.CARD_LABEL));
        } else if (AutofillSaveCardBottomSheetProperties.CARD_SUB_LABEL == propertyKey) {
            view.mCardSubLabel.setText(
                    model.get(AutofillSaveCardBottomSheetProperties.CARD_SUB_LABEL));
        } else if (AutofillSaveCardBottomSheetProperties.LEGAL_MESSAGE == propertyKey) {
            AutofillSaveCardBottomSheetProperties.LegalMessage legalMessage =
                    model.get(AutofillSaveCardBottomSheetProperties.LEGAL_MESSAGE);
            if (legalMessage == null
                    || legalMessage.mLines == null
                    || legalMessage.mLines.isEmpty()) {
                view.mLegalMessage.setVisibility(View.GONE);
                return;
            }
            view.mLegalMessage.setText(
                    AutofillUiUtils.getSpannableStringForLegalMessageLines(
                            view.mContentView.getContext(),
                            legalMessage.mLines,
                            /* underlineLinks= */ true,
                            legalMessage.mLink::accept));
            view.mLegalMessage.setMovementMethod(LinkMovementMethod.getInstance());
            view.mLegalMessage.setVisibility(View.VISIBLE);
        } else if (AutofillSaveCardBottomSheetProperties.ACCEPT_BUTTON_LABEL == propertyKey) {
            view.mAcceptButton.setText(
                    model.get(AutofillSaveCardBottomSheetProperties.ACCEPT_BUTTON_LABEL));
        } else if (AutofillSaveCardBottomSheetProperties.CANCEL_BUTTON_LABEL == propertyKey) {
            view.mCancelButton.setText(
                    model.get(AutofillSaveCardBottomSheetProperties.CANCEL_BUTTON_LABEL));
        }
    }
}
