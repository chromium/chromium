// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.util.Size;

import androidx.annotation.ColorInt;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabFavicon;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tab_ui.TabCardThemeUtil;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabContentManagerThumbnailProvider;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.theme.SurfaceColorUpdateUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/**
 * A {@link ThumbnailProvider} that will create a single Bitmap Thumbnail for all the related tabs
 * for the given tabs.
 */
@NullMarked
public class MultiThumbnailCardProvider implements ThumbnailProvider {
    private final TabContentManager mTabContentManager;
    private final TabContentManagerThumbnailProvider mTabContentManagerThumbnailProvider;
    private final ObservableSupplier<@Nullable TabGroupModelFilter>
            mCurrentTabGroupModelFilterSupplier;
    private final Callback<@Nullable TabGroupModelFilter> mOnTabGroupModelFilterChanged =
            this::onTabGroupModelFilterChanged;

    private final float mRadius;
    private final float mFaviconFrameCornerRadius;
    private final Paint mColordEmptyThumbnailPaint;
    private final Paint mEmptyThumbnailPaint;
    private final Paint mThumbnailFramePaint;
    private final Paint mThumbnailBasePaint;
    private final Paint mTextPaint;
    private final Paint mFaviconBackgroundPaint;
    private final Paint mSelectedEmptyThumbnailPaint;
    private final Paint mSelectedTextPaint;
    private final int mFaviconBackgroundPaintColor;

    private @ColorInt int mMiniThumbnailPlaceholderColor;
    private @Nullable @ColorInt Integer mGroupTintedMiniThumbnailPlaceholderColor;

    private final Context mContext;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final TabListFaviconProvider mTabListFaviconProvider;

    private class MultiThumbnailFetcher {
        private static final int MAX_THUMBNAIL_COUNT = 4;
        private final Tab mInitialTab;
        private final Callback<@Nullable Drawable> mResultCallback;
        private final boolean mIsTabSelected;
        private final AtomicInteger mThumbnailsToFetch = new AtomicInteger();

        private Canvas mCanvas;
        private Bitmap mMultiThumbnailBitmap;
        private @Nullable String mText;

        private final List<Rect> mFaviconRects = new ArrayList<>(MAX_THUMBNAIL_COUNT);
        private final List<RectF> mThumbnailRects = new ArrayList<>(MAX_THUMBNAIL_COUNT);
        private final List<RectF> mFaviconBackgroundRects = new ArrayList<>(MAX_THUMBNAIL_COUNT);
        private final int mThumbnailWidth;
        private final int mThumbnailHeight;
        private final @ColorInt int mResolvedEmptyPlaceholderColor;
        private final @ColorInt int mResolvedTextColor;

