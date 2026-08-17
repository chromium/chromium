// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.drawable.BitmapDrawable;
import android.text.format.Formatter;
import android.util.AttributeSet;
import android.util.Size;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;
import androidx.annotation.Px;
import androidx.annotation.StringRes;
import androidx.core.view.ViewCompat;

import org.chromium.base.Callback;
import org.chromium.base.MathUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabThumbnailView;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.tabs.TabAlert;

import java.util.function.Supplier;

@NullMarked
public class TabHoverCardView extends FrameLayout {
    // The max width of the tab hover card in terms of the enclosing window width percent.
    public static final float HOVER_CARD_MAX_WIDTH_PERCENT = 0.9f;
    public static final int MAX_HOVER_CARD_DELAY_MS = 800;
    public static final int MIN_HOVER_CARD_DELAY_MS = 300;
    static final int INVALID_TAB_ID = -1;

    /**
     * Delay is calculated as a logarithmic scale and bounded by a minimum width based on the width
     * of a pinned tab (or collapsed rail) and a maximum of standard width.
     *
     * <pre>
     *  delay (ms)
     *           |
     * max delay-|                                    *
     *           |                          *
     *           |                    *
     *           |                *
     *           |            *
     *           |         *
     *           |       *
     *           |     *
     *           |    *
     * min delay-|****
     *           |___________________________________________ width
     *               |                                |
     *           min width                        max width
     * </pre>
     *
     * @param widthDp The width of the hovered item or rail in dp.
     * @param minWidthDp The minimum width bound in dp.
     * @param maxWidthDp The maximum width bound in dp.
     * @return Calculated delay in milliseconds.
     */
    public static int getHoverCardDelay(float widthDp, float minWidthDp, float maxWidthDp) {
        widthDp = MathUtils.clamp(widthDp, minWidthDp, maxWidthDp);
        double logarithmicFraction =
                Math.log(widthDp - minWidthDp + 1.f) / Math.log(maxWidthDp - minWidthDp + 1.f);
        int scalingFactor = MAX_HOVER_CARD_DELAY_MS - MIN_HOVER_CARD_DELAY_MS;
        return (int) (logarithmicFraction * scalingFactor) + MIN_HOVER_CARD_DELAY_MS;
    }

    /**
     * Get the width of the hover card in px, bounded by {@link #HOVER_CARD_MAX_WIDTH_PERCENT} of
     * the window width.
     *
     * @param context The context used to load resources.
     * @return The bounded hover card width in px.
     */
    public static @Px int getHoverCardWidthPx(Context context) {
        float maxWidth = context.getResources().getDimension(R.dimen.tab_hover_card_width);
        float windowWidthPx = context.getResources().getDisplayMetrics().widthPixels;
        return Math.round(Math.min(maxWidth, HOVER_CARD_MAX_WIDTH_PERCENT * windowWidthPx));
    }

    private ViewGroup mContentView;
    private TextView mTitleView;
    private TextView mUrlView;
    private TextViewWithCompoundDrawables mAlertStatusView;
    private TextView mMemoryUsageView;
    private TabThumbnailView mThumbnailView;
    private @Nullable TabModelSelector mTabModelSelector;
    private @Nullable Callback<TabModel> mCurrentTabModelObserver;
    private @Nullable TabContentManager mTabContentManager;
    private @Nullable Tab mHoveredTab;
    private @Nullable TabObserver mHoveredTabObserver;

    private int mLastHoveredTabId = INVALID_TAB_ID;
    private boolean mIsShowing;

