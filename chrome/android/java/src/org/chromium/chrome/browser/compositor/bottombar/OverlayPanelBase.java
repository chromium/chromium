// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.view.ViewGroup;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.MathUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.PanelState;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.ui.theme.ChromeSemanticColorUtils;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

/** Base abstract class for the Overlay Panel. */
abstract class OverlayPanelBase implements OverlayPanelStateProvider {
    /** The side padding of Bar icons in dps. */
    private static final float BAR_ICON_SIDE_PADDING_DP = 12.f;

    /** The top padding of Bar icons in dps. */
    private static final float BAR_ICON_TOP_PADDING_DP = 10.f;

    /** The height of the Bar's border in dps. */
    private static final float BAR_BORDER_HEIGHT_DP = 1.f;

    /** The height of the expanded Overlay Panel relative to the height of the screen. */
    private static final float EXPANDED_PANEL_HEIGHT_PERCENTAGE = .7f;

    /** The width of the small version of the Overlay Panel in dps. */
    private static final float SMALL_PANEL_WIDTH_DP = 600.f;

    /**
     * The minimum width a screen should have in order to trigger the small version of the Panel.
     */
    private static final float SMALL_PANEL_WIDTH_THRESHOLD_DP = 680.f;

    /** The brightness of the base page when the Panel is peeking. */
    private static final float BASE_PAGE_BRIGHTNESS_STATE_PEEKED = 1.f;

    /** The brightness of the base page when the Panel is expanded. */
    private static final float BASE_PAGE_BRIGHTNESS_STATE_EXPANDED = .7f;

    /**
     * The brightness of the base page when the Panel is maximized. This value matches the alert
     * dialog brightness filter.
     */
    private static final float BASE_PAGE_BRIGHTNESS_STATE_MAXIMIZED = .4f;

    // -------------------------------------------------------------------------
    // TODO(donnd): crbug.com/1143472 - The close button from the legacy UI is
    // no longer used. Remove code related to the button here and in the native
    // interface.
    //
    /** The opacity of the Open-Tab icon when the Panel is peeking. */

    /** The opacity of the Open-Tab icon when the Panel is expanded. */

    /** The opacity of the Open-Tab icon when the Panel is maximized. */

    /** The opacity of the close icon when the Panel is peeking. */
    private static final float CLOSE_ICON_OPACITY_STATE_PEEKED = 0.f;

    /** The opacity of the close icon when the Panel is expanded. */
    private static final float CLOSE_ICON_OPACITY_STATE_EXPANDED = 1.f;

    /** The opacity of the close icon when the Panel is maximized. */
    private static final float CLOSE_ICON_OPACITY_STATE_MAXIMIZED = 1.f;

    /** The id of the close icon drawable. */
    public static final int CLOSE_ICON_DRAWABLE_ID = R.drawable.btn_close;

    // -------------------------------------------------------------------------

    /** The height of the Progress Bar in dps. */
    private static final float PROGRESS_BAR_HEIGHT_DP = 2.f;

    /**
     * The distance from the Progress Bar must be away from the bottom of the
     * screen in order to be completely visible. The closer the Progress Bar
     * gets to the bottom of the screen, the lower its opacity will be. When the
     * Progress Bar is at the very bottom of the screen (when the Overlay Panel
     * is peeking) it will be completely invisible.
     */
    private static final float PROGRESS_BAR_VISIBILITY_THRESHOLD_DP = 10.f;

    /** Ratio of dps per pixel. */
    protected final float mPxToDp;

    /** The height of the Toolbar in dp. */
    private float mToolbarHeightDp;

    /** The background color of the Bar. */
    private final @ColorInt int mBarBackgroundColor;

    /** The tint used for icons (e.g. close icon, etc). */
    private final @ColorInt int mIconColor;

    /** The tint used for drag handlebar. */
    private final @ColorInt int mDragHandlebarColor;

    /** The tint used for the progress bar background. */
    private final @ColorInt int mProgressBarBackgroundColor;

    /** The tint used for the progress bar. */
    private final @ColorInt int mProgressBarColor;

    /**
     * The Y coordinate to apply to the Base Page in order to keep the selection
     * in view when the Overlay Panel is in its EXPANDED state.
     */
    private float mBasePageTargetY;

    /** The current context. */
    protected final Context mContext;

    /** The current state of the Overlay Panel. */
    private @PanelState int mPanelState = PanelState.UNDEFINED;

    /** The padding on each side of the close and open-tab icons. */
    protected final int mButtonPaddingDps;

