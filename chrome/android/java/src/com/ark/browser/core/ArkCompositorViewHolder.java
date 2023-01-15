// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.ark.browser.core;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Build;
import android.os.Handler;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Pair;
import android.view.ActionMode;
import android.view.DragEvent;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.PointerIcon;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ActionMenuView;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.ViewCompat;
import androidx.fragment.app.FragmentActivity;

import com.ark.browser.tab.ArkSwipeRefreshHandler;
import com.ark.browser.tab.ArkTabImpl;
import com.ark.browser.tab.ArkTabWebContentsObserver;
import com.ark.browser.tab.core.ITabGroup;
import com.ark.browser.tab.dao.ArkTabStore;
import com.ark.browser.ui.dialog.SmartSearchPopupWindow;
import com.ark.browser.ui.widget.SmartSearchPanel;
import com.ark.browser.ui.widget.swiperefresh.SwipeRefreshLayout;
import com.ark.browser.utils.ArkLogger;

import org.chromium.base.Consumer;
import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.SysUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.compat.ApiHelperForN;
import org.chromium.base.compat.ApiHelperForO;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.ChromeActionModeHandler;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.CompositorView;
import org.chromium.chrome.browser.compositor.Invalidator;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.content.ContentOffsetProvider;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.ui.TabObscuringHandler;
import org.chromium.components.browser_ui.widget.InsetObserverView;
import org.chromium.components.browser_ui.widget.TouchEventObserver;
import org.chromium.components.content_capture.OnscreenContentProvider;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content.browser.selection.FloatingActionModeCallback;
import org.chromium.content_public.browser.ActionModeCallbackHelper;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.SelectAroundCaretResult;
import org.chromium.content_public.browser.SelectionClient;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.OverscrollRefreshHandler;
import org.chromium.ui.base.ApplicationViewportInsetSupplier;
import org.chromium.ui.base.EventForwarder;
import org.chromium.ui.base.EventOffsetHandler;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.resources.ResourceManager;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;
import org.chromium.ui.touch_selection.SelectionEventType;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * This class holds a {@link CompositorView}. This level of indirection is needed to benefit from
 * the {@link ViewGroup#onInterceptTouchEvent(MotionEvent)} capability on
 * available on {@link ViewGroup}s.
 * This class also holds the {@link LayoutManagerImpl} responsible to describe the items to be
 * drawn by the UI compositor on the native side.
 */
public class ArkCompositorViewHolder extends FrameLayout
        implements ContentOffsetProvider, LayoutManagerHost, LayoutRenderHost, Invalidator.Host,
        BrowserControlsStateProvider.Observer, InsetObserverView.WindowInsetObserver,
        TabObscuringHandler.Observer,
        ViewGroup.OnHierarchyChangeListener {

    private static final String TAG = "ArkCompositorViewHolder";

    private static final long SYSTEM_UI_VIEWPORT_UPDATE_DELAY_MS = 500;

    /**
     * Initializer interface used to decouple initialization from the class that owns
     * the CompositorViewHolder.
     */
    public interface Initializer {
        void initializeCompositorContent(LayoutManagerImpl layoutManager,
                                         ViewGroup contentContainer);
    }

    private final ObserverList<TouchEventObserver> mTouchEventObservers = new ObserverList<>();


    protected ArkWindowAndroid mWindowAndroid;

    protected ArkLayoutManager mLayoutManager;
    protected TabContentManager mTabContentManager;

    protected Callback mCallback;

    private final ArkTabStore mTabStore = new ArkTabStore();

    private final TabObserver mTabObserver = new EmptyTabObserver() {

        @Override
        public void onUrlUpdated(Tab tab) {
            if (mCallback != null) {
                ITabGroup tabGroup = mCallback.getTabList(mTabVisible);
                onBackPressedCallback.setEnabled(tabGroup.canGoBack());
            } else {
                setEnabled(false);
            }
        }

        @Override
        public void onShown(Tab tab, @TabSelectionType int type) {
            mLayoutManager.mStaticLayout.onShown(tab);
        }

        @Override
        public void onContentChanged(Tab tab) {
            mLayoutManager.initLayoutTabFromHost(tab.getId());
            setTab(tab);
        }

        @Override
        public void onBackgroundColorChanged(Tab tab, int color) {
            mLayoutManager.initLayoutTabFromHost(tab.getId());
        }

        @Override
        public void onDidChangeThemeColor(Tab tab, int color) {
            mLayoutManager.initLayoutTabFromHost(tab.getId());
        }

        @Override
        public void onContentViewScrollingStateChanged(boolean scrolling) {
            mContentViewScrolling = scrolling;
            if (!scrolling) updateContentViewChildrenDimension();
        }

        @Override
        public void onWebContentsSwapped(Tab tab, boolean didStartLoad, boolean didFinishLoad) {
            /**
             * After swapping web contents, any gesture active in the old ContentView is
             * cancelled. We still want to continue a previously running gesture in the new
             * ContentView, so we synthetically dispatch a new ACTION_DOWN MotionEvent with the
             * coordinates of where we estimate the pointer currently is (the coordinates of
             * the last ACTION_MOVE MotionEvent received before the swap).
             *
             * We wait for layout to happen as the newly created ContentView currently has a
             * width and height of zero, which would result in the event not being dispatched.
             */
            mContentView.addOnLayoutChangeListener(new OnLayoutChangeListener() {
                @Override
                public void onLayoutChange(View v, int left, int top, int right, int bottom,
                                           int oldLeft, int oldTop, int oldRight, int oldBottom) {
                    v.removeOnLayoutChangeListener(this);
                    if (mLastActiveTouchEvent == null) return;
                    MotionEvent touchEvent = MotionEvent.obtain(mLastActiveTouchEvent);
                    touchEvent.setAction(MotionEvent.ACTION_DOWN);
                    ArkCompositorViewHolder.this.dispatchTouchEvent(touchEvent);
                    for (int i = 1; i < mLastActiveTouchEvent.getPointerCount(); i++) {
                        MotionEvent pointerDownEvent =
                                MotionEvent.obtain(mLastActiveTouchEvent);
                        pointerDownEvent.setAction(MotionEvent.ACTION_POINTER_DOWN
                                | (i << MotionEvent.ACTION_POINTER_INDEX_SHIFT));
                        ArkCompositorViewHolder.this.dispatchTouchEvent(pointerDownEvent);
                    }
                }
            });
        }

        @Override
        public void onLoadProgressChanged(Tab tab, float progress) {
            super.onLoadProgressChanged(tab, progress);
        }

        @Override
        public void onNavigationEntriesDeleted(Tab tab) {
//            if (!tab.isDestroyed()) TabStateAttributes.from(tab).setIsTabStateDirty(true);
            mTabStore.addTabToSaveQueue(((ArkTabImpl) tab).getArkWeb());
        }

        @Override
        public void onPageLoadFinished(Tab tab, GURL url) {
//            if (!tab.isDestroyed()) TabStateAttributes.from(tab).setIsTabStateDirty(true);
            mTabStore.addTabToSaveQueue(((ArkTabImpl) tab).getArkWeb());
        }

        @Override
        public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
            super.onLoadStopped(tab, toDifferentDocument);
        }

        @Override
        public void onTitleUpdated(Tab tab) {
//            if (!tab.isDestroyed()) TabStateAttributes.from(tab).setIsTabStateDirty(true);
            mTabStore.addTabToSaveQueue(((ArkTabImpl) tab).getArkWeb());
        }

        @Override
        public void onDidFinishNavigation(Tab tab, NavigationHandle navigationHandle) {
            if (tab != mSwipeRefreshHandler.getTab()) {
                return;
            }
            if (navigationHandle.isInPrimaryMainFrame()) {
                mSwipeRefreshHandler.didStopRefreshing();
            }
        }
    };

    private final ArkSwipeRefreshHandler mSwipeRefreshHandler;

    private EventOffsetHandler mEventOffsetHandler;
    private boolean mIsKeyboardShowing;

    private final Invalidator mInvalidator = new Invalidator();
    private CompositorView mCompositorView;

    private boolean mContentOverlayVisiblity = true;
    private boolean mCanBeFocusable;

    private int mPendingFrameCount;

    private final ArrayList<Runnable> mPendingInvalidations = new ArrayList<>();
    private boolean mSkipInvalidation;

    /**
     * A task to be performed after a resize event.
     */
    private Runnable mPostHideKeyboardTask;

    private InsetObserverView mInsetObserverView;
    private ObservableSupplier<Integer> mAutofillUiBottomInsetSupplier;
    private boolean mShowingFullscreen;
    private Runnable mSystemUiFullscreenResizeRunnable;

    /**
     * The currently visible Tab.
     */
    @VisibleForTesting
    Tab mTabVisible;

//    /**
//     * The currently attached View.
//     */
//    private View mView;

    /**
     * Current ContentView. Updates when active tab is switched or WebContents is swapped
     * in the current Tab.
     */
    private final ArkContentView mContentView;

    // Cache objects that should not be created frequently.
    private final Rect mCacheRect = new Rect();
    private final Point mCachePoint = new Point();

    // If we've drawn at least one frame.
    private boolean mHasDrawnOnce;

    private boolean mControlsResizeView;
    private boolean mInGesture;
    private boolean mContentViewScrolling;
    private ApplicationViewportInsetSupplier mApplicationBottomInsetSupplier;
    private final org.chromium.base.Callback<Integer> mBottomInsetObserver = (inset) -> updateViewportSize();

    /**
     * Tracks whether geometrychange event is fired for the active tab when the keyboard
     * is shown/hidden. When active tab changes, this flag is reset so we can fire
     * geometrychange event for the new tab when the keyboard shows.
     */
    private boolean mHasKeyboardGeometryChangeFired;

    private OnscreenContentProvider mOnscreenContentProvider;

    private final Set<Runnable> mOnCompositorLayoutCallbacks = new HashSet<>();
    private final Set<Runnable> mDidSwapFrameCallbacks = new HashSet<>();
    private final Set<Runnable> mDidSwapBuffersCallbacks = new HashSet<>();

    /**
     * Last MotionEvent dispatched to this object for a currently active gesture. If there is no
     * active gesture, this is null.
     */
    private @Nullable
    MotionEvent mLastActiveTouchEvent;

    private final OnBackPressedCallback onBackPressedCallback = new OnBackPressedCallback(false) {
        @Override
        public void handleOnBackPressed() {
            ArkLogger.e(TAG, "handleOnBackPressed");
            if (mCallback != null) {
                ITabGroup tabGroup = mCallback.getTabList(mTabVisible);
                if (tabGroup.canGoBack()) {
                    tabGroup.goBack();
                    setEnabled(tabGroup.canGoBack());
                    return;
                }
            }
            setEnabled(false);
            org.chromium.utils.ContextUtils.activityFromContext(getContext()).onBackPressed();
        }
    };

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

    /**
     * Creates a {@link CompositorView}.
     *
     * @param c The Context to create this {@link CompositorView} in.
     */
    public ArkCompositorViewHolder(Context c) {
        super(c);
        internalInit();
        mContentView = ArkContentView.createContentView(c, null);
        initContentView();

        SwipeRefreshLayout swipeRefreshLayout = new SwipeRefreshLayout(c);
        addView(swipeRefreshLayout, new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        mSwipeRefreshHandler = new ArkSwipeRefreshHandler(swipeRefreshLayout);
    }

    /**
     * Creates a {@link CompositorView}.
     *
     * @param c     The Context to create this {@link CompositorView} in.
     * @param attrs The AttributeSet used to create this {@link CompositorView}.
     */
    public ArkCompositorViewHolder(Context c, AttributeSet attrs) {
        super(c, attrs);
        internalInit();
        mContentView = ArkContentView.createContentView(c, null);
        initContentView();


        SwipeRefreshLayout swipeRefreshLayout = new SwipeRefreshLayout(c);
        addView(swipeRefreshLayout, new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        mSwipeRefreshHandler = new ArkSwipeRefreshHandler(swipeRefreshLayout);
    }

    private void initContentView() {
        addView(mContentView, new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        mContentView.addOnHierarchyChangeListener(this);
        mContentView.addOnAttachStateChangeListener(new OnAttachStateChangeListener() {
            @Override
            public void onViewAttachedToWindow(View view) {
                if (mTabVisible != null) {
                    ((ArkTabImpl) mTabVisible).onViewAttachedToWindow(view);
                }
            }

            @Override
            public void onViewDetachedFromWindow(View view) {
                if (mTabVisible != null) {
                    ((ArkTabImpl) mTabVisible).onViewDetachedFromWindow(view);
                }
            }
        });
    }

    @Override
    public PointerIcon onResolvePointerIcon(MotionEvent event, int pointerIndex) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) return null;
        View activeView = mContentView;
        if (activeView == null || !ViewCompat.isAttachedToWindow(activeView)) return null;
        return ApiHelperForN.onResolvePointerIcon(activeView, event, pointerIndex);
    }

    private void internalInit() {
        mEventOffsetHandler =
                new EventOffsetHandler(new EventOffsetHandler.EventOffsetHandlerDelegate() {
                    // Cache objects that should not be created frequently.
                    private final RectF mCacheViewport = new RectF();

                    @Override
                    public float getTop() {
                        if (mLayoutManager != null) mLayoutManager.getViewportPixel(mCacheViewport);
                        return mCacheViewport.top;
                    }

                    @Override
                    public void setCurrentTouchEventOffsets(float top) {
                        if (mTabVisible == null) return;
                        WebContents webContents = mTabVisible.getWebContents();
                        if (webContents == null) return;
                        EventForwarder forwarder = webContents.getEventForwarder();
                        forwarder.setCurrentTouchEventOffsets(0, top);
                    }
                });

        addOnLayoutChangeListener(new OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                                       int oldLeft, int oldTop, int oldRight, int oldBottom) {
                Tab tab = getCurrentTab();
                // Set the size of NTP if we're in the attached state as it may have not been sized
                // properly when initializing tab. See the comment in #initializeTab() for why.
                if (tab != null && tab.isNativePage() && isAttachedToWindow(mContentView)) {
                    Point viewportSize = getViewportSize();
                    setSize(tab.getWebContents(), mContentView, viewportSize.x, viewportSize.y);
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
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        setOnSystemUiVisibilityChangeListener(new OnSystemUiVisibilityChangeListener() {
            @Override
            public void onSystemUiVisibilityChange(int visibility) {
                handleSystemUiVisibilityChange();
            }
        });
        handleSystemUiVisibilityChange();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            ApiHelperForO.setDefaultFocusHighlightEnabled(this, false);
        }
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
        ArkLogger.e(TAG, "getViewportSize width=%s, height=%s, mCacheRect=%s, mCachePoint=%s", getWidth(), getHeight(), mCacheRect, mCachePoint);
        return mCachePoint;
    }

    private void handleSystemUiVisibilityChange() {
        View view = mContentView;
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
            mSystemUiFullscreenResizeRunnable = this::handleWindowInsetChanged;
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
     * @param view The root view of the hierarchy.
     */
    public void setRootView(View view) {
        mCompositorView.setRootView(view);
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
            handleWindowInsetChanged();
        }
    }

    /**
     * A supplier providing an inset that resizes the page in addition or instead of the keyboard.
     * This is inset is used by autofill UI as addition to bottom controls.
     *
     * @param autofillUiBottomInsetSupplier A {@link ObservableSupplier<Integer>}.
     */
    public void setAutofillUiBottomInsetSupplier(
            ObservableSupplier<Integer> autofillUiBottomInsetSupplier) {
        mAutofillUiBottomInsetSupplier = autofillUiBottomInsetSupplier;
        mAutofillUiBottomInsetSupplier.addObserver(mBottomInsetObserver);
    }

    @Override
    public void onInsetChanged(int left, int top, int right, int bottom) {
        if (mShowingFullscreen) handleWindowInsetChanged();
    }

    private void handleWindowInsetChanged() {
        // Notify the WebContents that the size has changed.
        View contentView = mContentView;
        if (contentView != null) {
            Point viewportSize = getViewportSize();
            setSize(getWebContents(), contentView, viewportSize.x, viewportSize.y);
        }
        // Notify the compositor layout that the size has changed.  The layout does not drive
        // the WebContents sizing, so this needs to be done in addition to the above size update.
        onViewportChanged();
    }

    @Override
    public void onSafeAreaChanged(Rect area) {
    }

    /**
     * Should be called for cleanup when the CompositorView instance is no longer used.
     */
    public void shutDown() {
        onBackPressedCallback.setEnabled(false);
        setTab(null);
        if (mApplicationBottomInsetSupplier != null && mBottomInsetObserver != null) {
            mApplicationBottomInsetSupplier.removeObserver(mBottomInsetObserver);
        }
        if (mAutofillUiBottomInsetSupplier != null && mBottomInsetObserver != null) {
            mAutofillUiBottomInsetSupplier.removeObserver(mBottomInsetObserver);
        }

        mCompositorView.shutDown();
        if (mLayoutManager != null) mLayoutManager.destroy();
        if (mInsetObserverView != null) {
            mInsetObserverView.removeObserver(this);
            mInsetObserverView = null;
        }
        if (mOnscreenContentProvider != null) mOnscreenContentProvider.destroy();
        mContentView.removeOnHierarchyChangeListener(this);

        if (mTabContentManager != null) {
            mTabContentManager.destroy();
            mTabContentManager = null;
        }

        mSwipeRefreshHandler.destroy();

        if (mCallback != null) {
            mCallback.onShutDown();
        }
    }

//    /**
//     * This is called when the native library are ready.
//     */
//    public void onNativeLibraryReady(
//            WindowAndroid windowAndroid, TabContentManager tabContentManager) {
//        mCompositorView.initNativeCompositor(
//                SysUtils.isLowEndDevice(), windowAndroid, tabContentManager);
//
//        mApplicationBottomInsetSupplier = windowAndroid.getApplicationBottomInsetProvider();
//        mApplicationBottomInsetSupplier.addObserver(mBottomInsetObserver);
//    }

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
     * @return The {@link Invalidator} instance that is driven by this {@link ArkCompositorViewHolder}.
     */
    public Invalidator getInvalidator() {
        return mInvalidator;
    }

    /**
     * Add observer that needs to listen and process touch events.
     *
     * @param o {@link TouchEventObserver} object.
     */
    public void addTouchEventObserver(TouchEventObserver o) {
        mTouchEventObservers.addObserver(o);
    }

    /**
     * Remove observer that needs to listen and process touch events.
     *
     * @param o {@link TouchEventObserver} object.
     */
    public void removeTouchEventObserver(TouchEventObserver o) {
        mTouchEventObservers.removeObserver(o);
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent e) {
        super.onInterceptTouchEvent(e);
        for (TouchEventObserver o : mTouchEventObservers) {
            if (o.shouldInterceptTouchEvent(e)) return true;
        }

        updateIsInGesture(e);

        if (mLayoutManager == null) return false;

        mEventOffsetHandler.onInterceptTouchEvent(e);
        return mLayoutManager.onInterceptTouchEvent(e, mIsKeyboardShowing);
    }

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        super.onTouchEvent(e);

        updateIsInGesture(e);
        boolean consumed = mLayoutManager != null && mLayoutManager.onTouchEvent(e);
        mEventOffsetHandler.onTouchEvent(e);
        return consumed;
    }

    private void updateIsInGesture(MotionEvent e) {
        int eventAction = e.getActionMasked();
        if (eventAction == MotionEvent.ACTION_DOWN
                || eventAction == MotionEvent.ACTION_POINTER_DOWN) {
            mInGesture = true;
        } else if (eventAction == MotionEvent.ACTION_CANCEL
                || eventAction == MotionEvent.ACTION_UP) {
            mInGesture = false;
            updateViewportSize();
        }
    }

    @Override
    public boolean onInterceptHoverEvent(MotionEvent e) {
        mEventOffsetHandler.onInterceptHoverEvent(e);
        return super.onInterceptHoverEvent(e);
    }

    @Override
    public boolean dispatchHoverEvent(MotionEvent e) {
        return super.dispatchHoverEvent(e);
    }

    @Override
    public boolean dispatchDragEvent(DragEvent e) {
        mEventOffsetHandler.onPreDispatchDragEvent(e.getAction());
        boolean ret = super.dispatchDragEvent(e);
        mEventOffsetHandler.onPostDispatchDragEvent(e.getAction());
        return ret;
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent e) {
        updateLastActiveTouchEvent(e);
        for (TouchEventObserver o : mTouchEventObservers) o.handleTouchEvent(e);
        return super.dispatchTouchEvent(e);
    }

    private void updateLastActiveTouchEvent(MotionEvent e) {
        if (e.getActionMasked() == MotionEvent.ACTION_MOVE
                || e.getActionMasked() == MotionEvent.ACTION_DOWN
                || e.getActionMasked() == MotionEvent.ACTION_POINTER_DOWN
                || e.getActionMasked() == MotionEvent.ACTION_POINTER_UP) {
            mLastActiveTouchEvent = e;
        }
        if (e.getActionMasked() == MotionEvent.ACTION_CANCEL
                || e.getActionMasked() == MotionEvent.ACTION_UP) {
            mLastActiveTouchEvent = null;
        }
    }

    /**
     * @return The {@link LayoutManagerImpl} associated with this view.
     */
    public ArkLayoutManager getLayoutManager() {
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
//        if (mLayoutManager == null) return null;
//
//        IPage currentPage = TabListManager.getInstance().getCurrentPage();
//        if (currentPage == null) {
//            return null;
//        }
//
//        Tab currentTab = currentPage.getNativePage();
//
//        // If the tab model selector doesn't know of a current tab, use the last visible one.
//        if (currentTab == null) currentTab = mTabVisible;
//
//        return currentTab;
        return mTabVisible;
    }

    @Nullable
    public ContentView getContentView() {
        return mContentView;
    }

    public OverscrollRefreshHandler getSwipeRefreshHandler() {
        return mSwipeRefreshHandler;
    }

    protected WebContents getWebContents() {
        Tab tab = getCurrentTab();
        return tab != null ? tab.getWebContents() : null;
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        Tab tab = getCurrentTab();
        if (tab == null) {
            return;
        }
        Point viewportSize = getViewportSize();
        setSize(tab.getWebContents(), mContentView, viewportSize.x, viewportSize.y);
    }

    /**
     * Set tab-backed content view size.
     *
     * @param webContents {@link WebContents} for which the size of the view is set.
     * @param view        {@link View} of the content.
     * @param w           Width of the view.
     * @param h           Height of the view.
     */
    @VisibleForTesting
    void setSize(WebContents webContents, View view, int w, int h) {
        ArkLogger.d(TAG, "setSize w=" + w + " h=" + h + " webContents=" + webContents + " view=" + view);
        if (webContents == null || view == null) return;

        // The view size takes into account of the browser controls whose height
        // should be subtracted from the view if they are visible, therefore shrink
        // Blink-side view size.
        // TODO(https://crbug.com/1211066): Centralize the logic for calculating bottom insets.
        final int totalMinHeight = getKeyboardBottomInsetForControlsPixels();
        int controlsHeight = mControlsResizeView
                ? getTopControlsHeightPixels() + getBottomControlsHeightPixels()
                : totalMinHeight;

        if (isAttachedToWindow(view)) {
            // If overlay content flag is set and the keyboard is shown or hidden then resize the
            // visual/layout viewports in WebContents to match the previous size so there
            // isn't a change in size after the keyboard is raised or hidden.
            // Also the geometrychange event should only fire to the foreground tab.
            int keyboardHeight = 0;
            boolean overlayContentForegroundTab = shouldVirtualKeyboardOverlayContent(webContents);
            if (overlayContentForegroundTab) {
                // During orientation changes, width of the |WebContents| changes to match the width
                // of the screen and so does the keyboard. We fire geometrychange with the updated
                // keyboard size as well as resize the viewport so the height resize doesn't affect
                // the |WebContents|.
                keyboardHeight = KeyboardVisibilityDelegate.getInstance().calculateKeyboardHeight(
                        this.getRootView());
                h += keyboardHeight;
            }
            webContents.setSize(w, h - controlsHeight);
            if (overlayContentForegroundTab) {
                notifyVirtualKeyboardOverlayGeometryChangeEvent(w, keyboardHeight, webContents);
            }
        } else {
            setSizeOfUnattachedView(view, webContents, controlsHeight);
            requestRender();
        }
    }

    private static boolean isAttachedToWindow(View view) {
        return view != null && view.getWindowToken() != null;
    }

    /**
     * Returns true if the overlaycontent flag is set in the JS, else false.
     * This determines whether to fire geometrychange event to JS for the current visible tab
     * and also not resize the visual/layout viewports in response to keyboard visibility changes.
     *
     * @return Whether overlaycontent flag is set or not.
     */
    @VisibleForTesting
    boolean shouldVirtualKeyboardOverlayContent(WebContents webContents) {
        return webContents != null && mTabVisible != null
                && mTabVisible.getWebContents() == webContents
                && ImeAdapter.fromWebContents(webContents) != null
                && ImeAdapter.fromWebContents(webContents).shouldVirtualKeyboardOverlayContent();
    }

    /**
     * Notifies geometrychange event to JS.
     *
     * @param w              Width of the view.
     * @param keyboardHeight Height of the keyboard.
     * @param webContents    Active WebContent for which this event needs to be fired.
     */
    private void notifyVirtualKeyboardOverlayGeometryChangeEvent(
            int w, int keyboardHeight, WebContents webContents) {
        assert shouldVirtualKeyboardOverlayContent(webContents);

        boolean keyboardVisible = keyboardHeight > 0;
        if (!keyboardVisible && !mHasKeyboardGeometryChangeFired) {
            return;
        }

        mHasKeyboardGeometryChangeFired = keyboardVisible;
        Rect appRect = new Rect();
        getRootView().getWindowVisibleDisplayFrame(appRect);
        if (keyboardVisible) {
            // Fire geometrychange event to JS.
            // The assumption here is that the keyboard is docked at the bottom so we use the
            // root visible window frame's origin to calculate the position of the keyboard.
            notifyVirtualKeyboardOverlayRect(
                    webContents, appRect.left, appRect.top, w, keyboardHeight);
        } else {
            // Keyboard has hidden.
            notifyVirtualKeyboardOverlayRect(webContents, 0, 0, 0, 0);
        }
    }

    @Override
    public void onSurfaceResized(int width, int height) {
        View view = mContentView;
        WebContents webContents = getWebContents();
        if (view == null || webContents == null) return;
        onPhysicalBackingSizeChanged(webContents, width, height);
    }

    private void onPhysicalBackingSizeChanged(WebContents webContents, int width, int height) {
        if (mCompositorView != null) {
            mCompositorView.onPhysicalBackingSizeChanged(webContents, width, height);
        }
    }

    private void onControlsResizeViewChanged(WebContents webContents, boolean controlsResizeView) {
        if (webContents != null && mCompositorView != null) {
            mCompositorView.onControlsResizeViewChanged(webContents, controlsResizeView);
        }
    }

    /**
     * Fires geometrychange event to JS with the keyboard size.
     *
     * @param webContents Active WebContent for which this event needs to be fired.
     * @param x           When the keyboard is shown, it has the left position of the app's rect, else, 0.
     * @param y           When the keyboard is shown, it has the top position of the app's rect, else, 0.
     * @param width       When the keyboard is shown, it has the width of the view, else, 0.
     * @param height      The height of the keyboard.
     */
    @VisibleForTesting
    void notifyVirtualKeyboardOverlayRect(
            WebContents webContents, int x, int y, int width, int height) {
        if (mCompositorView != null) {
            mCompositorView.notifyVirtualKeyboardOverlayRect(webContents, x, y, width, height);
        }
    }

    /**
     * Called whenever the host activity is started.
     */
    public void onStart() {
        requestRender();
    }

    /**
     * Called whenever the host activity is stopped.
     */
    public void onStop() {
    }

    @Override
    public void onControlsOffsetChanged(int topOffset, int topControlsMinHeightOffset,
                                        int bottomOffset, int bottomControlsMinHeightOffset, boolean needsAnimate) {
        onViewportChanged();
        if (needsAnimate) requestRender();
        updateContentViewChildrenDimension();
    }

    @Override
    public void onBottomControlsHeightChanged(
            int bottomControlsHeight, int bottomControlsMinHeight) {
        if (mTabVisible == null) return;
        onBrowserControlsHeightChanged();
        Point viewportSize = getViewportSize();
        setSize(mTabVisible.getWebContents(), mContentView, viewportSize.x,
                viewportSize.y);
        onViewportChanged();
    }

    @Override
    public void onTopControlsHeightChanged(int topControlsHeight, int topControlsMinHeight) {
        if (mTabVisible == null) return;
        onBrowserControlsHeightChanged();
        Point viewportSize = getViewportSize();
        setSize(mTabVisible.getWebContents(), mContentView, viewportSize.x,
                viewportSize.y);
        onViewportChanged();
    }

    /**
     * Notify the {@link WebContents} of the browser controls height changes. Unlike #setSize, this
     * will make sure the renderer's properties are updated even if the size didn't change.
     */
    private void onBrowserControlsHeightChanged() {
        final WebContents webContents = getWebContents();
        if (webContents == null) return;
        webContents.notifyBrowserControlsHeightChanged();
    }

    /**
     * Updates viewport size to have it render the content correctly.
     */
    private void updateViewportSize() {
        if (mInGesture || mContentViewScrolling) return;
        boolean controlsResizeViewChanged = false;
        // Reflect the changes that may have happened in in view/control size.
        Point viewportSize = getViewportSize();
        setSize(getWebContents(), mContentView, viewportSize.x, viewportSize.y);
        if (controlsResizeViewChanged) {
            // Send this after setSize, so that RenderWidgetHost doesn't SynchronizeVisualProperties
            // in a partly-updated state.
            onControlsResizeViewChanged(getWebContents(), mControlsResizeView);
        }
    }

    // View.OnHierarchyChangeListener implementation

    @Override
    public void onChildViewRemoved(View parent, View child) {
        updateContentViewChildrenDimension();
    }

    @Override
    public void onChildViewAdded(View parent, View child) {
        updateContentViewChildrenDimension();
    }

    private void updateContentViewChildrenDimension() {
        TraceEvent.begin("CompositorViewHolder:updateContentViewChildrenDimension");
        ViewGroup view = mContentView;
        if (view != null) {
            float topViewsTranslation = getOverlayTranslateY();
            float bottomMargin = 0f;
            applyTranslationToTopChildViews(view, topViewsTranslation);
            applyMarginToFullscreenChildViews(view, topViewsTranslation, bottomMargin);
            updateViewportSize();
        }
        TraceEvent.end("CompositorViewHolder:updateContentViewChildrenDimension");
    }

    private static void applyMarginToFullscreenChildViews(
            ViewGroup contentView, float topMargin, float bottomMargin) {
        for (int i = 0; i < contentView.getChildCount(); i++) {
            View child = contentView.getChildAt(i);
            if (!(child.getLayoutParams() instanceof LayoutParams)) continue;
            LayoutParams layoutParams =
                    (LayoutParams) child.getLayoutParams();

            if (layoutParams.height == LayoutParams.MATCH_PARENT
                    && (layoutParams.topMargin != (int) topMargin
                    || layoutParams.bottomMargin != (int) bottomMargin)) {
                layoutParams.topMargin = (int) topMargin;
                layoutParams.bottomMargin = (int) bottomMargin;
                child.requestLayout();
                TraceEvent.instant("FullscreenManager:child.requestLayout()");
            }
        }
    }

    private static void applyTranslationToTopChildViews(ViewGroup contentView, float translation) {
        for (int i = 0; i < contentView.getChildCount(); i++) {
            View child = contentView.getChildAt(i);
            if (!(child.getLayoutParams() instanceof LayoutParams)) continue;

            LayoutParams layoutParams =
                    (LayoutParams) child.getLayoutParams();
            if (Gravity.TOP == (layoutParams.gravity & Gravity.FILL_VERTICAL)) {
                child.setTranslationY(translation);
                TraceEvent.instant("FullscreenManager:child.setTranslationY()");
            }
        }
    }

    /**
     * Sets the overlay mode.
     */
    public void setOverlayMode(boolean useOverlayMode) {
        if (mCompositorView != null) {
            mCompositorView.setOverlayVideoMode(useOverlayMode);
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
        ArkLogger.e(TAG, "onCompositorLayout");
        if (mLayoutManager != null) {
            mLayoutManager.onUpdate();
            mCompositorView.finalizeLayers(mLayoutManager, false);
        }

        mDidSwapFrameCallbacks.addAll(mOnCompositorLayoutCallbacks);
        mOnCompositorLayoutCallbacks.clear();
        updateNeedsSwapBuffersCallback();

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

        outRect.bottom -= getBottomControlsHeightPixels();
    }

    @Override
    public void getViewportFullControls(RectF outRect) {
        getWindowViewport(outRect);

        outRect.bottom -= getBottomControlsHeightPixels();
    }

    @Override
    public float getHeightMinusBrowserControls() {
        return getHeight() - (getTopControlsHeightPixels() + getBottomControlsHeightPixels());
    }

    @Override
    public void requestRender() {
        ArkLogger.e(TAG, "requestRender");
        requestRender(null);
    }

    @Override
    public void requestRender(Runnable onUpdateEffective) {
        if (onUpdateEffective != null) {
            mOnCompositorLayoutCallbacks.add(onUpdateEffective);
            updateNeedsSwapBuffersCallback();
        }
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
        if (mHasDrawnOnce) {
            post(new Runnable() {
                @Override
                public void run() {
                    mCompositorView.setBackgroundResource(0);
                }
            });
        }

        mHasDrawnOnce = true;

        mPendingFrameCount = pendingFrameCount;

        if (!mSkipInvalidation || pendingFrameCount == 0) flushInvalidation();
        mSkipInvalidation = !mSkipInvalidation;

        mDidSwapBuffersCallbacks.addAll(mDidSwapFrameCallbacks);
        mDidSwapFrameCallbacks.clear();
        updateNeedsSwapBuffersCallback();
    }

    @Override
    public void didSwapBuffers(boolean swappedCurrentSize) {
        for (Runnable runnable : mDidSwapBuffersCallbacks) {
            runnable.run();
        }
        mDidSwapBuffersCallbacks.clear();
        updateNeedsSwapBuffersCallback();
    }

    @Override
    public void setContentOverlayVisibility(boolean show, boolean canBeFocusable) {
        if (show != mContentOverlayVisiblity || canBeFocusable != mCanBeFocusable) {
            mContentOverlayVisiblity = show;
            mCanBeFocusable = canBeFocusable;
//            updateContentOverlayVisibility(mContentOverlayVisiblity);
            if (!mContentOverlayVisiblity) {
                setFocusable(mCanBeFocusable);
                setFocusableInTouchMode(mCanBeFocusable);
            }
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
    public void loadPersitentTextureDataIfNeeded() {
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        mIsKeyboardShowing =
                KeyboardVisibilityDelegate.getInstance().isKeyboardShowing(getContext(), this);
    }

    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        if (changed) onViewportChanged();
        ArkLogger.e(TAG, "onLayout l=%s, t=%s, r=%s, b=%s", l, t, r, b);
        super.onLayout(changed, l, t, r, b);
    }

    @Override
    public void clearChildFocus(View child) {
        // Override this method so that the ViewRoot doesn't go looking for a new
        // view to take focus. It will find the URL Bar, focus it, then refocus this
        // later, causing a keyboard flicker.
    }

    @Override
    public BrowserControlsManager getBrowserControlsManager() {
        return null;
    }

    @Override
    public FullscreenManager getFullscreenManager() {
        return null;
    }

    @Override
    public int getTopControlsHeightPixels() {
        return 0;
    }

    @Override
    public int getBottomControlsHeightPixels() {
        return getKeyboardBottomInsetForControlsPixels();
    }

    /**
     * If there is keyboard extension or replacement available, this method returns the inset that
     * resizes the page in addition to the bottom controls height.
     *
     * @return The inset height in pixels.
     */
    private int getKeyboardBottomInsetForControlsPixels() {
        return mAutofillUiBottomInsetSupplier != null
                && mAutofillUiBottomInsetSupplier.get() != null
                ? mAutofillUiBottomInsetSupplier.get()
                : 0;
    }

    /**
     * @return {@code true} if browser controls shrink Blink view's size.
     */
    public boolean controlsResizeView() {
        return mControlsResizeView;
    }

    @Override
    public float getOverlayTranslateY() {
        return 0f;
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
    }

    @Override
    public void hideKeyboard(Runnable postHideTask) {
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

    public WindowAndroid getWindowAndroid() {
        return mWindowAndroid;
    }

    public boolean onBackPressed() {
        if (mCallback != null) {
            ITabGroup tabGroup = mCallback.getTabList(mTabVisible);
            if (tabGroup.canGoBack()) {
                tabGroup.goBack();
                onBackPressedCallback.setEnabled(tabGroup.canGoBack());
                return true;
            }
        }
        onBackPressedCallback.setEnabled(false);
        return false;
    }

    /**
     * This is called when the native library are ready.
     */
    public void initCompositor(ArkWindowAndroid window, Callback callback) {
        mWindowAndroid = window;
        this.mCallback = callback;
        Activity activity = window.getActivity().get();
        ((FragmentActivity) activity).getOnBackPressedDispatcher()
                .addCallback((FragmentActivity) activity, onBackPressedCallback);
        mTabContentManager = new TabContentManager(activity);
        mTabContentManager.initWithNative();

        mCompositorView.initNativeCompositor(
                SysUtils.isLowEndDevice(), window, mTabContentManager);

        mLayoutManager = new ArkLayoutManager(this);
        mTabContentManager.addThumbnailChangeListener(new TabContentManager.ThumbnailChangeListener() {
            @Override
            public void onThumbnailChange(int id) {
                mLayoutManager.requestUpdate();
            }
        });

        mLayoutManager.init(mTabContentManager,
                mCompositorView.getResourceManager().getDynamicResourceLoader());

        onViewportChanged();

    }

//    private void updateContentOverlayVisibility(boolean show) {
//        if (mView == null) return;
//        WebContents webContents = getWebContents();
//        if (show) {
//            if (mView != getCurrentTab().getView() || mView.getParent() == this) return;
//            // During tab creation, we temporarily add the new tab's view to a FrameLayout to
//            // measure and lay it out. This way we could show the animation in the stack view.
//            // Therefore we should remove the view from that temporary FrameLayout here.
//            UiUtils.removeViewFromParent(mView);
//
//            if (webContents != null) {
//                assert !webContents.isDestroyed();
//                mContentView.setVisibility(View.VISIBLE);
//                updateViewportSize();
//            }
//
//            ArkLogger.e(TAG, "updateContentOverlayVisibility addView");
//
//            // CompositorView always has index of 0.
//            // TODO(crbug.com/1216949): Look into enforcing the z-order of the views.
//            addView(mView, 1);
//
//            setFocusable(false);
//            setFocusableInTouchMode(false);
//
//            // Claim focus for the new view unless the user is currently using the URL bar.
//            mView.requestFocus();
//        } else {
//            if (mView.getParent() == this) {
//                setFocusable(mCanBeFocusable);
//                setFocusableInTouchMode(mCanBeFocusable);
//
//                if (webContents != null && !webContents.isDestroyed()) {
//                    View contentView = mContentView;
//                    if (contentView != null) {
//                        contentView.setVisibility(INVISIBLE);
//                    }
//                }
//                removeView(mView);
//                mView = null;
//            }
//        }
//    }

    private final ArkTabWebContentsObserver.Callback mInitWebContentsObserver = (tab, webContents) -> {
        SelectionPopupController controller =
                SelectionPopupController.fromWebContents(webContents);
        controller.setActionModeCallback(new ChromeActionModeHandler.ActionModeCallback(
                        tab, webContents,
                        new Consumer<Boolean>() {
                            @Override
                            public void accept(Boolean aBoolean) {
                                Toast.makeText(ContextUtils.getApplicationContext(), "mActionBarObserver show：" + aBoolean, Toast.LENGTH_SHORT).show();
                            }
                        },
                        new org.chromium.base.Callback<String>() {
                            @Override
                            public void onResult(String result) {
                                Toast.makeText(ContextUtils.getApplicationContext(), "搜索：" + result, Toast.LENGTH_SHORT).show();
                            }
                        })
                );

//        final SmartSearchPanel smartSearchDialog = tab.getWindowAndroid().getActivity().get()
//                .findViewById(R.id.layout_smart_search);
        controller.setSelectionClient(new SelectionClient() {

            private SmartSearchPopupWindow smartSearchDialog;

//            private SmartSearchPanel smartSearchDialog;

            @Override
            public ActionMode startActionMode(View view, ActionModeCallbackHelper helper, ActionMode.Callback callback) {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                    return new ArkCustomActionMode((ViewGroup) view,
                            new FloatingActionModeCallback(helper, callback));
                }
                return null;
            }

            @Override
            public void onSelectionChanged(String selection) {
                ArkLogger.e(TAG, "onSelectionChanged selection=" + selection);
                if (TextUtils.isEmpty(selection)) {
                    hide();
                } else {
                    show();
                    if (smartSearchDialog != null) {
                        smartSearchDialog.updateKeyword(selection);
                    }
                }
            }

            @Override
            public void onSelectionEvent(@SelectionEventType int eventType, float posXPix, float posYPix) {
                ArkLogger.e(TAG, "onSelectionEvent eventType=" + eventType);
                if (SelectionEventType.SELECTION_HANDLES_SHOWN == eventType) {
                    show();
                } else if (SelectionEventType.SELECTION_HANDLES_CLEARED == eventType) {
                    hide();
                }
            }

            @Override
            public void selectAroundCaretAck(@Nullable SelectAroundCaretResult result) {}

            @Override
            public boolean requestSelectionPopupUpdates(boolean shouldSuggest) {
                return false;
            }

            @Override
            public void cancelAllRequests() {}

            @SuppressLint("ClickableViewAccessibility")
            private void show() {
                ArkLogger.e(TAG, "smartSearchDialog show");
                if (smartSearchDialog == null) {
                    smartSearchDialog = new SmartSearchPopupWindow(tab.getContext());
                    smartSearchDialog.setOnDismissListener(() -> {
                        smartSearchDialog = null;
                        controller.clearSelection();
                    });
                    smartSearchDialog.setOnPanelStateChangedListener(new SmartSearchPanel.OnPanelStateChangedListener() {

                        private boolean focused = true;

                        @Override
                        public void onStateChanged(SmartSearchPanel panel) {
                            if (focused != panel.isClosed()) {
                                focused = panel.isClosed();
                                controller.updateTextSelectionUI(focused);
                            }
                        }
                    });
                    smartSearchDialog.setOnTouchListener((View v, MotionEvent event) -> {
                        ArkLogger.e(TAG, "smartSearchDialog OnTouchListener rootView=" +
                                ArkCompositorViewHolder.this.getRootView());
                        ArkCompositorViewHolder.this.getRootView().dispatchTouchEvent(event);
                        return true;
                    });
                }
                smartSearchDialog.show();

//                if (smartSearchDialog.getVisibility() != VISIBLE) {
//                    smartSearchDialog.show();
//                }


//                if (smartSearchDialog == null) {
//                    smartSearchDialog = (SmartSearchPanel) LayoutInflater.from(activity).inflate(R.layout.layout_smart_search, null, false);
//                    manager.addView(smartSearchDialog, new ViewGroup.LayoutParams(
//                            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
//                    smartSearchDialog.show();
//                }

            }

            private void hide() {

                if (smartSearchDialog == null) {
                    return;
                }
                ArkLogger.e(TAG, "smartSearchDialog hide isShowing=" + smartSearchDialog.isShowing());
                if (smartSearchDialog.isShowing()) {
                    smartSearchDialog.dismiss();
                    smartSearchDialog = null;
                }

//                if (smartSearchDialog.getVisibility() == VISIBLE) {
////                    smartSearchDialog.hide();
//                    smartSearchDialog.setVisibility(INVISIBLE);
//                }

//                if (smartSearchDialog != null) {
//                    manager.removeView(smartSearchDialog);
//                    smartSearchDialog = null;
//                }
            }

        });
    };

    public void setTab(Tab tab) {

        // The StartSurfaceUserData.getInstance().getUnusedTabRestoredAtStartup() is only true when
        // the Start surface is showing in the startup and there isn't any Tab opened. Thus, no
        // Tab needs to be loaded. Once a new Tab is opening and Start surface is hiding, this flag
        // will be reset.
//        if (tab != null) {
//            tab.loadIfNeeded();
//        }

        if (mTabVisible == null && tab == null) {
            return;
        }

//        View newView = tab != null ? tab.getView() : null;
//        ArkLogger.d(TAG, "setTab tab=" + tab + " view=" + newView + " mContentOverlayVisiblity=" + mContentOverlayVisiblity);
//        if (newView != null && mView == newView) {
//            if (mCallback != null) {
//                ITabGroup tabGroup = mCallback.getTabList(mTabVisible);
//                onBackPressedCallback.setEnabled(tabGroup.canGoBack());
//            } else {
//                setEnabled(false);
//            }
//            return;
//        }

        // TODO(dtrainor): Look into changing this only if the views differ, but still parse the
        // WebContents list even if they're the same.
//        updateContentOverlayVisibility(false);

        if (mTabVisible != tab) {
            // Reset the geometrychange event flag so it can fire on the current active tab.
            mHasKeyboardGeometryChangeFired = false;
            if (mTabVisible != null) {
                if (mTabVisible.isInitialized()) {
                    ArkTabWebContentsObserver.from(mTabVisible)
                            .removeInitWebContentsObserver(mInitWebContentsObserver);
                }
                mTabVisible.hide(TabHidingType.CHANGED_TABS);
                mTabContentManager.detachTab(mTabVisible);
                mTabVisible.removeObserver(mTabObserver);
                if (mCallback != null) {
                    mCallback.onPageDetached(mTabVisible);
                }
                ((ArkTabImpl) mTabVisible).updateWindowAndroid(null);
//                mView = null;
            }

            mTabVisible = tab;
            if (mTabVisible != null) {
                mTabVisible.addObserver(mTabObserver);
                ((ArkTabImpl) mTabVisible).updateWindowAndroid(mWindowAndroid);
                mTabContentManager.attachTab(mTabVisible);
                mCompositorView.onTabChanged();
                mLayoutManager.initLayoutTabFromHost(mTabVisible.getId());
                ArkTabWebContentsObserver.from(mTabVisible).addInitWebContentsObserver(
                        mInitWebContentsObserver);
            }
        }

        mSwipeRefreshHandler.setTab((ArkTabImpl) mTabVisible);
        if (mTabVisible != null) {
            mContentView.setWebContents(mTabVisible.getWebContents());
            mTabVisible.loadIfNeeded();
            initializeTab(mTabVisible);
            mLayoutManager.onPageSelected(mTabVisible);
            mTabVisible.show(TabSelectionType.FROM_USER);
            if (mCallback != null) {
                mCallback.onPageAttached(mTabVisible);
            }
//            mView = mTabVisible.getView();
        }
//        updateContentOverlayVisibility(mContentOverlayVisiblity);

        setFocusable(false);
        setFocusableInTouchMode(false);
        mContentView.requestFocus();

        if (mOnscreenContentProvider == null) {
            mOnscreenContentProvider =
                    new OnscreenContentProvider(getContext(), this, getWebContents());
        } else {
            mOnscreenContentProvider.onWebContentsChanged(getWebContents());
        }

//        mLayoutManager.showLayout(LayoutType.BROWSING, false);

        if (mCallback != null) {
            ITabGroup tabGroup = mCallback.getTabList(mTabVisible);
            onBackPressedCallback.setEnabled(tabGroup.canGoBack());
        } else {
            setEnabled(false);
        }

    }

    /**
     * Sets the correct size for {@link View} on {@code tab} and sets the correct rendering
     * parameters on {@link WebContents} on {@code tab}.
     *
     * @param tab The {@link Tab} to initialize.
     */
    private void initializeTab(Tab tab) {
        WebContents webContents = tab.getWebContents();
        ArkLogger.e(TAG, "initializeTab webContents=" + webContents);
        if (webContents != null) {
//            webContents.setViewAndroidDelegate(new ArkTabViewAndroidDelegate(tab, mContentView));
//            webContents.setAccessDelegate(mContentView);
            onPhysicalBackingSizeChanged(
                    webContents, mCompositorView.getWidth(), mCompositorView.getHeight());
            onControlsResizeViewChanged(webContents, mControlsResizeView);
        }
        ArkLogger.d(TAG, "initializeTab tab=" + tab + " view=" + mContentView);
//        if (tab.getView() == null) return;

        // TextView with compound drawables in the NTP gets a wrong width when measure/layout is
        // performed in the unattached state. Delay the layout till #onLayoutChange().
        // See https://crbug.com/876686.
        if (tab.isNativePage() && !isAttachedToWindow(mContentView)) return;
        Point viewportSize = getViewportSize();
        setSize(webContents, mContentView, viewportSize.x, viewportSize.y);
    }

    /**
     * Resize {@code view} to match the size of this {@link FrameLayout}.  This will only happen if
     * the {@link View} is not part of the view hierarchy.
     *
     * @param view           The {@link View} to resize.
     * @param webContents    {@link WebContents} associated with the view.
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
    public void deferInvalidate(Runnable clientInvalidator) {
        if (mPendingFrameCount <= 0) {
            clientInvalidator.run();
        } else if (!mPendingInvalidations.contains(clientInvalidator)) {
            mPendingInvalidations.add(clientInvalidator);
        }
    }

    private void flushInvalidation() {
        if (mPendingInvalidations.isEmpty()) return;
        TraceEvent.instant("CompositorViewHolder.flushInvalidation");
        for (int i = 0; i < mPendingInvalidations.size(); i++) {
            mPendingInvalidations.get(i).run();
        }
        mPendingInvalidations.clear();
    }

    // TabObscuringHandler.Observer

    @Override
    public void updateObscured(boolean isObscured) {
        setFocusable(!isObscured);
    }

    // Should be called any time inputs used to compute `needsSwapCallback` changes.
    private void updateNeedsSwapBuffersCallback() {
        boolean needsSwapCallback = !mOnCompositorLayoutCallbacks.isEmpty()
                || !mDidSwapFrameCallbacks.isEmpty() || !mDidSwapBuffersCallbacks.isEmpty();
        mCompositorView.setRenderHostNeedsDidSwapBuffersCallback(needsSwapCallback);
    }


    private TabDelegateFactory mTabDelegateFactory;

    public TabDelegateFactory getTabDelegateFactory() {
        if (mTabDelegateFactory == null) {
            mTabDelegateFactory = new ArkTabDelegateFactory(
                    null, getFullscreenManager(),
                    () -> ArkCompositorViewHolder.this);
        }
        return mTabDelegateFactory;
    }

    public TabContentManager getTabContentManager() {
        return mTabContentManager;
    }

//    public boolean openNewPage(@NonNull Tab parent, LoadUrlParams params) {
//        ArkLogger.d(TAG, "openNewPage params=" + params);
//        if (mCallback == null) {
//            return false;
//        }
//        ITabGroup tabList = mCallback.getTabList(parent);
//        return tabList.openNewPage(parent, params);
//    }

    public interface Callback {

//        boolean openNewPage(@NonNull Tab current, @TabLaunchType int type, String url);

        ITabGroup getTabList(Tab current);

        void onPageAttached(@NonNull Tab page);

        void onPageDetached(@NonNull Tab page);

        void onShutDown();

    }


    private static class ArkCustomActionMode extends ActionMode {

        private static final String TAG = "ArkCustomActionMode";

        private final Rect mContentRect = new Rect();

        private final Menu mMenu;


        private final Context mContext;
        private final ViewGroup mParent;
        private final ActionMode.Callback2 mCallback;

        private final View mMenuView;
        private final LinearLayout mContainer;

        private final MenuInflater mInflater;

        private ArkCustomActionMode(ViewGroup view, ActionMode.Callback2 callback) {

            this.mParent = view;
            this.mCallback = callback;

            Context context = view.getContext();
            this.mContext = context;

            mMenuView = LayoutInflater.from(context).inflate(
                    org.chromium.chrome.R.layout.menu_action_mode, null, false);
            mMenuView.setVisibility(INVISIBLE);

            mContainer = mMenuView.findViewById(org.chromium.chrome.R.id.ll_container);
            mMenu = new ActionMenuView(mContext).getMenu();
            mInflater = new MenuInflater(mContext);
            invalidate();
        }

        @Override
        public void hide(long duration) {
            mMenuView.setVisibility(View.INVISIBLE);
        }


        @Override
        public void setTitle(CharSequence title) {

        }

        @Override
        public void setTitle(int resId) {

        }

        @Override
        public void setSubtitle(CharSequence subtitle) {

        }

        @Override
        public void setSubtitle(int resId) {

        }

        @Override
        public void setCustomView(View view) {

        }

        @Override
        public void invalidate() {
            if (mMenuView.getParent() == null) {
                mParent.addView(mMenuView, new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
            }

            if (mMenu.size() == 0) {
                mCallback.onPrepareActionMode(this, mMenu);
                mContainer.removeAllViews();

                for (int i = 0; i < mMenu.size(); i++) {
                    MenuItem item = mMenu.getItem(i);
                    ArkLogger.e(TAG, "invalidate item=" + item.getTitle());
                    mContainer.addView(createItem(item));
                }
            }

            invalidateContentRect();
        }

        @Override
        public void invalidateContentRect() {
            mParent.post(this::invalidateContentRectInner2);
        }

        private void invalidateContentRectInner2() {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                mCallback.onGetContentRect(this, mMenuView, mContentRect);
            }

            int width = mParent.getWidth();
            int height = mParent.getHeight();

            int centerX = mContentRect.centerX();
            int centerY = mContentRect.centerY();

            ArkLogger.e(TAG, "invalidateContentRect width=" + mMenuView.getWidth() + " height=" + mMenuView.getHeight());
            ArkLogger.e(TAG, "invalidateContentRect mContentRect=" + mContentRect);

            final int x;
            final int y;

            if (centerX * 2 <= width) {
                if (mMenuView.getWidth() < centerX * 2) {
                    x = centerX - mMenuView.getWidth() / 2;
                } else {
                    x = 0;
                }
            } else {
                if (centerX + mMenuView.getWidth() / 2 > width) {
                    x = width - mMenuView.getWidth();
                } else {
                    x = centerX - mMenuView.getWidth() / 2;
                }
            }

            if (mContentRect.bottom + mMenuView.getHeight() > height) {
                y = mContentRect.top - mMenuView.getHeight();
            } else {
                y = mContentRect.bottom;
            }

            ArkLogger.e(TAG, "invalidateContentRect x=" + x + " y=" + y);

            mMenuView.setX(x);
            mMenuView.setY(y);
            mMenuView.setVisibility(View.VISIBLE);
        }

        private View createItem(MenuItem item) {
            TextView textView = (TextView) LayoutInflater.from(mContext)
                    .inflate(org.chromium.chrome.R.layout.item_menu, null, false);
            textView.setText(item.getTitle());
            textView.setOnClickListener(v -> mCallback.onActionItemClicked(ArkCustomActionMode.this, item));
            return textView;
        }

        @Override
        public void finish() {
            mParent.removeView(mMenuView);
            mCallback.onDestroyActionMode(this);
        }

        @Override
        public Menu getMenu() {
            return mMenu;
        }

        @Override
        public CharSequence getTitle() {
            return null;
        }

        @Override
        public CharSequence getSubtitle() {
            return null;
        }

        @Override
        public View getCustomView() {
            return null;
        }

        @Override
        public MenuInflater getMenuInflater() {
            return mInflater;
        }
    }

}
