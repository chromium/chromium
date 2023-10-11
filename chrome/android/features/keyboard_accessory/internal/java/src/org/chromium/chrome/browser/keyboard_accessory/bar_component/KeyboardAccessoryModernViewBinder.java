// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import static org.chromium.chrome.browser.autofill.AutofillUiUtils.getCardIcon;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryIPHUtils.hasShownAnyAutofillIphBefore;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryIPHUtils.showHelpBubble;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.HAS_SUGGESTIONS;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.OBFUSCATED_CHILD_AT_CALLBACK;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SHOW_SWIPING_IPH;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.TraceEvent;
import org.chromium.chrome.browser.autofill.AutofillUiUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.AutofillBarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SheetOpenerBarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryViewBinder.BarItemViewHolder;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.Action;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.PopupItemId;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.RectProvider;

/**
 * Observes {@link KeyboardAccessoryProperties} changes (like a newly available tab) and triggers
 * the {@link KeyboardAccessoryViewBinder} which will modify the view accordingly.
 */
class KeyboardAccessoryModernViewBinder {
    static BarItemViewHolder create(ViewGroup parent, @BarItem.Type int viewType) {
        switch (viewType) {
            case BarItem.Type.SUGGESTION:
                return new BarItemChipViewHolder(parent);
            case BarItem.Type.TAB_LAYOUT:
                return new SheetOpenerViewHolder(parent);
            case BarItem.Type.ACTION_BUTTON:
                return new KeyboardAccessoryViewBinder.BarItemTextViewHolder(
                        parent, R.layout.keyboard_accessory_action_modern);
            case BarItem.Type.ACTION_CHIP:
                return new BarItemActionChipViewHolder(parent);
        }
        return KeyboardAccessoryViewBinder.create(parent, viewType);
    }

    static class BarItemChipViewHolder extends BarItemViewHolder<AutofillBarItem, ChipView> {
        private final View mRootViewForIPH;

        BarItemChipViewHolder(ViewGroup parent) {
            super(parent, R.layout.keyboard_accessory_suggestion);
            mRootViewForIPH = parent.getRootView();
        }

        @Override
        protected void bind(AutofillBarItem item, ChipView chipView) {
            TraceEvent.begin("BarItemChipViewHolder#bind");
            int iconId = item.getSuggestion().getIconId();
            if (item.getFeatureForIPH() != null) {
                if (item.getFeatureForIPH().equals(
                            FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_OFFER_FEATURE)) {
                    if (iconId != 0) {
                        showHelpBubble(item.getFeatureForIPH(), chipView.getStartIconViewRect(),
                                chipView.getContext(), mRootViewForIPH,
                                item.getSuggestion().getItemTag());
                    } else {
                        showHelpBubble(item.getFeatureForIPH(), chipView, mRootViewForIPH,
                                item.getSuggestion().getItemTag());
                    }
                } else {
                    showHelpBubble(item.getFeatureForIPH(), chipView, mRootViewForIPH, null);
                }
            }

            // Credit card chips never occupy the entire width of the window to allow for other
            // cards (if they exist) to be seen. Their max width is set to 85% of the window width.
            // The chip size is limited by truncating the card label.
            // TODO (crbug.com/1376691): Check if it's alright to instead show a fixed portion of
            // the following chip. This might give a more consistent user experience and allow wider
            // windows to show more information in a chip before truncating.
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_ENABLE_VIRTUAL_CARD_METADATA)
                    && ChromeFeatureList.isEnabled(
                            ChromeFeatureList.AUTOFILL_ENABLE_CARD_PRODUCT_NAME)
                    && containsCreditCardInfo(item.getSuggestion())) {
                int windowWidth =
                        chipView.getContext().getResources().getDisplayMetrics().widthPixels;
                chipView.setMaxWidth((int) (windowWidth * 0.85));
            } else {
                // For other data types, there is no limit on width.
                chipView.setMaxWidth(Integer.MAX_VALUE);
            }

            // When chips are recycled, the constraint on primary text width (that is applied on
            // long credit card suggestions) can persist. Reset such constraints.
            chipView.getPrimaryTextView().setMaxWidth(Integer.MAX_VALUE);
            chipView.getPrimaryTextView().setEllipsize(null);

