// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.res.ColorStateList;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.annotation.Px;
import androidx.core.view.ViewCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;

import java.util.List;

/**
 * Desktop-style hover card view for tab group headers in Vertical Tabs.
 *
 * <p>Renders the group title, up to 5 child tab titles, and an excess count footer using
 * pre-defined child views for zero-allocation performance.
 */
@NullMarked
public class TabGroupHoverCardView extends FrameLayout {
    /** Maximum number of child tab preview rows displayed in the hover card. */
    static final int MAX_PREVIEW_TABS = 5;

    private final TextView[] mChildTabViews = new TextView[MAX_PREVIEW_TABS];

    private ViewGroup mContentView;
    private TextView mGroupTitleView;
    private TextView mGroupExcessTabsView;

    public TabGroupHoverCardView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mContentView = findViewById(R.id.content_view);
        mGroupTitleView = mContentView.findViewById(R.id.group_title);
        mGroupExcessTabsView = mContentView.findViewById(R.id.group_excess_tabs);

        mChildTabViews[0] = mContentView.findViewById(R.id.group_child_tab_1);
        mChildTabViews[1] = mContentView.findViewById(R.id.group_child_tab_2);
        mChildTabViews[2] = mContentView.findViewById(R.id.group_child_tab_3);
        mChildTabViews[3] = mContentView.findViewById(R.id.group_child_tab_4);
        mChildTabViews[4] = mContentView.findViewById(R.id.group_child_tab_5);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        @Px int maxWidth = TabHoverCardView.getHoverCardWidthPx(getContext());
        if (MeasureSpec.getMode(widthMeasureSpec) != MeasureSpec.UNSPECIFIED) {
            maxWidth = Math.min(maxWidth, MeasureSpec.getSize(widthMeasureSpec));
        }

        int atMostWidthSpec = MeasureSpec.makeMeasureSpec(maxWidth, MeasureSpec.AT_MOST);
        super.onMeasure(atMostWidthSpec, heightMeasureSpec);
    }

    /**
     * Binds resolved tab group data to child views.
     *
     * @param title The group display title.
     * @param childTabTitles The list of formatted child tab titles (up to 5).
     * @param excessCount The number of excess tabs beyond the 5 previews.
     * @param isIncognito True if displaying incognito colors.
     */
    void bindData(String title, List<String> childTabTitles, int excessCount, boolean isIncognito) {
        updateColors(isIncognito);

        // Group Title.
        mGroupTitleView.setText(title);

        // Child Tab List (using pre-inflated child views).
        for (int i = 0; i < MAX_PREVIEW_TABS; i++) {
            if (i < childTabTitles.size()) {
                mChildTabViews[i].setText(childTabTitles.get(i));
                mChildTabViews[i].setVisibility(View.VISIBLE);
            } else {
                mChildTabViews[i].setVisibility(View.GONE);
            }
        }

        // Excess Tabs Counter Footer.
        if (excessCount > 0) {
            String excessText =
                    getContext()
                            .getResources()
                            .getQuantityString(
                                    R.plurals.tab_group_hover_card_excess_tabs,
                                    excessCount,
                                    excessCount);
            mGroupExcessTabsView.setText(excessText);
            mGroupExcessTabsView.setVisibility(View.VISIBLE);
        } else {
            mGroupExcessTabsView.setVisibility(View.GONE);
        }
    }

    /**
     * Positions and displays the hover card.
     *
     * @param x Target X screen coordinate in pixels.
     * @param y Target Y screen coordinate in pixels.
     */
    void show(float x, float y) {
        setX(x);
        setY(y);
        setVisibility(View.VISIBLE);
    }

    /** Hides the hover card. */
    void hide() {
        setVisibility(View.GONE);
    }

    /** Destroys references and hides the card. */
    void destroy() {
        hide();
    }

    // --- Helper Methods ---

    private void updateColors(boolean isIncognito) {
        Context context = getContext();
        mGroupTitleView.setTextColor(
                TabUiThemeProvider.getTabHoverCardTextColorPrimary(context, isIncognito));
        int textColorSecondary =
                TabUiThemeProvider.getTabHoverCardTextColorSecondary(context, isIncognito);
        for (TextView childView : mChildTabViews) {
            childView.setTextColor(textColorSecondary);
        }
        mGroupExcessTabsView.setTextColor(textColorSecondary);

        ViewCompat.setBackgroundTintList(
                mContentView,
                TabUiThemeProvider.getTabHoverCardBackgroundTintList(context, isIncognito));
    }

    // --- Testing Getters ---

    /** Returns the background tint list of the inner content container for testing. */
    @Nullable
    ColorStateList getBackgroundTintListForTesting() {
        return mContentView != null ? ViewCompat.getBackgroundTintList(mContentView) : null;
    }

    /** Returns the TextView displaying the group title for testing. */
    TextView getGroupTitleViewForTesting() {
        return mGroupTitleView;
    }

    /** Returns the TextView displaying the excess tabs count for testing. */
    TextView getGroupExcessTabsViewForTesting() {
        return mGroupExcessTabsView;
    }

    /** Returns the array of TextViews displaying child tab titles for testing. */
    TextView[] getChildTabViewsForTesting() {
        return mChildTabViews;
    }
}
