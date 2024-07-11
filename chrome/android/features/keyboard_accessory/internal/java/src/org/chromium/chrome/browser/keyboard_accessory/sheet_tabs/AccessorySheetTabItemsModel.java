// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import androidx.annotation.IntDef;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.AccessorySheetData;
import org.chromium.ui.modelutil.ListModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * This class describes the {@link ListModel} used for keyboard accessory sheets like the
 * {@link PasswordAccessorySheetCoordinator}.
 */
class AccessorySheetTabItemsModel
        extends ListModel<AccessorySheetTabItemsModel.AccessorySheetDataPiece> {
    /**
     * The {@link AccessorySheetData} has to be mapped to single items in a {@link RecyclerView}.
     * This class allows wrapping the {@link AccessorySheetData} into small chunks that are
     * organized in a {@link ListModel}. A specific ViewHolder is defined for each piece.
     */
    static class AccessorySheetDataPiece {
        @IntDef({
            Type.TITLE,
            Type.PASSWORD_INFO,
            Type.ADDRESS_INFO,
            Type.CREDIT_CARD_INFO,
            Type.TOUCH_TO_FILL_INFO,
            Type.FOOTER_COMMAND,
            Type.WARNING,
            Type.OPTION_TOGGLE,
            Type.PROMO_CODE_INFO,
            Type.IBAN_INFO,
            Type.PASSKEY_SECTION,
            Type.PLUS_ADDRESS_SECTION
        })
        @Retention(RetentionPolicy.SOURCE)
        @interface Type {
            /** An item in title style used to display text. Non-interactive. */
            int TITLE = 1;

            /** A section with user credentials. */
            int PASSWORD_INFO = 2;

            /** A section containing a user's name, address, etc. */
            int ADDRESS_INFO = 3;

            /** A section containing a payment information. */
            int CREDIT_CARD_INFO = 4;

            /** A section containing touch to fill information. */
            int TOUCH_TO_FILL_INFO = 5;

            /** A command at the end of the accessory sheet tab. */
            int FOOTER_COMMAND = 6;

            /** An optional warning to be displayed at the beginning of a sheet. */
            int WARNING = 7;

            /**
             * An optional toggle to be displayed at the beginning of a sheet. Used for example
             * to allow the user to enable password saving for a website for which saving was
             * previously disabled.
             */
            int OPTION_TOGGLE = 8;

            /** A section containing a promo code info. */
            int PROMO_CODE_INFO = 9;

            /** A section containing an IBAN info. */
            int IBAN_INFO = 10;

            /** A section containing a passkey. */
            int PASSKEY_SECTION = 11;

            /** A section containing a plus address info. */
            int PLUS_ADDRESS_SECTION = 12;
        }

        private Object mDataPiece;
        private @Type int mType;

        AccessorySheetDataPiece(Object dataPiece, @Type int type) {
            mDataPiece = dataPiece;
            mType = type;
        }

        static int getType(AccessorySheetDataPiece accessorySheetDataPiece) {
            return accessorySheetDataPiece.mType;
        }

        Object getDataPiece() {
            return mDataPiece;
        }
    }
}
