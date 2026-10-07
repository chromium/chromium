// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.util.Size;

import androidx.annotation.ColorInt;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ui.InnerGlowDrawable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFaviconMetadata;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabWebContentsFaviconDelegate;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Objects;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * A {@link ThumbnailProvider} that will create a single Bitmap Thumbnail for all the tabs in a tab
 * group.
 */
@NullMarked
public class MultiThumbnailCardProvider implements ThumbnailProvider {
    private static final PorterDuffXfermode SRC_IN_XFERMODE =
            new PorterDuffXfermode(PorterDuff.Mode.SRC_IN);

    private final TabContentManager mTabContentManager;
    private final TabContentManagerThumbnailProvider mTabContentManagerThumbnailProvider;
    private final NullableObservableSupplier<TabModel> mCurrentTabModelSupplier;
    private final float mRadius;
    private final float mFaviconFrameCornerRadius;
    private final Paint mEmptyThumbnailPaint;
    private final Paint mThumbnailBasePaint;
    private final Paint mTextPaint;
    private final Paint mFaviconBackgroundPaint;
    private final Drawable mEmptyThumbnailGhostLoadIllustration;
    private final Drawable mActingOverlayDrawable;
    private final Drawable mSparkIconDrawable;

    private final Context mContext;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final TabListFaviconProvider mTabListFaviconProvider;

    private class MultiThumbnailFetcher {
        private static final int MAX_THUMBNAIL_COUNT = 4;
        private final MultiThumbnailMetadata mMultiThumbnailMetadata;
        private final Callback<@Nullable Drawable> mResultCallback;
        private final AtomicInteger mThumbnailsToFetch = new AtomicInteger();
        private final Path mPath = new Path();
        private final List<Rect> mFaviconRects = new ArrayList<>(MAX_THUMBNAIL_COUNT);
        private final List<RectF> mThumbnailRects = new ArrayList<>(MAX_THUMBNAIL_COUNT);
        private final List<RectF> mFaviconBackgroundRects = new ArrayList<>(MAX_THUMBNAIL_COUNT);
        private final int mThumbnailWidth;
        private final int mThumbnailHeight;
        private final @ColorInt int mResolvedEmptyPlaceholderColor;
        private final @ColorInt int mResolvedTextColor;
        private final @ColorInt int mResolvedGhostIllustrationColor;
        private final @ColorInt int mResolvedFaviconBackgroundColor;
        private final Bitmap mMultiThumbnailBitmap;
        private final Canvas mCanvas;

        /**
         * Fetcher that get the thumbnail drawable depending on if the tab is selected.
         *
         * @see TabContentManager#getTabThumbnailWithCallback
         * @param metadata Thumbnail is generated for tabs in the group described by {@link
         *     MultiThumbnailMetadata}.
         * @param thumbnailSize Desired size of multi-thumbnail.
         * @param isTabSelected Whether the thumbnail is for a currently selected tab.
         * @param resultCallback Callback which receives generated bitmap.
         */
        MultiThumbnailFetcher(
                MultiThumbnailMetadata metadata,
                Size thumbnailSize,
                boolean isTabSelected,
                Callback<@Nullable Drawable> resultCallback) {
            mResultCallback = Objects.requireNonNull(resultCallback);
            mMultiThumbnailMetadata = metadata;

            if (thumbnailSize.getHeight() <= 0 || thumbnailSize.getWidth() <= 0) {
                float expectedThumbnailAspectRatio =
                        TabCardThemeUtil.getTabThumbnailAspectRatio(
                                mContext, mBrowserControlsStateProvider);
                mThumbnailWidth =
                        (int)
                                mContext.getResources()
                                        .getDimension(R.dimen.tab_grid_thumbnail_card_default_size);
                mThumbnailHeight = (int) (mThumbnailWidth / expectedThumbnailAspectRatio);
            } else {
                mThumbnailWidth = thumbnailSize.getWidth();
                mThumbnailHeight = thumbnailSize.getHeight();
            }

            @TabGroupColorId Integer actualColorId = metadata.tabGroupColor;
            boolean isIncognito = metadata.isIncognito;
            mResolvedEmptyPlaceholderColor =
                    TabCardThemeUtil.getMiniThumbnailPlaceholderColor(
                            mContext, isIncognito, isTabSelected, actualColorId);
            mResolvedTextColor =
                    TabCardThemeUtil.getTitleTextColor(
                            mContext, isIncognito, isTabSelected, actualColorId);
            mResolvedGhostIllustrationColor =
                    TabCardThemeUtil.getCardViewBackgroundColor(
                            mContext, isIncognito, isTabSelected, actualColorId);
            mResolvedFaviconBackgroundColor =
                    TabCardThemeUtil.getFaviconBackgroundColor(mContext, isIncognito);
            mMultiThumbnailBitmap =
                    Bitmap.createBitmap(mThumbnailWidth, mThumbnailHeight, Bitmap.Config.ARGB_8888);
            mCanvas = new Canvas(mMultiThumbnailBitmap);
        }