    /**
     * Indicates whether the Toolbar is allowed to hide vs cannot ever be hidden.
     *
     * @see OverlayPanel#shouldHideAndroidBrowserControls
     */
    protected boolean mCanHideAndroidBrowserControls = true;

    protected ObserverList<OverlayPanelStateProvider.Observer> mObservers = new ObserverList<>();

    // ============================================================================================
    // Constructor
    // ============================================================================================

    /**
     * @param context The current Android {@link Context}.
     * @param toolbarHeightDp The current height of the toolbar in dp.
     */
    public OverlayPanelBase(Context context, float toolbarHeightDp) {
        mContext = context;
        mToolbarHeightDp = toolbarHeightDp;
        mPxToDp = 1.f / mContext.getResources().getDisplayMetrics().density;

        mBarMarginSide = BAR_ICON_SIDE_PADDING_DP;
        mBarMarginTop = BAR_ICON_TOP_PADDING_DP;
        mProgressBarHeight = PROGRESS_BAR_HEIGHT_DP;
        mBarBorderHeight = BAR_BORDER_HEIGHT_DP;

        int bar_height_dimen = R.dimen.overlay_panel_bar_height;
        mBarHeight = mContext.getResources().getDimension(bar_height_dimen) * mPxToDp;

        final Resources resources = mContext.getResources();
        mBarBackgroundColor = ChromeSemanticColorUtils.getOverlayPanelBarBackgroundColor(mContext);
        mIconColor = SemanticColorUtils.getDefaultIconColor(context);
        mDragHandlebarColor = SemanticColorUtils.getDragHandlebarColor(context);
        mProgressBarBackgroundColor = SemanticColorUtils.getDefaultControlColorActive(context);
        mProgressBarColor = SemanticColorUtils.getDefaultControlColorActive(context);
        mButtonPaddingDps =
                (int) (mPxToDp * resources.getDimension(R.dimen.overlay_panel_button_padding));
    }

    // ============================================================================================
    // General API
    // ============================================================================================

    /**
     * @return An Android {@link Context}.
     */
    public Context getContext() {
        return mContext;
    }

    /** Tracks whether the panel has been hidden. {@See #showPanel, #hidePanel}.  */
    protected boolean mPanelHidden;

    /**
     * Animates the Overlay Panel to its closed state.
     * @param reason The reason for the change of panel state.
     * @param animate If the panel should animate closed.
     */
    protected abstract void closePanel(@StateChangeReason int reason, boolean animate);

    /**
     * Event notification that the Panel did get closed.
     * @param reason The reason the panel is closing.
     */
    protected abstract void onClosed(@StateChangeReason int reason);

    /** Temporarily hides a peeking panel for the given reason.  Does nothing if not peeking. */
    public abstract void hidePanel(@StateChangeReason int reason);

    /** Shows a previously hidden panel again.  {@See #hidePanel}. */
    public abstract void showPanel(@StateChangeReason int reason);

    /**
     * Handles when the Panel's container view size changes.
     *
     * @param width The new width of the Panel's container view.
     * @param height The new height of the Panel's container view.
     * @param previousWidth The previous width of the Panel's container view.
     */
    protected abstract void handleSizeChanged(float width, float height, float previousWidth);

    // ============================================================================================
    // OverlayPanelStateProvider
    // ============================================================================================

    @Override
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    // ============================================================================================
    // Layout Integration
    // ============================================================================================

    private float mLayoutWidth;
    private float mLayoutHeight;
    private float mLayoutYOffset;

    private float mMaximumWidth;
    private float mMaximumHeight;

    private boolean mIsFullWidthSizePanelForTesting;
    private boolean mOverrideIsFullWidthSizePanelForTesting;

    /**
     * Called when the layout has changed.
     *
     * @param width  The new width in dp.
     * @param height The new height in dp.
     * @param visibleViewportOffsetY The Y offset of the content in dp.
     */
    public void onLayoutChanged(float width, float height, float visibleViewportOffsetY) {
        if (width == mLayoutWidth
                && height == mLayoutHeight
                && visibleViewportOffsetY == mLayoutYOffset) {
            return;
        }

        float previousLayoutWidth = mLayoutWidth;

        mLayoutWidth = width;
        mLayoutHeight = height;
        mLayoutYOffset = visibleViewportOffsetY;

        mMaximumWidth = calculateOverlayPanelWidth();
        mMaximumHeight = getPanelHeightFromState(PanelState.MAXIMIZED);

        handleSizeChanged(width, height, previousLayoutWidth);
    }

    /**
     * @return Whether the Panel is in full width size.
     */
    @Override
    public boolean isFullWidthSizePanel() {
        return doesMatchFullWidthCriteria(getFullscreenWidth());
    }

