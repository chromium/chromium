// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import android.content.Context;
import android.graphics.PointF;
import android.graphics.RectF;
import android.view.MotionEvent;
import android.view.ViewGroup;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.layouts.EventFilter;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.resources.ResourceManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * Abstract representation of an OpenGL layout tailored to draw tabs. It is a framework used as an
 * alternative to the Android UI for lower level hardware accelerated rendering.
 * This layout also pass through all the events that may happen.
 */
public abstract class Layout {
    /** The orientation of the device. */
    @IntDef({Orientation.UNSET, Orientation.PORTRAIT, Orientation.LANDSCAPE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Orientation {
        int UNSET = 0;
        int PORTRAIT = 1;
        int LANDSCAPE = 2;
    }

    /** The possible variations of the visible viewport that different layouts may need. */
    @IntDef({
        ViewportMode.ALWAYS_FULLSCREEN,
        ViewportMode.ALWAYS_SHOWING_BROWSER_CONTROLS,
        ViewportMode.DYNAMIC_BROWSER_CONTROLS,
        ViewportMode.USE_PREVIOUS_BROWSER_CONTROLS_STATE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ViewportMode {
        /** The viewport is assumed to be always fullscreen. */
        int ALWAYS_FULLSCREEN = 0;

        /** The viewport is assuming that browser controls are permanently shown. */
        int ALWAYS_SHOWING_BROWSER_CONTROLS = 1;

        /** The viewport will account for animating browser controls (both shown and hidden). */
        int DYNAMIC_BROWSER_CONTROLS = 2;

        /** Use a viewport that accounts for the browser controls state in the previous layout. */
        int USE_PREVIOUS_BROWSER_CONTROLS_STATE = 3;
    }

    @IntDef({
        LayoutState.STARTING_TO_SHOW,
        LayoutState.SHOWING,
        LayoutState.STARTING_TO_HIDE,
        LayoutState.HIDDEN
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface LayoutState {
        /** The layout is going to hide as soon as the animation finishes. */
        int STARTING_TO_SHOW = 0;

        /** Actively being showed, no ongoing animation. */
        int SHOWING = 1;

        /** The layout is going to show as soon as the animation finishes. */
        int STARTING_TO_HIDE = 2;

        /** Not currently showed, and no ongoing animation. */
        int HIDDEN = 3;
    }

    /** Length of the unstalling animation. **/
    public static final long UNSTALLED_ANIMATION_DURATION_MS = 500;

    private static final float SNAP_SPEED = 1.0f; // dp per second

    // Drawing area properties.
    private float mWidthDp;
    private float mHeightDp;

    /** A {@link Context} instance. */
    private Context mContext;

    /** The current {@link Orientation} of the layout. */
    private @Orientation int mCurrentOrientation;

    // Tabs
    protected TabModelSelector mTabModelSelector;
    protected TabContentManager mTabContentManager;

    // Helpers
    private final LayoutUpdateHost mUpdateHost;
    protected final LayoutRenderHost mRenderHost;

    /** The tabs currently being rendered as part of this layout. The tabs are
     * drawn using the same ordering as this array. */
    protected LayoutTab[] mLayoutTabs;

    // Current state of the Layout.
    private @LayoutState int mLayoutState;

    // The ratio of dp to px.
    protected final float mDpToPx;
    protected final float mPxToDp;

    /**
     * The {@link Layout} is not usable until sizeChanged is called.
     * This is convenient this way so we can pre-create the layout before the host is fully defined.
     * @param context      The current Android's context.
     * @param updateHost   The parent {@link LayoutUpdateHost}.
     * @param renderHost   The parent {@link LayoutRenderHost}.
     */
    public Layout(Context context, LayoutUpdateHost updateHost, LayoutRenderHost renderHost) {
        mContext = context;
        mUpdateHost = updateHost;
        mRenderHost = renderHost;

        // Invalid sizes
        mWidthDp = -1;
        mHeightDp = -1;

        mCurrentOrientation = Orientation.UNSET;
        mDpToPx = context.getResources().getDisplayMetrics().density;
        mPxToDp = 1 / mDpToPx;

        mLayoutState = LayoutState.HIDDEN;
    }

    /**
     * @return The handler responsible for running compositor animations.
     */
    public CompositorAnimationHandler getAnimationHandler() {
        return mUpdateHost.getAnimationHandler();
    }

    /** Called when native initialization is completed. */
    public void onFinishNativeInitialization() {}

    /** Cleans up any internal state. This object should not be used after this call. */
    public void destroy() {}

    /**
     * @return The current {@link Context} instance associated with this {@link Layout}.
     */
    public Context getContext() {
        return mContext;
    }

    /**
     * @return Whether the {@link Layout} is currently active.
     */
    public boolean isActive() {
        return mUpdateHost.isActiveLayout(this);
    }

    /**
     * Creates a {@link LayoutTab}.
     * @param id              The id of the reference {@link Tab} in the {@link TabModel}.
     * @param isIncognito     Whether the new tab is incognito.
     * @return                The newly created {@link LayoutTab}.
     */
    public LayoutTab createLayoutTab(int id, boolean isIncognito) {
        return createLayoutTab(id, isIncognito, -1.f, -1.f);
    }

    /**
     * Creates a {@link LayoutTab}.
     * @param id               The id of the reference {@link Tab} in the {@link TabModel}.
     * @param isIncognito      Whether the new tab is incognito.
     * @param maxContentWidth  The max content width of the tab.  Negative numbers will use the
     *                         original content width.
     * @param maxContentHeight The max content height of the tab.  Negative numbers will use the
     *                         original content height.
     * @return                 The newly created {@link LayoutTab}.
     */
    public LayoutTab createLayoutTab(
            int id, boolean isIncognito, float maxContentWidth, float maxContentHeight) {
        LayoutTab layoutTab =
                mUpdateHost.createLayoutTab(id, isIncognito, maxContentWidth, maxContentHeight);
        initLayoutTabFromHost(layoutTab);
        return layoutTab;
    }

    /**
     * Releases the data we keep for that {@link LayoutTab}.
     * @param layoutTab The {@link LayoutTab} to release.
     */
    public void releaseTabLayout(LayoutTab layoutTab) {
        mUpdateHost.releaseTabLayout(layoutTab.getId());
    }

    /**
     * Releases cached title texture resources for the {@link LayoutTab}.
     * @param layoutTab The {@link LayoutTab} to release resources for.
     */
    public void releaseResourcesForTab(LayoutTab layoutTab) {
        mUpdateHost.releaseResourcesForTab(layoutTab.getId());
    }

    /**
     * Update the animation and give chance to cascade the changes.
     * @param time The current time of the app in ms.
     * @param dt   The delta time between update frames in ms.
     * @return     Whether the layout is done updating.
     */
    public final boolean onUpdate(long time, long dt) {
        final boolean doneAnimating = onUpdateAnimation(time, false);

        // Don't update the layout if onUpdateAnimation ended up making a new layout active.
        if (mUpdateHost.isActiveLayout(this)) updateLayout(time, dt);

        return doneAnimating;
    }

    /**
     * Layout-specific updates. Cascades the values updated by the animations.
     * @param time The current time of the app in ms.
     * @param dt   The delta time between update frames in ms.
     */
    protected void updateLayout(long time, long dt) {}

    /**
     * Update snapping to pixel. To be called once every frame.
     *
     * <p>TODO(crbug.com/40126259): Temporary placement. This is some Mediator logic and should move
     * to the appropriate location when doing MVC. Maybe move to {@link LayoutMediator}.
     *
     * @param dt The delta time between update frames in ms.
     * @param layoutTab The {@link LayoutTab} that needs to be updating.
     * @return True if the snapping requests to render at least one more frame.
     */
    protected boolean updateSnap(long dt, PropertyModel layoutTab) {
        final float step = dt * SNAP_SPEED / 1000.0f;
        final float renderX = layoutTab.get(LayoutTab.RENDER_X);
        final float renderY = layoutTab.get(LayoutTab.RENDER_Y);
        final float x = updateSnap(step, renderX, layoutTab.get(LayoutTab.X));
        final float y = updateSnap(step, renderY, layoutTab.get(LayoutTab.Y));
        final boolean change = x != renderX || y != renderY;
        layoutTab.set(LayoutTab.RENDER_X, x);
        layoutTab.set(LayoutTab.RENDER_Y, y);
        return change;
    }

    private float updateSnap(float step, float current, float ref) {
        if (Math.abs(current - ref) > mPxToDp) return ref;
        final float refRounded = Math.round(ref * mDpToPx) * mPxToDp;
        if (refRounded < ref) {
            current -= step;
            current = Math.max(refRounded, current);
        } else {
            current += step;
            current = Math.min(refRounded, current);
        }
        return current;
    }

    /**
     * Request that the renderer render a frame (after the current frame). This
     * should be called whenever a new frame should be rendered.
     */
    public void requestRender() {
        mRenderHost.requestRender();
    }

    /**
     * Requests one more frame of refresh for the transforms and changing properties. Primarily,
     * this is so animations can continue to animate.
     */
    public void requestUpdate() {
        mUpdateHost.requestUpdate();
    }

    /**
     * Called when the context and size of the view has changed.
     *
     * @param context     The current Android's context.
     */
    public void contextChanged(Context context) {
        mContext = context;
        LayoutTab.resetDimensionConstants(context);
    }

    /**
     * Called when the size of the viewport has changed.
     *
     * @param screenViewportPx The viewport of the screen in px.
     * @param orientation The new orientation. Valid values are defined by {@link Orientation}.
     */
    final void sizeChanged(RectF screenViewportPx, @Orientation int orientation) {
        mWidthDp = screenViewportPx.width() / mDpToPx;
        mHeightDp = screenViewportPx.height() / mDpToPx;
        mCurrentOrientation = orientation;
    }

    /**
     * Sets the the {@link TabModelSelector} for the layout.
     * @param modelSelector The {@link TabModelSelector} to be set on the layout.
     */
    public void setTabModelSelector(TabModelSelector modelSelector) {
        mTabModelSelector = modelSelector;
    }

    /**
     * Sets the {@link TabContentManager} needed for the layout to get thumbnails.
     * @param manager The {@link TabContentManager} to get tab display content.
     */
    protected void setTabContentManager(TabContentManager manager) {
        if (manager == null) return;

        mTabContentManager = manager;
    }

    /**
     * @return The sizing mode for the layout.
     */
    public @ViewportMode int getViewportMode() {
        return ViewportMode.ALWAYS_SHOWING_BROWSER_CONTROLS;
    }

    /**
     * Informs this cache of the visible {@link Tab} {@code id}s, as well as the
     * primary screen-filling tab.
     */
    protected void updateCacheVisibleIdsAndPrimary(List<Integer> visible, int primaryTabId) {
        if (mTabContentManager != null) mTabContentManager.updateVisibleIds(visible, primaryTabId);
    }

    /**
     * Informs this cache of the visible {@link Tab} {@code id}s, in cases where there
     * is no primary screen-filling tab.
     */
    protected void updateCacheVisibleIds(List<Integer> visible) {
        updateCacheVisibleIdsAndPrimary(visible, Tab.INVALID_TAB_ID);
    }

    /** To be called when the layout is starting a transition out of the view mode. */
    public void startHiding() {
        mUpdateHost.startHiding();
        mLayoutState = LayoutState.STARTING_TO_HIDE;
    }

    /**
     * @return True is the layout is in the process of hiding itself.
     */
    public boolean isStartingToHide() {
        return mLayoutState == LayoutState.STARTING_TO_HIDE;
    }

    /**
     * @return True is the layout is in the process of showing itself.
     */
    public boolean isStartingToShow() {
        return mLayoutState == LayoutState.STARTING_TO_SHOW;
    }

    /**
     * @return The incognito state of the layout.
     */
    public boolean isIncognito() {
        return mTabModelSelector.isIncognitoSelected();
    }

    /** To be called when the transition into the layout is done. */
    public void doneShowing() {
        if (mLayoutState != LayoutState.STARTING_TO_SHOW) return;

        mLayoutState = LayoutState.SHOWING;
        mUpdateHost.doneShowing();
    }

    /**
     * To be called when the transition out of the view mode is done.
     * This is currently called by the renderer when all the animation are done while hiding.
     */
    public void doneHiding() {
        if (mLayoutState != LayoutState.STARTING_TO_HIDE) return;

        mLayoutState = LayoutState.HIDDEN;
        mUpdateHost.doneHiding();
        if (mRenderHost != null && mRenderHost.getResourceManager() != null) {
            mRenderHost.getResourceManager().clearTintedResourceCache();
        }

        if (getSceneLayer() != null) getSceneLayer().removeFromParent();
    }

    /**
     * Initialize the layout to be shown.
     *
     * @param time The current time of the app in ms.
     * @param animate Whether to play an entry animation.
     */
    public void show(long time, boolean animate) {
        // TODO(crbug.com/40141330): Remove after LayoutManager explicitly hide the old layout.
        mLayoutState = LayoutState.STARTING_TO_SHOW;
    }

    /**
     * Hands the layout an Android view to attach it's views to.
     * @param container The Android View to attach the layout's views to.
     */
    public void attachViews(ViewGroup container) {}

    /** Signal to the Layout to detach it's views from the container. */
    public void detachViews() {}

    /** Forces the current animation to finish and broadcasts the proper event. */
    protected void forceAnimationToFinish() {}

    /**
     * @return The width of the drawing area in dp.
     */
    public float getWidth() {
        return mWidthDp;
    }

    /**
     * @return The height of the drawing area in dp.
     */
    public float getHeight() {
        return mHeightDp;
    }

    /**
     * @see Orientation
     * @return The orientation of the screen (portrait or landscape). Values are defined by
     *         {@link Orientation}.
     */
    public @Orientation int getOrientation() {
        return mCurrentOrientation;
    }

    /**
     * Initializes a {@link LayoutTab} with data from the {@link LayoutUpdateHost}. This function
     * eventually needs to be called but may be overridden to manage the posting traffic.
     *
     * @param layoutTab The {@link LayoutTab} To initialize from a
     *                  {@link Tab} on the UI thread.
     * @return          Whether the asynchronous initialization of the {@link LayoutTab} has really
     *                  been posted.
     */
    protected boolean initLayoutTabFromHost(LayoutTab layoutTab) {
        if (layoutTab.isInitFromHostNeeded()) {
            mUpdateHost.initLayoutTabFromHost(layoutTab.getId());
            return true;
        }
        return false;
    }

    /** Called by the LayoutManager when an animation should be killed. */
    public void unstallImmediately() {}

    /**
     * Called by the LayoutManager when an animation should be killed.
     * @param tabId The tab that the kill signal is associated with
     */
    public void unstallImmediately(int tabId) {}

    /**
     * Called by the LayoutManager when they system back button is pressed.
     * @return Whether or not the layout consumed the event.
     */
    public boolean onBackPressed() {
        return false;
    }

    /**
     * Called when a tab get selected. Typically when a tab get closed and the new current tab get
     * selected.
     * @param time      The current time of the app in ms.
     * @param tabId     The id of the selected tab.
     * @param prevId    The id of the previously selected tab.
     * @param incognito Whether or not the affected model was incognito.
     */
    public void onTabSelected(long time, int tabId, int prevId, boolean incognito) {}

    /**
     * Called when a tab is being closed. When called, the closing tab will not
     * be part of the model.
     * @param time      The current time of the app in ms.
     * @param tabId     The id of the tab being closed.
     * @param nextTabId The id if the tab that is being switched to.
     * @param incognito Whether or not the affected model was incognito.
     */
    public void onTabClosed(long time, int tabId, int nextTabId, boolean incognito) {}

    /**
     * Called when all the tabs in the current stack will be closed.
     * When called, the tabs will still be part of the model.
     * @param incognito True if this is the incognito tab model.
     */
    public void onTabsAllClosing(boolean incognito) {}

    /**
     * Called before a tab is created from the top left button.
     *
     * @param sourceTabId The id of the source tab.
     */
    public void onTabCreating(int sourceTabId) {}

    /**
     * Called when a tab is created from the top left button.
     * @param time           The current time of the app in ms.
     * @param tabId          The id of the newly created tab.
     * @param tabIndex       The index of the newly created tab.
     * @param sourceTabId    The id of the source tab.
     * @param newIsIncognito Whether the new tab is incognito.
     * @param background     Whether the tab is created in the background.
     * @param originX        The X screen coordinate in dp of the last touch down event that spawned
     *                       this tab.
     * @param originY        The Y screen coordinate in dp of the last touch down event that spawned
     *                       this tab.
     */
    public void onTabCreated(
            long time,
            int tabId,
            int tabIndex,
            int sourceTabId,
            boolean newIsIncognito,
            boolean background,
            float originX,
            float originY) {}

    /**
     * Called when a tab is restored (created FROM_RESTORE).
     * @param time  The current time of the app in ms.
     * @param tabId The id of the restored tab.
     */
    public void onTabRestored(long time, int tabId) {}

    /**
     * Called when the current tabModel switched (e.g. standard -> incognito).
     *
     * @param incognito True if the new model is incognito.
     */
    public void onTabModelSwitched(boolean incognito) {}

    /**
     * Called when a tab is finally closed if the action was previously undoable.
     * @param time      The current time of the app in ms.
     * @param id        The id of the Tab.
     * @param incognito True if the tab is incognito
     */
    public void onTabClosureCommitted(long time, int id, boolean incognito) {}

    /**
     * Steps the animation forward and updates all the animated values.
     * @param time      The current time of the app in ms.
     * @param jumpToEnd Whether to finish the animation.
     * @return          Whether the animation was finished.
     */
    protected boolean onUpdateAnimation(long time, boolean jumpToEnd) {
        return true;
    }

    /**
     * @return The {@link LayoutTab}s to be drawn.
     */
    public LayoutTab[] getLayoutTabsToRender() {
        return mLayoutTabs;
    }

    /**
     * @return Whether the layout is handling the model updates when a tab is closing.
     */
    public boolean handlesTabClosing() {
        return false;
    }

    /**
     * @return Whether the layout is handling the model updates when a tab is creating.
     */
    public boolean handlesTabCreating() {
        if (mLayoutTabs == null || mLayoutTabs.length != 1) return false;
        return false;
    }

    /**
     * @return True if the content decoration layer should be shown.
     */
    public boolean shouldDisplayContentOverlay() {
        return false;
    }

    /**
     * @return True if the host container can set itself as focusable e.g. for accessibility.
     *         Subclasses can override e.g. to provide a different default focused view.
     */
    public boolean canHostBeFocusable() {
        return true;
    }

    /**
     * @param e                 The {@link MotionEvent} to consider.
     * @param offsets           The current motion offsets that should be applied to the
     *                          {@link EventFilter}s.
     * @param isKeyboardShowing Whether or not the keyboard is showing.
     * @return The {@link EventFilter} the {@link Layout} is listening to.
     */
    public EventFilter findInterceptingEventFilter(
            MotionEvent e, PointF offsets, boolean isKeyboardShowing) {
        EventFilter layoutEventFilter = getEventFilter();
        if (layoutEventFilter != null) {
            if (offsets != null) {
                layoutEventFilter.setCurrentMotionEventOffsets(offsets.x, offsets.y);
            }
            if (layoutEventFilter.onInterceptTouchEvent(e, isKeyboardShowing)) {
                return layoutEventFilter;
            }
            if (layoutEventFilter.onInterceptHoverEvent(e)) {
                return layoutEventFilter;
            }
        }
        return null;
    }

    /**
     * Build a {@link SceneLayer} if it hasn't already been built, and update it and return it.
     *
     * @param viewport          A viewport in which to display content in px.
     * @param visibleViewport   The visible section of the viewport in px.
     * @param tabContentManager A tab content manager.
     * @param resourceManager   A resource manager.
     * @param browserControls   A browser controls state provider.
     * @return                  A {@link SceneLayer} that represents the content for this
     *                          {@link Layout}.
     */
    public final SceneLayer getUpdatedSceneLayer(
            RectF viewport,
            RectF visibleViewport,
            TabContentManager tabContentManager,
            ResourceManager resourceManager,
            BrowserControlsStateProvider browserControls) {
        updateSceneLayer(
                viewport, visibleViewport, tabContentManager, resourceManager, browserControls);
        return getSceneLayer();
    }

    /**
     * @return Whether or not to force the browser controls Android view to hide.
     */
    public boolean forceHideBrowserControlsAndroidView() {
        return false;
    }

    /**
     * @return Whether or not the layout should permanently show the browser controls.
     */
    public boolean forceShowBrowserControlsAndroidView() {
        return false;
    }

    /**
     * @return The EventFilter to use for processing events for this Layout.
     */
    protected abstract EventFilter getEventFilter();

    /**
     * Get an instance of {@link SceneLayer}. Any class inheriting {@link Layout}
     * should override this function in order for other functions to work.
     *
     * @return The scene layer for this {@link Layout}.
     */
    protected abstract SceneLayer getSceneLayer();

    /**
     * Update {@link SceneLayer} instance this layout holds. Any class inheriting {@link Layout}
     * should override this function in order for other functions to work.
     */
    protected void updateSceneLayer(
            RectF viewport,
            RectF contentViewport,
            TabContentManager tabContentManager,
            ResourceManager resourceManager,
            BrowserControlsStateProvider browserControls) {}

    /**
     * @return The {@link LayoutType}.
     */
    public abstract @LayoutType int getLayoutType();

    /** Returns whether the layout is currently running animations. */
    public boolean isRunningAnimations() {
        return false;
    }
}
