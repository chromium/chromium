// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.graphics.Bitmap;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ResourceId;

import java.util.ArrayList;
import java.util.List;

/**
 * An infobar for assisted credit card filling.
 */
public class AutofillCreditCardFillingInfoBar extends ConfirmInfoBar {
    private final List<CardDetail> mCardDetails = new ArrayList<>();

    /**
     * Creates a new instance of the infobar.
     *
     * @param nativeAutofillCreditCardFillingInfoBar The pointer to the native object for callbacks.
     * @param enumeratedIconId ID corresponding to the icon that will be shown for the InfoBar.
     *                         The ID must have been mapped using the ResourceMapper class before
     *                         passing it to this function.
     * @param iconBitmap Bitmap to use if there is no equivalent Java resource for enumeratedIconId.
     * @param message Message to display to the user indicating what the InfoBar is for.
     * @param buttonOk String to display on the OK button.
     * @param buttonCancel String to display on the Cancel button.
     */
    private AutofillCreditCardFillingInfoBar(long nativeAutofillCreditCardFillingInfoBar,
            int enumeratedIconId, Bitmap iconBitmap, String message, String buttonOk,
            String buttonCancel) {
        super(ResourceId.mapToDrawableId(enumeratedIconId), R.color.infobar_icon_drawable_color,
                iconBitmap, message, null, buttonOk, buttonCancel);
    }

    /**
     * Creates an infobar for assisted credit card filling.
     *
     * @param nativeAutofillCreditCardFillingInfoBar The pointer to the native object for callbacks.
     * @param enumeratedIconId ID corresponding to the icon that will be shown for the InfoBar.
     *                         The ID must have been mapped using the ResourceMapper class before
     *                         passing it to this function.
     * @param iconBitmap Bitmap to use if there is no equivalent Java resource for enumeratedIconId.
     * @param message Message to display to the user indicating what the InfoBar is for.
     * @param buttonOk String to display on the OK button.
     * @param buttonCancel String to display on the Cancel button.
     * @return A new instance of the infobar.
     */
    @CalledByNative
    private static AutofillCreditCardFillingInfoBar create(
            long nativeAutofillCreditCardFillingInfoBar, int enumeratedIconId, Bitmap iconBitmap,
            String message, String buttonOk, String buttonCancel) {
        return new AutofillCreditCardFillingInfoBar(nativeAutofillCreditCardFillingInfoBar,
                enumeratedIconId, iconBitmap, message, buttonOk, buttonCancel);
    }

    /**
     * Adds information to the infobar about the credit card that will be proposed for the assist.
     *
     * @param enumeratedIconId ID corresponding to the icon that will be shown for this credit card.
     *                         The ID must have been mapped using the ResourceMapper class before
     *                         passing it to this function.
     * @param label The credit card label, for example "***1234".
     * @param subLabel The credit card sub-label, for example "Exp: 06/17".
     */
    @CalledByNative
    private void addDetail(int enumeratedIconId, String label, String subLabel) {
        mCardDetails.add(new CardDetail(enumeratedIconId, label, subLabel));
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        super.createContent(layout);
        InfoBarControlLayout control = layout.addControlLayout();
        for (int i = 0; i < mCardDetails.size(); i++) {
            CardDetail detail = mCardDetails.get(i);
            control.addIcon(detail.issuerIconDrawableId, 0, detail.label, detail.subLabel);
        }
    }
}