    /**
     * @param containerWidth The width of the panel's container.
     * @return Whether the given width matches the criteria required for a full width Panel.
     */
    protected boolean doesMatchFullWidthCriteria(float containerWidth) {
        if (mOverrideIsFullWidthSizePanelForTesting) return mIsFullWidthSizePanelForTesting;

        return containerWidth <= SMALL_PANEL_WIDTH_THRESHOLD_DP;
    }

    /**
     * @return The current X-position of the Overlay Panel.
     */
    protected float calculateOverlayPanelX() {
        return isFullWidthSizePanel()
                ? 0.f
                : Math.round((getFullscreenWidth() - calculateOverlayPanelWidth()) / 2.f);
    }

    /**
     * @return The current Y-position of the Overlay Panel.
     */
    protected float calculateOverlayPanelY() {
        return getTabHeight() + heightForNeverHideBrowserControls() - mHeight;
    }

    /**
     * @return The current width of the Overlay Panel.
     */
    protected float calculateOverlayPanelWidth() {
        return isFullWidthSizePanel() ? getFullscreenWidth() : SMALL_PANEL_WIDTH_DP;
    }

    /**
     * @return Whether the Panel is showing.
     */
    public boolean isShowing() {
        return mHeight > 0;
    }

    /**
     * @return Supplier of whether the Panel is showing.
     */
    public ObservableSupplier<Boolean> isShowingSupplier() {
        return mIsShowingSupplier;
    }

    /**
     * @return Whether the Overlay Panel is opened. That is, whether the current height is greater
     * than the peeking height.
     */
    public boolean isPanelOpened() {
        return mHeight > getBarContainerHeight();
    }

    /**
     * @return The fullscreen width.
     */
    public float getFullscreenWidth() {
        return mLayoutWidth;
    }

    /**
     * @return The height of the tab the panel is displayed on top of.
     */
    public float getTabHeight() {
        return mLayoutHeight - heightForNeverHideBrowserControls();
    }

    /**
     * @return The maximum width of the Overlay Panel in pixels.
     */
    public int getMaximumWidthPx() {
        return Math.round(mMaximumWidth / mPxToDp);
    }

    /**
     * The Panel Bar Container is a abstract container that groups the Controls
     * that will be visible when the Panel is in the peeked state.
     *
     * @return The Panel Bar Container in dps.
     */
    public float getBarContainerHeight() {
        return getBarHeight();
    }

    /** @return The width of the Overlay Panel Content View in pixels. */
    public int getContentViewWidthPx() {
        return getMaximumWidthPx();
    }

    /** @return The height of the Overlay Panel Content View in pixels. */
    public int getContentViewHeightPx() {
        return Math.round(mMaximumHeight / mPxToDp);
    }

    /** @return The offset for the page content in DPs. */
    protected float getLayoutOffsetYDps() {
        return mLayoutYOffset * mPxToDp;
    }

    // ============================================================================================
    // Controls for a never hidden Toolbar.
    // ============================================================================================

    /**
     * Tells this Panel whether it can ever hide the Browser Controls (Toolbar).
     * This is set to false by a Partial-height Chrome Custom Tab, and defaults to true.
     * @param canHideAndroidBrowserControls whether hiding is ever allowed.
     */
    public void setCanHideAndroidBrowserControls(boolean canHideAndroidBrowserControls) {
        mCanHideAndroidBrowserControls = canHideAndroidBrowserControls;
    }

    /**
     * @return The Tab height adjustment needed for Android Browser controls that can never hide,
     * or 0 if the Toolbar is allowed to hide. When the Toolbar cannot hide, it obscures part of
     * the Base Page so the Overlay cannot use that part of the page height. Value in pixels.
     */
    private float heightForNeverHideBrowserControls() {
        return mCanHideAndroidBrowserControls ? 0.f : mToolbarHeightDp * mPxToDp;
    }

    @VisibleForTesting
    protected boolean getCanHideAndroidBrowserControls() {
        return mCanHideAndroidBrowserControls;
    }

    // --------------------------------------------------------------------------------------------
    // Overlay Panel states
    // --------------------------------------------------------------------------------------------

    private float mOffsetX;
    private float mOffsetY;
    private float mHeight;
    private boolean mIsMaximized;
    private final ObservableSupplierImpl<Boolean> mIsShowingSupplier =
            new ObservableSupplierImpl<>();

    /**
     * @return The horizontal offset of the Overlay Panel in DPs.
     */
    public float getOffsetX() {
        return mOffsetX;
    }

    /**
     * @return The vertical offset of the Overlay Panel in DPs.
     */
    public float getOffsetY() {
        return mOffsetY;
    }

