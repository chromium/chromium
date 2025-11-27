// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import android.content.Context;
import android.net.Uri;
import android.webkit.ValueCallback;
import android.webkit.WebView;

import androidx.annotation.GuardedBy;
import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import com.android.webview.chromium.CallbackConverter;
import com.android.webview.chromium.ProfileStore;
import com.android.webview.chromium.SharedStatics;
import com.android.webview.chromium.SharedTracingControllerAdapter;
import com.android.webview.chromium.WebViewChromiumAwInit;
import com.android.webview.chromium.WebViewChromiumAwInit.CallSite;
import com.android.webview.chromium.WebkitToSharedGlueConverter;

import org.chromium.android_webview.AwProxyController;
import org.chromium.android_webview.AwServiceWorkerController;
import org.chromium.android_webview.AwTracingController;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.support_lib_boundary.StaticsBoundaryInterface;
import org.chromium.support_lib_boundary.WebViewProviderFactoryBoundaryInterface;
import org.chromium.support_lib_boundary.WebViewStartUpCallbackBoundaryInterface;
import org.chromium.support_lib_boundary.WebViewStartUpConfigBoundaryInterface;
import org.chromium.support_lib_boundary.util.BoundaryInterfaceReflectionUtil;
import org.chromium.support_lib_boundary.util.Features;

import java.lang.reflect.InvocationHandler;
import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/** Support library glue version of WebViewChromiumFactoryProvider. */
public class SupportLibWebViewChromiumFactory implements WebViewProviderFactoryBoundaryInterface {
    // SupportLibWebkitToCompatConverterAdapter
    private final InvocationHandler mCompatConverterAdapter;
    private final WebViewChromiumAwInit mAwInit;
    private static final String[] sWebViewSupportedFeatures =
            new String[] {
                Features.VISUAL_STATE_CALLBACK,
                Features.OFF_SCREEN_PRERASTER,
                Features.SAFE_BROWSING_ENABLE,
                Features.DISABLED_ACTION_MODE_MENU_ITEMS,
                Features.START_SAFE_BROWSING,
                Features.SAFE_BROWSING_ALLOWLIST,
                Features.SAFE_BROWSING_WHITELIST,
                Features.SAFE_BROWSING_PRIVACY_POLICY_URL,
                Features.SERVICE_WORKER_BASIC_USAGE,
                Features.SERVICE_WORKER_CACHE_MODE,
                Features.SERVICE_WORKER_CONTENT_ACCESS,
                Features.SERVICE_WORKER_FILE_ACCESS,
                Features.SERVICE_WORKER_BLOCK_NETWORK_LOADS,
                Features.SERVICE_WORKER_SHOULD_INTERCEPT_REQUEST,
                Features.RECEIVE_WEB_RESOURCE_ERROR,
                Features.RECEIVE_HTTP_ERROR,
                Features.SAFE_BROWSING_HIT,
                Features.SHOULD_OVERRIDE_WITH_REDIRECTS,
                Features.WEB_RESOURCE_REQUEST_IS_REDIRECT,
                Features.WEB_RESOURCE_ERROR_GET_DESCRIPTION,
                Features.WEB_RESOURCE_ERROR_GET_CODE,
                Features.SAFE_BROWSING_RESPONSE_BACK_TO_SAFETY,
                Features.SAFE_BROWSING_RESPONSE_PROCEED,
                Features.SAFE_BROWSING_RESPONSE_SHOW_INTERSTITIAL,
                Features.WEB_MESSAGE_PORT_POST_MESSAGE,
                Features.WEB_MESSAGE_PORT_CLOSE,
                Features.WEB_MESSAGE_PORT_SET_MESSAGE_CALLBACK,
                Features.CREATE_WEB_MESSAGE_CHANNEL,
                Features.POST_WEB_MESSAGE,
                Features.WEB_MESSAGE_CALLBACK_ON_MESSAGE,
                Features.GET_WEB_VIEW_CLIENT,
                Features.GET_WEB_CHROME_CLIENT,
                Features.PROXY_OVERRIDE,
                Features.SUPPRESS_ERROR_PAGE + Features.DEV_SUFFIX,
                Features.GET_WEB_VIEW_RENDERER,
                Features.WEB_VIEW_RENDERER_TERMINATE,
                Features.TRACING_CONTROLLER_BASIC_USAGE,
                Features.WEB_VIEW_RENDERER_CLIENT_BASIC_USAGE,
                Features.MULTI_PROCESS_QUERY,
                Features.FORCE_DARK,
                Features.FORCE_DARK_BEHAVIOR,
                Features.WEB_MESSAGE_LISTENER,
                Features.DOCUMENT_START_SCRIPT,
                Features.PROXY_OVERRIDE_REVERSE_BYPASS,
                Features.GET_VARIATIONS_HEADER,
                Features.ALGORITHMIC_DARKENING,
                Features.ENTERPRISE_AUTHENTICATION_APP_LINK_POLICY,
                Features.GET_COOKIE_INFO,
                Features.WEB_MESSAGE_ARRAY_BUFFER,
                Features.IMAGE_DRAG_DROP,
                Features.USER_AGENT_METADATA,
                Features.MULTI_PROFILE,
                Features.ATTRIBUTION_BEHAVIOR,
                Features.WEBVIEW_MEDIA_INTEGRITY_API_STATUS,
                Features.MUTE_AUDIO,
                Features.WEB_AUTHENTICATION,
                Features.SPECULATIVE_LOADING,
                Features.BACK_FORWARD_CACHE,
                Features.PREFETCH_WITH_URL,
                Features.DEFAULT_TRAFFICSTATS_TAGGING,
                Features.ASYNC_WEBVIEW_STARTUP,
                Features.PRERENDER_WITH_URL,
                Features.WEB_STORAGE_DELETE_BROWSING_DATA,
                Features.SPECULATIVE_LOADING_CONFIG,
                Features.SAVE_STATE,
                Features.WEB_VIEW_NAVIGATION_CLIENT_BASIC_USAGE,
                Features.PROVIDER_WEAKLY_REF_WEBVIEW,
                Features.PAYMENT_REQUEST,
                Features.WEBVIEW_BUILDER + Features.DEV_SUFFIX,
                Features.COOKIE_INTERCEPT,
                Features.WARM_UP_RENDERER_PROCESS,
                Features.EXTRA_HEADER_FOR_ORIGINS,
                Features.BACK_FORWARD_CACHE_SETTINGS,
                Features.PRECONNECT,
                Features.HYPERLINK_CONTEXT_MENU_ITEMS + Features.DEV_SUFFIX,
                Features.ASYNC_WEBVIEW_STARTUP_ASYNC_STARTUP_LOCATIONS + Features.DEV_SUFFIX,
                Features.PAGE_IS_PRERENDERING,
                Features.CUSTOM_REQUEST_HEADERS,
                Features.RENDERER_LIBRARY_PREFETCH_MODE + Features.DEV_SUFFIX,
                Features.WEB_VIEW_NAVIGATION_LISTENER_V1,
                Features.ADD_QUIC_HINTS_V1,
                Features.ON_NAVIGATION_COMPLETED_NON_COMMITTED,
                Features.COMMITTED_NAVIGATION_GET_PAGE_NON_NULL,
                // Add new features above. New features must include `+ Features.DEV_SUFFIX`
                // when they're initially added (this can be removed in a future CL). The final
                // feature should have a trailing comma for cleaner diffs.
            };

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    // LINT.IfChange(ApiCall)

