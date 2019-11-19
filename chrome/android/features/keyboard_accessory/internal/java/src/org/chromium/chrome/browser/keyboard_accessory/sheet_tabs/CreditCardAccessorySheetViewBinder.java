// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import android.support.v7.content.res.AppCompatResources;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.DrawableRes;

import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.UserInfoField;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabModel.AccessorySheetDataPiece;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabViewBinder.ElementViewHolder;
import org.chromium.ui.modelutil.RecyclerViewAdapter;
import org.chromium.ui.modelutil.SimpleRecyclerViewMcp;
import org.chromium.ui.widget.ChipView;

class CreditCardAccessorySheetViewBinder {
    static ElementViewHolder create(ViewGroup parent, @AccessorySheetDataPiece.Type int viewType) {
        switch (viewType) {
            case AccessorySheetDataPiece.Type.WARNING: // Fallthrough to reuse title container.
            case AccessorySheetDataPiece.Type.TITLE:
                return new AccessorySheetTabViewBinder.TitleViewHolder(
                        parent, R.layout.keyboard_accessory_sheet_tab_title);
            case AccessorySheetDataPiece.Type.CREDIT_CARD_INFO:
                return new CreditCardInfoViewHolder(parent);
            case AccessorySheetDataPiece.Type.FOOTER_COMMAND:
                return AccessorySheetTabViewBinder.create(parent, viewType);
        }
        assert false : "Unhandled type of data piece: " + viewType;
        return null;
    }

    /**
     * View which represents a single credit card and its selectable fields.
     */
    static class CreditCardInfoViewHolder
            extends ElementViewHolder<KeyboardAccessoryData.UserInfo, CreditCardAccessoryInfoView> {
        CreditCardInfoViewHolder(ViewGroup parent) {
            super(parent, R.layout.keyboard_accessory_sheet_tab_credit_card_info);
        }

        @Override
        protected void bind(KeyboardAccessoryData.UserInfo info, CreditCardAccessoryInfoView view) {
            bindChipView(view.getCCNumber(), info.getFields().get(0));
            bindChipView(view.getExpMonth(), info.getFields().get(1));
            bindChipView(view.getExpYear(), info.getFields().get(2));
            bindChipView(view.getCardholder(), info.getFields().get(3));

            view.getExpiryGroup().setVisibility(view.getExpYear().getVisibility() == View.VISIBLE
                                    || view.getExpMonth().getVisibility() == View.VISIBLE
                            ? View.VISIBLE
                            : View.GONE);

            view.setIcon(AppCompatResources.getDrawable(
                    view.getContext(), getDrawableForOrigin(info.getOrigin())));
        }

        private static void bindChipView(ChipView chip, UserInfoField field) {
            chip.getPrimaryTextView().setText(field.getDisplayText());
            chip.getPrimaryTextView().setContentDescription(field.getA11yDescription());
            chip.setVisibility(field.getDisplayText().isEmpty() ? View.GONE : View.VISIBLE);
            if (!field.isSelectable()) {
                chip.setEnabled(false);
                return;
            }
            chip.setOnClickListener(src -> field.triggerSelection());
            chip.setClickable(true);
            chip.setEnabled(true);
        }

        private static @DrawableRes int getDrawableForOrigin(String origin) {
            switch (origin) {
                case "americanExpressCC":
                    return R.drawable.amex_card;
                case "dinersCC":
                    return R.drawable.diners_card;
                case "discoverCC":
                    return R.drawable.discover_card;
                case "eloCC":
                    return R.drawable.elo_card;
                case "jcbCC":
                    return R.drawable.jcb_card;
                case "masterCardCC":
                    return R.drawable.mc_card;
                case "mirCC":
                    return R.drawable.mir_card;
                case "unionPayCC":
                    return R.drawable.unionpay_card;
                case "visaCC":
                    return R.drawable.visa_card;
            }
            return R.drawable.infobar_autofill_cc;
        }
    }

    static void initializeView(RecyclerView view, AccessorySheetTabModel model) {
        view.setAdapter(new RecyclerViewAdapter<>(
                new SimpleRecyclerViewMcp<>(model, AccessorySheetDataPiece::getType,
                        AccessorySheetTabViewBinder.ElementViewHolder::bind),
                CreditCardAccessorySheetViewBinder::create));
        view.addItemDecoration(new DynamicInfoViewBottomSpacer(CreditCardAccessoryInfoView.class));
    }
}