    /**
     * @return The width of the Overlay Panel in dps.
     */
    public float getWidth() {
        return mMaximumWidth;
    }

    /**
     * @return The height of the Overlay Panel in dps.
     */
    public float getHeight() {
        return mHeight;
    }

    /**
     * @return Whether the Overlay Panel is fully maximized.
     */
    public boolean isMaximized() {
        return mIsMaximized;
    }

    // --------------------------------------------------------------------------------------------
    // Panel Bar states
    // --------------------------------------------------------------------------------------------
    private final float mBarMarginSide;
    private final float mBarMarginTop;

    private float mBarHeight;
    private boolean mIsBarBorderVisible;
    private float mBarBorderHeight;

    private float mCloseIconOpacity;
    private float mCloseIconWidth;

    private float mOpenTabIconWidth;

    /**
     * @return The side margin of the Bar.
     */
    public float getBarMarginSide() {
        return mBarMarginSide;
    }

    /**
     * @return The top margin of the Bar.
     */
    public float getBarMarginTop() {
        return mBarMarginTop;
    }

    /**
     * @return The bottom margin of the Bar in pixels.
     */
    public float getBarMarginBottomPx() {
        return 0;
    }

    /**
     * @return The height of the Bar in dp.
     */
    public float getBarHeight() {
        return mBarHeight;
    }

    /**
     * @return Whether the Bar border is visible.
     */
    public boolean isBarBorderVisible() {
        return mIsBarBorderVisible;
    }

    /**
     * @return The height of the Bar border.
     */
    public float getBarBorderHeight() {
        return mBarBorderHeight;
    }

    /**
     * @return The background color of the Bar.
     */
    public int getBarBackgroundColor() {
        return mBarBackgroundColor;
    }

    /**
     * @return The tint used for icons.
     */
    public int getIconColor() {
        return mIconColor;
    }

    /**
     * @return The tint used for drag handlebar.
     */
    public int getDragHandlebarColor() {
        return mDragHandlebarColor;
    }

    /** @return the color to use to draw the separator between the Bar and Content. */
    public int getSeparatorLineColor() {
        return SemanticColorUtils.getOverlayPanelSeparatorLineColor(mContext);
    }

    /**
     * @return The opacity of the close icon.
     */
    public float getCloseIconOpacity() {
        return mCloseIconOpacity;
    }

    /**
     * @return The width/height of the close icon.
     */
    public float getCloseIconDimension() {
        if (mCloseIconWidth == 0) {
            mCloseIconWidth =
                    ApiCompatibilityUtils.getDrawable(
                                            mContext.getResources(), CLOSE_ICON_DRAWABLE_ID)
                                    .getIntrinsicWidth()
                            * mPxToDp;
        }
        return mCloseIconWidth;
    }

    /**
     * @return The left X coordinate of the close icon.
     */
    public float getCloseIconX() {
        if (LocalizationUtils.isLayoutRtl()) {
            return getOffsetX() + getBarMarginSide();
        } else {
            return getOffsetX() + getWidth() - getBarMarginSide() - getCloseIconDimension();
        }
    }

    /**
     * @return The width/height of the open tab icon in DPs.
     */
    public float getOpenTabIconDimension() {
        if (mOpenTabIconWidth == 0) {
            Drawable icon =
                    ApiCompatibilityUtils.getDrawable(
                            mContext.getResources(), R.drawable.open_in_new_tab);
            mOpenTabIconWidth = icon.getIntrinsicWidth() * mPxToDp;
        }
        return mOpenTabIconWidth;
    }

    /**
     * @return The left X coordinate of the open new tab icon in DPs.
     */
    public float getOpenTabIconX() {
        float offset = getCloseIconDimension() + 2 * mButtonPaddingDps;
        if (LocalizationUtils.isLayoutRtl()) {
            return getCloseIconX() + offset;
        } else {
            return getCloseIconX() - offset;
        }
    }

    // --------------------------------------------------------------------------------------------
    // Base Page states
    // --------------------------------------------------------------------------------------------

    private float mBasePageY;
    private float mBasePageBrightness = 1.0f;

    /**
     * @return The vertical offset of the base page.
     */
    public float getBasePageY() {
        return mBasePageY;
    }

    /**
     * @return The brightness of the base page.
     */
    public float getBasePageBrightness() {
        return mBasePageBrightness;
    }

    // --------------------------------------------------------------------------------------------
    // Progress Bar states
    // --------------------------------------------------------------------------------------------

    private float mProgressBarOpacity;
    private boolean mIsProgressBarVisible;
    private float mProgressBarHeight;
    private float mProgressBarCompletion;

