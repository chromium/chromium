// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import android.view.View;
import android.view.ViewGroup;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.UserInfoField;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabModel.AccessorySheetDataPiece;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabViewBinder.ElementViewHolder;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.widget.ChipView;

/**
 * This stateless class provides methods to bind a {@link ListModel<AccessorySheetDataPiece>}
 * to the {@link RecyclerView} used as view of a tab for the address accessory sheet component.
 */
class AddressAccessorySheetViewBinder {
    static ElementViewHolder create(ViewGroup parent, @AccessorySheetDataPiece.Type int viewType) {
        switch (viewType) {
            case AccessorySheetDataPiece.Type.TITLE:
                return new AccessorySheetTabViewBinder.TitleViewHolder(
                        parent, R.layout.keyboard_accessory_sheet_tab_title);
            case AccessorySheetDataPiece.Type.ADDRESS_INFO:
                return new AddressInfoViewHolder(parent);
            case AccessorySheetDataPiece.Type.FOOTER_COMMAND:
                return AccessorySheetTabViewBinder.create(parent, viewType);
        }
        assert false : "Unhandled type of data piece: " + viewType;
        return null;
    }

    /**
     * Holds a View representing a set of address data.
     */
    static class AddressInfoViewHolder
            extends ElementViewHolder<KeyboardAccessoryData.UserInfo, AddressAccessoryInfoView> {
        AddressInfoViewHolder(ViewGroup parent) {
            super(parent, R.layout.keyboard_accessory_sheet_tab_address_info);
        }

        @Override
        protected void bind(KeyboardAccessoryData.UserInfo info, AddressAccessoryInfoView view) {
            bindChipView(view.getNameFull(), info.getFields().get(0));
            bindChipView(view.getCompanyName(), info.getFields().get(1));
            bindChipView(view.getAddressHomeLine1(), info.getFields().get(2));
            bindChipView(view.getAddressHomeLine2(), info.getFields().get(3));
            bindChipView(view.getAddressHomeZip(), info.getFields().get(4));
            bindChipView(view.getAddressHomeCity(), info.getFields().get(5));
            bindChipView(view.getAddressHomeState(), info.getFields().get(6));
            bindChipView(view.getAddressHomeCountry(), info.getFields().get(7));
            bindChipView(view.getPhoneHomeWholeNumber(), info.getFields().get(8));
            bindChipView(view.getEmailAddress(), info.getFields().get(9));
        }

        void bindChipView(ChipView chip, UserInfoField field) {
            chip.getPrimaryTextView().setText(field.getDisplayText());
            chip.getPrimaryTextView().setContentDescription(field.getA11yDescription());
            if (!field.isSelectable() || field.getDisplayText().isEmpty()) {
                chip.setVisibility(View.GONE);
                return;
            }
            chip.setVisibility(View.VISIBLE);
            chip.setOnClickListener(src -> field.triggerSelection());
            chip.setClickable(true);
            chip.setEnabled(true);
        }
    }

    static void initializeView(RecyclerView view, AccessorySheetTabModel model) {
        view.setAdapter(AddressAccessorySheetCoordinator.createAdapter(model));
        view.addItemDecoration(new DynamicInfoViewBottomSpacer(AddressAccessoryInfoView.class));
    }
}
