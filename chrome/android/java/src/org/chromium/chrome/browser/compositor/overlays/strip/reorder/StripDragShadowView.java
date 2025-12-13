// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.animation.Animator;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.util.AttributeSet;
import android.util.FloatProperty;
import android.util.Size;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;
import androidx.constraintlayout.widget.ConstraintLayout;

import org.chromium.base.DeviceInfo;
import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabFavicon;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabContentManagerThumbnailProvider;
import org.chromium.chrome.browser.tab_ui.TabThumbnailView;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider.MultiThumbnailMetadata;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.MultiThumbnailCardProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tab_groups.TabGroupColorPickerUtils;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.url.GURL;

import java.util.List;

@NullMarked
public class StripDragShadowView extends FrameLayout {
    private static final FloatProperty<StripDragShadowView> PROGRESS =
            new FloatProperty<>("progress") {
                @Override
                public void setValue(StripDragShadowView object, float v) {
                    object.setProgress(v);
                }

                @Override
                public Float get(StripDragShadowView object) {
                    return object.getProgress();
                }
            };

    // Constants
    @VisibleForTesting
    protected static final int WIDTH_DP = (int) StripLayoutUtils.MAX_TAB_WIDTH_DP;

    @VisibleForTesting
    protected static final int HEIGHT_DP = (int) StripLayoutUtils.MAX_TAB_WIDTH_DP;

    private static final int WIDTH_ON_XR_DP = 528;
    private static final long ANIM_EXPAND_MS = 200L;

    // Children Views
    private View mCardView;
    private TextView mTitleView;
    private ImageView mFaviconView;
    private TabThumbnailView mThumbnailView;

    // Internal State
    private int mSourceWidthPx;
    private final int mSourceHeightPx;
    private int mWidthPx;
    private int mHeightPx;
    private float mProgress;
    private @Nullable Animator mRunningAnimator;

    // External Dependencies
    private BrowserControlsStateProvider mBrowserControlStateProvider;
    private ObservableSupplier<LayerTitleCache> mLayerTitleCacheSupplier;
    private TabModelSelector mTabModelSelector;
    private ShadowUpdateHost mShadowUpdateHost;

    // Thumbnail Providers
    private MultiThumbnailCardProvider mMultiThumbnailCardProvider;
    private TabContentManagerThumbnailProvider mSingleThumbnailCardProvider;

    // Current Drag State
    private @Nullable Tab mTab;
    private @Nullable TabObserver mFaviconUpdateTabObserver;

    public interface ShadowUpdateHost {
        /**
         * Notify the host of this drag shadow that the source view has been changed and its drag
         * shadow needs to be updated accordingly.
         */
        void requestUpdate();
    }

    public StripDragShadowView(Context context, AttributeSet attrs) {
        super(context, attrs);

        Resources resources = context.getResources();
        mSourceHeightPx =
                resources.getDimensionPixelSize(R.dimen.tab_grid_card_header_height)
                        + (2 * resources.getDimensionPixelSize(R.dimen.tab_grid_card_margin));
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mCardView = findViewById(R.id.card_view);
        mTitleView = findViewById(R.id.tab_title);
        mFaviconView = findViewById(R.id.tab_favicon);
        mThumbnailView = findViewById(R.id.tab_thumbnail);
    }

