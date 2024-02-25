// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.expanded;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.AccessibilityDelegate;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.RadioButton;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.widget.SwitchCompat;

import com.google.android.material.materialswitch.MaterialSwitch;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneShotCallback;
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
    private final ObservableSupplier<LinearLayout> mLayoutSupplier;
    private final ImageView mPlayButton;
    private final ProgressBar mPlayButtonSpinner;
    private Callback<Boolean> mToggleHandler;
    private final String mLabel;

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
            @Nullable String header,
            @Action int action) {
        super(context);
        mMenu = parentMenu;
        mId = itemId;
        mActionType = action;
        mLabel = label;
        LayoutInflater inflater = LayoutInflater.from(context);
        LinearLayout layout = (LinearLayout) inflater.inflate(R.layout.readaloud_menu_item, null);
        layout.setOnClickListener(
                (view) -> {
                    onClick();
                });
        mLayout = layout;
        mLayoutSupplier = new ObservableSupplierImpl(mLayout);
        new OneShotCallback<LinearLayout>(mLayoutSupplier, this::onLayoutInflated);
        if (iconId != 0) {
            ImageView icon = layout.findViewById(R.id.icon);
            icon.setImageResource(iconId);
            // Icon is GONE by default.
            icon.setVisibility(View.VISIBLE);
        }
        TextView localeView = layout.findViewById(R.id.item_header);
        if (header == null) {
            localeView.setVisibility(View.GONE);
        } else {
            localeView.setVisibility(View.VISIBLE);
            localeView.setText(header);
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
        mPlayButton.setContentDescription(
                context.getResources().getString(R.string.readaloud_play) + " " + mLabel);
        mPlayButtonSpinner = (ProgressBar) findViewById(R.id.spinner);
        mPlayButtonSpinner.setContentDescription(
                context.getResources().getString(R.string.readaloud_playback_loading)
                        + " "
                        + mLabel);
    }

    private void onLayoutInflated(LinearLayout layout) {
        // accessibility delegate is only being set for radio and toggle switch item types
        if (mActionType != Action.RADIO && mActionType != Action.TOGGLE) {
            return;
        }
        // To improve the explore-by-touch experience, the button are hidden from accessibility
        // and instead, "checked" or "not checked" is read along with the button's label
        layout.setAccessibilityDelegate(
                new AccessibilityDelegate() {
                    @Override
                    public void onInitializeAccessibilityEvent(
                            View host, AccessibilityEvent event) {
                        super.onInitializeAccessibilityEvent(host, event);
                        if (mActionType == Action.RADIO) {
                            event.setChecked(getRadioButton().isChecked());
                        } else if (mActionType == Action.TOGGLE) {
                            event.setChecked(getToggleSwitch().isChecked());
                        }
                    }

                    @Override
                    public void onInitializeAccessibilityNodeInfo(
                            View host, AccessibilityNodeInfo info) {
                        super.onInitializeAccessibilityNodeInfo(host, info);
                        info.setCheckable(true);
                        if (mActionType == Action.RADIO) {
                            info.setChecked(getRadioButton().isChecked());
                            info.setEnabled(getRadioButton().isEnabled());
                        } else if (mActionType == Action.TOGGLE) {
                            info.setChecked(getToggleSwitch().isChecked());
                            info.setEnabled(getToggleSwitch().isEnabled());
                        }
                    }
                });
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
        if (mActionType == Action.TOGGLE) {
            getToggleSwitch().setEnabled(enabled);
        }
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
        mPlayButton.setContentDescription(
                getContext().getResources().getString(R.string.readaloud_play) + " " + mLabel);
    }

    void setPlayButtonPlaying() {
        mPlayButton.setImageResource(R.drawable.mini_pause_button);
        mPlayButton.setContentDescription(
                getContext().getResources().getString(R.string.readaloud_pause) + " " + mLabel);
    }

    void setSecondLine(String text) {
        TextView view = mLayout.findViewById(R.id.item_sublabel);
        view.setText(text);
        view.setVisibility(View.VISIBLE);
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

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public ObservableSupplierImpl<LinearLayout> getLayoutSupplier() {
        return (ObservableSupplierImpl) mLayoutSupplier;
    }
}
