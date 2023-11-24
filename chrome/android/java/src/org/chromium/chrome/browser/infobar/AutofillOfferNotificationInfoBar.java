// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.graphics.Typeface;
import android.text.SpannableString;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.TextUtils;
import android.text.style.StyleSpan;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.R;
import org.chromium.components.infobars.ConfirmInfoBar;
import org.chromium.components.infobars.InfoBarControlLayout;
import org.chromium.components.infobars.InfoBarLayout;
import org.chromium.ui.UiUtils;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.url.GURL;

/** Infobar to be displayed when an offer is available for the current merchant website. */
public class AutofillOfferNotificationInfoBar extends ConfirmInfoBar {
    private final long mNativeAutofillOfferNotificationInfoBar;
    private String mCreditCardIdentifierString;
    private GURL mOfferDeepLinkUrl;
    private String mTitleText;
    private int mHeaderIconDrawableId;
    private int mNetworkIconDrawableId = -1;

    private AutofillOfferNotificationInfoBar(
            long nativeAutofillOfferNotificationInfoBar,
            int headerIconDrawableId,
            String title,
            String positiveButtonLabel,
            GURL offerDeepLinkUrl) {
        // No icon is specified here; it is rather added in |createContent|. This hides the
        // ImageView that normally shows the icon and gets rid of the left padding of the infobar
        // content.
        super(
                /* iconId= */ 0,
                /* iconTintId= */ 0,
                /* iconBitmap= */ null,
                title,
                /* linkText= */ null,
                positiveButtonLabel,
                /* secondaryButtonText= */ null);
        this.mNativeAutofillOfferNotificationInfoBar = nativeAutofillOfferNotificationInfoBar;
        this.mOfferDeepLinkUrl = offerDeepLinkUrl;
        this.mTitleText = title;
        this.mHeaderIconDrawableId = headerIconDrawableId;
    }

    @CalledByNative
    private static AutofillOfferNotificationInfoBar create(
            long nativeAutofillOfferNotificationInfoBar,
            int headerIconDrawableId,
            String title,
            String positiveButtonLabel,
            GURL offerDeepLinkUrl) {
        return new AutofillOfferNotificationInfoBar(
                nativeAutofillOfferNotificationInfoBar,
                headerIconDrawableId,
                title,
                positiveButtonLabel,
                offerDeepLinkUrl);
    }

    @CalledByNative
    private void setCreditCardDetails(
            String creditCardIdentifierString, int networkIconDrawableId) {
        mCreditCardIdentifierString = creditCardIdentifierString;
        this.mNetworkIconDrawableId = networkIconDrawableId;
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        super.createContent(layout);
        if (TextUtils.isEmpty(mCreditCardIdentifierString) || mHeaderIconDrawableId == 0) {
            return;
        }
        UiUtils.removeViewFromParent(layout.getMessageTextView());
        layout.getMessageLayout().addIconTitle(mHeaderIconDrawableId, mTitleText);
        InfoBarControlLayout control = layout.addControlLayout();

        String offerDetails =
                getContext()
                        .getString(
                                R.string.autofill_offers_reminder_infobar_description_text,
                                mCreditCardIdentifierString);
        SpannableStringBuilder text = new SpannableStringBuilder(offerDetails);
        // Highlight the cardIdentifierString as bold.
        int indexForCardIdentifierString = offerDetails.indexOf(mCreditCardIdentifierString);
        text.setSpan(
                new StyleSpan(Typeface.BOLD),
                indexForCardIdentifierString,
                indexForCardIdentifierString + mCreditCardIdentifierString.length(),
                Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);

        if (mOfferDeepLinkUrl.isValid()) {
            String linkText =
                    getContext().getString(R.string.autofill_offers_reminder_deep_link_text);
            NoUnderlineClickableSpan noUnderlineClickableSpan =
                    new NoUnderlineClickableSpan(
                            getContext(),
                            (view) ->
                                    AutofillOfferNotificationInfoBarJni.get()
                                            .onOfferDeepLinkClicked(
                                                    mNativeAutofillOfferNotificationInfoBar,
                                                    AutofillOfferNotificationInfoBar.this,
                                                    mOfferDeepLinkUrl));
            SpannableString linkSpan = new SpannableString(" " + linkText);
            linkSpan.setSpan(
                    noUnderlineClickableSpan,
                    1,
                    linkText.length() + 1,
                    Spanned.SPAN_INCLUSIVE_INCLUSIVE);
            text.append(linkSpan);
        }
        control.addDescription(text);
    }

    @NativeMethods
    interface Natives {
        void onOfferDeepLinkClicked(
                long nativeAutofillOfferNotificationInfoBar,
                AutofillOfferNotificationInfoBar caller,
                GURL url);
    }
}
