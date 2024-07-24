// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Picture;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.net.http.SslCertificate;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.SystemClock;
import android.print.PrintDocumentAdapter;
import android.util.Log;
import android.util.SparseArray;
import android.view.DragEvent;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.PointerIcon;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStructure;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeProvider;
import android.view.autofill.AutofillValue;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.textclassifier.TextClassifier;
import android.webkit.DownloadListener;
import android.webkit.FindActionModeCallback;
import android.webkit.ValueCallback;
import android.webkit.WebBackForwardList;
import android.webkit.WebChromeClient;
import android.webkit.WebChromeClient.CustomViewCallback;
import android.webkit.WebMessage;
import android.webkit.WebMessagePort;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebView.VisualStateCallback;
import android.webkit.WebViewClient;
import android.webkit.WebViewProvider;
import android.webkit.WebViewRenderProcess;
import android.webkit.WebViewRenderProcessClient;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.RequiresApi;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwBrowserContextStore;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsStatics;
import org.chromium.android_webview.AwPrintDocumentAdapter;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.AwThreadUtils;
import org.chromium.android_webview.ManifestMetadataUtil;
import org.chromium.android_webview.R;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.android_webview.gfx.AwDrawFnImpl;
import org.chromium.android_webview.renderer_priority.RendererPriority;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.ScopedSysTraceEvent;
import org.chromium.components.content_capture.ContentCaptureFeatures;
import org.chromium.components.content_capture.OnscreenContentProvider;
import org.chromium.components.embedder_support.application.ClassLoaderContextWrapperFactory;
import org.chromium.content_public.browser.MessagePayload;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.content_public.browser.SmartClipProvider;
import org.chromium.url.GURL;

import java.io.BufferedWriter;
import java.io.File;
import java.lang.reflect.Field;
import java.util.Map;
import java.util.concurrent.Callable;
import java.util.concurrent.Executor;

/**
 * This class is the delegate to which WebViewProxy forwards all API calls.
 *
 * Most of the actual functionality is implemented by AwContents (or WebContents within
 * it). This class also contains WebView-specific APIs that require the creation of other
 * adapters (otherwise org.chromium.content would depend on the webview.chromium package)
 * and a small set of no-op deprecated APIs.
 */
