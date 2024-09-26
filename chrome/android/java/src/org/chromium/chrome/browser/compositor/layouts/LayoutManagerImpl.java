// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import android.content.Context;
import android.graphics.PointF;
import android.graphics.RectF;
import android.os.SystemClock;
import android.util.SparseArray;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.TraceEvent;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsUtils;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel;
import org.chromium.chrome.browser.compositor.layouts.Layout.Orientation;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.layouts.CompositorModelChangeProcessor;
import org.chromium.chrome.browser.layouts.EventFilter;
import org.chromium.chrome.browser.layouts.EventFilter.EventType;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.ManagedLayoutManager;
import org.chromium.chrome.browser.layouts.SceneOverlay;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.layouts.scene_layer.SceneOverlayLayer;
import org.chromium.chrome.browser.readaloud.ReadAloudMiniPlayerSceneLayer;
import org.chromium.chrome.browser.status_indicator.StatusIndicatorCoordinator;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.chrome.browser.toolbar.bottom.ScrollingBottomViewSceneLayer;
import org.chromium.chrome.browser.toolbar.top.TopToolbarOverlayCoordinator;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeBottomChinSceneLayer;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.SwipeHandler;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.SPenSupport;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.resources.ResourceManager;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;
import org.chromium.ui.util.TokenHolder;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * A class that is responsible for managing an active {@link Layout} to show to the screen.  This
 * includes lifecycle managment like showing/hiding this {@link Layout}.
 */
