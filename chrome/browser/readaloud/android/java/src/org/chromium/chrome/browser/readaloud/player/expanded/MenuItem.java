// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.expanded;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.CompoundButton;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.appcompat.widget.SwitchCompat;

import org.chromium.chrome.browser.readaloud.player.R;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** MenuItem is a view that can be used for all Read Aloud player menu item variants. */
class MenuItem extends FrameLayout {
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

    /**
     * @param context Context.
     * @param attrs Attribute set (could be from parent view).
     * @param parentMenu Menu to which this item belongs.
     * @param itemId Menu item's identifying number, to be used for handling clicks.
     * @param iconId Resource ID of an icon drawable. Pass 0 to show no icon.
     * @param label Primary text to show for the item.
     * @param action Extra widget to show at the end.
     */
    public MenuItem(
            Context context,
            AttributeSet attrs,
            Menu parentMenu,
            int itemId,
            int iconId,
            String label,
            @Action int action) {
        super(context, attrs);
        mMenu = parentMenu;
        mId = itemId;
        mActionType = action;

        LayoutInflater inflater = LayoutInflater.from(context);
        LinearLayout layout = (LinearLayout) inflater.inflate(R.layout.readaloud_menu_item, null);
        layout.setOnClickListener(
                (view) -> {
                    onClick();
                });

        if (iconId != 0) {
            ImageView icon = layout.findViewById(R.id.icon);
            icon.setImageResource(iconId);
            // Icon is GONE by default.
            icon.setVisibility(View.VISIBLE);
        }

        ((TextView) layout.findViewById(R.id.item_label)).setText(label);

        switch (mActionType) {
            case Action.EXPAND:
                View expandView = inflater.inflate(R.layout.expand_arrow_with_separator, null);
                View arrow = expandView.findViewById(R.id.expand_arrow);
                arrow.setClickable(false);
                arrow.setFocusable(false);
                setEndView(layout, expandView);
                break;

            case Action.TOGGLE:
                setEndView(layout, inflater.inflate(R.layout.readaloud_toggle_switch, null));
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
    }

    void addPlayButton() {
        ImageView playButton = (ImageView) findViewById(R.id.play_button);
        playButton.setVisibility(View.VISIBLE);
        playButton.setOnClickListener(
                (view) -> {
                    mMenu.onPlayButtonClicked(mId);
                });
    }

    // If enabled=false, disappear the item.
    // TODO gray out out and make unclickable instead?
    void setItemEnabled(boolean enabled) {
        setVisibility(enabled ? View.VISIBLE : View.GONE);
    }

    void setValue(boolean value) {
        if (mActionType == Action.TOGGLE) {
            getToggleSwitch().setChecked(value);
        } else if (mActionType == Action.RADIO) {
            getRadioButton().setChecked(value);
        }
    }

    void setChangeListener(CompoundButton.OnCheckedChangeListener onChange) {
        if (mActionType == Action.TOGGLE) {
            getToggleSwitch().setOnCheckedChangeListener(onChange);
        }
    }

    private void setEndView(LinearLayout layout, View view) {
        ((FrameLayout) layout.findViewById(R.id.end_view)).addView(view);
    }

    private void onClick() {
        if (mMenu == null) {
            return;
        }

        if (mActionType == Action.RADIO) {
            getRadioButton().toggle();
        } else if (mActionType == Action.TOGGLE) {
            getToggleSwitch().toggle();
        }
        mMenu.onItemClicked(mId);
    }

    private void onRadioButtonSelected() {
        if (mMenu == null) {
            return;
        }
        mMenu.onRadioButtonSelected(mId);
    }

    private SwitchCompat getToggleSwitch() {
        return (SwitchCompat) findViewById(R.id.toggle_switch);
    }

    private RadioButton getRadioButton() {
        return (RadioButton) findViewById(R.id.readaloud_radio_button);
    }
}