        /** Initialize rects used for thumbnails. */
        private void initializeRects() {
            Resources res = mContext.getResources();
            float halfThumbnailPadding =
                    res.getDimension(R.dimen.tab_grid_card_thumbnail_margin) / 2;

            float centerX = mThumbnailWidth * 0.5f;
            float centerY = mThumbnailHeight * 0.5f;

            mThumbnailRects.add(
                    new RectF(
                            0, 0, centerX - halfThumbnailPadding, centerY - halfThumbnailPadding));
            mThumbnailRects.add(
                    new RectF(
                            centerX + halfThumbnailPadding,
                            0,
                            mThumbnailWidth,
                            centerY - halfThumbnailPadding));
            mThumbnailRects.add(
                    new RectF(
                            0,
                            centerY + halfThumbnailPadding,
                            centerX - halfThumbnailPadding,
                            mThumbnailHeight));
            mThumbnailRects.add(
                    new RectF(
                            centerX + halfThumbnailPadding,
                            centerY + halfThumbnailPadding,
                            mThumbnailWidth,
                            mThumbnailHeight));

            // Initialize Rects for favicons and favicon frame.
            final float faviconFrameSize =
                    res.getDimension(R.dimen.tab_grid_thumbnail_favicon_frame_size);
            float offsetFromCard =
                    res.getDimension(R.dimen.tab_grid_thumbnail_favicon_frame_padding_from_card);
            float thumbnailFaviconPaddingFromBackground =
                    res.getDimension(R.dimen.tab_grid_thumbnail_favicon_padding_from_frame);

            for (int i = 0; i < 4; i++) {
                RectF faviconBackgroundRect =
                        getFaviconBackgroundRect(
                                mThumbnailRects.get(i), faviconFrameSize, offsetFromCard);
                mFaviconBackgroundRects.add(faviconBackgroundRect);

                RectF faviconRectF = new RectF(faviconBackgroundRect);
                faviconRectF.inset(
                        thumbnailFaviconPaddingFromBackground,
                        thumbnailFaviconPaddingFromBackground);

                Rect faviconRect = new Rect();
                faviconRectF.roundOut(faviconRect);
                mFaviconRects.add(faviconRect);
            }
        }

        private RectF getFaviconBackgroundRect(
                RectF thumbnailRect, float faviconFrameSize, float offsetFromCard) {
            float thumbnailRectLeft = thumbnailRect.left;
            float thumbnailRectRight = thumbnailRect.right;
            float thumbnailRectTop = thumbnailRect.top;

            RectF faviconBackgroundRect =
                    new RectF(
                            thumbnailRectLeft,
                            thumbnailRectTop,
                            thumbnailRectLeft + faviconFrameSize,
                            thumbnailRectTop + faviconFrameSize);

            float horizontalOffsetToApply = offsetFromCard;
            if (LocalizationUtils.isLayoutRtl()) {
                // In RTL (Right-to-Left) layout, calculate the effective 'horizontal' offset
                // from the thumbnail's left edge to position the favicon 'offsetFromCard'
                // pixels from the thumbnail's right edge.
                // This is done by taking the thumbnail's width (thumbnailRectRight -
                // thumbnailRectLeft),
                // subtracting the favicon's width (faviconFrameSize), and then further
                // subtracting the desired 'offsetFromCard' from the right.

                // Visualization for RTL:
                // [TL -------------------- TR]  (Thumbnail)
                //       [FL --- FR]             (Favicon, where FR is Favicon Right)
                //                 <- offset ->  (Desired space from TR)
                // FL = TR - TL - FaviconWidth - offset
                horizontalOffsetToApply =
                        thumbnailRectRight - thumbnailRectLeft - faviconFrameSize - offsetFromCard;
            }

            faviconBackgroundRect.offset(horizontalOffsetToApply, offsetFromCard);
            return faviconBackgroundRect;
        }

