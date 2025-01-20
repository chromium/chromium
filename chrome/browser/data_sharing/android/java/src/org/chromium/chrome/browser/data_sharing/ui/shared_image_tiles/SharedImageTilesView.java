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

import org.chromium.components.browser_ui.styles.ChromeColors;
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

    void setColorStyle(SharedImageTilesColor colorStyle) {
        switch (colorStyle.currentStyle) {
            case SharedImageTilesColor.Style.DEFAULT:
                setColorOnChildViews(
                        SemanticColorUtils.getDefaultTextColor(mContext),
                        SemanticColorUtils.getDefaultBgColor(mContext),
                        ChromeColors.getSurfaceColor(mContext, R.dimen.default_bg_elevation));
                break;
            case SharedImageTilesColor.Style.DYNAMIC:
                setColorOnChildViews(
                        SemanticColorUtils.getDefaultTextColorAccent1(mContext),
                        SemanticColorUtils.getDefaultBgColor(mContext),
                        SemanticColorUtils.getColorPrimaryContainer(mContext));
                break;
            case SharedImageTilesColor.Style.TAB_GROUP:
                setColorOnChildViews(
                        ChromeColors.getSurfaceColor(mContext, R.dimen.default_bg_elevation),
                        colorStyle.tabGroupColor,
                        colorStyle.tabGroupColor);
                break;
        }
    }

    void setColorOnChildViews(
            @ColorInt int textColor, @ColorInt int borderColor, @ColorInt int backgroundColor) {
        for (int i = 0; i < getChildCount(); i++) {
            ViewGroup view_group = (ViewGroup) getChildAt(i);
            GradientDrawable drawable = (GradientDrawable) view_group.getBackground();
            drawable.setColor(backgroundColor);
            drawable.setStroke(
                    mContext.getResources()
                            .getDimensionPixelSize(R.dimen.shared_image_tiles_icon_border),
                    borderColor);
        }

        mCountTileView.setTextColor(textColor);
    }

    void setType(@SharedImageTilesType int type) {
        switch (type) {
            case SharedImageTilesType.DEFAULT:
                setChildViewSize(
                        R.dimen.shared_image_tiles_icon_total_height,
                        R.style.TextAppearance_TextAccentMediumThick_Primary,
                        R.dimen.shared_image_tiles_text_padding);
                break;
            case SharedImageTilesType.SMALL:
                setChildViewSize(
                        R.dimen.small_shared_image_tiles_icon_total_height,
                        R.style.TextAppearance_SharedImageTilesSmall,
                        R.dimen.small_shared_image_tiles_text_padding);
                break;
        }
    }

    void setChildViewSize(
            @DimenRes int iconSizeDp, @StyleRes int textStyle, @DimenRes int textPaddingDp) {
        int iconSizePx = mContext.getResources().getDimensionPixelSize(iconSizeDp);
        int textPaddingPx = mContext.getResources().getDimensionPixelSize(textPaddingDp);
        // Loop through all child views.
        for (int i = 0; i < getChildCount(); i++) {
            ViewGroup viewGroup = (ViewGroup) getChildAt(i);
            viewGroup.getLayoutParams().height = iconSizePx;
            viewGroup.setMinimumWidth(iconSizePx);
        }

        // Set sizing for the number tile.
        mCountTileView.setTextAppearance(textStyle);
        mCountTileView.setPadding(
                /* left= */ textPaddingPx,
                /* top= */ 0,
                /* right= */ textPaddingPx,
                /* bottom= */ 0);
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
