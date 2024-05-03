// Copyright 2024 The Chromium Authors
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

import androidx.appcompat.content.res.AppCompatResources;
import androidx.vectordrawable.graphics.drawable.AnimatedVectorDrawableCompat;

import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableItemViewBase;

/** Holds the view for a selectable tab grid. */
public class SelectableTabListView extends SelectableItemViewBase<Integer> {
    public SelectableTabListView(Context context, AttributeSet attrs) {
        super(context, attrs);
        setSelectionOnLongClick(false);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        var resources = getResources();

        Drawable selectionListIcon =
                AppCompatResources.getDrawable(
                        getContext(), R.drawable.tab_grid_selection_list_icon);
        ImageView actionButton = findViewById(R.id.end_button);
        actionButton.setVisibility(View.VISIBLE);
        int lateralInset =
                resources.getDimensionPixelSize(
                        R.dimen.selection_tab_list_toggle_button_lateral_inset);
        int verticalInset =
                resources.getDimensionPixelSize(
                        R.dimen.selection_tab_list_toggle_button_vertical_inset);
        InsetDrawable drawable =
                new InsetDrawable(
                        selectionListIcon,
                        lateralInset,
                        verticalInset,
                        lateralInset,
                        verticalInset);
        actionButton.setBackground(drawable);
        findViewById(R.id.start_icon).setBackground(null);
        actionButton
                .getBackground()
                .setLevel(resources.getInteger(R.integer.list_item_level_default));
        actionButton.setImageDrawable(
                AnimatedVectorDrawableCompat.create(
                        getContext(), R.drawable.ic_check_googblue_20dp_animated));
    }

    // SelectableItemViewBase implementation.

    @Override
    protected void onClick() {
        super.onClick(this);
    }

    @Override
    protected void updateView(boolean animate) {}

    // View implementation.

    @Override
    public void onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo info) {
        super.onInitializeAccessibilityNodeInfo(info);

        info.setCheckable(true);
        info.setChecked(isChecked());
    }
}