        private void fetch() {
            initializeRects();

            // Live tab groups (tabGroupId != null) fetch thumbnails and favicons from their open
            // Tabs in TabModel, whereas SavedTabGroups (tabGroupId == null) only have URLs in
            // metadata.urlList and render favicons over empty placeholder slots.
            TabModel tabModel = mCurrentTabModelSupplier.get();
            assumeNonNull(tabModel);
            MultiThumbnailMetadata metadata = mMultiThumbnailMetadata;
            boolean hasLiveTabs = metadata.tabGroupId != null;
            List<Tab> tabsInGroup =
                    hasLiveTabs
                            ? tabModel.getTabsInGroup(metadata.tabGroupId)
                            : Collections.emptyList();
            int totalTabCount = hasLiveTabs ? tabsInGroup.size() : metadata.urlList.size();
            boolean showPlus = totalTabCount > MAX_THUMBNAIL_COUNT;
            int tabsToShow = showPlus ? MAX_THUMBNAIL_COUNT - 1 : totalTabCount;
            String text = showPlus ? "+" + (totalTabCount - tabsToShow) : null;
            mThumbnailsToFetch.set(tabsToShow);

            boolean anyHiddenTabActing =
                    hasLiveTabs && checkAnyHiddenTabActing(tabsInGroup, tabsToShow);

            // Fetch and draw all.
            for (int i = 0; i < MAX_THUMBNAIL_COUNT; i++) {
                RectF thumbnailRect = mThumbnailRects.get(i);
                if (i < tabsToShow) {
                    if (hasLiveTabs) {
                        final int index = i;
                        Tab tab = tabsInGroup.get(i);
                        GURL url = tab.getUrl();
                        Size tabThumbnailSize =
                                new Size((int) thumbnailRect.width(), (int) thumbnailRect.height());
                        mTabContentManager.getTabThumbnailWithCallback(
                                tab.getId(),
                                tabThumbnailSize,
                                thumbnail -> {
                                    if (tab.isClosing() || tab.isDestroyed()) return;

                                    drawFavicon(thumbnail, index, tab, url);
                                });
                    } else {
                        drawFavicon(
                                /* thumbnail= */ null, i, /* tab= */ null, metadata.urlList.get(i));
                    }
                } else {
                    drawThumbnailBitmapOnCanvasWithFrame(
                            /* thumbnail= */ null, i, /* showGhostLoadIllustration= */ false);
                    if (text != null && i == 3) {
                        if (anyHiddenTabActing) {
                            drawFaviconDrawableOnCanvasWithFrame(mSparkIconDrawable, i);
                        }
                        // Draw the text exactly centered on the thumbnail rect.
                        mTextPaint.setColor(mResolvedTextColor);
                        mCanvas.drawText(
                                text,
                                (thumbnailRect.left + thumbnailRect.right) / 2,
                                (thumbnailRect.top + thumbnailRect.bottom) / 2
                                        - ((mTextPaint.descent() + mTextPaint.ascent()) / 2),
                                mTextPaint);
                    }
                }
            }
        }

        private void drawThumbnailBitmapOnCanvasWithFrame(
                @Nullable Bitmap thumbnail, int index, boolean showGhostLoadIllustration) {
            final RectF rect = mThumbnailRects.get(index);
            if (thumbnail == null) {
                mEmptyThumbnailPaint.setColor(mResolvedEmptyPlaceholderColor);
                mCanvas.drawRoundRect(rect, mRadius, mRadius, mEmptyThumbnailPaint);

                if (showGhostLoadIllustration) {
                    Resources res = mContext.getResources();
                    mEmptyThumbnailGhostLoadIllustration.setTint(mResolvedGhostIllustrationColor);

                    int lrPadding =
                            res.getDimensionPixelSize(R.dimen.tab_grid_empty_thumbnail_lr_inset);
                    int topPadding =
                            res.getDimensionPixelSize(R.dimen.tab_grid_empty_thumbnail_top_inset);
                    int bottomPadding =
                            res.getDimensionPixelSize(
                                    R.dimen.tab_grid_empty_thumbnail_bottom_inset);

                    int left = Math.round(rect.left) + lrPadding;
                    int right = Math.round(rect.right) - lrPadding;
                    int top = Math.round(rect.top) + topPadding;
                    int bottom = Math.round(rect.bottom) - bottomPadding;

                    mEmptyThumbnailGhostLoadIllustration.setBounds(left, top, right, bottom);
                    mEmptyThumbnailGhostLoadIllustration.draw(mCanvas);
                }

                return;
            }

            mCanvas.save();
            mCanvas.clipRect(rect);
            Matrix m = new Matrix();

            final float newWidth = rect.width();
            final float scale =
                    Math.max(
                            newWidth / thumbnail.getWidth(), rect.height() / thumbnail.getHeight());
            m.setScale(scale, scale);
            final float xOffset =
                    rect.left + (int) ((newWidth - (thumbnail.getWidth() * scale)) / 2);
            final float yOffset = rect.top;
            m.postTranslate(xOffset, yOffset);

            // Draw the base paint first and set the base for thumbnail to draw. Clearing the xfer
            // mode defaults to SRC_OVER so the thumbnail can be drawn on top of this paint. See
            // https://crbug.com/40777171.
            mThumbnailBasePaint.setXfermode(null);
            mCanvas.drawRoundRect(rect, mRadius, mRadius, mThumbnailBasePaint);

            mThumbnailBasePaint.setXfermode(SRC_IN_XFERMODE);
            mCanvas.drawBitmap(thumbnail, m, mThumbnailBasePaint);
            mCanvas.restore();
            thumbnail.recycle();
        }

