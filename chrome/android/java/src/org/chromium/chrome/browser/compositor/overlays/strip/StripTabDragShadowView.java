// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.animation.Animator;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
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
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabThumbnailView;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.url.GURL;

public class StripTabDragShadowView extends FrameLayout {
    private static final FloatProperty<StripTabDragShadowView> PROGRESS =
            new FloatProperty<>("progress") {
                @Override
                public void setValue(StripTabDragShadowView object, float v) {
                    object.setProgress(v);
                }

                @Override
                public Float get(StripTabDragShadowView object) {
                    return object.getProgress();
                }
            };

    // Constants
    @VisibleForTesting protected static final int WIDTH_DP = 264;
    private static final long ANIM_EXPAND_MS = 200L;

    // Children Views
    private View mCardView;
    private TextView mTitleView;
    private ImageView mFaviconView;
    private TabThumbnailView mThumbnailView;

    // Internal State
    private Boolean mIncognito;
    private int mTabWidthPx;
    private int mTabHeightPx;
    private int mWidthPx;
    private int mHeightPx;
    private float mProgress;
    private Animator mRunningAnimator;

    // External Dependencies
    private BrowserControlsStateProvider mBrowserControlStateProvider;
    private Supplier<TabContentManager> mTabContentManagerSupplier;
    private Supplier<LayerTitleCache> mLayerTitleCacheSupplier;
    private ShadowUpdateHost mShadowUpdateHost;

    private Tab mTab;
    private TabObserver mFaviconUpdateTabObserver;

    public interface ShadowUpdateHost {
        /**
         * Notify the host of this drag shadow that the source view has been changed and its drag
         * shadow needs to be updated accordingly.
         */
        void requestUpdate();
    }

