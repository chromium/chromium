// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.graphics.Bitmap;

import androidx.annotation.ColorRes;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.ResourceId;

/**
 * An infobar to present a Safety Tip. This is a thin vineer over standard ConfirmInfoBar to provide
 * a description as well as a title.
 */
public class SafetyTipInfoBar extends ConfirmInfoBar {
    private static final String TAG = "SafetyTipInfoBar";
    private String mDescription;

    /**
     * Creates and begins the process for showing a SafetyTipInfoBar.  This constructor is similar
     * to ConfirmInfoBar's create(), adding a description.
     *
     * @param enumeratedIconId ID corresponding to the icon that will be shown for the infobar.
     *                         The ID must have been mapped using the ResourceMapper class before
     *                         passing it to this function.
     * @param iconBitmap Bitmap to use if there is no equivalent Java resource for
     *                   enumeratedIconId.
     * @param message Title of message to display to the user indicating what the infobar is for.
     *                This should be 'title', but we're keeping consistency with ConfirmInfoBar.
     * @param linkText Link text to display in addition to the message.
     * @param buttonOk String to display on the OK button.
     * @param buttonCancel String to display on the Cancel button.
     * @param description String to display below the "message" title.
     */
    @CalledByNative
    private static ConfirmInfoBar create(int enumeratedIconId, Bitmap iconBitmap, String message,
            String linkText, String buttonOk, String buttonCancel, String description) {
        int drawableId = ResourceId.mapToDrawableId(enumeratedIconId);

        return new SafetyTipInfoBar(
                drawableId, 0, iconBitmap, message, linkText, buttonOk, buttonCancel, description);
    }

    private SafetyTipInfoBar(int iconDrawableId, @ColorRes int iconTintId, Bitmap iconBitmap,
            String message, String linkText, String primaryButtonText, String secondaryButtonText,
            String description) {
        super(iconDrawableId, iconTintId, iconBitmap, message, linkText, primaryButtonText,
                secondaryButtonText);
        mDescription = description;
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        super.createContent(layout);
        layout.getMessageLayout().addDescription(mDescription);
    }
}