@SuppressWarnings("deprecation")
@Lifetime.WebView
class WebViewChromium
        implements WebViewProvider,
                WebViewProvider.ScrollDelegate,
                WebViewProvider.ViewDelegate,
                SmartClipProvider {
    private static final String TAG = WebViewChromium.class.getSimpleName();

    // The WebView that this WebViewChromium is the provider for.
    WebView mWebView;
    // Lets us access protected View-derived methods on the WebView instance we're backing.
    WebView.PrivateAccess mWebViewPrivate;
    // The client adapter class.
    private WebViewContentsClientAdapter mContentsClientAdapter;
    // The wrapped Context.
    private Context mContext;

    // Variables for functionality provided by this adapter ---------------------------------------
    private ContentSettingsAdapter mWebSettings;
    // The WebView wrapper for WebContents and required browser components.
    AwContents mAwContents;

    private final WebView.HitTestResult mHitTestResult;

    private final int mAppTargetSdkVersion;

    protected WebViewChromiumFactoryProvider mFactory;

    protected final SharedWebViewChromium mSharedWebViewChromium;

    private final boolean mShouldDisableThreadChecking;

    private static boolean sRecordWholeDocumentEnabledByApi;

    static void enableSlowWholeDocumentDraw() {
        sRecordWholeDocumentEnabledByApi = true;
    }

    // Used to record the UMA histogram WebView.WebViewApiCall. Since these values are persisted to
    // logs, they should never be renumbered or reused.
    // LINT.IfChange(ApiCall)
    @IntDef({
        ApiCall.ADD_JAVASCRIPT_INTERFACE,
        ApiCall.AUTOFILL,
        ApiCall.CAN_GO_BACK,
        ApiCall.CAN_GO_BACK_OR_FORWARD,
        ApiCall.CAN_GO_FORWARD,
        ApiCall.CAN_ZOOM_IN,
        ApiCall.CAN_ZOOM_OUT,
        ApiCall.CAPTURE_PICTURE,
        ApiCall.CLEAR_CACHE,
        ApiCall.CLEAR_FORM_DATA,
        ApiCall.CLEAR_HISTORY,
        ApiCall.CLEAR_MATCHES,
        ApiCall.CLEAR_SSL_PREFERENCES,
        ApiCall.CLEAR_VIEW,
        ApiCall.COPY_BACK_FORWARD_LIST,
        ApiCall.CREATE_PRINT_DOCUMENT_ADAPTER,
        ApiCall.CREATE_WEBMESSAGE_CHANNEL,
        ApiCall.DOCUMENT_HAS_IMAGES,
        ApiCall.DOES_SUPPORT_FULLSCREEN,
        ApiCall.EVALUATE_JAVASCRIPT,
        ApiCall.EXTRACT_SMART_CLIP_DATA,
        ApiCall.FIND_NEXT,
        ApiCall.GET_CERTIFICATE,
        ApiCall.GET_CONTENT_HEIGHT,
        ApiCall.GET_CONTENT_WIDTH,
        ApiCall.GET_FAVICON,
        ApiCall.GET_HIT_TEST_RESULT,
        ApiCall.GET_HTTP_AUTH_USERNAME_PASSWORD,
        ApiCall.GET_ORIGINAL_URL,
        ApiCall.GET_PROGRESS,
        ApiCall.GET_SCALE,
        ApiCall.GET_SETTINGS,
        ApiCall.GET_TEXT_CLASSIFIER,
        ApiCall.GET_TITLE,
        ApiCall.GET_URL,
        ApiCall.GET_WEBCHROME_CLIENT,
        ApiCall.GET_WEBVIEW_CLIENT,
        ApiCall.GO_BACK,
        ApiCall.GO_BACK_OR_FORWARD,
        ApiCall.GO_FORWARD,
        ApiCall.INSERT_VISUAL_STATE_CALLBACK,
        ApiCall.INVOKE_ZOOM_PICKER,
        ApiCall.IS_PAUSED,
        ApiCall.IS_PRIVATE_BROWSING_ENABLED,
        ApiCall.LOAD_DATA,
        ApiCall.LOAD_DATA_WITH_BASE_URL,
        ApiCall.NOTIFY_FIND_DIALOG_DISMISSED,
        ApiCall.ON_PAUSE,
        ApiCall.ON_PROVIDE_AUTOFILL_VIRTUAL_STRUCTURE,
        ApiCall.ON_RESUME,
        ApiCall.OVERLAY_HORIZONTAL_SCROLLBAR,
        ApiCall.OVERLAY_VERTICAL_SCROLLBAR,
        ApiCall.PAGE_DOWN,
        ApiCall.PAGE_UP,
        ApiCall.PAUSE_TIMERS,
        ApiCall.POST_MESSAGE_TO_MAIN_FRAME,
        ApiCall.POST_URL,
        ApiCall.RELOAD,
        ApiCall.REMOVE_JAVASCRIPT_INTERFACE,
        ApiCall.REQUEST_FOCUS_NODE_HREF,
        ApiCall.REQUEST_IMAGE_REF,
        ApiCall.RESTORE_STATE,
        ApiCall.RESUME_TIMERS,
        ApiCall.SAVE_STATE,
        ApiCall.SET_DOWNLOAD_LISTENER,
        ApiCall.SET_FIND_LISTENER,
        ApiCall.SET_HORIZONTAL_SCROLLBAR_OVERLAY,
        ApiCall.SET_HTTP_AUTH_USERNAME_PASSWORD,
        ApiCall.SET_INITIAL_SCALE,
        ApiCall.SET_NETWORK_AVAILABLE,
        ApiCall.SET_PICTURE_LISTENER,
        ApiCall.SET_SMART_CLIP_RESULT_HANDLER,
        ApiCall.SET_TEXT_CLASSIFIER,
        ApiCall.SET_VERTICAL_SCROLLBAR_OVERLAY,
        ApiCall.SET_WEBCHROME_CLIENT,
        ApiCall.SET_WEBVIEW_CLIENT,
        ApiCall.SHOW_FIND_DIALOG,
        ApiCall.STOP_LOADING,
        ApiCall.WEBVIEW_DATABASE_CLEAR_FORM_DATA,
        ApiCall.WEBVIEW_DATABASE_CLEAR_HTTP_AUTH_USERNAME_PASSWORD,
        ApiCall.WEBVIEW_DATABASE_CLEAR_USERNAME_PASSWORD,
        ApiCall.WEBVIEW_DATABASE_GET_HTTP_AUTH_USERNAME_PASSWORD,
        ApiCall.WEBVIEW_DATABASE_HAS_FORM_DATA,
        ApiCall.WEBVIEW_DATABASE_HAS_HTTP_AUTH_USERNAME_PASSWORD,
        ApiCall.WEBVIEW_DATABASE_HAS_USERNAME_PASSWORD,
        ApiCall.WEBVIEW_DATABASE_SET_HTTP_AUTH_USERNAME_PASSWORD,
        ApiCall.COOKIE_MANAGER_ACCEPT_COOKIE,
        ApiCall.COOKIE_MANAGER_ACCEPT_THIRD_PARTY_COOKIES,
        ApiCall.COOKIE_MANAGER_FLUSH,
        ApiCall.COOKIE_MANAGER_GET_COOKIE,
        ApiCall.COOKIE_MANAGER_HAS_COOKIES,
        ApiCall.COOKIE_MANAGER_REMOVE_ALL_COOKIE,
        ApiCall.COOKIE_MANAGER_REMOVE_ALL_COOKIES,
        ApiCall.COOKIE_MANAGER_REMOVE_EXPIRED_COOKIE,
        ApiCall.COOKIE_MANAGER_REMOVE_SESSION_COOKIE,
        ApiCall.COOKIE_MANAGER_REMOVE_SESSION_COOKIES,
        ApiCall.COOKIE_MANAGER_SET_ACCEPT_COOKIE,
        ApiCall.COOKIE_MANAGER_SET_ACCEPT_FILE_SCHEME_COOKIES,
        ApiCall.COOKIE_MANAGER_SET_ACCEPT_THIRD_PARTY_COOKIES,
        ApiCall.COOKIE_MANAGER_SET_COOKIE,
        ApiCall.WEB_STORAGE_DELETE_ALL_DATA,
        ApiCall.WEB_STORAGE_DELETE_ORIGIN,
        ApiCall.WEB_STORAGE_GET_ORIGINS,
        ApiCall.WEB_STORAGE_GET_QUOTA_FOR_ORIGIN,
        ApiCall.WEB_STORAGE_GET_USAGE_FOR_ORIGIN,
        ApiCall.WEB_SETTINGS_GET_ALLOW_CONTENT_ACCESS,
        ApiCall.WEB_SETTINGS_GET_ALLOW_FILE_ACCESS,
        ApiCall.WEB_SETTINGS_GET_ALLOW_FILE_ACCESS_FROM_FILE_URLS,
        ApiCall.WEB_SETTINGS_GET_ALLOW_UNIVERSAL_ACCESS_FROM_FILE_URLS,
        ApiCall.WEB_SETTINGS_GET_BLOCK_NETWORK_IMAGE,
        ApiCall.WEB_SETTINGS_GET_BLOCK_NETWORK_LOADS,
        ApiCall.WEB_SETTINGS_GET_BUILT_IN_ZOOM_CONTROLS,
        ApiCall.WEB_SETTINGS_GET_CACHE_MODE,
        ApiCall.WEB_SETTINGS_GET_CURSIVE_FONT_FAMILY,
        ApiCall.WEB_SETTINGS_GET_DATABASE_ENABLED,
        ApiCall.WEB_SETTINGS_GET_DEFAULT_FIXED_FONT_SIZE,
        ApiCall.WEB_SETTINGS_GET_DEFAULT_FONT_SIZE,
        ApiCall.WEB_SETTINGS_GET_DEFAULT_TEXT_ENCODING_NAME,
        ApiCall.WEB_SETTINGS_GET_DISABLED_ACTION_MODE_MENU_ITEMS,
        ApiCall.WEB_SETTINGS_GET_DISPLAY_ZOOM_CONTROLS,
        ApiCall.WEB_SETTINGS_GET_DOM_STORAGE_ENABLED,
        ApiCall.WEB_SETTINGS_GET_FANTASY_FONT_FAMILY,
        ApiCall.WEB_SETTINGS_GET_FIXED_FONT_FAMILY,
        ApiCall.WEB_SETTINGS_GET_FORCE_DARK,
        ApiCall.WEB_SETTINGS_GET_JAVA_SCRIPT_CAN_OPEN_WINDOWS_AUTOMATICALLY,
        ApiCall.WEB_SETTINGS_GET_JAVA_SCRIPT_ENABLED,
        ApiCall.WEB_SETTINGS_GET_LAYOUT_ALGORITHM,
        ApiCall.WEB_SETTINGS_GET_LOAD_WITH_OVERVIEW_MODE,
        ApiCall.WEB_SETTINGS_GET_LOADS_IMAGES_AUTOMATICALLY,
        ApiCall.WEB_SETTINGS_GET_MEDIA_PLAYBACK_REQUIRES_USER_GESTURE,
        ApiCall.WEB_SETTINGS_GET_MINIMUM_FONT_SIZE,
        ApiCall.WEB_SETTINGS_GET_MINIMUM_LOGICAL_FONT_SIZE,
        ApiCall.WEB_SETTINGS_GET_MIXED_CONTENT_MODE,
        ApiCall.WEB_SETTINGS_GET_OFFSCREEN_PRE_RASTER,
        ApiCall.WEB_SETTINGS_GET_PLUGIN_STATE,
        ApiCall.WEB_SETTINGS_GET_SAFE_BROWSING_ENABLED,
        ApiCall.WEB_SETTINGS_GET_SANS_SERIF_FONT_FAMILY,
        ApiCall.WEB_SETTINGS_GET_SAVE_FORM_DATA,
        ApiCall.WEB_SETTINGS_GET_SERIF_FONT_FAMILY,
        ApiCall.WEB_SETTINGS_GET_STANDARD_FONT_FAMILY,
        ApiCall.WEB_SETTINGS_GET_TEXT_ZOOM,
        ApiCall.WEB_SETTINGS_GET_USE_WIDE_VIEW_PORT,
        ApiCall.WEB_SETTINGS_GET_USER_AGENT_STRING,
        ApiCall.WEB_SETTINGS_SET_ALLOW_CONTENT_ACCESS,
        ApiCall.WEB_SETTINGS_SET_ALLOW_FILE_ACCESS,
        ApiCall.WEB_SETTINGS_SET_ALLOW_FILE_ACCESS_FROM_FILE_URLS,
        ApiCall.WEB_SETTINGS_SET_ALLOW_UNIVERSAL_ACCESS_FROM_FILE_URLS,
        ApiCall.WEB_SETTINGS_SET_BLOCK_NETWORK_IMAGE,
        ApiCall.WEB_SETTINGS_SET_BLOCK_NETWORK_LOADS,
        ApiCall.WEB_SETTINGS_SET_BUILT_IN_ZOOM_CONTROLS,
        ApiCall.WEB_SETTINGS_SET_CACHE_MODE,
        ApiCall.WEB_SETTINGS_SET_CURSIVE_FONT_FAMILY,
        ApiCall.WEB_SETTINGS_SET_DATABASE_ENABLED,
        ApiCall.WEB_SETTINGS_SET_DEFAULT_FIXED_FONT_SIZE,
        ApiCall.WEB_SETTINGS_SET_DEFAULT_FONT_SIZE,
        ApiCall.WEB_SETTINGS_SET_DEFAULT_TEXT_ENCODING_NAME,
        ApiCall.WEB_SETTINGS_SET_DISABLED_ACTION_MODE_MENU_ITEMS,
        ApiCall.WEB_SETTINGS_SET_DISPLAY_ZOOM_CONTROLS,
        ApiCall.WEB_SETTINGS_SET_DOM_STORAGE_ENABLED,
        ApiCall.WEB_SETTINGS_SET_FANTASY_FONT_FAMILY,
        ApiCall.WEB_SETTINGS_SET_FIXED_FONT_FAMILY,
        ApiCall.WEB_SETTINGS_SET_GEOLOCATION_ENABLED,
        ApiCall.WEB_SETTINGS_SET_JAVA_SCRIPT_CAN_OPEN_WINDOWS_AUTOMATICALLY,
        ApiCall.WEB_SETTINGS_SET_JAVA_SCRIPT_ENABLED,
        ApiCall.WEB_SETTINGS_SET_LAYOUT_ALGORITHM,
        ApiCall.WEB_SETTINGS_SET_LOAD_WITH_OVERVIEW_MODE,
        ApiCall.WEB_SETTINGS_SET_LOADS_IMAGES_AUTOMATICALLY,
        ApiCall.WEB_SETTINGS_SET_MEDIA_PLAYBACK_REQUIRES_USER_GESTURE,
        ApiCall.WEB_SETTINGS_SET_MINIMUM_FONT_SIZE,
        ApiCall.WEB_SETTINGS_SET_MINIMUM_LOGICAL_FONT_SIZE,
        ApiCall.WEB_SETTINGS_SET_MIXED_CONTENT_MODE,
        ApiCall.WEB_SETTINGS_SET_NEED_INITIAL_FOCUS,
        ApiCall.WEB_SETTINGS_SET_OFFSCREEN_PRE_RASTER,
        ApiCall.WEB_SETTINGS_SET_PLUGIN_STATE,
        ApiCall.WEB_SETTINGS_SET_SAFE_BROWSING_ENABLED,
        ApiCall.WEB_SETTINGS_SET_SANS_SERIF_FONT_FAMILY,
        ApiCall.WEB_SETTINGS_SET_SAVE_FORM_DATA,
        ApiCall.WEB_SETTINGS_SET_SERIF_FONT_FAMILY,
        ApiCall.WEB_SETTINGS_SET_STANDARD_FONT_FAMILY,
        ApiCall.WEB_SETTINGS_SET_SUPPORT_MULTIPLE_WINDOWS,
        ApiCall.WEB_SETTINGS_SET_SUPPORT_ZOOM,
        ApiCall.WEB_SETTINGS_SET_TEXT_SIZE,
        ApiCall.WEB_SETTINGS_SET_TEXT_ZOOM,
        ApiCall.WEB_SETTINGS_SET_USE_WIDE_VIEW_PORT,
        ApiCall.WEB_SETTINGS_SET_USER_AGENT_STRING,
        ApiCall.WEB_SETTINGS_SUPPORT_MULTIPLE_WINDOWS,
        ApiCall.WEB_SETTINGS_SUPPORT_ZOOM,
        ApiCall.GET_RENDERER_REQUESTED_PRIORITY,
        ApiCall.GET_RENDERER_PRIORITY_WAIVED_WHEN_NOT_VISIBLE,
        ApiCall.SET_RENDERER_PRIORITY_POLICY,
        ApiCall.LOAD_URL,
        ApiCall.LOAD_URL_ADDITIONAL_HEADERS,
        ApiCall.DESTROY,
        ApiCall.SAVE_WEB_ARCHIVE,
        ApiCall.FIND_ALL_ASYNC,
        ApiCall.GET_WEBVIEW_RENDER_PROCESS,
        ApiCall.SET_WEBVIEW_RENDER_PROCESS_CLIENT,
        ApiCall.GET_WEBVIEW_RENDER_PROCESS_CLIENT,
        ApiCall.FLING_SCROLL,
        ApiCall.ZOOM_IN,
        ApiCall.ZOOM_OUT,
        ApiCall.ZOOM_BY,
        ApiCall.ON_PROVIDE_CONTENT_CAPTURE_STRUCTURE,
        ApiCall.GET_ACCESSIBILITY_NODE_PROVIDER,
        ApiCall.ON_PROVIDE_VIRTUAL_STRUCTURE,
        ApiCall.SET_OVERSCROLL_MODE,
        ApiCall.SET_SCROLL_BAR_STYLE,
        ApiCall.SET_LAYOUT_PARAMS,
        ApiCall.PERFORM_LONG_CLICK,
        ApiCall.REQUEST_FOCUS,
        ApiCall.REQUEST_CHILD_RECTANGLE_ON_SCREEN,
        ApiCall.SET_BACKGROUND_COLOR,
        ApiCall.SET_LAYER_TYPE,
        ApiCall.GET_HANDLER,
        ApiCall.FIND_FOCUS,
        ApiCall.COMPUTE_SCROLL,
        ApiCall.SET_WEB_VIEW_CLIENT,
        ApiCall.WEB_SETTINGS_SET_USER_AGENT,
        ApiCall.WEB_SETTINGS_SET_FORCE_DARK,
        ApiCall.WEB_SETTINGS_SET_ALGORITHMIC_DARKENING_ALLOWED,
        ApiCall.WEB_SETTINGS_IS_ALGORITHMIC_DARKENING_ALLOWED,
        ApiCall.COOKIE_MANAGER_ALLOW_FILE_SCHEME_COOKIES,
        ApiCall.WEB_ICON_DATABASE_BULK_REQUEST_ICON_FOR_PAGE_URL,
        ApiCall.WEB_ICON_DATABASE_CLOSE,
        ApiCall.WEB_ICON_DATABASE_GET_INSTANCE,
        ApiCall.WEB_ICON_DATABASE_OPEN,
        ApiCall.WEB_ICON_DATABASE_RELEASE_ICON_FOR_PAGE_URL,
        ApiCall.WEB_ICON_DATABASE_REMOVE_ALL_ICONS,
        ApiCall.WEB_ICON_DATABASE_REQUEST_ICON_FOR_PAGE_URL,
        ApiCall.WEB_ICON_DATABASE_RETAIN_ICON_FOR_PAGE_URL,
        ApiCall.GEOLOCATION_PERMISSIONS_ALLOW,
        ApiCall.GEOLOCATION_PERMISSIONS_CLEAR,
        ApiCall.GEOLOCATION_PERMISSIONS_CLEAR_ALL,
        ApiCall.GEOLOCATION_PERMISSIONS_GET_ALLOWED,
        ApiCall.GEOLOCATION_PERMISSIONS_GET_ORIGINS
    })
    @interface ApiCall {
        int ADD_JAVASCRIPT_INTERFACE = 0;
        int AUTOFILL = 1;
        int CAN_GO_BACK = 2;
        int CAN_GO_BACK_OR_FORWARD = 3;
        int CAN_GO_FORWARD = 4;
        int CAN_ZOOM_IN = 5;
        int CAN_ZOOM_OUT = 6;
        int CAPTURE_PICTURE = 7;
        int CLEAR_CACHE = 8;
        int CLEAR_FORM_DATA = 9;
        int CLEAR_HISTORY = 10;
        int CLEAR_MATCHES = 11;
        int CLEAR_SSL_PREFERENCES = 12;
        int CLEAR_VIEW = 13;
        int COPY_BACK_FORWARD_LIST = 14;
        int CREATE_PRINT_DOCUMENT_ADAPTER = 15;
        int CREATE_WEBMESSAGE_CHANNEL = 16;
        int DOCUMENT_HAS_IMAGES = 17;
        int DOES_SUPPORT_FULLSCREEN = 18;
        int EVALUATE_JAVASCRIPT = 19;
        int EXTRACT_SMART_CLIP_DATA = 20;
        int FIND_NEXT = 21;
        int GET_CERTIFICATE = 22;
        int GET_CONTENT_HEIGHT = 23;
        int GET_CONTENT_WIDTH = 24;
        int GET_FAVICON = 25;
        int GET_HIT_TEST_RESULT = 26;
        int GET_HTTP_AUTH_USERNAME_PASSWORD = 27;
        int GET_ORIGINAL_URL = 28;
        int GET_PROGRESS = 29;
        int GET_SCALE = 30;
        int GET_SETTINGS = 31;
        int GET_TEXT_CLASSIFIER = 32;
        int GET_TITLE = 33;
        int GET_URL = 34;
        int GET_WEBCHROME_CLIENT = 35;
        int GET_WEBVIEW_CLIENT = 36;
        int GO_BACK = 37;
        int GO_BACK_OR_FORWARD = 38;
        int GO_FORWARD = 39;
        int INSERT_VISUAL_STATE_CALLBACK = 40;
        int INVOKE_ZOOM_PICKER = 41;
        int IS_PAUSED = 42;
        int IS_PRIVATE_BROWSING_ENABLED = 43;
        int LOAD_DATA = 44;
        int LOAD_DATA_WITH_BASE_URL = 45;
        int NOTIFY_FIND_DIALOG_DISMISSED = 46;
        int ON_PAUSE = 47;
        int ON_PROVIDE_AUTOFILL_VIRTUAL_STRUCTURE = 48;
        int ON_RESUME = 49;
        int OVERLAY_HORIZONTAL_SCROLLBAR = 50;
        int OVERLAY_VERTICAL_SCROLLBAR = 51;
        int PAGE_DOWN = 52;
        int PAGE_UP = 53;
        int PAUSE_TIMERS = 54;
        int POST_MESSAGE_TO_MAIN_FRAME = 55;
        int POST_URL = 56;
        int RELOAD = 57;
        int REMOVE_JAVASCRIPT_INTERFACE = 58;
        int REQUEST_FOCUS_NODE_HREF = 59;
        int REQUEST_IMAGE_REF = 60;
        int RESTORE_STATE = 61;
        int RESUME_TIMERS = 62;
        int SAVE_STATE = 63;
        int SET_DOWNLOAD_LISTENER = 64;
        int SET_FIND_LISTENER = 65;
        int SET_HORIZONTAL_SCROLLBAR_OVERLAY = 66;
        int SET_HTTP_AUTH_USERNAME_PASSWORD = 67;
        int SET_INITIAL_SCALE = 68;
        int SET_NETWORK_AVAILABLE = 69;
        int SET_PICTURE_LISTENER = 70;
        int SET_SMART_CLIP_RESULT_HANDLER = 71;
        int SET_TEXT_CLASSIFIER = 72;
        int SET_VERTICAL_SCROLLBAR_OVERLAY = 73;
        int SET_WEBCHROME_CLIENT = 74;
        int SET_WEBVIEW_CLIENT = 75;
        int SHOW_FIND_DIALOG = 76;
        int STOP_LOADING = 77;
        int WEBVIEW_DATABASE_GET_HTTP_AUTH_USERNAME_PASSWORD = 78;
        int WEBVIEW_DATABASE_CLEAR_FORM_DATA = 79;
        int WEBVIEW_DATABASE_CLEAR_HTTP_AUTH_USERNAME_PASSWORD = 80;
        int WEBVIEW_DATABASE_CLEAR_USERNAME_PASSWORD = 81;
        int WEBVIEW_DATABASE_HAS_FORM_DATA = 82;
        int WEBVIEW_DATABASE_HAS_HTTP_AUTH_USERNAME_PASSWORD = 83;
        int WEBVIEW_DATABASE_HAS_USERNAME_PASSWORD = 84;
        int WEBVIEW_DATABASE_SET_HTTP_AUTH_USERNAME_PASSWORD = 85;
        int COOKIE_MANAGER_ACCEPT_COOKIE = 86;
        int COOKIE_MANAGER_ACCEPT_THIRD_PARTY_COOKIES = 87;
        int COOKIE_MANAGER_FLUSH = 88;
        int COOKIE_MANAGER_GET_COOKIE = 89;
        int COOKIE_MANAGER_HAS_COOKIES = 90;
        int COOKIE_MANAGER_REMOVE_ALL_COOKIE = 91;
        int COOKIE_MANAGER_REMOVE_ALL_COOKIES = 92;
        int COOKIE_MANAGER_REMOVE_EXPIRED_COOKIE = 93;
        int COOKIE_MANAGER_REMOVE_SESSION_COOKIE = 94;
        int COOKIE_MANAGER_REMOVE_SESSION_COOKIES = 95;
        int COOKIE_MANAGER_SET_ACCEPT_COOKIE = 96;
        int COOKIE_MANAGER_SET_ACCEPT_FILE_SCHEME_COOKIES = 97;
        int COOKIE_MANAGER_SET_ACCEPT_THIRD_PARTY_COOKIES = 98;
        int COOKIE_MANAGER_SET_COOKIE = 99;
        int WEB_STORAGE_DELETE_ALL_DATA = 100;
        int WEB_STORAGE_DELETE_ORIGIN = 101;
        int WEB_STORAGE_GET_ORIGINS = 102;
        int WEB_STORAGE_GET_QUOTA_FOR_ORIGIN = 103;
        int WEB_STORAGE_GET_USAGE_FOR_ORIGIN = 104;
        int WEB_SETTINGS_GET_ALLOW_CONTENT_ACCESS = 105;
        int WEB_SETTINGS_GET_ALLOW_FILE_ACCESS = 106;
        int WEB_SETTINGS_GET_ALLOW_FILE_ACCESS_FROM_FILE_URLS = 107;
        int WEB_SETTINGS_GET_ALLOW_UNIVERSAL_ACCESS_FROM_FILE_URLS = 108;
        int WEB_SETTINGS_GET_BLOCK_NETWORK_IMAGE = 109;
        int WEB_SETTINGS_GET_BLOCK_NETWORK_LOADS = 110;
        int WEB_SETTINGS_GET_BUILT_IN_ZOOM_CONTROLS = 111;
        int WEB_SETTINGS_GET_CACHE_MODE = 112;
        int WEB_SETTINGS_GET_CURSIVE_FONT_FAMILY = 113;
        int WEB_SETTINGS_GET_DATABASE_ENABLED = 114;
        int WEB_SETTINGS_GET_DEFAULT_FIXED_FONT_SIZE = 115;
        int WEB_SETTINGS_GET_DEFAULT_FONT_SIZE = 116;
        int WEB_SETTINGS_GET_DEFAULT_TEXT_ENCODING_NAME = 117;
        int WEB_SETTINGS_GET_DISABLED_ACTION_MODE_MENU_ITEMS = 118;
        int WEB_SETTINGS_GET_DISPLAY_ZOOM_CONTROLS = 119;
        int WEB_SETTINGS_GET_DOM_STORAGE_ENABLED = 120;
        int WEB_SETTINGS_GET_FANTASY_FONT_FAMILY = 121;
        int WEB_SETTINGS_GET_FIXED_FONT_FAMILY = 122;
        int WEB_SETTINGS_GET_FORCE_DARK = 123;
        int WEB_SETTINGS_GET_JAVA_SCRIPT_CAN_OPEN_WINDOWS_AUTOMATICALLY = 124;
        int WEB_SETTINGS_GET_JAVA_SCRIPT_ENABLED = 125;
        int WEB_SETTINGS_GET_LAYOUT_ALGORITHM = 126;
        int WEB_SETTINGS_GET_LOAD_WITH_OVERVIEW_MODE = 127;
        int WEB_SETTINGS_GET_LOADS_IMAGES_AUTOMATICALLY = 128;
        int WEB_SETTINGS_GET_MEDIA_PLAYBACK_REQUIRES_USER_GESTURE = 129;
        int WEB_SETTINGS_GET_MINIMUM_FONT_SIZE = 130;
        int WEB_SETTINGS_GET_MINIMUM_LOGICAL_FONT_SIZE = 131;
        int WEB_SETTINGS_GET_MIXED_CONTENT_MODE = 132;
        int WEB_SETTINGS_GET_OFFSCREEN_PRE_RASTER = 133;
        int WEB_SETTINGS_GET_PLUGIN_STATE = 134;
        int WEB_SETTINGS_GET_SAFE_BROWSING_ENABLED = 135;
        int WEB_SETTINGS_GET_SANS_SERIF_FONT_FAMILY = 136;
        int WEB_SETTINGS_GET_SAVE_FORM_DATA = 137;
        int WEB_SETTINGS_GET_SERIF_FONT_FAMILY = 138;
        int WEB_SETTINGS_GET_STANDARD_FONT_FAMILY = 139;
        int WEB_SETTINGS_GET_TEXT_ZOOM = 140;
        int WEB_SETTINGS_GET_USE_WIDE_VIEW_PORT = 141;
        int WEB_SETTINGS_GET_USER_AGENT_STRING = 142;
        int WEB_SETTINGS_SET_ALLOW_CONTENT_ACCESS = 143;
        int WEB_SETTINGS_SET_ALLOW_FILE_ACCESS = 144;
        int WEB_SETTINGS_SET_ALLOW_FILE_ACCESS_FROM_FILE_URLS = 145;
        int WEB_SETTINGS_SET_ALLOW_UNIVERSAL_ACCESS_FROM_FILE_URLS = 146;
        int WEB_SETTINGS_SET_BLOCK_NETWORK_IMAGE = 147;
        int WEB_SETTINGS_SET_BLOCK_NETWORK_LOADS = 148;
        int WEB_SETTINGS_SET_BUILT_IN_ZOOM_CONTROLS = 149;
        int WEB_SETTINGS_SET_CACHE_MODE = 150;
        int WEB_SETTINGS_SET_CURSIVE_FONT_FAMILY = 151;
        int WEB_SETTINGS_SET_DATABASE_ENABLED = 152;
        int WEB_SETTINGS_SET_DEFAULT_FIXED_FONT_SIZE = 153;
        int WEB_SETTINGS_SET_DEFAULT_FONT_SIZE = 154;
        int WEB_SETTINGS_SET_DEFAULT_TEXT_ENCODING_NAME = 155;
        int WEB_SETTINGS_SET_DISABLED_ACTION_MODE_MENU_ITEMS = 156;
        int WEB_SETTINGS_SET_DISPLAY_ZOOM_CONTROLS = 157;
        int WEB_SETTINGS_SET_DOM_STORAGE_ENABLED = 158;
        int WEB_SETTINGS_SET_FANTASY_FONT_FAMILY = 159;
        int WEB_SETTINGS_SET_FIXED_FONT_FAMILY = 160;
        int WEB_SETTINGS_SET_GEOLOCATION_ENABLED = 161;
        int WEB_SETTINGS_SET_JAVA_SCRIPT_CAN_OPEN_WINDOWS_AUTOMATICALLY = 162;
        int WEB_SETTINGS_SET_JAVA_SCRIPT_ENABLED = 163;
        int WEB_SETTINGS_SET_LAYOUT_ALGORITHM = 164;
        int WEB_SETTINGS_SET_LOAD_WITH_OVERVIEW_MODE = 165;
        int WEB_SETTINGS_SET_LOADS_IMAGES_AUTOMATICALLY = 166;
        int WEB_SETTINGS_SET_MEDIA_PLAYBACK_REQUIRES_USER_GESTURE = 167;
        int WEB_SETTINGS_SET_MINIMUM_FONT_SIZE = 168;
        int WEB_SETTINGS_SET_MINIMUM_LOGICAL_FONT_SIZE = 169;
        int WEB_SETTINGS_SET_MIXED_CONTENT_MODE = 170;
        int WEB_SETTINGS_SET_NEED_INITIAL_FOCUS = 171;
        int WEB_SETTINGS_SET_OFFSCREEN_PRE_RASTER = 172;
        int WEB_SETTINGS_SET_PLUGIN_STATE = 173;
        int WEB_SETTINGS_SET_SAFE_BROWSING_ENABLED = 174;
        int WEB_SETTINGS_SET_SANS_SERIF_FONT_FAMILY = 175;
        int WEB_SETTINGS_SET_SAVE_FORM_DATA = 176;
        int WEB_SETTINGS_SET_SERIF_FONT_FAMILY = 177;
        int WEB_SETTINGS_SET_STANDARD_FONT_FAMILY = 178;
        int WEB_SETTINGS_SET_SUPPORT_MULTIPLE_WINDOWS = 179;
        int WEB_SETTINGS_SET_SUPPORT_ZOOM = 180;
        int WEB_SETTINGS_SET_TEXT_SIZE = 181;
        int WEB_SETTINGS_SET_TEXT_ZOOM = 182;
        int WEB_SETTINGS_SET_USE_WIDE_VIEW_PORT = 183;
        int WEB_SETTINGS_SET_USER_AGENT_STRING = 184;
        int WEB_SETTINGS_SUPPORT_MULTIPLE_WINDOWS = 185;
        int WEB_SETTINGS_SUPPORT_ZOOM = 186;
        int GET_RENDERER_REQUESTED_PRIORITY = 187;
        int GET_RENDERER_PRIORITY_WAIVED_WHEN_NOT_VISIBLE = 188;
        int SET_RENDERER_PRIORITY_POLICY = 189;
        int LOAD_URL = 190;
        int LOAD_URL_ADDITIONAL_HEADERS = 191;
        int DESTROY = 192;
        int SAVE_WEB_ARCHIVE = 193;
        int FIND_ALL_ASYNC = 194;
        int GET_WEBVIEW_RENDER_PROCESS = 195;
        int SET_WEBVIEW_RENDER_PROCESS_CLIENT = 196;
        int GET_WEBVIEW_RENDER_PROCESS_CLIENT = 197;
        int FLING_SCROLL = 198;
        int ZOOM_IN = 199;
        int ZOOM_OUT = 200;
        int ZOOM_BY = 201;
        int ON_PROVIDE_CONTENT_CAPTURE_STRUCTURE = 202;
        int GET_ACCESSIBILITY_NODE_PROVIDER = 203;
        int ON_PROVIDE_VIRTUAL_STRUCTURE = 204;
        int SET_OVERSCROLL_MODE = 205;
        int SET_SCROLL_BAR_STYLE = 206;
        int SET_LAYOUT_PARAMS = 207;
        int PERFORM_LONG_CLICK = 208;
        int REQUEST_FOCUS = 209;
        int REQUEST_CHILD_RECTANGLE_ON_SCREEN = 210;
        int SET_BACKGROUND_COLOR = 211;
        int SET_LAYER_TYPE = 212;
        int GET_HANDLER = 213;
        int FIND_FOCUS = 214;
        int COMPUTE_SCROLL = 215;
        int SET_WEB_VIEW_CLIENT = 216;
        int WEB_SETTINGS_SET_USER_AGENT = 217;
        int WEB_SETTINGS_SET_FORCE_DARK = 218;
        int WEB_SETTINGS_SET_ALGORITHMIC_DARKENING_ALLOWED = 219;
        int WEB_SETTINGS_IS_ALGORITHMIC_DARKENING_ALLOWED = 220;
        int COOKIE_MANAGER_ALLOW_FILE_SCHEME_COOKIES = 221;
        int WEB_ICON_DATABASE_BULK_REQUEST_ICON_FOR_PAGE_URL = 222;
        int WEB_ICON_DATABASE_CLOSE = 223;
        int WEB_ICON_DATABASE_GET_INSTANCE = 224;
        int WEB_ICON_DATABASE_OPEN = 225;
        int WEB_ICON_DATABASE_RELEASE_ICON_FOR_PAGE_URL = 226;
        int WEB_ICON_DATABASE_REMOVE_ALL_ICONS = 227;
        int WEB_ICON_DATABASE_REQUEST_ICON_FOR_PAGE_URL = 228;
        int WEB_ICON_DATABASE_RETAIN_ICON_FOR_PAGE_URL = 229;
        int GEOLOCATION_PERMISSIONS_ALLOW = 230;
        int GEOLOCATION_PERMISSIONS_CLEAR = 231;
        int GEOLOCATION_PERMISSIONS_CLEAR_ALL = 232;
        int GEOLOCATION_PERMISSIONS_GET_ALLOWED = 233;
        int GEOLOCATION_PERMISSIONS_GET_ORIGINS = 234;
        int COUNT = 235;
    }

    // LINT.ThenChange(/tools/metrics/histograms/metadata/android/enums.xml:WebViewApiCall)

    public static void recordWebViewApiCall(@ApiCall int sample) {
        RecordHistogram.recordEnumeratedHistogram("Android.WebView.ApiCall", sample, ApiCall.COUNT);
    }

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        SystemApiCall.ON_TOUCH_EVENT,
        SystemApiCall.ON_DRAG_EVENT,
        SystemApiCall.ON_CREATE_INPUT_CONNECTION,
        SystemApiCall.ON_KEY_MULTIPLE,
        SystemApiCall.ON_KEY_DOWN,
        SystemApiCall.ON_KEY_UP,
        SystemApiCall.ON_FOCUS_CHANGED,
        SystemApiCall.DISPATCH_KEY_EVENT,
        SystemApiCall.ON_HOVER_EVENT,
        SystemApiCall.ON_GENERIC_MOTION_EVENT,
        SystemApiCall.ON_TRACKBALL_EVENT,
        SystemApiCall.ON_START_TEMPORARY_DETACH,
        SystemApiCall.ON_FINISH_TEMPORARY_DETACH,
        SystemApiCall.ON_CHECK_IS_TEXT_EDITOR,
        SystemApiCall.ON_WINDOW_FOCUS_CHANGED,
        SystemApiCall.COUNT, // Added to suppress WrongConstant in #recordWebViewSystemApiCall
    })
    public @interface SystemApiCall {
        int ON_TOUCH_EVENT = 0;
        int ON_DRAG_EVENT = 1;
        int ON_CREATE_INPUT_CONNECTION = 2;
        int ON_KEY_MULTIPLE = 3;
        int ON_KEY_DOWN = 4;
        int ON_KEY_UP = 5;
        int ON_FOCUS_CHANGED = 6;
        int DISPATCH_KEY_EVENT = 7;
        int ON_HOVER_EVENT = 8;
        int ON_GENERIC_MOTION_EVENT = 9;
        int ON_TRACKBALL_EVENT = 10;
        int ON_START_TEMPORARY_DETACH = 11;
        int ON_FINISH_TEMPORARY_DETACH = 12;
        int ON_CHECK_IS_TEXT_EDITOR = 13;
        int ON_WINDOW_FOCUS_CHANGED = 14;
        // Remember to update SystemWebViewApiCall in enums.xml when adding new values here
        int COUNT = 15;
    }

    public static void recordWebViewSystemApiCall(@SystemApiCall int sample) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.ApiCall.System", sample, SystemApiCall.COUNT);
    }

    // This does not touch any global / non-threadsafe state, but note that
    // init is called right after and is NOT threadsafe.
    public WebViewChromium(
            WebViewChromiumFactoryProvider factory,
            WebView webView,
            WebView.PrivateAccess webViewPrivate,
            boolean shouldDisableThreadChecking) {
        try (ScopedSysTraceEvent e1 = ScopedSysTraceEvent.scoped("WebViewChromium.constructor")) {
            WebViewChromiumFactoryProvider.checkStorageIsNotDeviceProtected(webView.getContext());
            mWebView = webView;
            mWebViewPrivate = webViewPrivate;
            mHitTestResult = new WebView.HitTestResult();
            mContext = ClassLoaderContextWrapperFactory.get(mWebView.getContext());
            mAppTargetSdkVersion = mContext.getApplicationInfo().targetSdkVersion;
            mFactory = factory;
            mShouldDisableThreadChecking = shouldDisableThreadChecking;
            factory.addWebViewAssetPath(mWebView.getContext());
            mSharedWebViewChromium =
                    new SharedWebViewChromium(mFactory.getRunQueue(), mFactory.getAwInit());
        }
    }

    // See //android_webview/docs/how-does-on-create-window-work.md for more details.
    static void completeWindowCreation(WebView parent, WebView child) {
        AwContents parentContents = ((WebViewChromium) parent.getWebViewProvider()).mAwContents;
        AwContents childContents =
                child == null ? null : ((WebViewChromium) child.getWebViewProvider()).mAwContents;
        parentContents.supplyContentsForPopup(childContents);
    }

    // WebViewProvider methods --------------------------------------------------------------------

    @Override
    // BUG=6790250 |javaScriptInterfaces| was only ever used by the obsolete DumpRenderTree
    // so is ignored. TODO: remove it from WebViewProvider.
    public void init(
            final Map<String, Object> javaScriptInterfaces, final boolean privateBrowsing) {
        long startTime = SystemClock.uptimeMillis();
        boolean isFirstWebViewInit = !mFactory.hasStarted();
        try (ScopedSysTraceEvent e1 = ScopedSysTraceEvent.scoped("WebViewChromium.init")) {
            if (privateBrowsing) {
                mFactory.startYourEngines(true);
                final String msg = "Private browsing is not supported in WebView.";
                if (mAppTargetSdkVersion >= Build.VERSION_CODES.KITKAT) {
                    throw new IllegalArgumentException(msg);
                } else {
                    Log.w(TAG, msg);
                    TextView warningLabel = new TextView(mContext);
                    warningLabel.setText(mContext.getString(R.string.private_browsing_warning));
                    mWebView.addView(warningLabel);
                }
            }

            // Needed for https://crbug.com/1417872
            mWebView.setDefaultFocusHighlightEnabled(false);

            if (mAppTargetSdkVersion >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
                // If the app targets >= JB MR2 then we require that WebView is only used from a
                // single thread. So, we:
                // 1) start Chromium using the current thread as the UI thread (this is a no-op if
                //    it was already started).
                mFactory.startYourEngines(false);
                // 2) check that the current thread is the UI thread, which will throw if it was
                //    already started using a different thread as the UI thread.
                checkThread();
            } else {
                // For older apps, only the view methods that relate to the view hierarchy must come
                // from a single thread. Other calls, including the constructor itself, can come
                // from any thread, and will be posted to the UI thread if necessary.
                //
                // We used to defer the decision about which thread is the UI thread for as long as
                // possible to allow for the case where an app targeting < JB MR2 used a different
                // thread, but this significantly complicated initialization and is virtually never
                // encountered in the wild. We can't just use the current thread as the UI thread as
                // the normal case does, because it *is* somewhat common for old apps to construct
                // WebView on a background thread and then attach it to the view hierarchy on the
                // main looper.
                //
                // So, we just start Chromium using the main looper as the UI thread, which works
                // for virtually every old app, and accept that a very tiny number of them will
                // break.
                mFactory.startYourEngines(true);
            }

            final boolean isAccessFromFileURLsGrantedByDefault =
                    mAppTargetSdkVersion < Build.VERSION_CODES.JELLY_BEAN;
            final boolean areLegacyQuirksEnabled =
                    mAppTargetSdkVersion < Build.VERSION_CODES.KITKAT;
            final boolean allowEmptyDocumentPersistence =
                    mAppTargetSdkVersion <= Build.VERSION_CODES.M;
            final boolean allowGeolocationOnInsecureOrigins =
                    mAppTargetSdkVersion <= Build.VERSION_CODES.M;

            // https://crbug.com/698752
            final boolean doNotUpdateSelectionOnMutatingSelectionRange =
                    mAppTargetSdkVersion <= Build.VERSION_CODES.M;

            mContentsClientAdapter =
                    mFactory.createWebViewContentsClientAdapter(mWebView, mContext);
            try (ScopedSysTraceEvent e2 =
                    ScopedSysTraceEvent.scoped("WebViewChromium.ContentSettingsAdapter")) {
                mWebSettings =
                        mFactory.createContentSettingsAdapter(
                                new AwSettings(
                                        mContext,
                                        isAccessFromFileURLsGrantedByDefault,
                                        areLegacyQuirksEnabled,
                                        allowEmptyDocumentPersistence,
                                        allowGeolocationOnInsecureOrigins,
                                        doNotUpdateSelectionOnMutatingSelectionRange));
            }

            if (mAppTargetSdkVersion < Build.VERSION_CODES.LOLLIPOP) {
                // Prior to Lollipop we always allowed third party cookies and mixed content.
                mWebSettings.setMixedContentMode(WebSettings.MIXED_CONTENT_ALWAYS_ALLOW);
                mWebSettings.setAcceptThirdPartyCookies(true);
                mWebSettings.getAwSettings().setZeroLayoutHeightDisablesViewportQuirk(true);
            }

            if (mAppTargetSdkVersion >= Build.VERSION_CODES.P) {
                mWebSettings.getAwSettings().setCSSHexAlphaColorEnabled(true);
                mWebSettings.getAwSettings().setScrollTopLeftInteropEnabled(true);
            }

            if (mShouldDisableThreadChecking) disableThreadChecking();

            mSharedWebViewChromium.init(mContentsClientAdapter);

            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            initForReal();
                            if (privateBrowsing) {
                                // Intentionally irreversibly disable the webview instance, so that
                                // private user data cannot leak through misuse of a
                                // non-private-browsing WebView instance. Can't just null out
                                // mAwContents as we never null-check
                                // it before use.
                                destroy();
                            }
                        }
                    });
        }

        // If initialization hasn't been deferred, record a startup time histogram entry
        // and trace event(s).
        if (mFactory.hasStarted()) {
            if (isFirstWebViewInit) {
                RecordHistogram.recordTimesHistogram(
                        "Android.WebView.Startup.CreationTime.Stage2.ProviderInit.Cold",
                        SystemClock.uptimeMillis() - startTime);

                TraceEvent.webViewStartupTotalFactoryInit(
                        mFactory.getInitInfo().mTotalFactoryInitStartTime,
                        mFactory.getInitInfo().mTotalFactoryInitDuration);

                TraceEvent.webViewStartupStage1(
                        mFactory.getInitInfo().mStartTime, mFactory.getInitInfo().mDuration);

                TraceEvent.webViewStartupStage2(
                        startTime, SystemClock.uptimeMillis() - startTime, true);
            } else {
                RecordHistogram.recordTimesHistogram(
                        "Android.WebView.Startup.CreationTime.Stage2.ProviderInit.Warm",
                        SystemClock.uptimeMillis() - startTime);

                TraceEvent.webViewStartupStage2(
                        startTime, SystemClock.uptimeMillis() - startTime, false);
            }
        }
    }

    // This is a workaround for https://crbug.com/622151.
    // In HTC's email app, InputConnection.setComposingText() will call WebView.evaluateJavaScript,
    // and thread assertion will occur. We turn off WebView thread assertion for this app.
    // SuppressLint rationale: https://crrev.com/c/3439168/comments/d612ae09_b7bfca2b
    @SuppressLint("SoonBlockedPrivateApi")
    private void disableThreadChecking() {
        try {
            Class<?> webViewClass = Class.forName("android.webkit.WebView");
            Field field = webViewClass.getDeclaredField("sEnforceThreadChecking");
            field.setAccessible(true);
            field.setBoolean(null, false);
            field.setAccessible(false);
        } catch (ClassNotFoundException
                | NoSuchFieldException
                | IllegalAccessException
                | IllegalArgumentException e) {
            Log.w(TAG, "Failed to disable thread checking.");
        }
    }

    private void initForReal() {
        try (ScopedSysTraceEvent e1 = ScopedSysTraceEvent.scoped("WebViewChromium.initForReal")) {
            AwContentsStatics.setRecordFullDocument(
                    sRecordWholeDocumentEnabledByApi
                            || mAppTargetSdkVersion < Build.VERSION_CODES.LOLLIPOP);

            AwBrowserContext browserContext = null;
            // Temporary workaround for setting the profile at WebView startup.
            Integer appProfileNameTagKey =
                    ManifestMetadataUtil.getAppMultiProfileProfileNameTagKey();
            if (appProfileNameTagKey != null
                    && mWebView.getTag(appProfileNameTagKey) instanceof String profileName) {
                browserContext = AwBrowserContextStore.getNamedContext(profileName, true);
            }

            if (browserContext == null) {
                browserContext = mFactory.getDefaultBrowserContextOnUiThread();
            }

            mAwContents =
                    new AwContents(
                            browserContext,
                            mWebView,
                            mContext,
                            new InternalAccessAdapter(),
                            new WebViewNativeDrawFunctorFactory(),
                            mContentsClientAdapter,
                            mWebSettings.getAwSettings(),
                            new AwContents.DependencyFactory());
            if (mAppTargetSdkVersion >= Build.VERSION_CODES.KITKAT) {
                // On KK and above, favicons are automatically downloaded as the method
                // old apps use to enable that behavior is deprecated.
                AwContents.setShouldDownloadFavicons();
            }

            if (mAppTargetSdkVersion < Build.VERSION_CODES.LOLLIPOP) {
                // Prior to Lollipop, JavaScript objects injected via addJavascriptInterface
                // were not inspectable.
                mAwContents.disableJavascriptInterfacesInspection();
            }

            // TODO: This assumes AwContents ignores second Paint param.
            mAwContents.setLayerType(mWebView.getLayerType(), null);

            mSharedWebViewChromium.initForReal(mAwContents);
        }
    }

    private RuntimeException createThreadException() {
        return new IllegalStateException(
                "Calling View methods on another thread than the UI thread.");
    }

    protected boolean checkNeedsPost() {
        return mSharedWebViewChromium.checkNeedsPost();
    }

    //  Intentionally not static, as no need to check thread on static methods
    private void checkThread() {
        if (!ThreadUtils.runningOnUiThread()) {
            final RuntimeException threadViolation = createThreadException();
            AwThreadUtils.postToUiThreadLooper(
                    () -> {
                        throw threadViolation;
                    });
            throw createThreadException();
        }
    }

    @Override
    public void setHorizontalScrollbarOverlay(final boolean overlay) {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            setHorizontalScrollbarOverlay(overlay);
                        }
                    });
            return;
        }
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.SET_HORIZONTAL_SCROLLBAR_OVERLAY")) {
            recordWebViewApiCall(ApiCall.SET_HORIZONTAL_SCROLLBAR_OVERLAY);
            mAwContents.setHorizontalScrollbarOverlay(overlay);
        }
    }

    @Override
    public void setVerticalScrollbarOverlay(final boolean overlay) {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            setVerticalScrollbarOverlay(overlay);
                        }
                    });
            return;
        }
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.SET_VERTICAL_SCROLLBAR_OVERLAY")) {
            recordWebViewApiCall(ApiCall.SET_VERTICAL_SCROLLBAR_OVERLAY);
            mAwContents.setVerticalScrollbarOverlay(overlay);
        }
    }

    @Override
    public boolean overlayHorizontalScrollbar() {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            boolean ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<Boolean>() {
                                @Override
                                public Boolean call() {
                                    return overlayHorizontalScrollbar();
                                }
                            });
            return ret;
        }
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.OVERLAY_HORIZONTAL_SCROLLBAR")) {
            recordWebViewApiCall(ApiCall.OVERLAY_HORIZONTAL_SCROLLBAR);
            return mAwContents.overlayHorizontalScrollbar();
        }
    }

    @Override
    public boolean overlayVerticalScrollbar() {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            boolean ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<Boolean>() {
                                @Override
                                public Boolean call() {
                                    return overlayVerticalScrollbar();
                                }
                            });
            return ret;
        }
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.OVERLAY_VERTICAL_SCROLLBAR")) {
            recordWebViewApiCall(ApiCall.OVERLAY_VERTICAL_SCROLLBAR);
            return mAwContents.overlayVerticalScrollbar();
        }
    }

    @Override
    public int getVisibleTitleHeight() {
        // This is deprecated in WebView and should always return 0.
        return 0;
    }

    @Override
    public SslCertificate getCertificate() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            SslCertificate ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<SslCertificate>() {
                                @Override
                                public SslCertificate call() {
                                    return getCertificate();
                                }
                            });
            return ret;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.GET_CERTIFICATE")) {
            recordWebViewApiCall(ApiCall.GET_CERTIFICATE);
            return mAwContents.getCertificate();
        }
    }

    @Override
    public void setCertificate(SslCertificate certificate) {
        // intentional no-op
    }

    @Override
    public void savePassword(String host, String username, String password) {
        // This is a deprecated API: intentional no-op.
    }

    @Override
    public void setHttpAuthUsernamePassword(
            final String host, final String realm, final String username, final String password) {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            setHttpAuthUsernamePassword(host, realm, username, password);
                        }
                    });
            return;
        }
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.SET_HTTP_AUTH_USERNAME_PASSWORD")) {
            recordWebViewApiCall(ApiCall.SET_HTTP_AUTH_USERNAME_PASSWORD);
            ((WebViewDatabaseAdapter) mFactory.getWebViewDatabase(mContext))
                    .setHttpAuthUsernamePassword(host, realm, username, password);
        }
    }

    @Override
    public String[] getHttpAuthUsernamePassword(final String host, final String realm) {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            String[] ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<String[]>() {
                                @Override
                                public String[] call() {
                                    return getHttpAuthUsernamePassword(host, realm);
                                }
                            });
            return ret;
        }
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.GET_HTTP_AUTH_USERNAME_PASSWORD")) {
            recordWebViewApiCall(ApiCall.GET_HTTP_AUTH_USERNAME_PASSWORD);
            return ((WebViewDatabaseAdapter) mFactory.getWebViewDatabase(mContext))
                    .getHttpAuthUsernamePassword(host, realm);
        }
    }

    @Override
    public void destroy() {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            destroy();
                        }
                    });
            return;
        }

        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.DESTROY")) {
            recordWebViewApiCall(ApiCall.DESTROY);

            // Make sure that we do not trigger any callbacks after destruction
            setWebChromeClient(null);
            setWebViewClient(null);
            mContentsClientAdapter.setPictureListener(null, true);
            mContentsClientAdapter.setFindListener(null);
            mContentsClientAdapter.setDownloadListener(null);

            mAwContents.destroy();
        }
    }

    @Override
    public void setNetworkAvailable(final boolean networkUp) {
        // Note that this purely toggles the JS navigator.online property.
        // It does not in affect chromium or network stack state in any way.
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            setNetworkAvailable(networkUp);
                        }
                    });
            return;
        }
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.SET_NETWORK_AVAILABLE")) {
            recordWebViewApiCall(ApiCall.SET_NETWORK_AVAILABLE);
            mAwContents.setNetworkAvailable(networkUp);
        }
    }

    @Override
    public WebBackForwardList saveState(final Bundle outState) {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            WebBackForwardList ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<WebBackForwardList>() {
                                @Override
                                public WebBackForwardList call() {
                                    return saveState(outState);
                                }
                            });
            return ret;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.SAVE_STATE")) {
            recordWebViewApiCall(ApiCall.SAVE_STATE);
            if (outState == null) return null;
            if (!mAwContents.saveState(outState)) return null;
            return copyBackForwardList();
        }
    }

    @Override
    public boolean savePicture(Bundle b, File dest) {
        // Intentional no-op: hidden method on WebView.
        return false;
    }

    @Override
    public boolean restorePicture(Bundle b, File src) {
        // Intentional no-op: hidden method on WebView.
        return false;
    }

    @Override
    public WebBackForwardList restoreState(final Bundle inState) {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            WebBackForwardList ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<WebBackForwardList>() {
                                @Override
                                public WebBackForwardList call() {
                                    return restoreState(inState);
                                }
                            });
            return ret;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.RESTORE_STATE")) {
            recordWebViewApiCall(ApiCall.RESTORE_STATE);
            if (inState == null) return null;
            if (!mAwContents.restoreState(inState)) return null;
            return copyBackForwardList();
        }
    }

    @Override
    public void loadUrl(final String url, final Map<String, String> additionalHttpHeaders) {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            // Disallowed in WebView API for apps targeting a new SDK
            assert mAppTargetSdkVersion < Build.VERSION_CODES.JELLY_BEAN_MR2;
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            loadUrlNoPost(url, additionalHttpHeaders);
                        }
                    });
            return;
        }
        loadUrlNoPost(url, additionalHttpHeaders);
    }

    private void loadUrlNoPost(final String url, final Map<String, String> additionalHttpHeaders) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.LOAD_URL_ADDITIONAL_HEADERS")) {
            recordWebViewApiCall(ApiCall.LOAD_URL_ADDITIONAL_HEADERS);
            long startTime = SystemClock.uptimeMillis();
            mAwContents.loadUrl(url, additionalHttpHeaders);
            RecordHistogram.recordTimesHistogram(
                    "Android.WebView.ApiCall.Duration.Framework.LOAD_URL_ADDITIONAL_HEADERS",
                    SystemClock.uptimeMillis() - startTime);
        }
    }

    @Override
    public void loadUrl(final String url) {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            // Disallowed in WebView API for apps targeting a new SDK
            assert mAppTargetSdkVersion < Build.VERSION_CODES.JELLY_BEAN_MR2;
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            loadUrlNoPost(url);
                        }
                    });
            return;
        }
        loadUrlNoPost(url);
    }

    private void loadUrlNoPost(final String url) {
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.LOAD_URL")) {
            recordWebViewApiCall(ApiCall.LOAD_URL);
            long startTime = SystemClock.uptimeMillis();
            mAwContents.loadUrl(url);
            RecordHistogram.recordTimesHistogram(
                    "Android.WebView.ApiCall.Duration.Framework.LOAD_URL",
                    SystemClock.uptimeMillis() - startTime);
        }
    }

    @Override
    public void postUrl(final String url, final byte[] postData) {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            // Disallowed in WebView API for apps targeting a new SDK
            assert mAppTargetSdkVersion < Build.VERSION_CODES.JELLY_BEAN_MR2;
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            try (TraceEvent event =
                                    TraceEvent.scoped("WebView.APICall.Framework.POST_URL")) {
                                recordWebViewApiCall(ApiCall.POST_URL);
                                mAwContents.postUrl(url, postData);
                            }
                        }
                    });
            return;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.POST_URL")) {
            recordWebViewApiCall(ApiCall.POST_URL);
            mAwContents.postUrl(url, postData);
        }
    }

    @Override
    public void loadData(final String data, final String mimeType, final String encoding) {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            // Disallowed in WebView API for apps targeting a new SDK
            assert mAppTargetSdkVersion < Build.VERSION_CODES.JELLY_BEAN_MR2;
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            try (TraceEvent event =
                                    TraceEvent.scoped("WebView.APICall.Framework.LOAD_DATA")) {
                                recordWebViewApiCall(ApiCall.LOAD_DATA);
                                mAwContents.loadData(data, mimeType, encoding);
                            }
                        }
                    });
            return;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.LOAD_DATA")) {
            recordWebViewApiCall(ApiCall.LOAD_DATA);
            mAwContents.loadData(data, mimeType, encoding);
        }
    }

    @Override
    public void loadDataWithBaseURL(
            final String baseUrl,
            final String data,
            final String mimeType,
            final String encoding,
            final String historyUrl) {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            // Disallowed in WebView API for apps targeting a new SDK
            assert mAppTargetSdkVersion < Build.VERSION_CODES.JELLY_BEAN_MR2;
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            loadDataWithBaseURLNoPost(
                                    baseUrl, data, mimeType, encoding, historyUrl);
                        }
                    });
            return;
        }
        loadDataWithBaseURLNoPost(baseUrl, data, mimeType, encoding, historyUrl);
    }

    private void loadDataWithBaseURLNoPost(
            final String baseUrl,
            final String data,
            final String mimeType,
            final String encoding,
            final String historyUrl) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.LOAD_DATA_WITH_BASE_URL")) {
            recordWebViewApiCall(ApiCall.LOAD_DATA_WITH_BASE_URL);
            long startTime = SystemClock.uptimeMillis();
            mAwContents.loadDataWithBaseURL(baseUrl, data, mimeType, encoding, historyUrl);
            RecordHistogram.recordTimesHistogram(
                    "Android.WebView.ApiCall.Duration.Framework.LOAD_DATA_WITH_BASE_URL",
                    SystemClock.uptimeMillis() - startTime);
        }
    }

    @Override
    public void evaluateJavaScript(
            final String script, final ValueCallback<String> resultCallback) {
        if (mShouldDisableThreadChecking && checkNeedsPost()) {
            // This is a workaround for https://crbug.com/622151.
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            try (TraceEvent event =
                                    TraceEvent.scoped(
                                            "WebView.APICall.Framework.EVALUATE_JAVASCRIPT")) {
                                recordWebViewApiCall(ApiCall.EVALUATE_JAVASCRIPT);
                                mAwContents.evaluateJavaScript(
                                        script,
                                        CallbackConverter.fromValueCallback(resultCallback));
                            }
                        }
                    });
        } else {
            try (TraceEvent event =
                    TraceEvent.scoped("WebView.APICall.Framework.EVALUATE_JAVASCRIPT")) {
                recordWebViewApiCall(ApiCall.EVALUATE_JAVASCRIPT);
                checkThread();
                mAwContents.evaluateJavaScript(
                        script, CallbackConverter.fromValueCallback(resultCallback));
            }
        }
    }

    @Override
    public void saveWebArchive(String filename) {
        saveWebArchive(filename, false, null);
    }

    @Override
    public void saveWebArchive(
            final String basename, final boolean autoname, final ValueCallback<String> callback) {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            saveWebArchive(basename, autoname, callback);
                        }
                    });
            return;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.SAVE_WEB_ARCHIVE")) {
            recordWebViewApiCall(ApiCall.SAVE_WEB_ARCHIVE);
            mAwContents.saveWebArchive(
                    basename, autoname, CallbackConverter.fromValueCallback(callback));
        }
    }

    @Override
    public void stopLoading() {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            stopLoading();
                        }
                    });
            return;
        }

        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.STOP_LOADING")) {
            recordWebViewApiCall(ApiCall.STOP_LOADING);
            mAwContents.stopLoading();
        }
    }

    @Override
    public void reload() {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            reload();
                        }
                    });
            return;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.RELOAD")) {
            recordWebViewApiCall(ApiCall.RELOAD);
            mAwContents.reload();
        }
    }

    @Override
    public boolean canGoBack() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            Boolean ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<Boolean>() {
                                @Override
                                public Boolean call() {
                                    return canGoBack();
                                }
                            });
            return ret;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.CAN_GO_BACK")) {
            recordWebViewApiCall(ApiCall.CAN_GO_BACK);
            return mAwContents.canGoBack();
        }
    }

    @Override
    public void goBack() {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            goBack();
                        }
                    });
            return;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.GO_BACK")) {
            recordWebViewApiCall(ApiCall.GO_BACK);
            mAwContents.goBack();
        }
    }

    @Override
    public boolean canGoForward() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            Boolean ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<Boolean>() {
                                @Override
                                public Boolean call() {
                                    return canGoForward();
                                }
                            });
            return ret;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.CAN_GO_FORWARD")) {
            recordWebViewApiCall(ApiCall.CAN_GO_FORWARD);
            return mAwContents.canGoForward();
        }
    }

    @Override
    public void goForward() {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            goForward();
                        }
                    });
            return;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.GO_FORWARD")) {
            recordWebViewApiCall(ApiCall.GO_FORWARD);
            mAwContents.goForward();
        }
    }

    @Override
    public boolean canGoBackOrForward(final int steps) {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            Boolean ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<Boolean>() {
                                @Override
                                public Boolean call() {
                                    return canGoBackOrForward(steps);
                                }
                            });
            return ret;
        }
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.CAN_GO_BACK_OR_FORWARD")) {
            recordWebViewApiCall(ApiCall.CAN_GO_BACK_OR_FORWARD);
            return mAwContents.canGoBackOrForward(steps);
        }
    }

    @Override
    public void goBackOrForward(final int steps) {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            goBackOrForward(steps);
                        }
                    });
            return;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.GO_BACK_OR_FORWARD")) {
            recordWebViewApiCall(ApiCall.GO_BACK_OR_FORWARD);
            mAwContents.goBackOrForward(steps);
        }
    }

    @Override
    public boolean isPrivateBrowsingEnabled() {
        // Not supported in this WebView implementation.
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.IS_PRIVATE_BROWSING_ENABLED")) {
            recordWebViewApiCall(ApiCall.IS_PRIVATE_BROWSING_ENABLED);
            return false;
        }
    }

    @Override
    public boolean pageUp(final boolean top) {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            Boolean ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<Boolean>() {
                                @Override
                                public Boolean call() {
                                    return pageUp(top);
                                }
                            });
            return ret;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.PAGE_UP")) {
            recordWebViewApiCall(ApiCall.PAGE_UP);
            return mAwContents.pageUp(top);
        }
    }

    @Override
    public boolean pageDown(final boolean bottom) {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            Boolean ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<Boolean>() {
                                @Override
                                public Boolean call() {
                                    return pageDown(bottom);
                                }
                            });
            return ret;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.PAGE_DOWN")) {
            recordWebViewApiCall(ApiCall.PAGE_DOWN);
            return mAwContents.pageDown(bottom);
        }
    }

    @Override
    public void insertVisualStateCallback(
            final long requestId, final VisualStateCallback callback) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.INSERT_VISUAL_STATE_CALLBACK")) {
            recordWebViewApiCall(ApiCall.INSERT_VISUAL_STATE_CALLBACK);
            mSharedWebViewChromium.insertVisualStateCallback(
                    requestId,
                    callback == null
                            ? null
                            : new AwContents.VisualStateCallback() {
                                @Override
                                public void onComplete(long requestId) {
                                    callback.onComplete(requestId);
                                }
                            });
        }
    }

    @Override
    public void clearView() {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            clearView();
                        }
                    });
            return;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.CLEAR_VIEW")) {
            recordWebViewApiCall(ApiCall.CLEAR_VIEW);
            mAwContents.clearView();
        }
    }

    @Override
    public Picture capturePicture() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            Picture ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<Picture>() {
                                @Override
                                public Picture call() {
                                    return capturePicture();
                                }
                            });
            return ret;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.CAPTURE_PICTURE")) {
            recordWebViewApiCall(ApiCall.CAPTURE_PICTURE);
            return mAwContents.capturePicture();
        }
    }

    @Override
    public float getScale() {
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.GET_SCALE")) {
            recordWebViewApiCall(ApiCall.GET_SCALE);
            // No checkThread() as it is mostly thread safe (workaround for b/10652991).
            mFactory.startYourEngines(true);
            return mAwContents.getScale();
        }
    }

    @Override
    public void setInitialScale(final int scaleInPercent) {
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.SET_INITIAL_SCALE")) {
            recordWebViewApiCall(ApiCall.SET_INITIAL_SCALE);
            // No checkThread() as it is thread safe
            mWebSettings.getAwSettings().setInitialPageScale(scaleInPercent);
        }
    }

    @Override
    public void invokeZoomPicker() {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            invokeZoomPicker();
                        }
                    });
            return;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.INVOKE_ZOOM_PICKER")) {
            recordWebViewApiCall(ApiCall.INVOKE_ZOOM_PICKER);
            mAwContents.invokeZoomPicker();
        }
    }

    @Override
    public WebView.HitTestResult getHitTestResult() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            WebView.HitTestResult ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<WebView.HitTestResult>() {
                                @Override
                                public WebView.HitTestResult call() {
                                    return getHitTestResult();
                                }
                            });
            return ret;
        }
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.GET_HIT_TEST_RESULT")) {
            recordWebViewApiCall(ApiCall.GET_HIT_TEST_RESULT);
            AwContents.HitTestData data = mAwContents.getLastHitTestResult();
            mHitTestResult.setType(data.hitTestResultType);
            mHitTestResult.setExtra(data.hitTestResultExtraData);
            return mHitTestResult;
        }
    }

    @Override
    public void requestFocusNodeHref(final Message hrefMsg) {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            requestFocusNodeHref(hrefMsg);
                        }
                    });
            return;
        }
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.REQUEST_FOCUS_NODE_HREF")) {
            recordWebViewApiCall(ApiCall.REQUEST_FOCUS_NODE_HREF);
            mAwContents.requestFocusNodeHref(hrefMsg);
        }
    }

    @Override
    public void requestImageRef(final Message msg) {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            requestImageRef(msg);
                        }
                    });
            return;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.REQUEST_IMAGE_REF")) {
            recordWebViewApiCall(ApiCall.REQUEST_IMAGE_REF);
            mAwContents.requestImageRef(msg);
        }
    }

    @Override
    public String getUrl() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            String ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<String>() {
                                @Override
                                public String call() {
                                    return getUrl();
                                }
                            });
            return ret;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.GET_URL")) {
            recordWebViewApiCall(ApiCall.GET_URL);
            GURL url = mAwContents.getUrl();
            return url == null ? null : url.getSpec();
        }
    }

    @Override
    public String getOriginalUrl() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            String ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<String>() {
                                @Override
                                public String call() {
                                    return getOriginalUrl();
                                }
                            });
            return ret;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.GET_ORIGINAL_URL")) {
            recordWebViewApiCall(ApiCall.GET_ORIGINAL_URL);
            return mAwContents.getOriginalUrl();
        }
    }

    @Override
    public String getTitle() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            String ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<String>() {
                                @Override
                                public String call() {
                                    return getTitle();
                                }
                            });
            return ret;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.GET_TITLE")) {
            recordWebViewApiCall(ApiCall.GET_TITLE);
            return mAwContents.getTitle();
        }
    }

    @Override
    public Bitmap getFavicon() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            Bitmap ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<Bitmap>() {
                                @Override
                                public Bitmap call() {
                                    return getFavicon();
                                }
                            });
            return ret;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.GET_FAVICON")) {
            recordWebViewApiCall(ApiCall.GET_FAVICON);
            return mAwContents.getFavicon();
        }
    }

    @Override
    public String getTouchIconUrl() {
        // Intentional no-op: hidden method on WebView.
        return null;
    }

    @Override
    public int getProgress() {
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.GET_PROGRESS")) {
            recordWebViewApiCall(ApiCall.GET_PROGRESS);
            if (mAwContents == null) return 100;
            // No checkThread() because the value is cached java side (workaround for b/10533304).
            return mAwContents.getMostRecentProgress();
        }
    }

    @Override
    public int getContentHeight() {
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.GET_CONTENT_HEIGHT")) {
            recordWebViewApiCall(ApiCall.GET_CONTENT_HEIGHT);
            if (mAwContents == null) return 0;
            // No checkThread() as it is mostly thread safe (workaround for b/10594869).
            return mAwContents.getContentHeightCss();
        }
    }

    @Override
    public int getContentWidth() {
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.GET_CONTENT_WIDTH")) {
            recordWebViewApiCall(ApiCall.GET_CONTENT_WIDTH);
            if (mAwContents == null) return 0;
            // No checkThread() as it is mostly thread safe (workaround for b/10594869).
            return mAwContents.getContentWidthCss();
        }
    }

    @Override
    public void pauseTimers() {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            pauseTimers();
                        }
                    });
            return;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.PAUSE_TIMERS")) {
            recordWebViewApiCall(ApiCall.PAUSE_TIMERS);
            mAwContents.pauseTimers();
        }
    }

    @Override
    public void resumeTimers() {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            resumeTimers();
                        }
                    });
            return;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.RESUME_TIMERS")) {
            recordWebViewApiCall(ApiCall.RESUME_TIMERS);
            mAwContents.resumeTimers();
        }
    }

    @Override
    public void onPause() {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            onPause();
                        }
                    });
            return;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.ON_PAUSE")) {
            recordWebViewApiCall(ApiCall.ON_PAUSE);
            mAwContents.onPause();
        }
    }

    @Override
    public void onResume() {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            onResume();
                        }
                    });
            return;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.ON_RESUME")) {
            recordWebViewApiCall(ApiCall.ON_RESUME);
            mAwContents.onResume();
        }
    }

    @Override
    public boolean isPaused() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            Boolean ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<Boolean>() {
                                @Override
                                public Boolean call() {
                                    return isPaused();
                                }
                            });
            return ret;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.IS_PAUSED")) {
            recordWebViewApiCall(ApiCall.IS_PAUSED);
            return mAwContents.isPaused();
        }
    }

    @Override
    public void freeMemory() {
        // Intentional no-op. Memory is managed automatically by Chromium.
    }

    @Override
    public void clearCache(final boolean includeDiskFiles) {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            clearCache(includeDiskFiles);
                        }
                    });
            return;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.CLEAR_CACHE")) {
            recordWebViewApiCall(ApiCall.CLEAR_CACHE);
            mAwContents.clearCache(includeDiskFiles);
        }
    }

    /** This is a poorly named method, but we keep it for historical reasons. */
    @Override
    public void clearFormData() {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            clearFormData();
                        }
                    });
            return;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.CLEAR_FORM_DATA")) {
            recordWebViewApiCall(ApiCall.CLEAR_FORM_DATA);
            mAwContents.hideAutofillPopup();
        }
    }

    @Override
    public void clearHistory() {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            clearHistory();
                        }
                    });
            return;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.CLEAR_HISTORY")) {
            recordWebViewApiCall(ApiCall.CLEAR_HISTORY);
            mAwContents.clearHistory();
        }
    }

    @Override
    public void clearSslPreferences() {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            clearSslPreferences();
                        }
                    });
            return;
        }
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.CLEAR_SSL_PREFERENCES")) {
            recordWebViewApiCall(ApiCall.CLEAR_SSL_PREFERENCES);
            mAwContents.clearSslPreferences();
        }
    }

    @Override
    public WebBackForwardList copyBackForwardList() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            WebBackForwardList ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<WebBackForwardList>() {
                                @Override
                                public WebBackForwardList call() {
                                    return copyBackForwardList();
                                }
                            });
            return ret;
        }
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.COPY_BACK_FORWARD_LIST")) {
            recordWebViewApiCall(ApiCall.COPY_BACK_FORWARD_LIST);
            // mAwContents.getNavigationHistory() can be null here if mAwContents has been
            // destroyed, and we do not handle passing null to the WebBackForwardListChromium
            // constructor.
            NavigationHistory navHistory = mAwContents.getNavigationHistory();
            if (navHistory == null) navHistory = new NavigationHistory();
            return new WebBackForwardListChromium(navHistory);
        }
    }

    @Override
    public void setFindListener(WebView.FindListener listener) {
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.SET_FIND_LISTENER")) {
            recordWebViewApiCall(ApiCall.SET_FIND_LISTENER);
            mContentsClientAdapter.setFindListener(listener);
        }
    }

    @Override
    public void findNext(final boolean forwards) {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            findNext(forwards);
                        }
                    });
            return;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.FIND_NEXT")) {
            recordWebViewApiCall(ApiCall.FIND_NEXT);
            mAwContents.findNext(forwards);
        }
    }

    @Override
    public int findAll(final String searchString) {
        findAllAsync(searchString);
        return 0;
    }

    @Override
    public void findAllAsync(final String searchString) {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            findAllAsync(searchString);
                        }
                    });
            return;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.FIND_ALL_ASYNC")) {
            recordWebViewApiCall(ApiCall.FIND_ALL_ASYNC);
            mAwContents.findAllAsync(searchString);
        }
    }

    @Override
    public boolean showFindDialog(final String text, final boolean showIme) {
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.SHOW_FIND_DIALOG")) {
            recordWebViewApiCall(ApiCall.SHOW_FIND_DIALOG);
            mFactory.startYourEngines(false);
            if (checkNeedsPost()) {
                return false;
            }
            if (mWebView.getParent() == null) {
                return false;
            }

            FindActionModeCallback findAction = new FindActionModeCallback(mContext);
            if (findAction == null) {
                return false;
            }

            mWebView.startActionMode(findAction);
            findAction.setWebView(mWebView);
            if (showIme) {
                findAction.showSoftInput();
            }

            if (text != null) {
                findAction.setText(text);
                findAction.findAll();
            }

            return true;
        }
    }

    @Override
    public void notifyFindDialogDismissed() {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            notifyFindDialogDismissed();
                        }
                    });
            return;
        }
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.NOTIFY_FIND_DIALOG_DISMISSED")) {
            recordWebViewApiCall(ApiCall.NOTIFY_FIND_DIALOG_DISMISSED);
            clearMatches();
        }
    }

    @Override
    public void clearMatches() {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            clearMatches();
                        }
                    });
            return;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.CLEAR_MATCHES")) {
            recordWebViewApiCall(ApiCall.CLEAR_MATCHES);
            mAwContents.clearMatches();
        }
    }

    @Override
    public void documentHasImages(final Message response) {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            documentHasImages(response);
                        }
                    });
            return;
        }
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.DOCUMENT_HAS_IMAGES")) {
            recordWebViewApiCall(ApiCall.DOCUMENT_HAS_IMAGES);
            mAwContents.documentHasImages(response);
        }
    }

    @Override
    public void setWebViewClient(WebViewClient client) {
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.SET_WEBVIEW_CLIENT")) {
            recordWebViewApiCall(ApiCall.SET_WEBVIEW_CLIENT);
            mAwContents.cancelAllPrerendering();
            mSharedWebViewChromium.setWebViewClient(client);
            mContentsClientAdapter.setWebViewClient(mSharedWebViewChromium.getWebViewClient());
            if (client != null) {
                ApiImplementationLogger.logWebViewClientImplementation(client);
            }
        }
    }

    @Override
    public WebViewClient getWebViewClient() {
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.GET_WEBVIEW_CLIENT")) {
            recordWebViewApiCall(ApiCall.GET_WEBVIEW_CLIENT);
            return mSharedWebViewChromium.getWebViewClient();
        }
    }

    @RequiresApi(Build.VERSION_CODES.Q)
    @Override
    public WebViewRenderProcess getWebViewRenderProcess() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.GET_WEBVIEW_RENDER_PROCESS")) {
            recordWebViewApiCall(ApiCall.GET_WEBVIEW_RENDER_PROCESS);
            return GlueApiHelperForQ.getWebViewRenderProcess(
                    mSharedWebViewChromium.getRenderProcess());
        }
    }

    @RequiresApi(Build.VERSION_CODES.Q)
    @Override
    public void setWebViewRenderProcessClient(
            Executor executor, WebViewRenderProcessClient webViewRenderProcessClient) {
        if (webViewRenderProcessClient == null) {
            mSharedWebViewChromium.setWebViewRendererClientAdapter(null);
        } else {
            if (executor == null) {
                executor = (Runnable r) -> r.run();
            }
            try (TraceEvent event =
                    TraceEvent.scoped(
                            "WebView.APICall.Framework.SET_WEBVIEW_RENDER_PROCESS_CLIENT")) {
                recordWebViewApiCall(ApiCall.SET_WEBVIEW_RENDER_PROCESS_CLIENT);
                GlueApiHelperForQ.setWebViewRenderProcessClient(
                        mSharedWebViewChromium, executor, webViewRenderProcessClient);
            }
        }
    }

    @RequiresApi(Build.VERSION_CODES.Q)
    @Override
    public WebViewRenderProcessClient getWebViewRenderProcessClient() {
        SharedWebViewRendererClientAdapter adapter =
                mSharedWebViewChromium.getWebViewRendererClientAdapter();
        if (adapter == null || !(adapter instanceof WebViewRenderProcessClientAdapter)) {
            return null;
        }
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.GET_WEBVIEW_RENDER_PROCESS_CLIENT")) {
            recordWebViewApiCall(ApiCall.GET_WEBVIEW_RENDER_PROCESS_CLIENT);
            return GlueApiHelperForQ.getWebViewRenderProcessClient(adapter);
        }
    }

    @Override
    public void setDownloadListener(DownloadListener listener) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.SET_DOWNLOAD_LISTENER")) {
            recordWebViewApiCall(ApiCall.SET_DOWNLOAD_LISTENER);
            mContentsClientAdapter.setDownloadListener(listener);
        }
    }

    @Override
    public void setWebChromeClient(WebChromeClient client) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.SET_WEBCHROME_CLIENT")) {
            recordWebViewApiCall(ApiCall.SET_WEBCHROME_CLIENT);
            mAwContents.cancelAllPrerendering();
            mWebSettings.getAwSettings().setFullscreenSupported(doesSupportFullscreen(client));
            mSharedWebViewChromium.setWebChromeClient(client);
            mContentsClientAdapter.setWebChromeClient(mSharedWebViewChromium.getWebChromeClient());
            if (client != null) {
                ApiImplementationLogger.logWebChromeClientImplementation(client);
            }
        }
    }

    @Override
    public WebChromeClient getWebChromeClient() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.GET_WEBCHROME_CLIENT")) {
            recordWebViewApiCall(ApiCall.GET_WEBCHROME_CLIENT);
            return mSharedWebViewChromium.getWebChromeClient();
        }
    }

    /**
     * Returns true if the supplied {@link WebChromeClient} supports fullscreen.
     *
     * <p>For fullscreen support, implementations of {@link WebChromeClient#onShowCustomView}
     * and {@link WebChromeClient#onHideCustomView()} are required.
     */
    private boolean doesSupportFullscreen(WebChromeClient client) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.DOES_SUPPORT_FULLSCREEN")) {
            recordWebViewApiCall(ApiCall.DOES_SUPPORT_FULLSCREEN);
            if (client == null) {
                return false;
            }
            Class<?> clientClass = client.getClass();
            boolean foundShowMethod = false;
            boolean foundHideMethod = false;
            while (clientClass != WebChromeClient.class && (!foundShowMethod || !foundHideMethod)) {
                if (!foundShowMethod) {
                    try {
                        clientClass.getDeclaredMethod(
                                "onShowCustomView", View.class, CustomViewCallback.class);
                        foundShowMethod = true;
                    } catch (NoSuchMethodException e) {
                        // Intentionally empty.
                    }
                }

                if (!foundHideMethod) {
                    try {
                        clientClass.getDeclaredMethod("onHideCustomView");
                        foundHideMethod = true;
                    } catch (NoSuchMethodException e) {
                        // Intentionally empty.
                    }
                }
                clientClass = clientClass.getSuperclass();
            }
            return foundShowMethod && foundHideMethod;
        }
    }

    @Override
    @SuppressWarnings("deprecation")
    public void setPictureListener(final WebView.PictureListener listener) {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            setPictureListener(listener);
                        }
                    });
            return;
        }
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.SET_PICTURE_LISTENER")) {
            recordWebViewApiCall(ApiCall.SET_PICTURE_LISTENER);
            boolean invalidateOnly = mAppTargetSdkVersion >= Build.VERSION_CODES.JELLY_BEAN_MR2;
            mContentsClientAdapter.setPictureListener(listener, invalidateOnly);
            mAwContents.enableOnNewPicture(listener != null, invalidateOnly);
        }
    }

    @Override
    public void addJavascriptInterface(final Object obj, final String interfaceName) {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            addJavascriptInterface(obj, interfaceName);
                        }
                    });
            return;
        }
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.ADD_JAVASCRIPT_INTERFACE")) {
            recordWebViewApiCall(ApiCall.ADD_JAVASCRIPT_INTERFACE);
            mAwContents.addJavascriptInterface(obj, interfaceName);
        }
    }

    @Override
    public void removeJavascriptInterface(final String interfaceName) {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            removeJavascriptInterface(interfaceName);
                        }
                    });
            return;
        }
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.REMOVE_JAVASCRIPT_INTERFACE")) {
            recordWebViewApiCall(ApiCall.REMOVE_JAVASCRIPT_INTERFACE);
            mAwContents.removeJavascriptInterface(interfaceName);
        }
    }

    @Override
    public WebMessagePort[] createWebMessageChannel() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.CREATE_WEBMESSAGE_CHANNEL")) {
            recordWebViewApiCall(ApiCall.CREATE_WEBMESSAGE_CHANNEL);
            return WebMessagePortAdapter.fromMessagePorts(
                    mSharedWebViewChromium.createWebMessageChannel());
        }
    }

    @Override
    public void postMessageToMainFrame(final WebMessage message, final Uri targetOrigin) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.POST_MESSAGE_TO_MAIN_FRAME")) {
            recordWebViewApiCall(ApiCall.POST_MESSAGE_TO_MAIN_FRAME);
            // Create MessagePayload from AOSP WebMessage, MessagePayload is not directly supported
            // by AOSP.
            mSharedWebViewChromium.postMessageToMainFrame(
                    new MessagePayload(message.getData()),
                    targetOrigin.toString(),
                    WebMessagePortAdapter.toMessagePorts(message.getPorts()));
        }
    }

    @Override
    public WebSettings getSettings() {
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.GET_SETTINGS")) {
            recordWebViewApiCall(ApiCall.GET_SETTINGS);
            return mWebSettings;
        }
    }

    @Override
    public void setMapTrackballToArrowKeys(boolean setMap) {
        // This is a deprecated API: intentional no-op.
    }

    @Override
    public void flingScroll(final int vx, final int vy) {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            flingScroll(vx, vy);
                        }
                    });
            return;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.FLING_SCROLL")) {
            recordWebViewApiCall(ApiCall.FLING_SCROLL);
            mAwContents.flingScroll(vx, vy);
        }
    }

    @Override
    public View getZoomControls() {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            return null;
        }

        // This was deprecated in 2009 and hidden in JB MR1, so just provide the minimum needed
        // to stop very out-dated applications from crashing.
        Log.w(TAG, "WebView doesn't support getZoomControls");
        return mAwContents.getSettings().supportZoom() ? new View(mContext) : null;
    }

    @Override
    public boolean canZoomIn() {
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.CAN_ZOOM_IN")) {
            recordWebViewApiCall(ApiCall.CAN_ZOOM_IN);
            if (checkNeedsPost()) {
                return false;
            }
            return mAwContents.canZoomIn();
        }
    }

    @Override
    public boolean canZoomOut() {
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.CAN_ZOOM_OUT")) {
            recordWebViewApiCall(ApiCall.CAN_ZOOM_OUT);
            if (checkNeedsPost()) {
                return false;
            }
            return mAwContents.canZoomOut();
        }
    }

    @Override
    public boolean zoomIn() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            boolean ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<Boolean>() {
                                @Override
                                public Boolean call() {
                                    return zoomIn();
                                }
                            });
            return ret;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.ZOOM_IN")) {
            recordWebViewApiCall(ApiCall.ZOOM_IN);
            return mAwContents.zoomIn();
        }
    }

    @Override
    public boolean zoomOut() {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            boolean ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<Boolean>() {
                                @Override
                                public Boolean call() {
                                    return zoomOut();
                                }
                            });
            return ret;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.ZOOM_OUT")) {
            recordWebViewApiCall(ApiCall.ZOOM_OUT);
            return mAwContents.zoomOut();
        }
    }

    // TODO(paulmiller) Return void for consistency with AwContents.zoomBy and WebView.zoomBy -
    // tricky because frameworks WebViewProvider.zoomBy must change simultaneously
    @Override
    public boolean zoomBy(float factor) {
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.ZOOM_BY")) {
            recordWebViewApiCall(ApiCall.ZOOM_BY);
            mFactory.startYourEngines(true);
            // This is an L API and therefore we can enforce stricter threading constraints.
            checkThread();
            mAwContents.zoomBy(factor);
            return true;
        }
    }

    @Override
    public void dumpViewHierarchyWithProperties(BufferedWriter out, int level) {
        // Intentional no-op
    }

    @Override
    public View findHierarchyView(String className, int hashCode) {
        // Intentional no-op
        return null;
    }

    @Override
    public void setRendererPriorityPolicy(
            int rendererRequestedPriority, boolean waivedWhenNotVisible) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.SET_RENDERER_PRIORITY_POLICY",
                        rendererRequestedPriority)) {
            recordWebViewApiCall(ApiCall.SET_RENDERER_PRIORITY_POLICY);
            @RendererPriority int awRendererRequestedPriority;
            switch (rendererRequestedPriority) {
                case WebView.RENDERER_PRIORITY_WAIVED:
                    awRendererRequestedPriority = RendererPriority.WAIVED;
                    break;
                case WebView.RENDERER_PRIORITY_BOUND:
                    awRendererRequestedPriority = RendererPriority.LOW;
                    break;
                default:
                case WebView.RENDERER_PRIORITY_IMPORTANT:
                    awRendererRequestedPriority = RendererPriority.HIGH;
                    break;
            }
            mAwContents.setRendererPriorityPolicy(
                    awRendererRequestedPriority, waivedWhenNotVisible);
        }
    }

    @Override
    public int getRendererRequestedPriority() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.GET_RENDERER_REQUESTED_PRIORITY")) {
            recordWebViewApiCall(ApiCall.GET_RENDERER_REQUESTED_PRIORITY);
            @RendererPriority
            final int awRendererRequestedPriority = mAwContents.getRendererRequestedPriority();
            switch (awRendererRequestedPriority) {
                case RendererPriority.WAIVED:
                    return WebView.RENDERER_PRIORITY_WAIVED;
                case RendererPriority.LOW:
                    return WebView.RENDERER_PRIORITY_BOUND;
                default:
                case RendererPriority.HIGH:
                    return WebView.RENDERER_PRIORITY_IMPORTANT;
            }
        }
    }

    @Override
    public boolean getRendererPriorityWaivedWhenNotVisible() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.GET_RENDERER_PRIORITY_WAIVED_WHEN_NOT_VISIBLE")) {
            recordWebViewApiCall(ApiCall.GET_RENDERER_PRIORITY_WAIVED_WHEN_NOT_VISIBLE);
            return mAwContents.getRendererPriorityWaivedWhenNotVisible();
        }
    }

    @Override
    public void setTextClassifier(TextClassifier textClassifier) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.SET_TEXT_CLASSIFIER")) {
            recordWebViewApiCall(ApiCall.SET_TEXT_CLASSIFIER);
            mAwContents.setTextClassifier(textClassifier);
        }
    }

    @Override
    public TextClassifier getTextClassifier() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.GET_TEXT_CLASSIFIER")) {
            recordWebViewApiCall(ApiCall.GET_TEXT_CLASSIFIER);
            return mAwContents.getTextClassifier();
        }
    }

    @Override
    public void autofill(final SparseArray<AutofillValue> values) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            mFactory.runVoidTaskOnUiThreadBlocking(
                    new Runnable() {
                        @Override
                        public void run() {
                            autofill(values);
                        }
                    });
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.AUTOFILL")) {
            recordWebViewApiCall(ApiCall.AUTOFILL);
            mAwContents.autofill(values);
        }
    }

    @Override
    public void onProvideAutofillVirtualStructure(final ViewStructure structure, final int flags) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            mFactory.runVoidTaskOnUiThreadBlocking(
                    new Runnable() {
                        @Override
                        public void run() {
                            onProvideAutofillVirtualStructure(structure, flags);
                        }
                    });
            return;
        }
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.ON_PROVIDE_AUTOFILL_VIRTUAL_STRUCTURE")) {
            recordWebViewApiCall(ApiCall.ON_PROVIDE_AUTOFILL_VIRTUAL_STRUCTURE);
            mAwContents.onProvideAutoFillVirtualStructure(structure, flags);
        }
    }

    @Override
    public void onProvideContentCaptureStructure(ViewStructure structure, int flags) {
        if (ContentCaptureFeatures.isDumpForTestingEnabled()) {
            Log.i("ContentCapture", "onProvideContentCaptureStructure");
        }
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.ON_PROVIDE_CONTENT_CAPTURE_STRUCTURE")) {
            recordWebViewApiCall(ApiCall.ON_PROVIDE_CONTENT_CAPTURE_STRUCTURE);
            mAwContents.setOnscreenContentProvider(
                    new OnscreenContentProvider(
                            ClassLoaderContextWrapperFactory.get(mWebView.getContext()),
                            mWebView,
                            structure,
                            mAwContents.getWebContents()));
        }
    }

    // WebViewProvider glue methods ---------------------------------------------------------------

    @Override
    // This needs to be kept thread safe!
    public WebViewProvider.ViewDelegate getViewDelegate() {
        return this;
    }

    @Override
    // This needs to be kept thread safe!
    public WebViewProvider.ScrollDelegate getScrollDelegate() {
        return this;
    }

    // WebViewProvider.ViewDelegate implementation ------------------------------------------------

    // TODO: remove from WebViewProvider and use default implementation from
    // ViewGroup.
    @Override
    public boolean shouldDelayChildPressedState() {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            boolean ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<Boolean>() {
                                @Override
                                public Boolean call() {
                                    return shouldDelayChildPressedState();
                                }
                            });
            return ret;
        }
        return true;
    }

    @Override
    public AccessibilityNodeProvider getAccessibilityNodeProvider() {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            AccessibilityNodeProvider ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<AccessibilityNodeProvider>() {
                                @Override
                                public AccessibilityNodeProvider call() {
                                    return getAccessibilityNodeProvider();
                                }
                            });
            return ret;
        }
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.GET_ACCESSIBILITY_NODE_PROVIDER")) {
            recordWebViewApiCall(ApiCall.GET_ACCESSIBILITY_NODE_PROVIDER);
            return mAwContents.getAccessibilityNodeProvider();
        }
    }

    @Override
    public void onProvideVirtualStructure(final ViewStructure structure) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            mFactory.runVoidTaskOnUiThreadBlocking(
                    new Runnable() {
                        @Override
                        public void run() {
                            onProvideVirtualStructure(structure);
                        }
                    });
            return;
        }

        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.ON_PROVIDE_VIRTUAL_STRUCTURE")) {
            recordWebViewApiCall(ApiCall.ON_PROVIDE_VIRTUAL_STRUCTURE);
            mAwContents.onProvideVirtualStructure(structure);
        }
    }

    @Override
    public void onInitializeAccessibilityNodeInfo(final AccessibilityNodeInfo info) {
        // Intentional no-op. Chromium accessibility implementation currently does not need this
        // calls.
    }

    @Override
    public void onInitializeAccessibilityEvent(final AccessibilityEvent event) {
        // Intentional no-op. Chromium accessibility implementation currently does not need this
        // calls.
    }

    @Override
    public boolean performAccessibilityAction(final int action, final Bundle arguments) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            boolean ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<Boolean>() {
                                @Override
                                public Boolean call() {
                                    return performAccessibilityAction(action, arguments);
                                }
                            });
            return ret;
        }
        return mWebViewPrivate.super_performAccessibilityAction(action, arguments);
    }

    @Override
    public void setOverScrollMode(final int mode) {
        // This gets called from the android.view.View c'tor that WebView inherits from. This
        // causes the method to be called when mAwContents == null.
        // It's safe to ignore these calls however since AwContents will read the current value of
        // this setting when it's created.
        if (mAwContents == null) return;

        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            setOverScrollMode(mode);
                        }
                    });
            return;
        }
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.SET_OVERSCROLL_MODE")) {
            recordWebViewApiCall(ApiCall.SET_OVERSCROLL_MODE);
            mAwContents.setOverScrollMode(mode);
        }
    }

    @Override
    public void setScrollBarStyle(final int style) {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            setScrollBarStyle(style);
                        }
                    });
            return;
        }
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.SET_SCROLL_BAR_STYLE")) {
            recordWebViewApiCall(ApiCall.SET_SCROLL_BAR_STYLE);
            mAwContents.setScrollBarStyle(style);
        }
    }

    @Override
    public void onDrawVerticalScrollBar(
            final Canvas canvas,
            final Drawable scrollBar,
            final int l,
            final int t,
            final int r,
            final int b) {
        // WebViewClassic was overriding this method to handle rubberband over-scroll. Since
        // WebViewChromium doesn't support that the vanilla implementation of this method can be
        // used.
        mWebViewPrivate.super_onDrawVerticalScrollBar(canvas, scrollBar, l, t, r, b);
    }

    @Override
    public void onOverScrolled(
            final int scrollX, final int scrollY, final boolean clampedX, final boolean clampedY) {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            onOverScrolled(scrollX, scrollY, clampedX, clampedY);
                        }
                    });
            return;
        }
        mAwContents.onContainerViewOverScrolled(scrollX, scrollY, clampedX, clampedY);
    }

    @Override
    public void onWindowVisibilityChanged(final int visibility) {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            onWindowVisibilityChanged(visibility);
                        }
                    });
            return;
        }
        mAwContents.onWindowVisibilityChanged(visibility);
    }

    @Override
    @SuppressLint("DrawAllocation")
    public void onDraw(final Canvas canvas) {
        mFactory.startYourEngines(true);
        if (checkNeedsPost()) {
            mFactory.runVoidTaskOnUiThreadBlocking(
                    new Runnable() {
                        @Override
                        public void run() {
                            onDraw(canvas);
                        }
                    });
            return;
        }
        mAwContents.onDraw(canvas);
    }

    @Override
    public void setLayoutParams(final ViewGroup.LayoutParams layoutParams) {
        // This API is our strongest signal from the View system that this
        // WebView is going to be bound to a View hierarchy and so at this
        // point we must bind Chromium's UI thread to the current thread.
        mFactory.startYourEngines(false);
        checkThread();
        mWebViewPrivate.super_setLayoutParams(layoutParams);
        if (checkNeedsPost()) {
            mFactory.runVoidTaskOnUiThreadBlocking(
                    new Runnable() {
                        @Override
                        public void run() {
                            mAwContents.setLayoutParams(layoutParams);
                        }
                    });
            return;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.SET_LAYOUT_PARAMS")) {
            recordWebViewApiCall(ApiCall.SET_LAYOUT_PARAMS);
            mAwContents.setLayoutParams(layoutParams);
        }
    }

    @Override
    public void onActivityResult(final int requestCode, final int resultCode, final Intent data) {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            onActivityResult(requestCode, resultCode, data);
                        }
                    });
            return;
        }
        mAwContents.onActivityResult(requestCode, resultCode, data);
    }

    @Override
    public boolean performLongClick() {
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.PERFORM_LONG_CLICK")) {
            recordWebViewApiCall(ApiCall.PERFORM_LONG_CLICK);
            // Return false unless the WebView is attached to a View with a parent
            return mWebView.getParent() != null ? mWebViewPrivate.super_performLongClick() : false;
        }
    }

    @Override
    public void onConfigurationChanged(final Configuration newConfig) {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            onConfigurationChanged(newConfig);
                        }
                    });
            return;
        }
        mAwContents.onConfigurationChanged(newConfig);
    }

    @Override
    public boolean onDragEvent(final DragEvent event) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            boolean ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<Boolean>() {
                                @Override
                                public Boolean call() {
                                    return onDragEvent(event);
                                }
                            });
            return ret;
        }
        try (TraceEvent traceEvent = TraceEvent.scoped("WebView.APICall.Framework.ON_DRAG_EVENT")) {
            recordWebViewSystemApiCall(SystemApiCall.ON_DRAG_EVENT);
            return mAwContents.onDragEvent(event);
        }
    }

    @Override
    public InputConnection onCreateInputConnection(final EditorInfo outAttrs) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            return null;
        }
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.ON_CREATE_INPUT_CONNECTION")) {
            recordWebViewSystemApiCall(SystemApiCall.ON_CREATE_INPUT_CONNECTION);
            return mAwContents.onCreateInputConnection(outAttrs);
        }
    }

    @Override
    public boolean onKeyMultiple(final int keyCode, final int repeatCount, final KeyEvent event) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            boolean ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<Boolean>() {
                                @Override
                                public Boolean call() {
                                    return onKeyMultiple(keyCode, repeatCount, event);
                                }
                            });
            return ret;
        }
        try (TraceEvent traceEvent =
                TraceEvent.scoped("WebView.APICall.Framework.ON_KEY_MULTIPLE")) {
            recordWebViewSystemApiCall(SystemApiCall.ON_KEY_MULTIPLE);
            return false;
        }
    }

    @Override
    public boolean onKeyDown(final int keyCode, final KeyEvent event) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            boolean ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<Boolean>() {
                                @Override
                                public Boolean call() {
                                    return onKeyDown(keyCode, event);
                                }
                            });
            return ret;
        }
        try (TraceEvent traceEvent = TraceEvent.scoped("WebView.APICall.Framework.ON_KEY_DOWN")) {
            recordWebViewSystemApiCall(SystemApiCall.ON_KEY_DOWN);
            return false;
        }
    }

    @Override
    public boolean onKeyUp(final int keyCode, final KeyEvent event) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            boolean ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<Boolean>() {
                                @Override
                                public Boolean call() {
                                    return onKeyUp(keyCode, event);
                                }
                            });
            return ret;
        }
        try (TraceEvent traceEvent = TraceEvent.scoped("WebView.APICall.Framework.ON_KEY_UP")) {
            recordWebViewSystemApiCall(SystemApiCall.ON_KEY_UP);
            return mAwContents.onKeyUp(keyCode, event);
        }
    }

    @Override
    public void onAttachedToWindow() {
        // This API is our strongest signal from the View system that this
        // WebView is going to be bound to a View hierarchy and so at this
        // point we must bind Chromium's UI thread to the current thread.
        mFactory.startYourEngines(false);
        checkThread();
        mAwContents.onAttachedToWindow();
    }

    @Override
    public void onDetachedFromWindow() {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            onDetachedFromWindow();
                        }
                    });
            return;
        }

        mAwContents.onDetachedFromWindow();
    }

    @Override
    public void onVisibilityChanged(final View changedView, final int visibility) {
        // The AwContents will find out the container view visibility before the first draw so we
        // can safely ignore onVisibilityChanged callbacks that happen before init().
        if (mAwContents == null) return;

        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            onVisibilityChanged(changedView, visibility);
                        }
                    });
            return;
        }
        mAwContents.onVisibilityChanged(changedView, visibility);
    }

    @Override
    public void onWindowFocusChanged(final boolean hasWindowFocus) {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            onWindowFocusChanged(hasWindowFocus);
                        }
                    });
            return;
        }
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.ON_WINDOW_FOCUS_CHANGED")) {
            recordWebViewSystemApiCall(SystemApiCall.ON_WINDOW_FOCUS_CHANGED);
            mAwContents.onWindowFocusChanged(hasWindowFocus);
        }
    }

    @Override
    public void onFocusChanged(
            final boolean focused, final int direction, final Rect previouslyFocusedRect) {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            onFocusChanged(focused, direction, previouslyFocusedRect);
                        }
                    });
            return;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.ON_FOCUS_CHANGED")) {
            recordWebViewSystemApiCall(SystemApiCall.ON_FOCUS_CHANGED);
            mAwContents.onFocusChanged(focused, direction, previouslyFocusedRect);
        }
    }

    @Override
    public boolean setFrame(final int left, final int top, final int right, final int bottom) {
        return mWebViewPrivate.super_setFrame(left, top, right, bottom);
    }

    @Override
    public void onSizeChanged(final int w, final int h, final int ow, final int oh) {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            onSizeChanged(w, h, ow, oh);
                        }
                    });
            return;
        }
        mAwContents.onSizeChanged(w, h, ow, oh);
    }

    @Override
    public void onScrollChanged(final int l, final int t, final int oldl, final int oldt) {
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            onScrollChanged(l, t, oldl, oldt);
                        }
                    });
            return;
        }
        mAwContents.onContainerViewScrollChanged(l, t, oldl, oldt);
    }

    @Override
    public boolean dispatchKeyEvent(final KeyEvent event) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            boolean ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<Boolean>() {
                                @Override
                                public Boolean call() {
                                    return dispatchKeyEvent(event);
                                }
                            });
            return ret;
        }
        try (TraceEvent traceEvent =
                TraceEvent.scoped("WebView.APICall.Framework.DISPATCH_KEY_EVENT")) {
            recordWebViewSystemApiCall(SystemApiCall.DISPATCH_KEY_EVENT);
            return mAwContents.dispatchKeyEvent(event);
        }
    }

    @Override
    public boolean onTouchEvent(final MotionEvent ev) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            boolean ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<Boolean>() {
                                @Override
                                public Boolean call() {
                                    return onTouchEvent(ev);
                                }
                            });
            return ret;
        }
        try (TraceEvent traceEvent =
                TraceEvent.scoped("WebView.APICall.Framework.ON_TOUCH_EVENT")) {
            recordWebViewSystemApiCall(SystemApiCall.ON_TOUCH_EVENT);
            return mAwContents.onTouchEvent(ev);
        }
    }

    @Override
    public boolean onHoverEvent(final MotionEvent event) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            boolean ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<Boolean>() {
                                @Override
                                public Boolean call() {
                                    return onHoverEvent(event);
                                }
                            });
            return ret;
        }
        try (TraceEvent traceEvent =
                TraceEvent.scoped("WebView.APICall.Framework.ON_HOVER_EVENT")) {
            recordWebViewSystemApiCall(SystemApiCall.ON_HOVER_EVENT);
            return mAwContents.onHoverEvent(event);
        }
    }

    @Override
    public boolean onGenericMotionEvent(final MotionEvent event) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            boolean ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<Boolean>() {
                                @Override
                                public Boolean call() {
                                    return onGenericMotionEvent(event);
                                }
                            });
            return ret;
        }
        try (TraceEvent traceEvent =
                TraceEvent.scoped("WebView.APICall.Framework.ON_GENERIC_MOTION_EVENT")) {
            recordWebViewSystemApiCall(SystemApiCall.ON_GENERIC_MOTION_EVENT);
            return mAwContents.onGenericMotionEvent(event);
        }
    }

    @Override
    public boolean onTrackballEvent(MotionEvent ev) {
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.ON_TRACKBALL_EVENT")) {
            recordWebViewSystemApiCall(SystemApiCall.ON_TRACKBALL_EVENT);
            // Trackball event not handled, which eventually gets converted to DPAD keyevents
            return false;
        }
    }

    @Override
    public boolean requestFocus(final int direction, final Rect previouslyFocusedRect) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            boolean ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<Boolean>() {
                                @Override
                                public Boolean call() {
                                    return requestFocus(direction, previouslyFocusedRect);
                                }
                            });
            return ret;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.REQUEST_FOCUS")) {
            recordWebViewApiCall(ApiCall.REQUEST_FOCUS);
            mAwContents.requestFocus();
            return mWebViewPrivate.super_requestFocus(direction, previouslyFocusedRect);
        }
    }

    @Override
    @SuppressLint("DrawAllocation")
    public void onMeasure(final int widthMeasureSpec, final int heightMeasureSpec) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            mFactory.runVoidTaskOnUiThreadBlocking(
                    new Runnable() {
                        @Override
                        public void run() {
                            onMeasure(widthMeasureSpec, heightMeasureSpec);
                        }
                    });
            return;
        }
        mAwContents.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    @Override
    public boolean requestChildRectangleOnScreen(
            final View child, final Rect rect, final boolean immediate) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            boolean ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<Boolean>() {
                                @Override
                                public Boolean call() {
                                    return requestChildRectangleOnScreen(child, rect, immediate);
                                }
                            });
            return ret;
        }
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.REQUEST_CHILD_RECTANGLE_ON_SCREEN")) {
            recordWebViewApiCall(ApiCall.REQUEST_CHILD_RECTANGLE_ON_SCREEN);
            return mAwContents.requestChildRectangleOnScreen(child, rect, immediate);
        }
    }

    @Override
    public void setBackgroundColor(final int color) {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            setBackgroundColor(color);
                        }
                    });
            return;
        }
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.SET_BACKGROUND_COLOR")) {
            recordWebViewApiCall(ApiCall.SET_BACKGROUND_COLOR);
            mAwContents.setBackgroundColor(color);
        }
    }

    @Override
    public void setLayerType(final int layerType, final Paint paint) {
        // This can be called from WebView constructor in which case mAwContents
        // is still null. We set the layer type in initForReal in that case.
        if (mAwContents == null) return;
        if (checkNeedsPost()) {
            mFactory.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            setLayerType(layerType, paint);
                        }
                    });
            return;
        }
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.SET_LAYER_TYPE")) {
            recordWebViewApiCall(ApiCall.SET_LAYER_TYPE);
            mAwContents.setLayerType(layerType, paint);
        }
    }

    // Overrides method added to WebViewProvider.ViewDelegate interface
    // (not called in M and below)
    @Override
    public Handler getHandler(Handler originalHandler) {
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.GET_HANDLER")) {
            recordWebViewApiCall(ApiCall.GET_HANDLER);
            return originalHandler;
        }
    }

    // Overrides method added to WebViewProvider.ViewDelegate interface
    // (not called in M and below)
    @Override
    public View findFocus(View originalFocusedView) {
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.FIND_FOCUS")) {
            recordWebViewApiCall(ApiCall.FIND_FOCUS);
            return originalFocusedView;
        }
    }

    // Remove from superclass
    @Override
    public void preDispatchDraw(Canvas canvas) {
        // TODO(leandrogracia): remove this method from WebViewProvider if we think
        // we won't need it again.
    }

    @Override
    public void onStartTemporaryDetach() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.ON_START_TEMPORARY_DETACH")) {
            recordWebViewSystemApiCall(SystemApiCall.ON_START_TEMPORARY_DETACH);
            mAwContents.onStartTemporaryDetach();
        }
    }

    @Override
    public void onFinishTemporaryDetach() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.ON_FINISH_TEMPORARY_DETACH")) {
            recordWebViewSystemApiCall(SystemApiCall.ON_FINISH_TEMPORARY_DETACH);
            mAwContents.onFinishTemporaryDetach();
        }
    }

    @Override
    public boolean onCheckIsTextEditor() {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            return mFactory.runOnUiThreadBlocking(
                    new Callable<Boolean>() {
                        @Override
                        public Boolean call() {
                            return onCheckIsTextEditor();
                        }
                    });
        }
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.ON_CHECK_IS_TEXT_EDITOR")) {
            recordWebViewSystemApiCall(SystemApiCall.ON_CHECK_IS_TEXT_EDITOR);
            return mAwContents.onCheckIsTextEditor();
        }
    }

    // TODO(crbug.com/40280893): Add override annotation when SDK includes this method.
    public PointerIcon onResolvePointerIcon(MotionEvent event, int pointerIndex) {
        return mAwContents.onResolvePointerIcon(event, pointerIndex);
    }

    // WebViewProvider.ScrollDelegate implementation ----------------------------------------------

    @Override
    public int computeHorizontalScrollRange() {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            int ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<Integer>() {
                                @Override
                                public Integer call() {
                                    return computeHorizontalScrollRange();
                                }
                            });
            return ret;
        }
        return mAwContents.computeHorizontalScrollRange();
    }

    @Override
    public int computeHorizontalScrollOffset() {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            int ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<Integer>() {
                                @Override
                                public Integer call() {
                                    return computeHorizontalScrollOffset();
                                }
                            });
            return ret;
        }
        return mAwContents.computeHorizontalScrollOffset();
    }

    @Override
    public int computeVerticalScrollRange() {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            int ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<Integer>() {
                                @Override
                                public Integer call() {
                                    return computeVerticalScrollRange();
                                }
                            });
            return ret;
        }
        return mAwContents.computeVerticalScrollRange();
    }

    @Override
    public int computeVerticalScrollOffset() {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            int ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<Integer>() {
                                @Override
                                public Integer call() {
                                    return computeVerticalScrollOffset();
                                }
                            });
            return ret;
        }
        return mAwContents.computeVerticalScrollOffset();
    }

    @Override
    public int computeVerticalScrollExtent() {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            int ret =
                    mFactory.runOnUiThreadBlocking(
                            new Callable<Integer>() {
                                @Override
                                public Integer call() {
                                    return computeVerticalScrollExtent();
                                }
                            });
            return ret;
        }
        return mAwContents.computeVerticalScrollExtent();
    }

    @Override
    public void computeScroll() {
        mFactory.startYourEngines(false);
        if (checkNeedsPost()) {
            mFactory.runVoidTaskOnUiThreadBlocking(
                    new Runnable() {
                        @Override
                        public void run() {
                            computeScroll();
                        }
                    });
            return;
        }
        mAwContents.computeScroll();
    }

    @Override
    public PrintDocumentAdapter createPrintDocumentAdapter(String documentName) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.CREATE_PRINT_DOCUMENT_ADAPTER")) {
            recordWebViewApiCall(ApiCall.CREATE_PRINT_DOCUMENT_ADAPTER);
            checkThread();
            return new AwPrintDocumentAdapter(mAwContents.getPdfExporter(), documentName);
        }
    }

    // AwContents.NativeDrawFunctorFactory implementation ----------------------------------
    private class WebViewNativeDrawFunctorFactory implements AwContents.NativeDrawFunctorFactory {
        @Override
        public AwContents.NativeDrawGLFunctor createGLFunctor(long context) {
            return new DrawGLFunctor(context, mFactory.getWebViewDelegate());
        }

        @Override
        public AwDrawFnImpl.DrawFnAccess getDrawFnAccess() {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                return mFactory.getWebViewDelegate()::drawWebViewFunctor;
            }
            return null;
        }
    }

    // AwContents.InternalAccessDelegate implementation --------------------------------------
    private class InternalAccessAdapter implements AwContents.InternalAccessDelegate {
        @Override
        public boolean super_onKeyUp(int arg0, KeyEvent arg1) {
            // Intentional no-op
            return false;
        }

        @Override
        public boolean super_dispatchKeyEvent(KeyEvent event) {
            return mWebViewPrivate.super_dispatchKeyEvent(event);
        }

        @Override
        public boolean super_onGenericMotionEvent(MotionEvent arg0) {
            return mWebViewPrivate.super_onGenericMotionEvent(arg0);
        }

        @Override
        public void super_onConfigurationChanged(Configuration arg0) {
            // Intentional no-op
        }

        @Override
        public int super_getScrollBarStyle() {
            return mWebViewPrivate.super_getScrollBarStyle();
        }

        @Override
        public void super_startActivityForResult(Intent intent, int requestCode) {
            mWebViewPrivate.super_startActivityForResult(intent, requestCode);
        }

        @Override
        public void onScrollChanged(int l, int t, int oldl, int oldt) {
            // Intentional no-op.
            // Chromium calls this directly to trigger accessibility events. That isn't needed
            // for WebView since super_scrollTo invokes onScrollChanged for us.
        }

        @Override
        public void overScrollBy(
                int deltaX,
                int deltaY,
                int scrollX,
                int scrollY,
                int scrollRangeX,
                int scrollRangeY,
                int maxOverScrollX,
                int maxOverScrollY,
                boolean isTouchEvent) {
            mWebViewPrivate.overScrollBy(
                    deltaX,
                    deltaY,
                    scrollX,
                    scrollY,
                    scrollRangeX,
                    scrollRangeY,
                    maxOverScrollX,
                    maxOverScrollY,
                    isTouchEvent);
        }

        @Override
        public void super_scrollTo(int scrollX, int scrollY) {
            mWebViewPrivate.super_scrollTo(scrollX, scrollY);
        }

        @Override
        public void setMeasuredDimension(int measuredWidth, int measuredHeight) {
            mWebViewPrivate.setMeasuredDimension(measuredWidth, measuredHeight);
        }

        // @Override
        public boolean super_onHoverEvent(MotionEvent event) {
            return mWebViewPrivate.super_onHoverEvent(event);
        }
    }

    // Implements SmartClipProvider
    @Override
    public void extractSmartClipData(int x, int y, int width, int height) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.EXTRACT_SMART_CLIP_DATA")) {
            recordWebViewApiCall(ApiCall.EXTRACT_SMART_CLIP_DATA);
            checkThread();
            mAwContents.extractSmartClipData(x, y, width, height);
        }
    }

    // Implements SmartClipProvider
    @Override
    public void setSmartClipResultHandler(final Handler resultHandler) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.SET_SMART_CLIP_RESULT_HANDLER")) {
            recordWebViewApiCall(ApiCall.SET_SMART_CLIP_RESULT_HANDLER);
            checkThread();
            mAwContents.setSmartClipResultHandler(resultHandler);
        }
    }

    SharedWebViewChromium getSharedWebViewChromium() {
        return mSharedWebViewChromium;
    }
}
