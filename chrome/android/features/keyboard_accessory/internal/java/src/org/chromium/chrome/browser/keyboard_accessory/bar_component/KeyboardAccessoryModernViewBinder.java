// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryIPHUtils.hasShownAnyAutofillIphBefore;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryIPHUtils.showHelpBubble;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.KEYBOARD_TOGGLE_VISIBLE;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.OBFUSCATED_CHILD_AT_CALLBACK;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SHEET_TITLE;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SHOW_KEYBOARD_CALLBACK;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SHOW_SWIPING_IPH;

import android.view.View;
import android.view.ViewGroup;

import com.google.android.material.tabs.TabLayout;

import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.AutofillBarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.TabLayoutBarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryViewBinder.BarItemViewHolder;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ChipView;
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
                return new TabItemViewHolder(parent);
            case BarItem.Type.ACTION_BUTTON:
                return new KeyboardAccessoryViewBinder.BarItemTextViewHolder(
                        parent, R.layout.keyboard_accessory_action_modern);
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
            int iconId = item.getSuggestion().getIconId();
            if (item.getFeatureForIPH() != null) {
                if (item.getFeatureForIPH().equals(
                            FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_OFFER_FEATURE)) {
                    if (iconId != 0) {
                        showHelpBubble(item.getFeatureForIPH(), chipView.getStartIconViewRect(),
                                mRootViewForIPH, item.getSuggestion().getItemTag());
                    } else {
                        showHelpBubble(item.getFeatureForIPH(), chipView, mRootViewForIPH,
                                item.getSuggestion().getItemTag());
                    }
                } else {
                    showHelpBubble(item.getFeatureForIPH(), chipView, mRootViewForIPH, null);
                }
            }
            chipView.getPrimaryTextView().setText(item.getSuggestion().getLabel());
            if (!item.getSuggestion().getItemTag().isEmpty()) {
                chipView.getPrimaryTextView().setContentDescription(
                        item.getSuggestion().getLabel() + " " + item.getSuggestion().getItemTag());
            }
            chipView.getSecondaryTextView().setText(item.getSuggestion().getSublabel());
            chipView.getSecondaryTextView().setVisibility(
                    item.getSuggestion().getSublabel().isEmpty() ? View.GONE : View.VISIBLE);
            chipView.setIcon(iconId != 0 ? iconId : ChipView.INVALID_ICON_ID, false);
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
        }
    }

    static class TabItemViewHolder extends BarItemViewHolder<TabLayoutBarItem, TabLayout> {
        private TabLayoutBarItem mTabItem;
        private TabLayout mTabLayout;

        TabItemViewHolder(ViewGroup parent) {
            super(parent, R.layout.keyboard_accessory_tabs);
        }

        @Override
        protected void bind(TabLayoutBarItem tabItem, TabLayout tabLayout) {
            mTabItem = tabItem;
            mTabLayout = tabLayout;
            tabItem.notifyAboutViewCreation(tabLayout);
        }

        @Override
        protected void recycle() {
            mTabItem.notifyAboutViewDestruction(mTabLayout);
        }
    }

    static void bind(PropertyModel model, KeyboardAccessoryView view, PropertyKey propertyKey) {
        assert view instanceof KeyboardAccessoryModernView;
        KeyboardAccessoryModernView modernView = (KeyboardAccessoryModernView) view;
        boolean wasBound = KeyboardAccessoryViewBinder.bindInternal(model, modernView, propertyKey);
        if (propertyKey == KEYBOARD_TOGGLE_VISIBLE) {
            modernView.setKeyboardToggleVisibility(model.get(KEYBOARD_TOGGLE_VISIBLE));
        } else if (propertyKey == SHOW_KEYBOARD_CALLBACK) {
            modernView.setShowKeyboardCallback(model.get(SHOW_KEYBOARD_CALLBACK));
        } else if (propertyKey == SHEET_TITLE) {
            modernView.setSheetTitle(model.get(SHEET_TITLE));
        } else if (propertyKey == OBFUSCATED_CHILD_AT_CALLBACK) {
            modernView.setObfuscatedLastChildAt(model.get(OBFUSCATED_CHILD_AT_CALLBACK));
        } else if (propertyKey == SHOW_SWIPING_IPH) {
            RectProvider swipingIphRectProvider = modernView.getSwipingIphRect();
            if (model.get(SHOW_SWIPING_IPH) && swipingIphRectProvider != null
                    && hasShownAnyAutofillIphBefore()) {
                showHelpBubble(FeatureConstants.KEYBOARD_ACCESSORY_BAR_SWIPING_FEATURE,
                        swipingIphRectProvider, modernView.mBarItemsView);
            }
        } else {
            assert wasBound : "Every possible property update needs to be handled!";
        }
    }
}