            chipView.getPrimaryTextView().setText(item.getSuggestion().getLabel());
            if (item.getSuggestion().getItemTag() != null
                    && !item.getSuggestion().getItemTag().isEmpty()) {
                chipView.getPrimaryTextView().setContentDescription(
                        item.getSuggestion().getLabel() + " " + item.getSuggestion().getItemTag());
            } else {
                chipView.getPrimaryTextView().setContentDescription(
                        item.getSuggestion().getLabel());
            }
            chipView.getSecondaryTextView().setText(item.getSuggestion().getSublabel());
            chipView.getSecondaryTextView().setVisibility(
                    item.getSuggestion().getSublabel().isEmpty() ? View.GONE : View.VISIBLE);
            KeyboardAccessoryData.Action action = item.getAction();
            assert action != null : "Tried to bind item without action. Chose a wrong ViewHolder?";
            chipView.setOnClickListener(view -> {
                item.maybeEmitEventForIPH();
                action.getCallback().onResult(action);
            });
            if (action.getLongPressCallback() != null) {
                chipView.setOnLongClickListener(view -> {
                    action.getLongPressCallback().onResult(action);
                    return true; // Click event consumed!
                });
            }
            chipView.setIcon(
                    getCardIcon(chipView.getContext(), item.getSuggestion().getCustomIconUrl(),
                            iconId, AutofillUiUtils.CardIconSize.SMALL,
                            /* showCustomIcon= */ true),
                    /* tintWithTextColor= */ false);
            TraceEvent.end("BarItemChipViewHolder#bind");
        }
    }

    static class BarItemActionChipViewHolder extends BarItemViewHolder<BarItem, ChipView> {
        BarItemActionChipViewHolder(ViewGroup parent) {
            super(parent, R.layout.keyboard_accessory_suggestion);
        }

        @Override
        protected void bind(BarItem item, ChipView chipView) {
            Action action = item.getAction();
            chipView.getPrimaryTextView().setText(item.getCaptionId());
            chipView.setOnClickListener(view -> action.getCallback().onResult(action));
        }
    }

    static class SheetOpenerViewHolder extends BarItemViewHolder<SheetOpenerBarItem, View> {
        private SheetOpenerBarItem mSheetOpenerItem;
        private View mView;

        SheetOpenerViewHolder(ViewGroup parent) {
            super(parent, R.layout.keyboard_accessory_buttons);
        }

        @Override
        protected void bind(SheetOpenerBarItem sheetOpenerItem, View view) {
            mSheetOpenerItem = sheetOpenerItem;
            mView = view;
            sheetOpenerItem.notifyAboutViewCreation(view);
        }

        @Override
        protected void recycle() {
            mSheetOpenerItem.notifyAboutViewDestruction(mView);
        }
    }

    static void bind(PropertyModel model, KeyboardAccessoryView view, PropertyKey propertyKey) {
        assert view instanceof KeyboardAccessoryModernView;
        KeyboardAccessoryModernView modernView = (KeyboardAccessoryModernView) view;
        boolean wasBound = KeyboardAccessoryViewBinder.bindInternal(model, modernView, propertyKey);
        if (propertyKey == OBFUSCATED_CHILD_AT_CALLBACK) {
            modernView.setObfuscatedLastChildAt(model.get(OBFUSCATED_CHILD_AT_CALLBACK));
        } else if (propertyKey == SHOW_SWIPING_IPH) {
            RectProvider swipingIphRectProvider = modernView.getSwipingIphRect();
            if (model.get(SHOW_SWIPING_IPH) && swipingIphRectProvider != null
                    && hasShownAnyAutofillIphBefore()) {
                showHelpBubble(FeatureConstants.KEYBOARD_ACCESSORY_BAR_SWIPING_FEATURE,
                        swipingIphRectProvider, modernView.getContext(), modernView.mBarItemsView);
            }
        } else if (propertyKey == HAS_SUGGESTIONS) {
            modernView.setAccessibilityMessage(model.get(HAS_SUGGESTIONS));
        } else {
            assert wasBound : "Every possible property update needs to be handled!";
        }
    }

    private static boolean containsCreditCardInfo(AutofillSuggestion suggestion) {
        return suggestion.getPopupItemId() == PopupItemId.CREDIT_CARD_ENTRY
                || suggestion.getPopupItemId() == PopupItemId.VIRTUAL_CREDIT_CARD_ENTRY;
    }
}