        /**
         * Fetcher that get the thumbnail drawable depending on if the tab is selected.
         *
         * @see TabContentManager#getTabThumbnailWithCallback
         * @param initialTab Thumbnail is generated for tabs related to initialTab.
         * @param thumbnailSize Desired size of multi-thumbnail.
         * @param isTabSelected Whether the thumbnail is for a currently selected tab.
         * @param resultCallback Callback which receives generated bitmap.
         */
        MultiThumbnailFetcher(
                Tab initialTab,
                Size thumbnailSize,
                boolean isTabSelected,
                Callback<@Nullable Drawable> resultCallback) {
            mResultCallback = Objects.requireNonNull(resultCallback);
            mInitialTab = initialTab;
            mIsTabSelected = isTabSelected;

            if (thumbnailSize.getHeight() <= 0 || thumbnailSize.getWidth() <= 0) {
                float expectedThumbnailAspectRatio =
                        TabUtils.getTabThumbnailAspectRatio(
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

            TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
            @TabGroupColorId Integer actualColorId = null;
            boolean isIncognito = initialTab.isIncognitoBranded();
            if (filter != null && filter.isTabInTabGroup(initialTab)) {
                actualColorId = filter.getTabGroupColorWithFallback(initialTab.getRootId());
            }
            mResolvedEmptyPlaceholderColor =
                    TabCardThemeUtil.getMiniThumbnailPlaceholderColor(
                            mContext, isIncognito, mIsTabSelected, actualColorId);
            mResolvedTextColor =
                    TabCardThemeUtil.getTitleTextColor(
                            mContext, isIncognito, mIsTabSelected, actualColorId);
        }

        /** Initialize rects used for thumbnails. */
        private void initializeRects(Context context) {
            float thumbnailHorizontalPadding =
                    TabUiThemeProvider.getTabMiniThumbnailPaddingDimension(context);
            float thumbnailVerticalPadding = thumbnailHorizontalPadding;

            float centerX = mThumbnailWidth * 0.5f;
            float centerY = mThumbnailHeight * 0.5f;
            float halfThumbnailHorizontalPadding = thumbnailHorizontalPadding / 2;
            float halfThumbnailVerticalPadding = thumbnailVerticalPadding / 2;

            mThumbnailRects.add(
                    new RectF(
                            0,
                            0,
                            centerX - halfThumbnailHorizontalPadding,
                            centerY - halfThumbnailVerticalPadding));
            mThumbnailRects.add(
                    new RectF(
                            centerX + halfThumbnailHorizontalPadding,
                            0,
                            mThumbnailWidth,
                            centerY - halfThumbnailVerticalPadding));
            mThumbnailRects.add(
                    new RectF(
                            0,
                            centerY + halfThumbnailVerticalPadding,
                            centerX - halfThumbnailHorizontalPadding,
                            mThumbnailHeight));
            mThumbnailRects.add(
                    new RectF(
                            centerX + halfThumbnailHorizontalPadding,
                            centerY + halfThumbnailVerticalPadding,
                            mThumbnailWidth,
                            mThumbnailHeight));

            // Initialize Rects for favicons and favicon frame.
            final float halfFaviconFrameSize =
                    mContext.getResources()
                                    .getDimension(R.dimen.tab_grid_thumbnail_favicon_frame_size)
                            / 2f;
            float thumbnailFaviconPaddingFromBackground =
                    mContext.getResources()
                            .getDimension(R.dimen.tab_grid_thumbnail_favicon_padding_from_frame);
            for (int i = 0; i < 4; i++) {
                RectF thumbnailRect = mThumbnailRects.get(i);

                float thumbnailRectCenterX = thumbnailRect.centerX();
                float thumbnailRectCenterY = thumbnailRect.centerY();
                RectF faviconBackgroundRect =
                        new RectF(
                                thumbnailRectCenterX,
                                thumbnailRectCenterY,
                                thumbnailRectCenterX,
                                thumbnailRectCenterY);
                faviconBackgroundRect.inset(-halfFaviconFrameSize, -halfFaviconFrameSize);
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

        @Initializer
        private void initializeAndStartFetching(Tab initialTab) {
            // Initialize mMultiThumbnailBitmap.
            mMultiThumbnailBitmap =
                    Bitmap.createBitmap(mThumbnailWidth, mThumbnailHeight, Bitmap.Config.ARGB_8888);
            mCanvas = new Canvas(mMultiThumbnailBitmap);
            mCanvas.drawColor(Color.TRANSPARENT);

            // Initialize Tabs.
            TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
            assumeNonNull(filter);
            List<Tab> relatedTabList = filter.getRelatedTabList(initialTab.getId());
            int relatedTabCount = relatedTabList.size();
            boolean showPlus = relatedTabCount > MAX_THUMBNAIL_COUNT;
            int tabsToShow = showPlus ? MAX_THUMBNAIL_COUNT - 1 : relatedTabCount;
            Tab[] tabs = new Tab[MAX_THUMBNAIL_COUNT];
            mText = showPlus ? "+" + (relatedTabList.size() - tabsToShow) : null;
            mThumbnailsToFetch.set(tabsToShow);
            for (int i = 0; i < tabsToShow; i++) {
                tabs[i] = relatedTabList.get(i);
            }

            // Fetch and draw all.
            for (int i = 0; i < MAX_THUMBNAIL_COUNT; i++) {
                Tab tab = tabs[i];
                RectF thumbnailRect = mThumbnailRects.get(i);
                if (tab != null) {
                    // Create final copies to get lambda captures to compile.
                    final int index = i;
                    final Size tabThumbnailSize =
                            new Size((int) thumbnailRect.width(), (int) thumbnailRect.height());
                    // getTabThumbnailWithCallback() might call the callback up to twice,
                    // so use |lastFavicon| to avoid fetching the favicon the second time.
                    // Fetching the favicon after getting the live thumbnail would lead to
                    // visible flicker.
                    final AtomicReference<Drawable> lastFavicon = new AtomicReference<>();
                    mTabContentManager.getTabThumbnailWithCallback(
                            tab.getId(),
                            tabThumbnailSize,
                            thumbnail -> {
                                if (tab.isClosing() || tab.isDestroyed()) return;

                                drawThumbnailBitmapOnCanvasWithFrame(thumbnail, index);
                                if (lastFavicon.get() != null) {
                                    drawFaviconThenMaybeSendBack(lastFavicon.get(), index);
                                } else {
                                    mTabListFaviconProvider.getFaviconDrawableForTabAsync(
                                            tab,
                                            (Drawable favicon) -> {
                                                if (tab.isClosing() || tab.isDestroyed()) return;

                                                lastFavicon.set(favicon);
                                                drawFaviconThenMaybeSendBack(favicon, index);
                                            });
                                }
                            });
                } else {
                    drawThumbnailBitmapOnCanvasWithFrame(null, i);
                    if (mText != null && i == 3) {
                        // Draw the text exactly centered on the thumbnail rect.
                        Paint textPaint = mIsTabSelected ? mSelectedTextPaint : mTextPaint;
                        mCanvas.drawText(
                                mText,
                                (thumbnailRect.left + thumbnailRect.right) / 2,
                                (thumbnailRect.top + thumbnailRect.bottom) / 2
                                        - ((mTextPaint.descent() + mTextPaint.ascent()) / 2),
                                textPaint);
                    }
                }
            }
        }

        private void drawThumbnailBitmapOnCanvasWithFrame(@Nullable Bitmap thumbnail, int index) {
            final RectF rect = mThumbnailRects.get(index);
            if (thumbnail == null) {
                if (SurfaceColorUpdateUtils.useNewGm3GtsTabGroupColors()) {
                    mTextPaint.setColor(mResolvedTextColor);
                    mColordEmptyThumbnailPaint.setColor(mResolvedEmptyPlaceholderColor);
                    Paint emptyThumbnailPaint =
                            mIsTabSelected
                                    ? mSelectedEmptyThumbnailPaint
                                    : mColordEmptyThumbnailPaint;
                    mCanvas.drawRoundRect(rect, mRadius, mRadius, emptyThumbnailPaint);
                    return;
                }
                Paint emptyThumbnailPaint =
                        mIsTabSelected ? mSelectedEmptyThumbnailPaint : mEmptyThumbnailPaint;
                mCanvas.drawRoundRect(rect, mRadius, mRadius, emptyThumbnailPaint);
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

            // Draw the base paint first and set the base for thumbnail to draw. Setting the xfer
            // mode as SRC_OVER so the thumbnail can be drawn on top of this paint. See
            // https://crbug.com/1227619.
            mThumbnailBasePaint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.SRC_OVER));
            mCanvas.drawRoundRect(rect, mRadius, mRadius, mThumbnailBasePaint);

            mThumbnailBasePaint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.SRC_IN));
            mCanvas.drawBitmap(thumbnail, m, mThumbnailBasePaint);
            mCanvas.restore();
            thumbnail.recycle();
        }

        private void drawFaviconDrawableOnCanvasWithFrame(Drawable favicon, int index) {
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

        private void fetch() {
            initializeRects(mContext);
            initializeAndStartFetching(mInitialTab);
        }
    }

    public MultiThumbnailCardProvider(
            Context context,
            BrowserControlsStateProvider browserControlsStateProvider,
            TabContentManager tabContentManager,
            ObservableSupplier<@Nullable TabGroupModelFilter> currentTabGroupModelFilterSupplier) {
        mContext = context;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        Resources resources = context.getResources();

        mTabContentManager = tabContentManager;
        mTabContentManagerThumbnailProvider =
                new TabContentManagerThumbnailProvider(tabContentManager);
        mCurrentTabGroupModelFilterSupplier = currentTabGroupModelFilterSupplier;
        mRadius = resources.getDimension(R.dimen.tab_list_mini_card_radius);
        mFaviconFrameCornerRadius =
                resources.getDimension(R.dimen.tab_grid_thumbnail_favicon_frame_corner_radius);

        mTabListFaviconProvider =
                new TabListFaviconProvider(
                        context,
                        false,
                        R.dimen.default_favicon_corner_radius,
                        TabFavicon::getBitmap);

        // Initialize Paints to use.
        mEmptyThumbnailPaint = new Paint();
        mEmptyThumbnailPaint.setStyle(Paint.Style.FILL);
        mEmptyThumbnailPaint.setAntiAlias(true);
        mEmptyThumbnailPaint.setColor(
                TabCardThemeUtil.getMiniThumbnailPlaceholderColor(
                        context, false, false, /* colorId */ null));

        mSelectedEmptyThumbnailPaint = new Paint(mEmptyThumbnailPaint);
        mSelectedEmptyThumbnailPaint.setColor(
                TabCardThemeUtil.getMiniThumbnailPlaceholderColor(
                        context, false, true, /* colorId */ null));

        mColordEmptyThumbnailPaint = new Paint(mEmptyThumbnailPaint);

        // Paint used to set base for thumbnails, in case mEmptyThumbnailPaint has transparency.
        mThumbnailBasePaint = new Paint(mEmptyThumbnailPaint);
        mThumbnailBasePaint.setColor(Color.BLACK);
        mThumbnailBasePaint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.SRC_IN));

