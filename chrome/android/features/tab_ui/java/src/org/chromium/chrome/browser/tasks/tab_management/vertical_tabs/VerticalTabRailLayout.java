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
    private @Nullable Callback<@RailCollapseState Integer> mExpandOrCollapseOnHoverListener;

    private VerticalTabListRecyclerView mRecyclerView;
    private TabListRecyclerView mPinnedTabsRecyclerView;
    private View mSpacerView;
    private LinearLayout mHeaderContainer;
    private LinearLayout mFooterContainer;
    private ImageButton mCollapseButton;
    private View mSearchButton;
    private View mHeaderSpacer;
    private View mNewTabButton;
    private ImageButton mIncognitoButton;
    private @Px int mButtonSizePx;
    private @Px int mIncognitoChipSizePx;
    private @Px int mFooterButtonGapPx;
    private @RailCollapseState int mCollapseState = RailCollapseState.EXPANDED;
    // Cache for the last applied collapse state to prevent redundant header layout updates.
    private @RailCollapseState int mLastAppliedCollapseState = RailCollapseState.UNKNOWN;

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

        mFooterContainer = findViewById(R.id.vertical_tab_footer_container);
        assert mFooterContainer != null;

        mCollapseButton = findViewById(R.id.collapse_button);
        assert mCollapseButton != null;

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

        mIncognitoButton = findViewById(R.id.new_incognito_tab_button);
        assert mIncognitoButton != null;
        TooltipCompat.setTooltipText(
                mIncognitoButton,
                getContext()
                        .getString(R.string.accessibility_tabstrip_btn_incognito_toggle_standard));

        // Update header dimensions
        Resources res = getContext().getResources();
        boolean isTablet = VerticalTabUtils.isTablet(getContext());
        mButtonSizePx =
                res.getDimensionPixelSize(
                        isTablet
                                ? R.dimen.vertical_tabs_header_button_size_tablet
                                : R.dimen.vertical_tabs_header_button_size);
        mIncognitoChipSizePx =
                res.getDimensionPixelSize(
                        isTablet
                                ? R.dimen.vertical_tabs_footer_button_height_tablet
                                : R.dimen.vertical_tabs_footer_button_height);
        mFooterButtonGapPx = res.getDimensionPixelSize(R.dimen.vertical_tabs_footer_button_gap);
        updateHeaderLayout();
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

    /** Returns the footer container view. */
    public LinearLayout getFooterContainer() {
        return mFooterContainer;
    }

    /** Returns the incognito tab switcher button view in the footer. */
    public ImageButton getIncognitoButton() {
        return mIncognitoButton;
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
        updateHeaderLayout();
    }

    /** Returns whether the rail is currently in the collapsed state. */
    public boolean isCollapsed() {
        return mCollapseState == RailCollapseState.COLLAPSED;
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

    /** Updates header child view styling and layout parameters based on the rail collapse state. */
    private void updateHeaderLayout() {
        if (mLastAppliedCollapseState == mCollapseState) {
            return;
        }
        mLastAppliedCollapseState = mCollapseState;

        boolean isCollapsed = mCollapseState == RailCollapseState.COLLAPSED;
        boolean showSingleRowHeader = !isCollapsed;

        Resources res = getResources();

        // The whole header button container
        mHeaderContainer.setOrientation(
                showSingleRowHeader ? LinearLayout.HORIZONTAL : LinearLayout.VERTICAL);
        mHeaderContainer.setGravity(isCollapsed ? Gravity.CENTER_HORIZONTAL : Gravity.NO_GRAVITY);

        // Collapse button
        boolean isManuallyExpanded = mCollapseState == RailCollapseState.EXPANDED;
        ViewGroup.MarginLayoutParams collapseParams =
                (ViewGroup.MarginLayoutParams) mCollapseButton.getLayoutParams();
        collapseParams.width = mButtonSizePx;
        collapseParams.height = mButtonSizePx;
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

        // Search button
        LinearLayout.LayoutParams searchParams =
                (LinearLayout.LayoutParams) mSearchButton.getLayoutParams();
        searchParams.width = mButtonSizePx;
        searchParams.height = mButtonSizePx;

        mCollapseButton.setLayoutParams(collapseParams);
        mSearchButton.setLayoutParams(searchParams);
        updateFooterLayout();
    }

    /**
     * Updates footer container and child view layout parameters based on the current rail collapse
     * state and incognito button visibility.
     */
    void updateFooterLayout() {
        boolean isCollapsed = mCollapseState == RailCollapseState.COLLAPSED;
        boolean isIncognitoVisible = mIncognitoButton.getVisibility() == View.VISIBLE;

        mFooterContainer.setOrientation(
                isCollapsed ? LinearLayout.VERTICAL : LinearLayout.HORIZONTAL);
        mFooterContainer.setGravity(
                isCollapsed ? Gravity.CENTER_HORIZONTAL : Gravity.CENTER_VERTICAL);

        int newTabHeight = mIncognitoChipSizePx;

        LinearLayout.LayoutParams newTabParams =
                (LinearLayout.LayoutParams) mNewTabButton.getLayoutParams();
        newTabParams.width =
                isCollapsed
                        ? mButtonSizePx
                        : (isIncognitoVisible ? 0 : ViewGroup.LayoutParams.MATCH_PARENT);
        newTabParams.height = isCollapsed ? mButtonSizePx : newTabHeight;
        newTabParams.weight = (!isCollapsed && isIncognitoVisible) ? 1.0f : 0.0f;
        newTabParams.bottomMargin = (isCollapsed && isIncognitoVisible) ? mFooterButtonGapPx : 0;
        newTabParams.setMarginEnd(0);
        mNewTabButton.setLayoutParams(newTabParams);

        LinearLayout.LayoutParams incognitoParams =
                (LinearLayout.LayoutParams) mIncognitoButton.getLayoutParams();
        incognitoParams.width = isCollapsed ? mButtonSizePx : mIncognitoChipSizePx;
        incognitoParams.height = isCollapsed ? mButtonSizePx : mIncognitoChipSizePx;
        incognitoParams.weight = 0.0f;
        incognitoParams.setMarginStart(
                (!isCollapsed && isIncognitoVisible) ? mFooterButtonGapPx : 0);
        mIncognitoButton.setLayoutParams(incognitoParams);
    }

    @Px
    int getButtonSizePxForTesting() {
        return mButtonSizePx;
    }

    @Px
    int getIncognitoChipSizePxForTesting() {
        return mIncognitoChipSizePx;
    }
}
