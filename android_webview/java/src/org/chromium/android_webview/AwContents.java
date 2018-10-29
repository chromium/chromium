// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.Activity;
import android.content.ComponentCallbacks2;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorMatrix;
import android.graphics.ColorMatrixColorFilter;
import android.graphics.Paint;
import android.graphics.Picture;
import android.graphics.Rect;
import android.net.Uri;
import android.net.http.SslCertificate;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.SystemClock;
import android.support.annotation.IntDef;
import android.text.TextUtils;
import android.util.Base64;
import android.util.Pair;
import android.util.SparseArray;
import android.view.DragEvent;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStructure;
import android.view.accessibility.AccessibilityNodeProvider;
import android.view.animation.AnimationUtils;
import android.view.autofill.AutofillValue;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.textclassifier.TextClassifier;
import android.webkit.JavascriptInterface;

import org.chromium.android_webview.permission.AwGeolocationCallback;
import org.chromium.android_webview.permission.AwPermissionRequest;
import org.chromium.android_webview.renderer_priority.RendererPriority;
import org.chromium.base.Callback;
import org.chromium.base.LocaleUtils;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.components.autofill.AutofillProvider;
import org.chromium.components.navigation_interception.InterceptNavigationDelegate;
import org.chromium.components.navigation_interception.NavigationParams;
import org.chromium.content_public.browser.ChildProcessImportance;
import org.chromium.content_public.browser.ContentViewStatics;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.ImeEventObserver;
import org.chromium.content_public.browser.JavaScriptCallback;
import org.chromium.content_public.browser.JavascriptInjector;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.MessagePort;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.content_public.browser.SelectionClient;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.SmartClipProvider;
import org.chromium.content_public.browser.ViewEventSink;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsAccessibility;
import org.chromium.content_public.browser.WebContentsInternals;
import org.chromium.content_public.browser.navigation_controller.LoadURLType;
import org.chromium.content_public.browser.navigation_controller.UserAgentOverrideOption;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.content_public.common.Referrer;
import org.chromium.content_public.common.UseZoomForDSFPolicy;
import org.chromium.device.gamepad.GamepadList;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.network.mojom.ReferrerPolicy;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid.DisplayAndroidObserver;

import java.io.File;
import java.lang.annotation.Annotation;
import java.lang.ref.WeakReference;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;
import java.util.WeakHashMap;
import java.util.concurrent.Callable;

/**
 * Exposes the native AwContents class, and together these classes wrap the WebContents
 * and Browser components that are required to implement Android WebView API. This is the
 * primary entry point for the WebViewProvider implementation; it holds a 1:1 object
 * relationship with application WebView instances.
 * (We define this class independent of the hidden WebViewProvider interfaces, to allow
 * continuous build &amp; test in the open source SDK-based tree).
 */
@JNINamespace("android_webview")
public class AwContents implements SmartClipProvider {
    private static final String TAG = "AwContents";
    private static final boolean TRACE = false;
    private static final int NO_WARN = 0;
    private static final int WARN = 1;
    private static final String PRODUCT_VERSION = AwContentsStatics.getProductVersion();

    private static final String WEB_ARCHIVE_EXTENSION = ".mht";
    // The request code should be unique per WebView/AwContents object.
    private static final int PROCESS_TEXT_REQUEST_CODE = 100;

    // Used to avoid enabling zooming in / out if resulting zooming will
    // produce little visible difference.
    private static final float ZOOM_CONTROLS_EPSILON = 0.007f;

    private static final double MIN_SCREEN_HEIGHT_PERCENTAGE_FOR_INTERSTITIAL = 0.7;

    private static final String SAMSUNG_WORKAROUND_BASE_URL = "email://";
    private static final int SAMSUNG_WORKAROUND_DELAY = 200;

    public static final String DATA_URI_HISTOGRAM_NAME =
            "Android.WebView.LoadUrl.DataUriHasOctothorpe";

    private static class ForceAuxiliaryBitmapRendering {
        private static final boolean sResult = lazyCheck();
        private static boolean lazyCheck() {
            return !nativeHasRequiredHardwareExtensions();
        }
    }

    // Used to record the UMA histogram WebView.LoadDataWithBaseUrl.HistoryUrl. Since these values
    // are persisted to logs, they should never be renumbered nor reused.
    @IntDef({HistoryUrl.EMPTY, HistoryUrl.BASEURL, HistoryUrl.DIFFERENT, HistoryUrl.COUNT})
    @interface HistoryUrl {
        int EMPTY = 0;
        int BASEURL = 1;
        int DIFFERENT = 2;
        int COUNT = 3;
    }

    /**
     * WebKit hit test related data structure. These are used to implement
     * getHitTestResult, requestFocusNodeHref, requestImageRef methods in WebView.
     * All values should be updated together. The native counterpart is
     * AwHitTestData.
     */
    public static class HitTestData {
        // Used in getHitTestResult.
        public int hitTestResultType;
        public String hitTestResultExtraData;

        // Used in requestFocusNodeHref (all three) and requestImageRef (only imgSrc).
        public String href;
        public String anchorText;
        public String imgSrc;
    }

    /**
     * Interface that consumers of {@link AwContents} must implement to allow the proper
     * dispatching of view methods through the containing view.
     */
    public interface InternalAccessDelegate extends ViewEventSink.InternalAccessDelegate {
        /**
         * @see View#overScrollBy(int, int, int, int, int, int, int, int, boolean);
         */
        void overScrollBy(int deltaX, int deltaY,
                int scrollX, int scrollY,
                int scrollRangeX, int scrollRangeY,
                int maxOverScrollX, int maxOverScrollY,
                boolean isTouchEvent);

        /**
         * @see View#scrollTo(int, int)
         */
        void super_scrollTo(int scrollX, int scrollY);

        /**
         * @see View#setMeasuredDimension(int, int)
         */
        void setMeasuredDimension(int measuredWidth, int measuredHeight);

        /**
         * @see View#getScrollBarStyle()
         */
        int super_getScrollBarStyle();

        /**
         * @see View#startActivityForResult(Intent, int)
         */
        void super_startActivityForResult(Intent intent, int requestCode);

        /**
         * @see View#onConfigurationChanged(Configuration)
         */
        void super_onConfigurationChanged(Configuration newConfig);
    }

    /**
     * Factory interface used for constructing functors that the Android framework uses for
     * calling back into Chromium code to render the the contents of a Chromium frame into
     * an Android view.
     */
    public interface NativeDrawGLFunctorFactory {
        /**
         * Create a functor associated with native context |context|.
         */
        NativeDrawGLFunctor createFunctor(long context);
    }

    /**
     * Interface that consumers of {@link AwContents} must implement to support
     * native GL rendering.
     */
    public interface NativeDrawGLFunctor {
        /**
         * Requests a callback on the native DrawGL method (see getAwDrawGLFunction).
         *
         * If called from within onDraw, |canvas| should be non-null and must be hardware
         * accelerated. |releasedCallback| should be null if |canvas| is null, or if
         * supportsDrawGLFunctorReleasedCallback returns false.
         *
         * @return false indicates the GL draw request was not accepted, and the caller
         *         should fallback to the SW path.
         */
        boolean requestDrawGL(Canvas canvas, Runnable releasedCallback);

        /**
         * Requests a callback on the native DrawGL method (see getAwDrawGLFunction).
         *
         * |containerView| must be hardware accelerated. If |waitForCompletion| is true, this method
         * will not return until functor has returned.
         */
        boolean requestInvokeGL(View containerView, boolean waitForCompletion);

        /**
         * Test whether the Android framework supports notifying when a functor is free
         * to be destroyed via the callback mechanism provided to the functor factory.
         *
         * @return true if destruction needs to wait on a framework callback, or false
         *         if it can occur immediately.
         */
        boolean supportsDrawGLFunctorReleasedCallback();

        /**
         * Detaches the GLFunctor from the view tree.
         */
        void detach(View containerView);

        /**
         * Get a Runnable that is used to destroy the native portion of the functor. After the
         * run method of this Runnable is called, no other methods should be called on the Java
         * object.
         */
        Runnable getDestroyRunnable();

    }

    /**
     * Class to facilitate dependency injection. Subclasses by test code to provide mock versions of
     * certain AwContents dependencies.
     */
    public static class DependencyFactory {
        public AwLayoutSizer createLayoutSizer() {
            return new AwLayoutSizer();
        }

        public AwScrollOffsetManager createScrollOffsetManager(
                AwScrollOffsetManager.Delegate delegate) {
            return new AwScrollOffsetManager(delegate);
        }

        public AutofillProvider createAutofillProvider(Context context, ViewGroup containerView) {
            return null;
        }
    }

    /**
     * Visual state callback, see {@link #insertVisualStateCallback} for details.
     *
     */
    @VisibleForTesting
    public abstract static class VisualStateCallback {
        /**
         * @param requestId the id passed to {@link AwContents#insertVisualStateCallback}
         * which can be used to match requests with the corresponding callbacks.
         */
        public abstract void onComplete(long requestId);
    }

    private long mNativeAwContents;
    private final AwBrowserContext mBrowserContext;
    // mContainerView and mCurrentFunctor form a pair that needs to stay in sync.
    private ViewGroup mContainerView;
    private AwGLFunctor mCurrentFunctor;
    private AwGLFunctor mInitialFunctor;
    private AwGLFunctor mFullScreenFunctor; // Only non-null when in fullscreen mode.
    private final Context mContext;
    private final int mAppTargetSdkVersion;
    private AwViewAndroidDelegate mViewAndroidDelegate;
    private WindowAndroidWrapper mWindowAndroid;
    private WebContents mWebContents;
    private ViewEventSink mViewEventSink;
    private WebContentsInternalsHolder mWebContentsInternalsHolder;
    private NavigationController mNavigationController;
    private final AwContentsClient mContentsClient;
    private AwWebContentsObserver mWebContentsObserver;
    private final AwContentsClientBridge mContentsClientBridge;
    private final AwWebContentsDelegateAdapter mWebContentsDelegate;
    private final AwContentsBackgroundThreadClient mBackgroundThreadClient;
    private final AwContentsIoThreadClient mIoThreadClient;
    private final InterceptNavigationDelegateImpl mInterceptNavigationDelegate;
    private InternalAccessDelegate mInternalAccessAdapter;
    private final NativeDrawGLFunctorFactory mNativeDrawGLFunctorFactory;
    private final AwLayoutSizer mLayoutSizer;
    private final AwZoomControls mZoomControls;
    private final AwScrollOffsetManager mScrollOffsetManager;
    private OverScrollGlow mOverScrollGlow;
    private final DisplayAndroidObserver mDisplayObserver;
    // This can be accessed on any thread after construction. See AwContentsIoThreadClient.
    private final AwSettings mSettings;
    private final ScrollAccessibilityHelper mScrollAccessibilityHelper;

    private final ObserverList<PopupTouchHandleDrawable> mTouchHandleDrawables =
            new ObserverList<>();

    private boolean mIsPaused;
    private boolean mIsViewVisible;
    private boolean mIsWindowVisible;
    private boolean mIsAttachedToWindow;
    // Visiblity state of |mWebContents|.
    private boolean mIsContentVisible;
    private boolean mIsUpdateVisibilityTaskPending;
    private Runnable mUpdateVisibilityRunnable;

    private @RendererPriority int mRendererPriority;
    private boolean mRendererPriorityWaivedWhenNotVisible;

    private Bitmap mFavicon;
    private boolean mHasRequestedVisitedHistoryFromClient;
    // Whether this WebView is a popup.
    private boolean mIsPopupWindow;

    // The base background color, i.e. not accounting for any CSS body from the current page.
    private int mBaseBackgroundColor = Color.WHITE;

    // Must call nativeUpdateLastHitTestData first to update this before use.
    private final HitTestData mPossiblyStaleHitTestData = new HitTestData();

    private final DefaultVideoPosterRequestHandler mDefaultVideoPosterRequestHandler;

    // Bound method for suppling Picture instances to the AwContentsClient. Will be null if the
    // picture listener API has not yet been enabled, or if it is using invalidation-only mode.
    private Callable<Picture> mPictureListenerContentProvider;

    private boolean mContainerViewFocused;
    private boolean mWindowFocused;

    // These come from the compositor and are updated synchronously (in contrast to the values in
    // RenderCoordinates, which are updated at end of every frame).
    private float mPageScaleFactor = 1.0f;
    private float mMinPageScaleFactor = 1.0f;
    private float mMaxPageScaleFactor = 1.0f;
    private float mContentWidthDip;
    private float mContentHeightDip;

    private AwAutofillClient mAwAutofillClient;

    private AwPdfExporter mAwPdfExporter;

    private AwViewMethods mAwViewMethods;
    private final FullScreenTransitionsState mFullScreenTransitionsState;

    // This flag indicates that ShouldOverrideUrlNavigation should be posted
    // through the resourcethrottle. This is only used for popup windows.
    private boolean mDeferredShouldOverrideUrlLoadingIsPendingForPopup;

    // This is a workaround for some qualcomm devices discarding buffer on
    // Activity restore.
    private boolean mInvalidateRootViewOnNextDraw;

    // The framework may temporarily detach our container view, for example during layout if
    // we are a child of a ListView. This may cause many toggles of View focus, which we suppress
    // when in this state.
    private boolean mTemporarilyDetached;

    private Handler mHandler;

    // True when this AwContents has been destroyed.
    // Do not use directly, call isDestroyed() instead.
    private boolean mIsDestroyed;

    // True if this AwContents is in no operation state.
    // Do not use directly, call isNoOperation() instead.
    private boolean mIsNoOperation;

    private AutofillProvider mAutofillProvider;

    private static String sCurrentLocales = "";

    private Paint mPaintForNWorkaround;

    // A holder of objects passed from WebContents and should be owned by AwContents that may
    // have direct or indirect reference back to WebView. They are used internally by
    // WebContents but all the references can create a new gc root that can keep WebView
    // instances from being freed when they are detached from view tree, hence lead to
    // memory leak. To avoid the issue, it is possible to use |WebContents.setInternalHolder|
    // to move the holder of those internal objects to AwContents. Note that they are still
    // used by WebContents, and AwContents doesn't have to know what's inside the holder.
    private WebContentsInternals mWebContentsInternals;

    private JavascriptInjector mJavascriptInjector;

    private static class WebContentsInternalsHolder implements WebContents.InternalsHolder {
        private final WeakReference<AwContents> mAwContentsRef;

        private WebContentsInternalsHolder(AwContents awContents) {
            mAwContentsRef = new WeakReference<>(awContents);
        }

        @Override
        public void set(WebContentsInternals internals) {
            AwContents awContents = mAwContentsRef.get();
            if (awContents == null) {
                throw new IllegalStateException("AwContents should be available at this time");
            }
            awContents.mWebContentsInternals = internals;
        }

        @Override
        public WebContentsInternals get() {
            AwContents awContents = mAwContentsRef.get();
            return awContents == null ? null : awContents.mWebContentsInternals;
        }

        public boolean weakRefCleared() {
            return mAwContentsRef.get() == null;
        }
    }

    private static final class AwContentsDestroyRunnable implements Runnable {
        private final long mNativeAwContents;
        // Hold onto a reference to the window (via its wrapper), so that it is not destroyed
        // until we are done here.
        private final WindowAndroidWrapper mWindowAndroid;

        private AwContentsDestroyRunnable(
                long nativeAwContents, WindowAndroidWrapper windowAndroid) {
            mNativeAwContents = nativeAwContents;
            mWindowAndroid = windowAndroid;
        }

        @Override
        public void run() {
            nativeDestroy(mNativeAwContents);
        }
    }

    /**
     * A class that stores the state needed to enter and exit fullscreen.
     */
    private static class FullScreenTransitionsState {
        private final ViewGroup mInitialContainerView;
        private final InternalAccessDelegate mInitialInternalAccessAdapter;
        private final AwViewMethods mInitialAwViewMethods;
        private FullScreenView mFullScreenView;
        /** Whether the initial container view was focused when we entered fullscreen */
        private boolean mWasInitialContainerViewFocused;

        private FullScreenTransitionsState(ViewGroup initialContainerView,
                InternalAccessDelegate initialInternalAccessAdapter,
                AwViewMethods initialAwViewMethods) {
            mInitialContainerView = initialContainerView;
            mInitialInternalAccessAdapter = initialInternalAccessAdapter;
            mInitialAwViewMethods = initialAwViewMethods;
        }

        private void enterFullScreen(FullScreenView fullScreenView,
                boolean wasInitialContainerViewFocused) {
            mFullScreenView = fullScreenView;
            mWasInitialContainerViewFocused = wasInitialContainerViewFocused;
        }

        private boolean wasInitialContainerViewFocused() {
            return mWasInitialContainerViewFocused;
        }

        private void exitFullScreen() {
            mFullScreenView = null;
        }

        private boolean isFullScreen() {
            return mFullScreenView != null;
        }

        private ViewGroup getInitialContainerView() {
            return mInitialContainerView;
        }

        private InternalAccessDelegate getInitialInternalAccessDelegate() {
            return mInitialInternalAccessAdapter;
        }

        private AwViewMethods getInitialAwViewMethods() {
            return mInitialAwViewMethods;
        }

        private FullScreenView getFullScreenView() {
            return mFullScreenView;
        }
    }

    // Reference to the active mNativeAwContents pointer while it is active use
    // (ie before it is destroyed).
    private CleanupReference mCleanupReference;

    //--------------------------------------------------------------------------------------------
    private class IoThreadClientImpl extends AwContentsIoThreadClient {
        // All methods are called on the IO thread.

