// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.ark.browser;

import android.content.Context;
import android.graphics.Color;
import android.graphics.PointF;
import android.graphics.RectF;
import android.os.SystemClock;
import android.util.SparseArray;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import com.ark.browser.tab.PageCacheManager;
import com.ark.browser.utils.ArkLogger;

import org.chromium.base.ObserverList;
import org.chromium.base.TraceEvent;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.Layout.Orientation;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutProvider;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.SceneChangeObserver;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.layouts.CompositorModelChangeProcessor;
import org.chromium.chrome.browser.layouts.EventFilter;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.ManagedLayoutManager;
import org.chromium.chrome.browser.layouts.SceneOverlay;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.layouts.scene_layer.SceneOverlayLayer;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.embedder_support.util.UrlConstants;
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
public class ArkLayoutManager implements ManagedLayoutManager, LayoutUpdateHost, LayoutProvider {

    private static final String TAG = "ArkLayoutManager";

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
    protected ArkStaticLayout mStaticLayout;

    // External Observers
    private final ObserverList<LayoutStateObserver> mLayoutObservers = new ObserverList<>();
    // TODO(crbug.com/1108496): Remove after all SceneChangeObserver migrates to
    // LayoutStateObserver.
    private final ObserverList<SceneChangeObserver> mSceneChangeObservers = new ObserverList<>();

    // Current Event Fitler State
    private EventFilter mActiveEventFilter;

    // Internal State
    private final SparseArray<LayoutTab> mTabCache = new SparseArray<>();
    private int mControlsHidingToken = TokenHolder.INVALID_TOKEN;
    private boolean mUpdateRequested;

    private final Context mContext;

    // Used to store the visible viewport and not create a new Rect object every frame.
    private final RectF mCachedVisibleViewport = new RectF();
    private final RectF mCachedWindowViewport = new RectF();

    private final RectF mCachedRect = new RectF();
    private final PointF mCachedPoint = new PointF();

    // Whether the currently active event filter has changed.
    private boolean mIsNewEventFilter;

    /** The animation handler responsible for updating all the browser compositor's animations. */
    private final CompositorAnimationHandler mAnimationHandler;

    private final CompositorModelChangeProcessor.FrameRequestSupplier mFrameRequestSupplier;

    private BrowserControlsStateProvider mBrowserControlsStateProvider;

    /** The overlays that can be drawn on top of the active layout. */
    protected final List<SceneOverlay> mSceneOverlays = new ArrayList<>();

    /** A map of {@link SceneOverlay} to its position relative to the others. */
    private final Map<Class<?>, Integer> mOverlayOrderMap = new HashMap<>();

    /**
     * Creates a {@link ArkLayoutManager} instance.
     * @param host A {@link LayoutManagerHost} instance.
     */
    public ArkLayoutManager(LayoutManagerHost host) {
        mHost = host;
        mPxToDp = 1.f / mHost.getContext().getResources().getDisplayMetrics().density;
        mContext = host.getContext();

        // clang-format off
        // Overlays are ordered back (closest to the web content) to front.
        Class[] overlayOrder = new Class[] {
                ContextualSearchPanel.class};
        // clang-format on

        for (int i = 0; i < overlayOrder.length; i++) mOverlayOrderMap.put(overlayOrder[i], i);

        mAnimationHandler = new CompositorAnimationHandler(this::requestUpdate);

        mFrameRequestSupplier =
                new CompositorModelChangeProcessor.FrameRequestSupplier(this::requestUpdate);
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
     * Gives the {@link ArkLayoutManager} a chance to intercept and process touch events from the
     * Android {@link View} system.
     * @param e                 The {@link MotionEvent} that might be intercepted.
     * @param isKeyboardShowing Whether or not the keyboard is showing.
     * @return                  Whether or not this current touch gesture should be intercepted and
     *                          continually forwarded to this class.
     */
    public boolean onInterceptTouchEvent(MotionEvent e, boolean isKeyboardShowing) {
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
            layoutFilter = mStaticLayout.findInterceptingEventFilter(e, offsets, isKeyboardShowing);
        }

        mIsNewEventFilter = layoutFilter != mActiveEventFilter;
        mActiveEventFilter = layoutFilter;

        if (mActiveEventFilter != null) mStaticLayout.unstallImmediately();

        return mActiveEventFilter != null;
    }

