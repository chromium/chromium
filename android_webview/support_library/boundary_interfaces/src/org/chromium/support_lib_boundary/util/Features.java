// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary.util;

/**
 * Class containing all the features the support library can support.
 * This class lives in the boundary interface directory so that the Android Support Library and
 * Chromium can share its definition.
 */
public class Features {
    // Features suffixed with DEV will only be visible on debug devices.
    public static final String DEV_SUFFIX = ":dev";

    // This class just contains constants representing features.
    private Features() {}

    // WebViewCompat#postVisualStateCallback
    // WebViewClientCompat#onPageCommitVisible
    public static final String VISUAL_STATE_CALLBACK = "VISUAL_STATE_CALLBACK";

    // WebViewClientCompat#onReceivedError(WebView, WebResourceRequest, WebResourceError)
    public static final String RECEIVE_WEB_RESOURCE_ERROR = "RECEIVE_WEB_RESOURCE_ERROR";

    // WebViewClientCompat#onReceivedHttpError
    public static final String RECEIVE_HTTP_ERROR = "RECEIVE_HTTP_ERROR";

    // WebViewClientCompat#onSafeBrowsingHit
    public static final String SAFE_BROWSING_HIT = "SAFE_BROWSING_HIT";

    // WebViewClientCompat#shouldOverrideUrlLoading
    public static final String SHOULD_OVERRIDE_WITH_REDIRECTS = "SHOULD_OVERRIDE_WITH_REDIRECTS";

    // WebSettingsCompat.getOffscreenPreRaster
    // WebSettingsCompat.setOffscreenPreRaster
    public static final String OFF_SCREEN_PRERASTER = "OFF_SCREEN_PRERASTER";

    // WebSettingsCompat.getSafeBrowsingEnabled
    // WebSettingsCompat.setSafeBrowsingEnabled
    public static final String SAFE_BROWSING_ENABLE = "SAFE_BROWSING_ENABLE";

    // WebSettingsCompat.getDisabledActionModeMenuItems
    // WebSettingsCompat.setDisabledActionModeMenuItems
    public static final String DISABLED_ACTION_MODE_MENU_ITEMS = "DISABLED_ACTION_MODE_MENU_ITEMS";

    // WebViewCompat.startSafeBrowsing
    public static final String START_SAFE_BROWSING = "START_SAFE_BROWSING";

    // WebViewCompat.setSafeBrowsingAllowlist
    public static final String SAFE_BROWSING_ALLOWLIST = "SAFE_BROWSING_ALLOWLIST";

    // WebViewCompat.setSafeBrowsingWhitelist
    public static final String SAFE_BROWSING_WHITELIST = "SAFE_BROWSING_WHITELIST";

    // WebViewCompat.getSafeBrowsingPrivacyPolicyUrl
    public static final String SAFE_BROWSING_PRIVACY_POLICY_URL =
            "SAFE_BROWSING_PRIVACY_POLICY_URL";

    // ServiceWorkerControllerCompat.getInstance
    // ServiceWorkerControllerCompat.getServiceWorkerWebSettings
    // ServiceWorkerControllerCompat.setServiceWorkerClient
    public static final String SERVICE_WORKER_BASIC_USAGE = "SERVICE_WORKER_BASIC_USAGE";

    // ServiceWorkerClientCompat.shouldInterceptRequest
    public static final String SERVICE_WORKER_SHOULD_INTERCEPT_REQUEST =
            "SERVICE_WORKER_SHOULD_INTERCEPT_REQUEST";

    // ServiceWorkerWebSettingsCompat.getCacheMode
    // ServiceWorkerWebSettingsCompat.setCacheMode
    public static final String SERVICE_WORKER_CACHE_MODE = "SERVICE_WORKER_CACHE_MODE";

