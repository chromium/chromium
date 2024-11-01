// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.GradientDrawable;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.DimenRes;
import androidx.annotation.StyleRes;

import org.chromium.components.browser_ui.styles.SemanticColorUtils;

/** View logic for SharedImageTiles component. */
public class SharedImageTilesView extends LinearLayout {
    private final Context mContext;
    private TextView mCountTileView;
    private LinearLayout mLastButtonTileView;

    /**
     * Constructor for a SharedImageTilesView.
     *
     * @param context The context this view will work in.
     * @param attrs The attribute set for this view.
     */
    public SharedImageTilesView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mCountTileView = findViewById(R.id.tiles_count);
        mLastButtonTileView = findViewById(R.id.last_tile_container);
    }

    void setTileBackgroundColor(@ColorInt int backgroundColor) {
        LinearLayout container = (LinearLayout) findViewById(R.id.last_tile_container);
        if (container != null) {
            GradientDrawable drawable = (GradientDrawable) container.getBackground();
            drawable.setColor(backgroundColor);
        }
    }

    void setColorTheme(@SharedImageTilesColor int color) {
        // Note: This view is following the SharedImageTilesColor.DEFAULT theme by default.
        // Resetting to default theme after setting dynamic theme is not supported.
        if (color == SharedImageTilesColor.DYNAMIC) {
            mCountTileView.setTextColor(
                    SemanticColorUtils.getDefaultIconColorOnAccent1Container(mContext));
            setTileBackgroundColor(SemanticColorUtils.getColorPrimaryContainer(mContext));
        }
    }

    void setType(@SharedImageTilesType int type) {
        switch (type) {
            case SharedImageTilesType.DEFAULT:
                setChildViewSize(
                        R.dimen.shared_image_tiles_icon_total_height,
                        R.style.TextAppearance_TextAccentMediumThick_Primary);
                break;
            case SharedImageTilesType.SMALL:
                setChildViewSize(
                        R.dimen.small_shared_image_tiles_icon_total_height,
                        R.style.TextAppearance_SharedImageTilesSmall);
                break;
        }
    }

    void setChildViewSize(@DimenRes int iconSizeDp, @StyleRes int textStyle) {
        int iconSizePx = mContext.getResources().getDimensionPixelSize(iconSizeDp);
        // Loop through all child views.
        for (int i = 0; i < getChildCount(); i++) {
            ViewGroup viewGroup = (ViewGroup) getChildAt(i);
            viewGroup.getLayoutParams().height = iconSizePx;
            viewGroup.setMinimumWidth(iconSizePx);
        }

        // Set sizing for the number tile.
        mCountTileView.setTextAppearance(textStyle);
    }

    void resetIconTiles(int count) {
        // Remove all child icon views and hide last view, which is the count tile or the button
        // tile.
        for (int i = 0; i < getChildCount() - 1; i++) {
            removeViewAt(i);
        }
        mCountTileView.setVisibility(View.GONE);
        mLastButtonTileView.setVisibility(View.GONE);

        // Add icon views.
        for (int i = 0; i < count; i++) {
            addView(
                    LayoutInflater.from(mContext)
                            .inflate(R.layout.shared_image_tiles_icon, this, false),
                    0);
        }
    }

    void showCountTile(int count) {
        mLastButtonTileView.setVisibility(View.VISIBLE);
        mCountTileView.setVisibility(View.VISIBLE);
        Resources res = mContext.getResources();
        String countText =
                res.getString(R.string.shared_image_tiles_count, Integer.toString(count));
        mCountTileView.setText(countText);
    }
}
