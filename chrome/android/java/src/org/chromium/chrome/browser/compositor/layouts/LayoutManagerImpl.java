// Copyright 2015 The Chromium Authors. All rights reserved.
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

import org.chromium.base.ObserverList;
import org.chromium.base.TraceEvent;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsUtils;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel;
import org.chromium.chrome.browser.compositor.layouts.Layout.Orientation;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.continuous_search.ContinuousSearchContainerCoordinator;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.gesturenav.HistoryNavigationCoordinator;
import org.chromium.chrome.browser.layouts.CompositorModelChangeProcessor;
import org.chromium.chrome.browser.layouts.EventFilter;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.ManagedLayoutManager;
import org.chromium.chrome.browser.layouts.SceneOverlay;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.layouts.scene_layer.SceneOverlayLayer;
import org.chromium.chrome.browser.status_indicator.StatusIndicatorCoordinator;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.chrome.browser.toolbar.bottom.ScrollingBottomViewSceneLayer;
import org.chromium.chrome.browser.toolbar.top.TopToolbarOverlayCoordinator;
import org.chromium.chrome.browser.ui.native_page.NativePage;
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
public class LayoutManagerImpl implements ManagedLayoutManager, LayoutUpdateHost, LayoutProvider,
                                          TabModelSelector.CloseAllTabsDelegate {
    /** Sampling at 60 fps. */
    private static final long FRAME_DELTA_TIME_MS = 16;

    /** Used to convert pixels to dp. */
    protected final float mPxToDp;

    /** The {@link LayoutManagerHost}, who is responsible for showing the active {@link Layout}. */
    protected final LayoutManagerHost mHost;

    /**
     * A means of notifying features that the browser controls' android view is being forced to
     * hide.
     */
    private final ObservableSupplierImpl<Boolean> mAndroidViewShownSupplier;

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

    private TabModelSelectorObserver mTabModelSelectorObserver;
    private TabModelSelectorTabObserver mTabModelSelectorTabObserver;

    // An observer for watching TabModelFilters changes events.
    private TabModelObserver mTabModelFilterObserver;

    // External Observers
    private final ObserverList<LayoutStateObserver> mLayoutObservers = new ObserverList<>();
    // TODO(crbug.com/1108496): Remove after all SceneChangeObserver migrates to
    // LayoutStateObserver.
    private final ObserverList<SceneChangeObserver> mSceneChangeObservers = new ObserverList<>();

    // Current Layout State
    private Layout mActiveLayout;
    private Layout mNextActiveLayout;

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
    private final ObservableSupplierImpl<BrowserControlsStateProvider>
            mBrowserControlsStateProviderSupplier = new ObservableSupplierImpl<>();
    private final CompositorModelChangeProcessor.FrameRequestSupplier mFrameRequestSupplier;

    /** The overlays that can be drawn on top of the active layout. */
    protected final List<SceneOverlay> mSceneOverlays = new ArrayList<>();

    /** A map of {@link SceneOverlay} to its position relative to the others. */
    private Map<Class, Integer> mOverlayOrderMap = new HashMap<>();

    /** The supplier used to supply the LayoutStateProvider. */
    private final OneshotSupplierImpl<LayoutStateProvider> mLayoutStateProviderOneshotSupplier;

    /** The supplier of {@link ThemeColorProvider} for top UI. */
    private final Supplier<TopUiThemeColorProvider> mTopUiThemeColorProvider;

    /** A cache of title textures to use in different layouts. */
    protected Supplier<LayerTitleCache> mLayerTitleCacheSupplier;

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
            if (type == TabLaunchType.FROM_RESTORE || type == TabLaunchType.FROM_REPARENTING
                    || type == TabLaunchType.FROM_EXTERNAL_APP
                    || type == TabLaunchType.FROM_LAUNCHER_SHORTCUT
                    || type == TabLaunchType.FROM_STARTUP) {
                return;
            }

            tabCreating(
                    getTabModelSelector().getCurrentTabId(), tab.getUrlString(), tab.isIncognito());
        }

        @Override
        public void didAddTab(
                Tab tab, @TabLaunchType int launchType, @TabCreationState int creationState) {
            int tabId = tab.getId();
            if (launchType == TabLaunchType.FROM_RESTORE) {
                getActiveLayout().onTabRestored(time(), tabId);
            } else {
                boolean incognito = tab.isIncognito();
                boolean willBeSelected = launchType != TabLaunchType.FROM_LONGPRESS_BACKGROUND
                                && launchType != TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP
                        || (!getTabModelSelector().isIncognitoSelected() && incognito);
                float lastTapX = LocalizationUtils.isLayoutRtl() ? mHost.getWidth() * mPxToDp : 0.f;
                float lastTapY = 0.f;
                if (launchType != TabLaunchType.FROM_CHROME_UI) {
                    float heightDelta = mHost.getHeightMinusBrowserControls() * mPxToDp;
                    lastTapX = mPxToDp * mLastTapX;
                    lastTapY = mPxToDp * mLastTapY - heightDelta;
                }

                tabCreated(tabId, getTabModelSelector().getCurrentTabId(), launchType, incognito,
                        willBeSelected, lastTapX, lastTapY);
            }
        }

        @Override
        public void didCloseTab(int tabId, boolean incognito) {
            tabClosed(tabId, incognito, false);
        }

        @Override
        public void tabPendingClosure(Tab tab) {
            tabClosed(tab.getId(), tab.isIncognito(), false);
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
     * Creates a {@link LayoutManagerImpl} instance.
     * @param host A {@link LayoutManagerHost} instance.
     * @param contentContainer A {@link ViewGroup} for Android views to be bound to.
     * @param tabContentManagerSupplier Supplier of the {@link TabContentManager} instance.
     * @param layerTitleCacheSupplier A supplier of the cache of title textures.
     * @param layoutStateProviderOneshotSupplier Supplier used to supply the {@link
     *         LayoutStateProvider}.
     * @param topUiThemeColorProvider {@link ThemeColorProvider} for top UI.
     */
    public LayoutManagerImpl(LayoutManagerHost host, ViewGroup contentContainer,
            ObservableSupplier<TabContentManager> tabContentManagerSupplier,
            Supplier<LayerTitleCache> layerTitleCacheSupplier,
            OneshotSupplierImpl<LayoutStateProvider> layoutStateProviderOneshotSupplier,
            Supplier<TopUiThemeColorProvider> topUiThemeColorProvider) {
        mHost = host;
        mPxToDp = 1.f / mHost.getContext().getResources().getDisplayMetrics().density;
        mAndroidViewShownSupplier = new ObservableSupplierImpl<>();
        mAndroidViewShownSupplier.set(true);
        mTabContentManagerSupplier = tabContentManagerSupplier;
        mLayoutStateProviderOneshotSupplier = layoutStateProviderOneshotSupplier;
        mLayerTitleCacheSupplier = layerTitleCacheSupplier;
        mTopUiThemeColorProvider = topUiThemeColorProvider;
        mContext = host.getContext();
        LayoutRenderHost renderHost = host.getLayoutRenderHost();

        // clang-format off
        // Overlays are ordered back (closest to the web content) to front.
        Class[] overlayOrder = new Class[] {
                HistoryNavigationCoordinator.getSceneOverlayClass(),
                ContinuousSearchContainerCoordinator.getSceneOverlayClass(),
                TopToolbarOverlayCoordinator.class,
                ScrollingBottomViewSceneLayer.class,
                StripLayoutHelperManager.class,
                StatusIndicatorCoordinator.getSceneOverlayClass(),
                ContextualSearchPanel.class};
        // clang-format on

        for (int i = 0; i < overlayOrder.length; i++) mOverlayOrderMap.put(overlayOrder[i], i);

        assert contentContainer != null;
        mContentContainer = contentContainer;

        mAnimationHandler = new CompositorAnimationHandler(this::requestUpdate);

        mOverlayPanelManager = new OverlayPanelManager();

        mFrameRequestSupplier =
                new CompositorModelChangeProcessor.FrameRequestSupplier(this::requestUpdate);

        mLayoutStateProviderOneshotSupplier.set(this);
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
     * Gives the {@link LayoutManagerImpl} a chance to intercept and process touch events from the
     * Android {@link View} system.
     * @param e                 The {@link MotionEvent} that might be intercepted.
     * @param isKeyboardShowing Whether or not the keyboard is showing.
     * @return                  Whether or not this current touch gesture should be intercepted and
     *                          continually forwarded to this class.
     */
    public boolean onInterceptTouchEvent(MotionEvent e, boolean isKeyboardShowing) {
        if (mActiveLayout == null) return false;

        if (e.getAction() == MotionEvent.ACTION_DOWN) {
            mLastTapX = (int) e.getX();
            mLastTapY = (int) e.getY();
        }

        PointF offsets = getMotionOffsets(e);

        // The last added overlay will be drawn on top of everything else, therefore the last
        // filter added should have the first chance to intercept any touch events.
        EventFilter layoutFilter = null;
        for (int i = mSceneOverlays.size() - 1; i >= 0; i--) {
            if (!mSceneOverlays.get(i).isSceneOverlayTreeShowing()) continue;
            EventFilter eventFilter = mSceneOverlays.get(i).getEventFilter();
            if (eventFilter == null) continue;
            if (offsets != null) eventFilter.setCurrentMotionEventOffsets(offsets.x, offsets.y);
            if (eventFilter.onInterceptTouchEvent(e, isKeyboardShowing)) {
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

        // TODO(mdjones): Remove the time related params from this method. The new animation system
        // has its own timer.
        boolean areAnimatorsComplete = mAnimationHandler.pushUpdate();

        // TODO(crbug.com/1070281): Remove after the FrameRequestSupplier migrates to the animation
        //  system.
        final Layout layout = getActiveLayout();

        // TODO(crbug.com/1070281): Layout itself should decide when it's done hiding and done
        //  showing.
        if (layout != null && layout.onUpdate(timeMs, dtMs) && areAnimatorsComplete) {
            if (layout.isStartingToHide()) {
                layout.doneHiding();
            } else if (layout.isStartingToShow()) {
                layout.doneShowing();
            }
        }

        // TODO(1100332): Once overlays are MVC, this should no longer be needed.
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
     */
    public void init(TabModelSelector selector, TabCreatorManager creator,
            @Nullable ControlContainer controlContainer,
            DynamicResourceLoader dynamicResourceLoader) {
        LayoutRenderHost renderHost = mHost.getLayoutRenderHost();

        // Build Layouts
        mStaticLayout = new StaticLayout(mContext, this, renderHost, mHost, mFrameRequestSupplier,
                selector, mTabContentManagerSupplier.get(), mBrowserControlsStateProviderSupplier,
                mTopUiThemeColorProvider);

        // Set up layout parameters
        mStaticLayout.setLayoutHandlesTabLifecycles(true);

        setNextLayout(null);

        // Initialize Layouts
        mStaticLayout.onFinishNativeInitialization();

        // Initialize Layouts
        mBrowserControlsStateProviderSupplier.set(mHost.getBrowserControlsManager());

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
        mTabModelSelectorTabObserver = new TabModelSelectorTabObserver(mTabModelSelector) {
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
                initLayoutTabFromHost(tab.getId());
            }

            @Override
            public void onDidChangeThemeColor(Tab tab, int color) {
                initLayoutTabFromHost(tab.getId());
            }
        };

        if (mNextActiveLayout != null) startShowing(mNextActiveLayout, true);

        mTabModelSelectorObserver = new TabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                tabModelSwitched(newModel.isIncognito());
            }
        };
        selector.addObserver(mTabModelSelectorObserver);
        selector.setCloseAllTabsDelegate(this);

        mTabModelFilterObserver = createTabModelObserver();
        getTabModelSelector().getTabModelFilterProvider().addTabModelFilterObserver(
                mTabModelFilterObserver);
    }

    @Override
    public void destroy() {
        mAnimationHandler.destroy();
        mSceneChangeObservers.clear();
        if (mStaticLayout != null) mStaticLayout.destroy();
        if (mOverlayPanelManager != null) mOverlayPanelManager.destroy();
        if (mTabModelSelectorTabObserver != null) mTabModelSelectorTabObserver.destroy();
        if (mTabModelSelectorObserver != null) {
            getTabModelSelector().removeObserver(mTabModelSelectorObserver);
        }
        if (mTabModelFilterObserver != null) {
            getTabModelSelector().getTabModelFilterProvider().removeTabModelFilterObserver(
                    mTabModelFilterObserver);
        }
    }

    /** @return A resource manager to pull textures from. */
    public ResourceManager getResourceManager() {
        if (mHost.getLayoutRenderHost() == null) return null;
        return mHost.getLayoutRenderHost().getResourceManager();
    }

    @Override
    public <V extends SceneLayer> CompositorModelChangeProcessor<V> createCompositorMCP(
            PropertyModel model, V view,
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
    public SceneLayer getUpdatedActiveSceneLayer(TabContentManager tabContentManager,
            ResourceManager resourceManager, BrowserControlsManager browserControlsManager) {
        updateControlsHidingState(browserControlsManager);
        getViewportPixel(mCachedVisibleViewport);
        mHost.getWindowViewport(mCachedWindowViewport);
        SceneLayer layer = mActiveLayout.getUpdatedSceneLayer(mCachedWindowViewport,
                mCachedVisibleViewport, mLayerTitleCacheSupplier.get(), tabContentManager,
                resourceManager, browserControlsManager);

        float offsetPx = mBrowserControlsStateProviderSupplier.get() == null
                ? 0
                : mBrowserControlsStateProviderSupplier.get().getTopControlOffset();

        for (int i = 0; i < mSceneOverlays.size(); i++) {
            // If the SceneOverlay is not showing, don't bother adding it to the tree.
            if (!mSceneOverlays.get(i).isSceneOverlayTreeShowing()) continue;

            SceneOverlayLayer overlayLayer =
                    mSceneOverlays.get(i).getUpdatedSceneOverlayTree(mCachedWindowViewport,
                            mCachedVisibleViewport, resourceManager, offsetPx * mPxToDp);

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
            mControlsHidingToken = controlsVisibilityManager.hideAndroidControlsAndClearOldToken(
                    mControlsHidingToken);
            mAndroidViewShownSupplier.set(false);
        } else {
            controlsVisibilityManager.releaseAndroidControlsHidingToken(mControlsHidingToken);
            mAndroidViewShownSupplier.set(true);
        }
    }

    /**
     * Called when the viewport has been changed.
     */
    public void onViewportChanged() {
        if (getActiveLayout() != null) {
            float previousWidth = getActiveLayout().getWidth();
            float previousHeight = getActiveLayout().getHeight();

            float oldViewportTop = mCachedWindowViewport.top;
            mHost.getWindowViewport(mCachedWindowViewport);
            mHost.getVisibleViewport(mCachedVisibleViewport);
            getActiveLayout().sizeChanged(mCachedVisibleViewport, mCachedWindowViewport,
                    mHost.getTopControlsHeightPixels(), mHost.getBottomControlsHeightPixels(),
                    getOrientation());

            float width = mCachedWindowViewport.width() * mPxToDp;
            float height = mCachedWindowViewport.height() * mPxToDp;
            if (width != previousWidth || height != previousHeight
                    || oldViewportTop != mCachedVisibleViewport.top) {
                for (int i = 0; i < mSceneOverlays.size(); i++) {
                    mSceneOverlays.get(i).onSizeChanged(
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
     * @param sourceId    The id of the creating tab if any.
     * @param url         The url of the created tab.
     * @param isIncognito Whether or not created tab will be incognito.
     */
    protected void tabCreating(int sourceId, String url, boolean isIncognito) {
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
    protected void tabCreated(int id, int sourceId, @TabLaunchType int launchType,
            boolean incognito, boolean willBeSelected, float originX, float originY) {
        int newIndex = TabModelUtils.getTabIndexById(getTabModelSelector().getModel(incognito), id);
        getActiveLayout().onTabCreated(
                time(), id, newIndex, sourceId, incognito, !willBeSelected, originX, originY);
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

    @Override
    public boolean closeAllTabsRequest(boolean incognito) {
        if (!getActiveLayout().handlesCloseAll()) return false;

        getActiveLayout().onTabsAllClosing(time(), incognito);
        return true;
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

        boolean canUseLiveTexture = tab.getWebContents() != null && !SadTab.isShowing(tab)
                && !isNativePage && !tab.isHidden();

        TopUiThemeColorProvider topUiTheme = mTopUiThemeColorProvider.get();
        layoutTab.initFromHost(topUiTheme.getBackgroundColor(tab), shouldStall(tab),
                canUseLiveTexture, topUiTheme.getSceneLayerBackground(tab),
                ThemeUtils.getTextBoxColorForToolbarBackground(mContext.getResources(), tab,
                        topUiTheme.calculateColor(tab, tab.getThemeColor())),
                topUiTheme.getTextBoxBackgroundAlpha(tab));

        mHost.requestRender();
    }

    // Whether the tab is ready to display or it should be faded in as it loads.
    private static boolean shouldStall(Tab tab) {
        return (tab.isFrozen() || tab.needsReload())
                && !NativePage.isNativePageUrl(tab.getUrlString(), tab.isIncognito());
    }

    @Override
    public LayoutTab createLayoutTab(int id, boolean incognito, boolean showCloseButton,
            boolean isTitleNeeded, float maxContentWidth, float maxContentHeight) {
        LayoutTab tab = mTabCache.get(id);
        if (tab == null) {
            tab = new LayoutTab(id, incognito, mHost.getWidth(), mHost.getHeight(), showCloseButton,
                    isTitleNeeded);
            mTabCache.put(id, tab);
        } else {
            tab.init(mHost.getWidth(), mHost.getHeight(), showCloseButton, isTitleNeeded);
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
    public void startHiding(int nextTabId, boolean hintAtTabSelection) {
        requestUpdate();
        if (hintAtTabSelection) {
            notifyObserversOnTabSelectionHinted(nextTabId);

            // TODO(crbug.com/1108496): Remove after migrates to LayoutStateObserver.
            for (SceneChangeObserver observer : mSceneChangeObservers) {
                observer.onTabSelectionHinted(nextTabId);
            }
        }

        Layout layoutBeingHidden = getActiveLayout();
        notifyObserversLayoutStartedHiding(layoutBeingHidden.getLayoutType(),
                shouldShowToolbarAnimationOnHide(layoutBeingHidden, nextTabId),
                shouldDelayHideAnimation(layoutBeingHidden));
    }

    @Override
    public void doneHiding() {
        // TODO: If next layout is default layout clear caches (should this be a sub layout thing?)

        assert mNextActiveLayout != null : "Need to have a next active layout.";
        if (mNextActiveLayout != null) {
            // Notify LayoutObservers the active layout is finished hiding.
            notifyObserversLayoutFinishedHiding(getActiveLayout().getLayoutType());

            startShowing(mNextActiveLayout, true);
        }
    }

    @Override
    public void doneShowing() {
        // Notify LayoutObservers the active layout is finished showing.
        notifyObserversLayoutFinishedShowing(getActiveLayout().getLayoutType());
    }

    /**
     * Should be called by control logic to show a new {@link Layout}.
     *
     * TODO(dtrainor, clholgat): Clean up the show logic to guarantee startHiding/doneHiding get
     * called.
     *
     * @param layout  The new {@link Layout} to show.
     * @param animate Whether or not {@code layout} should animate as it shows.
     */
    protected void startShowing(Layout layout, boolean animate) {
        assert layout != null : "Can't show a null layout.";

        // Set the new layout
        setNextLayout(null);
        Layout oldLayout = getActiveLayout();
        if (oldLayout != layout) {
            if (oldLayout != null) {
                oldLayout.forceAnimationToFinish();
                oldLayout.detachViews();

                // TODO(crbug.com/1108496): hide oldLayout if it's not hidden.
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
            controlsVisibilityManager.getBrowserVisibilityDelegate().releasePersistentShowingToken(
                    mControlsShowingToken);

            // Grab a new fullscreen token if this layout can't be in fullscreen.
            if (getActiveLayout().forceShowBrowserControlsAndroidView()) {
                mControlsShowingToken = controlsVisibilityManager.getBrowserVisibilityDelegate()
                                                .showControlsPersistent();
            }
        }

        onViewportChanged();
        getActiveLayout().show(time(), animate);
        mHost.setContentOverlayVisibility(getActiveLayout().shouldDisplayContentOverlay(),
                getActiveLayout().canHostBeFocusable());
        mHost.requestRender();

        // TODO(crbug.com/1108496): Remove after migrates to LayoutStateObserver#onStartedShowing.
        // Notify observers about the new scene.
        for (SceneChangeObserver observer : mSceneChangeObservers) {
            observer.onSceneChange(getActiveLayout());
        }

        notifyObserversLayoutStartedShowing(
                layout.getLayoutType(), shouldShowToolbarAnimationOnShow(animate));
    }

    /**
     * Sets the next {@link Layout} to show after the current {@link Layout} is finished and is done
     * hiding.
     * @param layout The new {@link Layout} to show.
     */
    public void setNextLayout(Layout layout) {
        mNextActiveLayout = (layout == null) ? getDefaultLayout() : layout;
    }

    @Override
    public boolean isActiveLayout(Layout layout) {
        return layout == mActiveLayout;
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
            if (mSceneOverlays.get(i).onBackPressed()) return true;
        }
        return getActiveLayout() != null && getActiveLayout().onBackPressed();
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
    }

    @VisibleForTesting
    void setSceneOverlayOrderForTesting(Map<Class, Integer> order) {
        mOverlayOrderMap = order;
    }

    @VisibleForTesting
    List<SceneOverlay> getSceneOverlaysForTesting() {
        return mSceneOverlays;
    }

    /**
     * Clears all content associated with {@code tabId} from the internal caches.
     * @param tabId The id of the tab to clear.
     */
    protected void emptyCachesExcept(int tabId) {
        LayoutTab tab = mTabCache.get(tabId);
        mTabCache.clear();
        if (tab != null) mTabCache.put(tabId, tab);
    }

    private @Orientation int getOrientation() {
        return mHost.getWidth() > mHost.getHeight() ? Orientation.LANDSCAPE : Orientation.PORTRAIT;
    }

    @VisibleForTesting
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
    public void addObserver(LayoutStateObserver listener) {
        mLayoutObservers.addObserver(listener);
    }

    @Override
    public void removeObserver(LayoutStateObserver listener) {
        mLayoutObservers.removeObserver(listener);
    }

    protected final void notifyObserversLayoutStartedShowing(
            @LayoutType int layoutType, boolean showToolbar) {
        mLayoutStateProviderOneshotSupplier.onAvailable((unused) -> {
            for (LayoutStateObserver observer : mLayoutObservers) {
                observer.onStartedShowing(layoutType, showToolbar);
            }
        });
    }

    protected final void notifyObserversLayoutFinishedShowing(@LayoutType int layoutType) {
        mLayoutStateProviderOneshotSupplier.onAvailable((unused) -> {
            for (LayoutStateObserver observer : mLayoutObservers) {
                observer.onFinishedShowing(layoutType);
            }
        });
    }

    protected final void notifyObserversLayoutStartedHiding(
            @LayoutType int layoutType, boolean showToolbar, boolean delayAnimation) {
        mLayoutStateProviderOneshotSupplier.onAvailable((unused) -> {
            for (LayoutStateObserver observer : mLayoutObservers) {
                observer.onStartedHiding(layoutType, showToolbar, delayAnimation);
            }
        });
    }

    protected final void notifyObserversLayoutFinishedHiding(@LayoutType int layoutType) {
        mLayoutStateProviderOneshotSupplier.onAvailable((unused) -> {
            for (LayoutStateObserver observer : mLayoutObservers) {
                observer.onFinishedHiding(layoutType);
            }
        });
    }

    protected final void notifyObserversOnTabSelectionHinted(int tabId) {
        mLayoutStateProviderOneshotSupplier.onAvailable((unused) -> {
            for (LayoutStateObserver observer : mLayoutObservers) {
                observer.onTabSelectionHinted(tabId);
            }
        });
    }

    protected boolean shouldShowToolbarAnimationOnShow(boolean isAnimate) {
        return false;
    }

    protected boolean shouldShowToolbarAnimationOnHide(Layout layoutBeingHidden, int nextTabId) {
        return false;
    }

    protected boolean shouldDelayHideAnimation(Layout layoutBeingHidden) {
        return false;
    }
}
