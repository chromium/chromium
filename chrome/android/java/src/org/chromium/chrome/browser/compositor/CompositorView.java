// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.View;
import android.widget.FrameLayout;

import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutProvider;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.compositor.resources.StaticResourcePreloads;
import org.chromium.chrome.browser.compositor.scene_layer.SceneLayer;
import org.chromium.chrome.browser.externalnav.IntentWithGesturesHandler;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tabmodel.TabModelImpl;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.resources.AndroidResourceType;
import org.chromium.ui.resources.ResourceManager;

import java.util.ArrayList;
import java.util.List;

/**
 * The is the {@link View} displaying the ui compositor results; including webpages and tabswitcher.
 */
@JNINamespace("android")
public class CompositorView
        extends FrameLayout implements CompositorSurfaceManager.SurfaceManagerCallbackTarget {
    private static final String TAG = "CompositorView";
    private static final long NANOSECONDS_PER_MILLISECOND = 1000000;

    // Cache objects that should not be created every frame
    private final Rect mCacheAppRect = new Rect();
    private final int[] mCacheViewPosition = new int[2];

    private CompositorSurfaceManager mCompositorSurfaceManager;
    private boolean mOverlayVideoEnabled;
    private boolean mAlwaysTranslucent;

    // Are we waiting to hide the outgoing surface until the foreground has something to display?
    // If == 0, then no.  If > 0, then yes.  We'll hide when it transitions from one to zero.
    private int mFramesUntilHideBackground;

    private long mNativeCompositorView;
    private final LayoutRenderHost mRenderHost;
    private int mPreviousWindowTop = -1;

    // Resource Management
    private ResourceManager mResourceManager;

    // Lazily populated as it is needed.
    private View mRootActivityView;
    private WindowAndroid mWindowAndroid;
    private LayerTitleCache mLayerTitleCache;
    private TabContentManager mTabContentManager;

    private View mRootView;
    private boolean mPreloadedResources;
    private List<Runnable> mDrawingFinishedCallbacks;

    /**
     * Creates a {@link CompositorView}. This can be called only after the native library is
     * properly loaded.
     * @param c        The Context to create this {@link CompositorView} in.
     * @param host     The renderer host owning this view.
     */
    public CompositorView(Context c, LayoutRenderHost host) {
        super(c);
        mRenderHost = host;
        initializeIfOnUiThread();
    }

    /**
     * The {@link CompositorSurfaceManagerImpl} constructor creates a handler (inside the
     * SurfaceView constructor on android N and before) and thus can only be called on the UI
     * thread. If the layout is inflated on a background thread this fails, thus we only initialize
     * the {@link CompositorSurfaceManager} in the constructor if on the UI thread (or we are
     * running on android O+), otherwise it is initialized inside the first call to
     * {@link #setRootView}.
     */
    private void initializeIfOnUiThread() {
        if (!ThreadUtils.runningOnUiThread() && Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            return;
        }

        mCompositorSurfaceManager = new CompositorSurfaceManagerImpl(this, this);

        // Cover the black surface before it has valid content.  Set this placeholder view to
        // visible, but don't yet make SurfaceView visible, in order to delay
        // surfaceCreate/surfaceChanged calls until the native library is loaded.
        setBackgroundColor(Color.WHITE);
        super.setVisibility(View.VISIBLE);

        // Request the opaque surface.  We might need the translucent one, but
        // we don't know yet.  We'll switch back later if we discover that
        // we're on a low memory device that always uses translucent.
        mCompositorSurfaceManager.requestSurface(PixelFormat.OPAQUE);
    }

    /**
     * @param view The root view of the hierarchy.
     */
    public void setRootView(View view) {
        // If layout was inflated on a background thread, then the CompositorView should be
        // initialized now.
        if (mCompositorSurfaceManager == null) {
            ThreadUtils.assertOnUiThread();
            initializeIfOnUiThread();
        }
        mRootView = view;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        if (mRootView != null) {
            mRootView.getWindowVisibleDisplayFrame(mCacheAppRect);

            // Check whether the top position of the window has changed as we always must
            // resize in that case to the specified height spec.  On certain versions of
            // Android when you change the top position (i.e. by leaving fullscreen) and
            // do not shrink the SurfaceView, it will appear to be pinned to the top of
            // the screen under the notification bar and all touch offsets will be wrong
            // as well as a gap will appear at the bottom of the screen.
            int windowTop = mCacheAppRect.top;
            boolean topChanged = windowTop != mPreviousWindowTop;
            mPreviousWindowTop = windowTop;

            Activity activity = mWindowAndroid != null ? mWindowAndroid.getActivity().get() : null;
            boolean isMultiWindow = MultiWindowUtils.getInstance().isLegacyMultiWindow(activity)
                    || MultiWindowUtils.getInstance().isInMultiWindowMode(activity);

            // If the measured width is the same as the allowed width (i.e. the orientation has
            // not changed) and multi-window mode is off, use the largest measured height seen thus
            // far.  This will prevent surface resizes as a result of showing the keyboard.
            if (!topChanged && !isMultiWindow
                    && getMeasuredWidth() == MeasureSpec.getSize(widthMeasureSpec)
                    && getMeasuredHeight() > MeasureSpec.getSize(heightMeasureSpec)) {
                heightMeasureSpec =
                        MeasureSpec.makeMeasureSpec(getMeasuredHeight(), MeasureSpec.EXACTLY);
            }
        }
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        mPreviousWindowTop = -1;
    }

    /**
     * @return The ResourceManager.
     */
    public ResourceManager getResourceManager() {
        return mResourceManager;
    }

    /**
     * @return The active {@link SurfaceView} of this compositor.
     */
    public View getActiveSurfaceView() {
        return mCompositorSurfaceManager.getActiveSurfaceView();
    }

    /**
     * Should be called for cleanup when the CompositorView instance is no longer used.
     */
    public void shutDown() {
        mCompositorSurfaceManager.shutDown();
        if (mNativeCompositorView != 0) nativeDestroy(mNativeCompositorView);
        mNativeCompositorView = 0;
    }

    /**
     * Initializes the {@link CompositorView}'s native parts (e.g. the rendering parts).
     * @param lowMemDevice         If this is a low memory device.
     * @param windowAndroid        A {@link WindowAndroid} instance.
     * @param layerTitleCache      A {@link LayerTitleCache} instance.
     * @param tabContentManager    A {@link TabContentManager} instance.
     */
    public void initNativeCompositor(boolean lowMemDevice, WindowAndroid windowAndroid,
            LayerTitleCache layerTitleCache, TabContentManager tabContentManager) {
        // https://crbug.com/802160. We can't call setWindowAndroid here because updating the window
        // visibility here breaks exiting Reader Mode somehow.
        mWindowAndroid = windowAndroid;
        mLayerTitleCache = layerTitleCache;
        mTabContentManager = tabContentManager;

        mNativeCompositorView =
                nativeInit(lowMemDevice, windowAndroid, layerTitleCache, tabContentManager);

        // compositor_impl_android.cc will use 565 EGL surfaces if and only if we're using a low
        // memory device, and no alpha channel is desired.  Otherwise, it will use 8888.  Since
        // SurfaceFlinger doesn't need the eOpaque flag to optimize out alpha blending during
        // composition if the buffer has no alpha channel, we can avoid using the extra background
        // surface (and the memory it requires) in the low memory case.  The output buffer will
        // either have an alpha channel or not, depending on whether the compositor needs it.  We
        // can keep the surface translucent all the times without worrying about the impact on power
        // usage during SurfaceFlinger composition. We might also want to set |mAlwaysTranslucent|
        // on non-low memory devices, if we are running on hardware that implements efficient alpha
        // blending.
        mAlwaysTranslucent = lowMemDevice;

        // In case we changed the requested format due to |lowMemDevice|,
        // re-request the surface now.
        mCompositorSurfaceManager.requestSurface(getSurfacePixelFormat());

        setVisibility(View.VISIBLE);

        // Grab the Resource Manager
        mResourceManager = nativeGetResourceManager(mNativeCompositorView);

        // Redraw in case there are callbacks pending |mDrawingFinishedCallbacks|.
        nativeSetNeedsComposite(mNativeCompositorView);
    }

    private void setWindowAndroid(WindowAndroid windowAndroid) {
        mWindowAndroid = windowAndroid;
        onWindowVisibilityChangedInternal(getWindowVisibility());
    }

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        return super.onTouchEvent(e);
    }

    /**
     * Enables/disables overlay video mode. Affects alpha blending on this view.
     * @param enabled Whether to enter or leave overlay video mode.
     */
    public void setOverlayVideoMode(boolean enabled) {
        nativeSetOverlayVideoMode(mNativeCompositorView, enabled);

        mOverlayVideoEnabled = enabled;
        // Request the new surface, even if it's the same as the old one.  We'll get a synthetic
        // destroy / create / changed callback in that case, possibly before this returns.
        mCompositorSurfaceManager.requestSurface(getSurfacePixelFormat());
        // Note that we don't know if we'll get a surfaceCreated / surfaceDestoyed for this surface.
        // We do know that if we do get one, then it will be for the surface that we just requested.
    }

    private int getSurfacePixelFormat() {
        return (mOverlayVideoEnabled || mAlwaysTranslucent) ? PixelFormat.TRANSLUCENT
                                                            : PixelFormat.OPAQUE;
    }

    @Override
    public void surfaceRedrawNeededAsync(Runnable drawingFinished) {
        if (mDrawingFinishedCallbacks == null) mDrawingFinishedCallbacks = new ArrayList<>();
        mDrawingFinishedCallbacks.add(drawingFinished);
        if (mNativeCompositorView != 0) nativeSetNeedsComposite(mNativeCompositorView);
    }

    @Override
    public void surfaceChanged(Surface surface, int format, int width, int height) {
        if (mNativeCompositorView == 0) return;

        nativeSurfaceChanged(mNativeCompositorView, format, width, height, surface);
        mRenderHost.onSurfaceResized(width, height);
    }

    @Override
    public void surfaceCreated(Surface surface) {
        if (mNativeCompositorView == 0) return;

        nativeSurfaceCreated(mNativeCompositorView);
        mFramesUntilHideBackground = 2;
        mRenderHost.onSurfaceCreated();
    }

    @Override
    public void surfaceDestroyed(Surface surface) {
        if (mNativeCompositorView == 0) return;

        nativeSurfaceDestroyed(mNativeCompositorView);
    }

    @Override
    public void onWindowVisibilityChanged(int visibility) {
        super.onWindowVisibilityChanged(visibility);
        onWindowVisibilityChangedInternal(visibility);
    }

    private void onWindowVisibilityChangedInternal(int visibility) {
        if (mWindowAndroid == null) return;
        if (visibility == View.GONE) {
            mWindowAndroid.onVisibilityChanged(false);
        } else if (visibility == View.VISIBLE) {
            mWindowAndroid.onVisibilityChanged(true);
        }
        IntentWithGesturesHandler.getInstance().clear();
    }

    void onPhysicalBackingSizeChanged(WebContents webContents, int width, int height) {
        nativeOnPhysicalBackingSizeChanged(mNativeCompositorView, webContents, width, height);
    }

    @CalledByNative
    private void onCompositorLayout() {
        mRenderHost.onCompositorLayout();
    }

    @CalledByNative
    private void recreateSurface() {
        mCompositorSurfaceManager.recreateSurface();
    }

    /**
     * Request compositor view to render a frame.
     */
    public void requestRender() {
        if (mNativeCompositorView != 0) nativeSetNeedsComposite(mNativeCompositorView);
    }

    @CalledByNative
    private void didSwapFrame(int pendingFrameCount) {
        mRenderHost.didSwapFrame(pendingFrameCount);
    }

    @CalledByNative
    private void didSwapBuffers(boolean swappedCurrentSize) {
        // If we're in the middle of a surface swap, then see if we've received a new frame yet for
        // the new surface before hiding the outgoing surface.
        if (mFramesUntilHideBackground > 1) {
            // We need at least one more frame before we hide the outgoing surface.  Make sure that
            // there will be a frame.
            mFramesUntilHideBackground--;
            requestRender();
        } else if (mFramesUntilHideBackground == 1) {
            // We can hide the outgoing surface, since the incoming one has a frame.  It's okay if
            // we've don't have an unowned surface.
            mFramesUntilHideBackground = 0;
            mCompositorSurfaceManager.doneWithUnownedSurface();
        }

        // Only run our draw finished callbacks if the frame we swapped was the correct size.
        if (swappedCurrentSize) {
            runDrawFinishedCallbacks();
        }
    }

    /**
     * Converts the layout into compositor layers. This is to be called on every frame the layout
     * is changing.
     * @param provider               Provides the layout to be rendered.
     * @param forRotation            Whether or not this is a special draw during a rotation.
     */
    public void finalizeLayers(final LayoutProvider provider, boolean forRotation) {
        TraceEvent.begin("CompositorView:finalizeLayers");
        Layout layout = provider.getActiveLayout();
        if (layout == null || mNativeCompositorView == 0) {
            TraceEvent.end("CompositorView:finalizeLayers");
            return;
        }

        if (!mPreloadedResources) {
            // Attempt to prefetch any necessary resources
            mResourceManager.preloadResources(AndroidResourceType.STATIC,
                    StaticResourcePreloads.getSynchronousResources(getContext()),
                    StaticResourcePreloads.getAsynchronousResources(getContext()));
            mPreloadedResources = true;
        }

        // IMPORTANT: Do not do anything that impacts the compositor layer tree before this line.
        // If you do, you could inadvertently trigger follow up renders.  For further information
        // see dtrainor@, tedchoc@, or klobag@.

        nativeSetLayoutBounds(mNativeCompositorView);

        SceneLayer sceneLayer =
                provider.getUpdatedActiveSceneLayer(mLayerTitleCache, mTabContentManager,
                mResourceManager, provider.getFullscreenManager());

        nativeSetSceneLayer(mNativeCompositorView, sceneLayer);

        TabModelImpl.flushActualTabSwitchLatencyMetric();
        nativeFinalizeLayers(mNativeCompositorView);
        TraceEvent.end("CompositorView:finalizeLayers");
    }

    @Override
    public void setWillNotDraw(boolean willNotDraw) {
        mCompositorSurfaceManager.setWillNotDraw(willNotDraw);
    }

    @Override
    public void setBackgroundDrawable(Drawable background) {
        // We override setBackgroundDrawable since that's the common entry point from all the
        // setBackground* calls in View.  We still call to setBackground on the SurfaceView because
        // SetBackgroundDrawable is deprecated, and the semantics are the same I think.
        super.setBackgroundDrawable(background);
        mCompositorSurfaceManager.setBackgroundDrawable(background);
    }

    @Override
    public void setVisibility(int visibility) {
        super.setVisibility(visibility);
        // Also set the visibility on any child SurfaceViews, since that hides the surface as
        // well. Otherwise, the surface is kept, which can interfere with VR.
        mCompositorSurfaceManager.setVisibility(visibility);
        // Clear out any outstanding callbacks that won't run if set to invisible.
        if (visibility == View.INVISIBLE) {
            runDrawFinishedCallbacks();
        }
    }

    private void runDrawFinishedCallbacks() {
        List<Runnable> runnables = mDrawingFinishedCallbacks;
        mDrawingFinishedCallbacks = null;
        if (runnables == null) return;
        for (Runnable r : runnables) {
            r.run();
        }
    }

    /**
     * Replaces the surface manager and swaps the window the compositor is attached to as tab
     * reparenting doesn't handle replacing of the window the compositor uses.
     *
     * @param vrCompositorSurfaceManager The surface manager for VR.
     * @param window The VR WindowAndroid to switch to.
     */
    public void replaceSurfaceManagerForVr(
            CompositorSurfaceManager vrCompositorSurfaceManager, WindowAndroid window) {
        mCompositorSurfaceManager.shutDown();
        nativeSetCompositorWindow(mNativeCompositorView, window);
        mCompositorSurfaceManager = vrCompositorSurfaceManager;
        mCompositorSurfaceManager.requestSurface(PixelFormat.OPAQUE);
        nativeSetNeedsComposite(mNativeCompositorView);
        setWindowAndroid(window);
    }

    /**
     * Restores the non-VR surface manager and passes back control over the surface(s) to it.
     * Also restores the non-VR WindowAndroid.
     *
     * @param windowToRestore The non-VR WindowAndroid to restore.
     */
    public void onExitVr(WindowAndroid windowToRestore) {
        if (mNativeCompositorView == 0) return;
        setWindowAndroid(windowToRestore);
        mCompositorSurfaceManager.shutDown();
        nativeSetCompositorWindow(mNativeCompositorView, mWindowAndroid);
        mCompositorSurfaceManager = new CompositorSurfaceManagerImpl(this, this);
        mCompositorSurfaceManager.requestSurface(getSurfacePixelFormat());
        nativeSetNeedsComposite(mNativeCompositorView);
        mCompositorSurfaceManager.setVisibility(getVisibility());
    }

    private native long nativeInit(boolean lowMemDevice, WindowAndroid windowAndroid,
            LayerTitleCache layerTitleCache, TabContentManager tabContentManager);
    private native void nativeDestroy(long nativeCompositorView);
    private native ResourceManager nativeGetResourceManager(long nativeCompositorView);
    private native void nativeSurfaceCreated(long nativeCompositorView);
    private native void nativeSurfaceDestroyed(long nativeCompositorView);
    private native void nativeSurfaceChanged(
            long nativeCompositorView, int format, int width, int height, Surface surface);
    private native void nativeOnPhysicalBackingSizeChanged(
            long nativeCompositorView, WebContents webContents, int width, int height);
    private native void nativeFinalizeLayers(long nativeCompositorView);
    private native void nativeSetNeedsComposite(long nativeCompositorView);
    private native void nativeSetLayoutBounds(long nativeCompositorView);
    private native void nativeSetOverlayVideoMode(long nativeCompositorView, boolean enabled);
    private native void nativeSetSceneLayer(long nativeCompositorView, SceneLayer sceneLayer);
    private native void nativeSetCompositorWindow(long nativeCompositorView, WindowAndroid window);
}
