// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BACK_PRESS_HANDLER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplIssuerContextProperties.APPLY_ISSUER_DEACTIVATED_STYLE;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplIssuerContextProperties.ISSUER_ICON_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplIssuerContextProperties.ISSUER_LINKED;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplIssuerContextProperties.ISSUER_NAME;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplIssuerContextProperties.ISSUER_SELECTION_TEXT;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplIssuerContextProperties.ON_ISSUER_CLICK_ACTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplIssuerTosTextItemProperties.BNPL_TOS_ICON_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplIssuerTosTextItemProperties.DESCRIPTION_TEXT;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSelectionProgressHeaderProperties.BNPL_BACK_BUTTON_ENABLED;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSelectionProgressHeaderProperties.BNPL_ON_BACK_BUTTON_CLICKED;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSelectionProgressTermsProperties.APPLY_LINK_DEACTIVATED_STYLE;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSelectionProgressTermsProperties.HIDE_OPTIONS_LINK_TEXT;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSelectionProgressTermsProperties.ON_LINK_CLICK_CALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSelectionProgressTermsProperties.TERMS_TEXT_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSuggestionProperties.BNPL_ICON_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSuggestionProperties.BNPL_ITEM_COLLECTION_INFO;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSuggestionProperties.IS_ENABLED;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSuggestionProperties.ON_BNPL_CLICK_ACTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSuggestionProperties.PRIMARY_TEXT;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplSuggestionProperties.SECONDARY_TEXT;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplTosHeaderProperties.ISSUER_IMAGE_DRAWABLE_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.BnplTosHeaderProperties.ISSUER_TITLE_STRING;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ButtonProperties.ON_CLICK_ACTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ButtonProperties.TEXT_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CURRENT_SCREEN;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.APPLY_DEACTIVATED_STYLE;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.CARD_IMAGE;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.FIRST_LINE_LABEL;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.ITEM_COLLECTION_INFO;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.MAIN_TEXT;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.MAIN_TEXT_CONTENT_DESCRIPTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.MINOR_TEXT;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.ON_CREDIT_CARD_CLICK_ACTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.CreditCardSuggestionProperties.SECOND_LINE_LABEL;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ErrorDescriptionProperties.ERROR_DESCRIPTION_STRING;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.FOCUSED_VIEW_ID_FOR_ACCESSIBILITY;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.FooterProperties.OPEN_MANAGEMENT_UI_CALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.FooterProperties.OPEN_MANAGEMENT_UI_TITLE_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.FooterProperties.SCAN_CREDIT_CARD_CALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.FooterProperties.SHOULD_SHOW_SCAN_CREDIT_CARD;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.HeaderProperties.IMAGE_DRAWABLE_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.HeaderProperties.SUBTITLE_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.HeaderProperties.TITLE_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.HeaderProperties.TITLE_STRING;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.IbanProperties.IBAN_NICKNAME;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.IbanProperties.IBAN_VALUE;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.IbanProperties.ON_IBAN_CLICK_ACTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.LoyaltyCardProperties.LOYALTY_CARD_ICON;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.LoyaltyCardProperties.LOYALTY_CARD_NUMBER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.LoyaltyCardProperties.MERCHANT_NAME;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.LoyaltyCardProperties.ON_LOYALTY_CARD_CLICK_ACTION;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ProgressIconProperties.PROGRESS_CONTENT_DESCRIPTION_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.SHEET_CLOSED_DESCRIPTION_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.SHEET_CONTENT_DESCRIPTION_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.SHEET_FULL_HEIGHT_DESCRIPTION_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.SHEET_HALF_HEIGHT_DESCRIPTION_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.TermsLabelProperties.TERMS_LABEL_TEXT_ID;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.TosFooterProperties.LEGAL_MESSAGE_LINES;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.TosFooterProperties.LINK_OPENER;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.VISIBLE;

import android.text.SpannableString;
import android.text.TextPaint;
import android.text.TextUtils;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.annotation.IdRes;
import androidx.annotation.NonNull;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.browser.autofill.AutofillUiUtils;
import org.chromium.chrome.browser.touch_to_fill.common.FillableItemCollectionInfo;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.AllLoyaltyCardsItemProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.util.AttrUtils;

/**
 * Provides functions that map {@link TouchToFillPaymentMethodProperties} changes in a {@link
 * PropertyModel} to the suitable method in {@link TouchToFillPaymentMethodView}.
 */
class TouchToFillPaymentMethodViewBinder {
    static final float GRAYED_OUT_OPACITY_ALPHA = 0.38f;
    static final float COMPLETE_OPACITY_ALPHA = 1.0f;