        private void drawFaviconDrawableOnCanvasWithFrame(Drawable favicon, int index) {
            mFaviconBackgroundPaint.setColor(mResolvedFaviconBackgroundColor);
            mCanvas.drawRoundRect(
                    mFaviconBackgroundRects.get(index),
                    mFaviconFrameCornerRadius,
                    mFaviconFrameCornerRadius,
                    mFaviconBackgroundPaint);
            Rect oldBounds = new Rect(favicon.getBounds());
            favicon.setBounds(mFaviconRects.get(index));
            favicon.draw(mCanvas);
            // Restore the bounds since this may be a shared drawable.
            favicon.setBounds(oldBounds);
        }

        private void drawFaviconThenMaybeSendBack(Drawable favicon, int index) {
            drawFaviconDrawableOnCanvasWithFrame(favicon, index);
            if (mThumbnailsToFetch.decrementAndGet() == 0) {
                BitmapDrawable drawable = new BitmapDrawable(mMultiThumbnailBitmap);
                PostTask.postTask(TaskTraits.UI_USER_VISIBLE, mResultCallback.bind(drawable));
            }
        }

        private boolean checkAnyHiddenTabActing(List<Tab> tabsInGroup, int tabsToShow) {
            List<Integer> actingTabIds = mMultiThumbnailMetadata.actingTabIds;
            if (actingTabIds.isEmpty()) return false;
            for (int i = tabsToShow; i < tabsInGroup.size(); i++) {
                if (actingTabIds.contains(tabsInGroup.get(i).getId())) {
                    return true;
                }
            }
            return false;
        }

        private void drawActingOverlay(int index) {
            RectF rect = mThumbnailRects.get(index);
            mCanvas.save();
            mPath.reset();
            mPath.addRoundRect(rect, mRadius, mRadius, Path.Direction.CW);
            mCanvas.clipPath(mPath);
            mActingOverlayDrawable.setBounds(
                    Math.round(rect.left),
                    Math.round(rect.top),
                    Math.round(rect.right),
                    Math.round(rect.bottom));
            mActingOverlayDrawable.draw(mCanvas);
            mCanvas.restore();
        }

        private void drawFavicon(
                @Nullable Bitmap thumbnail, int index, @Nullable Tab tab, GURL url) {
            drawThumbnailBitmapOnCanvasWithFrame(
                    thumbnail, index, /* showGhostLoadIllustration= */ true);

            if (tab != null && mMultiThumbnailMetadata.actingTabIds.contains(tab.getId())) {
                drawActingOverlay(index);
                drawFaviconThenMaybeSendBack(mSparkIconDrawable, index);
                return;
            }

            mTabListFaviconProvider.getFaviconDrawableForTabAsync(
                    new TabFaviconMetadata(
                            tab,
                            url,
                            mMultiThumbnailMetadata.isIncognito,
                            /* isInTabGroup= */ true),
                    (Drawable favicon) -> {
                        if (tab != null && (tab.isClosing() || tab.isDestroyed())) return;

                        drawFaviconThenMaybeSendBack(favicon, index);
                    });
        }
    }