    /**
     * @return Whether the Progress Bar is visible.
     */
    public boolean isProgressBarVisible() {
        return mIsProgressBarVisible;
    }

    /**
     * @param isVisible Whether the Progress Bar should be visible.
     */
    protected void setProgressBarVisible(boolean isVisible) {
        mIsProgressBarVisible = isVisible;
    }

    /**
     * @return The Progress Bar height.
     */
    public float getProgressBarHeight() {
        return mProgressBarHeight;
    }

    /**
     * @return The Progress Bar opacity.
     */
    public float getProgressBarOpacity() {
        return mProgressBarOpacity;
    }

    /**
     * @return The completion percentage of the Progress Bar.
     */
    public float getProgressBarCompletion() {
        return mProgressBarCompletion;
    }

    /**
     * @param completion The completion to be set.
     */
    protected void setProgressBarCompletion(float completion) {
        mProgressBarCompletion = completion;
    }

    /** Returns the progress bar background color. */
    public @ColorInt int getProgressBarBackgroundColor() {
        return mProgressBarBackgroundColor;
    }

    /** Returns the progress bar color. */
    public @ColorInt int getProgressBarColor() {
        return mProgressBarColor;
    }

    // ============================================================================================
    // State Handler
    // ============================================================================================

    /**
     * @return The panel's state.
     */
    public @PanelState int getPanelState() {
        return mPanelState;
    }

    /**
     * Sets the panel's state.
     * @param state The panel state to transition to.
     * @param reason The reason for a change in the panel's state.
     */
    protected void setPanelState(@PanelState int state, @StateChangeReason int reason) {
        if (mPanelHidden) return;

        if (state == PanelState.CLOSED) {
            mHeight = 0;
            mIsShowingSupplier.set(isShowing());
            onClosed(reason);
        }

        // We should only set the state at the end of this method, in oder to make sure that
        // all callbacks will be fired before changing the state of the Panel. This prevents
        // some flakiness on tests since they rely on changes of state to determine when a
        // particular action has been completed.
        mPanelState = state;
        for (Observer observer : mObservers) {
            observer.onOverlayPanelStateChanged(state, mBarBackgroundColor);
        }
    }

    /**
     * Determines if a given {@code PanelState} is a valid UI state. The UNDEFINED state should
     * never be considered a valid UI state.
     *
     * @param state The given state.
     * @return Whether the state is valid.
     */
    private boolean isValidUiState(@PanelState int state) {
        // TODO(pedrosimonetti): consider removing the UNDEFINED state
        // which would allow removing this method.
        return state != PanelState.UNDEFINED;
    }

    /**
     * Gets the panel's state that is before the given {@code PanelState} in the order of states.
     * @param state The given state.
     * @return The previous state.
     */
    private @PanelState int getPreviousPanelState(@PanelState int state) {
        @Nullable
        @PanelState
        Integer prevState =
                state >= PanelState.PEEKED && state <= PanelState.MAXIMIZED ? state - 1 : null;

        return prevState != null ? prevState : PanelState.UNDEFINED;
    }

    // ============================================================================================
    // Helpers
    // ============================================================================================

    /**
     * Gets the height of the Overlay Panel in dps for a given |state|.
     *
     * @param state The state whose height will be calculated.
     * @return The height of the Overlay Panel in dps for a given |state|.
     */
    public float getPanelHeightFromState(@Nullable @PanelState Integer state) {
        if (state == null) {
            return 0;
        } else if (state == PanelState.PEEKED) {
            return getPeekedHeight();
        } else if (state == PanelState.EXPANDED) {
            return getExpandedHeight();
        } else if (state == PanelState.MAXIMIZED) {
            return getMaximizedHeight();
        }
        return 0;
    }

    /**
     * @return The peeked height of the panel in dps.
     */
    protected float getPeekedHeight() {
        return getBarHeight();
    }

    /**
     * @return The expanded height of the panel in dps.
     */
    protected float getExpandedHeight() {
        if (isFullWidthSizePanel()) {
            return getTabHeight() * EXPANDED_PANEL_HEIGHT_PERCENTAGE;
        } else {
            return (getTabHeight() - mToolbarHeightDp * mPxToDp) * EXPANDED_PANEL_HEIGHT_PERCENTAGE;
        }
    }

    /**
     * @return The maximized height of the panel in dps.
     */
    protected float getMaximizedHeight() {
        return getTabHeight();
    }

    /**
     * @return The fraction of the distance the panel has to be to its next state before animating
     *         itself there. Default is the panel must be half of the way to the next state.
     */
    protected float getThresholdToNextState() {
        return 0.5f;
    }

