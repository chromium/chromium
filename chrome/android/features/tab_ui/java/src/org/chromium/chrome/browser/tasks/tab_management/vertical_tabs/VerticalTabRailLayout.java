// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.content.Context;
import android.content.res.Resources;
import android.util.AttributeSet;
import android.view.DragEvent;
import android.view.Gravity;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.LinearLayout;

import androidx.annotation.Px;
import androidx.appcompat.widget.TooltipCompat;
import androidx.constraintlayout.widget.ConstraintLayout;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tasks.tab_management.TabListRecyclerView;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabListProperties.RailCollapseState;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.tab_ui.R;

/**
 * Root layout for the vertical tab rail container. Encapsulates child view layout styling based on
 * collapse state and intercepts mouse motion events to detect hover state transitions.
 */
// TODO(crbug.com/527641177): Migrate remaining view-only logic (e.g. empty space touch and context
// click handlers) from VerticalTabListCoordinator to VerticalTabRailLayout.
@NullMarked
public class VerticalTabRailLayout extends ConstraintLayout {
    private static final int HEADER_BUTTON_COUNT_SINGLE_ROW = 3;
    private @Nullable Callback<@RailCollapseState Integer> mExpandOrCollapseOnHoverListener;

    private VerticalTabListRecyclerView mRecyclerView;
    private TabListRecyclerView mPinnedTabsRecyclerView;
    private View mSpacerView;
    private LinearLayout mHeaderContainer;
    private LinearLayout mTabActionButtonsContainer;
    private ImageButton mCollapseButton;
    private View mGridButton;
    private View mSearchButton;
    private View mHeaderSpacer;
    private View mNewTabButton;
    private @Px int mMinSingleButtonRowWidthPx;
    private @Px int mHeaderButtonSizePx;
    private @Px int mHeaderButtonGapPx;
    private @RailCollapseState int mCollapseState = RailCollapseState.EXPANDED;
    private @RailCollapseState int mLastAppliedCollapseState = RailCollapseState.UNKNOWN;
    private boolean mLastAppliedShowSingleRowHeader;

    public VerticalTabRailLayout(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mRecyclerView = findViewById(R.id.tab_list_recycler_view);
        assert mRecyclerView != null;

        mPinnedTabsRecyclerView = findViewById(R.id.pinned_tabs_recycler_view);
        assert mPinnedTabsRecyclerView != null;

        mSpacerView = findViewById(R.id.desktop_window_spacer);
        assert mSpacerView != null;

        mHeaderContainer = findViewById(R.id.vertical_tab_header_container);
        assert mHeaderContainer != null;

        mTabActionButtonsContainer = findViewById(R.id.tab_action_buttons_container);
        assert mTabActionButtonsContainer != null;

        mCollapseButton = findViewById(R.id.collapse_button);
        assert mCollapseButton != null;

        mGridButton = findViewById(R.id.grid_button);
        assert mGridButton != null;
        TooltipCompat.setTooltipText(
                mGridButton, getContext().getString(R.string.accessibility_tab_groups));

        mSearchButton = findViewById(R.id.tab_search_button);
        assert mSearchButton != null;
        TooltipCompat.setTooltipText(
                mSearchButton,
                getContext().getString(R.string.accessibility_search_loupe_tooltip_text));

        mHeaderSpacer = findViewById(R.id.header_spacer);
        assert mHeaderSpacer != null;

        mNewTabButton = findViewById(R.id.new_tab_button);
        assert mNewTabButton != null;
        TooltipCompat.setTooltipText(
                mNewTabButton, getContext().getString(R.string.accessibility_toolbar_btn_new_tab));

        updateButtonSizes();
    }

    private void updateButtonSizes() {
        VerticalTabListViewBinder.updateButtonSizes(this);

        Resources res = getContext().getResources();
        mHeaderButtonSizePx = res.getDimensionPixelSize(R.dimen.vertical_tabs_header_button_size);
        mHeaderButtonGapPx = res.getDimensionPixelSize(R.dimen.vertical_tabs_header_button_gap);
        mMinSingleButtonRowWidthPx =
                mHeaderButtonSizePx * HEADER_BUTTON_COUNT_SINGLE_ROW
                        + mHeaderButtonGapPx * (HEADER_BUTTON_COUNT_SINGLE_ROW - 1)
                        + getPaddingStart()
                        + getPaddingEnd();
    }

    /** Returns the main tab list recycler view. */
    public VerticalTabListRecyclerView getRecyclerView() {
        return mRecyclerView;
    }

