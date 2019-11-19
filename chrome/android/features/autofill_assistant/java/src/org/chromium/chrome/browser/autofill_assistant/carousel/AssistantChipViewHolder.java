// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.carousel;

import android.support.v7.widget.RecyclerView.ViewHolder;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.autofill_assistant.R;

/**
 * The {@link ViewHolder} responsible for reflecting an {@link AssistantChip} to a {@link
 * ButtonView}.
 */
public class AssistantChipViewHolder extends ViewHolder {
    private final ButtonView mView;

    /** The type of this ViewHolder, as returned by {@link #getViewType(AssistantChip)}. */
    private final int mType;

    private AssistantChipViewHolder(ButtonView view, int type) {
        super(view);
        mView = view;
        mType = type;
    }

    public static AssistantChipViewHolder create(ViewGroup parent, int viewType) {
        LayoutInflater layoutInflater = LayoutInflater.from(parent.getContext());
        ButtonView view = null;
        switch (viewType % AssistantChip.Type.NUM_ENTRIES) {
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
        if (viewType >= AssistantChip.Type.NUM_ENTRIES) {
            view.setEnabled(false);
        }

        return new AssistantChipViewHolder(view, viewType);
    }

    public static int getViewType(AssistantChip chip) {
        // We add AssistantChip.Type.CHIP_TYPE_NUMBER to differentiate between enabled and disabled
        // chips of the same type. Ideally, we should return a (type, disabled) tuple but
        // RecyclerView does not allow that.
        if (chip.isDisabled()) {
            return chip.getType() + AssistantChip.Type.NUM_ENTRIES;
        }

        return chip.getType();
    }

    public ButtonView getView() {
        return mView;
    }

    public int getType() {
        return mType;
    }

    public void bind(AssistantChip chip) {
        String text = chip.getText();
        if (text.isEmpty()) {
            mView.getPrimaryTextView().setVisibility(View.GONE);
        } else {
            mView.getPrimaryTextView().setText(text);
            mView.getPrimaryTextView().setVisibility(View.VISIBLE);
        }

        mView.setOnClickListener(ignoredView -> chip.getSelectedListener().run());

        int iconResource;
        int iconDescriptionResource = 0;
        switch (chip.getIcon()) {
            case AssistantChip.Icon.CLEAR:
                iconResource = R.drawable.ic_clear_black_24dp;
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
            default:
                iconResource = ButtonView.INVALID_ICON_ID;
                break;
        }

        mView.setIcon(iconResource, /* tintWithTextColor= */ true);

        // We set the content description of the icon on the whole button view if there is no text.
        // Otherwise the text description will be used by TalkBack.
        if (iconDescriptionResource != 0 && text.isEmpty()) {
            mView.setContentDescription(mView.getContext().getString(iconDescriptionResource));
        } else {
            mView.setContentDescription(null);
        }
    }
}
