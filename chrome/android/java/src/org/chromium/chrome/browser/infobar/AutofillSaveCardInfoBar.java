// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.TextUtils;
import android.text.style.ClickableSpan;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.R;
import org.chromium.components.autofill.payments.CardDetail;
import org.chromium.components.autofill.payments.LegalMessageLine;
import org.chromium.components.browser_ui.widget.RoundedCornerImageView;
import org.chromium.components.infobars.ConfirmInfoBar;
import org.chromium.components.infobars.InfoBarControlLayout;
import org.chromium.components.infobars.InfoBarLayout;
import org.chromium.ui.UiUtils;

import java.util.ArrayList;
import java.util.LinkedList;
import java.util.List;

/** An infobar for saving credit card information. */
public class AutofillSaveCardInfoBar extends ConfirmInfoBar {

    private final @Nullable String mAccountFooterEmail;
    private final @Nullable Bitmap mAccountFooterAvatar;
    private final long mNativeAutofillSaveCardInfoBar;
    private final List<CardDetail> mCardDetails = new ArrayList<>();
    private int mIconDrawableId = -1;
    private String mTitleText;
    private String mDescriptionText;
    private boolean mIsGooglePayBrandingEnabled;
    private final LinkedList<LegalMessageLine> mLegalMessageLines =
            new LinkedList<LegalMessageLine>();

    /**
     * Creates a new instance of the infobar.
     *
     * @param nativeAutofillSaveCardInfoBar The pointer to the native object for callbacks.
     * @param iconId ID corresponding to the icon that will be shown for the InfoBar.
     * @param iconBitmap Bitmap to use if there is no equivalent Java resource for iconId.
     * @param message Title of the infobar to display along the icon.
     * @param linkText Link text to display in addition to the message.
     * @param buttonOk String to display on the OK button.
     * @param buttonCancel String to display on the Cancel button.
     * @param accountFooterEmail The email to be shown on the footer, or null. The footer is
     * only shown if both this and |accountFooterAvatar| are provided.
     * @param accountFooterAvatar The avatar to be shown on the footer, or null. The footer is
     * only shown if both this and |accountFooterEmail| are provided.
     */
    private AutofillSaveCardInfoBar(
            long nativeAutofillSaveCardInfoBar,
            int iconId,
            Bitmap iconBitmap,
            String message,
            String linkText,
            String buttonOk,
            String buttonCancel,
            boolean isGooglePayBrandingEnabled,
            @Nullable String accountFooterEmail,
            @Nullable Bitmap accountFooterAvatar) {
        // If Google Pay branding is enabled, no icon is specified here; it is rather added in
        // |createContent|. This hides the ImageView that normally shows the icon and gets rid of
        // the left padding of the infobar content.
        super(
                isGooglePayBrandingEnabled ? 0 : iconId,
                isGooglePayBrandingEnabled ? 0 : R.color.infobar_icon_drawable_color,
                iconBitmap,
                message,
                linkText,
                buttonOk,
                buttonCancel);
        mIconDrawableId = iconId;
        mTitleText = message;
        mIsGooglePayBrandingEnabled = isGooglePayBrandingEnabled;
        mNativeAutofillSaveCardInfoBar = nativeAutofillSaveCardInfoBar;
        mAccountFooterEmail = accountFooterEmail;
        mAccountFooterAvatar = accountFooterAvatar;
    }

    /**
     * Creates an infobar for saving a credit card.
     *
     * @param nativeAutofillSaveCardInfoBar The pointer to the native object for callbacks.
     * @param iconId ID corresponding to the icon that will be shown for the InfoBar.
     * @param iconBitmap Bitmap to use if there is no equivalent Java resource for iconId.
     * @param message Title of the infobar to display along the icon.
     * @param linkText Link text to display in addition to the message.
     * @param buttonOk String to display on the OK button.
     * @param buttonCancel String to display on the Cancel button.
     * @param accountFooterEmail The email to be shown on the footer, or null. The footer is
     * only shown if both this and |accountFooterAvatar| are provided.
     * @param accountFooterAvatar The avatar to be shown on the footer, or null. The footer is
     * only shown if both this and |accountFooterEmail| are provided.
     * @return A new instance of the infobar.
     */
    @CalledByNative
    private static AutofillSaveCardInfoBar create(
            long nativeAutofillSaveCardInfoBar,
            int iconId,
            Bitmap iconBitmap,
            String message,
            String linkText,
            String buttonOk,
            String buttonCancel,
            boolean isGooglePayBrandingEnabled,
            @Nullable String accountFooterEmail,
            @Nullable Bitmap accountFooterAvatar) {
        return new AutofillSaveCardInfoBar(
                nativeAutofillSaveCardInfoBar,
                iconId,
                iconBitmap,
                message,
                linkText,
                buttonOk,
                buttonCancel,
                isGooglePayBrandingEnabled,
                accountFooterEmail,
                accountFooterAvatar);
    }

