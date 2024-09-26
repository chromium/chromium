// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryIPHUtils.hasShownAnyAutofillIphBefore;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryIPHUtils.showHelpBubble;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.ANIMATION_LISTENER;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BAR_ITEMS;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BOTTOM_OFFSET_PX;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.DISABLE_ANIMATIONS_FOR_TESTING;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.HAS_SUGGESTIONS;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.OBFUSCATED_CHILD_AT_CALLBACK;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.ON_TOUCH_EVENT_CALLBACK;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SHEET_OPENER_ITEM;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SHOW_SWIPING_IPH;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SKIP_CLOSING_ANIMATION;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.VISIBLE;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.LayoutRes;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.TraceEvent;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.AutofillBarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SheetOpenerBarItem;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.Action;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.SuggestionType;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.RectProvider;

import java.util.function.Function;

/**
 * Observes {@link KeyboardAccessoryProperties} changes (like a newly available tab) and modifies
 * the view accordingly.
 */
class KeyboardAccessoryViewBinder {
    private static final float GRAYED_OUT_OPACITY_ALPHA = 0.38f;
    private static final float COMPLETE_OPACITY_ALPHA = 1.0f;

    static BarItemViewHolder create(
            KeyboardAccessoryView keyboarAccessory,
            UiConfiguration uiConfiguration,
            ViewGroup parent,
            @BarItem.Type int viewType) {
        switch (viewType) {
            case BarItem.Type.SUGGESTION:
                return new BarItemChipViewHolder(
                        parent, keyboarAccessory, uiConfiguration.suggestionDrawableFunction);
            case BarItem.Type.TAB_LAYOUT:
                return new SheetOpenerViewHolder(parent);
            case BarItem.Type.ACTION_BUTTON:
                return new BarItemTextViewHolder(parent, R.layout.keyboard_accessory_action);
            case BarItem.Type.ACTION_CHIP:
                return new BarItemActionChipViewHolder(parent);
        }
        assert false : "Action type " + viewType + " was not handled!";
        return null;
    }

    /** Generic UI Configurations that help to transform specific model data. */
    static class UiConfiguration {
        /** Converts an {@link AutofillSuggestion} to the appropriate drawable. */
        public Function<AutofillSuggestion, Drawable> suggestionDrawableFunction;
    }

    abstract static class BarItemViewHolder<T extends BarItem, V extends View>
            extends RecyclerView.ViewHolder {
        BarItemViewHolder(ViewGroup parent, @LayoutRes int layout) {
            super(LayoutInflater.from(parent.getContext()).inflate(layout, parent, false));
        }

        @SuppressWarnings("unchecked")
        void bind(BarItem barItem) {
            bind((T) barItem, (V) itemView);
        }

        /**
         * Called when the ViewHolder is bound.
         *
         * @param item The {@link BarItem} that this ViewHolder represents.
         * @param view The {@link View} that this ViewHolder binds the bar item to.
         */
        protected abstract void bind(T item, V view);

        /**
         * The opposite of {@link #bind}. Use this to free expensive resources or reset observers.
         */
        protected void recycle() {}
    }

    static class BarItemChipViewHolder extends BarItemViewHolder<AutofillBarItem, ChipView> {
        private static final float LARGE_FONT_THRESHOLD = 1.3f;
        private final View mRootViewForIPH;
        private final KeyboardAccessoryView mKeyboardAccessory;
        private final Function<AutofillSuggestion, Drawable> mSuggestionDrawableFunction;

        BarItemChipViewHolder(
                ViewGroup parent,
                KeyboardAccessoryView keyboardAccessory,
                Function<AutofillSuggestion, Drawable> suggestionDrawableFunction) {
            super(parent, selectLayoutForScale(parent.getContext()));
            mRootViewForIPH = parent.getRootView();
            mKeyboardAccessory = keyboardAccessory;
            mSuggestionDrawableFunction = suggestionDrawableFunction;
        }

