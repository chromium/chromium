// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.ark.browser.tab;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapShader;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.drawable.Drawable;
import android.util.Size;

import androidx.annotation.ColorInt;

import com.ark.browser.core.ArkWebManager;
import com.ark.browser.tab.core.ITab;
import com.ark.browser.tab.core.ITabGroup;
import com.zpj.utils.ScreenUtils;

import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/**
 * A {@link ThumbnailProvider} that will create a single Bitmap Thumbnail for all
 * the related tabs for the given tabs.
 */
public class MultiThumbnailCardProvider implements ThumbnailProvider {
    private final TabContentManager mTabContentManager;

    private final float mRadius;
    private final float mFaviconFrameCornerRadius;
    private final Paint mEmptyThumbnailPaint;
    private final Paint mThumbnailFramePaint;
    private final Paint mThumbnailBasePaint;
    private final Paint mTextPaint;
    private final Paint mFaviconBackgroundPaint;
    private final Paint mSelectedEmptyThumbnailPaint;
    private final Paint mSelectedTextPaint;
    private final int mFaviconBackgroundPaintColor;
    private TabListFaviconProvider mTabListFaviconProvider;
    private Context mContext;

    private class MultiThumbnailFetcher {

        private static final int MAX_TAB_COUNT = 6;

        private final ITabGroup mInitialTab;
        private final Callback<Bitmap> mFinalCallback;
        private final boolean mForceUpdate;
        private final boolean mWriteToCache;
        private final boolean mIsTabSelected;
        private final List<ITab> mTabs = new ArrayList<>(MAX_TAB_COUNT);
        private final AtomicInteger mThumbnailsToFetch = new AtomicInteger();

        private Canvas mCanvas;
        private Bitmap mMultiThumbnailBitmap;
        private String mText;

        private final List<Rect> mFaviconRects = new ArrayList<>(MAX_TAB_COUNT);
        private final List<RectF> mThumbnailRects = new ArrayList<>(MAX_TAB_COUNT);
        private final List<RectF> mFaviconBackgroundRects = new ArrayList<>(MAX_TAB_COUNT);
        private final int mThumbnailWidth;
        private final int mThumbnailHeight;

        /**
         * Fetcher that get the thumbnail drawable depending on if the tab is selected.
         * @see TabContentManager#getTabThumbnailWithCallback
         * @param initialTab Thumbnail is generated for tabs related to initialTab.
         * @param thumbnailSize Desired size of multi-thumbnail.
         * @param finalCallback Callback which receives generated bitmap.
         * @param forceUpdate, writeToCache Required for bitmap generator.
         * @param isTabSelected Whether the thumbnail is for a currently selected tab.
         */
        MultiThumbnailFetcher(ITabGroup initialTab, Size thumbnailSize,
                              Callback<Bitmap> finalCallback, boolean forceUpdate, boolean writeToCache,
                              boolean isTabSelected) {
            mFinalCallback = finalCallback;
            mInitialTab = initialTab;
            mForceUpdate = forceUpdate;
            mWriteToCache = writeToCache;
            mIsTabSelected = isTabSelected;

            mThumbnailWidth = (int) (ScreenUtils.getScreenWidth() * 0.68f);
            mThumbnailHeight = (int) (ScreenUtils.getScreenHeight() * 0.68f);
        }

