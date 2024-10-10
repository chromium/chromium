// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.ComponentCallbacks2;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Picture;
import android.graphics.Rect;
import android.net.Uri;
import android.net.http.SslCertificate;
import android.os.Build;
import android.os.Bundle;
import android.os.Debug;
import android.os.Handler;
import android.os.Message;
import android.os.SystemClock;
import android.text.TextUtils;
import android.util.Base64;
import android.util.Pair;
import android.util.SparseArray;
import android.view.DragAndDropPermissions;
import android.view.DragEvent;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.PointerIcon;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStructure;
import android.view.Window;
import android.view.WindowManager;
import android.view.accessibility.AccessibilityNodeProvider;
import android.view.animation.AnimationUtils;
import android.view.autofill.AutofillValue;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.textclassifier.TextClassifier;
import android.webkit.JavascriptInterface;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.CalledByNativeUnchecked;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.autofill.AndroidAutofillSafeModeAction;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.common.AwSwitches;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.android_webview.gfx.AwDrawFnImpl;
import org.chromium.android_webview.gfx.AwFunctor;
import org.chromium.android_webview.gfx.AwGLFunctor;
import org.chromium.android_webview.gfx.AwPicture;
import org.chromium.android_webview.gfx.RectUtils;
import org.chromium.android_webview.metrics.AwOriginVisitLogger;
import org.chromium.android_webview.metrics.BackForwardCacheNotRestoredReason;
import org.chromium.android_webview.permission.AwGeolocationCallback;
import org.chromium.android_webview.permission.AwPermissionRequest;
import org.chromium.android_webview.renderer_priority.RendererPriority;
import org.chromium.android_webview.selection.AwSelectionActionMenuDelegate;
import org.chromium.base.BaseFeatures;
import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.LocaleUtils;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TimeUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.jank_tracker.FrameMetricsListener;
import org.chromium.base.jank_tracker.FrameMetricsStore;
import org.chromium.base.jank_tracker.JankReportingScheduler;
import org.chromium.base.jank_tracker.JankScenario;
import org.chromium.base.jank_tracker.JankTracker;
import org.chromium.base.jank_tracker.JankTrackerImpl;
import org.chromium.base.jank_tracker.JankTrackerStateController;
import org.chromium.base.memory.MemoryInfoBridge;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.ScopedSysTraceEvent;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.autofill.AutofillProvider;
import org.chromium.components.autofill.AutofillSelectionMenuItemHelper;
import org.chromium.components.content_capture.OnscreenContentProvider;
import org.chromium.components.embedder_support.util.TouchEventFilter;
import org.chromium.components.embedder_support.util.WebResourceResponseInfo;
import org.chromium.components.navigation_interception.InterceptNavigationDelegate;
import org.chromium.components.sensitive_content.SensitiveContentFeatures;
import org.chromium.components.stylus_handwriting.StylusHandwritingFeatureMap;
import org.chromium.components.stylus_handwriting.StylusWritingController;
import org.chromium.components.stylus_handwriting.StylusWritingSettingsState;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.viz.common.VizFeatures;
import org.chromium.components.zoom.ZoomConstants;
import org.chromium.content_public.browser.ChildProcessImportance;
import org.chromium.content_public.browser.ContentViewStatics;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.ImeEventObserver;
import org.chromium.content_public.browser.JavaScriptCallback;
import org.chromium.content_public.browser.JavascriptInjector;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.MessagePayload;
import org.chromium.content_public.browser.MessagePort;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.SelectionClient;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.SmartClipProvider;
import org.chromium.content_public.browser.ViewEventSink;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsAccessibility;
import org.chromium.content_public.browser.WebContentsInternals;
import org.chromium.content_public.browser.navigation_controller.LoadURLType;
import org.chromium.content_public.browser.navigation_controller.UserAgentOverrideOption;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.content_public.common.Referrer;
import org.chromium.device.gamepad.GamepadList;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.network.mojom.ReferrerPolicy;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid.DisplayAndroidObserver;
import org.chromium.url.GURL;

import java.io.File;
import java.lang.annotation.Annotation;
import java.lang.ref.WeakReference;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.WeakHashMap;
import java.util.concurrent.Callable;
import java.util.function.BiFunction;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Exposes the native AwContents class, and together these classes wrap the WebContents and Browser
 * components that are required to implement Android WebView API. This is the primary entry point
 * for the WebViewProvider implementation; it holds a 1:1 object relationship with application
 * WebView instances. (We define this class independent of the hidden WebViewProvider interfaces, to
 * allow continuous build &amp; test in the open source SDK-based tree).
 */
