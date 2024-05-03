// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.widget.RelativeLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.widget.RoundedCornerImageView;

/**
 * The view for a tab suggestion tile. These tile comes in two variants: A larger one for the
 * "single-tile" case, and a smaller one for the "multi-tile" case.
 */
public class TabResumptionTileView extends RelativeLayout {
    private RoundedCornerImageView mIconView;
    private final int mSalientImageCornerRadiusPx;

    public TabResumptionTileView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        mSalientImageCornerRadiusPx =
                context.getResources()
                        .getDimensionPixelSize(
                                R.dimen.tab_resumption_module_icon_rounded_corner_radius);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mIconView = findViewById(R.id.tile_icon);
    }

    void destroy() {
        setOnLongClickListener(null);
        setOnClickListener(null);
    }

    /**
     * Assigns all texts for the "single-tile" case.
     *
     * @param preInfoText Info to show above main text.
     * @param displayText Main text (page title).
     * @param postInfoText Info to show below main text.
     */
    public void setSuggestionTextsSingle(
            String preInfoText, String displayText, String postInfoText) {
        ((TextView) findViewById(R.id.tile_pre_info_text)).setText(preInfoText);
        ((TextView) findViewById(R.id.tile_display_text)).setText(displayText);
        ((TextView) findViewById(R.id.tile_post_info_text)).setText(postInfoText);
        setContentDescription(preInfoText + ", " + displayText + ", " + postInfoText);
    }

    /**
     * Assigns all texts for the "multi-tile" case.
     *
     * @param displayText Main text (page title).
     * @param postInfoText Info to show below main text.
     */
    public void setSuggestionTextsMulti(String displayText, String infoText) {
        ((TextView) findViewById(R.id.tile_display_text)).setText(displayText);
        ((TextView) findViewById(R.id.tile_info_text)).setText(infoText);
        setContentDescription(displayText + ", " + infoText);
    }

    /** Assigns the main URL image. */
    public void setImageDrawable(Drawable drawable) {
        mIconView.setImageDrawable(drawable);
    }

    /** Updates the image view to show a salient image. */
    public void updateForSalientImage() {
        mIconView.setPadding(0, 0, 0, 0);
        mIconView.setRoundedCorners(
                mSalientImageCornerRadiusPx,
                mSalientImageCornerRadiusPx,
                mSalientImageCornerRadiusPx,
                mSalientImageCornerRadiusPx);
    }
}
