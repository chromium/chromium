// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import static org.chromium.chrome.browser.autofill.AutofillUiUtils.getCardIcon;

import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.DrawableRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.autofill.AutofillUiUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.UserInfoField;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabItemsModel.AccessorySheetDataPiece;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabViewBinder.ElementViewHolder;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.ui.modelutil.RecyclerViewAdapter;
import org.chromium.ui.modelutil.SimpleRecyclerViewMcp;

class CreditCardAccessorySheetViewBinder {
    static ElementViewHolder create(ViewGroup parent, @AccessorySheetDataPiece.Type int viewType) {
        switch (viewType) {
            case AccessorySheetDataPiece.Type.WARNING: // Fallthrough to reuse title container.
            case AccessorySheetDataPiece.Type.TITLE:
                return new AccessorySheetTabViewBinder.TitleViewHolder(
                        parent, R.layout.keyboard_accessory_sheet_tab_title);
            case AccessorySheetDataPiece.Type.CREDIT_CARD_INFO:
                return new CreditCardInfoViewHolder(parent);
            case AccessorySheetDataPiece.Type.PROMO_CODE_INFO:
                return new PromoCodeInfoViewHolder(parent);
            case AccessorySheetDataPiece.Type.FOOTER_COMMAND:
                return AccessorySheetTabViewBinder.create(parent, viewType);
        }
        assert false : "Unhandled type of data piece: " + viewType;
        return null;
    }

    private static void bindChipView(ChipView chip, UserInfoField field) {
        chip.getPrimaryTextView().setText(field.getDisplayText());
        chip.getPrimaryTextView().setContentDescription(field.getA11yDescription());
        chip.setVisibility(field.getDisplayText().isEmpty() ? View.GONE : View.VISIBLE);
        if (!field.isSelectable()) {
            chip.setEnabled(false);
        } else {
            chip.setOnClickListener(src -> field.triggerSelection());
            chip.setClickable(true);
            chip.setEnabled(true);
        }
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
            bindChipView(view.getCvc(), info.getFields().get(4));

            view.getExpiryGroup().setVisibility(view.getExpYear().getVisibility() == View.VISIBLE
                                    || view.getExpMonth().getVisibility() == View.VISIBLE
                            ? View.VISIBLE
                            : View.GONE);
            view.setIcon(getCardIcon(view.getContext(), info.getIconUrl(),
                    getDrawableForOrigin(info.getOrigin()), AutofillUiUtils.CardIconSize.SMALL,
                    /* showCustomIcon= */ true));
        }

        private static @DrawableRes int getDrawableForOrigin(String origin) {
            boolean use_new_data = ChromeFeatureList.isEnabled(
                    ChromeFeatureList.AUTOFILL_ENABLE_NEW_CARD_ART_AND_NETWORK_IMAGES);

            switch (origin) {
                case "americanExpressCC":
                    return use_new_data ? R.drawable.amex_metadata_card : R.drawable.amex_card;
                case "dinersCC":
                    return use_new_data ? R.drawable.diners_metadata_card : R.drawable.diners_card;
                case "discoverCC":
                    return use_new_data ? R.drawable.discover_metadata_card
                                        : R.drawable.discover_card;
                case "eloCC":
                    return use_new_data ? R.drawable.elo_metadata_card : R.drawable.elo_card;
                case "jcbCC":
                    return use_new_data ? R.drawable.jcb_metadata_card : R.drawable.jcb_card;
                case "masterCardCC":
                    return use_new_data ? R.drawable.mc_metadata_card : R.drawable.mc_card;
                case "mirCC":
                    return use_new_data ? R.drawable.mir_metadata_card : R.drawable.mir_card;
                case "troyCC":
                    return use_new_data ? R.drawable.troy_metadata_card : R.drawable.troy_card;
                case "unionPayCC":
                    return use_new_data ? R.drawable.unionpay_metadata_card
                                        : R.drawable.unionpay_card;
                case "visaCC":
                    return use_new_data ? R.drawable.visa_metadata_card : R.drawable.visa_card;
            }
            return R.drawable.infobar_autofill_cc;
        }
    }

    /**
     * View which represents a single Promo Code Offer and its fields.
     */
    static class PromoCodeInfoViewHolder
            extends ElementViewHolder<KeyboardAccessoryData.PromoCodeInfo,
                    PromoCodeAccessoryInfoView> {
        PromoCodeInfoViewHolder(ViewGroup parent) {
            super(parent, R.layout.keyboard_accessory_sheet_tab_promo_code_info);
        }

        @Override
        protected void bind(
                KeyboardAccessoryData.PromoCodeInfo info, PromoCodeAccessoryInfoView view) {
            bindChipView(view.getPromoCode(), info.getPromoCode());
            view.getDetailsText().setText(info.getDetailsText());
            view.getDetailsText().setVisibility(
                    info.getDetailsText().isEmpty() ? View.GONE : View.VISIBLE);

            view.setIcon(AppCompatResources.getDrawable(
                    view.getContext(), R.drawable.ic_logo_googleg_24dp));
        }
    }

    static void initializeView(RecyclerView view, AccessorySheetTabItemsModel model) {
        view.setAdapter(new RecyclerViewAdapter<>(
                new SimpleRecyclerViewMcp<>(model, AccessorySheetDataPiece::getType,
                        AccessorySheetTabViewBinder.ElementViewHolder::bind),
                CreditCardAccessorySheetViewBinder::create));
        view.addItemDecoration(new DynamicInfoViewBottomSpacer(CreditCardAccessoryInfoView.class));
        view.addItemDecoration(new DynamicInfoViewBottomSpacer(PromoCodeAccessoryInfoView.class));
    }
}
