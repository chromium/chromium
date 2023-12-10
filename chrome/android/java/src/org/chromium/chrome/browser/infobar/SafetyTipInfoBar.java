// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.graphics.Bitmap;
import android.text.SpannableString;
import android.text.SpannableStringBuilder;
import android.text.Spanned;

import androidx.annotation.ColorRes;

import org.jni_zero.CalledByNative;

import org.chromium.components.infobars.ConfirmInfoBar;
import org.chromium.components.infobars.InfoBarLayout;
import org.chromium.ui.text.NoUnderlineClickableSpan;

/**
 * An infobar to present a Safety Tip. This is a thin vineer over standard ConfirmInfoBar to provide
 * a description as well as a title.
 */
public class SafetyTipInfoBar extends ConfirmInfoBar {
    private String mDescription;
    private String mLearnMoreLinkText;

    /**
     * Creates and begins the process for showing a SafetyTipInfoBar.  This constructor is similar
     * to ConfirmInfoBar's create(), adding a description.
     *
     * @param iconId ID corresponding to the icon that will be shown for the infobar.
     * @param iconBitmap Bitmap to use if there is no equivalent Java resource for iconId.
     * @param message Title of message to display to the user indicating what the infobar is for.
     *                This should be 'title', but we're keeping consistency with ConfirmInfoBar.
     * @param linkText Link text to display in addition to the message.
     * @param buttonOk String to display on the OK button.
     * @param buttonCancel String to display on the Cancel button.
     * @param description String to display below the "message" title.
     */
    @CalledByNative
    private static ConfirmInfoBar create(
            int iconId,
            Bitmap iconBitmap,
            String message,
            String linkText,
            String buttonOk,
            String buttonCancel,
            String description) {
        return new SafetyTipInfoBar(
                iconId, 0, iconBitmap, message, linkText, buttonOk, buttonCancel, description);
    }

    private SafetyTipInfoBar(
            int iconDrawableId,
            @ColorRes int iconTintId,
            Bitmap iconBitmap,
            String message,
            String linkText,
            String primaryButtonText,
            String secondaryButtonText,
            String description) {
        super(
                iconDrawableId,
                iconTintId,
                iconBitmap,
                message,
                null,
                primaryButtonText,
                secondaryButtonText);
        mDescription = description;
        mLearnMoreLinkText = linkText;
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        super.createContent(layout);

        SpannableStringBuilder descriptionMessage = new SpannableStringBuilder(mDescription);
        if (mLearnMoreLinkText != null && !mLearnMoreLinkText.isEmpty()) {
            SpannableString link = new SpannableString(mLearnMoreLinkText);
            link.setSpan(
                    new NoUnderlineClickableSpan(layout.getContext(), view -> onLinkClicked()),
                    0,
                    link.length(),
                    Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
            descriptionMessage.append(" ").append(link);
        }
        layout.getMessageLayout().addDescription(descriptionMessage);
    }
}
