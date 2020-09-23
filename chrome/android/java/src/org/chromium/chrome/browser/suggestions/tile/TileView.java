// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.R;

/**
 * The view for a tile with icon and text.
 *
 * Displays the title of the site beneath a large icon.
 */
public class TileView extends FrameLayout {
    private ImageView mBadgeView;
    private TextView mTitleView;
    protected ImageView mIconView;

    /**
     * Constructor for inflating from XML.
     */
    public TileView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mIconView = findViewById(R.id.tile_view_icon);
        mBadgeView = findViewById(R.id.offline_badge);
        mTitleView = findViewById(R.id.tile_view_title);
    }

    /**
     * Initializes the view. Non-MVC components should call this immediately after inflation.
     *
     * @param title The title of the tile.
     * @param showOfflineBadge Whether to show the offline badge.
     * @param icon The icon to display on the tile.
     * @param titleLines The number of text lines to use for the tile title.
     */
    void initialize(String title, boolean showOfflineBadge, Drawable icon, int titleLines) {
        setOfflineBadgeVisibility(showOfflineBadge);
        setIconDrawable(icon);
        setTitle(title, titleLines);
    }

    /**
     * Renders the icon or clears it from the view if the icon is null.
     */
    public void setIconDrawable(Drawable icon) {
        mIconView.setImageDrawable(icon);
    }

    /** Shows or hides the offline badge to reflect the offline availability. */
    void setOfflineBadgeVisibility(boolean showOfflineBadge) {
        mBadgeView.setVisibility(showOfflineBadge ? VISIBLE : GONE);
    }

    /** Sets the title text and number lines. */
    public void setTitle(String title, int titleLines) {
        mTitleView.setLines(titleLines);
        mTitleView.setText(title);
    }
}
