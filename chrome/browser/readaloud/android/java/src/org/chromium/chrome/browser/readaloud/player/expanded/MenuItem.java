// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.expanded;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.RadioButton;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.appcompat.widget.SwitchCompat;

import com.google.android.material.materialswitch.MaterialSwitch;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.readaloud.player.R;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** MenuItem is a view that can be used for all Read Aloud player menu item variants. */
public class MenuItem extends FrameLayout {
    private static final String TAG = "ReadAloudMenuItem";

    /** Menu item actions that show up as widgets at the end. */
    @IntDef({Action.NONE, Action.EXPAND, Action.RADIO, Action.TOGGLE})
    @Retention(RetentionPolicy.SOURCE)
    @interface Action {
        /** No special action. */
        int NONE = 0;

        /** Show a separator and arrow meaning that clicking will open a submenu. */
        int EXPAND = 1;

        /** Show a radio button. */
        int RADIO = 2;

        /** Show a toggle switch. */
        int TOGGLE = 3;
    }

    private final int mId;
    private final @Action int mActionType;
    private final Menu mMenu;
    private final LinearLayout mLayout;
    private final ImageView mPlayButton;
    private final ProgressBar mPlayButtonSpinner;
    private Callback<Boolean> mToggleHandler;

    /**
     * @param context Context.
     * @param parentMenu Menu to which this item belongs.
     * @param itemId Menu item's identifying number, to be used for handling clicks.
     * @param iconId Resource ID of an icon drawable. Pass 0 to show no icon.
     * @param label Primary text to show for the item.
     * @param action Extra widget to show at the end.
     */
    public MenuItem(
            Context context,
            Menu parentMenu,
            int itemId,
            int iconId,
            String label,
            @Action int action,
            String contentDescription) {
        super(context);
        mMenu = parentMenu;
        mId = itemId;
        mActionType = action;

        LayoutInflater inflater = LayoutInflater.from(context);
        LinearLayout layout = (LinearLayout) inflater.inflate(R.layout.readaloud_menu_item, null);
        layout.setOnClickListener(
                (view) -> {
                    onClick();
                });
        mLayout = layout;
        if (iconId != 0) {
            ImageView icon = layout.findViewById(R.id.icon);
            icon.setImageResource(iconId);
            // Icon is GONE by default.
            icon.setVisibility(View.VISIBLE);
        }

        ((TextView) layout.findViewById(R.id.item_label)).setText(label);
        layout.setContentDescription(contentDescription);

        switch (mActionType) {
            case Action.EXPAND:
                View expandView = inflater.inflate(R.layout.expand_arrow_with_separator, null);
                View arrow = expandView.findViewById(R.id.expand_arrow);
                arrow.setClickable(false);
                arrow.setFocusable(false);
                setEndView(layout, expandView);
                break;

            case Action.TOGGLE:
                MaterialSwitch materialSwitch =
                        (MaterialSwitch) inflater.inflate(R.layout.readaloud_toggle_switch, null);
                materialSwitch.setOnCheckedChangeListener(
                        (view, value) -> {
                            onToggle(value);
                        });
                setEndView(layout, materialSwitch);
                break;

            case Action.RADIO:
                RadioButton radioButton =
                        (RadioButton) inflater.inflate(R.layout.readaloud_radio_button, null);
                radioButton.setOnCheckedChangeListener(
                        (view, value) -> {
                            if (value) {
                                onRadioButtonSelected();
                            }
                        });
                setEndView(layout, radioButton);
                break;
            case Action.NONE:
            default:
                break;
        }
        addView(layout);

        mPlayButton = (ImageView) findViewById(R.id.play_button);
        mPlayButtonSpinner = (ProgressBar) findViewById(R.id.spinner);
    }

    void setToggleHandler(Callback<Boolean> handler) {
        mToggleHandler = handler;
    }

    void addPlayButton() {
        mPlayButton.setVisibility(View.VISIBLE);
        mPlayButton.setOnClickListener(
                (view) -> {
                    mMenu.onPlayButtonClicked(mId);
                });
    }

    void setItemEnabled(boolean enabled) {
        mLayout.setClickable(enabled);
        mLayout.setFocusable(enabled);
    }

    void setValue(boolean value) {
        if (mActionType == Action.TOGGLE) {
            getToggleSwitch().setChecked(value);
        } else if (mActionType == Action.RADIO) {
            getRadioButton().setChecked(value);
        }
    }

    void showPlayButtonSpinner() {
        mPlayButton.setVisibility(View.GONE);
        mPlayButtonSpinner.setVisibility(View.VISIBLE);
    }

    void showPlayButton() {
        mPlayButtonSpinner.setVisibility(View.GONE);
        mPlayButton.setVisibility(View.VISIBLE);
    }

    void setPlayButtonStopped() {
        mPlayButton.setImageResource(R.drawable.mini_play_button);
    }

    void setPlayButtonPlaying() {
        mPlayButton.setImageResource(R.drawable.mini_pause_button);
    }

    private void setEndView(LinearLayout layout, View view) {
        ((FrameLayout) layout.findViewById(R.id.end_view)).addView(view);
    }

    // On click won't be propagated here if the parent layout is not clickable
    private void onClick() {
        assert mMenu != null;
        if (mActionType == Action.RADIO) {
            getRadioButton().toggle();
        } else if (mActionType == Action.TOGGLE) {
            getToggleSwitch().toggle();
        }
        mMenu.onItemClicked(mId);
    }

    private void onRadioButtonSelected() {
        assert mMenu != null;
        mMenu.onRadioButtonSelected(mId);
    }

    private void onToggle(boolean newValue) {
        if (mToggleHandler != null) {
            mToggleHandler.onResult(newValue);
        }
    }

    private SwitchCompat getToggleSwitch() {
        return (SwitchCompat) findViewById(R.id.toggle_switch);
    }

    private RadioButton getRadioButton() {
        return (RadioButton) findViewById(R.id.readaloud_radio_button);
    }
}