    @IntDef({
        ApiCall.ADD_WEB_MESSAGE_LISTENER,
        ApiCall.CLEAR_PROXY_OVERRIDE,
        ApiCall.GET_PROXY_CONTROLLER,
        ApiCall.GET_SAFE_BROWSING_PRIVACY_POLICY_URL,
        ApiCall.GET_SERVICE_WORKER_CONTROLLER,
        ApiCall.GET_SERVICE_WORKER_WEB_SETTINGS,
        ApiCall.GET_TRACING_CONTROLLER,
        ApiCall.GET_WEBCHROME_CLIENT,
        ApiCall.GET_WEBVIEW_CLIENT,
        ApiCall.GET_WEBVIEW_RENDERER,
        ApiCall.GET_WEBVIEW_RENDERER_CLIENT,
        ApiCall.INIT_SAFE_BROWSING,
        ApiCall.INSERT_VISUAL_STATE_CALLBACK,
        ApiCall.IS_MULTI_PROCESS_ENABLED,
        ApiCall.JS_REPLY_POST_MESSAGE,
        ApiCall.POST_MESSAGE_TO_MAIN_FRAME,
        ApiCall.REMOVE_WEB_MESSAGE_LISTENER,
        ApiCall.SERVICE_WORKER_SETTINGS_GET_ALLOW_CONTENT_ACCESS,
        ApiCall.SERVICE_WORKER_SETTINGS_GET_ALLOW_FILE_ACCESS,
        ApiCall.SERVICE_WORKER_SETTINGS_GET_BLOCK_NETWORK_LOADS,
        ApiCall.SERVICE_WORKER_SETTINGS_GET_CACHE_MODE,
        ApiCall.SERVICE_WORKER_SETTINGS_SET_ALLOW_CONTENT_ACCESS,
        ApiCall.SERVICE_WORKER_SETTINGS_SET_ALLOW_FILE_ACCESS,
        ApiCall.SERVICE_WORKER_SETTINGS_SET_BLOCK_NETWORK_LOADS,
        ApiCall.SERVICE_WORKER_SETTINGS_SET_CACHE_MODE,
        ApiCall.SET_PROXY_OVERRIDE,
        ApiCall.SET_SAFE_BROWSING_ALLOWLIST_DEPRECATED_NAME,
        ApiCall.SET_SERVICE_WORKER_CLIENT,
        ApiCall.SET_WEBVIEW_RENDERER_CLIENT,
        ApiCall.TRACING_CONTROLLER_IS_TRACING,
        ApiCall.TRACING_CONTROLLER_START,
        ApiCall.TRACING_CONTROLLER_STOP,
        ApiCall.WEB_MESSAGE_GET_DATA,
        ApiCall.WEB_MESSAGE_GET_PORTS,
        ApiCall.WEB_MESSAGE_PORT_CLOSE,
        ApiCall.WEB_MESSAGE_PORT_POST_MESSAGE,
        ApiCall.WEB_MESSAGE_PORT_SET_CALLBACK,
        ApiCall.WEB_MESSAGE_PORT_SET_CALLBACK_WITH_HANDLER,
        ApiCall.WEB_RESOURCE_REQUEST_IS_REDIRECT,
        ApiCall.WEB_SETTINGS_GET_DISABLED_ACTION_MODE_MENU_ITEMS,
        ApiCall.WEB_SETTINGS_GET_FORCE_DARK,
        ApiCall.WEB_SETTINGS_GET_FORCE_DARK_BEHAVIOR,
        ApiCall.WEB_SETTINGS_GET_OFFSCREEN_PRE_RASTER,
        ApiCall.WEB_SETTINGS_GET_SAFE_BROWSING_ENABLED,
        ApiCall.WEB_SETTINGS_GET_WILL_SUPPRESS_ERROR_PAGE,
        ApiCall.WEB_SETTINGS_SET_DISABLED_ACTION_MODE_MENU_ITEMS,
        ApiCall.WEB_SETTINGS_SET_FORCE_DARK,
        ApiCall.WEB_SETTINGS_SET_FORCE_DARK_BEHAVIOR,
        ApiCall.WEB_SETTINGS_SET_OFFSCREEN_PRE_RASTER,
        ApiCall.WEB_SETTINGS_SET_SAFE_BROWSING_ENABLED,
        ApiCall.WEB_SETTINGS_SET_WILL_SUPPRESS_ERROR_PAGE,
        ApiCall.WEBVIEW_RENDERER_TERMINATE,
        ApiCall.ADD_DOCUMENT_START_SCRIPT,
        ApiCall.REMOVE_DOCUMENT_START_SCRIPT,
        ApiCall.SET_SAFE_BROWSING_ALLOWLIST,
        ApiCall.SET_PROXY_OVERRIDE_REVERSE_BYPASS,
        ApiCall.WEB_SETTINGS_SET_REQUESTED_WITH_HEADER_MODE,
        ApiCall.WEB_SETTINGS_GET_REQUESTED_WITH_HEADER_MODE,
        ApiCall.SERVICE_WORKER_SETTINGS_SET_REQUESTED_WITH_HEADER_MODE,
        ApiCall.SERVICE_WORKER_SETTINGS_GET_REQUESTED_WITH_HEADER_MODE,
        ApiCall.GET_VARIATIONS_HEADER,
        ApiCall.WEB_SETTINGS_GET_ENTERPRISE_AUTHENTICATION_APP_LINK_POLICY_ENABLED,
        ApiCall.WEB_SETTINGS_SET_ENTERPRISE_AUTHENTICATION_APP_LINK_POLICY_ENABLED,
        ApiCall.COOKIE_MANAGER_GET_COOKIE_INFO,
        ApiCall.WEB_MESSAGE_GET_MESSAGE_PAYLOAD,
        ApiCall.WEB_MESSAGE_PAYLOAD_GET_TYPE,
        ApiCall.WEB_MESSAGE_PAYLOAD_GET_AS_STRING,
        ApiCall.WEB_MESSAGE_PAYLOAD_GET_AS_ARRAY_BUFFER,
        ApiCall.WEB_SETTINGS_SET_REQUESTED_WITH_HEADER_ORIGIN_ALLOWLIST,
        ApiCall.WEB_SETTINGS_GET_REQUESTED_WITH_HEADER_ORIGIN_ALLOWLIST,
        ApiCall.SERVICE_WORKER_SETTINGS_SET_REQUESTED_WITH_HEADER_ORIGIN_ALLOWLIST,
        ApiCall.SERVICE_WORKER_SETTINGS_GET_REQUESTED_WITH_HEADER_ORIGIN_ALLOWLIST,
        ApiCall.GET_IMAGE_DRAG_DROP_IMPLEMENTATION,
        ApiCall.JS_REPLY_POST_MESSAGE_WITH_PAYLOAD,
        ApiCall.WEB_SETTINGS_SET_USER_AGENT_METADATA,
        ApiCall.WEB_SETTINGS_GET_USER_AGENT_METADATA,
        ApiCall.SERVICE_WORKER_CLIENT_SHOULD_INTERCEPT_REQUEST,
        ApiCall.WEB_SETTINGS_SET_ALGORITHMIC_DARKENING_ALLOWED,
        ApiCall.WEB_SETTINGS_IS_ALGORITHMIC_DARKENING_ALLOWED,
        ApiCall.CREATE_WEB_MESSAGE_CHANNEL,
        ApiCall.CREATE_WEBVIEW,
        ApiCall.GET_STATICS,
        ApiCall.GET_PROFILE_STORE,
        ApiCall.GET_OR_CREATE_PROFILE,
        ApiCall.GET_PROFILE,
        ApiCall.GET_ALL_PROFILE_NAMES,
        ApiCall.DELETE_PROFILE,
        ApiCall.GET_PROFILE_NAME,
        ApiCall.GET_PROFILE_COOKIE_MANAGER,
        ApiCall.GET_PROFILE_WEB_STORAGE,
        ApiCall.GET_PROFILE_GEO_LOCATION_PERMISSIONS,
        ApiCall.GET_PROFILE_SERVICE_WORKER_CONTROLLER,
        ApiCall.SET_WEBVIEW_PROFILE,
        ApiCall.GET_WEBVIEW_PROFILE,
        ApiCall.SET_ATTRIBUTION_BEHAVIOR,
        ApiCall.GET_ATTRIBUTION_BEHAVIOR,
        ApiCall.GET_WEBVIEW_MEDIA_INTEGRITY_API_DEFAULT_STATUS,
        ApiCall.GET_WEBVIEW_MEDIA_INTEGRITY_API_OVERRIDE_RULES,
        ApiCall.SET_WEBVIEW_MEDIA_INTEGRITY_API_STATUS,
        ApiCall.SET_AUDIO_MUTED,
        ApiCall.IS_AUDIO_MUTED,
        ApiCall.WEB_SETTINGS_SET_WEBAUTHN_SUPPORT,
        ApiCall.WEB_SETTINGS_GET_WEBAUTHN_SUPPORT,
        ApiCall.SET_SPECULATIVE_LOADING_STATUS,
        ApiCall.GET_SPECULATIVE_LOADING_STATUS,
        ApiCall.SET_BACK_FORWARD_CACHE_ENABLED,
        ApiCall.GET_BACK_FORWARD_CACHE_ENABLED,
        ApiCall.PREFETCH_URL,
        ApiCall.PREFETCH_URL_WITH_PARAMS,
        ApiCall.CLEAR_PREFETCH,
        ApiCall.CANCEL_PREFETCH,
        ApiCall.SET_DEFAULT_TRAFFICSTATS_TAG,
        ApiCall.SET_DEFAULT_TRAFFICSTATS_UID,
        ApiCall.START_UP_WEBVIEW,
        ApiCall.PRERENDER_URL,
        ApiCall.PRERENDER_URL_WITH_PARAMS,
        ApiCall.WEB_STORAGE_DELETE_BROWSING_DATA,
        ApiCall.WEB_STORAGE_DELETE_BROWSING_DATA_FOR_SITE,
        ApiCall.SET_SPECULATIVE_LOADING_CONFIG,
        ApiCall.SAVE_STATE,
        ApiCall.GET_WEBVIEW_NAVIGATION_CLIENT,
        ApiCall.SET_WEBVIEW_NAVIGATION_CLIENT,
        ApiCall.NAVIGATION_GET_URL,
        ApiCall.NAVIGATION_WAS_INITIATED_BY_PAGE,
        ApiCall.NAVIGATION_IS_SAME_DOCUMENT,
        ApiCall.NAVIGATION_IS_RELOAD,
        ApiCall.NAVIGATION_IS_HISTORY,
        ApiCall.NAVIGATION_IS_RESTORE,
        ApiCall.NAVIGATION_IS_BACK,
        ApiCall.NAVIGATION_IS_FORWARD,
        ApiCall.NAVIGATION_DID_COMMIT,
        ApiCall.NAVIGATION_DID_COMMIT_ERROR_PAGE,
        ApiCall.NAVIGATION_GET_STATUS_CODE,
        ApiCall.CLEAR_ASYNC_SHOULD_INTERCEPT_REQUEST,
        ApiCall.SET_ASYNC_SHOULD_INTERCEPT_REQUEST,
        ApiCall.SERVICE_WORKER_CLEAR_ASYNC_SHOULD_INTERCEPT_REQUEST,
        ApiCall.SERVICE_WORKER_SET_ASYNC_SHOULD_INTERCEPT_REQUEST,
        ApiCall.WEB_RESPONSE_CALLBACK_DO_NOT_INTERCEPT,
        ApiCall.WEB_RESPONSE_CALLBACK_INTERCEPT,
        ApiCall.NAVIGATION_GET_PAGE,
        ApiCall.SET_PAYMENT_REQUEST_ENABLED,
        ApiCall.GET_PAYMENT_REQUEST_ENABLED,
        ApiCall.SET_HAS_ENROLLED_INSTRUMENT_ENABLED,
        ApiCall.GET_HAS_ENROLLED_INSTRUMENT_ENABLED,
        ApiCall.GET_WEBVIEW_BUILDER,
        ApiCall.SET_INCLUDE_COOKIES_ON_INTERCEPT,
        ApiCall.GET_INCLUDE_COOKIES_ON_INTERCEPT,
        ApiCall.SERVICE_WORKER_SET_INCLUDE_COOKIES_ON_INTERCEPT,
        ApiCall.SERVICE_WORKER_GET_INCLUDE_COOKIES_ON_INTERCEPT,
        ApiCall.PROFILE_WARM_UP_RENDERER_PROCESS,
        ApiCall.SET_ORIGIN_MATCHED_HEADER,
        ApiCall.HAS_ORIGIN_MATCHED_HEADER,
        ApiCall.CLEAR_ORIGIN_MATCHED_HEADER,
        ApiCall.CLEAR_ALL_ORIGIN_MATCHED_HEADERS,
        ApiCall.SET_BACK_FORWARD_CACHE_SETTINGS,
        ApiCall.GET_BACK_FORWARD_CACHE_SETTINGS,
        ApiCall.BACK_FORWARD_CACHE_SETTINGS_GET_TIMEOUT_IN_SECONDS,
        ApiCall.BACK_FORWARD_CACHE_SETTINGS_GET_MAX_PAGES_IN_CACHE,
        ApiCall.PRECONNECT,
        ApiCall.SET_HYPERLINK_CONTEXT_MENU_ITEMS,
        ApiCall.PAGE_IS_PRERENDERING,
        ApiCall.ADD_ORIGIN_MATCHED_HEADER,
        ApiCall.GET_ORIGIN_MATCHED_HEADERS,
        ApiCall.SET_RENDERER_LIBRARY_PREFETCH_MODE,
        ApiCall.GET_RENDERER_LIBRARY_PREFETCH_MODE,
        ApiCall.BACK_FORWARD_CACHE_SETTINGS_SET_TIMEOUT_IN_SECONDS,
        ApiCall.BACK_FORWARD_CACHE_SETTINGS_SET_MAX_PAGES_IN_CACHE,
        ApiCall.ADD_NAVIGATION_LISTENER,
        ApiCall.REMOVE_NAVIGATION_LISTENER,
        ApiCall.ADD_QUIC_HINTS,
        // Add new constants above. The final constant should have a trailing comma for cleaner
        // diffs.
        ApiCall.COUNT, // Added to suppress WrongConstant in #recordApiCall
    })
    public @interface ApiCall {
        int ADD_WEB_MESSAGE_LISTENER = 0;
        int CLEAR_PROXY_OVERRIDE = 1;
        int GET_PROXY_CONTROLLER = 2;
        int GET_SAFE_BROWSING_PRIVACY_POLICY_URL = 3;
        int GET_SERVICE_WORKER_CONTROLLER = 4;
        int GET_SERVICE_WORKER_WEB_SETTINGS = 5;
        int GET_TRACING_CONTROLLER = 6;
        int GET_WEBCHROME_CLIENT = 7;
        int GET_WEBVIEW_CLIENT = 8;
        int GET_WEBVIEW_RENDERER = 9;
        int GET_WEBVIEW_RENDERER_CLIENT = 10;
        int INIT_SAFE_BROWSING = 11;
        int INSERT_VISUAL_STATE_CALLBACK = 12;
        int IS_MULTI_PROCESS_ENABLED = 13;
        int JS_REPLY_POST_MESSAGE = 14;
        int POST_MESSAGE_TO_MAIN_FRAME = 15;
        int REMOVE_WEB_MESSAGE_LISTENER = 16;
        int SERVICE_WORKER_SETTINGS_GET_ALLOW_CONTENT_ACCESS = 17;
        int SERVICE_WORKER_SETTINGS_GET_ALLOW_FILE_ACCESS = 18;
        int SERVICE_WORKER_SETTINGS_GET_BLOCK_NETWORK_LOADS = 19;
        int SERVICE_WORKER_SETTINGS_GET_CACHE_MODE = 20;
        int SERVICE_WORKER_SETTINGS_SET_ALLOW_CONTENT_ACCESS = 21;
        int SERVICE_WORKER_SETTINGS_SET_ALLOW_FILE_ACCESS = 22;
        int SERVICE_WORKER_SETTINGS_SET_BLOCK_NETWORK_LOADS = 23;
        int SERVICE_WORKER_SETTINGS_SET_CACHE_MODE = 24;
        int SET_PROXY_OVERRIDE = 25;
        int SET_SAFE_BROWSING_ALLOWLIST_DEPRECATED_NAME = 26;
        int SET_SERVICE_WORKER_CLIENT = 27;
        int SET_WEBVIEW_RENDERER_CLIENT = 28;
        int TRACING_CONTROLLER_IS_TRACING = 29;
        int TRACING_CONTROLLER_START = 30;
        int TRACING_CONTROLLER_STOP = 31;
        int WEB_MESSAGE_GET_DATA = 32;
        int WEB_MESSAGE_GET_PORTS = 33;
        int WEB_MESSAGE_PORT_CLOSE = 34;
        int WEB_MESSAGE_PORT_POST_MESSAGE = 35;
        int WEB_MESSAGE_PORT_SET_CALLBACK = 36;
        int WEB_MESSAGE_PORT_SET_CALLBACK_WITH_HANDLER = 37;
        int WEB_RESOURCE_REQUEST_IS_REDIRECT = 38;
        int WEB_SETTINGS_GET_DISABLED_ACTION_MODE_MENU_ITEMS = 39;
        int WEB_SETTINGS_GET_FORCE_DARK = 40;
        int WEB_SETTINGS_GET_FORCE_DARK_BEHAVIOR = 41;
        int WEB_SETTINGS_GET_OFFSCREEN_PRE_RASTER = 42;
        int WEB_SETTINGS_GET_SAFE_BROWSING_ENABLED = 43;
        int WEB_SETTINGS_GET_WILL_SUPPRESS_ERROR_PAGE = 44;
        int WEB_SETTINGS_SET_DISABLED_ACTION_MODE_MENU_ITEMS = 45;
        int WEB_SETTINGS_SET_FORCE_DARK = 46;
        int WEB_SETTINGS_SET_FORCE_DARK_BEHAVIOR = 47;
        int WEB_SETTINGS_SET_OFFSCREEN_PRE_RASTER = 48;
        int WEB_SETTINGS_SET_SAFE_BROWSING_ENABLED = 49;
        int WEB_SETTINGS_SET_WILL_SUPPRESS_ERROR_PAGE = 50;
        int WEBVIEW_RENDERER_TERMINATE = 51;
        int ADD_DOCUMENT_START_SCRIPT = 52;
        int REMOVE_DOCUMENT_START_SCRIPT = 53;
        int SET_SAFE_BROWSING_ALLOWLIST = 54;
        int SET_PROXY_OVERRIDE_REVERSE_BYPASS = 55;
        @Deprecated int WEB_SETTINGS_SET_REQUESTED_WITH_HEADER_MODE = 56;
        @Deprecated int WEB_SETTINGS_GET_REQUESTED_WITH_HEADER_MODE = 57;
        @Deprecated int SERVICE_WORKER_SETTINGS_SET_REQUESTED_WITH_HEADER_MODE = 58;
        @Deprecated int SERVICE_WORKER_SETTINGS_GET_REQUESTED_WITH_HEADER_MODE = 59;
        int GET_VARIATIONS_HEADER = 60;
        int WEB_SETTINGS_GET_ENTERPRISE_AUTHENTICATION_APP_LINK_POLICY_ENABLED = 61;
        int WEB_SETTINGS_SET_ENTERPRISE_AUTHENTICATION_APP_LINK_POLICY_ENABLED = 62;
        int COOKIE_MANAGER_GET_COOKIE_INFO = 63;
        int WEB_MESSAGE_GET_MESSAGE_PAYLOAD = 64;
        int WEB_MESSAGE_PAYLOAD_GET_TYPE = 65;
        int WEB_MESSAGE_PAYLOAD_GET_AS_STRING = 66;
        int WEB_MESSAGE_PAYLOAD_GET_AS_ARRAY_BUFFER = 67;
        @Deprecated int WEB_SETTINGS_SET_REQUESTED_WITH_HEADER_ORIGIN_ALLOWLIST = 68;
        @Deprecated int WEB_SETTINGS_GET_REQUESTED_WITH_HEADER_ORIGIN_ALLOWLIST = 69;
        @Deprecated int SERVICE_WORKER_SETTINGS_SET_REQUESTED_WITH_HEADER_ORIGIN_ALLOWLIST = 70;
        @Deprecated int SERVICE_WORKER_SETTINGS_GET_REQUESTED_WITH_HEADER_ORIGIN_ALLOWLIST = 71;
        int GET_IMAGE_DRAG_DROP_IMPLEMENTATION = 72;
        @Deprecated int RESTRICT_SENSITIVE_WEB_CONTENT = 73;
        int JS_REPLY_POST_MESSAGE_WITH_PAYLOAD = 74;
        int WEB_SETTINGS_SET_USER_AGENT_METADATA = 75;
        int WEB_SETTINGS_GET_USER_AGENT_METADATA = 76;
        int SERVICE_WORKER_CLIENT_SHOULD_INTERCEPT_REQUEST = 77;
        int WEB_SETTINGS_SET_ALGORITHMIC_DARKENING_ALLOWED = 78;
        int WEB_SETTINGS_IS_ALGORITHMIC_DARKENING_ALLOWED = 79;
        int CREATE_WEB_MESSAGE_CHANNEL = 80;
        int CREATE_WEBVIEW = 81;
        int GET_STATICS = 82;
        int GET_PROFILE_STORE = 83;
        int GET_OR_CREATE_PROFILE = 84;
        int GET_PROFILE = 85;
        int GET_ALL_PROFILE_NAMES = 86;
        int DELETE_PROFILE = 87;
        int GET_PROFILE_NAME = 88;
        int GET_PROFILE_COOKIE_MANAGER = 89;
        int GET_PROFILE_WEB_STORAGE = 90;
        int GET_PROFILE_GEO_LOCATION_PERMISSIONS = 91;
        int GET_PROFILE_SERVICE_WORKER_CONTROLLER = 92;

