// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.preference.Preference;
import androidx.preference.PreferenceGroupAdapter;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.containment.ContainmentViewStyler;

/**
 * Controls the highlight of the selected main menu item. We should consider migrating this into
 * Containment implementation.
 */
@NullMarked
class SelectionDecoration extends RecyclerView.ItemDecoration {
    /** Margin between items. This is added both top and bottom. */
    private final int mVerticalMarginPx;

    /**
     * Margin of the left side of the item.
     *
     * <p>This is short term workaround until crbug.com/454247949 is addressed
     */
    private final int mLeftMarginPx;

    /** Card background of the selected item. */
    private final Drawable mSelectedBackground;

    /**
     * Key of the selected preference defined in main_preference.xml. Maybe null if no entry in the
     * xml is selected.
     */
    private @Nullable String mKey;

    /** A flag to re-draw background decoration. */
    private boolean mIsDirty = true;

    SelectionDecoration(
            int verticalMarginPx, int leftMarginPx, float radiusPx, int selectedBackgroundColor) {
        mVerticalMarginPx = verticalMarginPx;
        mLeftMarginPx = leftMarginPx;
        mSelectedBackground =
                ContainmentViewStyler.createRoundedDrawable(
                        radiusPx, radiusPx, selectedBackgroundColor);
    }

    /**
     * Sets the key of the preference entry in main_preferences.xml to be highlighted. Setting null
     * unsets the highlight.
     */
    void setKey(@Nullable String key) {
        mKey = key;
        mIsDirty = true;
    }

    @Override
    public void getItemOffsets(
            Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
        super.getItemOffsets(outRect, view, parent, state);
        mIsDirty = true;

        ViewGroup.MarginLayoutParams layoutParams =
                (ViewGroup.MarginLayoutParams) view.getLayoutParams();
        layoutParams.setMargins(
                mLeftMarginPx, mVerticalMarginPx, layoutParams.rightMargin, mVerticalMarginPx);
        view.setLayoutParams(layoutParams);
    }

    @Override
    public void onDraw(Canvas c, RecyclerView parent, RecyclerView.State state) {
        if (!mIsDirty) {
            return;
        }
        mIsDirty = false;
        setChildViewColor(parent);
        super.onDraw(c, parent, state);
    }

    /**
     * Updates the background of child views. If there's no focused preference, highlight will be
     * just disabled.
     */
    private void setChildViewColor(RecyclerView parent) {
        var preferenceGroupAdapter = (PreferenceGroupAdapter) parent.getAdapter();
        assert preferenceGroupAdapter != null;

        for (int i = 0; i < parent.getChildCount(); ++i) {
            View view = parent.getChildAt(i);

            boolean selected = false;
            if (mKey != null) {
                int position = parent.getChildAdapterPosition(view);
                Preference preference = preferenceGroupAdapter.getItem(position);
                if (preference != null && TextUtils.equals(mKey, preference.getKey())) {
                    selected = true;
                }
            }

            if (selected) {
                view.setBackground(mSelectedBackground);
                if (view.findViewById(android.R.id.title) instanceof TextView textView) {
                    textView.setTextAppearance(
                            R.style.TextAppearance_SettingsSelectedMainMenuItemTitle);
                }
            } else {
                view.setBackground(null);
                if (view.findViewById(android.R.id.title) instanceof TextView textView) {
                    textView.setTextAppearance(R.style.TextAppearance_SettingsMainMenuItemTitle);
                }
            }
        }
    }
}