    /**
     * Gives the {@link ArkLayoutManager} a chance to process the touch events from the Android
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
        ArkLogger.e("ArkLayoutManager", "onUpdate");
        TraceEvent.begin("LayoutDriver:onUpdate");
        onUpdate(time(), FRAME_DELTA_TIME_MS);
        TraceEvent.end("LayoutDriver:onUpdate");
    }

    /**
     * Updates the state of the layout.
     * @param timeMs The time in milliseconds.
     * @param dtMs   The delta time since the last update in milliseconds.
     * @return       Whether or not the {@link ArkLayoutManager} needs more updates.
     */
    @VisibleForTesting
    boolean onUpdate(long timeMs, long dtMs) {
        ArkLogger.e("ArkLayoutManager", "onUpdate timeMs=" + timeMs
                + " dtMs=" + dtMs + " mUpdateRequested=" + mUpdateRequested);
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
     * Initializes the {@link ArkLayoutManager}.  Must be called before using this object.
     * @param dynamicResourceLoader    A {@link DynamicResourceLoader} instance.
     */
    public void init(TabContentManager tabContentManager, DynamicResourceLoader dynamicResourceLoader) {
        LayoutRenderHost renderHost = mHost.getLayoutRenderHost();

        mBrowserControlsStateProvider = mHost.getBrowserControlsManager();

        // Build Layouts
        mStaticLayout = new ArkStaticLayout(mContext, this, renderHost, mHost, mFrameRequestSupplier,
                tabContentManager);

        startShowing(mStaticLayout, false);
    }

    @Override
    public void destroy() {
        mAnimationHandler.destroy();
        mSceneChangeObservers.clear();
        if (mStaticLayout != null) mStaticLayout.destroy();
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

    public void onPageSelected(Tab tab) {
        if (tab == null) {
            return;
        }
        mStaticLayout.setStaticTab(tab);
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
        SceneLayer layer = mStaticLayout.getUpdatedSceneLayer(mCachedWindowViewport,
                mCachedVisibleViewport, tabContentManager, resourceManager, browserControlsManager);

        float offsetPx = mBrowserControlsStateProvider == null
                ? 0
                : mBrowserControlsStateProvider.getTopControlOffset();

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

        if (overlayHidesControls || mStaticLayout.forceHideBrowserControlsAndroidView()) {
            mControlsHidingToken = controlsVisibilityManager.hideAndroidControlsAndClearOldToken(
                    mControlsHidingToken);
        } else {
            controlsVisibilityManager.releaseAndroidControlsHidingToken(mControlsHidingToken);
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
     * @param isIncognito Whether or not created tab will be incognito.
     */
    protected void tabCreating(int sourceId, boolean isIncognito) {
        if (getActiveLayout() != null) getActiveLayout().onTabCreating(sourceId);
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

    @Override
    public void initLayoutTabFromHost(final int tabId) {
        ArkLogger.e(TAG, "initLayoutTabFromHost tabId=" + tabId + " getActiveLayout=" + getActiveLayout());
        if (getActiveLayout() == null) return;

        Tab tab = PageCacheManager.getInstance().findPage(tabId);
        ArkLogger.e(TAG, "initLayoutTabFromHost tab=" + tab);
        if (tab == null) return;

        LayoutTab layoutTab = mTabCache.get(tabId);
        ArkLogger.e(TAG, "initLayoutTabFromHost layoutTab=" + layoutTab);
        if (layoutTab == null) {
            layoutTab = createLayoutTab(tabId, false, -1, -1);
        }

        GURL url = tab.getUrl();
        boolean isNativePage =
                tab.isNativePage() || url.getScheme().equals(UrlConstants.CHROME_NATIVE_SCHEME);

        boolean canUseLiveTexture = tab.getWebContents() != null && !SadTab.isShowing(tab)
                && !isNativePage && !tab.isHidden();

        layoutTab.initFromHost(Color.WHITE, shouldStall(tab),
                canUseLiveTexture, tab.getThemeColor(),
                ThemeUtils.getTextBoxColorForToolbarBackground(
                        mContext, tab, Color.BLUE),
                0.5f);

        mStaticLayout.updateStaticTab(tab);

        mHost.requestRender();
    }

    // Whether the tab is ready to display or it should be faded in as it loads.
    private static boolean shouldStall(Tab tab) {
        return (tab.isFrozen() || tab.needsReload())
                && !NativePage.isNativePageUrl(tab.getUrl(), tab.isIncognito());
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

    @Override
    public Layout getActiveLayout() {
        return mStaticLayout;
    }

    @Override
    public void getViewportPixel(RectF rect) {
        mHost.getVisibleViewport(rect);
    }

    @Override
    public BrowserControlsManager getBrowserControlsManager() {
        return mHost != null ? mHost.getBrowserControlsManager() : null;
    }

    @Override
    public void requestUpdate() {
        ArkLogger.d(TAG, "requestUpdate");
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
            // TODO(crbug.com/1108496): Remove after migrates to LayoutStateObserver.
            for (SceneChangeObserver observer : mSceneChangeObservers) {
                observer.onTabSelectionHinted(nextTabId);
            }

            for (LayoutStateObserver observer : mLayoutObservers) {
                observer.onTabSelectionHinted(nextTabId);
            }
        }

        Layout layoutBeingHidden = getActiveLayout();
        for (LayoutStateObserver observer : mLayoutObservers) {
            observer.onStartedHiding(layoutBeingHidden.getLayoutType(),
                    shouldShowToolbarAnimationOnHide(layoutBeingHidden, nextTabId),
                    shouldDelayHideAnimation(layoutBeingHidden));
        }
    }

    @Override
    public void doneHiding() {
    }

    @Override
    public void doneShowing() {
        // Notify LayoutObservers the active layout is finished showing.
        for (LayoutStateObserver observer : mLayoutObservers) {
            observer.onFinishedShowing(getActiveLayout().getLayoutType());
        }
    }

    @Override
    public void showLayout(int layoutType, boolean animate) {
    }

    /**
     * @param layoutType A layout type to get the implementation for.
     * @return The layout implementation for the provided type.
     */
    protected Layout getLayoutForType(@LayoutType int layoutType) {
        return mStaticLayout;
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
        // This can happen in some cases where the start surface may not have been created yet.
        if (layout == null) return;

        onViewportChanged();
        getActiveLayout().show(time(), animate);
        mHost.setContentOverlayVisibility(getActiveLayout().shouldDisplayContentOverlay(),
                getActiveLayout().canHostBeFocusable());
        requestUpdate();

        // TODO(crbug.com/1108496): Remove after migrates to LayoutStateObserver#onStartedShowing.
        // Notify observers about the new scene.
        for (SceneChangeObserver observer : mSceneChangeObservers) {
            observer.onSceneChange(getActiveLayout());
        }

        for (LayoutStateObserver observer : mLayoutObservers) {
            observer.onStartedShowing(
                    layout.getLayoutType(), shouldShowToolbarAnimationOnShow(animate));
        }
    }

    @Override
    public boolean isActiveLayout(Layout layout) {
        return layout == mStaticLayout;
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
