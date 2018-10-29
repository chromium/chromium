// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.support.annotation.Nullable;
import android.support.annotation.Px;
import android.support.v4.view.ViewCompat;
import android.support.v4.view.accessibility.AccessibilityEventCompat;
import android.support.v4.view.accessibility.AccessibilityNodeInfoCompat;
import android.support.v4.widget.ExploreByTouchHelper;
import android.util.AttributeSet;
import android.util.Pair;
import android.view.DragEvent;
import android.view.MotionEvent;
import android.view.PointerIcon;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.widget.FrameLayout;

import org.chromium.base.SysUtils;
import org.chromium.base.TraceEvent;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.InsetObserverView;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardExtensionSizeManager;
import org.chromium.chrome.browser.compositor.Invalidator.Client;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.components.VirtualView;
import org.chromium.chrome.browser.compositor.layouts.content.ContentOffsetProvider;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManagementDelegate;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager.FullscreenListener;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.widget.ControlContainer;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.EventForwarder;
import org.chromium.ui.base.SPenSupport;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.resources.ResourceManager;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

import java.util.ArrayList;
import java.util.List;

/**
 * This class holds a {@link CompositorView}. This level of indirection is needed to benefit from
 * the {@link android.view.ViewGroup#onInterceptTouchEvent(android.view.MotionEvent)} capability on
 * available on {@link android.view.ViewGroup}s.
 * This class also holds the {@link LayoutManager} responsible to describe the items to be
 * drawn by the UI compositor on the native side.
 */