    /**
     * Constructs a {@link MultiThumbnailCardProvider}.
     *
     * @param context Context of the application.
     * @param browserControlsStateProvider For getting browser controls height.
     * @param tabContentManager Tab content manager for fetching thumbnails.
     * @param currentTabModelSupplier Supplier of the current tab model.
     * @param tabWebContentsFaviconDelegate Delegate to retrieve web contents favicons.
     */
    public MultiThumbnailCardProvider(
            Context context,
            BrowserControlsStateProvider browserControlsStateProvider,
            TabContentManager tabContentManager,
            NullableObservableSupplier<TabModel> currentTabModelSupplier,
            @Nullable TabWebContentsFaviconDelegate tabWebContentsFaviconDelegate) {
        mContext = context;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        Resources resources = context.getResources();

        mTabContentManager = tabContentManager;
        mTabContentManagerThumbnailProvider =
                new TabContentManagerThumbnailProvider(tabContentManager);
        mCurrentTabModelSupplier = currentTabModelSupplier;
        mRadius = resources.getDimension(R.dimen.tab_list_mini_card_radius);
        mFaviconFrameCornerRadius =
                resources.getDimension(R.dimen.tab_grid_thumbnail_favicon_frame_corner_radius);

        mTabListFaviconProvider =
                new TabListFaviconProvider(
                        context,
                        TabListMode.GRID,
                        R.dimen.default_favicon_corner_radius,
                        tabWebContentsFaviconDelegate);

        // Initialize Paints to use.
        mEmptyThumbnailPaint = new Paint();
        mEmptyThumbnailPaint.setStyle(Paint.Style.FILL);
        mEmptyThumbnailPaint.setAntiAlias(true);

        mEmptyThumbnailGhostLoadIllustration =
                assumeNonNull(
                                AppCompatResources.getDrawable(
                                        mContext, R.drawable.empty_thumbnail_background))
                        .mutate();

        mActingOverlayDrawable = InnerGlowDrawable.createGtsPreviewGlow(mContext);
        mSparkIconDrawable =
                assumeNonNull(
                        AppCompatResources.getDrawable(mContext, R.drawable.ic_spark_blue_16dp));

        // Paint used to set base for thumbnails, in case mEmptyThumbnailPaint has transparency.
        mThumbnailBasePaint = new Paint(mEmptyThumbnailPaint);
        mThumbnailBasePaint.setColor(Color.BLACK);

        // TODO(crbug.com/41477335): Use pre-defined styles to avoid style out of sync if any
        // text/color styles changes.
        mTextPaint = new Paint();
        mTextPaint.setTextSize(resources.getDimension(R.dimen.compositor_tab_title_text_size));
        mTextPaint.setFakeBoldText(true);
        mTextPaint.setAntiAlias(true);
        mTextPaint.setTextAlign(Paint.Align.CENTER);

        mFaviconBackgroundPaint = new Paint();
        mFaviconBackgroundPaint.setAntiAlias(true);
        mFaviconBackgroundPaint.setStyle(Paint.Style.FILL);
        mFaviconBackgroundPaint.setShadowLayer(
                resources.getDimension(R.dimen.tab_grid_thumbnail_favicon_background_radius),
                0,
                resources.getDimension(R.dimen.tab_grid_thumbnail_favicon_background_down_shift),
                context.getColor(R.color.baseline_neutral_20_alpha_38));
    }

    /**
     * @param regularProfile The regular profile to use for favicons.
     */
    public void initWithNative(Profile regularProfile) {
        mTabListFaviconProvider.initWithNative(regularProfile);
    }

    /** Destroy any member that needs clean up. */
    public void destroy() {
        mTabListFaviconProvider.destroy();
    }

    @Override
    public void getTabThumbnailWithCallback(
            MultiThumbnailMetadata metadata,
            Size thumbnailSize,
            boolean isSelected,
            Callback<@Nullable Drawable> callback) {
        TabModel tabModel = mCurrentTabModelSupplier.get();
        assumeNonNull(tabModel);
        assert tabModel.isTabStateInitialized();

        if (metadata.tabId == Tab.INVALID_TAB_ID) {
            new MultiThumbnailFetcher(metadata, thumbnailSize, isSelected, callback).fetch();
            return;
        }

        assert tabModel.getTabById(metadata.tabId) != null;
        mTabContentManagerThumbnailProvider.getTabThumbnailWithCallback(
                metadata, thumbnailSize, isSelected, callback);
    }
}
