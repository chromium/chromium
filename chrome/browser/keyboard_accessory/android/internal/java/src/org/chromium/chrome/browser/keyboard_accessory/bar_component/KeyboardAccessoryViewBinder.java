// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryIphUtils.hasShownAnyAutofillIphBefore;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryIphUtils.showHelpBubble;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.ANIMATE_SUGGESTIONS_FROM_TOP;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.ANIMATION_LISTENER;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BAR_ITEMS;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BAR_ITEMS_FIXED;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.DISABLE_ANIMATIONS_FOR_TESTING;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.DISMISS_ITEM;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.HAS_STICKY_LAST_ITEM;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.HAS_SUGGESTIONS;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.OBFUSCATED_CHILD_AT_CALLBACK;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.ON_TOUCH_EVENT_CALLBACK;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SHEET_OPENER_ITEM;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SHOW_SWIPING_IPH;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SKIP_CLOSING_ANIMATION;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.STYLE;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.VISIBLE;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.TextView;

import androidx.annotation.LayoutRes;
import androidx.annotation.StyleRes;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.base.TraceEvent;
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.ActionBarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.AutofillBarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.GroupBarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SheetOpenerBarItem;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.Action;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.SuggestionType;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.RectProvider;

import java.util.function.Function;

/**
 * Observes {@link KeyboardAccessoryProperties} changes (like a newly available tab) and modifies
 * the view accordingly.
 */