    /**
     * Finds the state which has the nearest height compared to a given
     * |desiredPanelHeight|.
     *
     * @param desiredPanelHeight The height to compare to.
     * @param velocity The velocity of the swipe if applicable. The swipe is upward if less than 0.
     * @return The nearest panel state.
     */
    protected @PanelState int findNearestPanelStateFromHeight(
            float desiredPanelHeight, float velocity) {
        // If the panel was flung hard enough to make the desired height negative, it's closed.
        if (desiredPanelHeight < 0) return PanelState.CLOSED;

        // First, find the two states that the desired panel height is between.
        @PanelState int nextState = PanelState.UNDEFINED;
        @PanelState int prevState = nextState;
        for (@PanelState int state = PanelState.UNDEFINED;
                state < PanelState.NUM_ENTRIES;
                state++) {
            if (!isValidUiState(state)) continue;
            prevState = nextState;
            nextState = state;
            // The values in PanelState are ascending, they should be kept that way in order for
            // this to work.
            if (desiredPanelHeight >= getPanelHeightFromState(prevState)
                    && desiredPanelHeight < getPanelHeightFromState(nextState)) {
                break;
            }
        }

        // If the desired height is close enough to a certain state, depending on the direction of
        // the velocity, move to that state.
        float lowerBound = getPanelHeightFromState(prevState);
        float distance = getPanelHeightFromState(nextState) - lowerBound;
        float thresholdToNextState =
                velocity < 0.0f ? getThresholdToNextState() : 1.0f - getThresholdToNextState();
        if ((desiredPanelHeight - lowerBound) / distance > thresholdToNextState) {
            return nextState;
        } else {
            return prevState;
        }
    }

    /**
     * Sets the last panel height within the limits allowable by our UI.
     *
     * @param height The height of the panel in dps.
     */
    protected void setClampedPanelHeight(float height) {
        final float clampedHeight =
                MathUtils.clamp(
                        height,
                        getPanelHeightFromState(PanelState.MAXIMIZED),
                        getPanelHeightFromState(PanelState.PEEKED));
        setPanelHeight(clampedHeight);
    }

    /**
     * Sets the panel height.
     *
     * @param height The height of the panel in dps.
     */
    protected void setPanelHeight(float height) {
        updatePanelForHeight(height);
    }

    /**
     * @param state The Panel state.
     * @return Whether the Panel height matches the one from the given state.
     */
    protected boolean doesPanelHeightMatchState(@PanelState int state) {
        return state == getPanelState()
                && MathUtils.areFloatsEqual(getHeight(), getPanelHeightFromState(state));
    }

    // ============================================================================================
    // UI Update Handling
    // ============================================================================================

    /**
     * Updates the UI state for a given |height|.
     *
     * @param height The Overlay Panel height.
     */
    private void updatePanelForHeight(float height) {
        @PanelState int endState = findLargestPanelStateFromHeight(height);
        @PanelState int startState = getPreviousPanelState(endState);
        float percentage = getStateCompletion(height, startState, endState);

        updatePanelSize(height);

        if (endState == PanelState.CLOSED || endState == PanelState.PEEKED) {
            updatePanelForCloseOrPeek(percentage);
        } else if (endState == PanelState.EXPANDED) {
            updatePanelForExpansion(percentage);
        } else if (endState == PanelState.MAXIMIZED) {
            updatePanelForMaximization(percentage);
        }

        updateStatusBar();
    }

    /**
     * Updates the Panel size information.
     *
     * @param height The Overlay Panel height.
     */
    private void updatePanelSize(float height) {
        mHeight = height;
        mOffsetX = calculateOverlayPanelX();
        mOffsetY = calculateOverlayPanelY();
        mIsMaximized = height == getPanelHeightFromState(PanelState.MAXIMIZED);
        mIsShowingSupplier.set(isShowing());
    }

    /**
     * Finds the largest Panel state which is being transitioned to/from.
     * Whenever the Panel is in between states, let's say, when resizing the
     * Panel from its peeked to expanded state, we need to know those two states
     * in order to calculate how closely we are from one of them. This method
     * will always return the nearest state with the largest height, and
     * together with the state preceding it, it's possible to calculate how far
     * the Panel is from them.
     *
     * @param panelHeight The height to compare to.
     * @return The panel state which is being transitioned to/from.
     */
    private @PanelState int findLargestPanelStateFromHeight(float panelHeight) {
        @PanelState int stateFound = PanelState.CLOSED;

        // Iterate over all states and find the largest one which is being
        // transitioned to/from.
        for (@PanelState int state = PanelState.UNDEFINED;
                state < PanelState.NUM_ENTRIES;
                state++) {
            if (!isValidUiState(state)) continue;
            if (panelHeight <= getPanelHeightFromState(state)) {
                stateFound = state;
                break;
            }
        }

        return stateFound;
    }

