// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.tab_layout_component;

import android.content.Context;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.Gravity;
import android.widget.LinearLayout;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.widget.ChromeImageButton;

import java.util.ArrayList;

/**
 * A {@link LinearLayout} containing the sheet opener buttons in the keyboard
 * accessory.
 */
public class KeyboardAccessoryButtonGroupView extends LinearLayout {
    private final ArrayList<ChromeImageButton> mButtons = new ArrayList<>();
    private KeyboardAccessoryButtonGroupListener mListener;

    /**
     * This interface should be implemented by classes which want to observe clicks on the buttons
     * in this view.
     */
    interface KeyboardAccessoryButtonGroupListener {
        void onButtonClicked(int position);
    }

    /**
     * Constructor for inflating from XML.
     */
    public KeyboardAccessoryButtonGroupView(Context context, AttributeSet attrs) {
        super(context, attrs);
        this.setGravity(Gravity.CENTER);
    }

    /**
     * Creates a new button and appends it to the end of the button group at the end of the bar.
     * @param id The id of the tab
     * @param icon The icon to be displayed in the button.
     * @param contentDescription The contentDescription to be used for the button.
     */
    public void addButton(Drawable icon, CharSequence contentDescription) {
        ChromeImageButton button = new ChromeImageButton(getContext());
        button.setMaxWidth(
                getResources().getDimensionPixelSize(R.dimen.keyboard_accessory_tab_icon_width));
        button.setMaxHeight(
                getResources().getDimensionPixelSize(R.dimen.keyboard_accessory_tab_size));
        button.setPaddingRelative(
                getResources().getDimensionPixelSize(R.dimen.keyboard_accessory_bar_item_padding),
                0,
                getResources().getDimensionPixelSize(R.dimen.keyboard_accessory_bar_item_padding),
                0);
        button.setImageDrawable(icon.mutate()); // mutate() needed to change the active tint.
        button.getDrawable().setColorFilter(
                SemanticColorUtils.getDefaultIconColor(getContext()), PorterDuff.Mode.SRC_IN);
        button.setContentDescription(contentDescription);
        button.setBackground(null);
        button.setOnClickListener(view -> {
            if (mListener == null) return;
            mListener.onButtonClicked(mButtons.indexOf(view));
        });
        mButtons.add(button);
        addView(button);
    }

    void removeAllButtons() {
        mButtons.clear();
        removeAllViews();
    }

    void setButtonSelectionListener(KeyboardAccessoryButtonGroupListener listener) {
        mListener = listener;
    }

    @VisibleForTesting
    public ArrayList<ChromeImageButton> getButtons() {
        return mButtons;
    }
}