        /**
         * Initialize rects used for thumbnails.
         */
        private void initializeRects(Context context) {
            float thumbnailHorizontalPadding = context.getResources()
                    .getDimension(R.dimen.tab_grid_card_thumbnail_margin);
            float thumbnailVerticalPadding = thumbnailHorizontalPadding;

            float centerX = mThumbnailWidth * 0.5f;
            float centerY = mThumbnailHeight * 0.5f;
            float halfThumbnailHorizontalPadding = thumbnailHorizontalPadding / 2;
            float halfThumbnailVerticalPadding = thumbnailVerticalPadding / 2;


            // TODO optimise code
            float height = (mThumbnailHeight - thumbnailHorizontalPadding * 6) / 3;

            float top = 2 * thumbnailHorizontalPadding;
            float bottom = top + height;
            mThumbnailRects.add(new RectF(thumbnailHorizontalPadding, top,
                    centerX - halfThumbnailHorizontalPadding, bottom));

            mThumbnailRects.add(new RectF(centerX + halfThumbnailHorizontalPadding, top,
                    mThumbnailWidth - thumbnailHorizontalPadding, bottom));

            top = bottom + thumbnailHorizontalPadding;
            bottom = top + height;

            mThumbnailRects.add(new RectF(thumbnailHorizontalPadding, top,
                    centerX - halfThumbnailHorizontalPadding, bottom));

            mThumbnailRects.add(new RectF(centerX + halfThumbnailHorizontalPadding, top,
                    mThumbnailWidth - thumbnailHorizontalPadding, bottom));

            top = bottom + thumbnailHorizontalPadding;
            bottom = top + height;

            mThumbnailRects.add(new RectF(thumbnailHorizontalPadding, top,
                    centerX - halfThumbnailHorizontalPadding, bottom));

            mThumbnailRects.add(new RectF(centerX + halfThumbnailHorizontalPadding, top,
                    mThumbnailWidth - thumbnailHorizontalPadding, bottom));


            // Initialize Rects for favicons and favicon frame.
            final float halfFaviconFrameSize = mContext.getResources().getDimension(
                            R.dimen.tab_grid_thumbnail_favicon_frame_size) / 2f;
            float thumbnailFaviconPaddingFromBackground = mContext.getResources().getDimension(
                    R.dimen.tab_grid_thumbnail_favicon_padding_from_frame);
            for (int i = 0; i < MAX_TAB_COUNT; i++) {
                RectF thumbnailRect = mThumbnailRects.get(i);

                float thumbnailRectCenterX = thumbnailRect.centerX();
                float thumbnailRectCenterY = thumbnailRect.centerY();
                RectF faviconBackgroundRect = new RectF(thumbnailRectCenterX, thumbnailRectCenterY,
                        thumbnailRectCenterX, thumbnailRectCenterY);
                faviconBackgroundRect.inset(-halfFaviconFrameSize, -halfFaviconFrameSize);
                mFaviconBackgroundRects.add(faviconBackgroundRect);

                RectF faviconRectF = new RectF(faviconBackgroundRect);
                faviconRectF.inset(thumbnailFaviconPaddingFromBackground,
                        thumbnailFaviconPaddingFromBackground);
                Rect faviconRect = new Rect();
                faviconRectF.roundOut(faviconRect);
                mFaviconRects.add(faviconRect);
            }
        }

