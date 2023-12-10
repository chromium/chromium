// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.graphics.Bitmap;

import androidx.annotation.ColorRes;

import org.jni_zero.CalledByNative;

import org.chromium.components.infobars.ConfirmInfoBar;
import org.chromium.components.infobars.InfoBarLayout;

/**
 * An infobar to disclose known monitoring to the user. This is a thin veneer over
 * standard ConfirmInfoBar to provide a description as well as a title.
 */
public class KnownInterceptionDisclosureInfoBar extends ConfirmInfoBar {
    private String mDescription;

    /**
     * Creates and begins the process for showing a KnownInterceptionDisclosureInfoBar.
     * This constructor is similar to ConfirmInfoBar's create(), adding a description.
     *
     * @param iconId ID corresponding to the icon that will be shown for the infobar.
     * @param iconBitmap Bitmap to use if there is no equivalent Java resource for iconId.
     * @param message Title of message to display to the user indicating what the infobar is for.
     *                This should be 'title', but we're keeping consistency with ConfirmInfoBar.
     * @param linkText Link text to display in addition to the message.
     * @param buttonOk String to display on the OK button.
     * @param description String to display below the "message" title.
     */
    @CalledByNative
    private static ConfirmInfoBar create(
            int iconId,
            Bitmap iconBitmap,
            String message,
            String linkText,
            String buttonOk,
            String description) {
        return new KnownInterceptionDisclosureInfoBar(
                iconId, 0, iconBitmap, message, linkText, buttonOk, description);
    }

    private KnownInterceptionDisclosureInfoBar(
            int iconDrawableId,
            @ColorRes int iconTintId,
            Bitmap iconBitmap,
            String message,
            String linkText,
            String primaryButtonText,
            String description) {
        super(iconDrawableId, iconTintId, iconBitmap, message, linkText, primaryButtonText, "");
        mDescription = description;
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        super.createContent(layout);
        layout.getMessageLayout().addDescription(mDescription);
    }
}