        int SET_WEBVIEW_PROFILE = 93;
        int GET_WEBVIEW_PROFILE = 94;
        int SET_ATTRIBUTION_BEHAVIOR = 95;
        int GET_ATTRIBUTION_BEHAVIOR = 96;
        int GET_WEBVIEW_MEDIA_INTEGRITY_API_DEFAULT_STATUS = 97;
        int GET_WEBVIEW_MEDIA_INTEGRITY_API_OVERRIDE_RULES = 98;
        int SET_WEBVIEW_MEDIA_INTEGRITY_API_STATUS = 99;
        int SET_AUDIO_MUTED = 100;
        int IS_AUDIO_MUTED = 101;
        int WEB_SETTINGS_SET_WEBAUTHN_SUPPORT = 102;
        int WEB_SETTINGS_GET_WEBAUTHN_SUPPORT = 103;
        int SET_SPECULATIVE_LOADING_STATUS = 104;
        int GET_SPECULATIVE_LOADING_STATUS = 105;
        int SET_BACK_FORWARD_CACHE_ENABLED = 106;
        int GET_BACK_FORWARD_CACHE_ENABLED = 107;
        int PREFETCH_URL = 108;
        int PREFETCH_URL_WITH_PARAMS = 109;
        int CLEAR_PREFETCH = 110;
        int CANCEL_PREFETCH = 111;
        int SET_DEFAULT_TRAFFICSTATS_TAG = 112;
        int SET_DEFAULT_TRAFFICSTATS_UID = 113;
        int START_UP_WEBVIEW = 114;
        int PRERENDER_URL = 115;
        int PRERENDER_URL_WITH_PARAMS = 116;
        int WEB_STORAGE_DELETE_BROWSING_DATA = 117;
        int WEB_STORAGE_DELETE_BROWSING_DATA_FOR_SITE = 118;
        int SET_SPECULATIVE_LOADING_CONFIG = 119;
        int SAVE_STATE = 120;
        @Deprecated int GET_WEBVIEW_NAVIGATION_CLIENT = 121;
        @Deprecated int SET_WEBVIEW_NAVIGATION_CLIENT = 122;
        int NAVIGATION_GET_URL = 123;
        int NAVIGATION_WAS_INITIATED_BY_PAGE = 124;
        int NAVIGATION_IS_SAME_DOCUMENT = 125;
        int NAVIGATION_IS_RELOAD = 126;
        int NAVIGATION_IS_HISTORY = 127;
        int NAVIGATION_IS_RESTORE = 128;
        int NAVIGATION_IS_BACK = 129;
        int NAVIGATION_IS_FORWARD = 130;
        int NAVIGATION_DID_COMMIT = 131;
        int NAVIGATION_DID_COMMIT_ERROR_PAGE = 132;
        int NAVIGATION_GET_STATUS_CODE = 133;
        @Deprecated // Never launched
        int CLEAR_ASYNC_SHOULD_INTERCEPT_REQUEST = 134;
        @Deprecated // Never launched
        int SET_ASYNC_SHOULD_INTERCEPT_REQUEST = 135;
        int SERVICE_WORKER_CLEAR_ASYNC_SHOULD_INTERCEPT_REQUEST = 136;
        int SERVICE_WORKER_SET_ASYNC_SHOULD_INTERCEPT_REQUEST = 137;
        int WEB_RESPONSE_CALLBACK_DO_NOT_INTERCEPT = 138;
        int WEB_RESPONSE_CALLBACK_INTERCEPT = 139;
        int NAVIGATION_GET_PAGE = 140;
        int SET_PAYMENT_REQUEST_ENABLED = 141;
        int GET_PAYMENT_REQUEST_ENABLED = 142;
        int SET_HAS_ENROLLED_INSTRUMENT_ENABLED = 143;
        int GET_HAS_ENROLLED_INSTRUMENT_ENABLED = 144;
        int GET_WEBVIEW_BUILDER = 145;
        int SET_INCLUDE_COOKIES_ON_INTERCEPT = 146;
        int GET_INCLUDE_COOKIES_ON_INTERCEPT = 147;
        int SERVICE_WORKER_SET_INCLUDE_COOKIES_ON_INTERCEPT = 148;
        int SERVICE_WORKER_GET_INCLUDE_COOKIES_ON_INTERCEPT = 149;
        int PROFILE_WARM_UP_RENDERER_PROCESS = 150;
        int SET_ORIGIN_MATCHED_HEADER = 151;
        int HAS_ORIGIN_MATCHED_HEADER = 152;
        int CLEAR_ORIGIN_MATCHED_HEADER = 153;
        int CLEAR_ALL_ORIGIN_MATCHED_HEADERS = 154;
        int SET_BACK_FORWARD_CACHE_SETTINGS = 155;
        int GET_BACK_FORWARD_CACHE_SETTINGS = 156;
        int BACK_FORWARD_CACHE_SETTINGS_GET_TIMEOUT_IN_SECONDS = 157;
        int BACK_FORWARD_CACHE_SETTINGS_GET_MAX_PAGES_IN_CACHE = 158;
        int PRECONNECT = 159;
        int SET_HYPERLINK_CONTEXT_MENU_ITEMS = 160;
        int PAGE_IS_PRERENDERING = 161;
        int ADD_ORIGIN_MATCHED_HEADER = 162;
        int GET_ORIGIN_MATCHED_HEADERS = 163;
        int SET_RENDERER_LIBRARY_PREFETCH_MODE = 164;
        int GET_RENDERER_LIBRARY_PREFETCH_MODE = 165;
        int BACK_FORWARD_CACHE_SETTINGS_SET_TIMEOUT_IN_SECONDS = 166;
        int BACK_FORWARD_CACHE_SETTINGS_SET_MAX_PAGES_IN_CACHE = 167;
        int ADD_NAVIGATION_LISTENER = 168;
        int REMOVE_NAVIGATION_LISTENER = 169;
        int ADD_QUIC_HINTS = 170;
        // Remember to update AndroidXWebkitApiCall in enums.xml when adding new values here
        int COUNT = 171;
    }

