// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Outline;
import android.util.AttributeSet;
import android.view.DragEvent;
import android.view.Gravity;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewOutlineProvider;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.widget.TooltipCompat;
import androidx.constraintlayout.widget.ConstraintLayout;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabListProperties.RailCollapseState;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.base.ViewUtils;

/**
 * Root layout for the vertical tab rail container. Encapsulates child view layout styling based on
 * collapse state and intercepts mouse motion events to detect hover state transitions.
 */
// TODO(crbug.com/527641177): Migrate remaining view-only logic (e.g. empty space touch and context
// click handlers) from VerticalTabListCoordinator to VerticalTabRailLayout.
@NullMarked
public class VerticalTabRailLayout extends ConstraintLayout {
    /** Functional interface for delegating key events captured by the vertical tab rail. */
    @FunctionalInterface
    interface KeyEventListener {
        /**
         * Handles a key event dispatched to the vertical tab rail.
         *
         * @param event The {@link KeyEvent} dispatched to the rail.
         * @return Whether the event was handled.
         */
        boolean onKeyEvent(KeyEvent event);
    }

    private @Nullable Callback<@RailCollapseState Integer> mExpandOrCollapseOnHoverListener;
    private @Nullable KeyEventListener mKeyEventListener;
    private VerticalTabListRecyclerView mRecyclerView;
    private VerticalPinnedTabListRecyclerView mPinnedTabsRecyclerView;
    private View mSpacerView;
    private View mPinnedTabsSeparatorView;
    private LinearLayout mHeaderContainer;
    private LinearLayout mFooterContainer;
    private ImageButton mCollapseButton;
    private View mSearchButton;
    private ImageView mSearchIcon;
    private TextView mSearchLabel;
    private View mHeaderSpacer;
    private ImageButton mNewTabButton;
    private ImageButton mIncognitoButton;
    private @Px int mIncognitoChipSizePx;
    private @Px int mFooterButtonGapPx;
    private @Px int mHeaderButtonWidthPx;
    private @Px int mHeaderButtonHeightPx;
    private @Px int mFooterButtonCollapsedWidthPx;
    private @Px int mFooterButtonCollapsedHeightPx;
    private @Px int mHoverOverlayCornerRadiusPx;
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

        mPinnedTabsSeparatorView = findViewById(R.id.pinned_tabs_separator);
        assert mPinnedTabsSeparatorView != null;

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

        mSearchIcon = findViewById(R.id.tab_search_icon);
        assert mSearchIcon != null;

