// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.InsetDrawable;
import android.util.AttributeSet;
import android.view.View;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.ImageView;

import androidx.core.content.res.ResourcesCompat;
import androidx.vectordrawable.graphics.drawable.AnimatedVectorDrawableCompat;

import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableItemView;

/**
 * Holds the view for a selectable tab grid.
 */
public class SelectableTabGridView extends SelectableItemView<Integer> {
    public SelectableTabGridView(Context context, AttributeSet attrs) {
        super(context, attrs);
        setSelectionOnLongClick(false);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        Drawable selectionListIcon = ResourcesCompat.getDrawable(
                getResources(), R.drawable.tab_grid_selection_list_icon, getContext().getTheme());
        ImageView actionButton = (ImageView) fastFindViewById(R.id.action_button);

        if (actionButton != null) {
            InsetDrawable drawable = new InsetDrawable(selectionListIcon,
                    (int) getContext().getResources().getDimension(
                            R.dimen.selection_tab_grid_toggle_button_inset));
            actionButton.setBackground(drawable);
            // Remove the original content from SelectableItemView since we are not using them.
            removeView(mContentView);
        } else {
            actionButton = mEndButtonView;
            actionButton.setVisibility(View.VISIBLE);
            InsetDrawable drawable = new InsetDrawable(selectionListIcon,
                    (int) getResources().getDimension(
                            R.dimen.selection_tab_list_toggle_button_lateral_inset),
                    (int) getResources().getDimension(
                            R.dimen.selection_tab_list_toggle_button_vertical_inset),
                    (int) getResources().getDimension(
                            R.dimen.selection_tab_list_toggle_button_lateral_inset),
                    (int) getResources().getDimension(
                            R.dimen.selection_tab_list_toggle_button_vertical_inset));
            actionButton.setBackground(drawable);
            mStartIconView.setBackground(null);
        }
        actionButton.getBackground().setLevel(
                getResources().getInteger(R.integer.list_item_level_default));
        actionButton.setImageDrawable(AnimatedVectorDrawableCompat.create(
                getContext(), R.drawable.ic_check_googblue_20dp_animated));
    }

    @Override
    protected void onClick() {
        super.onClick(this);
    }

    @Override
    protected void updateView(boolean animate) {}

    @Override
    public void onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo info) {
        super.onInitializeAccessibilityNodeInfo(info);

        info.setCheckable(true);
        info.setChecked(isChecked());
    }
}