    // ServiceWorkerWebSettingsCompat.getAllowContentAccess
    // ServiceWorkerWebSettingsCompat.setAllowContentAccess
    public static final String SERVICE_WORKER_CONTENT_ACCESS = "SERVICE_WORKER_CONTENT_ACCESS";

    // ServiceWorkerWebSettingsCompat.getAllowFileAccess
    // ServiceWorkerWebSettingsCompat.setAllowFileAccess
    public static final String SERVICE_WORKER_FILE_ACCESS = "SERVICE_WORKER_FILE_ACCESS";

    // ServiceWorkerWebSettingsCompat.getBlockNetworkLoads
    // ServiceWorkerWebSettingsCompat.setBlockNetworkLoads
    public static final String SERVICE_WORKER_BLOCK_NETWORK_LOADS =
            "SERVICE_WORKER_BLOCK_NETWORK_LOADS";

    // WebResourceRequest.isRedirect
    public static final String WEB_RESOURCE_REQUEST_IS_REDIRECT =
            "WEB_RESOURCE_REQUEST_IS_REDIRECT";

    // WebResourceError.getDescription
    public static final String WEB_RESOURCE_ERROR_GET_DESCRIPTION =
            "WEB_RESOURCE_ERROR_GET_DESCRIPTION";

    // WebResourceError.getErrorCode
    public static final String WEB_RESOURCE_ERROR_GET_CODE = "WEB_RESOURCE_ERROR_GET_CODE";

    // SafeBrowsingResponse.backToSafety
    public static final String SAFE_BROWSING_RESPONSE_BACK_TO_SAFETY =
            "SAFE_BROWSING_RESPONSE_BACK_TO_SAFETY";

    // SafeBrowsingResponse.proceed
    public static final String SAFE_BROWSING_RESPONSE_PROCEED = "SAFE_BROWSING_RESPONSE_PROCEED";

    // SafeBrowsingResponse.showInterstitial
    public static final String SAFE_BROWSING_RESPONSE_SHOW_INTERSTITIAL =
            "SAFE_BROWSING_RESPONSE_SHOW_INTERSTITIAL";

    /** @deprecated Feature was renamed to WEB_MESSAGE_ARRAY_BUFFER. Do not reuse feature name. */
    @Deprecated()
    public static final String WEB_MESSAGE_GET_MESSAGE_PAYLOAD = "WEB_MESSAGE_GET_MESSAGE_PAYLOAD";

    // JsReplyProxy.postMessageWithPayload
    // WebMessage.getMessagePayload
    // WebMessagePayload.getAsArrayBuffer
    // WebMessagePayload.getAsString
    // WebMessagePayload.getType
    public static final String WEB_MESSAGE_ARRAY_BUFFER = "WEB_MESSAGE_ARRAY_BUFFER";

    // WebMessagePortCompat.postMessage
    public static final String WEB_MESSAGE_PORT_POST_MESSAGE = "WEB_MESSAGE_PORT_POST_MESSAGE";

    // WebMessagePortCompat.close
    public static final String WEB_MESSAGE_PORT_CLOSE = "WEB_MESSAGE_PORT_CLOSE";

    // WebMessagePortCompat.setWebMessageCallback(WebMessageCallbackCompat)
    // WebMessagePortCompat.setWebMessageCallback(WebMessageCallbackCompat, Handler)
    public static final String WEB_MESSAGE_PORT_SET_MESSAGE_CALLBACK =
            "WEB_MESSAGE_PORT_SET_MESSAGE_CALLBACK";

    // WebViewCompat.createWebMessageChannel
    public static final String CREATE_WEB_MESSAGE_CHANNEL = "CREATE_WEB_MESSAGE_CHANNEL";

    // WebViewCompat.postWebMessage
    public static final String POST_WEB_MESSAGE = "POST_WEB_MESSAGE";

    // WebMessageCallbackCompat.onMessage
    public static final String WEB_MESSAGE_CALLBACK_ON_MESSAGE = "WEB_MESSAGE_CALLBACK_ON_MESSAGE";

