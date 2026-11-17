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
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabThumbnailView;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.tabs.TabAlert;

import java.util.function.Supplier;

@NullMarked
public class TabHoverCardView extends HoverCardView {
    static final int INVALID_TAB_ID = -1;

    private TextView mTitleView;
    private TextView mUrlView;
    private TextViewWithCompoundDrawables mAlertStatusView;
    private TextView mMemoryUsageView;
    private TabThumbnailView mThumbnailView;
    private @Nullable TabContentManager mTabContentManager;
    private @Nullable Tab mHoveredTab;
    private @Nullable TabObserver mHoveredTabObserver;
    private @Nullable Runnable mOnCardHeightChangedCallback;

    private int mLastHoveredTabId = INVALID_TAB_ID;

    public TabHoverCardView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        ViewGroup contentView = getContentView();
        mTitleView = contentView.findViewById(R.id.title);
        mUrlView = contentView.findViewById(R.id.url);
        mAlertStatusView = contentView.findViewById(R.id.alert_status);
        mMemoryUsageView = contentView.findViewById(R.id.memory_usage);
        mThumbnailView = contentView.findViewById(R.id.thumbnail);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int exactWidthSpec =
                MeasureSpec.makeMeasureSpec(getHoverCardWidthPx(getContext()), MeasureSpec.EXACTLY);
        super.onMeasure(exactWidthSpec, heightMeasureSpec);
    }

    /**
     * Set a callback invoked when the card height dynamically changes (e.g. memory usage update).
     */
    public void setOnCardHeightChangedCallback(@Nullable Runnable callback) {
        mOnCardHeightChangedCallback = callback;
    }

    /**
     * Bind tab data and show the tab hover card at explicit coordinates.
     *
     * @param hoveredTab The {@link Tab} instance of the hovered tab.
     * @param x The x-coordinate in px.
     * @param y The y-coordinate in px.
     */
    public void show(@Nullable Tab hoveredTab, float x, float y) {
        if (hoveredTab == null) return;
        bindTab(hoveredTab);
        show(x, y);
    }

    /**
     * Bind the hovered tab data to the view components (title, url, alert status, thumbnail
     * placeholder).
     *
     * @param hoveredTab The {@link Tab} instance of the hovered tab.
     */
    public void bindTab(Tab hoveredTab) {
        if (mHoveredTab != hoveredTab) {
            unsubscribeFromTab();
            mHoveredTab = hoveredTab;
            mHoveredTab.addObserver(getTabObserver());
        }
        mLastHoveredTabId = hoveredTab.getId();

        mTitleView.setText(hoveredTab.getTitle());
        updateUrlView(hoveredTab);
        updateAlertStatusView(hoveredTab.getAlertState());

        mMemoryUsageView.setVisibility(GONE);
        updateAlertStatusBottomMargin();
        hoveredTab.getMemoryUsageBytes(
                bytes -> {
                    if (hoveredTab.getId() != mLastHoveredTabId || !isShowing()) return;
                    if (bytes > 0) {
                        String memoryText = Formatter.formatShortFileSize(getContext(), bytes);
                        mMemoryUsageView.setText(
                                getContext()
                                        .getString(
                                                R.string.tab_hover_card_memory_usage, memoryText));
                        mMemoryUsageView.setVisibility(VISIBLE);
                        updateAlertStatusBottomMargin();
                        if (mOnCardHeightChangedCallback != null) {
                            mOnCardHeightChangedCallback.run();
                        }
                    }
                });

        int width = getHoverCardWidthPx(getContext());
        updateThumbnail(hoveredTab, width);
    }

    /** Hide the tab hover card. */
    @Override
    public void hide() {
        unsubscribeFromTab();
        super.hide();
        mThumbnailView.setImageDrawable(null);
        mThumbnailView.setVisibility(GONE);
        mLastHoveredTabId = INVALID_TAB_ID;
    }

    /**
     * Initializes the tab hover card view with the tab model selector and thumbnail content
     * manager.
     *
     * @param tabModelSelector The {@link TabModelSelector} to observe.
     * @param tabContentManagerSupplier Supplier of the manager providing tab thumbnail snapshots.
     */
    public void initialize(
            TabModelSelector tabModelSelector,
            Supplier<@Nullable TabContentManager> tabContentManagerSupplier) {
        mTabContentManager = tabContentManagerSupplier.get();
        super.initialize(tabModelSelector);
    }

    @Override
    protected void updateHoverCardColors(boolean incognito) {
        super.updateHoverCardColors(incognito);
        mTitleView.setTextColor(
                TabUiThemeProvider.getTabHoverCardTextColorPrimary(getContext(), incognito));
        mUrlView.setTextColor(
                TabUiThemeProvider.getTabHoverCardTextColorSecondary(getContext(), incognito));
        mAlertStatusView.setTextColor(
                TabUiThemeProvider.getTabHoverCardTextColorSecondary(getContext(), incognito));
        mMemoryUsageView.setTextColor(
                TabUiThemeProvider.getTabHoverCardTextColorSecondary(getContext(), incognito));
    }

    @Override
    public void destroy() {
        mOnCardHeightChangedCallback = null;
        super.destroy();
    }

    private void unsubscribeFromTab() {
        if (mHoveredTab != null) {
            if (mHoveredTabObserver != null) {
                mHoveredTab.removeObserver(mHoveredTabObserver);
            }
            mHoveredTab = null;
        }
    }

    private TabObserver getTabObserver() {
        if (mHoveredTabObserver == null) {
            mHoveredTabObserver =
                    new TabObserver() {
                        @Override
                        public void onAlertStateChanged(Tab tab, @TabAlert int alertState) {
                            if (tab.getId() == mLastHoveredTabId && isShowing()) {
                                updateAlertStatusView(alertState);
                            }
                        }

                        @Override
                        public void onTitleUpdated(Tab tab) {
                            if (tab.getId() == mLastHoveredTabId && isShowing()) {
                                mTitleView.setText(tab.getTitle());
                            }
                        }

                        @Override
                        public void onUrlUpdated(Tab tab) {
                            if (tab.getId() == mLastHoveredTabId && isShowing()) {
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

    private void updateAlertStatusView(@TabAlert int alertState) {
        @DrawableRes int iconRes = TabUtils.getTabAlertDrawable(alertState);
        @StringRes int stringRes = TabUtils.getTabAlertDescriptionRes(alertState);

        boolean showAlert = iconRes != Resources.ID_NULL && stringRes != Resources.ID_NULL;
        boolean visibilityChanged = (mAlertStatusView.getVisibility() == VISIBLE) != showAlert;
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
        if (visibilityChanged && isShowing() && mOnCardHeightChangedCallback != null) {
            mOnCardHeightChangedCallback.run();
        }
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

        // Display placeholder and make thumbnail visible synchronously so that initial hover card
        // measurement accounts for the thumbnail's height prior to positioning calculations.
        //  Always use the unselected tab version of the thumbnail placeholder.
        mThumbnailView.updateThumbnailPlaceholder(
                hoveredTab.isIncognito(), /* isSelected= */ false, /* colorId= */ null);
        mThumbnailView.setVisibility(VISIBLE);

        var thumbnailSize = new Size(Math.round(hoverCardWidthPx), Math.round(thumbnailHeightPx));
        assumeNonNull(mTabContentManager);
        mTabContentManager.getTabThumbnailWithCallback(
                hoveredTab.getId(),
                thumbnailSize,
                thumbnail -> {
                    // Thumbnail request was for a previous hover.
                    if (hoveredTab.getId() != mLastHoveredTabId) return;
                    // View is not visible any more.
                    if (!isShowing()) return;
                    if (thumbnail != null) {
                        TabUtils.setDrawableAndUpdateImageMatrix(
                                mThumbnailView, new BitmapDrawable(thumbnail), thumbnailSize);
                    }
                });
    }

    int getLastHoveredTabIdForTesting() {
        return mLastHoveredTabId;
    }
}