        @Override
        public int getCacheMode() {
            return mSettings.getCacheMode();
        }

        @Override
        public AwContentsBackgroundThreadClient getBackgroundThreadClient() {
            return mBackgroundThreadClient;
        }

        @Override
        public boolean shouldBlockContentUrls() {
            return !mSettings.getAllowContentAccess();
        }

        @Override
        public boolean shouldBlockFileUrls() {
            return !mSettings.getAllowFileAccess();
        }

        @Override
        public boolean shouldBlockNetworkLoads() {
            return mSettings.getBlockNetworkLoads();
        }

        @Override
        public boolean shouldAcceptThirdPartyCookies() {
            return mSettings.getAcceptThirdPartyCookies();
        }

        @Override
        public boolean getSafeBrowsingEnabled() {
            return mSettings.getSafeBrowsingEnabled();
        }
    }

    private class BackgroundThreadClientImpl extends AwContentsBackgroundThreadClient {
        // All methods are called on the background thread.

        @Override
        public AwWebResourceResponse shouldInterceptRequest(
                AwContentsClient.AwWebResourceRequest request) {
            String url = request.url;
            AwWebResourceResponse awWebResourceResponse;
            // Return the response directly if the url is default video poster url.
            awWebResourceResponse = mDefaultVideoPosterRequestHandler.shouldInterceptRequest(url);
            if (awWebResourceResponse != null) return awWebResourceResponse;

            awWebResourceResponse = mContentsClient.shouldInterceptRequest(request);

            if (awWebResourceResponse == null) {
                mContentsClient.getCallbackHelper().postOnLoadResource(url);
            }

            if (awWebResourceResponse != null && awWebResourceResponse.getData() == null) {
                // In this case the intercepted URLRequest job will simulate an empty response
                // which doesn't trigger the onReceivedError callback. For WebViewClassic
                // compatibility we synthesize that callback.  http://crbug.com/180950
                mContentsClient.getCallbackHelper().postOnReceivedError(
                        request,
                        /* error description filled in by the glue layer */
                        new AwContentsClient.AwWebResourceError());
            }
            return awWebResourceResponse;
        }
    }

    //--------------------------------------------------------------------------------------------
    // When the navigation is for a newly created WebView (i.e. a popup), intercept the navigation
    // here for implementing shouldOverrideUrlLoading. This is to send the shouldOverrideUrlLoading
    // callback to the correct WebViewClient that is associated with the WebView.
    // Otherwise, use this delegate only to post onPageStarted messages.
    //
    // We are not using WebContentsObserver.didStartLoading because of stale URLs, out of order
    // onPageStarted's and double onPageStarted's.
    //
    private class InterceptNavigationDelegateImpl implements InterceptNavigationDelegate {
        @Override
        public boolean shouldIgnoreNavigation(NavigationParams navigationParams) {
            // The shouldOverrideUrlLoading call might have resulted in posting messages to the
            // UI thread. Using sendMessage here (instead of calling onPageStarted directly)
            // will allow those to run in order.
            if (!navigationParams.isRendererInitiated) {
                mContentsClient.getCallbackHelper().postOnPageStarted(navigationParams.url);
            }
            return false;
        }
    }

    //--------------------------------------------------------------------------------------------
    private class AwLayoutSizerDelegate implements AwLayoutSizer.Delegate {
        @Override
        public void requestLayout() {
            mContainerView.requestLayout();
        }

        @Override
        public void setMeasuredDimension(int measuredWidth, int measuredHeight) {
            mInternalAccessAdapter.setMeasuredDimension(measuredWidth, measuredHeight);
        }

        @Override
        public boolean isLayoutParamsHeightWrapContent() {
            return mContainerView.getLayoutParams() != null
                    && (mContainerView.getLayoutParams().height
                            == ViewGroup.LayoutParams.WRAP_CONTENT);
        }

        @Override
        public void setForceZeroLayoutHeight(boolean forceZeroHeight) {
            getSettings().setForceZeroLayoutHeight(forceZeroHeight);
        }
    }

    //--------------------------------------------------------------------------------------------
    private class AwScrollOffsetManagerDelegate implements AwScrollOffsetManager.Delegate {
        @Override
        public void overScrollContainerViewBy(int deltaX, int deltaY, int scrollX, int scrollY,
                int scrollRangeX, int scrollRangeY, boolean isTouchEvent) {
            mInternalAccessAdapter.overScrollBy(deltaX, deltaY, scrollX, scrollY,
                    scrollRangeX, scrollRangeY, 0, 0, isTouchEvent);
        }

        @Override
        public void scrollContainerViewTo(int x, int y) {
            mInternalAccessAdapter.super_scrollTo(x, y);
        }

        @Override
        public void scrollNativeTo(int x, int y) {
            if (!isDestroyedOrNoOperation(NO_WARN)) nativeScrollTo(mNativeAwContents, x, y);
        }

        @Override
        public void smoothScroll(int targetX, int targetY, long durationMs) {
            if (!isDestroyedOrNoOperation(NO_WARN)) {
                nativeSmoothScroll(mNativeAwContents, targetX, targetY, durationMs);
            }
        }

        @Override
        public int getContainerViewScrollX() {
            return mContainerView.getScrollX();
        }

        @Override
        public int getContainerViewScrollY() {
            return mContainerView.getScrollY();
        }

        @Override
        public void invalidate() {
            postInvalidateOnAnimation();
        }

        @Override
        public void cancelFling() {
            mWebContents.getEventForwarder().cancelFling(SystemClock.uptimeMillis());
        }
    }

    //--------------------------------------------------------------------------------------------
    private class AwGestureStateListener implements GestureStateListener {
        @Override
        public void onPinchStarted() {
            // While it's possible to re-layout the view during a pinch gesture, the effect is very
            // janky (especially that the page scale update notification comes from the renderer
            // main thread, not from the impl thread, so it's usually out of sync with what's on
            // screen). It's also quite expensive to do a re-layout, so we simply postpone
            // re-layout for the duration of the gesture. This is compatible with what
            // WebViewClassic does.
            mLayoutSizer.freezeLayoutRequests();
        }

        @Override
        public void onPinchEnded() {
            mLayoutSizer.unfreezeLayoutRequests();
        }

        @Override
        public void onScrollUpdateGestureConsumed() {
            mScrollAccessibilityHelper.postViewScrolledAccessibilityEventCallback();
            mZoomControls.invokeZoomPicker();
        }

        @Override
        public void onScrollStarted(int scrollOffsetY, int scrollExtentY) {
            mZoomControls.invokeZoomPicker();
        }

        @Override
        public void onScaleLimitsChanged(float minPageScaleFactor, float maxPageScaleFactor) {
            mZoomControls.updateZoomControls();
        }

        @Override
        public void onScrollOffsetOrExtentChanged(int scrollOffsetY, int scrollExtentY) {
            mZoomControls.updateZoomControls();
        }
    }

    //--------------------------------------------------------------------------------------------
    private class AwComponentCallbacks implements ComponentCallbacks2 {
        @Override
        public void onTrimMemory(final int level) {
            boolean visibleRectEmpty = getGlobalVisibleRect().isEmpty();
            final boolean visible = mIsViewVisible && mIsWindowVisible && !visibleRectEmpty;
            ThreadUtils.runOnUiThreadBlocking(() -> {
                if (isDestroyedOrNoOperation(NO_WARN)) return;
                if (level >= TRIM_MEMORY_MODERATE) {
                    mInitialFunctor.deleteHardwareRenderer();
                    if (mFullScreenFunctor != null) {
                        mFullScreenFunctor.deleteHardwareRenderer();
                    }
                }
                nativeTrimMemory(mNativeAwContents, level, visible);
            });
        }

        @Override
        public void onLowMemory() {}

        @Override
        public void onConfigurationChanged(Configuration configuration) {
            updateDefaultLocale();
            mSettings.updateAcceptLanguages();
        }
    };

    //--------------------------------------------------------------------------------------------
    private class AwDisplayAndroidObserver implements DisplayAndroidObserver {
        @Override
        public void onRotationChanged(int rotation) {}

        @Override
        public void onDIPScaleChanged(float dipScale) {
            if (TRACE) Log.i(TAG, "%s onDIPScaleChanged dipScale=%f", this, dipScale);

            nativeSetDipScale(mNativeAwContents, dipScale);
            mLayoutSizer.setDIPScale(dipScale);
            mSettings.setDIPScale(dipScale);
        }
    };

    //--------------------------------------------------------------------------------------------
    /**
     * @param browserContext the browsing context to associate this view contents with.
     * @param containerView the view-hierarchy item this object will be bound to.
     * @param context the context to use, usually containerView.getContext().
     * @param internalAccessAdapter to access private methods on containerView.
     * @param nativeGLDelegate to access the GL functor provided by the WebView.
     * @param contentsClient will receive API callbacks from this WebView Contents.
     * @param awSettings AwSettings instance used to configure the AwContents.
     *
     * This constructor uses the default view sizing policy.
     */
    public AwContents(AwBrowserContext browserContext, ViewGroup containerView, Context context,
            InternalAccessDelegate internalAccessAdapter,
            NativeDrawGLFunctorFactory nativeDrawGLFunctorFactory, AwContentsClient contentsClient,
            AwSettings awSettings) {
        this(browserContext, containerView, context, internalAccessAdapter,
                nativeDrawGLFunctorFactory, contentsClient, awSettings, new DependencyFactory());
    }

    /**
     * @param dependencyFactory an instance of the DependencyFactory used to provide instances of
     *                          classes that this class depends on.
     *
     * This version of the constructor is used in test code to inject test versions of the above
     * documented classes.
     */
    public AwContents(AwBrowserContext browserContext, ViewGroup containerView, Context context,
            InternalAccessDelegate internalAccessAdapter,
            NativeDrawGLFunctorFactory nativeDrawGLFunctorFactory, AwContentsClient contentsClient,
            AwSettings settings, DependencyFactory dependencyFactory) {
        try (ScopedSysTraceEvent e1 = ScopedSysTraceEvent.scoped("AwContents.constructor")) {
            mRendererPriority = RendererPriority.HIGH;
            updateDefaultLocale();
            settings.updateAcceptLanguages();

            mBrowserContext = browserContext;

            // setWillNotDraw(false) is required since WebView draws it's own contents using it's
            // container view. If this is ever not the case we should remove this, as it removes
            // Android's gatherTransparentRegion optimization for the view.
            mContainerView = containerView;
            mContainerView.setWillNotDraw(false);

            mHandler = new Handler();
            mContext = context;
            mAutofillProvider = dependencyFactory.createAutofillProvider(context, mContainerView);
            mAppTargetSdkVersion = mContext.getApplicationInfo().targetSdkVersion;
            mInternalAccessAdapter = internalAccessAdapter;
            mNativeDrawGLFunctorFactory = nativeDrawGLFunctorFactory;
            mInitialFunctor = new AwGLFunctor(mNativeDrawGLFunctorFactory, mContainerView);
            mCurrentFunctor = mInitialFunctor;
            mContentsClient = contentsClient;
            mContentsClient.getCallbackHelper().setCancelCallbackPoller(
                    () -> AwContents.this.isDestroyedOrNoOperation(NO_WARN));
            mAwViewMethods = new AwViewMethodsImpl();
            mFullScreenTransitionsState = new FullScreenTransitionsState(
                    mContainerView, mInternalAccessAdapter, mAwViewMethods);
            mLayoutSizer = dependencyFactory.createLayoutSizer();
            mSettings = settings;
            mLayoutSizer.setDelegate(new AwLayoutSizerDelegate());
            mWebContentsDelegate = new AwWebContentsDelegateAdapter(
                    this, contentsClient, settings, mContext, mContainerView);
            mContentsClientBridge = new AwContentsClientBridge(
                    mContext, contentsClient, AwContentsStatics.getClientCertLookupTable());
            mZoomControls = new AwZoomControls(this);
            mBackgroundThreadClient = new BackgroundThreadClientImpl();
            mIoThreadClient = new IoThreadClientImpl();
            mInterceptNavigationDelegate = new InterceptNavigationDelegateImpl();
            mDisplayObserver = new AwDisplayAndroidObserver();
            mUpdateVisibilityRunnable = () -> updateWebContentsVisibility();

            AwSettings.ZoomSupportChangeListener zoomListener =
                    (supportsDoubleTapZoom, supportsMultiTouchZoom) -> {
                if (isDestroyedOrNoOperation(NO_WARN)) return;
                GestureListenerManager gestureManager =
                        GestureListenerManager.fromWebContents(mWebContents);
                gestureManager.updateDoubleTapSupport(supportsDoubleTapZoom);
                gestureManager.updateMultiTouchZoomSupport(supportsMultiTouchZoom);
            };
            mSettings.setZoomListener(zoomListener);
            mDefaultVideoPosterRequestHandler =
                    new DefaultVideoPosterRequestHandler(mContentsClient);
            mSettings.setDefaultVideoPosterURL(
                    mDefaultVideoPosterRequestHandler.getDefaultVideoPosterURL());
            mScrollOffsetManager = dependencyFactory.createScrollOffsetManager(
                    new AwScrollOffsetManagerDelegate());
            mScrollAccessibilityHelper = new ScrollAccessibilityHelper(mContainerView);

            setOverScrollMode(mContainerView.getOverScrollMode());
            setScrollBarStyle(mInternalAccessAdapter.super_getScrollBarStyle());

            setNewAwContents(nativeInit(mBrowserContext));

            onContainerViewChanged();
        }
    }

    private void initWebContents(ViewAndroidDelegate viewDelegate,
            InternalAccessDelegate internalDispatcher, WebContents webContents,
            WindowAndroid windowAndroid, WebContentsInternalsHolder internalsHolder) {
        webContents.initialize(
                PRODUCT_VERSION, viewDelegate, internalDispatcher, windowAndroid, internalsHolder);
        mViewEventSink = ViewEventSink.from(mWebContents);
        mViewEventSink.setHideKeyboardOnBlur(false);
        SelectionPopupController controller = SelectionPopupController.fromWebContents(webContents);
        controller.setActionModeCallback(new AwActionModeCallback(mContext, this, webContents));
        if (mAutofillProvider != null) {
            controller.setNonSelectionActionModeCallback(
                    new AutofillActionModeCallback(mContext, mAutofillProvider));
        }
        controller.setSelectionClient(SelectionClient.createSmartSelectionClient(webContents));

        // Listen for dpad events from IMEs (e.g. Samsung Cursor Control) so we know to enable
        // spatial navigation mode to allow these events to move focus out of the WebView.
        ImeAdapter.fromWebContents(webContents).addEventObserver(new ImeEventObserver() {
            @Override
            public void onBeforeSendKeyEvent(KeyEvent event) {
                if (AwContents.isDpadEvent(event)) {
                    mSettings.setSpatialNavigationEnabled(true);
                }
            }
        });
    }

    private boolean isSamsungMailApp() {
        // There are 2 different Samsung mail apps exhibiting bugs related to
        // http://crbug.com/781535.
        String currentPackageName = mContext.getPackageName();
        return "com.android.email".equals(currentPackageName)
                || "com.samsung.android.email.composer".equals(currentPackageName);
    }

    boolean isFullScreen() {
        return mFullScreenTransitionsState.isFullScreen();
    }

    /**
     * Transitions this {@link AwContents} to fullscreen mode and returns the
     * {@link View} where the contents will be drawn while in fullscreen, or null
     * if this AwContents has already been destroyed.
     */
    View enterFullScreen() {
        assert !isFullScreen();
        if (isDestroyedOrNoOperation(NO_WARN)) return null;

        // Detach to tear down the GL functor if this is still associated with the old
        // container view. It will be recreated during the next call to onDraw attached to
        // the new container view.
        onDetachedFromWindow();

        // In fullscreen mode FullScreenView owns the AwViewMethodsImpl and AwContents
        // a NullAwViewMethods.
        FullScreenView fullScreenView = new FullScreenView(mContext, mAwViewMethods, this);
        fullScreenView.setFocusable(true);
        fullScreenView.setFocusableInTouchMode(true);
        boolean wasInitialContainerViewFocused = mContainerView.isFocused();
        if (wasInitialContainerViewFocused) {
            fullScreenView.requestFocus();
        }
        mFullScreenFunctor = new AwGLFunctor(mNativeDrawGLFunctorFactory, fullScreenView);
        mFullScreenTransitionsState.enterFullScreen(fullScreenView, wasInitialContainerViewFocused);
        mAwViewMethods = new NullAwViewMethods(this, mInternalAccessAdapter, mContainerView);

        // Associate this AwContents with the FullScreenView.
        setInternalAccessAdapter(fullScreenView.getInternalAccessAdapter());
        setContainerView(fullScreenView, mFullScreenFunctor);

        return fullScreenView;
    }

    /**
     * Called when the app has requested to exit fullscreen.
     */
    void requestExitFullscreen() {
        if (!isDestroyedOrNoOperation(NO_WARN)) mWebContents.exitFullscreen();
    }