@NullMarked
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
            case BarItem.Type.LOYALTY_CARD_SUGGESTION:
            case BarItem.Type.HOME_AND_WORK_SUGGESTION:
            case BarItem.Type.PAYMENTS_SUGGESTION:
                return new BarItemChipViewHolder(
                        parent,
                        keyboarAccessory,
                        uiConfiguration.suggestionDrawableFunction,
                        viewType);
            case BarItem.Type.TAB_LAYOUT:
                return new SheetOpenerViewHolder(parent);
            case BarItem.Type.ACTION_BUTTON:
            case BarItem.Type.DISMISS_CHIP:
                return new BarItemTextViewHolder(parent, viewType);
            case BarItem.Type.ACTION_CHIP:
                return new BarItemActionChipViewHolder(parent);
            case BarItem.Type.GROUP:
                return new BarItemGroupViewHolder(keyboarAccessory, uiConfiguration, parent);
            default:
                throw new IllegalStateException("Action type " + viewType + " was not handled!");
        }
    }

    /** Generic UI Configurations that help to transform specific model data. */
    static class UiConfiguration {
        /** Converts an {@link AutofillSuggestion} to the appropriate drawable. */
        public final Function<@Nullable AutofillSuggestion, @Nullable Drawable>
                suggestionDrawableFunction;

        UiConfiguration(
                Function<@Nullable AutofillSuggestion, @Nullable Drawable>
                        suggestionDrawableFunction) {
            this.suggestionDrawableFunction = suggestionDrawableFunction;
        }
    }

    abstract static class BarItemViewHolder<T extends BarItem, V extends View>
            extends RecyclerView.ViewHolder {
        private static final float LARGE_FONT_THRESHOLD = 1.3f;

        BarItemViewHolder(ViewGroup parent, @LayoutRes int layout) {
            this(LayoutInflater.from(parent.getContext()).inflate(layout, parent, false));
        }

        BarItemViewHolder(View barItem) {
            super(barItem);
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

        protected static boolean useLargeChips(Context context) {
            return ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_ELEGANT_TEXT_HEIGHT)
                    && context.getResources().getConfiguration().fontScale >= LARGE_FONT_THRESHOLD;
        }
    }

    static class BarItemGroupViewHolder
            extends BarItemViewHolder<GroupBarItem, KeyboardAccessoryChipGroup> {
        private final KeyboardAccessoryView mKeyboardAccessory;
        private final UiConfiguration mUiConfiguration;
        private final ViewGroup mParent;

        BarItemGroupViewHolder(
                KeyboardAccessoryView keyboarAccessory,
                UiConfiguration uiConfiguration,
                ViewGroup parent) {
            super(new KeyboardAccessoryChipGroup(parent.getContext()));
            mKeyboardAccessory = keyboarAccessory;
            mUiConfiguration = uiConfiguration;
            mParent = parent;
        }

        @Override
        protected void bind(GroupBarItem group, KeyboardAccessoryChipGroup chipGroup) {
            chipGroup.removeAllViews();
            for (ActionBarItem item : group.getActionBarItems()) {
                BarItemViewHolder viewHolder =
                        create(mKeyboardAccessory, mUiConfiguration, mParent, item.getViewType());

                viewHolder.bind(item, viewHolder.itemView);
                chipGroup.addView(viewHolder.itemView);
            }
        }
    }

    static class BarItemChipViewHolder extends BarItemViewHolder<AutofillBarItem, ChipView> {
        private final View mRootViewForIPH;
        private final KeyboardAccessoryView mKeyboardAccessory;
        private final Function<@Nullable AutofillSuggestion, @Nullable Drawable>
                mSuggestionDrawableFunction;

        BarItemChipViewHolder(
                ViewGroup parent,
                KeyboardAccessoryView keyboardAccessory,
                Function<@Nullable AutofillSuggestion, @Nullable Drawable>
                        suggestionDrawableFunction,
                @BarItem.Type int barItemType) {
            super(
                    new ChipView(
                            parent.getContext(),
                            null,
                            0,
                            selectStyleForSuggestion(parent.getContext(), barItemType)));
            // TODO: crbug.com/385172647 - Move height parameters to the xml file once the feature
            // is launched.
            if (ChromeFeatureList.isEnabled(
                    ChromeFeatureList.AUTOFILL_ENABLE_KEYBOARD_ACCESSORY_CHIP_REDESIGN)) {
                itemView.setMinimumHeight(
                        parent.getContext()
                                .getResources()
                                .getDimensionPixelSize(
                                        R.dimen.keyboard_accessory_chip_min_height_redesign));
            }
            mRootViewForIPH = parent.getRootView();
            mKeyboardAccessory = keyboardAccessory;
            mSuggestionDrawableFunction = suggestionDrawableFunction;
        }

        @Override
        protected void bind(AutofillBarItem item, ChipView chipView) {
            TraceEvent.begin("BarItemChipViewHolder#bind");
            boolean iphShown =
                    KeyboardAccessoryIphUtils.maybeShowIph(
                            mKeyboardAccessory.getFeatureEngagementTracker(),
                            item,
                            chipView,
                            mRootViewForIPH);
            mKeyboardAccessory.setAllowClicksWhileObscured(iphShown);

            // Credit card or IBAN chips never occupy the entire width of the window to allow for
            // other cards or IBANs (if they exist) to be seen. Their max width is set to 85% of
            // the window width.
            // The chip size is limited by truncating the card/IBAN label.
            // TODO (crbug.com/1376691): Check if it's alright to instead show a fixed portion of
            // the following chip. This might give a more consistent user experience and allow wider
            // windows to show more information in a chip before truncating.
            if (containsIbanInfo(item.getSuggestion())
                    || containsCreditCardInfo(item.getSuggestion())) {
                int windowWidth =
                        chipView.getContext().getResources().getDisplayMetrics().widthPixels;
                chipView.setMaxWidth((int) (windowWidth * 0.85));
            } else {
                // When chips are recycled, the constraint on primary text width (that is applied on
                // long credit card suggestions) can persist. Reset such constraints.
                chipView.setMaxWidth(Integer.MAX_VALUE);
            }

            chipView.getPrimaryTextView().setText(item.getSuggestion().getLabel());
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
                        item.maybeEmitEventForIph(mKeyboardAccessory.getFeatureEngagementTracker());
                        action.getCallback().onResult(action);
                    });
            @Nullable Callback<Action> longPressCallback = action.getLongPressCallback();
            if (longPressCallback != null) {
                chipView.setOnLongClickListener(
                        view -> {
                            longPressCallback.onResult(action);
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
                        chipView.getContext().getColorStateList(R.color.black_alpha_12));
            } else {
                chipView.setEnabled(true);
                iconAlpha = COMPLETE_OPACITY_ALPHA;
            }
            Drawable iconDrawable = mSuggestionDrawableFunction.apply(item.getSuggestion());
            if (iconDrawable != null) {
                iconDrawable.setAlpha((int) (255 * iconAlpha));
            }
            chipView.setIconWithTint(iconDrawable, /* tintWithTextColor= */ false);

            @Nullable String voiceOver = item.getSuggestion().getVoiceOver();
            if (!TextUtils.isEmpty(voiceOver)) {
                chipView.setContentDescription(voiceOver);
            }

            TraceEvent.end("BarItemChipViewHolder#bind");
        }

        @StyleRes
        private static int selectStyleForSuggestion(
                Context context, @BarItem.Type int barItemType) {
            if (ChromeFeatureList.isEnabled(
                    ChromeFeatureList.AUTOFILL_ENABLE_KEYBOARD_ACCESSORY_CHIP_REDESIGN)) {
                switch (barItemType) {
                    case BarItem.Type.LOYALTY_CARD_SUGGESTION:
                        // Loyalty cards suggestions have round icons.
                        return useLargeChips(context)
                                ? R.style.KeyboardAccessoryLoyaltyCardLargeTwoLineChip
                                : R.style.KeyboardAccessoryLoyaltyCardTwoLineChip;
                    case BarItem.Type.HOME_AND_WORK_SUGGESTION:
                        return useLargeChips(context)
                                ? R.style.KeyboardAccessoryHomeAndWorkLargeTwoLineChip
                                : R.style.KeyboardAccessoryHomeAndWorkTwoLineChip;
                    case BarItem.Type.PAYMENTS_SUGGESTION:
                        return useLargeChips(context)
                                ? R.style.KeyboardAccessoryPaymentsLargeTwoLineChip
                                : R.style.KeyboardAccessoryPaymentsTwoLineChip;
                    case BarItem.Type.SUGGESTION:
                        return useLargeChips(context)
                                ? R.style.KeyboardAccessoryLargeTwoLineChip
                                : R.style.KeyboardAccessoryTwoLineChip;
                    case BarItem.Type.ACTION_CHIP:
                    case BarItem.Type.DISMISS_CHIP:
                    case BarItem.Type.TAB_LAYOUT:
                    case BarItem.Type.ACTION_BUTTON:
                    default:
                        assert false : "Only suggestion chips have custom styles";
                        return 0;
                }
            }
            switch (barItemType) {
                case BarItem.Type.LOYALTY_CARD_SUGGESTION:
                    // Loyalty cards suggestions have round icons.
                    return useLargeChips(context)
                            ? R.style.KeyboardAccessoryLoyaltyCardLargeChip
                            : R.style.KeyboardAccessoryLoyaltyCardChip;
                case BarItem.Type.HOME_AND_WORK_SUGGESTION:
                    return useLargeChips(context)
                            ? R.style.KeyboardAccessoryHomeAndWorkLargeChip
                            : R.style.KeyboardAccessoryHomeAndWorkChip;
                case BarItem.Type.SUGGESTION:
                case BarItem.Type.PAYMENTS_SUGGESTION:
                    return useLargeChips(context)
                            ? R.style.KeyboardAccessoryLargeChip
                            : R.style.KeyboardAccessoryChip;
                case BarItem.Type.ACTION_CHIP:
                case BarItem.Type.DISMISS_CHIP:
                case BarItem.Type.TAB_LAYOUT:
                case BarItem.Type.ACTION_BUTTON:
                default:
                    assert false : "Only suggestion chips have custom styles";
                    return 0;
            }
        }
    }

    static class BarItemTextViewHolder extends BarItemViewHolder<ActionBarItem, TextView> {
        private final @BarItem.Type int mBarItemType;

        BarItemTextViewHolder(ViewGroup parent, @BarItem.Type int barItemType) {
            super(
                    new ButtonCompat(
                            parent.getContext(),
                            selectStyleForSuggestion(parent.getContext(), barItemType)));
            mBarItemType = barItemType;
        }

        @Override
        public void bind(ActionBarItem barItem, TextView textView) {
            KeyboardAccessoryData.Action action = barItem.getAction();
            assert action != null : "Tried to bind item without action. Chose a wrong ViewHolder?";
            textView.setText(barItem.getCaptionId());
            textView.setOnClickListener(view -> action.getCallback().onResult(action));
            // Margins can be either set in XML layouts or programmatically, they can't be part of
            // the KeyboardAccessory* styles.
            applyMargins(textView);
        }

        private void applyMargins(TextView textView) {
            MarginLayoutParams params =
                    new MarginLayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.MATCH_PARENT);
            Resources resources = textView.getContext().getResources();
            switch (mBarItemType) {
                case BarItem.Type.ACTION_BUTTON:
                    if (!ChromeFeatureList.isEnabled(
                            ChromeFeatureList.AUTOFILL_ENABLE_KEYBOARD_ACCESSORY_CHIP_REDESIGN)) {
                        params.setMarginEnd(
                                resources.getDimensionPixelSize(
                                        R.dimen.keyboard_accessory_bar_item_padding));
                    }
                    break;
                case BarItem.Type.DISMISS_CHIP:
                    params.setMarginEnd(
                            resources.getDimensionPixelSize(
                                    R.dimen.keyboard_accessory_dismiss_button_margin_end));
                    break;
                default:
                    assert false : "Not a button item type: " + mBarItemType;
            }
            textView.setLayoutParams(params);
        }

        @StyleRes
        private static int selectStyleForSuggestion(
                Context context, @BarItem.Type int barItemType) {
            switch (barItemType) {
                case BarItem.Type.ACTION_BUTTON:
                    if (ChromeFeatureList.isEnabled(
                            ChromeFeatureList.AUTOFILL_ENABLE_KEYBOARD_ACCESSORY_CHIP_REDESIGN)) {
                        return useLargeChips(context)
                                ? R.style.KeyboardAccessoryLargeTwoLineActionButtonThemeOverlay
                                : R.style.KeyboardAccessoryTwoLineActionButtonThemeOverlay;
                    }
                    return R.style.KeyboardAccessoryActionButtonThemeOverlay;
                case BarItem.Type.DISMISS_CHIP:
                    return R.style.KeyboardAccessoryDismissButtonThemeOverlay;
                default:
                    assert false : "Not a button item type: " + barItemType;
                    return 0;
            }
        }
    }

    static class BarItemActionChipViewHolder extends BarItemViewHolder<ActionBarItem, ChipView> {
        BarItemActionChipViewHolder(ViewGroup parent) {
            super(new ChipView(parent.getContext(), null, 0, selectStyle(parent.getContext())));
        }

        @Override
        protected void bind(ActionBarItem item, ChipView chipView) {
            chipView.getPrimaryTextView().setText(item.getCaptionId());
            @Nullable Action action = item.getAction();
            if (action != null) {
                chipView.setOnClickListener(view -> action.getCallback().onResult(action));
            }
        }

        private static @StyleRes int selectStyle(Context context) {
            if (ChromeFeatureList.isEnabled(
                    ChromeFeatureList.AUTOFILL_ENABLE_KEYBOARD_ACCESSORY_CHIP_REDESIGN)) {
                return useLargeChips(context)
                        ? R.style.KeyboardAccessoryLargeTwoLineChip
                        : R.style.KeyboardAccessoryTwoLineChip;
            }
            return useLargeChips(context)
                    ? R.style.KeyboardAccessoryLargeChip
                    : R.style.KeyboardAccessoryChip;
        }
    }

    static class SheetOpenerViewHolder extends BarItemViewHolder<SheetOpenerBarItem, View> {
        private @MonotonicNonNull SheetOpenerBarItem mSheetOpenerItem;

        SheetOpenerViewHolder(ViewGroup parent) {
            super(parent, R.layout.keyboard_accessory_buttons);
        }

        @Override
        @EnsuresNonNull("mSheetOpenerItem")
        protected void bind(SheetOpenerBarItem sheetOpenerItem, View view) {
            mSheetOpenerItem = sheetOpenerItem;
            sheetOpenerItem.notifyAboutViewCreation(itemView);
        }

        @Override
        protected void recycle() {
            if (mSheetOpenerItem != null) {
                mSheetOpenerItem.notifyAboutViewDestruction(itemView);
            }
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
        if (propertyKey == BAR_ITEMS || propertyKey == BAR_ITEMS_FIXED) {
            // Intentionally empty. The adapter will observe changes to bar items.
        } else if (propertyKey == DISABLE_ANIMATIONS_FOR_TESTING) {
            if (model.get(DISABLE_ANIMATIONS_FOR_TESTING)) {
                view.disableAnimationsForTesting(); // IN-TEST
            }
        } else if (propertyKey == VISIBLE) {
            view.setVisible(model.get(VISIBLE));
        } else if (propertyKey == SKIP_CLOSING_ANIMATION) {
            view.setSkipClosingAnimation(model.get(SKIP_CLOSING_ANIMATION));
            if (!model.get(VISIBLE)) {
                view.setVisible(false); // Update to cancel any animation.
            }
        } else if (propertyKey == STYLE) {
            view.setStyle(model.get(STYLE));
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
                boolean isIphShown =
                        showHelpBubble(
                                view.getFeatureEngagementTracker(),
                                FeatureConstants.KEYBOARD_ACCESSORY_BAR_SWIPING_FEATURE,
                                swipingIphRectProvider,
                                view.getContext(),
                                view.mBarItemsView);
                view.setAllowClicksWhileObscured(isIphShown);
            }
        } else if (propertyKey == HAS_SUGGESTIONS) {
            view.setAccessibilityMessage(model.get(HAS_SUGGESTIONS));
        } else if (propertyKey == HAS_STICKY_LAST_ITEM) {
            view.setHasStickyLastItem(model.get(HAS_STICKY_LAST_ITEM));
        } else if (propertyKey == ANIMATE_SUGGESTIONS_FROM_TOP) {
            view.setAnimateSuggestionsFromTop(model.get(ANIMATE_SUGGESTIONS_FROM_TOP));
        } else if (propertyKey == SHEET_OPENER_ITEM || propertyKey == DISMISS_ITEM) {
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
