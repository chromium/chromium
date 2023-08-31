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

import org.chromium.base.Callback;
import org.chromium.chrome.browser.autofill.AutofillUiUtils;
import org.chromium.components.autofill.payments.LegalMessageLine;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.LinkedList;

/** The view-binder of the autofill virtual card enrollment bottom sheet UI. */
/*package*/ class AutofillVcnEnrollBottomSheetViewBinder {
    private final Callback<String> mUrlLauncher;
    private final int mIssuerIconWidth;
    private final int mIssuerIconHeight;

    /**
     * Creates the view-binder of the autofill virtual card enrollment bottom sheet UI.
     *
     * @param urlLauncher The callback to invoke when a link is tapped.
     * @param issuerIconWidth The width of the card image.
     * @param issuerIconHeight The height of the card image.
     */
    /*package*/ AutofillVcnEnrollBottomSheetViewBinder(
            Callback<String> urlLauncher, int issuerIconWidth, int issuerIconHeight) {
        mUrlLauncher = urlLauncher;
        mIssuerIconWidth = issuerIconWidth;
        mIssuerIconHeight = issuerIconHeight;
    }

    /**
     * Updates the view based on changes in the model.
     *
     * @param model The updated model to read.
     * @param view The view to update.
     * @param propertyKey The property of the model that has changed.
     */
    /*package*/ void bind(
            PropertyModel model, AutofillVcnEnrollBottomSheetView view, PropertyKey propertyKey) {
        if (AutofillVcnEnrollBottomSheetProperties.MESSAGE_TEXT == propertyKey) {
            view.mDialogTitle.setText(
                    model.get(AutofillVcnEnrollBottomSheetProperties.MESSAGE_TEXT));

        } else if (AutofillVcnEnrollBottomSheetProperties.DESCRIPTION_TEXT == propertyKey) {
            view.mVirtualCardDescription.setText(getDescriptionSpan(
                    model.get(AutofillVcnEnrollBottomSheetProperties.DESCRIPTION_TEXT)));
            view.mVirtualCardDescription.setMovementMethod(LinkMovementMethod.getInstance());

        } else if (AutofillVcnEnrollBottomSheetProperties.CARD_CONTAINER_ACCESSIBILITY_DESCRIPTION
                == propertyKey) {
            view.mCardContainer.setContentDescription(
                    model.get(AutofillVcnEnrollBottomSheetProperties
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
            view.mGoogleLegalMessage.setText(getLegalMessageSpan(view.mContentView.getContext(),
                    model.get(AutofillVcnEnrollBottomSheetProperties.GOOGLE_LEGAL_MESSAGES)));
            view.mGoogleLegalMessage.setMovementMethod(LinkMovementMethod.getInstance());

        } else if (AutofillVcnEnrollBottomSheetProperties.ISSUER_LEGAL_MESSAGES == propertyKey) {
            view.mIssuerLegalMessage.setText(getLegalMessageSpan(view.mContentView.getContext(),
                    model.get(AutofillVcnEnrollBottomSheetProperties.ISSUER_LEGAL_MESSAGES)));
            view.mIssuerLegalMessage.setMovementMethod(LinkMovementMethod.getInstance());

        } else if (AutofillVcnEnrollBottomSheetProperties.ACCEPT_BUTTON_LABEL == propertyKey) {
            view.mAcceptButton.setText(
                    model.get(AutofillVcnEnrollBottomSheetProperties.ACCEPT_BUTTON_LABEL));

        } else if (AutofillVcnEnrollBottomSheetProperties.CANCEL_BUTTON_LABEL == propertyKey) {
            view.mCancelButton.setText(
                    model.get(AutofillVcnEnrollBottomSheetProperties.CANCEL_BUTTON_LABEL));
        }
    }

    // Returns the virtual card description text with a "learn more" link.
    private SpannableString getDescriptionSpan(ArrayList<String> descriptionTextComponents) {
        SpannableString result = new SpannableString(new String());
        if (descriptionTextComponents == null || descriptionTextComponents.size() != 3) {
            return result;
        }

        String descriptionText = descriptionTextComponents.get(0);
        String learnMoreLinkText = descriptionTextComponents.get(1);
        String learnMoreLinkUrl = descriptionTextComponents.get(2);

        if (learnMoreLinkText.isEmpty()) return result;

        int learnMoreStart = descriptionText.indexOf(learnMoreLinkText);
        if (learnMoreStart < 0) return result;

        int learnMoreEnd = learnMoreStart + learnMoreLinkText.length();
        result = new SpannableString(descriptionText);
        result.setSpan(new ClickableSpan() {
            @Override
            public void onClick(View view) {
                mUrlLauncher.onResult(learnMoreLinkUrl);
            }
        }, learnMoreStart, learnMoreEnd, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
        return result;
    }

    // Returns a scaled bitmap.
    private Bitmap scaleBitmap(Bitmap bitmap) {
        return bitmap != null ? Bitmap.createScaledBitmap(
                       bitmap, mIssuerIconWidth, mIssuerIconHeight, /*filter=*/true)
                              : null;
    }

    // Returns the legal message text formatted with links.
    private SpannableStringBuilder getLegalMessageSpan(
            Context context, LinkedList<LegalMessageLine> lines) {
        return lines != null && !lines.isEmpty()
                ? AutofillUiUtils.getSpannableStringForLegalMessageLines(
                        context, lines, /*underlineLinks=*/true, mUrlLauncher::onResult)
                : new SpannableStringBuilder();
    }
}