        mThumbnailFramePaint = new Paint();
        mThumbnailFramePaint.setStyle(Paint.Style.STROKE);
        mThumbnailFramePaint.setStrokeWidth(
                resources.getDimension(R.dimen.tab_list_mini_card_frame_size));
        mThumbnailFramePaint.setColor(SemanticColorUtils.getDividerLineBgColor(context));
        mThumbnailFramePaint.setAntiAlias(true);

        // TODO(crbug.com/41477335): Use pre-defined styles to avoid style out of sync if any
        // text/color styles
        // changes.
        mTextPaint = new Paint();
        mTextPaint.setTextSize(resources.getDimension(R.dimen.compositor_tab_title_text_size));
        mTextPaint.setFakeBoldText(true);
        mTextPaint.setAntiAlias(true);
        mTextPaint.setTextAlign(Paint.Align.CENTER);
        mTextPaint.setColor(
                TabCardThemeUtil.getTabGroupNumberTextColor(
                        context, false, false, /* colorId */ null));

        mSelectedTextPaint = new Paint(mTextPaint);
        mSelectedTextPaint.setColor(
                TabCardThemeUtil.getTabGroupNumberTextColor(
                        context, false, true, /* colorId */ null));

        mFaviconBackgroundPaintColor = context.getColor(R.color.favicon_background_color);
        mFaviconBackgroundPaint = new Paint();
        mFaviconBackgroundPaint.setAntiAlias(true);
        mFaviconBackgroundPaint.setColor(mFaviconBackgroundPaintColor);
        mFaviconBackgroundPaint.setStyle(Paint.Style.FILL);
        mFaviconBackgroundPaint.setShadowLayer(
                resources.getDimension(R.dimen.tab_grid_thumbnail_favicon_background_radius),
                0,
                resources.getDimension(R.dimen.tab_grid_thumbnail_favicon_background_down_shift),
                context.getColor(R.color.baseline_neutral_20_alpha_38));

