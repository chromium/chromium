// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.drawable.Drawable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.MathUtils;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tasks.pseudotab.PseudoTab;
import org.chromium.chrome.tab_ui.R;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/**
 * A {@link TabListMediator.ThumbnailProvider} that will create a single Bitmap Thumbnail for all
 * the related tabs for the given tabs.
 */
public class MultiThumbnailCardProvider implements TabListMediator.ThumbnailProvider {
    private final TabContentManager mTabContentManager;
    private final TabModelSelector mTabModelSelector;
    private final TabModelSelectorObserver mTabModelSelectorObserver;

    private final float mRadius;
    private final float mFaviconCirclePadding;
    private final int mThumbnailWidth;
    private final int mThumbnailHeight;
    private final Paint mEmptyThumbnailPaint;
    private final Paint mThumbnailFramePaint;
    private final Paint mTextPaint;
    private final Paint mFaviconBackgroundPaint;
    private final int mFaviconBackgroundPaintColor;
    private final List<Rect> mFaviconRects = new ArrayList<>(4);
    private final List<RectF> mThumbnailRects = new ArrayList<>(4);
    private final List<RectF> mFaviconBackgroundRects = new ArrayList<>(4);
    private TabListFaviconProvider mTabListFaviconProvider;

    private class MultiThumbnailFetcher {
        private final PseudoTab mInitialTab;
        private final Callback<Bitmap> mFinalCallback;
        private final boolean mForceUpdate;
        private final boolean mWriteToCache;
        private final List<PseudoTab> mTabs = new ArrayList<>(4);
        private final AtomicInteger mThumbnailsToFetch = new AtomicInteger();

        private Canvas mCanvas;
        private Bitmap mMultiThumbnailBitmap;
        private String mText;

        /**
         * @see TabContentManager#getTabThumbnailWithCallback
         */
        MultiThumbnailFetcher(PseudoTab initialTab, Callback<Bitmap> finalCallback,
                boolean forceUpdate, boolean writeToCache) {
            mFinalCallback = finalCallback;
            mInitialTab = initialTab;
            mForceUpdate = forceUpdate;
            mWriteToCache = writeToCache;
        }

        private void initializeAndStartFetching(PseudoTab tab) {
            // Initialize mMultiThumbnailBitmap.
            int width = mThumbnailWidth;
            int height = mThumbnailHeight;
            mMultiThumbnailBitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
            mCanvas = new Canvas(mMultiThumbnailBitmap);
            mCanvas.drawColor(Color.TRANSPARENT);

            // Initialize Tabs.
            List<PseudoTab> relatedTabList = PseudoTab.getRelatedTabs(tab, mTabModelSelector);
            if (relatedTabList.size() <= 4) {
                mThumbnailsToFetch.set(relatedTabList.size());

                mTabs.add(tab);
                relatedTabList.remove(tab);

                for (int i = 0; i < 3; i++) {
                    mTabs.add(i < relatedTabList.size() ? relatedTabList.get(i) : null);
                }
            } else {
                mText = "+" + (relatedTabList.size() - 3);
                mThumbnailsToFetch.set(3);

                mTabs.add(tab);
                relatedTabList.remove(tab);

                mTabs.add(relatedTabList.get(0));
                mTabs.add(relatedTabList.get(1));
                mTabs.add(null);
            }

            // Fetch and draw all.
            for (int i = 0; i < 4; i++) {
                if (mTabs.get(i) != null) {
                    final int index = i;
                    final String url = mTabs.get(i).getUrl();
                    final boolean isIncognito = mTabs.get(i).isIncognito();
                    // getTabThumbnailWithCallback() might call the callback up to twice,
                    // so use |lastFavicon| to avoid fetching the favicon the second time.
                    // Fetching the favicon after getting the live thumbnail would lead to
                    // visible flicker.
                    final AtomicReference<Drawable> lastFavicon = new AtomicReference<>();
                    mTabContentManager.getTabThumbnailWithCallback(
                            mTabs.get(i).getId(), thumbnail -> {
                                drawThumbnailBitmapOnCanvasWithFrame(thumbnail, index);
                                if (lastFavicon.get() != null) {
                                    drawFaviconThenMaybeSendBack(lastFavicon.get(), index);
                                } else {
                                    mTabListFaviconProvider.getFaviconForUrlAsync(
                                            url, isIncognito, (Drawable favicon) -> {
                                                lastFavicon.set(favicon);
                                                drawFaviconThenMaybeSendBack(favicon, index);
                                            });
                                }
                            }, mForceUpdate && i == 0, mWriteToCache && i == 0);
                } else {
                    drawThumbnailBitmapOnCanvasWithFrame(null, i);
                    if (mText != null && i == 3) {
                        // Draw the text exactly centered on the thumbnail rect.
                        mCanvas.drawText(mText,
                                (mThumbnailRects.get(i).left + mThumbnailRects.get(i).right) / 2,
                                (mThumbnailRects.get(i).top + mThumbnailRects.get(i).bottom) / 2
                                        - ((mTextPaint.descent() + mTextPaint.ascent()) / 2),
                                mTextPaint);
                    }
                }
            }
        }