    public StripTabDragShadowView(Context context, AttributeSet attrs) {
        super(context, attrs);

        Resources resources = context.getResources();
        mWidthPx = (int) (resources.getDisplayMetrics().density * WIDTH_DP);
        mTabHeightPx =
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
     * @param tabContentManagerSupplier Supplier for the {@link TabContentManager}.
     * @param layerTitleCacheSupplier Supplier for the {@link LayerTitleCache}.
     * @param shadowUpdateHost The host to push updates to.
     */
    public void initialize(
            BrowserControlsStateProvider browserControlsStateProvider,
            Supplier<TabContentManager> tabContentManagerSupplier,
            Supplier<LayerTitleCache> layerTitleCacheSupplier,
            ShadowUpdateHost shadowUpdateHost) {
        mBrowserControlStateProvider = browserControlsStateProvider;
        mTabContentManagerSupplier = tabContentManagerSupplier;
        mLayerTitleCacheSupplier = layerTitleCacheSupplier;
        mShadowUpdateHost = shadowUpdateHost;

        mCardView.getBackground().mutate();
        mTitleView.setTextAppearance(R.style.TextAppearance_TextMedium_Primary);

        // To look symmetric when the title is the full width of the card, the title needs an end
        // margin that matches the start padding of the favicon. This is not applicable in the xml
        // layout, because the tab_grid_card_item expects to have an action button that exists after
        // the title to handle this symmetry.
        RelativeLayout.LayoutParams layoutParams =
                (RelativeLayout.LayoutParams) mTitleView.getLayoutParams();
        int padding = getResources().getDimensionPixelSize(R.dimen.tab_grid_card_favicon_padding);
        layoutParams.setMarginEnd(padding);
        mTitleView.setLayoutParams(layoutParams);

        mFaviconUpdateTabObserver =
                new EmptyTabObserver() {
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

    /**
     * Set state on tab drag start.
     *
     * @param tab The {@link Tab} being dragged.
     * @param tabWidthPx Width of the source strip tab container in px.
     */
    public void prepareForDrag(Tab tab, int tabWidthPx) {
        mTab = tab;
        mTab.addObserver(mFaviconUpdateTabObserver);
        mTabWidthPx = tabWidthPx;

        update();
    }

    /** Clear state on tab drag end. */
    public void clear() {
        mTab.removeObserver(mFaviconUpdateTabObserver);
        mTab = null;
    }

    /** Run the expand animation. */
    public void expand() {
        if (mRunningAnimator != null && mRunningAnimator.isRunning()) mRunningAnimator.end();

        setProgress(0.f);
        mRunningAnimator = ObjectAnimator.ofFloat(this, PROGRESS, 1.f);
        mRunningAnimator.setInterpolator(Interpolators.EMPHASIZED);
        mRunningAnimator.setDuration(ANIM_EXPAND_MS);
        mRunningAnimator.start();
    }

    private void update() {
        // TODO(crbug.com/40287709): Unify the shared code for creating the GTS-style card.
        // Set to final size. Even though the size will be animated, we need to initially set to the
        // final size, so that we allocate the appropriate amount of space when
        // #onProvideShadowMetrics is called on drag start.
        mHeightPx =
                TabUtils.deriveGridCardHeight(mWidthPx, getContext(), mBrowserControlStateProvider);

        ViewGroup.LayoutParams layoutParams = getLayoutParams();
        layoutParams.width = mWidthPx;
        layoutParams.height = mHeightPx;
        setLayoutParams(layoutParams);
        this.layout(0, 0, mWidthPx, mHeightPx);

        // Request the thumbnail.
        Size cardSize = new Size(mWidthPx, mHeightPx);
        Size thumbnailSize = TabUtils.deriveThumbnailSize(cardSize, getContext());
        mTabContentManagerSupplier
                .get()
                .getTabThumbnailWithCallback(
                        mTab.getId(),
                        thumbnailSize,
                        result -> {
                            if (result != null) {
                                TabUtils.setDrawableAndUpdateImageMatrix(
                                        mThumbnailView, new BitmapDrawable(result), thumbnailSize);
                            } else {
                                mThumbnailView.setImageDrawable(null);
                            }
                            mShadowUpdateHost.requestUpdate();
                        });

        // Update title and set original favicon.
        LayerTitleCache layerTitleCache = mLayerTitleCacheSupplier.get();

        mTitleView.setText(
                layerTitleCache.getUpdatedTitle(
                        mTab, getContext().getString(R.string.tab_loading_default_title)));

        boolean fetchFaviconFromHistory = mTab.isNativePage() || mTab.getWebContents() == null;
        mFaviconView.setImageBitmap(layerTitleCache.getOriginalFavicon(mTab));
        if (fetchFaviconFromHistory) {
            layerTitleCache.fetchFaviconWithCallback(
                    mTab,
                    (image, iconUrl) -> {
                        if (image != null) {
                            mFaviconView.setImageBitmap(image);
                            mShadowUpdateHost.requestUpdate();
                        }
                    });
        }

        // Update incognito state.
        setIncognito(mTab.isIncognito());
    }

    private void setIncognito(boolean incognito) {
        if (mIncognito == null || mIncognito != incognito) {
            mIncognito = incognito;

            mCardView.setBackgroundTintList(
                    ColorStateList.valueOf(
                            TabUiThemeUtil.getTabStripContainerColor(
                                    getContext(),
                                    mIncognito,
                                    /* foreground= */ true,
                                    /* isReordering= */ false,
                                    /* isPlaceholder= */ false,
                                    /* isHovered= */ false)));

            @ColorInt
            int textColor =
                    AppCompatResources.getColorStateList(
                                    getContext(),
                                    mIncognito
                                            ? R.color.compositor_tab_title_bar_text_incognito
                                            : R.color.compositor_tab_title_bar_text)
                            .getDefaultColor();
            mTitleView.setTextColor(textColor);

            mThumbnailView.updateThumbnailPlaceholder(mIncognito, false);
        }
    }

    private void setProgress(float progress) {
        assert progress >= 0.f && progress <= 1.f : "Invalid animation progress value.";
        mProgress = progress;

        ViewGroup.LayoutParams layoutParams = getLayoutParams();
        layoutParams.width = (int) lerp(mTabWidthPx, mWidthPx, progress);
        layoutParams.height = (int) lerp(mTabHeightPx, mHeightPx, progress);
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

    protected Tab getTabForTesting() {
        return mTab;
    }

    protected Animator getRunningAnimatorForTesting() {
        return mRunningAnimator;
    }
}
