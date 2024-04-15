// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.widget.ImageView;
import android.widget.RelativeLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleMetricsUtils.ClickInfo;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleUtils.SuggestionClickCallbacks;

/**
 * The view for a tab suggestion tile. These tile comes in two variants: A larger one for the
 * "single-tile" case, and a smaller one for the "multi-tile" case.
 */
public class TabResumptionTileView extends RelativeLayout {
    public TabResumptionTileView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
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
        ((ImageView) findViewById(R.id.tile_icon)).setImageDrawable(drawable);
    }

    /** Binds the click handler with an associated URL. */
    public void bindSuggestionClickCallback(
            SuggestionClickCallbacks callbacks,
            SuggestionEntry entry,
            int tileCount,
            int tileIndex) {
        setOnClickListener(
                v -> {
                    @ClickInfo
                    int clickInfo =
                            TabResumptionModuleMetricsUtils.computeClickInfo(tileCount, tileIndex);
                    TabResumptionModuleMetricsUtils.recordClickInfo(clickInfo);
                    if (entry instanceof LocalTabSuggestionEntry) {
                        callbacks.onSuggestionClickByTabId(
                                ((LocalTabSuggestionEntry) entry).tab.getId());
                    } else {
                        callbacks.onSuggestionClickByUrl(entry.url);
                    }
                });
        // Handle and return false to avoid obstructing long click handling of containing Views.
        setOnLongClickListener(v -> false);
    }
}
