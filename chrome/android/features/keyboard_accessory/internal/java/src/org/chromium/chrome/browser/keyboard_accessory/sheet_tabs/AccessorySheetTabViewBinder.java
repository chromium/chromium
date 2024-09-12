// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import static org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabProperties.IS_DEFAULT_A11Y_FOCUS_REQUESTED;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabProperties.ITEMS;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabProperties.SCROLL_LISTENER;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.appcompat.widget.SwitchCompat;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabItemsModel.AccessorySheetDataPiece;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This stateless class provides methods to bind a {@link ListModel<AccessorySheetDataPiece>}
 * to the {@link RecyclerView} used as view of a tab for the accessory sheet component.
 */
class AccessorySheetTabViewBinder {
    /** Holds any View that represents a list entry. */
    abstract static class ElementViewHolder<T, V extends View> extends RecyclerView.ViewHolder {
        ElementViewHolder(ViewGroup parent, int layout) {
            super(LayoutInflater.from(parent.getContext()).inflate(layout, parent, false));
        }

        @SuppressWarnings("unchecked")
        void bind(AccessorySheetDataPiece accessorySheetDataWrapper) {
            bind((T) accessorySheetDataWrapper.getDataPiece(), (V) itemView);
        }

        protected abstract void bind(T t, V view);
    }

    /**
     * Creates an {@link ElementViewHolder} for the given |viewType|.
     *
     * @param parent A {@link android.view.ViewParent} to attach this view to.
     * @param viewType A {@link AccessorySheetDataPiece.Type} describing the view to be created.
     * @return A {@link ElementViewHolder}.
     */
    static ElementViewHolder create(ViewGroup parent, @AccessorySheetDataPiece.Type int viewType) {
        switch (viewType) {
            case AccessorySheetDataPiece.Type.FOOTER_COMMAND:
                return new FooterCommandViewHolder(parent);
            case AccessorySheetDataPiece.Type.OPTION_TOGGLE:
                return new OptionToggleViewHolder(parent);
        }
        assert false : "Unhandled type of data piece: " + viewType;
        return null;
    }

    /** Holds a Title consisting of a top divider, a text view and a bottom divider. */
    static class TitleViewHolder extends ElementViewHolder<String, TextView> {
        TitleViewHolder(ViewGroup parent) {
            super(parent, R.layout.keyboard_accessory_sheet_tab_title);
        }

        @Override
        protected void bind(String displayText, TextView titleView) {
            titleView.setText(displayText);
            titleView.setContentDescription(displayText);
        }
    }

    /** Holds a clickable {@link TextView} that represents a footer command. */
    static class FooterCommandViewHolder
            extends ElementViewHolder<KeyboardAccessoryData.FooterCommand, TextView> {
        FooterCommandViewHolder(ViewGroup parent) {
            super(parent, R.layout.password_accessory_sheet_option);
        }

        @Override
        protected void bind(KeyboardAccessoryData.FooterCommand footerCommand, TextView view) {
            view.setText(footerCommand.getDisplayText());
            view.setContentDescription(footerCommand.getDisplayText());
            view.setOnClickListener(v -> footerCommand.execute());
        }
    }

    /** Holds a title, subtitle which shows the state of the toggle and a toggle. */
    static class OptionToggleViewHolder
            extends ElementViewHolder<KeyboardAccessoryData.OptionToggle, LinearLayout> {
        OptionToggleViewHolder(ViewGroup parent) {
            super(parent, R.layout.keyboard_accessory_sheet_tab_option_toggle);
        }

        @Override
        protected void bind(KeyboardAccessoryData.OptionToggle optionToggle, LinearLayout view) {
            view.setClickable(true);
            view.setOnClickListener(
                    v -> optionToggle.getCallback().onResult(!optionToggle.isEnabled()));

            TextView titleText = view.findViewById(R.id.option_toggle_title);
            titleText.setText(optionToggle.getDisplayText());

            TextView subtitleText = view.findViewById(R.id.option_toggle_subtitle);
            subtitleText.setText(optionToggle.isEnabled() ? R.string.text_on : R.string.text_off);

            SwitchCompat switchView = view.findViewById(R.id.option_toggle_switch);
            switchView.setChecked(optionToggle.isEnabled());
            switchView.setBackground(null);
        }
    }

    static void initializeView(
            RecyclerView view, @Nullable RecyclerView.OnScrollListener scrollListener) {
        view.setLayoutManager(
                new LinearLayoutManager(view.getContext(), LinearLayoutManager.VERTICAL, false));
        view.setItemAnimator(null);
        if (scrollListener != null) view.addOnScrollListener(scrollListener);
    }

    public static void bind(
            PropertyModel model, AccessorySheetTabView view, PropertyKey propertyKey) {
        if (propertyKey == ITEMS) {
            // TODO(crbug.com/40257527): move setting adapter from initializeView() (in descendants)
        } else if (propertyKey == SCROLL_LISTENER) {
            // TODO(crbug.com/40257527): move setting listener from initializeView()
        } else if (propertyKey == IS_DEFAULT_A11Y_FOCUS_REQUESTED) {
            if (model.get(IS_DEFAULT_A11Y_FOCUS_REQUESTED)) {
                view.requestDefaultA11yFocus();
            }
        } else {
            assert false : "Binding property not implemented: " + propertyKey;
        }
    }
}