public class CompositorViewHolder extends FrameLayout
        implements ContentOffsetProvider, LayoutManagerHost, LayoutRenderHost, Invalidator.Host,
                   FullscreenListener, InsetObserverView.WindowInsetObserver,
                   KeyboardExtensionSizeManager.Observer {
    private static final long SYSTEM_UI_VIEWPORT_UPDATE_DELAY_MS = 500;

    private boolean mIsKeyboardShowing;

    private final Invalidator mInvalidator = new Invalidator();
    private LayoutManager mLayoutManager;
    private LayerTitleCache mLayerTitleCache;
    private CompositorView mCompositorView;

    private boolean mContentOverlayVisiblity = true;

    private int mPendingFrameCount;

    private final ArrayList<Invalidator.Client> mPendingInvalidations =
            new ArrayList<>();
    private boolean mSkipInvalidation;

    /**
     * A task to be performed after a resize event.
     */
    private Runnable mPostHideKeyboardTask;

    private TabModelSelector mTabModelSelector;
    private ChromeFullscreenManager mFullscreenManager;
    private View mAccessibilityView;
    private CompositorAccessibilityProvider mNodeProvider;

    /** The toolbar control container. **/
    private ControlContainer mControlContainer;

    private @Nullable KeyboardExtensionSizeManager mKeyboardExtensionSizeManager;
    private InsetObserverView mInsetObserverView;
    private boolean mShowingFullscreen;
    private Runnable mSystemUiFullscreenResizeRunnable;

    /** The currently visible Tab. */
    private Tab mTabVisible;

    /** The currently attached View. */
    private View mView;

    private TabObserver mTabObserver;

    // Cache objects that should not be created frequently.
    private final RectF mCacheViewport = new RectF();
    private final Rect mCacheRect = new Rect();
    private final Point mCachePoint = new Point();

    // If we've drawn at least one frame.
    private boolean mHasDrawnOnce;

    private boolean mIsInVr;

    /**
     * This view is created on demand to display debugging information.
     */
    private static class DebugOverlay extends View {
        private final List<Pair<Rect, Integer>> mRectangles = new ArrayList<>();
        private final Paint mPaint = new Paint();
        private boolean mFirstPush = true;

        /**
         * @param context The current Android's context.
         */
        public DebugOverlay(Context context) {
            super(context);
        }

        /**
         * Pushes a rectangle to be drawn on the screen on top of everything.
         *
         * @param rect  The rectangle to be drawn on screen
         * @param color The color of the rectangle
         */
        public void pushRect(Rect rect, int color) {
            if (mFirstPush) {
                mRectangles.clear();
                mFirstPush = false;
            }
            mRectangles.add(new Pair<>(rect, color));
            invalidate();
        }

        @Override
        protected void onDraw(Canvas canvas) {
            for (int i = 0; i < mRectangles.size(); i++) {
                mPaint.setColor(mRectangles.get(i).second);
                canvas.drawRect(mRectangles.get(i).first, mPaint);
            }
            mFirstPush = true;
        }
    }

    private DebugOverlay mDebugOverlay;

    private View mUrlBar;

    /**
     * Creates a {@link CompositorView}.
     * @param c The Context to create this {@link CompositorView} in.
     */
    public CompositorViewHolder(Context c) {
        super(c);

        internalInit();
    }

    @Override
    public PointerIcon onResolvePointerIcon(MotionEvent event, int pointerIndex) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) return null;
        View activeView = getContentView();
        if (activeView == null || !ViewCompat.isAttachedToWindow(activeView)) return null;
        return activeView.onResolvePointerIcon(event, pointerIndex);
    }

    /**
     * Creates a {@link CompositorView}.
     * @param c     The Context to create this {@link CompositorView} in.
     * @param attrs The AttributeSet used to create this {@link CompositorView}.
     */
    public CompositorViewHolder(Context c, AttributeSet attrs) {
        super(c, attrs);

        internalInit();
    }

    private void internalInit() {
        mTabObserver = new EmptyTabObserver() {
            @Override
            public void onContentChanged(Tab tab) {
                CompositorViewHolder.this.onContentChanged();
            }
        };

        addOnLayoutChangeListener(new OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                Tab tab = getCurrentTab();
                // Set the size of NTP if we're in the attached state as it may have not been sized
                // properly when initializing tab. See the comment in #initializeTab() for why.
                if (tab != null && tab.isNativePage() && isAttachedToWindow(tab.getView())) {
                    Point viewportSize = getViewportSize();
                    setSize(tab.getWebContents(), tab.getView(), viewportSize.x, viewportSize.y);
                }
                onViewportChanged();

                // If there's an event that needs to occur after the keyboard is hidden, post
                // it as a delayed event.  Otherwise this happens in the midst of the
                // ContentView's relayout, which causes the ContentView to relayout on top of the
                // stack view.  The 30ms is arbitrary, hoping to let the view get one repaint
                // in so the full page is shown.
                if (mPostHideKeyboardTask != null) {
                    new Handler().postDelayed(mPostHideKeyboardTask, 30);
                    mPostHideKeyboardTask = null;
                }
            }
        });

        mCompositorView = new CompositorView(getContext(), this);
        // mCompositorView should always be the first child.
        addView(mCompositorView, 0,
                new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        setOnSystemUiVisibilityChangeListener(new OnSystemUiVisibilityChangeListener() {
            @Override
            public void onSystemUiVisibilityChange(int visibility) {
                handleSystemUiVisibilityChange();
            }
        });
        handleSystemUiVisibilityChange();
    }

    private Point getViewportSize() {
        // When in fullscreen mode, the window does not get resized when showing the onscreen
        // keyboard[1].  To work around this, we monitor the visible display frame to mimic the
        // resize state to ensure the web contents has the correct width and height.
        //
        // This path should not be used in the non-fullscreen case as it would negate the
        // performance benefits of the app setting SOFT_INPUT_ADJUST_PAN.  This would force the
        // app into a constant SOFT_INPUT_ADJUST_RESIZE mode, which causes more churn on the page
        // layout than required in cases that you're editing in Chrome UI outside of the web
        // contents.
        //
        // [1] - https://developer.android.com/reference/android/view/WindowManager.LayoutParams.html#FLAG_FULLSCREEN
        if (mShowingFullscreen
                && KeyboardVisibilityDelegate.getInstance().isKeyboardShowing(getContext(), this)) {
            getWindowVisibleDisplayFrame(mCacheRect);

            // On certain devices, getWindowVisibleDisplayFrame is larger than the screen size, so
            // this ensures we never draw beyond the underlying dimensions of the view.
            // https://crbug.com/854109
            mCachePoint.set(Math.min(mCacheRect.width(), getWidth()),
                    Math.min(mCacheRect.height(), getHeight()));
        } else {
            mCachePoint.set(getWidth(), getHeight());
        }
        return mCachePoint;
    }

    private void handleSystemUiVisibilityChange() {
        View view = getContentView();
        if (view == null || !ViewCompat.isAttachedToWindow(view)) view = this;

        int uiVisibility = 0;
        while (view != null) {
            uiVisibility |= view.getSystemUiVisibility();
            if (!(view.getParent() instanceof View)) break;
            view = (View) view.getParent();
        }

        // SYSTEM_UI_FLAG_FULLSCREEN is cleared when showing the soft keyboard in older version of
        // Android (prior to P).  The immersive mode flags are not cleared, so use those in
        // combination to detect this state.
        boolean isInFullscreen = (uiVisibility & View.SYSTEM_UI_FLAG_FULLSCREEN) != 0
                || (uiVisibility & View.SYSTEM_UI_FLAG_IMMERSIVE) != 0
                || (uiVisibility & View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY) != 0;
        boolean layoutFullscreen = (uiVisibility & View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN) != 0;

        if (mShowingFullscreen == isInFullscreen) return;
        mShowingFullscreen = isInFullscreen;

        if (mSystemUiFullscreenResizeRunnable == null) {
            mSystemUiFullscreenResizeRunnable = () -> {
                View contentView = getContentView();
                if (contentView != null) {
                    Point viewportSize = getViewportSize();
                    setSize(getWebContents(), contentView, viewportSize.x, viewportSize.y);
                }
                onViewportChanged();
            };
        } else {
            getHandler().removeCallbacks(mSystemUiFullscreenResizeRunnable);
        }

        // If SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN is set, defer updating the viewport to allow
        // Android's animations to complete.  The getWindowVisibleDisplayFrame values do not get
        // updated until a fair amount after onSystemUiVisibilityChange is broadcast.
        //
        // SYSTEM_UI_VIEWPORT_UPDATE_DELAY_MS was chosen by increasing the time until the UI did
        // not reliably jump from updating the viewport too early.
        long delay = layoutFullscreen ? SYSTEM_UI_VIEWPORT_UPDATE_DELAY_MS : 0;
        postDelayed(mSystemUiFullscreenResizeRunnable, delay);
    }

    /**
     * @param layoutManager The {@link LayoutManager} instance that will be driving what
     *                      shows in this {@link CompositorViewHolder}.
     */
    public void setLayoutManager(LayoutManager layoutManager) {
        mLayoutManager = layoutManager;
        onViewportChanged();
    }

    /**
     * @param view The root view of the hierarchy.
     */
    public void setRootView(View view) {
        mCompositorView.setRootView(view);
    }

    /**
     * @param controlContainer The ControlContainer.
     */
    public void setControlContainer(ControlContainer controlContainer) {
        DynamicResourceLoader loader = mCompositorView.getResourceManager() != null
                ? mCompositorView.getResourceManager().getDynamicResourceLoader()
                : null;
        if (loader != null && mControlContainer != null) {
            loader.unregisterResource(R.id.control_container);
        }
        mControlContainer = controlContainer;
        if (loader != null && mControlContainer != null) {
            loader.registerResource(
                    R.id.control_container, mControlContainer.getToolbarResourceAdapter());
        }
    }

    /**
     * Set the InsetObserverView that can be monitored for changes to the window insets from Android
     * system UI.
     */
    public void setInsetObserverView(InsetObserverView view) {
        if (mInsetObserverView != null) {
            mInsetObserverView.removeObserver(this);
        }
        mInsetObserverView = view;
        if (mInsetObserverView != null) {
            mInsetObserverView.addObserver(this);
            onViewportChanged();
        }
    }

    @Override
    public void onInsetChanged(int left, int top, int right, int bottom) {
        if (mShowingFullscreen) onViewportChanged();
    }

    @Override
    public void onSafeAreaChanged(Rect area) {}

    /**
     * Allows to set (or unset if called with null) the {@link KeyboardExtensionSizeManager} that
     * provides the dimensions of any keyboard extensions or replacements. Registers an observer to
     * react to size changes immediately.
     * @param manager A {@link KeyboardExtensionSizeManager}. Optional.
     */
    public void setKeyboardExtensionView(@Nullable KeyboardExtensionSizeManager manager) {
        if (mKeyboardExtensionSizeManager != null) {
            mKeyboardExtensionSizeManager.removeObserver(this);
        }
        mKeyboardExtensionSizeManager = manager;
        if (mKeyboardExtensionSizeManager != null) {
            mKeyboardExtensionSizeManager.addObserver(this);
            onViewportChanged();
        }
    }

    @Override
    public void onKeyboardExtensionHeightChanged(int keyboardHeight) {
        onUpdateViewportSize();
    }

    /**
     * Returns the combined height of all extensions to or replacements of the keyboard which
     * consume space at the bottom of the content area.
     * @return the full height in pixels.
     */
    public @Px int getKeyboardExtensionsHeight() {
        if (mKeyboardExtensionSizeManager == null) return 0;
        return mKeyboardExtensionSizeManager.getKeyboardExtensionHeight();
    }

    /**
     * Should be called for cleanup when the CompositorView instance is no longer used.
     */
    public void shutDown() {
        setTab(null);
        if (mLayerTitleCache != null) mLayerTitleCache.shutDown();
        mCompositorView.shutDown();
        if (mLayoutManager != null) mLayoutManager.destroy();
        if (mInsetObserverView != null) {
            mInsetObserverView.removeObserver(this);
            mInsetObserverView = null;
        }
    }

    /**
     * This is called when the native library are ready.
     */
    public void onNativeLibraryReady(
            WindowAndroid windowAndroid, TabContentManager tabContentManager) {
        assert mLayerTitleCache == null : "Should be called once";

        if (DeviceClassManager.enableLayerDecorationCache()) {
            mLayerTitleCache = new LayerTitleCache(getContext());
        }

        mCompositorView.initNativeCompositor(
                SysUtils.isLowEndDevice(), windowAndroid, mLayerTitleCache, tabContentManager);

        if (mLayerTitleCache != null) {
            mLayerTitleCache.setResourceManager(getResourceManager());
        }

        if (mControlContainer != null) {
            mCompositorView.getResourceManager().getDynamicResourceLoader().registerResource(
                    R.id.control_container, mControlContainer.getToolbarResourceAdapter());
        }
    }

    /**
     * Perform any initialization necessary for showing a reparented tab.
     */
    public void prepareForTabReparenting() {
        if (mHasDrawnOnce) return;

        // Set the background to white while we wait for the first swap of buffers. This gets
        // corrected inside the view.
        mCompositorView.setBackgroundColor(Color.WHITE);
    }

    @Override
    public ResourceManager getResourceManager() {
        return mCompositorView.getResourceManager();
    }

    /**
     * @return The {@link DynamicResourceLoader} for registering resources.
     */
    public DynamicResourceLoader getDynamicResourceLoader() {
        return mCompositorView.getResourceManager().getDynamicResourceLoader();
    }

    /**
     * @return The {@link Invalidator} instance that is driven by this {@link CompositorViewHolder}.
     */
    public Invalidator getInvalidator() {
        return mInvalidator;
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent e) {
        super.onInterceptTouchEvent(e);

        if (mLayoutManager == null) return false;

        setContentViewMotionEventOffsets(e, false);
        return mLayoutManager.onInterceptTouchEvent(e, mIsKeyboardShowing);
    }

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        super.onTouchEvent(e);

        if (mFullscreenManager != null) mFullscreenManager.onMotionEvent(e);
        boolean consumed = mLayoutManager != null && mLayoutManager.onTouchEvent(e);
        setContentViewMotionEventOffsets(e, true);
        return consumed;
    }

    @Override
    public boolean onInterceptHoverEvent(MotionEvent e) {
        setContentViewMotionEventOffsets(e, true);
        return super.onInterceptHoverEvent(e);
    }

    @Override
    public boolean dispatchHoverEvent(MotionEvent e) {
        if (mNodeProvider != null) {
            if (mNodeProvider.dispatchHoverEvent(e)) {
                return true;
            }
        }
        return super.dispatchHoverEvent(e);
    }

    @Override
    public boolean dispatchDragEvent(DragEvent e) {
        if (mTabVisible == null) return false;
        WebContents webContents = mTabVisible.getWebContents();
        if (webContents == null) return false;

        if (mLayoutManager != null) mLayoutManager.getViewportPixel(mCacheViewport);
        EventForwarder forwarder = webContents.getEventForwarder();
        forwarder.setCurrentTouchEventOffsets(-mCacheViewport.left, -mCacheViewport.top);
        boolean ret = super.dispatchDragEvent(e);

        int action = e.getAction();
        if (action == DragEvent.ACTION_DRAG_EXITED || action == DragEvent.ACTION_DRAG_ENDED
                || action == DragEvent.ACTION_DROP) {
            forwarder.setCurrentTouchEventOffsets(0.f, 0.f);
        }
        return ret;
    }

    /**
     * @return The {@link LayoutManager} associated with this view.
     */
    public LayoutManager getLayoutManager() {
        return mLayoutManager;
    }

    /**
     * @return The SurfaceView proxy used by the Compositor.
     */
    public CompositorView getCompositorView() {
        return mCompositorView;
    }

    /**
     * @return The active {@link android.view.SurfaceView} of the Compositor.
     */
    public View getActiveSurfaceView() {
        return mCompositorView.getActiveSurfaceView();
    }

    private Tab getCurrentTab() {
        if (mLayoutManager == null || mTabModelSelector == null) return null;
        Tab currentTab = mTabModelSelector.getCurrentTab();

        // If the tab model selector doesn't know of a current tab, use the last visible one.
        if (currentTab == null) currentTab = mTabVisible;

        return currentTab;
    }

    private View getContentView() {
        Tab tab = getCurrentTab();
        return tab != null ? tab.getContentView() : null;
    }

    private WebContents getWebContents() {
        Tab tab = getCurrentTab();
        return tab != null ? tab.getWebContents() : null;
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        if (mTabModelSelector == null) return;

        Point viewportSize = getViewportSize();
        for (TabModel tabModel : mTabModelSelector.getModels()) {
            for (int i = 0; i < tabModel.getCount(); ++i) {
                Tab tab = tabModel.getTabAt(i);
                if (tab == null) continue;
                setSize(tab.getWebContents(), tab.getContentView(), viewportSize.x, viewportSize.y);
            }
        }
    }

    /**
     * Set tab-backed content view size.
     *
     * @param webContents {@link WebContents} for which the size of the view is set.
     * @param view {@link View} of the content.
     * @param w Width of the view.
     * @param h Height of the view.
     */
    private void setSize(WebContents webContents, View view, int w, int h) {
        if (webContents == null || view == null) return;

        // When in VR, the CompositorView doesn't control the size of the WebContents.
        if (mIsInVr) return;

        // The view size takes into account of the browser controls whose height
        // should be subtracted from the view if they are visible, therefore shrink
        // Blink-side view size.
        int controlsHeight = controlsResizeView()
                ? getTopControlsHeightPixels() + getBottomControlsHeightPixels()
                : 0;
        controlsHeight += getKeyboardExtensionsHeight();
        if (isAttachedToWindow(view)) {
            webContents.setSize(w, h - controlsHeight);
        } else {
            setSizeOfUnattachedView(view, webContents, controlsHeight);
            requestRender();
        }
    }

    private static boolean isAttachedToWindow(View view) {
        return view != null && view.getWindowToken() != null;
    }

    @Override
    public void onSurfaceResized(int width, int height) {
        View view = getContentView();
        WebContents webContents = getWebContents();
        if (view == null || webContents == null) return;
        onPhysicalBackingSizeChanged(webContents, width, height);
    }

    private void onPhysicalBackingSizeChanged(WebContents webContents, int width, int height) {
        if (mCompositorView != null) {
            mCompositorView.onPhysicalBackingSizeChanged(webContents, width, height);
        }
    }

    /**
     * Called whenever the host activity is started.
     */
    public void onStart() {
        if (mFullscreenManager != null) {
            mFullscreenManager.addListener(this);
        }
        requestRender();
    }

    /**
     * Called whenever the host activity is stopped.
     */
    public void onStop() {
        if (mFullscreenManager != null) mFullscreenManager.removeListener(this);
    }

    @Override
    public void onContentOffsetChanged(float offset) {
        onViewportChanged();
    }

    @Override
    public void onControlsOffsetChanged(float topOffset, float bottomOffset, boolean needsAnimate) {
        onViewportChanged();
        if (needsAnimate) requestRender();
    }

    @Override
    public void onBottomControlsHeightChanged(int bottomControlsHeight) {
        if (mTabVisible == null) return;
        mTabVisible.setBottomControlsHeight(bottomControlsHeight);
        Point viewportSize = getViewportSize();
        setSize(mTabVisible.getWebContents(), mTabVisible.getContentView(), viewportSize.x,
                viewportSize.y);
    }

    @Override
    public void onToggleOverlayVideoMode(boolean enabled) {
        if (mCompositorView != null) {
            mCompositorView.setOverlayVideoMode(enabled);
        }
    }

    @Override
    public void onUpdateViewportSize() {
        // Reflect the changes that may have happened in in view/control size.
        Point viewportSize = getViewportSize();
        setSize(getWebContents(), getContentView(), viewportSize.x, viewportSize.y);
    }

    /**
     * Sets the overlay mode.
     */
    public void setOverlayMode(boolean useOverlayMode) {
        if (mCompositorView != null) {
            mCompositorView.setOverlayVideoMode(useOverlayMode);
        }
    }

    private void setContentViewMotionEventOffsets(MotionEvent e, boolean canClear) {
        // TODO(dtrainor): Factor this out to LayoutDriver.
        if (e == null || mTabVisible == null) return;

        WebContents webContents = mTabVisible.getWebContents();
        if (webContents == null) return;

        int actionMasked = SPenSupport.convertSPenEventAction(e.getActionMasked());

        if (actionMasked == MotionEvent.ACTION_DOWN
                || actionMasked == MotionEvent.ACTION_HOVER_ENTER
                || actionMasked == MotionEvent.ACTION_HOVER_MOVE) {
            if (mLayoutManager != null) mLayoutManager.getViewportPixel(mCacheViewport);
            webContents.getEventForwarder().setCurrentTouchEventOffsets(
                    -mCacheViewport.left, -mCacheViewport.top);
        } else if (canClear && (actionMasked == MotionEvent.ACTION_UP
                                       || actionMasked == MotionEvent.ACTION_CANCEL
                                       || actionMasked == MotionEvent.ACTION_HOVER_EXIT)) {
            webContents.getEventForwarder().setCurrentTouchEventOffsets(0.f, 0.f);
        }
    }

    private void onViewportChanged() {
        if (mLayoutManager != null) mLayoutManager.onViewportChanged();
    }

    /**
     * To be called once a frame before commit.
     */
    @Override
    public void onCompositorLayout() {
        TraceEvent.begin("CompositorViewHolder:layout");
        if (mLayoutManager != null) {
            mLayoutManager.onUpdate();
            mCompositorView.finalizeLayers(mLayoutManager, false);
        }

        TraceEvent.end("CompositorViewHolder:layout");
    }

    @Override
    public void getWindowViewport(RectF outRect) {
        Point viewportSize = getViewportSize();
        outRect.set(0, 0, viewportSize.x, viewportSize.y);
    }

    @Override
    public void getVisibleViewport(RectF outRect) {
        getWindowViewport(outRect);

        float bottomControlOffset = 0;
        if (mFullscreenManager != null) {
            // All of these values are in pixels.
            outRect.top += mFullscreenManager.getTopVisibleContentOffset();
            bottomControlOffset = mFullscreenManager.getBottomControlOffset();
        }
        outRect.bottom -= (getBottomControlsHeightPixels() - bottomControlOffset);
    }

    @Override
    public void getViewportFullControls(RectF outRect) {
        getWindowViewport(outRect);

        if (mFullscreenManager != null) {
            // All of these values are in pixels.
            outRect.top += mFullscreenManager.getTopControlsHeight();
            outRect.bottom -= mFullscreenManager.getBottomControlsHeight();
        }
    }

    @Override
    public float getHeightMinusBrowserControls() {
        return getHeight() - (getTopControlsHeightPixels() + getBottomControlsHeightPixels());
    }

    @Override
    public void requestRender() {
        mCompositorView.requestRender();
    }

    @Override
    public void onSurfaceCreated() {
        mPendingFrameCount = 0;
        flushInvalidation();
    }

    @Override
    public void didSwapFrame(int pendingFrameCount) {
        TraceEvent.instant("didSwapFrame");

        // Wait until the second frame to turn off the placeholder background for the CompositorView
        // and the tab strip, to ensure the compositor frame has been drawn.
        final ViewGroup controlContainer = (ViewGroup) mControlContainer;
        if (mHasDrawnOnce) {
            post(new Runnable() {
                @Override
                public void run() {
                    mCompositorView.setBackgroundResource(0);
                    if (controlContainer != null) {
                        controlContainer.setBackgroundResource(0);
                    }
                }
            });
        }

        mHasDrawnOnce = true;

        mPendingFrameCount = pendingFrameCount;

        if (!mSkipInvalidation || pendingFrameCount == 0) flushInvalidation();
        mSkipInvalidation = !mSkipInvalidation;
    }

    @Override
    public void setContentOverlayVisibility(boolean show) {
        if (show != mContentOverlayVisiblity) {
            mContentOverlayVisiblity = show;
            updateContentOverlayVisibility(mContentOverlayVisiblity);
        }
    }

    @Override
    public LayoutRenderHost getLayoutRenderHost() {
        return this;
    }

    @Override
    public void pushDebugRect(Rect rect, int color) {
        if (mDebugOverlay == null) {
            mDebugOverlay = new DebugOverlay(getContext());
            addView(mDebugOverlay);
        }
        mDebugOverlay.pushRect(rect, color);
    }

    @Override
    public void loadPersitentTextureDataIfNeeded() {}

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        mIsKeyboardShowing =
                KeyboardVisibilityDelegate.getInstance().isKeyboardShowing(getContext(), this);
    }

    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        if (changed) onViewportChanged();
        super.onLayout(changed, l, t, r, b);

        invalidateAccessibilityProvider();
    }

    @Override
    public void clearChildFocus(View child) {
        // Override this method so that the ViewRoot doesn't go looking for a new
        // view to take focus. It will find the URL Bar, focus it, then refocus this
        // later, causing a keyboard flicker.
    }

    @Override
    public ChromeFullscreenManager getFullscreenManager() {
        return mFullscreenManager;
    }

    /**
     * Sets a fullscreen handler.
     * @param fullscreen A fullscreen handler.
     */
    public void setFullscreenHandler(ChromeFullscreenManager fullscreen) {
        mFullscreenManager = fullscreen;
        if (mFullscreenManager != null) {
            mFullscreenManager.addListener(this);
        }
        onViewportChanged();
    }

    @Override
    public int getBrowserControlsBackgroundColor() {
        return mTabVisible == null ? Color.WHITE : mTabVisible.getThemeColor();
    }

    @Override
    public int getTopControlsHeightPixels() {
        return mFullscreenManager != null ? mFullscreenManager.getTopControlsHeight() : 0;
    }

    @Override
    public int getBottomControlsHeightPixels() {
        return mFullscreenManager != null ? mFullscreenManager.getBottomControlsHeight() : 0;
    }

    /**
     * @return {@code true} if browser controls shrink Blink view's size.
     */
    public boolean controlsResizeView() {
        return mFullscreenManager != null ? mFullscreenManager.controlsResizeView() : false;
    }

    @Override
    public float getOverlayTranslateY() {
        return mFullscreenManager.getTopVisibleContentOffset();
    }

    /**
     * Sets the URL bar. This is needed so that the ContentViewHolder can find out
     * whether it can claim focus.
     */
    public void setUrlBar(View urlBar) {
        mUrlBar = urlBar;
    }

    @Override
    public void onAttachedToWindow() {
        mInvalidator.set(this);
        super.onAttachedToWindow();
    }

    @Override
    public void onDetachedFromWindow() {
        flushInvalidation();
        mInvalidator.set(null);
        super.onDetachedFromWindow();

        // Removes the accessibility node provider from this view.
        if (mNodeProvider != null) {
            mAccessibilityView.setAccessibilityDelegate(null);
            mNodeProvider = null;
            removeView(mAccessibilityView);
            mAccessibilityView = null;
        }
    }

    @Override
    public void hideKeyboard(Runnable postHideTask) {
        // When this is called we actually want to hide the keyboard whatever owns it.
        // This includes hiding the keyboard, and dropping focus from the URL bar.
        // See http://crbug/236424
        // TODO(aberent) Find a better place to put this, possibly as part of a wider
        // redesign of focus control.
        if (mUrlBar != null) mUrlBar.clearFocus();
        boolean wasVisible = false;
        if (hasFocus()) {
            wasVisible = KeyboardVisibilityDelegate.getInstance().hideKeyboard(this);
        }
        if (wasVisible) {
            mPostHideKeyboardTask = postHideTask;
        } else {
            postHideTask.run();
        }
    }

    /**
     * Sets the appropriate objects this class should represent.
     * @param tabModelSelector        The {@link TabModelSelector} this View should hold and
     *                                represent.
     * @param tabCreatorManager       The {@link TabCreatorManager} for this view.
     * @param tabContentManager       The {@link TabContentManager} for the tabs.
     * @param androidContentContainer The {@link ViewGroup} the {@link LayoutManager} should bind
     *                                Android content to.
     * @param contextualSearchManager A {@link ContextualSearchManagementDelegate} instance.
     */
    public void onFinishNativeInitialization(TabModelSelector tabModelSelector,
            TabCreatorManager tabCreatorManager, TabContentManager tabContentManager,
            ViewGroup androidContentContainer,
            ContextualSearchManagementDelegate contextualSearchManager) {
        assert mLayoutManager != null;
        mLayoutManager.init(tabModelSelector, tabCreatorManager, tabContentManager,
                androidContentContainer, contextualSearchManager,
                mCompositorView.getResourceManager().getDynamicResourceLoader());

        mTabModelSelector = tabModelSelector;
        tabModelSelector.addObserver(new EmptyTabModelSelectorObserver() {
            @Override
            public void onChange() {
                onContentChanged();
            }

            @Override
            public void onNewTabCreated(Tab tab) {
                initializeTab(tab);
            }
        });

        mLayerTitleCache.setTabModelSelector(mTabModelSelector);

        onContentChanged();
    }

    private void updateContentOverlayVisibility(boolean show) {
        if (mView == null) return;
        WebContents webContents = getWebContents();
        if (show) {
            if (mView.getParent() != this) {
                // During tab creation, we temporarily add the new tab's view to a FrameLayout to
                // measure and lay it out. This way we could show the animation in the stack view.
                // Therefore we should remove the view from that temporary FrameLayout here.
                UiUtils.removeViewFromParent(mView);

                if (webContents != null) {
                    assert !webContents.isDestroyed();
                    getContentView().setVisibility(View.VISIBLE);
                    if (mFullscreenManager != null) mFullscreenManager.updateViewportSize();
                }

                // CompositorView always has index of 0.
                addView(mView, 1);

                setFocusable(false);
                setFocusableInTouchMode(false);

                // Claim focus for the new view unless the user is currently using the URL bar.
                if (mUrlBar == null || !mUrlBar.hasFocus()) mView.requestFocus();
            }
        } else {
            if (mView.getParent() == this) {
                setFocusable(true);
                setFocusableInTouchMode(true);

                if (webContents != null && !webContents.isDestroyed()) {
                    getContentView().setVisibility(View.INVISIBLE);
                }
                removeView(mView);
            }
        }
    }

    @Override
    public void onContentChanged() {
        if (mTabModelSelector == null) {
            // Not yet initialized, onContentChanged() will eventually get called by
            // setTabModelSelector.
            return;
        }
        Tab tab = mTabModelSelector.getCurrentTab();
        setTab(tab);
    }

    private void setTab(Tab tab) {
        if (tab != null) tab.loadIfNeeded();

        View newView = tab != null ? tab.getView() : null;
        if (mView == newView) return;

        // TODO(dtrainor): Look into changing this only if the views differ, but still parse the
        // WebContents list even if they're the same.
        updateContentOverlayVisibility(false);

        if (mTabVisible != tab) {
            if (mTabVisible != null) mTabVisible.removeObserver(mTabObserver);
            if (tab != null) tab.addObserver(mTabObserver);
        }

        mTabVisible = tab;
        mView = newView;

        updateContentOverlayVisibility(mContentOverlayVisiblity);

        if (mTabVisible != null) initializeTab(mTabVisible);
    }

    /**
     * Sets the correct size for {@link View} on {@code tab} and sets the correct rendering
     * parameters on {@link WebContents} on {@code tab}.
     * @param tab The {@link Tab} to initialize.
     */
    private void initializeTab(Tab tab) {
        WebContents webContents = tab.getWebContents();
        if (webContents != null) {
            onPhysicalBackingSizeChanged(
                    webContents, mCompositorView.getWidth(), mCompositorView.getHeight());
        }
        if (tab.getView() == null) return;
        tab.setTopControlsHeight(getTopControlsHeightPixels(), controlsResizeView());
        tab.setBottomControlsHeight(getBottomControlsHeightPixels());

        // TextView with compound drawables in the NTP gets a wrong width when measure/layout is
        // performed in the unattached state. Delay the layout till #onLayoutChange().
        // See https://crbug.com/876686.
        if (tab.isNativePage() && !isAttachedToWindow(tab.getView())) return;
        Point viewportSize = getViewportSize();
        setSize(webContents, tab.getView(), viewportSize.x, viewportSize.y);
    }

    /**
     * Resize {@code view} to match the size of this {@link FrameLayout}.  This will only happen if
     * the {@link View} is not part of the view hierarchy.
     * @param view The {@link View} to resize.
     * @param webContents {@link WebContents} associated with the view.
     * @param controlsHeight Height of top/bottom browser controls combined.
     */
    private void setSizeOfUnattachedView(View view, WebContents webContents, int controlsHeight) {
        // Need to call layout() for the following View if it is not attached to the view hierarchy.
        // Calling {@code view.onSizeChanged()} is dangerous because if the View has a different
        // size than the WebContents, it might think a future size update is a NOOP and not call
        // onSizeChanged() on the WebContents.
        if (isAttachedToWindow(view)) return;
        Point viewportSize = getViewportSize();
        int width = viewportSize.x;
        int height = viewportSize.y;
        view.measure(MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY));
        view.layout(0, 0, view.getMeasuredWidth(), view.getMeasuredHeight());
        webContents.setSize(view.getWidth(), view.getHeight() - controlsHeight);
    }

    @Override
    public TitleCache getTitleCache() {
        return mLayerTitleCache;
    }

    @Override
    public void deferInvalidate(Client client) {
        if (mPendingFrameCount <= 0) {
            client.doInvalidate();
        } else if (!mPendingInvalidations.contains(client)) {
            mPendingInvalidations.add(client);
        }
    }

    private void flushInvalidation() {
        if (mPendingInvalidations.isEmpty()) return;
        TraceEvent.instant("CompositorViewHolder.flushInvalidation");
        for (int i = 0; i < mPendingInvalidations.size(); i++) {
            mPendingInvalidations.get(i).doInvalidate();
        }
        mPendingInvalidations.clear();
    }

    /**
     * Called when VR is entered. The CompositorViewHolder loses control over WebContents sizing.
     */
    public void onEnterVr() {
        mIsInVr = true;
    }

    /**
     * Called when VR is exited. The CompositorViewHolder regains control over WebContents sizing.
     */
    public void onExitVr() {
        mIsInVr = false;
        onUpdateViewportSize();
    }

    @Override
    public void invalidateAccessibilityProvider() {
        if (mNodeProvider != null) {
            mNodeProvider.sendEventForVirtualView(
                    mNodeProvider.getAccessibilityFocusedVirtualViewId(),
                    AccessibilityEventCompat.TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED);
            mNodeProvider.invalidateRoot();
        }
    }

    /**
     * Called when the accessibility enabled state changes.
     * @param enabled Whether accessibility is enabled.
     */
    public void onAccessibilityStatusChanged(boolean enabled) {
        // Instantiate and install the accessibility node provider on this view if necessary.
        // This overrides any hover event listeners or accessibility delegates
        // that may have been added elsewhere.
        if (enabled && (mNodeProvider == null)) {
            mAccessibilityView = new View(getContext());
            addView(mAccessibilityView);
            mNodeProvider = new CompositorAccessibilityProvider(mAccessibilityView);
            ViewCompat.setAccessibilityDelegate(mAccessibilityView, mNodeProvider);
        }
    }

    /**
     * Class used to provide a virtual view hierarchy to the Accessibility
     * framework for this view and its contained items.
     * <p>
     * <strong>NOTE:</strong> This class is fully backwards compatible for
     * compilation, but will only provide touch exploration on devices running
     * Ice Cream Sandwich and above.
     * </p>
     */
    private class CompositorAccessibilityProvider extends ExploreByTouchHelper {
        private final float mDpToPx;
        List<VirtualView> mVirtualViews = new ArrayList<>();
        private final Rect mPlaceHolderRect = new Rect(0, 0, 1, 1);
        private static final String PLACE_HOLDER_STRING = "";
        private final RectF mTouchTarget = new RectF();
        private final Rect mPixelRect = new Rect();

        public CompositorAccessibilityProvider(View forView) {
            super(forView);
            mDpToPx = getContext().getResources().getDisplayMetrics().density;
        }

        @Override
        protected int getVirtualViewAt(float x, float y) {
            if (mVirtualViews == null) return INVALID_ID;
            for (int i = 0; i < mVirtualViews.size(); i++) {
                if (mVirtualViews.get(i).checkClicked(x / mDpToPx, y / mDpToPx)) {
                    return i;
                }
            }
            return INVALID_ID;
        }

        @Override
        protected void getVisibleVirtualViews(List<Integer> virtualViewIds) {
            if (mLayoutManager == null) return;
            mVirtualViews.clear();
            mLayoutManager.getVirtualViews(mVirtualViews);
            for (int i = 0; i < mVirtualViews.size(); i++) {
                virtualViewIds.add(i);
            }
        }

        @Override
        protected boolean onPerformActionForVirtualView(
                int virtualViewId, int action, Bundle arguments) {
            switch (action) {
                case AccessibilityNodeInfoCompat.ACTION_CLICK:
                    mVirtualViews.get(virtualViewId).handleClick(LayoutManager.time());
                    return true;
            }

            return false;
        }

        @Override
        protected void onPopulateEventForVirtualView(int virtualViewId, AccessibilityEvent event) {
            if (mVirtualViews == null || mVirtualViews.size() <= virtualViewId) {
                // TODO(clholgat): Remove this work around when the Android bug is fixed.
                // crbug.com/420177
                event.setContentDescription(PLACE_HOLDER_STRING);
                return;
            }
            VirtualView view = mVirtualViews.get(virtualViewId);

            event.setContentDescription(view.getAccessibilityDescription());
            event.setClassName(CompositorViewHolder.class.getName());
        }

        @Override
        protected void onPopulateNodeForVirtualView(
                int virtualViewId, AccessibilityNodeInfoCompat node) {
            if (mVirtualViews == null || mVirtualViews.size() <= virtualViewId) {
                // TODO(clholgat): Remove this work around when the Android bug is fixed.
                // crbug.com/420177
                node.setBoundsInParent(mPlaceHolderRect);
                node.setContentDescription(PLACE_HOLDER_STRING);
                return;
            }
            VirtualView view = mVirtualViews.get(virtualViewId);
            view.getTouchTarget(mTouchTarget);

            node.setBoundsInParent(rectToPx(mTouchTarget));
            node.setContentDescription(view.getAccessibilityDescription());
            node.addAction(AccessibilityNodeInfoCompat.ACTION_CLICK);
            node.addAction(AccessibilityNodeInfoCompat.ACTION_FOCUS);
            node.addAction(AccessibilityNodeInfoCompat.ACTION_LONG_CLICK);
        }

        private Rect rectToPx(RectF rect) {
            rect.roundOut(mPixelRect);
            mPixelRect.left = (int) (mPixelRect.left * mDpToPx);
            mPixelRect.top = (int) (mPixelRect.top * mDpToPx);
            mPixelRect.right = (int) (mPixelRect.right * mDpToPx);
            mPixelRect.bottom = (int) (mPixelRect.bottom * mDpToPx);

            // Don't let any zero sized rects through, they'll cause parent
            // size errors in L.
            if (mPixelRect.width() == 0) {
                mPixelRect.right = mPixelRect.left + 1;
            }
            if (mPixelRect.height() == 0) {
                mPixelRect.bottom = mPixelRect.top + 1;
            }
            return mPixelRect;
        }
    }
}
