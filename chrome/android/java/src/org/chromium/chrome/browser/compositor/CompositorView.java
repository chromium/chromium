// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.view.Surface;
import android.view.View;
import android.widget.FrameLayout;

import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutProvider;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.compositor.resources.StaticResourcePreloads;
import org.chromium.chrome.browser.externalnav.IntentWithRequestMetadataHandler;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tabmodel.TabSwitchMetrics;
import org.chromium.components.browser_ui.styles.ChromeColors;
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
        extends FrameLayout implements CompositorSurfaceManager.SurfaceManagerCallbackTarget,
                                       WindowAndroid.SelectionHandlesObserver {
    private static final String TAG = "CompositorView";

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
    private TabContentManager mTabContentManager;

    private View mRootView;
    private boolean mPreloadedResources;
    private List<Runnable> mDrawingFinishedCallbacks;

    // True while the compositor view is in VR Browser mode (obsolescent), or in a WebXR
    // "immersive-ar" session with DOM Overlay enabled. This disables SurfaceControl while active.
    private boolean mIsInXr;

    private boolean mIsSurfaceControlEnabled;
    private boolean mSelectionHandlesActive;

    // On P and above, toggling the screen off gets us in a state where the Surface is destroyed but
    // it is never recreated when it is turned on again. This is the only workaround that seems to
    // be working, see crbug.com/931195.
    class ScreenStateReceiverWorkaround extends BroadcastReceiver {
        ScreenStateReceiverWorkaround() {
            IntentFilter filter = new IntentFilter(Intent.ACTION_SCREEN_OFF);
            getContext().getApplicationContext().registerReceiver(this, filter);
        }

        void shutDown() {
            getContext().getApplicationContext().unregisterReceiver(this);
        }

        @Override
        public void onReceive(Context context, Intent intent) {
            if (intent.getAction().equals(Intent.ACTION_SCREEN_OFF)
                    && mCompositorSurfaceManager != null && !mIsInXr
                    && mNativeCompositorView != 0) {
                mCompositorSurfaceManager.shutDown();
                createCompositorSurfaceManager();
            }
        }
    }

    private ScreenStateReceiverWorkaround mScreenStateReceiver;

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
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            mScreenStateReceiver = new ScreenStateReceiverWorkaround();
        }

        // Cover the black surface before it has valid content.  Set this placeholder view to
        // visible, but don't yet make SurfaceView visible, in order to delay
        // surfaceCreate/surfaceChanged calls until the native library is loaded.
        setBackgroundColor(ChromeColors.getPrimaryBackgroundColor(getResources(), false));
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
     * WindowAndroid.SelectionHandlesObserver impl.
     */
    @Override
    public void onSelectionHandlesStateChanged(boolean active) {
        // If the feature is disabled or we're in Vr mode, we are already rendering directly to the
        // SurfaceView.
        if (!mIsSurfaceControlEnabled || mIsInXr) return;

        if (mSelectionHandlesActive == active) return;
        mSelectionHandlesActive = active;

        // If selection handles are active, we need to switch to render to the SurfaceView so the
        // Magnifier widget can copy from its buffers to show in the UI.
        boolean switchToSurfaceView = mSelectionHandlesActive;

        // Cache the backbuffer for the currently visible Surface in the GPU process, if we're going
        // from SurfaceControl to SurfaceView. We need to preserve it until the new SurfaceView has
        // content.
        // When the CompositorImpl is switched to a new SurfaceView the Surface associated with the
        // current SurfaceView is disconnected from GL and its EGLSurface is destroyed. But the
        // buffers associated with that Surface are preserved (in android's internal BufferQueue)
        // when rendering directly to the SurfaceView. So caching the SurfaceView is enough to
        // preserve the old content.
        // But with SurfaceControl, switching to a new SurfaceView evicts that content when
        // destroying the GLSurface in the GPU process. So we need to explicitly preserve them in
        // the GPU process during this transition.
        if (switchToSurfaceView) {
            CompositorViewJni.get().cacheBackBufferForCurrentSurface(
                    mNativeCompositorView, CompositorView.this);
        }

        // Trigger the creation of a new SurfaceView. CompositorSurfaceManager will handle caching
        // the old one during the transition.
        mCompositorSurfaceManager.requestSurface(getSurfacePixelFormat());
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
        if (mScreenStateReceiver != null) {
            mScreenStateReceiver.shutDown();
        }
        if (mNativeCompositorView != 0) {
            CompositorViewJni.get().destroy(mNativeCompositorView, CompositorView.this);
        }
        mNativeCompositorView = 0;
    }

    /**
     * Initializes the {@link CompositorView}'s native parts (e.g. the rendering parts).
     * @param lowMemDevice         If this is a low memory device.
     * @param windowAndroid        A {@link WindowAndroid} instance.
     * @param tabContentManager    A {@link TabContentManager} instance.
     */
    public void initNativeCompositor(boolean lowMemDevice, WindowAndroid windowAndroid,
            TabContentManager tabContentManager) {
        // https://crbug.com/802160. We can't call setWindowAndroid here because updating the window
        // visibility here breaks exiting Reader Mode somehow.
        mWindowAndroid = windowAndroid;
        mWindowAndroid.addSelectionHandlesObserver(this);

        mTabContentManager = tabContentManager;

        mNativeCompositorView = CompositorViewJni.get().init(
                CompositorView.this, lowMemDevice, windowAndroid, tabContentManager);

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
        mResourceManager = CompositorViewJni.get().getResourceManager(
                mNativeCompositorView, CompositorView.this);

        // Redraw in case there are callbacks pending |mDrawingFinishedCallbacks|.
        CompositorViewJni.get().setNeedsComposite(mNativeCompositorView, CompositorView.this);
    }

    private void setWindowAndroid(WindowAndroid windowAndroid) {
        assert mWindowAndroid != null;
        mWindowAndroid.removeSelectionHandlesObserver(this);

        mWindowAndroid = windowAndroid;
        mWindowAndroid.addSelectionHandlesObserver(this);
        onWindowVisibilityChangedInternal(getWindowVisibility());
    }

    /**
     * Enables/disables overlay video mode. Affects alpha blending on this view.
     * @param enabled Whether to enter or leave overlay video mode.
     */
    public void setOverlayVideoMode(boolean enabled) {
        CompositorViewJni.get().setOverlayVideoMode(
                mNativeCompositorView, CompositorView.this, enabled);

        mOverlayVideoEnabled = enabled;
        // Request the new surface, even if it's the same as the old one.  We'll get a synthetic
        // destroy / create / changed callback in that case, possibly before this returns.
        mCompositorSurfaceManager.requestSurface(getSurfacePixelFormat());
        // Note that we don't know if we'll get a surfaceCreated / surfaceDestoyed for this surface.
        // We do know that if we do get one, then it will be for the surface that we just requested.
    }

    /**
     * Enables/disables immersive AR overlay mode, a variant of overlay video mode.
     * @param enabled Whether to enter or leave overlay immersive ar mode.
     */
    public void setOverlayImmersiveArMode(boolean enabled, boolean domSurfaceNeedsConfiguring) {
        // Disable SurfaceControl for the duration of the session. This works around a black
        // screen after activating the screen keyboard (IME), see https://crbug.com/1166248.
        mIsInXr = enabled;

        if (domSurfaceNeedsConfiguring) {
            setOverlayVideoMode(enabled);
        }

        CompositorViewJni.get().setOverlayImmersiveArMode(
                mNativeCompositorView, CompositorView.this, enabled);
        // Entering or exiting AR mode can leave SurfaceControl in a confused state, especially if
        // the screen keyboard (IME) was activated, see https://crbug.com/1166248 and
        // https://crbug.com/1169822. Reset the surface manager at session start and exit to work
        // around this.
        mCompositorSurfaceManager.shutDown();
        createCompositorSurfaceManager();
    }

    private int getSurfacePixelFormat() {
        if (mOverlayVideoEnabled || mAlwaysTranslucent) {
            return PixelFormat.TRANSLUCENT;
        }

        if (mIsSurfaceControlEnabled) {
            // In SurfaceControl mode, we can always use a translucent format since there is no
            // buffer associated to the SurfaceView, and the buffers passed to the SurfaceControl
            // API are correctly tagged with whether blending is needed in the GPU process itself.
            // But if we need to temporarily render directly to a SurfaceView, then opaque format is
            // needed.
            // The transition between the 2 also relies on them using different Surfaces (through
            // different format requests).
            return canUseSurfaceControl() ? PixelFormat.TRANSLUCENT : PixelFormat.OPAQUE;
        }

        return PixelFormat.OPAQUE;
    }

    private boolean canUseSurfaceControl() {
        return !mIsInXr && !mSelectionHandlesActive;
    }

    @Override
    public void surfaceRedrawNeededAsync(Runnable drawingFinished) {
        if (mDrawingFinishedCallbacks == null) {
            mDrawingFinishedCallbacks = new ArrayList<>();
        }
        mDrawingFinishedCallbacks.add(drawingFinished);
        if (mNativeCompositorView != 0) {
            CompositorViewJni.get().setNeedsComposite(mNativeCompositorView, CompositorView.this);
        }
    }

    @Override
    public void surfaceChanged(Surface surface, int format, int width, int height) {
        if (mNativeCompositorView == 0) return;

        CompositorViewJni.get().surfaceChanged(mNativeCompositorView, CompositorView.this, format,
                width, height, canUseSurfaceControl(), surface);
        mRenderHost.onSurfaceResized(width, height);
    }

    @Override
    public void surfaceCreated(Surface surface) {
        if (mNativeCompositorView == 0) return;

        CompositorViewJni.get().surfaceCreated(mNativeCompositorView, CompositorView.this);
        mFramesUntilHideBackground = 2;
        mRenderHost.onSurfaceCreated();
    }

    @Override
    public void surfaceDestroyed(Surface surface, boolean androidSurfaceDestroyed) {
        if (mNativeCompositorView == 0) return;

        // When we switch from Chrome to other app we can't detach child surface controls because it
        // leads to a visible hole: b/157439199. To avoid this we don't detach surfaces if the
        // surface is going to be destroyed, they will be detached and freed by OS.
        if (androidSurfaceDestroyed) {
            CompositorViewJni.get().preserveChildSurfaceControls(
                    mNativeCompositorView, CompositorView.this);
        }

        CompositorViewJni.get().surfaceDestroyed(mNativeCompositorView, CompositorView.this);
    }

    @Override
    public void unownedSurfaceDestroyed() {
        CompositorViewJni.get().evictCachedBackBuffer(mNativeCompositorView, CompositorView.this);
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
        IntentWithRequestMetadataHandler.getInstance().clear();
    }

    void onPhysicalBackingSizeChanged(WebContents webContents, int width, int height) {
        CompositorViewJni.get().onPhysicalBackingSizeChanged(
                mNativeCompositorView, CompositorView.this, webContents, width, height);
    }

    void onControlsResizeViewChanged(WebContents webContents, boolean controlsResizeView) {
        CompositorViewJni.get().onControlsResizeViewChanged(
                mNativeCompositorView, CompositorView.this, webContents, controlsResizeView);
    }

    /**
     * Notifies geometrychange event to JS.
     * @param webContents Active WebContent for which this event needs to be fired.
     * @param x When the keyboard is shown, it has the left position of the app's rect, else, 0.
     * @param y When the keyboard is shown, it has the top position of the app's rect, else, 0.
     * @param width  When the keyboard is shown, it has the width of the view, else, 0.
     * @param height When the keyboard is shown, it has the height of the keyboard, else, 0.
     */
    void notifyVirtualKeyboardOverlayRect(
            WebContents webContents, int x, int y, int width, int height) {
        CompositorViewJni.get().notifyVirtualKeyboardOverlayRect(
                mNativeCompositorView, CompositorView.this, webContents, x, y, width, height);
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
        if (mNativeCompositorView != 0) {
            CompositorViewJni.get().setNeedsComposite(mNativeCompositorView, CompositorView.this);
        }
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

            // Evict the SurfaceView and the associated backbuffer now that the new SurfaceView is
            // ready.
            CompositorViewJni.get().evictCachedBackBuffer(
                    mNativeCompositorView, CompositorView.this);
            mCompositorSurfaceManager.doneWithUnownedSurface();
        }

        // Only run our draw finished callbacks if the frame we swapped was the correct size.
        if (swappedCurrentSize) {
            runDrawFinishedCallbacks();
        }

        mRenderHost.didSwapBuffers(swappedCurrentSize);
    }

    @CalledByNative
    private void notifyWillUseSurfaceControl() {
        mIsSurfaceControlEnabled = true;
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

        CompositorViewJni.get().setLayoutBounds(mNativeCompositorView, CompositorView.this);

        SceneLayer sceneLayer = provider.getUpdatedActiveSceneLayer(mTabContentManager,
                mResourceManager, provider.getBrowserControlsManager());

        CompositorViewJni.get().setSceneLayer(
                mNativeCompositorView, CompositorView.this, sceneLayer);

        TabSwitchMetrics.flushActualTabSwitchLatencyMetric();
        CompositorViewJni.get().finalizeLayers(mNativeCompositorView, CompositorView.this);
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
        mIsInXr = true;

        mCompositorSurfaceManager.shutDown();
        CompositorViewJni.get().setCompositorWindow(
                mNativeCompositorView, CompositorView.this, window);
        mCompositorSurfaceManager = vrCompositorSurfaceManager;
        mCompositorSurfaceManager.requestSurface(PixelFormat.OPAQUE);
        CompositorViewJni.get().setNeedsComposite(mNativeCompositorView, CompositorView.this);
        setWindowAndroid(window);
    }

    /**
     * Restores the non-VR surface manager and passes back control over the surface(s) to it.
     * Also restores the non-VR WindowAndroid.
     *
     * @param windowToRestore The non-VR WindowAndroid to restore.
     */
    public void onExitVr(WindowAndroid windowToRestore) {
        mIsInXr = false;

        if (mNativeCompositorView == 0) return;
        setWindowAndroid(windowToRestore);
        mCompositorSurfaceManager.shutDown();
        CompositorViewJni.get().setCompositorWindow(
                mNativeCompositorView, CompositorView.this, mWindowAndroid);
        createCompositorSurfaceManager();
    }

    private void createCompositorSurfaceManager() {
        mCompositorSurfaceManager = new CompositorSurfaceManagerImpl(this, this);
        mCompositorSurfaceManager.requestSurface(getSurfacePixelFormat());
        CompositorViewJni.get().setNeedsComposite(mNativeCompositorView, CompositorView.this);
        mCompositorSurfaceManager.setVisibility(getVisibility());
    }

    /**
     * Notifies the native compositor that a tab change has occurred. This
     * should be called when changing to a valid tab.
     */
    public void onTabChanged() {
        CompositorViewJni.get().onTabChanged(mNativeCompositorView, CompositorView.this);
    }

    @NativeMethods
    interface Natives {
        long init(CompositorView caller, boolean lowMemDevice, WindowAndroid windowAndroid,
                TabContentManager tabContentManager);
        void destroy(long nativeCompositorView, CompositorView caller);
        ResourceManager getResourceManager(long nativeCompositorView, CompositorView caller);
        void surfaceCreated(long nativeCompositorView, CompositorView caller);
        void surfaceDestroyed(long nativeCompositorView, CompositorView caller);
        void surfaceChanged(long nativeCompositorView, CompositorView caller, int format, int width,
                int height, boolean backedBySurfaceTexture, Surface surface);
        void onPhysicalBackingSizeChanged(long nativeCompositorView, CompositorView caller,
                WebContents webContents, int width, int height);
        void onControlsResizeViewChanged(long nativeCompositorView, CompositorView caller,
                WebContents webContents, boolean controlsResizeView);
        void notifyVirtualKeyboardOverlayRect(long nativeCompositorView, CompositorView caller,
                WebContents webContents, int x, int y, int width, int height);
        void finalizeLayers(long nativeCompositorView, CompositorView caller);
        void setNeedsComposite(long nativeCompositorView, CompositorView caller);
        void setLayoutBounds(long nativeCompositorView, CompositorView caller);
        void setOverlayVideoMode(long nativeCompositorView, CompositorView caller, boolean enabled);
        void setOverlayImmersiveArMode(
                long nativeCompositorView, CompositorView caller, boolean enabled);
        void setSceneLayer(long nativeCompositorView, CompositorView caller, SceneLayer sceneLayer);
        void setCompositorWindow(
                long nativeCompositorView, CompositorView caller, WindowAndroid window);
        void cacheBackBufferForCurrentSurface(long nativeCompositorView, CompositorView caller);
        void evictCachedBackBuffer(long nativeCompositorView, CompositorView caller);
        void onTabChanged(long nativeCompositorView, CompositorView caller);
        void preserveChildSurfaceControls(long nativeCompositorView, CompositorView caller);
    }
}