        private void initializeAndStartFetching() {
            // Initialize mMultiThumbnailBitmap.
            mMultiThumbnailBitmap =
                    Bitmap.createBitmap(mThumbnailWidth, mThumbnailHeight, Bitmap.Config.ARGB_8888);
            mCanvas = new Canvas(mMultiThumbnailBitmap);
            mCanvas.drawColor(Color.TRANSPARENT);

            // Initialize Tabs.
            List<ITab> relatedTabList = mInitialTab.getTabList();
            if (relatedTabList.size() <= MAX_TAB_COUNT) {
                int thumbCount = relatedTabList.size();
                mThumbnailsToFetch.set(thumbCount);
                mTabs.addAll(relatedTabList);
                if (thumbCount < MAX_TAB_COUNT) {
                    for (int i = 0; i < MAX_TAB_COUNT - thumbCount; i++) {
                        mTabs.add(null);
                    }
                }
            } else {
                int thumbCount = MAX_TAB_COUNT - 1;
                mText = "+" + (relatedTabList.size() - thumbCount);
                mThumbnailsToFetch.set(thumbCount);

                for (int i = 0; i < thumbCount; i++) {
                    mTabs.add(relatedTabList.get(i));
                }
                mTabs.add(null);
            }

            // Fetch and draw all.
            for (int i = 0; i < MAX_TAB_COUNT; i++) {
                if (mTabs.get(i) != null) {
                    final int index = i;
                    PageInfo pageInfo = mTabs.get(i).getCurrentPageInfo();
                    if (pageInfo == null) {
                        drawThumbnailBitmapOnCanvasWithFrame(null, i);
                        continue;
                    }
                    final GURL url = new GURL(pageInfo.getUrl());
                    final boolean isIncognito = pageInfo.isIncognito();
                    // getTabThumbnailWithCallback() might call the callback up to twice,
                    // so use |lastFavicon| to avoid fetching the favicon the second time.
                    // Fetching the favicon after getting the live thumbnail would lead to
                    // visible flicker.
                    final AtomicReference<Drawable> lastFavicon = new AtomicReference<>();
                    mTabContentManager.getTabThumbnailWithCallback(
                            ArkWebManager.getWebContents(pageInfo.getId()), pageInfo.getId(),
                            thumbnail -> {
                                drawThumbnailBitmapOnCanvasWithFrame(thumbnail, index);
                                if (lastFavicon.get() != null) {
                                    drawFaviconThenMaybeSendBack(lastFavicon.get(), index);
                                } else {
                                    mTabListFaviconProvider.getFaviconDrawableForUrlAsync(
                                            url, isIncognito, (Drawable favicon) -> {
                                                lastFavicon.set(favicon);
                                                drawFaviconThenMaybeSendBack(favicon, index);
                                            });
                                }
                            }, mForceUpdate && i == 0, mWriteToCache && i == 0);
                } else {
                    drawThumbnailBitmapOnCanvasWithFrame(null, i);
                    if (mText != null && i == MAX_TAB_COUNT - 1) {
                        // Draw the text exactly centered on the thumbnail rect.
                        Paint textPaint = mIsTabSelected ? mSelectedTextPaint : mTextPaint;
                        mCanvas.drawText(mText,
                                (mThumbnailRects.get(i).left + mThumbnailRects.get(i).right) / 2,
                                (mThumbnailRects.get(i).top + mThumbnailRects.get(i).bottom) / 2
                                        - ((mTextPaint.descent() + mTextPaint.ascent()) / 2),
                                textPaint);
                    }
                }
            }

            if (relatedTabList.isEmpty()) {
                PostTask.postTask(TaskTraits.USER_VISIBLE,
                        mFinalCallback.bind(mMultiThumbnailBitmap));
            }
        }

        private void drawThumbnailBitmapOnCanvasWithFrame(Bitmap thumbnail, int index) {
            final RectF rect = mThumbnailRects.get(index);
            if (thumbnail == null) {
                mEmptyThumbnailPaint.clearShadowLayer();
                Paint emptyThumbnailPaint =
                        mIsTabSelected ? mSelectedEmptyThumbnailPaint : mEmptyThumbnailPaint;
                mCanvas.drawRoundRect(rect, mRadius, mRadius, emptyThumbnailPaint);
                return;
            }

            mEmptyThumbnailPaint.setShadowLayer(
                    mContext.getResources().getDimension(R.dimen.tab_grid_thumbnail_favicon_background_radius), 0,
                    mContext.getResources().getDimension(R.dimen.tab_grid_thumbnail_favicon_background_down_shift),
                    Color.LTGRAY);

            mCanvas.drawRoundRect(rect, mRadius, mRadius, mEmptyThumbnailPaint);

            Matrix m = new Matrix();

            final float newWidth = rect.width();
            final float scale = Math.max(
                    newWidth / thumbnail.getWidth(), rect.height() / thumbnail.getHeight());
            m.setScale(scale, scale);
            final float xOffset =
                    rect.left + (int) ((newWidth - (thumbnail.getWidth() * scale)) / 2);
            final float yOffset = rect.top;
            m.postTranslate(xOffset, yOffset);

            BitmapShader bitmapShader = new BitmapShader(thumbnail, Shader.TileMode.CLAMP, Shader.TileMode.CLAMP);
            bitmapShader.setLocalMatrix(m);
            mThumbnailBasePaint.setShader(bitmapShader);
            mCanvas.drawRoundRect(rect, mRadius, mRadius, mThumbnailBasePaint);

            thumbnail.recycle();
        }

