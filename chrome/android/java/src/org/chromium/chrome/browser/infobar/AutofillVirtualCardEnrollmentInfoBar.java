// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.graphics.Bitmap;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.TextUtils;
import android.text.style.ClickableSpan;
import android.view.View;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeStringConstants;
import org.chromium.chrome.browser.autofill.LegalMessageLine;
import org.chromium.components.autofill.VirtualCardEnrollmentLinkType;
import org.chromium.components.infobars.ConfirmInfoBar;
import org.chromium.components.infobars.InfoBarControlLayout;
import org.chromium.components.infobars.InfoBarLayout;
import org.chromium.ui.UiUtils;
import org.chromium.ui.text.NoUnderlineClickableSpan;

import java.util.LinkedList;

/**
 * An infobar for virtual card enrollment information.
 */
public class AutofillVirtualCardEnrollmentInfoBar extends ConfirmInfoBar {
    private final long mNativeAutofillVirtualCardEnrollmentInfoBar;
    private Bitmap mIssuerIcon;
    private String mCardLabel;
    private int mIconDrawableId = -1;
    private String mTitleText;
    private String mDescriptionText;
    private String mLearnMoreLinkText;
    private final LinkedList<LegalMessageLine> mGoogleLegalMessageLines =
            new LinkedList<LegalMessageLine>();
    private final LinkedList<LegalMessageLine> mIssuerLegalMessageLines =
            new LinkedList<LegalMessageLine>();

    /**
     * Creates a new instance of the infobar.
     *
     * @param nativeAutofillVirtualCardEnrollmentInfoBar The pointer to the native object for
     *         callbacks.
     * @param iconId ID corresponding to the icon that will be shown for the InfoBar.
     * @param iconBitmap Bitmap to use if there is no equivalent Java resource for iconId.
     * @param message Title of the infobar to display along the icon.
     * @param linkText Link text to display in addition to the message.
     * @param buttonOk String to display on the OK button.
     * @param buttonCancel String to display on the Cancel button.
     */
    private AutofillVirtualCardEnrollmentInfoBar(long nativeAutofillVirtualCardEnrollmentInfoBar,
            int iconId, Bitmap iconBitmap, String message, String linkText, String buttonOk,
            String buttonCancel) {
        super(0, 0, iconBitmap, message, linkText, buttonOk, buttonCancel);
        mIconDrawableId = iconId;
        mTitleText = message;
        mNativeAutofillVirtualCardEnrollmentInfoBar = nativeAutofillVirtualCardEnrollmentInfoBar;
    }

    /**
     * Creates an infobar for saving a credit card.
     *
     * @param nativeAutofillVirtualCardEnrollmentInfoBar The pointer to the native object for
     *         callbacks.
     * @param iconId ID corresponding to the icon that will be shown for the InfoBar.
     * @param iconBitmap Bitmap to use if there is no equivalent Java resource for iconId.
     * @param message Title of the infobar to display along the icon.
     * @param linkText Link text to display in addition to the message.
     * @param buttonOk String to display on the OK button.
     * @param buttonCancel String to display on the Cancel button.
     * @return A new instance of the infobar.
     */
    @CalledByNative
    private static AutofillVirtualCardEnrollmentInfoBar create(
            long nativeAutofillVirtualCardEnrollmentInfoBar, int iconId, Bitmap iconBitmap,
            String message, String linkText, String buttonOk, String buttonCancel) {
        return new AutofillVirtualCardEnrollmentInfoBar(nativeAutofillVirtualCardEnrollmentInfoBar,
                iconId, iconBitmap, message, linkText, buttonOk, buttonCancel);
    }

    /**
     * Adds information to the infobar about the credit card that will be enrolled into virtual
     * card.
     *
     * @param issuerIcon Bitmap image of the icon that will be shown for this credit card.
     * @param label The credit card label, for example "***1234".
     */
    @CalledByNative
    private void addCardDetail(Bitmap issuerIcon, String label) {
        mIssuerIcon = issuerIcon;
        mCardLabel = label;
    }

    /**
     * Sets description line to the infobar.
     *
     * @param descriptionText description line text.
     * @param learnMoreLinkText text of the learn more link.
     */
    @CalledByNative
    private void setDescription(String descriptionText, String learnMoreLinkText) {
        mDescriptionText = descriptionText;
        mLearnMoreLinkText = learnMoreLinkText;
    }

    /**
     * Adds a line of Google legal message plain text to the infobar.
     *
     * @param text The Google legal message plain text.
     */
    @CalledByNative
    private void addGoogleLegalMessageLine(String text) {
        mGoogleLegalMessageLines.add(new LegalMessageLine(text));
    }

    /**
     * Adds a line of issuer legal message plain text to the infobar.
     *
     * @param text The issuer legal message plain text.
     */
    @CalledByNative
    private void addIssuerLegalMessageLine(String text) {
        mIssuerLegalMessageLines.add(new LegalMessageLine(text));
    }