    public TabHoverCardView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mContentView = findViewById(R.id.content_view);
        mTitleView = mContentView.findViewById(R.id.title);
        mUrlView = mContentView.findViewById(R.id.url);
        mAlertStatusView = mContentView.findViewById(R.id.alert_status);
        mMemoryUsageView = mContentView.findViewById(R.id.memory_usage);
        mThumbnailView = mContentView.findViewById(R.id.thumbnail);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int exactWidthSpec =
                MeasureSpec.makeMeasureSpec(getHoverCardWidthPx(getContext()), MeasureSpec.EXACTLY);
        super.onMeasure(exactWidthSpec, heightMeasureSpec);
    }

    /**
     * Show the tab hover card at explicit coordinates.
     *
     * @param hoveredTab The {@link Tab} instance of the hovered tab.
     * @param x The x-coordinate in px.
     * @param y The y-coordinate in px.
     */
    public void show(@Nullable Tab hoveredTab, float x, float y) {
        if (hoveredTab == null) return;
        if (mHoveredTab != hoveredTab) {
            unsubscribeFromTab();
            mHoveredTab = hoveredTab;
            mHoveredTab.addObserver(getTabObserver());
        }
        mLastHoveredTabId = hoveredTab.getId();
        mIsShowing = true;

        mTitleView.setText(hoveredTab.getTitle());
        updateUrlView(hoveredTab);
        updateAlertStatusView(hoveredTab.getAlertState());

        mMemoryUsageView.setVisibility(GONE);
        updateAlertStatusBottomMargin();
        hoveredTab.getMemoryUsageBytes(
                bytes -> {
                    if (hoveredTab.getId() != mLastHoveredTabId || !mIsShowing) return;
                    if (bytes > 0) {
                        String memoryText = Formatter.formatShortFileSize(getContext(), bytes);
                        mMemoryUsageView.setText(
                                getContext()
                                        .getString(
                                                R.string.tab_hover_card_memory_usage, memoryText));
                        mMemoryUsageView.setVisibility(VISIBLE);
                        updateAlertStatusBottomMargin();
                    }
                });

        setX(x);
        setY(y);

        int width = getHoverCardWidthPx(getContext());
        updateThumbnail(hoveredTab, width);

        setVisibility(VISIBLE);
    }

    /** Hide the tab hover card. */
    public void hide() {
        unsubscribeFromTab();
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
            Supplier<@Nullable TabContentManager> tabContentManagerSupplier) {
        mTabModelSelector = tabModelSelector;
        mTabContentManager = tabContentManagerSupplier.get();
        mCurrentTabModelObserver =
                (TabModel tabModel) -> updateHoverCardColors(tabModel.isIncognitoBranded());
        mTabModelSelector
                .getCurrentTabModelSupplier()
                .addSyncObserverAndPostIfNonNull(mCurrentTabModelObserver);
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
                TabUiThemeProvider.getTabHoverCardTextColorPrimary(getContext(), incognito));
        mUrlView.setTextColor(
                TabUiThemeProvider.getTabHoverCardTextColorSecondary(getContext(), incognito));
        mAlertStatusView.setTextColor(
                TabUiThemeProvider.getTabHoverCardTextColorSecondary(getContext(), incognito));
        mMemoryUsageView.setTextColor(
                TabUiThemeProvider.getTabHoverCardTextColorSecondary(getContext(), incognito));

        ViewCompat.setBackgroundTintList(
                mContentView,
                TabUiThemeProvider.getTabHoverCardBackgroundTintList(getContext(), incognito));
    }

    @Override
    public @Nullable ColorStateList getBackgroundTintList() {
        return mContentView != null
                ? ViewCompat.getBackgroundTintList(mContentView)
                : super.getBackgroundTintList();
    }

    public void destroy() {
        unsubscribeFromTab();
        if (mTabModelSelector != null) {
            assumeNonNull(mCurrentTabModelObserver);
            mTabModelSelector.getCurrentTabModelSupplier().removeObserver(mCurrentTabModelObserver);
            mTabModelSelector = null;
        }
    }

    private void unsubscribeFromTab() {
        if (mHoveredTab != null && mHoveredTabObserver != null) {
            mHoveredTab.removeObserver(mHoveredTabObserver);
            mHoveredTab = null;
        }
    }

    private TabObserver getTabObserver() {
        if (mHoveredTabObserver == null) {
            mHoveredTabObserver =
                    new EmptyTabObserver() {
                        @Override
                        public void onAlertStateChanged(
                                Tab tab, @Nullable @TabAlert Integer alertState) {
                            if (tab.getId() == mLastHoveredTabId && mIsShowing) {
                                updateAlertStatusView(alertState);
                            }
                        }

                        @Override
                        public void onTitleUpdated(Tab tab) {
                            if (tab.getId() == mLastHoveredTabId && mIsShowing) {
                                mTitleView.setText(tab.getTitle());
                            }
                        }

                        @Override
                        public void onUrlUpdated(Tab tab) {
                            if (tab.getId() == mLastHoveredTabId && mIsShowing) {
                                updateUrlView(tab);
                            }
                        }
                    };
        }
        return mHoveredTabObserver;
    }

    private void updateUrlView(Tab hoveredTab) {
        String url = hoveredTab.getUrl().getHost();
        // If the URL is a Chrome scheme, display the GURL spec instead of the host. For e.g., use
        // chrome://newtab instead of just newtab on the hover card.
        if (UrlUtilities.isInternalScheme(hoveredTab.getUrl())) {
            url = UrlUtilities.stripTrailingSlash(hoveredTab.getUrl().getSpec());
        }
        mUrlView.setText(url);
    }

    private void updateAlertStatusView(@Nullable @TabAlert Integer alertState) {
        @DrawableRes int iconRes = TabUtils.getTabAlertDrawable(alertState);
        @StringRes int stringRes = TabUtils.getTabAlertDescriptionRes(alertState);

        boolean showAlert = iconRes != Resources.ID_NULL && stringRes != Resources.ID_NULL;
        if (showAlert) {
            @ColorInt int defaultTint = SemanticColorUtils.getDefaultIconColorAccent1(getContext());
            @ColorInt
            int tint = TabUtils.getTabAlertTintColor(getContext(), alertState, defaultTint);
            mAlertStatusView.setCompoundDrawablesRelativeWithIntrinsicBounds(iconRes, 0, 0, 0);
            mAlertStatusView.setDrawableTintColor(ColorStateList.valueOf(tint));
            mAlertStatusView.setText(stringRes);
        }

        mAlertStatusView.setVisibility(showAlert ? VISIBLE : GONE);
        updateAlertStatusBottomMargin();
    }

    private void updateAlertStatusBottomMargin() {
        int bottomMarginDimen =
                (mAlertStatusView.getVisibility() == VISIBLE
                                && mMemoryUsageView.getVisibility() == VISIBLE)
                        ? R.dimen.tab_hover_card_footer_row_spacing
                        : R.dimen.tab_hover_card_text_content_margin;
        int bottomMarginPx = getContext().getResources().getDimensionPixelSize(bottomMarginDimen);
        MarginLayoutParams layoutParams = (MarginLayoutParams) mAlertStatusView.getLayoutParams();
        if (layoutParams != null && layoutParams.bottomMargin != bottomMarginPx) {
            layoutParams.bottomMargin = bottomMarginPx;
            mAlertStatusView.setLayoutParams(layoutParams);
        }
    }

    private void updateThumbnail(Tab hoveredTab, float hoverCardWidthPx) {
        float hoverCardDefaultWidthPx =
                getContext().getResources().getDimension(R.dimen.tab_hover_card_width);
        float hoverCardThumbnailDefaultHeightPx =
                getContext().getResources().getDimension(R.dimen.tab_hover_card_thumbnail_height);

        // Update the thumbnail height to maintain the aspect ratio.
        var thumbnailLayoutParams = mThumbnailView.getLayoutParams();
        float thumbnailHeightPx = hoverCardThumbnailDefaultHeightPx;
        if (hoverCardDefaultWidthPx > 0) {
            thumbnailHeightPx =
                    hoverCardWidthPx / hoverCardDefaultWidthPx * hoverCardThumbnailDefaultHeightPx;
        }
        if (Math.round(thumbnailHeightPx) != thumbnailLayoutParams.height) {
            thumbnailLayoutParams.height = Math.round(thumbnailHeightPx);
            mThumbnailView.setLayoutParams(thumbnailLayoutParams);
        }

        var thumbnailSize = new Size(Math.round(hoverCardWidthPx), Math.round(thumbnailHeightPx));
        assumeNonNull(mTabContentManager);
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
                                hoveredTab.isIncognito(),
                                /* isSelected= */ false,
                                /* colorId= */ null);
                    }
                    mThumbnailView.setVisibility(VISIBLE);
                });
    }

    int getLastHoveredTabIdForTesting() {
        return mLastHoveredTabId;
    }

    boolean isShowingForTesting() {
        return mIsShowing;
    }
}