        private void drawFaviconDrawableOnCanvasWithFrame(Drawable favicon, int index) {
            mCanvas.drawRoundRect(mFaviconBackgroundRects.get(index), mFaviconFrameCornerRadius,
                    mFaviconFrameCornerRadius, mFaviconBackgroundPaint);
            favicon.setBounds(mFaviconRects.get(index));
            favicon.draw(mCanvas);
        }

        private void drawFaviconThenMaybeSendBack(Drawable favicon, int index) {
            drawFaviconDrawableOnCanvasWithFrame(favicon, index);
            if (mThumbnailsToFetch.decrementAndGet() == 0) {
                PostTask.postTask(TaskTraits.USER_VISIBLE,
                        mFinalCallback.bind(mMultiThumbnailBitmap));
            }
        }

        private void fetch() {
            initializeRects(mContext);
            initializeAndStartFetching();
        }
    }

    public MultiThumbnailCardProvider(Context context, TabContentManager tabContentManager) {
        mContext = context;
        Resources resources = context.getResources();

        mTabContentManager = tabContentManager;
        mRadius = resources.getDimension(R.dimen.tab_list_mini_card_radius);
        mFaviconFrameCornerRadius =
                resources.getDimension(R.dimen.tab_grid_thumbnail_favicon_frame_corner_radius);

        mTabListFaviconProvider = new TabListFaviconProvider(context, false);

        // Initialize Paints to use.
        mEmptyThumbnailPaint = new Paint();
        mEmptyThumbnailPaint.setStyle(Paint.Style.FILL);
        mEmptyThumbnailPaint.setAntiAlias(true);
        mEmptyThumbnailPaint.setColor(getMiniThumbnailPlaceholderColor(context, false, false));

        mSelectedEmptyThumbnailPaint = new Paint(mEmptyThumbnailPaint);
        mSelectedEmptyThumbnailPaint.setColor(getMiniThumbnailPlaceholderColor(context, false, true));

        // Paint used to set base for thumbnails, in case mEmptyThumbnailPaint has transparency.
        mThumbnailBasePaint = new Paint();
        mThumbnailBasePaint.setStyle(Paint.Style.FILL);
        mThumbnailBasePaint.setAntiAlias(true);
        mThumbnailBasePaint.setColor(Color.BLACK);

        mThumbnailFramePaint = new Paint();
        mThumbnailFramePaint.setStyle(Paint.Style.STROKE);
        mThumbnailFramePaint.setStrokeWidth(
                resources.getDimension(R.dimen.tab_list_mini_card_frame_size));
        mThumbnailFramePaint.setColor(context.getResources().getColor(R.color.divider_line_bg_color_baseline));
        mThumbnailFramePaint.setAntiAlias(true);

        // TODO(996048): Use pre-defined styles to avoid style out of sync if any text/color styles
        // changes.
        mTextPaint = new Paint();
        mTextPaint.setTextSize(resources.getDimension(R.dimen.compositor_tab_title_text_size));
        mTextPaint.setFakeBoldText(true);
        mTextPaint.setAntiAlias(true);
        mTextPaint.setTextAlign(Paint.Align.CENTER);
        mTextPaint.setColor(getTabGroupNumberTextColor(context, false, false));

        mSelectedTextPaint = new Paint(mTextPaint);
        mSelectedTextPaint.setColor(getTabGroupNumberTextColor(context, false, true));

        mFaviconBackgroundPaintColor = Color.WHITE;
        mFaviconBackgroundPaint = new Paint();
        mFaviconBackgroundPaint.setAntiAlias(true);
        mFaviconBackgroundPaint.setColor(mFaviconBackgroundPaintColor);
        mFaviconBackgroundPaint.setStyle(Paint.Style.FILL);
        mFaviconBackgroundPaint.setShadowLayer(
                resources.getDimension(R.dimen.tab_grid_thumbnail_favicon_background_radius), 0,
                resources.getDimension(R.dimen.tab_grid_thumbnail_favicon_background_down_shift),
                resources.getColor(R.color.modern_grey_800_alpha_38));

        // TODO observe changes
//        mTabModelSelectorObserver = new TabModelSelectorObserver() {
//            @Override
//            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
//                boolean isIncognito = newModel.isIncognito();
//                mEmptyThumbnailPaint.setColor(TabUiThemeProvider.getMiniThumbnailPlaceholderColor(
//                        context, isIncognito, false));
//                mTextPaint.setColor(
//                        TabUiThemeProvider.getTabGroupNumberTextColor(context, isIncognito, false));
//                mThumbnailFramePaint.setColor(
//                        TabUiThemeProvider.getMiniThumbnailFrameColor(context, isIncognito));
//                mFaviconBackgroundPaint.setColor(
//                        TabUiThemeProvider.getFaviconBackgroundColor(context, isIncognito));
//
//                mSelectedEmptyThumbnailPaint.setColor(
//                        TabUiThemeProvider.getMiniThumbnailPlaceholderColor(
//                                context, isIncognito, true));
//                mSelectedTextPaint.setColor(
//                        TabUiThemeProvider.getTabGroupNumberTextColor(context, isIncognito, true));
//            }
//        };
//        mTabModelSelector.addObserver(mTabModelSelectorObserver);
    }