    /**
     * Set external dependencies and starting view properties.
     *
     * @param browserControlsStateProvider Provider for top browser controls state.
     * @param multiThumbnailCardProvider Provider for group thumbnails.
     * @param tabContentManagerSupplier Supplier for the {@link TabContentManager}.
     * @param layerTitleCacheSupplier Supplier for the {@link LayerTitleCache}.
     * @param tabModelSelector The {@link TabModelSelector} to use.
     * @param shadowUpdateHost The host to push updates to.
     */
    @Initializer
    public void initialize(
            BrowserControlsStateProvider browserControlsStateProvider,
            MultiThumbnailCardProvider multiThumbnailCardProvider,
            TabContentManager tabContentManager,
            ObservableSupplier<LayerTitleCache> layerTitleCacheSupplier,
            TabModelSelector tabModelSelector,
            ShadowUpdateHost shadowUpdateHost) {
        mBrowserControlStateProvider = browserControlsStateProvider;
        mLayerTitleCacheSupplier = layerTitleCacheSupplier;
        mTabModelSelector = tabModelSelector;
        mShadowUpdateHost = shadowUpdateHost;

        mMultiThumbnailCardProvider = multiThumbnailCardProvider;
        mSingleThumbnailCardProvider = new TabContentManagerThumbnailProvider(tabContentManager);

        mCardView.getBackground().mutate();
        mTitleView.setTextAppearance(R.style.TextAppearance_TextMedium_Primary);

        // To look symmetric when the title is the full width of the card, the title needs an end
        // margin that matches the start padding of the favicon. This is not applicable in the xml
        // layout, because the tab_grid_card_item expects to have an action button that exists after
        // the title to handle this symmetry.
        ConstraintLayout.LayoutParams layoutParams =
                (ConstraintLayout.LayoutParams) mTitleView.getLayoutParams();
        int padding =
                getResources().getDimensionPixelSize(R.dimen.tab_grid_card_favicon_padding_start);
        layoutParams.setMarginEnd(padding);
        mTitleView.setLayoutParams(layoutParams);
    }

    /**
     * Set state on tab drag start.
     *
     * @param tab The {@link Tab} being dragged.
     * @param sourceWidthPx Width of the source strip tab container in px.
     */
    public void prepareForTabDrag(Tab tab, int sourceWidthPx) {
        Context context = getContext();
        boolean isIncognito = tab.isIncognitoBranded();

        // Background color
        mCardView.setBackgroundTintList(
                ColorStateList.valueOf(TabUiThemeUtil.getDraggedTabBackgroundColor(context)));

        // Title text
        LayerTitleCache layerTitleCache = mLayerTitleCacheSupplier.get();
        assumeNonNull(layerTitleCache);
        String defaultTitle = context.getString(R.string.tab_loading_default_title);
        mTitleView.setText(layerTitleCache.getUpdatedTitle(tab, defaultTitle));
        mTitleView.setTextColor(TabUiThemeUtil.getTabTextColor(context, isIncognito));

        // Tab favicon
        Bitmap tabFavicon = TabFavicon.getBitmap(tab);
        boolean fetchFaviconFromHistory = tabFavicon == null;
        if (fetchFaviconFromHistory) {
            mFaviconView.setImageBitmap(layerTitleCache.getDefaultFavicon(tab));
            layerTitleCache.fetchFaviconWithCallback(tab, this::onFaviconFetch);
        } else {
            mFaviconView.setImageBitmap(tabFavicon);
        }

        mFaviconUpdateTabObserver = getFaviconUpdateTabObserver();
        tab.addObserver(mFaviconUpdateTabObserver);

        // Set the thumbnail to visible.
        mThumbnailView.setVisibility(View.VISIBLE);

        prepareForDrag(
                mSingleThumbnailCardProvider,
                tab,
                MultiThumbnailMetadata.createMetadataWithoutUrls(
                        tab.getId(),
                        /* isInTabGroup= */ false,
                        isIncognito,
                        /* tabGroupColor= */ null),
                sourceWidthPx,
                /* isMultiTabDrag= */ false);
    }

    /**
     * Set state on multi tab drag start.
     *
     * @param tab A {@link Tab} in the selection being dragged.
     * @param multiSelectedTabs The list of {@link Tab}s in the selection being dragged.
     * @param sourceWidthPx Width of the source strip tab container in px.
     */
    public void prepareForMultiTabDrag(Tab tab, List<Tab> multiSelectedTabs, int sourceWidthPx) {
        Context context = getContext();
        boolean isIncognito = tab.isIncognitoBranded();

        // Background color
        mCardView.setBackgroundTintList(
                ColorStateList.valueOf(TabUiThemeUtil.getDraggedTabBackgroundColor(context)));

        // Multi tab title text
        int numberOfSelectedTabs = multiSelectedTabs.size();
        String titleText =
                getResources()
                        .getQuantityString(
                                R.plurals.number_of_selected_items,
                                numberOfSelectedTabs,
                                numberOfSelectedTabs);
        mTitleView.setText(titleText);
        mTitleView.setTextColor(TabUiThemeUtil.getTabTextColor(context, isIncognito));

        // Favicon
        LayerTitleCache layerTitleCache = mLayerTitleCacheSupplier.get();
        assumeNonNull(layerTitleCache);
        mFaviconView.setImageBitmap(layerTitleCache.getDefaultFavicon(tab));
        // Hide the thumbnail and favicon to create a "pill" shape.
        mThumbnailView.setVisibility(View.GONE);

        prepareForDrag(
                mMultiThumbnailCardProvider,
                tab,
                /* metadata= */ null,
                sourceWidthPx,
                /* isMultiTabDrag= */ true);
    }

