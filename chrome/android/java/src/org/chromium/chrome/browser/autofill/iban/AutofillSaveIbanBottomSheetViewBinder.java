// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.iban;

import android.text.SpannableStringBuilder;
import android.text.method.LinkMovementMethod;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.DrawableRes;

import org.chromium.chrome.browser.autofill.AutofillUiUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This class is responsible for binding and translating the IBAN model defined in
 * AutofillSaveIbanBottomSheetProperties to an AutofillSaveIbanBottomSheetView View.
 */
/*package*/ class AutofillSaveIbanBottomSheetViewBinder {
    static void bind(
            PropertyModel model, AutofillSaveIbanBottomSheetView view, PropertyKey propertyKey) {
        if (AutofillSaveIbanBottomSheetProperties.LOGO_ICON == propertyKey) {
            @DrawableRes int iconID = model.get(AutofillSaveIbanBottomSheetProperties.LOGO_ICON);
            // IconID is 0 when local save is being offered. Only server save has the GPay logo.
            if (iconID == 0) {
                view.mLogoIcon.setVisibility(View.GONE);
                return;
            }
            view.mLogoIcon.setImageResource(iconID);
            view.mLogoIcon.setVisibility(View.VISIBLE);
        } else if (AutofillSaveIbanBottomSheetProperties.TITLE == propertyKey) {
            view.mTitle.setText(model.get(AutofillSaveIbanBottomSheetProperties.TITLE));
        } else if (AutofillSaveIbanBottomSheetProperties.DESCRIPTION == propertyKey) {
            setMaybeEmptyText(
                    view.mDescription,
                    model.get(AutofillSaveIbanBottomSheetProperties.DESCRIPTION));
        } else if (AutofillSaveIbanBottomSheetProperties.IBAN_VALUE == propertyKey) {
            view.mIbanValue.setText(model.get(AutofillSaveIbanBottomSheetProperties.IBAN_VALUE));
        } else if (AutofillSaveIbanBottomSheetProperties.ACCEPT_BUTTON_LABEL == propertyKey) {
            view.mAcceptButton.setText(
                    model.get(AutofillSaveIbanBottomSheetProperties.ACCEPT_BUTTON_LABEL));
        } else if (AutofillSaveIbanBottomSheetProperties.CANCEL_BUTTON_LABEL == propertyKey) {
            view.mCancelButton.setText(
                    model.get(AutofillSaveIbanBottomSheetProperties.CANCEL_BUTTON_LABEL));
        } else if (AutofillSaveIbanBottomSheetProperties.ON_ACCEPT_BUTTON_CLICK_ACTION
                == propertyKey) {
            view.mAcceptButton.setOnClickListener(
                    model.get(AutofillSaveIbanBottomSheetProperties.ON_ACCEPT_BUTTON_CLICK_ACTION));
        } else if (AutofillSaveIbanBottomSheetProperties.ON_CANCEL_BUTTON_CLICK_ACTION
                == propertyKey) {
            view.mCancelButton.setOnClickListener(
                    model.get(AutofillSaveIbanBottomSheetProperties.ON_CANCEL_BUTTON_CLICK_ACTION));
        } else if (AutofillSaveIbanBottomSheetProperties.LEGAL_MESSAGE == propertyKey) {
            AutofillSaveIbanBottomSheetProperties.LegalMessage legalMessage =
                    model.get(AutofillSaveIbanBottomSheetProperties.LEGAL_MESSAGE);
            if (legalMessage.mLines.isEmpty()) {
                view.mLegalMessage.setVisibility(View.GONE);
                return;
            }

            SpannableStringBuilder stringBuilder =
                    AutofillUiUtils.getSpannableStringForLegalMessageLines(
                            view.mContentView.getContext(),
                            legalMessage.mLines,
                            /* underlineLinks= */ true,
                            legalMessage.mLink::accept);
            view.mLegalMessage.setText(stringBuilder);
            view.mLegalMessage.setVisibility(View.VISIBLE);
            view.mLegalMessage.setMovementMethod(LinkMovementMethod.getInstance());
        }
    }

    private static void setMaybeEmptyText(TextView textView, String text) {
        if (text.isEmpty()) {
            textView.setVisibility(View.GONE);
            return;
        }
        textView.setText(text);
        textView.setVisibility(View.VISIBLE);
    }
}
