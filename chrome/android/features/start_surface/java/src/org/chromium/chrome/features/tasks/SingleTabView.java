// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.tasks;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.widget.ImageView;
import android.widget.ImageView.ScaleType;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.tasks.tab_management.TabGridThumbnailView;
import org.chromium.chrome.start_surface.R;

/** View of the tab on the single tab tab switcher. */
class SingleTabView extends LinearLayout {
    private final Context mContext;
    private ImageView mFavicon;
    private TextView mTitle;
    @Nullable
    private TabGridThumbnailView mTabThumbnail;
    @Nullable
    private TextView mUrl;

    /** Default constructor needed to inflate via XML. */
    public SingleTabView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mFavicon = findViewById(R.id.tab_favicon_view);
        mTitle = findViewById(R.id.tab_title_view);
        mTabThumbnail = findViewById(R.id.tab_thumbnail);
        mUrl = findViewById(R.id.tab_url_view);

        if (mTabThumbnail != null) {
            mTabThumbnail.updateThumbnailPlaceholder(/*isIncognito=*/false, /*isSelected=*/false);
        }
    }

    /**
     * Set the favicon.
     * @param favicon The given favicon {@link Drawable}.
     */
    public void setFavicon(Drawable favicon) {
        mFavicon.setImageDrawable(favicon);
    }

    /**
     * Set the Tab thumbnail.
     * @param thumbnail The given Tab thumbnail {@link Bitmap}.
     */
    public void setTabThumbnail(Bitmap thumbnail) {
        if (mTabThumbnail == null) return;

        mTabThumbnail.setScaleType(ScaleType.MATRIX);
        if (thumbnail == null || thumbnail.getWidth() <= 0 || thumbnail.getHeight() <= 0) {
            mTabThumbnail.setImageMatrix(new Matrix());
            return;
        }

        final int width = mTabThumbnail.getMeasuredWidth();
        final int height = mTabThumbnail.getMeasuredHeight();
        final float scale = Math.max(
                (float) width / thumbnail.getWidth(), (float) height / thumbnail.getHeight());
        final int xOffset = (int) (width - thumbnail.getWidth() * scale) / 2;

        Matrix m = new Matrix();
        m.setScale(scale, scale);
        m.postTranslate(xOffset, 0);
        mTabThumbnail.setImageMatrix(m);
        mTabThumbnail.setImageBitmap(thumbnail);
    }

    /**
     * Set the title.
     * @param title The given title.
     */
    public void setTitle(String title) {
        mTitle.setText(title);
    }

    /**
     * Set the URL.
     * @param url The given URL.
     */
    public void setUrl(String url) {
        mUrl.setText(url);
    }
}
