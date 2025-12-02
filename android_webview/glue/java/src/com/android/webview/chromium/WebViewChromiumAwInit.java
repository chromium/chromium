// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.app.compat.CompatChanges;
import android.content.Context;
import android.content.res.Resources;
import android.os.Build;
import android.os.Looper;
import android.os.SystemClock;
import android.util.Log;
import android.webkit.CookieManager;
import android.webkit.WebSettings;
import android.webkit.WebViewDatabase;

import androidx.annotation.GuardedBy;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.android.webview.chromium.WebViewChromium.ApiCall;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.AwClassPreloader;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsStatics;
import org.chromium.android_webview.AwCookieManager;
import org.chromium.android_webview.AwCrashyClassUtils;
import org.chromium.android_webview.AwDarkMode;
import org.chromium.android_webview.AwLocaleConfig;
import org.chromium.android_webview.AwProxyController;
import org.chromium.android_webview.AwThreadUtils;
import org.chromium.android_webview.AwTracingController;
import org.chromium.android_webview.DualTraceEvent;
import org.chromium.android_webview.HttpAuthDatabase;
import org.chromium.android_webview.R;
import org.chromium.android_webview.WebViewChromiumRunQueue;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.common.AwResource;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.android_webview.common.WebViewCachedFlags;
import org.chromium.android_webview.gfx.AwDrawFnImpl;
import org.chromium.android_webview.metrics.TrackExitReasons;
import org.chromium.android_webview.variations.FastVariationsSeedSafeModeAction;
import org.chromium.android_webview.variations.VariationsSeedLoader;
import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.ApkInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.EarlyTraceEvent;
import org.chromium.base.PathService;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LoaderErrors;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.build.BuildConfig;
import org.chromium.content_public.browser.BrowserStartupController.StartupCallback;
import org.chromium.content_public.browser.BrowserStartupController.StartupMetrics;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ResourceBundle;

import java.util.ArrayDeque;
import java.util.Locale;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Class controlling the Chromium initialization for WebView. We hold on to most static objects used
 * by WebView here. This class is shared between the webkit glue layer and the support library glue
 * layer.
 */
@Lifetime.Singleton
public class WebViewChromiumAwInit {
    private static final String TAG = "WebViewChromiumAwInit";

    private static final String HTTP_AUTH_DATABASE_FILE = "http_auth.db";

    private static final String ASSET_PATH_WORKAROUND_HISTOGRAM_NAME =
            "Android.WebView.AssetPathWorkaroundUsed.StartChromiumLocked";

    public static class WebViewStartUpDiagnostics {
        private final Object mLock = new Object();

        @GuardedBy("mLock")
        private Long mTotalTimeUiThreadChromiumInitMillis;

        @GuardedBy("mLock")
        private Long mMaxTimePerTaskUiThreadChromiumInitMillis;

        @GuardedBy("mLock")
        private Throwable mSynchronousChromiumInitLocation;

        @GuardedBy("mLock")
        private Throwable mProviderInitOnMainLooperLocation;

        @GuardedBy("mLock")
        private Throwable mAsynchronousChromiumInitLocation;

        public Long getTotalTimeUiThreadChromiumInitMillis() {
            synchronized (mLock) {
                return mTotalTimeUiThreadChromiumInitMillis;
            }
        }

        public Long getMaxTimePerTaskUiThreadChromiumInitMillis() {
            synchronized (mLock) {
                return mMaxTimePerTaskUiThreadChromiumInitMillis;
            }
        }

        public @Nullable Throwable getSynchronousChromiumInitLocationOrNull() {
            synchronized (mLock) {
                return mSynchronousChromiumInitLocation;
            }
        }

        public @Nullable Throwable getProviderInitOnMainLooperLocationOrNull() {
            synchronized (mLock) {
                return mProviderInitOnMainLooperLocation;
            }
        }

        public @Nullable Throwable getAsynchronousChromiumInitLocationOrNull() {
            synchronized (mLock) {
                return mAsynchronousChromiumInitLocation;
            }
        }

        void setTotalTimeUiThreadChromiumInitMillis(Long time) {
            synchronized (mLock) {
                // The setter should only be called once.
                assert (mTotalTimeUiThreadChromiumInitMillis == null);
                mTotalTimeUiThreadChromiumInitMillis = time;
            }
        }

        void setMaxTimePerTaskUiThreadChromiumInitMillis(Long time) {
            synchronized (mLock) {
                // The setter should only be called once.
                assert (mMaxTimePerTaskUiThreadChromiumInitMillis == null);
                mMaxTimePerTaskUiThreadChromiumInitMillis = time;
            }
        }

        void setSynchronousChromiumInitLocation(Throwable t) {
            synchronized (mLock) {
                // The setter should only be called once.
                assert (mSynchronousChromiumInitLocation == null);
                mSynchronousChromiumInitLocation = t;
            }
        }

        void setProviderInitOnMainLooperLocation(Throwable t) {
            synchronized (mLock) {
                // The setter should only be called once.
                assert (mProviderInitOnMainLooperLocation == null);
                mProviderInitOnMainLooperLocation = t;
            }
        }

        void setAsynchronousChromiumInitLocation(Throwable t) {
            synchronized (mLock) {
                // The setter should only be called once.
                assert (mAsynchronousChromiumInitLocation == null);
                mAsynchronousChromiumInitLocation = t;
            }
        }
    }

    public interface WebViewStartUpCallback {
        void onSuccess(WebViewStartUpDiagnostics result);
    }

    @GuardedBy("mLazyInitLock")
    private CookieManagerAdapter mDefaultCookieManager;

    @GuardedBy("mLazyInitLock")
    private WebIconDatabaseAdapter mWebIconDatabase;

    @GuardedBy("mLazyInitLock")
    private WebViewDatabaseAdapter mDefaultWebViewDatabase;

    // Volatile to guard for incorrectly trying to use this without calling `startChromium`.
    // TODO(crbug.com/389871700): Consider hiding the variable where it can't be incorrectly
    // accessed. See crrev.com/c/6081452/comment/9dff4e5e_c049d778/ for context.
    private volatile ChromiumStartedGlobals mChromiumStartedGlobals;

    private final DefaultProfileHolder mDefaultProfileHolder = new DefaultProfileHolder();

    private final Object mSeedLoaderLock = new Object();

    @GuardedBy("mSeedLoaderLock")
    private VariationsSeedLoader mSeedLoader;

    // This is only accessed during WebViewChromiumFactoryProvider.initialize() which is guarded by
    // the WebViewFactory lock in the framework, and on the UI thread during startChromium
    // which cannot be called before initialize() has completed.
    private Thread mSetUpResourcesThread;

    // Guards access to fields that are initialized on first use rather than by startChromium.
    // This lock is used across WebViewChromium startup classes ie WebViewChromiumAwInit,
    // SupportLibWebViewChromiumFactory and WebViewChromiumFactoryProvider so as to avoid deadlock.
    // TODO(crbug.com/397385172): Get rid of this lock.
    private final Object mLazyInitLock = new Object();

    private final Object mThreadSettingLock = new Object();

    @GuardedBy("mThreadSettingLock")
    private boolean mThreadIsSet;

    private final CountDownLatch mStartupFinished = new CountDownLatch(1);

    // mInitState should only transition from INIT_NOT_STARTED to INIT_FINISHED with possibly
    // INIT_POSTED as an intermediate state. INIT_POSTED is set right before posting `startChromium`
    // on the UI thread in case of async startup.
    private static final int INIT_NOT_STARTED = 0;
    private static final int INIT_POSTED = 1;
    private static final int INIT_FINISHED = 2;

    private final AtomicInteger mInitState = new AtomicInteger(INIT_NOT_STARTED);

    // Looper on which `getDefaultCookieManager` is called for the first time.
    private final AtomicReference<Looper> mFirstGetDefaultCookieManagerLooper =
            new AtomicReference<Looper>();
    // Set to true if/when `getDefaultCookieManager` is called.
    private final AtomicBoolean mGetDefaultCookieManagerCalled = new AtomicBoolean(false);

    private final WebViewChromiumFactoryProvider mFactory;
    private final WebViewStartUpDiagnostics mWebViewStartUpDiagnostics =
            new WebViewStartUpDiagnostics();
    private final WebViewChromiumRunQueue mWebViewStartUpCallbackRunQueue =
            new WebViewChromiumRunQueue();

    private final AtomicInteger mChromiumFirstStartupRequestMode =
            new AtomicInteger(StartupTasksRunner.UNSET);
    // Only accessed from the UI thread
    private StartupTasksRunner mStartupTasksRunner;
    private RuntimeException mStartupException;
    private Error mStartupError;
    private boolean mIsStartupTaskExperimentEnabled;
    private boolean mIsStartupTaskExperimentP2Enabled;
    private boolean mIsStartupTasksYieldToNativeExperimentEnabled;

    private volatile boolean mShouldInitializeDefaultProfile = true;

    // TODO: DIR_RESOURCE_PAKS_ANDROID needs to live somewhere sensible,
    // inlined here for simplicity setting up the HTMLViewer demo. Unfortunately
    // it can't go into base.PathService, as the native constant it refers to
    // lives in the ui/ layer. See ui/base/ui_base_paths.h
    private static final int DIR_RESOURCE_PAKS_ANDROID = 3003;