    /**
     * Returns this {@link AwContents} to embedded mode, where the {@link AwContents} are drawn
     * in the WebView.
     */
    void exitFullScreen() {
        if (!isFullScreen() || isDestroyedOrNoOperation(NO_WARN)) {
            // exitFullScreen() can be called without a prior call to enterFullScreen() if a
            // "misbehave" app overrides onShowCustomView but does not add the custom view to
            // the window. Exiting avoids a crash.
            return;
        }

        // Detach to tear down the GL functor if this is still associated with the old
        // container view. It will be recreated during the next call to onDraw attached to
        // the new container view.
        // NOTE: we cannot use mAwViewMethods here because its type is NullAwViewMethods.
        AwViewMethods awViewMethodsImpl = mFullScreenTransitionsState.getInitialAwViewMethods();
        awViewMethodsImpl.onDetachedFromWindow();

        // Swap the view delegates. In embedded mode the FullScreenView owns a
        // NullAwViewMethods and AwContents the AwViewMethodsImpl.
        FullScreenView fullscreenView = mFullScreenTransitionsState.getFullScreenView();
        fullscreenView.setAwViewMethods(new NullAwViewMethods(
                this, fullscreenView.getInternalAccessAdapter(), fullscreenView));
        mAwViewMethods = awViewMethodsImpl;
        ViewGroup initialContainerView = mFullScreenTransitionsState.getInitialContainerView();

        // Re-associate this AwContents with the WebView.
        setInternalAccessAdapter(mFullScreenTransitionsState.getInitialInternalAccessDelegate());
        setContainerView(initialContainerView, mInitialFunctor);

        // Return focus to the WebView.
        if (mFullScreenTransitionsState.wasInitialContainerViewFocused()) {
            mContainerView.requestFocus();
        }
        mFullScreenTransitionsState.exitFullScreen();
        // Drop AwContents last reference to this functor. AwGLFunctor is responsible for cleanup.
        mFullScreenFunctor = null;
    }

    private void setInternalAccessAdapter(InternalAccessDelegate internalAccessAdapter) {
        mInternalAccessAdapter = internalAccessAdapter;
        mViewEventSink.setAccessDelegate(mInternalAccessAdapter);
    }

    private void setContainerView(ViewGroup newContainerView, AwGLFunctor currentFunctor) {
        // setWillNotDraw(false) is required since WebView draws it's own contents using it's
        // container view. If this is ever not the case we should remove this, as it removes
        // Android's gatherTransparentRegion optimization for the view.
        mContainerView = newContainerView;
        mCurrentFunctor = currentFunctor;
        updateNativeAwGLFunctor();
        mContainerView.setWillNotDraw(false);

        mViewAndroidDelegate.setContainerView(mContainerView);
        if (mAwPdfExporter != null) {
            mAwPdfExporter.setContainerView(mContainerView);
        }
        mWebContentsDelegate.setContainerView(mContainerView);
        for (PopupTouchHandleDrawable drawable: mTouchHandleDrawables) {
            drawable.onContainerViewChanged(newContainerView);
        }
        onContainerViewChanged();
    }

    /**
     * Reconciles the state of this AwContents object with the state of the new container view.
     */
    @SuppressLint("NewApi") // ViewGroup#isAttachedToWindow requires API level 19.
    private void onContainerViewChanged() {
        // NOTE: mAwViewMethods is used by the old container view, the WebView, so it might refer
        // to a NullAwViewMethods when in fullscreen. To ensure that the state is reconciled with
        // the new container view correctly, we bypass mAwViewMethods and use the real
        // implementation directly.
        AwViewMethods awViewMethodsImpl = mFullScreenTransitionsState.getInitialAwViewMethods();
        awViewMethodsImpl.onVisibilityChanged(mContainerView, mContainerView.getVisibility());
        awViewMethodsImpl.onWindowVisibilityChanged(mContainerView.getWindowVisibility());

        boolean containerViewAttached = mContainerView.isAttachedToWindow();
        if (containerViewAttached && !mIsAttachedToWindow) {
            awViewMethodsImpl.onAttachedToWindow();
        } else if (!containerViewAttached && mIsAttachedToWindow) {
            awViewMethodsImpl.onDetachedFromWindow();
        }
        awViewMethodsImpl.onSizeChanged(
                mContainerView.getWidth(), mContainerView.getHeight(), 0, 0);
        awViewMethodsImpl.onWindowFocusChanged(mContainerView.hasWindowFocus());
        awViewMethodsImpl.onFocusChanged(mContainerView.hasFocus(), 0, null);
        mContainerView.requestLayout();
        if (mAutofillProvider != null) mAutofillProvider.onContainerViewChanged(mContainerView);
    }

    // This class destroys the WindowAndroid when after it is gc-ed.
    private static class WindowAndroidWrapper {
        private final WindowAndroid mWindowAndroid;
        private final CleanupReference mCleanupReference;

        private static final class DestroyRunnable implements Runnable {
            private final WindowAndroid mWindowAndroid;
            private DestroyRunnable(WindowAndroid windowAndroid) {
                mWindowAndroid = windowAndroid;
            }
            @Override
            public void run() {
                mWindowAndroid.destroy();
            }
        }

        public WindowAndroidWrapper(WindowAndroid windowAndroid) {
            try (ScopedSysTraceEvent e =
                            ScopedSysTraceEvent.scoped("WindowAndroidWrapper.constructor")) {
                mWindowAndroid = windowAndroid;
                mCleanupReference = new CleanupReference(this, new DestroyRunnable(windowAndroid));
            }
        }

        public WindowAndroid getWindowAndroid() {
            return mWindowAndroid;
        }
    }
    private static WeakHashMap<Context, WindowAndroidWrapper> sContextWindowMap;

    // getWindowAndroid is only called on UI thread, so there are no threading issues with lazy
    // initialization.
    private static WindowAndroidWrapper getWindowAndroid(Context context) {
        if (sContextWindowMap == null) sContextWindowMap = new WeakHashMap<>();
        WindowAndroidWrapper wrapper = sContextWindowMap.get(context);
        if (wrapper != null) return wrapper;

        try (ScopedSysTraceEvent e = ScopedSysTraceEvent.scoped("AwContents.getWindowAndroid")) {
            boolean contextWrapsActivity = activityFromContext(context) != null;
            if (contextWrapsActivity) {
                ActivityWindowAndroid activityWindow;
                try (ScopedSysTraceEvent e2 =
                                ScopedSysTraceEvent.scoped("AwContents.createActivityWindow")) {
                    final boolean listenToActivityState = false;
                    activityWindow = new ActivityWindowAndroid(context, listenToActivityState);
                }
                wrapper = new WindowAndroidWrapper(activityWindow);
            } else {
                wrapper = new WindowAndroidWrapper(new WindowAndroid(context));
            }
            sContextWindowMap.put(context, wrapper);
        }
        return wrapper;
    }

    // Set current locales to native.
    @VisibleForTesting
    public static void updateDefaultLocale() {
        String locales = LocaleUtils.getDefaultLocaleListString();
        if (!sCurrentLocales.equals(locales)) {
            sCurrentLocales = locales;

            // We cannot use the first language in sCurrentLocales for the UI language even on
            // Android N. LocaleUtils.getDefaultLocaleString() is capable for UI language but
            // it is not guaranteed to be listed at the first of sCurrentLocales. Therefore,
            // both values are passed to native.
            nativeUpdateDefaultLocale(LocaleUtils.getDefaultLocaleString(), sCurrentLocales);
        }
    }

    private void updateNativeAwGLFunctor() {
        nativeSetAwGLFunctor(mNativeAwContents,
                mCurrentFunctor != null ? mCurrentFunctor.getNativeAwGLFunctor() : 0);
    }

