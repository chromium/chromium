// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.vcn;

import android.content.Context;
import android.graphics.Bitmap;
import android.text.SpannableString;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.view.View;
import android.widget.TextView;

import org.chromium.chrome.browser.autofill.AutofillUiUtils;
import org.chromium.chrome.browser.autofill.vcn.AutofillVcnEnrollBottomSheetProperties.Description;
import org.chromium.chrome.browser.autofill.vcn.AutofillVcnEnrollBottomSheetProperties.IssuerIcon;
import org.chromium.chrome.browser.autofill.vcn.AutofillVcnEnrollBottomSheetProperties.LegalMessages;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** The view-binder of the autofill virtual card enrollment bottom sheet UI. */
/*package*/ class AutofillVcnEnrollBottomSheetViewBinder {
    /**
     * Updates the view based on changes in the model.
     *
     * @param model The updated model to read.
     * @param view The view to update.
     * @param propertyKey The property of the model that has changed.
     */
    static void bind(
            PropertyModel model, AutofillVcnEnrollBottomSheetView view, PropertyKey propertyKey) {
        if (AutofillVcnEnrollBottomSheetProperties.MESSAGE_TEXT == propertyKey) {
            view.mDialogTitle.setText(
                    model.get(AutofillVcnEnrollBottomSheetProperties.MESSAGE_TEXT));

        } else if (AutofillVcnEnrollBottomSheetProperties.DESCRIPTION == propertyKey) {
            view.mVirtualCardDescription.setText(
                    getDescriptionSpan(
                            model.get(AutofillVcnEnrollBottomSheetProperties.DESCRIPTION)));
            view.mVirtualCardDescription.setMovementMethod(LinkMovementMethod.getInstance());

        } else if (AutofillVcnEnrollBottomSheetProperties.CARD_CONTAINER_ACCESSIBILITY_DESCRIPTION
                == propertyKey) {
            view.mCardContainer.setContentDescription(
                    model.get(
                            AutofillVcnEnrollBottomSheetProperties
                                    .CARD_CONTAINER_ACCESSIBILITY_DESCRIPTION));

        } else if (AutofillVcnEnrollBottomSheetProperties.ISSUER_ICON == propertyKey) {
            view.mIssuerIcon.setImageBitmap(
                    scaleBitmap(model.get(AutofillVcnEnrollBottomSheetProperties.ISSUER_ICON)));

        } else if (AutofillVcnEnrollBottomSheetProperties.CARD_LABEL == propertyKey) {
            view.mCardLabel.setText(model.get(AutofillVcnEnrollBottomSheetProperties.CARD_LABEL));

        } else if (AutofillVcnEnrollBottomSheetProperties.CARD_DESCRIPTION == propertyKey) {
            view.mCardDescription.setText(
                    model.get(AutofillVcnEnrollBottomSheetProperties.CARD_DESCRIPTION));

        } else if (AutofillVcnEnrollBottomSheetProperties.GOOGLE_LEGAL_MESSAGES == propertyKey) {
            setLegalMessageOrHideIfEmpty(
                    view.mContentView.getContext(),
                    view.mGoogleLegalMessage,
                    model.get(AutofillVcnEnrollBottomSheetProperties.GOOGLE_LEGAL_MESSAGES));

        } else if (AutofillVcnEnrollBottomSheetProperties.ISSUER_LEGAL_MESSAGES == propertyKey) {
            setLegalMessageOrHideIfEmpty(
                    view.mContentView.getContext(),
                    view.mIssuerLegalMessage,
                    model.get(AutofillVcnEnrollBottomSheetProperties.ISSUER_LEGAL_MESSAGES));

        } else if (AutofillVcnEnrollBottomSheetProperties.ACCEPT_BUTTON_LABEL == propertyKey) {
            view.mAcceptButton.setText(
                    model.get(AutofillVcnEnrollBottomSheetProperties.ACCEPT_BUTTON_LABEL));

        } else if (AutofillVcnEnrollBottomSheetProperties.CANCEL_BUTTON_LABEL == propertyKey) {
            view.mCancelButton.setText(
                    model.get(AutofillVcnEnrollBottomSheetProperties.CANCEL_BUTTON_LABEL));
        } else if (AutofillVcnEnrollBottomSheetProperties.SHOW_LOADING_STATE == propertyKey) {
            if (model.get(AutofillVcnEnrollBottomSheetProperties.SHOW_LOADING_STATE)) {
                view.mAcceptButton.setVisibility(View.GONE);
                view.mCancelButton.setVisibility(View.GONE);
                view.mLoadingView.showLoadingUI(/* skipDelay= */ true);
                view.mLoadingViewContainer.setVisibility(View.VISIBLE);
            } else {
                view.mLoadingViewContainer.setVisibility(View.GONE);
                view.mLoadingView.hideLoadingUI();
                view.mAcceptButton.setVisibility(View.VISIBLE);
                view.mCancelButton.setVisibility(View.VISIBLE);
            }
        } else if (AutofillVcnEnrollBottomSheetProperties.LOADING_DESCRIPTION == propertyKey) {
            view.mLoadingViewContainer.setContentDescription(
                    model.get(AutofillVcnEnrollBottomSheetProperties.LOADING_DESCRIPTION));
        }
    }

    // Returns the virtual card description text with a "learn more" link.
    private static SpannableString getDescriptionSpan(Description description) {
        SpannableString result = new SpannableString(new String());
        if (description == null
                || description.mText == null
                || description.mText.isEmpty()
                || description.mLearnMoreLinkText == null
                || description.mLearnMoreLinkText.isEmpty()
                || description.mLearnMoreLinkUrl == null
                || description.mLearnMoreLinkUrl.isEmpty()
                || description.mLinkOpener == null) {
            return result;
        }

        int learnMoreStart = description.mText.indexOf(description.mLearnMoreLinkText);
        if (learnMoreStart < 0) return result;

        int learnMoreEnd = learnMoreStart + description.mLearnMoreLinkText.length();
        result = new SpannableString(description.mText);
        result.setSpan(
                new ClickableSpan() {
                    @Override
                    public void onClick(View view) {
                        description.mLinkOpener.openLink(
                                description.mLearnMoreLinkUrl, description.mLearnMoreLinkType);
                    }
                },
                learnMoreStart,
                learnMoreEnd,
                Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
        return result;
    }

    // Returns a scaled bitmap.
    private static Bitmap scaleBitmap(IssuerIcon issuerIcon) {
        return issuerIcon != null
                        && issuerIcon.mBitmap != null
                        && issuerIcon.mWidth > 0
                        && issuerIcon.mHeight > 0
                ? Bitmap.createScaledBitmap(
                        issuerIcon.mBitmap,
                        issuerIcon.mWidth,
                        issuerIcon.mHeight,
                        /* filter= */ true)
                : null;
    }

    // Returns the legal message text formatted with links.
    private static SpannableStringBuilder getLegalMessageSpan(
            Context context, LegalMessages legalMessages) {
        return legalMessages != null
                        && legalMessages.mLines != null
                        && !legalMessages.mLines.isEmpty()
                        && legalMessages.mLinkOpener != null
                ? AutofillUiUtils.getSpannableStringForLegalMessageLines(
                        context,
                        legalMessages.mLines,
                        /* underlineLinks= */ true,
                        (String url) -> {
                            legalMessages.mLinkOpener.openLink(url, legalMessages.mLinkType);
                        })
                : new SpannableStringBuilder();
    }

    // Sets the legal message text formatted with links, or hides the text view for empty legal
    // message.
    private static void setLegalMessageOrHideIfEmpty(
            Context context, TextView textView, LegalMessages legalMessages) {
        SpannableStringBuilder stringBuilder = getLegalMessageSpan(context, legalMessages);
        if (stringBuilder.length() > 0) {
            textView.setText(stringBuilder);
            textView.setMovementMethod(LinkMovementMethod.getInstance());
            textView.setVisibility(View.VISIBLE);
        } else {
            textView.setVisibility(View.GONE);
        }
    }
}