    // WebViewCompat.getWebViewClient
    public static final String GET_WEB_VIEW_CLIENT = "GET_WEB_VIEW_CLIENT";

    // WebViewCompat.getWebChromeClient
    public static final String GET_WEB_CHROME_CLIENT = "GET_WEB_CHROME_CLIENT";

    // ProxyController.setProxyOverride
    // ProxyController.clearProxyOverride
    public static final String PROXY_OVERRIDE = "PROXY_OVERRIDE:3";

    // ProxyController.setProxyOverride
    public static final String PROXY_OVERRIDE_REVERSE_BYPASS = "PROXY_OVERRIDE_REVERSE_BYPASS";

    // WebSettingsCompat.setWillSuppressErrorPage
    // WebSettingsCompat.getWillSuppressErrorPage
    public static final String SUPPRESS_ERROR_PAGE = "SUPPRESS_ERROR_PAGE";

    // WebViewCompat.getWebViewRenderer
    public static final String GET_WEB_VIEW_RENDERER = "GET_WEB_VIEW_RENDERER";

    // WebViewRenderer.terminate
    public static final String WEB_VIEW_RENDERER_TERMINATE = "WEB_VIEW_RENDERER_TERMINATE";

    // TracingController.getInstance
    // TracingController.isTracing
    // TracingController.start
    // TracingController.stop
    public static final String TRACING_CONTROLLER_BASIC_USAGE = "TRACING_CONTROLLER_BASIC_USAGE";

    // Renderer client set/getter and callbacks:
    //
    // WebView.getWebViewRendererClient
    // WebView.setWebViewRendererClient
    // WebViewRendererClient.onRendererUnresponsive()
    // WebViewRendererClient.onRendererResponsive()
    public static final String WEB_VIEW_RENDERER_CLIENT_BASIC_USAGE =
            "WEB_VIEW_RENDERER_CLIENT_BASIC_USAGE";

    // WebViewCompat.isMultiProcessEnabled
    public static final String MULTI_PROCESS_QUERY = "MULTI_PROCESS_QUERY";

    // WebSettingsCompat.setForceDark
    // WebSettingsCompat.getForceDark
    public static final String FORCE_DARK = "FORCE_DARK";

    // Preferences between force dark and media query for dark theme support:
    //
    // WebSettingsCompat.setForceDarkBehavior
    // WebSettingsCompat.getForceDarkBehavior
    public static final String FORCE_DARK_BEHAVIOR = "FORCE_DARK_BEHAVIOR";

    // WebSettingsCompat.setAlgorithmicDarkeningAllowed
    // WebSettingsCompat.isAlgorithmicDarkeningAllowed
    public static final String ALGORITHMIC_DARKENING = "ALGORITHMIC_DARKENING";

    // WebViewCompat.addWebMessageListener
    // WebViewCompat.removeWebMessageListener
    public static final String WEB_MESSAGE_LISTENER = "WEB_MESSAGE_LISTENER";

    // WebViewCompat.addDocumentStartJavascript
    public static final String DOCUMENT_START_SCRIPT = "DOCUMENT_START_SCRIPT:1";

    // WebSettingsCompat.setWebAuthnSupport
    // WebSettingsCompat.getWebAuthnSupport
    public static final String WEB_AUTHENTICATION = "WEB_AUTHENTICATION";

    // WebSettingsCompat.setRequestedWithHeaderMode
    // WebSettingsCompat.getRequestedWithHeaderMode
    // ServiceWorkerWebSettingsCompat.setRequestedWithHeaderMode
    // ServiceWorkerWebSettingsCompat.getRequestedWithHeaderMode
    /** @deprecated Feature was never launched. Do not reuse feature name. */
    @Deprecated()
    public static final String REQUESTED_WITH_HEADER_CONTROL = "REQUESTED_WITH_HEADER_CONTROL";

