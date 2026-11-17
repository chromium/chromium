// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.ColorStateList;
import android.util.AttributeSet;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.Px;
import androidx.core.view.ViewCompat;

import org.chromium.base.Callback;
import org.chromium.base.MathUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/** Base view class for tab and tab group hover cards. */
@NullMarked
public abstract class HoverCardView extends FrameLayout {
    // The max width of the tab hover card in terms of the enclosing window width percent.
    private static final float HOVER_CARD_MAX_WIDTH_PERCENT = 0.9f;
    public static final int MAX_HOVER_CARD_DELAY_MS = 800;
    public static final int MIN_HOVER_CARD_DELAY_MS = 300;

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
     * Get the width of the hover card in px, bounded by {@code HOVER_CARD_MAX_WIDTH_PERCENT} of the
     * window width.
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
    private @Nullable TabModelSelector mTabModelSelector;
    private boolean mIsShowing;

    private @Nullable Callback<TabModel> mCurrentTabModelObserver;

    public HoverCardView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mContentView = findViewById(R.id.content_view);
    }

    /**
     * Perform tasks after the view is inflated: update the hover card colors, and add a {@link
     * Callback} to tab model supplier to update the view when a tab model is selected.
     *
     * @param tabModelSelector The {@link TabModelSelector} to observe.
     */
    protected void initialize(TabModelSelector tabModelSelector) {
        if (mTabModelSelector != null && mCurrentTabModelObserver != null) {
            mTabModelSelector.getCurrentTabModelSupplier().removeObserver(mCurrentTabModelObserver);
        }
        mTabModelSelector = tabModelSelector;
        mCurrentTabModelObserver =
                (TabModel tabModel) -> updateHoverCardColors(tabModel.isIncognitoBranded());
        mTabModelSelector
                .getCurrentTabModelSupplier()
                .addSyncObserverAndPostIfNonNull(mCurrentTabModelObserver);
        updateHoverCardColors(mTabModelSelector.isIncognitoSelected());
    }

    protected ViewGroup getContentView() {
        return mContentView;
    }

    protected boolean isShowing() {
        return mIsShowing;
    }

    /**
     * Update the hover card background tint based on the theme and incognito mode.
     *
     * @param incognito Whether the incognito mode is selected, {@code true} for incognito, {@code
     *     false} otherwise.
     */
    protected void updateHoverCardColors(boolean incognito) {
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

    /**
     * Show the hover card at explicit coordinates.
     *
     * @param x The x-coordinate in px.
     * @param y The y-coordinate in px.
     */
    public void show(float x, float y) {
        mIsShowing = true;
        setX(x);
        setY(y);
        setVisibility(VISIBLE);
    }

    /** Hide the hover card. */
    public void hide() {
        mIsShowing = false;
        setVisibility(GONE);
    }

    /** Destroy references and hide the hover card. */
    public void destroy() {
        hide();
        if (mTabModelSelector != null) {
            assumeNonNull(mCurrentTabModelObserver);
            mTabModelSelector.getCurrentTabModelSupplier().removeObserver(mCurrentTabModelObserver);
            mTabModelSelector = null;
        }
    }
}
