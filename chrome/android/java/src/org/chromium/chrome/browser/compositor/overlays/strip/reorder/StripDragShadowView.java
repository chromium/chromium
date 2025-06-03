// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

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
import android.widget.RelativeLayout;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabContentManagerThumbnailProvider;
import org.chromium.chrome.browser.tab_ui.TabThumbnailView;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.MultiThumbnailCardProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tab_groups.TabGroupColorPickerUtils;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.util.XrUtils;
import org.chromium.url.GURL;

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
    private Animator mRunningAnimator;

    // External Dependencies
    private BrowserControlsStateProvider mBrowserControlStateProvider;
    private Supplier<LayerTitleCache> mLayerTitleCacheSupplier;
    private TabModelSelector mTabModelSelector;
    private ShadowUpdateHost mShadowUpdateHost;

    // Thumbnail Providers
    private MultiThumbnailCardProvider mMultiThumbnailCardProvider;
    private TabContentManagerThumbnailProvider mSingleThumbnailCardProvider;

    // Current Drag State
    private Tab mTab;
    private TabObserver mFaviconUpdateTabObserver;

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
    public void initialize(
            BrowserControlsStateProvider browserControlsStateProvider,
            MultiThumbnailCardProvider multiThumbnailCardProvider,
            Supplier<TabContentManager> tabContentManagerSupplier,
            Supplier<LayerTitleCache> layerTitleCacheSupplier,
            TabModelSelector tabModelSelector,
            ShadowUpdateHost shadowUpdateHost) {
        mBrowserControlStateProvider = browserControlsStateProvider;
        mLayerTitleCacheSupplier = layerTitleCacheSupplier;
        mTabModelSelector = tabModelSelector;
        mShadowUpdateHost = shadowUpdateHost;

        mMultiThumbnailCardProvider = multiThumbnailCardProvider;
        mSingleThumbnailCardProvider =
                new TabContentManagerThumbnailProvider(tabContentManagerSupplier.get());

        mCardView.getBackground().mutate();
        mTitleView.setTextAppearance(R.style.TextAppearance_TextMedium_Primary);

        // To look symmetric when the title is the full width of the card, the title needs an end
        // margin that matches the start padding of the favicon. This is not applicable in the xml
        // layout, because the tab_grid_card_item expects to have an action button that exists after
        // the title to handle this symmetry.
        RelativeLayout.LayoutParams layoutParams =
                (RelativeLayout.LayoutParams) mTitleView.getLayoutParams();
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
                ColorStateList.valueOf(
                        TabUiThemeUtil.getTabStripSelectedTabColor(context, isIncognito)));

        // Title text
        LayerTitleCache layerTitleCache = mLayerTitleCacheSupplier.get();
        String defaultTitle = context.getString(R.string.tab_loading_default_title);
        mTitleView.setText(layerTitleCache.getUpdatedTitle(tab, defaultTitle));
        mTitleView.setTextColor(TabUiThemeUtil.getTabTextColor(context, isIncognito));

        // Tab favicon
        boolean fetchFaviconFromHistory = tab.isNativePage() || tab.getWebContents() == null;
        mFaviconView.setImageBitmap(layerTitleCache.getOriginalFavicon(tab));
        if (fetchFaviconFromHistory) {
            layerTitleCache.fetchFaviconWithCallback(tab, this::onFaviconFetch);
        }

        mFaviconUpdateTabObserver = getFaviconUpdateTabObserver();
        tab.addObserver(mFaviconUpdateTabObserver);
        prepareForDrag(mSingleThumbnailCardProvider, tab, sourceWidthPx);
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

        // Background color
        @TabGroupColorId int colorId = modelFilter.getTabGroupColorWithFallback(tab.getRootId());
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
        String titleText =
                layerTitleCache.getUpdatedGroupTitle(
                        tab.getTabGroupId(),
                        StripLayoutUtils.getGroupTitleText(context, modelFilter, tab),
                        isIncognito);
        mTitleView.setText(titleText);
        mTitleView.setTextColor(
                TabGroupColorPickerUtils.getTabGroupColorPickerItemTextColor(
                        context, colorId, isIncognito));

        // Clear the tab favicon if needed
        mFaviconView.setImageBitmap(null);

        prepareForDrag(mMultiThumbnailCardProvider, tab, sourceWidthPx);
    }

    private void prepareForDrag(ThumbnailProvider thumbnailProvider, Tab tab, int sourceWidthPx) {
        mTab = tab;
        mSourceWidthPx = sourceWidthPx;

        // TODO(crbug.com/40287709): Unify the shared code for creating the GTS-style card.
        // Set to final size. Even though the size will be animated, we need to initially set to the
        // final size, so that we allocate the appropriate amount of space when
        // #onProvideShadowMetrics is called on drag start.
        Size cardSize = getCardSize();
        mWidthPx = cardSize.getWidth();
        mHeightPx = cardSize.getHeight();

        ViewGroup.LayoutParams layoutParams = getLayoutParams();
        layoutParams.width = mWidthPx;
        layoutParams.height = mHeightPx;
        setLayoutParams(layoutParams);
        this.layout(0, 0, mWidthPx, mHeightPx);

        // Request the thumbnail.
        Size thumbnailSize = TabUtils.deriveThumbnailSize(cardSize, getContext());
        thumbnailProvider.getTabThumbnailWithCallback(
                tab.getId(),
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
                tab.isIncognitoBranded(), /* isSelected= */ false);
    }

    /** Clear state on tab drag end. */
    public void clear() {
        mTab.removeObserver(mFaviconUpdateTabObserver);
        mTab = null;
        mFaviconUpdateTabObserver = null;
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
        if (XrUtils.isXrDevice()) {
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
            public void onFaviconUpdated(Tab tab, Bitmap icon, GURL iconUrl) {
                if (icon != null) {
                    mFaviconView.setImageBitmap(icon);
                } else {
                    mFaviconView.setImageBitmap(
                            mLayerTitleCacheSupplier.get().getOriginalFavicon(tab));
                }
                mShadowUpdateHost.requestUpdate();
            }
        };
    }

    protected Tab getTabForTesting() {
        return mTab;
    }

    protected Animator getRunningAnimatorForTesting() {
        return mRunningAnimator;
    }
}
