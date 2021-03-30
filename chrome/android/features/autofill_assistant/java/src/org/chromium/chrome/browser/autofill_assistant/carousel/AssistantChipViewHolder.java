// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.carousel;

import android.view.LayoutInflater;
import android.view.Menu;
import android.view.View;
import android.view.ViewGroup;
import android.widget.PopupMenu;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.LayoutUtils;

/**
 * The {@link ViewHolder} responsible for reflecting an {@link AssistantChip} to a {@link
 * ButtonView}.
 */
public class AssistantChipViewHolder extends ViewHolder {
    private final ButtonView mView;
    private @Nullable PopupMenu mPopupMenu;

    /** The type of this ViewHolder, as returned by {@link #getViewType(AssistantChip)}. */
    private final int mType;

    private AssistantChipViewHolder(ButtonView view, int type) {
        super(view);
        mView = view;
        mType = type;
    }

    public static AssistantChipViewHolder create(ViewGroup parent, int viewType) {
        LayoutInflater layoutInflater = LayoutUtils.createInflater(parent.getContext());
        ButtonView view = null;
        switch (viewType) {
            case AssistantChip.Type.CHIP_ASSISTIVE:
                view = (ButtonView) layoutInflater.inflate(
                        R.layout.autofill_assistant_button_assistive, /* root= */ null);
                break;
            case AssistantChip.Type.BUTTON_FILLED_BLUE:
                view = (ButtonView) layoutInflater.inflate(
                        R.layout.autofill_assistant_button_filled, /* root= */ null);
                break;
            case AssistantChip.Type.BUTTON_HAIRLINE:
                view = (ButtonView) layoutInflater.inflate(
                        R.layout.autofill_assistant_button_hairline, /* root= */ null);
                break;
            default:
                assert false : "Unsupported view type " + viewType;
        }

        view.setLayoutParams(new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        return new AssistantChipViewHolder(view, viewType);
    }

    public static int getViewType(AssistantChip chip) {
        return chip.getType();
    }

    public ButtonView getView() {
        return mView;
    }

    public int getType() {
        return mType;
    }

    public void bind(AssistantChip chip) {
        mView.setEnabled(!chip.isDisabled());
        mView.setVisibility(chip.isVisible() ? View.VISIBLE : View.GONE);

        String text = chip.getText();
        if (text.isEmpty()) {
            mView.getPrimaryTextView().setVisibility(View.GONE);
        } else {
            mView.getPrimaryTextView().setText(text);
            mView.getPrimaryTextView().setVisibility(View.VISIBLE);
        }

        // Setting this view to clickable may be required for a11y to correctly announce it.
        mView.setClickable(true);

        // If a popup is specified, instead of invoking chip.getSelectedListener, show the popup
        // menu and invoke the popup callback.
        if (chip.getPopupItems() != null) {
            mPopupMenu = new PopupMenu(mView.getContext(), mView);
            for (int i = 0; i < chip.getPopupItems().size(); i++) {
                mPopupMenu.getMenu().add(Menu.NONE, i, Menu.NONE, chip.getPopupItems().get(i));
            }
            mPopupMenu.setOnMenuItemClickListener(item -> {
                chip.getOnPopupItemSelectedCallback().onResult(item.getItemId());
                return true;
            });
            mView.setOnClickListener(ignoredView -> mPopupMenu.show());
        } else {
            mView.setOnClickListener(ignoredView -> chip.getSelectedListener().run());
        }

        int iconResource;
        int iconDescriptionResource = 0;
        switch (chip.getIcon()) {
            case AssistantChip.Icon.CLEAR:
                iconResource = R.drawable.ic_clear_black_chrome_24dp;
                iconDescriptionResource = R.string.close;
                break;
            case AssistantChip.Icon.DONE:
                iconResource = R.drawable.ic_done_black_24dp;
                iconDescriptionResource = R.string.done;
                break;
            case AssistantChip.Icon.REFRESH:
                iconResource = R.drawable.ic_refresh_black_24dp;
                iconDescriptionResource = R.string.menu_refresh;
                break;
            case AssistantChip.Icon.OVERFLOW:
                iconResource = R.drawable.ic_overflow_black_24dp;
                iconDescriptionResource = R.string.autofill_assistant_overflow_options;
                break;
            default:
                iconResource = ButtonView.INVALID_ICON_ID;
                break;
        }

        mView.setIcon(iconResource, /* tintWithTextColor= */ true);

        String contentDescription = chip.getContentDescription();
        if (contentDescription != null) {
            mView.setContentDescription(contentDescription);
        } else if (iconDescriptionResource != 0 && text.isEmpty()) {
            mView.setContentDescription(mView.getContext().getString(iconDescriptionResource));
        } else {
            mView.setContentDescription(text);
        }
    }
}