@Lifetime.WebView
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

    private static int sLastId;
    // Unique id given to each AwContents object, starting from 1.
    private final int mId;

    @VisibleForTesting
    public static final String LOAD_URL_SCHEME_HISTOGRAM_NAME = "Android.WebView.LoadUrl.UrlScheme";

    // Permit any number of slashes, since chromium seems to canonicalize bad values.
    private static final Pattern sFileAndroidAssetPattern =
            Pattern.compile("^file:/*android_(asset|res).*");

    // Matches a data URL that (may) have a valid fragment selector, pulling the fragment selector
    // out into a group. Such a URL must contain a single '#' character and everything after that
    // must be a valid DOM id.
    // DOM id grammar: https://www.w3.org/TR/1999/REC-html401-19991224/types.html#type-name
    private static final Pattern sDataURLWithSelectorPattern =
            Pattern.compile("^[^#]*(#[A-Za-z][A-Za-z0-9\\-_:.]*)$");

    private static final String CONSTRUCTOR_HISTOGRAM_NAME =
            "Android.WebView.AwContentsConstructorTime";

    private static class ForceAuxiliaryBitmapRendering {
        private static final boolean sResult = lazyCheck();

        private static boolean lazyCheck() {
            return !AwContentsJni.get().hasRequiredHardwareExtensions();
        }
    }

    // Used to record the UMA histogram Android.WebView.LoadDataWithBaseUrl.UrlScheme. Since these
    // values are persisted to logs, they should never be renumbered or reused.
    @VisibleForTesting
    @IntDef({
        UrlScheme.EMPTY,
        UrlScheme.UNKNOWN_SCHEME,
        UrlScheme.HTTP_SCHEME,
        UrlScheme.HTTPS_SCHEME,
        UrlScheme.FILE_SCHEME,
        UrlScheme.FTP_SCHEME,
        UrlScheme.DATA_SCHEME,
        UrlScheme.JAVASCRIPT_SCHEME,
        UrlScheme.ABOUT_SCHEME,
        UrlScheme.CHROME_SCHEME,
        UrlScheme.BLOB_SCHEME,
        UrlScheme.CONTENT_SCHEME,
        UrlScheme.INTENT_SCHEME,
        UrlScheme.FILE_ANDROID_ASSET_SCHEME
    })
    public @interface UrlScheme {
        int EMPTY = 0;
        int UNKNOWN_SCHEME = 1;
        int HTTP_SCHEME = 2;
        int HTTPS_SCHEME = 3;
        int FILE_SCHEME = 4;
        int FTP_SCHEME = 5;
        int DATA_SCHEME = 6;
        int JAVASCRIPT_SCHEME = 7;
        int ABOUT_SCHEME = 8;
        int CHROME_SCHEME = 9;
        int BLOB_SCHEME = 10;
        int CONTENT_SCHEME = 11;
        int INTENT_SCHEME = 12;
        int FILE_ANDROID_ASSET_SCHEME = 13; // Covers android_asset and android_res URLs
        int COUNT = 14;
    }

    // Used to record Android.WebView.UsedInPopupWindow.
    @IntDef({
        UsedInPopupWindow.NOT_IN_POPUP_WINDOW,
        UsedInPopupWindow.IN_POPUP_WINDOW,
        UsedInPopupWindow.UNKNOWN,
    })
    public @interface UsedInPopupWindow {
        int NOT_IN_POPUP_WINDOW = 0;
        int IN_POPUP_WINDOW = 1;
        int UNKNOWN = 2;
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
        /** @see View#overScrollBy(int, int, int, int, int, int, int, int, boolean); */
        void overScrollBy(
                int deltaX,
                int deltaY,
                int scrollX,
                int scrollY,
                int scrollRangeX,
                int scrollRangeY,
                int maxOverScrollX,
                int maxOverScrollY,
                boolean isTouchEvent);

        /** @see View#scrollTo(int, int) */
        void super_scrollTo(int scrollX, int scrollY);

        /** @see View#setMeasuredDimension(int, int) */
        void setMeasuredDimension(int measuredWidth, int measuredHeight);

        /** @see View#getScrollBarStyle() */
        int super_getScrollBarStyle();

        /** @see View#startActivityForResult(Intent, int) */
        void super_startActivityForResult(Intent intent, int requestCode);

        /** @see View#onConfigurationChanged(Configuration) */
        void super_onConfigurationChanged(Configuration newConfig);
    }

    /**
     * Factory interface used for constructing functors that the Android framework uses for
     * calling back into Chromium code to render the the contents of a Chromium frame into
     * an Android view.
     */
    public interface NativeDrawFunctorFactory {
        /** Create a GL functor associated with native context |context|. */
        NativeDrawGLFunctor createGLFunctor(long context);

        /**
         * Used for draw_fn functor. Only one of these methods need to return non-null.
         * Prefer this over createGLFunctor.
         */
        AwDrawFnImpl.DrawFnAccess getDrawFnAccess();
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
         * accelerated. |releasedCallback| should be null if |canvas| is null.
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

        /** Detaches the GLFunctor from the view tree. */
        void detach(View containerView);

        /**
         * Destroy this functor instance and any native objects associated with it. No method is
         * called after destroy.
         */
        void destroy();
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
    private AwBrowserContext mBrowserContext;
    private ViewGroup mContainerView;
    private AwFunctor mDrawFunctor;
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
    private final NativeDrawFunctorFactory mNativeDrawFunctorFactory;
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
    private long mPreferredFrameIntervalNanos;

    // Visibility state of |mWebContents|.
    private boolean mIsContentVisible;
    private boolean mIsUpdateVisibilityTaskPending;
    private Runnable mUpdateVisibilityRunnable;

    /**
     * Set to true if there is ever a call to {@link AwContents#getBrowserContext()}.
     * This flag is primarily used to prevent setting a new browser context via. {@link
     * AwContents#setBrowserContext(AwBrowserContext)} after it has been retrieved externally.
     */
    private boolean mBrowserContextAccessed;

    /**
     * Set to true if the browser context has ever been set explicitly via.
     * {@link AwContents#setBrowserContext(AwBrowserContext)}.
     */
    private boolean mBrowserContextSetExplicitly;

    /**
     * Set to true if {@link AwContents#evaluateJavaScript(String, Callback)}
     * has been called.
     */
    private boolean mHasEvaluatedJavascript;

    @VisibleForTesting public static final long FUNCTOR_RECLAIM_DELAY_MS = 10000;
    @VisibleForTesting public static final long METRICS_COLLECTION_DELAY_MS = 1000;
    private static final long CURRENTLY_VISIBLE = -1;
    private long mLastWindowVisibleTime = -1;
    private boolean mHasPendingReclaimTask;
    private BiFunction<Runnable, Long, Void> mPostDelayedTaskForTesting;
    private static final long MEMORY_COLLECTION_INTERVAL_MS = 5 * 60 * 1000;
    private static long sLastCollectionTime = -MEMORY_COLLECTION_INTERVAL_MS;

    @VisibleForTesting
    public static final String PSS_HISTOGRAM = "Android.WebView.Memory.FunctorReclaim.OtherPss";

    @VisibleForTesting
    public static final String PRIVATE_DIRTY_HISTOGRAM =
            "Android.WebView.Memory.FunctorReclaim.OtherPrivateDirty";

    private @RendererPriority int mRendererPriority;
    private boolean mRendererPriorityWaivedWhenNotVisible;

    private Bitmap mFavicon;
    private boolean mHasRequestedVisitedHistoryFromClient;
    // Whether this WebView is a popup.
    private boolean mIsPopupWindow;

    // The base background color, i.e. not accounting for any CSS body from the current page.
    private int mBaseBackgroundColor = Color.WHITE;

    // Did background set by developer, now used for dark mode.
    private boolean mDidInitBackground;

    // Must call AwContentsJni.get().updateLastHitTestData first to update this before use.
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

    private AwPdfExporter mAwPdfExporter;

    private AwViewMethods mAwViewMethods;
    private final FullScreenTransitionsState mFullScreenTransitionsState;

    // This is a workaround for some qualcomm devices discarding buffer on
    // Activity restore.
    private boolean mInvalidateRootViewOnNextDraw;

    // The framework may temporarily detach our container view, for example during layout if
    // we are a child of a ListView. This may cause many toggles of View focus, which we suppress
    // when in this state.
    private boolean mTemporarilyDetached;

    // True when this AwContents has been destroyed.
    // Do not use directly, call isDestroyed() instead.
    private boolean mIsDestroyed;

    private AutofillProvider mAutofillProvider;

    private static String sCurrentLocales = "";

    // A holder of objects passed from WebContents and should be owned by AwContents that may
    // have direct or indirect reference back to WebView. They are used internally by
    // WebContents but all the references can create a new gc root that can keep WebView
    // instances from being freed when they are detached from view tree, hence lead to
    // memory leak. To avoid the issue, it is possible to use |WebContents.setInternalHolder|
    // to move the holder of those internal objects to AwContents. Note that they are still
    // used by WebContents, and AwContents doesn't have to know what's inside the holder.
    private WebContentsInternals mWebContentsInternals;

    private JavascriptInjector mJavascriptInjector;

    private OnscreenContentProvider mOnscreenContentProvider;

    private AwDisplayCutoutController mDisplayCutoutController;
    private final AwDisplayModeController mDisplayModeController;
    private final Rect mCachedSafeAreaRect = new Rect();

    // The current AwWindowCoverageTracker, if any. This will be non-null when the AwContents is
    // attached to the Window and size tracking is enabled. It will be null otherwise.
    private AwWindowCoverageTracker mAwWindowCoverageTracker;

    private AwFrameMetricsListener mAwFrameMetricsListener;

    private AwDarkMode mAwDarkMode;
    private AwWebContentsMetricsRecorder mAwWebContentsMetricsRecorder;

    private StylusWritingController mStylusWritingController;

    // Permissions are requested on a drop event, and are released when another drag starts
    // (drag-started event) or when the current page navigates to a new URL.
    private DragAndDropPermissions mDragAndDropPermissions;

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
            mWindowAndroid.incrementRefFromDestroyRunnable();
        }

        @Override
        public void run() {
            AwContentsJni.get().destroy(mNativeAwContents);
            mWindowAndroid.decrementRefFromDestroyRunnable();
        }
    }

    /** A class that stores the state needed to enter and exit fullscreen. */
    private static class FullScreenTransitionsState {
        private final ViewGroup mInitialContainerView;
        private final InternalAccessDelegate mInitialInternalAccessAdapter;
        private final AwViewMethods mInitialAwViewMethods;
        private FullScreenView mFullScreenView;

        /** Whether the initial container view was focused when we entered fullscreen */
        private boolean mWasInitialContainerViewFocused;

        private int mScrollX;
        private int mScrollY;

        private FullScreenTransitionsState(
                ViewGroup initialContainerView,
                InternalAccessDelegate initialInternalAccessAdapter,
                AwViewMethods initialAwViewMethods) {
            mInitialContainerView = initialContainerView;
            mInitialInternalAccessAdapter = initialInternalAccessAdapter;
            mInitialAwViewMethods = initialAwViewMethods;
        }

        private void enterFullScreen(
                FullScreenView fullScreenView,
                boolean wasInitialContainerViewFocused,
                int scrollX,
                int scrollY) {
            mFullScreenView = fullScreenView;
            mWasInitialContainerViewFocused = wasInitialContainerViewFocused;
            mScrollX = scrollX;
            mScrollY = scrollY;
        }

        private boolean wasInitialContainerViewFocused() {
            return mWasInitialContainerViewFocused;
        }

        private int getScrollX() {
            return mScrollX;
        }

        private int getScrollY() {
            return mScrollY;
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

    // --------------------------------------------------------------------------------------------
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
        public boolean shouldBlockSpecialFileUrls() {
            return mSettings.getBlockSpecialFileUrls();
        }

        @Override
        public boolean shouldBlockNetworkLoads() {
            return mSettings.getBlockNetworkLoads();
        }

        @Override
        public boolean shouldAcceptCookies() {
            return mBrowserContext.getCookieManager().acceptCookie();
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
        public WebResourceResponseInfo shouldInterceptRequest(
                AwContentsClient.AwWebResourceRequest request) {
            String url = request.url;
            WebResourceResponseInfo webResourceResponseInfo;
            // Return the response directly if the url is default video poster url.
            webResourceResponseInfo = mDefaultVideoPosterRequestHandler.shouldInterceptRequest(url);
            if (webResourceResponseInfo != null) return webResourceResponseInfo;

            webResourceResponseInfo = mContentsClient.shouldInterceptRequest(request);

            if (webResourceResponseInfo == null) {
                mContentsClient.getCallbackHelper().postOnLoadResource(url);
            }

            if (webResourceResponseInfo != null && webResourceResponseInfo.getData() == null) {
                // In this case the intercepted URLRequest job will simulate an empty response
                // which doesn't trigger the onReceivedError callback. For WebViewClassic
                // compatibility we synthesize that callback.  http://crbug.com/180950
                mContentsClient
                        .getCallbackHelper()
                        .postOnReceivedError(
                                request,
                                /* error description filled in by the glue layer */
                                new AwContentsClient.AwWebResourceError());
            }
            return webResourceResponseInfo;
        }
    }

    // --------------------------------------------------------------------------------------------
    // When the navigation is for a newly created WebView (i.e. a popup), intercept the navigation
    // here for implementing shouldOverrideUrlLoading. This is to send the shouldOverrideUrlLoading
    // callback to the correct WebViewClient that is associated with the WebView.
    // Otherwise, use this delegate only to post onPageStarted messages.
    //
    // We are not using WebContentsObserver.didStartLoading because of stale URLs, out of order
    // onPageStarted's and double onPageStarted's.
    //
    private class InterceptNavigationDelegateImpl extends InterceptNavigationDelegate {
        @Override
        public boolean shouldIgnoreNavigation(
                NavigationHandle navigationHandle,
                GURL escapedUrl,
                boolean hiddenCrossFrame,
                boolean isSandboxedFrame) {
            // The shouldOverrideUrlLoading call might have resulted in posting messages to the
            // UI thread. Using sendMessage here (instead of calling onPageStarted directly)
            // will allow those to run in order.
            if (!AwComputedFlags.pageStartedOnCommitEnabled(
                    navigationHandle.isRendererInitiated())) {
                GURL url =
                        navigationHandle.getBaseUrlForDataUrl().isEmpty()
                                ? navigationHandle.getUrl()
                                : navigationHandle.getBaseUrlForDataUrl();
                mContentsClient.getCallbackHelper().postOnPageStarted(url.getPossiblyInvalidSpec());
            }
            return false;
        }
    }

    // --------------------------------------------------------------------------------------------
    private class AwLayoutSizerDelegate implements AwLayoutSizer.Delegate {
        @Override
        public void requestLayout() {
            ViewUtils.requestLayout(
                    mContainerView, "AwContents.AwLayoutSizerDelegate.requestLayout");
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

    // --------------------------------------------------------------------------------------------
    private class AwScrollOffsetManagerDelegate implements AwScrollOffsetManager.Delegate {
        @Override
        public void overScrollContainerViewBy(
                int deltaX,
                int deltaY,
                int scrollX,
                int scrollY,
                int scrollRangeX,
                int scrollRangeY,
                boolean isTouchEvent) {
            mInternalAccessAdapter.overScrollBy(
                    deltaX,
                    deltaY,
                    scrollX,
                    scrollY,
                    scrollRangeX,
                    scrollRangeY,
                    0,
                    0,
                    isTouchEvent);
        }

        @Override
        public void scrollContainerViewTo(int x, int y) {
            try {
                mInternalAccessAdapter.super_scrollTo(x, y);
            } catch (Throwable e) {
                AwThreadUtils.postToCurrentLooper(
                        () -> {
                            Log.e(
                                    TAG,
                                    "The following exception was raised by scrollContainerViewTo:");
                            throw e;
                        });
            }
        }

        @Override
        public void scrollNativeTo(int x, int y) {
            if (!isDestroyed(NO_WARN)) {
                AwContentsJni.get().scrollTo(mNativeAwContents, x, y);
            }
        }

        @Override
        public void smoothScroll(int targetX, int targetY, long durationMs) {
            if (!isDestroyed(NO_WARN)) {
                AwContentsJni.get().smoothScroll(mNativeAwContents, targetX, targetY, durationMs);
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
            mContainerView.postInvalidateOnAnimation();
        }

        @Override
        public void cancelFling() {
            mWebContents.getEventForwarder().cancelFling(SystemClock.uptimeMillis());
        }
    }

    // --------------------------------------------------------------------------------------------
    private class AwGestureStateListener extends GestureStateListener {
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
            if (!AwFeatureMap.isEnabled(
                    AwFeatures.WEBVIEW_DO_NOT_SEND_ACCESSIBILITY_EVENTS_ON_GSU)) {
                mScrollAccessibilityHelper.postViewScrolledAccessibilityEventCallback();
            }
            if (AwFeatureMap.isEnabled(AwFeatures.WEBVIEW_INVOKE_ZOOM_PICKER_ON_GSU)) {
                mZoomControls.invokeZoomPicker();
            }
        }

        @Override
        public void onScrollStarted(int scrollOffsetY, int scrollExtentY, boolean isDirectionUp) {
            if (!AwFeatureMap.isEnabled(AwFeatures.WEBVIEW_INVOKE_ZOOM_PICKER_ON_GSU)) {
                // This needs to be paired with call to setAutoDismissed(true) and a call to invoke
                // zoom picker, so that a delayed hide task is posted by android. This is happening
                // on scroll end below.
                mZoomControls.setAutoDismissed(false);
            }
            mZoomControls.invokeZoomPicker();
            if (mAwFrameMetricsListener != null) {
                mAwFrameMetricsListener.onWebContentsScrollStateUpdate(
                        /* isScrolling= */ true, mId);
            }
            if (AwFeatureMap.isEnabled(
                    AwFeatures.WEBVIEW_DO_NOT_SEND_ACCESSIBILITY_EVENTS_ON_GSU)) {
                mScrollAccessibilityHelper.setIsInAScroll(true);
            }
        }

        @Override
        public void onScrollEnded(int scrollOffsetY, int scrollExtentY) {
            if (!AwFeatureMap.isEnabled(AwFeatures.WEBVIEW_INVOKE_ZOOM_PICKER_ON_GSU)) {
                mZoomControls.setAutoDismissed(true);
                // A call to invoke is required so that a delayed hide task can be posted by
                // android.
                mZoomControls.invokeZoomPicker();
            }
            if (mAwFrameMetricsListener != null) {
                mAwFrameMetricsListener.onWebContentsScrollStateUpdate(
                        /* isScrolling= */ false, mId);
            }
            if (AwFeatureMap.isEnabled(
                    AwFeatures.WEBVIEW_DO_NOT_SEND_ACCESSIBILITY_EVENTS_ON_GSU)) {
                mScrollAccessibilityHelper.setIsInAScroll(false);
            }
        }

        @Override
        public void onScaleLimitsChanged(float minPageScaleFactor, float maxPageScaleFactor) {
            mZoomControls.updateZoomControls();
        }
    }

    // --------------------------------------------------------------------------------------------
    private class AwComponentCallbacks implements ComponentCallbacks2 {
        @Override
        public void onTrimMemory(final int level) {
            AwContents.this.onTrimMemory(level);
        }

        @Override
        public void onLowMemory() {}

        @Override
        public void onConfigurationChanged(Configuration configuration) {
            updateDefaultLocale();
        }
    }
    ;

    // --------------------------------------------------------------------------------------------
    private class AwDisplayAndroidObserver implements DisplayAndroidObserver {
        @Override
        public void onRotationChanged(int rotation) {}

        @Override
        public void onDIPScaleChanged(float dipScale) {
            if (TRACE) Log.i(TAG, "%s onDIPScaleChanged dipScale=%f", this, dipScale);

            AwContentsJni.get().setDipScale(mNativeAwContents, dipScale);
            mLayoutSizer.setDIPScale(dipScale);
            mSettings.setDIPScale(dipScale);
        }
    }
    ;

    /** Tracks and reports the percentage of coverage of AwContents on the root view. */
    @VisibleForTesting
    public static class AwWindowCoverageTracker {
        private static final long RECALCULATION_DELAY_MS = 200;

        @VisibleForTesting
        public static final Map<View, AwWindowCoverageTracker> sWindowCoverageTrackers =
                new HashMap<>();

        private final View mRootView;
        private List<AwContents> mAwContentsList = new ArrayList<>();
        private long mRecalculationTime;
        private boolean mPendingRecalculation;

        private AwWindowCoverageTracker(View rootView) {
            mRootView = rootView;

            sWindowCoverageTrackers.put(rootView, this);
        }

        public static AwWindowCoverageTracker getOrCreateForRootView(
                AwContents contents, View rootView) {
            AwWindowCoverageTracker tracker = sWindowCoverageTrackers.get(rootView);

            if (tracker == null) {
                if (TRACE) {
                    Log.i(TAG, "%s creating WindowCoverageTracker for %s", contents, rootView);
                }

                tracker = new AwWindowCoverageTracker(rootView);
            }

            return tracker;
        }

        public void trackContents(AwContents contents) {
            contents.mAwWindowCoverageTracker = this;
            mAwContentsList.add(contents);
        }

        public void untrackContents(AwContents contents) {
            contents.mAwWindowCoverageTracker = null;
            mAwContentsList.remove(contents);

            // If that was the last AwContents, remove ourselves from the static map.
            if (!isTracking()) {
                if (TRACE) Log.i(TAG, "%s removing " + this, contents);
                sWindowCoverageTrackers.remove(mRootView);
            }
        }

        private boolean isTracking() {
            return mAwContentsList.size() > 0;
        }

        /**
         * Notifies this object that a recalculation of the window coverage is necessary.
         *
         * This should be called every time any of the tracked AwContents changes its size,
         * visibility, or scheme.
         *
         * Recalculation won't happen immediately, and will be rate limited.
         */
        public void onInputsUpdated() {
            long time = SystemClock.uptimeMillis();

            if (mPendingRecalculation) return;
            mPendingRecalculation = true;

            if (time > mRecalculationTime + RECALCULATION_DELAY_MS) {
                // Enough time has elapsed since the last recalculation, run it now.
                mRecalculationTime = time;
            } else {
                // Not enough time has elapsed, run it once enough time has elapsed.
                mRecalculationTime += RECALCULATION_DELAY_MS;
            }

            PostTask.postDelayedTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        recalculate();
                        mPendingRecalculation = false;
                    },
                    mRecalculationTime - time);
        }

        private static int[] toIntArray(List<Integer> list) {
            int[] array = new int[list.size()];
            for (int i = 0; i < list.size(); i++) {
                array[i] = list.get(i);
            }
            return array;
        }

        private void recalculate() {
            if (TRACE) Log.i(TAG, "%s recalculate", this);

            List<Rect> contentRects = new ArrayList<>();

            Rect rootVisibleRect =
                    new Rect(
                            (int) mRootView.getX(),
                            (int) mRootView.getY(),
                            (int) mRootView.getX() + mRootView.getWidth(),
                            (int) mRootView.getY() + mRootView.getHeight());
            int rootArea = RectUtils.getRectArea(rootVisibleRect);

            int globalPercentage = 0;

            // Note that a scheme could occur more than once at a time.
            List<String> schemes = new ArrayList<>();
            List<Integer> schemePercentages = new ArrayList<>();

            // If the root view has a width or height of 0 then nothing is visible, so leave the
            // lists empty and pass them on like that. Also, we don't want to divide by 0.
            if (rootArea > 0) {
                for (AwContents content : mAwContentsList) {
                    // A workaround for a deeper problem: https://crbug.com/1232765#c19
                    if (content.isDestroyed(NO_WARN)) continue;
                    if (content.mIsAttachedToWindow
                            && content.mIsViewVisible
                            && content.mIsWindowVisible) {
                        // The result of getGlobalVisibleRect can change underneath us, so take a
                        // protective copy.
                        Rect contentRect = new Rect(content.getGlobalVisibleRect());

                        // If the intersect method returns true then it may have modified
                        // contentRect. A Rect with area 0 will not intersect with anything.
                        if (contentRect.intersect(rootVisibleRect)) {
                            contentRects.add(contentRect);
                            schemes.add(AwContentsJni.get().getScheme(content.mNativeAwContents));
                            schemePercentages.add(
                                    RectUtils.getRectArea(contentRect) * 100 / rootArea);
                        }
                    }
                }

                globalPercentage =
                        RectUtils.calculatePixelsOfCoverage(rootVisibleRect, contentRects)
                                * 100
                                / rootArea;
            }

            AwContentsJni.get()
                    .updateScreenCoverage(
                            globalPercentage,
                            schemes.toArray(new String[schemes.size()]),
                            toIntArray(schemePercentages));
        }
    }

    // A Webview class that implements the listener part of the JankTracker requirement. It mirrors
    // JankActivityTracker in starting and stopping the listener and collection.
    private static class AwFrameMetricsListener {
        private static final WeakHashMap<Window, AwFrameMetricsListener> sWindowMap =
                new WeakHashMap<>();

        private boolean mAttached;
        private JankTrackerStateController mController;
        private JankTracker mJankTracker;
        private WeakReference<Window> mWindow;
        private int mAttachedWebviews;
        private int mVisibleWebviews;

        private static final WeakHashMap<Window, Integer> sNumActiveScrolls = new WeakHashMap<>();

        private AwFrameMetricsListener() {
            FrameMetricsStore metricsStore = new FrameMetricsStore();
            mController =
                    new JankTrackerStateController(
                            new FrameMetricsListener(metricsStore),
                            new JankReportingScheduler(metricsStore));
            mJankTracker = new JankTrackerImpl(mController);
            mAttached = false;
        }

        private void attachListener(Window window) {
            if (mAttached) return;
            mWindow = new WeakReference<Window>(window);
            mController.startMetricCollection(window);
            mAttached = true;
        }

        private void detachListener(Window window) {
            if (!mAttached || window != mWindow.get()) return;
            mController.stopMetricCollection(window);
            mAttached = false;
        }

        private void incrementAttachedWebviews() {
            mAttachedWebviews++;
        }

        private void decrementAttachedWebviews() {
            mAttachedWebviews--;
            assert mAttachedWebviews >= 0;
        }

        private int getAttachedWebviews() {
            return mAttachedWebviews;
        }

        public static AwFrameMetricsListener onAttachedToWindow(Window window) {
            AwFrameMetricsListener listener = sWindowMap.get(window);
            if (listener == null) {
                listener = new AwFrameMetricsListener();
                listener.attachListener(window);
                sWindowMap.put(window, listener);
            }
            listener.incrementAttachedWebviews();
            return listener;
        }

        public static void onDetachedFromWindow(Window window) {
            AwFrameMetricsListener listener = sWindowMap.get(window);
            listener.decrementAttachedWebviews();
            if (listener.getAttachedWebviews() >= 1) return;
            listener.detachListener(window);
            sWindowMap.remove(window);
        }

        public void onWebviewVisible() {
            if (!mAttached) return;
            mVisibleWebviews++;
            if (mVisibleWebviews > 1) return;
            mController.startPeriodicReporting();
            mController.startMetricCollection(null);
        }

        public void onWebviewHidden() {
            if (!mAttached) return;
            mVisibleWebviews--;
            assert mVisibleWebviews >= 0;
            if (mVisibleWebviews == 0) {
                mController.stopMetricCollection(null);
                mController.stopPeriodicReporting();
            }
        }

        public void onWebContentsScrollStateUpdate(boolean isScrolling, long scrollId) {
            if (!mAttached) return;
            // scrollIds are unique across multiple webviews in a window.
            Window window = mWindow.get();
            if (window == null) return;
            int numActiveScrolls = sNumActiveScrolls.getOrDefault(window, 0);
            if (isScrolling) {
                numActiveScrolls += 1;
                mJankTracker.startTrackingScenario(
                        new JankScenario(JankScenario.Type.WEBVIEW_SCROLLING, scrollId));
            } else {
                assert numActiveScrolls >= 1;
                numActiveScrolls -= 1;
                mJankTracker.finishTrackingScenario(
                        new JankScenario(JankScenario.Type.WEBVIEW_SCROLLING, scrollId),
                        TimeUtils.uptimeMillis() * TimeUtils.NANOSECONDS_PER_MILLISECOND);
            }

            if (numActiveScrolls == 0) {
                mJankTracker.finishTrackingScenario(
                        JankScenario.COMBINED_WEBVIEW_SCROLLING,
                        TimeUtils.uptimeMillis() * TimeUtils.NANOSECONDS_PER_MILLISECOND);
                sNumActiveScrolls.remove(window);
                return;
            }
            if (numActiveScrolls == 1 && isScrolling) {
                mJankTracker.startTrackingScenario(JankScenario.COMBINED_WEBVIEW_SCROLLING);
            }
            sNumActiveScrolls.put(window, numActiveScrolls);
        }
    }

    // --------------------------------------------------------------------------------------------
    /**
     * @param browserContext the browsing context to associate this view contents with.
     * @param containerView the view-hierarchy item this object will be bound to.
     * @param context the context to use, usually containerView.getContext().
     * @param internalAccessAdapter to access private methods on containerView.
     * @param nativeDrawFunctorFactory to access the functor provided by the WebView.
     * @param contentsClient will receive API callbacks from this WebView Contents.
     * @param awSettings AwSettings instance used to configure the AwContents.
     *
     * This constructor uses the default view sizing policy.
     */
    public AwContents(
            AwBrowserContext browserContext,
            ViewGroup containerView,
            Context context,
            InternalAccessDelegate internalAccessAdapter,
            NativeDrawFunctorFactory nativeDrawFunctorFactory,
            AwContentsClient contentsClient,
            AwSettings awSettings) {
        this(
                browserContext,
                containerView,
                context,
                internalAccessAdapter,
                nativeDrawFunctorFactory,
                contentsClient,
                awSettings,
                new DependencyFactory());
    }

    /**
     * @param dependencyFactory an instance of the DependencyFactory used to provide instances of
     *     classes that this class depends on.
     *     <p>This version of the constructor is used in test code to inject test versions of the
     *     above documented classes.
     */
    public AwContents(
            AwBrowserContext browserContext,
            ViewGroup containerView,
            Context context,
            InternalAccessDelegate internalAccessAdapter,
            NativeDrawFunctorFactory nativeDrawFunctorFactory,
            AwContentsClient contentsClient,
            AwSettings settings,
            DependencyFactory dependencyFactory) {
        assert browserContext != null;
        long startTime = SystemClock.uptimeMillis();
        sLastId += 1;
        mId = sLastId;
        if (!browserContext.isDefaultAwBrowserContext()) {
            // The browser context has been explicitly set by the application.
            mBrowserContextSetExplicitly = true;
        }
        try (ScopedSysTraceEvent e1 = ScopedSysTraceEvent.scoped("AwContents.constructor")) {
            mDisplayModeController =
                    new AwDisplayModeController(
                            new AwDisplayModeController.Delegate() {
                                @Override
                                public int getDisplayWidth() {
                                    WindowAndroid windowAndroid = mWindowAndroid.getWindowAndroid();
                                    return windowAndroid.getDisplay().getDisplayWidth();
                                }

                                @Override
                                public int getDisplayHeight() {
                                    WindowAndroid windowAndroid = mWindowAndroid.getWindowAndroid();
                                    return windowAndroid.getDisplay().getDisplayHeight();
                                }
                            },
                            containerView);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P
                    && AwFeatureMap.isEnabled(AwFeatures.WEBVIEW_DISPLAY_CUTOUT)) {
                mDisplayCutoutController =
                        new AwDisplayCutoutController(
                                new AwDisplayCutoutController.Delegate() {
                                    @Override
                                    public float getDipScale() {
                                        WindowAndroid windowAndroid =
                                                mWindowAndroid.getWindowAndroid();
                                        return windowAndroid.getDisplay().getDipScale();
                                    }

                                    @Override
                                    public void setDisplayCutoutSafeArea(
                                            AwDisplayCutoutController.Insets insets) {
                                        if (mWebContents == null) return;
                                        mWebContents.setDisplayCutoutSafeArea(
                                                insets.toRect(mCachedSafeAreaRect));
                                    }
                                },
                                containerView);
            }
            mRendererPriority = RendererPriority.HIGH;
            mSettings = settings;
            updateDefaultLocale();

            mBrowserContext = browserContext;

            // setWillNotDraw(false) is required since WebView draws its own contents using its
            // container view. If this is ever not the case we should remove this, as it removes
            // Android's gatherTransparentRegion optimization for the view.
            mContainerView = containerView;
            mContainerView.setWillNotDraw(false);

            mContext = context;
            mAppTargetSdkVersion = mContext.getApplicationInfo().targetSdkVersion;
            mInternalAccessAdapter = internalAccessAdapter;
            mNativeDrawFunctorFactory = nativeDrawFunctorFactory;
            mContentsClient = contentsClient;
            mContentsClient
                    .getCallbackHelper()
                    .setCancelCallbackPoller(() -> AwContents.this.isDestroyed(NO_WARN));
            mAwViewMethods = new AwViewMethodsImpl();
            mFullScreenTransitionsState =
                    new FullScreenTransitionsState(
                            mContainerView, mInternalAccessAdapter, mAwViewMethods);
            mLayoutSizer = dependencyFactory.createLayoutSizer();
            mLayoutSizer.setDelegate(new AwLayoutSizerDelegate());
            mWebContentsDelegate =
                    new AwWebContentsDelegateAdapter(
                            this, contentsClient, settings, mContext, mContainerView);
            mContentsClientBridge =
                    new AwContentsClientBridge(
                            mContext, contentsClient, AwContentsStatics.getClientCertLookupTable());
            mZoomControls = new AwZoomControls(this);
            mBackgroundThreadClient = new BackgroundThreadClientImpl();
            mIoThreadClient = new IoThreadClientImpl();
            mInterceptNavigationDelegate = new InterceptNavigationDelegateImpl();
            mDisplayObserver = new AwDisplayAndroidObserver();
            mUpdateVisibilityRunnable = () -> updateWebContentsVisibility();

            AwSettings.ZoomSupportChangeListener zoomListener =
                    (supportsDoubleTapZoom, supportsMultiTouchZoom) -> {
                        if (isDestroyed(NO_WARN)) return;
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
            mScrollOffsetManager =
                    dependencyFactory.createScrollOffsetManager(
                            new AwScrollOffsetManagerDelegate());
            mScrollAccessibilityHelper = new ScrollAccessibilityHelper(mContainerView);

            setOverScrollMode(mContainerView.getOverScrollMode());
            setScrollBarStyle(mInternalAccessAdapter.super_getScrollBarStyle());

            mAwDarkMode = new AwDarkMode(context);
            mStylusWritingController =
                    new StylusWritingController(
                            context,
                            AwFeatureMap.isEnabled(
                                    AwFeatures.WEBVIEW_LAZY_FETCH_HAND_WRITING_ICON));

            setNewAwContents(
                    AwContentsJni.get().init(mBrowserContext.getNativeBrowserContextPointer()));

            onContainerViewChanged();
        }
        long delta = SystemClock.uptimeMillis() - startTime;
        RecordHistogram.recordTimesHistogram(CONSTRUCTOR_HISTOGRAM_NAME, delta);
        if (mId == 1) {
            RecordHistogram.recordTimesHistogram(CONSTRUCTOR_HISTOGRAM_NAME + ".First", delta);
        }
    }

    private void initWebContents(
            ViewAndroidDelegate viewDelegate,
            InternalAccessDelegate internalDispatcher,
            WebContents webContents,
            WindowAndroid windowAndroid,
            WebContentsInternalsHolder internalsHolder,
            AwSelectionActionMenuDelegate selectionActionMenuDelegate) {
        webContents.setDelegates(
                PRODUCT_VERSION, viewDelegate, internalDispatcher, windowAndroid, internalsHolder);
        mViewEventSink = ViewEventSink.from(mWebContents);
        mViewEventSink.setHideKeyboardOnBlur(false);
        SelectionPopupController controller = SelectionPopupController.fromWebContents(webContents);
        controller.setActionModeCallback(new AwActionModeCallback(mContext, this, webContents));
        controller.setSelectionClient(SelectionClient.createSmartSelectionClient(webContents));
        controller.setSelectionActionMenuDelegate(selectionActionMenuDelegate);
        AwSelectionDropdownMenuDelegate.maybeSetWebViewDropdownSelectionMenuDelegate(controller);

        // Listen for dpad events from IMEs (e.g. Samsung Cursor Control) so we know to enable
        // spatial navigation mode to allow these events to move focus out of the WebView.
        ImeAdapter.fromWebContents(webContents)
                .addEventObserver(
                        new ImeEventObserver() {
                            @Override
                            public void onBeforeSendKeyEvent(KeyEvent event) {
                                if (AwContents.isDpadEvent(event)) {
                                    mSettings.setSpatialNavigationEnabled(true);
                                }
                            }
                        });
    }

    private void initializeAutofillProvider(
            AwSelectionActionMenuDelegate selectionActionMenuDelegate) {
        if (mAutofillProvider == null) {
            mAutofillProvider =
                    new AutofillProvider(mContext, mContainerView, mWebContents, "Android WebView");
        } else {
            mAutofillProvider.setWebContents(mWebContents);
        }
        selectionActionMenuDelegate.setAutofillSelectionMenuItemHelper(
                new AutofillSelectionMenuItemHelper(mContext, mAutofillProvider));
        AwContentsJni.get().initializeAndroidAutofill(mNativeAwContents);
    }

    private boolean isSamsungMailApp() {
        // There are 2 different Samsung mail apps exhibiting bugs related to
        // http://crbug.com/781535.
        String currentPackageName = mContext.getPackageName();
        return "com.android.email".equals(currentPackageName)
                || "com.samsung.android.email.composer".equals(currentPackageName);
    }

    public boolean isFullScreen() {
        return mFullScreenTransitionsState.isFullScreen();
    }

    /**
     * For multi-profile public API. For internal access to the browser context,
     * use the member variable {@link AwContents#mBrowserContext} directly. All Exception messages
     * should be developer friendly and refer to the browser context as a "Profile".
     *
     * @throws IllegalStateException if the WebView has been destroyed via. {@link
     *         AwContents#destroy()}.
     */
    @NonNull
    public AwBrowserContext getBrowserContext() {
        if (isDestroyed(NO_WARN)) {
            throw new IllegalStateException("Cannot get profile for destroyed WebView.");
        }
        mBrowserContextAccessed = true;
        return mBrowserContext;
    }

    /**
     * For multi-profile public API. Sets a new browser context which will
     * cause the web contents to reinitialize. All Exception messages should
     * be developer friendly and refer to the browser context as a "Profile".
     *
     * @throws IllegalStateException if the WebView has been destroyed via. {@link
     *         AwContents#destroy()}.
     * @throws IllegalStateException if the browser context has been accessed via. {@link
     *         AwContents#getBrowserContext()}.
     * @throws IllegalStateException if the browser context has already been set explicitly via.
     *         {@link AwContents#setBrowserContext(AwBrowserContext)}.
     * @throws IllegalStateException if the {@link AwContents#evaluateJavaScript(String, Callback)}
     *         has been called on the WebView.
     * @throws IllegalStateException if the WebView has previously navigated to a web page.
     */
    public void setBrowserContext(@NonNull AwBrowserContext browserContext) {
        if (browserContext == mBrowserContext) {
            return;
        }
        if (isDestroyed(NO_WARN)) {
            throw new IllegalStateException(
                    "Cannot set new profile on a WebView that has been destroyed");
        }
        if (mBrowserContextAccessed) {
            throw new IllegalStateException(
                    "Cannot set new profile after the current one has been retrieved via. "
                            + "getProfile");
        }
        if (mBrowserContextSetExplicitly) {
            throw new IllegalStateException(
                    "Cannot set new profile after one has already been set" + "via. setProfile");
        }
        if (mHasEvaluatedJavascript) {
            throw new IllegalStateException(
                    "Cannot set new profile after call to evaluateJavascript");
        }

        final NavigationHistory navigationHistory = getNavigationHistory();
        if (navigationHistory.getEntryCount() != 0
                && !navigationHistory.getEntryAtIndex(0).isInitialEntry()) {
            throw new IllegalStateException(
                    "Cannot set new profile on a WebView that has been previously navigated.");
        }

        // Save existing state and reset.
        StateSnapshot previousState = captureStateAndResetView();
        mBrowserContext = browserContext;
        mBrowserContextSetExplicitly = true;
        setNewAwContents(
                AwContentsJni.get().init(mBrowserContext.getNativeBrowserContextPointer()));

        // Finally refresh all view state.
        restoreState(previousState);
    }

    /**
     * Transitions this {@link AwContents} to fullscreen mode and returns the
     * {@link View} where the contents will be drawn while in fullscreen, or null
     * if this AwContents has already been destroyed.
     */
    View enterFullScreen() {
        assert !isFullScreen();
        if (isDestroyed(NO_WARN)) return null;

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
        mFullScreenTransitionsState.enterFullScreen(
                fullScreenView,
                wasInitialContainerViewFocused,
                mScrollOffsetManager.getScrollX(),
                mScrollOffsetManager.getScrollY());
        mAwViewMethods = new NullAwViewMethods(this, mInternalAccessAdapter, mContainerView);

        // Associate this AwContents with the FullScreenView.
        setInternalAccessAdapter(fullScreenView.getInternalAccessAdapter());
        setContainerView(fullScreenView);

        return fullScreenView;
    }

    /** Called when the app has requested to exit fullscreen. */
    public void requestExitFullscreen() {
        if (!isDestroyed(NO_WARN)) mWebContents.exitFullscreen();
    }

    /**
     * Returns this {@link AwContents} to embedded mode, where the {@link AwContents} are drawn
     * in the WebView.
     */
    void exitFullScreen() {
        if (!isFullScreen() || isDestroyed(NO_WARN)) {
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
        fullscreenView.setAwViewMethods(
                new NullAwViewMethods(
                        this, fullscreenView.getInternalAccessAdapter(), fullscreenView));
        mAwViewMethods = awViewMethodsImpl;
        ViewGroup initialContainerView = mFullScreenTransitionsState.getInitialContainerView();

        // Re-associate this AwContents with the WebView.
        setInternalAccessAdapter(mFullScreenTransitionsState.getInitialInternalAccessDelegate());
        setContainerView(initialContainerView);

        // Return focus to the WebView.
        if (mFullScreenTransitionsState.wasInitialContainerViewFocused()) {
            mContainerView.requestFocus();
        }

        if (!isDestroyed(NO_WARN)) {
            AwContentsJni.get()
                    .restoreScrollAfterTransition(
                            mNativeAwContents,
                            mFullScreenTransitionsState.getScrollX(),
                            mFullScreenTransitionsState.getScrollY());
        }

        mFullScreenTransitionsState.exitFullScreen();
    }

    private void setInternalAccessAdapter(InternalAccessDelegate internalAccessAdapter) {
        mInternalAccessAdapter = internalAccessAdapter;
        mViewEventSink.setAccessDelegate(mInternalAccessAdapter);
    }

    private void setContainerView(ViewGroup newContainerView) {
        // setWillNotDraw(false) is required since WebView draws its own contents using its
        // container view. If this is ever not the case we should remove this, as it removes
        // Android's gatherTransparentRegion optimization for the view.
        mContainerView = newContainerView;
        mContainerView.setWillNotDraw(false);

        assert mDrawFunctor == null;

        mViewAndroidDelegate.setContainerView(mContainerView);
        if (mAwPdfExporter != null) {
            mAwPdfExporter.setContainerView(mContainerView);
        }
        mWebContentsDelegate.setContainerView(mContainerView);
        for (PopupTouchHandleDrawable drawable : mTouchHandleDrawables) {
            drawable.onContainerViewChanged(newContainerView);
        }
        onContainerViewChanged();
    }

    /** Reconciles the state of this AwContents object with the state of the new container view. */
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
        // Skip passing size of FullScreenView down. FullScreenView is newly created and detached
        // so has initial size 0x0 before layout. Avoid this temporary resize to 0x0 which can
        // cause flickers and sometimes layout problems in the web page.
        if ((mContainerView instanceof FullScreenView)) {
            assert !containerViewAttached;
        } else {
            awViewMethodsImpl.onSizeChanged(
                    mContainerView.getWidth(), mContainerView.getHeight(), 0, 0);
        }
        awViewMethodsImpl.onWindowFocusChanged(mContainerView.hasWindowFocus());
        awViewMethodsImpl.onFocusChanged(mContainerView.hasFocus(), 0, null);
        ViewUtils.requestLayout(mContainerView, "AwContents.onContainerViewChanged");
        if (mAutofillProvider != null) mAutofillProvider.onContainerViewChanged(mContainerView);
        mDisplayModeController.setCurrentContainerView(mContainerView);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            if (mDisplayCutoutController != null) {
                mDisplayCutoutController.setCurrentContainerView(mContainerView);
            }
        }
    }

    /**
     * Used for saving and restoring the WebView's state during a native web contents change.
     * Currently this occurs either after receiving popup contents, or after a browser context
     * change via. the multi-profile public API.
     */
    private static class StateSnapshot {
        public final boolean wasAttached;
        public final boolean wasViewVisible;
        public final boolean wasWindowVisible;
        public final boolean wasPaused;
        public final boolean wasFocused;
        public final boolean wasWindowFocused;
        public final @NonNull Map<String, Pair<Object, Class>> javascriptInterfaces;
        public final @Nullable WebMessageListenerInfo[] webMessageListenerInfo;
        public final @Nullable StartupJavascriptInfo[] startupJavascriptInfo;

        public StateSnapshot(@NonNull AwContents awContents) {
            wasAttached = awContents.mIsAttachedToWindow;
            wasViewVisible = awContents.mIsViewVisible;
            wasWindowVisible = awContents.mIsWindowVisible;
            wasPaused = awContents.mIsPaused;
            wasFocused = awContents.mContainerViewFocused;
            wasWindowFocused = awContents.mWindowFocused;

            // Save injected JavaScript interfaces.
            javascriptInterfaces = new HashMap<>();
            if (awContents.mWebContents != null) {
                javascriptInterfaces.putAll(awContents.getJavascriptInjector().getInterfaces());
            }

            // Save injected WebMessageListeners.
            webMessageListenerInfo =
                    AwContentsJni.get().getWebMessageListenerInfos(awContents.mNativeAwContents);
            startupJavascriptInfo =
                    AwContentsJni.get().getDocumentStartupJavascripts(awContents.mNativeAwContents);
        }
    }

    // This class destroys the WindowAndroid when after it is gc-ed.
    private static class WindowAndroidWrapper {
        private final WindowAndroid mWindowAndroid;
        private final CleanupReference mCleanupReference;

        // This ref-counts is used only to destroy WindowAndroid eagerly
        // when AwContents is destroyed. The CleanupReference is still used
        // if a Wrapper is created without any AwContents.
        private int mRefFromAwContentsDestroyRunnable;

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

        public void incrementRefFromDestroyRunnable() {
            mRefFromAwContentsDestroyRunnable++;
        }

        public void decrementRefFromDestroyRunnable() {
            assert mRefFromAwContentsDestroyRunnable > 0;
            mRefFromAwContentsDestroyRunnable--;
            maybeCleanupEarly();
        }

        private void maybeCleanupEarly() {
            if (mRefFromAwContentsDestroyRunnable != 0) return;

            Context context = mWindowAndroid.getContext().get();
            if (context != null && sContextWindowMap.get(context) != this) return;

            mCleanupReference.cleanupNow();
            if (context != null) sContextWindowMap.remove(context);
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
            Activity activity = ContextUtils.activityFromContext(context);
            if (activity != null) {
                ActivityWindowAndroid activityWindow;
                try (ScopedSysTraceEvent e2 =
                        ScopedSysTraceEvent.scoped("AwContents.createActivityWindow")) {
                    final boolean listenToActivityState = false;
                    activityWindow =
                            new ActivityWindowAndroid(
                                    context,
                                    listenToActivityState,
                                    IntentRequestTracker.createFromActivity(activity));
                }
                wrapper = new WindowAndroidWrapper(activityWindow);
            } else {
                wrapper = new WindowAndroidWrapper(new WindowAndroid(context));
            }
            sContextWindowMap.put(context, wrapper);
        }
        return wrapper;
    }

    /**
     * Set current locales to native. Propagates this information to the Accept-Language header for
     * subsequent requests. Note that this will affect <b>all</b> AwContents, not just this
     * instance, as all WebViews share the same NetworkContext/UrlRequestContextGetter.
     */
    @VisibleForTesting
    public void updateDefaultLocale() {
        String locales = LocaleUtils.getDefaultLocaleListString();
        if (!sCurrentLocales.equals(locales)) {
            sCurrentLocales = locales;

            // We cannot use the first language in sCurrentLocales for the UI language even on
            // Android N. LocaleUtils.getDefaultLocaleString() is capable for UI language but
            // it is not guaranteed to be listed at the first of sCurrentLocales. Therefore,
            // both values are passed to native.
            AwContentsJni.get()
                    .updateDefaultLocale(LocaleUtils.getDefaultLocaleString(), sCurrentLocales);
            mSettings.updateAcceptLanguages();
        }
    }

    private void setFunctor(AwFunctor functor) {
        if (mDrawFunctor == functor) return;
        AwFunctor oldFunctor = mDrawFunctor;
        mDrawFunctor = functor;
        updateNativeAwGLFunctor();

        if (oldFunctor != null) oldFunctor.destroy();
    }

    private void updateNativeAwGLFunctor() {
        AwContentsJni.get()
                .setCompositorFrameConsumer(
                        mNativeAwContents,
                        mDrawFunctor != null ? mDrawFunctor.getNativeCompositorFrameConsumer() : 0);
    }

    /* Common initialization routine for adopting a native AwContents instance into this
     * java instance.
     *
     * TAKE CARE! This method can get called multiple times per java instance. Code accordingly.
     * ^^^^^^^^^  See the native class declaration for more details on relative object lifetimes.
     */
    private void setNewAwContents(long newAwContentsPtr) {
        // Move the TextClassifier to the new WebContents.
        TextClassifier textClassifier = mWebContents != null ? getTextClassifier() : null;
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

        mWebContents = AwContentsJni.get().getWebContents(mNativeAwContents);

        if (!mBrowserContextSetExplicitly) {
            mBrowserContext = AwContentsJni.get().getBrowserContext(mNativeAwContents);
        }

        mWindowAndroid = getWindowAndroid(mContext);
        mViewAndroidDelegate =
                new AwViewAndroidDelegate(mContainerView, mContentsClient, mScrollOffsetManager);
        mWebContentsInternalsHolder = new WebContentsInternalsHolder(this);
        AwSelectionActionMenuDelegate selectionActionMenuDelegate =
                new AwSelectionActionMenuDelegate();
        initWebContents(
                mViewAndroidDelegate,
                mInternalAccessAdapter,
                mWebContents,
                mWindowAndroid.getWindowAndroid(),
                mWebContentsInternalsHolder,
                selectionActionMenuDelegate);
        AwContentsJni.get()
                .setJavaPeers(
                        mNativeAwContents,
                        this,
                        mWebContentsDelegate,
                        mContentsClientBridge,
                        mIoThreadClient,
                        mInterceptNavigationDelegate);
        GestureListenerManager.fromWebContents(mWebContents)
                .addListener(new AwGestureStateListener());

        mNavigationController = mWebContents.getNavigationController();
        installWebContentsObservers();
        mSettings.setWebContents(mWebContents);
        mAwDarkMode.setWebContents(mWebContents);

        if (AndroidAutofillSafeModeAction.isAndroidAutofillDisabled()) {
            Log.i(TAG, "Android autofill is disabled by SafeMode");
        } else {
            initializeAutofillProvider(selectionActionMenuDelegate);
            // The sensitive content client has to be instantiated after the autofill
            // client, because the sensitive content client starts a flow which uses
            // `ScopedAutofillManagersObservation`.
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM
                    // If the content sensitivity of the container view (WebView) is not
                    // `CONTENT_SENSITIVITY_AUTO`, then we consider that the developer of the app
                    // which embeds WebView has opted out of the sensitive content feature.
                    && mContainerView.getContentSensitivity() == View.CONTENT_SENSITIVITY_AUTO
                    && AwFeatureMap.isEnabled(SensitiveContentFeatures.SENSITIVE_CONTENT)) {
                AwContentsJni.get().initSensitiveContentClient(mNativeAwContents);
            }
        }

        mDisplayObserver.onDIPScaleChanged(getDeviceScaleFactor());

        updateWebContentsVisibility();

        // The native side object has been bound to this java instance, so now is the time to
        // bind all the native->java relationships.
        mCleanupReference =
                new CleanupReference(
                        this, new AwContentsDestroyRunnable(mNativeAwContents, mWindowAndroid));
        if (textClassifier != null) setTextClassifier(textClassifier);
        if (mOnscreenContentProvider != null) {
            mOnscreenContentProvider.onWebContentsChanged(mWebContents);
        }

        mStylusWritingController.onWebContentsChanged(mWebContents);
    }

    private void installWebContentsObservers() {
        if (mWebContentsObserver != null) {
            mWebContentsObserver.destroy();
        }
        mWebContentsObserver = new AwWebContentsObserver(mWebContents, this, mContentsClient);
        if (mAwWebContentsMetricsRecorder != null) {
            mAwWebContentsMetricsRecorder.destroy();
        }
        mAwWebContentsMetricsRecorder =
                new AwWebContentsMetricsRecorder(mWebContents, mContext, mSettings);
    }

    /**
     * Called on the "source" AwContents that is opening the popup window to
     * provide the AwContents to host the pop up content.
     *
     * See //android_webview/docs/how-does-on-create-window-work.md for more details.
     */
    public void supplyContentsForPopup(AwContents newContents) {
        if (TRACE) Log.i(TAG, "%s supplyContentsForPopup", this);
        if (isDestroyed(WARN)) return;
        long popupNativeAwContents = AwContentsJni.get().releasePopupAwContents(mNativeAwContents);
        if (popupNativeAwContents == 0) {
            Log.w(TAG, "Popup WebView bind failed: no pending content.");
            if (newContents != null) newContents.destroy();
            return;
        }
        if (newContents == null) {
            AwContentsJni.get().destroy(popupNativeAwContents);
            return;
        }

        newContents.receivePopupContents(popupNativeAwContents);
    }

    /**
     * Captures the WebView's state before doing a full reset of the view state to prepare
     * for a new native web contents.
     */
    private StateSnapshot captureStateAndResetView() {
        // Save the existing state.
        StateSnapshot state = new StateSnapshot(this);

        // Reset the view state.
        if (state.wasFocused) onFocusChanged(false, 0, null);
        if (state.wasWindowFocused) onWindowFocusChanged(false);
        if (state.wasViewVisible) setViewVisibilityInternal(false);
        if (state.wasWindowVisible) setWindowVisibilityInternal(false);
        if (state.wasAttached) onDetachedFromWindow();
        if (!state.wasPaused) onPause();

        return state;
    }

    /**
     * Restores the WebView to its previous state before a native web contents change.
     * @param previousState the state to restore to.
     */
    private void restoreState(StateSnapshot previousState) {
        if (!previousState.wasPaused) onResume();
        if (previousState.wasAttached) {
            onAttachedToWindow();
            mContainerView.postInvalidateOnAnimation();
        }
        onSizeChanged(mContainerView.getWidth(), mContainerView.getHeight(), 0, 0);
        if (previousState.wasWindowVisible) setWindowVisibilityInternal(true);
        if (previousState.wasViewVisible) setViewVisibilityInternal(true);
        if (previousState.wasWindowFocused) onWindowFocusChanged(true);
        if (previousState.wasFocused) onFocusChanged(true, 0, null);

        // Restore injected JavaScript interfaces.
        for (Map.Entry<String, Pair<Object, Class>> entry :
                previousState.javascriptInterfaces.entrySet()) {
            @SuppressWarnings("unchecked")
            Class<? extends Annotation> requiredAnnotation = entry.getValue().second;
            getJavascriptInjector()
                    .addPossiblyUnsafeInterface(
                            entry.getValue().first, entry.getKey(), requiredAnnotation);
        }

        // Restore injected WebMessageListeners.
        WebMessageListenerInfo[] previousWebMessageListenerInfo =
                previousState.webMessageListenerInfo;
        if (previousWebMessageListenerInfo != null) {
            for (WebMessageListenerInfo info : previousWebMessageListenerInfo) {
                addWebMessageListener(
                        info.mObjectName, info.mAllowedOriginRules, info.mHolder.getListener());
            }
        }
        StartupJavascriptInfo[] previousDocumentStartupJavascripts =
                previousState.startupJavascriptInfo;
        if (previousDocumentStartupJavascripts != null) {
            for (StartupJavascriptInfo info : previousDocumentStartupJavascripts) {
                addDocumentStartJavaScript(info.mScript, info.mAllowedOriginRules);
            }
        }
    }

    // Recap: supplyContentsForPopup() is called on the parent window's content, this method is
    // called on the popup window's content.
    // See //android_webview/docs/how-does-on-create-window-work.md for more details.
    private void receivePopupContents(long popupNativeAwContents) {
        if (isDestroyed(WARN)) return;
        // Capture as reset the view state for mNativeAwContents.
        StateSnapshot prePopupState = captureStateAndResetView();

        setNewAwContents(popupNativeAwContents);
        // We defer loading any URL on the popup until it has been properly initialized (through
        // setNewAwContents). We resume the load here.
        AwContentsJni.get().resumeLoadingCreatedPopupWebContents(mNativeAwContents);

        // Finally refresh all view state for mNativeAwContents.
        restoreState(prePopupState);

        mIsPopupWindow = true;
    }

    private JavascriptInjector getJavascriptInjector() {
        if (mJavascriptInjector == null) {
            mJavascriptInjector = JavascriptInjector.fromWebContents(mWebContents);
        }
        return mJavascriptInjector;
    }

    @CalledByNative
    private void onRendererResponsive(AwRenderProcess renderProcess) {
        if (isDestroyed(NO_WARN)) return;
        AwThreadUtils.postToCurrentLooper(
                () -> mContentsClient.onRendererResponsive(renderProcess));
    }

    @CalledByNative
    private void onRendererUnresponsive(AwRenderProcess renderProcess) {
        if (isDestroyed(NO_WARN)) return;
        AwThreadUtils.postToCurrentLooper(
                () -> mContentsClient.onRendererUnresponsive(renderProcess));
    }

    @VisibleForTesting
    @CalledByNativeUnchecked
    protected boolean onRenderProcessGone(int childProcessID, boolean crashed) {
        if (isDestroyed(NO_WARN)) return true;
        return mContentsClient.onRenderProcessGone(
                new AwRenderProcessGoneDetail(
                        crashed, AwContentsJni.get().getEffectivePriority(mNativeAwContents)));
    }

    public @RendererPriority int getEffectivePriorityForTesting() {
        assert !isDestroyed(NO_WARN);
        return AwContentsJni.get().getEffectivePriority(mNativeAwContents);
    }

    public AwDarkMode getAwDarkModeForTesting() {
        return mAwDarkMode;
    }

    public void flushBackForwardCache() {
        flushBackForwardCache(BackForwardCacheNotRestoredReason.CACHE_FLUSHED);
    }

    public void flushBackForwardCache(int reason) {
        if (isDestroyed(NO_WARN)) return;
        AwContentsJni.get().flushBackForwardCache(mNativeAwContents, reason);
    }

    public void cancelAllPrerendering() {
        if (isDestroyed(NO_WARN)) return;
        AwContentsJni.get().cancelAllPrerendering(mNativeAwContents);
    }

    /** Destroys this object and deletes its native counterpart. */
    public void destroy() {
        if (TRACE) Log.i(TAG, "%s destroy", this);
        if (isDestroyed(NO_WARN)) return;

        if (mOnscreenContentProvider != null) {
            mOnscreenContentProvider.destroy();
            mOnscreenContentProvider = null;
        }

        if (mAutofillProvider != null) {
            mAutofillProvider.destroy();
            mAutofillProvider = null;
        }

        if (mAwDarkMode != null) {
            mAwDarkMode.destroy();
            mAwDarkMode = null;
        }

        // Remove pending messages
        mContentsClient.getCallbackHelper().removeCallbacksAndMessages();

        if (mIsAttachedToWindow) {
            Log.w(TAG, "WebView.destroy() called while WebView is still attached to window.");
            // Need to call detach to avoid leaks because the real detach later will be ignored.
            onDetachedFromWindow();
        }
        mIsDestroyed = true;
        PostTask.postTask(TaskTraits.UI_DEFAULT, () -> destroyNatives());
    }

    /** Deletes the native counterpart of this object. */
    @VisibleForTesting
    public void destroyNatives() {
        if (mCleanupReference != null) {
            assert mNativeAwContents != 0;

            mWebContentsObserver.destroy();
            mWebContentsObserver = null;
            mAwWebContentsMetricsRecorder.destroy();
            mAwWebContentsMetricsRecorder = null;
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

    public AutofillProvider getAutofillProviderForTesting() {
        return mAutofillProvider;
    }

    // Can be called from any thread.
    public AwSettings getSettings() {
        if (TRACE) Log.i(TAG, "%s getSettings", this);
        return mSettings;
    }

    ViewGroup getContainerView() {
        return mContainerView;
    }

    public AwPdfExporter getPdfExporter() {
        if (TRACE) Log.i(TAG, "%s getPdfExporter", this);
        if (isDestroyed(WARN)) return null;
        if (mAwPdfExporter == null) {
            mAwPdfExporter = new AwPdfExporter(mContainerView);
            AwContentsJni.get().createPdfExporter(mNativeAwContents, mAwPdfExporter);
        }
        return mAwPdfExporter;
    }

    public static void setAwDrawSWFunctionTable(long functionTablePointer) {
        AwContentsJni.get().setAwDrawSWFunctionTable(functionTablePointer);
    }

    public static void setAwDrawGLFunctionTable(long functionTablePointer) {
        AwContentsJni.get().setAwDrawGLFunctionTable(functionTablePointer);
    }

    public static long getAwDrawGLFunction() {
        return AwGLFunctor.getAwDrawGLFunction();
    }

    public static void setShouldDownloadFavicons() {
        AwContentsJni.get().setShouldDownloadFavicons();
    }

    /**
     * Disables contents of JS-to-Java bridge objects to be inspectable using
     * Object.keys() method and "for .. in" loops. This is intended for applications
     * targeting earlier Android releases where this was not possible, and we want
     * to ensure backwards compatible behavior.
     */
    public void disableJavascriptInterfacesInspection() {
        if (TRACE) Log.i(TAG, "%s disableJavascriptInterfacesInspection", this);
        if (!isDestroyed(WARN)) {
            getJavascriptInjector().setAllowInspection(false);
        }
    }

    /**
     * Intended for test code.
     * @return the number of native instances of this class.
     */
    @VisibleForTesting
    public static int getNativeInstanceCount() {
        return AwContentsJni.get().getNativeInstanceCount();
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

    public void setOnscreenContentProvider(OnscreenContentProvider onscreenContentProvider) {
        if (mOnscreenContentProvider != null) {
            mOnscreenContentProvider.destroy();
        }
        mOnscreenContentProvider = onscreenContentProvider;
    }

    /** Release any DragAndDropPermissions currently held. */
    protected void releaseDragAndDropPermissions() {
        if (mDragAndDropPermissions != null) {
            mDragAndDropPermissions.release();
            mDragAndDropPermissions = null;
        }
    }

    // --------------------------------------------------------------------------------------------
    //  WebView[Provider] method implementations
    // --------------------------------------------------------------------------------------------

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
        if (TRACE) Log.i(TAG, "%s getContentHeightCss", this);
        if (isDestroyed(WARN)) return 0;
        return (int) Math.ceil(mContentHeightDip);
    }

    public int getContentWidthCss() {
        if (TRACE) Log.i(TAG, "%s getContentWidthCss", this);
        if (isDestroyed(WARN)) return 0;
        return (int) Math.ceil(mContentWidthDip);
    }

    public Picture capturePicture() {
        if (TRACE) Log.i(TAG, "%s capturePicture", this);
        if (isDestroyed(WARN)) return null;
        return new AwPicture(
                AwContentsJni.get()
                        .capturePicture(
                                mNativeAwContents,
                                mScrollOffsetManager.computeHorizontalScrollRange(),
                                mScrollOffsetManager.computeVerticalScrollRange()));
    }

    public void clearView() {
        if (TRACE) Log.i(TAG, "%s clearView", this);
        if (!isDestroyed(WARN)) AwContentsJni.get().clearView(mNativeAwContents);
    }

    /**
     * Enable the onNewPicture callback.
     * @param enabled Flag to enable the callback.
     * @param invalidationOnly Flag to call back only on invalidation without providing a picture.
     */
    public void enableOnNewPicture(boolean enabled, boolean invalidationOnly) {
        if (TRACE) Log.i(TAG, "%s enableOnNewPicture=%s", this, enabled);
        if (isDestroyed(WARN)) return;
        if (invalidationOnly) {
            mPictureListenerContentProvider = null;
        } else if (enabled && mPictureListenerContentProvider == null) {
            mPictureListenerContentProvider = () -> capturePicture();
        }
        AwContentsJni.get().enableOnNewPicture(mNativeAwContents, enabled);
    }

    public void findAllAsync(String searchString) {
        if (TRACE) Log.i(TAG, "%s findAllAsync", this);
        if (isDestroyed(WARN)) return;
        if (searchString == null) {
            throw new IllegalArgumentException("Search string shouldn't be null");
        }
        AwContentsJni.get().findAllAsync(mNativeAwContents, searchString);
    }

    public void findNext(boolean forward) {
        if (TRACE) Log.i(TAG, "%s findNext", this);
        if (!isDestroyed(WARN)) {
            AwContentsJni.get().findNext(mNativeAwContents, forward);
        }
    }

    public void clearMatches() {
        if (TRACE) Log.i(TAG, "%s clearMatches", this);
        if (!isDestroyed(WARN)) {
            AwContentsJni.get().clearMatches(mNativeAwContents);
        }
    }

    /** @return load progress of the WebContents, on a scale of 0-100. */
    public int getMostRecentProgress() {
        if (isDestroyed(WARN)) return 0;
        if (!mWebContents.isLoading()) return 100;
        return Math.round(100 * mWebContents.getLoadProgress());
    }

    public Bitmap getFavicon() {
        if (TRACE) Log.i(TAG, "%s getFavicon", this);
        if (isDestroyed(WARN)) return null;
        return mFavicon;
    }

    private void requestVisitedHistoryFromClient() {
        Callback<String[]> callback =
                value -> {
                    if (value != null) {
                        // Replace null values with empty strings, because they can't be represented
                        // as native strings.
                        for (int i = 0; i < value.length; i++) {
                            if (value[i] == null) value[i] = "";
                        }
                    }

                    PostTask.runOrPostTask(
                            TaskTraits.UI_DEFAULT,
                            () -> {
                                if (!isDestroyed(NO_WARN)) {
                                    AwContentsJni.get().addVisitedLinks(mNativeAwContents, value);
                                }
                            });
                };
        mContentsClient.getVisitedHistory(callback);
    }

    /* package */ static final Pattern BAD_HEADER_CHAR = Pattern.compile("[\u0000\r\n]");
    /* package */ static final String BAD_HEADER_MSG =
            "HTTP headers must not contain null, CR, or NL characters. ";

    /** WebView.loadUrl. */
    public void loadUrl(String url, Map<String, String> additionalHttpHeaders) {
        if (TRACE) Log.i(TAG, "%s loadUrl(extra headers)=%s", this, url);
        if (isDestroyed(WARN)) return;
        // Early out to match old WebView implementation
        if (url == null) {
            return;
        }
        // TODO: We may actually want to do some preliminary checks here (like filter
        // about://chrome).

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
            for (Map.Entry<String, String> header : additionalHttpHeaders.entrySet()) {
                String headerName = header.getKey();
                String headerValue = header.getValue();
                if (headerName != null && BAD_HEADER_CHAR.matcher(headerName).find()) {
                    throw new IllegalArgumentException(
                            BAD_HEADER_MSG + "Invalid header name '" + headerName + "'.");
                }
                if (headerValue != null && BAD_HEADER_CHAR.matcher(headerValue).find()) {
                    throw new IllegalArgumentException(
                            BAD_HEADER_MSG
                                    + "Header '"
                                    + headerName
                                    + "' has invalid value '"
                                    + headerValue
                                    + "'");
                }
            }
            params.setExtraHeaders(new HashMap<String, String>(additionalHttpHeaders));
        }

        loadUrl(params);
    }

    /** WebView.loadUrl. */
    public void loadUrl(String url) {
        if (TRACE) Log.i(TAG, "%s loadUrl=%s", this, url);
        if (isDestroyed(WARN)) return;
        // Early out to match old WebView implementation
        if (url == null) {
            return;
        }
        loadUrl(url, null);
    }

    /** WebView.postUrl. */
    public void postUrl(String url, byte[] postData) {
        if (TRACE) Log.i(TAG, "%s postUrl=%s", this, url);
        if (isDestroyed(WARN)) return;
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

    private static void recordLoadUrlScheme(@UrlScheme int value) {
        RecordHistogram.recordEnumeratedHistogram(
                LOAD_URL_SCHEME_HISTOGRAM_NAME, value, UrlScheme.COUNT);
    }

    /** WebView.loadData. */
    public void loadData(String data, String mimeType, String encoding) {
        if (TRACE) Log.i(TAG, "%s loadData", this);
        if (isDestroyed(WARN)) return;
        if (data != null && data.contains("#")) {
            if (ContextUtils.getApplicationContext().getApplicationInfo().targetSdkVersion
                            < Build.VERSION_CODES.Q
                    && !isBase64Encoded(encoding)) {
                // As of Chromium M72, data URI parsing strictly enforces encoding of '#'. To
                // support WebView applications which were not expecting this change, we do it for
                // them.
                data = fixupOctothorpesInLoadDataContent(data);
            }
        }
        loadUrl(
                LoadUrlParams.createLoadDataParams(
                        fixupData(data), fixupMimeType(mimeType), isBase64Encoded(encoding)));
    }

    /**
     * Helper method to fixup content passed to {@link #loadData} which may not have had '#'
     * characters encoded correctly. Historically Chromium did not strictly enforce the encoding of
     * '#' characters in Data URLs; they would be treated both as renderable content and as
     * potential URL fragments for DOM id matching. This behavior changed in Chromium M72 where
     * stricter parsing was enforced; the first '#' character now marks the end of the renderable
     * section and the start of the DOM fragment.
     *
     * @param data The content passed to {@link #loadData}, which may contain unencoded '#'s.
     * @return A version of the input with '#' characters correctly encoded, preserving any DOM id
     *         selector which may have been present in the original.
     */
    @VisibleForTesting
    public static String fixupOctothorpesInLoadDataContent(String data) {
        // If the data may have had a valid DOM selector, we duplicate the selector and append it as
        // a proper URL fragment. For example, "<a id='target'>Target</a>#target" will be converted
        // to "<a id='target'>Target</a>%23target#target". This preserves both the rendering (which
        // should render 'Target#target' on the page) and the DOM selector behavior (which should
        // scroll to the anchor).
        Matcher matcher = sDataURLWithSelectorPattern.matcher(data);
        String suffix = matcher.matches() ? matcher.group(1) : "";
        return data.replace("#", "%23") + suffix;
    }

    private @UrlScheme int schemeForUrl(String url) {
        if (url == null || url.equals(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL)) {
            return UrlScheme.EMPTY;
        } else if (url.startsWith("http:")) {
            return UrlScheme.HTTP_SCHEME;
        } else if (url.startsWith("https:")) {
            return UrlScheme.HTTPS_SCHEME;
        } else if (sFileAndroidAssetPattern.matcher(url).matches()) {
            return UrlScheme.FILE_ANDROID_ASSET_SCHEME;
        } else if (url.startsWith("file:")) {
            return UrlScheme.FILE_SCHEME;
        } else if (url.startsWith("ftp:")) {
            return UrlScheme.FTP_SCHEME;
        } else if (url.startsWith("data:")) {
            return UrlScheme.DATA_SCHEME;
        } else if (url.startsWith("javascript:")) {
            return UrlScheme.JAVASCRIPT_SCHEME;
        } else if (url.startsWith("about:")) {
            return UrlScheme.ABOUT_SCHEME;
        } else if (url.startsWith("chrome:")) {
            return UrlScheme.CHROME_SCHEME;
        } else if (url.startsWith("blob:")) {
            return UrlScheme.BLOB_SCHEME;
        } else if (url.startsWith("content:")) {
            return UrlScheme.CONTENT_SCHEME;
        } else if (url.startsWith("intent:")) {
            return UrlScheme.INTENT_SCHEME;
        }
        return UrlScheme.UNKNOWN_SCHEME;
    }

    /** WebView.loadDataWithBaseURL. */
    public void loadDataWithBaseURL(
            String baseUrl, String data, String mimeType, String encoding, String historyUrl) {
        if (TRACE) Log.i(TAG, "%s loadDataWithBaseURL=%s", this, baseUrl);
        if (isDestroyed(WARN)) return;

        data = fixupData(data);
        mimeType = fixupMimeType(mimeType);
        LoadUrlParams loadUrlParams;
        baseUrl = fixupBase(baseUrl);
        historyUrl = fixupHistory(historyUrl);

        if (baseUrl.startsWith("data:")) {
            // For backwards compatibility with WebViewClassic, we use the value of |encoding|
            // as the charset, as long as it's not "base64".
            boolean isBase64 = isBase64Encoded(encoding);
            loadUrlParams =
                    LoadUrlParams.createLoadDataParamsWithBaseUrl(
                            data,
                            mimeType,
                            isBase64,
                            baseUrl,
                            historyUrl,
                            isBase64 ? null : encoding);
        } else {
            // When loading data with a non-data: base URL, the classic WebView would effectively
            // "dump" that string of data into the WebView without going through regular URL
            // loading steps such as decoding URL-encoded entities. We achieve this same behavior by
            // base64 encoding the data that is passed here and then loading that as a data: URL.
            try {
                loadUrlParams =
                        LoadUrlParams.createLoadDataParamsWithBaseUrl(
                                Base64.encodeToString(data.getBytes("utf-8"), Base64.DEFAULT),
                                mimeType,
                                true,
                                baseUrl,
                                historyUrl,
                                "utf-8");
            } catch (java.io.UnsupportedEncodingException e) {
                Log.wtf(TAG, "Unable to load data string %s", data, e);
                return;
            }
        }

        // This is a workaround for an issue with PlzNavigate and one of Samsung's OEM mail apps.
        // See http://crbug.com/781535.
        if (isSamsungMailApp() && SAMSUNG_WORKAROUND_BASE_URL.equals(loadUrlParams.getBaseUrl())) {
            PostTask.postDelayedTask(
                    TaskTraits.UI_DEFAULT, () -> loadUrl(loadUrlParams), SAMSUNG_WORKAROUND_DELAY);
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
        if (params.getBaseUrl() == null) {
            // Don't record the URL if this was loaded via loadDataWithBaseURL(). That API is
            // tracked separately under Android.WebView.LoadDataWithBaseUrl.BaseUrl.
            recordLoadUrlScheme(schemeForUrl(params.getUrl()));
        }

        if (params.getLoadUrlType() == LoadURLType.DATA && !params.isBaseUrlDataScheme()) {
            // This allows data URLs with a non-data base URL access to file:///android_asset/ and
            // file:///android_res/ URLs. If AwSettings.getAllowFileAccess permits, it will also
            // allow access to file:// URLs (subject to OS level permission checks).
            params.setCanLoadLocalResources(true);
            AwContentsJni.get().grantFileSchemeAccesstoChildProcess(mNativeAwContents);
        }

        // If we are reloading the same url, then set transition type as reload.
        if (params.getUrl() != null
                && params.getUrl().equals(mWebContents.getLastCommittedUrl().getSpec())
                && params.getTransitionType() == PageTransition.TYPED) {
            params.setTransitionType(PageTransition.RELOAD);
        }
        params.setTransitionType(params.getTransitionType() | PageTransition.FROM_API);

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

        AwContentsJni.get()
                .setExtraHeadersForUrl(
                        mNativeAwContents,
                        params.getUrl(),
                        params.getExtraHttpRequestHeadersString());
        params.setExtraHeaders(new HashMap<String, String>());

        // Ideally, the URL would only be "fixed" for user input (e.g. for URLs
        // entered into the Omnibox), but some WebView API consumers rely on
        // the legacy behavior where all navigations were subject to the
        // "fixing".  See also https://crbug.com/1145717.
        params.setUrl(UrlFormatter.fixupUrl(params.getUrl()).getPossiblyInvalidSpec());

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
    public GURL getUrl() {
        if (TRACE) Log.i(TAG, "%s getUrl", this);
        if (isDestroyed(WARN)) return null;
        GURL url = mWebContents.getVisibleUrl();
        if (url == null || url.getSpec().trim().isEmpty()) return null;
        return url;
    }

    /**
     * Gets the last committed URL. It represents the current page that is
     * displayed in WebContents. It represents the current security context.
     *
     * @return The URL of the current page or null if it's empty.
     */
    public String getLastCommittedUrl() {
        if (TRACE) Log.i(TAG, "%s getLastCommittedUrl", this);
        if (isDestroyed(NO_WARN)) return null;
        GURL url = mWebContents.getLastCommittedUrl();
        if (url == null || url.isEmpty()) return null;
        return url.getSpec();
    }

    public void requestFocus() {
        mAwViewMethods.requestFocus();
    }

    public void setBackgroundColor(int color) {
        if (TRACE) Log.i(TAG, "%s setBackgroundColor=%x", this, color);
        mBaseBackgroundColor = color;
        mDidInitBackground = true;
        if (!isDestroyed(WARN)) {
            AwContentsJni.get().setBackgroundColor(mNativeAwContents, color);
        }
    }

    /** @see android.view.View#setLayerType() */
    public void setLayerType(int layerType, Paint paint) {
        if (TRACE) Log.i(TAG, "%s setLayerType", this);
        mAwViewMethods.setLayerType(layerType, paint);
    }

    public int getEffectiveBackgroundColorForTesting() {
        return getEffectiveBackgroundColor();
    }

    int getEffectiveBackgroundColor() {
        // Do not ask the WebContents for the background color, as it will always
        // report white prior to initial navigation or post destruction,  whereas we want
        // to use the client supplied base value in those cases.
        if (isDestroyed(NO_WARN)) {
            return mBaseBackgroundColor;
        } else if (!mContentsClient.isCachedRendererBackgroundColorValid()) {
            // In force dark mode or the dark style preferred , if background color not set,
            // this cause a white flash, just show black background.
            if ((mSettings.isForceDarkApplied() || mSettings.prefersDarkFromTheme())
                    && !mDidInitBackground) {
                return Color.BLACK;
            }
            return mBaseBackgroundColor;
        }
        return mContentsClient.getCachedRendererBackgroundColor();
    }

    public boolean isMultiTouchZoomSupported() {
        return mSettings.supportsMultiTouchZoom();
    }

    public View getZoomControlsViewForTest() {
        return mZoomControls.getZoomControlsViewForTest();
    }

    public AwZoomControls getZoomControlsForTest() {
        return mZoomControls;
    }

    /** @see View#setOverScrollMode(int) */
    public void setOverScrollMode(int mode) {
        if (TRACE) Log.i(TAG, "%s setOverScrollMode", this);
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

    /** @see View#setScrollBarStyle(int) */
    public void setScrollBarStyle(int style) {
        if (TRACE) Log.i(TAG, "%s setScrollBarStyle", this);
        boolean scrollbars =
                style == View.SCROLLBARS_INSIDE_OVERLAY || style == View.SCROLLBARS_OUTSIDE_OVERLAY;
        mOverlayHorizontalScrollbar = scrollbars;
        mOverlayVerticalScrollbar = scrollbars;
    }

    /** @see View#setHorizontalScrollbarOverlay(boolean) */
    public void setHorizontalScrollbarOverlay(boolean overlay) {
        if (TRACE) Log.i(TAG, "%s setHorizontalScrollbarOverlay=%s", this, overlay);
        mOverlayHorizontalScrollbar = overlay;
    }

    /** @see View#setVerticalScrollbarOverlay(boolean) */
    public void setVerticalScrollbarOverlay(boolean overlay) {
        if (TRACE) Log.i(TAG, "%s setVerticalScrollbarOverlay=%s", this, overlay);
        mOverlayVerticalScrollbar = overlay;
    }

    /** @see View#overlayHorizontalScrollbar() */
    public boolean overlayHorizontalScrollbar() {
        if (TRACE) Log.i(TAG, "%s overlayHorizontalScrollbar", this);
        return mOverlayHorizontalScrollbar;
    }

    /** @see View#overlayVerticalScrollbar() */
    public boolean overlayVerticalScrollbar() {
        if (TRACE) Log.i(TAG, "%s overlayVerticalScrollbar", this);
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
    public void onContainerViewOverScrolled(
            int scrollX, int scrollY, boolean clampedX, boolean clampedY) {
        mAwViewMethods.onContainerViewOverScrolled(scrollX, scrollY, clampedX, clampedY);
    }

    /** @see android.webkit.WebView#requestChildRectangleOnScreen(View, Rect, boolean) */
    public boolean requestChildRectangleOnScreen(View child, Rect rect, boolean immediate) {
        if (isDestroyed(WARN)) return false;
        return mScrollOffsetManager.requestChildRectangleOnScreen(
                child.getLeft() - child.getScrollX(),
                child.getTop() - child.getScrollY(),
                rect,
                immediate);
    }

    /** @see View#computeHorizontalScrollRange() */
    public int computeHorizontalScrollRange() {
        return mAwViewMethods.computeHorizontalScrollRange();
    }

    /** @see View#computeHorizontalScrollOffset() */
    public int computeHorizontalScrollOffset() {
        return mAwViewMethods.computeHorizontalScrollOffset();
    }

    /** @see View#computeVerticalScrollRange() */
    public int computeVerticalScrollRange() {
        return mAwViewMethods.computeVerticalScrollRange();
    }

    /** @see View#computeVerticalScrollOffset() */
    public int computeVerticalScrollOffset() {
        return mAwViewMethods.computeVerticalScrollOffset();
    }

    /** @see View#computeVerticalScrollExtent() */
    public int computeVerticalScrollExtent() {
        return mAwViewMethods.computeVerticalScrollExtent();
    }

    /** @see View.computeScroll() */
    public void computeScroll() {
        mAwViewMethods.computeScroll();
    }

    /** @see View#onCheckIsTextEditor() */
    public boolean onCheckIsTextEditor() {
        return mAwViewMethods.onCheckIsTextEditor();
    }

    /** @see android.webkit.WebView#stopLoading() */
    public void stopLoading() {
        if (TRACE) Log.i(TAG, "%s stopLoading", this);
        if (!isDestroyed(WARN)) mWebContents.stop();
    }

    /** @see android.webkit.WebView#reload() */
    public void reload() {
        if (TRACE) Log.i(TAG, "%s reload", this);
        if (!isDestroyed(WARN)) mNavigationController.reload(true);
    }

    /** @see android.webkit.WebView#canGoBack() */
    public boolean canGoBack() {
        return isDestroyed(WARN) ? false : mNavigationController.canGoBack();
    }

    /** @see android.webkit.WebView#goBack() */
    public void goBack() {
        if (TRACE) Log.i(TAG, "%s goBack", this);
        if (!isDestroyed(WARN) && mNavigationController.canGoBack()) {
            mNavigationController.goBack();
        }
    }

    /** @see android.webkit.WebView#canGoForward() */
    public boolean canGoForward() {
        return isDestroyed(WARN) ? false : mNavigationController.canGoForward();
    }

    /** @see android.webkit.WebView#goForward() */
    public void goForward() {
        if (TRACE) Log.i(TAG, "%s goForward", this);
        if (!isDestroyed(WARN) && mNavigationController.canGoForward()) {
            mNavigationController.goForward();
        }
    }

    /** @see android.webkit.WebView#canGoBackOrForward(int) */
    public boolean canGoBackOrForward(int steps) {
        return isDestroyed(WARN) ? false : mNavigationController.canGoToOffset(steps);
    }

    /** @see android.webkit.WebView#goBackOrForward(int) */
    public void goBackOrForward(int steps) {
        if (TRACE) Log.i(TAG, "%s goBackOrForward=%d", this, steps);
        if (!canGoBackOrForward(steps)) {
            return;
        }
        if (!isDestroyed(WARN)) mNavigationController.goToOffset(steps);
    }

    /** @see android.webkit.WebView#pauseTimers() */
    public void pauseTimers() {
        if (TRACE) Log.i(TAG, "%s pauseTimers", this);
        if (!isDestroyed(WARN)) {
            ContentViewStatics.setWebKitSharedTimersSuspended(true);
        }
    }

    /** @see android.webkit.WebView#resumeTimers() */
    public void resumeTimers() {
        if (TRACE) Log.i(TAG, "%s resumeTimers", this);
        if (!isDestroyed(WARN)) {
            ContentViewStatics.setWebKitSharedTimersSuspended(false);
        }
    }

    /** @see android.webkit.WebView#onPause() */
    public void onPause() {
        if (TRACE) Log.i(TAG, "%s onPause", this);
        if (mIsPaused || isDestroyed(NO_WARN)) return;
        mIsPaused = true;
        AwContentsJni.get().setIsPaused(mNativeAwContents, mIsPaused);

        // Geolocation is paused/resumed via the page visibility mechanism.
        updateWebContentsVisibility();
    }

    /** @see android.webkit.WebView#onResume() */
    public void onResume() {
        if (TRACE) Log.i(TAG, "%s onResume", this);
        if (!mIsPaused || isDestroyed(NO_WARN)) return;
        mIsPaused = false;
        AwContentsJni.get().setIsPaused(mNativeAwContents, mIsPaused);
        updateWebContentsVisibility();
    }

    /** @see android.webkit.WebView#isPaused() */
    public boolean isPaused() {
        if (TRACE) Log.i(TAG, "%s isPaused", this);
        return isDestroyed(WARN) ? false : mIsPaused;
    }

    /** @see android.webkit.WebView#onCreateInputConnection(EditorInfo) */
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        return mAwViewMethods.onCreateInputConnection(outAttrs);
    }

    /** @see android.webkit.WebView#onDragEvent(DragEvent) */
    public boolean onDragEvent(DragEvent event) {
        return mAwViewMethods.onDragEvent(event);
    }

    /** @see android.webkit.WebView#onKeyUp(int, KeyEvent) */
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        return mAwViewMethods.onKeyUp(keyCode, event);
    }

    /** @see android.webkit.WebView#dispatchKeyEvent(KeyEvent) */
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
        if (!isDestroyed(WARN)) {
            AwContentsJni.get().clearCache(mNativeAwContents, includeDiskFiles);
        }
    }

    public void documentHasImages(Message message) {
        if (TRACE) Log.i(TAG, "%s documentHasImages", this);
        if (!isDestroyed(WARN)) {
            AwContentsJni.get().documentHasImages(mNativeAwContents, message);
        }
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
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    public String getOriginalUrl() {
        if (TRACE) Log.i(TAG, "%s getOriginalUrl", this);
        if (isDestroyed(WARN)) return null;
        NavigationHistory history = getNavigationHistory();
        int currentIndex = history.getCurrentEntryIndex();
        // The current entry will always exist, but only return it if it is not
        // the initial NavigationEntry, to preserve legacy behavior. This is
        // because initial NavigationEntries used to not exist (see
        // https://crbug.com/524208), and the API used to return null when
        // no navigation had committed. Keeping the legacy behavior prevents
        // unexpected breakages on things that depend on it. See also
        // https://crbug.com/1277414.
        if (!history.getEntryAtIndex(currentIndex).isInitialEntry()) {
            return history.getEntryAtIndex(currentIndex).getOriginalUrl().getSpec();
        }

        // When there is no committed navigation, return null.
        return null;
    }

    /** @see NavigationController#getNavigationHistory() */
    public NavigationHistory getNavigationHistory() {
        return isDestroyed(WARN) ? null : mNavigationController.getNavigationHistory();
    }

    /** @see android.webkit.WebView#getTitle() */
    public String getTitle() {
        if (TRACE) Log.i(TAG, "%s getTitle", this);
        return isDestroyed(WARN) ? null : mWebContents.getTitle();
    }

    /** @see android.webkit.WebView#clearHistory() */
    public void clearHistory() {
        if (TRACE) Log.i(TAG, "%s clearHistory", this);
        if (!isDestroyed(WARN)) mNavigationController.clearHistory();
    }

    /** @see android.webkit.WebView#getCertificate() */
    public SslCertificate getCertificate() {
        if (TRACE) Log.i(TAG, "%s getCertificate", this);
        return isDestroyed(WARN)
                ? null
                : SslUtil.getCertificateFromDerBytes(
                        AwContentsJni.get().getCertificate(mNativeAwContents));
    }

    /** @see android.webkit.WebView#clearSslPreferences() */
    public void clearSslPreferences() {
        if (TRACE) Log.i(TAG, "%s clearSslPreferences", this);
        if (!isDestroyed(WARN)) mNavigationController.clearSslPreferences();
    }

    /**
     * Method to return all hit test values relevant to public WebView API.
     * Note that this expose more data than needed for WebView.getHitTestResult.
     * Unsafely returning reference to mutable internal object to avoid excessive
     * garbage allocation on repeated calls.
     */
    public HitTestData getLastHitTestResult() {
        if (TRACE) Log.i(TAG, "%s getLastHitTestResult", this);
        if (isDestroyed(WARN)) return null;
        AwContentsJni.get().updateLastHitTestData(mNativeAwContents);
        return mPossiblyStaleHitTestData;
    }

    /** @see android.webkit.WebView#requestFocusNodeHref() */
    public void requestFocusNodeHref(Message msg) {
        if (TRACE) Log.i(TAG, "%s requestFocusNodeHref", this);
        if (msg == null || isDestroyed(WARN)) return;

        AwContentsJni.get().updateLastHitTestData(mNativeAwContents);
        Bundle data = msg.getData();

        // In order to maintain compatibility with the old WebView's implementation,
        // the absolute (full) url is passed in the |url| field, not only the href attribute.
        data.putString("url", mPossiblyStaleHitTestData.href);
        data.putString("title", mPossiblyStaleHitTestData.anchorText);
        data.putString("src", mPossiblyStaleHitTestData.imgSrc);
        msg.setData(data);
        msg.sendToTarget();
    }

    /** @see android.webkit.WebView#requestImageRef() */
    public void requestImageRef(Message msg) {
        if (TRACE) Log.i(TAG, "%s requestImageRef", this);
        if (msg == null || isDestroyed(WARN)) return;

        AwContentsJni.get().updateLastHitTestData(mNativeAwContents);
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
     * Add a JavaScript snippet that will run after the document has been created, but before any
     * script in the document executes. Note that calling this method multiple times will add
     * multiple scripts. Added scripts will take effect from the next navigation. If want to remove
     * previously set script, use the returned ScriptHandler object to do so. Any JavaScript objects
     * injected by addWebMessageListener() or addJavascriptInterface() will be available to use in
     * this script. Scripts can be removed using the ScriptHandler object returned when they were
     * added. The DOM tree may not be ready at the moment that the script runs.
     *
     * <p>If multiple scripts are added, they will be executed in the same order they were added.
     *
     * @param script The JavaScript snippet to be run.
     * @param allowedOriginRules The JavaScript snippet will run on every frame whose origin matches
     *     any one of the allowedOriginRules.
     * @throws IllegalArgumentException if one of the allowedOriginRules is invalid or one of
     *     jsObjectName and allowedOriginRules is {@code null}.
     * @return A {@link ScriptHandler} for removing the script.
     */
    public ScriptHandler addDocumentStartJavaScript(
            @NonNull String script, @NonNull String[] allowedOriginRules) {
        if (TRACE) Log.i(TAG, "%s addDocumentStartJavaScript", this);
        if (isDestroyed(WARN)) return null;
        if (script == null) {
            throw new IllegalArgumentException("script shouldn't be null.");
        }

        for (int i = 0; i < allowedOriginRules.length; ++i) {
            if (TextUtils.isEmpty(allowedOriginRules[i])) {
                throw new IllegalArgumentException(
                        "allowedOriginRules[" + i + "] shouldn't be null or empty");
            }
        }

        return new ScriptHandler(
                AwContents.this,
                AwContentsJni.get()
                        .addDocumentStartJavaScript(mNativeAwContents, script, allowedOriginRules));
    }

    /* package */ void removeDocumentStartJavaScript(int scriptId) {
        if (isDestroyed(WARN)) return;
        AwContentsJni.get().removeDocumentStartJavaScript(mNativeAwContents, scriptId);
    }

    /**
     * Add the {@link WebMessageListener} to AwContents, it will also inject the JavaScript object
     * with the given name to frames that have origins matching the allowedOriginRules. Note that
     * this call will not inject the JS object immediately. The JS object will be injected only for
     * future navigations (in DidClearWindowObject).
     *
     * @param jsObjectName The name for the injected JavaScript object for this {@link
     *     WebMessageListener}.
     * @param allowedOriginRules A list of matching rules for the allowed origins. The JavaScript
     *     object will be injected when the frame's origin matches any one of the allowed origins.
     *     If a wildcard "*" is provided, it will inject JavaScript object to all frames.
     * @param listener The {@link WebMessageListener} to be called when received onPostMessage().
     * @throws IllegalArgumentException if one of the allowedOriginRules is invalid or one of
     *     jsObjectName and allowedOriginRules is {@code null}.
     * @throws NullPointerException if listener is {@code null}.
     */
    public void addWebMessageListener(
            @NonNull String jsObjectName,
            @NonNull String[] allowedOriginRules,
            @NonNull WebMessageListener listener) {
        if (TRACE) Log.i(TAG, "%s addWebMessageListener=%s", this, jsObjectName);
        if (isDestroyed(WARN)) return;
        if (listener == null) {
            throw new NullPointerException("listener shouldn't be null");
        }

        if (TextUtils.isEmpty(jsObjectName)) {
            throw new IllegalArgumentException("jsObjectName shouldn't be null or empty string");
        }

        for (int i = 0; i < allowedOriginRules.length; ++i) {
            if (TextUtils.isEmpty(allowedOriginRules[i])) {
                throw new IllegalArgumentException(
                        "allowedOriginRules[" + i + "] is null or empty");
            }
        }

        final String exceptionMessage =
                AwContentsJni.get()
                        .addWebMessageListener(
                                mNativeAwContents,
                                new WebMessageListenerHolder(listener),
                                jsObjectName,
                                allowedOriginRules);

        if (!TextUtils.isEmpty(exceptionMessage)) {
            throw new IllegalArgumentException(exceptionMessage);
        }
    }

    /**
     * Removes the {@link WebMessageListener} added by {@link addWebMessageListener}. This call will
     * immediately remove the JavaScript object/WebMessageListener mapping pair. So any messages
     * from the JavaScript object will be dropped. However the JavaScript object will only be
     * removed for future navigations.
     */
    public void removeWebMessageListener(@NonNull String jsObjectName) {
        if (TRACE) Log.i(TAG, "%s removeWebMessageListener=%s", this, jsObjectName);
        if (isDestroyed(WARN)) return;
        AwContentsJni.get().removeWebMessageListener(mNativeAwContents, jsObjectName);
    }

    /**
     * @see android.webkit.WebView#getScale()
     *
     * Please note that the scale returned is the page scale multiplied by
     * the screen density factor. See CTS WebViewTest.testSetInitialScale.
     */
    public float getScale() {
        if (TRACE) Log.i(TAG, "%s getScale", this);
        if (isDestroyed(WARN)) return 1;
        return mPageScaleFactor * getDeviceScaleFactor();
    }

    /** @see android.webkit.WebView#flingScroll(int, int) */
    public void flingScroll(int velocityX, int velocityY) {
        if (TRACE) Log.i(TAG, "%s flingScroll", this);
        if (isDestroyed(WARN)) return;
        mWebContents
                .getEventForwarder()
                .startFling(SystemClock.uptimeMillis(), -velocityX, -velocityY, false, true);
    }

    /** @see android.webkit.WebView#pageUp(boolean) */
    public boolean pageUp(boolean top) {
        if (TRACE) Log.i(TAG, "%s pageUp", this);
        if (isDestroyed(WARN)) return false;
        return mScrollOffsetManager.pageUp(top);
    }

    /** @see android.webkit.WebView#pageDown(boolean) */
    public boolean pageDown(boolean bottom) {
        if (TRACE) Log.i(TAG, "%s pageDown", this);
        if (isDestroyed(WARN)) return false;
        return mScrollOffsetManager.pageDown(bottom);
    }

    /** @see android.webkit.WebView#canZoomIn() */
    // This method uses the term 'zoom' for legacy reasons, but relates
    // to what chrome calls the 'page scale factor'.
    public boolean canZoomIn() {
        if (TRACE) Log.i(TAG, "%s canZoomIn", this);
        if (isDestroyed(WARN)) return false;
        final float zoomInExtent = mMaxPageScaleFactor - mPageScaleFactor;
        return zoomInExtent > ZOOM_CONTROLS_EPSILON;
    }

    /** @see android.webkit.WebView#canZoomOut() */
    // This method uses the term 'zoom' for legacy reasons, but relates
    // to what chrome calls the 'page scale factor'.
    public boolean canZoomOut() {
        if (TRACE) Log.i(TAG, "%s canZoomOut", this);
        if (isDestroyed(WARN)) return false;
        final float zoomOutExtent = mPageScaleFactor - mMinPageScaleFactor;
        return zoomOutExtent > ZOOM_CONTROLS_EPSILON;
    }

    /** @see android.webkit.WebView#zoomIn() */
    // This method uses the term 'zoom' for legacy reasons, but relates
    // to what chrome calls the 'page scale factor'.
    public boolean zoomIn() {
        if (!canZoomIn()) {
            return false;
        }
        zoomBy(ZoomConstants.ZOOM_IN_DELTA);
        return true;
    }

    /** @see android.webkit.WebView#zoomOut() */
    // This method uses the term 'zoom' for legacy reasons, but relates
    // to what chrome calls the 'page scale factor'.
    public boolean zoomOut() {
        if (!canZoomOut()) {
            return false;
        }
        zoomBy(ZoomConstants.ZOOM_OUT_DELTA);
        return true;
    }

    /** Resets the zoom to default */
    // This method uses the term 'zoom' for legacy reasons, but relates
    // to what chrome calls the 'page scale factor'.
    public boolean zoomReset() {
        zoomByInternal(ZoomConstants.ZOOM_RESET_DELTA);
        return true;
    }

    /** @see android.webkit.WebView#zoomBy() */
    // This method uses the term 'zoom' for legacy reasons, but relates
    // to what chrome calls the 'page scale factor'.
    public void zoomBy(float delta) {
        if (delta < 0.01f || delta > 100.0f) {
            throw new IllegalStateException("zoom delta value outside [0.01, 100] range.");
        }
        zoomByInternal(delta);
    }

    private void zoomByInternal(float delta) {
        if (TRACE) Log.i(TAG, "%s zoomBy=%f", this, delta);
        if (isDestroyed(WARN)) return;
        AwContentsJni.get().zoomBy(mNativeAwContents, delta);
    }

    /** @see android.webkit.WebView#invokeZoomPicker() */
    public void invokeZoomPicker() {
        if (TRACE) Log.i(TAG, "%s invokeZoomPicker", this);
        if (!isDestroyed(WARN)) mZoomControls.invokeZoomPicker();
    }

    /** @see android.webkit.WebView#preauthorizePermission(Uri, long) */
    public void preauthorizePermission(Uri origin, long resources) {
        if (TRACE) Log.i(TAG, "%s preauthorizePermission=%s", this, origin);
        if (isDestroyed(NO_WARN)) return;
        AwContentsJni.get().preauthorizePermission(mNativeAwContents, origin.toString(), resources);
    }

    /** @see WebContents.evaluateJavaScript(String, JavaScriptCallback) */
    public void evaluateJavaScript(String script, final Callback<String> callback) {
        if (TRACE) Log.i(TAG, "%s evaluateJavascript=%s", this, script);
        if (isDestroyed(WARN)) return;
        JavaScriptCallback jsCallback = null;
        if (callback != null) {
            jsCallback =
                    jsonResult -> {
                        // Post the application callback back to the current thread to ensure the
                        // application callback is executed without any native code on the stack.
                        // This so that any exception thrown by the application callback won't
                        // have to be propagated through a native call stack.
                        AwThreadUtils.postToCurrentLooper(callback.bind(jsonResult));
                    };
        }

        mHasEvaluatedJavascript = true;
        mWebContents.evaluateJavaScript(script, jsCallback);
    }

    public void evaluateJavaScriptForTests(String script, final Callback<String> callback) {
        if (TRACE) Log.i(TAG, "%s evaluateJavascriptForTests=%s", this, script);
        if (isDestroyed(NO_WARN)) return;
        JavaScriptCallback jsCallback = null;
        if (callback != null) {
            jsCallback = jsonResult -> callback.onResult(jsonResult);
        }

        mWebContents.evaluateJavaScriptForTests(script, jsCallback);
    }

    /**
     * Send a MessageEvent to main frame.
     *
     * @param targetOrigin The expected target frame's origin.
     * @param sentPorts ports for the JavaScript MessageEvent.
     */
    public void postMessageToMainFrame(
            MessagePayload messagePayload, String targetOrigin, MessagePort[] sentPorts) {
        if (TRACE) Log.i(TAG, "%s postMessageToMainFrame", this);
        if (isDestroyed(WARN)) return;

        RenderFrameHost mainFrame = mWebContents.getMainFrame();
        // If the RenderFrameHost or the RenderFrame doesn't exist we couldn't post the message.
        if (mainFrame == null || !mainFrame.isRenderFrameLive()) return;

        mWebContents.postMessageToMainFrame(messagePayload, null, targetOrigin, sentPorts);
    }

    /** Creates a message channel and returns the ports for each end of the channel. */
    public MessagePort[] createMessageChannel() {
        if (TRACE) Log.i(TAG, "%s createMessageChannel", this);
        if (isDestroyed(WARN)) return null;
        return MessagePort.createPair();
    }

    public boolean hasAccessedInitialDocument() {
        if (TRACE) Log.i(TAG, "%s hasAccessedInitialDocument", this);
        if (isDestroyed(NO_WARN)) return false;
        return mWebContents.hasAccessedInitialDocument();
    }

    private WebContentsAccessibility getWebContentsAccessibility() {
        return WebContentsAccessibility.fromWebContents(mWebContents);
    }

    public void onProvideVirtualStructure(ViewStructure structure) {
        if (TRACE) Log.i(TAG, "%s onProvideVirtualStructure", this);
        if (isDestroyed(WARN)) return;
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
        if (TRACE) Log.i(TAG, "%s onProvideAutoFillVirtualStructure", this);
        if (mAutofillProvider != null) {
            mAutofillProvider.onProvideAutoFillVirtualStructure(structure, flags);
        }
    }

    public void autofill(final SparseArray<AutofillValue> values) {
        if (TRACE) Log.i(TAG, "%s autofill", this);
        if (mAutofillProvider != null) {
            mAutofillProvider.autofill(values);
        }
    }

    public boolean isSelectActionModeAllowed(int actionModeItem) {
        return (mSettings.getDisabledActionModeMenuItems() & actionModeItem) != actionModeItem;
    }

    // --------------------------------------------------------------------------------------------
    //  View and ViewGroup method implementations
    // --------------------------------------------------------------------------------------------
    /**
     * Calls android.view.View#startActivityForResult.  A RuntimeException will
     * be thrown by Android framework if startActivityForResult is called with
     * a non-Activity context.
     */
    void startActivityForResult(Intent intent, int requestCode) {
        // Even in fullscreen mode, startActivityForResult will still use the
        // initial internal access delegate because it has access to
        // the hidden API View#startActivityForResult.
        mFullScreenTransitionsState
                .getInitialInternalAccessDelegate()
                .super_startActivityForResult(intent, requestCode);
    }

    void startProcessTextIntent(Intent intent) {
        if (ContextUtils.activityFromContext(mContext) == null) {
            mContext.startActivity(intent);
            return;
        }

        startActivityForResult(intent, PROCESS_TEXT_REQUEST_CODE);
    }

    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (TRACE) Log.i(TAG, "%s onActivityResult", this);
        if (isDestroyed(NO_WARN)) return;
        if (requestCode == PROCESS_TEXT_REQUEST_CODE) {
            SelectionPopupController.fromWebContents(mWebContents)
                    .onReceivedProcessTextResult(resultCode, data);
        } else {
            Log.e(TAG, "Received activity result for an unknown request code %d", requestCode);
        }
    }

    /** @see android.webkit.View#onTouchEvent() */
    public boolean onTouchEvent(MotionEvent event) {
        return mAwViewMethods.onTouchEvent(event);
    }

    /** @see android.view.View#onHoverEvent() */
    public boolean onHoverEvent(MotionEvent event) {
        return mAwViewMethods.onHoverEvent(event);
    }

    /** @see android.view.View#onGenericMotionEvent() */
    public boolean onGenericMotionEvent(MotionEvent event) {
        return isDestroyed(NO_WARN) ? false : mAwViewMethods.onGenericMotionEvent(event);
    }

    /** @see android.view.View#onConfigurationChanged() */
    public void onConfigurationChanged(Configuration newConfig) {
        if (TRACE) Log.i(TAG, "%s onConfigurationChanged", this);
        mAwViewMethods.onConfigurationChanged(newConfig);
        if (!isDestroyed(NO_WARN)) {
            AwContentsJni.get().onConfigurationChanged(mNativeAwContents);
        }
    }

    /** @see android.view.View#onAttachedToWindow() */
    public void onAttachedToWindow() {
        if (TRACE) Log.i(TAG, "%s onAttachedToWindow", this);
        mTemporarilyDetached = false;
        mAwViewMethods.onAttachedToWindow();
        mWindowAndroid.getWindowAndroid().getDisplay().addObserver(mDisplayObserver);

        AwWindowCoverageTracker tracker =
                AwWindowCoverageTracker.getOrCreateForRootView(this, mContainerView.getRootView());
        tracker.trackContents(this);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            if (mDisplayCutoutController != null) mDisplayCutoutController.onAttachedToWindow();
        }

        if (AwFeatureMap.isEnabled(BaseFeatures.COLLECT_ANDROID_FRAME_TIMELINE_METRICS)) {
            Window window = mWindowAndroid.getWindowAndroid().getWindow();
            if (window != null && mContainerView.isHardwareAccelerated()) {
                mAwFrameMetricsListener = AwFrameMetricsListener.onAttachedToWindow(window);
            }
        }

        ViewGroup.LayoutParams viewGroupParams = mContainerView.getRootView().getLayoutParams();
        if (viewGroupParams instanceof WindowManager.LayoutParams params) {
            if (params.type == WindowManager.LayoutParams.TYPE_APPLICATION_PANEL
                    || params.type == WindowManager.LayoutParams.TYPE_APPLICATION_ATTACHED_DIALOG) {
                recordUsedInPopupWindow(UsedInPopupWindow.IN_POPUP_WINDOW);
            } else {
                recordUsedInPopupWindow(UsedInPopupWindow.NOT_IN_POPUP_WINDOW);
            }
        } else {
            recordUsedInPopupWindow(UsedInPopupWindow.UNKNOWN);
        }
    }

    private static void recordUsedInPopupWindow(@UsedInPopupWindow int value) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.UsedInPopupWindow", value, UsedInPopupWindow.COUNT);
    }

    private void detachWindowCoverageTracker() {
        if (mAwWindowCoverageTracker == null) return;
        mAwWindowCoverageTracker.untrackContents(this);
    }

    /** @see android.view.View#onDetachedFromWindow() */
    @SuppressLint("MissingSuperCall")
    public void onDetachedFromWindow() {
        if (TRACE) Log.i(TAG, "%s onDetachedFromWindow", this);

        detachWindowCoverageTracker();
        mWindowAndroid.getWindowAndroid().getDisplay().removeObserver(mDisplayObserver);
        mAwViewMethods.onDetachedFromWindow();

        if (mAwFrameMetricsListener != null) {
            Window window = mWindowAndroid.getWindowAndroid().getWindow();
            if (window != null && mContainerView.isHardwareAccelerated()) {
                AwFrameMetricsListener.onDetachedFromWindow(window);
                mAwFrameMetricsListener = null;
            }
        }
    }

    /** @see android.view.View#onWindowFocusChanged() */
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        mAwViewMethods.onWindowFocusChanged(hasWindowFocus);
    }

    /** @see android.view.View#onFocusChanged() */
    public void onFocusChanged(boolean focused, int direction, Rect previouslyFocusedRect) {
        if (!mTemporarilyDetached) {
            mAwViewMethods.onFocusChanged(focused, direction, previouslyFocusedRect);
        }
    }

    /** @see android.view.View#onStartTemporaryDetach() */
    public void onStartTemporaryDetach() {
        mTemporarilyDetached = true;
    }

    /** @see android.view.View#onFinishTemporaryDetach() */
    public void onFinishTemporaryDetach() {
        mTemporarilyDetached = false;
    }

    /** @see android.view.View#onSizeChanged() */
    public void onSizeChanged(int w, int h, int ow, int oh) {
        mAwViewMethods.onSizeChanged(w, h, ow, oh);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            if (mDisplayCutoutController != null) mDisplayCutoutController.onSizeChanged();
        }
    }

    /** @see android.view.View#onVisibilityChanged() */
    public void onVisibilityChanged(View changedView, int visibility) {
        mAwViewMethods.onVisibilityChanged(changedView, visibility);
    }

    /** @see android.view.View#onWindowVisibilityChanged() */
    public void onWindowVisibilityChanged(int visibility) {
        mAwViewMethods.onWindowVisibilityChanged(visibility);
    }

    /** @see android.view.View#onResolvePointerIcon(MotionEvent, int) */
    public PointerIcon onResolvePointerIcon(MotionEvent event, int pointerIndex) {
        return mStylusWritingController.resolvePointerIcon();
    }

    private void setViewVisibilityInternal(boolean visible) {
        mIsViewVisible = visible;
        if (!isDestroyed(NO_WARN)) {
            AwContentsJni.get().setViewVisibility(mNativeAwContents, mIsViewVisible);
        }
        postUpdateWebContentsVisibility();
    }

    private void setWindowVisibilityInternal(boolean visible) {
        mIsWindowVisible = visible;
        if (!isDestroyed(NO_WARN)) {
            AwContentsJni.get().setWindowVisibility(mNativeAwContents, mIsWindowVisible);

            if (mAwFrameMetricsListener != null) {
                if (mIsWindowVisible) {
                    mAwFrameMetricsListener.onWebviewVisible();
                } else {
                    mAwFrameMetricsListener.onWebviewHidden();
                }
            }
        }
        // Using TimeUtils to allow it being overridden in tests.
        mLastWindowVisibleTime = visible ? CURRENTLY_VISIBLE : TimeUtils.uptimeMillis();
        if (!visible) afterWindowHiddenTask();
        postUpdateWebContentsVisibility();
    }

    private void afterWindowHiddenTask() {
        try (TraceEvent e = TraceEvent.scoped("afterWindowHiddenTask")) {
            if (isDestroyed(NO_WARN)) return;
            if (mLastWindowVisibleTime == CURRENTLY_VISIBLE) return;

            long timeNotVisibleMs = TimeUtils.uptimeMillis() - mLastWindowVisibleTime;
            // Not in background for long enough.
            if (timeNotVisibleMs < FUNCTOR_RECLAIM_DELAY_MS) {
                // A task has been posted. If it's not far enough into the future, it will
                // reschedule itself, nothing to do.
                if (mHasPendingReclaimTask) return;
                mHasPendingReclaimTask = true;

                // We may have any number of fg <-> bg transitions happen in the meantime. Make
                // sure that we only reclaim memory when we've spent enough continuous time in
                // background. Use a weak ref to make sure we don't prevent AwContents from being
                // GC-eligible while this task is in the queue.
                WeakReference<AwContents> weakAwc = new WeakReference(this);
                Runnable task =
                        () -> {
                            AwContents awc = weakAwc.get();
                            if (awc != null) {
                                awc.mHasPendingReclaimTask = false;
                                awc.afterWindowHiddenTask();
                            }
                        };
                long delayMs = FUNCTOR_RECLAIM_DELAY_MS - timeNotVisibleMs;
                postDelayedTaskWithOverride(task, delayMs);
                return;
            }

            if (mDrawFunctor != null) {
                // Clear the functor. This causes native-side resources to be freed. The functor
                // will be re-created at the next draw.
                setFunctor(null);
                // Since we discarded the functor, it is a good time to reclaim resources as well.
                AwContentsJni.get()
                        .trimMemory(
                                mNativeAwContents, ComponentCallbacks2.TRIM_MEMORY_COMPLETE, false);
            }
            // Not immediately collecting memory metrics, because actual memory release can take
            // some time, either through async tasks here, or in the driver.
            postDelayedTaskWithOverride(this::maybeRecordMemory, METRICS_COLLECTION_DELAY_MS);
        }
    }

    /* PostTask can be overridden for testing. */
    private void postDelayedTaskWithOverride(Runnable task, long delayMs) {
        if (mPostDelayedTaskForTesting != null) {
            mPostDelayedTaskForTesting.apply(task, delayMs);
        } else {
            PostTask.postDelayedTask(TaskTraits.UI_DEFAULT, task, delayMs);
        }
    }

    @VisibleForTesting
    public boolean hasDrawFunctor() {
        return mDrawFunctor != null;
    }

    public void setPostDelayedTaskForTesting(BiFunction<Runnable, Long, Void> fn) {
        mPostDelayedTaskForTesting = fn;
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
        PostTask.postTask(TaskTraits.UI_DEFAULT, mUpdateVisibilityRunnable);
    }

    private void updateWebContentsVisibility() {
        mIsUpdateVisibilityTaskPending = false;
        if (isDestroyed(NO_WARN)) return;
        boolean contentVisible = AwContentsJni.get().isVisible(mNativeAwContents);

        if (contentVisible && !mIsContentVisible) {
            mWebContents.updateWebContentsVisibility(Visibility.VISIBLE);
        } else if (!contentVisible && mIsContentVisible) {
            mWebContents.updateWebContentsVisibility(Visibility.HIDDEN);
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
        if (isDestroyed(NO_WARN)) return mIsContentVisible;
        return AwContentsJni.get().isVisible(mNativeAwContents);
    }

    /**
     * Returns true if the web contents has an associated interstitial.
     * This method is only called by tests.
     */
    public boolean isDisplayingInterstitialForTesting() {
        return AwContentsJni.get().isDisplayingInterstitialForTesting(mNativeAwContents);
    }

    /** Key for opaque state in bundle. Note this is only public for tests. */
    public static final String SAVE_RESTORE_STATE_KEY = "WEBVIEW_CHROMIUM_STATE";

    /**
     * Save the state of this AwContents into provided Bundle.
     * @return False if saving state failed.
     */
    public boolean saveState(Bundle outState) {
        if (TRACE) Log.i(TAG, "%s saveState", this);
        if (isDestroyed(WARN) || outState == null) return false;

        byte[] state = AwContentsJni.get().getOpaqueState(mNativeAwContents);
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
        if (isDestroyed(WARN) || inState == null) return false;

        byte[] state = inState.getByteArray(SAVE_RESTORE_STATE_KEY);
        if (state == null) return false;

        boolean result = AwContentsJni.get().restoreFromOpaqueState(mNativeAwContents, state);

        // The onUpdateTitle callback normally happens when a page is loaded,
        // but is optimized out in the restoreState case because the title is
        // already restored. See WebContentsImpl::UpdateTitleForEntry. So we
        // call the callback explicitly here.
        if (result) mContentsClient.onReceivedTitle(mWebContents.getTitle());

        return result;
    }

    /** @see JavascriptInjector#addPossiblyUnsafeInterface(Object, String, Class) */
    public void addJavascriptInterface(Object object, String name) {
        if (TRACE) Log.i(TAG, "%s addJavascriptInterface=%s", this, name);
        if (isDestroyed(WARN)) return;
        Class<? extends Annotation> requiredAnnotation = null;
        if (mAppTargetSdkVersion >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            requiredAnnotation = JavascriptInterface.class;
        }

        getJavascriptInjector().addPossiblyUnsafeInterface(object, name, requiredAnnotation);
    }

    /** @see android.webkit.WebView#removeJavascriptInterface(String) */
    public void removeJavascriptInterface(String interfaceName) {
        if (TRACE) Log.i(TAG, "%s removeInterface=%s", this, interfaceName);
        if (isDestroyed(WARN)) return;

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

    /** @see android.webkit.WebView#performAccessibilityAction(int, Bundle) */
    public boolean performAccessibilityAction(int action, Bundle arguments) {
        return mAwViewMethods.performAccessibilityAction(action, arguments);
    }

    /**
     * @see android.webkit.WebView#clearFormData().
     *     <p>The popup shall also be hidden on the WebView detached from window.
     */
    public void hideAutofillPopup() {
        if (TRACE) Log.i(TAG, "%s hideAutofillPopup", this);
        if (mAutofillProvider != null) {
            mAutofillProvider.hideDatalistPopup();
        }
    }

    public void setNetworkAvailable(boolean networkUp) {
        if (TRACE) Log.i(TAG, "%s setNetworkAvailable=%s", this, networkUp);
        if (!isDestroyed(WARN)) {
            // For backward compatibility when an app uses this API disable the
            // Network Information API to prevent inconsistencies,
            // see crbug.com/520088.
            NetworkChangeNotifier.setAutoDetectConnectivityState(false);
            AwContentsJni.get().setJsOnlineProperty(mNativeAwContents, networkUp);
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
     * @throws IllegalStateException if this method is invoked after {@link #destroy()} has been
     * called.
     */
    public void insertVisualStateCallback(long requestId, VisualStateCallback callback) {
        if (TRACE) Log.i(TAG, "%s insertVisualStateCallback", this);
        if (isDestroyed(NO_WARN)) {
            throw new IllegalStateException(
                    "insertVisualStateCallback cannot be called after the WebView has been"
                            + " destroyed");
        }
        if (callback == null) {
            throw new IllegalArgumentException("VisualStateCallback shouldn't be null");
        }
        AwContentsJni.get().insertVisualStateCallback(mNativeAwContents, requestId, callback);
    }

    public boolean isPopupWindow() {
        return mIsPopupWindow;
    }

    private void updateChildProcessImportance() {
        @ChildProcessImportance int effectiveImportance = ChildProcessImportance.IMPORTANT;
        if (mRendererPriorityWaivedWhenNotVisible && !mIsContentVisible) {
            effectiveImportance = ChildProcessImportance.NORMAL;
        } else {
            switch (mRendererPriority) {
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

    public void setAudioMuted(boolean mute) {
        if (AwFeatureMap.isEnabled(AwFeatures.WEBVIEW_MUTE_AUDIO)) {
            mWebContents.setAudioMuted(mute);
        }
    }

    public boolean isAudioMuted() {
        if (AwFeatureMap.isEnabled(AwFeatures.WEBVIEW_MUTE_AUDIO)) {
            return mWebContents.isAudioMuted();
        }
        return false;
    }

    public void setRendererPriorityPolicy(
            @RendererPriority int rendererRequestedPriority, boolean waivedWhenNotVisible) {
        if (TRACE) Log.i(TAG, "%s setRendererPriorityPolicy", this);
        mRendererPriority = rendererRequestedPriority;
        mRendererPriorityWaivedWhenNotVisible = waivedWhenNotVisible;
        updateChildProcessImportance();
    }

    public void setTextClassifier(TextClassifier textClassifier) {
        if (TRACE) Log.i(TAG, "%s setTextClassifier", this);
        assert mWebContents != null;
        SelectionPopupController.fromWebContents(mWebContents).setTextClassifier(textClassifier);
    }

    public TextClassifier getTextClassifier() {
        if (TRACE) Log.i(TAG, "%s getTextClassifier", this);
        assert mWebContents != null;
        return SelectionPopupController.fromWebContents(mWebContents).getTextClassifier();
    }

    public AwRenderProcess getRenderProcess() {
        if (isDestroyed(WARN)) {
            return null;
        }
        return AwContentsJni.get().getRenderProcess(mNativeAwContents);
    }

    @VisibleForTesting
    public AwDisplayCutoutController getDisplayCutoutController() {
        return mDisplayCutoutController;
    }

    public int getDisplayMode() {
        return mDisplayModeController.getDisplayMode();
    }

    // --------------------------------------------------------------------------------------------
    //  Methods called from native via JNI
    // --------------------------------------------------------------------------------------------

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
        PopupTouchHandleDrawable drawable =
                PopupTouchHandleDrawable.create(
                        mTouchHandleDrawables, mWebContents, mContainerView);
        return drawable.getNativeDrawable();
    }

    /** Callback for generateMHTML. */
    @CalledByNative
    private static void generateMHTMLCallback(String path, long size, Callback<String> callback) {
        if (callback == null) return;
        AwThreadUtils.postToUiThreadLooper(
                () -> {
                    callback.onResult(size < 0 ? null : path);
                });
    }

    @CalledByNative
    private void onReceivedHttpAuthRequest(AwHttpAuthHandler handler, String host, String realm) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.ON_RECEIVED_HTTP_AUTH_REQUEST")) {
            mContentsClient.onReceivedHttpAuthRequest(handler, host, realm);

            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_RECEIVED_HTTP_AUTH_REQUEST);
        }
    }

    public AwGeolocationPermissions getGeolocationPermissions() {
        return mBrowserContext.getGeolocationPermissions();
    }

    public void invokeGeolocationCallback(boolean value, String requestingFrame) {
        if (TRACE) Log.i(TAG, "%s invokeGeolocationCallback", this);
        if (isDestroyed(NO_WARN)) return;
        AwContentsJni.get().invokeGeolocationCallback(mNativeAwContents, value, requestingFrame);
    }

    @CalledByNative
    private void onGeolocationPermissionsShowPrompt(String origin) {
        if (isDestroyed(NO_WARN)) return;
        AwGeolocationPermissions permissions = mBrowserContext.getGeolocationPermissions();
        // Reject if geoloaction is disabled, or the origin has a retained deny
        if (!mSettings.getGeolocationEnabled()) {
            AwContentsJni.get().invokeGeolocationCallback(mNativeAwContents, false, origin);
            return;
        }
        // Allow if the origin has a retained allow
        if (permissions.hasOrigin(origin)) {
            AwContentsJni.get()
                    .invokeGeolocationCallback(
                            mNativeAwContents, permissions.isOriginAllowed(origin), origin);
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
    private void logOriginVisit(long originHash) {
        if (isDestroyed(NO_WARN)) return;
        PostTask.postTask(
                TaskTraits.BEST_EFFORT_MAY_BLOCK,
                () -> AwOriginVisitLogger.logOriginVisit(originHash));
    }

    @CalledByNative
    public void onFindResultReceived(
            int activeMatchOrdinal, int numberOfMatches, boolean isDoneCounting) {
        mContentsClient.onFindResultReceived(activeMatchOrdinal, numberOfMatches, isDoneCounting);
    }

    @CalledByNative
    public void onNewPicture() {
        // Don't call capturePicture() here but instead defer it until the posted task runs within
        // the callback helper, to avoid doubling back into the renderer compositor in the middle
        // of the notification it is sending up to here.
        mContentsClient.getCallbackHelper().postOnNewPicture(mPictureListenerContentProvider);
    }

    @CalledByNative
    public void onPreferredFrameIntervalChanged(long preferredFrameIntervalNanos) {
        mPreferredFrameIntervalNanos = preferredFrameIntervalNanos;
    }

    /**
     * Invokes the given {@link VisualStateCallback}.
     *
     * @param callback the callback to be invoked
     * @param requestId the id passed to {@link AwContents#insertVisualStateCallback}
     */
    @CalledByNative
    public void invokeVisualStateCallback(
            final VisualStateCallback callback, final long requestId) {
        if (isDestroyed(NO_WARN)) return;
        // Posting avoids invoking the callback inside invoking_composite_
        // (see synchronous_compositor_impl.cc and crbug/452530).
        AwThreadUtils.postToUiThreadLooper(() -> callback.onComplete(requestId));
    }

    // Called as a result of AwContentsJni.get().updateLastHitTestData.
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
    private void postInvalidate(boolean insideVSync) {
        if (insideVSync) {
            mContainerView.invalidate();
        } else {
            mContainerView.postInvalidateOnAnimation();
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
    private void updateScrollState(
            int maxContainerViewScrollOffsetX,
            int maxContainerViewScrollOffsetY,
            float contentWidthDip,
            float contentHeightDip,
            float pageScaleFactor,
            float minPageScaleFactor,
            float maxPageScaleFactor) {
        mContentWidthDip = contentWidthDip;
        mContentHeightDip = contentHeightDip;
        mScrollOffsetManager.setMaxScrollOffset(
                maxContainerViewScrollOffsetX, maxContainerViewScrollOffsetY);
        setPageScaleFactorAndLimits(pageScaleFactor, minPageScaleFactor, maxPageScaleFactor);
    }

    @CalledByNative
    private void didOverscroll(
            int deltaX, int deltaY, float velocityX, float velocityY, boolean insideVSync) {
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
        mOverScrollGlow.absorbGlow(
                x,
                y,
                oldX,
                oldY,
                scrollRangeX,
                scrollRangeY,
                (float) Math.hypot(velocityX, velocityY));

        if (mOverScrollGlow.isAnimating()) {
            postInvalidate(insideVSync);
        }
    }

    /** Determine if at least one edge of the WebView extends over the edge of the window. */
    private boolean extendsOutOfWindow() {
        int[] loc = new int[2];
        mContainerView.getLocationOnScreen(loc);
        int x = loc[0];
        int y = loc[1];
        mContainerView.getRootView().getLocationOnScreen(loc);
        int rootX = loc[0];
        int rootY = loc[1];

        // Get the position of the current view, relative to its root view
        int relativeX = x - rootX;
        int relativeY = y - rootY;

        if (relativeX < 0
                || relativeY < 0
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

    /**
     * Return the device locale in the same format we use to populate the 'hl' query parameter for
     * Safe Browsing interstitial urls, as done in BaseUIManager::app_locale().
     */
    public static String getSafeBrowsingLocaleForTesting() {
        return AwContentsJni.get().getSafeBrowsingLocaleForTesting();
    }

    /** Returns the AwContents instance associated with |webContents|, or NULL */
    public static AwContents fromWebContents(WebContents webContents) {
        return AwContentsJni.get().fromWebContents(webContents);
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
            mContentsClient
                    .getCallbackHelper()
                    .postOnScaleChangedScaled(
                            oldPageScaleFactor * dipScale, mPageScaleFactor * dipScale);
        }
        mZoomControls.updateZoomControls();
    }

    private void saveWebArchiveInternal(String path, final Callback<String> callback) {
        if (path == null || isDestroyed(WARN)) {
            if (callback == null) return;

            AwThreadUtils.postToUiThreadLooper(callback.bind(null));
        } else {
            AwContentsJni.get().generateMHTML(mNativeAwContents, path, callback);
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
        if (TRACE) Log.i(TAG, "%s extractSmartClipData", this);
        if (!isDestroyed(WARN)) {
            mWebContents.requestSmartClipExtract(x, y, width, height);
        }
    }

    @Override
    public void setSmartClipResultHandler(final Handler resultHandler) {
        if (TRACE) Log.i(TAG, "%s setSmartClipResultHandler", this);
        if (isDestroyed(WARN)) return;

        mWebContents.setSmartClipResultHandler(resultHandler);
    }

    protected void insertVisualStateCallbackIfNotDestroyed(
            long requestId, VisualStateCallback callback) {
        if (TRACE) Log.i(TAG, "%s insertVisualStateCallbackIfNotDestroyed", this);
        if (isDestroyed(NO_WARN)) return;
        AwContentsJni.get().insertVisualStateCallback(mNativeAwContents, requestId, callback);
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

    @VisibleForTesting
    public void onTrimMemory(final int level) {
        try (TraceEvent e = TraceEvent.scoped("onTrimMemory", String.valueOf(level))) {
            boolean visibleRectEmpty = getGlobalVisibleRect().isEmpty();
            final boolean visible = mIsViewVisible && mIsWindowVisible && !visibleRectEmpty;

            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        if (isDestroyed(NO_WARN)) return;
                        // Post the task in the case where we would have cleared the functor if the
                        // feature was enabled, so that the two experiment arms have the same
                        // number of samples.
                        if (level >= ComponentCallbacks2.TRIM_MEMORY_BACKGROUND) {
                            postDelayedTaskWithOverride(
                                    this::maybeRecordMemory, METRICS_COLLECTION_DELAY_MS);
                            if (mDrawFunctor != null) {
                                mDrawFunctor.trimMemory();
                                setFunctor(null);
                            }
                        }
                        AwContentsJni.get().trimMemory(mNativeAwContents, level, visible);
                    });
        }
    }

    private void maybeRecordMemory() {
        // Note: there is a corner case here: if there are no visible WebViews, but the last one
        // was removed too recently to have had its functor reclaimed, we still collect data.
        // This likely doesn't matter too much, especially since as noted below, the metrics are
        // expected to only be useful to tell whether the experiment produces a signal.
        if (AwContentsLifecycleNotifier.getInstance().getAppState() != AppState.BACKGROUND) return;

        // Comment below from base/android/meminfo_dump_provider.cc:
        //
        // This is best-effort, and will be wrong if there are other callers of
        // ActivityManager#getProcessMemoryInfo(), either in this process or from another
        // process which is allowed to do so (typically, adb).
        //
        // However, since the framework doesn't document throttling in any non-vague terms and
        // the results are not timestamped, this is the best we can do. The delay and the rest
        // of the assumptions here come from
        // https://android.googlesource.com/platform/frameworks/base/+/refs/heads/android13-dev/services/core/java/com/android/server/am/ActivityManagerService.java#4093.
        //
        // We could always report the value on pre-Q devices, but that would skew reported
        // data. Also, some OEMs may have cherry-picked the Q change, meaning that it's safer
        // and more accurate to not report likely-stale data on all Android releases.
        //
        // Nevertheless, this has proved useful to detect whether an experiment is doing
        // *something* for Chromium (the browser, not WebView), where is it collected as part of
        // memory metrics, that are not collected in WebView.
        long now = SystemClock.uptimeMillis();
        if (now - sLastCollectionTime < MEMORY_COLLECTION_INTERVAL_MS) return;
        sLastCollectionTime = now;

        Runnable recordMetrics =
                () -> {
                    Debug.MemoryInfo info = MemoryInfoBridge.getActivityManagerMemoryInfoForSelf();
                    if (info == null) return;

                    RecordHistogram.recordMemoryMediumMBHistogram(
                            PSS_HISTOGRAM, info.otherPss / 1024);
                    RecordHistogram.recordMemoryMediumMBHistogram(
                            PRIVATE_DIRTY_HISTOGRAM, info.otherPrivateDirty / 1024);
                };

        // Record synchronously for testing, to reduce flakiness.
        if (mPostDelayedTaskForTesting != null) {
            recordMetrics.run();
        } else {
            AsyncTask.THREAD_POOL_EXECUTOR.execute(recordMetrics);
        }
    }

    public static void resetRecordMemoryForTesting() {
        sLastCollectionTime = -MEMORY_COLLECTION_INTERVAL_MS;
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

        // Variables that track the state as of the previous onDraw call.
        private Rect mPreviousGlobalVisibleRect = new Rect();
        private boolean mPreviouslyVisible;
        private String mPreviousScheme = "";

        @SuppressLint("DrawAllocation") // For new AwFunctor.
        @Override
        public void onDraw(Canvas canvas) {
            if (isDestroyed(NO_WARN)) {
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

            if (canvas.isHardwareAccelerated() && mDrawFunctor == null) {
                AwFunctor newFunctor;
                AwDrawFnImpl.DrawFnAccess drawFnAccess =
                        mNativeDrawFunctorFactory.getDrawFnAccess();
                if (drawFnAccess != null) {
                    newFunctor = new AwDrawFnImpl(drawFnAccess);
                } else {
                    newFunctor = new AwGLFunctor(mNativeDrawFunctorFactory, mContainerView);
                }
                setFunctor(newFunctor);
            }

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM
                    && AwFeatureMap.isEnabled(VizFeatures.WEBVIEW_FRAME_RATE_HINTS)) {
                float frame_rate = View.REQUESTED_FRAME_RATE_CATEGORY_NO_PREFERENCE;
                if (mPreferredFrameIntervalNanos > 0) {
                    frame_rate = (float) 1e9 / mPreferredFrameIntervalNanos;
                }
                mContainerView.setRequestedFrameRate(frame_rate);
                float velocity =
                        AwContentsJni.get().getVelocityInPixelsPerSecond(mNativeAwContents);
                mContainerView.setFrameContentVelocity(velocity);
            }

            mScrollOffsetManager.syncScrollOffsetFromOnDraw();
            int scrollX = mContainerView.getScrollX();
            int scrollY = mContainerView.getScrollY();
            Rect globalVisibleRect = getGlobalVisibleRect();

            if (mAwWindowCoverageTracker != null) {
                boolean visible = mIsAttachedToWindow && mIsViewVisible && mIsWindowVisible;
                String scheme = AwContentsJni.get().getScheme(mNativeAwContents);

                if (!globalVisibleRect.equals(mPreviousGlobalVisibleRect)
                        || mPreviouslyVisible != visible
                        || !mPreviousScheme.equals(scheme)) {
                    mPreviousGlobalVisibleRect.set(globalVisibleRect);
                    mPreviouslyVisible = visible;
                    mPreviousScheme = scheme;

                    mAwWindowCoverageTracker.onInputsUpdated();
                }
            }

            boolean did_draw =
                    AwContentsJni.get()
                            .onDraw(
                                    mNativeAwContents,
                                    canvas,
                                    canvas.isHardwareAccelerated(),
                                    scrollX,
                                    scrollY,
                                    globalVisibleRect.left,
                                    globalVisibleRect.top,
                                    globalVisibleRect.right,
                                    globalVisibleRect.bottom,
                                    ForceAuxiliaryBitmapRendering.sResult);
            if (canvas.isHardwareAccelerated()
                    && !ForceAuxiliaryBitmapRendering.sResult
                    && AwContentsJni.get().needToDrawBackgroundColor(mNativeAwContents)) {
                TraceEvent.instant("DrawBackgroundColor");
                canvas.drawColor(getEffectiveBackgroundColor());
            }
            if (did_draw
                    && canvas.isHardwareAccelerated()
                    && !ForceAuxiliaryBitmapRendering.sResult) {
                did_draw = mDrawFunctor.requestDraw(canvas);
            }
            if (did_draw) {
                int scrollXDiff = mContainerView.getScrollX() - scrollX;
                int scrollYDiff = mContainerView.getScrollY() - scrollY;
                canvas.translate(-scrollXDiff, -scrollYDiff);
            } else {
                TraceEvent.instant("NativeDrawFailed");
                canvas.drawColor(getEffectiveBackgroundColor());
            }

            if (mOverScrollGlow != null
                    && mOverScrollGlow.drawEdgeGlows(
                            canvas,
                            mScrollOffsetManager.computeMaximumHorizontalScrollOffset(),
                            mScrollOffsetManager.computeMaximumVerticalScrollOffset())) {
                mContainerView.postInvalidateOnAnimation();
            }

            if (mInvalidateRootViewOnNextDraw) {
                mContainerView.getRootView().invalidate();
                mInvalidateRootViewOnNextDraw = false;
            }

            // Tint everything one color, to make WebViews easier to spot.
            if (CommandLine.getInstance().hasSwitch(AwSwitches.HIGHLIGHT_ALL_WEBVIEWS)) {
                int semiTransparentYellow = Color.argb(80, 252, 252, 109);
                canvas.drawColor(semiTransparentYellow);
            }
        }

        @Override
        public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            mLayoutSizer.onMeasure(widthMeasureSpec, heightMeasureSpec);
        }

        @Override
        public void requestFocus() {
            if (isDestroyed(NO_WARN)) return;
            if (!mContainerView.isInTouchMode() && mSettings.shouldFocusFirstNode()) {
                AwContentsJni.get().focusFirstNode(mNativeAwContents);
            }
        }

        @Override
        public void setLayerType(int layerType, Paint paint) {
            mLayerType = layerType;
            updateHardwareAcceleratedFeaturesToggle();
        }

        private void updateHardwareAcceleratedFeaturesToggle() {
            mSettings.setEnableSupportedHardwareAcceleratedFeatures(
                    mIsAttachedToWindow
                            && mContainerView.isHardwareAccelerated()
                            && (mLayerType == View.LAYER_TYPE_NONE
                                    || mLayerType == View.LAYER_TYPE_HARDWARE));
        }

        @Override
        public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
            return isDestroyed(NO_WARN)
                    ? null
                    : ImeAdapter.fromWebContents(mWebContents).onCreateInputConnection(outAttrs);
        }

        @Override
        public boolean onDragEvent(DragEvent event) {
            if (isDestroyed(NO_WARN)) {
                return false;
            }

            if (AwFeatureMap.isEnabled(AwFeatures.WEBVIEW_DRAG_DROP_FILES)) {
                if (event.getAction() == DragEvent.ACTION_DRAG_STARTED) {
                    releaseDragAndDropPermissions();
                } else if (event.getAction() == DragEvent.ACTION_DROP) {
                    Activity activity = ContextUtils.activityFromContext(mContext);
                    if (activity != null) {
                        mDragAndDropPermissions = activity.requestDragAndDropPermissions(event);
                    }
                }
            }
            return mWebContents.getEventForwarder().onDragEvent(event, mContainerView);
        }

        @Override
        public boolean onKeyUp(int keyCode, KeyEvent event) {
            return isDestroyed(NO_WARN)
                    ? false
                    : mWebContents.getEventForwarder().onKeyUp(keyCode, event);
        }

        @Override
        public boolean dispatchKeyEvent(KeyEvent event) {
            if (isDestroyed(NO_WARN)) return false;
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
            if (TouchEventFilter.hasInvalidToolType(event)) return false;
            if (isDestroyed(NO_WARN)) return false;
            if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
                mSettings.setSpatialNavigationEnabled(false);
            }

            AwContentsJni.get().onInputEvent(mNativeAwContents);

            mScrollOffsetManager.setProcessingTouchEvent(true);
            boolean rv = mWebContents.getEventForwarder().onTouchEvent(event);
            mScrollOffsetManager.setProcessingTouchEvent(false);

            if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
                // Note this will trigger IPC back to browser even if nothing is
                // hit.
                float eventX = event.getX();
                float eventY = event.getY();
                float touchMajor = Math.max(event.getTouchMajor(), event.getTouchMinor());
                AwContentsJni.get()
                        .requestNewHitTestDataAt(mNativeAwContents, eventX, eventY, touchMajor);
                // If the stylus is above an editable element, prevent the parent element from
                // intercepting the scroll event.
                if (event.getPointerCount() == 1
                        && event.getToolType(0) == MotionEvent.TOOL_TYPE_STYLUS
                        && getLastHitTestResult().hitTestResultType
                                == 9 /* HitTestDataType::kEditText */) {
                    mContainerView.getParent().requestDisallowInterceptTouchEvent(true);
                }
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
            return isDestroyed(NO_WARN)
                    ? false
                    : mWebContents.getEventForwarder().onHoverEvent(event);
        }

        @Override
        public boolean onGenericMotionEvent(MotionEvent event) {
            return isDestroyed(NO_WARN)
                    ? false
                    : mWebContents.getEventForwarder().onGenericMotionEvent(event);
        }

        @Override
        public void onConfigurationChanged(Configuration newConfig) {
            if (!isDestroyed(NO_WARN)) {
                mViewEventSink.onConfigurationChanged(newConfig);
                mInternalAccessAdapter.super_onConfigurationChanged(newConfig);
            }
        }

        @Override
        public void onAttachedToWindow() {
            if (isDestroyed(NO_WARN)) return;
            if (mIsAttachedToWindow) {
                Log.w(TAG, "onAttachedToWindow called when already attached. Ignoring");
                return;
            }
            mIsAttachedToWindow = true;

            mViewEventSink.onAttachedToWindow();
            AwContentsJni.get()
                    .onAttachedToWindow(
                            mNativeAwContents,
                            mContainerView.getWidth(),
                            mContainerView.getHeight());
            updateHardwareAcceleratedFeaturesToggle();
            postUpdateWebContentsVisibility();

            updateDefaultLocale();

            if (mComponentCallbacks != null) return;
            mComponentCallbacks = new AwComponentCallbacks();
            mContext.registerComponentCallbacks(mComponentCallbacks);
            if (StylusHandwritingFeatureMap.isEnabled(
                    StylusHandwritingFeatureMap.CACHE_STYLUS_SETTINGS)) {
                StylusWritingSettingsState.getInstance().registerObserver(mStylusWritingController);
            }
        }

        @Override
        public void onDetachedFromWindow() {
            if (isDestroyed(NO_WARN)) return;
            if (!mIsAttachedToWindow) {
                Log.w(TAG, "onDetachedFromWindow called when already detached. Ignoring");
                return;
            }
            mIsAttachedToWindow = false;
            hideAutofillPopup();
            AwContentsJni.get().onDetachedFromWindow(mNativeAwContents);

            mViewEventSink.onDetachedFromWindow();
            updateHardwareAcceleratedFeaturesToggle();
            postUpdateWebContentsVisibility();
            setFunctor(null);

            if (mComponentCallbacks != null) {
                mContext.unregisterComponentCallbacks(mComponentCallbacks);
                mComponentCallbacks = null;
            }

            if (StylusHandwritingFeatureMap.isEnabled(
                    StylusHandwritingFeatureMap.CACHE_STYLUS_SETTINGS)) {
                StylusWritingSettingsState.getInstance()
                        .unregisterObserver(mStylusWritingController);
            }

            mScrollAccessibilityHelper.removePostedCallbacks();
            mZoomControls.dismissZoomPicker();
        }

        @Override
        public void onWindowFocusChanged(boolean hasWindowFocus) {
            if (isDestroyed(NO_WARN)) return;
            mWindowFocused = hasWindowFocus;
            mViewEventSink.onWindowFocusChanged(hasWindowFocus);
            mStylusWritingController.onWindowFocusChanged(hasWindowFocus);
            Clipboard.getInstance().onWindowFocusChanged(hasWindowFocus);
        }

        @Override
        public void onFocusChanged(boolean focused, int direction, Rect previouslyFocusedRect) {
            if (isDestroyed(NO_WARN)) return;
            mContainerViewFocused = focused;
            mViewEventSink.onViewFocusChanged(focused);
        }

        @Override
        public void onSizeChanged(int w, int h, int ow, int oh) {
            if (isDestroyed(NO_WARN)) return;
            mScrollOffsetManager.setContainerViewSize(w, h);
            // The AwLayoutSizer needs to go first so that if we're in
            // fixedLayoutSize mode the update to enter fixedLayoutSize mode is sent before the
            // first resize update.
            mLayoutSizer.onSizeChanged(w, h, ow, oh);
            AwContentsJni.get().onSizeChanged(mNativeAwContents, w, h, ow, oh);
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
        public void onContainerViewOverScrolled(
                int scrollX, int scrollY, boolean clampedX, boolean clampedY) {
            int oldX = mContainerView.getScrollX();
            int oldY = mContainerView.getScrollY();

            mScrollOffsetManager.onContainerViewOverScrolled(scrollX, scrollY, clampedX, clampedY);

            if (mOverScrollGlow != null) {
                mOverScrollGlow.pullGlow(
                        mContainerView.getScrollX(),
                        mContainerView.getScrollY(),
                        oldX,
                        oldY,
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
            if (isDestroyed(NO_WARN)) return;
            AwContentsJni.get()
                    .onComputeScroll(
                            mNativeAwContents, AnimationUtils.currentAnimationTimeMillis());
        }

        @Override
        public boolean onCheckIsTextEditor() {
            if (isDestroyed(NO_WARN)) return false;
            ImeAdapter imeAdapter = ImeAdapter.fromWebContents(mWebContents);
            return imeAdapter != null ? imeAdapter.onCheckIsTextEditor() : false;
        }

        @Override
        public AccessibilityNodeProvider getAccessibilityNodeProvider() {
            if (isDestroyed(NO_WARN)) return null;
            WebContentsAccessibility wcax = getWebContentsAccessibility();
            return wcax != null ? wcax.getAccessibilityNodeProvider() : null;
        }

        @Override
        public boolean performAccessibilityAction(final int action, final Bundle arguments) {
            return false;
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

    @NativeMethods
    interface Natives {
        long init(long browserContextPointer);

        void destroy(long nativeAwContents);

        boolean hasRequiredHardwareExtensions();

        void setAwDrawSWFunctionTable(long functionTablePointer);

        void setAwDrawGLFunctionTable(long functionTablePointer);

        int getNativeInstanceCount();

        void setShouldDownloadFavicons();

        void updateDefaultLocale(String locale, String localeList);

        String getSafeBrowsingLocaleForTesting();

        AwContents fromWebContents(WebContents webContents);

        void setJavaPeers(
                long nativeAwContents,
                AwContents awContents,
                AwWebContentsDelegate webViewWebContentsDelegate,
                AwContentsClientBridge contentsClientBridge,
                AwContentsIoThreadClient ioThreadClient,
                InterceptNavigationDelegate navigationInterceptionDelegate);

        void initializeAndroidAutofill(long nativeAwContents);

        void initSensitiveContentClient(long nativeAwContents);

        WebContents getWebContents(long nativeAwContents);

        AwBrowserContext getBrowserContext(long nativeAwContents);

        void setCompositorFrameConsumer(long nativeAwContents, long nativeCompositorFrameConsumer);

        void documentHasImages(long nativeAwContents, Message message);

        void generateMHTML(long nativeAwContents, String path, Callback<String> callback);

        void addVisitedLinks(long nativeAwContents, String[] visitedLinks);

        void zoomBy(long nativeAwContents, float delta);

        void onComputeScroll(long nativeAwContents, long currentAnimationTimeMillis);

        boolean onDraw(
                long nativeAwContents,
                Canvas canvas,
                boolean isHardwareAccelerated,
                int scrollX,
                int scrollY,
                int visibleLeft,
                int visibleTop,
                int visibleRight,
                int visibleBottom,
                boolean forceAuxiliaryBitmapRendering);

        float getVelocityInPixelsPerSecond(long nativeAwContents);

        boolean needToDrawBackgroundColor(long nativeAwContents);

        void findAllAsync(long nativeAwContents, String searchString);

        void findNext(long nativeAwContents, boolean forward);

        void clearMatches(long nativeAwContents);

        void clearCache(long nativeAwContents, boolean includeDiskFiles);

        byte[] getCertificate(long nativeAwContents);

        // Coordinates are in physical pixels when --use-zoom-for-dsf is enabled.
        // Otherwise, coordinates are in desity independent pixels.
        void requestNewHitTestDataAt(long nativeAwContents, float x, float y, float touchMajor);

        void updateLastHitTestData(long nativeAwContents);

        void onSizeChanged(long nativeAwContents, int w, int h, int ow, int oh);

        void scrollTo(long nativeAwContents, int x, int y);

        void restoreScrollAfterTransition(long nativeAwContents, int x, int y);

        void smoothScroll(long nativeAwContents, int targetX, int targetY, long durationMs);

        void setViewVisibility(long nativeAwContents, boolean visible);

        void setWindowVisibility(long nativeAwContents, boolean visible);

        void setIsPaused(long nativeAwContents, boolean paused);

        void onAttachedToWindow(long nativeAwContents, int w, int h);

        void onDetachedFromWindow(long nativeAwContents);

        boolean isVisible(long nativeAwContents);

        boolean isDisplayingInterstitialForTesting(long nativeAwContents);

        void setDipScale(long nativeAwContents, float dipScale);

        String getScheme(long nativeAwContents);

        void updateScreenCoverage(int globalPercentage, String[] schemes, int[] schemePercentages);

        void onInputEvent(long nativeAwContents);

        // Returns null if save state fails.
        byte[] getOpaqueState(long nativeAwContents);

        // Returns false if restore state fails.
        boolean restoreFromOpaqueState(long nativeAwContents, byte[] state);

        long releasePopupAwContents(long nativeAwContents);

        void focusFirstNode(long nativeAwContents);

        void setBackgroundColor(long nativeAwContents, int color);

        long capturePicture(long nativeAwContents, int width, int height);

        void enableOnNewPicture(long nativeAwContents, boolean enabled);

        void insertVisualStateCallback(
                long nativeAwContents, long requestId, VisualStateCallback callback);

        void clearView(long nativeAwContents);

        void setExtraHeadersForUrl(long nativeAwContents, String url, String extraHeaders);

        void invokeGeolocationCallback(
                long nativeAwContents, boolean value, String requestingFrame);

        int getEffectivePriority(long nativeAwContents);

        void setJsOnlineProperty(long nativeAwContents, boolean networkUp);

        void trimMemory(long nativeAwContents, int level, boolean visible);

        void createPdfExporter(long nativeAwContents, AwPdfExporter awPdfExporter);

        void preauthorizePermission(long nativeAwContents, String origin, long resources);

        void grantFileSchemeAccesstoChildProcess(long nativeAwContents);

        void resumeLoadingCreatedPopupWebContents(long nativeAwContents);

        AwRenderProcess getRenderProcess(long nativeAwContents);

        int addDocumentStartJavaScript(
                long nativeAwContents, String script, String[] allowedOriginRules);

        void removeDocumentStartJavaScript(long nativeAwContents, int scriptId);

        String addWebMessageListener(
                long nativeAwContents,
                WebMessageListenerHolder listener,
                String jsObjectName,
                String[] allowedOrigins);

        void removeWebMessageListener(long nativeAwContents, String jsObjectName);

        @JniType("std::vector")
        WebMessageListenerInfo[] getWebMessageListenerInfos(long nativeAwContents);

        @JniType("std::vector")
        StartupJavascriptInfo[] getDocumentStartupJavascripts(long nativeAwContents);

        void onConfigurationChanged(long nativeAwContents);

        void flushBackForwardCache(long nativeAwContents, int reason);

        void cancelAllPrerendering(long nativeAwContents);
    }
}
