// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.AccessorySheetData;
import org.chromium.ui.modelutil.PropertyModel;

/** This class contains the logic specific to the password accessory sheet. */
class PasswordAccessorySheetMediator extends AccessorySheetTabMediator {
    private final Callback<Boolean> mOnToggleChanged;

    PasswordAccessorySheetMediator(
            PropertyModel model,
            int tabType,
            int userInfoType,
            int manageActionToRecord,
            Callback<Boolean> onToggleChanged) {
        super(model, userInfoType, manageActionToRecord);
        mOnToggleChanged = onToggleChanged;
    }

    @Override
    public void onItemAvailable(int typeId, AccessorySheetData accessorySheetData) {
        super.onItemAvailable(typeId, accessorySheetData);
        if (accessorySheetData == null || accessorySheetData.getOptionToggle() == null) {
            // This call makes sure that the default tab icon is used when the toggle doesn't exist,
            // in case the cached icon is obsolete.
            mOnToggleChanged.onResult(true);
        }
    }

    @Override
    protected void onToggleChanged(boolean enabled) {
        mOnToggleChanged.onResult(enabled);
    }
}
