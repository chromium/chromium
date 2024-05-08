// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.save_card;

import android.text.SpannableStringBuilder;
import android.text.method.LinkMovementMethod;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.DrawableRes;

import com.google.common.base.Strings;

import org.chromium.chrome.browser.autofill.AutofillUiUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/*package*/ class AutofillSaveCardBottomSheetViewBinder {
    static void bind(
            PropertyModel model, AutofillSaveCardBottomSheetView view, PropertyKey propertyKey) {
        if (AutofillSaveCardBottomSheetProperties.TITLE == propertyKey) {
            setTextViewText(view.mTitle, model.get(AutofillSaveCardBottomSheetProperties.TITLE));
        } else if (AutofillSaveCardBottomSheetProperties.DESCRIPTION == propertyKey) {
            setTextViewText(
                    view.mDescription,
                    model.get(AutofillSaveCardBottomSheetProperties.DESCRIPTION));
        } else if (AutofillSaveCardBottomSheetProperties.LOGO_ICON == propertyKey) {
            @DrawableRes int iconID = model.get(AutofillSaveCardBottomSheetProperties.LOGO_ICON);
            if (iconID == 0) {
                view.mLogoIcon.setVisibility(View.GONE);
                return;
            }
            view.mLogoIcon.setImageResource(iconID);
            view.mLogoIcon.setVisibility(View.VISIBLE);
        } else if (AutofillSaveCardBottomSheetProperties.CARD_DESCRIPTION == propertyKey) {
            view.mCardView.setContentDescription(
                    model.get(AutofillSaveCardBottomSheetProperties.CARD_DESCRIPTION));
        } else if (AutofillSaveCardBottomSheetProperties.CARD_ICON == propertyKey) {
            view.mCardIcon.setImageResource(
                    model.get(AutofillSaveCardBottomSheetProperties.CARD_ICON));
        } else if (AutofillSaveCardBottomSheetProperties.CARD_LABEL == propertyKey) {
            setTextViewText(
                    view.mCardLabel, model.get(AutofillSaveCardBottomSheetProperties.CARD_LABEL));
        } else if (AutofillSaveCardBottomSheetProperties.CARD_SUB_LABEL == propertyKey) {
            setTextViewText(
                    view.mCardSubLabel,
                    model.get(AutofillSaveCardBottomSheetProperties.CARD_SUB_LABEL));
        } else if (AutofillSaveCardBottomSheetProperties.LEGAL_MESSAGE == propertyKey) {
            AutofillSaveCardBottomSheetProperties.LegalMessage legalMessage =
                    model.get(AutofillSaveCardBottomSheetProperties.LEGAL_MESSAGE);
            if (legalMessage == null || legalMessage.mLink == null) {
                view.mLegalMessage.setVisibility(View.GONE);
                return;
            }

            SpannableStringBuilder stringBuilder =
                    AutofillUiUtils.getSpannableStringForLegalMessageLines(
                            view.mContentView.getContext(),
                            legalMessage.mLines,
                            /* underlineLinks= */ true,
                            legalMessage.mLink::accept);
            if (Strings.isNullOrEmpty(stringBuilder.toString())) {
                view.mLegalMessage.setVisibility(View.GONE);
                return;
            }

            view.mLegalMessage.setText(stringBuilder);
            view.mLegalMessage.setVisibility(View.VISIBLE);
            view.mLegalMessage.setMovementMethod(LinkMovementMethod.getInstance());
        } else if (AutofillSaveCardBottomSheetProperties.ACCEPT_BUTTON_LABEL == propertyKey) {
            setTextViewText(
                    view.mAcceptButton,
                    model.get(AutofillSaveCardBottomSheetProperties.ACCEPT_BUTTON_LABEL));
        } else if (AutofillSaveCardBottomSheetProperties.CANCEL_BUTTON_LABEL == propertyKey) {
            setTextViewText(
                    view.mCancelButton,
                    model.get(AutofillSaveCardBottomSheetProperties.CANCEL_BUTTON_LABEL));
        } else if (AutofillSaveCardBottomSheetProperties.SHOW_LOADING_STATE == propertyKey) {
            if (model.get(AutofillSaveCardBottomSheetProperties.SHOW_LOADING_STATE)) {
                view.mAcceptButton.setVisibility(View.GONE);
                view.mCancelButton.setVisibility(View.GONE);
                view.mLoadingView.showLoadingUI(/* skipDelay= */ true);
            } else {
                view.mLoadingView.hideLoadingUI();
                view.mAcceptButton.setVisibility(View.VISIBLE);
                view.mCancelButton.setVisibility(View.VISIBLE);
            }
        }
    }

    private static void setTextViewText(TextView textView, String text) {
        if (Strings.isNullOrEmpty(text)) {
            textView.setVisibility(View.GONE);
            return;
        }
        textView.setText(text);
        textView.setVisibility(View.VISIBLE);
    }
}
