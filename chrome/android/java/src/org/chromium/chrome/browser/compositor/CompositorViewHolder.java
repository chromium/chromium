// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor;

import static androidx.core.view.WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.RectF;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.SystemClock;
import android.util.AttributeSet;
import android.view.DragAndDropPermissions;
import android.view.DragEvent;
import android.view.MotionEvent;
import android.view.PointerIcon;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.accessibility.AccessibilityEvent;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;
import androidx.customview.widget.ExploreByTouchHelper;

import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.SysUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsUtils;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.layouts.EventFilter.EventType;
import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLoadIfNeededCaller;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementFieldTrial;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.chrome.browser.toolbar.ToolbarFeatures;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.browser_ui.widget.TouchEventObserver;
import org.chromium.components.browser_ui.widget.TouchEventProvider;
import org.chromium.components.content_capture.OnscreenContentProvider;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.prefs.PrefService;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.ApplicationViewportInsetSupplier;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.EventForwarder;
import org.chromium.ui.base.EventOffsetHandler;
import org.chromium.ui.base.SPenSupport;
import org.chromium.ui.base.UiAndroidFeatureList;
import org.chromium.ui.base.UiAndroidFeatureMap;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.ViewportInsets;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.mojom.VirtualKeyboardMode;
import org.chromium.ui.resources.ResourceManager;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * This class holds a {@link CompositorView}. This level of indirection is needed to benefit from
 * the {@link android.view.ViewGroup#onInterceptTouchEvent(android.view.MotionEvent)} capability on
 * available on {@link android.view.ViewGroup}s. This class also holds the {@link LayoutManagerImpl}
 * responsible to describe the items to be drawn by the UI compositor on the native side.
 */
public class CompositorViewHolder extends FrameLayout
        implements LayoutManagerHost,
                LayoutRenderHost,
                TouchEventProvider,
                BrowserControlsStateProvider.Observer,
                ChromeAccessibilityUtil.Observer,
                TabObscuringHandler.Observer,
                ViewGroup.OnHierarchyChangeListener {
    private static final long SYSTEM_UI_VIEWPORT_UPDATE_DELAY_MS = 500;

    /**
     * Initializer interface used to decouple initialization from the class that owns
     * the CompositorViewHolder.
     */
    public interface Initializer {
        /**
         * Initializes the {@link CompositorViewHolder} with the relevant content it needs to
         * properly show content on the screen.
         *
         * @param layoutManager A {@link LayoutManagerImpl} instance. This class is responsible for
         *     driving all high level screen content and determines which {@link Layout} is shown
         *     when.
         * @param urlBar The {@link View} representing the URL bar (must be focusable) or {@code
         *     null} if none exists.
         * @param controlContainer A {@link ControlContainer} instance to draw.
         */
        void initializeCompositorContent(
                LayoutManagerImpl layoutManager, View urlBar, ControlContainer controlContainer);
    }

    private final ObserverList<TouchEventObserver> mTouchEventObservers = new ObserverList<>();
    // Tracks current aggregated state of if the compositor is in motion. This could be an ongoing
    // touch by the user, or a scroll that's in progress.
    private final ObservableSupplierImpl<Boolean> mInMotionSupplier =
            new ObservableSupplierImpl<>();

    private boolean mIsKeyboardShowing;
    private boolean mNativeInitialized;
    private LayoutManagerImpl mLayoutManager;
    private Activity mActivity;
    private CompositorView mCompositorView;

    private boolean mContentOverlayVisiblity = true;
    private boolean mCanBeFocusable;

    /** A task to be performed after a resize event. */
    private Runnable mPostHideKeyboardTask;

    private TabModelSelector mTabModelSelector;
    private @Nullable BrowserControlsManager mBrowserControlsManager;
    private View mAccessibilityView;
    private CompositorAccessibilityProvider mNodeProvider;

    /** The toolbar control container. **/
    private @Nullable ControlContainer mControlContainer;

    private boolean mShowingFullscreen;
    private Runnable mSystemUiFullscreenResizeRunnable;

    /** The currently visible Tab. */
    @VisibleForTesting Tab mTabVisible;

    /** The currently attached View. */
    private View mView;

    /**
     * Current ContentView. Updates when active tab is switched or WebContents is swapped
     * in the current Tab.
     */
    private ContentView mContentView;

    // Cache objects that should not be created frequently.
    private final Rect mCacheRect = new Rect();
    private final Point mCachePoint = new Point();

    private boolean mControlsResizeView;
    private boolean mInGesture;
    private boolean mContentViewScrolling;
    // The number of active touch pointers. We are sending a gesture begin
    // event for every added touch point, and a gesnture end event for every
    // removed touch point.
    // TODO(crbug.com/265479149): We will remove |mInGesture| if we enable the
    // SUPPRESS_TOOLBAR_CAPTURES_AT_GESTURE_END feature.
    private int mNumGestureActiveTouches;
    private ApplicationViewportInsetSupplier mApplicationBottomInsetSupplier;

    // Handler for changes to viewport insets.
    private Callback<ViewportInsets> mOnViewportInsetsChanged;

    /**
     * Tracks whether geometrychange event is fired for the active tab when the keyboard
     *  is shown/hidden. When active tab changes, this flag is reset so we can fire
     *  geometrychange event for the new tab when the keyboard shows.
     */
    private boolean mHasKeyboardGeometryChangeFired;

    /**
     * By default, the virtual keyboard overlays content, only resizing the visual viewport.
     * Web content can use APIs that can change this to cause the WebContents to be resized.
     */
    @VirtualKeyboardMode.EnumType
    private int mVirtualKeyboardMode = VirtualKeyboardMode.RESIZES_VISUAL;

    private OnscreenContentProvider mOnscreenContentProvider;

    private final Set<Runnable> mOnCompositorLayoutCallbacks = new HashSet<>();
    private final Set<Runnable> mDidSwapFrameCallbacks = new HashSet<>();
    private final Set<Runnable> mDidSwapBuffersCallbacks = new HashSet<>();

    /** Used to remove the temporary tab strip on startup, once ready (or timed out). */
    private Runnable mSetBackgroundRunnable;

    private boolean mDelayTempStripRemoval;
    private boolean mSetBackgroundTimedOut;
    private boolean mCanSetBackground;
    private boolean mFirstTabCreated;
    private boolean mHasDrawnOnce;
    private int mDelayTempStripRemovalTimeoutMs;
    private long mBuffersSwappedTimestamp;
    private long mTabStateInitializedTimestamp;

    private TopUiThemeColorProvider mTopUiThemeColorProvider;

    // Permissions are requested on a drop event, and are released when another drag starts
    // (drag-started event) or when the current page navigates to a new URL or the tab changes.
    private DragAndDropPermissions mDragAndDropPermissions;
    // The URI when a drop contains a single URI. If the tab changes and is loading this URI, we do
    // not clear the permissions.
    private Uri mDropUri;

    private final EventOffsetHandler mEventOffsetHandler =
            new EventOffsetHandler(
                    new EventOffsetHandler.EventOffsetHandlerDelegate() {
                        // Cache objects that should not be created frequently.
                        private final RectF mCacheViewport = new RectF();

                        @Override
                        public float getTop() {
                            if (mLayoutManager != null) {
                                mLayoutManager.getViewportPixel(mCacheViewport);
                            }
                            return mCacheViewport.top;
                        }

                        @Override
                        public void setCurrentTouchEventOffsets(float top) {
                            EventForwarder forwarder = getEventForwarder();
                            if (forwarder != null) forwarder.setCurrentTouchOffsetY(top);
                        }

                        @Override
                        public void setCurrentDragEventOffsets(float dx, float dy) {
                            EventForwarder forwarder = getEventForwarder();
                            if (forwarder != null) forwarder.setDragDispatchingOffset(dx, dy);
                        }

                        private EventForwarder getEventForwarder() {
                            if (mTabVisible == null) return null;
                            WebContents webContents = mTabVisible.getWebContents();
                            if (webContents == null) return null;
                            return webContents.getEventForwarder();
                        }
                    });

    private final TabObserver mTabObserver =
            new EmptyTabObserver() {
                @Override
                public void onContentChanged(Tab tab) {
                    CompositorViewHolder.this.onContentChanged();
                }

                @Override
                public void onPageLoadStarted(Tab tab, GURL url) {
                    CompositorViewHolder.this.releaseDragAndDropPermissions();
                }

                @Override
                public void onContentViewScrollingStateChanged(boolean scrolling) {
                    mContentViewScrolling = scrolling;
                    updateInMotion();
                    if (!scrolling) updateContentViewChildrenDimension();
                }

                @Override
                public void onWillShowBrowserControls(Tab tab, boolean viewTransitionOptIn) {
                    CompositorViewHolder.this.onWillShowBrowserControls(viewTransitionOptIn);
                }

                @Override
                public void onVirtualKeyboardModeChanged(
                        Tab tab, @VirtualKeyboardMode.EnumType int mode) {
                    updateVirtualKeyboardMode(mode);
                }

                @Override
                public void onDidFinishNavigationInPrimaryMainFrame(
                        Tab tab, NavigationHandle navigation) {
                    if (!navigation.isSameDocument() && navigation.hasCommitted()) {
                        assert getWebContents() == tab.getWebContents();
                        assert getWebContents() != null;
                        updateVirtualKeyboardMode(getWebContents().getVirtualKeyboardMode());
                    }
                }

                // TODO(crbug.com/265479149): Split out a specific delegate for
                // gesture listening below and remove from TabObserver.
                @Override
                public void onGestureBegin() {
                    mNumGestureActiveTouches++;
                    updateInMotion();
                }

                @Override
                public void onGestureEnd() {
                    mNumGestureActiveTouches--;
                    updateInMotion();
                }
            };

    private View mUrlBar;

    private PrefService mPrefService;

    @Override
    public PointerIcon onResolvePointerIcon(MotionEvent event, int pointerIndex) {
        View activeView = getContentView();
        if (activeView == null || !ViewCompat.isAttachedToWindow(activeView)) return null;
        return activeView.onResolvePointerIcon(event, pointerIndex);
    }

    /**
     * Creates a {@link CompositorView}.
     *
     * @param c The Context to create this {@link CompositorView} in.
     * @param attrs The AttributeSet used to create this {@link CompositorView}.
     */
    public CompositorViewHolder(Context c, AttributeSet attrs) {
        super(c, attrs);
        internalInit();
    }

    private void internalInit() {
        addOnLayoutChangeListener(
                (v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                    Tab tab = getCurrentTab();
                    if (tab != null) {
                        // Set the size of NTP if we're in the attached state as it may have not
                        // been sized properly when initializing tab. See the comment in
                        // #initializeTab()
                        // for why.
                        boolean attachedNativePage =
                                tab.isNativePage() && isAttachedToWindow(tab.getView());
                        boolean sizeChanged =
                                (right - left) != (oldRight - oldLeft)
                                        || (top - bottom) != (oldTop - oldBottom);
                        if (attachedNativePage || sizeChanged) {
                            tryUpdateControlsAndWebContentsSizing();
                        }
                    }

                    onViewportChanged();

                    // If there's an event that needs to occur after the keyboard is hidden, post
                    // it as a delayed event.  Otherwise this happens in the midst of the
                    // ContentView's relayout, which causes the ContentView to relayout on top of
                    // the
                    // stack view.  The 30ms is arbitrary, hoping to let the view get one repaint
                    // in so the full page is shown.
                    if (mPostHideKeyboardTask != null) {
                        new Handler().postDelayed(mPostHideKeyboardTask, 30);
                        mPostHideKeyboardTask = null;
                    }
                });

        mCompositorView = new CompositorView(getContext(), this);
        // mCompositorView should always be the first child.
        addView(
                mCompositorView,
                0,
                new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        setOnSystemUiVisibilityChangeListener(visibility -> handleSystemUiVisibilityChange());
        if (isFullscreenApiMigrationEnabled()) {
            setOnApplyWindowInsetsListener(
                    (view, windowInsets) -> {
                        handleSystemUiVisibilityChange();
                        return windowInsets;
                    });
        }
        handleSystemUiVisibilityChange();

        mDelayTempStripRemoval = TabUiFeatureUtilities.isDelayTempStripRemovalEnabled(getContext());
        mDelayTempStripRemovalTimeoutMs =
                TabManagementFieldTrial.DELAY_TEMP_STRIP_TIMEOUT_MS.getValue();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            setDefaultFocusHighlightEnabled(false);
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
        // [1] -
        // https://developer.android.com/reference/android/view/WindowManager.LayoutParams.html#FLAG_FULLSCREEN
        if (mShowingFullscreen
                && KeyboardVisibilityDelegate.getInstance().isKeyboardShowing(getContext(), this)) {
            getWindowVisibleDisplayFrame(mCacheRect);

            // On certain devices, getWindowVisibleDisplayFrame is larger than the screen size, so
            // this ensures we never draw beyond the underlying dimensions of the view.
            // https://crbug.com/854109
            mCachePoint.set(
                    Math.min(mCacheRect.width(), getWidth()),
                    Math.min(mCacheRect.height(), getHeight()));
        } else {
            mCachePoint.set(getWidth(), getHeight());
        }
        return mCachePoint;
    }

    @VisibleForTesting
    void handleSystemUiVisibilityChange() {
        View view = getContentView();
        if (view == null || !ViewCompat.isAttachedToWindow(view)) view = this;

        int uiVisibility = 0;
        while (view != null) {
            uiVisibility |= view.getSystemUiVisibility();
            if (!(view.getParent() instanceof View)) break;
            view = (View) view.getParent();
        }

        boolean isInFullscreen = isInFullscreenMode(uiVisibility, view);
        boolean layoutFullscreen = isLayoutFullscreen(uiVisibility);

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

    private static boolean isFullscreenApiMigrationEnabled() {
        return ChromeFeatureList.sFullscreenInsetsApiMigration.isEnabled()
                || (BuildInfo.getInstance().isAutomotive
                        && ChromeFeatureList.sFullscreenInsetsApiMigrationOnAutomotive.isEnabled());
    }

    private boolean isInFullscreenMode(int uiVisibility, View view) {
        // If the fullscreen api migration is enabled, check the updated API instead.
        if (isFullscreenApiMigrationEnabled()) {
            if (view != null
                    && view.getRootWindowInsets() != null
                    && mActivity != null
                    && mActivity.getWindow() != null
                    && mActivity.getWindow().getDecorView() != null) {
                Window window = mActivity.getWindow();
                return !WindowInsetsCompat.toWindowInsetsCompat(view.getRootWindowInsets(), view)
                                .isVisible(WindowInsetsCompat.Type.statusBars())
                        || WindowCompat.getInsetsController(window, window.getDecorView())
                                        .getSystemBarsBehavior()
                                == BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE;
            } else {
                return false;
            }
        } else {
            // SYSTEM_UI_FLAG_FULLSCREEN is cleared when showing the soft keyboard in older version
            // of
            // Android (prior to P).  The immersive mode flags are not cleared, so use those in
            // combination to detect this state.
            return (uiVisibility & View.SYSTEM_UI_FLAG_FULLSCREEN) != 0
                    || (uiVisibility & View.SYSTEM_UI_FLAG_IMMERSIVE) != 0
                    || (uiVisibility & View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY) != 0;
        }
    }

    private boolean isLayoutFullscreen(int uiVisibility) {
        if (isFullscreenApiMigrationEnabled()) {
            if (mActivity != null
                    && mActivity.getWindow() != null
                    && mActivity.getWindow().getDecorView() != null) {
                // TODO(crbug.com/41492646): Coordinate usage of #setDecorFitsSystemWindows
                return !mActivity.getWindow().getDecorView().getFitsSystemWindows();
            } else {
                return false;
            }
        } else {
            return (uiVisibility & View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN) != 0;
        }
    }

    /**
     * @param layoutManager The {@link LayoutManagerImpl} instance that will be driving what
     *                      shows in this {@link CompositorViewHolder}.
     */
    public void setLayoutManager(LayoutManagerImpl layoutManager) {
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
    public void setControlContainer(@Nullable ControlContainer controlContainer) {
        DynamicResourceLoader loader =
                mCompositorView.getResourceManager() != null
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

        mSetBackgroundRunnable =
                () -> {
                    // Wait until the second frame to turn off the placeholder background for the
                    // CompositorView and the tab strip, to ensure the compositor frame has been
                    // drawn.
                    final ViewGroup controlContainerVG = (ViewGroup) mControlContainer;
                    mCompositorView.setBackgroundResource(0);
                    if (controlContainerVG != null) {
                        mControlContainer.setCompositorBackgroundInitialized();
                    }
                };
    }

    /**
     * @param themeColorProvider {@link ThemeColorProvider} for top UI part.
     */
    public void setTopUiThemeColorProvider(TopUiThemeColorProvider themeColorProvider) {
        mTopUiThemeColorProvider = themeColorProvider;
    }

    /**
     * Sets the ApplicationViewportInsetSupplier that will notify CompositorViewHolder when the
     * WebContent must be resized by viewport insets.
     */
    public void setApplicationViewportInsetSupplier(ApplicationViewportInsetSupplier supplier) {
        assert mApplicationBottomInsetSupplier == null;
        mApplicationBottomInsetSupplier = supplier;
        mApplicationBottomInsetSupplier.setVirtualKeyboardMode(mVirtualKeyboardMode);
        mOnViewportInsetsChanged = (unused) -> handleWindowInsetChanged();
        mApplicationBottomInsetSupplier.addObserver(mOnViewportInsetsChanged);
    }

    // This method is called when any viewport insets change but is needed to watch for keyboard
    // state changes while fullscreened and is used to simulate a view resize. This is only needed
    // if the page has opted in to keyboard resizes.
    private void handleWindowInsetChanged() {
        if (mApplicationBottomInsetSupplier != null
                && mApplicationBottomInsetSupplier.insetsAffectWebContentsSize()) {
            tryUpdateControlsAndWebContentsSizing();
        }

        // Notify the compositor layout that the size has changed.  The layout does not drive
        // the WebContents sizing, so this needs to be done in addition to the above size
        // update.
        onViewportChanged();
    }

    /** Should be called for cleanup when the CompositorView instance is no longer used. */
    public void shutDown() {
        setTab(null);
        if (mApplicationBottomInsetSupplier != null) {
            assert mOnViewportInsetsChanged != null;
            mApplicationBottomInsetSupplier.removeObserver(mOnViewportInsetsChanged);
        }

        mCompositorView.shutDown();
        if (mLayoutManager != null) mLayoutManager.destroy();
        if (mOnscreenContentProvider != null) mOnscreenContentProvider.destroy();
        if (mContentView != null) {
            mContentView.removeOnHierarchyChangeListener(this);
        }
    }

    /** This is called when the native library are ready. */
    public void onNativeLibraryReady(
            WindowAndroid windowAndroid,
            TabContentManager tabContentManager,
            PrefService prefService) {
        mActivity = windowAndroid.getActivity().get();
        mCompositorView.initNativeCompositor(
                SysUtils.isLowEndDevice(), windowAndroid, tabContentManager);

        if (mControlContainer != null) {
            mCompositorView
                    .getResourceManager()
                    .getDynamicResourceLoader()
                    .registerResource(
                            R.id.control_container, mControlContainer.getToolbarResourceAdapter());
        }

        mPrefService = prefService;
    }

    /** Perform any initialization necessary for showing a reparented tab. */
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

    // TouchEventProvider implementation.

    @Override
    public void addTouchEventObserver(TouchEventObserver o) {
        mTouchEventObservers.addObserver(o);
    }

    @Override
    public void removeTouchEventObserver(TouchEventObserver o) {
        mTouchEventObservers.removeObserver(o);
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent e) {
        super.onInterceptTouchEvent(e);
        for (TouchEventObserver o : mTouchEventObservers) {
            if (o.onInterceptTouchEvent(e)) return true;
        }

        if (mLayoutManager == null) return false;

        int actionMasked = SPenSupport.convertSPenEventAction(e.getActionMasked());
        if (actionMasked == MotionEvent.ACTION_DOWN) {
            mEventOffsetHandler.onInterceptTouchDownEvent(e);
        }
        return mLayoutManager.onInterceptMotionEvent(e, mIsKeyboardShowing, EventType.TOUCH);
    }

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        super.onTouchEvent(e);
        for (TouchEventObserver o : mTouchEventObservers) {
            if (o.onTouchEvent(e)) return true;
        }

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
            tryUpdateControlsAndWebContentsSizing();
        }
    }

    private void updateInMotion() {
        // TODO(crbug.com/40244051): Track fling as well.
        boolean inMotion = mContentViewScrolling;
        if (ChromeFeatureList.sSuppressToolbarCapturesAtGestureEnd.isEnabled()) {
            inMotion |= mNumGestureActiveTouches > 0;
        } else {
            inMotion |= mInGesture;
        }
        mInMotionSupplier.set(inMotion);
        if (mContentView != null) {
            mContentView.setDeferKeepScreenOnChanges(inMotion);
        }
    }

    /**
     * Aggregated supplier for whether the compositor's content is moving. Currently tracking in
     * touch event and in scroll event. Performance is critical while this supplier returns true,
     * and clients that have expensive operations may consider deferring until after the motion is
     * over.
     */
    public ObservableSupplier<Boolean> getInMotionSupplier() {
        return mInMotionSupplier;
    }

    @Override
    public boolean onInterceptHoverEvent(MotionEvent e) {
        mEventOffsetHandler.onInterceptHoverEvent(e);
        if (mLayoutManager == null) return super.onInterceptHoverEvent(e);
        return mLayoutManager.onInterceptMotionEvent(e, mIsKeyboardShowing, EventType.HOVER);
    }

    @Override
    public boolean onHoverEvent(MotionEvent e) {
        super.onHoverEvent(e);
        boolean consumed = mLayoutManager != null && mLayoutManager.onHoverEvent(e);
        mEventOffsetHandler.onHoverEvent(e);
        return consumed;
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
        mEventOffsetHandler.onPreDispatchDragEvent(e.getAction(), 0.f, 0.f);
        if (UiAndroidFeatureMap.isEnabled(UiAndroidFeatureList.DRAG_DROP_FILES)) {
            if (e.getAction() == DragEvent.ACTION_DRAG_STARTED) {
                releaseDragAndDropPermissions();
            } else if (e.getAction() == DragEvent.ACTION_DROP) {
                mDragAndDropPermissions = mActivity.requestDragAndDropPermissions(e);
                if (e.getClipData() != null && e.getClipData().getItemCount() == 1) {
                    mDropUri = e.getClipData().getItemAt(0).getUri();
                }
            }
        }
        boolean ret = super.dispatchDragEvent(e);
        mEventOffsetHandler.onPostDispatchDragEvent(e.getAction());
        return ret;
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent e) {
        assert e != null : "The motion event dispatched shouldn't be null!";
        updateIsInGesture(e);
        for (TouchEventObserver o : mTouchEventObservers) {
            if (o.dispatchTouchEvent(e)) return true;
        }

        // This is where input events go from android through native to the web content. This
        // process is latency sensitive. Ideally observers that might be expensive, such as
        // notifying in motion, should be done after this.
        boolean handled = super.dispatchTouchEvent(e);

        updateInMotion();
        return handled;
    }

    /**
     * @return The {@link LayoutManagerImpl} associated with this view.
     */
    public LayoutManagerImpl getLayoutManager() {
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

    @VisibleForTesting
    Tab getCurrentTab() {
        if (mLayoutManager == null || mTabModelSelector == null) return null;
        Tab currentTab = mTabModelSelector.getCurrentTab();

        // If the tab model selector doesn't know of a current tab, use the last visible one.
        if (currentTab == null) currentTab = mTabVisible;

        return currentTab;
    }

    @VisibleForTesting
    ViewGroup getContentView() {
        Tab tab = getCurrentTab();
        return tab != null ? tab.getContentView() : null;
    }

    protected WebContents getWebContents() {
        Tab tab = getCurrentTab();
        return tab != null ? tab.getWebContents() : null;
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);

        if (mTabModelSelector == null) return;

        for (TabModel tabModel : mTabModelSelector.getModels()) {
            for (int i = 0; i < tabModel.getCount(); ++i) {
                Tab tab = tabModel.getTabAt(i);
                if (tab == null) continue;
                updateWebContentsSize(tab);
            }
        }
    }

    /**
     * Ensures the tab-backed webContents' size is up to date.
     *
     * Using this view's current size, taking into account the current state of UI like the virtual
     * keyboard and browser controls and resizes as well as the virtual keyboard resizing mode,
     * updates the size of the given Tab's WebContents. If the given view isn't attached to the
     * Window, this method will force it to layout and use that size.
     *
     * @param tab {@link Tab} for which the size of the view is set.
     */
    @VisibleForTesting
    void updateWebContentsSize(Tab tab) {
        if (tab == null) return;

        WebContents webContents = tab.getWebContents();
        View view = tab.getContentView();
        if (webContents == null || view == null) return;

        Point viewportSize = getViewportSize();
        int width = viewportSize.x;
        int height = viewportSize.y;

        // The view size takes into account of the browser controls whose height should be
        // subtracted from the view if they are visible, therefore shrink Blink-side view size.
        // TODO(crbug.com/40767446): Centralize the logic for calculating bottom insets by
        // merging them into ApplicationBottomInsetSupplier.
        int controlsInsets = 0;
        if (mBrowserControlsManager != null) {
            int controlsMinHeight =
                    mBrowserControlsManager.getTopControlsMinHeight()
                            + mBrowserControlsManager.getBottomControlsMinHeight();
            int controlsHeight =
                    mBrowserControlsManager.getTopControlsHeight()
                            + mBrowserControlsManager.getBottomControlsHeight();
            controlsInsets = mControlsResizeView ? controlsHeight : controlsMinHeight;
        }

        int keyboardInset =
                mApplicationBottomInsetSupplier != null
                        ? mApplicationBottomInsetSupplier.get().webContentsHeightInset
                        : 0;

        int viewportInsets = controlsInsets + keyboardInset;

        if (isAttachedToWindow(view)) {
            webContents.setSize(width, height - viewportInsets);

            // Dispatch the geometrychange JavaScript event to the page.
            // TODO(bokan): This doesn't belong in updateWebContentsSize. Ideally the content/ layer
            // would listen to changes in keyboard state and dispatch this event itself.
            if (mVirtualKeyboardMode == VirtualKeyboardMode.OVERLAYS_CONTENT) {
                int keyboardHeight =
                        KeyboardVisibilityDelegate.getInstance()
                                .calculateTotalKeyboardHeight(this.getRootView());
                notifyVirtualKeyboardOverlayGeometryChangeEvent(width, keyboardHeight, webContents);
            }
        } else {
            // Need to call layout() for the following View if it is not attached to the view
            // hierarchy. Calling {@code view.onSizeChanged()} is dangerous because if the View has
            // a different size than the WebContents, it might think a future size update is a NOOP
            // and not call onSizeChanged() on the WebContents.
            view.measure(
                    MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY),
                    MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY));
            view.layout(0, 0, view.getMeasuredWidth(), view.getMeasuredHeight());
            webContents.setSize(view.getWidth(), view.getHeight() - viewportInsets);
            requestRender();
        }
    }

    private static boolean isAttachedToWindow(View view) {
        return view != null && view.getWindowToken() != null;
    }

    @VirtualKeyboardMode.EnumType
    private int defaultVirtualKeyboardMode() {
        if (mPrefService.getBoolean(Pref.VIRTUAL_KEYBOARD_RESIZES_LAYOUT_BY_DEFAULT)) {
            return VirtualKeyboardMode.RESIZES_CONTENT;
        }
        return VirtualKeyboardMode.RESIZES_VISUAL;
    }

    /**
     * Notifies geometrychange event to JS.
     * @param w  Width of the view.
     * @param keyboardHeight Height of the keyboard.
     * @param webContents Active WebContent for which this event needs to be fired.
     */
    private void notifyVirtualKeyboardOverlayGeometryChangeEvent(
            int w, int keyboardHeight, WebContents webContents) {
        assert mVirtualKeyboardMode == VirtualKeyboardMode.OVERLAYS_CONTENT;

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

    private void onControlsResizeViewChanged(WebContents webContents, boolean controlsResizeView) {
        if (webContents != null && mCompositorView != null) {
            mCompositorView.onControlsResizeViewChanged(webContents, controlsResizeView);
        }
    }

    /**
     * Fires geometrychange event to JS with the keyboard size.
     * @param webContents Active WebContent for which this event needs to be fired.
     * @param x When the keyboard is shown, it has the left position of the app's rect, else, 0.
     * @param y When the keyboard is shown, it has the top position of the app's rect, else, 0.
     * @param width  When the keyboard is shown, it has the width of the view, else, 0.
     * @param height The height of the keyboard.
     */
    @VisibleForTesting
    void notifyVirtualKeyboardOverlayRect(
            WebContents webContents, int x, int y, int width, int height) {
        if (mCompositorView != null) {
            mCompositorView.notifyVirtualKeyboardOverlayRect(webContents, x, y, width, height);
        }
    }

    /** Called whenever the host activity is started. */
    public void onStart() {
        if (mBrowserControlsManager != null) mBrowserControlsManager.addObserver(this);
        requestRender();
    }

    /** Called whenever the host activity is stopped. */
    public void onStop() {
        if (mBrowserControlsManager != null) mBrowserControlsManager.removeObserver(this);
    }

    @Override
    public void onControlsOffsetChanged(
            int topOffset,
            int topControlsMinHeightOffset,
            int bottomOffset,
            int bottomControlsMinHeightOffset,
            boolean needsAnimate,
            boolean isVisibilityForced) {
        onViewportChanged();

        // When scrolling browser controls in viz, don't produce new browser frames unless it's
        // forced with |needs_animate|
        boolean scrollingWithBciv =
                ToolbarFeatures.isBrowserControlsInVizEnabled(
                                DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext()))
                        && (mInGesture || mContentViewScrolling);
        if (needsAnimate && !scrollingWithBciv) {
            requestRender();
        }

        updateContentViewChildrenDimension();
    }

    @Override
    public void onBottomControlsHeightChanged(
            int bottomControlsHeight, int bottomControlsMinHeight) {
        if (mTabVisible == null) return;
        onBrowserControlsHeightChanged();
        updateWebContentsSize(getCurrentTab());
        onViewportChanged();
    }

    @Override
    public void onTopControlsHeightChanged(int topControlsHeight, int topControlsMinHeight) {
        if (mTabVisible == null) return;
        onBrowserControlsHeightChanged();
        updateWebContentsSize(getCurrentTab());
        onViewportChanged();
    }

    /**
     * Notify the {@link WebContents} of the browser controls height changes. Unlike
     * #updateWebContentsSize, this will make sure the renderer's properties are updated even if the
     * size didn't change.
     */
    private void onBrowserControlsHeightChanged() {
        final WebContents webContents = getWebContents();
        if (webContents == null) return;
        webContents.notifyBrowserControlsHeightChanged();
    }

    /**
     * Attempts to update browser controls sizing state and then synchronizes the WebContents size
     * based on the current viewport and insets. No-op if the user is currently scrolling or in a
     * gesture.
     */
    private void tryUpdateControlsAndWebContentsSizing() {
        if (mInGesture || mContentViewScrolling) return;
        boolean controlsResizeViewChanged = false;
        if (mBrowserControlsManager != null) {
            // Update content viewport size only if the browser controls are not moving, i.e. not
            // scrolling or animating.
            if (!BrowserControlsUtils.areBrowserControlsIdle(mBrowserControlsManager)) return;

            boolean controlsResizeView =
                    BrowserControlsUtils.controlsResizeView(mBrowserControlsManager);
            if (controlsResizeView != mControlsResizeView) {
                mControlsResizeView = controlsResizeView;
                controlsResizeViewChanged = true;
            }
        }
        // Reflect the changes that may have happened in in view/control size.
        updateWebContentsSize(getCurrentTab());
        if (controlsResizeViewChanged) {
            // Send this after updateWebContentsSize, so that RenderWidgetHost doesn't
            // SynchronizeVisualProperties in a partly-updated state.
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
        ViewGroup view = getContentView();
        if (view != null) {
            assert mBrowserControlsManager != null;
            float topViewsTranslation = mBrowserControlsManager.getTopVisibleContentOffset();
            float bottomMargin =
                    BrowserControlsUtils.getBottomContentOffset(mBrowserControlsManager);
            applyMarginToFullscreenChildViews(view, topViewsTranslation, bottomMargin);
            tryUpdateControlsAndWebContentsSizing();
        }
        TraceEvent.end("CompositorViewHolder:updateContentViewChildrenDimension");
    }

    private static void applyMarginToFullscreenChildViews(
            ViewGroup contentView, float topMargin, float bottomMargin) {
        for (int i = 0; i < contentView.getChildCount(); i++) {
            View child = contentView.getChildAt(i);
            if (!(child.getLayoutParams() instanceof FrameLayout.LayoutParams)) continue;
            FrameLayout.LayoutParams layoutParams =
                    (FrameLayout.LayoutParams) child.getLayoutParams();

            if (layoutParams.height == LayoutParams.MATCH_PARENT
                    && (layoutParams.topMargin != (int) topMargin
                            || layoutParams.bottomMargin != (int) bottomMargin)) {
                layoutParams.topMargin = (int) topMargin;
                layoutParams.bottomMargin = (int) bottomMargin;
                ViewUtils.requestLayout(
                        child, "CompositorViewHolder.applyMarginToFullscreenChildViews");
                TraceEvent.instant("FullscreenManager:child.requestLayout()");
            }
        }
    }

    /** Sets the overlay mode. */
    public void setOverlayMode(boolean useOverlayMode) {
        if (mCompositorView != null) {
            mCompositorView.setOverlayVideoMode(useOverlayMode);
        }
    }

    private void onViewportChanged() {
        if (mLayoutManager != null) mLayoutManager.onViewportChanged();
    }

    /** To be called once a frame before commit. */
    @Override
    public void onCompositorLayout() {
        TraceEvent.begin("CompositorViewHolder:layout");
        if (mLayoutManager != null) {
            mLayoutManager.onUpdate();
            mCompositorView.finalizeLayers(mLayoutManager);
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

        if (mApplicationBottomInsetSupplier != null) {
            outRect.bottom -= mApplicationBottomInsetSupplier.get().viewVisibleHeightInset;
        }

        // mApplicationBottomInsetSupplier doesn't include browser controls.
        if (mBrowserControlsManager != null) {
            // All of these values are in pixels.
            outRect.top += mBrowserControlsManager.getTopVisibleContentOffset();
            float bottomControlOffset = mBrowserControlsManager.getBottomControlOffset();
            outRect.bottom -= (getBottomControlsHeightPixels() - bottomControlOffset);
        }
    }

    @Override
    public void getViewportFullControls(RectF outRect) {
        getWindowViewport(outRect);

        if (mApplicationBottomInsetSupplier != null) {
            outRect.bottom -= mApplicationBottomInsetSupplier.get().viewVisibleHeightInset;
        }

        // mApplicationBottomInsetSupplier doesn't include browser controls.
        outRect.top += getTopControlsHeightPixels();
        outRect.bottom -= getBottomControlsHeightPixels();
    }

    @Override
    public void requestRender() {
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
    public void didSwapFrame(int pendingFrameCount) {
        TraceEvent.instant("didSwapFrame");

        mHasDrawnOnce = true;

        mDidSwapBuffersCallbacks.addAll(mDidSwapFrameCallbacks);
        mDidSwapFrameCallbacks.clear();
        updateNeedsSwapBuffersCallback();
    }

    @Override
    public void didSwapBuffers(boolean swappedCurrentSize, int framesUntilHideBackground) {
        if (mSetBackgroundRunnable != null
                && mHasDrawnOnce
                && framesUntilHideBackground == 0
                && !mCanSetBackground) {
            // Remove temporary background if tab state is ready. Otherwise, mark that the
            // background can be removed and handle in TabModelSelectorObserver.
            if (!mDelayTempStripRemoval
                    || mTabModelSelector.isTabStateInitialized()
                    || mSetBackgroundTimedOut) {
                runSetBackgroundRunnable();
            } else {
                mCanSetBackground = true;
            }

            // If tab state is already initialized, record how long it took for the real tab strip
            // to be ready to be drawn.
            if (mTabStateInitializedTimestamp != 0) {
                RecordHistogram.recordTimesHistogram(
                        "Android.TabStrip.TimeToBufferSwapAfterInitializeTabState",
                        SystemClock.elapsedRealtime() - mTabStateInitializedTimestamp);
            } else {
                mBuffersSwappedTimestamp = SystemClock.elapsedRealtime();
            }
        }

        for (Runnable runnable : mDidSwapBuffersCallbacks) {
            runnable.run();
        }
        mDidSwapBuffersCallbacks.clear();
        updateNeedsSwapBuffersCallback();
    }

    private void runSetBackgroundRunnable() {
        // This runnable should only be run once.
        if (mSetBackgroundRunnable == null) return;

        new Handler().post(mSetBackgroundRunnable);
        mSetBackgroundRunnable = null;

        // Mark that we timed out if we remove the background before the tab state is initialized.
        // Called when the background is actually being removed, since if the timeout is reached,
        // but the second buffer swap happens after the tab state is initialized, we shouldn't
        // actually see any jank.
        RecordHistogram.recordBooleanHistogram(
                "Android.TabStrip.DelayTempStripRemovalTimedOut",
                !mTabModelSelector.isTabStateInitialized());
    }

    @VisibleForTesting
    void maybeInitializeSetBackgroundRunnableTimeout() {
        if (mDelayTempStripRemoval && !mFirstTabCreated) {
            mFirstTabCreated = true;
            new Handler()
                    .postDelayed(
                            () -> {
                                // If null, the background has already been removed before the
                                // timeout.
                                if (mSetBackgroundRunnable == null) return;

                                if (mCanSetBackground) {
                                    // If the background can be removed, remove it now.
                                    runSetBackgroundRunnable();
                                } else {
                                    // If the background cannot be removed, mark that we have timed
                                    // out, so that we can remove the background when the buffer
                                    // swaps.
                                    mSetBackgroundTimedOut = true;
                                }
                            },
                            mDelayTempStripRemovalTimeoutMs);
        }
    }

    @Override
    public void setContentOverlayVisibility(boolean show, boolean canBeFocusable) {
        if (show != mContentOverlayVisiblity || canBeFocusable != mCanBeFocusable) {
            mContentOverlayVisiblity = show;
            mCanBeFocusable = canBeFocusable;
            updateContentOverlayVisibility(mContentOverlayVisiblity);
        }
    }

    @Override
    public LayoutRenderHost getLayoutRenderHost() {
        return this;
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
    public BrowserControlsManager getBrowserControlsManager() {
        return mBrowserControlsManager;
    }

    @Override
    public FullscreenManager getFullscreenManager() {
        return mBrowserControlsManager.getFullscreenManager();
    }

    /**
     * Sets a browser controls manager.
     * @param manager A browser controls manager.
     */
    public void setBrowserControlsManager(BrowserControlsManager manager) {
        mBrowserControlsManager = manager;
        mBrowserControlsManager.addObserver(this);
        onViewportChanged();
    }

    public int getTopControlsHeightPixels() {
        return mBrowserControlsManager != null ? mBrowserControlsManager.getTopControlsHeight() : 0;
    }

    public int getBottomControlsHeightPixels() {
        return mBrowserControlsManager != null
                ? mBrowserControlsManager.getBottomControlsHeight()
                : 0;
    }

    /**
     * @return {@code true} if browser controls shrink Blink view's size.
     */
    public boolean controlsResizeView() {
        return mControlsResizeView;
    }

    /**
     * Sets the URL bar. This is needed so that the ContentViewHolder can find out
     * whether it can claim focus.
     */
    public void setUrlBar(View urlBar) {
        mUrlBar = urlBar;
    }

    @Override
    public void onDetachedFromWindow() {
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
        if (mUrlBar != null && mUrlBar.isFocused()) mUrlBar.clearFocus();
        boolean wasVisible = false;
        if (hasFocus()) {
            KeyboardVisibilityDelegate keyboardVisibilityDelegate =
                    KeyboardVisibilityDelegate.getInstance();
            wasVisible = keyboardVisibilityDelegate.isKeyboardShowing(getContext(), this);
            if (wasVisible) {
                keyboardVisibilityDelegate.hideKeyboard(this);
            }
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
     */
    public void onFinishNativeInitialization(
            TabModelSelector tabModelSelector, TabCreatorManager tabCreatorManager) {
        assert mLayoutManager != null;
        mLayoutManager.init(
                tabModelSelector,
                tabCreatorManager,
                mControlContainer,
                mCompositorView.getResourceManager().getDynamicResourceLoader(),
                mTopUiThemeColorProvider);

        mTabModelSelector = tabModelSelector;
        tabModelSelector.addObserver(
                new TabModelSelectorObserver() {
                    @Override
                    public void onChange() {
                        onContentChanged();
                    }

                    @Override
                    public void onTabStateInitialized() {
                        // Tab state is initialized, so remove background if we've not yet done so
                        // and a frame is ready.
                        if (mDelayTempStripRemoval
                                && mSetBackgroundRunnable != null
                                && mCanSetBackground) {
                            runSetBackgroundRunnable();
                        }

                        // If real tab strip is ready to be drawn, record how long it took for the
                        // tab state to be initialized.
                        if (mBuffersSwappedTimestamp != 0) {
                            RecordHistogram.recordTimesHistogram(
                                    "Android.TabStrip.TimeToInitializeTabStateAfterBufferSwap",
                                    SystemClock.elapsedRealtime() - mBuffersSwappedTimestamp);
                        } else {
                            mTabStateInitializedTimestamp = SystemClock.elapsedRealtime();
                        }
                    }

                    @Override
                    public void onNewTabCreated(Tab tab, @TabCreationState int creationState) {
                        initializeTab(tab);
                        maybeInitializeSetBackgroundRunnableTimeout();
                    }
                });

        onContentChanged();
        mNativeInitialized = true;
    }

    private void updateContentOverlayVisibility(boolean show) {
        if (mView == null) return;
        WebContents webContents = getWebContents();
        if (show) {
            if (mView != getCurrentTab().getView() || mView.getParent() == this) return;
            // During tab creation, we temporarily add the new tab's view to a FrameLayout to
            // measure and lay it out. This way we could show the animation in the stack view.
            // Therefore we should remove the view from that temporary FrameLayout here.
            UiUtils.removeViewFromParent(mView);

            if (webContents != null) {
                assert !webContents.isDestroyed();
                getContentView().setVisibility(View.VISIBLE);
                tryUpdateControlsAndWebContentsSizing();
            }

            // CompositorView always has index of 0.
            // TODO(crbug.com/40770763): Look into enforcing the z-order of the views.
            addView(mView, 1);

            setFocusable(false);
            setFocusableInTouchMode(false);

            // Claim focus for the new view unless the user is currently using the URL bar.
            if (mUrlBar == null || !mUrlBar.hasFocus()) mView.requestFocus();
        } else {
            if (mView.getParent() == this) {
                setFocusable(mCanBeFocusable);
                setFocusableInTouchMode(mCanBeFocusable);

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

    @VisibleForTesting
    void onWillShowBrowserControls(boolean viewTransitionOptIn) {
        // TODO(bokan): Flag guarding potential new behavior
        // https://crbug.com/332331777.
        if (!viewTransitionOptIn && !ChromeFeatureList.sBrowserControlsEarlyResize.isEnabled()) {
            return;
        }

        // Let observers know the controls will be shown, resize the web content
        // immediately rather than waiting for the controls animation to finish. This
        // helps makes the resize more predictable, in particular, when capturing
        // snapshots of outgoing content for a view transition.
        if (mControlsResizeView) return;
        mControlsResizeView = true;
        updateWebContentsSize(getCurrentTab());
        onControlsResizeViewChanged(getWebContents(), mControlsResizeView);
    }

    private void setTab(Tab tab) {
        if (tab != null) {
            tab.loadIfNeeded(TabLoadIfNeededCaller.SET_TAB);
        }

        View newView = tab != null ? tab.getView() : null;
        if (mView == newView) return;

        // TODO(dtrainor): Look into changing this only if the views differ, but still parse the
        // WebContents list even if they're the same.
        updateContentOverlayVisibility(false);

        if (mTabVisible != tab) {
            // Reset the geometrychange event flag so it can fire on the current active tab.
            mHasKeyboardGeometryChangeFired = false;
            if (mTabVisible != null) mTabVisible.removeObserver(mTabObserver);
            if (tab != null) {
                tab.addObserver(mTabObserver);
                mCompositorView.onTabChanged();
            }
            updateViewStateListener(tab != null ? tab.getContentView() : null);
        }

        mTabVisible = tab;
        mView = newView;

        updateContentOverlayVisibility(mContentOverlayVisiblity);

        if (mTabVisible != null) initializeTab(mTabVisible);

        if (mOnscreenContentProvider == null) {
            mOnscreenContentProvider =
                    new OnscreenContentProvider(getContext(), this, getWebContents());
        } else {
            mOnscreenContentProvider.onWebContentsChanged(getWebContents());
        }

        // Clear drop permissions when tab changes unless this is a new tab loading from the drop.
        if (mDropUri == null || !tab.getUrl().getSpec().equals(mDropUri.toString())) {
            releaseDragAndDropPermissions();
        }
    }

    private void updateViewStateListener(ContentView newContentView) {
        if (mContentView != null) {
            mContentView.removeOnHierarchyChangeListener(this);
            mContentView.setDeferKeepScreenOnChanges(false);
            mContentView.setEventOffsetHandlerForDragDrop(null);
        }
        if (newContentView != null) {
            newContentView.addOnHierarchyChangeListener(this);
            newContentView.setEventOffsetHandlerForDragDrop(mEventOffsetHandler);
        }
        mContentView = newContentView;
    }

    @VisibleForTesting
    void updateVirtualKeyboardMode(@VirtualKeyboardMode.EnumType int newMode) {
        // UNSET means the author hasn't explicitly set a preference but the mode should have been
        // set to the default in that case.
        assert mVirtualKeyboardMode != VirtualKeyboardMode.UNSET;

        if (newMode == VirtualKeyboardMode.UNSET) {
            newMode = defaultVirtualKeyboardMode();
        }

        if (mVirtualKeyboardMode == newMode) return;

        mVirtualKeyboardMode = newMode;

        if (mApplicationBottomInsetSupplier != null) {
            mApplicationBottomInsetSupplier.setVirtualKeyboardMode(mVirtualKeyboardMode);
        }
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
            onControlsResizeViewChanged(webContents, mControlsResizeView);

            updateVirtualKeyboardMode(webContents.getVirtualKeyboardMode());
        } else if (tab.getView() != null) {
            updateVirtualKeyboardMode(VirtualKeyboardMode.UNSET);
        }

        if (tab.getView() == null) return;

        // Update WebContents' size only if the currently visible View is the ContentView. If
        // unattached, the ContentView will be sized here to ensure it stays in sync with
        // WebContents but other types of Views can just wait for layout as usual.
        if (tab.getView() != tab.getContentView()) return;

        updateWebContentsSize(tab);
    }

    @Override
    public void invalidateAccessibilityProvider() {
        if (mNodeProvider != null) mNodeProvider.invalidateRoot();
    }

    // ChromeAccessibilityUtil.Observer

    @Override
    public void onAccessibilityModeChanged(boolean enabled) {
        // Instantiate and install the accessibility node provider on this view if necessary.
        // This overrides any hover event listeners or accessibility delegates
        // that may have been added elsewhere.
        assert mLayoutManager != null;
        if (enabled && (mNodeProvider == null)) {
            mAccessibilityView =
                    new View(getContext()) {
                        boolean mIsCheckingForVirtualViews;
                        final List<VirtualView> mVirtualViews = new ArrayList<>();

                        /**
                         * Checks if there are any a11y focusable VirtualViews. If there are, set the view
                         * to be View.IMPORTANT_FOR_ACCESSIBILITY_AUTO (and therefore return true). If there
                         * are not, set the view to be View.IMPORTANT_FOR_ACCESSIBILITY_NO (and therefore
                         * return false).
                         *
                         * @return Whether or not the view should be a11y focusable.
                         */
                        @Override
                        public boolean isImportantForAccessibility() {
                            if (mNativeInitialized && !mIsCheckingForVirtualViews) {
                                mIsCheckingForVirtualViews = true;
                                mVirtualViews.clear();
                                mLayoutManager.getVirtualViews(mVirtualViews);
                                int importantForAccessibility =
                                        mVirtualViews.size() == 0
                                                ? View.IMPORTANT_FOR_ACCESSIBILITY_NO
                                                : View.IMPORTANT_FOR_ACCESSIBILITY_AUTO;
                                if (getImportantForAccessibility() != importantForAccessibility) {
                                    setImportantForAccessibility(importantForAccessibility);
                                    sendAccessibilityEvent(
                                            AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED);
                                }
                                mIsCheckingForVirtualViews = false;
                            }

                            return super.isImportantForAccessibility();
                        }
                    };
            addView(mAccessibilityView);
            mNodeProvider = new CompositorAccessibilityProvider(mAccessibilityView);
            ViewCompat.setAccessibilityDelegate(mAccessibilityView, mNodeProvider);
        }
    }

    // TabObscuringHandler.Observer

    @Override
    public void updateObscured(boolean obscureTabContent, boolean obscureToolbar) {
        setFocusable(!obscureTabContent);
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
                if (mVirtualViews.get(i).checkClickedOrHovered(x / mDpToPx, y / mDpToPx)) {
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
                    mVirtualViews.get(virtualViewId).handleClick(LayoutManagerImpl.time());
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
            if (view.hasClickAction()) {
                node.addAction(AccessibilityNodeInfoCompat.ACTION_CLICK);
            }
            node.addAction(AccessibilityNodeInfoCompat.ACTION_FOCUS);
            if (view.hasLongClickAction()) {
                node.addAction(AccessibilityNodeInfoCompat.ACTION_LONG_CLICK);
            }
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

    // Should be called any time inputs used to compute `needsSwapCallback` changes.
    private void updateNeedsSwapBuffersCallback() {
        boolean needsSwapCallback =
                !mHasDrawnOnce
                        || !mOnCompositorLayoutCallbacks.isEmpty()
                        || !mDidSwapFrameCallbacks.isEmpty()
                        || !mDidSwapBuffersCallbacks.isEmpty();
        mCompositorView.setRenderHostNeedsDidSwapBuffersCallback(needsSwapCallback);
    }

    void setCompositorViewForTesting(CompositorView compositorView) {
        mCompositorView = compositorView;
    }

    @VirtualKeyboardMode.EnumType
    public int getVirtualKeyboardModeForTesting() {
        return mVirtualKeyboardMode;
    }

    /** Release any DragAndDropPermissions currently held. */
    private void releaseDragAndDropPermissions() {
        if (mDragAndDropPermissions != null) {
            mDragAndDropPermissions.release();
            mDragAndDropPermissions = null;
        }
        mDropUri = null;
    }
}