        private void drawThumbnailBitmapOnCanvasWithFrame(Bitmap thumbnail, int index) {
            // Draw the rounded rect. If Bitmap is not null, this is used for XferMode.
            mCanvas.drawRoundRect(
                    mThumbnailRects.get(index), mRadius, mRadius, mEmptyThumbnailPaint);

            if (thumbnail == null) return;

            thumbnail =
                    Bitmap.createScaledBitmap(thumbnail, (int) mThumbnailRects.get(index).width(),
                            (int) mThumbnailRects.get(index).height(), true);

            mEmptyThumbnailPaint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.SRC_IN));
            mCanvas.drawBitmap(thumbnail,
                    new Rect(0, 0, thumbnail.getWidth(), thumbnail.getHeight()),
                    mThumbnailRects.get(index), mEmptyThumbnailPaint);
            thumbnail.recycle();

            mEmptyThumbnailPaint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.SRC_OVER));

            mCanvas.drawRoundRect(
                    mThumbnailRects.get(index), mRadius, mRadius, mThumbnailFramePaint);
        }

        private void drawFaviconDrawableOnCanvasWithFrame(Drawable favicon, int index) {
            RectF rectF = mFaviconBackgroundRects.get(index);
            mCanvas.drawCircle((rectF.left + rectF.right) / 2, (rectF.bottom + rectF.top) / 2,
                    rectF.width() / 2 - mFaviconCirclePadding, mFaviconBackgroundPaint);
            favicon.setBounds(mFaviconRects.get(index));
            favicon.draw(mCanvas);
        }

        private void drawFaviconThenMaybeSendBack(Drawable favicon, int index) {
            drawFaviconDrawableOnCanvasWithFrame(favicon, index);
            if (mThumbnailsToFetch.decrementAndGet() == 0) {
                PostTask.postTask(UiThreadTaskTraits.USER_VISIBLE,
                        mFinalCallback.bind(mMultiThumbnailBitmap));
            }
        }

        private void fetch() {
            initializeAndStartFetching(mInitialTab);
        }
    }

    MultiThumbnailCardProvider(Context context, TabContentManager tabContentManager,
            TabModelSelector tabModelSelector) {
        Resources resource = context.getResources();
        float expectedThumbnailAspectRatio =
                (float) TabUiFeatureUtilities.THUMBNAIL_ASPECT_RATIO.getValue();
        expectedThumbnailAspectRatio = MathUtils.clamp(expectedThumbnailAspectRatio, 0.5f, 2.0f);

        mThumbnailWidth = (int) resource.getDimension(R.dimen.tab_grid_thumbnail_card_default_size);
        mThumbnailHeight = (int) (mThumbnailWidth / expectedThumbnailAspectRatio);

        mTabContentManager = tabContentManager;
        mTabModelSelector = tabModelSelector;
        mRadius = resource.getDimension(R.dimen.tab_list_mini_card_radius);
        mFaviconCirclePadding =
                resource.getDimension(R.dimen.tab_grid_thumbnail_favicon_background_padding);

        mTabListFaviconProvider = new TabListFaviconProvider(context, false);

        // Initialize Paints to use.
        mEmptyThumbnailPaint = new Paint();
        mEmptyThumbnailPaint.setStyle(Paint.Style.FILL);
        mEmptyThumbnailPaint.setColor(ApiCompatibilityUtils.getColor(
                resource, R.color.tab_list_mini_card_default_background_color));
        mEmptyThumbnailPaint.setAntiAlias(true);

        mThumbnailFramePaint = new Paint();
        mThumbnailFramePaint.setStyle(Paint.Style.STROKE);
        mThumbnailFramePaint.setStrokeWidth(
                resource.getDimension(R.dimen.tab_list_mini_card_frame_size));
        mThumbnailFramePaint.setColor(
                ApiCompatibilityUtils.getColor(resource, R.color.divider_line_bg_color));
        mThumbnailFramePaint.setAntiAlias(true);

        // TODO(996048): Use pre-defined styles to avoid style out of sync if any text/color styles
        // changes.
        mTextPaint = new Paint();
        mTextPaint.setTextSize(resource.getDimension(R.dimen.compositor_tab_title_text_size));
        mTextPaint.setFakeBoldText(true);
        mTextPaint.setAntiAlias(true);
        mTextPaint.setTextAlign(Paint.Align.CENTER);
        mTextPaint.setColor(ApiCompatibilityUtils.getColor(resource, R.color.default_text_color));

        mFaviconBackgroundPaintColor =
                ApiCompatibilityUtils.getColor(resource, R.color.favicon_background_color);
        mFaviconBackgroundPaint = new Paint();
        mFaviconBackgroundPaint.setAntiAlias(true);
        mFaviconBackgroundPaint.setColor(mFaviconBackgroundPaintColor);
        mFaviconBackgroundPaint.setStyle(Paint.Style.FILL);
        mFaviconBackgroundPaint.setShadowLayer(
                resource.getDimension(R.dimen.tab_grid_thumbnail_favicon_background_radius), 0,
                resource.getDimension(R.dimen.tab_grid_thumbnail_favicon_background_down_shift),
                resource.getColor(R.color.modern_grey_800_alpha_38));

        // Initialize Rects for thumbnails.
        float thumbnailHorizontalPadding = resource.getDimension(R.dimen.tab_list_card_padding);
        float thumbnailVerticalPadding = thumbnailHorizontalPadding / expectedThumbnailAspectRatio;
        float thumbnailFaviconBackgroundPadding =
                resource.getDimension(R.dimen.tab_grid_thumbnail_favicon_frame_padding);
        float thumbnailFaviconPaddingFromBackground =
                resource.getDimension(R.dimen.tab_grid_thumbnail_favicon_padding_from_frame);
        float centerX = mThumbnailWidth * 0.5f;
        float centerY = mThumbnailHeight * 0.5f;
        float halfThumbnailHorizontalPadding = thumbnailHorizontalPadding / 2;
        float halfThumbnailVerticalPadding = thumbnailVerticalPadding / 2;

        mThumbnailRects.add(new RectF(thumbnailHorizontalPadding, thumbnailVerticalPadding,
                centerX - halfThumbnailHorizontalPadding, centerY - halfThumbnailVerticalPadding));
        mThumbnailRects.add(new RectF(centerX + halfThumbnailHorizontalPadding,
                thumbnailVerticalPadding, mThumbnailWidth - thumbnailHorizontalPadding,
                centerY - halfThumbnailVerticalPadding));
        mThumbnailRects.add(new RectF(thumbnailHorizontalPadding,
                centerY + halfThumbnailVerticalPadding, centerX - halfThumbnailHorizontalPadding,
                mThumbnailHeight - thumbnailVerticalPadding));
        mThumbnailRects.add(new RectF(centerX + halfThumbnailHorizontalPadding,
                centerY + halfThumbnailVerticalPadding,
                mThumbnailWidth - thumbnailHorizontalPadding,
                mThumbnailHeight - thumbnailVerticalPadding));

        // Initialize Rects for favicons and favicon backgrounds.
        final float faviconBackgroundRadius =
                mThumbnailRects.get(0).width() / 2f - thumbnailFaviconBackgroundPadding;
        for (int i = 0; i < 4; i++) {
            RectF thumbnailRect = mThumbnailRects.get(i);

            float thumbnailRectCenterX = thumbnailRect.centerX();
            float thumbnailRectCenterY = thumbnailRect.centerY();
            RectF faviconBackgroundRect = new RectF(thumbnailRectCenterX, thumbnailRectCenterY,
                    thumbnailRectCenterX, thumbnailRectCenterY);
            faviconBackgroundRect.inset(-faviconBackgroundRadius, -faviconBackgroundRadius);
            mFaviconBackgroundRects.add(faviconBackgroundRect);

            RectF faviconRectF = new RectF(faviconBackgroundRect);
            faviconRectF.inset(
                    thumbnailFaviconPaddingFromBackground, thumbnailFaviconPaddingFromBackground);
            Rect faviconRect = new Rect();
            faviconRectF.roundOut(faviconRect);
            mFaviconRects.add(faviconRect);
        }

        mTabModelSelectorObserver = new TabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                boolean isIncognito = newModel.isIncognito();
                mEmptyThumbnailPaint.setColor(
                        TabUiColorProvider.getMiniThumbnailPlaceHolderColor(context, isIncognito));
                mThumbnailFramePaint.setColor(
                        TabUiColorProvider.getMiniThumbnailFrameColor(context, isIncognito));
                mTextPaint.setColor(TabUiColorProvider.getTitleTextColor(context, isIncognito));
                mFaviconBackgroundPaint.setColor(
                        TabUiColorProvider.getFaviconBackgroundColor(context, isIncognito));
            }
        };
        mTabModelSelector.addObserver(mTabModelSelectorObserver);
    }

    public void initWithNative() {
        Profile profile = mTabModelSelector.getCurrentModel().getProfile();
        mTabListFaviconProvider.initWithNative(profile);
    }

    /**
     * Destroy any member that needs clean up.
     */
    public void destroy() {
        mTabModelSelector.removeObserver(mTabModelSelectorObserver);
    }

    @Override
    public void getTabThumbnailWithCallback(
            int tabId, Callback<Bitmap> finalCallback, boolean forceUpdate, boolean writeToCache) {
        PseudoTab tab = PseudoTab.fromTabId(tabId);
        if (tab == null || PseudoTab.getRelatedTabs(tab, mTabModelSelector).size() == 1) {
            mTabContentManager.getTabThumbnailWithCallback(
                    tabId, finalCallback, forceUpdate, writeToCache);
            return;
        }

        new MultiThumbnailFetcher(tab, finalCallback, forceUpdate, writeToCache).fetch();
    }
}
