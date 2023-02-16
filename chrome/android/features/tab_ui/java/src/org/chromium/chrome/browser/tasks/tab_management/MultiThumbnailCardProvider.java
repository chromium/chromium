// Copyright 2019 The Chromium Authors
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
import android.util.Size;

import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tasks.pseudotab.PseudoTab;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.url.GURL;

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
        private final PseudoTab mInitialTab;
        private final Callback<Bitmap> mFinalCallback;
        private final boolean mForceUpdate;
        private final boolean mWriteToCache;
        private final boolean mIsTabSelected;
        private final List<PseudoTab> mTabs = new ArrayList<>(4);
        private final AtomicInteger mThumbnailsToFetch = new AtomicInteger();

        private Canvas mCanvas;
        private Bitmap mMultiThumbnailBitmap;
        private String mText;

        private final List<Rect> mFaviconRects = new ArrayList<>(4);
        private final List<RectF> mThumbnailRects = new ArrayList<>(4);
        private final List<RectF> mFaviconBackgroundRects = new ArrayList<>(4);
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
        MultiThumbnailFetcher(PseudoTab initialTab, Size thumbnailSize,
                Callback<Bitmap> finalCallback, boolean forceUpdate, boolean writeToCache,
                boolean isTabSelected) {
            mFinalCallback = finalCallback;
            mInitialTab = initialTab;
            mForceUpdate = forceUpdate;
            mWriteToCache = writeToCache;
            mIsTabSelected = isTabSelected;

            if (thumbnailSize == null || thumbnailSize.getHeight() <= 0
                    || thumbnailSize.getWidth() <= 0) {
                float expectedThumbnailAspectRatio = TabUtils.getTabThumbnailAspectRatio(mContext);
                mThumbnailWidth = (int) mContext.getResources().getDimension(
                        R.dimen.tab_grid_thumbnail_card_default_size);
                mThumbnailHeight = (int) (mThumbnailWidth / expectedThumbnailAspectRatio);
            } else {
                mThumbnailWidth = thumbnailSize.getWidth();
                mThumbnailHeight = thumbnailSize.getHeight();
            }
        }

        /**
         * Initialize rects used for thumbnails.
         */
        private void initializeRects(Context context) {
            float thumbnailHorizontalPadding =
                    TabUiThemeProvider.getTabMiniThumbnailPaddingDimension(context);
            float thumbnailVerticalPadding = thumbnailHorizontalPadding;

            float centerX = mThumbnailWidth * 0.5f;
            float centerY = mThumbnailHeight * 0.5f;
            float halfThumbnailHorizontalPadding = thumbnailHorizontalPadding / 2;
            float halfThumbnailVerticalPadding = thumbnailVerticalPadding / 2;

            mThumbnailRects.add(new RectF(0, 0, centerX - halfThumbnailHorizontalPadding,
                    centerY - halfThumbnailVerticalPadding));
            mThumbnailRects.add(new RectF(centerX + halfThumbnailHorizontalPadding, 0,
                    mThumbnailWidth, centerY - halfThumbnailVerticalPadding));
            mThumbnailRects.add(new RectF(0, centerY + halfThumbnailVerticalPadding,
                    centerX - halfThumbnailHorizontalPadding, mThumbnailHeight));
            mThumbnailRects.add(new RectF(centerX + halfThumbnailHorizontalPadding,
                    centerY + halfThumbnailVerticalPadding, mThumbnailWidth, mThumbnailHeight));

            // Initialize Rects for favicons and favicon frame.
            final float halfFaviconFrameSize =
                    mContext.getResources().getDimension(
                            R.dimen.tab_grid_thumbnail_favicon_frame_size)
                    / 2f;
            float thumbnailFaviconPaddingFromBackground = mContext.getResources().getDimension(
                    R.dimen.tab_grid_thumbnail_favicon_padding_from_frame);
            for (int i = 0; i < 4; i++) {
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

        private void initializeAndStartFetching(PseudoTab tab) {
            // Initialize mMultiThumbnailBitmap.
            mMultiThumbnailBitmap =
                    Bitmap.createBitmap(mThumbnailWidth, mThumbnailHeight, Bitmap.Config.ARGB_8888);
            mCanvas = new Canvas(mMultiThumbnailBitmap);
            mCanvas.drawColor(Color.TRANSPARENT);

            // Initialize Tabs.
            List<PseudoTab> relatedTabList =
                    PseudoTab.getRelatedTabs(mContext, tab, mTabModelSelector);
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
                    final GURL url = mTabs.get(i).getUrl();
                    final boolean isIncognito = mTabs.get(i).isIncognito();
                    final Size tabThumbnailSize = new Size((int) mThumbnailRects.get(i).width(),
                            (int) mThumbnailRects.get(i).height());
                    // getTabThumbnailWithCallback() might call the callback up to twice,
                    // so use |lastFavicon| to avoid fetching the favicon the second time.
                    // Fetching the favicon after getting the live thumbnail would lead to
                    // visible flicker.
                    final AtomicReference<Drawable> lastFavicon = new AtomicReference<>();
                    mTabContentManager.getTabThumbnailWithCallback(
                            mTabs.get(i).getId(), tabThumbnailSize, thumbnail -> {
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
                    if (mText != null && i == 3) {
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
        }

        private void drawThumbnailBitmapOnCanvasWithFrame(Bitmap thumbnail, int index) {
            if (thumbnail == null) {
                Paint emptyThumbnailPaint =
                        mIsTabSelected ? mSelectedEmptyThumbnailPaint : mEmptyThumbnailPaint;
                mCanvas.drawRoundRect(
                        mThumbnailRects.get(index), mRadius, mRadius, emptyThumbnailPaint);
                return;
            }

            // Draw the base paint first and set the base for thumbnail to draw. Setting the xfer
            // mode as SRC_OVER so the thumbnail can be drawn on top of this paint. See
            // https://crbug.com/1227619.
            mThumbnailBasePaint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.SRC_OVER));
            mCanvas.drawRoundRect(
                    mThumbnailRects.get(index), mRadius, mRadius, mThumbnailBasePaint);

            thumbnail =
                    Bitmap.createScaledBitmap(thumbnail, (int) mThumbnailRects.get(index).width(),
                            (int) mThumbnailRects.get(index).height(), true);
            mThumbnailBasePaint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.SRC_IN));
            mCanvas.drawBitmap(thumbnail,
                    new Rect(0, 0, thumbnail.getWidth(), thumbnail.getHeight()),
                    mThumbnailRects.get(index), mThumbnailBasePaint);
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
                PostTask.postTask(UiThreadTaskTraits.USER_VISIBLE,
                        mFinalCallback.bind(mMultiThumbnailBitmap));
            }
        }

        private void fetch() {
            initializeRects(mContext);
            initializeAndStartFetching(mInitialTab);
        }
    }

    MultiThumbnailCardProvider(Context context, TabContentManager tabContentManager,
            TabModelSelector tabModelSelector) {
        mContext = context;
        Resources resource = context.getResources();

        mTabContentManager = tabContentManager;
        mTabModelSelector = tabModelSelector;
        mRadius = resource.getDimension(R.dimen.tab_list_mini_card_radius);
        mFaviconFrameCornerRadius =
                resource.getDimension(R.dimen.tab_grid_thumbnail_favicon_frame_corner_radius);

        mTabListFaviconProvider = new TabListFaviconProvider(context, false);

        // Initialize Paints to use.
        mEmptyThumbnailPaint = new Paint();
        mEmptyThumbnailPaint.setStyle(Paint.Style.FILL);
        mEmptyThumbnailPaint.setAntiAlias(true);
        mEmptyThumbnailPaint.setColor(
                TabUiThemeProvider.getMiniThumbnailPlaceHolderColor(context, false, false));

        mSelectedEmptyThumbnailPaint = new Paint(mEmptyThumbnailPaint);
        mSelectedEmptyThumbnailPaint.setColor(
                TabUiThemeProvider.getMiniThumbnailPlaceHolderColor(context, false, true));

        // Paint used to set base for thumbnails, in case mEmptyThumbnailPaint has transparency.
        mThumbnailBasePaint = new Paint(mEmptyThumbnailPaint);
        mThumbnailBasePaint.setColor(Color.BLACK);
        mThumbnailBasePaint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.SRC_IN));

        mThumbnailFramePaint = new Paint();
        mThumbnailFramePaint.setStyle(Paint.Style.STROKE);
        mThumbnailFramePaint.setStrokeWidth(
                resource.getDimension(R.dimen.tab_list_mini_card_frame_size));
        mThumbnailFramePaint.setColor(SemanticColorUtils.getDividerLineBgColor(context));
        mThumbnailFramePaint.setAntiAlias(true);

        // TODO(996048): Use pre-defined styles to avoid style out of sync if any text/color styles
        // changes.
        mTextPaint = new Paint();
        mTextPaint.setTextSize(resource.getDimension(R.dimen.compositor_tab_title_text_size));
        mTextPaint.setFakeBoldText(true);
        mTextPaint.setAntiAlias(true);
        mTextPaint.setTextAlign(Paint.Align.CENTER);
        mTextPaint.setColor(TabUiThemeProvider.getTabGroupNumberTextColor(context, false, false));

        mSelectedTextPaint = new Paint(mTextPaint);
        mSelectedTextPaint.setColor(
                TabUiThemeProvider.getTabGroupNumberTextColor(context, false, true));

        mFaviconBackgroundPaintColor = context.getColor(R.color.favicon_background_color);
        mFaviconBackgroundPaint = new Paint();
        mFaviconBackgroundPaint.setAntiAlias(true);
        mFaviconBackgroundPaint.setColor(mFaviconBackgroundPaintColor);
        mFaviconBackgroundPaint.setStyle(Paint.Style.FILL);
        mFaviconBackgroundPaint.setShadowLayer(
                resource.getDimension(R.dimen.tab_grid_thumbnail_favicon_background_radius), 0,
                resource.getDimension(R.dimen.tab_grid_thumbnail_favicon_background_down_shift),
                resource.getColor(R.color.modern_grey_800_alpha_38));

        mTabModelSelectorObserver = new TabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                boolean isIncognito = newModel.isIncognito();
                mEmptyThumbnailPaint.setColor(TabUiThemeProvider.getMiniThumbnailPlaceHolderColor(
                        context, isIncognito, false));
                mTextPaint.setColor(
                        TabUiThemeProvider.getTabGroupNumberTextColor(context, isIncognito, false));
                mThumbnailFramePaint.setColor(
                        TabUiThemeProvider.getMiniThumbnailFrameColor(context, isIncognito));
                mFaviconBackgroundPaint.setColor(
                        TabUiThemeProvider.getFaviconBackgroundColor(context, isIncognito));

                mSelectedEmptyThumbnailPaint.setColor(
                        TabUiThemeProvider.getMiniThumbnailPlaceHolderColor(
                                context, isIncognito, true));
                mSelectedTextPaint.setColor(
                        TabUiThemeProvider.getTabGroupNumberTextColor(context, isIncognito, true));
            }
        };
        mTabModelSelector.addObserver(mTabModelSelectorObserver);
    }

    public void initWithNative() {
        mTabListFaviconProvider.initWithNative(
                mTabModelSelector.getModel(/*isIncognito=*/false).getProfile());
    }

    /**
     * Destroy any member that needs clean up.
     */
    public void destroy() {
        mTabModelSelector.removeObserver(mTabModelSelectorObserver);
    }

    @Override
    public void getTabThumbnailWithCallback(int tabId, Size thumbnailSize,
            Callback<Bitmap> finalCallback, boolean forceUpdate, boolean writeToCache,
            boolean isSelected) {
        Tab tab = mTabModelSelector.getTabById(tabId);
        PseudoTab pseudoTab = (tab != null) ? PseudoTab.fromTab(tab) : PseudoTab.fromTabId(tabId);
        if (pseudoTab == null
                || PseudoTab.getRelatedTabs(mContext, pseudoTab, mTabModelSelector).size() == 1) {
            mTabContentManager.getTabThumbnailWithCallback(
                    tabId, thumbnailSize, finalCallback, forceUpdate, writeToCache);
            return;
        }
        new MultiThumbnailFetcher(
                pseudoTab, thumbnailSize, finalCallback, forceUpdate, writeToCache, isSelected)
                .fetch();
    }
}
