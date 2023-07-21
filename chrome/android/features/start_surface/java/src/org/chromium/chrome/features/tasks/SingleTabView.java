// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.tasks;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.graphics.drawable.BitmapDrawable;
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
            mTabThumbnail.setScaleType(ScaleType.MATRIX);
            mTabThumbnail.updateThumbnailPlaceholder(/*isIncognito=*/false, /*isSelected=*/false);
        }
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        if (mTabThumbnail == null) return;

        Bitmap thumbnail = null;
        Drawable drawable = mTabThumbnail.getDrawable();
        if (drawable instanceof BitmapDrawable) {
            thumbnail = ((BitmapDrawable) drawable).getBitmap();
        }
        if (thumbnail == null) return;

        updateThumbnailMatrix(thumbnail);
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

        if (thumbnail == null || thumbnail.getWidth() <= 0 || thumbnail.getHeight() <= 0) {
            mTabThumbnail.setImageMatrix(new Matrix());
            return;
        }
        mTabThumbnail.setImageBitmap(thumbnail);

        updateThumbnailMatrix(thumbnail);
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

    private void updateThumbnailMatrix(Bitmap thumbnail) {
        final int width = mTabThumbnail.getMeasuredWidth();
        final int height = mTabThumbnail.getMeasuredHeight();
        if (width == 0 || height == 0) {
            mTabThumbnail.setImageMatrix(new Matrix());
            return;
        }

        final float scale = Math.max(
                (float) width / thumbnail.getWidth(), (float) height / thumbnail.getHeight());
        final int xOffset = (int) (width - thumbnail.getWidth() * scale) / 2;

        Matrix m = new Matrix();
        m.setScale(scale, scale);
        m.postTranslate(xOffset, 0);
        mTabThumbnail.setImageMatrix(m);
    }
}