    // WebSettingsCompat.setRequestedWithHeaderAllowList
    // WebSettingsCompat.getRequestedWithHeaderAllowList
    // ServiceWorkerWebSettingsCompat.setRequestedWithHeaderAllowList
    // ServiceWorkerWebSettingsCompat.getRequestedWithHeaderAllowList
    public static final String REQUESTED_WITH_HEADER_ALLOW_LIST =
            "REQUESTED_WITH_HEADER_ALLOW_LIST";

    // WebViewCompat.getVariationsHeader
    public static final String GET_VARIATIONS_HEADER = "GET_VARIATIONS_HEADER";

    // WebSettingsCompat.setEnterpriseAuthenticationAppLinkPolicyEnabled
    // WebSettingsCompat.getEnterpriseAuthenticationAppLinkPolicyEnabled
    public static final String ENTERPRISE_AUTHENTICATION_APP_LINK_POLICY =
            "ENTERPRISE_AUTHENTICATION_APP_LINK_POLICY";

    // CookieManagerCompat.getCookieInfo
    public static final String GET_COOKIE_INFO = "GET_COOKIE_INFO";

    // DropDataContentProvider.onCreate
    // DropDataContentProvider.getStreamTypes
    // DropDataContentProvider.openFile
    // DropDataContentProvider.query
    // DropDataContentProvider.getType
    // DropDataContentProvider.cache
    // DropDataContentProvider.setClearCachedDataIntervalMs
    // DropDataContentProvider.onDragEnd
    // DropDataContentProvider.call
    public static final String IMAGE_DRAG_DROP = "IMAGE_DRAG_DROP";

    // ProfileStore.getInstance
    // ProfileStore.getOrCreateProfile
    // ProfileStore.getProfile
    // ProfileStore.getAllProfileNames
    // ProfileStore.deleteProfile
    // Profile.getName
    // Profile.getCookieManager
    // Profile.getWebStorage
    // Profile.getGeolocationPermissions
    // Profile.getServiceWorkerController
    public static final String MULTI_PROFILE = "MULTI_PROFILE";

    // WebSettingsCompat.enableRestrictSensitiveWebContent
    @Deprecated()
    public static final String RESTRICT_SENSITIVE_WEB_CONTENT = "RESTRICT_SENSITIVE_WEB_CONTENT";

    // WebSettingsCompat.setUserAgentMetadataFromMap
    // WebSettingsCompat.getUserAgentMetadataMap
    public static final String USER_AGENT_METADATA = "USER_AGENT_METADATA";

    // WebSettingsCompat.setAttributionBehavior
    // WebSettingsCompat.getAttributionBehavior
    public static final String ATTRIBUTION_BEHAVIOR = "ATTRIBUTION_BEHAVIOR";

    // WebSettingsCompat.setWebViewMediaIntegrityApiStatus
    // WebSettingsCompat.getWebViewMediaIntegrityApiDefaultStatus
    // WebSettingsCompat.getWebViewMediaIntegrityApiOverrideRules
    public static final String WEBVIEW_MEDIA_INTEGRITY_API_STATUS = "WEBVIEW_INTEGRITY_API_STATUS";

    // WebViewCompat.setAudioMuted
    // WebViewCompat.isAudioMuted
    public static final String MUTE_AUDIO = "MUTE_AUDIO";

    // WebSettingsCompat.setSpeculativeLoadingStatus
    // WebSettingsCompat.getSpeculativeLoadingStatus
    public static final String SPECULATIVE_LOADING = "SPECULATIVE_LOADING";

    // WebSettingsCompat.setBackForwardCacheEnabled
    // WebSettingsCompat.getBackForwardCacheEnabled
    public static final String BACK_FORWARD_CACHE = "BACK_FORWARD_CACHE";

    // Profile.prefetchUrl
    // Profile.clearPrefetchCache
    public static final String PREFETCH_WITH_URL = "PREFETCH_URL";
}
