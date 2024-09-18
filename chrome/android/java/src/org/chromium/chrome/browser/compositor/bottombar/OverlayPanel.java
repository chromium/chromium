// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar;

import android.app.Activity;
import android.content.Context;
import android.graphics.RectF;
import android.os.Handler;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager.PanelPriority;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.layouts.SceneChangeObserver;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.MotionEventHandler;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.OverlayPanelEventFilter;
import org.chromium.chrome.browser.layouts.EventFilter;
import org.chromium.chrome.browser.layouts.SceneOverlay;
import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.chrome.browser.layouts.scene_layer.SceneOverlayLayer;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBrowserControlsConstraintsHelper;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.ScrollDirection;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.SwipeHandler;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.resources.ResourceManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/** Controls the Overlay Panel. */
public class OverlayPanel extends OverlayPanelAnimation
        implements ActivityStateListener,
                SwipeHandler,
                MotionEventHandler,
                OverlayPanelContentFactory,
                SceneOverlay {
    /** The delay after which the hide progress will be hidden. */
    private static final long HIDE_PROGRESS_BAR_DELAY_MS = 1000 / 60 * 4;

    /** State of the Overlay Panel. */
    @IntDef({
        PanelState.UNDEFINED,
        PanelState.CLOSED,
        PanelState.PEEKED,
        PanelState.EXPANDED,
        PanelState.MAXIMIZED
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface PanelState {
        // Values can't have gaps and should be numerated from 0.
        // Values CLOSED - MAXIMIZED are sorted and show next states.
        // TODO(pedrosimonetti): consider removing the UNDEFINED state
        int UNDEFINED = 0;
        int CLOSED = 1;
        int PEEKED = 2;
        int EXPANDED = 3;
        int MAXIMIZED = 4;
        int NUM_ENTRIES = 5;
    }

    /** The reason for a change in the Overlay Panel's state. */
    @IntDef({
        StateChangeReason.UNKNOWN,
        StateChangeReason.RESET,
        StateChangeReason.BACK_PRESS,
        StateChangeReason.TEXT_SELECT_TAP,
        StateChangeReason.TEXT_SELECT_LONG_PRESS,
        StateChangeReason.INVALID_SELECTION,
        StateChangeReason.CLEARED_SELECTION,
        StateChangeReason.BASE_PAGE_TAP,
        StateChangeReason.BASE_PAGE_SCROLL,
        StateChangeReason.SEARCH_BAR_TAP,
        StateChangeReason.SERP_NAVIGATION,
        StateChangeReason.TAB_PROMOTION,
        StateChangeReason.CLICK,
        StateChangeReason.SWIPE,
        StateChangeReason.FLING,
        StateChangeReason.OPTIN,
        StateChangeReason.OPTOUT,
        StateChangeReason.CLOSE_BUTTON,
        StateChangeReason.PANEL_SUPPRESS,
        StateChangeReason.PANEL_UNSUPPRESS,
        StateChangeReason.TAP_SUPPRESS,
        StateChangeReason.NAVIGATION
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface StateChangeReason {
        int UNKNOWN = 0;
        int RESET = 1;
        int BACK_PRESS = 2;
        int TEXT_SELECT_TAP = 3;
        int TEXT_SELECT_LONG_PRESS = 4;
        int INVALID_SELECTION = 5;
        int CLEARED_SELECTION = 6;
        int BASE_PAGE_TAP = 7;
        int BASE_PAGE_SCROLL = 8;
        int SEARCH_BAR_TAP = 9;
        int SERP_NAVIGATION = 10;
        int TAB_PROMOTION = 11;
        int CLICK = 12;
        int SWIPE = 13;
        int FLING = 14;
        int OPTIN = 15;
        int OPTOUT = 16;
        int CLOSE_BUTTON = 17;
        int PANEL_SUPPRESS = 18;
        int PANEL_UNSUPPRESS = 19;
        int TAP_SUPPRESS = 20;
        int NAVIGATION = 21;
        // Always update MAX_VALUE to match the last StateChangeReason in the list.
        int MAX_VALUE = 21;
    }

    /** A layout manager for tracking changes in the active layout. */
    private final LayoutManagerImpl mLayoutManager;

    /** The observer that reacts to scene-change events. */
    private final SceneChangeObserver mSceneChangeObserver;

    /** Used to measure the browser controls. */
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;

    /** The {@link WindowAndroid} for the current activity.  */
    private final WindowAndroid mWindowAndroid;

    /** The {@link Profile} associated with this Panel. */
    private final Profile mProfile;

    /**
     * The {@link CompositorViewHolder}, used as an anchor view. Also injected into other classes.
     */
    private final ViewGroup mCompositorViewHolder;

    /** Supplies the current activity {@link Tab}. */
    private final Supplier<Tab> mCurrentTabSupplier;

    /** OverlayPanel manager handle for notifications of opening and closing. */
    protected final OverlayPanelManager mPanelManager;

    /** The activity this panel is in. */
    protected Activity mActivity;

    /** The initial height of the Overlay Panel. */
    private float mInitialPanelHeight;

    /** The initial location of a touch on the panel */
    private float mInitialPanelTouchY;

    /** Whether a touch gesture has been detected. */
    private boolean mHasDetectedTouchGesture;

    /** The EventFilter that this panel uses. */
    protected EventFilter mEventFilter;

    /** That factory that creates OverlayPanelContents. */
    private OverlayPanelContentFactory mContentFactory;

    /** Container for content the panel will show. */
    private OverlayPanelContent mContent;

    /** If the base page text controls have been cleared. */
    private boolean mDidClearTextControls;

    /** If the panel should be ignoring swipe events (for compatibility mode). */
    private boolean mIgnoreSwipeEvents;

    /** This is used to make sure there is one show request to one close request. */
    private boolean mPanelShown;

    /** Sequence number for peekPanel() calls to distinguish stale from fresh. */
    private int mLastPeekSequence;

    /**
     * Cache the viewport width and height of the screen to filter SceneOverlay#onSizeChanged
     * events.
     */
    private float mViewportWidth;

    private float mViewportHeight;

    // ============================================================================================
    // Constructor
    // ============================================================================================

    /**
     * @param context The current Android {@link Context}.
     * @param layoutManager A {@link LayoutManagerImpl} for observing changes in the active layout.
     * @param panelManager The {@link OverlayPanelManager} responsible for showing panels.
     * @param browserControlsStateProvider The {@link BrowserControlsStateProvider} for measuring
     *     controls.
     * @param windowAndroid The {@link WindowAndroid} for the current activity.
     * @param profile The Profile this OverlayPanel is associated with.
     * @param compositorViewHolder The {@link CompositorViewHolder}
     * @param toolbarHeightDp The height of the toolbar in dp.
     * @param currentTabSupplier Supplies the current {@link Tab}.
     */
    public OverlayPanel(
            @NonNull Context context,
            @NonNull LayoutManagerImpl layoutManager,
            @NonNull OverlayPanelManager panelManager,
            @NonNull BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull WindowAndroid windowAndroid,
            @NonNull Profile profile,
            @NonNull ViewGroup compositorViewHolder,
            float toolbarHeightDp,
            @NonNull Supplier<Tab> currentTabSupplier) {
        super(context, layoutManager, toolbarHeightDp);
        mLayoutManager = layoutManager;
        mContentFactory = this;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mWindowAndroid = windowAndroid;
        mProfile = profile;
        mCompositorViewHolder = compositorViewHolder;
        mCurrentTabSupplier = currentTabSupplier;

        mPanelManager = panelManager;
        mPanelManager.registerPanel(this);
        mEventFilter = new OverlayPanelEventFilter(mContext, this);

        mSceneChangeObserver =
                new SceneChangeObserver() {
                    @Override
                    public void onSceneChange(Layout layout) {
                        closePanel(StateChangeReason.UNKNOWN, false);
                    }
                };

        mLayoutManager.addSceneChangeObserver(mSceneChangeObserver);
    }

    /** Return the {@link WindowAndroid} this panel is associated with. */
    protected WindowAndroid getWindowAndroid() {
        return mWindowAndroid;
    }

    /** Return the {@link Profile} this panel is associated with. */
    protected Profile getProfile() {
        return mProfile;
    }

    /**
     * Return the {@link ViewGroup} corresponding to the CompositorViewHolder of the Activity
     * containing this OverlayPanel.
     */
    protected ViewGroup getCompositorViewHolder() {
        return mCompositorViewHolder;
    }

    /** Return the {@link Tab} supplier for the Activity containing this OverlayPanel. */
    protected Supplier<Tab> getCurrentTabSupplier() {
        return mCurrentTabSupplier;
    }

    /** Destroy the native components associated with this panel's content. */
    public void destroy() {
        closePanel(StateChangeReason.UNKNOWN, false);
        mLayoutManager.removeSceneChangeObserver(mSceneChangeObserver);
        ApplicationStatus.unregisterActivityStateListener(this);
    }

    /**
     * Destroy the components associated with this panel. This should be overridden by panel
     * implementations to destroy text views and other elements.
     */
    protected void destroyComponents() {
        destroyOverlayPanelContent();
    }

    @Override
    protected void onClosed(@StateChangeReason int reason) {
        mPanelShown = false;
        setBasePageTextControlsVisibility(true);
        destroyComponents();
        mPanelManager.notifyPanelClosed(this, reason);
    }

    // ============================================================================================
    // General API
    // ============================================================================================

    /**
     * @return True if the panel is in the PEEKED state.
     */
    public boolean isPeeking() {
        return doesPanelHeightMatchState(PanelState.PEEKED);
    }

    /**
     * @return Whether the Panel is in its expanded state.
     */
    public boolean isExpanded() {
        return doesPanelHeightMatchState(PanelState.EXPANDED);
    }

    @Override
    public void closePanel(@StateChangeReason int reason, boolean animate) {
        // If the panel hasn't peeked, then it shouldn't need to close.
        if (!mPanelShown) return;

        super.closePanel(reason, animate);
    }

    /**
     * Request that this panel be shown.
     * @param reason The reason the panel is being shown.
     */
    public void requestPanelShow(@StateChangeReason int reason) {
        if (mPanelShown) return;

        if (mPanelManager != null) {
            mPanelManager.requestPanelShow(this, reason);
        }
    }

    public int getLastPeekSequence() {
        return mLastPeekSequence;
    }

    @Override
    public void peekPanel(@StateChangeReason int reason) {
        // TODO(mdjones): This is making a protected API public and should be removed. Animation
        // should only be controlled by the OverlayPanelManager.

        // Since the OverlayPanelManager can show panels without requestPanelShow being called, the
        // flag for the panel being shown should be set to true here.
        mPanelShown = true;
        ++mLastPeekSequence;
        super.peekPanel(reason);
    }

    /**
     * @param url The URL that the panel should load.
     */
    public void loadUrlInPanel(String url) {
        getOverlayPanelContent().loadUrl(url, true);
    }

    /**
     * @param url The URL that the panel should load.
     * @param shouldLoadImmediately If the URL should be loaded immediately when this method is
     *                              called.
     */
    public void loadUrlInPanel(String url, boolean shouldLoadImmediately) {
        getOverlayPanelContent().loadUrl(url, shouldLoadImmediately);
    }

    /**
     * @return True if a URL has been loaded in the panel's current WebContents.
     */
    public boolean isProcessingPendingNavigation() {
        return mContent != null && mContent.isProcessingPendingNavigation();
    }

    /**
     * @param activity The {@link Activity} associated with the panel.
     */
    public void setActivity(Activity activity) {
        mActivity = activity;
        if (mActivity != null) {
            ApplicationStatus.registerStateListenerForActivity(this, mActivity);
        } else {
            throw new RuntimeException("Activity provided to OverlayPanel cannot be null!");
        }
    }

    /**
     * Notify the panel's content that it has been touched.
     * @param x The X position of the touch in dp.
     */
    public void notifyBarTouched(float x) {
        if (!isCoordinateInsideCloseButton(x)) {
            getOverlayPanelContent().showContent();
        }
    }

    /**
     * Acknowledges that there was a touch in the search content view, though no immediate action
     * needs to be taken. This should be overridden by child classes.
     * TODO(mdjones): Get a better name for this.
     */
    public void onTouchSearchContentViewAck() {}

    /**
     * Get a panel's display priority. This has a default to MEDIUM and should be overridden by
     * child classes.
     * @return The panel's display priority.
     */
    public @PanelPriority int getPriority() {
        return PanelPriority.MEDIUM;
    }

    /**
     * @return True if a panel can be suppressed. This should be overridden by each panel.
     */
    public boolean canBeSuppressed() {
        return false;
    }

    /**
     * @return The absolute amount in DP that the browser controls have shifted off screen.
     */
    protected float getBrowserControlsOffsetDp() {
        if (mActivity == null) return 0.0f;
        return -mBrowserControlsStateProvider.getTopControlOffset() * mPxToDp;
    }

    /**
     * Set the visibility of the base page text selection controls. This will also attempt to
     * remove focus from the base page to clear any open controls.
     * @param visible If the text controls are visible.
     */
    protected void setBasePageTextControlsVisibility(boolean visible) {
        if (mActivity == null) return;
        if (!mCurrentTabSupplier.hasValue()) return;

        WebContents baseWebContents = mCurrentTabSupplier.get().getWebContents();
        if (baseWebContents == null) return;

        // If the panel does not have focus or isn't open, return.
        if (isPanelOpened() && mDidClearTextControls && !visible) return;
        if (!isPanelOpened() && !mDidClearTextControls && visible) return;

        mDidClearTextControls = !visible;

        SelectionPopupController spc = SelectionPopupController.fromWebContents(baseWebContents);
        if (!visible) spc.setPreserveSelectionOnNextLossOfFocus(true);
        updateFocus(baseWebContents, visible);

        spc.updateTextSelectionUI(visible);
    }

    // Claim or lose focus of the content view of the base WebContents. This keeps
    // the state of the text selected for overlay in consistent way.
    private static void updateFocus(WebContents baseWebContents, boolean focusBaseView) {
        View baseContentView =
                baseWebContents.getViewAndroidDelegate() != null
                        ? baseWebContents.getViewAndroidDelegate().getContainerView()
                        : null;
        if (baseContentView == null) return;

        if (focusBaseView) {
            baseContentView.requestFocus();
        } else {
            baseContentView.clearFocus();
        }
    }

    /**
     * Returns whether the panel has been activated -- asked to show.  It may not yet be physically
     * showing due animation.  Use {@link #isShowing} instead to determine if the panel is
     * physically visible.
     * @return Whether the panel is showing or about to show.
     */
    public boolean isActive() {
        return mPanelShown;
    }

    // ============================================================================================
    // ActivityStateListener
    // ============================================================================================

    @Override
    public void onActivityStateChange(Activity activity, int newState) {
        boolean isMultiWindowMode = MultiWindowUtils.getInstance().isInMultiWindowMode(mActivity);

        // In multi-window mode the activity that was interacted with last is resumed and
        // all others are paused. We should not close Contextual Search in this case,
        // because the activity may be visible even though it is paused.
        if (isMultiWindowMode
                && (newState == ActivityState.PAUSED || newState == ActivityState.RESUMED)) {
            return;
        }

        if (newState == ActivityState.RESUMED
                || newState == ActivityState.STOPPED
                || newState == ActivityState.DESTROYED) {
            closePanel(StateChangeReason.UNKNOWN, false);
        }
    }

    // ============================================================================================
    // Content
    // ============================================================================================

    /**
     * @return True if the panel's content view is showing.
     */
    public boolean isContentShowing() {
        return mContent != null && mContent.isContentShowing();
    }

    /**
     * @return The WebContents that this panel currently holds.
     */
    public WebContents getWebContents() {
        return mContent != null ? mContent.getWebContents() : null;
    }

    /**
     * @return The container view that this panel currently holds.
     */
    public ViewGroup getContainerView() {
        return mContent != null ? mContent.getContainerView() : null;
    }

    /**
     * Call this when a loadUrl request has failed to notify the panel that the WebContents can
     * be reused.  See crbug.com/682953 for details.
     */
    public void onLoadUrlFailed() {
        if (mContent != null) mContent.onLoadUrlFailed();
    }

    /** Progress observer progress indicator animation for a panel. */
    public class PanelProgressObserver extends OverlayPanelContentProgressObserver {
        @Override
        public void onProgressBarStarted() {
            setProgressBarCompletion(0);
            setProgressBarVisible(true);
            requestUpdate();
        }

        @Override
        public void onProgressBarUpdated(float progress) {
            setProgressBarCompletion(progress);
            requestUpdate();
        }

        @Override
        public void onProgressBarFinished() {
            // Hides the Progress Bar after a delay to make sure it is rendered for at least
            // a few frames, otherwise its completion won't be visually noticeable.
            new Handler()
                    .postDelayed(
                            new Runnable() {
                                @Override
                                public void run() {
                                    setProgressBarVisible(false);
                                    requestUpdate();
                                }
                            },
                            HIDE_PROGRESS_BAR_DELAY_MS);
        }
    }

    /**
     * Create a new OverlayPanelContent object. This can be overridden for tests.
     *
     * @return A new OverlayPanelContent object.
     */
    @Override
    public OverlayPanelContent createNewOverlayPanelContent() {
        return new OverlayPanelContent(
                new OverlayPanelContentDelegate(),
                new OverlayPanelContentProgressObserver(),
                mActivity,
                mProfile,
                getBarHeight(),
                mCompositorViewHolder,
                mWindowAndroid,
                mCurrentTabSupplier);
    }

    /** Add any other objects that depend on the OverlayPanelContent having already been created. */
    private OverlayPanelContent createNewOverlayPanelContentInternal() {
        OverlayPanelContent content = mContentFactory.createNewOverlayPanelContent();
        content.setContentViewSize(
                getContentViewWidthPx(), getContentViewHeightPx(), isFullWidthSizePanel());
        return content;
    }

    /**
     * @return A new OverlayPanelContent if the instance was null or the existing one.
     */
    protected OverlayPanelContent getOverlayPanelContent() {
        // Only create the content when necessary
        if (mContent == null) {
            mContent = createNewOverlayPanelContentInternal();
        }
        return mContent;
    }

    /** Destroy the native components of the OverlayPanelContent. */
    protected void destroyOverlayPanelContent() {
        // It is possible that an OverlayPanelContent was never created for this panel.
        if (mContent != null) {
            mContent.destroy();
            mContent = null;
        }
    }

    /**
     * Updates the browser controls state for the base tab.  As these values are set at the renderer
     * level, there is potential for this impacting other tabs that might share the same
     * process. See {@link BrowserControlsState.update(Tab, tab, int current, boolean animate)}
     * @param current The desired current state for the controls.  Pass
     *                {@link BrowserControlsState#BOTH} to preserve the current position.
     * @param animate Whether the controls should animate to the specified ending condition or
     *                should jump immediately.
     */
    public void updateBrowserControlsState(int current, boolean animate) {
        TabBrowserControlsConstraintsHelper.update(mCurrentTabSupplier.get(), current, animate);
    }

    /** Sets the top control state based on the internals of the panel. */
    public void updateBrowserControlsState() {
        if (mContent == null) return;

        // Consider the ContentView height to be fullscreen, and inform the system that
        // the Toolbar is always visible (from the Compositor's perspective), even though
        // the Toolbar and Base Page might be offset outside the screen. This means the
        // renderer will consider the ContentView height to be the fullscreen height
        // minus the Toolbar height.
        //
        // This is necessary to fix the bugs: crbug.com/510205 and crbug.com/510206
        mContent.updateBrowserControlsState(isFullWidthSizePanel());
    }

    /**
     * @return The vertical scroll position of the content.
     */
    public float getContentVerticalScroll() {
        return mContent != null ? mContent.getContentVerticalScroll() : 0.0f;
    }

    // ============================================================================================
    // OverlayPanelBase methods.
    // ============================================================================================

    @Override
    protected void onHeightAnimationFinished() {
        super.onHeightAnimationFinished();

        if (getPanelState() == PanelState.PEEKED || getPanelState() == PanelState.CLOSED) {
            setBasePageTextControlsVisibility(true);
        } else {
            setBasePageTextControlsVisibility(false);
        }
        if (mContent != null) {
            mContent.setPanelTopOffset((int) ((mViewportHeight - getHeight()) / mPxToDp));
        }
    }

    // ============================================================================================
    // Layout Integration
    // ============================================================================================

    /** Updates the Panel so it preserves its state when the orientation changes. */
    protected void updatePanelForOrientationChange() {
        resizePanelToState(getPanelState(), StateChangeReason.UNKNOWN);
    }

    // ============================================================================================
    // Generic Event Handling
    // ============================================================================================

    /** Handles the beginning of the swipe gesture. */
    public void handleSwipeStart() {
        cancelHeightAnimation();

        mHasDetectedTouchGesture = false;
        mInitialPanelHeight = getHeight();
    }

    /**
     * Handles the movement of the swipe gesture.
     *
     * @param ty The movement's total displacement in dps.
     */
    public void handleSwipeMove(float ty) {
        if (mContent != null && ty > 0 && getPanelState() == PanelState.MAXIMIZED) {
            // Resets the Content View scroll position when swiping the Panel down
            // after being maximized.
            mContent.resetContentViewScroll();
        }

        // Negative ty value means an upward movement so subtracting ty means expanding the panel.
        setClampedPanelHeight(mInitialPanelHeight - ty);
        requestUpdate();
    }

    /** Handles the end of the swipe gesture. */
    public void handleSwipeEnd() {
        // This method will be called after handleFling() and handleClick()
        // methods because we also need to track down the onUpOrCancel()
        // action from the Layout. Therefore the animation to the nearest
        // PanelState should only happen when no other gesture has been
        // detected.
        if (!mHasDetectedTouchGesture) {
            mHasDetectedTouchGesture = true;
            animateToNearestState();
        }
    }

    /**
     * Handles the fling gesture.
     *
     * @param velocity The velocity of the gesture in dps per second.
     */
    public void handleFling(float velocity) {
        mHasDetectedTouchGesture = true;
        animateToProjectedState(velocity);
    }

    /**
     * @param x The x coordinate in dp.
     * @return Whether the given |x| coordinate is inside the close button.
     */
    protected boolean isCoordinateInsideCloseButton(float x) {
        if (LocalizationUtils.isLayoutRtl()) {
            return x <= (getCloseIconX() + getCloseIconDimension() + mButtonPaddingDps);
        } else {
            return x >= (getCloseIconX() - mButtonPaddingDps);
        }
    }

    /**
     * @param x The x coordinate in dp.
     * @return Whether the given |x| coordinate is inside the open-in-new-tab button.
     */
    protected boolean isCoordinateInsideOpenTabButton(float x) {
        // Calculation is the same for RTL: within the button plus padding.
        return getOpenTabIconX() - mButtonPaddingDps <= x
                && x <= getOpenTabIconX() + getOpenTabIconDimension() + mButtonPaddingDps;
    }

    /**
     * Handles the click gesture.
     *
     * @param x The x coordinate of the gesture.
     * @param y The y coordinate of the gesture.
     */
    public void handleClick(float x, float y) {
        mHasDetectedTouchGesture = true;
        if (isCoordinateInsideBasePage(x, y)) {
            closePanel(StateChangeReason.BASE_PAGE_TAP, true);
        } else if (isCoordinateInsideBar(x, y) && !onInterceptBarClick()) {
            handleBarClick(x, y);
        }
    }

    /**
     * Handles the click gesture specifically on the bar.
     *
     * @param x The x coordinate of the gesture.
     * @param y The y coordinate of the gesture.
     */
    protected void handleBarClick(float x, float y) {
        if (isPeeking()) {
            expandPanel(StateChangeReason.SEARCH_BAR_TAP);
        }
    }

    /**
     * Allows the click on the bar to be intercepted.
     * @return True if the click on the bar was intercepted by this function.
     */
    protected boolean onInterceptBarClick() {
        return false;
    }

    /**
     * If the panel is intercepting the initial bar swipe event. This should be overridden per
     * panel.
     * @return True if the panel intercepted the initial bar swipe.
     */
    public boolean onInterceptBarSwipe() {
        return false;
    }

    // ============================================================================================
    // Gesture Event helpers
    // ============================================================================================

    /**
     * @param x The x coordinate in dp.
     * @param y The y coordinate in dp.
     * @return Whether the given coordinate is inside the bar area of the overlay.
     */
    public boolean isCoordinateInsideBar(float x, float y) {
        return isCoordinateInsideOverlayPanel(x, y)
                && y >= getOffsetY()
                && y <= (getOffsetY() + getBarContainerHeight());
    }

    /**
     * @param x The x coordinate in dp.
     * @param y The y coordinate in dp.
     * @return Whether the given coordinate is inside the Overlay Content View area.
     */
    public boolean isCoordinateInsideContent(float x, float y) {
        return isCoordinateInsideOverlayPanel(x, y) && y > getContentY();
    }

    /**
     * @return The horizontal offset of the Overlay Content View in dp.
     */
    public float getContentX() {
        return getOffsetX();
    }

    /**
     * @return The vertical offset of the Overlay Content View in dp.
     */
    public float getContentY() {
        return getOffsetY() + getBarContainerHeight();
    }

    /**
     * @param x The x coordinate in dp.
     * @param y The y coordinate in dp.
     * @return Whether the given coordinate is inside the Overlay Panel area.
     */
    public boolean isCoordinateInsideOverlayPanel(float x, float y) {
        return y >= getOffsetY()
                && y <= (getOffsetY() + getHeight())
                && x >= getOffsetX()
                && x <= (getOffsetX() + getWidth());
    }

    /**
     * @param x The x coordinate in dp.
     * @param y The y coordinate in dp.
     * @return Whether the given coordinate is inside the Base Page area.
     */
    private boolean isCoordinateInsideBasePage(float x, float y) {
        return !isCoordinateInsideOverlayPanel(x, y);
    }

    @VisibleForTesting
    public void setOverlayPanelContentFactory(OverlayPanelContentFactory factory) {
        mContentFactory = factory;
    }

    // ============================================================================================
    // GestureHandler and SwipeHandler implementation.
    // ============================================================================================

    @Override
    public void onDown(float x, float y, boolean fromMouse, int buttons) {
        mInitialPanelTouchY = y;
        handleSwipeStart();
    }

    @Override
    public void drag(float x, float y, float deltaX, float deltaY, float tx, float ty) {
        handleSwipeMove(y - mInitialPanelTouchY);
    }

    @Override
    public void onUpOrCancel() {
        handleSwipeEnd();
    }

    @Override
    public void fling(float x, float y, float velocityX, float velocityY) {
        handleFling(velocityY);
    }

    @Override
    public void click(float x, float y, boolean fromMouse, int buttons) {
        handleClick(x, y);
    }

    @Override
    public void onLongPress(float x, float y) {}

    @Override
    public void onPinch(float x0, float y0, float x1, float y1, boolean firstEvent) {}

    @Override
    public void onHoverEnter(float x, float y) {}

    @Override
    public void onHoverMove(float x, float y) {}

    @Override
    public void onHoverExit() {}

    // SwipeHandler implementation.

    @Override
    public void onSwipeStarted(@ScrollDirection int direction, MotionEvent ev) {
        if (onInterceptBarSwipe()) {
            mIgnoreSwipeEvents = true;
            return;
        }
        handleSwipeStart();
    }

    @Override
    public void onSwipeUpdated(MotionEvent current, float tx, float ty, float dx, float dy) {
        if (mIgnoreSwipeEvents) return;
        handleSwipeMove(ty * mPxToDp);
    }

    @Override
    public void onSwipeFinished() {
        if (mIgnoreSwipeEvents) {
            mIgnoreSwipeEvents = false;
            return;
        }
        handleSwipeEnd();
    }

    @Override
    public void onFling(
            @ScrollDirection int direction,
            MotionEvent current,
            float tx,
            float ty,
            float vx,
            float vy) {
        if (mIgnoreSwipeEvents) return;
        handleFling(vy * mPxToDp);
    }

    @Override
    public boolean isSwipeEnabled(@ScrollDirection int direction) {
        return direction == ScrollDirection.UP && isShowing();
    }

    // Other event handlers.

    /**
     * The user has performed a down event and has not performed a move or up yet. This event is
     * commonly used to provide visual feedback to the user to let them know that their action has
     * been recognized.
     * See {@link GestureDetector.SimpleOnGestureListener#onShowPress()}.
     * @param x The x coordinate in dp.
     * @param y The y coordinate in dp.
     */
    public void onShowPress(float x, float y) {}

    // ============================================================================================
    // SceneOverlay implementation.
    // ============================================================================================

    @Override
    public SceneOverlayLayer getUpdatedSceneOverlayTree(
            RectF viewport, RectF visibleViewport, ResourceManager resourceManager, float yOffset) {
        return null;
    }

    @Override
    public boolean isSceneOverlayTreeShowing() {
        return isShowing();
    }

    @Override
    public EventFilter getEventFilter() {
        return mEventFilter;
    }

    @Override
    public void onSizeChanged(
            float width, float height, float visibleViewportOffsetY, int orientation) {
        // Filter events that don't change the viewport width or height.
        if (height != mViewportHeight || width != mViewportWidth) {
            // We only care if the orientation is changing or we're shifting in/out of multi-window.
            // In either case the screen's viewport width or height will certainly change.
            mViewportWidth = width;
            mViewportHeight = height;

            onLayoutChanged(width, height, visibleViewportOffsetY);
            resizePanelContentView();
        }
    }

    /** Resize the panel's ContentView. Apply adjusted bar size to the height. */
    protected void resizePanelContentView() {
        if (!isShowing()) return;

        OverlayPanelContent panelContent = getOverlayPanelContent();

        // Device could have been rotated before panel webcontent creation. Update content size.
        panelContent.setContentViewSize(
                getContentViewWidthPx(), getContentViewHeightPx(), isFullWidthSizePanel());
        panelContent.resizePanelContentView();
    }

    @Override
    public void getVirtualViews(List<VirtualView> views) {
        // TODO(mdjones): Add views for accessibility.
    }

    @Override
    public boolean handlesTabCreating() {
        // If the panel is not opened, do not handle tab creating.
        if (!isPanelOpened()) return false;
        // Updates BrowserControls' State so the Toolbar becomes visible.
        // TODO(pedrosimonetti): The transition when promoting to a new tab is only smooth
        // if the SearchContentView's vertical scroll position is zero. Otherwise the
        // ContentView will appear to jump in the screen. Coordinate with @dtrainor to solve
        // this problem.
        updateBrowserControlsState(BrowserControlsState.BOTH, false);
        return true;
    }

    @Override
    public boolean shouldHideAndroidBrowserControls() {
        return isPanelOpened() && mCanHideAndroidBrowserControls;
    }

    @Override
    public boolean updateOverlay(long time, long dt) {
        if (isPanelOpened()) setBasePageTextControlsVisibility(false);
        return true;
    }

    @Override
    public boolean onBackPressed() {
        if (!isShowing()) return false;
        closePanel(StateChangeReason.BACK_PRESS, true);
        return true;
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return isShowingSupplier();
    }

    @Override
    public @BackPressResult int handleBackPress() {
        return onBackPressed() ? BackPressResult.SUCCESS : BackPressResult.FAILURE;
    }
}
