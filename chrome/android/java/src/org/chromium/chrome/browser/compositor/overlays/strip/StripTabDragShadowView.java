// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.util.AttributeSet;
import android.util.Size;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabThumbnailView;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
import org.chromium.url.GURL;

public class StripTabDragShadowView extends FrameLayout {
    // Constants
    @VisibleForTesting protected static final int WIDTH_DP = 264;

    // Children Views
    private View mCardView;
    private TextView mTitleView;
    private ImageView mFaviconView;
    private TabThumbnailView mThumbnailView;

    // Internal State
    private Boolean mIncognito;
    private int mWidthPx;

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

        mWidthPx = (int) (context.getResources().getDisplayMetrics().density * WIDTH_DP);
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

        int padding = (int) TabUiThemeProvider.getTabCardTopFaviconPadding(getContext());
        mFaviconView.setPadding(padding, padding, padding, padding);
        mCardView.getBackground().mutate();
        mTitleView.setTextAppearance(R.style.TextAppearance_TextMedium_Primary);

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
     */
    public void setTab(Tab tab) {
        mTab = tab;
        mTab.addObserver(mFaviconUpdateTabObserver);

        update();
    }

    /** Clear state on tab drag end. */
    public void clear() {
        mTab.removeObserver(mFaviconUpdateTabObserver);
        mTab = null;
    }

    private void update() {
        // TODO(https://crbug.com/1499119): Unify the shared code for creating the GTS-style card.
        // Set to final size. Even though the size will be animated, we need to initially set to the
        // final size, so that we allocate the appropriate amount of space when
        // #onProvideShadowMetrics is called on drag start.
        int heightPx =
                TabUtils.deriveGridCardHeight(mWidthPx, getContext(), mBrowserControlStateProvider);

        ViewGroup.LayoutParams layoutParams = getLayoutParams();
        layoutParams.width = mWidthPx;
        layoutParams.height = heightPx;
        setLayoutParams(layoutParams);
        this.layout(0, 0, mWidthPx, heightPx);

        // Request the thumbnail.
        Size cardSize = new Size(mWidthPx, heightPx);
        Size thumbnailSize = TabUtils.deriveThumbnailSize(cardSize, getContext());
        mTabContentManagerSupplier
                .get()
                .getTabThumbnailWithCallback(
                        mTab.getId(),
                        thumbnailSize,
                        result -> {
                            if (result != null) {
                                TabUtils.setBitmapAndUpdateImageMatrix(
                                        mThumbnailView, result, thumbnailSize);
                            } else {
                                mThumbnailView.setImageDrawable(null);
                            }
                            mShadowUpdateHost.requestUpdate();
                        },
                        /* forceUpdate= */ true,
                        /* writeBack= */ true);

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

    protected Tab getTabForTesting() {
        return mTab;
    }
}