    /** Returns the pinned tabs recycler view. */
    public TabListRecyclerView getPinnedTabsRecyclerView() {
        return mPinnedTabsRecyclerView;
    }

    /** Returns the header container view. */
    public LinearLayout getHeaderContainer() {
        return mHeaderContainer;
    }

    /** Returns the tab action buttons container view. */
    public LinearLayout getTabActionButtonsContainer() {
        return mTabActionButtonsContainer;
    }

    /** Sets the visibility of the desktop window top spacer. */
    public void setDesktopWindowSpacerVisible(boolean visible) {
        mSpacerView.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    /** Sets the hover listener to be notified when hover state transitions occur. */
    public void setExpandOrCollapseOnHoverListener(
            @Nullable Callback<@RailCollapseState Integer> listener) {
        mExpandOrCollapseOnHoverListener = listener;
    }

    /** Updates internal child view styling based on the current rail collapse state. */
    public void setCollapseState(@RailCollapseState int collapseState) {
        if (mCollapseState == collapseState) return;
        mCollapseState = collapseState;
        updateHeaderLayoutForWidth(getWidth());
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        if (w != oldw) {
            updateHeaderLayoutForWidth(w);
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        super.onWindowFocusChanged(hasWindowFocus);
        if (!hasWindowFocus && mExpandOrCollapseOnHoverListener != null) {
            // If the current state is EXPANDED, this will be ignored safely in
            // VerticalTabRailCollapseController.
            mExpandOrCollapseOnHoverListener.onResult(RailCollapseState.COLLAPSED);
        }
    }

    @Override
    public boolean onDragEvent(DragEvent event) {
        int action = event.getAction();
        if ((action == DragEvent.ACTION_DRAG_EXITED || action == DragEvent.ACTION_DRAG_ENDED)
                && mExpandOrCollapseOnHoverListener != null) {
            // If the current state is EXPANDED, this will be ignored safely in
            // VerticalTabRailCollapseController.
            mExpandOrCollapseOnHoverListener.onResult(RailCollapseState.COLLAPSED);
        }
        return super.onDragEvent(event);
    }

    @Override
    public boolean dispatchGenericMotionEvent(MotionEvent event) {
        expandOrCollapseOnHover(event);
        if (super.dispatchGenericMotionEvent(event)) return true;
        // Prevent mouse button presses/releases from falling back to the window's
        // focused view (ContentView), which would send out-of-bounds mouse events to Blink and
        // blur the active webpage document.
        if (event != null && event.isFromSource(InputDevice.SOURCE_MOUSE)) {
            int action = event.getActionMasked();
            if (action == MotionEvent.ACTION_BUTTON_PRESS
                    || action == MotionEvent.ACTION_BUTTON_RELEASE) {
                return true;
            }
        }
        return false;
    }

    private void expandOrCollapseOnHover(@Nullable MotionEvent event) {
        if (mExpandOrCollapseOnHoverListener == null) return;
        if (!VerticalTabUtils.isExpandOnHoverEnabled()) return;
        if (event == null || !event.isFromSource(InputDevice.SOURCE_MOUSE)) return;
        if (!mCollapseButton.isEnabled()) return;

        int action = event.getActionMasked();

        int[] location = new int[2];
        getLocationOnScreen(location);
        int left = location[0];
        int top = location[1];
        float rawX = event.getRawX();
        float rawY = event.getRawY();

        boolean isInside =
                rawX >= left && rawX < left + getWidth() && rawY >= top && rawY < top + getHeight();

        if (isInside && action == MotionEvent.ACTION_HOVER_ENTER) {
            mExpandOrCollapseOnHoverListener.onResult(RailCollapseState.EXPANDED_FOR_HOVERING);
        } else if (!isInside && action == MotionEvent.ACTION_HOVER_EXIT) {
            mExpandOrCollapseOnHoverListener.onResult(RailCollapseState.COLLAPSED);
        }
    }

    /**
     * Updates header child view styling and layout parameters for the given rail width.
     *
     * @param width The measured width of the rail layout, or 0 if unmeasured.
     */
    private void updateHeaderLayoutForWidth(int width) {
        boolean isCollapsed = mCollapseState == RailCollapseState.COLLAPSED;
        // Default to single-row header when unmeasured (width <= 0) to match the initial XML state.
        boolean canFitHeaderButtonsInSingleRow = width <= 0 || width >= mMinSingleButtonRowWidthPx;
        boolean showSingleRowHeader = !isCollapsed && canFitHeaderButtonsInSingleRow;
        if (mCollapseState == mLastAppliedCollapseState
                && mLastAppliedShowSingleRowHeader == showSingleRowHeader) {
            return;
        }
        mLastAppliedCollapseState = mCollapseState;
        mLastAppliedShowSingleRowHeader = showSingleRowHeader;

        Resources res = getResources();

        // The whole header button container
        mHeaderContainer.setOrientation(
                showSingleRowHeader ? LinearLayout.HORIZONTAL : LinearLayout.VERTICAL);
        mHeaderContainer.setGravity(isCollapsed ? Gravity.CENTER_HORIZONTAL : Gravity.NO_GRAVITY);

        // Collapse button
        boolean isManuallyExpanded = mCollapseState == RailCollapseState.EXPANDED;
        var collapseParams = (ViewGroup.MarginLayoutParams) mCollapseButton.getLayoutParams();
        collapseParams.width = mHeaderButtonSizePx;
        collapseParams.height = mHeaderButtonSizePx;
        collapseParams.bottomMargin =
                showSingleRowHeader
                        ? 0
                        : res.getDimensionPixelOffset(
                                R.dimen.vertical_tabs_header_padding_vertical);
        mCollapseButton.setImageResource(
                isManuallyExpanded
                        ? R.drawable.vertical_tabs_menu_collapse
                        : R.drawable.vertical_tabs_menu_expand);
        int resId =
                isManuallyExpanded
                        ? R.string.accessibility_collapse_vertical_tabs
                        : R.string.accessibility_expand_vertical_tabs;
        String tooltipText = getContext().getString(resId);
        mCollapseButton.setContentDescription(tooltipText);
        TooltipCompat.setTooltipText(mCollapseButton, tooltipText);

        // Horizontal header spacer
        mHeaderSpacer.setVisibility(showSingleRowHeader ? View.VISIBLE : View.GONE);

        // Tab actions container (for grid and search buttons)
        var tabActionParams = mTabActionButtonsContainer.getLayoutParams();
        tabActionParams.width =
                (!isCollapsed && !showSingleRowHeader)
                        ? ViewGroup.LayoutParams.MATCH_PARENT
                        : ViewGroup.LayoutParams.WRAP_CONTENT;

        mTabActionButtonsContainer.setOrientation(
                isCollapsed ? LinearLayout.VERTICAL : LinearLayout.HORIZONTAL);
        mTabActionButtonsContainer.setGravity(
                isCollapsed ? Gravity.CENTER_HORIZONTAL : Gravity.NO_GRAVITY);

        // Grid button
        boolean fillRowSpace = !isCollapsed && !showSingleRowHeader;

        var gridParams = (LinearLayout.LayoutParams) mGridButton.getLayoutParams();
        gridParams.width = fillRowSpace ? 0 : mHeaderButtonSizePx;
        gridParams.height = mHeaderButtonSizePx;
        gridParams.weight = fillRowSpace ? 1.0f : 0.0f;
        gridParams.setMarginEnd(isCollapsed ? 0 : mHeaderButtonGapPx);
        gridParams.bottomMargin = isCollapsed ? mHeaderButtonGapPx : 0;
        mGridButton.setBackgroundResource(
                isCollapsed
                        ? R.drawable.vertical_tabs_top_rounded_button_background
                        : R.drawable.vertical_tabs_left_rounded_button_background);

        // Search button
        var searchParams = (LinearLayout.LayoutParams) mSearchButton.getLayoutParams();
        searchParams.width = fillRowSpace ? 0 : mHeaderButtonSizePx;
        searchParams.height = mHeaderButtonSizePx;
        searchParams.weight = fillRowSpace ? 1.0f : 0.0f;
        mSearchButton.setBackgroundResource(
                isCollapsed
                        ? R.drawable.vertical_tabs_bottom_rounded_button_background
                        : R.drawable.vertical_tabs_right_rounded_button_background);

        // New tab button
        var newTabParams = mNewTabButton.getLayoutParams();
        int newTabHeight = res.getDimensionPixelSize(R.dimen.vertical_tabs_new_tab_button_height);
        newTabParams.width =
                isCollapsed ? mHeaderButtonSizePx : ViewGroup.LayoutParams.MATCH_PARENT;
        newTabParams.height = isCollapsed ? mHeaderButtonSizePx : newTabHeight;

        mCollapseButton.setLayoutParams(collapseParams);
        mTabActionButtonsContainer.setLayoutParams(tabActionParams);
        mGridButton.setLayoutParams(gridParams);
        mSearchButton.setLayoutParams(searchParams);
        mNewTabButton.setLayoutParams(newTabParams);
    }

    @Px
    int getMinSingleButtonRowWidthPxForTesting() {
        return mMinSingleButtonRowWidthPx;
    }
}