    /**
     * Adds information to the infobar about the credit card that will be saved.
     *
     * @param iconId ID corresponding to the icon that will be shown for this credit card.
     * @param label The credit card label, for example "***1234".
     * @param subLabel The credit card sub-label, for example "Exp: 06/17".
     */
    @CalledByNative
    private void addDetail(int iconId, String label, String subLabel) {
        mCardDetails.add(new CardDetail(iconId, label, subLabel));
    }

    /**
     * Sets description line to the infobar.
     *
     * @param text description line text.
     */
    @CalledByNative
    private void setDescriptionText(String text) {
        mDescriptionText = text;
    }

    /**
     * Adds a line of legal message plain text to the infobar.
     *
     * @param text The legal message plain text.
     */
    @CalledByNative
    private void addLegalMessageLine(String text) {
        mLegalMessageLines.add(new LegalMessageLine(text));
    }

    /**
     * Marks up the last added line of legal message text with a link.
     *
     * @param start The inclusive offset of the start of the link in the text.
     * @param end The exclusive offset of the end of the link in the text.
     * @param url The URL to open when the link is clicked.
     */
    @CalledByNative
    private void addLinkToLastLegalMessageLine(int start, int end, String url) {
        mLegalMessageLines.getLast().links.add(new LegalMessageLine.Link(start, end, url));
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        super.createContent(layout);

        // If Google Pay branding is enabled, add both the icon and the title message to the message
        // container, since no icon was added to the ImageView that normally shows the icon.
        if (mIsGooglePayBrandingEnabled) {
            UiUtils.removeViewFromParent(layout.getMessageTextView());
            layout.getMessageLayout().addIconTitle(mIconDrawableId, mTitleText);
        }

        InfoBarControlLayout control = layout.addControlLayout();
        if (!TextUtils.isEmpty(mDescriptionText)) {
            control.addDescription(mDescriptionText);
        }

        for (int i = 0; i < mCardDetails.size(); i++) {
            CardDetail detail = mCardDetails.get(i);
            control.addIcon(
                    detail.issuerIconDrawableId,
                    0,
                    detail.label,
                    detail.subLabel,
                    R.dimen.infobar_descriptive_text_size);
        }

        for (LegalMessageLine line : mLegalMessageLines) {
            SpannableString text = new SpannableString(line.text);
            for (final LegalMessageLine.Link link : line.links) {
                text.setSpan(
                        new ClickableSpan() {
                            @Override
                            public void onClick(View view) {
                                AutofillSaveCardInfoBarJni.get()
                                        .onLegalMessageLinkClicked(
                                                mNativeAutofillSaveCardInfoBar,
                                                AutofillSaveCardInfoBar.this,
                                                link.url);
                            }
                        },
                        link.start,
                        link.end,
                        Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
            }
            control.addDescription(text);
        }

        if (mAccountFooterEmail != null && mAccountFooterAvatar != null) {
            Resources res = layout.getResources();
            int smallIconSize = res.getDimensionPixelSize(R.dimen.infobar_small_icon_size);
            int padding = res.getDimensionPixelOffset(R.dimen.infobar_padding);

            LinearLayout footer =
                    (LinearLayout)
                            LayoutInflater.from(layout.getContext())
                                    .inflate(R.layout.infobar_footer, null, false);

            TextView emailView = footer.findViewById(R.id.infobar_footer_email);
            emailView.setText(mAccountFooterEmail);

            RoundedCornerImageView profilePicView =
                    footer.findViewById(R.id.infobar_footer_profile_pic);
            Bitmap resizedProfilePic =
                    Bitmap.createScaledBitmap(
                            mAccountFooterAvatar, smallIconSize, smallIconSize, false);
            profilePicView.setRoundedCorners(
                    smallIconSize / 2, smallIconSize / 2, smallIconSize / 2, smallIconSize / 2);
            profilePicView.setImageBitmap(resizedProfilePic);

            layout.addFooterView(footer);
        }
    }

    @NativeMethods
    interface Natives {
        void onLegalMessageLinkClicked(
                long nativeAutofillSaveCardInfoBar, AutofillSaveCardInfoBar caller, String url);
    }
}