        // Run this immediately if non-null as in the TabListEditor context we might try to load
        // tabs thumbnails before the post task normally run by ObservableSupplier#addObserver is
        // run.
        TabGroupModelFilter currentFilter =
                mCurrentTabGroupModelFilterSupplier.addObserver(mOnTabGroupModelFilterChanged);
        if (currentFilter != null) {
            mOnTabGroupModelFilterChanged.onResult(currentFilter);
        }
    }

    private void onTabGroupModelFilterChanged(@Nullable TabGroupModelFilter filter) {
        assert filter != null;
        boolean isIncognito = filter.getTabModel().isIncognitoBranded();
        mMiniThumbnailPlaceholderColor =
                TabCardThemeUtil.getMiniThumbnailPlaceholderColor(
                        mContext, isIncognito, false, /* colorId */ null);
        if (mGroupTintedMiniThumbnailPlaceholderColor == null) {
            mEmptyThumbnailPaint.setColor(mMiniThumbnailPlaceholderColor);
        }
        mTextPaint.setColor(
                TabCardThemeUtil.getTabGroupNumberTextColor(
                        mContext, isIncognito, false, /* colorId */ null));
        mThumbnailFramePaint.setColor(
                TabUiThemeProvider.getMiniThumbnailFrameColor(mContext, isIncognito));
        mFaviconBackgroundPaint.setColor(
                TabUiThemeProvider.getFaviconBackgroundColor(mContext, isIncognito));

        mSelectedEmptyThumbnailPaint.setColor(
                TabCardThemeUtil.getMiniThumbnailPlaceholderColor(
                        mContext, isIncognito, true, /* colorId */ null));
        mSelectedTextPaint.setColor(
                TabCardThemeUtil.getTabGroupNumberTextColor(
                        mContext, isIncognito, true, /* colorId */ null));
    }