public class LayoutManagerImpl
        implements ManagedLayoutManager, LayoutUpdateHost, LayoutProvider, BackPressHandler {
    /** Sampling at 60 fps. */
    private static final long FRAME_DELTA_TIME_MS = 16;

    /** Used to convert pixels to dp. */
    protected final float mPxToDp;

    /** The {@link LayoutManagerHost}, who is responsible for showing the active {@link Layout}. */
    protected final LayoutManagerHost mHost;

    /** The last X coordinate of the last {@link MotionEvent#ACTION_DOWN} event. */
    protected int mLastTapX;

    /** The last Y coordinate of the last {@link MotionEvent#ACTION_DOWN} event. */
    protected int mLastTapY;

    // Layouts
    /** A {@link Layout} used for showing a normal web page. */
    protected StaticLayout mStaticLayout;

    private final ViewGroup mContentContainer;

    // External Dependencies
    private TabModelSelector mTabModelSelector;

    private final Callback<TabModel> mCurrentTabModelObserver =
            (tabModel) -> {
                tabModelSwitched(tabModel.isIncognitoBranded());
            };
    private TabModelSelectorTabObserver mTabModelSelectorTabObserver;

    // An observer for watching TabModelFilters changes events.
    private TabModelObserver mTabModelFilterObserver;

    // External Observers
    private final ObserverList<LayoutStateObserver> mLayoutObservers = new ObserverList<>();
    // TODO(crbug.com/40141330): Remove after all SceneChangeObserver migrates to
    // LayoutStateObserver.
    private final ObserverList<SceneChangeObserver> mSceneChangeObservers = new ObserverList<>();

    // Current Layout State
    private Layout mActiveLayout;
    private Layout mNextActiveLayout;
    private boolean mAnimateNextLayout;

    // Current Event Fitler State
    private EventFilter mActiveEventFilter;

    // Internal State
    private final SparseArray<LayoutTab> mTabCache = new SparseArray<>();
    private int mControlsShowingToken = TokenHolder.INVALID_TOKEN;
    private int mControlsHidingToken = TokenHolder.INVALID_TOKEN;
    private boolean mUpdateRequested;
    private final OverlayPanelManager mOverlayPanelManager;

    private final Context mContext;

    // Whether or not the last layout was showing the browser controls.
    private boolean mPreviousLayoutShowingToolbar;

    // Used to store the visible viewport and not create a new Rect object every frame.
    private final RectF mCachedVisibleViewport = new RectF();
    private final RectF mCachedWindowViewport = new RectF();

    private final RectF mCachedRect = new RectF();
    private final PointF mCachedPoint = new PointF();

    // Whether the currently active event filter has changed.
    private boolean mIsNewEventFilter;

    /** The animation handler responsible for updating all the browser compositor's animations. */
    private final CompositorAnimationHandler mAnimationHandler;

    private final ObservableSupplierImpl<TabModelSelector> mTabModelSelectorSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplier<TabContentManager> mTabContentManagerSupplier;
    private final CompositorModelChangeProcessor.FrameRequestSupplier mFrameRequestSupplier;

    private BrowserControlsStateProvider mBrowserControlsStateProvider;

    /** The overlays that can be drawn on top of the active layout. */
    protected final List<SceneOverlay> mSceneOverlays = new ArrayList<>();

    /** A map of {@link SceneOverlay} to its position relative to the others. */
    private Map<Class, Integer> mOverlayOrderMap = new HashMap<>();

    /** The supplier of {@link ThemeColorProvider} for top UI. */
    private final Supplier<TopUiThemeColorProvider> mTopUiThemeColorProvider;

    /** The supplier of whether this is going to intercept back press gesture. */
    private final ObservableSupplierImpl<Boolean> mHandleBackPressChangedSupplier =
            new ObservableSupplierImpl<>();

    /** When non-null, #doneShowing should call into the sequencer instead of doing normal work. */
    private ShowingEventSequencer mShowingEventSequencer;

    /**
     * Protected class to handle {@link TabModelObserver} related tasks. Extending classes will
     * need to override any related calls to add new functionality
     */
    protected class LayoutManagerTabModelObserver implements TabModelObserver {
        @Override
        public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
            if (type == TabSelectionType.FROM_OMNIBOX) {
                switchToTab(tab, lastId);
            } else if (tab.getId() != lastId) {
                tabSelected(tab.getId(), lastId, tab.isIncognito());
            }
        }

        @Override
        public void willAddTab(Tab tab, @TabLaunchType int type) {
            // Open the new tab
            if (type == TabLaunchType.FROM_RESTORE
                    || type == TabLaunchType.FROM_REPARENTING
                    || type == TabLaunchType.FROM_EXTERNAL_APP
                    || type == TabLaunchType.FROM_LAUNCHER_SHORTCUT
                    || type == TabLaunchType.FROM_STARTUP
                    || type == TabLaunchType.FROM_APP_WIDGET
                    || type == TabLaunchType.FROM_SYNC_BACKGROUND) {
                return;
            }

            tabCreating(getTabModelSelector().getCurrentTabId(), tab.isIncognito());
        }

        @Override
        public void didAddTab(
                Tab tab,
                @TabLaunchType int launchType,
                @TabCreationState int creationState,
                boolean markedForSelection) {
            int tabId = tab.getId();
            if (launchType == TabLaunchType.FROM_RESTORE) {
                getActiveLayout().onTabRestored(time(), tabId);
            } else {
                boolean incognito = tab.isIncognito();
                boolean willBeSelected =
                        (launchType != TabLaunchType.FROM_LONGPRESS_BACKGROUND
                                        && launchType
                                                != TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP
                                        && launchType != TabLaunchType.FROM_RECENT_TABS
                                        && launchType != TabLaunchType.FROM_RESTORE_TABS_UI
                                        && launchType != TabLaunchType.FROM_SYNC_BACKGROUND
                                        && launchType
                                                != TabLaunchType
                                                        .FROM_COLLABORATION_BACKGROUND_IN_GROUP)
                                || (!getTabModelSelector().isIncognitoSelected() && incognito);
                float lastTapX = LocalizationUtils.isLayoutRtl() ? mHost.getWidth() * mPxToDp : 0.f;
                float lastTapY = 0.f;
                if (launchType != TabLaunchType.FROM_CHROME_UI) {
                    lastTapX = mPxToDp * mLastTapX;
                    lastTapY = mPxToDp * mLastTapY;
                }

                tabCreated(
                        tabId,
                        getTabModelSelector().getCurrentTabId(),
                        launchType,
                        incognito,
                        willBeSelected,
                        lastTapX,
                        lastTapY);
            }
        }

        @Override
        public void willCloseAllTabs(boolean isIncognito) {
            onTabsAllClosing(isIncognito);
        }

        @Override
        public void onFinishingTabClosure(Tab tab) {
            tabClosed(tab.getId(), tab.isIncognito(), false);
        }

        @Override
        public void tabPendingClosure(Tab tab) {
            tabClosed(tab.getId(), tab.isIncognito(), false);
        }

        @Override
        public void multipleTabsPendingClosure(List<Tab> tabs, boolean isAllTabs) {
            // Handled by willCloseAllTabs;
            if (isAllTabs) return;

            for (Tab tab : tabs) {
                tabClosed(tab.getId(), tab.isIncognito(), false);
            }
        }

        @Override
        public void tabClosureCommitted(Tab tab) {
            LayoutManagerImpl.this.tabClosureCommitted(tab.getId(), tab.isIncognito());
        }

        @Override
        public void tabRemoved(Tab tab) {
            tabClosed(tab.getId(), tab.isIncognito(), true);
        }
    }

    /**
     * Scoped class that temporarily delays doneShowing. This stops reentrancy from Layouts without
     * animations that try to call {@link #doneShowing()} immediately. The done showing transition
     * is different from all the others in that the manager drives it instead, instead of the
     * layouts calling up into the host to drive it. This is why only done showing needs help
     * stopping reentrancy.
     */
    private class ShowingEventSequencer implements AutoCloseable {
        private boolean mPendingDoneShowing;

        private ShowingEventSequencer() {
            assert LayoutManagerImpl.this.mShowingEventSequencer == null;
            LayoutManagerImpl.this.mShowingEventSequencer = this;
        }

        @Override
        public void close() {
            assert LayoutManagerImpl.this.mShowingEventSequencer == this;
            LayoutManagerImpl.this.mShowingEventSequencer = null;
            if (mPendingDoneShowing) {
                LayoutManagerImpl.this.doneShowing();
            }
        }

        public void setPendingDoneShowing() {
            mPendingDoneShowing = true;
        }
    }

    /**
     * Creates a {@link LayoutManagerImpl} instance.
     * @param host A {@link LayoutManagerHost} instance.
     * @param contentContainer A {@link ViewGroup} for Android views to be bound to.
     * @param tabContentManagerSupplier Supplier of the {@link TabContentManager} instance.
     * @param topUiThemeColorProvider {@link ThemeColorProvider} for top UI.
     */
    public LayoutManagerImpl(
            LayoutManagerHost host,
            ViewGroup contentContainer,
            ObservableSupplier<TabContentManager> tabContentManagerSupplier,
            Supplier<TopUiThemeColorProvider> topUiThemeColorProvider) {
        mHost = host;
        mPxToDp = 1.f / mHost.getContext().getResources().getDisplayMetrics().density;
        mTabContentManagerSupplier = tabContentManagerSupplier;
        mTopUiThemeColorProvider = topUiThemeColorProvider;
        mContext = host.getContext();

        // Overlays are ordered back (closest to the web content) to front.
        Class[] overlayOrder;

        overlayOrder =
                new Class[] {
                    // Place the tab strip behind the toolbar scene layer as during tab strip
                    // transition, the toolbar will move up and cover the tab strip.
                    StripLayoutHelperManager.class,
                    TopToolbarOverlayCoordinator.class,
                    EdgeToEdgeBottomChinSceneLayer.class,
                    // StripLayoutHelperManager should be updated before
                    // ScrollingBottomViewSceneLayer Since ScrollingBottomViewSceneLayer change
                    // the container size, it causes relocation tab strip scene layer.
                    ScrollingBottomViewSceneLayer.class,
                    StatusIndicatorCoordinator.getSceneOverlayClass(),
                    ContextualSearchPanel.class,
                    ReadAloudMiniPlayerSceneLayer.class
                };

        for (int i = 0; i < overlayOrder.length; i++) mOverlayOrderMap.put(overlayOrder[i], i);

        assert contentContainer != null;
        mContentContainer = contentContainer;

        mAnimationHandler = new CompositorAnimationHandler(this::requestUpdate);

        mOverlayPanelManager = new OverlayPanelManager();

        mFrameRequestSupplier =
                new CompositorModelChangeProcessor.FrameRequestSupplier(this::requestUpdate);
    }

    /**
     * @return The layout manager's panel manager.
     */
    public OverlayPanelManager getOverlayPanelManager() {
        return mOverlayPanelManager;
    }

    @Override
    public CompositorAnimationHandler getAnimationHandler() {
        return mAnimationHandler;
    }

    /**
     * @return The actual current time of the app in ms.
     */
    public static long time() {
        return SystemClock.uptimeMillis();
    }

    /**
     * Gives the {@link LayoutManagerImpl} a chance to intercept and process motion events from the
     * Android {@link View} system.
     * @param e                 The {@link MotionEvent} that might be intercepted.
     * @param isKeyboardShowing Whether or not the keyboard is showing.
     * @param eventType         The type of input event that is processed by an {@link EventFilter}.
     * @return                  Whether or not this current motion event should be intercepted and
     *                          continually forwarded to this class.
     */
    public boolean onInterceptMotionEvent(
            MotionEvent e, boolean isKeyboardShowing, @EventType int eventType) {
        if (mActiveLayout == null) return false;

        if (e.getAction() == MotionEvent.ACTION_DOWN) {
            mLastTapX = (int) e.getX();
            mLastTapY = (int) e.getY();
        }

        PointF offsets = getMotionOffsets(e);

        // The last added overlay will be drawn on top of everything else, therefore the last
        // filter added should have the first chance to intercept any motion events.
        EventFilter layoutFilter = null;
        for (int i = mSceneOverlays.size() - 1; i >= 0; i--) {
            if (!mSceneOverlays.get(i).isSceneOverlayTreeShowing()) continue;
            EventFilter eventFilter = mSceneOverlays.get(i).getEventFilter();
            if (eventFilter == null) continue;
            if (offsets != null) eventFilter.setCurrentMotionEventOffsets(offsets.x, offsets.y);
            if (isEventInterceptedByEventFilter(e, eventFilter, eventType, isKeyboardShowing)) {
                layoutFilter = eventFilter;
                break;
            }
        }

        // If no overlay's filter took the event, check the layout.
        if (layoutFilter == null) {
            layoutFilter = mActiveLayout.findInterceptingEventFilter(e, offsets, isKeyboardShowing);
        }

        mIsNewEventFilter = layoutFilter != mActiveEventFilter;
        mActiveEventFilter = layoutFilter;

        if (mActiveEventFilter != null) mActiveLayout.unstallImmediately();

        return mActiveEventFilter != null;
    }

    private boolean isEventInterceptedByEventFilter(
            MotionEvent event,
            EventFilter eventFilter,
            @EventType int eventType,
            boolean isKeyboardShowing) {
        switch (eventType) {
            case EventType.TOUCH:
                return eventFilter.onInterceptTouchEvent(event, isKeyboardShowing);
            case EventType.HOVER:
                return eventFilter.onInterceptHoverEvent(event);
            default:
                break;
        }
        return false;
    }

    /**
     * Gives the {@link LayoutManagerImpl} a chance to process the touch events from the Android
     * {@link View} system.
     * @param e A {@link MotionEvent} instance.
     * @return  Whether or not {@code e} was consumed.
     */
    public boolean onTouchEvent(MotionEvent e) {
        if (mActiveEventFilter == null) return false;

        // Make sure the first event through the filter is an ACTION_DOWN.
        if (mIsNewEventFilter && e.getActionMasked() != MotionEvent.ACTION_DOWN) {
            MotionEvent downEvent = MotionEvent.obtain(e);
            downEvent.setAction(MotionEvent.ACTION_DOWN);
            if (!onTouchEventInternal(downEvent)) return false;
        }
        mIsNewEventFilter = false;

        return onTouchEventInternal(e);
    }

    private boolean onTouchEventInternal(MotionEvent e) {
        boolean consumed = mActiveEventFilter.onTouchEvent(e);
        PointF offsets = getMotionOffsets(e);
        if (offsets != null) mActiveEventFilter.setCurrentMotionEventOffsets(offsets.x, offsets.y);
        return consumed;
    }

    /**
     * Gives the {@link LayoutManagerImpl} a chance to process the hover events from the Android
     * {@link View} system.
     * @param e A {@link MotionEvent} instance.
     * @return  Whether or not {@code e} was consumed.
     */
    public boolean onHoverEvent(MotionEvent e) {
        if (mActiveEventFilter == null) return false;

        // Make sure the first event through the filter is an ACTION_HOVER_ENTER.
        if (mIsNewEventFilter && e.getActionMasked() != MotionEvent.ACTION_HOVER_ENTER) {
            MotionEvent hoverEnterEvent = MotionEvent.obtain(e);
            hoverEnterEvent.setAction(MotionEvent.ACTION_HOVER_ENTER);
            if (!onHoverEventInternal(hoverEnterEvent)) return false;
        }
        mIsNewEventFilter = false;

        return onHoverEventInternal(e);
    }

    private boolean onHoverEventInternal(MotionEvent e) {
        boolean consumed = mActiveEventFilter.onHoverEvent(e);
        PointF offsets = getMotionOffsets(e);
        if (offsets != null) mActiveEventFilter.setCurrentMotionEventOffsets(offsets.x, offsets.y);
        return consumed;
    }

    private PointF getMotionOffsets(MotionEvent e) {
        int actionMasked = SPenSupport.convertSPenEventAction(e.getActionMasked());

        if (actionMasked == MotionEvent.ACTION_DOWN
                || actionMasked == MotionEvent.ACTION_HOVER_ENTER) {
            getViewportPixel(mCachedRect);

            mCachedPoint.set(-mCachedRect.left, -mCachedRect.top);
            return mCachedPoint;
        } else if (actionMasked == MotionEvent.ACTION_UP
                || actionMasked == MotionEvent.ACTION_CANCEL
                || actionMasked == MotionEvent.ACTION_HOVER_EXIT) {
            mCachedPoint.set(0, 0);
            return mCachedPoint;
        }

        return null;
    }

    /**
     * Updates the state of the active {@link Layout} if needed.  This updates the animations and
     * cascades the changes to the tabs.
     */
    public void onUpdate() {
        TraceEvent.begin("LayoutDriver:onUpdate");
        onUpdate(time(), FRAME_DELTA_TIME_MS);
        TraceEvent.end("LayoutDriver:onUpdate");
    }

    /**
     * Updates the state of the layout.
     * @param timeMs The time in milliseconds.
     * @param dtMs   The delta time since the last update in milliseconds.
     * @return       Whether or not the {@link LayoutManagerImpl} needs more updates.
     */
    @VisibleForTesting
    boolean onUpdate(long timeMs, long dtMs) {
        if (!mUpdateRequested) {
            mFrameRequestSupplier.set(timeMs);
            return false;
        }
        mUpdateRequested = false;

        // TODO(crbug.com/40126259): Remove after the FrameRequestSupplier migrates to the animation
        //  system.
        final Layout layout = getActiveLayout();

        // TODO(mdjones): Remove the time related params from this method. The new animation system
        // has its own timer.
        boolean areAnimatorsComplete = mAnimationHandler.pushUpdate();
        if (layout != null) {
            areAnimatorsComplete &= !layout.isRunningAnimations();
        }

        // TODO(crbug.com/40126259): Layout itself should decide when it's done hiding and done
        //  showing.
        if (layout != null && layout.onUpdate(timeMs, dtMs) && areAnimatorsComplete) {
            if (layout.isStartingToHide()) {
                layout.doneHiding();
            } else if (layout.isStartingToShow()) {
                layout.doneShowing();
            }
        }

        // TODO(crbug.com/40137900): Once overlays are MVC, this should no longer be needed.
        for (int i = 0; i < mSceneOverlays.size(); i++) {
            mSceneOverlays.get(i).updateOverlay(timeMs, dtMs);
        }

        mFrameRequestSupplier.set(timeMs);
        return mUpdateRequested;
    }

    /**
     * Initializes the {@link LayoutManagerImpl}.  Must be called before using this object.
     * @param selector                 A {@link TabModelSelector} instance.
     * @param creator                  A {@link TabCreatorManager} instance.
     * @param controlContainer         A {@link ControlContainer} for browser controls' layout.
     * @param dynamicResourceLoader    A {@link DynamicResourceLoader} instance.
     * @param topUiColorProvider       A theme color provider for the top browser controls.
     */
    public void init(
            TabModelSelector selector,
            TabCreatorManager creator,
            @Nullable ControlContainer controlContainer,
            DynamicResourceLoader dynamicResourceLoader,
            TopUiThemeColorProvider topUiColorProvider) {
        LayoutRenderHost renderHost = mHost.getLayoutRenderHost();

        mBrowserControlsStateProvider = mHost.getBrowserControlsManager();

        // Build Layouts
        mStaticLayout =
                new StaticLayout(
                        mContext,
                        this,
                        renderHost,
                        mHost,
                        mFrameRequestSupplier,
                        selector,
                        mTabContentManagerSupplier.get(),
                        mBrowserControlsStateProvider,
                        mTopUiThemeColorProvider);

        setNextLayout(null, true);

        // Set the dynamic resource loader for all overlay panels.
        mOverlayPanelManager.setDynamicResourceLoader(dynamicResourceLoader);
        mOverlayPanelManager.setContainerView(mContentContainer);

        // The {@link setTabModelSelector} should be called after all of the initialization above
        // complete. See https://crbug.com/1132948.
        if (mTabModelSelector == null) {
            setTabModelSelector(selector);
        }
    }

    // TODO(hanxi): Passes the TabModelSelectorSupplier in the constructor since the
    // mTabModelSelector should only be set once.
    public void setTabModelSelector(TabModelSelector selector) {
        mTabModelSelector = selector;
        mTabModelSelectorSupplier.set(selector);
        mTabModelSelectorTabObserver =
                new TabModelSelectorTabObserver(mTabModelSelector) {
                    @Override
                    public void onShown(Tab tab, @TabSelectionType int type) {
                        initLayoutTabFromHost(tab.getId());
                    }

                    @Override
                    public void onHidden(Tab tab, @TabHidingType int type) {
                        initLayoutTabFromHost(tab.getId());
                    }

                    @Override
                    public void onContentChanged(Tab tab) {
                        initLayoutTabFromHost(tab.getId());
                    }

                    @Override
                    public void onBackgroundColorChanged(Tab tab, int color) {
                        // The NavBarColorMatchesTabBackground increases the frequency of these
                        // notifications, so Chrome should use a more targeted method to limit
                        // performance impact.
                        if (ChromeFeatureList.sNavBarColorMatchesTabBackground.isEnabled()) {
                            updateLayoutTabBackgroundColor(tab.getId());
                        } else {
                            initLayoutTabFromHost(tab.getId());
                        }
                    }

                    @Override
                    public void onDidChangeThemeColor(Tab tab, int color) {
                        initLayoutTabFromHost(tab.getId());
                    }
                };

        if (mNextActiveLayout != null) startShowing(mNextActiveLayout, true);

        selector.getCurrentTabModelSupplier().addObserver(mCurrentTabModelObserver);

        mTabModelFilterObserver = createTabModelObserver();
        getTabModelSelector()
                .getTabModelFilterProvider()
                .addTabModelFilterObserver(mTabModelFilterObserver);
    }

    @Override
    public void destroy() {
        mAnimationHandler.destroy();
        mSceneChangeObservers.clear();
        if (mStaticLayout != null) mStaticLayout.destroy();
        if (mOverlayPanelManager != null) mOverlayPanelManager.destroy();
        if (mTabModelSelectorTabObserver != null) mTabModelSelectorTabObserver.destroy();
        if (getTabModelSelector() != null) {
            getTabModelSelector()
                    .getCurrentTabModelSupplier()
                    .removeObserver(mCurrentTabModelObserver);
        }
        if (mTabModelFilterObserver != null) {
            getTabModelSelector()
                    .getTabModelFilterProvider()
                    .removeTabModelFilterObserver(mTabModelFilterObserver);
        }
    }

    /** @return A resource manager to pull textures from. */
    public ResourceManager getResourceManager() {
        if (mHost.getLayoutRenderHost() == null) return null;
        return mHost.getLayoutRenderHost().getResourceManager();
    }

    @Override
    public <V extends SceneLayer> CompositorModelChangeProcessor<V> createCompositorMCP(
            PropertyModel model,
            V view,
            PropertyModelChangeProcessor.ViewBinder<PropertyModel, V, PropertyKey> viewBinder) {
        return CompositorModelChangeProcessor.create(
                model, view, viewBinder, mFrameRequestSupplier, true);
    }

    /**
     * @param observer Adds {@code observer} to be notified when the active {@code Layout} changes.
     */
    public void addSceneChangeObserver(SceneChangeObserver observer) {
        mSceneChangeObservers.addObserver(observer);
    }

    /**
     * @param observer Removes {@code observer}.
     */
    public void removeSceneChangeObserver(SceneChangeObserver observer) {
        mSceneChangeObservers.removeObserver(observer);
    }

    @Override
    public SceneLayer getUpdatedActiveSceneLayer(
            TabContentManager tabContentManager,
            ResourceManager resourceManager,
            BrowserControlsManager browserControlsManager) {
        updateControlsHidingState(browserControlsManager);
        getViewportPixel(mCachedVisibleViewport);
        mHost.getWindowViewport(mCachedWindowViewport);
        SceneLayer layer =
                mActiveLayout.getUpdatedSceneLayer(
                        mCachedWindowViewport,
                        mCachedVisibleViewport,
                        tabContentManager,
                        resourceManager,
                        browserControlsManager);

        float offsetPx =
                mBrowserControlsStateProvider == null
                        ? 0
                        : mBrowserControlsStateProvider.getTopControlOffset();

        for (int i = 0; i < mSceneOverlays.size(); i++) {
            // If the SceneOverlay is not showing, don't bother adding it to the tree.
            if (!mSceneOverlays.get(i).isSceneOverlayTreeShowing()) continue;

            SceneOverlayLayer overlayLayer =
                    mSceneOverlays
                            .get(i)
                            .getUpdatedSceneOverlayTree(
                                    mCachedWindowViewport,
                                    mCachedVisibleViewport,
                                    resourceManager,
                                    offsetPx * mPxToDp);

            overlayLayer.setContentTree(layer);
            layer = overlayLayer;
        }

        return layer;
    }

    private void updateControlsHidingState(
            BrowserControlsVisibilityManager controlsVisibilityManager) {
        if (controlsVisibilityManager == null) {
            return;
        }

        boolean overlayHidesControls = false;
        for (int i = 0; i < mSceneOverlays.size(); i++) {
            // If any overlay wants to hide tha Android version of the browser controls, hide them.
            if (mSceneOverlays.get(i).shouldHideAndroidBrowserControls()) {
                overlayHidesControls = true;
                break;
            }
        }

        if (overlayHidesControls || mActiveLayout.forceHideBrowserControlsAndroidView()) {
            mControlsHidingToken =
                    controlsVisibilityManager.hideAndroidControlsAndClearOldToken(
                            mControlsHidingToken);
        } else {
            controlsVisibilityManager.releaseAndroidControlsHidingToken(mControlsHidingToken);
        }
    }

    /** Called when the viewport has been changed. */
    public void onViewportChanged() {
        if (getActiveLayout() != null) {
            float previousWidth = getActiveLayout().getWidth();
            float previousHeight = getActiveLayout().getHeight();

            float oldWindowViewportTop = mCachedWindowViewport.top;
            float oldVisibleViewportTop = mCachedVisibleViewport.top;
            mHost.getWindowViewport(mCachedWindowViewport);
            mHost.getVisibleViewport(mCachedVisibleViewport);
            getActiveLayout().sizeChanged(mCachedWindowViewport, getOrientation());

            float width = mCachedWindowViewport.width() * mPxToDp;
            float height = mCachedWindowViewport.height() * mPxToDp;
            if (width != previousWidth
                    || height != previousHeight
                    // TODO (crbug.com/325501037) - Clean up this odd check comparing the window
                    // and visible viewport values after fixing the contextual search menu's
                    // reliance on it.
                    || oldWindowViewportTop != mCachedVisibleViewport.top
                    || oldVisibleViewportTop != mCachedVisibleViewport.top) {
                for (int i = 0; i < mSceneOverlays.size(); i++) {
                    mSceneOverlays
                            .get(i)
                            .onSizeChanged(
                                    width, height, mCachedVisibleViewport.top, getOrientation());
                }
            }
        }

        for (int i = 0; i < mTabCache.size(); i++) {
            // This assumes that the content width/height is always the size of the host.
            mTabCache.valueAt(i).setContentSize(mHost.getWidth(), mHost.getHeight());
        }
    }

    /**
     * @return The default {@link Layout} to show when {@link Layout}s get hidden and the next
     *         {@link Layout} to show isn't known.
     */
    protected Layout getDefaultLayout() {
        return mStaticLayout;
    }

    /**
     * @return The {@link TabModelObserver} instance this class should be using.
     */
    protected LayoutManagerChrome.LayoutManagerTabModelObserver createTabModelObserver() {
        return new LayoutManagerChrome.LayoutManagerTabModelObserver();
    }

    @VisibleForTesting
    public void tabSelected(int tabId, int prevId, boolean incognito) {
        // Update the model here so we properly set the right selected TabModel.
        if (getActiveLayout() != null) {
            getActiveLayout().onTabSelected(time(), tabId, prevId, incognito);
        }
    }

    /**
     * Should be called when a tab creating event is triggered (called before the tab is done being
     * created).
     *
     * @param sourceId The id of the creating tab if any.
     * @param isIncognito Whether or not created tab will be incognito.
     */
    protected void tabCreating(int sourceId, boolean isIncognito) {
        if (getActiveLayout() != null) getActiveLayout().onTabCreating(sourceId);
    }

    /**
     * Should be called when a tab created event is triggered.
     * @param id             The id of the tab that was created.
     * @param sourceId       The id of the creating tab if any.
     * @param launchType     How the tab was launched.
     * @param incognito      Whether or not the created tab is incognito.
     * @param willBeSelected Whether or not the created tab will be selected.
     * @param originX        The x coordinate of the action that created this tab in dp.
     * @param originY        The y coordinate of the action that created this tab in dp.
     */
    protected void tabCreated(
            int id,
            int sourceId,
            @TabLaunchType int launchType,
            boolean incognito,
            boolean willBeSelected,
            float originX,
            float originY) {
        int newIndex = TabModelUtils.getTabIndexById(getTabModelSelector().getModel(incognito), id);
        getActiveLayout()
                .onTabCreated(
                        time(),
                        id,
                        newIndex,
                        sourceId,
                        incognito,
                        !willBeSelected,
                        originX,
                        originY);
    }

    /**
     * Should be called when a tab closed event is triggered.
     * @param id         The id of the closed tab.
     * @param nextId     The id of the next tab that will be visible, if any.
     * @param incognito  Whether or not the closed tab is incognito.
     * @param tabRemoved Whether the tab was removed from the model (e.g. for reparenting), rather
     *                   than closed and destroyed.
     */
    protected void tabClosed(int id, int nextId, boolean incognito, boolean tabRemoved) {
        if (getActiveLayout() != null) getActiveLayout().onTabClosed(time(), id, nextId, incognito);
    }

    private void tabClosed(int tabId, boolean incognito, boolean tabRemoved) {
        Tab currentTab =
                getTabModelSelector() != null ? getTabModelSelector().getCurrentTab() : null;
        int nextTabId = currentTab != null ? currentTab.getId() : Tab.INVALID_TAB_ID;
        tabClosed(tabId, nextTabId, incognito, tabRemoved);
    }

    /**
     * Called when a tab closure has been committed and all tab cleanup should happen.
     * @param id        The id of the closed tab.
     * @param incognito Whether or not the closed tab is incognito.
     */
    protected void tabClosureCommitted(int id, boolean incognito) {
        if (getActiveLayout() != null) {
            getActiveLayout().onTabClosureCommitted(time(), id, incognito);
        }
    }

    /**
     * Called when the selected tab model has switched.
     * @param incognito Whether or not the new current tab model is incognito.
     */
    protected void tabModelSwitched(boolean incognito) {
        if (getActiveLayout() != null) getActiveLayout().onTabModelSwitched(incognito);
    }

    public void onTabsAllClosing(boolean incognito) {
        if (getActiveLayout() == null) return;

        getActiveLayout().onTabsAllClosing(incognito);
    }

    protected Supplier<TopUiThemeColorProvider> getTopUiThemeColorProvider() {
        return mTopUiThemeColorProvider;
    }

    @Override
    public void initLayoutTabFromHost(final int tabId) {
        if (getTabModelSelector() == null || getActiveLayout() == null) return;

        TabModelSelector selector = getTabModelSelector();
        Tab tab = selector.getTabById(tabId);
        if (tab == null) return;

        LayoutTab layoutTab = mTabCache.get(tabId);
        if (layoutTab == null) return;

        GURL url = tab.getUrl();
        boolean isNativePage =
                tab.isNativePage() || url.getScheme().equals(UrlConstants.CHROME_NATIVE_SCHEME);

        boolean canUseLiveTexture =
                tab.getWebContents() != null
                        && !SadTab.isShowing(tab)
                        && !isNativePage
                        && !tab.isHidden();

        TopUiThemeColorProvider topUiTheme = mTopUiThemeColorProvider.get();
        layoutTab.initFromHost(
                topUiTheme.getBackgroundColor(tab),
                shouldStall(tab),
                canUseLiveTexture,
                topUiTheme.getSceneLayerBackground(tab),
                ThemeUtils.getTextBoxColorForToolbarBackground(
                        mContext, tab, topUiTheme.calculateColor(tab, tab.getThemeColor())));

        mHost.requestRender();
    }

    private void updateLayoutTabBackgroundColor(final int tabId) {
        if (getTabModelSelector() == null || getActiveLayout() == null) return;

        TabModelSelector selector = getTabModelSelector();
        Tab tab = selector.getTabById(tabId);
        if (tab == null) return;

        LayoutTab layoutTab = mTabCache.get(tabId);
        if (layoutTab == null) return;

        layoutTab.set(
                LayoutTab.BACKGROUND_COLOR, mTopUiThemeColorProvider.get().getBackgroundColor(tab));
    }

    // Whether the tab is ready to display or it should be faded in as it loads.
    private static boolean shouldStall(Tab tab) {
        return (tab.isFrozen() || tab.needsReload()) && !tab.isNativePage();
    }

    @Override
    public LayoutTab createLayoutTab(
            int id, boolean incognito, float maxContentWidth, float maxContentHeight) {
        LayoutTab tab = mTabCache.get(id);
        if (tab == null) {
            tab = new LayoutTab(id, incognito, mHost.getWidth(), mHost.getHeight());
            mTabCache.put(id, tab);
        } else {
            tab.init(mHost.getWidth(), mHost.getHeight());
        }
        if (maxContentWidth > 0.f) tab.setMaxContentWidth(maxContentWidth);
        if (maxContentHeight > 0.f) tab.setMaxContentHeight(maxContentHeight);

        return tab;
    }

    @Override
    public void releaseTabLayout(int id) {
        mTabCache.remove(id);
    }

    @Override
    public void releaseResourcesForTab(int tabId) {}

    /**
     * @return The {@link TabModelSelector} instance this class knows about.
     */
    protected TabModelSelector getTabModelSelector() {
        return mTabModelSelector;
    }

    /**
     * @return The next {@link Layout} that will be shown.  If no {@link Layout} has been set
     *         since the last time {@link #startShowing(Layout, boolean)} was called, this will be
     *         {@link #getDefaultLayout()}.
     */
    protected Layout getNextLayout() {
        return mNextActiveLayout != null ? mNextActiveLayout : getDefaultLayout();
    }

    /** @return Whether a next layout has been explicitly specified. */
    protected boolean hasExplicitNextLayout() {
        return mNextActiveLayout != null;
    }

    @Override
    public Layout getActiveLayout() {
        return mActiveLayout;
    }

    @Override
    public void getViewportPixel(RectF rect) {
        if (getActiveLayout() == null) {
            mHost.getWindowViewport(rect);
            return;
        }

        switch (getActiveLayout().getViewportMode()) {
            case Layout.ViewportMode.ALWAYS_FULLSCREEN:
                mHost.getWindowViewport(rect);
                break;
            case Layout.ViewportMode.ALWAYS_SHOWING_BROWSER_CONTROLS:
                mHost.getViewportFullControls(rect);
                break;
            case Layout.ViewportMode.USE_PREVIOUS_BROWSER_CONTROLS_STATE:
                if (mPreviousLayoutShowingToolbar) {
                    mHost.getViewportFullControls(rect);
                } else {
                    mHost.getWindowViewport(rect);
                }
                break;
            case Layout.ViewportMode.DYNAMIC_BROWSER_CONTROLS:
            default:
                mHost.getVisibleViewport(rect);
        }
    }

    @Override
    public BrowserControlsManager getBrowserControlsManager() {
        return mHost != null ? mHost.getBrowserControlsManager() : null;
    }

    @Override
    public void requestUpdate() {
        requestUpdate(null);
    }

    @Override
    public void requestUpdate(Runnable onUpdateEffective) {
        if (mUpdateRequested && onUpdateEffective == null) return;
        mHost.requestRender(onUpdateEffective);
        mUpdateRequested = true;
    }

    @Override
    public void startHiding() {
        requestUpdate();
        Layout layoutBeingHidden = getActiveLayout();
        for (LayoutStateObserver observer : mLayoutObservers) {
            observer.onStartedHiding(layoutBeingHidden.getLayoutType());
        }
    }

    @Override
    public void doneHiding() {
        // TODO: If next layout is default layout clear caches (should this be a sub layout thing?)

        assert mNextActiveLayout != null : "Need to have a next active layout.";
        if (mNextActiveLayout != null) {
            // Notify LayoutObservers the active layout is finished hiding.
            for (LayoutStateObserver observer : mLayoutObservers) {
                observer.onFinishedHiding(getActiveLayout().getLayoutType());
            }

            startShowing(mNextActiveLayout, mAnimateNextLayout);
        }
    }

    @Override
    public void doneShowing() {
        if (mShowingEventSequencer != null) {
            mShowingEventSequencer.setPendingDoneShowing();
            return;
        }

        // Notify LayoutObservers the active layout is finished showing.
        for (LayoutStateObserver observer : mLayoutObservers) {
            observer.onFinishedShowing(getActiveLayout().getLayoutType());
        }
    }

    @Override
    public void showLayout(int layoutType, boolean animate) {
        Layout activeLayout = getActiveLayout();
        if (activeLayout != null && !activeLayout.isStartingToHide()) {
            setNextLayout(getLayoutForType(layoutType), animate);
            activeLayout.startHiding();
        } else {
            startShowing(getLayoutForType(layoutType), animate);
        }
    }

    /**
     * @param layoutType A layout type to get the implementation for.
     * @return The layout implementation for the provided type.
     */
    protected Layout getLayoutForType(@LayoutType int layoutType) {
        // TODO(crbug.com/40790324): Register these types and look them up in a map rather than
        // overriding this
        //                method in multiple places.
        // Use the static layout by default or if explicitly specified.
        if (layoutType == LayoutType.NONE || layoutType == LayoutType.BROWSING) {
            return mStaticLayout;
        }

        assert false : "Unsupported layout type: " + layoutType;
        return null;
    }

    /**
     * Should be called by control logic to show a new {@link Layout}.
     *
     * <p>TODO(dtrainor, clholgat): Clean up the show logic to guarantee startHiding/doneHiding get
     * called.
     *
     * @param layout The new {@link Layout} to show.
     * @param animate Whether or not {@code layout} should animate as it shows.
     */
    protected void startShowing(Layout layout, boolean animate) {
        assert layout != null : "Can't show a null layout.";

        // Set the new layout
        setNextLayout(null, true);
        Layout oldLayout = getActiveLayout();
        if (oldLayout != layout) {
            if (oldLayout != null) {
                oldLayout.forceAnimationToFinish();
                oldLayout.detachViews();

                // TODO(crbug.com/40141330): hide oldLayout if it's not hidden.
            }
            layout.contextChanged(mHost.getContext());
            layout.attachViews(mContentContainer);
            mActiveLayout = layout;
        }

        BrowserControlsVisibilityManager controlsVisibilityManager =
                mHost.getBrowserControlsManager();
        if (controlsVisibilityManager != null) {
            mPreviousLayoutShowingToolbar =
                    !BrowserControlsUtils.areBrowserControlsOffScreen(controlsVisibilityManager);

            // Release any old fullscreen token we were holding.
            controlsVisibilityManager
                    .getBrowserVisibilityDelegate()
                    .releasePersistentShowingToken(mControlsShowingToken);

            // Grab a new fullscreen token if this layout can't be in fullscreen.
            if (getActiveLayout().forceShowBrowserControlsAndroidView()) {
                mControlsShowingToken =
                        controlsVisibilityManager
                                .getBrowserVisibilityDelegate()
                                .showControlsPersistent();
            }
        }

        onViewportChanged();

        // In order to prevent another state transition in the middle of processing this one,
        // scopedSequencer will add itself as a member of this class, and then remove itself once
        // its scope is closed.
        try (ShowingEventSequencer scopedSequencer = new ShowingEventSequencer()) {
            getActiveLayout().show(time(), animate);
            mHost.setContentOverlayVisibility(
                    getActiveLayout().shouldDisplayContentOverlay(),
                    getActiveLayout().canHostBeFocusable());
            requestUpdate();

            // TODO(crbug.com/40141330): Remove after migrates to
            // LayoutStateObserver#onStartedShowing. Notify observers about the new scene.
            for (SceneChangeObserver observer : mSceneChangeObservers) {
                observer.onSceneChange(getActiveLayout());
            }

            for (LayoutStateObserver observer : mLayoutObservers) {
                observer.onStartedShowing(layout.getLayoutType());
            }
        }
    }

    /**
     * Sets the next {@link Layout} to show after the current {@link Layout} is finished and is done
     * hiding.
     * @param layout The new {@link Layout} to show.
     * @param animate Whether the next layout should be animated.
     */
    protected void setNextLayout(Layout layout, boolean animate) {
        mNextActiveLayout = (layout == null) ? getDefaultLayout() : layout;
        mAnimateNextLayout = animate;
    }

    @Override
    public @LayoutType int getNextLayoutType() {
        return mNextActiveLayout != null ? mNextActiveLayout.getLayoutType() : LayoutType.NONE;
    }

    @Override
    public boolean isActiveLayout(Layout layout) {
        return layout == mActiveLayout;
    }

    @Override
    public int getActiveLayoutType() {
        return getActiveLayout() != null ? getActiveLayout().getLayoutType() : LayoutType.NONE;
    }

    /**
     * Get a list of virtual views for accessibility.
     *
     * @param views A List to populate with virtual views.
     */
    public void getVirtualViews(List<VirtualView> views) {
        // Nothing to do here yet.
    }

    /**
     * @return The {@link SwipeHandler} responsible for processing swipe events for the normal
     *         toolbar. By default this returns null.
     */
    public SwipeHandler getToolbarSwipeHandler() {
        return null;
    }

    /**
     * Creates a {@link SwipeHandler} instance.
     * @param supportSwipeDown Whether or not to the handler should support swipe down gesture.
     * @return The {@link SwipeHandler} cerated.
     */
    public SwipeHandler createToolbarSwipeHandler(boolean supportSwipeDown) {
        return null;
    }

    /**
     * Should be called when the user presses the back button on the phone.
     * @return Whether or not the back button was consumed by the active {@link Layout}.
     */
    public boolean onBackPressed() {
        for (int i = 0; i < mSceneOverlays.size(); i++) {
            if (!mSceneOverlays.get(i).isSceneOverlayTreeShowing()) continue;

            // If the back button was consumed by any overlays, return true.
            if (mSceneOverlays.get(i).onBackPressed()) {
                BackPressManager.record(BackPressHandler.Type.SCENE_OVERLAY);
                return true;
            }
        }
        // Back press metrics of active layout is recorded by their implementations.
        return getActiveLayout() != null && getActiveLayout().onBackPressed();
    }

    @Override
    public @BackPressResult int handleBackPress() {
        for (SceneOverlay sceneOverlay : mSceneOverlays) {
            Boolean enabled = sceneOverlay.getHandleBackPressChangedSupplier().get();
            if (enabled != null && enabled) {
                return sceneOverlay.handleBackPress();
            }
        }
        return BackPressResult.FAILURE;
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mHandleBackPressChangedSupplier;
    }

    private void onBackPressStateChanged() {
        for (SceneOverlay sceneOverlay : mSceneOverlays) {
            Boolean enabled = sceneOverlay.getHandleBackPressChangedSupplier().get();
            if (enabled != null && enabled) {
                mHandleBackPressChangedSupplier.set(true);
                return;
            }
        }
        mHandleBackPressChangedSupplier.set(false);
    }

    @Override
    public void addSceneOverlay(SceneOverlay overlay) {
        if (mSceneOverlays.contains(overlay)) throw new RuntimeException("Overlay already added!");

        if (!mOverlayOrderMap.containsKey(overlay.getClass())) {
            throw new RuntimeException("Please add overlay to order list in constructor.");
        }

        int overlayPosition = mOverlayOrderMap.get(overlay.getClass());

        int index;
        for (index = 0; index < mSceneOverlays.size(); index++) {
            if (overlayPosition < mOverlayOrderMap.get(mSceneOverlays.get(index).getClass())) break;
        }

        mSceneOverlays.add(index, overlay);
        overlay.getHandleBackPressChangedSupplier().addObserver((v) -> onBackPressStateChanged());
    }

    void setSceneOverlayOrderForTesting(Map<Class, Integer> order) {
        mOverlayOrderMap = order;
    }

    List<SceneOverlay> getSceneOverlaysForTesting() {
        return mSceneOverlays;
    }

    /**
     * Clears all content associated with {@code tabId} from the internal caches.
     * @param tabId The id of the tab to clear.
     */
    protected void emptyTabCachesExcept(int tabId) {
        LayoutTab tab = mTabCache.get(tabId);
        mTabCache.clear();
        if (tab != null) mTabCache.put(tabId, tab);
    }

    private @Orientation int getOrientation() {
        return mHost.getWidth() > mHost.getHeight() ? Orientation.LANDSCAPE : Orientation.PORTRAIT;
    }

    public LayoutTab getLayoutTabForTesting(int tabId) {
        return mTabCache.get(tabId);
    }

    /**
     * Should be called when a tab switch event is triggered, only can switch to the Tab which in
     * the current TabModel.
     * @param tab        The tab that will be switched to.
     * @param lastTabId  The id of the tab that was switched from.
     */
    protected void switchToTab(Tab tab, int lastTabId) {
        tabSelected(tab.getId(), lastTabId, tab.isIncognito());
    }

    // LayoutStateProvider implementation.
    @Override
    public boolean isLayoutVisible(int layoutType) {
        return getActiveLayout() != null && getActiveLayout().getLayoutType() == layoutType;
    }

    @Override
    public boolean isLayoutStartingToHide(int layoutType) {
        return isLayoutVisible(layoutType) && getActiveLayout().isStartingToHide();
    }

    @Override
    public boolean isLayoutStartingToShow(int layoutType) {
        return isLayoutVisible(layoutType) && getActiveLayout().isStartingToShow();
    }

    @Override
    public void addObserver(LayoutStateObserver listener) {
        mLayoutObservers.addObserver(listener);
    }

    @Override
    public void removeObserver(LayoutStateObserver listener) {
        mLayoutObservers.removeObserver(listener);
    }
}
