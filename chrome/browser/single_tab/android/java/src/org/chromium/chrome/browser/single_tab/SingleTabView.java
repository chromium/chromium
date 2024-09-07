// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.single_tab;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Matrix;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageView;
import android.widget.ImageView.ScaleType;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.magic_stack.HomeModulesMetricsUtils;
import org.chromium.chrome.browser.tab_ui.TabThumbnailView;

/** View of the tab on the single tab tab switcher. */
class SingleTabView extends LinearLayout {
    @Nullable private TextView mSeeMoreLinkView;
    private ImageView mFavicon;
    private TextView mTitle;
    @Nullable private TabThumbnailView mTabThumbnail;
    @Nullable private TextView mUrl;

    /** Default constructor needed to inflate via XML. */
    public SingleTabView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mSeeMoreLinkView = findViewById(R.id.tab_switcher_see_more_link);
        mFavicon = findViewById(R.id.tab_favicon_view);
        mTitle = findViewById(R.id.tab_title_view);
        mTabThumbnail = findViewById(R.id.tab_thumbnail);
        mUrl = findViewById(R.id.tab_url_view);

        if (mTabThumbnail != null) {
            if (HomeModulesMetricsUtils.useMagicStack()) {
                Resources resources = getResources();
                MarginLayoutParams marginLayoutParams =
                        (MarginLayoutParams) mTabThumbnail.getLayoutParams();
                int size =
                        resources.getDimensionPixelSize(
                                R.dimen.single_tab_module_tab_thumbnail_size_big);
                marginLayoutParams.width = size;
                marginLayoutParams.height = size;

                TextView tabSwitcherTitleDescription =
                        findViewById(R.id.tab_switcher_title_description);
                MarginLayoutParams titleDescriptionMarginLayoutParams =
                        (MarginLayoutParams) tabSwitcherTitleDescription.getLayoutParams();
                titleDescriptionMarginLayoutParams.bottomMargin =
                        resources.getDimensionPixelSize(
                                R.dimen.single_tab_module_title_margin_bottom);
                tabSwitcherTitleDescription.setText(
                        resources.getQuantityString(
                                R.plurals.home_modules_tab_resumption_title, 1));
            }
            mTabThumbnail.setScaleType(ScaleType.MATRIX);
            mTabThumbnail.updateThumbnailPlaceholder(
                    /* isIncognito= */ false, /* isSelected= */ false);
        }
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        if (mTabThumbnail == null) return;

        Drawable drawable = mTabThumbnail.getDrawable();
        if (drawable == null) return;

        updateThumbnailMatrix(drawable);
    }

    /**
     * Set the listener for "See more" link, which gets shown if `listener` is non-null.
     *
     * @param listener The given listener.
     */
    public void setOnSeeMoreLinkClickListener(@Nullable Runnable listener) {
        if (mSeeMoreLinkView != null) {
            mSeeMoreLinkView.setVisibility((listener != null) ? View.VISIBLE : View.GONE);
            if (listener != null) {
                mSeeMoreLinkView.setOnClickListener(v -> listener.run());
            }
        }
    }

    /**
     * Set the favicon.
     *
     * @param favicon The given favicon {@link Drawable}.
     */
    public void setFavicon(Drawable favicon) {
        mFavicon.setImageDrawable(favicon);
    }

    /**
     * Set the Tab thumbnail.
     *
     * @param thumbnail The given Tab thumbnail {@link Drawable}.
     */
    public void setTabThumbnail(@Nullable Drawable thumbnail) {
        if (mTabThumbnail == null) return;

        if (thumbnail == null
                || thumbnail.getIntrinsicWidth() <= 0
                || thumbnail.getIntrinsicHeight() <= 0) {
            mTabThumbnail.setImageMatrix(new Matrix());
            return;
        }
        mTabThumbnail.setImageDrawable(thumbnail);

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

    private void updateThumbnailMatrix(Drawable thumbnail) {
        final int width = mTabThumbnail.getMeasuredWidth();
        final int height = mTabThumbnail.getMeasuredHeight();
        if (width == 0 || height == 0) {
            mTabThumbnail.setImageMatrix(new Matrix());
            return;
        }

        final float scale =
                Math.max(
                        (float) width / thumbnail.getIntrinsicWidth(),
                        (float) height / thumbnail.getIntrinsicHeight());
        final int xOffset = (int) (width - thumbnail.getIntrinsicWidth() * scale) / 2;

        Matrix m = new Matrix();
        m.setScale(scale, scale);
        m.postTranslate(xOffset, 0);
        mTabThumbnail.setImageMatrix(m);
    }
}
