// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.content.Context;
import android.graphics.drawable.BitmapDrawable;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.util.Size;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.coordinatorlayout.widget.CoordinatorLayout;
import androidx.core.view.ViewCompat;

import org.chromium.base.Callback;
import org.chromium.base.MathUtils;
import org.chromium.base.SysUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabThumbnailView;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeProvider;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.ui.base.LocalizationUtils;

public class StripTabHoverCardView extends FrameLayout {
    // The max width of the tab hover card in terms of the enclosing window width percent.
    static final float HOVER_CARD_MAX_WIDTH_PERCENT = 0.9f;
    static final int INVALID_TAB_ID = -1;

    private ViewGroup mContentView;
    private TextView mTitleView;
    private TextView mUrlView;
    private TabThumbnailView mThumbnailView;
    private TabModelSelector mTabModelSelector;
    private Callback<TabModel> mCurrentTabModelObserver;
    private TabContentManager mTabContentManager;

    private int mLastHoveredTabId = INVALID_TAB_ID;
    private boolean mIsShowing;

    public StripTabHoverCardView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mContentView = findViewById(R.id.content_view);
        mTitleView = mContentView.findViewById(R.id.title);
        mUrlView = mContentView.findViewById(R.id.url);
        mThumbnailView = mContentView.findViewById(R.id.thumbnail);
        maybeUpdateBackgroundOnLowEndDevice();
    }

    /**
     * Show the strip tab hover card.
     *
     * @param hoveredTab The {@link Tab} instance of the hovered tab.
     * @param isSelectedTab Whether the hovered tab is selected, {@code true} if the hovered tab is
     *     also the selected tab, {@code false} otherwise.
     * @param tabX To compute hover card positioning.
     * @param tabWidth To compute hover card positioning.
     * @param height The height of the tab strip stack.
     */
    public void show(
            Tab hoveredTab, boolean isSelectedTab, float tabX, float tabWidth, float height) {
        if (hoveredTab == null) return;
        mLastHoveredTabId = hoveredTab.getId();
        mIsShowing = true;
        updateThumbnail(hoveredTab);

        mTitleView.setText(hoveredTab.getTitle());
        String url = hoveredTab.getUrl().getHost();
        // If the URL is a Chrome scheme, display the GURL spec instead of the host. For e.g., use
        // chrome://newtab instead of just newtab on the hover card.
        if (UrlUtilities.isInternalScheme(hoveredTab.getUrl())) {
            url = hoveredTab.getUrl().getSpec();
            // GURL#getSpec() returns a string with a trailing "/", remove this.
            url = url.replaceFirst("/$", "");
        }
        mUrlView.setText(url);

        float[] position = getHoverCardPosition(isSelectedTab, tabX, tabWidth, height);
        setX(position[0]);
        setY(position[1]);

        setVisibility(VISIBLE);
    }

    /** Hide the strip tab hover card. */
    public void hide() {
        mIsShowing = false;
        setVisibility(GONE);
        mThumbnailView.setImageDrawable(null);
        mLastHoveredTabId = INVALID_TAB_ID;
    }

    /**
     * Perform tasks after the view is inflated: update the hover card colors, and add a {@link
     * Callback<TabModel>} to tab model supplier to update the view when a tab model is selected.
     *
     * @param tabModelSelector The {@link TabModelSelector} to observe.
     * @param tabContentManagerSupplier Supplier of the {@link TabContentManager} instance.
     */
    public void initialize(
            TabModelSelector tabModelSelector,
            ObservableSupplier<TabContentManager> tabContentManagerSupplier) {
        mTabModelSelector = tabModelSelector;
        mTabContentManager = tabContentManagerSupplier.get();
        mCurrentTabModelObserver =
                (tabModel) -> {
                    updateHoverCardColors(tabModel.isIncognitoBranded());
                };
        mTabModelSelector.getCurrentTabModelSupplier().addObserver(mCurrentTabModelObserver);
        updateHoverCardColors(mTabModelSelector.isIncognitoSelected());
    }

    /**
     * Update the hover card background and text colors based on the theme and incognito mode.
     *
     * @param incognito Whether the incognito mode is selected, {@code true} for incognito, {@link
     *     false} otherwise.
     */
    public void updateHoverCardColors(boolean incognito) {
        mTitleView.setTextColor(
                TabUiThemeProvider.getStripTabHoverCardTextColorPrimary(getContext(), incognito));
        mUrlView.setTextColor(
                TabUiThemeProvider.getStripTabHoverCardTextColorSecondary(getContext(), incognito));

        ViewCompat.setBackgroundTintList(
                this,
                TabUiThemeProvider.getStripTabHoverCardBackgroundTintList(getContext(), incognito));
    }

    public void destroy() {
        if (mTabModelSelector != null) {
            mTabModelSelector.getCurrentTabModelSupplier().removeObserver(mCurrentTabModelObserver);
            mTabModelSelector = null;
        }
    }

    /**
     * Get the x and y coordinates of the position of the hover card, in px.
     *
     * @param isSelectedTab Whether the tab is the selected tab, {@code true} if the hovered tab is
     *     also the selected tab, {@code false} otherwise.
     * @param tabX The tab x-position to compute hover card positioning.
     * @param tabWidth The tab width to compute hover card positioning.
     * @param height The height of the strip stack, to determine the y position of the card.
     * @return A float array specifying the x (array[0]) and y (array[1]) coordinates of the
     *     position of the hover card, in px.
     */
    float[] getHoverCardPosition(boolean isSelectedTab, float tabX, float tabWidth, float height) {
        // 1. Determine the window width.
        DisplayMetrics displayMetrics = getContext().getResources().getDisplayMetrics();
        float displayDensity = displayMetrics.density;
        float windowWidthPx = displayMetrics.widthPixels;
        float windowWidthDp = windowWidthPx / displayDensity;

        // 2. Determine the hover card width, making adjustments relative to the window width if
        // applicable.
        float hoverCardWidthPx =
                getContext().getResources().getDimension(R.dimen.tab_hover_card_width);
        // Hover card width should be a maximum of 90% of the window width.
        hoverCardWidthPx = Math.min(hoverCardWidthPx, HOVER_CARD_MAX_WIDTH_PERCENT * windowWidthPx);
        float hoverCardWidthDp = hoverCardWidthPx / displayDensity;
        // Update the card LayoutParams if an adjustment on the current width is required.
        var layoutParams = getLayoutParams();
        if (hoverCardWidthPx != layoutParams.width) {
            setLayoutParams(
                    new CoordinatorLayout.LayoutParams(
                            Math.round(hoverCardWidthPx), layoutParams.height));
        }

        // 3. Determine the horizontal position of the hover card.
        float hoverCardXDp =
                LocalizationUtils.isLayoutRtl() ? (tabX - (hoverCardWidthDp - tabWidth)) : tabX;
        // Adjust the inactive folio tab hover card to align with the tab container
        // edge.
        if (!isSelectedTab) {
            hoverCardXDp +=
                    MathUtils.flipSignIf(
                            getContext()
                                            .getResources()
                                            .getDimension(R.dimen.inactive_tab_hover_card_x_offset)
                                    / displayDensity,
                            LocalizationUtils.isLayoutRtl());
        }

        // On a low-end device adjust the card to account for the shadow length of the background
        // drawable.
        if (SysUtils.isLowEndDevice()) {
            hoverCardXDp -=
                    getContext().getResources().getDimension(R.dimen.tab_hover_card_elevation)
                            / displayDensity;
        }

        float windowHorizontalMarginDp =
                getContext()
                                .getResources()
                                .getDimension(R.dimen.tab_hover_card_window_horizontal_margin)
                        / displayDensity;
        // Align the hover card at a minimum horizontal margin of 8dp from the window left edge.
        if (hoverCardXDp < windowHorizontalMarginDp) {
            hoverCardXDp = windowHorizontalMarginDp;
        }
        // Align the hover card at a minimum horizontal margin of 8dp from the window right edge.
        if (hoverCardXDp + hoverCardWidthDp > windowWidthDp - windowHorizontalMarginDp) {
            hoverCardXDp = windowWidthDp - hoverCardWidthDp - windowHorizontalMarginDp;
        }

        // 4. Determine the vertical position of the hover card.
        float hoverCardYDp = height;

        // On a low-end device adjust the card to account for the shadow length of the background
        // drawable.
        if (SysUtils.isLowEndDevice()) {
            hoverCardYDp -=
                    getContext().getResources().getDimension(R.dimen.tab_hover_card_elevation)
                            / displayDensity;
        }

        return new float[] {hoverCardXDp * displayDensity, hoverCardYDp * displayDensity};
    }

    void maybeUpdateBackgroundOnLowEndDevice() {
        if (!SysUtils.isLowEndDevice()) return;
        mContentView.setBackgroundResource(R.drawable.popup_bg_8dp);
        setBackground(null);
    }

    private void updateThumbnail(Tab hoveredTab) {
        var thumbnailSize =
                new Size(
                        Math.round(
                                getContext()
                                        .getResources()
                                        .getDimension(R.dimen.tab_hover_card_width)),
                        Math.round(
                                getContext()
                                        .getResources()
                                        .getDimension(R.dimen.tab_hover_card_thumbnail_height)));
        mTabContentManager.getTabThumbnailWithCallback(
                hoveredTab.getId(),
                thumbnailSize,
                thumbnail -> {
                    // Thumbnail request was for a previous hover.
                    if (hoveredTab.getId() != mLastHoveredTabId) return;
                    // View is not visible any more.
                    if (!mIsShowing) return;
                    if (thumbnail != null) {
                        TabUtils.setDrawableAndUpdateImageMatrix(
                                mThumbnailView, new BitmapDrawable(thumbnail), thumbnailSize);
                    } else {
                        // Always use the unselected tab version of the thumbnail placeholder.
                        mThumbnailView.updateThumbnailPlaceholder(
                                hoveredTab.isIncognito(), /* isSelected= */ false);
                    }
                    mThumbnailView.setVisibility(VISIBLE);
                });
    }

    Callback<TabModel> getCurrentTabModelObserverForTesting() {
        return mCurrentTabModelObserver;
    }

    int getLastHoveredTabIdForTesting() {
        return mLastHoveredTabId;
    }

    boolean isShowingForTesting() {
        return mIsShowing;
    }
}
