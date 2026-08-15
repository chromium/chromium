// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.GradientDrawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.core.widget.ImageViewCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;

import java.util.List;

/**
 * Desktop-style hover card view for tab group headers in Vertical Tabs.
 *
 * <p>Renders the group title with group color indicator, up to 5 child tab titles, and an excess
 * count footer using pre-defined child views for zero-allocation performance.
 */
@NullMarked
public class TabGroupHoverCardView extends FrameLayout {
    static final int MAX_PREVIEW_TABS = 5;

    private final TextView[] mChildTabViews = new TextView[MAX_PREVIEW_TABS];

    private ImageView mGroupColorIconView;
    private TextView mGroupTitleView;
    private TextView mGroupExcessTabsView;

    public TabGroupHoverCardView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mGroupColorIconView = findViewById(R.id.group_color_icon);
        mGroupTitleView = findViewById(R.id.group_title);
        mGroupExcessTabsView = findViewById(R.id.group_excess_tabs);

        mChildTabViews[0] = findViewById(R.id.group_child_tab_1);
        mChildTabViews[1] = findViewById(R.id.group_child_tab_2);
        mChildTabViews[2] = findViewById(R.id.group_child_tab_3);
        mChildTabViews[3] = findViewById(R.id.group_child_tab_4);
        mChildTabViews[4] = findViewById(R.id.group_child_tab_5);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int maxWidth = TabHoverCardView.getHoverCardWidthPx(getContext());
        if (MeasureSpec.getMode(widthMeasureSpec) != MeasureSpec.UNSPECIFIED) {
            maxWidth = Math.min(maxWidth, MeasureSpec.getSize(widthMeasureSpec));
        }

        int exactWidthSpec = MeasureSpec.makeMeasureSpec(maxWidth, MeasureSpec.EXACTLY);
        super.onMeasure(exactWidthSpec, heightMeasureSpec);
    }

    /**
     * Binds resolved tab group data to child views.
     *
     * @param title The group display title.
     * @param groupColor The group color integer.
     * @param childTabTitles The list of formatted child tab titles (up to 5).
     * @param excessCount The number of excess tabs beyond the 5 previews.
     * @param isIncognito True if displaying incognito colors.
     */
    void bindData(
            String title,
            @ColorInt int groupColor,
            List<String> childTabTitles,
            int excessCount,
            boolean isIncognito) {
        updateColors(isIncognito);

        // Group Title.
        mGroupTitleView.setText(title);

        // Group Color Icon.
        ImageViewCompat.setImageTintList(mGroupColorIconView, ColorStateList.valueOf(groupColor));

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
    private void showAt(float x, float y) {
        setX(x);
        setY(y);
        setVisibility(View.VISIBLE);
    }

    /**
     * Displays the tab group hover card with resolved data.
     *
     * @param title The group display title.
     * @param groupColor The group color integer.
     * @param childTabTitles The list of formatted child tab titles (up to 5).
     * @param excessCount The number of excess tabs beyond the 5 previews.
     * @param isIncognito True if displaying incognito colors.
     * @param x Target X screen coordinate in pixels.
     * @param y Target Y screen coordinate in pixels.
     */
    void show(
            String title,
            @ColorInt int groupColor,
            List<String> childTabTitles,
            int excessCount,
            boolean isIncognito,
            float x,
            float y) {
        bindData(title, groupColor, childTabTitles, excessCount, isIncognito);
        showAt(x, y);
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

        if (getBackground() instanceof GradientDrawable background) {
            GradientDrawable gradientDrawable = (GradientDrawable) background.mutate();
            int bgColor =
                    isIncognito
                            ? context.getColor(R.color.incognito_tab_hover_card_bg_color)
                            : context.getColor(R.color.tab_hover_card_bg_color);
            int strokeColor =
                    isIncognito
                            ? context.getColor(R.color.tab_grid_card_divider_tint_color_incognito)
                            : SemanticColorUtils.getColorSurfaceContainer(context);
            int strokeWidth =
                    context.getResources()
                            .getDimensionPixelSize(R.dimen.tab_group_hover_card_border_width);
            gradientDrawable.setColor(bgColor);
            gradientDrawable.setStroke(strokeWidth, strokeColor);
        }
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
