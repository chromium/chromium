// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.content.Context;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.coordinatorlayout.widget.CoordinatorLayout;
import androidx.core.view.ViewCompat;

import org.chromium.base.MathUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementFieldTrial;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeProvider;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.ui.base.LocalizationUtils;

public class StripTabHoverCardView extends LinearLayout {
    // The max width of the tab hover card in terms of the enclosing window width percent.
    static final float HOVER_CARD_MAX_WIDTH_PERCENT = 0.9f;

    private TextView mTitleView;
    private TextView mUrlView;
    private TabModelSelector mTabModelSelector;
    private TabModelSelectorObserver mTabModelSelectorObserver;

    public StripTabHoverCardView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    public StripTabHoverCardView(Context context) {
        super(context);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitleView = findViewById(R.id.title);
        mUrlView = findViewById(R.id.url);
    }

    /**
     * Show the strip tab hover card.
     * @param hoveredTab The {@link Tab} instance of the hovered tab.
     * @param hoveredStripLayoutTab The {@link StripLayoutTab} instance of the hovered tab.
     * @param isSelectedTab Whether the hovered tab is selected, {@code true} if the hovered tab is
     *         also the selected tab, {@code false} otherwise.
     * @param height The height of the tab strip stack.
     */
    public void show(Tab hoveredTab, StripLayoutTab hoveredStripLayoutTab, boolean isSelectedTab,
            float height) {
        if (hoveredTab == null) return;
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

        float[] position = getHoverCardPosition(hoveredStripLayoutTab, isSelectedTab, height);
        setX(position[0]);
        setY(position[1]);

        setVisibility(VISIBLE);
    }

    /**
     * Hide the strip tab hover card.
     */
    public void hide() {
        setVisibility(GONE);
    }

    /**
     * Perform tasks after the view is inflated: update the hover card colors, and add a {@link
     * TabModelSelectorObserver} to update the view when a tab model is selected.
     * @param tabModelSelector The {@link TabModelSelector} to observe.
     */
    public void initialize(TabModelSelector tabModelSelector) {
        mTabModelSelector = tabModelSelector;
        mTabModelSelectorObserver = new TabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                updateHoverCardColors(newModel.isIncognito());
            }
        };
        mTabModelSelector.addObserver(mTabModelSelectorObserver);
        updateHoverCardColors(mTabModelSelector.isIncognitoSelected());
    }

    /**
     * Update the hover card background and text colors based on the theme and incognito mode.
     * @param incognito Whether the incognito mode is selected, {@code true} for incognito, {@link
     *         false} otherwise.
     */
    public void updateHoverCardColors(boolean incognito) {
        if (!ChromeFeatureList.isEnabled(
                    ChromeFeatureList.ADVANCED_PERIPHERALS_SUPPORT_TAB_STRIP)) {
            return;
        }

        mTitleView.setTextColor(
                TabUiThemeProvider.getStripTabHoverCardTextColorPrimary(getContext(), incognito));
        mUrlView.setTextColor(
                TabUiThemeProvider.getStripTabHoverCardTextColorSecondary(getContext(), incognito));

        ViewCompat.setBackgroundTintList(this,
                TabUiThemeProvider.getStripTabHoverCardBackgroundTintList(getContext(), incognito));
    }

    public void destroy() {
        if (mTabModelSelector != null) {
            mTabModelSelector.removeObserver(mTabModelSelectorObserver);
            mTabModelSelector = null;
        }
    }

    /**
     * Get the x and y coordinates of the position of the hover card, in px.
     *
     * @param hoveredTab The {@link StripLayoutTab} that is hovered on.
     * @param isSelectedTab Whether the tab is the selected tab, {@code true} if the hovered tab is
     *         also the selected tab, {@code false} otherwise.
     * @param height The height of the strip stack, to determine the y position of the card.
     * @return A float array specifying the x (array[0]) and y (array[1]) coordinates of the
     *         position of the hover card, in px.
     */
    float[] getHoverCardPosition(StripLayoutTab hoveredTab, boolean isSelectedTab, float height) {
        boolean isFolioEnabled = TabManagementFieldTrial.isTabStripFolioEnabled();
        boolean isDetachedEnabled = TabManagementFieldTrial.isTabStripDetachedEnabled();

        // 1. Determine the window width.
        DisplayMetrics displayMetrics = getContext().getResources().getDisplayMetrics();
        float displayDensity = displayMetrics.density;
        float windowWidthPx = displayMetrics.widthPixels;
        float windowWidthDp = windowWidthPx / displayDensity;

        // 2. Determine the hover card width, making adjustments relative to the window width if
        // applicable.
        float hoverCardWidthPx = getContext().getResources().getDimension(
                org.chromium.chrome.R.dimen.tab_hover_card_width);
        // Hover card width should be a maximum of 90% of the window width.
        hoverCardWidthPx = Math.min(hoverCardWidthPx, HOVER_CARD_MAX_WIDTH_PERCENT * windowWidthPx);
        float hoverCardWidthDp = hoverCardWidthPx / displayDensity;
        // Update the card LayoutParams if an adjustment on the original width is required.
        if (hoverCardWidthPx
                != getContext().getResources().getDimension(
                        org.chromium.chrome.R.dimen.tab_hover_card_width)) {
            var layoutParams = getLayoutParams();
            setLayoutParams(new CoordinatorLayout.LayoutParams(
                    Math.round(hoverCardWidthPx), layoutParams.height));
        }

        // 3. Determine the horizontal position of the hover card.
        float hoverCardXDp = LocalizationUtils.isLayoutRtl()
                ? (hoveredTab.getDrawX() - (hoverCardWidthDp - hoveredTab.getWidth()))
                : hoveredTab.getDrawX();
        // Adjust the TSR detached and inactive folio tab hover card to align with the tab container
        // edge.
        if (isDetachedEnabled || (isFolioEnabled && !isSelectedTab)) {
            hoverCardXDp += MathUtils.flipSignIf(
                    getContext().getResources().getDimension(
                            org.chromium.chrome.R.dimen.tsr_no_feet_tab_hover_card_x_offset)
                            / displayDensity,
                    LocalizationUtils.isLayoutRtl());
        }

        float windowHorizontalMarginDp =
                getContext().getResources().getDimension(
                        org.chromium.chrome.R.dimen.tab_hover_card_window_horizontal_margin)
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
        // Adjust the TSR detached tab hover card to be at a fixed distance from the tab container.
        if (isDetachedEnabled) {
            hoverCardYDp += StripLayoutHelper.FOLIO_DETACHED_BOTTOM_MARGIN_DP;
        }

        return new float[] {hoverCardXDp * displayDensity, hoverCardYDp * displayDensity};
    }

    void setTitleViewForTesting(TextView titleView) {
        mTitleView = titleView;
    }

    void setUrlViewForTesting(TextView urlView) {
        mUrlView = urlView;
    }

    TabModelSelectorObserver getTabModelSelectorObserverForTesting() {
        return mTabModelSelectorObserver;
    }
}
