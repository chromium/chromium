// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.content.Context;
import android.content.res.Resources;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.LinearLayout;

import androidx.appcompat.widget.TooltipCompat;
import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

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
    private static final float SCROLL_OFFSET_DIVISOR = 4f;

    private @Nullable Callback<Integer> mExpandOrCollapseOnHoverListener;

    private TabListRecyclerView mRecyclerView;
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
        TooltipCompat.setTooltipText(mCollapseButton, mCollapseButton.getContentDescription());

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
    }

    /** Returns the main tab list recycler view. */
    public TabListRecyclerView getRecyclerView() {
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

    /** Initializes and configures the main tab list recycler view. */
    public void initRecyclerView(RecyclerView.Adapter<?> adapter) {
        LinearLayoutManager layoutManager =
                new LinearLayoutManager(getContext(), LinearLayoutManager.VERTICAL, false);
        mRecyclerView.setLayoutManager(layoutManager);
        mRecyclerView.setAdapter(adapter);
        mRecyclerView.setupCustomItemAnimator(/* useClipAnimations= */ true);
        mRecyclerView.setVisibility(View.VISIBLE);
    }

    /**
     * Scrolls the main recycler view to the specified position with an offset if it is not
     * completely visible.
     */
    public void scrollToPositionWithOffset(int position) {
        RecyclerView.LayoutManager layoutManager = mRecyclerView.getLayoutManager();
        if (layoutManager instanceof LinearLayoutManager linearLayoutManager) {
            mRecyclerView.post(
                    () -> {
                        int first = linearLayoutManager.findFirstCompletelyVisibleItemPosition();
                        int last = linearLayoutManager.findLastCompletelyVisibleItemPosition();
                        if (position < first || position > last) {
                            int offset =
                                    Math.round(mRecyclerView.getHeight() / SCROLL_OFFSET_DIVISOR);
                            linearLayoutManager.scrollToPositionWithOffset(
                                    position, Math.max(0, offset));
                        }
                    });
        }
    }

    /** Sets the hover listener to be notified when hover state transitions occur. */
    public void setExpandOrCollapseOnHoverListener(@Nullable Callback<Integer> listener) {
        mExpandOrCollapseOnHoverListener = listener;
    }

    /** Updates internal child view styling based on the current rail collapse state. */
    public void setCollapseState(@RailCollapseState int collapseState) {
        updateCollapsedState(collapseState);
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

        mHeaderContainer.setOrientation(
                isCollapsed ? LinearLayout.VERTICAL : LinearLayout.HORIZONTAL);
        mHeaderContainer.setGravity(isCollapsed ? Gravity.CENTER_HORIZONTAL : Gravity.NO_GRAVITY);

        var collapseParams = (ViewGroup.MarginLayoutParams) mCollapseButton.getLayoutParams();
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
        gridParams.setMarginEnd(isCollapsed ? 0 : gap);
        gridParams.bottomMargin = isCollapsed ? gap : 0;
        mGridButton.setLayoutParams(gridParams);
        mGridButton.setBackgroundResource(
                isCollapsed
                        ? R.drawable.vertical_tabs_top_rounded_button_background
                        : R.drawable.vertical_tabs_left_rounded_button_background);

        mSearchButton.setBackgroundResource(
                isCollapsed
                        ? R.drawable.vertical_tabs_bottom_rounded_button_background
                        : R.drawable.vertical_tabs_right_rounded_button_background);

        mHeaderSpacer.setVisibility(isCollapsed ? View.GONE : View.VISIBLE);

        var newTabParams = mNewTabButton.getLayoutParams();
        newTabParams.width =
                isCollapsed
                        ? res.getDimensionPixelSize(R.dimen.vertical_tabs_header_button_size)
                        : ViewGroup.LayoutParams.MATCH_PARENT;
        newTabParams.height =
                isCollapsed
                        ? res.getDimensionPixelSize(R.dimen.vertical_tabs_header_button_size)
                        : res.getDimensionPixelSize(R.dimen.vertical_tabs_new_tab_button_height);
        mNewTabButton.setLayoutParams(newTabParams);
    }
}
