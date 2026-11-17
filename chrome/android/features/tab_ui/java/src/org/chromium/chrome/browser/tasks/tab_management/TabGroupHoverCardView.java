// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.Px;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.util.List;

/**
 * Hover card view for tab group headers.
 *
 * <p>Renders the group title, up to 5 child tab titles, and an excess count footer using
 * pre-defined child views for zero-allocation performance.
 */
@NullMarked
public class TabGroupHoverCardView extends HoverCardView {
    /** Maximum number of child tab preview rows displayed in the hover card. */
    public static final int MAX_PREVIEW_TABS = 5;

    private final TextView[] mChildTabViews = new TextView[MAX_PREVIEW_TABS];

    private TextView mGroupTitleView;
    private TextView mGroupExcessTabsView;
    private @Nullable TabGroupHoverCardPresenter mPresenter;

    public TabGroupHoverCardView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        ViewGroup contentView = getContentView();
        mGroupTitleView = contentView.findViewById(R.id.group_title);
        mGroupExcessTabsView = contentView.findViewById(R.id.group_excess_tabs);

        mChildTabViews[0] = contentView.findViewById(R.id.group_child_tab_1);
        mChildTabViews[1] = contentView.findViewById(R.id.group_child_tab_2);
        mChildTabViews[2] = contentView.findViewById(R.id.group_child_tab_3);
        mChildTabViews[3] = contentView.findViewById(R.id.group_child_tab_4);
        mChildTabViews[4] = contentView.findViewById(R.id.group_child_tab_5);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        @Px int maxWidth = getHoverCardWidthPx(getContext());
        if (MeasureSpec.getMode(widthMeasureSpec) != MeasureSpec.UNSPECIFIED) {
            maxWidth = Math.min(maxWidth, MeasureSpec.getSize(widthMeasureSpec));
        }

        int atMostWidthSpec = MeasureSpec.makeMeasureSpec(maxWidth, MeasureSpec.AT_MOST);
        super.onMeasure(atMostWidthSpec, heightMeasureSpec);
    }

    @Override
    public void initialize(TabModelSelector tabModelSelector) {
        super.initialize(tabModelSelector);
        mPresenter = new TabGroupHoverCardPresenter(tabModelSelector);
    }

    /**
     * Resolves group metadata via {@link TabGroupHoverCardPresenter} and binds the data to the
     * hover card view.
     *
     * @param tabGroupId The stable tab group ID (Token).
     * @return True if data was successfully bound, false if group is invalid.
     */
    public boolean bindData(@Nullable Token tabGroupId) {
        if (mPresenter == null) return false;
        return mPresenter.bindData(this, tabGroupId);
    }

    /**
     * Binds resolved tab group data to child views.
     *
     * @param title The group display title.
     * @param childTabTitles The list of formatted child tab titles (up to {@value
     *     #MAX_PREVIEW_TABS}).
     * @param excessCount The number of excess tabs beyond {@value #MAX_PREVIEW_TABS} previews.
     * @param isIncognito True if displaying incognito colors.
     */
    public void bindData(
            String title, List<String> childTabTitles, int excessCount, boolean isIncognito) {
        updateHoverCardColors(isIncognito);

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

    @Override
    public void destroy() {
        mPresenter = null;
        super.destroy();
    }

    @Override
    protected void updateHoverCardColors(boolean isIncognito) {
        super.updateHoverCardColors(isIncognito);
        Context context = getContext();
        mGroupTitleView.setTextColor(
                TabUiThemeProvider.getTabHoverCardTextColorPrimary(context, isIncognito));
        int textColorSecondary =
                TabUiThemeProvider.getTabHoverCardTextColorSecondary(context, isIncognito);
        for (TextView childView : mChildTabViews) {
            childView.setTextColor(textColorSecondary);
        }
        mGroupExcessTabsView.setTextColor(textColorSecondary);
    }

    // --- Testing Getters ---

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
