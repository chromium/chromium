// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Outline;
import android.util.AttributeSet;
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

import org.chromium.base.MathUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabListProperties.RailCollapseState;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.base.ViewUtils;

/**
 * Root layout for the vertical tab rail container. Encapsulates child view layout styling based on
 * collapse state and forwards raw pointer events to a {@link RailEventListener}.
 */
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

    /**
     * Receives the raw pointer events dispatched to the rail, before its children handle them. A
     * parent view only sees the events its children consume from its dispatch methods, so the rail
     * forwards them as they are and the listener interprets them.
     */
    interface RailEventListener {
        /** Called for every generic motion event (e.g. a mouse hover) dispatched to the rail. */
        void onGenericMotionEventDispatched(MotionEvent event);

        /** Called for every touch event dispatched to the rail. */
        void onTouchEventDispatched(MotionEvent event);
    }

    private @Nullable RailEventListener mRailEventListener;
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
    private @Px int mCollapsedRailWidthPx;
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
                mSearchButton, getContext().getString(R.string.vertical_tabs_tab_search));

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
        mCollapsedRailWidthPx =
                ViewUtils.dpToPx(
                        getContext(), VerticalTabUtils.SIDE_UI_CONTAINER_COLLAPSED_WIDTH_DP);
        // Rounds the right corners of the hover overlay; the left corners are shifted out of
        // bounds. The radius follows the rail width, see getHoverOverlayCornerRadius(). Only
        // clipped to while hover expanded, see updateHeaderLayout().
        setOutlineProvider(
                new ViewOutlineProvider() {
                    @Override
                    public void getOutline(View view, Outline outline) {
                        @Px int radius = getHoverOverlayCornerRadius(view.getWidth());
                        outline.setRoundRect(-radius, 0, view.getWidth(), view.getHeight(), radius);
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

    /** Sets the listener that receives the raw pointer events dispatched to the rail. */
    void setRailEventListener(@Nullable RailEventListener listener) {
        mRailEventListener = listener;
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
    public boolean dispatchTouchEvent(MotionEvent event) {
        if (mRailEventListener != null) mRailEventListener.onTouchEventDispatched(event);
        return super.dispatchTouchEvent(event);
    }

    @Override
    public boolean dispatchGenericMotionEvent(MotionEvent event) {
        if (mRailEventListener != null) mRailEventListener.onGenericMotionEventDispatched(event);
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

        Context context = getContext();
        Resources res = getResources();
        boolean isTablet = VerticalTabUtils.isTablet(context);

        // The whole header button container. The buttons are start-aligned with an explicit offset
        // rather than centered, so they keep the same position when the rail is expanded for
        // hovering.
        mHeaderContainer.setOrientation(
                showSingleRowHeader ? LinearLayout.HORIZONTAL : LinearLayout.VERTICAL);
        mHeaderContainer.setGravity(showSingleRowHeader ? Gravity.NO_GRAVITY : Gravity.START);
        int headerButtonMarginStart =
                showSingleRowHeader
                        ? 0
                        : getCollapsedRailCenteringMarginStart(context, mHeaderButtonWidthPx);

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
        String tooltipText = context.getString(resId);
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
        // Align the label with the tab titles. It can be negative when the icon is wider than the
        // space before the tab titles.
        int searchIconEnd = searchParams.getMarginStart() + searchIconParams.width;
        LinearLayout.LayoutParams searchLabelParams =
                (LinearLayout.LayoutParams) mSearchLabel.getLayoutParams();
        searchLabelParams.setMarginStart(getHoverTabTitleStart(context) - searchIconEnd);

        mCollapseButton.setLayoutParams(collapseParams);
        mSearchButton.setLayoutParams(searchParams);
        mSearchIcon.setLayoutParams(searchIconParams);
        mSearchLabel.setLayoutParams(searchLabelParams);
        // Clip on hover expand, and keep it while collapsing back so the corners shrink with the
        // width. Off after a manual expand, which moves the web contents edge with the rail.
        setClipToOutline(
                isExpandedForHovering
                        || (mCollapseState == RailCollapseState.COLLAPSED && getClipToOutline()));
        updatePinnedTabsSeparatorLayout();
        updateFooterLayout();
    }

    /**
     * Updates the separator between pinned and regular tabs. While the rail uses collapsed
     * positioning, it is start-aligned at its collapsed centered offset, so its start keeps the
     * same position when the rail is expanded for hovering. While expanded for hovering, it extends
     * from that start to the end of the tab rows. Otherwise, it keeps its fixed width, centered in
     * the rail.
     */
    private void updatePinnedTabsSeparatorLayout() {
        ConstraintLayout.LayoutParams params =
                (ConstraintLayout.LayoutParams) mPinnedTabsSeparatorView.getLayoutParams();
        @Px
        int separatorWidthPx =
                getResources().getDimensionPixelSize(R.dimen.vertical_tabs_pinned_separator_width);
        boolean useCollapsedPositioning =
                VerticalTabRailCollapseController.shouldUseCollapsedPositioning(mCollapseState);
        params.width =
                mCollapseState == RailCollapseState.EXPANDED_FOR_HOVERING
                        ? ConstraintLayout.LayoutParams.MATCH_CONSTRAINT
                        : separatorWidthPx;
        params.setMarginStart(
                useCollapsedPositioning
                        ? getCollapsedRailCenteringMarginStart(getContext(), separatorWidthPx)
                        : 0);
        params.horizontalBias = useCollapsedPositioning ? 0f : 0.5f;
        mPinnedTabsSeparatorView.setLayoutParams(params);
    }

    /**
     * Returns the hover overlay corner radius for the given rail width: the width past the
     * collapsed rail width, capped at the full radius. 0 while the rail abuts the web contents.
     */
    @Px
    int getHoverOverlayCornerRadius(@Px int railWidthPx) {
        return MathUtils.clamp(railWidthPx - mCollapsedRailWidthPx, 0, mHoverOverlayCornerRadiusPx);
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
     * Returns the horizontal position in pixels of the tab rows while the rail is expanded for
     * hovering, relative to the rail's start padding. The tab rows keep their collapsed start.
     */
    static @Px int getHoverTabRowStart(Context context) {
        return getCollapsedRailCenteringMarginStart(
                context, TabVerticalViewBinder.getCollapsedTabItemWidth(context));
    }

    /**
     * Returns the horizontal position in pixels of the tab titles while the rail is expanded for
     * hovering, relative to the rail's start padding. Mirrors the favicon and title margins in
     * {@code vertical_tab_item.xml}.
     */
    static @Px int getHoverTabTitleStart(Context context) {
        Resources res = context.getResources();
        return getHoverTabRowStart(context)
                + res.getDimensionPixelSize(R.dimen.vertical_tab_item_padding_horizontal)
                + res.getDimensionPixelSize(R.dimen.vertical_tab_item_icon_size)
                + res.getDimensionPixelSize(R.dimen.vertical_tab_item_favicon_margin_end);
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
        // While expanded for hovering, the new tab button keeps its collapsed start position and
        // extends from there, like the tab rows, so both have the same width.
        newTabParams.setMarginStart(
                mCollapseState == RailCollapseState.EXPANDED_FOR_HOVERING
                        ? getHoverTabRowStart(getContext())
                        : 0);
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