    /**
     * Marks up the last added line of Google legal message text with a link.
     *
     * @param start The inclusive offset of the start of the link in the text.
     * @param end The exclusive offset of the end of the link in the text.
     * @param url The URL to open when the link is clicked.
     */
    @CalledByNative
    private void addLinkToLastGoogleLegalMessageLine(int start, int end, String url) {
        mGoogleLegalMessageLines.getLast().links.add(new LegalMessageLine.Link(start, end, url));
    }

    /**
     * Marks up the last added line of issuer legal message text with a link.
     *
     * @param start The inclusive offset of the start of the link in the text.
     * @param end The exclusive offset of the end of the link in the text.
     * @param url The URL to open when the link is clicked.
     */
    @CalledByNative
    private void addLinkToLastIssuerLegalMessageLine(int start, int end, String url) {
        mIssuerLegalMessageLines.getLast().links.add(new LegalMessageLine.Link(start, end, url));
    }

    // TODO(vishwasuppoor@): Refactor and use getSpannableStringForLegalMessageLines from
    // AutofillUtils.
    private void addLegalMessageLines(LinkedList<LegalMessageLine> legalMessageLines,
            InfoBarControlLayout control,
            @VirtualCardEnrollmentLinkType int virtualCardEnrollmentLinkType) {
        for (LegalMessageLine line : legalMessageLines) {
            SpannableString text = new SpannableString(line.text);
            for (final LegalMessageLine.Link link : line.links) {
                text.setSpan(new ClickableSpan() {
                    @Override
                    public void onClick(View view) {
                        AutofillVirtualCardEnrollmentInfoBarJni.get().onInfobarLinkClicked(
                                mNativeAutofillVirtualCardEnrollmentInfoBar,
                                AutofillVirtualCardEnrollmentInfoBar.this, link.url,
                                virtualCardEnrollmentLinkType);
                    }
                }, link.start, link.end, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
            }
            control.addDescription(text);
        }
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        super.createContent(layout);

        UiUtils.removeViewFromParent(layout.getMessageTextView());
        layout.getMessageLayout().addIconTitle(mIconDrawableId, mTitleText);

        InfoBarControlLayout control = layout.addControlLayout();
        if (!TextUtils.isEmpty(mDescriptionText) && !TextUtils.isEmpty(mLearnMoreLinkText)) {
            SpannableString text = new SpannableString(mDescriptionText);
            int offset = mDescriptionText.length() - mLearnMoreLinkText.length();
            text.setSpan(new NoUnderlineClickableSpan(layout.getContext(), (unused) -> {
                AutofillVirtualCardEnrollmentInfoBarJni.get().onInfobarLinkClicked(
                        mNativeAutofillVirtualCardEnrollmentInfoBar,
                        AutofillVirtualCardEnrollmentInfoBar.this,
                        ChromeStringConstants.AUTOFILL_VIRTUAL_CARD_ENROLLMENT_SUPPORT_URL,
                        VirtualCardEnrollmentLinkType.VIRTUAL_CARD_ENROLLMENT_LEARN_MORE_LINK);
            }), offset, offset + mLearnMoreLinkText.length(), Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
            control.addDescription(text);
        }

        String formattedCardLabel = String.format("%s\n%s %s",
                layout.getContext().getString(
                        R.string.autofill_virtual_card_enrollment_dialog_card_container_title),
                layout.getContext().getString(
                        R.string.autofill_virtual_card_enrollment_infobar_card_prefix),
                mCardLabel);
        Bitmap scaledIssuerIcon = Bitmap.createScaledBitmap(mIssuerIcon,
                layout.getResources().getDimensionPixelSize(
                        R.dimen.virtual_card_enrollment_dialog_card_art_width),
                layout.getResources().getDimensionPixelSize(
                        R.dimen.virtual_card_enrollment_dialog_card_art_height),
                true);
        control.addIcon(scaledIssuerIcon, 0, formattedCardLabel, null,
                R.dimen.infobar_descriptive_text_size);

        addLegalMessageLines(mGoogleLegalMessageLines, control,
                VirtualCardEnrollmentLinkType.VIRTUAL_CARD_ENROLLMENT_GOOGLE_PAYMENTS_TOS_LINK);
        addLegalMessageLines(mIssuerLegalMessageLines, control,
                VirtualCardEnrollmentLinkType.VIRTUAL_CARD_ENROLLMENT_ISSUER_TOS_LINK);
    }

    @NativeMethods
    interface Natives {
        void onInfobarLinkClicked(long nativeAutofillVirtualCardEnrollmentInfoBar,
                AutofillVirtualCardEnrollmentInfoBar caller, String url,
                @VirtualCardEnrollmentLinkType int virtualCardEnrollmentLinkType);
    }
}