        @Override
        protected void bind(AutofillBarItem item, ChipView chipView) {
            TraceEvent.begin("BarItemChipViewHolder#bind");
            int iconId = item.getSuggestion().getIconId();
            boolean isIPHShown = false;
            if (item.getFeatureForIPH() != null) {
                if (item.getFeatureForIPH()
                        .equals(FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_OFFER_FEATURE)) {
                    if (iconId != 0) {
                        isIPHShown =
                                showHelpBubble(
                                        mKeyboardAccessory.getFeatureEngagementTracker(),
                                        item.getFeatureForIPH(),
                                        chipView.getStartIconViewRect(),
                                        chipView.getContext(),
                                        mRootViewForIPH,
                                        item.getSuggestion().getItemTag());
                    } else {
                        isIPHShown =
                                showHelpBubble(
                                        mKeyboardAccessory.getFeatureEngagementTracker(),
                                        item.getFeatureForIPH(),
                                        chipView,
                                        mRootViewForIPH,
                                        item.getSuggestion().getItemTag());
                    }
                } else if (item.getFeatureForIPH()
                        .equals(
                                FeatureConstants
                                        .KEYBOARD_ACCESSORY_PLUS_ADDRESS_CREATE_SUGGESTION)) {
                    isIPHShown =
                            showHelpBubble(
                                    mKeyboardAccessory.getFeatureEngagementTracker(),
                                    item.getFeatureForIPH(),
                                    chipView,
                                    mRootViewForIPH,
                                    item.getSuggestion().getIPHDescriptionText());
                } else {
                    isIPHShown =
                            showHelpBubble(
                                    mKeyboardAccessory.getFeatureEngagementTracker(),
                                    item.getFeatureForIPH(),
                                    chipView,
                                    mRootViewForIPH,
                                    null);
                }
            }
            mKeyboardAccessory.setAllowClicksWhileObscured(isIPHShown);

            // Credit card or IBAN chips never occupy the entire width of the window to allow for
            // other cards or IBANs (if they exist) to be seen. Their max width is set to 85% of
            // the window width.
            // The chip size is limited by truncating the card/IBAN label.
            // TODO (crbug.com/1376691): Check if it's alright to instead show a fixed portion of
            // the following chip. This might give a more consistent user experience and allow wider
            // windows to show more information in a chip before truncating.
            if (containsIbanInfo(item.getSuggestion())
                    || (ChromeFeatureList.isEnabled(
                                    ChromeFeatureList.AUTOFILL_ENABLE_VIRTUAL_CARD_METADATA)
                            && ChromeFeatureList.isEnabled(
                                    ChromeFeatureList.AUTOFILL_ENABLE_CARD_PRODUCT_NAME)
                            && containsCreditCardInfo(item.getSuggestion()))) {
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
                chipView.getPrimaryTextView()
                        .setContentDescription(
                                item.getSuggestion().getLabel()
                                        + " "
                                        + item.getSuggestion().getItemTag());
            } else {
                chipView.getPrimaryTextView()
                        .setContentDescription(item.getSuggestion().getLabel());
            }
            chipView.getSecondaryTextView().setText(item.getSuggestion().getSublabel());
            chipView.getSecondaryTextView()
                    .setVisibility(
                            item.getSuggestion().getSublabel().isEmpty()
                                    ? View.GONE
                                    : View.VISIBLE);
            KeyboardAccessoryData.Action action = item.getAction();
            assert action != null : "Tried to bind item without action. Chose a wrong ViewHolder?";
            chipView.setOnClickListener(
                    view -> {
                        item.maybeEmitEventForIPH(mKeyboardAccessory.getFeatureEngagementTracker());
                        action.getCallback().onResult(action);
                    });
            if (action.getLongPressCallback() != null) {
                chipView.setOnLongClickListener(
                        view -> {
                            action.getLongPressCallback().onResult(action);
                            return true; // Click event consumed!
                        });
            }

            float iconAlpha;
            if (item.getSuggestion().applyDeactivatedStyle()) {
                // Disabling chipview if deactivated style is set.
                chipView.setEnabled(false);
                iconAlpha = GRAYED_OUT_OPACITY_ALPHA;
                // Restoring the chipview border post disabling it to meet the
                // required UI.
                chipView.setBorder(
                        chipView.getResources().getDimensionPixelSize(R.dimen.chip_border_width),
                        chipView.getContext().getColor(R.color.black_alpha_12));
            } else {
                chipView.setEnabled(true);
                iconAlpha = COMPLETE_OPACITY_ALPHA;
            }
            Drawable iconDrawable = mSuggestionDrawableFunction.apply(item.getSuggestion());
            if (iconDrawable != null) {
                iconDrawable.setAlpha((int) (255 * iconAlpha));
            }
            chipView.setIcon(iconDrawable, /* tintWithTextColor= */ false);

            TraceEvent.end("BarItemChipViewHolder#bind");
        }