    /**
     * Sets the new mini thumbnail placeholder color. If {@code null} is provided, the placeholder
     * color will be reset to the default.
     *
     * @param color The new mini thumbnail placeholder color, or {@code null} if resetting.
     */
    public void setMiniThumbnailPlaceholderColor(@Nullable @ColorInt Integer color) {
        mGroupTintedMiniThumbnailPlaceholderColor = color;
        if (mGroupTintedMiniThumbnailPlaceholderColor == null) {
            mEmptyThumbnailPaint.setColor(mMiniThumbnailPlaceholderColor);
        } else {
            mEmptyThumbnailPaint.setColor(mGroupTintedMiniThumbnailPlaceholderColor);
        }
    }

    /**
     * @param regularProfile The regular profile to use for favicons.
     */
    public void initWithNative(Profile regularProfile) {
        mTabListFaviconProvider.initWithNative(regularProfile);
    }

    /** Destroy any member that needs clean up. */
    public void destroy() {
        mCurrentTabGroupModelFilterSupplier.removeObserver(mOnTabGroupModelFilterChanged);
        mTabListFaviconProvider.destroy();
    }

    @Override
    public void getTabThumbnailWithCallback(
            int tabId,
            Size thumbnailSize,
            boolean isSelected,
            Callback<@Nullable Drawable> callback) {
        TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
        assumeNonNull(filter);
        assert filter.isTabModelRestored();

        Tab tab = filter.getTabModel().getTabById(tabId);
        assert tab != null;

        boolean useMultiThumbnail = filter.isTabInTabGroup(tab);
        if (useMultiThumbnail) {
            new MultiThumbnailFetcher(tab, thumbnailSize, isSelected, callback).fetch();
            return;
        }
        mTabContentManagerThumbnailProvider.getTabThumbnailWithCallback(
                tabId, thumbnailSize, isSelected, callback);
    }
}