    /**
     * Set state on group drag start.
     *
     * @param tab A {@link Tab} in the group being dragged.
     * @param sourceWidthPx Width of the source group indicator in px.
     */
    public void prepareForGroupDrag(Tab tab, int sourceWidthPx) {
        Context context = getContext();
        boolean isIncognito = tab.isIncognitoBranded();
        TabGroupModelFilter modelFilter =
                mTabModelSelector
                        .getTabGroupModelFilterProvider()
                        .getTabGroupModelFilter(isIncognito);
        assumeNonNull(modelFilter);

        // Background color
        Token tabGroupId = tab.getTabGroupId();
        assert tabGroupId != null : "The tab group ID should be non-null";
        @TabGroupColorId int colorId = modelFilter.getTabGroupColorWithFallback(tabGroupId);

        @ColorInt
        int groupColor =
                TabGroupColorPickerUtils.getTabGroupColorPickerItemColor(
                        context, colorId, isIncognito);
        mCardView.setBackgroundTintList(ColorStateList.valueOf(groupColor));
        mMultiThumbnailCardProvider.setMiniThumbnailPlaceholderColor(
                TabUiThemeUtil.getMiniThumbnailPlaceholderColorForGroup(
                        context, isIncognito, groupColor));

        // Group title text
        LayerTitleCache layerTitleCache = mLayerTitleCacheSupplier.get();
        assumeNonNull(layerTitleCache);
        String titleText =
                layerTitleCache.getUpdatedGroupTitle(
                        tabGroupId,
                        TabGroupTitleUtils.getDisplayableTitle(context, modelFilter, tabGroupId),
                        isIncognito);
        mTitleView.setText(titleText);
        mTitleView.setTextColor(
                TabGroupColorPickerUtils.getTabGroupColorPickerItemTextColor(
                        context, colorId, isIncognito));

        // Clear the tab favicon if needed
        mFaviconView.setImageBitmap(null);
        // Set the thumbnail to visible.
        mThumbnailView.setVisibility(View.VISIBLE);

        prepareForDrag(
                mMultiThumbnailCardProvider,
                tab,
                MultiThumbnailMetadata.createMetadataWithoutUrls(
                        tab.getId(), /* isInTabGroup= */ true, isIncognito, colorId),
                sourceWidthPx,
                /* isMultiTabDrag= */ false);
    }

    private void prepareForDrag(
            ThumbnailProvider thumbnailProvider,
            Tab tab,
            @Nullable MultiThumbnailMetadata metadata,
            int sourceWidthPx,
            boolean isMultiTabDrag) {
        mTab = tab;
        mSourceWidthPx = sourceWidthPx;

        // TODO(crbug.com/40287709): Unify the shared code for creating the GTS-style card.
        // Set to final size. Even though the size will be animated, we need to initially set to the
        // final size, so that we allocate the appropriate amount of space when
        // #onProvideShadowMetrics is called on drag start.
        Size cardSize = getCardSize();
        mWidthPx = isMultiTabDrag ? (int) (cardSize.getWidth() * 0.6f) : cardSize.getWidth();
        mHeightPx = isMultiTabDrag ? mSourceHeightPx : cardSize.getHeight();

        ViewGroup.LayoutParams layoutParams = getLayoutParams();
        layoutParams.width = mWidthPx;
        layoutParams.height = mHeightPx;
        setLayoutParams(layoutParams);
        this.layout(0, 0, mWidthPx, mHeightPx);

        if (isMultiTabDrag) return;
        assert metadata != null;

        // Request the thumbnail.
        Size thumbnailSize = TabUtils.deriveThumbnailSize(cardSize, getContext());
        thumbnailProvider.getTabThumbnailWithCallback(
                metadata,
                thumbnailSize,
                /* isSelected= */ false,
                result -> {
                    if (result != null) {
                        TabUtils.setDrawableAndUpdateImageMatrix(
                                mThumbnailView, result, thumbnailSize);
                    } else {
                        mThumbnailView.setImageDrawable(null);
                    }
                    mShadowUpdateHost.requestUpdate();
                });
        mThumbnailView.updateThumbnailPlaceholder(
                tab.isIncognitoBranded(), /* isSelected= */ false, /* colorId= */ null);
    }

