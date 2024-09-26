// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.button_group_component;

import android.content.Context;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.Gravity;
import android.widget.LinearLayout;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;

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

    /** Constructor for inflating from XML. */
    public KeyboardAccessoryButtonGroupView(Context context, AttributeSet attrs) {
        super(context, attrs);
        this.setGravity(Gravity.CENTER);
    }

    /**
     * Creates a new button and appends it to the end of the button group at the end of the bar.
     *
     * @param icon The icon to be displayed in the button.
     * @param contentDescription The contentDescription to be used for the button.
     */
    public void addButton(Drawable icon, CharSequence contentDescription) {
        ChromeImageButton button = new ChromeImageButton(getContext());
        button.setMaxWidth(
                getResources().getDimensionPixelSize(R.dimen.keyboard_accessory_tab_icon_width));
        button.setMaxHeight(
                getResources().getDimensionPixelSize(R.dimen.keyboard_accessory_tab_size));
        button.setMinimumWidth(button.getMaxWidth());
        button.setMinimumHeight(button.getMaxHeight());
        button.setPaddingRelative(
                getResources().getDimensionPixelSize(R.dimen.keyboard_accessory_bar_item_padding),
                0,
                getResources().getDimensionPixelSize(R.dimen.keyboard_accessory_bar_item_padding),
                0);
        button.setImageDrawable(icon.mutate()); // mutate() needed to change the active tint.
        button.getDrawable()
                .setColorFilter(
                        SemanticColorUtils.getDefaultIconColor(getContext()),
                        PorterDuff.Mode.SRC_IN);
        button.setContentDescription(contentDescription);
        button.setBackground(null);
        button.setOnClickListener(
                view -> {
                    if (mListener == null) return;
                    mListener.onButtonClicked(mButtons.indexOf(view));
                });
        mButtons.add(button);
        addView(button);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        // The parent, which is KeyboardAccessoryView's recycler view may measure
        // StickyLastItemDecoration offsets before the buttons are added. Notify the parent to
        // recalculate the offset during each measurement.
        // TODO(crbug.com/40898366): Find a better alternative.
        if (getParent() == null || !(getParent() instanceof RecyclerView)) return;
        RecyclerView parent = (RecyclerView) getParent();
        parent.post(parent::invalidateItemDecorations);
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