    /* Common initialization routine for adopting a native AwContents instance into this
     * java instance.
     *
     * TAKE CARE! This method can get called multiple times per java instance. Code accordingly.
     * ^^^^^^^^^  See the native class declaration for more details on relative object lifetimes.
     */
    private void setNewAwContents(long newAwContentsPtr) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            setNewAwContentsPreO(newAwContentsPtr);
        } else {
            // Move the TextClassifier to the new WebContents.
            TextClassifier textClassifier = mWebContents != null ? getTextClassifier() : null;
            setNewAwContentsPreO(newAwContentsPtr);
            if (textClassifier != null) setTextClassifier(textClassifier);
        }
    }

    // Helper for setNewAwContents containing everything which applies to pre-O.
    private void setNewAwContentsPreO(long newAwContentsPtr) {
        if (mNativeAwContents != 0) {
            destroyNatives();
            mWebContents = null;
            mWebContentsInternalsHolder = null;
            mWebContentsInternals = null;
            mNavigationController = null;
            mJavascriptInjector = null;
        }

        assert mNativeAwContents == 0 && mCleanupReference == null && mWebContents == null;

        mNativeAwContents = newAwContentsPtr;
        updateNativeAwGLFunctor();
        // TODO(joth): when the native and java counterparts of AwBrowserContext are hooked up to
        // each other, we should update |mBrowserContext| according to the newly received native
        // WebContent's browser context.

        mWebContents = nativeGetWebContents(mNativeAwContents);

        mWindowAndroid = getWindowAndroid(mContext);
        mViewAndroidDelegate =
                new AwViewAndroidDelegate(mContainerView, mContentsClient, mScrollOffsetManager);
        mWebContentsInternalsHolder = new WebContentsInternalsHolder(this);
        initWebContents(mViewAndroidDelegate, mInternalAccessAdapter, mWebContents,
                mWindowAndroid.getWindowAndroid(), mWebContentsInternalsHolder);
        nativeSetJavaPeers(mNativeAwContents, this, mWebContentsDelegate, mContentsClientBridge,
                mIoThreadClient, mInterceptNavigationDelegate, mAutofillProvider);
        GestureListenerManager.fromWebContents(mWebContents)
                .addListener(new AwGestureStateListener());

        mNavigationController = mWebContents.getNavigationController();
        installWebContentsObserver();
        mSettings.setWebContents(mWebContents);
        if (mAutofillProvider != null) mAutofillProvider.setWebContents(mWebContents);

        mDisplayObserver.onDIPScaleChanged(getDeviceScaleFactor());

        updateWebContentsVisibility();

        // The native side object has been bound to this java instance, so now is the time to
        // bind all the native->java relationships.
        mCleanupReference = new CleanupReference(
                this, new AwContentsDestroyRunnable(mNativeAwContents, mWindowAndroid));
    }

    private void installWebContentsObserver() {
        if (mWebContentsObserver != null) {
            mWebContentsObserver.destroy();
        }
        mWebContentsObserver = new AwWebContentsObserver(mWebContents, this, mContentsClient);
    }

    /**
     * Called on the "source" AwContents that is opening the popup window to
     * provide the AwContents to host the pop up content.
     */
    public void supplyContentsForPopup(AwContents newContents) {
        if (isDestroyedOrNoOperation(WARN)) return;
        long popupNativeAwContents = nativeReleasePopupAwContents(mNativeAwContents);
        if (popupNativeAwContents == 0) {
            Log.w(TAG, "Popup WebView bind failed: no pending content.");
            if (newContents != null) newContents.destroy();
            return;
        }
        if (newContents == null) {
            nativeDestroy(popupNativeAwContents);
            return;
        }

        newContents.receivePopupContents(popupNativeAwContents);
    }

    // Recap: supplyContentsForPopup() is called on the parent window's content, this method is
    // called on the popup window's content.
    private void receivePopupContents(long popupNativeAwContents) {
        if (isDestroyedOrNoOperation(WARN)) return;
        mDeferredShouldOverrideUrlLoadingIsPendingForPopup = true;
        // Save existing view state.
        final boolean wasAttached = mIsAttachedToWindow;
        final boolean wasViewVisible = mIsViewVisible;
        final boolean wasWindowVisible = mIsWindowVisible;
        final boolean wasPaused = mIsPaused;
        final boolean wasFocused = mContainerViewFocused;
        final boolean wasWindowFocused = mWindowFocused;

        // Properly clean up existing mNativeAwContents.
        if (wasFocused) onFocusChanged(false, 0, null);
        if (wasWindowFocused) onWindowFocusChanged(false);
        if (wasViewVisible) setViewVisibilityInternal(false);
        if (wasWindowVisible) setWindowVisibilityInternal(false);
        if (wasAttached) onDetachedFromWindow();
        if (!wasPaused) onPause();

        // Save injected JavaScript interfaces.
        Map<String, Pair<Object, Class>> javascriptInterfaces =
                new HashMap<String, Pair<Object, Class>>();
        if (mWebContents != null) {
            javascriptInterfaces.putAll(getJavascriptInjector().getInterfaces());
        }

        setNewAwContents(popupNativeAwContents);
        // We defer loading any URL on the popup until it has been properly intialized (through
        // setNewAwContents). We resume the load here.
        nativeResumeLoadingCreatedPopupWebContents(mNativeAwContents);

        // Finally refresh all view state for mNativeAwContents.
        if (!wasPaused) onResume();
        if (wasAttached) {
            onAttachedToWindow();
            postInvalidateOnAnimation();
        }
        onSizeChanged(mContainerView.getWidth(), mContainerView.getHeight(), 0, 0);
        if (wasWindowVisible) setWindowVisibilityInternal(true);
        if (wasViewVisible) setViewVisibilityInternal(true);
        if (wasWindowFocused) onWindowFocusChanged(wasWindowFocused);
        if (wasFocused) onFocusChanged(true, 0, null);

        mIsPopupWindow = true;

        // Restore injected JavaScript interfaces.
        for (Map.Entry<String, Pair<Object, Class>> entry : javascriptInterfaces.entrySet()) {
            @SuppressWarnings("unchecked")
            Class<? extends Annotation> requiredAnnotation = entry.getValue().second;
            getJavascriptInjector().addPossiblyUnsafeInterface(
                    entry.getValue().first, entry.getKey(), requiredAnnotation);
        }
    }

    private JavascriptInjector getJavascriptInjector() {
        if (mJavascriptInjector == null) {
            mJavascriptInjector = JavascriptInjector.fromWebContents(mWebContents);
        }
        return mJavascriptInjector;
    }

    @VisibleForTesting
    @CalledByNative
    protected void onRenderProcessGone(int childProcessID) {
        // This is the first callback we got for render process gone, we can't destroy the WebView
        // now because we need to get next callback onRenderProcessGoneWithDetail() to know whether
        // render process crashed or was killed.
        // However, we should make WebView no operation to avoid being in weird states.
        // TODO: We might be able to combine destroyed and no operation states.
        mIsNoOperation = true;
    }

    @VisibleForTesting
    @CalledByNative
    protected boolean onRenderProcessGoneDetail(int childProcessID, boolean crashed) {
        if (isDestroyed(NO_WARN)) return true;
        return mContentsClient.onRenderProcessGone(new AwRenderProcessGoneDetail(
                crashed, nativeGetEffectivePriority(mNativeAwContents)));
    }

    @VisibleForTesting
    public @RendererPriority int getEffectivePriorityForTesting() {
        assert !isDestroyed(NO_WARN);
        return nativeGetEffectivePriority(mNativeAwContents);
    }

    private boolean isNoOperation() {
        return mIsNoOperation;
    }

    private boolean isDestroyedOrNoOperation(int warnIfDestroyed) {
        return isDestroyed(warnIfDestroyed) || isNoOperation();
    }

    /**
     * Destroys this object and deletes its native counterpart.
     */
    public void destroy() {
        if (TRACE) Log.i(TAG, "%s destroy", this);
        if (isDestroyed(NO_WARN)) return;

        // Remove pending messages
        mContentsClient.getCallbackHelper().removeCallbacksAndMessages();

        if (mIsAttachedToWindow) {
            Log.w(TAG, "WebView.destroy() called while WebView is still attached to window.");
            // Need to call detach to avoid leaks because the real detach later will be ignored.
            onDetachedFromWindow();
        }
        mIsNoOperation = true;
        mIsDestroyed = true;
        mHandler.post(() -> destroyNatives());
    }

    /**
     * Deletes the native counterpart of this object.
     */
    private void destroyNatives() {
        if (mCleanupReference != null) {
            assert mNativeAwContents != 0;

            mWebContentsObserver.destroy();
            mWebContentsObserver = null;
            mNativeAwContents = 0;
            mWebContents = null;
            mWebContentsInternals = null;
            mNavigationController = null;

            mCleanupReference.cleanupNow();
            mCleanupReference = null;
        }

        assert mWebContents == null;
        assert mNavigationController == null;
        assert mNativeAwContents == 0;

        onDestroyed();
    }

    @VisibleForTesting
    protected void onDestroyed() {}

    /**
     * Returns whether this instance of WebView is flagged as destroyed.
     * If {@link WARN} is passed as a parameter, the method also issues a warning
     * log message and dumps stack, as embedders are advised not to call any
     * methods on destroyed WebViews.
     *
     * @param warnIfDestroyed use {@link WARN} if the check is done from a method
     * that is called via public WebView API, and {@link NO_WARN} otherwise.
     * @return whether this instance of WebView is flagged as destroyed.
     */
    private boolean isDestroyed(int warnIfDestroyed) {
        if (mIsDestroyed && warnIfDestroyed == WARN) {
            Log.w(TAG, "Application attempted to call on a destroyed WebView", new Throwable());
        }
        boolean destroyRunnableHasRun =
                mCleanupReference != null && mCleanupReference.hasCleanedUp();
        boolean weakRefsCleared =
                mWebContentsInternalsHolder != null && mWebContentsInternalsHolder.weakRefCleared();
        if (TRACE && destroyRunnableHasRun && !mIsDestroyed) {
            // Swallow the error. App developers are not going to do anything with an error msg.
            Log.d(TAG, "AwContents is kept alive past CleanupReference by finalizer");
        }
        return mIsDestroyed || destroyRunnableHasRun || weakRefsCleared;
    }

    @VisibleForTesting
    public WebContents getWebContents() {
        return mWebContents;
    }

    @VisibleForTesting
    public NavigationController getNavigationController() {
        return mNavigationController;
    }

    // Can be called from any thread.
    public AwSettings getSettings() {
        return mSettings;
    }

    ViewGroup getContainerView() {
        return mContainerView;
    }

    public AwPdfExporter getPdfExporter() {
        if (isDestroyedOrNoOperation(WARN)) return null;
        if (mAwPdfExporter == null) {
            mAwPdfExporter = new AwPdfExporter(mContainerView);
            nativeCreatePdfExporter(mNativeAwContents, mAwPdfExporter);
        }
        return mAwPdfExporter;
    }

    public static void setAwDrawSWFunctionTable(long functionTablePointer) {
        nativeSetAwDrawSWFunctionTable(functionTablePointer);
    }

    public static void setAwDrawGLFunctionTable(long functionTablePointer) {
        nativeSetAwDrawGLFunctionTable(functionTablePointer);
    }

    public static long getAwDrawGLFunction() {
        return AwGLFunctor.getAwDrawGLFunction();
    }

    public static void setShouldDownloadFavicons() {
        nativeSetShouldDownloadFavicons();
    }

    public static Activity activityFromContext(Context context) {
        try (ScopedSysTraceEvent e = ScopedSysTraceEvent.scoped("AwContents.activityFromContext")) {
            return WindowAndroid.activityFromContext(context);
        }
    }
    /**
     * Disables contents of JS-to-Java bridge objects to be inspectable using
     * Object.keys() method and "for .. in" loops. This is intended for applications
     * targeting earlier Android releases where this was not possible, and we want
     * to ensure backwards compatible behavior.
     */
    public void disableJavascriptInterfacesInspection() {
        if (!isDestroyedOrNoOperation(WARN)) {
            getJavascriptInjector().setAllowInspection(false);
        }
    }

    /**
     * Intended for test code.
     * @return the number of native instances of this class.
     */
    @VisibleForTesting
    public static int getNativeInstanceCount() {
        return nativeGetNativeInstanceCount();
    }

    // This is only to avoid heap allocations inside getGlobalVisibleRect. It should treated
    // as a local variable in the function and not used anywhere else.
    private static final Rect sLocalGlobalVisibleRect = new Rect();

    private Rect getGlobalVisibleRect() {
        if (!mContainerView.getGlobalVisibleRect(sLocalGlobalVisibleRect)) {
            sLocalGlobalVisibleRect.setEmpty();
        }
        return sLocalGlobalVisibleRect;
    }

    //--------------------------------------------------------------------------------------------
    //  WebView[Provider] method implementations
    //--------------------------------------------------------------------------------------------

    public void onDraw(Canvas canvas) {
        try {
            TraceEvent.begin("AwContents.onDraw");
            mAwViewMethods.onDraw(canvas);
        } finally {
            TraceEvent.end("AwContents.onDraw");
        }
    }

    public void setLayoutParams(final ViewGroup.LayoutParams layoutParams) {
        mLayoutSizer.onLayoutParamsChange();
    }

    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        mAwViewMethods.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    public int getContentHeightCss() {
        if (isDestroyedOrNoOperation(WARN)) return 0;
        return (int) Math.ceil(mContentHeightDip);
    }

    public int getContentWidthCss() {
        if (isDestroyedOrNoOperation(WARN)) return 0;
        return (int) Math.ceil(mContentWidthDip);
    }

    public Picture capturePicture() {
        if (TRACE) Log.i(TAG, "%s capturePicture", this);
        if (isDestroyedOrNoOperation(WARN)) return null;
        return new AwPicture(nativeCapturePicture(mNativeAwContents,
                mScrollOffsetManager.computeHorizontalScrollRange(),
                mScrollOffsetManager.computeVerticalScrollRange()));
    }

    public void clearView() {
        if (TRACE) Log.i(TAG, "%s clearView", this);
        if (!isDestroyedOrNoOperation(WARN)) nativeClearView(mNativeAwContents);
    }

    /**
     * Enable the onNewPicture callback.
     * @param enabled Flag to enable the callback.
     * @param invalidationOnly Flag to call back only on invalidation without providing a picture.
     */
    public void enableOnNewPicture(boolean enabled, boolean invalidationOnly) {
        if (TRACE) Log.i(TAG, "%s enableOnNewPicture=%s", this, enabled);
        if (isDestroyedOrNoOperation(WARN)) return;
        if (invalidationOnly) {
            mPictureListenerContentProvider = null;
        } else if (enabled && mPictureListenerContentProvider == null) {
            mPictureListenerContentProvider = () -> capturePicture();
        }
        nativeEnableOnNewPicture(mNativeAwContents, enabled);
    }

    public void findAllAsync(String searchString) {
        if (TRACE) Log.i(TAG, "%s findAllAsync", this);
        if (!isDestroyedOrNoOperation(WARN)) nativeFindAllAsync(mNativeAwContents, searchString);
    }

    public void findNext(boolean forward) {
        if (TRACE) Log.i(TAG, "%s findNext", this);
        if (!isDestroyedOrNoOperation(WARN)) nativeFindNext(mNativeAwContents, forward);
    }

    public void clearMatches() {
        if (TRACE) Log.i(TAG, "%s clearMatches", this);
        if (!isDestroyedOrNoOperation(WARN)) nativeClearMatches(mNativeAwContents);
    }

    /**
     * @return load progress of the WebContents.
     */
    public int getMostRecentProgress() {
        if (isDestroyedOrNoOperation(WARN)) return 0;
        // WebContentsDelegateAndroid conveniently caches the most recent notified value for us.
        return mWebContentsDelegate.getMostRecentProgress();
    }

    public Bitmap getFavicon() {
        if (isDestroyedOrNoOperation(WARN)) return null;
        return mFavicon;
    }

    private void requestVisitedHistoryFromClient() {
        Callback<String[]> callback = value -> {
            if (value != null) {
                // Replace null values with empty strings, because they can't be represented as
                // native strings.
                for (int i = 0; i < value.length; i++) {
                    if (value[i] == null) value[i] = "";
                }
            }

            ThreadUtils.runOnUiThread(() -> {
                if (!isDestroyedOrNoOperation(NO_WARN)) {
                    nativeAddVisitedLinks(mNativeAwContents, value);
                }
            });
        };
        mContentsClient.getVisitedHistory(callback);
    }

    /**
     * WebView.loadUrl.
     */
    public void loadUrl(String url, Map<String, String> additionalHttpHeaders) {
        if (TRACE) Log.i(TAG, "%s loadUrl(extra headers)=%s", this, url);
        if (isDestroyedOrNoOperation(WARN)) return;
        // Early out to match old WebView implementation
        if (url == null) {
            return;
        }
        // TODO: We may actually want to do some sanity checks here (like filter about://chrome).

        // For backwards compatibility, apps targeting less than K will have JS URLs evaluated
        // directly and any result of the evaluation will not replace the current page content.
        // Matching Chrome behavior more closely; apps targetting >= K that load a JS URL will
        // have the result of that URL replace the content of the current page.
        final String javaScriptScheme = "javascript:";
        if (mAppTargetSdkVersion < Build.VERSION_CODES.KITKAT && url.startsWith(javaScriptScheme)) {
            evaluateJavaScript(url.substring(javaScriptScheme.length()), null);
            return;
        }

        LoadUrlParams params = new LoadUrlParams(url, PageTransition.TYPED);
        if (additionalHttpHeaders != null) {
            params.setExtraHeaders(new HashMap<String, String>(additionalHttpHeaders));
        }

        final String dataScheme = "data:";
        if (url.startsWith(dataScheme) && url.contains("#")) {
            RecordHistogram.recordBooleanHistogram(DATA_URI_HISTOGRAM_NAME, true);
        }

        loadUrl(params);
    }

    /**
     * WebView.loadUrl.
     */
    public void loadUrl(String url) {
        if (TRACE) Log.i(TAG, "%s loadUrl=%s", this, url);
        if (isDestroyedOrNoOperation(WARN)) return;
        // Early out to match old WebView implementation
        if (url == null) {
            return;
        }
        loadUrl(url, null);
    }

    /**
     * WebView.postUrl.
     */
    public void postUrl(String url, byte[] postData) {
        if (TRACE) Log.i(TAG, "%s postUrl=%s", this, url);
        if (isDestroyedOrNoOperation(WARN)) return;
        LoadUrlParams params = LoadUrlParams.createLoadHttpPostParams(url, postData);
        Map<String, String> headers = new HashMap<String, String>();
        headers.put("Content-Type", "application/x-www-form-urlencoded");
        params.setExtraHeaders(headers);
        loadUrl(params);
    }

    private static String fixupMimeType(String mimeType) {
        return TextUtils.isEmpty(mimeType) ? "text/html" : mimeType;
    }

    private static String fixupData(String data) {
        return TextUtils.isEmpty(data) ? "" : data;
    }

    private static String fixupBase(String url) {
        return TextUtils.isEmpty(url) ? ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL : url;
    }

    private static String fixupHistory(String url) {
        return TextUtils.isEmpty(url) ? ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL : url;
    }

    private static boolean isBase64Encoded(String encoding) {
        return "base64".equals(encoding);
    }

    private static void recordHistoryUrl(@HistoryUrl int value) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.LoadDataWithBaseUrl.HistoryUrl", value, HistoryUrl.COUNT);
    }

    /**
     * WebView.loadData.
     */
    public void loadData(String data, String mimeType, String encoding) {
        if (TRACE) Log.i(TAG, "%s loadData", this);
        if (isDestroyedOrNoOperation(WARN)) return;
        if (data != null && data.contains("#")) {
            RecordHistogram.recordBooleanHistogram(DATA_URI_HISTOGRAM_NAME, true);
        }
        loadUrl(LoadUrlParams.createLoadDataParams(
                fixupData(data), fixupMimeType(mimeType), isBase64Encoded(encoding)));
    }

    /**
     * WebView.loadDataWithBaseURL.
     */
    public void loadDataWithBaseURL(
            String baseUrl, String data, String mimeType, String encoding, String historyUrl) {
        if (TRACE) Log.i(TAG, "%s loadDataWithBaseURL=%s", this, baseUrl);
        if (isDestroyedOrNoOperation(WARN)) return;
        if (data != null && data.contains("#")) {
            RecordHistogram.recordBooleanHistogram(DATA_URI_HISTOGRAM_NAME, true);
        }

        data = fixupData(data);
        mimeType = fixupMimeType(mimeType);
        LoadUrlParams loadUrlParams;
        baseUrl = fixupBase(baseUrl);
        historyUrl = fixupHistory(historyUrl);

        if (historyUrl.equals(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL)) {
            recordHistoryUrl(HistoryUrl.EMPTY);
        } else if (historyUrl.equals(baseUrl)) {
            recordHistoryUrl(HistoryUrl.BASEURL);
        } else {
            recordHistoryUrl(HistoryUrl.DIFFERENT);
        }

        if (baseUrl.startsWith("data:")) {
            // For backwards compatibility with WebViewClassic, we use the value of |encoding|
            // as the charset, as long as it's not "base64".
            boolean isBase64 = isBase64Encoded(encoding);
            loadUrlParams = LoadUrlParams.createLoadDataParamsWithBaseUrl(
                    data, mimeType, isBase64, baseUrl, historyUrl, isBase64 ? null : encoding);
        } else {
            // When loading data with a non-data: base URL, the classic WebView would effectively
            // "dump" that string of data into the WebView without going through regular URL
            // loading steps such as decoding URL-encoded entities. We achieve this same behavior by
            // base64 encoding the data that is passed here and then loading that as a data: URL.
            try {
                loadUrlParams = LoadUrlParams.createLoadDataParamsWithBaseUrl(
                        Base64.encodeToString(data.getBytes("utf-8"), Base64.DEFAULT), mimeType,
                        true, baseUrl, historyUrl, "utf-8");
            } catch (java.io.UnsupportedEncodingException e) {
                Log.wtf(TAG, "Unable to load data string %s", data, e);
                return;
            }
        }

        // This is a workaround for an issue with PlzNavigate and one of Samsung's OEM mail apps.
        // See http://crbug.com/781535.
        if (isSamsungMailApp() && SAMSUNG_WORKAROUND_BASE_URL.equals(loadUrlParams.getBaseUrl())) {
            ThreadUtils.postOnUiThreadDelayed(
                    () -> loadUrl(loadUrlParams), SAMSUNG_WORKAROUND_DELAY);
            return;
        }
        loadUrl(loadUrlParams);
    }

    /**
     * Load url without fixing up the url string. Consumers of ContentView are responsible for
     * ensuring the URL passed in is properly formatted (i.e. the scheme has been added if left
     * off during user input).
     *
     * @param params Parameters for this load.
     */
    @VisibleForTesting
    public void loadUrl(LoadUrlParams params) {
        if (params.getLoadUrlType() == LoadURLType.DATA && !params.isBaseUrlDataScheme()) {
            // This allows data URLs with a non-data base URL access to file:///android_asset/ and
            // file:///android_res/ URLs. If AwSettings.getAllowFileAccess permits, it will also
            // allow access to file:// URLs (subject to OS level permission checks).
            params.setCanLoadLocalResources(true);
            nativeGrantFileSchemeAccesstoChildProcess(mNativeAwContents);
        }

        // If we are reloading the same url, then set transition type as reload.
        if (params.getUrl() != null && params.getUrl().equals(mWebContents.getLastCommittedUrl())
                && params.getTransitionType() == PageTransition.TYPED) {
            params.setTransitionType(PageTransition.RELOAD);
        }
        params.setTransitionType(
                params.getTransitionType() | PageTransition.FROM_API);

        // For WebView, always use the user agent override, which is set
        // every time the user agent in AwSettings is modified.
        params.setOverrideUserAgent(UserAgentOverrideOption.TRUE);


        // We don't pass extra headers to the content layer, as WebViewClassic
        // was adding them in a very narrow set of conditions. See http://crbug.com/306873
        // However, if the embedder is attempting to inject a Referer header for their
        // loadUrl call, then we set that separately and remove it from the extra headers map/
        final String referer = "referer";
        Map<String, String> extraHeaders = params.getExtraHeaders();
        if (extraHeaders != null) {
            for (String header : extraHeaders.keySet()) {
                if (referer.equals(header.toLowerCase(Locale.US))) {
                    params.setReferrer(
                            new Referrer(extraHeaders.remove(header), ReferrerPolicy.DEFAULT));
                    params.setExtraHeaders(extraHeaders);
                    break;
                }
            }
        }

        nativeSetExtraHeadersForUrl(
                mNativeAwContents, params.getUrl(), params.getExtraHttpRequestHeadersString());
        params.setExtraHeaders(new HashMap<String, String>());

        mNavigationController.loadUrl(params);

        // The behavior of WebViewClassic uses the populateVisitedLinks callback in WebKit.
        // Chromium does not use this use code path and the best emulation of this behavior to call
        // request visited links once on the first URL load of the WebView.
        if (!mHasRequestedVisitedHistoryFromClient) {
            mHasRequestedVisitedHistoryFromClient = true;
            requestVisitedHistoryFromClient();
        }
    }

    /**
     * Get the URL of the current page. This is the visible URL of the {@link WebContents} which may
     * be a pending navigation or the last committed URL. For the last committed URL use
     * #getLastCommittedUrl().
     *
     * @return The URL of the current page or null if it's empty.
     */
    public String getUrl() {
        if (isDestroyedOrNoOperation(WARN)) return null;
        String url = mWebContents.getVisibleUrl();
        if (url == null || url.trim().isEmpty()) return null;
        return url;
    }

    /**
     * Gets the last committed URL. It represents the current page that is
     * displayed in WebContents. It represents the current security context.
     *
     * @return The URL of the current page or null if it's empty.
     */
    public String getLastCommittedUrl() {
        if (isDestroyedOrNoOperation(NO_WARN)) return null;
        String url = mWebContents.getLastCommittedUrl();
        if (url == null || url.trim().isEmpty()) return null;
        return url;
    }

    public void requestFocus() {
        mAwViewMethods.requestFocus();
    }

    public void setBackgroundColor(int color) {
        mBaseBackgroundColor = color;
        if (!isDestroyedOrNoOperation(WARN)) nativeSetBackgroundColor(mNativeAwContents, color);
    }

    /**
     * @see android.view.View#setLayerType()
     */
    public void setLayerType(int layerType, Paint paint) {
        mAwViewMethods.setLayerType(layerType, paint);
    }

    int getEffectiveBackgroundColor() {
        // Do not ask the WebContents for the background color, as it will always
        // report white prior to initial navigation or post destruction,  whereas we want
        // to use the client supplied base value in those cases.
        if (isDestroyedOrNoOperation(NO_WARN)
                || !mContentsClient.isCachedRendererBackgroundColorValid()) {
            return mBaseBackgroundColor;
        }
        return mContentsClient.getCachedRendererBackgroundColor();
    }

    public boolean isMultiTouchZoomSupported() {
        return mSettings.supportsMultiTouchZoom();
    }

    public View getZoomControlsForTest() {
        return mZoomControls.getZoomControlsViewForTest();
    }

    /**
     * @see View#setOverScrollMode(int)
     */
    public void setOverScrollMode(int mode) {
        if (mode != View.OVER_SCROLL_NEVER) {
            mOverScrollGlow = new OverScrollGlow(mContext, mContainerView);
        } else {
            mOverScrollGlow = null;
        }
    }

    // TODO(mkosiba): In WebViewClassic these appear in some of the scroll extent calculation
    // methods but toggling them has no visiual effect on the content (in other words the scrolling
    // code behaves as if the scrollbar-related padding is in place but the onDraw code doesn't
    // take that into consideration).
    // http://crbug.com/269032
    private boolean mOverlayHorizontalScrollbar = true;
    private boolean mOverlayVerticalScrollbar;

    /**
     * @see View#setScrollBarStyle(int)
     */
    public void setScrollBarStyle(int style) {
        if (style == View.SCROLLBARS_INSIDE_OVERLAY
                || style == View.SCROLLBARS_OUTSIDE_OVERLAY) {
            mOverlayHorizontalScrollbar = mOverlayVerticalScrollbar = true;
        } else {
            mOverlayHorizontalScrollbar = mOverlayVerticalScrollbar = false;
        }
    }

    /**
     * @see View#setHorizontalScrollbarOverlay(boolean)
     */
    public void setHorizontalScrollbarOverlay(boolean overlay) {
        if (TRACE) Log.i(TAG, "%s setHorizontalScrollbarOverlay=%s", this, overlay);
        mOverlayHorizontalScrollbar = overlay;
    }

    /**
     * @see View#setVerticalScrollbarOverlay(boolean)
     */
    public void setVerticalScrollbarOverlay(boolean overlay) {
        if (TRACE) Log.i(TAG, "%s setVerticalScrollbarOverlay=%s", this, overlay);
        mOverlayVerticalScrollbar = overlay;
    }

    /**
     * @see View#overlayHorizontalScrollbar()
     */
    public boolean overlayHorizontalScrollbar() {
        return mOverlayHorizontalScrollbar;
    }

    /**
     * @see View#overlayVerticalScrollbar()
     */
    public boolean overlayVerticalScrollbar() {
        return mOverlayVerticalScrollbar;
    }

    /**
     * Called by the embedder when the scroll offset of the containing view has changed.
     * @see View#onScrollChanged(int,int)
     */
    public void onContainerViewScrollChanged(int l, int t, int oldl, int oldt) {
        mAwViewMethods.onContainerViewScrollChanged(l, t, oldl, oldt);
    }

    /**
     * Called by the embedder when the containing view is to be scrolled or overscrolled.
     * @see View#onOverScrolled(int,int,int,int)
     */
    public void onContainerViewOverScrolled(int scrollX, int scrollY, boolean clampedX,
            boolean clampedY) {
        mAwViewMethods.onContainerViewOverScrolled(scrollX, scrollY, clampedX, clampedY);
    }

    /**
     * @see android.webkit.WebView#requestChildRectangleOnScreen(View, Rect, boolean)
     */
    public boolean requestChildRectangleOnScreen(View child, Rect rect, boolean immediate) {
        if (isDestroyedOrNoOperation(WARN)) return false;
        return mScrollOffsetManager.requestChildRectangleOnScreen(
                child.getLeft() - child.getScrollX(), child.getTop() - child.getScrollY(),
                rect, immediate);
    }

    /**
     * @see View#computeHorizontalScrollRange()
     */
    public int computeHorizontalScrollRange() {
        return mAwViewMethods.computeHorizontalScrollRange();
    }

    /**
     * @see View#computeHorizontalScrollOffset()
     */
    public int computeHorizontalScrollOffset() {
        return mAwViewMethods.computeHorizontalScrollOffset();
    }

    /**
     * @see View#computeVerticalScrollRange()
     */
    public int computeVerticalScrollRange() {
        return mAwViewMethods.computeVerticalScrollRange();
    }

    /**
     * @see View#computeVerticalScrollOffset()
     */
    public int computeVerticalScrollOffset() {
        return mAwViewMethods.computeVerticalScrollOffset();
    }

    /**
     * @see View#computeVerticalScrollExtent()
     */
    public int computeVerticalScrollExtent() {
        return mAwViewMethods.computeVerticalScrollExtent();
    }

    /**
     * @see View.computeScroll()
     */
    public void computeScroll() {
        mAwViewMethods.computeScroll();
    }

    /**
     * @see View#onCheckIsTextEditor()
     */
    public boolean onCheckIsTextEditor() {
        return mAwViewMethods.onCheckIsTextEditor();
    }

    /**
     * @see android.webkit.WebView#stopLoading()
     */
    public void stopLoading() {
        if (TRACE) Log.i(TAG, "%s stopLoading", this);
        if (!isDestroyedOrNoOperation(WARN)) mWebContents.stop();
    }

    /**
     * @see android.webkit.WebView#reload()
     */
    public void reload() {
        if (TRACE) Log.i(TAG, "%s reload", this);
        if (!isDestroyedOrNoOperation(WARN)) mNavigationController.reload(true);
    }

    /**
     * @see android.webkit.WebView#canGoBack()
     */
    public boolean canGoBack() {
        return isDestroyedOrNoOperation(WARN) ? false : mNavigationController.canGoBack();
    }

    /**
     * @see android.webkit.WebView#goBack()
     */
    public void goBack() {
        if (TRACE) Log.i(TAG, "%s goBack", this);
        if (!isDestroyedOrNoOperation(WARN)) mNavigationController.goBack();
    }

    /**
     * @see android.webkit.WebView#canGoForward()
     */
    public boolean canGoForward() {
        return isDestroyedOrNoOperation(WARN) ? false : mNavigationController.canGoForward();
    }

    /**
     * @see android.webkit.WebView#goForward()
     */
    public void goForward() {
        if (TRACE) Log.i(TAG, "%s goForward", this);
        if (!isDestroyedOrNoOperation(WARN)) mNavigationController.goForward();
    }

    /**
     * @see android.webkit.WebView#canGoBackOrForward(int)
     */
    public boolean canGoBackOrForward(int steps) {
        return isDestroyedOrNoOperation(WARN) ? false : mNavigationController.canGoToOffset(steps);
    }

    /**
     * @see android.webkit.WebView#goBackOrForward(int)
     */
    public void goBackOrForward(int steps) {
        if (TRACE) Log.i(TAG, "%s goBackOrForwad=%d", this, steps);
        if (!isDestroyedOrNoOperation(WARN)) mNavigationController.goToOffset(steps);
    }

    /**
     * @see android.webkit.WebView#pauseTimers()
     */
    public void pauseTimers() {
        if (TRACE) Log.i(TAG, "%s pauseTimers", this);
        if (!isDestroyedOrNoOperation(WARN)) {
            ContentViewStatics.setWebKitSharedTimersSuspended(true);
        }
    }

    /**
     * @see android.webkit.WebView#resumeTimers()
     */
    public void resumeTimers() {
        if (TRACE) Log.i(TAG, "%s resumeTimers", this);
        if (!isDestroyedOrNoOperation(WARN)) {
            ContentViewStatics.setWebKitSharedTimersSuspended(false);
        }
    }

    /**
     * @see android.webkit.WebView#onPause()
     */
    public void onPause() {
        if (TRACE) Log.i(TAG, "%s onPause", this);
        if (mIsPaused || isDestroyedOrNoOperation(NO_WARN)) return;
        mIsPaused = true;
        nativeSetIsPaused(mNativeAwContents, mIsPaused);

        // Geolocation is paused/resumed via the page visibility mechanism.
        updateWebContentsVisibility();
    }

    /**
     * @see android.webkit.WebView#onResume()
     */
    public void onResume() {
        if (TRACE) Log.i(TAG, "%s onResume", this);
        if (!mIsPaused || isDestroyedOrNoOperation(NO_WARN)) return;
        mIsPaused = false;
        nativeSetIsPaused(mNativeAwContents, mIsPaused);
        updateWebContentsVisibility();
    }

    /**
     * @see android.webkit.WebView#isPaused()
     */
    public boolean isPaused() {
        return isDestroyedOrNoOperation(WARN) ? false : mIsPaused;
    }

    /**
     * @see android.webkit.WebView#onCreateInputConnection(EditorInfo)
     */
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        return mAwViewMethods.onCreateInputConnection(outAttrs);
    }

    /**
     * @see android.webkit.WebView#onDragEvent(DragEvent)
     */
    public boolean onDragEvent(DragEvent event) {
        return mAwViewMethods.onDragEvent(event);
    }

    /**
     * @see android.webkit.WebView#onKeyUp(int, KeyEvent)
     */
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        return mAwViewMethods.onKeyUp(keyCode, event);
    }

    /**
     * @see android.webkit.WebView#dispatchKeyEvent(KeyEvent)
     */
    public boolean dispatchKeyEvent(KeyEvent event) {
        return mAwViewMethods.dispatchKeyEvent(event);
    }

    /**
     * Clears the resource cache. Note that the cache is per-application, so this will clear the
     * cache for all WebViews used.
     *
     * @param includeDiskFiles if false, only the RAM cache is cleared
     */
    public void clearCache(boolean includeDiskFiles) {
        if (TRACE) Log.i(TAG, "%s clearCache", this);
        if (!isDestroyedOrNoOperation(WARN)) nativeClearCache(mNativeAwContents, includeDiskFiles);
    }

    @VisibleForTesting
    public void killRenderProcess() {
        if (TRACE) Log.i(TAG, "%s killRenderProcess", this);
        if (isDestroyedOrNoOperation(WARN)) {
            throw new IllegalStateException("killRenderProcess() shouldn't be invoked after render"
                    + " process is gone or webview is destroyed");
        }
        nativeKillRenderProcess(mNativeAwContents);
    }

    public void documentHasImages(Message message) {
        if (!isDestroyedOrNoOperation(WARN)) nativeDocumentHasImages(mNativeAwContents, message);
    }

    public void saveWebArchive(
            final String basename, boolean autoname, final Callback<String> callback) {
        if (TRACE) Log.i(TAG, "%s saveWebArchive=%s", this, basename);
        if (!autoname) {
            saveWebArchiveInternal(basename, callback);
            return;
        }
        // If auto-generating the file name, handle the name generation on a background thread
        // as it will require I/O access for checking whether previous files existed.
        new AsyncTask<String>() {
            @Override
            protected String doInBackground() {
                return generateArchiveAutoNamePath(getOriginalUrl(), basename);
            }

            @Override
            protected void onPostExecute(String result) {
                saveWebArchiveInternal(result, callback);
            }
        }
                .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    public String getOriginalUrl() {
        if (isDestroyedOrNoOperation(WARN)) return null;
        NavigationHistory history = mNavigationController.getNavigationHistory();
        int currentIndex = history.getCurrentEntryIndex();
        if (currentIndex >= 0 && currentIndex < history.getEntryCount()) {
            return history.getEntryAtIndex(currentIndex).getOriginalUrl();
        }
        return null;
    }

    /**
     * @see NavigationController#getNavigationHistory()
     */
    public NavigationHistory getNavigationHistory() {
        return isDestroyedOrNoOperation(WARN) ? null : mNavigationController.getNavigationHistory();
    }

    /**
     * @see android.webkit.WebView#getTitle()
     */
    public String getTitle() {
        return isDestroyedOrNoOperation(WARN) ? null : mWebContents.getTitle();
    }

    /**
     * @see android.webkit.WebView#clearHistory()
     */
    public void clearHistory() {
        if (TRACE) Log.i(TAG, "%s clearHistory", this);
        if (!isDestroyedOrNoOperation(WARN)) mNavigationController.clearHistory();
    }

    /**
     * @see android.webkit.WebView#getCertificate()
     */
    public SslCertificate getCertificate() {
        return isDestroyedOrNoOperation(WARN) ? null
                : SslUtil.getCertificateFromDerBytes(nativeGetCertificate(mNativeAwContents));
    }

    /**
     * @see android.webkit.WebView#clearSslPreferences()
     */
    public void clearSslPreferences() {
        if (TRACE) Log.i(TAG, "%s clearSslPreferences", this);
        if (!isDestroyedOrNoOperation(WARN)) mNavigationController.clearSslPreferences();
    }

    /**
     * Method to return all hit test values relevant to public WebView API.
     * Note that this expose more data than needed for WebView.getHitTestResult.
     * Unsafely returning reference to mutable internal object to avoid excessive
     * garbage allocation on repeated calls.
     */
    public HitTestData getLastHitTestResult() {
        if (TRACE) Log.i(TAG, "%s getLastHitTestResult", this);
        if (isDestroyedOrNoOperation(WARN)) return null;
        nativeUpdateLastHitTestData(mNativeAwContents);
        return mPossiblyStaleHitTestData;
    }

    /**
     * @see android.webkit.WebView#requestFocusNodeHref()
     */
    public void requestFocusNodeHref(Message msg) {
        if (TRACE) Log.i(TAG, "%s requestFocusNodeHref", this);
        if (msg == null || isDestroyedOrNoOperation(WARN)) return;

        nativeUpdateLastHitTestData(mNativeAwContents);
        Bundle data = msg.getData();

        // In order to maintain compatibility with the old WebView's implementation,
        // the absolute (full) url is passed in the |url| field, not only the href attribute.
        data.putString("url", mPossiblyStaleHitTestData.href);
        data.putString("title", mPossiblyStaleHitTestData.anchorText);
        data.putString("src", mPossiblyStaleHitTestData.imgSrc);
        msg.setData(data);
        msg.sendToTarget();
    }

    /**
     * @see android.webkit.WebView#requestImageRef()
     */
    public void requestImageRef(Message msg) {
        if (TRACE) Log.i(TAG, "%s requestImageRef", this);
        if (msg == null || isDestroyedOrNoOperation(WARN)) return;

        nativeUpdateLastHitTestData(mNativeAwContents);
        Bundle data = msg.getData();
        data.putString("url", mPossiblyStaleHitTestData.imgSrc);
        msg.setData(data);
        msg.sendToTarget();
    }

    @VisibleForTesting
    public float getPageScaleFactor() {
        return mPageScaleFactor;
    }

    private float getDeviceScaleFactor() {
        return mWindowAndroid.getWindowAndroid().getDisplay().getDipScale();
    }

    /**
     * @see android.webkit.WebView#getScale()
     *
     * Please note that the scale returned is the page scale multiplied by
     * the screen density factor. See CTS WebViewTest.testSetInitialScale.
     */
    public float getScale() {
        if (isDestroyedOrNoOperation(WARN)) return 1;
        return mPageScaleFactor * getDeviceScaleFactor();
    }

    /**
     * @see android.webkit.WebView#flingScroll(int, int)
     */
    public void flingScroll(int velocityX, int velocityY) {
        if (TRACE) Log.i(TAG, "%s flingScroll", this);
        if (isDestroyedOrNoOperation(WARN)) return;
        mWebContents.getEventForwarder().startFling(
                SystemClock.uptimeMillis(), -velocityX, -velocityY, false, true);
    }

    /**
     * @see android.webkit.WebView#pageUp(boolean)
     */
    public boolean pageUp(boolean top) {
        if (TRACE) Log.i(TAG, "%s pageUp", this);
        if (isDestroyedOrNoOperation(WARN)) return false;
        return mScrollOffsetManager.pageUp(top);
    }

    /**
     * @see android.webkit.WebView#pageDown(boolean)
     */
    public boolean pageDown(boolean bottom) {
        if (TRACE) Log.i(TAG, "%s pageDown", this);
        if (isDestroyedOrNoOperation(WARN)) return false;
        return mScrollOffsetManager.pageDown(bottom);
    }

    /**
     * @see android.webkit.WebView#canZoomIn()
     */
    // This method uses the term 'zoom' for legacy reasons, but relates
    // to what chrome calls the 'page scale factor'.
    public boolean canZoomIn() {
        if (isDestroyedOrNoOperation(WARN)) return false;
        final float zoomInExtent = mMaxPageScaleFactor - mPageScaleFactor;
        return zoomInExtent > ZOOM_CONTROLS_EPSILON;
    }

    /**
     * @see android.webkit.WebView#canZoomOut()
     */
    // This method uses the term 'zoom' for legacy reasons, but relates
    // to what chrome calls the 'page scale factor'.
    public boolean canZoomOut() {
        if (isDestroyedOrNoOperation(WARN)) return false;
        final float zoomOutExtent = mPageScaleFactor - mMinPageScaleFactor;
        return zoomOutExtent > ZOOM_CONTROLS_EPSILON;
    }

    /**
     * @see android.webkit.WebView#zoomIn()
     */
    // This method uses the term 'zoom' for legacy reasons, but relates
    // to what chrome calls the 'page scale factor'.
    public boolean zoomIn() {
        if (!canZoomIn()) {
            return false;
        }
        zoomBy(1.25f);
        return true;
    }

    /**
     * @see android.webkit.WebView#zoomOut()
     */
    // This method uses the term 'zoom' for legacy reasons, but relates
    // to what chrome calls the 'page scale factor'.
    public boolean zoomOut() {
        if (!canZoomOut()) {
            return false;
        }
        zoomBy(0.8f);
        return true;
    }

    /**
     * @see android.webkit.WebView#zoomBy()
     */
    // This method uses the term 'zoom' for legacy reasons, but relates
    // to what chrome calls the 'page scale factor'.
    public void zoomBy(float delta) {
        if (isDestroyedOrNoOperation(WARN)) return;
        if (delta < 0.01f || delta > 100.0f) {
            throw new IllegalStateException("zoom delta value outside [0.01, 100] range.");
        }
        nativeZoomBy(mNativeAwContents, delta);
    }

    /**
     * @see android.webkit.WebView#invokeZoomPicker()
     */
    public void invokeZoomPicker() {
        if (TRACE) Log.i(TAG, "%s invokeZoomPicker", this);
        if (!isDestroyedOrNoOperation(WARN)) mZoomControls.invokeZoomPicker();
    }

    /**
     * @see android.webkit.WebView#preauthorizePermission(Uri, long)
     */
    public void preauthorizePermission(Uri origin, long resources) {
        if (isDestroyedOrNoOperation(NO_WARN)) return;
        nativePreauthorizePermission(mNativeAwContents, origin.toString(), resources);
    }

    /**
     * @see WebContents.evaluateJavaScript(String, JavaScriptCallback)
     */
    public void evaluateJavaScript(String script, final Callback<String> callback) {
        if (TRACE) Log.i(TAG, "%s evaluateJavascript=%s", this, script);
        if (isDestroyedOrNoOperation(WARN)) return;
        JavaScriptCallback jsCallback = null;
        if (callback != null) {
            jsCallback = jsonResult -> {
                // Post the application callback back to the current thread to ensure the
                // application callback is executed without any native code on the stack. This
                // so that any exception thrown by the application callback won't have to be
                // propagated through a native call stack.
                mHandler.post(() -> callback.onResult(jsonResult));
            };
        }

        mWebContents.evaluateJavaScript(script, jsCallback);
    }

    public void evaluateJavaScriptForTests(String script, final Callback<String> callback) {
        if (TRACE) Log.i(TAG, "%s evaluateJavascriptForTests=%s", this, script);
        if (isDestroyedOrNoOperation(NO_WARN)) return;
        JavaScriptCallback jsCallback = null;
        if (callback != null) {
            jsCallback = jsonResult -> callback.onResult(jsonResult);
        }

        mWebContents.evaluateJavaScriptForTests(script, jsCallback);
    }

    public void postMessageToFrame(
            String frameName, String message, String targetOrigin, MessagePort[] sentPorts) {
        if (isDestroyedOrNoOperation(WARN)) return;
        mWebContents.postMessageToFrame(frameName, message, null, targetOrigin, sentPorts);
    }

    /**
     * Creates a message channel and returns the ports for each end of the channel.
     */
    public MessagePort[] createMessageChannel() {
        if (TRACE) Log.i(TAG, "%s createMessageChannel", this);
        if (isDestroyedOrNoOperation(WARN)) return null;
        return MessagePort.createPair();
    }

    public boolean hasAccessedInitialDocument() {
        if (isDestroyedOrNoOperation(NO_WARN)) return false;
        return mWebContents.hasAccessedInitialDocument();
    }

    private WebContentsAccessibility getWebContentsAccessibility() {
        return WebContentsAccessibility.fromWebContents(mWebContents);
    }

    @TargetApi(Build.VERSION_CODES.M)
    public void onProvideVirtualStructure(ViewStructure structure) {
        if (isDestroyedOrNoOperation(WARN)) return;
        if (!mWebContentsObserver.didEverCommitNavigation()) {
            // TODO(sgurun) write a test case for this condition crbug/605251
            structure.setChildCount(0);
            return;
        }
        // for webview, the platform already calculates the scroll (as it is a view) in
        // ViewStructure tree. Do not offset for it in the snapshop x,y position calculations.
        getWebContentsAccessibility().onProvideVirtualStructure(structure, true);
    }

    public void onProvideAutoFillVirtualStructure(ViewStructure structure, int flags) {
        if (mAutofillProvider != null) {
            mAutofillProvider.onProvideAutoFillVirtualStructure(structure, flags);
        }
    }

    public void autofill(final SparseArray<AutofillValue> values) {
        if (mAutofillProvider != null) {
            mAutofillProvider.autofill(values);
        }
    }

    public boolean isSelectActionModeAllowed(int actionModeItem) {
        return (mSettings.getDisabledActionModeMenuItems() & actionModeItem) != actionModeItem;
    }

    //--------------------------------------------------------------------------------------------
    //  View and ViewGroup method implementations
    //--------------------------------------------------------------------------------------------
    /**
     * Calls android.view.View#startActivityForResult.  A RuntimeException will
     * be thrown by Android framework if startActivityForResult is called with
     * a non-Activity context.
     */
    void startActivityForResult(Intent intent, int requestCode) {
        // Even in fullscreen mode, startActivityForResult will still use the
        // initial internal access delegate because it has access to
        // the hidden API View#startActivityForResult.
        mFullScreenTransitionsState.getInitialInternalAccessDelegate()
                .super_startActivityForResult(intent, requestCode);
    }

    void startProcessTextIntent(Intent intent) {
        // on Android M, WebView is not able to replace the text with the processed text.
        // So set the readonly flag for M.
        if (Build.VERSION.SDK_INT == Build.VERSION_CODES.M) {
            intent.putExtra(Intent.EXTRA_PROCESS_TEXT_READONLY, true);
        }

        if (WindowAndroid.activityFromContext(mContext) == null) {
            mContext.startActivity(intent);
            return;
        }

        startActivityForResult(intent, PROCESS_TEXT_REQUEST_CODE);
    }

    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (isDestroyedOrNoOperation(NO_WARN)) return;
        if (requestCode == PROCESS_TEXT_REQUEST_CODE) {
            SelectionPopupController.fromWebContents(mWebContents)
                    .onReceivedProcessTextResult(resultCode, data);
        } else {
            Log.e(TAG, "Received activity result for an unknown request code %d", requestCode);
        }
    }

    /**
     * @see android.webkit.View#onTouchEvent()
     */
    public boolean onTouchEvent(MotionEvent event) {
        return mAwViewMethods.onTouchEvent(event);
    }

    /**
     * @see android.view.View#onHoverEvent()
     */
    public boolean onHoverEvent(MotionEvent event) {
        return mAwViewMethods.onHoverEvent(event);
    }

    /**
     * @see android.view.View#onGenericMotionEvent()
     */
    public boolean onGenericMotionEvent(MotionEvent event) {
        return isDestroyedOrNoOperation(NO_WARN) ? false
                                                 : mAwViewMethods.onGenericMotionEvent(event);
    }

    /**
     * @see android.view.View#onConfigurationChanged()
     */
    public void onConfigurationChanged(Configuration newConfig) {
        mAwViewMethods.onConfigurationChanged(newConfig);
    }

    /**
     * @see android.view.View#onAttachedToWindow()
     */
    public void onAttachedToWindow() {
        if (TRACE) Log.i(TAG, "%s onAttachedToWindow", this);
        mTemporarilyDetached = false;
        mAwViewMethods.onAttachedToWindow();
        mWindowAndroid.getWindowAndroid().getDisplay().addObserver(mDisplayObserver);
    }

    /**
     * @see android.view.View#onDetachedFromWindow()
     */
    @SuppressLint("MissingSuperCall")
    public void onDetachedFromWindow() {
        if (TRACE) Log.i(TAG, "%s onDetachedFromWindow", this);
        mWindowAndroid.getWindowAndroid().getDisplay().removeObserver(mDisplayObserver);
        mAwViewMethods.onDetachedFromWindow();
    }

    /**
     * @see android.view.View#onWindowFocusChanged()
     */
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        mAwViewMethods.onWindowFocusChanged(hasWindowFocus);
    }

    /**
     * @see android.view.View#onFocusChanged()
     */
    public void onFocusChanged(boolean focused, int direction, Rect previouslyFocusedRect) {
        if (!mTemporarilyDetached) {
            mAwViewMethods.onFocusChanged(focused, direction, previouslyFocusedRect);
        }
    }

    /**
     * @see android.view.View#onStartTemporaryDetach()
     */
    public void onStartTemporaryDetach() {
        mTemporarilyDetached = true;
    }

    /**
     * @see android.view.View#onFinishTemporaryDetach()
     */
    public void onFinishTemporaryDetach() {
        mTemporarilyDetached = false;
    }

    /**
     * @see android.view.View#onSizeChanged()
     */
    public void onSizeChanged(int w, int h, int ow, int oh) {
        mAwViewMethods.onSizeChanged(w, h, ow, oh);
    }

    /**
     * @see android.view.View#onVisibilityChanged()
     */
    public void onVisibilityChanged(View changedView, int visibility) {
        mAwViewMethods.onVisibilityChanged(changedView, visibility);
    }

    /**
     * @see android.view.View#onWindowVisibilityChanged()
     */
    public void onWindowVisibilityChanged(int visibility) {
        mAwViewMethods.onWindowVisibilityChanged(visibility);
    }

    private void setViewVisibilityInternal(boolean visible) {
        mIsViewVisible = visible;
        if (!isDestroyedOrNoOperation(NO_WARN)) {
            nativeSetViewVisibility(mNativeAwContents, mIsViewVisible);
        }
        postUpdateWebContentsVisibility();
    }

    private void setWindowVisibilityInternal(boolean visible) {
        mInvalidateRootViewOnNextDraw |= Build.VERSION.SDK_INT <= Build.VERSION_CODES.LOLLIPOP
                && visible && !mIsWindowVisible;
        mIsWindowVisible = visible;
        if (!isDestroyedOrNoOperation(NO_WARN)) {
            nativeSetWindowVisibility(mNativeAwContents, mIsWindowVisible);
        }
        postUpdateWebContentsVisibility();
    }

    private void postUpdateWebContentsVisibility() {
        if (mIsUpdateVisibilityTaskPending) return;
        // When WebView is attached to a visible window, WebView will be
        // attached to a window whose visibility is initially invisible, then
        // the window visibility will be updated to true. This means CVC
        // visibility will be set to false then true immediately, in the same
        // function call of View#dispatchAttachedToWindow.  DetachedFromWindow
        // is a similar case, where window visibility changes before AwContents
        // is detached from window.
        //
        // To prevent this flip of CVC visibility, post the task to update CVC
        // visibility during attach, detach and window visibility change.
        mIsUpdateVisibilityTaskPending = true;
        mHandler.post(mUpdateVisibilityRunnable);
    }

    private void updateWebContentsVisibility() {
        mIsUpdateVisibilityTaskPending = false;
        if (isDestroyedOrNoOperation(NO_WARN)) return;
        boolean contentVisible = nativeIsVisible(mNativeAwContents);

        if (contentVisible && !mIsContentVisible) {
            mWebContents.onShow();
        } else if (!contentVisible && mIsContentVisible) {
            mWebContents.onHide();
        }
        mIsContentVisible = contentVisible;
        updateChildProcessImportance();
    }

    /**
     * Returns true if the page is visible according to DOM page visibility API.
     * See http://www.w3.org/TR/page-visibility/
     * This method is only called by tests and will return the supposed CVC
     * visibility without waiting a pending mUpdateVisibilityRunnable to run.
     */
    @VisibleForTesting
    public boolean isPageVisible() {
        if (isDestroyedOrNoOperation(NO_WARN)) return mIsContentVisible;
        return nativeIsVisible(mNativeAwContents);
    }

    /**
     * Key for opaque state in bundle. Note this is only public for tests.
     */
    public static final String SAVE_RESTORE_STATE_KEY = "WEBVIEW_CHROMIUM_STATE";

    /**
     * Save the state of this AwContents into provided Bundle.
     * @return False if saving state failed.
     */
    public boolean saveState(Bundle outState) {
        if (TRACE) Log.i(TAG, "%s saveState", this);
        if (isDestroyedOrNoOperation(WARN) || outState == null) return false;

        byte[] state = nativeGetOpaqueState(mNativeAwContents);
        if (state == null) return false;

        outState.putByteArray(SAVE_RESTORE_STATE_KEY, state);
        return true;
    }

    /**
     * Restore the state of this AwContents into provided Bundle.
     * @param inState Must be a bundle returned by saveState.
     * @return False if restoring state failed.
     */
    public boolean restoreState(Bundle inState) {
        if (TRACE) Log.i(TAG, "%s restoreState", this);
        if (isDestroyedOrNoOperation(WARN) || inState == null) return false;

        byte[] state = inState.getByteArray(SAVE_RESTORE_STATE_KEY);
        if (state == null) return false;

        boolean result = nativeRestoreFromOpaqueState(mNativeAwContents, state);

        // The onUpdateTitle callback normally happens when a page is loaded,
        // but is optimized out in the restoreState case because the title is
        // already restored. See WebContentsImpl::UpdateTitleForEntry. So we
        // call the callback explicitly here.
        if (result) mContentsClient.onReceivedTitle(mWebContents.getTitle());

        return result;
    }

    /**
     * @see JavascriptInjector#addPossiblyUnsafeInterface(Object, String, Class)
     */
    @SuppressLint("NewApi")  // JavascriptInterface requires API level 17.
    public void addJavascriptInterface(Object object, String name) {
        if (TRACE) Log.i(TAG, "%s addJavascriptInterface=%s", this, name);
        if (isDestroyedOrNoOperation(WARN)) return;
        Class<? extends Annotation> requiredAnnotation = null;
        if (mAppTargetSdkVersion >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            requiredAnnotation = JavascriptInterface.class;
        }

        getJavascriptInjector().addPossiblyUnsafeInterface(object, name, requiredAnnotation);
    }

    /**
     * @see android.webkit.WebView#removeJavascriptInterface(String)
     */
    public void removeJavascriptInterface(String interfaceName) {
        if (TRACE) Log.i(TAG, "%s removeInterface=%s", this, interfaceName);
        if (isDestroyedOrNoOperation(WARN)) return;

        getJavascriptInjector().removeInterface(interfaceName);
    }

    /**
     * If native accessibility (not script injection) is enabled, and if this is
     * running on JellyBean or later, returns an AccessibilityNodeProvider that
     * implements native accessibility for this view. Returns null otherwise.
     * @return The AccessibilityNodeProvider, if available, or null otherwise.
     */
    public AccessibilityNodeProvider getAccessibilityNodeProvider() {
        return mAwViewMethods.getAccessibilityNodeProvider();
    }

    public boolean supportsAccessibilityAction(int action) {
        return isDestroyedOrNoOperation(WARN)
                ? false
                : getWebContentsAccessibility().supportsAction(action);
    }

    /**
     * @see android.webkit.WebView#performAccessibilityAction(int, Bundle)
     */
    public boolean performAccessibilityAction(int action, Bundle arguments) {
        return mAwViewMethods.performAccessibilityAction(action, arguments);
    }

    /**
     * @see android.webkit.WebView#clearFormData()
     */
    public void hideAutofillPopup() {
        if (TRACE) Log.i(TAG, "%s hideAutofillPopup", this);
        if (mAwAutofillClient != null) {
            mAwAutofillClient.hideAutofillPopup();
        }
    }

    public void setNetworkAvailable(boolean networkUp) {
        if (TRACE) Log.i(TAG, "%s setNetworkAvailable=%s", this, networkUp);
        if (!isDestroyedOrNoOperation(WARN)) {
            // For backward compatibility when an app uses this API disable the
            // Network Information API to prevent inconsistencies,
            // see crbug.com/520088.
            NetworkChangeNotifier.setAutoDetectConnectivityState(false);
            nativeSetJsOnlineProperty(mNativeAwContents, networkUp);
        }
    }

    /**
     * Inserts a {@link VisualStateCallback}.
     *
     * The {@link VisualStateCallback} will be inserted in Blink and will be invoked when the
     * contents of the DOM tree at the moment that the callback was inserted (or later) are drawn
     * into the screen. In other words, the following events need to happen before the callback is
     * invoked:
     * 1. The DOM tree is committed becoming the pending tree - see ThreadProxy::BeginMainFrame
     * 2. The pending tree is activated becoming the active tree
     * 3. A frame swap happens that draws the active tree into the screen
     *
     * @param requestId an id that will be returned from the callback invocation to allow
     * callers to match requests with callbacks.
     * @param callback the callback to be inserted
     * @throw IllegalStateException if this method is invoked after {@link #destroy()} has been
     * called.
     */
    public void insertVisualStateCallback(long requestId, VisualStateCallback callback) {
        if (TRACE) Log.i(TAG, "%s insertVisualStateCallback", this);
        if (isDestroyed(NO_WARN)) throw new IllegalStateException(
                "insertVisualStateCallback cannot be called after the WebView has been destroyed");
        nativeInsertVisualStateCallback(mNativeAwContents, requestId, callback);
    }

    public boolean isPopupWindow() {
        return mIsPopupWindow;
    }

    private void updateChildProcessImportance() {
        @ChildProcessImportance
        int effectiveImportance = ChildProcessImportance.IMPORTANT;
        if (mRendererPriorityWaivedWhenNotVisible && !mIsContentVisible) {
            effectiveImportance = ChildProcessImportance.NORMAL;
        } else {
            switch (mRendererPriority) {
                case RendererPriority.INITIAL:
                case RendererPriority.HIGH:
                    effectiveImportance = ChildProcessImportance.IMPORTANT;
                    break;
                case RendererPriority.LOW:
                    effectiveImportance = ChildProcessImportance.MODERATE;
                    break;
                case RendererPriority.WAIVED:
                    effectiveImportance = ChildProcessImportance.NORMAL;
                    break;
                default:
                    assert false;
            }
        }
        mWebContents.setImportance(effectiveImportance);
    }

    @RendererPriority
    public int getRendererRequestedPriority() {
        return mRendererPriority;
    }

    public boolean getRendererPriorityWaivedWhenNotVisible() {
        return mRendererPriorityWaivedWhenNotVisible;
    }

    public void setRendererPriorityPolicy(
            @RendererPriority int rendererRequestedPriority, boolean waivedWhenNotVisible) {
        mRendererPriority = rendererRequestedPriority;
        mRendererPriorityWaivedWhenNotVisible = waivedWhenNotVisible;
        updateChildProcessImportance();
    }

    public void setTextClassifier(TextClassifier textClassifier) {
        assert mWebContents != null;
        SelectionPopupController.fromWebContents(mWebContents).setTextClassifier(textClassifier);
    }

    public TextClassifier getTextClassifier() {
        assert mWebContents != null;
        return SelectionPopupController.fromWebContents(mWebContents).getTextClassifier();
    }

    public AwRenderProcess getRenderProcess() {
        if (isDestroyedOrNoOperation(WARN)) {
            return null;
        }
        return nativeGetRenderProcess(mNativeAwContents);
    }

    //--------------------------------------------------------------------------------------------
    //  Methods called from native via JNI
    //--------------------------------------------------------------------------------------------

    @CalledByNative
    private static void onDocumentHasImagesResponse(boolean result, Message message) {
        message.arg1 = result ? 1 : 0;
        message.sendToTarget();
    }

    @CalledByNative
    private void onReceivedTouchIconUrl(String url, boolean precomposed) {
        mContentsClient.onReceivedTouchIconUrl(url, precomposed);
    }

    @CalledByNative
    private void onReceivedIcon(Bitmap bitmap) {
        mContentsClient.onReceivedIcon(bitmap);
        mFavicon = bitmap;
    }

    @CalledByNative
    private long onCreateTouchHandle() {
        PopupTouchHandleDrawable drawable = PopupTouchHandleDrawable.create(
                mTouchHandleDrawables, mWebContents, mContainerView);
        return drawable.getNativeDrawable();
    }

    /** Callback for generateMHTML. */
    @CalledByNative
    private static void generateMHTMLCallback(String path, long size, Callback<String> callback) {
        if (callback == null) return;
        callback.onResult(size < 0 ? null : path);
    }

    @CalledByNative
    private void onReceivedHttpAuthRequest(AwHttpAuthHandler handler, String host, String realm) {
        mContentsClient.onReceivedHttpAuthRequest(handler, host, realm);
    }

    public AwGeolocationPermissions getGeolocationPermissions() {
        return mBrowserContext.getGeolocationPermissions();
    }

    public void invokeGeolocationCallback(boolean value, String requestingFrame) {
        if (isDestroyedOrNoOperation(NO_WARN)) return;
        nativeInvokeGeolocationCallback(mNativeAwContents, value, requestingFrame);
    }

    @CalledByNative
    private void onGeolocationPermissionsShowPrompt(String origin) {
        if (isDestroyedOrNoOperation(NO_WARN)) return;
        AwGeolocationPermissions permissions = mBrowserContext.getGeolocationPermissions();
        // Reject if geoloaction is disabled, or the origin has a retained deny
        if (!mSettings.getGeolocationEnabled()) {
            nativeInvokeGeolocationCallback(mNativeAwContents, false, origin);
            return;
        }
        // Allow if the origin has a retained allow
        if (permissions.hasOrigin(origin)) {
            nativeInvokeGeolocationCallback(mNativeAwContents, permissions.isOriginAllowed(origin),
                    origin);
            return;
        }
        mContentsClient.onGeolocationPermissionsShowPrompt(
                origin, new AwGeolocationCallback(origin, this));
    }

    @CalledByNative
    private void onGeolocationPermissionsHidePrompt() {
        mContentsClient.onGeolocationPermissionsHidePrompt();
    }

    @CalledByNative
    private void onPermissionRequest(AwPermissionRequest awPermissionRequest) {
        mContentsClient.onPermissionRequest(awPermissionRequest);
    }

    @CalledByNative
    private void onPermissionRequestCanceled(AwPermissionRequest awPermissionRequest) {
        mContentsClient.onPermissionRequestCanceled(awPermissionRequest);
    }

    @CalledByNative
    public void onFindResultReceived(int activeMatchOrdinal, int numberOfMatches,
            boolean isDoneCounting) {
        mContentsClient.onFindResultReceived(activeMatchOrdinal, numberOfMatches, isDoneCounting);
    }

    @CalledByNative
    public void onNewPicture() {
        // Don't call capturePicture() here but instead defer it until the posted task runs within
        // the callback helper, to avoid doubling back into the renderer compositor in the middle
        // of the notification it is sending up to here.
        mContentsClient.getCallbackHelper().postOnNewPicture(mPictureListenerContentProvider);
    }

    /**
     * Invokes the given {@link VisualStateCallback}.
     *
     * @param callback the callback to be invoked
     * @param requestId the id passed to {@link AwContents#insertVisualStateCallback}
     * @param result true if the callback should succeed and false otherwise
     */
    @CalledByNative
    public void invokeVisualStateCallback(
            final VisualStateCallback callback, final long requestId) {
        if (isDestroyedOrNoOperation(NO_WARN)) return;
        // Posting avoids invoking the callback inside invoking_composite_
        // (see synchronous_compositor_impl.cc and crbug/452530).
        mHandler.post(() -> callback.onComplete(requestId));
    }

    // Called as a result of nativeUpdateLastHitTestData.
    @CalledByNative
    private void updateHitTestData(
            int type, String extra, String href, String anchorText, String imgSrc) {
        mPossiblyStaleHitTestData.hitTestResultType = type;
        mPossiblyStaleHitTestData.hitTestResultExtraData = extra;
        mPossiblyStaleHitTestData.href = href;
        mPossiblyStaleHitTestData.anchorText = anchorText;
        mPossiblyStaleHitTestData.imgSrc = imgSrc;
    }

    @CalledByNative
    private void postInvalidateOnAnimation() {
        if (!mWindowAndroid.getWindowAndroid().isInsideVSync()) {
            mContainerView.postInvalidateOnAnimation();
        } else {
            mContainerView.invalidate();
        }
    }

    @CalledByNative
    private int[] getLocationOnScreen() {
        int[] result = new int[2];
        mContainerView.getLocationOnScreen(result);
        return result;
    }

    @CalledByNative
    private void onWebLayoutPageScaleFactorChanged(float webLayoutPageScaleFactor) {
        // This change notification comes from the renderer thread, not from the cc/ impl thread.
        mLayoutSizer.onPageScaleChanged(webLayoutPageScaleFactor);
    }

    @CalledByNative
    private void onWebLayoutContentsSizeChanged(int widthCss, int heightCss) {
        // This change notification comes from the renderer thread, not from the cc/ impl thread.
        mLayoutSizer.onContentSizeChanged(widthCss, heightCss);
    }

    @CalledByNative
    private void scrollContainerViewTo(int xPx, int yPx) {
        // Both xPx and yPx are in physical pixel scale.
        mScrollOffsetManager.scrollContainerViewTo(xPx, yPx);
    }

    @CalledByNative
    private void updateScrollState(int maxContainerViewScrollOffsetX,
            int maxContainerViewScrollOffsetY, int contentWidthDip, int contentHeightDip,
            float pageScaleFactor, float minPageScaleFactor, float maxPageScaleFactor) {
        mContentWidthDip = contentWidthDip;
        mContentHeightDip = contentHeightDip;
        mScrollOffsetManager.setMaxScrollOffset(maxContainerViewScrollOffsetX,
                maxContainerViewScrollOffsetY);
        setPageScaleFactorAndLimits(pageScaleFactor, minPageScaleFactor, maxPageScaleFactor);
    }

    @CalledByNative
    private void setAwAutofillClient(AwAutofillClient client) {
        mAwAutofillClient = client;
        client.init(mContext);
    }

    @CalledByNative
    private void didOverscroll(int deltaX, int deltaY, float velocityX, float velocityY) {
        mScrollOffsetManager.overScrollBy(deltaX, deltaY);

        if (mOverScrollGlow == null) return;

        mOverScrollGlow.setOverScrollDeltas(deltaX, deltaY);
        final int oldX = mContainerView.getScrollX();
        final int oldY = mContainerView.getScrollY();
        final int x = oldX + deltaX;
        final int y = oldY + deltaY;
        final int scrollRangeX = mScrollOffsetManager.computeMaximumHorizontalScrollOffset();
        final int scrollRangeY = mScrollOffsetManager.computeMaximumVerticalScrollOffset();
        // absorbGlow() will release the glow if it is not finished.
        mOverScrollGlow.absorbGlow(x, y, oldX, oldY, scrollRangeX, scrollRangeY,
                (float) Math.hypot(velocityX, velocityY));

        if (mOverScrollGlow.isAnimating()) {
            postInvalidateOnAnimation();
        }
    }

    /**
     * Determine if at least one edge of the WebView extends over the edge of the window.
     */
    private boolean extendsOutOfWindow() {
        int loc[] = new int[2];
        mContainerView.getLocationOnScreen(loc);
        int x = loc[0];
        int y = loc[1];
        mContainerView.getRootView().getLocationOnScreen(loc);
        int rootX = loc[0];
        int rootY = loc[1];

        // Get the position of the current view, relative to its root view
        int relativeX = x - rootX;
        int relativeY = y - rootY;

        if (relativeX < 0 || relativeY < 0
                || relativeX + mContainerView.getWidth() > mContainerView.getRootView().getWidth()
                || relativeY + mContainerView.getHeight()
                        > mContainerView.getRootView().getHeight()) {
            return true;
        }
        return false;
    }

    /**
     * Determine if it's reasonable to show any sort of interstitial. If the WebView is not visible,
     * the user may not be able to interact with the UI.
     * @return true if the WebView is visible
     */
    @VisibleForTesting
    @CalledByNative
    protected boolean canShowInterstitial() {
        return mIsAttachedToWindow && mIsViewVisible;
    }

    @CalledByNative
    private int getErrorUiType() {
        if (extendsOutOfWindow()) {
            return ErrorUiType.QUIET_GIANT;
        } else if (canShowBigInterstitial()) {
            return ErrorUiType.LOUD;
        } else {
            return ErrorUiType.QUIET_SMALL;
        }
    }

    /**
     * Determine if it's suitable to show the interstitial for browsers and main UIs. If the WebView
     * is close to full-screen, we assume the app is using it as the main UI, so we show the same
     * interstitial Chrome uses. Otherwise, we assume the WebView is part of a larger composed page,
     * and will show a different interstitial.
     * @return true if the WebView should display the large interstitial
     */
    @VisibleForTesting
    protected boolean canShowBigInterstitial() {
        double percentOfScreenHeight =
                (double) mContainerView.getHeight() / mContainerView.getRootView().getHeight();

        // Make a guess as to whether the WebView is the predominant part of the UI
        return mContainerView.getWidth() == mContainerView.getRootView().getWidth()
                && percentOfScreenHeight >= MIN_SCREEN_HEIGHT_PERCENTAGE_FOR_INTERSTITIAL;
    }

    @VisibleForTesting
    public void evaluateJavaScriptOnInterstitialForTesting(
            String script, final Callback<String> callback) {
        if (TRACE) Log.i(TAG, "%s evaluateJavaScriptOnInterstitialForTesting=%s", this, script);
        if (isDestroyedOrNoOperation(WARN)) return;
        JavaScriptCallback jsCallback = null;
        if (callback != null) {
            jsCallback = jsonResult -> callback.onResult(jsonResult);
        }

        nativeEvaluateJavaScriptOnInterstitialForTesting(mNativeAwContents, script, jsCallback);
    }

    @CalledByNative
    private static void onEvaluateJavaScriptResultForTesting(
            String jsonResult, JavaScriptCallback callback) {
        callback.handleJavaScriptResult(jsonResult);
    }

    /**
     * Return the device locale in the same format we use to populate the 'hl' query parameter for
     * Safe Browsing interstitial urls, as done in BaseUIManager::app_locale().
     */
    @VisibleForTesting
    public static String getSafeBrowsingLocaleForTesting() {
        return nativeGetSafeBrowsingLocaleForTesting();
    }

    // -------------------------------------------------------------------------------------------
    // Helper methods
    // -------------------------------------------------------------------------------------------

    private void setPageScaleFactorAndLimits(
            float pageScaleFactor, float minPageScaleFactor, float maxPageScaleFactor) {
        if (mPageScaleFactor == pageScaleFactor
                && mMinPageScaleFactor == minPageScaleFactor
                && mMaxPageScaleFactor == maxPageScaleFactor) {
            return;
        }
        mMinPageScaleFactor = minPageScaleFactor;
        mMaxPageScaleFactor = maxPageScaleFactor;
        if (mPageScaleFactor != pageScaleFactor) {
            float oldPageScaleFactor = mPageScaleFactor;
            mPageScaleFactor = pageScaleFactor;
            float dipScale = getDeviceScaleFactor();
            mContentsClient.getCallbackHelper().postOnScaleChangedScaled(
                    oldPageScaleFactor * dipScale, mPageScaleFactor * dipScale);
        }
    }

    private void saveWebArchiveInternal(String path, final Callback<String> callback) {
        if (path == null || isDestroyedOrNoOperation(WARN)) {
            if (callback == null) return;

            ThreadUtils.runOnUiThread(() -> callback.onResult(null));
        } else {
            nativeGenerateMHTML(mNativeAwContents, path, callback);
        }
    }

    /**
     * Try to generate a pathname for saving an MHTML archive. This roughly follows WebView's
     * autoname logic.
     */
    private static String generateArchiveAutoNamePath(String originalUrl, String baseName) {
        String name = null;
        if (originalUrl != null && !originalUrl.isEmpty()) {
            try {
                String path = new URL(originalUrl).getPath();
                int lastSlash = path.lastIndexOf('/');
                if (lastSlash > 0) {
                    name = path.substring(lastSlash + 1);
                } else {
                    name = path;
                }
            } catch (MalformedURLException e) {
                // If it fails parsing the URL, we'll just rely on the default name below.
            }
        }

        if (TextUtils.isEmpty(name)) name = "index";

        String testName = baseName + name + WEB_ARCHIVE_EXTENSION;
        if (!new File(testName).exists()) return testName;

        for (int i = 1; i < 100; i++) {
            testName = baseName + name + "-" + i + WEB_ARCHIVE_EXTENSION;
            if (!new File(testName).exists()) return testName;
        }

        Log.e(TAG, "Unable to auto generate archive name for path: %s", baseName);
        return null;
    }

    @Override
    public void extractSmartClipData(int x, int y, int width, int height) {
        if (!isDestroyedOrNoOperation(WARN)) {
            mWebContents.requestSmartClipExtract(x, y, width, height);
        }
    }

    @Override
    public void setSmartClipResultHandler(final Handler resultHandler) {
        if (isDestroyedOrNoOperation(WARN)) return;

        mWebContents.setSmartClipResultHandler(resultHandler);
    }

    protected void insertVisualStateCallbackIfNotDestroyed(
            long requestId, VisualStateCallback callback) {
        if (TRACE) Log.i(TAG, "%s insertVisualStateCallbackIfNotDestroyed", this);
        if (isDestroyedOrNoOperation(NO_WARN)) return;
        nativeInsertVisualStateCallback(mNativeAwContents, requestId, callback);
    }

    public static boolean isDpadEvent(KeyEvent event) {
        if (event.getAction() == KeyEvent.ACTION_DOWN) {
            switch (event.getKeyCode()) {
                case KeyEvent.KEYCODE_DPAD_CENTER:
                case KeyEvent.KEYCODE_DPAD_DOWN:
                case KeyEvent.KEYCODE_DPAD_UP:
                case KeyEvent.KEYCODE_DPAD_LEFT:
                case KeyEvent.KEYCODE_DPAD_RIGHT:
                    return true;
            }
        }
        return false;
    }

    // --------------------------------------------------------------------------------------------
    // This is the AwViewMethods implementation that does real work. The AwViewMethodsImpl is
    // hooked up to the WebView in embedded mode and to the FullScreenView in fullscreen mode,
    // but not to both at the same time.
    private class AwViewMethodsImpl implements AwViewMethods {
        private int mLayerType = View.LAYER_TYPE_NONE;
        private ComponentCallbacks2 mComponentCallbacks;

        // Only valid within software onDraw().
        private final Rect mClipBoundsTemporary = new Rect();

        @Override
        public void onDraw(Canvas canvas) {
            if (isDestroyedOrNoOperation(NO_WARN)) {
                TraceEvent.instant("EarlyOut_destroyed");
                canvas.drawColor(getEffectiveBackgroundColor());
                return;
            }

            // For hardware draws, the clip at onDraw time could be different
            // from the clip during DrawGL.
            if (!canvas.isHardwareAccelerated() && !canvas.getClipBounds(mClipBoundsTemporary)) {
                TraceEvent.instant("EarlyOut_software_empty_clip");
                return;
            }

            mScrollOffsetManager.syncScrollOffsetFromOnDraw();
            int scrollX = mContainerView.getScrollX();
            int scrollY = mContainerView.getScrollY();
            Rect globalVisibleRect = getGlobalVisibleRect();
            // Workaround for bug in libhwui on N that does not swap if inserting functor is the
            // only operation in a canvas. See crbug.com/704212.
            if (Build.VERSION.SDK_INT == Build.VERSION_CODES.N
                    || Build.VERSION.SDK_INT == Build.VERSION_CODES.N_MR1) {
                if (mPaintForNWorkaround == null) {
                    mPaintForNWorkaround = new Paint();
                    // Note a completely transparent color will get optimized out. So draw almost
                    // transparent black, but then scale alpha down to effectively 0.
                    mPaintForNWorkaround.setColor(Color.argb(1, 0, 0, 0));
                    ColorMatrix colorMatrix = new ColorMatrix();
                    colorMatrix.setScale(0.f, 0.f, 0.f, 0.1f);
                    mPaintForNWorkaround.setColorFilter(new ColorMatrixColorFilter(colorMatrix));
                }
                canvas.drawRect(0, 0, 1, 1, mPaintForNWorkaround);
            }
            boolean did_draw = nativeOnDraw(mNativeAwContents, canvas,
                    canvas.isHardwareAccelerated(), scrollX, scrollY, globalVisibleRect.left,
                    globalVisibleRect.top, globalVisibleRect.right, globalVisibleRect.bottom,
                    ForceAuxiliaryBitmapRendering.sResult);
            if (canvas.isHardwareAccelerated()
                    && nativeNeedToDrawBackgroundColor(mNativeAwContents)) {
                TraceEvent.instant("DrawBackgroundColor");
                canvas.drawColor(getEffectiveBackgroundColor());
            }
            if (did_draw && canvas.isHardwareAccelerated()
                    && !ForceAuxiliaryBitmapRendering.sResult) {
                did_draw = mCurrentFunctor.requestDrawGL(canvas);
            }
            if (did_draw) {
                int scrollXDiff = mContainerView.getScrollX() - scrollX;
                int scrollYDiff = mContainerView.getScrollY() - scrollY;
                canvas.translate(-scrollXDiff, -scrollYDiff);
            } else {
                TraceEvent.instant("NativeDrawFailed");
                canvas.drawColor(getEffectiveBackgroundColor());
            }

            if (mOverScrollGlow != null && mOverScrollGlow.drawEdgeGlows(canvas,
                    mScrollOffsetManager.computeMaximumHorizontalScrollOffset(),
                    mScrollOffsetManager.computeMaximumVerticalScrollOffset())) {
                postInvalidateOnAnimation();
            }

            if (mInvalidateRootViewOnNextDraw) {
                mContainerView.getRootView().invalidate();
                mInvalidateRootViewOnNextDraw = false;
            }
        }

        @Override
        public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            mLayoutSizer.onMeasure(widthMeasureSpec, heightMeasureSpec);
        }

        @Override
        public void requestFocus() {
            if (isDestroyedOrNoOperation(NO_WARN)) return;
            if (!mContainerView.isInTouchMode() && mSettings.shouldFocusFirstNode()) {
                nativeFocusFirstNode(mNativeAwContents);
            }
        }

        @Override
        public void setLayerType(int layerType, Paint paint) {
            mLayerType = layerType;
            updateHardwareAcceleratedFeaturesToggle();
        }

        private void updateHardwareAcceleratedFeaturesToggle() {
            mSettings.setEnableSupportedHardwareAcceleratedFeatures(
                    mIsAttachedToWindow && mContainerView.isHardwareAccelerated()
                            && (mLayerType == View.LAYER_TYPE_NONE
                                    || mLayerType == View.LAYER_TYPE_HARDWARE));
        }

        @Override
        public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
            return isDestroyedOrNoOperation(NO_WARN)
                    ? null
                    : ImeAdapter.fromWebContents(mWebContents).onCreateInputConnection(outAttrs);
        }

        @Override
        public boolean onDragEvent(DragEvent event) {
            return isDestroyedOrNoOperation(NO_WARN)
                    ? false
                    : mWebContents.getEventForwarder().onDragEvent(event, mContainerView);
        }

        @Override
        public boolean onKeyUp(int keyCode, KeyEvent event) {
            return isDestroyedOrNoOperation(NO_WARN)
                    ? false
                    : mWebContents.getEventForwarder().onKeyUp(keyCode, event);
        }

        @Override
        public boolean dispatchKeyEvent(KeyEvent event) {
            if (isDestroyedOrNoOperation(NO_WARN)) return false;
            if (isDpadEvent(event)) {
                mSettings.setSpatialNavigationEnabled(true);
            }

            // Following check is dup'ed from |ContentUiEventHandler.dispatchKeyEvent| to avoid
            // embedder-specific customization, which is necessary only for WebView.
            if (GamepadList.dispatchKeyEvent(event)) return true;

            // This check reflects Chrome's behavior and is a workaround for http://b/7697782.
            if (mContentsClient.hasWebViewClient()
                    && mContentsClient.shouldOverrideKeyEvent(event)) {
                return mInternalAccessAdapter.super_dispatchKeyEvent(event);
            }
            return mWebContents.getEventForwarder().dispatchKeyEvent(event);
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            if (isDestroyedOrNoOperation(NO_WARN)) return false;
            if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
                mSettings.setSpatialNavigationEnabled(false);
            }

            mScrollOffsetManager.setProcessingTouchEvent(true);
            boolean rv = mWebContents.getEventForwarder().onTouchEvent(event);
            mScrollOffsetManager.setProcessingTouchEvent(false);

            if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
                // Note this will trigger IPC back to browser even if nothing is
                // hit.
                float eventX = event.getX();
                float eventY = event.getY();
                float touchMajor = Math.max(event.getTouchMajor(), event.getTouchMinor());
                if (!UseZoomForDSFPolicy.isUseZoomForDSFEnabled()) {
                    float dipScale = getDeviceScaleFactor();
                    eventX /= dipScale;
                    eventY /= dipScale;
                    touchMajor /= dipScale;
                }
                nativeRequestNewHitTestDataAt(mNativeAwContents, eventX, eventY, touchMajor);
            }

            if (mOverScrollGlow != null) {
                if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
                    mOverScrollGlow.setShouldPull(true);
                } else if (event.getActionMasked() == MotionEvent.ACTION_UP
                        || event.getActionMasked() == MotionEvent.ACTION_CANCEL) {
                    mOverScrollGlow.setShouldPull(false);
                    mOverScrollGlow.releaseAll();
                }
            }

            return rv;
        }

        @Override
        public boolean onHoverEvent(MotionEvent event) {
            return isDestroyedOrNoOperation(NO_WARN)
                    ? false
                    : mWebContents.getEventForwarder().onHoverEvent(event);
        }

        @Override
        public boolean onGenericMotionEvent(MotionEvent event) {
            return isDestroyedOrNoOperation(NO_WARN)
                    ? false
                    : mWebContents.getEventForwarder().onGenericMotionEvent(event);
        }

        @Override
        public void onConfigurationChanged(Configuration newConfig) {
            if (!isDestroyedOrNoOperation(NO_WARN)) {
                mViewEventSink.onConfigurationChanged(newConfig);
                mInternalAccessAdapter.super_onConfigurationChanged(newConfig);
            }
        }

        @Override
        public void onAttachedToWindow() {
            if (isDestroyedOrNoOperation(NO_WARN)) return;
            if (mIsAttachedToWindow) {
                Log.w(TAG, "onAttachedToWindow called when already attached. Ignoring");
                return;
            }
            mIsAttachedToWindow = true;

            mViewEventSink.onAttachedToWindow();
            nativeOnAttachedToWindow(mNativeAwContents, mContainerView.getWidth(),
                    mContainerView.getHeight());
            updateHardwareAcceleratedFeaturesToggle();
            postUpdateWebContentsVisibility();
            mCurrentFunctor.onAttachedToWindow();

            updateDefaultLocale();
            mSettings.updateAcceptLanguages();

            if (mComponentCallbacks != null) return;
            mComponentCallbacks = new AwComponentCallbacks();
            mContext.registerComponentCallbacks(mComponentCallbacks);
        }

        @Override
        public void onDetachedFromWindow() {
            if (isDestroyedOrNoOperation(NO_WARN)) return;
            if (!mIsAttachedToWindow) {
                Log.w(TAG, "onDetachedFromWindow called when already detached. Ignoring");
                return;
            }
            mIsAttachedToWindow = false;
            hideAutofillPopup();
            nativeOnDetachedFromWindow(mNativeAwContents);

            mViewEventSink.onDetachedFromWindow();
            updateHardwareAcceleratedFeaturesToggle();
            postUpdateWebContentsVisibility();
            mCurrentFunctor.onDetachedFromWindow();

            if (mComponentCallbacks != null) {
                mContext.unregisterComponentCallbacks(mComponentCallbacks);
                mComponentCallbacks = null;
            }

            mScrollAccessibilityHelper.removePostedCallbacks();
            mZoomControls.dismissZoomPicker();
        }

        @Override
        public void onWindowFocusChanged(boolean hasWindowFocus) {
            if (isDestroyedOrNoOperation(NO_WARN)) return;
            mWindowFocused = hasWindowFocus;
            mViewEventSink.onWindowFocusChanged(hasWindowFocus);
        }

        @Override
        public void onFocusChanged(boolean focused, int direction, Rect previouslyFocusedRect) {
            if (isDestroyedOrNoOperation(NO_WARN)) return;
            mContainerViewFocused = focused;
            mViewEventSink.onViewFocusChanged(focused);
        }

        @Override
        public void onSizeChanged(int w, int h, int ow, int oh) {
            if (isDestroyedOrNoOperation(NO_WARN)) return;
            mScrollOffsetManager.setContainerViewSize(w, h);
            // The AwLayoutSizer needs to go first so that if we're in
            // fixedLayoutSize mode the update
            // to enter fixedLayoutSize mode is sent before the first resize
            // update.
            mLayoutSizer.onSizeChanged(w, h, ow, oh);
            nativeOnSizeChanged(mNativeAwContents, w, h, ow, oh);
        }

        @Override
        public void onVisibilityChanged(View changedView, int visibility) {
            boolean viewVisible = mContainerView.getVisibility() == View.VISIBLE;
            if (mIsViewVisible == viewVisible) return;
            setViewVisibilityInternal(viewVisible);
        }

        @Override
        public void onWindowVisibilityChanged(int visibility) {
            boolean windowVisible = visibility == View.VISIBLE;
            if (mIsWindowVisible == windowVisible) return;
            setWindowVisibilityInternal(windowVisible);
        }

        @Override
        public void onContainerViewScrollChanged(int l, int t, int oldl, int oldt) {
            // A side-effect of View.onScrollChanged is that the scroll accessibility event being
            // sent by the base class implementation. This is completely hidden from the base
            // classes and cannot be prevented, which is why we need the code below.
            mScrollAccessibilityHelper.removePostedViewScrolledAccessibilityEventCallback();
            mScrollOffsetManager.onContainerViewScrollChanged(l, t);
        }

        @Override
        public void onContainerViewOverScrolled(int scrollX, int scrollY, boolean clampedX,
                boolean clampedY) {
            int oldX = mContainerView.getScrollX();
            int oldY = mContainerView.getScrollY();

            mScrollOffsetManager.onContainerViewOverScrolled(scrollX, scrollY, clampedX, clampedY);

            if (mOverScrollGlow != null) {
                mOverScrollGlow.pullGlow(mContainerView.getScrollX(), mContainerView.getScrollY(),
                        oldX, oldY,
                        mScrollOffsetManager.computeMaximumHorizontalScrollOffset(),
                        mScrollOffsetManager.computeMaximumVerticalScrollOffset());
            }
        }

        @Override
        public int computeHorizontalScrollRange() {
            return mScrollOffsetManager.computeHorizontalScrollRange();
        }

        @Override
        public int computeHorizontalScrollOffset() {
            return mScrollOffsetManager.computeHorizontalScrollOffset();
        }

        @Override
        public int computeVerticalScrollRange() {
            return mScrollOffsetManager.computeVerticalScrollRange();
        }

        @Override
        public int computeVerticalScrollOffset() {
            return mScrollOffsetManager.computeVerticalScrollOffset();
        }

        @Override
        public int computeVerticalScrollExtent() {
            return mScrollOffsetManager.computeVerticalScrollExtent();
        }

        @Override
        public void computeScroll() {
            if (isDestroyedOrNoOperation(NO_WARN)) return;
            nativeOnComputeScroll(mNativeAwContents, AnimationUtils.currentAnimationTimeMillis());
        }

        @Override
        public boolean onCheckIsTextEditor() {
            if (isDestroyedOrNoOperation(NO_WARN)) return false;
            ImeAdapter imeAdapter = ImeAdapter.fromWebContents(mWebContents);
            return imeAdapter != null ? imeAdapter.onCheckIsTextEditor() : false;
        }

        @Override
        public AccessibilityNodeProvider getAccessibilityNodeProvider() {
            if (isDestroyedOrNoOperation(NO_WARN)) return null;
            WebContentsAccessibility wcax = getWebContentsAccessibility();
            return wcax != null ? wcax.getAccessibilityNodeProvider() : null;
        }

        @Override
        public boolean performAccessibilityAction(final int action, final Bundle arguments) {
            if (isDestroyedOrNoOperation(NO_WARN)) return false;
            WebContentsAccessibility wcax = getWebContentsAccessibility();
            return wcax != null ? wcax.performAction(action, arguments) : false;
        }
    }

    // Return true if the GeolocationPermissionAPI should be used.
    @CalledByNative
    private boolean useLegacyGeolocationPermissionAPI() {
        // Always return true since we are not ready to swap the geolocation yet.
        // TODO: If we decide not to migrate the geolocation, there are some unreachable
        // code need to remove. http://crbug.com/396184.
        return true;
    }

    //--------------------------------------------------------------------------------------------
    //  Native methods
    //--------------------------------------------------------------------------------------------

    private static native long nativeInit(AwBrowserContext browserContext);
    private static native void nativeDestroy(long nativeAwContents);
    private static native boolean nativeHasRequiredHardwareExtensions();
    private static native void nativeSetAwDrawSWFunctionTable(long functionTablePointer);
    private static native void nativeSetAwDrawGLFunctionTable(long functionTablePointer);
    private static native int nativeGetNativeInstanceCount();
    private static native void nativeSetShouldDownloadFavicons();
    private static native void nativeUpdateDefaultLocale(String locale, String localeList);
    private static native String nativeGetSafeBrowsingLocaleForTesting();

    private native void nativeEvaluateJavaScriptOnInterstitialForTesting(
            long nativeAwContents, String script, JavaScriptCallback jsCallback);
    private native void nativeSetJavaPeers(long nativeAwContents, AwContents awContents,
            AwWebContentsDelegate webViewWebContentsDelegate,
            AwContentsClientBridge contentsClientBridge, AwContentsIoThreadClient ioThreadClient,
            InterceptNavigationDelegate navigationInterceptionDelegate,
            AutofillProvider autofillProvider);
    private native WebContents nativeGetWebContents(long nativeAwContents);
    private native void nativeSetAwGLFunctor(long nativeAwContents, long nativeAwGLFunctor);

    private native void nativeDocumentHasImages(long nativeAwContents, Message message);
    private native void nativeGenerateMHTML(
            long nativeAwContents, String path, Callback<String> callback);

    private native void nativeAddVisitedLinks(long nativeAwContents, String[] visitedLinks);
    private native void nativeZoomBy(long nativeAwContents, float delta);
    private native void nativeOnComputeScroll(
            long nativeAwContents, long currentAnimationTimeMillis);
    private native boolean nativeOnDraw(long nativeAwContents, Canvas canvas,
            boolean isHardwareAccelerated, int scrollX, int scrollY, int visibleLeft,
            int visibleTop, int visibleRight, int visibleBottom,
            boolean forceAuxiliaryBitmapRendering);
    private native boolean nativeNeedToDrawBackgroundColor(long nativeAwContents);
    private native void nativeFindAllAsync(long nativeAwContents, String searchString);
    private native void nativeFindNext(long nativeAwContents, boolean forward);
    private native void nativeClearMatches(long nativeAwContents);
    private native void nativeClearCache(long nativeAwContents, boolean includeDiskFiles);
    private native void nativeKillRenderProcess(long nativeAwContents);
    private native byte[] nativeGetCertificate(long nativeAwContents);

    // Coordinates are in physical pixels when --use-zoom-for-dsf is enabled.
    // Otherwise, coordinates are in desity independent pixels.
    private native void nativeRequestNewHitTestDataAt(long nativeAwContents, float x, float y,
            float touchMajor);
    private native void nativeUpdateLastHitTestData(long nativeAwContents);

    private native void nativeOnSizeChanged(long nativeAwContents, int w, int h, int ow, int oh);
    private native void nativeScrollTo(long nativeAwContents, int x, int y);
    private native void nativeSmoothScroll(
            long nativeAwContents, int targetX, int targetY, long durationMs);
    private native void nativeSetViewVisibility(long nativeAwContents, boolean visible);
    private native void nativeSetWindowVisibility(long nativeAwContents, boolean visible);
    private native void nativeSetIsPaused(long nativeAwContents, boolean paused);
    private native void nativeOnAttachedToWindow(long nativeAwContents, int w, int h);
    private native void nativeOnDetachedFromWindow(long nativeAwContents);
    private native boolean nativeIsVisible(long nativeAwContents);
    private native void nativeSetDipScale(long nativeAwContents, float dipScale);

    // Returns null if save state fails.
    private native byte[] nativeGetOpaqueState(long nativeAwContents);

    // Returns false if restore state fails.
    private native boolean nativeRestoreFromOpaqueState(long nativeAwContents, byte[] state);

    private native long nativeReleasePopupAwContents(long nativeAwContents);
    private native void nativeFocusFirstNode(long nativeAwContents);
    private native void nativeSetBackgroundColor(long nativeAwContents, int color);

    private native long nativeCapturePicture(long nativeAwContents, int width, int height);
    private native void nativeEnableOnNewPicture(long nativeAwContents, boolean enabled);
    private native void nativeInsertVisualStateCallback(
            long nativeAwContents, long requestId, VisualStateCallback callback);
    private native void nativeClearView(long nativeAwContents);
    private native void nativeSetExtraHeadersForUrl(long nativeAwContents,
            String url, String extraHeaders);

    private native void nativeInvokeGeolocationCallback(
            long nativeAwContents, boolean value, String requestingFrame);
    private native int nativeGetEffectivePriority(long nativeAwContents);

    private native void nativeSetJsOnlineProperty(long nativeAwContents, boolean networkUp);

    private native void nativeTrimMemory(long nativeAwContents, int level, boolean visible);

    private native void nativeCreatePdfExporter(long nativeAwContents, AwPdfExporter awPdfExporter);

    private native void nativePreauthorizePermission(long nativeAwContents, String origin,
            long resources);

    private native void nativeGrantFileSchemeAccesstoChildProcess(long nativeAwContents);
    private native void nativeResumeLoadingCreatedPopupWebContents(long nativeAwContents);

    private native AwRenderProcess nativeGetRenderProcess(long nativeAwContents);
}
