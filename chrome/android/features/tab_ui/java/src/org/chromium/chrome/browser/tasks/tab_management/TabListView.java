// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.InsetDrawable;
import android.util.AttributeSet;
import android.view.View;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.ImageView;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.vectordrawable.graphics.drawable.AnimatedVectorDrawableCompat;

import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabActionState;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableItemViewBase;

// TODO(crbug.com/339038505): De-dupe logic in TabGridView.
/** Holds the view for a tab list. */
public class TabListView extends SelectableItemViewBase<Integer> {
    private @TabActionState int mTabActionState = TabActionState.UNSET;
    private ImageView mActionButton;

    public TabListView(Context context, AttributeSet attrs) {
        super(context, attrs);
        setSelectionOnLongClick(false);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mActionButton = findViewById(R.id.end_button);
    }

    void setTabActionState(@TabActionState int tabActionState) {
        if (mTabActionState == tabActionState) return;

        mTabActionState = tabActionState;
        int accessibilityMode = IMPORTANT_FOR_ACCESSIBILITY_YES;
        if (mTabActionState == TabActionState.CLOSABLE) {
            setTabActionButtonCloseDrawable();
        } else if (mTabActionState == TabActionState.SELECTABLE) {
            accessibilityMode = IMPORTANT_FOR_ACCESSIBILITY_NO;
            setTabActionButtonSelectionDrawable();
        }

        mActionButton.setImportantForAccessibility(accessibilityMode);
    }

    private void setTabActionButtonCloseDrawable() {
        assert mTabActionState != TabActionState.UNSET;
        var resources = getResources();

        mActionButton.setVisibility(View.VISIBLE);
        int closeButtonSize = (int) resources.getDimension(R.dimen.tab_grid_close_button_size);
        Bitmap bitmap = BitmapFactory.decodeResource(resources, R.drawable.btn_close);
        Bitmap.createScaledBitmap(bitmap, closeButtonSize, closeButtonSize, true);
        mActionButton.setImageBitmap(bitmap);
        mActionButton.setBackground(null);
    }

    private void setTabActionButtonSelectionDrawable() {
        assert mTabActionState != TabActionState.UNSET;
        var resources = getResources();

        Drawable selectionListIcon =
                AppCompatResources.getDrawable(
                        getContext(), R.drawable.tab_grid_selection_list_icon);
        mActionButton.setVisibility(View.VISIBLE);
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
        mActionButton.setBackground(drawable);
        findViewById(R.id.start_icon).setBackground(null);
        mActionButton
                .getBackground()
                .setLevel(resources.getInteger(R.integer.list_item_level_default));
        mActionButton.setImageDrawable(
                AnimatedVectorDrawableCompat.create(
                        getContext(), R.drawable.ic_check_googblue_20dp_animated));
    }

    // SelectableItemViewBase implementation.

    @Override
    protected void updateView(boolean animate) {}

    @Override
    protected void handleNonSelectionClick() {}

    // TODO(crbug.com/339038201): Consider capturing click events and discarding them while not in
    // selection mode.

    // View implementation.

    @Override
    public void onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo info) {
        super.onInitializeAccessibilityNodeInfo(info);

        if (mTabActionState == TabActionState.SELECTABLE) {
            info.setCheckable(true);
            info.setChecked(isChecked());
        }
    }
}
