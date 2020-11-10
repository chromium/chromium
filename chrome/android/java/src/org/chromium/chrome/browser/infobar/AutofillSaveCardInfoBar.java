// Copyright 2016 The Chromium Authors. All rights reserved.
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

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.widget.RoundedCornerImageView;
import org.chromium.components.infobars.ConfirmInfoBar;
import org.chromium.components.infobars.InfoBarControlLayout;
import org.chromium.components.infobars.InfoBarLayout;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.ui.UiUtils;

import java.util.ArrayList;
import java.util.LinkedList;
import java.util.List;

/**
 * An infobar for saving credit card information.
 */
public class AutofillSaveCardInfoBar extends ConfirmInfoBar {
    /**
     * Legal message line with links to show in the infobar.
     */
    public static class LegalMessageLine {
        /**
         * A link in the legal message line.
         */
        public static class Link {
            /**
             * The starting inclusive index of the link position in the text.
             */
            public int start;

            /**
             * The ending exclusive index of the link position in the text.
             */
            public int end;

            /**
             * The URL of the link.
             */
            public String url;

            /**
             * Creates a new instance of the link.
             *
             * @param start The starting inclusive index of the link position in the text.
             * @param end The ending exclusive index of the link position in the text.
             * @param url The URL of the link.
             */
            public Link(int start, int end, String url) {
                this.start = start;
                this.end = end;
                this.url = url;
            }
        }

        /**
         * The plain text legal message line.
         */
        public String text;

        /**
         * A collection of links in the legal message line.
         */
        public final List<Link> links = new LinkedList<Link>();

        /**
         * Creates a new instance of the legal message line.
         *
         * @param text The plain text legal message.
         */
        public LegalMessageLine(String text) {
            this.text = text;
        }
    }

    private final AccountInfo mAccountInfo;
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
     * @param accountInfo AccountInfo which includes user's email address and profile picture.
     */
    private AutofillSaveCardInfoBar(long nativeAutofillSaveCardInfoBar, int iconId,
            Bitmap iconBitmap, String message, String linkText, String buttonOk,
            String buttonCancel, boolean isGooglePayBrandingEnabled, AccountInfo accountInfo) {
        // If Google Pay branding is enabled, no icon is specified here; it is rather added in
        // |createContent|. This hides the ImageView that normally shows the icon and gets rid of
        // the left padding of the infobar content.
        super(isGooglePayBrandingEnabled ? 0 : iconId,
                isGooglePayBrandingEnabled ? 0 : R.color.infobar_icon_drawable_color, iconBitmap,
                message, linkText, buttonOk, buttonCancel);
        mIconDrawableId = iconId;
        mTitleText = message;
        mIsGooglePayBrandingEnabled = isGooglePayBrandingEnabled;
        mNativeAutofillSaveCardInfoBar = nativeAutofillSaveCardInfoBar;
        mAccountInfo = accountInfo;
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
     * @param accountInfo AccountInfo which includes user's email address and profile picture.
     * @return A new instance of the infobar.
     */
    @CalledByNative
    private static AutofillSaveCardInfoBar create(long nativeAutofillSaveCardInfoBar, int iconId,
            Bitmap iconBitmap, String message, String linkText, String buttonOk,
            String buttonCancel, boolean isGooglePayBrandingEnabled, AccountInfo accountInfo) {
        return new AutofillSaveCardInfoBar(nativeAutofillSaveCardInfoBar, iconId, iconBitmap,
                message, linkText, buttonOk, buttonCancel, isGooglePayBrandingEnabled, accountInfo);
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
            control.addIcon(detail.issuerIconDrawableId, 0, detail.label, detail.subLabel,
                    R.dimen.infobar_descriptive_text_size);
        }

        for (LegalMessageLine line : mLegalMessageLines) {
            SpannableString text = new SpannableString(line.text);
            for (final LegalMessageLine.Link link : line.links) {
                text.setSpan(new ClickableSpan() {
                    @Override
                    public void onClick(View view) {
                        AutofillSaveCardInfoBarJni.get().onLegalMessageLinkClicked(
                                mNativeAutofillSaveCardInfoBar, AutofillSaveCardInfoBar.this,
                                link.url);
                    }
                }, link.start, link.end, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
            }
            control.addDescription(text);
        }

        if (ChromeFeatureList.isEnabled(
                    ChromeFeatureList.AUTOFILL_ENABLE_SAVE_CARD_INFO_BAR_ACCOUNT_INDICATION_FOOTER)
                && mAccountInfo != null && !TextUtils.isEmpty(mAccountInfo.getEmail())
                && mAccountInfo.getAccountImage() != null) {
            Resources res = layout.getResources();
            int smallIconSize = res.getDimensionPixelSize(R.dimen.infobar_small_icon_size);
            int padding = res.getDimensionPixelOffset(R.dimen.infobar_padding);

            LinearLayout footer = (LinearLayout) LayoutInflater.from(layout.getContext())
                                          .inflate(R.layout.infobar_footer, null, false);

            TextView emailView = (TextView) footer.findViewById(R.id.infobar_footer_email);
            emailView.setText(mAccountInfo.getEmail());

            RoundedCornerImageView profilePicView =
                    (RoundedCornerImageView) footer.findViewById(R.id.infobar_footer_profile_pic);
            Bitmap resizedProfilePic = Bitmap.createScaledBitmap(
                    mAccountInfo.getAccountImage(), smallIconSize, smallIconSize, false);
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
