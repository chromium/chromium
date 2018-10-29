// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import android.content.Context;
import android.graphics.PointF;
import android.graphics.RectF;
import android.os.Handler;
import android.os.SystemClock;
import android.util.SparseArray;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.ObserverList;
import org.chromium.base.TraceEvent;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelContentViewDelegate;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel;
import org.chromium.chrome.browser.compositor.layouts.Layout.Orientation;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.components.VirtualView;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EdgeSwipeHandler;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EventFilter;
import org.chromium.chrome.browser.compositor.overlays.SceneOverlay;
import org.chromium.chrome.browser.compositor.scene_layer.SceneLayer;
import org.chromium.chrome.browser.compositor.scene_layer.ToolbarSceneLayer;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManagementDelegate;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.TabHidingType;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.SPenSupport;
import org.chromium.ui.resources.ResourceManager;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

import java.util.List;

/**
 * A class that is responsible for managing an active {@link Layout} to show to the screen.  This
 * includes lifecycle managment like showing/hiding this {@link Layout}.
 */
public class LayoutManager implements LayoutUpdateHost, LayoutProvider,
                                      OverlayPanelContentViewDelegate,
                                      TabModelSelector.CloseAllTabsDelegate {
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
    protected final StaticLayout mStaticLayout;

    // External Dependencies
    private TabModelSelector mTabModelSelector;

    private TabModelObserver mTabModelObserver;
    private TabModelSelectorObserver mTabModelSelectorObserver;
    private TabModelSelectorTabObserver mTabModelSelectorTabObserver;
    private ViewGroup mContentContainer;

    // External Observers
    private final ObserverList<SceneChangeObserver> mSceneChangeObservers = new ObserverList<>();

    // Current Layout State
    private Layout mActiveLayout;
    private Layout mNextActiveLayout;

    // Current Event Fitler State
    private EventFilter mActiveEventFilter;

    // Internal State
    private final SparseArray<LayoutTab> mTabCache = new SparseArray<>();
    private int mControlsShowingToken = FullscreenManager.INVALID_TOKEN;
    private int mControlsHidingToken = FullscreenManager.INVALID_TOKEN;
    private boolean mUpdateRequested;
    private final ContextualSearchPanel mContextualSearchPanel;
    private final OverlayPanelManager mOverlayPanelManager;
    private final ToolbarSceneLayer mToolbarOverlay;

    /** A delegate for interacting with the Contextual Search manager. */
    protected ContextualSearchManagementDelegate mContextualSearchDelegate;

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

    /**
     * Protected class to handle {@link TabModelObserver} related tasks. Extending classes will
     * need to override any related calls to add new functionality */
    protected class LayoutManagerTabModelObserver extends EmptyTabModelObserver {
        @Override
        public void didSelectTab(Tab tab, @TabModel.TabSelectionType int type, int lastId) {
            if (tab.getId() != lastId) tabSelected(tab.getId(), lastId, tab.isIncognito());
        }

        @Override
        public void willAddTab(Tab tab, @TabModel.TabLaunchType int type) {
            // Open the new tab
            if (type == TabModel.TabLaunchType.FROM_RESTORE
                    || type == TabModel.TabLaunchType.FROM_REPARENTING
                    || type == TabModel.TabLaunchType.FROM_EXTERNAL_APP
                    || type == TabModel.TabLaunchType.FROM_LAUNCHER_SHORTCUT)
                return;

            tabCreating(getTabModelSelector().getCurrentTabId(), tab.getUrl(), tab.isIncognito());
        }

        @Override
        public void didAddTab(Tab tab, @TabModel.TabLaunchType int launchType) {
            int tabId = tab.getId();
            if (launchType == TabModel.TabLaunchType.FROM_RESTORE) {
                getActiveLayout().onTabRestored(time(), tabId);
            } else {
                boolean incognito = tab.isIncognito();
                boolean willBeSelected =
                        launchType != TabModel.TabLaunchType.FROM_LONGPRESS_BACKGROUND
                        || (!getTabModelSelector().isIncognitoSelected() && incognito);
                float lastTapX = LocalizationUtils.isLayoutRtl() ? mHost.getWidth() * mPxToDp : 0.f;
                float lastTapY = 0.f;
                if (launchType != TabModel.TabLaunchType.FROM_CHROME_UI) {
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
            LayoutManager.this.tabClosureCommitted(tab.getId(), tab.isIncognito());
        }

        @Override
        public void tabRemoved(Tab tab) {
            tabClosed(tab.getId(), tab.isIncognito(), true);
        }
    }

    /**
     * Creates a {@link LayoutManager} instance.
     * @param host A {@link LayoutManagerHost} instance.
     */
    public LayoutManager(LayoutManagerHost host) {
        mHost = host;
        mPxToDp = 1.f / mHost.getContext().getResources().getDisplayMetrics().density;

        mContext = host.getContext();
        LayoutRenderHost renderHost = host.getLayoutRenderHost();

        mAnimationHandler = new CompositorAnimationHandler(this);

        mToolbarOverlay = new ToolbarSceneLayer(mContext, this, renderHost);

        mOverlayPanelManager = new OverlayPanelManager();

        // Build Layouts
        mStaticLayout = new StaticLayout(mContext, this, renderHost, null, mOverlayPanelManager);

        // Contextual Search scene overlay.
        mContextualSearchPanel = new ContextualSearchPanel(mContext, this, mOverlayPanelManager);

        // Set up layout parameters
        mStaticLayout.setLayoutHandlesTabLifecycles(true);

        setNextLayout(null);
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
     * Gives the {@link LayoutManager} a chance to intercept and process touch events from the
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

        EventFilter layoutFilter =
                mActiveLayout.findInterceptingEventFilter(e, offsets, isKeyboardShowing);
        mIsNewEventFilter = layoutFilter != mActiveEventFilter;
        mActiveEventFilter = layoutFilter;

        if (mActiveEventFilter != null) mActiveLayout.unstallImmediately();

        return mActiveEventFilter != null;
    }

    /**
     * Gives the {@link LayoutManager} a chance to process the touch events from the Android
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
     * @return       Whether or not the {@link LayoutManager} needs more updates.
     */
    @VisibleForTesting
    public boolean onUpdate(long timeMs, long dtMs) {
        if (!mUpdateRequested) return false;
        mUpdateRequested = false;

        // TODO(mdjones): Remove the time related params from this method. The new animation system
        // has its own timer.
        boolean areAnimatorsComplete = mAnimationHandler.pushUpdate();

        final Layout layout = getActiveLayout();
        if (layout != null && layout.onUpdate(timeMs, dtMs) && layout.isHiding()
                && areAnimatorsComplete) {
            layout.doneHiding();
        }
        return mUpdateRequested;
    }

    /**
     * Initializes the {@link LayoutManager}.  Must be called before using this object.
     * @param selector                 A {@link TabModelSelector} instance.
     * @param creator                  A {@link TabCreatorManager} instance.
     * @param content                  A {@link TabContentManager} instance.
     * @param androidContentContainer  A {@link ViewGroup} for Android views to be bound to.
     * @param contextualSearchDelegate A {@link ContextualSearchManagementDelegate} instance.
     * @param dynamicResourceLoader    A {@link DynamicResourceLoader} instance.
     */
    public void init(TabModelSelector selector, TabCreatorManager creator,
            TabContentManager content, ViewGroup androidContentContainer,
            ContextualSearchManagementDelegate contextualSearchDelegate,
            DynamicResourceLoader dynamicResourceLoader) {
        // Add any SceneOverlays to a layout.
        addAllSceneOverlays();

        // Save state
        mContextualSearchDelegate = contextualSearchDelegate;

        // Initialize Layouts
        mStaticLayout.setTabModelSelector(selector, content);

        // Initialize Contextual Search Panel
        mContextualSearchPanel.setManagementDelegate(contextualSearchDelegate);

        // Set back flow communication
        if (contextualSearchDelegate != null) {
            contextualSearchDelegate.setContextualSearchPanel(mContextualSearchPanel);
        }

        // Set the dynamic resource loader for all overlay panels.
        mOverlayPanelManager.setDynamicResourceLoader(dynamicResourceLoader);
        mOverlayPanelManager.setContainerView(androidContentContainer);

        mTabModelSelector = selector;
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

        mContentContainer = androidContentContainer;

        if (mNextActiveLayout != null) startShowing(mNextActiveLayout, true);

        updateLayoutForTabModelSelector();

        mTabModelSelectorObserver = new EmptyTabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                tabModelSwitched(newModel.isIncognito());
            }
        };
        selector.addObserver(mTabModelSelectorObserver);
        selector.setCloseAllTabsDelegate(this);

        mTabModelObserver = createTabModelObserver();
        for (TabModel model : selector.getModels()) model.addObserver(mTabModelObserver);
    }

    /**
     * Cleans up and destroys this object.  It should not be used after this.
     */
    public void destroy() {
        mAnimationHandler.destroy();
        mSceneChangeObservers.clear();
        if (mStaticLayout != null) mStaticLayout.destroy();
        if (mOverlayPanelManager != null) mOverlayPanelManager.destroy();
        if (mTabModelSelectorTabObserver != null) mTabModelSelectorTabObserver.destroy();
        if (mTabModelSelectorObserver != null) {
            getTabModelSelector().removeObserver(mTabModelSelectorObserver);
        }
        if (mTabModelObserver != null) {
            for (TabModel model : getTabModelSelector().getModels()) {
                model.removeObserver(mTabModelObserver);
            }
        }
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
    public SceneLayer getUpdatedActiveSceneLayer(LayerTitleCache layerTitleCache,
            TabContentManager tabContentManager, ResourceManager resourceManager,
            ChromeFullscreenManager fullscreenManager) {
        updateControlsHidingState(fullscreenManager);
        getViewportPixel(mCachedVisibleViewport);
        mHost.getWindowViewport(mCachedWindowViewport);
        return mActiveLayout.getUpdatedSceneLayer(mCachedWindowViewport, mCachedVisibleViewport,
                layerTitleCache, tabContentManager, resourceManager, fullscreenManager);
    }

    private void updateControlsHidingState(ChromeFullscreenManager fullscreenManager) {
        if (fullscreenManager == null) {
            return;
        }
        if (mActiveLayout.forceHideBrowserControlsAndroidView()) {
            mControlsHidingToken =
                    fullscreenManager.hideAndroidControlsAndClearOldToken(mControlsHidingToken);
        } else {
            fullscreenManager.releaseAndroidControlsHidingToken(mControlsHidingToken);
        }
    }

    @Override
    public void releaseOverlayPanelContent() {
        if (getTabModelSelector() == null) return;
        Tab tab = getTabModelSelector().getCurrentTab();
        if (tab != null) tab.updateFullscreenEnabledState();
    }

    /**
     * Called when the viewport has been changed.
     */
    public void onViewportChanged() {
        if (getActiveLayout() != null) {
            mHost.getWindowViewport(mCachedWindowViewport);
            mHost.getVisibleViewport(mCachedVisibleViewport);
            getActiveLayout().sizeChanged(mCachedVisibleViewport, mCachedWindowViewport,
                    mHost.getTopControlsHeightPixels(), mHost.getBottomControlsHeightPixels(),
                    getOrientation());
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
    protected void tabCreated(int id, int sourceId, @TabModel.TabLaunchType int launchType,
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

        String url = tab.getUrl();
        boolean isNativePage = tab.isNativePage()
                || (url != null && url.startsWith(UrlConstants.CHROME_NATIVE_URL_PREFIX));
        int themeColor = tab.getThemeColor();

        boolean canUseLiveTexture = tab.getWebContents() != null && !SadTab.isShowing(tab)
                && !isNativePage && !tab.isHidden();

        boolean isNtp = tab.getNativePage() instanceof NewTabPage;
        boolean isLocationBarShownInNtp =
                isNtp ? ((NewTabPage) tab.getNativePage()).isLocationBarShownInNTP() : false;
        boolean needsUpdate = layoutTab.initFromHost(tab.getBackgroundColor(), tab.shouldStall(),
                canUseLiveTexture, themeColor,
                ColorUtils.getTextBoxColorForToolbarBackground(
                        mContext.getResources(), isLocationBarShownInNtp, themeColor),
                ColorUtils.getTextBoxAlphaForToolbarBackground(tab));
        if (needsUpdate) requestUpdate();

        mHost.requestRender();
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
    public ChromeFullscreenManager getFullscreenManager() {
        return mHost != null ? mHost.getFullscreenManager() : null;
    }

    @Override
    public void requestUpdate() {
        if (!mUpdateRequested) mHost.requestRender();
        mUpdateRequested = true;
    }

    @Override
    public void startHiding(int nextTabId, boolean hintAtTabSelection) {
        requestUpdate();
        if (hintAtTabSelection) {
            for (SceneChangeObserver observer : mSceneChangeObservers) {
                observer.onTabSelectionHinted(nextTabId);
            }
        }
    }

    @Override
    public void doneHiding() {
        // TODO: If next layout is default layout clear caches (should this be a sub layout thing?)

        assert mNextActiveLayout != null : "Need to have a next active layout.";
        if (mNextActiveLayout != null) startShowing(mNextActiveLayout, true);
    }

    @Override
    public void doneShowing() {}

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
        assert mTabModelSelector != null : "init() must be called first.";
        assert layout != null : "Can't show a null layout.";

        // Set the new layout
        setNextLayout(null);
        Layout oldLayout = getActiveLayout();
        if (oldLayout != layout) {
            if (oldLayout != null) {
                oldLayout.forceAnimationToFinish();
                oldLayout.detachViews();
            }
            // TODO(fhorschig): This might be removed as soon as keyboard replacements get triggered
            // by the normal keyboard hiding signals.
            for (SceneChangeObserver observer : mSceneChangeObservers) {
                observer.onSceneStartShowing(layout);
            }
            layout.contextChanged(mHost.getContext());
            layout.attachViews(mContentContainer);
            mActiveLayout = layout;
        }

        ChromeFullscreenManager fullscreenManager = mHost.getFullscreenManager();
        if (fullscreenManager != null) {
            mPreviousLayoutShowingToolbar = !fullscreenManager.areBrowserControlsOffScreen();

            // Release any old fullscreen token we were holding.
            fullscreenManager.getBrowserVisibilityDelegate().releasePersistentShowingToken(
                    mControlsShowingToken);

            // Grab a new fullscreen token if this layout can't be in fullscreen.
            if (getActiveLayout().forceShowBrowserControlsAndroidView()) {
                mControlsShowingToken =
                        fullscreenManager.getBrowserVisibilityDelegate().showControlsPersistent();
            }
        }

        onViewportChanged();
        getActiveLayout().show(time(), animate);
        mHost.setContentOverlayVisibility(getActiveLayout().shouldDisplayContentOverlay());
        mHost.requestRender();

        // Notify observers about the new scene.
        for (SceneChangeObserver observer : mSceneChangeObservers) {
            observer.onSceneChange(getActiveLayout());
        }
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
     * @return The {@link EdgeSwipeHandler} responsible for processing swipe events for the toolbar.
     *         By default this returns null.
     */
    public EdgeSwipeHandler getToolbarSwipeHandler() {
        return null;
    }

    /**
     * Should be called when the user presses the back button on the phone.
     * @return Whether or not the back button was consumed by the active {@link Layout}.
     */
    public boolean onBackPressed() {
        return getActiveLayout() != null && getActiveLayout().onBackPressed();
    }

    /**
     * Adds the {@link SceneOverlay} across all {@link Layout}s owned by this class.
     * @param helper A {@link SceneOverlay} instance.
     */
    protected void addGlobalSceneOverlay(SceneOverlay helper) {
        mStaticLayout.addSceneOverlay(helper);
    }

    /**
     * Add any {@link SceneOverlay}s to the layout. This can be used to add the overlays in a
     * particular order.
     * Classes that override this method should be careful about the order that
     * overlays are added and when super is called (i.e. cases where one overlay needs to be
     * on top of another positioned.
     */
    protected void addAllSceneOverlays() {
        addGlobalSceneOverlay(mToolbarOverlay);
        mStaticLayout.addSceneOverlay(mContextualSearchPanel);
    }

    /**
     * Add a {@link SceneOverlay} to the back of the list. This means the overlay will be drawn
     * first and therefore behind all other overlays currently in the list.
     * @param overlay The overlay to be added to the back of the list.
     */
    public void addSceneOverlayToBack(SceneOverlay overlay) {
        mStaticLayout.addSceneOverlayToBack(overlay);
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

    private int getOrientation() {
        return mHost.getWidth() > mHost.getHeight() ? Orientation.LANDSCAPE : Orientation.PORTRAIT;
    }

    /**
     * Updates the Layout for the state of the {@link TabModelSelector} after initialization.
     * If the TabModelSelector is not yet initialized when this function is called, a
     * {@link org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver} is created to
     * listen for when it is ready.
     */
    private void updateLayoutForTabModelSelector() {
        if (mTabModelSelector.isTabStateInitialized() && getActiveLayout() != null) {
            getActiveLayout().onTabStateInitialized();
        } else {
            mTabModelSelector.addObserver(new EmptyTabModelSelectorObserver() {
                @Override
                public void onTabStateInitialized() {
                    if (getActiveLayout() != null) getActiveLayout().onTabStateInitialized();

                    final EmptyTabModelSelectorObserver observer = this;
                    new Handler().post(new Runnable() {
                        @Override
                        public void run() {
                            mTabModelSelector.removeObserver(observer);
                        }
                    });
                }
            });
        }
    }
}