    /**
     * Gets the state completion percentage, taking into consideration the |height| of the Overlay
     * Panel, and the initial and final states. A completion of 0 means the Panel is in the initial
     * state and a completion of 1 means the Panel is in the final state.
     *
     * @param height The height of the Overlay Panel.
     * @param startState The initial state of the Panel.
     * @param endState The final state of the Panel.
     * @return The completion percentage.
     */
    private float getStateCompletion(
            float height, @PanelState int startState, @PanelState int endState) {
        float startSize = getPanelHeightFromState(startState);
        float endSize = getPanelHeightFromState(endState);
        // NOTE(pedrosimonetti): Handle special case from PanelState.UNDEFINED
        // to PanelState.CLOSED, where both have a height of zero. Returning
        // zero here means the Panel will be reset to its CLOSED state.
        float completionPercent =
                startSize == 0.f && endSize == 0.f
                        ? 0.f
                        : (height - startSize) / (endSize - startSize);

        return completionPercent;
    }

    /**
     * Updates the UI state for the closed to peeked transition (and vice
     * versa), according to a completion |percentage|.
     *
     * Note that this method may be called when the panel is going from expanded to peeked because
     * the end panel state for the transitions is calculated based on the panel height. When the
     * panel reaches the peeking height, the calculated end state is peeked.
     *
     * @param percentage The completion percentage.
     */
    protected void updatePanelForCloseOrPeek(float percentage) {
        // Base page offset.
        mBasePageY = 0.f;

        // Base page brightness.
        mBasePageBrightness = BASE_PAGE_BRIGHTNESS_STATE_PEEKED;

        // Bar border.
        mIsBarBorderVisible = false;

        // Close icon opacity.
        mCloseIconOpacity = CLOSE_ICON_OPACITY_STATE_PEEKED;

        // Progress Bar.
        mProgressBarOpacity = 0.f;
    }

    /**
     * Updates the UI state for the peeked to expanded transition (and vice
     * versa), according to a completion |percentage|.
     *
     * Note that this method will never be called with percentage = 0.f. Once the panel
     * reaches the peeked state #updatePanelForCloseOrPeek() will be called instead of this method
     * because the end panel state for transitions is calculated based on the panel height.
     *
     * @param percentage The completion percentage.
     */
    protected void updatePanelForExpansion(float percentage) {
        // Base page offset.
        mBasePageY = MathUtils.interpolate(0.f, getBasePageTargetY(), percentage);

        // Base page brightness.
        mBasePageBrightness =
                MathUtils.interpolate(
                        BASE_PAGE_BRIGHTNESS_STATE_PEEKED,
                        BASE_PAGE_BRIGHTNESS_STATE_EXPANDED,
                        percentage);

        // Bar border.
        mIsBarBorderVisible = true;

        // Determine fading element opacities. The arrow icon needs to finish fading out before
        // the close icon starts fading in. Any other elements fading in or fading out should use
        // the same percentage.
        float fadingInPercentage = Math.max(percentage - .5f, 0.f) / .5f;

        // Close Icon.
        mCloseIconOpacity =
                MathUtils.interpolate(
                        CLOSE_ICON_OPACITY_STATE_PEEKED,
                        CLOSE_ICON_OPACITY_STATE_EXPANDED,
                        fadingInPercentage);

        // Progress Bar.
        float peekedHeight = getPanelHeightFromState(PanelState.PEEKED);
        float threshold = PROGRESS_BAR_VISIBILITY_THRESHOLD_DP / mPxToDp;
        float diff = Math.min(mHeight - peekedHeight, threshold);
        // Fades the Progress Bar the closer it gets to the bottom of the
        // screen.
        mProgressBarOpacity = MathUtils.interpolate(0.f, 1.f, diff / threshold);
    }

    /**
     * Updates the UI state for the expanded to maximized transition (and vice
     * versa), according to a completion |percentage|.
     *
     * @param percentage The completion percentage.
     */
    protected void updatePanelForMaximization(float percentage) {
        // Base page offset.
        mBasePageY = getBasePageTargetY();

        // Base page brightness.
        mBasePageBrightness =
                MathUtils.interpolate(
                        BASE_PAGE_BRIGHTNESS_STATE_EXPANDED,
                        BASE_PAGE_BRIGHTNESS_STATE_MAXIMIZED,
                        percentage);

        // Bar border.
        mIsBarBorderVisible = true;

        // Close Icon.
        mCloseIconOpacity = CLOSE_ICON_OPACITY_STATE_MAXIMIZED;

        // Progress Bar.
        mProgressBarOpacity = 1.f;
    }

