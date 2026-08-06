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
    private @Nullable Callback<@RailCollapseState Integer> mExpandOrCollapseOnHoverListener;

    private VerticalTabListRecyclerView mRecyclerView;
    private TabListRecyclerView mPinnedTabsRecyclerView;
    private View mSpacerView;
    private LinearLayout mHeaderContainer;
    private ImageButton mCollapseButton;
    private View mGridButton;
    private View mSearchButton;
    private View mHeaderSpacer;
    private View mNewTabButton;

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
        updateCollapsedState(collapseState);
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

    private void updateCollapsedState(@RailCollapseState int railCollapseState) {
        boolean isCollapsed = railCollapseState == RailCollapseState.COLLAPSED;
        boolean isManuallyExpanded = railCollapseState == RailCollapseState.EXPANDED;
        Resources res = getResources();
        int buttonSize = res.getDimensionPixelSize(R.dimen.vertical_tabs_header_button_size);
        int newTabHeight = res.getDimensionPixelSize(R.dimen.vertical_tabs_new_tab_button_height);

        mHeaderContainer.setOrientation(
                isCollapsed ? LinearLayout.VERTICAL : LinearLayout.HORIZONTAL);
        mHeaderContainer.setGravity(isCollapsed ? Gravity.CENTER_HORIZONTAL : Gravity.NO_GRAVITY);

        var collapseParams = (ViewGroup.MarginLayoutParams) mCollapseButton.getLayoutParams();
        collapseParams.width = buttonSize;
        collapseParams.height = buttonSize;
        collapseParams.setMarginEnd(
                isCollapsed
                        ? 0
                        : res.getDimensionPixelSize(
                                R.dimen.vertical_tabs_header_button_collapsed_margin_end));
        collapseParams.bottomMargin =
                isCollapsed
                        ? res.getDimensionPixelOffset(R.dimen.vertical_tabs_header_padding_vertical)
                        : 0;
        mCollapseButton.setLayoutParams(collapseParams);
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

        int gap = res.getDimensionPixelSize(R.dimen.vertical_tabs_header_button_gap);
        var gridParams = (ViewGroup.MarginLayoutParams) mGridButton.getLayoutParams();
        gridParams.width = buttonSize;
        gridParams.height = buttonSize;
        gridParams.setMarginEnd(isCollapsed ? 0 : gap);
        gridParams.bottomMargin = isCollapsed ? gap : 0;
        mGridButton.setLayoutParams(gridParams);
        mGridButton.setBackgroundResource(
                isCollapsed
                        ? R.drawable.vertical_tabs_top_rounded_button_background
                        : R.drawable.vertical_tabs_left_rounded_button_background);

        var searchParams = (ViewGroup.MarginLayoutParams) mSearchButton.getLayoutParams();
        searchParams.width = buttonSize;
        searchParams.height = buttonSize;
        mSearchButton.setLayoutParams(searchParams);
        mSearchButton.setBackgroundResource(
                isCollapsed
                        ? R.drawable.vertical_tabs_bottom_rounded_button_background
                        : R.drawable.vertical_tabs_right_rounded_button_background);

        mHeaderSpacer.setVisibility(isCollapsed ? View.GONE : View.VISIBLE);

        var newTabParams = mNewTabButton.getLayoutParams();
        newTabParams.width = isCollapsed ? buttonSize : ViewGroup.LayoutParams.MATCH_PARENT;
        newTabParams.height = isCollapsed ? buttonSize : newTabHeight;
        mNewTabButton.setLayoutParams(newTabParams);
    }
}
