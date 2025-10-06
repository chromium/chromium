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
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabFavicon;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tab_ui.TabCardThemeUtil;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabContentManagerThumbnailProvider;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFaviconMetadata;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.theme.ThemeModuleUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.url.GURL;

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
    /**
     * The metadata details for a thumbnail item as part of a multi thumbnail card representation.
     * This object represents both real {@link Tab}s and {@link SavedTabGroupTab}s. If the tab field
     * is null, a SavedTabGroupTab is being referenced.
     */
    private static class ThumbnailItemMetadata {
        public final @Nullable Tab tab;
        public final GURL url;

        ThumbnailItemMetadata(@Nullable Tab tab, GURL url) {
            this.tab = tab;
            this.url = url;
        }
    }

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
    private final Drawable mEmptyThumbnailGhostLoadIllustration;
    private final Drawable mSelectedEmptyThumbnailGhostLoadIllustration;

    private @ColorInt int mMiniThumbnailPlaceholderColor;
    private @Nullable @ColorInt Integer mGroupTintedMiniThumbnailPlaceholderColor;

    private final Context mContext;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final TabListFaviconProvider mTabListFaviconProvider;

    private class MultiThumbnailFetcher {
        private static final int MAX_THUMBNAIL_COUNT = 4;
        private final MultiThumbnailMetadata mMultiThumbnailMetadata;
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
        private final @ColorInt int mResolvedGhostIllustrationColor;

        /**
         * Fetcher that get the thumbnail drawable depending on if the tab is selected.
         *
         * @see TabContentManager#getTabThumbnailWithCallback
         * @param metadata Thumbnail is generated for tabs related to {@link
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

            @TabGroupColorId Integer actualColorId = null;
            boolean isIncognito = metadata.isIncognito;
            if (metadata.isInTabGroup) {
                actualColorId = metadata.tabGroupColor;
            }
            mResolvedEmptyPlaceholderColor =
                    TabCardThemeUtil.getMiniThumbnailPlaceholderColor(
                            mContext, isIncognito, mIsTabSelected, actualColorId);
            mResolvedTextColor =
                    TabCardThemeUtil.getTitleTextColor(
                            mContext, isIncognito, mIsTabSelected, actualColorId);
            mResolvedGhostIllustrationColor =
                    TabUiThemeProvider.getEmptyThumbnailColor(
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
            final float faviconFrameSize =
                    mContext.getResources()
                            .getDimension(R.dimen.tab_grid_thumbnail_favicon_frame_size);
            float offsetFromCard =
                    mContext.getResources()
                            .getDimension(
                                    R.dimen.tab_grid_thumbnail_favicon_frame_padding_from_card);
            float thumbnailFaviconPaddingFromBackground =
                    mContext.getResources()
                            .getDimension(R.dimen.tab_grid_thumbnail_favicon_padding_from_frame);

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

        @Initializer
        private void initializeAndStartFetching(MultiThumbnailMetadata metadata) {
            // Initialize mMultiThumbnailBitmap.
            mMultiThumbnailBitmap =
                    Bitmap.createBitmap(mThumbnailWidth, mThumbnailHeight, Bitmap.Config.ARGB_8888);
            mCanvas = new Canvas(mMultiThumbnailBitmap);
            mCanvas.drawColor(Color.TRANSPARENT);

            // Initialize Tabs.
            List<ThumbnailItemMetadata> thumbnailItemList = getThumbnailItems(metadata);
            int relatedTabCount = thumbnailItemList.size();
            boolean showPlus = relatedTabCount > MAX_THUMBNAIL_COUNT;
            int tabsToShow = showPlus ? MAX_THUMBNAIL_COUNT - 1 : relatedTabCount;
            ThumbnailItemMetadata[] thumbnailItems = new ThumbnailItemMetadata[MAX_THUMBNAIL_COUNT];
            mText = showPlus ? "+" + (thumbnailItemList.size() - tabsToShow) : null;
            mThumbnailsToFetch.set(tabsToShow);
            for (int i = 0; i < tabsToShow; i++) {
                thumbnailItems[i] = thumbnailItemList.get(i);
            }

            // Fetch and draw all.
            for (int i = 0; i < MAX_THUMBNAIL_COUNT; i++) {
                ThumbnailItemMetadata thumbnailItem = thumbnailItems[i];
                RectF thumbnailRect = mThumbnailRects.get(i);
                if (thumbnailItem != null) {
                    // Create final copies to get lambda captures to compile.
                    final int index = i;
                    final Size tabThumbnailSize =
                            new Size((int) thumbnailRect.width(), (int) thumbnailRect.height());
                    // getTabThumbnailWithCallback() might call the callback up to twice,
                    // so use |lastFavicon| to avoid fetching the favicon the second time.
                    // Fetching the favicon after getting the live thumbnail would lead to
                    // visible flicker.
                    final AtomicReference<Drawable> lastFavicon = new AtomicReference<>();
                    if (thumbnailItem.tab != null) {
                        Tab tab = thumbnailItem.tab;
                        mTabContentManager.getTabThumbnailWithCallback(
                                tab.getId(),
                                tabThumbnailSize,
                                thumbnail -> {
                                    if (tab.isClosing() || tab.isDestroyed()) return;

                                    drawFavicon(thumbnail, index, lastFavicon, thumbnailItem);
                                });
                    } else {
                        drawFavicon(/* thumbnail= */ null, index, lastFavicon, thumbnailItem);
                    }
                } else {
                    drawThumbnailBitmapOnCanvasWithFrame(
                            null, i, /* showGhostLoadIllustration= */ false);
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

        private void drawThumbnailBitmapOnCanvasWithFrame(
                @Nullable Bitmap thumbnail, int index, boolean showGhostLoadIllustration) {
            final RectF rect = mThumbnailRects.get(index);
            if (thumbnail == null) {
                if (useNewGm3GtsTabGroupColors()) {
                    mTextPaint.setColor(mResolvedTextColor);
                    mColordEmptyThumbnailPaint.setColor(mResolvedEmptyPlaceholderColor);
                    Paint emptyThumbnailPaint =
                            mIsTabSelected
                                    ? mSelectedEmptyThumbnailPaint
                                    : mColordEmptyThumbnailPaint;
                    mCanvas.drawRoundRect(rect, mRadius, mRadius, emptyThumbnailPaint);
                } else {
                    Paint emptyThumbnailPaint =
                            mIsTabSelected ? mSelectedEmptyThumbnailPaint : mEmptyThumbnailPaint;
                    mCanvas.drawRoundRect(rect, mRadius, mRadius, emptyThumbnailPaint);
                }

                if (showGhostLoadIllustration) {
                    Resources res = mContext.getResources();
                    if (useNewGm3GtsTabGroupColors()) {
                        mEmptyThumbnailGhostLoadIllustration.setTint(
                                mResolvedGhostIllustrationColor);
                    }
                    Drawable ghostLoadIllustration =
                            mIsTabSelected
                                    ? mSelectedEmptyThumbnailGhostLoadIllustration
                                    : mEmptyThumbnailGhostLoadIllustration;

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
                    Rect rectDrawable = new Rect(left, top, right, bottom);

                    ghostLoadIllustration.setBounds(rectDrawable);
                    ghostLoadIllustration.draw(mCanvas);
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
            initializeAndStartFetching(mMultiThumbnailMetadata);
        }

        private List<ThumbnailItemMetadata> getThumbnailItems(MultiThumbnailMetadata metadata) {
            List<ThumbnailItemMetadata> thumbnailItems = new ArrayList<>();
            TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
            assumeNonNull(filter);
            if (metadata.tabId != Tab.INVALID_TAB_ID) {
                // Retrieve all related tabs in the tab model for non-SavedTabGroup groups.
                List<Tab> relatedTabList = filter.getRelatedTabList(metadata.tabId);
                for (Tab tab : relatedTabList) {
                    thumbnailItems.add(new ThumbnailItemMetadata(tab, tab.getUrl()));
                }
            } else {
                // Populate just the URLs for SavedTabGroupTabs.
                for (GURL url : metadata.urlList) {
                    thumbnailItems.add(new ThumbnailItemMetadata(/* tab= */ null, url));
                }
            }

            return thumbnailItems;
        }

        private void drawFavicon(
                @Nullable Bitmap thumbnail,
                int index,
                AtomicReference<Drawable> lastFavicon,
                ThumbnailItemMetadata thumbnailItem) {
            Tab tab = thumbnailItem.tab;
            drawThumbnailBitmapOnCanvasWithFrame(
                    thumbnail, index, /* showGhostLoadIllustration= */ true);
            if (lastFavicon.get() != null) {
                drawFaviconThenMaybeSendBack(lastFavicon.get(), index);
            } else {
                mTabListFaviconProvider.getFaviconDrawableForTabAsync(
                        new TabFaviconMetadata(
                                tab,
                                thumbnailItem.url,
                                mMultiThumbnailMetadata.isIncognito,
                                mMultiThumbnailMetadata.isInTabGroup),
                        (Drawable favicon) -> {
                            if (tab != null) {
                                if (tab.isClosing() || tab.isDestroyed()) return;
                            }

                            lastFavicon.set(favicon);
                            drawFaviconThenMaybeSendBack(favicon, index);
                        });
            }
        }

        /** Whether new GM3 colors are being used for the tab group colors. */
        public static boolean useNewGm3GtsTabGroupColors() {
            return ChromeFeatureList.sAndroidTabGroupsColorUpdateGm3.isEnabled()
                    || ThemeModuleUtils.isForceEnableDependencies();
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
                        context, false, false, /* colorId= */ null));

        mSelectedEmptyThumbnailPaint = new Paint(mEmptyThumbnailPaint);
        mSelectedEmptyThumbnailPaint.setColor(
                TabCardThemeUtil.getMiniThumbnailPlaceholderColor(
                        context, false, true, /* colorId= */ null));

        mColordEmptyThumbnailPaint = new Paint(mEmptyThumbnailPaint);

        final Drawable ghostThumbnail =
                AppCompatResources.getDrawable(mContext, R.drawable.empty_thumbnail_background);
        mEmptyThumbnailGhostLoadIllustration =
                assumeNonNull(ghostThumbnail.getConstantState()).newDrawable();

        mEmptyThumbnailGhostLoadIllustration.setTint(
                TabUiThemeProvider.getEmptyThumbnailColor(
                        mContext, false, false, /* colorId= */ null));

        mSelectedEmptyThumbnailGhostLoadIllustration =
                ghostThumbnail.getConstantState().newDrawable().mutate();
        mSelectedEmptyThumbnailGhostLoadIllustration.setTint(
                TabUiThemeProvider.getEmptyThumbnailColor(
                        mContext, false, true, /* colorId= */ null));

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
        // text/color styles changes.
        mTextPaint = new Paint();
        mTextPaint.setTextSize(resources.getDimension(R.dimen.compositor_tab_title_text_size));
        mTextPaint.setFakeBoldText(true);
        mTextPaint.setAntiAlias(true);
        mTextPaint.setTextAlign(Paint.Align.CENTER);
        mTextPaint.setColor(
                TabCardThemeUtil.getTabGroupNumberTextColor(
                        context, false, false, /* colorId= */ null));

        mSelectedTextPaint = new Paint(mTextPaint);
        mSelectedTextPaint.setColor(
                TabCardThemeUtil.getTabGroupNumberTextColor(
                        context, false, true, /* colorId= */ null));

        mFaviconBackgroundPaint = new Paint();
        mFaviconBackgroundPaint.setAntiAlias(true);
        mFaviconBackgroundPaint.setColor(
                TabUiThemeProvider.getFaviconBackgroundColor(context, /* isIncognito= */ false));
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
                        mContext, isIncognito, false, /* colorId= */ null);
        if (mGroupTintedMiniThumbnailPlaceholderColor == null) {
            mEmptyThumbnailPaint.setColor(mMiniThumbnailPlaceholderColor);
        }
        mTextPaint.setColor(
                TabCardThemeUtil.getTabGroupNumberTextColor(
                        mContext, isIncognito, false, /* colorId= */ null));
        mThumbnailFramePaint.setColor(
                TabUiThemeProvider.getMiniThumbnailFrameColor(mContext, isIncognito));
        mFaviconBackgroundPaint.setColor(
                TabUiThemeProvider.getFaviconBackgroundColor(mContext, isIncognito));

        mSelectedEmptyThumbnailPaint.setColor(
                TabCardThemeUtil.getMiniThumbnailPlaceholderColor(
                        mContext, isIncognito, true, /* colorId= */ null));
        mSelectedTextPaint.setColor(
                TabCardThemeUtil.getTabGroupNumberTextColor(
                        mContext, isIncognito, true, /* colorId= */ null));

        mEmptyThumbnailGhostLoadIllustration.setTint(
                TabUiThemeProvider.getEmptyThumbnailColor(
                        mContext, isIncognito, false, /* colorId= */ null));
        mSelectedEmptyThumbnailGhostLoadIllustration.setTint(
                TabUiThemeProvider.getEmptyThumbnailColor(
                        mContext, isIncognito, true, /* colorId= */ null));
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
            MultiThumbnailMetadata metadata,
            Size thumbnailSize,
            boolean isSelected,
            Callback<@Nullable Drawable> callback) {
        TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
        assumeNonNull(filter);
        assert filter.isTabModelRestored();

        if (metadata.tabId != Tab.INVALID_TAB_ID) {
            Tab tab = filter.getTabModel().getTabById(metadata.tabId);
            assert tab != null;
        }

        boolean useMultiThumbnail = metadata.isInTabGroup;
        if (useMultiThumbnail) {
            new MultiThumbnailFetcher(metadata, thumbnailSize, isSelected, callback).fetch();
            return;
        }
        mTabContentManagerThumbnailProvider.getTabThumbnailWithCallback(
                metadata, thumbnailSize, isSelected, callback);
    }
}
