// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.InsetDrawable;
import android.support.graphics.drawable.AnimatedVectorDrawableCompat;
import android.support.v4.content.res.ResourcesCompat;
import android.util.AttributeSet;
import android.widget.ImageView;

import org.chromium.chrome.browser.widget.selection.SelectableItemView;

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

        Drawable selectionListIcon = ResourcesCompat.getDrawable(getResources(),
                org.chromium.chrome.tab_ui.R.drawable.tab_grid_selection_list_icon,
                getContext().getTheme());
        InsetDrawable drawable = new InsetDrawable(selectionListIcon,
                (int) getResources().getDimension(
                        org.chromium.chrome.tab_ui.R.dimen.selection_tab_grid_toggle_button_inset));
        ImageView actionButton =
                (ImageView) fastFindViewById(org.chromium.chrome.tab_ui.R.id.action_button);
        actionButton.setBackground(drawable);
        actionButton.getBackground().setLevel(getResources().getInteger(
                org.chromium.chrome.tab_ui.R.integer.list_item_level_default));
        actionButton.setImageDrawable(AnimatedVectorDrawableCompat.create(getContext(),
                org.chromium.chrome.tab_ui.R.drawable.ic_check_googblue_20dp_animated));
    }

    @Override
    protected void onClick() {
        super.onClick(this);
    }
}