    /** Updates the Status Bar. */
    protected void updateStatusBar() {}

    /** @return the maximum brightness of the base page. */
    protected float getMaxBasePageBrightness() {
        return BASE_PAGE_BRIGHTNESS_STATE_PEEKED;
    }

    /** @return the minimum brightness of the base page. */
    protected float getMinBasePageBrightness() {
        return BASE_PAGE_BRIGHTNESS_STATE_MAXIMIZED;
    }

    // ============================================================================================
    // Base Page Offset
    // ============================================================================================

    /**
     * Calculates the desired offset for the Base Page. The purpose of this method is to allow
     * subclasses to provide an specific offset, which can be useful for keeping a certain
     * portion of the Base Page visible when a Panel is in expanded state. To facilitate the
     * calculation, the first argument contains the height of the Panel in the expanded state.
     *
     * @return The desired offset for the Base Page in DPs
     */
    protected float calculateBasePageDesiredOffset() {
        return 0.f;
    }

    /**
     * Updates the target offset of the Base Page in order to keep the selection in view
     * after expanding the Panel.
     */
    protected void updateBasePageTargetY() {
        mBasePageTargetY = calculateBasePageTargetY();
    }

    /**
     * Calculates the target offset of the Base Page in order to achieve the desired offset
     * specified by {@link #calculateBasePageDesiredOffset} while assuring that the Base
     * Page will always fill the gap between the Panel and the top of the screen, because
     * there's nothing to see below the Base Page layer. This method will take into
     * consideration the Toolbar height, and adjust the offset accordingly, in order to
     * move the Toolbar out of the view as the Panel expands.
     *
     * @return The target offset Y in DPs.
     */
    private float calculateBasePageTargetY() {
        // Only a fullscreen wide Panel should offset the base page. A small panel should
        // always return zero to ensure the Base Page remains in the same position.
        if (!isFullWidthSizePanel()) return 0.f;

        // Start with the desired offset taking viewport offset into consideration and make sure
        // the result is <= 0 so the page moves up and not down.
        float offset = Math.min(calculateBasePageDesiredOffset() - getLayoutOffsetYDps(), 0.0f);

        // Make sure the offset is not greater than the expanded height, because
        // there's nothing to render below the Page.
        offset = Math.max(offset, -getExpandedHeight());

        return offset;
    }

    /**
     * @return The Y coordinate to apply to the Base Page in order to keep the selection
     *         in view when the Overlay Panel is in EXPANDED state.
     */
    private float getBasePageTargetY() {
        return mBasePageTargetY;
    }

    // ============================================================================================
    // Resource Loader
    // ============================================================================================

    protected ViewGroup mContainerView;
    protected DynamicResourceLoader mResourceLoader;

    /**
     * @param resourceLoader The {@link DynamicResourceLoader} to register and unregister the view.
     */
    public void setDynamicResourceLoader(DynamicResourceLoader resourceLoader) {
        mResourceLoader = resourceLoader;
    }

    /**
     * Sets the container ViewGroup to which the auxiliary Views will be attached to.
     *
     * @param container The {@link ViewGroup} container.
     */
    public void setContainerView(ViewGroup container) {
        mContainerView = container;
    }

    // ============================================================================================
    // Test Infrastructure
    // ============================================================================================

    /**
     * @param height The height of the Overlay Panel to be set.
     */
    public void setHeightForTesting(float height) {
        mHeight = height;
        mIsShowingSupplier.set(isShowing());
    }

    /**
     * @param offsetY The vertical offset of the Overlay Panel to be
     *            set.
     */
    public void setOffsetYForTesting(float offsetY) {
        mOffsetY = offsetY;
    }

    /**
     * @param isMaximized The setting for whether the Overlay Panel is fully
     *            maximized.
     */
    public void setMaximizedForTesting(boolean isMaximized) {
        mIsMaximized = isMaximized;
    }

    /**
     * @param barHeight The height of the Overlay Bar to be set.
     */
    public void setSearchBarHeightForTesting(float barHeight) {
        mBarHeight = barHeight;
    }

    /**
     * Overrides the FullWidthSizePanel state for testing.
     *
     * @param isFullWidthSizePanel Whether the Panel has a full width size.
     */
    public void setIsFullWidthSizePanelForTesting(boolean isFullWidthSizePanel) {
        mOverrideIsFullWidthSizePanelForTesting = true;
        mIsFullWidthSizePanelForTesting = isFullWidthSizePanel;
    }
}
