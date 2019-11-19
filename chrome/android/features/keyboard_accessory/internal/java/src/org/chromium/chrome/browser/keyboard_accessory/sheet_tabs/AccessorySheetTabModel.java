// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import android.support.v7.widget.RecyclerView;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.AccessorySheetData;
import org.chromium.ui.modelutil.ListModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * This class describes the {@link ListModel} used for keyboard accessory sheets like the
 * {@link PasswordAccessorySheetCoordinator}.
 */
class AccessorySheetTabModel extends ListModel<AccessorySheetTabModel.AccessorySheetDataPiece> {
    /**
     * The {@link AccessorySheetData} has to be mapped to single items in a {@link RecyclerView}.
     * This class allows wrapping the {@link AccessorySheetData} into small chunks that are
     * organized in a {@link ListModel}. A specific ViewHolder is defined for each piece.
     */
    static class AccessorySheetDataPiece {
        @IntDef({Type.TITLE, Type.PASSWORD_INFO, Type.ADDRESS_INFO, Type.CREDIT_CARD_INFO,
                Type.TOUCH_TO_FILL_INFO, Type.FOOTER_COMMAND, Type.WARNING})
        @Retention(RetentionPolicy.SOURCE)
        @interface Type {
            /**
             * An item in title style used to display text. Non-interactive.
             */
            int TITLE = 1;
            /**
             * A section with user credentials.
             */
            int PASSWORD_INFO = 2;
            /**
             * A section containing a user's name, address, etc.
             */
            int ADDRESS_INFO = 3;
            /**
             * A section containing a payment information.
             */
            int CREDIT_CARD_INFO = 4;
            /**
             * A section containing touch to fill information.
             */
            int TOUCH_TO_FILL_INFO = 5;
            /**
             * A command at the end of the accessory sheet tab.
             */
            int FOOTER_COMMAND = 6;
            /**
             * An optional warning to be displayed at the beginning of a sheet.
             */
            int WARNING = 7;
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
