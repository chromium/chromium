// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import static org.chromium.chrome.browser.autofill.AutofillUiUtils.getSpannableStringForLegalMessageLines;

import android.content.Context;
import android.graphics.Bitmap;
import android.text.SpannableString;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.TextUtils;
import android.text.style.TextAppearanceSpan;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeStringConstants;
import org.chromium.chrome.browser.autofill.AutofillUiUtils;
import org.chromium.components.autofill.ImageSize;
import org.chromium.components.autofill.VirtualCardEnrollmentLinkType;
import org.chromium.components.autofill.payments.LegalMessageLine;
import org.chromium.components.infobars.ConfirmInfoBar;
import org.chromium.components.infobars.InfoBarControlLayout;
import org.chromium.components.infobars.InfoBarLayout;
import org.chromium.ui.UiUtils;
import org.chromium.ui.text.NoUnderlineClickableSpan;

import java.util.LinkedList;

/** An infobar for virtual card enrollment information. */
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
    private AutofillVirtualCardEnrollmentInfoBar(
            long nativeAutofillVirtualCardEnrollmentInfoBar,
            int iconId,
            Bitmap iconBitmap,
            String message,
            String linkText,
            String buttonOk,
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
            long nativeAutofillVirtualCardEnrollmentInfoBar,
            int iconId,
            Bitmap iconBitmap,
            String message,
            String linkText,
            String buttonOk,
            String buttonCancel) {
        return new AutofillVirtualCardEnrollmentInfoBar(
                nativeAutofillVirtualCardEnrollmentInfoBar,
                iconId,
                iconBitmap,
                message,
                linkText,
                buttonOk,
                buttonCancel);
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

    // Add legal message lines with links underlined.
    private void addLegalMessageLines(
            Context context,
            InfoBarControlLayout control,
            LinkedList<LegalMessageLine> legalMessageLines,
            @VirtualCardEnrollmentLinkType int virtualCardEnrollmentLinkType) {
        SpannableStringBuilder legalMessageLinesText =
                getSpannableStringForLegalMessageLines(
                        context,
                        legalMessageLines,
                        /* underlineLinks= */ true,
                        url ->
                                AutofillVirtualCardEnrollmentInfoBarJni.get()
                                        .onInfobarLinkClicked(
                                                mNativeAutofillVirtualCardEnrollmentInfoBar,
                                                AutofillVirtualCardEnrollmentInfoBar.this,
                                                url,
                                                virtualCardEnrollmentLinkType));
        control.addDescription(
                legalMessageLinesText, R.style.TextAppearance_TextSmall_Secondary_Baseline);
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        super.createContent(layout);

        // Remove the default title view.
        UiUtils.removeViewFromParent(layout.getMessageTextView());

        InfoBarControlLayout control = layout.addControlLayout();

        // Add the illustration icon.
        control.addLeadImage(R.drawable.virtual_card_enrollment_illustration);

        // Add Google Pay icon and title
        control.addIconTitle(mIconDrawableId, mTitleText);

        // Add infobar description.
        if (!TextUtils.isEmpty(mDescriptionText) && !TextUtils.isEmpty(mLearnMoreLinkText)) {
            SpannableString text = new SpannableString(mDescriptionText);
            int offset = mDescriptionText.length() - mLearnMoreLinkText.length();
            text.setSpan(
                    new NoUnderlineClickableSpan(
                            layout.getContext(),
                            (unused) -> {
                                AutofillVirtualCardEnrollmentInfoBarJni.get()
                                        .onInfobarLinkClicked(
                                                mNativeAutofillVirtualCardEnrollmentInfoBar,
                                                AutofillVirtualCardEnrollmentInfoBar.this,
                                                ChromeStringConstants
                                                        .AUTOFILL_VIRTUAL_CARD_ENROLLMENT_SUPPORT_URL,
                                                VirtualCardEnrollmentLinkType
                                                        .VIRTUAL_CARD_ENROLLMENT_LEARN_MORE_LINK);
                            }),
                    offset,
                    offset + mLearnMoreLinkText.length(),
                    Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
            control.addDescription(text);
        }

        // The card container contains two lines. The first line contains the card name and number,
        // and the second line contains the "Virtual card" label. The second line has a different
        // text appearance than the first line and thus requires us to set the span.
        SpannableString cardContainerText =
                new SpannableString(
                        String.format(
                                "%s\n%s",
                                mCardLabel,
                                layout.getContext()
                                        .getString(
                                                R.string
                                                        .autofill_virtual_card_enrollment_dialog_card_container_title)));
        int spanOffsetStart = mCardLabel.length() + 1;
        cardContainerText.setSpan(
                new TextAppearanceSpan(
                        layout.getContext(), R.style.TextAppearance_TextSmall_Secondary_Baseline),
                spanOffsetStart,
                cardContainerText.length(),
                Spanned.SPAN_INCLUSIVE_EXCLUSIVE);

        // Get and resize the issuer icon.
        AutofillUiUtils.CardIconSpecs cardIconSpecs =
                AutofillUiUtils.CardIconSpecs.create(layout.getContext(), ImageSize.LARGE);
        Bitmap scaledIssuerIcon =
                Bitmap.createScaledBitmap(
                        mIssuerIcon, cardIconSpecs.getWidth(), cardIconSpecs.getHeight(), true);

        // Add the issuer icon and the card container text.
        control.addIcon(
                scaledIssuerIcon,
                0,
                cardContainerText,
                null,
                R.dimen.infobar_descriptive_text_size);

        addLegalMessageLines(
                layout.getContext(),
                control,
                mGoogleLegalMessageLines,
                VirtualCardEnrollmentLinkType.VIRTUAL_CARD_ENROLLMENT_GOOGLE_PAYMENTS_TOS_LINK);
        addLegalMessageLines(
                layout.getContext(),
                control,
                mIssuerLegalMessageLines,
                VirtualCardEnrollmentLinkType.VIRTUAL_CARD_ENROLLMENT_ISSUER_TOS_LINK);
    }

    @NativeMethods
    interface Natives {
        void onInfobarLinkClicked(
                long nativeAutofillVirtualCardEnrollmentInfoBar,
                AutofillVirtualCardEnrollmentInfoBar caller,
                String url,
                @VirtualCardEnrollmentLinkType int virtualCardEnrollmentLinkType);
    }
}