    /**
     * The collection info is added by setting an instance of this delegate on the last text view
     * (it is important to sound naturally and mimic the default message), so that the message which
     * is built from item children gets a suffix like ", 1 of 3.". This delegate also assumes that
     * its host sets text on {@link AccessibilityNodeInfo}, which is true for {@link TextView}.
     */
    private static class TextViewCollectionInfoAccessibilityDelegate
            extends View.AccessibilityDelegate {
        private final FillableItemCollectionInfo mCollectionInfo;

        public TextViewCollectionInfoAccessibilityDelegate(
                @NonNull FillableItemCollectionInfo collectionInfo) {
            mCollectionInfo = collectionInfo;
        }

        @Override
        public void onInitializeAccessibilityNodeInfo(
                @NonNull View host, @NonNull AccessibilityNodeInfo info) {
            super.onInitializeAccessibilityNodeInfo(host, info);

            assert info.getText() != null;
            info.setContentDescription(
                    host.getContext()
                            .getString(
                                    R.string.autofill_payment_method_a11y_item_collection_info,
                                    info.getText(),
                                    mCollectionInfo.getPosition(),
                                    mCollectionInfo.getTotal()));
        }
    }

    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     *
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link TouchToFillPaymentMethodView} to update.
     * @param propertyKey The {@link PropertyKey} which changed.
     */
    static void bindTouchToFillPaymentMethodView(
            PropertyModel model, TouchToFillPaymentMethodView view, PropertyKey propertyKey) {
        if (propertyKey == DISMISS_HANDLER) {
            view.setDismissHandler(model.get(DISMISS_HANDLER));
        } else if (propertyKey == BACK_PRESS_HANDLER) {
            view.setBackPressHandler(model.get(BACK_PRESS_HANDLER));
        } else if (propertyKey == VISIBLE) {
            view.setVisible(model.get(VISIBLE));
        } else if (propertyKey == SHEET_ITEMS) {
            // SHEET_ITEMS, CURRENT_SCREEN and FOCUSED_VIEW_ID_FOR_ACCESSIBILITY properties are
            // always updated together.
            view.setCurrentScreen(model.get(CURRENT_SCREEN));
            TouchToFillPaymentMethodCoordinator.setUpSheetItems(model, view);
            view.updateScreenHeight();
            // Screen readers can automatically determine the initially focused field. Modify the
            // initially focused field only if the screen was changed (i.e. after the user
            // interaction).
            if (model.get(FOCUSED_VIEW_ID_FOR_ACCESSIBILITY) != 0) {
                view.setFocusedViewIdForAccessibility(model.get(FOCUSED_VIEW_ID_FOR_ACCESSIBILITY));
            }
        } else if (propertyKey == CURRENT_SCREEN
                || propertyKey == FOCUSED_VIEW_ID_FOR_ACCESSIBILITY) {
            // Intentionally ignored.
        } else if (propertyKey == SHEET_CONTENT_DESCRIPTION_ID) {
            view.setSheetContentDescriptionId(model.get(SHEET_CONTENT_DESCRIPTION_ID));
        } else if (propertyKey == SHEET_HALF_HEIGHT_DESCRIPTION_ID) {
            view.setSheetHalfHeigthDescriptionId(model.get(SHEET_HALF_HEIGHT_DESCRIPTION_ID));
        } else if (propertyKey == SHEET_FULL_HEIGHT_DESCRIPTION_ID) {
            view.setSheetFullHeightDescriptionId(model.get(SHEET_FULL_HEIGHT_DESCRIPTION_ID));
        } else if (propertyKey == SHEET_CLOSED_DESCRIPTION_ID) {
            view.setSheetClosedDescriptionId(model.get(SHEET_CLOSED_DESCRIPTION_ID));
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    private TouchToFillPaymentMethodViewBinder() {}

    /**
     * Factory used to create a card item inside the ListView inside the
     * TouchToFillPaymentMethodView.
     *
     * @param parent The parent {@link ViewGroup} of the new item.
     */
    static View createCardItemView(ViewGroup parent) {
        View cardItem =
                LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.touch_to_fill_credit_card_sheet_item, parent, false);
        AutofillUiUtils.setFilterTouchForSecurity(cardItem);
        return cardItem;
    }

    /**
     * Factory used to create an IBAN item inside the ListView inside the
     * TouchToFillPaymentMethodView.
     *
     * @param parent The parent {@link ViewGroup} of the new item.
     */
    static View createIbanItemView(ViewGroup parent) {
        View ibanItem =
                LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.touch_to_fill_iban_sheet_item, parent, false);
        AutofillUiUtils.setFilterTouchForSecurity(ibanItem);
        return ibanItem;
    }

    static View createLoyaltyCardItemView(ViewGroup parent) {
        View loyaltyCardItem =
                LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.touch_to_fill_loyalty_card_sheet_item, parent, false);
        AutofillUiUtils.setFilterTouchForSecurity(loyaltyCardItem);
        return loyaltyCardItem;
    }

    static View createBnplIssuerTosItemView(ViewGroup parent) {
        View bnplIssuerTosItem =
                LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.touch_to_fill_bnpl_tos_sheet_item, parent, false);
        AutofillUiUtils.setFilterTouchForSecurity(bnplIssuerTosItem);
        return bnplIssuerTosItem;
    }

    /** Binds the item view to the model properties. */
    static void bindCardItemView(PropertyModel model, View view, PropertyKey propertyKey) {
        TextView mainText = view.findViewById(R.id.main_text);
        TextView minorText = view.findViewById(R.id.minor_text);
        ImageView icon = view.findViewById(R.id.favicon);
        TextView firstLineLabel = view.findViewById(R.id.first_line_label);
        TextView secondLineLabel = view.findViewById(R.id.second_line_label);
        // If card benefits are displayed on the first line, the second line will show
        // primary label with the expiration date or the virtual card status.
        TextView primaryLabel = firstLineLabel;
        if (!TextUtils.isEmpty(model.get(SECOND_LINE_LABEL))) {
            secondLineLabel.setVisibility(View.VISIBLE);
            primaryLabel = secondLineLabel;
        } else if (secondLineLabel != null) {
            secondLineLabel.setVisibility(View.GONE);
        }
        if (propertyKey == CARD_IMAGE) {
            icon.setImageDrawable(model.get(CARD_IMAGE));
        } else if (propertyKey == MAIN_TEXT) {
            mainText.setText(model.get(MAIN_TEXT));
        } else if (propertyKey == MAIN_TEXT_CONTENT_DESCRIPTION) {
            mainText.setContentDescription(model.get(MAIN_TEXT_CONTENT_DESCRIPTION));
        } else if (propertyKey == MINOR_TEXT) {
            minorText.setText(model.get(MINOR_TEXT));
        } else if (propertyKey == FIRST_LINE_LABEL) {
            firstLineLabel.setText(model.get(FIRST_LINE_LABEL));
        } else if (propertyKey == SECOND_LINE_LABEL) {
            secondLineLabel.setText(model.get(SECOND_LINE_LABEL));
        } else if (propertyKey == ON_CREDIT_CARD_CLICK_ACTION) {
            view.setOnClickListener(unusedView -> model.get(ON_CREDIT_CARD_CLICK_ACTION).run());
        } else if (propertyKey == ITEM_COLLECTION_INFO) {
            FillableItemCollectionInfo collectionInfo = model.get(ITEM_COLLECTION_INFO);
            if (collectionInfo != null) {
                primaryLabel.setAccessibilityDelegate(
                        new TextViewCollectionInfoAccessibilityDelegate(collectionInfo));
            }
        } else if (propertyKey == APPLY_DEACTIVATED_STYLE) {
            if (model.get(APPLY_DEACTIVATED_STYLE)) {
                view.setEnabled(false);
                // When merchants have opted out of virtual cards, we convey it
                // via a message in primary label. Since this message is
                // important, we remove the max lines limit to avoid truncation.
                primaryLabel.setMaxLines(Integer.MAX_VALUE);
                mainText.setTextAppearance(R.style.TextAppearance_TextMedium_Disabled);
                minorText.setTextAppearance(R.style.TextAppearance_TextMedium_Disabled);
                icon.setAlpha(GRAYED_OUT_OPACITY_ALPHA);
            } else {
                view.setEnabled(true);
                primaryLabel.setMaxLines(1);
                mainText.setTextAppearance(R.style.TextAppearance_TextMedium_Primary);
                minorText.setTextAppearance(R.style.TextAppearance_TextMedium_Primary);
                icon.setAlpha(COMPLETE_OPACITY_ALPHA);
            }

        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    static void bindIbanItemView(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == IBAN_VALUE) {
            if (model.get(IBAN_NICKNAME).isEmpty()) {
                TextView ibanPrimaryText = view.findViewById(R.id.iban_primary);
                ibanPrimaryText.setText(model.get(IBAN_VALUE));
                ibanPrimaryText.setTextAppearance(R.style.TextAppearance_TextLarge_Primary);
            } else {
                TextView ibanSecondaryText = view.findViewById(R.id.iban_secondary);
                ibanSecondaryText.setText(model.get(IBAN_VALUE));
                ibanSecondaryText.setVisibility(View.VISIBLE);
            }
        } else if (propertyKey == IBAN_NICKNAME) {
            if (!model.get(IBAN_NICKNAME).isEmpty()) {
                TextView ibanPrimaryText = view.findViewById(R.id.iban_primary);
                ibanPrimaryText.setText(model.get(IBAN_NICKNAME));
                ibanPrimaryText.setVisibility(View.VISIBLE);
            }
        } else if (propertyKey == ON_IBAN_CLICK_ACTION) {
            view.setOnClickListener(unusedView -> model.get(ON_IBAN_CLICK_ACTION).run());
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    static void bindLoyaltyCardItemView(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == LOYALTY_CARD_NUMBER) {
            TextView loyaltyCardNumber = view.findViewById(R.id.loyalty_card_number);
            loyaltyCardNumber.setText(model.get(LOYALTY_CARD_NUMBER));
            loyaltyCardNumber.setTextAppearance(R.style.TextAppearance_TextLarge_Primary);
        } else if (propertyKey == MERCHANT_NAME) {
            TextView merchantName = view.findViewById(R.id.merchant_name);
            merchantName.setText(model.get(MERCHANT_NAME));
            merchantName.setVisibility(View.VISIBLE);
        } else if (propertyKey == LOYALTY_CARD_ICON) {
            ImageView loyaltyCardIcon = view.findViewById(R.id.loyalty_card_icon);
            loyaltyCardIcon.setImageDrawable(model.get(LOYALTY_CARD_ICON));
        } else if (propertyKey == ON_LOYALTY_CARD_CLICK_ACTION) {
            view.setOnClickListener(unusedView -> model.get(ON_LOYALTY_CARD_CLICK_ACTION).run());
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    static View createAllLoyaltyCardsItemView(ViewGroup parent) {
        View view =
                LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.touch_to_fill_all_loyalty_cards_item, parent, false);
        AutofillUiUtils.setFilterTouchForSecurity(view);
        return view;
    }

    static void bindAllLoyaltyCardsItemView(
            PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == AllLoyaltyCardsItemProperties.ON_CLICK_ACTION) {
            view.setOnClickListener(
                    unusedView -> model.get(AllLoyaltyCardsItemProperties.ON_CLICK_ACTION).run());
        } else {
            assert false : "Unhandled update to property: " + propertyKey;
        }
    }

    /**
     * Factory used to create a new header inside the ListView inside the {@link
     * TouchToFillPaymentMethodView}.
     *
     * @param parent The parent {@link ViewGroup} of the new item.
     */
    static View createHeaderItemView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.touch_to_fill_payment_method_header_item, parent, false);
    }

    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     *
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link View} of the header to update.
     * @param key The {@link PropertyKey} which changed.
     */
    static void bindHeaderView(PropertyModel model, View view, PropertyKey propertyKey) {
        ImageView sheetHeaderImage = view.findViewById(R.id.branding_icon);
        TextView sheetHeaderTitle = view.findViewById(R.id.touch_to_fill_sheet_title);
        TextView sheetHeaderSubtitle = view.findViewById(R.id.touch_to_fill_sheet_subtitle);

        if (propertyKey == IMAGE_DRAWABLE_ID) {
            sheetHeaderImage.setImageDrawable(
                    AppCompatResources.getDrawable(
                            view.getContext(), model.get(IMAGE_DRAWABLE_ID)));
        } else if (propertyKey == TITLE_ID) {
            sheetHeaderTitle.setText(view.getContext().getString(model.get(TITLE_ID)));
        } else if (propertyKey == SUBTITLE_ID) {
            sheetHeaderSubtitle.setVisibility(View.VISIBLE);
            sheetHeaderSubtitle.setText(view.getContext().getString(model.get(SUBTITLE_ID)));
        } else if (propertyKey == TITLE_STRING) {
            sheetHeaderTitle.setText(model.get(TITLE_STRING));
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    /**
     * Factory used to create a new BNPL ToS header inside the ListView inside the {@link
     * TouchToFillPaymentMethodView}.
     *
     * @param parent The parent {@link ViewGroup} of the new item.
     */
    static View createBnplTosHeaderView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.touch_to_fill_bnpl_tos_header_item, parent, false);
    }

    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     *
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link View} of the header to update.
     * @param key The {@link PropertyKey} which changed.
     */
    static void bindBnplTosHeaderView(PropertyModel model, View view, PropertyKey propertyKey) {
        ImageView sheetHeaderImage = view.findViewById(R.id.bnpl_tos_branding_icon);
        TextView sheetHeaderTitle = view.findViewById(R.id.bnpl_tos_title);

        if (propertyKey == ISSUER_IMAGE_DRAWABLE_ID) {
            sheetHeaderImage.setImageDrawable(
                    AppCompatResources.getDrawable(
                            view.getContext(), model.get(ISSUER_IMAGE_DRAWABLE_ID)));
        } else if (propertyKey == ISSUER_TITLE_STRING) {
            sheetHeaderTitle.setText(model.get(ISSUER_TITLE_STRING));
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    /**
     * Factory used to create a new BNPL header for selection and progress screens inside the
     * ListView inside the {@link TouchToFillPaymentMethodView}.
     *
     * @param parent The parent {@link ViewGroup} of the new item.
     */
    static View createBnplSelectionProgressHeaderItemView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(
                        R.layout.touch_to_fill_bnpl_selection_and_progress_screen_header_item,
                        parent,
                        false);
    }

    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     *
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link View} of the header to update.
     * @param key The {@link PropertyKey} which changed.
     */
    static void bindBnplSelectionProgressHeaderView(
            PropertyModel model, View view, PropertyKey propertyKey) {
        ImageView backButton = view.findViewById(R.id.bnpl_header_back_button);

        if (propertyKey == BNPL_BACK_BUTTON_ENABLED) {
            final boolean isEnabled = model.get(BNPL_BACK_BUTTON_ENABLED);
            backButton.setEnabled(isEnabled);
            backButton.setAlpha(isEnabled ? COMPLETE_OPACITY_ALPHA : GRAYED_OUT_OPACITY_ALPHA);
        } else if (propertyKey == BNPL_ON_BACK_BUTTON_CLICKED) {
            backButton.setOnClickListener(
                    unusedView -> model.get(BNPL_ON_BACK_BUTTON_CLICKED).run());
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    /**
     * Factory used to create a new "Continue" or "Autofill" button that fills in data into the
     * focused field.
     *
     * @param parent The parent {@link ViewGroup} of the new item.
     */
    static Button createFillButtonView(ViewGroup parent) {
        Button buttonView =
                (Button)
                        LayoutInflater.from(parent.getContext())
                                .inflate(R.layout.touch_to_fill_fill_button, parent, false);
        AutofillUiUtils.setFilterTouchForSecurity(buttonView);
        return buttonView;
    }

    /**
     * Factory used to create a new "Cancel" button that dismiss the current screen.
     *
     * @param parent The parent {@link ViewGroup} of the new item.
     */
    static Button createTextButtonView(ViewGroup parent) {
        Button buttonView =
                (Button)
                        LayoutInflater.from(parent.getContext())
                                .inflate(R.layout.touch_to_fill_text_button, parent, false);
        AutofillUiUtils.setFilterTouchForSecurity(buttonView);
        return buttonView;
    }

    /**
     * Factory used to create a new "Wallet settings" button that redirects the user to the
     * corresponding Chrome settings page.
     *
     * @param parent The parent {@link ViewGroup} of the new item.
     */
    static Button createWalletSettingsButtonView(ViewGroup parent) {
        Button buttonView =
                (Button)
                        LayoutInflater.from(parent.getContext())
                                .inflate(
                                        R.layout.touch_to_fill_wallet_settings_button,
                                        parent,
                                        false);
        AutofillUiUtils.setFilterTouchForSecurity(buttonView);
        return buttonView;
    }

    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     *
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param button The {@link Button} from the bottom sheet to update.
     * @param key The {@link PropertyKey} which changed.
     */
    static void bindButtonView(PropertyModel model, Button button, PropertyKey propertyKey) {
        if (propertyKey == TEXT_ID) {
            button.setText(model.get(TEXT_ID));
        } else if (propertyKey == ON_CLICK_ACTION) {
            button.setOnClickListener(unusedView -> model.get(ON_CLICK_ACTION).run());
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    /**
     * Factory used to create a new label inside the TouchToFillPaymentMethodView. This label shows
     * the `Terms apply for card benefits` message when at least one of the cards has benefits.
     *
     * @param parent The parent {@link ViewGroup} of the new item.
     */
    static View createTermsLabelView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.touch_to_fill_terms_label_sheet_item, parent, false);
    }

    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     *
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link View} of the header to update.
     * @param propertyKey The {@link PropertyKey} which changed.
     */
    static void bindTermsLabelView(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == TERMS_LABEL_TEXT_ID) {
            TextView termsLabelTextView = view.findViewById(R.id.touch_to_fill_terms_label);
            termsLabelTextView.setText(model.get(TERMS_LABEL_TEXT_ID));
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    /**
     * Factory used to create a BNPL suggestion item inside the ListView inside the
     * TouchToFillPaymentMethodView.
     *
     * @param parent The parent {@link ViewGroup} of the new item.
     */
    static View createBnplItemView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.touch_to_fill_bnpl_sheet_item, parent, false);
    }

    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     *
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link View} of the header to update.
     * @param propertyKey The {@link PropertyKey} which changed.
     */
    static void bindBnplItemView(PropertyModel model, View view, PropertyKey propertyKey) {
        ImageView icon = view.findViewById(R.id.bnpl_icon);
        TextView primaryText = view.findViewById(R.id.primary_text);
        TextView secondaryText = view.findViewById(R.id.secondary_text);

        if (propertyKey == BNPL_ICON_ID) {
            int iconId = model.get(BNPL_ICON_ID);
            icon.setImageDrawable(AppCompatResources.getDrawable(view.getContext(), iconId));
        } else if (propertyKey == PRIMARY_TEXT) {
            primaryText.setText(model.get(PRIMARY_TEXT));
        } else if (propertyKey == SECONDARY_TEXT) {
            secondaryText.setText(model.get(SECONDARY_TEXT));
        } else if (propertyKey == ON_BNPL_CLICK_ACTION) {
            view.setOnClickListener(unusedView -> model.get(ON_BNPL_CLICK_ACTION).run());
        } else if (propertyKey == IS_ENABLED) {
            if (model.get(IS_ENABLED)) {
                view.setEnabled(true);
                primaryText.setTextAppearance(R.style.TextAppearance_TextMedium_Primary);
                secondaryText.setTextAppearance(R.style.TextAppearance_TextMedium_Secondary);
                icon.setAlpha(COMPLETE_OPACITY_ALPHA);
            } else {
                view.setEnabled(false);
                primaryText.setTextAppearance(R.style.TextAppearance_TextMedium_Disabled);
                secondaryText.setTextAppearance(R.style.TextAppearance_TextMedium_Disabled);
                icon.setAlpha(GRAYED_OUT_OPACITY_ALPHA);
            }
        } else if (propertyKey == BNPL_ITEM_COLLECTION_INFO) {
            FillableItemCollectionInfo collectionInfo = model.get(BNPL_ITEM_COLLECTION_INFO);
            if (collectionInfo != null) {
                secondaryText.setAccessibilityDelegate(
                        new TextViewCollectionInfoAccessibilityDelegate(collectionInfo));
            }
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    static View createBnplIssuerItemView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.touch_to_fill_bnpl_issuer_selection_sheet_item, parent, false);
    }

    static void bindBnplIssuerItemView(PropertyModel model, View view, PropertyKey propertyKey) {
        TextView issuerName = view.findViewById(R.id.bnpl_issuer_name);
        TextView selectionText = view.findViewById(R.id.bnpl_issuer_selection_text);
        TextView linkedStatusPill = view.findViewById(R.id.bnpl_issuer_linked_status_pill);
        ImageView issuerIcon = view.findViewById(R.id.bnpl_issuer_icon);

        if (propertyKey == ISSUER_NAME) {
            issuerName.setText(model.get(ISSUER_NAME));
        } else if (propertyKey == ISSUER_SELECTION_TEXT) {
            selectionText.setText(model.get(ISSUER_SELECTION_TEXT));
        } else if (propertyKey == ISSUER_ICON_ID) {
            issuerIcon.setImageDrawable(
                    AppCompatResources.getDrawable(view.getContext(), model.get(ISSUER_ICON_ID)));
        } else if (propertyKey == ISSUER_LINKED) {
            linkedStatusPill.setVisibility(model.get(ISSUER_LINKED) ? View.VISIBLE : View.GONE);
        } else if (propertyKey == ON_ISSUER_CLICK_ACTION) {
            view.setOnClickListener(v -> model.get(ON_ISSUER_CLICK_ACTION).run());
        } else if (propertyKey == APPLY_ISSUER_DEACTIVATED_STYLE) {
            if (model.get(APPLY_ISSUER_DEACTIVATED_STYLE)) {
                view.setEnabled(false);
                issuerName.setTextAppearance(R.style.TextAppearance_TextMedium_Disabled);
                selectionText.setTextAppearance(R.style.TextAppearance_TextMedium_Disabled);
                linkedStatusPill.setAlpha(GRAYED_OUT_OPACITY_ALPHA);
                issuerIcon.setAlpha(GRAYED_OUT_OPACITY_ALPHA);
            } else {
                view.setEnabled(true);
                issuerName.setTextAppearance(R.style.TextAppearance_TextMedium_Primary);
                selectionText.setTextAppearance(R.style.TextAppearance_TextMedium_Secondary);
                linkedStatusPill.setAlpha(COMPLETE_OPACITY_ALPHA);
                issuerIcon.setAlpha(COMPLETE_OPACITY_ALPHA);
            }
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    /**
     * Factory used to create a progress icon item inside the ListView inside the
     * TouchToFillPaymentMethodView.
     *
     * @param parent The parent {@link ViewGroup} of the new item.
     * @return A new {@link View} for the progress icon item.
     */
    static View createProgressIconView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.touch_to_fill_progress_icon_sheet_item, parent, false);
    }

    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     *
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link View} of the progress icon to update.
     * @param propertyKey The {@link PropertyKey} which changed.
     */
    static void bindProgressIconView(PropertyModel model, View view, PropertyKey propertyKey) {
        ProgressBar progressSpinner = view.findViewById(R.id.progress_spinner);

        if (propertyKey == PROGRESS_CONTENT_DESCRIPTION_ID) {
            progressSpinner.setContentDescription(
                    view.getContext().getString(model.get(PROGRESS_CONTENT_DESCRIPTION_ID)));
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    /**
     * Factory used to create a new error description item inside the ListView inside the
     * TouchToFillPaymentMethodView.
     *
     * @param parent The parent {@link ViewGroup} of the new item.
     * @return A new {@link View} for the error description item.
     */
    static View createErrorDescriptionView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.touch_to_fill_error_description_sheet_item, parent, false);
    }

    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     *
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link View} of the error description to update.
     * @param propertyKey The {@link PropertyKey} which changed.
     */
    static void bindErrorDescriptionView(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == ERROR_DESCRIPTION_STRING) {
            TextView errorDescriptionTextView =
                    view.findViewById(R.id.touch_to_fill_error_description);
            errorDescriptionTextView.setText(model.get(ERROR_DESCRIPTION_STRING));
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     *
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link View} of the header to update.
     * @param propertyKey The {@link PropertyKey} which changed.
     */
    static void bindBnplIssuerTosItemView(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == BNPL_TOS_ICON_ID) {
            ImageView iconView = view.findViewById(R.id.bnpl_tos_icon);
            iconView.setImageDrawable(
                    AppCompatResources.getDrawable(view.getContext(), model.get(BNPL_TOS_ICON_ID)));
        } else if (propertyKey == DESCRIPTION_TEXT) {
            TextView textView = view.findViewById(R.id.bnpl_tos_text);
            textView.setText(model.get(DESCRIPTION_TEXT), TextView.BufferType.SPANNABLE);
            textView.setMovementMethod(LinkMovementMethod.getInstance());
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    /**
     * Factory used to create a new footer inside the ListView inside the
     * TouchToFillPaymentMethodView.
     *
     * @param parent The parent {@link ViewGroup} of the new item.
     */
    static View createFooterItemView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.touch_to_fill_payment_method_footer_item, parent, false);
    }

    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link View} of the header to update.
     * @param key The {@link PropertyKey} which changed.
     */
    static void bindFooterView(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == SHOULD_SHOW_SCAN_CREDIT_CARD) {
            setScanCreditCardButton(view, model.get(SHOULD_SHOW_SCAN_CREDIT_CARD));
        } else if (propertyKey == SCAN_CREDIT_CARD_CALLBACK) {
            setCallbackForButton(view, R.id.scan_new_card, model.get(SCAN_CREDIT_CARD_CALLBACK));
        } else if (propertyKey == OPEN_MANAGEMENT_UI_TITLE_ID) {
            setShowPaymentMethodsSettingsTitle(
                    view, view.getContext().getString(model.get(OPEN_MANAGEMENT_UI_TITLE_ID)));
        } else if (propertyKey == OPEN_MANAGEMENT_UI_CALLBACK) {
            setCallbackForButton(
                    view, R.id.open_management_ui, model.get(OPEN_MANAGEMENT_UI_CALLBACK));
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    /**
     * Factory used to create a new footer inside the ListView inside the
     * TouchToFillPaymentMethodView for showing legal messages.
     *
     * @param parent The parent {@link ViewGroup} of the new item.
     */
    static View createLegalMessageItemView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.touch_to_fill_legal_message_item, parent, false);
    }

    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     *
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link View} of the header to update.
     * @param key The {@link PropertyKey} which changed.
     */
    static void bindLegalMessageItemView(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == LEGAL_MESSAGE_LINES || propertyKey == LINK_OPENER) {
            TextView textView = view.findViewById(R.id.legal_message);
            textView.setText(
                    AutofillUiUtils.getSpannableStringForLegalMessageLines(
                            textView.getContext(),
                            model.get(LEGAL_MESSAGE_LINES),
                            /* underlineLinks= */ false,
                            (String url) -> {
                                model.get(LINK_OPENER).accept(url);
                            }));
            textView.setMovementMethod(LinkMovementMethod.getInstance());
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    /**
     * Factory used to create a new BNPL terms for selection and progress screens inside the
     * ListView inside the {@link TouchToFillPaymentMethodView}.
     *
     * @param parent The parent {@link ViewGroup} of the new item.
     */
    static View createBnplSelectionProgressTermsItemView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(
                        R.layout.touch_to_fill_bnpl_selection_and_progress_screen_terms_item,
                        parent,
                        false);
    }

    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     *
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link View} of the header to update.
     * @param key The {@link PropertyKey} which changed.
     */
    static void bindBnplSelectionProgressTermsView(
            PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == TERMS_TEXT_ID) {
            TextView termsLabelTextView = view.findViewById(R.id.bnpl_terms_label);
            termsLabelTextView.setText(model.get(TERMS_TEXT_ID));
        } else if (propertyKey == HIDE_OPTIONS_LINK_TEXT
                || propertyKey == ON_LINK_CLICK_CALLBACK
                || propertyKey == APPLY_LINK_DEACTIVATED_STYLE) {
            buildOpenPaymentSettingsSpannable(model, view);
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    /**
     * Builds the entire SpannableString for the open payment settings label part of the terms from
     * the properties in the model.
     */
    private static void buildOpenPaymentSettingsSpannable(PropertyModel model, View view) {
        ClickableSpan span;
        if (model.get(APPLY_LINK_DEACTIVATED_STYLE)) {
            // For the disabled state, create a custom span 38% opacity for the link text.
            span =
                    new ClickableSpan() {
                        @Override
                        public void onClick(View widget) {
                            // This is intentionally left empty as there are no click events
                            // when disabled.
                        }

                        @Override
                        public void updateDrawState(TextPaint textPaint) {
                            // Resolves the standard link color, just like ChromeClickableSpan does.
                            int defaultColor =
                                    view.getContext()
                                            .getColor(R.color.default_text_color_link_baseline);
                            int linkColor =
                                    AttrUtils.resolveColor(
                                            view.getContext().getTheme(),
                                            R.attr.globalClickableSpanColor,
                                            defaultColor);
                            // Create the new color for the disabled link with 38% opacity.
                            int alpha = (int) (255 * GRAYED_OUT_OPACITY_ALPHA);
                            int lowOpacityColor = (linkColor & 0x00FFFFFF) | (alpha << 24);
                            textPaint.setColor(lowOpacityColor);
                            textPaint.setUnderlineText(true);
                        }
                    };
        } else {
            // For the enabled state, create a ChromeClickableSpan.
            span = new ChromeClickableSpan(view.getContext(), model.get(ON_LINK_CLICK_CALLBACK));
        }
        String rawFooterText = model.get(HIDE_OPTIONS_LINK_TEXT);
        TextView footer = view.findViewById(R.id.bnpl_open_payment_settings_label);
        SpannableString spannableFooter =
                SpanApplier.applySpans(
                        rawFooterText, new SpanApplier.SpanInfo("<link>", "</link>", span));
        footer.setText(spannableFooter);
        footer.setMovementMethod(LinkMovementMethod.getInstance());
    }

    private static void setScanCreditCardButton(View view, boolean shouldShowScanCreditCard) {
        View scanCreditCard = view.findViewById(R.id.scan_new_card);
        if (shouldShowScanCreditCard) {
            scanCreditCard.setVisibility(View.VISIBLE);
        } else {
            scanCreditCard.setVisibility(View.GONE);
            scanCreditCard.setOnClickListener(null);
        }
    }

    private static void setShowPaymentMethodsSettingsTitle(View view, String title) {
        TextView managePaymentMethodsButton = view.findViewById(R.id.open_management_ui);
        managePaymentMethodsButton.setText(title);
    }

    private static void setCallbackForButton(View view, @IdRes int buttonId, Runnable callback) {
        View buttonView = view.findViewById(buttonId);
        buttonView.setOnClickListener(unused -> callback.run());
    }
}