    public void initWithNative() {
        mTabListFaviconProvider.initWithNative(Profile.getLastUsedRegularProfile());
    }

    /**
     * Destroy any member that needs clean up.
     */
    public void destroy() {
//        mTabModelSelector.removeObserver(mTabModelSelectorObserver);
    }

    @Override
    public void getTabThumbnailWithCallback(ITab tab, Size thumbnailSize,
                                            Callback<Bitmap> finalCallback, boolean forceUpdate,
                                            boolean writeToCache, boolean isSelected) {
        if (!mTabListFaviconProvider.isInitialized()) {
            initWithNative();
        }
        if (tab instanceof ITabGroup) {
            new MultiThumbnailFetcher((ITabGroup) tab, thumbnailSize, finalCallback,
                    forceUpdate, writeToCache, isSelected).fetch();
        } else {
            if (tab.getCurrentPageInfo() == null) {
                finalCallback.onResult(null);
                return;
            }
            int pageId = tab.getCurrentPageInfo().getId();
            WebContents webContents = null;
            if (forceUpdate) {
                webContents = ArkWebManager.getWebContents(pageId);
            }
            mTabContentManager.getTabThumbnailWithCallback(webContents, pageId, finalCallback,
                    forceUpdate, writeToCache);
        }


//        Tab tab = mTabModelSelector.getTabById(tabId);
//        PseudoTab pseudoTab = (tab != null) ? PseudoTab.fromTab(tab) : PseudoTab.fromTabId(tabId);
//        if (pseudoTab == null
//                || PseudoTab.getRelatedTabs(mContext, pseudoTab, mTabModelSelector).size() == 1) {
//            mTabContentManager.getTabThumbnailWithCallback(
//                    tabId, thumbnailSize, finalCallback, forceUpdate, writeToCache);
//            return;
//        }

    }

    public static @ColorInt int getMiniThumbnailPlaceholderColor(
            Context context, boolean isIncognito, boolean isSelected) {
        if (isIncognito) {
            if (isSelected) {
                return Color.BLACK;
            }
            return Color.parseColor("#666666");
        } else {
            if (isSelected) {
                return Color.LTGRAY;
            }
            return Color.parseColor("#EAEDF4");
        }
    }

    public static @ColorInt int getTabGroupNumberTextColor(
            Context context, boolean isIncognito, boolean isSelected) {
        if (isIncognito) {
            if (isSelected) {
                return Color.LTGRAY;
            }
            return Color.WHITE;
        } else {
            if (isSelected) {
                return Color.BLACK;
            }
            return Color.parseColor("#666666");
        }
    }

}
