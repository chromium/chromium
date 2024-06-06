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
    static final String SEPARATE_COMMA = ", ";
    static final String SEPARATE_PERIOD = ". ";

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
     * Assigns all texts for the "single-tile" case and returns the content description string.
     *
     * @param preInfoText Info to show above main text.
     * @param displayText Main text (page title).
     * @param postInfoText Info to show below main text.
     */
    public String setSuggestionTextsSingle(
            @Nullable String preInfoText, String displayText, String postInfoText) {
        // TODO(b/337858147): Change the visibility and text of TextView R.id.tile_pre_info_text
        // for Tab resumption V2 UX.
        ((TextView) findViewById(R.id.tile_pre_info_text)).setText(preInfoText);
        ((TextView) findViewById(R.id.tile_display_text)).setText(displayText);
        ((TextView) findViewById(R.id.tile_post_info_text)).setText(postInfoText);

        StringBuilder stringBuilder = new StringBuilder();
        if (preInfoText != null) {
            stringBuilder.append(preInfoText);
            stringBuilder.append(SEPARATE_COMMA);
        }
        stringBuilder.append(displayText);
        stringBuilder.append(SEPARATE_COMMA);
        stringBuilder.append(postInfoText);
        stringBuilder.append(SEPARATE_PERIOD);

        String contentDescription = stringBuilder.toString();
        setContentDescription(contentDescription);
        return contentDescription;
    }

    /**
     * Assigns all texts for the "multi-tile" case and returns the content description string.
     *
     * @param displayText Main text (page title).
     * @param infoText Info to show below main text.
     */
    public String setSuggestionTextsMulti(String displayText, String infoText) {
        ((TextView) findViewById(R.id.tile_display_text)).setText(displayText);
        ((TextView) findViewById(R.id.tile_info_text)).setText(infoText);

        StringBuilder stringBuilder = new StringBuilder();
        stringBuilder.append(displayText);
        stringBuilder.append(SEPARATE_COMMA);
        stringBuilder.append(infoText);
        stringBuilder.append(SEPARATE_PERIOD);

        String contentDescription = stringBuilder.toString();
        setContentDescription(contentDescription);
        return contentDescription;
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