        @LayoutRes
        private static int selectLayoutForScale(Context context) {
            if (!ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_ELEGANT_TEXT_HEIGHT)) {
                return R.layout.keyboard_accessory_suggestion;
            }
            return context.getResources().getConfiguration().fontScale >= LARGE_FONT_THRESHOLD
                    ? R.layout.keyboard_accessory_suggestion_large
                    : R.layout.keyboard_accessory_suggestion;
        }
    }

    static class BarItemTextViewHolder extends BarItemViewHolder<BarItem, TextView> {
        BarItemTextViewHolder(ViewGroup parent, @LayoutRes int layout) {
            super(parent, layout);
        }

        @Override
        public void bind(BarItem barItem, TextView textView) {
            KeyboardAccessoryData.Action action = barItem.getAction();
            assert action != null : "Tried to bind item without action. Chose a wrong ViewHolder?";
            textView.setText(barItem.getCaptionId());
            textView.setOnClickListener(view -> action.getCallback().onResult(action));
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

    /**
     * Tries to bind the given property to the given view by using the value in the given model.
     *
     * @param model A {@link PropertyModel}.
     * @param view A {@link KeyboardAccessoryView}.
     * @param propertyKey A {@link PropertyKey}.
     */
    static void bind(PropertyModel model, KeyboardAccessoryView view, PropertyKey propertyKey) {
        if (propertyKey == BAR_ITEMS) {
            // Intentionally empty. The adapter will observe changes to BAR_ITEMS.
        } else if (propertyKey == DISABLE_ANIMATIONS_FOR_TESTING) {
            if (model.get(DISABLE_ANIMATIONS_FOR_TESTING)) view.disableAnimationsForTesting();
        } else if (propertyKey == VISIBLE) {
            view.setVisible(model.get(VISIBLE));
        } else if (propertyKey == SKIP_CLOSING_ANIMATION) {
            view.setSkipClosingAnimation(model.get(SKIP_CLOSING_ANIMATION));
            if (!model.get(VISIBLE)) {
                view.setVisible(false); // Update to cancel any animation.
            }
        } else if (propertyKey == BOTTOM_OFFSET_PX) {
            view.setBottomOffset(model.get(BOTTOM_OFFSET_PX));
        } else if (propertyKey == ANIMATION_LISTENER) {
            view.setAnimationListener(model.get(ANIMATION_LISTENER));
        } else if (propertyKey == OBFUSCATED_CHILD_AT_CALLBACK) {
            view.setObfuscatedLastChildAt(model.get(OBFUSCATED_CHILD_AT_CALLBACK));
        } else if (propertyKey == ON_TOUCH_EVENT_CALLBACK) {
            view.setOnTouchEventCallback(model.get(ON_TOUCH_EVENT_CALLBACK));
        } else if (propertyKey == SHOW_SWIPING_IPH) {
            RectProvider swipingIphRectProvider = view.getSwipingIphRect();
            if (model.get(SHOW_SWIPING_IPH)
                    && swipingIphRectProvider != null
                    && hasShownAnyAutofillIphBefore(view.getFeatureEngagementTracker())) {
                boolean isIPHShown =
                        showHelpBubble(
                                view.getFeatureEngagementTracker(),
                                FeatureConstants.KEYBOARD_ACCESSORY_BAR_SWIPING_FEATURE,
                                swipingIphRectProvider,
                                view.getContext(),
                                view.mBarItemsView);
                view.setAllowClicksWhileObscured(isIPHShown);
            }
        } else if (propertyKey == HAS_SUGGESTIONS) {
            view.setAccessibilityMessage(model.get(HAS_SUGGESTIONS));
        } else if (propertyKey == SHEET_OPENER_ITEM) {
            // No binding required.
        } else {
            assert false : "Every possible property update needs to be handled!";
        }
    }

    private static boolean containsCreditCardInfo(AutofillSuggestion suggestion) {
        return suggestion.getSuggestionType() == SuggestionType.CREDIT_CARD_ENTRY
                || suggestion.getSuggestionType() == SuggestionType.VIRTUAL_CREDIT_CARD_ENTRY;
    }

    private static boolean containsIbanInfo(AutofillSuggestion suggestion) {
        return suggestion.getSuggestionType() == SuggestionType.IBAN_ENTRY;
    }
}
