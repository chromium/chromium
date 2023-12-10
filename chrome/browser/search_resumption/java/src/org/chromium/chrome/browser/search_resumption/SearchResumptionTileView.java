// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_resumption;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.RelativeLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.core.content.ContextCompat;

import org.chromium.chrome.browser.search_resumption.SearchResumptionTileBuilder.OnSuggestionClickCallback;
import org.chromium.url.GURL;

/** The view for a search suggestion tile. */
public class SearchResumptionTileView extends RelativeLayout {
    private GURL mGurl;
    private TextView mTileContent;

    public SearchResumptionTileView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTileContent = findViewById(R.id.tile_content);
    }

    /** Updates the content of the tile. */
    void updateSuggestionData(GURL gUrl, String displayText) {
        mGurl = gUrl;
        mTileContent.setText(displayText);
        setContentDescription(mTileContent.getText());
    }

    void addOnSuggestionClickCallback(OnSuggestionClickCallback callback) {
        setOnClickListener(v -> callback.onSuggestionClick(mGurl));
    }

    /**
     * Updates the background image according to the position of the tile.
     * @param index The index of the tile in its parent ViewGroup.
     * @param totalCount The total child number of the parent ViewGroup.
     */
    void mayUpdateBackground(int index, int totalCount) {
        if (index == 0) {
            setBackground(
                    ContextCompat.getDrawable(
                            getContext(), R.drawable.search_resumption_module_background_top));
        } else if (index == totalCount - 1) {
            setBackground(
                    ContextCompat.getDrawable(
                            getContext(), R.drawable.search_resumption_module_background_bottom));
        }
    }

    void destroy() {
        setOnClickListener(null);
    }

    String getTextForTesting() {
        return mTileContent.getText().toString();
    }
}