    // This enum must be kept in sync with WebViewStartup.CallSite in chrome_track_event.proto and
    // WebViewStartupCallSite in enums.xml.
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    // LINT.IfChange(CallSite)
    @IntDef({
        CallSite.GET_AW_TRACING_CONTROLLER,
        CallSite.GET_AW_PROXY_CONTROLLER,
        CallSite.GET_DEFAULT_GEOLOCATION_PERMISSIONS,
        CallSite.GET_DEFAULT_SERVICE_WORKER_CONTROLLER,
        CallSite.GET_WEB_ICON_DATABASE,
        CallSite.GET_DEFAULT_WEB_STORAGE,
        CallSite.GET_DEFAULT_WEBVIEW_DATABASE,
        CallSite.GET_TRACING_CONTROLLER,
        CallSite.ASYNC_WEBVIEW_STARTUP,
        CallSite.WEBVIEW_INSTANCE_OVERLAY_HORIZONTAL_SCROLLBAR,
        CallSite.WEBVIEW_INSTANCE_OVERLAY_VERTICAL_SCROLLBAR,
        CallSite.WEBVIEW_INSTANCE_GET_CERTIFICATE,
        CallSite.WEBVIEW_INSTANCE_GET_HTTP_AUTH_USERNAME_PASSWORD,
        CallSite.WEBVIEW_INSTANCE_SAVE_STATE,
        CallSite.WEBVIEW_INSTANCE_RESTORE_STATE,
        CallSite.WEBVIEW_INSTANCE_LOAD_URL,
        CallSite.WEBVIEW_INSTANCE_POST_URL,
        CallSite.WEBVIEW_INSTANCE_LOAD_DATA,
        CallSite.WEBVIEW_INSTANCE_LOAD_DATA_WITH_BASE_URL,
        CallSite.WEBVIEW_INSTANCE_EVALUATE_JAVASCRIPT,
        CallSite.WEBVIEW_INSTANCE_CAN_GO_BACK,
        CallSite.WEBVIEW_INSTANCE_CAN_GO_FORWARD,
        CallSite.WEBVIEW_INSTANCE_CAN_GO_BACK_OR_FORWARD,
        CallSite.WEBVIEW_INSTANCE_IS_PAUSED,
        CallSite.WEBVIEW_INSTANCE_COPY_BACK_FORWARD_LIST,
        CallSite.WEBVIEW_INSTANCE_SHOW_FIND_DIALOG,
        CallSite.WEBVIEW_INSTANCE_SET_WEBVIEW_CLIENT,
        CallSite.WEBVIEW_INSTANCE_SET_WEBCHROME_CLIENT,
        CallSite.WEBVIEW_INSTANCE_CREATE_WEBMESSAGE_CHANNEL,
        CallSite.WEBVIEW_INSTANCE_GET_ZOOM_CONTROLS,
        CallSite.WEBVIEW_INSTANCE_ZOOM_IN,
        CallSite.WEBVIEW_INSTANCE_ZOOM_OUT,
        CallSite.WEBVIEW_INSTANCE_ZOOM_BY,
        CallSite.WEBVIEW_INSTANCE_SET_RENDERER_PRIORITY_POLICY,
        CallSite.WEBVIEW_INSTANCE_GET_RENDERER_REQUESTED_PRIORITY,
        CallSite.WEBVIEW_INSTANCE_GET_RENDERER_PRIORITY_WAIVED_WHEN_NOT_VISIBLE,
        CallSite.WEBVIEW_INSTANCE_SET_TEXT_CLASSIFIER,
        CallSite.WEBVIEW_INSTANCE_GET_TEXT_CLASSIFIER,
        CallSite.WEBVIEW_INSTANCE_AUTOFILL,
        CallSite.WEBVIEW_INSTANCE_ON_PROVIDE_AUTOFILL_VIRTUAL_STRUCTURE,
        CallSite.WEBVIEW_INSTANCE_ON_PROVIDE_CONTENT_CAPTURE_STRUCTURE,
        CallSite.WEBVIEW_INSTANCE_SHOULD_DELAY_CHILD_PRESSED_STATE,
        CallSite.WEBVIEW_INSTANCE_GET_ACCESSIBILITY_NODE_PROVIDER,
        CallSite.WEBVIEW_INSTANCE_ON_PROVIDE_VIRTUAL_STRUCTURE,
        CallSite.WEBVIEW_INSTANCE_PERFORM_ACCESSIBILITY_ACTION,
        CallSite.WEBVIEW_INSTANCE_ON_DRAW,
        CallSite.WEBVIEW_INSTANCE_SET_LAYOUT_PARAMS,
        CallSite.WEBVIEW_INSTANCE_ON_DRAG_EVENT,
        CallSite.WEBVIEW_INSTANCE_ON_CREATE_INPUT_CONNECTION,
        CallSite.WEBVIEW_INSTANCE_ON_KEY_MULTIPLE,
        CallSite.WEBVIEW_INSTANCE_ON_KEY_DOWN,
        CallSite.WEBVIEW_INSTANCE_ON_KEY_UP,
        CallSite.WEBVIEW_INSTANCE_ON_ATTACHED_TO_WINDOW,
        CallSite.WEBVIEW_INSTANCE_DISPATCH_KEY_EVENT,
        CallSite.WEBVIEW_INSTANCE_ON_TOUCH_EVENT,
        CallSite.WEBVIEW_INSTANCE_ON_HOVER_EVENT,
        CallSite.WEBVIEW_INSTANCE_ON_GENERIC_MOTION_EVENT,
        CallSite.WEBVIEW_INSTANCE_REQUEST_FOCUS,
        CallSite.WEBVIEW_INSTANCE_ON_MEASURE,
        CallSite.WEBVIEW_INSTANCE_REQUEST_CHILD_RECTANGLE_ON_SCREEN,
        CallSite.WEBVIEW_INSTANCE_SET_BACKGROUND_COLOR,
        CallSite.WEBVIEW_INSTANCE_ON_START_TEMPORARY_DETACH,
        CallSite.WEBVIEW_INSTANCE_ON_FINISH_TEMPORARY_DETACH,
        CallSite.WEBVIEW_INSTANCE_ON_CHECK_IS_TEXT_EDITOR,
        CallSite.WEBVIEW_INSTANCE_ON_APPLY_WINDOW_INSETS,
        CallSite.WEBVIEW_INSTANCE_ON_RESOLVE_POINTER_ICON,
        CallSite.WEBVIEW_INSTANCE_COMPUTE_HORIZONTAL_SCROLL_RANGE,
        CallSite.WEBVIEW_INSTANCE_COMPUTE_HORIZONTAL_SCROLL_OFFSET,
        CallSite.WEBVIEW_INSTANCE_COMPUTE_VERTICAL_SCROLL_RANGE,
        CallSite.WEBVIEW_INSTANCE_COMPUTE_VERTICAL_SCROLL_OFFSET,
        CallSite.WEBVIEW_INSTANCE_COMPUTE_VERTICAL_SCROLL_EXTENT,
        CallSite.WEBVIEW_INSTANCE_COMPUTE_SCROLL,
        CallSite.WEBVIEW_INSTANCE_CREATE_PRINT_DOCUMENT_ADAPTER,
        CallSite.WEBVIEW_INSTANCE_EXTRACT_SMART_CLIP_DATA,
        CallSite.WEBVIEW_INSTANCE_SET_SMART_CLIP_RESULT_HANDLER,
        CallSite.WEBVIEW_INSTANCE_GET_RENDER_PROCESS,
        CallSite.WEBVIEW_INSTANCE_GET_WEBVIEW_RENDERER_CLIENT_ADAPTER,
        CallSite.WEBVIEW_INSTANCE_PAGE_UP,
        CallSite.WEBVIEW_INSTANCE_PAGE_DOWN,
        CallSite.WEBVIEW_INSTANCE_LOAD_URL_ADDITIONAL_HEADERS,
        CallSite.WEBVIEW_INSTANCE_INIT,
        CallSite.WEBVIEW_INSTANCE_CAPTURE_PICTURE,
        CallSite.WEBVIEW_INSTANCE_GET_SCALE,
        CallSite.WEBVIEW_INSTANCE_SET_INITIAL_SCALE,
        CallSite.WEBVIEW_INSTANCE_GET_HIT_TEST_RESULT,
        CallSite.WEBVIEW_INSTANCE_GET_URL,
        CallSite.WEBVIEW_INSTANCE_GET_ORIGINAL_URL,
        CallSite.WEBVIEW_INSTANCE_GET_TITLE,
        CallSite.WEBVIEW_INSTANCE_GET_FAVICON,
        CallSite.STATIC_FIND_ADDRESS,
        CallSite.STATIC_GET_DEFAULT_USER_AGENT,
        CallSite.STATIC_SET_WEB_CONTENTS_DEBUGGING_ENABLED,
        CallSite.STATIC_CLEAR_CLIENT_CERT_PREFERENCES,
        CallSite.STATIC_FREE_MEMORY_FOR_TESTS,
        CallSite.STATIC_ENABLE_SLOW_WHOLE_DOCUMENT_DRAW,
        CallSite.STATIC_PARSE_FILE_CHOOSER_RESULT,
        CallSite.STATIC_INIT_SAFE_BROWSING,
        CallSite.STATIC_SET_SAFE_BROWSING_ALLOWLIST,
        CallSite.STATIC_GET_SAFE_BROWSING_PRIVACY_POLICY_URL,
        CallSite.STATIC_IS_MULTI_PROCESS_ENABLED,
        CallSite.STATIC_GET_VARIATIONS_HEADER,
        CallSite.STATIC_SET_RENDERER_LIBRARY_PREFETCH_MODE,
        CallSite.STATIC_GET_RENDERER_LIBRARY_PREFETCH_MODE,
        CallSite.GET_DEFAULT_COOKIE_MANAGER,
        CallSite.COUNT,
    })
    public @interface CallSite {
        int GET_AW_TRACING_CONTROLLER = 0;
        int GET_AW_PROXY_CONTROLLER = 1;
        // Value 2 was used as a catch all for all WebView instance methods.
        // Value 3 was used as a catch all for all static methods.
        // Both values 2 and 3 are deprecated and should no longer be used.
        int GET_DEFAULT_GEOLOCATION_PERMISSIONS = 4;
        int GET_DEFAULT_SERVICE_WORKER_CONTROLLER = 5;
        int GET_WEB_ICON_DATABASE = 6;
        int GET_DEFAULT_WEB_STORAGE = 7;
        int GET_DEFAULT_WEBVIEW_DATABASE = 8;
        int GET_TRACING_CONTROLLER = 9;
        int ASYNC_WEBVIEW_STARTUP = 10;
        int WEBVIEW_INSTANCE_OVERLAY_HORIZONTAL_SCROLLBAR = 11;
        int WEBVIEW_INSTANCE_OVERLAY_VERTICAL_SCROLLBAR = 12;
        int WEBVIEW_INSTANCE_GET_CERTIFICATE = 13;
        int WEBVIEW_INSTANCE_GET_HTTP_AUTH_USERNAME_PASSWORD = 14;
        int WEBVIEW_INSTANCE_SAVE_STATE = 15;
        int WEBVIEW_INSTANCE_RESTORE_STATE = 16;
        int WEBVIEW_INSTANCE_LOAD_URL = 17;
        int WEBVIEW_INSTANCE_POST_URL = 18;
        int WEBVIEW_INSTANCE_LOAD_DATA = 19;
        int WEBVIEW_INSTANCE_LOAD_DATA_WITH_BASE_URL = 20;
        int WEBVIEW_INSTANCE_EVALUATE_JAVASCRIPT = 21;
        int WEBVIEW_INSTANCE_CAN_GO_BACK = 22;
        int WEBVIEW_INSTANCE_CAN_GO_FORWARD = 23;
        int WEBVIEW_INSTANCE_CAN_GO_BACK_OR_FORWARD = 24;
        int WEBVIEW_INSTANCE_IS_PAUSED = 25;
        int WEBVIEW_INSTANCE_COPY_BACK_FORWARD_LIST = 26;
        int WEBVIEW_INSTANCE_SHOW_FIND_DIALOG = 27;
        int WEBVIEW_INSTANCE_SET_WEBVIEW_CLIENT = 28;
        int WEBVIEW_INSTANCE_SET_WEBCHROME_CLIENT = 29;
        int WEBVIEW_INSTANCE_CREATE_WEBMESSAGE_CHANNEL = 30;
        int WEBVIEW_INSTANCE_GET_ZOOM_CONTROLS = 31;
        int WEBVIEW_INSTANCE_ZOOM_IN = 32;
        int WEBVIEW_INSTANCE_ZOOM_OUT = 33;
        int WEBVIEW_INSTANCE_ZOOM_BY = 34;
        int WEBVIEW_INSTANCE_SET_RENDERER_PRIORITY_POLICY = 35;
        int WEBVIEW_INSTANCE_GET_RENDERER_REQUESTED_PRIORITY = 36;
        int WEBVIEW_INSTANCE_GET_RENDERER_PRIORITY_WAIVED_WHEN_NOT_VISIBLE = 37;
        int WEBVIEW_INSTANCE_SET_TEXT_CLASSIFIER = 38;
        int WEBVIEW_INSTANCE_GET_TEXT_CLASSIFIER = 39;
        int WEBVIEW_INSTANCE_AUTOFILL = 40;
        int WEBVIEW_INSTANCE_ON_PROVIDE_AUTOFILL_VIRTUAL_STRUCTURE = 41;
        int WEBVIEW_INSTANCE_ON_PROVIDE_CONTENT_CAPTURE_STRUCTURE = 42;
        int WEBVIEW_INSTANCE_SHOULD_DELAY_CHILD_PRESSED_STATE = 43;
        int WEBVIEW_INSTANCE_GET_ACCESSIBILITY_NODE_PROVIDER = 44;
        int WEBVIEW_INSTANCE_ON_PROVIDE_VIRTUAL_STRUCTURE = 45;
        int WEBVIEW_INSTANCE_PERFORM_ACCESSIBILITY_ACTION = 46;
        int WEBVIEW_INSTANCE_ON_DRAW = 47;
        int WEBVIEW_INSTANCE_SET_LAYOUT_PARAMS = 48;
        int WEBVIEW_INSTANCE_ON_DRAG_EVENT = 49;
        int WEBVIEW_INSTANCE_ON_CREATE_INPUT_CONNECTION = 50;
        int WEBVIEW_INSTANCE_ON_KEY_MULTIPLE = 51;
        int WEBVIEW_INSTANCE_ON_KEY_DOWN = 52;
        int WEBVIEW_INSTANCE_ON_KEY_UP = 53;
        int WEBVIEW_INSTANCE_ON_ATTACHED_TO_WINDOW = 54;
        int WEBVIEW_INSTANCE_DISPATCH_KEY_EVENT = 55;
        int WEBVIEW_INSTANCE_ON_TOUCH_EVENT = 56;
        int WEBVIEW_INSTANCE_ON_HOVER_EVENT = 57;
        int WEBVIEW_INSTANCE_ON_GENERIC_MOTION_EVENT = 58;
        int WEBVIEW_INSTANCE_REQUEST_FOCUS = 59;
        int WEBVIEW_INSTANCE_ON_MEASURE = 60;
        int WEBVIEW_INSTANCE_REQUEST_CHILD_RECTANGLE_ON_SCREEN = 61;
        int WEBVIEW_INSTANCE_SET_BACKGROUND_COLOR = 62;
        int WEBVIEW_INSTANCE_ON_START_TEMPORARY_DETACH = 63;
        int WEBVIEW_INSTANCE_ON_FINISH_TEMPORARY_DETACH = 64;
        int WEBVIEW_INSTANCE_ON_CHECK_IS_TEXT_EDITOR = 65;
        int WEBVIEW_INSTANCE_ON_APPLY_WINDOW_INSETS = 66;
        int WEBVIEW_INSTANCE_ON_RESOLVE_POINTER_ICON = 67;
        int WEBVIEW_INSTANCE_COMPUTE_HORIZONTAL_SCROLL_RANGE = 68;
        int WEBVIEW_INSTANCE_COMPUTE_HORIZONTAL_SCROLL_OFFSET = 69;
        int WEBVIEW_INSTANCE_COMPUTE_VERTICAL_SCROLL_RANGE = 70;
        int WEBVIEW_INSTANCE_COMPUTE_VERTICAL_SCROLL_OFFSET = 71;
        int WEBVIEW_INSTANCE_COMPUTE_VERTICAL_SCROLL_EXTENT = 72;
        int WEBVIEW_INSTANCE_COMPUTE_SCROLL = 73;
        int WEBVIEW_INSTANCE_CREATE_PRINT_DOCUMENT_ADAPTER = 74;
        int WEBVIEW_INSTANCE_EXTRACT_SMART_CLIP_DATA = 75;
        int WEBVIEW_INSTANCE_SET_SMART_CLIP_RESULT_HANDLER = 76;
        int WEBVIEW_INSTANCE_GET_RENDER_PROCESS = 77;
        int WEBVIEW_INSTANCE_GET_WEBVIEW_RENDERER_CLIENT_ADAPTER = 78;
        int WEBVIEW_INSTANCE_PAGE_UP = 79;
        int WEBVIEW_INSTANCE_PAGE_DOWN = 80;
        int WEBVIEW_INSTANCE_LOAD_URL_ADDITIONAL_HEADERS = 81;
        int WEBVIEW_INSTANCE_INIT = 82;
        int WEBVIEW_INSTANCE_CAPTURE_PICTURE = 83;
        int WEBVIEW_INSTANCE_GET_SCALE = 84;
        int WEBVIEW_INSTANCE_SET_INITIAL_SCALE = 85;
        int WEBVIEW_INSTANCE_GET_HIT_TEST_RESULT = 86;
        int WEBVIEW_INSTANCE_GET_URL = 87;
        int WEBVIEW_INSTANCE_GET_ORIGINAL_URL = 88;
        int WEBVIEW_INSTANCE_GET_TITLE = 89;
        int WEBVIEW_INSTANCE_GET_FAVICON = 90;
        int STATIC_FIND_ADDRESS = 91;
        int STATIC_GET_DEFAULT_USER_AGENT = 92;
        int STATIC_SET_WEB_CONTENTS_DEBUGGING_ENABLED = 93;
        int STATIC_CLEAR_CLIENT_CERT_PREFERENCES = 94;
        int STATIC_FREE_MEMORY_FOR_TESTS = 95;
        int STATIC_ENABLE_SLOW_WHOLE_DOCUMENT_DRAW = 96;
        int STATIC_PARSE_FILE_CHOOSER_RESULT = 97;
        int STATIC_INIT_SAFE_BROWSING = 98;
        int STATIC_SET_SAFE_BROWSING_ALLOWLIST = 99;
        int STATIC_GET_SAFE_BROWSING_PRIVACY_POLICY_URL = 100;
        int STATIC_IS_MULTI_PROCESS_ENABLED = 101;
        int STATIC_GET_VARIATIONS_HEADER = 102;
        // Values 103 and 104 were used for traffic stats, which no longer start up chromium.
        int STATIC_SET_RENDERER_LIBRARY_PREFETCH_MODE = 105;
        int STATIC_GET_RENDERER_LIBRARY_PREFETCH_MODE = 106;
        int GET_DEFAULT_COOKIE_MANAGER = 107;
        // Remember to update WebViewStartupCallSite in enums.xml when adding new values here.
        int COUNT = 108;
    };

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:WebViewStartupCallSite)

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        CookieManagerThreadingCondition.NOT_CALLED_BEFORE_UI_THREAD_SET,
        CookieManagerThreadingCondition.CALLED_ON_NON_LOOPER_THREAD,
        CookieManagerThreadingCondition.CALLED_FROM_BACKGROUND_LOOPER_AND_UI_THREAD_IS_MAIN_LOOPER,
        CookieManagerThreadingCondition
                .CALLED_FROM_BACKGROUND_LOOPER_AND_UI_THREAD_IS_SAME_BACKGROUND_LOOPER,
        CookieManagerThreadingCondition
                .CALLED_FROM_BACKGROUND_LOOPER_AND_UI_THREAD_IS_DIFFERENT_BACKGROUND_LOOPER,
        CookieManagerThreadingCondition.CALLED_FROM_MAIN_LOOPER_AND_UI_THREAD_IS_MAIN_LOOPER,
        CookieManagerThreadingCondition.CALLED_FROM_MAIN_LOOPER_AND_UI_THREAD_IS_BACKGROUND_LOOPER,
    })
    private @interface CookieManagerThreadingCondition {
        int NOT_CALLED_BEFORE_UI_THREAD_SET = 0;
        int CALLED_ON_NON_LOOPER_THREAD = 1;
        int CALLED_FROM_BACKGROUND_LOOPER_AND_UI_THREAD_IS_MAIN_LOOPER = 2;
        int CALLED_FROM_BACKGROUND_LOOPER_AND_UI_THREAD_IS_SAME_BACKGROUND_LOOPER = 3;
        int CALLED_FROM_BACKGROUND_LOOPER_AND_UI_THREAD_IS_DIFFERENT_BACKGROUND_LOOPER = 4;
        int CALLED_FROM_MAIN_LOOPER_AND_UI_THREAD_IS_MAIN_LOOPER = 5;
        int CALLED_FROM_MAIN_LOOPER_AND_UI_THREAD_IS_BACKGROUND_LOOPER = 6;
        int COUNT = 7;
    };

    private static void logCookieManagerThreadingCondition(
            @CookieManagerThreadingCondition int condition) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.Startup.CookieManagerThreadingCondition",
                condition,
                CookieManagerThreadingCondition.COUNT);
    }

    WebViewChromiumAwInit(WebViewChromiumFactoryProvider factory) {
        mFactory = factory;
        // Do not make calls into 'factory' in this ctor - this ctor is called from the
        // WebViewChromiumFactoryProvider ctor, so 'factory' is not properly initialized yet.
    }

    private void startChromium(@CallSite int callSite, boolean triggeredFromUIThread) {
        assert ThreadUtils.runningOnUiThread();

        if (mInitState.get() == INIT_FINISHED) {
            return;
        }

        if (anyStartupTaskExperimentIsEnabled()) {
            if (mStartupException != null) {
                throw mStartupException;
            } else if (mStartupError != null) {
                throw mStartupError;
            }

            // This can be non-null for async-then-sync or multiple-async calls.
            if (mStartupTasksRunner == null) {
                mStartupTasksRunner = initializeStartupTasksRunner();
            }
        } else {
            // Makes sure we run all of the startup tasks.
            mStartupTasksRunner = initializeStartupTasksRunner();
        }

        mStartupTasksRunner.run(callSite, triggeredFromUIThread);
    }

    void setProviderInitOnMainLooperLocation(Throwable t) {
        mWebViewStartUpDiagnostics.setProviderInitOnMainLooperLocation(t);
    }

    // Called once during the WebViewChromiumFactoryProvider initialization
    void setStartupTaskExperimentEnabled(boolean enabled) {
        assert mInitState.get() == INIT_NOT_STARTED;
        mIsStartupTaskExperimentEnabled = enabled;
    }

    // Called once during the WebViewChromiumFactoryProvider initialization
    void setStartupTaskExperimentP2Enabled(boolean enabled) {
        assert mInitState.get() == INIT_NOT_STARTED;
        mIsStartupTaskExperimentP2Enabled = enabled;
    }

    // Called once during the WebViewChromiumFactoryProvider initialization
    void setStartupTasksYieldToNativeExperimentEnabled(boolean enabled) {
        assert mInitState.get() == INIT_NOT_STARTED;
        mIsStartupTasksYieldToNativeExperimentEnabled = enabled;
    }

    // These are startup tasks that can either run during provider init or during `startChromium`.
    // This is extracted out so that we can experiment with calling this in either of these
    // locations.
    public void runNonUiThreadCapableStartupTasks() {
        ResourceBundle.setAvailablePakLocales(AwLocaleConfig.getWebViewSupportedPakLocales());

        try (DualTraceEvent ignored2 = DualTraceEvent.scoped("LibraryLoader.ensureInitialized")) {
            LibraryLoader.getInstance().ensureInitialized();
        }

        // TODO(crbug.com/400414092): PathService overrides should be obsolete now.
        PathService.override(PathService.DIR_MODULE, "/system/lib/");
        PathService.override(DIR_RESOURCE_PAKS_ANDROID, "/system/framework/webview/paks");

        initPlatSupportLibrary();
        AwContentsStatics.setCheckClearTextPermitted(
                ContextUtils.getApplicationContext().getApplicationInfo().targetSdkVersion
                        >= Build.VERSION_CODES.O);
    }

    // Initializes a new StartupTaskRunner with a list of tasks to run for chromium startup.
    // Postcondition of calling `.run` on the returned StartupTasksRunner is that Chromium startup
    // is finished.
    // Note: You should abstract any logic that is not strictly dependent on glue layer code into
    // a static method in AwBrowserProcess so they can be unit-tested.
    private StartupTasksRunner initializeStartupTasksRunner() {
        ArrayDeque<Runnable> preBrowserProcessStartTasks = new ArrayDeque<>();
        ArrayDeque<Runnable> postBrowserProcessStartTasks = new ArrayDeque<>();
        preBrowserProcessStartTasks.addLast(
                () -> {
                    if (anyStartupTaskExperimentIsEnabled()) {
                        // Disable java-side PostTask scheduling. The native-side task runners
                        // are also disabled in the native code. The unscheduled prenative tasks
                        // are migrated to the native task runner. The native task runner is
                        // enabled when we are done with startup.
                        PostTask.disablePreNativeUiTasks(true);
                    }

                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                        TrackExitReasons.startTrackingStartup();
                    }

                    if (!WebViewCachedFlags.get()
                            .isCachedFeatureEnabled(
                                    AwFeatures.WEBVIEW_MOVE_WORK_TO_PROVIDER_INIT)) {
                        runNonUiThreadCapableStartupTasks();
                    }
                    waitUntilSetUpResources();
                    // NOTE: Finished writing Java resources. From this point on, it's safe
                    // to use them.

                    // TODO(crbug.com/400413041) : Remove this workaround.
                    // Try to work around the resources problem.
                    //
                    // WebViewFactory adds WebView's asset path to the host app before any
                    // of the code in the APK starts running, but it adds it using an old
                    // mechanism that doesn't persist if the app's resource configuration
                    // changes for any other reason.
                    //
                    // By the time we get here, it's possible it's gone missing due to
                    // something on the UI thread having triggered a resource update. This
                    // can happen either because WebView initialization was triggered by a
                    // background thread (and thus this code is running inside a posted task
                    // on the UI thread which may have taken any amount of time to actually
                    // run), or because the app used CookieManager first, which triggers the
                    // code being loaded and WebViewFactory doing the initial resources add,
                    // but does not call startChromium until the app uses some other
                    // API, an arbitrary amount of time later. So, we can try to add them
                    // again using the "better" method in WebViewDelegate.
                    //
                    // However, we only want to try this if the resources are actually
                    // missing, because in the past we've seen this cause apps that were
                    // working to *start* crashing. The first resource that gets accessed in
                    // startup happens during the AwBrowserProcess.start() call when trying
                    // to determine if the device is a tablet, and that's the most common
                    // place for us to crash. So, try calling that same method and see if it
                    // throws - if so then we're unlikely to make the situation any worse by
                    // trying to fix the path.
                    //
                    // This cannot fix the problem in all cases - if the app is using a
                    // weird ContextWrapper or doing other unusual things with
                    // resources/assets then even adding it with this mechanism might not
                    // help.
                    try {
                        DeviceFormFactor.isTablet();
                        RecordHistogram.recordBooleanHistogram(
                                ASSET_PATH_WORKAROUND_HISTOGRAM_NAME, false);
                    } catch (Resources.NotFoundException e) {
                        RecordHistogram.recordBooleanHistogram(
                                ASSET_PATH_WORKAROUND_HISTOGRAM_NAME, true);
                        mFactory.addWebViewAssetPath(ContextUtils.getApplicationContext());
                    }

                    AwBrowserProcess.configureChildProcessLauncher();

                    // finishVariationsInitLocked() must precede native initialization so
                    // the seed is available when AwFeatureListCreator::SetUpFieldTrials()
                    // runs.
                    if (!FastVariationsSeedSafeModeAction.hasRun()) {
                        finishVariationsInitLocked();
                    }
                });

        addBrowserProcessStartTasksToQueue(
                preBrowserProcessStartTasks, postBrowserProcessStartTasks);

        // This has to be done after variations are initialized, so components could
        // be registered or not depending on the variations flags.
        postBrowserProcessStartTasks.addLast(AwBrowserProcess::loadComponents);
        postBrowserProcessStartTasks.addLast(
                () -> {
                    AwBrowserProcess.initializeMetricsLogUploader();

                    int targetSdkVersion =
                            ContextUtils.getApplicationContext()
                                    .getApplicationInfo()
                                    .targetSdkVersion;
                    RecordHistogram.recordSparseHistogram(
                            "Android.WebView.TargetSdkVersion", targetSdkVersion);

                    try (DualTraceEvent e =
                            DualTraceEvent.scoped(
                                    "WebViewChromiumAwInit.initThreadUnsafeSingletons")) {
                        mChromiumStartedGlobals = new ChromiumStartedGlobals();
                    }
                    if (mShouldInitializeDefaultProfile) {
                        try (DualTraceEvent e =
                                DualTraceEvent.scoped(
                                        "WebViewChromiumAwInit.initializeDefaultProfile")) {
                            mDefaultProfileHolder.initializeDefaultProfileOnUI();
                        }
                    }

                    if (ApkInfo.isDebugAndroidOrApp()) {
                        getSharedStatics().setWebContentsDebuggingEnabledUnconditionally(true);
                    }

                    if ((Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU)
                            ? CompatChanges.isChangeEnabled(WebSettings.ENABLE_SIMPLIFIED_DARK_MODE)
                            : targetSdkVersion >= Build.VERSION_CODES.TIRAMISU) {
                        AwDarkMode.enableSimplifiedDarkMode();
                    }

                    if (WebViewCachedFlags.get()
                            .isCachedFeatureEnabled(
                                    AwFeatures.WEBVIEW_OPT_IN_TO_GMS_BIND_SERVICE_OPTIMIZATION)) {
                        AwBrowserProcess.maybeEnableSafeBrowsingFromGms();
                        AwBrowserProcess.setupSupervisedUser();
                        AwBrowserProcess.handleMinidumpsAndSetMetricsConsent(
                                /* updateMetricsConsent= */ true);
                    }

                    AwBrowserProcess.postBackgroundTasks(
                            mFactory.isSafeModeEnabled(), mFactory.getWebViewPrefs());

                    AconfigFlaggedApiDelegate delegate = AconfigFlaggedApiDelegate.getInstance();
                    if (delegate != null) {
                        AwContentsStatics.setSelectionActionMenuClient(
                                delegate.getSelectionActionMenuClient(
                                        mFactory.getWebViewDelegate()));
                    }

                    AwCrashyClassUtils.maybeCrashIfEnabled();
                    // Must happen right after Chromium initialization is complete.
                    mInitState.set(INIT_FINISHED);
                    mStartupFinished.countDown();
                    // This runs all the pending tasks queued for after Chromium init is
                    // finished, so should run after `mInitState` is `INIT_FINISHED`.
                    mFactory.getRunQueue().notifyChromiumStarted();
                    if (anyStartupTaskExperimentIsEnabled()) {
                        // Re-enables the taskrunners
                        PostTask.disablePreNativeUiTasks(false);
                        AwBrowserProcess.onStartupComplete();
                    }
                });

        return new StartupTasksRunner(preBrowserProcessStartTasks, postBrowserProcessStartTasks);
    }

    private void addBrowserProcessStartTasksToQueue(
            ArrayDeque<Runnable> preBrowserProcessStartTasks,
            ArrayDeque<Runnable> postBrowserProcessStartTasks) {
        StartupCallback callback =
                new StartupCallback() {
                    @Override
                    public void onSuccess(@Nullable StartupMetrics metrics) {
                        mStartupTasksRunner.recordContentMetrics(metrics);
                        mStartupTasksRunner.finishAsyncRun();
                    }

                    @Override
                    public void onFailure() {
                        throw new ProcessInitException(LoaderErrors.NATIVE_STARTUP_FAILED);
                    }
                };
        // Currently, browser process startup is run synchronously. With the phase 2 startup tasks
        // experiment, run browser process startup asynchronously. The callback then triggers the
        // continuation of our startup tasks execution.
        // If a sync startup preempts an async startup, we need to run browser process startup
        // synchronously if the scheduled browser process async startup hasn't completed.
        if (mIsStartupTaskExperimentP2Enabled || mIsStartupTasksYieldToNativeExperimentEnabled) {
            preBrowserProcessStartTasks.addLast(
                    () -> {
                        AwBrowserProcess.runPreBrowserProcessStart();
                        if (mStartupTasksRunner.getRunState() == StartupTasksRunner.ASYNC) {
                            AwBrowserProcess.triggerAsyncBrowserProcess(
                                    callback, !mIsStartupTasksYieldToNativeExperimentEnabled);
                        }
                    });
            postBrowserProcessStartTasks.addLast(
                    () -> {
                        AwBrowserProcess.finishBrowserProcessStart();
                        runImmediateTaskAfterBrowserProcessInit();
                    });
        } else {
            preBrowserProcessStartTasks.addLast(
                    () -> {
                        // Starts browser process synchronously.
                        AwBrowserProcess.runPreBrowserProcessStart();
                        AwBrowserProcess.finishBrowserProcessStart();
                        if (mStartupTasksRunner.getRunState() == StartupTasksRunner.ASYNC) {
                            // Tell the StartupTaskRunner to continue with the
                            // postBrowserProcessStartQueue.
                            mStartupTasksRunner.finishAsyncRun();
                        }
                    });

            postBrowserProcessStartTasks.addLast(this::runImmediateTaskAfterBrowserProcessInit);
        }
    }

    // Run the next startup task following BrowserProcess init.
    private void runImmediateTaskAfterBrowserProcessInit() {
        // TODO(crbug.com/332706093): See if this can be moved before loading native.
        AwClassPreloader.preloadClasses();
        if (!WebViewCachedFlags.get()
                .isCachedFeatureEnabled(
                        AwFeatures.WEBVIEW_OPT_IN_TO_GMS_BIND_SERVICE_OPTIMIZATION)) {
            AwBrowserProcess.handleMinidumpsAndSetMetricsConsent(/* updateMetricsConsent= */ true);
        }
        AwBrowserProcess.doNetworkInitializations(ContextUtils.getApplicationContext());
    }

    private void recordStartupMetrics(
            @CallSite int startCallSite,
            @CallSite int finishCallSite,
            long startTimeMs,
            long totalTimeTakenMs,
            long longestUiBlockingTaskTimeMs,
            @StartupTasksRunner.StartupMode int startupMode) {
        long wallClockTimeMs = SystemClock.uptimeMillis() - startTimeMs;
        // Record asyncStartup API metrics
        mWebViewStartUpDiagnostics.setTotalTimeUiThreadChromiumInitMillis(totalTimeTakenMs);
        mWebViewStartUpDiagnostics.setMaxTimePerTaskUiThreadChromiumInitMillis(
                longestUiBlockingTaskTimeMs);
        mWebViewStartUpCallbackRunQueue.notifyChromiumStarted();

        // Record histograms
        String startupModeString =
                switch (startupMode) {
                    case StartupTasksRunner.StartupMode.FULLY_SYNC -> ".FullySync";
                    case StartupTasksRunner.StartupMode.FULLY_ASYNC -> ".FullyAsync";
                    case StartupTasksRunner.StartupMode
                            .ASYNC_BUT_FULLY_SYNC -> ".AsyncButFullySync";
                    case StartupTasksRunner.StartupMode
                            .PARTIAL_ASYNC_THEN_SYNC -> ".PartialAsyncThenSync";
                    default -> ".Unknown";
                };
        RecordHistogram.recordTimesHistogram(
                "Android.WebView.Startup.CreationTime.StartChromiumLocked", totalTimeTakenMs);
        RecordHistogram.recordTimesHistogram(
                "Android.WebView.Startup.CreationTime.StartChromiumLocked" + startupModeString,
                totalTimeTakenMs);
        RecordHistogram.recordTimesHistogram(
                "Android.WebView.Startup.ChromiumInitTime.LongestUiBlockingTaskTime",
                longestUiBlockingTaskTimeMs);
        RecordHistogram.recordTimesHistogram(
                "Android.WebView.Startup.ChromiumInitTime.LongestUiBlockingTaskTime"
                        + startupModeString,
                longestUiBlockingTaskTimeMs);
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.Startup.ChromiumInitTime.StartupMode",
                startupMode,
                StartupTasksRunner.StartupMode.COUNT);
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.Startup.CreationTime.InitReason2", startCallSite, CallSite.COUNT);
        if (startupMode == StartupTasksRunner.StartupMode.ASYNC_BUT_FULLY_SYNC
                || startupMode == StartupTasksRunner.StartupMode.PARTIAL_ASYNC_THEN_SYNC) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.WebView.Startup.ChromiumInitTime.AsyncToSyncSwitchReason2",
                    finishCallSite,
                    CallSite.COUNT);
        }
        RecordHistogram.recordTimesHistogram(
                "Android.WebView.Startup.ChromiumInitTime.WallClockTime", wallClockTimeMs);
        RecordHistogram.recordTimesHistogram(
                "Android.WebView.Startup.ChromiumInitTime.WallClockTime" + startupModeString,
                wallClockTimeMs);

        // Stop early trace event collection.
        // They have already been emitted if a trace session was started to capture startup.
        EarlyTraceEvent.reset();

        // Record traces
        TraceEvent.webViewStartupStartChromiumLocked(
                startTimeMs,
                totalTimeTakenMs,
                /* startCallSite= */ startCallSite,
                /* finishCallSite= */ finishCallSite,
                /* startupMode= */ startupMode);
        // Also create the trace events for the earlier WebViewChromiumFactoryProvider init, which
        // happens before tracing is ready.
        TraceEvent.webViewStartupTotalFactoryInit(
                mFactory.getInitInfo().mTotalFactoryInitStartTime,
                mFactory.getInitInfo().mTotalFactoryInitDuration);
        TraceEvent.webViewStartupStage1(
                mFactory.getInitInfo().mStartTime, mFactory.getInitInfo().mDuration);
    }

    /**
     * Set up resources on a background thread, in parallel with chromium initialization as it takes
     * some time. This method is called once during WebViewChromiumFactoryProvider initialization
     * which is guaranteed to finish before this field is accessed by waitUntilSetUpResources.
     *
     * @param context The context.
     */
    void setUpResourcesOnBackgroundThread(int packageId, Context context) {
        try (DualTraceEvent e =
                DualTraceEvent.scoped("WebViewChromiumAwInit.setUpResourcesOnBackgroundThread")) {
            assert mSetUpResourcesThread == null : "This method shouldn't be called twice.";

            Runnable setUpResourcesRunnable =
                    new Runnable() {
                        @Override
                        public void run() {
                            try (DualTraceEvent e =
                                    DualTraceEvent.scoped("WebViewChromiumAwInit.setUpResources")) {
                                R.onResourcesLoaded(packageId);

                                AwResource.setResources(context.getResources());
                                AwResource.setConfigKeySystemUuidMapping(
                                        android.R.array.config_keySystemUuidMapping);
                            }
                        }
                    };

            // Make sure that ResourceProvider is initialized before starting the browser process.
            mSetUpResourcesThread = new Thread(setUpResourcesRunnable);
            mSetUpResourcesThread.start();
        }
    }

    private void waitUntilSetUpResources() {
        try (DualTraceEvent e =
                DualTraceEvent.scoped("WebViewChromiumAwInit.waitUntilSetUpResources")) {
            mSetUpResourcesThread.join();
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
    }

    boolean isChromiumInitialized() {
        return mInitState.get() == INIT_FINISHED;
    }

    /**
     * If UI thread is not set, Android main looper will be set as the UI thread.
     *
     * <p>Postcondition: Chromium startup is finished when this method returns.
     */
    void triggerAndWaitForChromiumStarted(@CallSite int callSite) {
        if (triggerChromiumStartupAndReturnTrueIfStartupIsFinished(callSite, false)) {
            return;
        }
        // For threadSafe WebView APIs that can trigger startup, holding a lock while waiting for
        // the startup to complete can lead to a deadlock. This would happen when:
        // - A background thread B call threadsafe funcA and acquires mLazyInitLock.
        // - Thread B posts the startup task to the UI thread and waits for completion.
        // - UI thread calls funcA before it has executed the posted startup task.
        // - UI thread blocks trying to acquire mLazyInitLock that's held by thread B.
        // - Deadlock!
        // See crbug.com/395877483 for more details.
        assert !Thread.holdsLock(mLazyInitLock);

        try (DualTraceEvent event =
                DualTraceEvent.scoped("WebViewChromiumAwInit.waitForUIThreadInit")) {
            long startTime = SystemClock.uptimeMillis();
            // Wait for the UI thread to finish init.
            while (true) {
                try {
                    mStartupFinished.await();
                    break;
                } catch (InterruptedException e) {
                    // Keep trying; we can't abort init as WebView APIs do not declare that they
                    // throw InterruptedException.
                }
            }
            RecordHistogram.recordTimesHistogram(
                    "Android.WebView.Startup.CreationTime.waitForUIThreadInit",
                    SystemClock.uptimeMillis() - startTime);
        }
    }

    /**
     * If UI thread is not set, Android main looper will be set as the UI thread.
     *
     * <p>Postcondition: Chromium startup will be finished in the near future.
     */
    void postChromiumStartupIfNeeded(@CallSite int callSite) {
        triggerChromiumStartupAndReturnTrueIfStartupIsFinished(callSite, true);
    }

    /**
     * Triggers Chromium startup.
     *
     * <p>If `alwaysPost` is true, startup is always posted to the UI thread.
     *
     * <p>If `alwaysPost` is false, startup is posted to UI thread if not called on the UI thread
     * and startup will be run synchronously if called on the UI thread.
     *
     * <p>If the UI thread is not set explicitly before calling this method, the main looper is
     * chosen as the UI thread.
     *
     * @returns true if Chromium startup if finished, false if startup will be finished in the near
     *     future. If false, caller may choose to wait on the {@code mStartupFinished} latch, or
     *     {@link WebViewStartUpCallback}.
     */
    private boolean triggerChromiumStartupAndReturnTrueIfStartupIsFinished(
            @CallSite int callSite, boolean alwaysPost) {
        if (mInitState.get() == INIT_FINISHED) { // Early-out for the common case.
            return true;
        }
        try (DualTraceEvent e1 =
                DualTraceEvent.scoped(
                        "WebViewChromiumFactoryProvider.triggerChromiumStartupAndReturnTrueIfStartupIsFinished")) {
            maybeSetChromiumUiThread(Looper.getMainLooper());
            boolean runSynchronously = !alwaysPost && ThreadUtils.runningOnUiThread();
            mChromiumFirstStartupRequestMode.compareAndSet(
                    StartupTasksRunner.UNSET,
                    runSynchronously ? StartupTasksRunner.SYNC : StartupTasksRunner.ASYNC);
            if (runSynchronously) {
                mWebViewStartUpDiagnostics.setSynchronousChromiumInitLocation(
                        new Throwable(
                                "Location where Chromium init was started synchronously on the UI"
                                        + " thread"));
                // If we are currently running on the UI thread then we must do init now. If there
                // was already a task posted to the UI thread from another thread to do it, it will
                // just no-op when it runs.
                startChromium(callSite, /* triggeredFromUIThread= */ true);
                return true;
            }
            if (mInitState.compareAndSet(INIT_NOT_STARTED, INIT_POSTED)) {
                if (callSite != CallSite.ASYNC_WEBVIEW_STARTUP) {
                    mWebViewStartUpDiagnostics.setAsynchronousChromiumInitLocation(
                            new Throwable(
                                    "Location where Chromium init was started asynchronously on a"
                                            + " non-UI thread"));
                }
                // If we're not running on the UI thread (because init was triggered by a
                // thread-safe
                // function), post init to the UI thread, since init is *not* thread-safe.
                AwThreadUtils.postToUiThreadLooper(
                        () -> startChromium(callSite, /* triggeredFromUIThread= */ false));
            }
            return false;
        }
    }

    void maybeSetChromiumUiThread(Looper looper) {
        synchronized (mThreadSettingLock) {
            if (mThreadIsSet) {
                return;
            }
            Looper mainLooper = Looper.getMainLooper();
            boolean isUiThreadMainLooper = mainLooper.equals(looper);
            Log.v(
                    TAG,
                    "Binding Chromium to "
                            + (isUiThreadMainLooper ? "main" : "background")
                            + " looper "
                            + looper);
            RecordHistogram.recordBooleanHistogram(
                    "Android.WebView.Startup.IsUiThreadMainLooper", isUiThreadMainLooper);

            // Temporary metric collection for different threading conditions related to
            // CookieManager.
            boolean cookieManagerCalled = mGetDefaultCookieManagerCalled.get();
            if (cookieManagerCalled) {
                Looper cookieManagerLooper = mFirstGetDefaultCookieManagerLooper.get();
                if (cookieManagerLooper == null) {
                    logCookieManagerThreadingCondition(
                            CookieManagerThreadingCondition.CALLED_ON_NON_LOOPER_THREAD);
                } else if (!mainLooper.equals(cookieManagerLooper)) {
                    if (isUiThreadMainLooper) {
                        logCookieManagerThreadingCondition(
                                CookieManagerThreadingCondition
                                        .CALLED_FROM_BACKGROUND_LOOPER_AND_UI_THREAD_IS_MAIN_LOOPER);
                    } else if (looper.equals(cookieManagerLooper)) {
                        logCookieManagerThreadingCondition(
                                CookieManagerThreadingCondition
                                        .CALLED_FROM_BACKGROUND_LOOPER_AND_UI_THREAD_IS_SAME_BACKGROUND_LOOPER);
                    } else {
                        logCookieManagerThreadingCondition(
                                CookieManagerThreadingCondition
                                        .CALLED_FROM_BACKGROUND_LOOPER_AND_UI_THREAD_IS_DIFFERENT_BACKGROUND_LOOPER);
                    }
                } else if (mainLooper.equals(cookieManagerLooper)) {
                    if (isUiThreadMainLooper) {
                        logCookieManagerThreadingCondition(
                                CookieManagerThreadingCondition
                                        .CALLED_FROM_MAIN_LOOPER_AND_UI_THREAD_IS_MAIN_LOOPER);
                    } else {
                        logCookieManagerThreadingCondition(
                                CookieManagerThreadingCondition
                                        .CALLED_FROM_MAIN_LOOPER_AND_UI_THREAD_IS_BACKGROUND_LOOPER);
                    }
                }
            } else {
                logCookieManagerThreadingCondition(
                        CookieManagerThreadingCondition.NOT_CALLED_BEFORE_UI_THREAD_SET);
            }

            ThreadUtils.setUiThread(looper);
            mThreadIsSet = true;
        }
    }

    private void initPlatSupportLibrary() {
        try (DualTraceEvent e =
                DualTraceEvent.scoped("WebViewChromiumAwInit.initPlatSupportLibrary")) {
            AwDrawFnImpl.setDrawFnFunctionTable(DrawFunctor.getDrawFnFunctionTable());
            AwContents.setAwDrawSWFunctionTable(GraphicsUtils.getDrawSWFunctionTable());
        }
    }

    public SharedStatics getSharedStatics() {
        return mFactory.getSharedStatics();
    }

    boolean isMultiProcessEnabled() {
        return mFactory.isMultiProcessEnabled();
    }

    boolean isAsyncStartupWithMultiProcessExperimentEnabled() {
        return mFactory.isAsyncStartupWithMultiProcessExperimentEnabled();
    }

    public AwTracingController getAwTracingController() {
        triggerAndWaitForChromiumStarted(CallSite.GET_AW_TRACING_CONTROLLER);
        return mChromiumStartedGlobals.mAwTracingController;
    }

    public AwProxyController getAwProxyController() {
        triggerAndWaitForChromiumStarted(CallSite.GET_AW_PROXY_CONTROLLER);
        return mChromiumStartedGlobals.mAwProxyController;
    }

    public CookieManager getDefaultCookieManager() {
        if (!mGetDefaultCookieManagerCalled.get()) {
            mFirstGetDefaultCookieManagerLooper.compareAndSet(null, Looper.myLooper());
            mGetDefaultCookieManagerCalled.set(true);
        }
        if (WebViewCachedFlags.get()
                .isCachedFeatureEnabled(AwFeatures.WEBVIEW_BYPASS_PROVISIONAL_COOKIE_MANAGER)) {
            return getDefaultProfile(CallSite.GET_DEFAULT_COOKIE_MANAGER).getCookieManager();
        } else {
            synchronized (mLazyInitLock) {
                if (mDefaultCookieManager == null) {
                    mDefaultCookieManager =
                            new CookieManagerAdapter(AwCookieManager.getDefaultCookieManager());
                }
                return mDefaultCookieManager;
            }
        }
    }

    public android.webkit.WebIconDatabase getWebIconDatabase() {
        triggerAndWaitForChromiumStarted(CallSite.GET_WEB_ICON_DATABASE);
        WebViewChromium.recordWebViewApiCall(ApiCall.WEB_ICON_DATABASE_GET_INSTANCE);
        synchronized (mLazyInitLock) {
            if (mWebIconDatabase == null) {
                mWebIconDatabase = new WebIconDatabaseAdapter();
            }
            return mWebIconDatabase;
        }
    }

    public WebViewDatabase getDefaultWebViewDatabase(final Context context) {
        triggerAndWaitForChromiumStarted(CallSite.GET_DEFAULT_WEBVIEW_DATABASE);
        synchronized (mLazyInitLock) {
            if (mDefaultWebViewDatabase == null) {
                mDefaultWebViewDatabase =
                        new WebViewDatabaseAdapter(
                                mFactory,
                                HttpAuthDatabase.newInstance(context, HTTP_AUTH_DATABASE_FILE));
            }
            return mDefaultWebViewDatabase;
        }
    }

    // See comments in VariationsSeedLoader.java on when it's safe to call this.
    void startVariationsInit() {
        synchronized (mSeedLoaderLock) {
            if (mSeedLoader == null) {
                mSeedLoader = new VariationsSeedLoader();
                mSeedLoader.startVariationsInit();
            }
        }
    }

    private void finishVariationsInitLocked() {
        try (DualTraceEvent e =
                DualTraceEvent.scoped("WebViewChromiumAwInit.finishVariationsInitLocked")) {
            synchronized (mSeedLoaderLock) {
                if (mSeedLoader == null) {
                    Log.e(TAG, "finishVariationsInitLocked() called before startVariationsInit()");
                    startVariationsInit();
                }
                mSeedLoader.finishVariationsInit();
                mSeedLoader = null; // Allow this to be GC'd after its background thread finishes.
            }
        }
    }

    public WebViewChromiumRunQueue getRunQueue() {
        return mFactory.getRunQueue();
    }

    public Object getLazyInitLock() {
        return mLazyInitLock;
    }

    // Starts up WebView asynchronously.
    // MUST NOT be called on the UI thread.
    // The callback can either be called synchronously or on the UI thread.
    public void startUpWebView(
            @NonNull WebViewStartUpCallback callback,
            boolean shouldRunUiThreadStartUpTasks,
            @Nullable Set<String> profilesToLoad) {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            throw new IllegalStateException(
                    "startUpWebView should not be called on the Android main looper");
        }

        if (profilesToLoad != null) {
            if (!shouldRunUiThreadStartUpTasks) {
                throw new IllegalArgumentException(
                        "Can't specify profiles to load without running UI thread startup tasks");
            }
            mShouldInitializeDefaultProfile = false;
        }

        if (!shouldRunUiThreadStartUpTasks) {
            callback.onSuccess(mWebViewStartUpDiagnostics);
            return;
        }

        mWebViewStartUpCallbackRunQueue.addTask(
                () -> {
                    Set<String> profilesCopy =
                            profilesToLoad != null
                                    ? profilesToLoad
                                    : Set.of(AwBrowserContext.getDefaultContextName());

                    for (String context : profilesCopy) {
                        ProfileStore.getInstance()
                                .getOrCreateProfile(
                                        context, ProfileStore.CallSite.ASYNC_WEBVIEW_STARTUP);
                    }
                    callback.onSuccess(mWebViewStartUpDiagnostics);
                });
        postChromiumStartupIfNeeded(CallSite.ASYNC_WEBVIEW_STARTUP);
    }

    private boolean anyStartupTaskExperimentIsEnabled() {
        return mIsStartupTaskExperimentEnabled
                || mIsStartupTaskExperimentP2Enabled
                || mIsStartupTasksYieldToNativeExperimentEnabled;
    }

    // These are objects that need to be created on the UI thread and after chromium has started.
    // Thus created during startChromium for ease.
    private static final class ChromiumStartedGlobals {
        final AwTracingController mAwTracingController;
        final AwProxyController mAwProxyController;

        ChromiumStartedGlobals() {
            mAwProxyController = new AwProxyController();
            mAwTracingController = new AwTracingController();
        }
    }

    public Profile getDefaultProfile(@CallSite int callSite) {
        return mDefaultProfileHolder.getDefaultProfile(callSite);
    }

    private final class DefaultProfileHolder {
        private volatile Profile mDefaultProfile;
        private final CountDownLatch mDefaultProfileIsInitialized = new CountDownLatch(1);

        /** Must be called on the UI thread. */
        public void initializeDefaultProfileOnUI() {
            if (BuildConfig.ENABLE_ASSERTS && !ThreadUtils.runningOnUiThread()) {
                throw new RuntimeException(
                        "DefaultProfileHolder called on " + Thread.currentThread());
            }
            if (mDefaultProfile != null) return;
            mDefaultProfile =
                    ProfileStore.getInstance()
                            .getOrCreateProfile(
                                    AwBrowserContext.getDefaultContextName(),
                                    ProfileStore.CallSite.GET_DEFAULT_PROFILE);
            mDefaultProfileIsInitialized.countDown();
        }

        /**
         * Ensures the default profile and its dependencies are initialized on the UI thread.
         *
         * <p>The {@code StartupWebView} API allows for initializing a specific list of profiles,
         * which may not include the default profile. This method acts as a safeguard, ensuring the
         * default profile is ready the first time a thread-safe framework API is called.
         */
        private void ensureInitializationIsDone(@CallSite int callSite) {
            triggerAndWaitForChromiumStarted(callSite);
            if (mDefaultProfile != null) {
                return;
            }

            ThreadUtils.runOnUiThread(this::initializeDefaultProfileOnUI);
            // Wait for the UI to finish.
            while (true) {
                try {
                    mDefaultProfileIsInitialized.await();
                    break;
                } catch (InterruptedException e) {
                    // Keep trying; we can't abort here as WebView APIs do not declare that they
                    // throw InterruptedException.
                }
            }
        }

        public Profile getDefaultProfile(@CallSite int callSite) {
            ensureInitializationIsDone(callSite);
            return mDefaultProfile;
        }
    }

    // This class is responsible for running chromium startup tasks asynchronously or synchronously
    // depending on if startup is triggered from the background or UI thread.
    private final class StartupTasksRunner {
        private final ArrayDeque<Runnable> mPreBrowserProcessStartQueue;
        private final ArrayDeque<Runnable> mPostBrowserProcessStartQueue;
        private final int mPreBrowserProcessStartTasksSize;
        private final int mNumTasks;
        private boolean mAsyncHasBeenTriggered;
        private long mLongestUiBlockingTaskTimeMs;
        private long mTotalTimeTakenMs;
        private long mStartupTimeMs;
        private boolean mStartupStarted;
        private @CallSite int mStartCallSite = CallSite.COUNT;
        private @CallSite int mFinishCallSite = CallSite.COUNT;
        private boolean mFirstTaskFromSynchronousCall;
        private int mRunState = StartupTasksRunner.UNSET;

        private static final int UNSET = 0;
        private static final int SYNC = 1;
        private static final int ASYNC = 2;

        // LINT.IfChange(WebViewChromiumStartupMode)
        @IntDef({
            StartupMode.FULLY_SYNC,
            StartupMode.FULLY_ASYNC,
            StartupMode.PARTIAL_ASYNC_THEN_SYNC,
            StartupMode.ASYNC_BUT_FULLY_SYNC,
            StartupMode.COUNT,
        })
        @interface StartupMode {
            // Startup was triggered on the UI thread and completed synchronously
            int FULLY_SYNC = 0;
            // Startup was triggered on a background thread and completed asynchronously
            int FULLY_ASYNC = 1;
            // Startup was triggered on a background thread, some tasks ran asynchronously. Then
            // another init call on the UI thread preempted the async run and startup completed
            // synchronously
            int PARTIAL_ASYNC_THEN_SYNC = 2;
            // Startup was triggered on a background thread, the posted task was not run yet. Then
            // another init call on the UI thread was started before the posted task and startup
            // fully completed synchronously
            int ASYNC_BUT_FULLY_SYNC = 3;
            // Remember to update WebViewStartupMode in enums.xml when adding new values here.
            int COUNT = 4;
        };

        // LINT.ThenChange(//base/tracing/protos/chrome_track_event.proto:WebViewChromiumStartupMode)

        StartupTasksRunner(
                ArrayDeque<Runnable> preBrowserProcessStartTasks,
                ArrayDeque<Runnable> postBrowserProcessStartTasks) {
            mPreBrowserProcessStartQueue = preBrowserProcessStartTasks;
            mPostBrowserProcessStartQueue = postBrowserProcessStartTasks;
            mPreBrowserProcessStartTasksSize = preBrowserProcessStartTasks.size();
            mNumTasks = mPreBrowserProcessStartTasksSize + postBrowserProcessStartTasks.size();
        }

        void run(@CallSite int callSite, boolean triggeredFromUIThread) {
            assert ThreadUtils.runningOnUiThread();

            if (!mStartupStarted) {
                mStartupStarted = true;
                mFirstTaskFromSynchronousCall = triggeredFromUIThread;
                mStartCallSite = callSite;
                mFinishCallSite = callSite;
                mStartupTimeMs = SystemClock.uptimeMillis();
            }

            // Early return to avoid repeating the return call within sync and async blocks
            if (mPostBrowserProcessStartQueue.isEmpty()) {
                assert mInitState.get() == INIT_FINISHED;
                return;
            }

            if (anyStartupTaskExperimentIsEnabled() && !triggeredFromUIThread) {
                // Prevents triggering async run multiple times and thus reduce the interval between
                // tasks.
                if (mAsyncHasBeenTriggered) {
                    return;
                }
                mAsyncHasBeenTriggered = true;
                startAsyncRun();
            } else {
                // This lets us track the reason for a sync finish, especially relevant if we
                // started off asynchronously.
                mFinishCallSite = callSite;
                try (DualTraceEvent event =
                        DualTraceEvent.scoped("WebViewChromiumAwInit.startChromiumLockedSync")) {
                    timedRunWithExceptionHandling(this::runSync);
                }
            }
        }

        private void runSync() {
            assert ThreadUtils.runningOnUiThread();

            // Avoid changing runState when there's no task to be run synchronously.
            if (mPreBrowserProcessStartQueue.isEmpty() && mPostBrowserProcessStartQueue.isEmpty()) {
                return;
            }

            mRunState = SYNC;

            Runnable task = mPreBrowserProcessStartQueue.poll();
            while (task != null) {
                task.run();
                task = mPreBrowserProcessStartQueue.poll();
            }

            task = mPostBrowserProcessStartQueue.poll();
            while (task != null) {
                task.run();
                task = mPostBrowserProcessStartQueue.poll();
            }
        }

        private void startAsyncRun() {
            assert ThreadUtils.runningOnUiThread();
            runAsyncStartupTaskAndPostNext(/* taskNum= */ 1, mPreBrowserProcessStartQueue);
        }

        // Continues running the tasks in the postBrowserProcessStartQueue. This method is often
        // called inline, so post the next task in order to maintain the gap between the previous
        // task and the next task.
        void finishAsyncRun() {
            AwThreadUtils.postToUiThreadLooper(
                    () ->
                            runAsyncStartupTaskAndPostNext(
                                    mPreBrowserProcessStartTasksSize + 1,
                                    mPostBrowserProcessStartQueue));
        }

        private void runAsyncStartupTaskAndPostNext(int taskNum, ArrayDeque<Runnable> queue) {
            assert ThreadUtils.runningOnUiThread();

            Runnable task = queue.poll();
            if (task == null) {
                return;
            }

            mRunState = ASYNC;

            try (DualTraceEvent event =
                    DualTraceEvent.scoped(
                            String.format(
                                    Locale.US,
                                    "WebViewChromiumAwInit.startChromiumLockedAsync_task%d/%d",
                                    taskNum,
                                    mNumTasks))) {
                timedRunWithExceptionHandling(task);
            }

            if (!queue.isEmpty()) { // Avoids unnecessarily posting to the UI thread
                AwThreadUtils.postToUiThreadLooper(
                        () -> runAsyncStartupTaskAndPostNext(taskNum + 1, queue));
            }
        }

        // Runs the startup task while keeping track of metrics and dealing with exceptions
        private void timedRunWithExceptionHandling(Runnable task) {
            assert ThreadUtils.runningOnUiThread();

            try {
                long startTimeMs = SystemClock.uptimeMillis();
                task.run();
                long durationMs = SystemClock.uptimeMillis() - startTimeMs;

                mLongestUiBlockingTaskTimeMs = Math.max(mLongestUiBlockingTaskTimeMs, durationMs);
                mTotalTimeTakenMs += durationMs;
                if (mPostBrowserProcessStartQueue.isEmpty()) {
                    // We are done running all the tasks, so record the metrics.
                    recordStartupMetrics(
                            mStartCallSite,
                            mFinishCallSite,
                            /* startTimeMs= */ mStartupTimeMs,
                            /* totalTimeTakenMs= */ mTotalTimeTakenMs,
                            /* longestUiBlockingTaskTimeMs= */ mLongestUiBlockingTaskTimeMs,
                            calculateStartupMode());
                }
            } catch (RuntimeException | Error e) {
                Log.e(TAG, "WebView chromium startup failed", e);
                if (e instanceof RuntimeException re) {
                    mStartupException = re;
                } else {
                    mStartupError = (Error) e;
                }
                throw e;
            }
        }

        // Record metrics for tasks that were posted by the BrowserStartupController since the
        // StartupTaskRunner cannot account for them directly.
        void recordContentMetrics(@Nullable StartupMetrics metrics) {
            assert metrics != null;
            mLongestUiBlockingTaskTimeMs =
                    Math.max(
                            mLongestUiBlockingTaskTimeMs,
                            metrics.getLongestDurationOfPostedTasksMs());
            mTotalTimeTakenMs += metrics.getTotalDurationOfPostedTasksMs();
        }

        // To determine the startup mode, we track:
        // 1. Whether the initial startup request was synchronous or asynchronous.
        // 2. Whether the first task ran synchronously or asynchronously.
        // 3. Whether the last task ran synchronously or asynchronously.
        private @StartupMode int calculateStartupMode() {
            // The control arm of our experiment runs fully synchronously.
            if (!anyStartupTaskExperimentIsEnabled()) {
                return StartupMode.FULLY_SYNC;
            }

            if (mFirstTaskFromSynchronousCall) {
                return mChromiumFirstStartupRequestMode.get() == SYNC
                        ? StartupMode.FULLY_SYNC
                        : StartupMode.ASYNC_BUT_FULLY_SYNC;
            }
            return mRunState == SYNC
                    ? StartupMode.PARTIAL_ASYNC_THEN_SYNC
                    : StartupMode.FULLY_ASYNC;
        }

        // Returns the state in which the StartupTaskRunner is running. Either async or
        // synchronously.
        int getRunState() {
            return mRunState;
        }
    }
}