    /** Clear state on tab drag end. */
    public void clear() {
        if (mFaviconUpdateTabObserver != null) {
            assumeNonNull(mTab).removeObserver(mFaviconUpdateTabObserver);
            mTab = null;
            mFaviconUpdateTabObserver = null;
        }
    }

    /** Run the expand animation. */
    public void expand() {
        if (mRunningAnimator != null && mRunningAnimator.isRunning()) mRunningAnimator.end();

        setProgress(0.f);
        mRunningAnimator = ObjectAnimator.ofFloat(this, PROGRESS, 1.f);
        mRunningAnimator.setInterpolator(Interpolators.STANDARD_DEFAULT_EFFECTS);
        mRunningAnimator.setDuration(ANIM_EXPAND_MS);
        mRunningAnimator.start();
    }

    private void setProgress(float progress) {
        assert progress >= 0.f && progress <= 1.f : "Invalid animation progress value.";
        mProgress = progress;

        ViewGroup.LayoutParams layoutParams = getLayoutParams();
        layoutParams.width = (int) lerp(mSourceWidthPx, mWidthPx, progress);
        layoutParams.height = (int) lerp(mSourceHeightPx, mHeightPx, progress);
        setLayoutParams(layoutParams);
        post(() -> mShadowUpdateHost.requestUpdate());
    }

    private float getProgress() {
        return mProgress;
    }

    /** Linear interpolate from start value to stop value by amount [0..1] */
    private float lerp(float start, float stop, float amount) {
        return start + ((stop - start) * amount);
    }

    private Size getCardSize() {
        Context context = getContext();
        float density = context.getResources().getDisplayMetrics().density;

        // XR uses a separate target width.
        if (DeviceInfo.isXr()) {
            int width = (int) (density * WIDTH_ON_XR_DP);
            int height =
                    TabUtils.deriveGridCardHeight(width, context, mBrowserControlStateProvider);
            return new Size(width, height);
        }

        // Otherwise, use the default max width and max height to determine the size.
        int width = (int) (density * WIDTH_DP);
        int height = TabUtils.deriveGridCardHeight(width, context, mBrowserControlStateProvider);
        if (height > HEIGHT_DP) {
            height = (int) (density * HEIGHT_DP);
            width = TabUtils.deriveGridCardWidth(height, context, mBrowserControlStateProvider);
        }
        return new Size(width, height);
    }

    private void onFaviconFetch(Bitmap image, GURL iconUrl) {
        if (image == null) return;

        mFaviconView.setImageBitmap(image);
        mShadowUpdateHost.requestUpdate();
    }

    private TabObserver getFaviconUpdateTabObserver() {
        return new EmptyTabObserver() {
            @Override
            public void onFaviconUpdated(Tab tab, @Nullable Bitmap icon, @Nullable GURL iconUrl) {
                if (icon == null) {
                    icon = TabFavicon.getBitmap(tab);
                    if (icon == null) {
                        LayerTitleCache layerTitleCache = mLayerTitleCacheSupplier.get();
                        assumeNonNull(layerTitleCache);
                        icon = layerTitleCache.getDefaultFavicon(tab);
                    }
                }
                mFaviconView.setImageBitmap(icon);
                mShadowUpdateHost.requestUpdate();
            }
        };
    }

    protected @Nullable Tab getTabForTesting() {
        return mTab;
    }

    protected @Nullable Animator getRunningAnimatorForTesting() {
        return mRunningAnimator;
    }
}