        mSearchLabel = findViewById(R.id.tab_search_label);
        assert mSearchLabel != null;

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
        mIncognitoChipSizePx =
                res.getDimensionPixelSize(
                        isTablet
                                ? R.dimen.vertical_tabs_footer_button_height_tablet
                                : R.dimen.vertical_tabs_footer_button_height);
        mFooterButtonGapPx = res.getDimensionPixelSize(R.dimen.vertical_tabs_footer_button_gap);
        mHeaderButtonWidthPx =
                res.getDimensionPixelSize(
                        isTablet
                                ? R.dimen.vertical_tabs_header_button_width_tablet
                                : R.dimen.vertical_tabs_header_button_size);
        mHeaderButtonHeightPx =
                res.getDimensionPixelSize(
                        isTablet
                                ? R.dimen.vertical_tabs_header_button_height_tablet
                                : R.dimen.vertical_tabs_header_button_size);
        mFooterButtonCollapsedWidthPx =
                res.getDimensionPixelSize(
                        isTablet
                                ? R.dimen.vertical_tabs_footer_button_collapsed_width_tablet
                                : R.dimen.vertical_tabs_header_button_size);
        mFooterButtonCollapsedHeightPx =
                res.getDimensionPixelSize(
                        isTablet
                                ? R.dimen.vertical_tabs_footer_button_collapsed_height_tablet
                                : R.dimen.vertical_tabs_header_button_size);
        mHoverOverlayCornerRadiusPx =
                res.getDimensionPixelSize(R.dimen.vertical_tabs_hover_overlay_corner_radius);
        // While expanded for hovering, the rail overlays the web contents with rounded right
        // corners. The outline is shifted left by the radius so its left corners fall outside the
        // view bounds and only the right corners are rounded. It is only clipped to while hover
        // expanded, see updateHeaderLayout().
        setOutlineProvider(
                new ViewOutlineProvider() {
                    @Override
                    public void getOutline(View view, Outline outline) {
                        outline.setRoundRect(
                                -mHoverOverlayCornerRadiusPx,
                                0,
                                view.getWidth(),
                                view.getHeight(),
                                mHoverOverlayCornerRadiusPx);
                    }
                });
        updateHeaderLayout();
    }

    /** Returns the main tab list recycler view. */
    VerticalTabListRecyclerView getRecyclerView() {
        return mRecyclerView;
    }

    /** Returns the pinned tabs recycler view. */
    VerticalPinnedTabListRecyclerView getPinnedTabsRecyclerView() {
        return mPinnedTabsRecyclerView;
    }

    @VisibleForTesting
    View getPinnedTabsSeparatorView() {
        return mPinnedTabsSeparatorView;
    }

    /** Returns the header container view. */
    LinearLayout getHeaderContainer() {
        return mHeaderContainer;
    }

    /** Returns the footer container view. */
    LinearLayout getFooterContainer() {
        return mFooterContainer;
    }

    /** Returns the incognito tab switcher button view in the footer. */
    ImageButton getIncognitoButton() {
        return mIncognitoButton;
    }

    /** Returns the new tab button in the footer. */
    ImageButton getNewTabButton() {
        return mNewTabButton;
    }

    /** Returns the collapse button in the header. */
    ImageButton getCollapseButton() {
        return mCollapseButton;
    }

    /** Returns the clickable tab search button container in the header. */
    View getSearchButton() {
        return mSearchButton;
    }

    /** Returns the icon of the tab search button. */
    ImageView getSearchIcon() {
        return mSearchIcon;
    }

    /** Returns the label of the tab search button, shown while expanded for hovering. */
    TextView getSearchLabel() {
        return mSearchLabel;
    }

    /** Sets the visibility of the separator between pinned tabs and regular tabs. */
    void setPinnedTabsSeparatorVisible(boolean visible) {
        if (mPinnedTabsSeparatorView != null) {
            mPinnedTabsSeparatorView.setVisibility(visible ? View.VISIBLE : View.GONE);
        }

        if (mRecyclerView != null) {
            ViewGroup.MarginLayoutParams lp =
                    (ViewGroup.MarginLayoutParams) mRecyclerView.getLayoutParams();

            // Keep everything in the constraint layout exactly as is if the separator is not
            // visible.
            int targetMargin =
                    visible
                            ? getContext()
                                    .getResources()
                                    .getDimensionPixelSize(
                                            R.dimen.vertical_tabs_pinned_separator_margin_vertical)
                            : 0;
            if (lp.topMargin != targetMargin) {
                lp.topMargin = targetMargin;
                mRecyclerView.setLayoutParams(lp);
            }
        }
    }

    /** Sets the visibility of the desktop window top spacer. */
    public void setDesktopWindowSpacerVisible(boolean visible) {
        mSpacerView.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    /** Sets the hover listener to be notified when hover state transitions occur. */
    void setExpandOrCollapseOnHoverListener(
            @Nullable Callback<@RailCollapseState Integer> listener) {
        mExpandOrCollapseOnHoverListener = listener;
    }

    /** Updates internal child view styling based on the current rail collapse state. */
    void setCollapseState(@RailCollapseState int collapseState) {
        if (mCollapseState == collapseState) return;
        mCollapseState = collapseState;
        updateHeaderLayout();
    }

    /** Returns whether the rail is currently in the collapsed state. */
    boolean isCollapsed() {
        return mCollapseState == RailCollapseState.COLLAPSED;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        if (mPinnedTabsRecyclerView != null
                && mPinnedTabsRecyclerView.getVisibility() != View.GONE) {
            int totalHeight = MeasureSpec.getSize(heightMeasureSpec);

            // Measure child containers to determine available space.
            int headerHeight = mHeaderContainer != null ? mHeaderContainer.getMeasuredHeight() : 0;
            int footerHeight = mFooterContainer != null ? mFooterContainer.getMeasuredHeight() : 0;
            int spacerHeight =
                    (mSpacerView != null && mSpacerView.getVisibility() == View.VISIBLE)
                            ? mSpacerView.getMeasuredHeight()
                            : 0;

            int availableTabSpace = totalHeight - headerHeight - footerHeight - spacerHeight;
            int maxPinnedTabHeight = Math.max(0, availableTabSpace / 2);

            ViewGroup.LayoutParams lp = mPinnedTabsRecyclerView.getLayoutParams();
            if (lp instanceof ConstraintLayout.LayoutParams clp) {
                if (clp.matchConstraintMaxHeight != maxPinnedTabHeight) {
                    clp.matchConstraintMaxHeight = maxPinnedTabHeight;
                }
            }
        }
        // Measure all children while enforcing the params we defined above.
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
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

    /** Sets the {@link KeyEventListener} to intercept key events dispatched to the rail. */
    void setKeyEventListener(@Nullable KeyEventListener listener) {
        mKeyEventListener = listener;
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (mKeyEventListener != null && mKeyEventListener.onKeyEvent(event)) {
            return true;
        }
        return super.dispatchKeyEvent(event);
    }

    private void expandOrCollapseOnHover(@Nullable MotionEvent event) {
        if (mExpandOrCollapseOnHoverListener == null) return;
        if (!VerticalTabUtils.isExpandOnHoverEnabled()) return;
        if (event == null || !event.isFromSource(InputDevice.SOURCE_MOUSE)) return;
        if (!mCollapseButton.isEnabled()) return;

        int action = event.getActionMasked();
        if (mCollapseState == RailCollapseState.EXPANDED_FOR_HOVERING
                && action == MotionEvent.ACTION_HOVER_MOVE) {
            return;
        }

        float rawX = event.getRawX();
        float rawY = event.getRawY();
        boolean isInside = containsRawPoint(this, rawX, rawY);

        // Do not hover-expand over the collapse button so clicking it triggers a full
        // collapsed-to-expanded animation instead of cutting an in-flight hover animation short.
        boolean isOverCollapseButton = containsRawPoint(mCollapseButton, rawX, rawY);
        boolean isHoverEnterOrMove =
                action == MotionEvent.ACTION_HOVER_ENTER || action == MotionEvent.ACTION_HOVER_MOVE;

        if (isInside && !isOverCollapseButton && isHoverEnterOrMove) {
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

        // The collapsed and hover-expanded rails keep the header buttons in their collapsed
        // positions.
        boolean showSingleRowHeader =
                !VerticalTabRailCollapseController.shouldUseCollapsedPositioning(mCollapseState);

        Resources res = getResources();
        boolean isTablet = VerticalTabUtils.isTablet(getContext());

        // The whole header button container. The buttons are start-aligned with an explicit offset
        // rather than centered, so they keep the same position when the rail is expanded for
        // hovering.
        mHeaderContainer.setOrientation(
                showSingleRowHeader ? LinearLayout.HORIZONTAL : LinearLayout.VERTICAL);
        mHeaderContainer.setGravity(showSingleRowHeader ? Gravity.NO_GRAVITY : Gravity.START);
        int headerButtonMarginStart =
                showSingleRowHeader
                        ? 0
                        : getCollapsedRailCenteringMarginStart(getContext(), mHeaderButtonWidthPx);

        // Collapse button
        boolean isManuallyExpanded = mCollapseState == RailCollapseState.EXPANDED;
        ViewGroup.MarginLayoutParams collapseParams =
                (ViewGroup.MarginLayoutParams) mCollapseButton.getLayoutParams();
        collapseParams.width = mHeaderButtonWidthPx;
        collapseParams.height = mHeaderButtonHeightPx;
        collapseParams.setMarginStart(headerButtonMarginStart);
        collapseParams.bottomMargin =
                showSingleRowHeader
                        ? 0
                        : res.getDimensionPixelOffset(
                                R.dimen.vertical_tabs_header_padding_vertical);
        mCollapseButton.setImageResource(
                isTablet
                        ? (isManuallyExpanded
                                ? R.drawable.vertical_tabs_menu_collapse_24dp
                                : R.drawable.vertical_tabs_menu_expand_24dp)
                        : (isManuallyExpanded
                                ? R.drawable.vertical_tabs_menu_collapse
                                : R.drawable.vertical_tabs_menu_expand));
        mSearchIcon.setImageResource(
                isTablet ? R.drawable.ic_manage_search_24dp : R.drawable.ic_manage_search_20dp);
        int resId =
                isManuallyExpanded
                        ? R.string.accessibility_collapse_vertical_tabs
                        : R.string.accessibility_expand_vertical_tabs;
        String tooltipText = getContext().getString(resId);
        mCollapseButton.setContentDescription(tooltipText);
        TooltipCompat.setTooltipText(mCollapseButton, tooltipText);

        // Horizontal header spacer
        mHeaderSpacer.setVisibility(showSingleRowHeader ? View.VISIBLE : View.GONE);

        // Search button. While the rail is expanded for hovering, the button spans the rail width
        // like the tab rows and shows its label. The icon keeps the button width, so it stays in
        // the same position in every state.
        boolean isExpandedForHovering = mCollapseState == RailCollapseState.EXPANDED_FOR_HOVERING;
        LinearLayout.LayoutParams searchParams =
                (LinearLayout.LayoutParams) mSearchButton.getLayoutParams();
        searchParams.width =
                isExpandedForHovering ? ViewGroup.LayoutParams.MATCH_PARENT : mHeaderButtonWidthPx;
        searchParams.height = mHeaderButtonHeightPx;
        searchParams.setMarginStart(headerButtonMarginStart);
        LinearLayout.LayoutParams searchIconParams =
                (LinearLayout.LayoutParams) mSearchIcon.getLayoutParams();
        searchIconParams.width = mHeaderButtonWidthPx;
        mSearchLabel.setVisibility(isExpandedForHovering ? View.VISIBLE : View.GONE);

        mCollapseButton.setLayoutParams(collapseParams);
        mSearchButton.setLayoutParams(searchParams);
        mSearchIcon.setLayoutParams(searchIconParams);
        // Round the right corners of the hover overlay.
        setClipToOutline(isExpandedForHovering);
        updatePinnedTabsSeparatorLayout();
        updateFooterLayout();
    }

    /**
     * Updates the separator between pinned and regular tabs. While the rail is expanded for
     * hovering, the separator spans the tab row width. Otherwise, it keeps its fixed width,
     * centered in the rail.
     */
    private void updatePinnedTabsSeparatorLayout() {
        ConstraintLayout.LayoutParams params =
                (ConstraintLayout.LayoutParams) mPinnedTabsSeparatorView.getLayoutParams();
        if (mCollapseState == RailCollapseState.EXPANDED_FOR_HOVERING) {
            params.width = ConstraintLayout.LayoutParams.MATCH_CONSTRAINT;
            params.setMarginStart(
                    getCollapsedRailCenteringMarginStart(
                            getContext(),
                            TabVerticalViewBinder.getCollapsedTabItemWidth(getContext())));
        } else {
            params.width =
                    getResources()
                            .getDimensionPixelSize(R.dimen.vertical_tabs_pinned_separator_width);
            params.setMarginStart(0);
        }
        mPinnedTabsSeparatorView.setLayoutParams(params);
    }

    /**
     * Returns the start margin in pixels that places a rail child of the given width at the same
     * horizontal position it has when centered in the collapsed rail.
     */
    static @Px int getCollapsedRailCenteringMarginStart(Context context, @Px int childWidthPx) {
        int collapsedRailWidthPx =
                ViewUtils.dpToPx(context, VerticalTabUtils.SIDE_UI_CONTAINER_COLLAPSED_WIDTH_DP);
        int railPaddingStartPx =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.vertical_tabs_rail_horizontal_margin);
        return (collapsedRailWidthPx - childWidthPx) / 2 - railPaddingStartPx;
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
                        ? mFooterButtonCollapsedWidthPx
                        : (isIncognitoVisible ? 0 : ViewGroup.LayoutParams.MATCH_PARENT);
        newTabParams.height = isCollapsed ? mFooterButtonCollapsedHeightPx : newTabHeight;
        newTabParams.weight = (!isCollapsed && isIncognitoVisible) ? 1.0f : 0.0f;
        newTabParams.bottomMargin = (isCollapsed && isIncognitoVisible) ? mFooterButtonGapPx : 0;
        newTabParams.setMarginEnd(0);
        mNewTabButton.setLayoutParams(newTabParams);

        LinearLayout.LayoutParams incognitoParams =
                (LinearLayout.LayoutParams) mIncognitoButton.getLayoutParams();
        incognitoParams.width = isCollapsed ? mFooterButtonCollapsedWidthPx : mIncognitoChipSizePx;
        incognitoParams.height =
                isCollapsed ? mFooterButtonCollapsedHeightPx : mIncognitoChipSizePx;
        incognitoParams.weight = 0.0f;
        incognitoParams.setMarginStart(
                (!isCollapsed && isIncognitoVisible) ? mFooterButtonGapPx : 0);
        mIncognitoButton.setLayoutParams(incognitoParams);
    }

    /** Returns whether the given screen coordinates fall inside {@code view}'s bounds. */
    private static boolean containsRawPoint(View view, float rawX, float rawY) {
        int[] location = new int[2];
        view.getLocationOnScreen(location);
        return rawX >= location[0]
                && rawX < location[0] + view.getWidth()
                && rawY >= location[1]
                && rawY < location[1] + view.getHeight();
    }

    @Px
    int getHeaderButtonWidthPxForTesting() {
        return mHeaderButtonWidthPx;
    }

    @Px
    int getHeaderButtonHeightPxForTesting() {
        return mHeaderButtonHeightPx;
    }

    @Px
    int getIncognitoChipSizePxForTesting() {
        return mIncognitoChipSizePx;
    }
}
