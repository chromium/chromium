// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.LayoutRes;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabModel.AccessorySheetDataPiece;
import org.chromium.ui.modelutil.ListModel;

/**
 * This stateless class provides methods to bind a {@link ListModel<AccessorySheetDataPiece>}
 * to the {@link RecyclerView} used as view of a tab for the accessory sheet component.
 */
class AccessorySheetTabViewBinder {
    /**
     * Holds any View that represents a list entry.
     */
    static abstract class ElementViewHolder<T, V extends View> extends RecyclerView.ViewHolder {
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
     * @param parent A {@link android.view.ViewParent} to attach this view to.
     * @param viewType A {@link AccessorySheetDataPiece.Type} describing the view to be created.
     * @return A {@link ElementViewHolder}.
     */
    static ElementViewHolder create(ViewGroup parent, @AccessorySheetDataPiece.Type int viewType) {
        switch (viewType) {
            case AccessorySheetDataPiece.Type.TITLE:
                return new TitleViewHolder(parent);
            case AccessorySheetDataPiece.Type.FOOTER_COMMAND:
                return new FooterCommandViewHolder(parent);
        }
        assert false : "Unhandled type of data piece: " + viewType;
        return null;
    }

    /**
     * Holds a Title consisting of a top divider, a text view and a bottom divider.
     */
    static class TitleViewHolder extends ElementViewHolder<String, LinearLayout> {
        TitleViewHolder(ViewGroup parent) {
            this(parent, R.layout.keyboard_accessory_sheet_tab_legacy_title);
        }

        TitleViewHolder(ViewGroup parent, @LayoutRes int layout) {
            super(parent, layout);
        }

        @Override
        protected void bind(String displayText, LinearLayout view) {
            TextView titleView = view.findViewById(R.id.tab_title);
            titleView.setText(displayText);
            titleView.setContentDescription(displayText);
        }
    }

    /**
     * Holds a clickable {@link TextView} that represents a footer command.
     */
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

    static void initializeView(
            RecyclerView view, @Nullable RecyclerView.OnScrollListener scrollListener) {
        view.setLayoutManager(
                new LinearLayoutManager(view.getContext(), LinearLayoutManager.VERTICAL, false));
        view.setItemAnimator(null);
        if (scrollListener != null) view.addOnScrollListener(scrollListener);
    }
}
