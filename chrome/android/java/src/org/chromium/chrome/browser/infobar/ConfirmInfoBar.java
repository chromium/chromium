// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.graphics.Bitmap;

import androidx.annotation.ColorRes;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.ResourceId;

/**
 * An infobar that presents the user with several buttons.
 *
 * TODO(newt): merge this into InfoBar.java.
 */
public class ConfirmInfoBar extends InfoBar {
    /** Text shown on the primary button, e.g. "OK". */
    private final String mPrimaryButtonText;

    /** Text shown on the secondary button, e.g. "Cancel".*/
    private final String mSecondaryButtonText;

    /** Text shown on the link, e.g. "Learn more". */
    private final String mLinkText;

    protected ConfirmInfoBar(int iconDrawableId, @ColorRes int iconTintId, Bitmap iconBitmap,
            String message, String linkText, String primaryButtonText, String secondaryButtonText) {
        super(iconDrawableId, iconTintId, message, iconBitmap);
        mPrimaryButtonText = primaryButtonText;
        mSecondaryButtonText = secondaryButtonText;
        mLinkText = linkText;
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        setButtons(layout, mPrimaryButtonText, mSecondaryButtonText);
        if (mLinkText != null && !mLinkText.isEmpty()) layout.appendMessageLinkText(mLinkText);
    }

    /**
     * If your custom infobar overrides this function, YOU'RE PROBABLY DOING SOMETHING WRONG.
     *
     * Adds buttons to the infobar.  This should only be overridden in cases where an infobar
     * requires adding something other than a button for its secondary View on the bottom row
     * (almost never).
     *
     * @param primaryText Text to display on the primary button.
     * @param secondaryText Text to display on the secondary button.  May be null.
     */
    protected void setButtons(InfoBarLayout layout, String primaryText, String secondaryText) {
        layout.setButtons(primaryText, secondaryText);
    }

    @Override
    public void onButtonClicked(final boolean isPrimaryButton) {
        int action = isPrimaryButton ? ActionType.OK : ActionType.CANCEL;
        onButtonClicked(action);
    }

    /**
     * Creates and begins the process for showing a ConfirmInfoBar.
     * @param enumeratedIconId ID corresponding to the icon that will be shown for the infobar.
     *                         The ID must have been mapped using the ResourceMapper class before
     *                         passing it to this function.
     * @param iconBitmap Bitmap to use if there is no equivalent Java resource for
     *                   enumeratedIconId.
     * @param message Message to display to the user indicating what the infobar is for.
     * @param linkText Link text to display in addition to the message.
     * @param buttonOk String to display on the OK button.
     * @param buttonCancel String to display on the Cancel button.
     */
    @CalledByNative
    private static ConfirmInfoBar create(int enumeratedIconId, Bitmap iconBitmap, String message,
            String linkText, String buttonOk, String buttonCancel) {
        int drawableId = ResourceId.mapToDrawableId(enumeratedIconId);

        ConfirmInfoBar infoBar = new ConfirmInfoBar(
                drawableId, 0, iconBitmap, message, linkText, buttonOk, buttonCancel);

        return infoBar;
    }
}
