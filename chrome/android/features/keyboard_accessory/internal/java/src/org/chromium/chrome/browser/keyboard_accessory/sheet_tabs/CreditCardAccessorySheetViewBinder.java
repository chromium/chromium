// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.DrawableRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.autofill.AutofillUiUtils;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.UserInfoField;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabModel.AccessorySheetDataPiece;
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
            // If the icon url is present, fetch the bitmap from the PersonalDataManager. In
            // the event that the bitmap is not present in the PersonalDataManager, fall back to the
            // icon corresponding to the `mOrigin`.
            Bitmap iconBitmap = null;
            Resources res = view.getContext().getResources();
            if (info.getIconUrl() != null && info.getIconUrl().isValid()) {
                iconBitmap =
                        PersonalDataManager.getInstance().getCustomImageForAutofillSuggestionIfAvailable(
                                AutofillUiUtils.getCCIconURLWithParams(info.getIconUrl(),
                                        res.getDimensionPixelSize(
                                                R.dimen.keyboard_accessory_bar_item_cc_icon_width),
                                        res.getDimensionPixelSize(
                                                R.dimen.keyboard_accessory_suggestion_icon_size)));
            }
            if (iconBitmap != null) {
                view.setIcon(new BitmapDrawable(res, iconBitmap));
            } else {
                view.setIcon(AppCompatResources.getDrawable(
                        view.getContext(), getDrawableForOrigin(info.getOrigin())));
            }
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
                case "troyCC":
                    return R.drawable.troy_card;
                case "unionPayCC":
                    return R.drawable.unionpay_card;
                case "visaCC":
                    return R.drawable.visa_card;
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

    static void initializeView(RecyclerView view, AccessorySheetTabModel model) {
        view.setAdapter(new RecyclerViewAdapter<>(
                new SimpleRecyclerViewMcp<>(model, AccessorySheetDataPiece::getType,
                        AccessorySheetTabViewBinder.ElementViewHolder::bind),
                CreditCardAccessorySheetViewBinder::create));
        view.addItemDecoration(new DynamicInfoViewBottomSpacer(CreditCardAccessoryInfoView.class));
        view.addItemDecoration(new DynamicInfoViewBottomSpacer(PromoCodeAccessoryInfoView.class));
    }
}
