// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.button_group_component;

import android.content.Context;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.LinearLayout;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;

import java.util.ArrayList;

/** A {@link LinearLayout} containing the sheet opener buttons in the keyboard accessory. */
@NullMarked
public class KeyboardAccessoryButtonGroupView extends LinearLayout {
    private final ArrayList<ImageButton> mButtons = new ArrayList<>();
    private @Nullable KeyboardAccessoryButtonGroupListener mListener;

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
        LayoutInflater inflater = LayoutInflater.from(getContext());
        ImageButton button =
                (ImageButton)
                        inflater.inflate(R.layout.keyboard_accessory_image_button, this, false);
        button.setImageDrawable(icon.mutate()); // mutate() needed to change the active tint.
        button.getDrawable()
                .setColorFilter(
                        SemanticColorUtils.getDefaultIconColor(getContext()),
                        PorterDuff.Mode.SRC_IN);
        button.setContentDescription(contentDescription);
        button.setOnClickListener(
                view -> {
                    if (mListener == null) return;
                    mListener.onButtonClicked(mButtons.indexOf(view));
                });
        // Add a spacing between buttons in the group.
        ViewGroup.MarginLayoutParams marginParams =
                (ViewGroup.MarginLayoutParams) button.getLayoutParams();
        int spacing =
                getResources().getDimensionPixelSize(R.dimen.keyboard_accessory_tab_icon_spacing);
        marginParams.leftMargin = spacing;
        marginParams.rightMargin = spacing;
        button.setLayoutParams(marginParams);
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
    public ArrayList<ImageButton> getButtons() {
        return mButtons;
    }
}
