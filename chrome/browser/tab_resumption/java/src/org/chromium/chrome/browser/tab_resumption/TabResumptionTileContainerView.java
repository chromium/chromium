// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleUtils.SuggestionClickCallback;

/** The view containing suggestion tiles on the tab resumption module. */
public class TabResumptionTileContainerView extends LinearLayout {

    public TabResumptionTileContainerView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void removeAllViews() {
        // Children alternate between tile and divider. Call destroy() on tiles.
        for (int i = 0; i < getChildCount(); ++i) {
            View view = getChildAt(i);
            if (view instanceof TabResumptionTileView) {
                ((TabResumptionTileView) view).destroy();
            }
        }
        super.removeAllViews();
    }

    public void destroy() {
        removeAllViews();
    }

    /**
     * Adds all new {@link TabResumptionTileView} instances, after removing existing ones and
     * returns the text of all instances.
     */
    public String renderAllTiles(
            SuggestionBundle bundle,
            UrlImageProvider urlImageProvider,
            SuggestionClickCallback suggestionClickCallback) {
        removeAllViews();

        String allTilesTexts = "";
        boolean isSingle = bundle.entries.size() == 1;
        for (SuggestionEntry entry : bundle.entries) {
            // Add divider if some tile already exists.
            if (getChildCount() > 0) {
                View divider =
                        (View)
                                LayoutInflater.from(getContext())
                                        .inflate(
                                                R.layout.tab_resumption_module_divider,
                                                this,
                                                false);
                addView(divider);
            }
            int layoutId =
                    isSingle
                            ? R.layout.tab_resumption_module_single_tile_layout
                            : R.layout.tab_resumption_module_multi_tile_layout;
            TabResumptionTileView tileView =
                    (TabResumptionTileView)
                            LayoutInflater.from(getContext()).inflate(layoutId, this, false);
            allTilesTexts +=
                    loadTileTexts(entry, bundle.referenceTimeMs, isSingle, tileView) + ". ";
            loadTileUrlImage(entry, urlImageProvider, tileView);
            tileView.bindSuggestionClickCallback(suggestionClickCallback, entry.url);
            addView(tileView);
        }
        return allTilesTexts;
    }

    /** Renders and returns the texts of a {@link TabResumptionTileView}. */
    private String loadTileTexts(
            SuggestionEntry entry,
            long referenceTimeMs,
            boolean isSingle,
            TabResumptionTileView tileView) {
        Resources res = getContext().getResources();
        String recencyString =
                TabResumptionModuleUtils.getRecencyString(
                        getResources(), referenceTimeMs - entry.lastActiveTime);
        if (isSingle) {
            String preInfoText =
                    res.getString(R.string.tab_resumption_module_single_pre_info, entry.sourceName);
            String postInfoText =
                    res.getString(
                            R.string.tab_resumption_module_single_post_info,
                            recencyString,
                            entry.url.getHost());
            tileView.setSuggestionTextsSingle(preInfoText, entry.title, postInfoText);
            return preInfoText + ", " + entry.title + ", " + postInfoText;
        } else {
            String infoText =
                    res.getString(
                            R.string.tab_resumption_module_multi_info,
                            recencyString,
                            entry.sourceName);
            tileView.setSuggestionTextsMulti(entry.title, infoText);
            return entry.title + ", " + infoText;
        }
    }

    /** Loads the main URL image of a {@link TabResumptionTileView}. */
    private void loadTileUrlImage(
            SuggestionEntry entry,
            UrlImageProvider urlImageProvider,
            TabResumptionTileView tileView) {
        Drawable storedUrlDrawable = entry.getUrlDrawable();
        if (storedUrlDrawable != null) {
            // Use stored Drawable if available.
            tileView.setImageDrawable(storedUrlDrawable);
        } else {
            // Otherwise fetch URL image, convert to Drawable, then use and store.
            urlImageProvider.fetchImageForUrl(
                    entry.url,
                    (Bitmap bitmap) -> {
                        Resources res = getContext().getResources();
                        Drawable urlDrawable = new BitmapDrawable(res, bitmap);
                        entry.setUrlDrawable(urlDrawable);
                        tileView.setImageDrawable(urlDrawable);
                    });
        }
    }
}