    // LINT.ThenChange(/tools/metrics/histograms/metadata/android/enums.xml:AndroidXWebkitApiCall)

    public static void recordApiCall(@ApiCall int apiCall) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.AndroidX.ApiCall", apiCall, ApiCall.COUNT);
    }

    private final InvocationHandler mStatics;

    @GuardedBy("mAwInit.getLazyInitLock()")
    private InvocationHandler mServiceWorkerController;

    @GuardedBy("mAwInit.getLazyInitLock()")
    private InvocationHandler mTracingController;

    @GuardedBy("mAwInit.getLazyInitLock()")
    private InvocationHandler mProxyController;

    @GuardedBy("mAwInit.getLazyInitLock()")
    private InvocationHandler mDropDataProvider;

    @GuardedBy("mAwInit.getLazyInitLock()")
    private InvocationHandler mProfileStore;

    public SupportLibWebViewChromiumFactory() {
        mCompatConverterAdapter =
                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                        new SupportLibWebkitToCompatConverterAdapter());
        mAwInit = WebkitToSharedGlueConverter.getGlobalAwInit();
        mStatics =
                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                        new StaticsAdapter(mAwInit.getSharedStatics()));
    }

    @Override
    public /* WebViewBuilderBoundaryInterface */ InvocationHandler getWebViewBuilder() {
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.AndroidX.GET_WEBVIEW_BUILDER")) {
            recordApiCall(ApiCall.GET_WEBVIEW_BUILDER);
            return BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                    new SupportLibWebViewBuilderAdapter());
        }
    }

    @Override
    public /* WebViewProvider */ InvocationHandler createWebView(WebView webView) {
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.AndroidX.CREATE_WEBVIEW")) {
            recordApiCall(ApiCall.CREATE_WEBVIEW);
            return BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                    new SupportLibWebViewChromium(webView));
        }
    }

    @Override
    public InvocationHandler getWebkitToCompatConverter() {
        return mCompatConverterAdapter;
    }

    private static class StaticsAdapter implements StaticsBoundaryInterface {
        private final SharedStatics mSharedStatics;

        StaticsAdapter(SharedStatics sharedStatics) {
            mSharedStatics = sharedStatics;
        }

        @Override
        public void initSafeBrowsing(Context context, ValueCallback<Boolean> callback) {
            try (TraceEvent event =
                    TraceEvent.scoped("WebView.APICall.AndroidX.INIT_SAFE_BROWSING")) {
                recordApiCall(ApiCall.INIT_SAFE_BROWSING);
                mSharedStatics.initSafeBrowsing(
                        context, CallbackConverter.fromValueCallback(callback));
            }
        }

        @Override
        public void setSafeBrowsingAllowlist(Set<String> hosts, ValueCallback<Boolean> callback) {
            try (TraceEvent event =
                    TraceEvent.scoped("WebView.APICall.AndroidX.SET_SAFE_BROWSING_ALLOWLIST")) {
                recordApiCall(ApiCall.SET_SAFE_BROWSING_ALLOWLIST);
                mSharedStatics.setSafeBrowsingAllowlist(
                        new ArrayList<>(hosts), CallbackConverter.fromValueCallback(callback));
            }
        }

        @Override
        public void setSafeBrowsingWhitelist(List<String> hosts, ValueCallback<Boolean> callback) {
            try (TraceEvent event =
                    TraceEvent.scoped(
                            "WebView.APICall.AndroidX.SET_SAFE_BROWSING_ALLOWLIST_DEPRECATED_NAME")) {
                recordApiCall(ApiCall.SET_SAFE_BROWSING_ALLOWLIST_DEPRECATED_NAME);
                mSharedStatics.setSafeBrowsingAllowlist(
                        hosts, CallbackConverter.fromValueCallback(callback));
            }
        }

        @Override
        public Uri getSafeBrowsingPrivacyPolicyUrl() {
            try (TraceEvent event =
                    TraceEvent.scoped(
                            "WebView.APICall.AndroidX.GET_SAFE_BROWSING_PRIVACY_POLICY_URL")) {
                recordApiCall(ApiCall.GET_SAFE_BROWSING_PRIVACY_POLICY_URL);
                return mSharedStatics.getSafeBrowsingPrivacyPolicyUrl();
            }
        }

        @Override
        public boolean isMultiProcessEnabled() {
            try (TraceEvent event =
                    TraceEvent.scoped("WebView.APICall.AndroidX.IS_MULTI_PROCESS_ENABLED")) {
                recordApiCall(ApiCall.IS_MULTI_PROCESS_ENABLED);
                return mSharedStatics.isMultiProcessEnabled();
            }
        }

        @Override
        public String getVariationsHeader() {
            try (TraceEvent event =
                    TraceEvent.scoped("WebView.APICall.AndroidX.GET_VARIATIONS_HEADER")) {
                recordApiCall(ApiCall.GET_VARIATIONS_HEADER);
                return mSharedStatics.getVariationsHeader();
            }
        }

        @Override
        public void setDefaultTrafficStatsTag(int tag) {
            try (TraceEvent event =
                    TraceEvent.scoped("WebView.APICall.AndroidX.SET_DEFAULT_TRAFFICSTATS_TAG")) {
                recordApiCall(ApiCall.SET_DEFAULT_TRAFFICSTATS_TAG);
                mSharedStatics.setDefaultTrafficStatsTag(tag);
            }
        }

        @Override
        public void setDefaultTrafficStatsUid(int uid) {
            try (TraceEvent event =
                    TraceEvent.scoped("WebView.APICall.AndroidX.SET_DEFAULT_TRAFFICSTATS_UID")) {
                recordApiCall(ApiCall.SET_DEFAULT_TRAFFICSTATS_UID);
                mSharedStatics.setDefaultTrafficStatsUid(uid);
            }
        }

        @Override
        public void setRendererLibraryPrefetchMode(int mode) {
            try (TraceEvent event =
                    TraceEvent.scoped(
                            "WebView.APICall.AndroidX.SET_RENDERER_LIBRARY_PREFETCH_MODE")) {
                recordApiCall(ApiCall.SET_RENDERER_LIBRARY_PREFETCH_MODE);
                mSharedStatics.setRendererLibraryPrefetchMode(mode);
            }
        }

        @Override
        public int getRendererLibraryPrefetchMode() {
            try (TraceEvent event =
                    TraceEvent.scoped(
                            "WebView.APICall.AndroidX.GET_RENDERER_LIBRARY_PREFETCH_MODE")) {
                recordApiCall(ApiCall.GET_RENDERER_LIBRARY_PREFETCH_MODE);
                return mSharedStatics.getRendererLibraryPrefetchMode();
            }
        }
    }

    @Override
    public InvocationHandler getStatics() {
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.AndroidX.GET_STATICS")) {
            recordApiCall(ApiCall.GET_STATICS);
            return mStatics;
        }
    }

    @Override
    public String[] getSupportedFeatures() {
        return sWebViewSupportedFeatures;
    }

    public static String[] getSupportedFeaturesForTesting() {
        return sWebViewSupportedFeatures;
    }

    @Override
    public InvocationHandler getServiceWorkerController() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.GET_SERVICE_WORKER_CONTROLLER")) {
            recordApiCall(ApiCall.GET_SERVICE_WORKER_CONTROLLER);
            AwServiceWorkerController serviceWorkerController =
                    mAwInit.getDefaultProfile(CallSite.GET_DEFAULT_SERVICE_WORKER_CONTROLLER)
                            .getBrowserContext()
                            .getServiceWorkerController();
            synchronized (mAwInit.getLazyInitLock()) {
                if (mServiceWorkerController == null) {
                    mServiceWorkerController =
                            BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                    new SupportLibServiceWorkerControllerAdapter(
                                            serviceWorkerController));
                }

                return mServiceWorkerController;
            }
        }
    }

    @Override
    public InvocationHandler getTracingController() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.GET_TRACING_CONTROLLER")) {
            recordApiCall(ApiCall.GET_TRACING_CONTROLLER);
            AwTracingController tracingController = mAwInit.getAwTracingController();
            synchronized (mAwInit.getLazyInitLock()) {
                if (mTracingController == null) {
                    mTracingController =
                            BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                    new SupportLibTracingControllerAdapter(
                                            new SharedTracingControllerAdapter(
                                                    mAwInit.getRunQueue(), tracingController)));
                }

                return mTracingController;
            }
        }
    }

    @Override
    public InvocationHandler getProxyController() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.GET_PROXY_CONTROLLER")) {
            recordApiCall(ApiCall.GET_PROXY_CONTROLLER);
            AwProxyController proxyController = mAwInit.getAwProxyController();
            synchronized (mAwInit.getLazyInitLock()) {
                if (mProxyController == null) {
                    mProxyController =
                            BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                    new SupportLibProxyControllerAdapter(
                                            mAwInit.getRunQueue(), proxyController));
                }

                return mProxyController;
            }
        }
    }

    @Override
    public InvocationHandler getDropDataProvider() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.GET_IMAGE_DRAG_DROP_IMPLEMENTATION")) {
            recordApiCall(ApiCall.GET_IMAGE_DRAG_DROP_IMPLEMENTATION);
            synchronized (mAwInit.getLazyInitLock()) {
                if (mDropDataProvider == null) {
                    mDropDataProvider =
                            BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                    new SupportLibDropDataContentProviderAdapter());
                }
                return mDropDataProvider;
            }
        }
    }

    @Override
    public InvocationHandler getProfileStore() {
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.AndroidX.GET_PROFILE_STORE")) {
            recordApiCall(ApiCall.GET_PROFILE_STORE);
            synchronized (mAwInit.getLazyInitLock()) {
                if (mProfileStore == null) {
                    mProfileStore =
                            BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                    new SupportLibProfileStore(ProfileStore.getInstance()));
                }

                return mProfileStore;
            }
        }
    }

    @Override
    public void startUpWebView(
            /* WebViewStartUpConfig */ InvocationHandler configInvoHandler,
            /* WebViewStartUpCallback */ InvocationHandler callbackInvoHandler) {
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.AndroidX.START_UP_WEBVIEW")) {
            recordApiCall(ApiCall.START_UP_WEBVIEW);
            final WebViewStartUpConfigBoundaryInterface webViewStartUpConfig =
                    BoundaryInterfaceReflectionUtil.castToSuppLibClass(
                            WebViewStartUpConfigBoundaryInterface.class, configInvoHandler);
            final WebViewStartUpCallbackBoundaryInterface webViewStartUpCallback =
                    BoundaryInterfaceReflectionUtil.castToSuppLibClass(
                            WebViewStartUpCallbackBoundaryInterface.class, callbackInvoHandler);
            WebViewChromiumAwInit.WebViewStartUpCallback callback =
                    result -> {
                        SupportLibStartUpResult supportLibResult = new SupportLibStartUpResult();
                        supportLibResult.setTotalTimeInUiThreadMillis(
                                result.getTotalTimeUiThreadChromiumInitMillis());
                        supportLibResult.setMaxTimePerTaskInUiThreadMillis(
                                result.getMaxTimePerTaskUiThreadChromiumInitMillis());
                        Throwable syncChromiumInitLocation =
                                result.getSynchronousChromiumInitLocationOrNull();
                        if (syncChromiumInitLocation != null) {
                            supportLibResult.addBlockingStartUpLocation(syncChromiumInitLocation);
                        }
                        Throwable providerInitOnMainLooperLocation =
                                result.getProviderInitOnMainLooperLocationOrNull();
                        if (providerInitOnMainLooperLocation != null) {
                            supportLibResult.addBlockingStartUpLocation(
                                    providerInitOnMainLooperLocation);
                        }
                        Throwable asyncChromiumInitLocation =
                                result.getAsynchronousChromiumInitLocationOrNull();
                        if (asyncChromiumInitLocation != null) {
                            supportLibResult.addAsyncStartUpLocation(asyncChromiumInitLocation);
                        }
                        webViewStartUpCallback.onSuccess(
                                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                        supportLibResult));
                    };

            mAwInit.startUpWebView(
                    callback,
                    webViewStartUpConfig.shouldRunUiThreadStartUpTasks(),
                    getProfilesToLoad(webViewStartUpConfig));
        }
    }

    // TODO(crbug.com/431984603): Remove and use the get method directly when any breaking change is
    // done to startUpWebView API.
    @Nullable
    private static Set<String> getProfilesToLoad(
            WebViewStartUpConfigBoundaryInterface webViewStartUpConfig) {
        Set<String> profilesToLoad = null;
        try {
            profilesToLoad = webViewStartUpConfig.getProfileNamesToLoad();
        } catch (RuntimeException e) {
            // This is an ugly fix to make sure that we don't crash with older BoundaryInterface in
            // AndroidX older versions. We should ideally fix that by making the
            // WebViewStartUpCallbackBoundaryInterface implements FeatureFlagHolderBoundaryInterface
            // or convert the Proxy way to use BiConsumer similar to WebViewBuilder.
        }
        return profilesToLoad;
    }
}
