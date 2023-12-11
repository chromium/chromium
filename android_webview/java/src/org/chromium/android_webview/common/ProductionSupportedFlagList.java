// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

import org.chromium.base.BaseFeatures;
import org.chromium.base.BaseSwitches;
import org.chromium.blink_public.common.BlinkFeatures;
import org.chromium.blink_scheduler.BlinkSchedulerFeatures;
import org.chromium.cc.base.CcFeatures;
import org.chromium.cc.base.CcSwitches;
import org.chromium.components.autofill.AndroidAutofillFeatures;
import org.chromium.components.autofill.AutofillFeatures;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.gwp_asan.GwpAsanFeatures;
import org.chromium.components.metrics.AndroidMetricsFeatures;
import org.chromium.components.metrics.MetricsFeatures;
import org.chromium.components.metrics.MetricsSwitches;
import org.chromium.components.network_session_configurator.NetworkSessionSwitches;
import org.chromium.components.permissions.PermissionsAndroidFeatureList;
import org.chromium.components.safe_browsing.SafeBrowsingFeatures;
import org.chromium.components.variations.VariationsSwitches;
import org.chromium.components.viz.common.VizFeatures;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.gpu.config.GpuFeatures;
import org.chromium.gpu.config.GpuSwitches;
import org.chromium.net.NetFeatures;
import org.chromium.services.network.NetworkServiceFeatures;
import org.chromium.services.tracing.TracingServiceFeatures;
import org.chromium.ui.accessibility.AccessibilityFeatures;
import org.chromium.ui.base.UiAndroidFeatures;

/**
 * List of experimental features/flags supported for user devices. Add features/flags to this list
 * with scrutiny: any feature/flag in this list can be enabled for production Android devices, and
 * so it must not compromise the Android security model (i.e., WebView must still protect the app's
 * private data from being user visible).
 *
 * <p>This lives in the common package so it can be shared by dev UI (to know which features/flags
 * to display) as well as the WebView implementation (so it knows which features/flags are safe to
 * honor).
 */
public final class ProductionSupportedFlagList {
    // Do not instantiate this class.
    private ProductionSupportedFlagList() {}

    /**
     * A list of commandline flags supported on user devices. See
     * android_webview/docs/developer-ui.md for info about how this is used
     * (https://chromium.googlesource.com/chromium/src/+/HEAD/android_webview/docs/developer-ui.md#Adding-your-flags-and-features-to-the-UI).
     *
     * <p>See
     * https://chromium.googlesource.com/chromium/src/+/HEAD/tools/metrics/histograms/README.md#Flag-Histograms
     * for more info about flag labels if you want histogram data about usage. This involves
     * updating the "LoginCustomFlags" field in tools/metrics/histograms/enums.xml.
     */
    public static final Flag[] sFlagList = {
        Flag.commandLine(
                AwSwitches.HIGHLIGHT_ALL_WEBVIEWS,
                "Highlight the contents (including web contents) of all WebViews with a yellow "
                        + "tint. This is useful for identifying WebViews in an Android "
                        + "application."),
        Flag.commandLine(
                AwSwitches.WEBVIEW_VERBOSE_LOGGING,
                "WebView will log additional debugging information to logcat, such as "
                        + "variations and commandline state."),
        Flag.commandLine(
                CcSwitches.SHOW_COMPOSITED_LAYER_BORDERS,
                "Renders a border around compositor layers to help debug and study layer "
                        + "compositing."),
        Flag.commandLine(
                CcSwitches.ANIMATED_IMAGE_RESUME, "Resumes animated images from where they were."),
        Flag.commandLine(
                AwSwitches.FINCH_SEED_EXPIRATION_AGE,
                "Forces all variations seeds to be considered stale.",
                "0"),
        Flag.commandLine(
                AwSwitches.FINCH_SEED_IGNORE_PENDING_DOWNLOAD,
                "Forces the WebView service to reschedule a variations seed download job even "
                        + "if one is already pending."),
        Flag.commandLine(
                AwSwitches.FINCH_SEED_NO_CHARGING_REQUIREMENT,
                "Forces WebView's service to always schedule a new variations seed download "
                        + "job, even if the device is not charging. Note this switch may be "
                        + "necessary for testing on Android emulators as these are not always "
                        + "considered to be charging."),
        Flag.commandLine(
                AwSwitches.FINCH_SEED_MIN_DOWNLOAD_PERIOD,
                "Disables throttling of variations seed download jobs.",
                "0"),
        Flag.commandLine(
                AwSwitches.FINCH_SEED_MIN_UPDATE_PERIOD,
                "Disables throttling of new variations seed requests to the WebView service.",
                "0"),
        Flag.commandLine(
                MetricsSwitches.FORCE_ENABLE_METRICS_REPORTING,
                "Forces WebView's metrics reporting to be enabled. This overrides user "
                        + "settings and capacity sampling, but does not override the app's "
                        + "choice to opt-out."),
        Flag.commandLine(
                AwSwitches.WEBVIEW_LOG_JS_CONSOLE_MESSAGES,
                "Mirrors JavaScript console messages to system logs."),
        Flag.commandLine(
                BaseSwitches.ENABLE_CRASH_REPORTER_FOR_TESTING,
                "Used for turning on Breakpad crash reporting in a debug environment where "
                        + "crash reporting is typically compiled but disabled."),
        Flag.commandLine(
                GpuSwitches.DISABLE_GPU_RASTERIZATION,
                "Disables GPU rasterization, i.e. rasterizes on the CPU only."),
        Flag.commandLine(
                GpuSwitches.IGNORE_GPU_BLOCKLIST,
                "Overrides the built-in software rendering list and enables "
                        + "GPU acceleration on unsupported device configurations."),
        Flag.commandLine(
                AwSwitches.WEBVIEW_ENABLE_MODERN_COOKIE_SAME_SITE,
                "Enables modern SameSite cookie behavior: 1) SameSite=Lax by default "
                        + "(cookies without a SameSite attribute are treated as SameSite=Lax); "
                        + "2) Schemeful Same-Site (site boundaries include the URL scheme)."),
        Flag.commandLine(
                ContentSwitches.SITE_PER_PROCESS,
                "Security mode that enables site isolation for all sites inside WebView. In "
                        + "this mode, each renderer process will contain pages from at most "
                        + "one site, using out-of-process iframes when needed. Highly "
                        + "experimental."),
        Flag.commandLine(
                NetworkSessionSwitches.ENABLE_HTTP2_GREASE_SETTINGS,
                "Enable sending HTTP/2 SETTINGS parameters with reserved identifiers."),
        Flag.commandLine(
                NetworkSessionSwitches.DISABLE_HTTP2_GREASE_SETTINGS,
                "Disable sending HTTP/2 SETTINGS parameters with reserved identifiers."),
        Flag.commandLine(
                VariationsSwitches.ENABLE_FINCH_SEED_DELTA_COMPRESSION,
                "Enables delta-compression when requesting a new seed from the server."),
        Flag.commandLine(
                AwSwitches.WEBVIEW_FENCED_FRAMES,
                "Enables fenced frames. Also implies SharedStorageAPI, "
                        + "and PrivacySandboxAdsAPIsOverride"),
        Flag.commandLine(
                AwSwitches.WEBVIEW_DISABLE_APP_RECOVERY,
                "Disables WebView from checking for app recovery mitigations."),
        Flag.commandLine(
                AwSwitches.WEBVIEW_ENABLE_APP_RECOVERY,
                "Enables WebView to check for app recovery mitigations."),
        Flag.baseFeature(
                BlinkFeatures.USER_AGENT_CLIENT_HINT,
                "Enables user-agent client hints in WebView."),
        Flag.baseFeature(
                "DefaultPassthroughCommandDecoder", "Use the passthrough GLES2 command decoder."),
        Flag.baseFeature(
                GpuFeatures.WEBVIEW_SURFACE_CONTROL,
                "Use SurfaceControl. Requires WebViewThreadSafeMedia and Android device and OS "
                        + "support."),
        Flag.baseFeature(
                GpuFeatures.WEBVIEW_THREAD_SAFE_MEDIA,
                "Use thread-safe media path, requires Android P."),
        Flag.baseFeature(
                GpuFeatures.AGGRESSIVE_SKIA_GPU_RESOURCE_PURGE,
                "More aggressively purge skia GPU resources"),
        Flag.baseFeature(
                VizFeatures.ALLOW_BYPASS_RENDER_PASS_QUADS,
                "Enable bypass render pass for RenderPassDrawQuads"),
        Flag.baseFeature(
                VizFeatures.WEBVIEW_NEW_INVALIDATE_HEURISTIC,
                "More robust heuristic for calling Invalidate"),
        Flag.baseFeature(VizFeatures.WEBVIEW_VULKAN_INTERMEDIATE_BUFFER, "For debugging vulkan"),
        Flag.baseFeature(
                VizFeatures.ALLOW_UNDAMAGED_NONROOT_RENDER_PASS_TO_SKIP,
                "Enable optimization for skipping undamaged nonroot render passes."),
        Flag.baseFeature(
                VizFeatures.DRAW_IMMEDIATELY_WHEN_INTERACTIVE,
                "Enable optimization for immediate activation and draw when interactive."),
        Flag.baseFeature(GpuFeatures.USE_GLES2_FOR_OOP_R, "Force Skia context to use es2 only."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_CONNECTIONLESS_SAFE_BROWSING,
                "Uses GooglePlayService's 'connectionless' APIs for Safe Browsing "
                        + "security checks."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_BROTLI_SUPPORT,
                "Enables brotli compression support in WebView."),
        Flag.baseFeature(NetFeatures.PRIORITY_HEADER, "Enables the HTTP priority header."),
        Flag.baseFeature(
                NetFeatures.ZSTD_CONTENT_ENCODING,
                "Enables zstd content-encoding support in the browser."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_EXIT_REASON_METRIC, "Records various system exit reasons"),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_EXTRA_HEADERS_SAME_ORIGIN_ONLY,
                "Only allow extra headers added via loadUrl() to be sent to the same origin "
                        + "as the original request."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_DISPLAY_CUTOUT,
                "Enables display cutout (notch) support in WebView for Android P and above."),
        Flag.baseFeature(
                BlinkFeatures.WEBVIEW_ACCELERATE_SMALL_CANVASES,
                "Accelerate all canvases in webview."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_MIXED_CONTENT_AUTOUPGRADES,
                "Enables autoupgrades for audio/video/image mixed content when mixed content "
                        + "mode is set to MIXED_CONTENT_COMPATIBILITY_MODE"),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_JAVA_JS_BRIDGE_MOJO,
                "Enables the new Java/JS Bridge code path with mojo implementation."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_ORIGIN_TRIALS, "Enables Origin Trials support on WebView."),
        Flag.baseFeature(
                BlinkFeatures.GMS_CORE_EMOJI,
                "Enables retrieval of the emoji font through GMS Core "
                        + "improving emoji glyph coverage."),
        Flag.baseFeature(
                AndroidAutofillFeatures.ANDROID_AUTOFILL_BOTTOM_SHEET_WORKAROUND_NAME,
                "Enable the workaround for autofill bottom sheet platform bug."),
        Flag.baseFeature(
                AndroidAutofillFeatures.ANDROID_AUTOFILL_FORM_SUBMISSION_CHECK_BY_ID_NAME,
                "When enabled, form submissions are reported to AutofillManager iff the form "
                        + "global ids match."),
        Flag.baseFeature(
                AndroidAutofillFeatures.ANDROID_AUTOFILL_PREFILL_REQUESTS_FOR_LOGIN_FORMS_NAME,
                "When enabled, prefill requests are supported for login forms."),
        Flag.baseFeature(
                AndroidAutofillFeatures.ANDROID_AUTOFILL_SUPPORT_VISIBILITY_CHANGES_NAME,
                "Enables communicating visibility changes of form fields of a form in an "
                        + "ongoing Autofill session to Android AutofillManager."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_ENABLE_DEPENDENT_LOCALITY_PARSING,
                "Enables parsing dependent locality fields (e.g. Bairros in Brazil)."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_ENABLE_EXPIRATION_DATE_IMPROVEMENTS,
                "Enables various improvements to handling expiration dates."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_ENABLE_SELECT_LIST,
                "Enables autofill of <selectlist> elements."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_ENABLE_SUPPORT_FOR_PHONE_NUMBER_TRUNK_TYPES,
                "Rationalizes city-and-number and city-code fields to the "
                        + "correct trunk-prefix types."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_DETECT_REMOVED_FORM_CONTROLS,
                "Enables Autofill to detect if form controls are removed from the DOM"),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_DONT_PRESERVE_AUTOFILL_STATE,
                "Retrieves is_autofilled state from blink instead of the cache"),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_PARSE_ASYNC,
                "Parse forms asynchronously outside of the UI thread."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_PARSING_PATTERN_PROVIDER,
                "Enables Autofill to use its new method to retrieve parsing patterns."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_PAGE_LANGUAGE_DETECTION,
                "Enables Autofill to retrieve the page language for form parsing."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_PREFER_LABELS_IN_SOME_COUNTRIES,
                "When enabled, Autofill will first look at field labels and then at field "
                        + "attributes when classifying address fields in Mexico."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_ALWAYS_PARSE_PLACEHOLDERS,
                "When enabled, Autofill local heuristics consider the placeholder attribute "
                        + "for determining field types."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_SERVER_BEHAVIORS,
                "When enabled, Autofill will request experimental "
                        + "predictions from the Autofill API."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_ENABLE_SUPPORT_FOR_BETWEEN_STREETS,
                "When enabled, Autofill supports between streets fields."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_ENABLE_SUPPORT_FOR_ADMIN_LEVEL2,
                "When enabled, Autofill supports admin-level2 fields."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_ENABLE_SUPPORT_FOR_ADDRESS_OVERFLOW,
                "When enabled, Autofill supports overflow fields."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_ENABLE_SUPPORT_FOR_ADDRESS_OVERFLOW_AND_LANDMARK,
                "When enabled, Autofill supports overflow and landmark fields."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_ENABLE_SUPPORT_FOR_BETWEEN_STREETS_OR_LANDMARK,
                "When enabled, Autofill supports between streets or landmark fields."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_ENABLE_SUPPORT_FOR_LANDMARK,
                "When enabled, Autofill supports landmark fields."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_ENABLE_PARSING_OF_STREET_LOCATION,
                "When enabled, Autofill supports parsing fields as street locations."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_ENABLE_RATIONALIZATION_ENGINE_FOR_MX,
                "When enabled, Autofill performs Mexico specific rationalization."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_ENABLE_SUPPORT_FOR_APARTMENT_NUMBERS,
                "When enabled, Autofill supports apartment number fields."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_USE_I18N_ADDRESS_MODEL,
                "When enabled, Autofill uses the i18n version of the address model."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_STREET_NAME_OR_HOUSE_NUMBER_PRECEDENCE_OVER_AUTOCOMPLETE,
                "When enabled, Autofill prioritizes local heuristics over some server "
                        + "classifications."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_ENABLE_ZIP_ONLY_ADDRESS_FORMS,
                "When enabled, Autofill supports forms consisting of only zip code fields."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_DEFAULT_TO_CITY_AND_NUMBER,
                "When enabled, Autofill heuristics will prioritize filling phone numbers in "
                        + "local format, not in international format."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_LOCAL_HEURISTICS_OVERRIDES,
                "When enabled, When enabled, some local heuristic predictions will take "
                        + "precedence over the autocomplete attribute and server predictions, "
                        + "when determining a field's overall type."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_ENABLE_EMAIL_HEURISTIC_ONLY_ADDRESS_FORMS,
                "When enabled, Autofill supports forms consisting of only email fields."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_TEXT_AREA_CHANGE_EVENTS,
                "When enabled, autofill responds to textarea change events."),
        Flag.baseFeature(
                FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_VIRTUAL_CARD_FEATURE,
                "When enabled, merchant bound virtual cards will be offered in the keyboard "
                        + "accessory."),
        Flag.baseFeature(
                NetworkServiceFeatures.PRIVATE_STATE_TOKENS,
                "Enables the prototype Private State Tokens API."),
        Flag.baseFeature(
                NetworkServiceFeatures.COOKIE_ACCESS_DETAILS_NOTIFICATION_DE_DUPING,
                "Enables de-duplicating cookie access details that are sent to observers via"
                        + " OnCookiesAccessed."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_EMPTY_COMPONENT_LOADER_POLICY,
                "Enables loading a fake empty (no-op) component during WebView startup."),
        Flag.commandLine(
                AwSwitches.WEBVIEW_SELECTIVE_IMAGE_INVERSION_DARKENING,
                "Enables use selective image inversion to automatically darken page, it will be"
                        + " used when WebView is in dark mode, but website doesn't provide dark"
                        + " style."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_FORCE_DARK_MODE_MATCH_THEME,
                "Automatically darken page if"
                        + " WebView is set to FORCE_DARK_AUTO and the app has dark theme"),
        Flag.baseFeature(
                ContentFeatures.VERIFY_DID_COMMIT_PARAMS,
                "Enables reporting of browser and renderer navigation inconsistencies on"
                        + "navigations"),
        Flag.baseFeature(
                ContentFeatures.USER_MEDIA_CAPTURE_ON_FOCUS,
                "Enables GetUserMedia API will only resolve when the document calling it has"
                        + "focus"),
        Flag.baseFeature(
                ContentFeatures.COMPOSITE_BG_COLOR_ANIMATION,
                "When enabled, the background-color animation runs on the compositor thread."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_USE_METRICS_UPLOAD_SERVICE,
                "Upload UMA metrics logs through MetricsUploadService not via GMS-core"
                        + " directly."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_USE_METRICS_UPLOAD_SERVICE_ONLY_SDK_RUNTIME,
                "Upload UMA metrics logs through MetricsUploadService not via GMS-core"
                        + " directly when running within the SDK Runtime."),
        Flag.baseFeature(
                AndroidMetricsFeatures.ANDROID_METRICS_ASYNC_METRIC_LOGGING,
                "Initiate metric uploading on a background thread."),
        Flag.baseFeature(
                BlinkFeatures.SET_TIMEOUT_WITHOUT_CLAMP,
                "Enables faster setTimeout(,0) by removing the 1 ms clamping."),
        Flag.baseFeature(
                BlinkFeatures.PAINT_HOLDING_CROSS_ORIGIN,
                "Defers the first commit until FCP or timeout for cross-origin navigations."),
        Flag.baseFeature(
                ContentFeatures.NAVIGATION_NETWORK_RESPONSE_QUEUE,
                "Schedules tasks related to the navigation network responses on a higher "
                        + "priority task queue."),
        Flag.baseFeature(
                ContentFeatures.EARLY_ESTABLISH_GPU_CHANNEL,
                "Enable establishing the GPU channel early in renderer startup."),
        Flag.baseFeature(
                ContentFeatures.GIN_JAVA_BRIDGE_MOJO,
                "Enable the mojo based GIN java bridge implementation."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_X_REQUESTED_WITH_HEADER_CONTROL,
                "Restricts insertion of XRequestedWith header on outgoing requests "
                        + "to those that have been allow-listed through the appropriate "
                        + "developer API."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_X_REQUESTED_WITH_HEADER_MANIFEST_ALLOW_LIST,
                "Enables support for providing an allow-list for the X-Requested-Header "
                        + "through AndroidManifest.xml meta-data."),
        Flag.baseFeature(
                BlinkFeatures.VIEWPORT_HEIGHT_CLIENT_HINT_HEADER,
                "Enables the use of sec-ch-viewport-height client hint."),
        Flag.baseFeature(
                GpuFeatures.CANVAS_CONTEXT_LOST_IN_BACKGROUND,
                "Free Canvas2D resources when the webview is in the background."),
        Flag.baseFeature(
                GpuFeatures.USE_CLIENT_GMB_INTERFACE,
                "Uses the ClientGmbInetrface to create GpuMemoryBuffers for Renderers."),
        Flag.baseFeature(
                GpuFeatures.USE_GPU_SCHEDULER_DFS, "Uses the new SchedulerDFS GPU job scheduler."),
        Flag.baseFeature(
                BlinkFeatures.UACH_OVERRIDE_BLANK,
                "Changes behavior of User-Agent Client Hints to send blank headers "
                        + "when the User-Agent string is overriden"),
        Flag.baseFeature(
                BlinkFeatures.MAX_UNTHROTTLED_TIMEOUT_NESTING_LEVEL,
                "Increases the nesting threshold before which "
                        + "setTimeout(..., <4ms) starts being clamped to 4 ms."),
        Flag.baseFeature(
                BlinkFeatures.ESTABLISH_GPU_CHANNEL_ASYNC,
                "Enables establishing the GPU channel asnchronously when requesting a new "
                        + "layer tree frame sink."),
        Flag.baseFeature(
                BlinkFeatures.DECODE_SCRIPT_SOURCE_OFF_THREAD,
                "If enabled, script source text will be decoded and hashed off the main"
                        + "thread."),
        Flag.baseFeature(
                BaseFeatures.OPTIMIZE_DATA_URLS, "Optimizes parsing and loading of data: URLs."),
        Flag.baseFeature(
                BlinkFeatures.PREFETCH_FONT_LOOKUP_TABLES,
                "If enabled, font lookup tables will be prefetched on renderer startup."),
        Flag.baseFeature(
                BlinkFeatures.PRECOMPILE_INLINE_SCRIPTS,
                "If enabled, inline scripts will be stream compiled using a background HTML"
                        + " scanner."),
        Flag.baseFeature(
                BaseFeatures.RUN_TASKS_BY_BATCHES,
                "Run tasks in queue for 8ms before before sending a system message."),
        Flag.baseFeature(
                BlinkFeatures.DEPRECATE_UNLOAD,
                "If false prevents the gradual deprecation of the unload event."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_RECORD_APP_DATA_DIRECTORY_SIZE,
                "Record the size of the embedding app's data directory"),
        Flag.baseFeature(
                BlinkFeatures.EARLY_EXIT_ON_NOOP_CLASS_OR_STYLE_CHANGE,
                "Early exit when the style or class attribute of a DOM element is set to the"
                        + " same value as before."),
        Flag.baseFeature(
                BlinkFeatures.MAIN_THREAD_HIGH_PRIORITY_IMAGE_LOADING,
                "If enabled, image load tasks on visible pages have high priority."),
        Flag.baseFeature(
                BlinkFeatures.THREADED_PRELOAD_SCANNER,
                "If enabled, the HTMLPreloadScanner will run on a worker thread."),
        Flag.baseFeature(
                BlinkFeatures.TIMED_HTML_PARSER_BUDGET,
                "If enabled, the HTMLDocumentParser will use a budget based on elapsed time"
                        + " rather than token count."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_HIT_TEST_IN_BLINK_ON_TOUCH_START,
                "Hit test on touch start in blink"),
        Flag.baseFeature(BaseFeatures.ALIGN_WAKE_UPS, "Align delayed wake ups at 125 Hz"),
        Flag.baseFeature(
                BlinkSchedulerFeatures.THREADED_SCROLL_PREVENT_RENDERING_STARVATION,
                "Enable rendering starvation-prevention during threaded scrolling."
                        + " See https://crbug.com/1315279."),
        Flag.baseFeature(
                BlinkSchedulerFeatures.PRIORITIZE_COMPOSITING_AFTER_DELAY_TRIALS,
                "Controls the delay after which main thread compositing tasks "
                        + "are prioritized over other non-input tasks."),
        Flag.baseFeature(
                BlinkFeatures.VIEW_TRANSITION_ON_NAVIGATION,
                "Enables the experimental View Transitions API for navigations."
                        + " See https://github.com/WICG/view-transitions/blob/main/explainer.md."),
        Flag.baseFeature(
                GpuFeatures.INCREASED_CMD_BUFFER_PARSE_SLICE,
                "Enable the use of an increased parse slice size per command buffer before"
                        + " each forced context switch."),
        Flag.baseFeature(
                AccessibilityFeatures.ABLATE_SEND_PENDING_ACCESSIBILITY_EVENTS,
                "Enable to increase the cost of SendPendingAccessibilityEvents"),
        Flag.baseFeature(
                BlinkFeatures.RUN_TEXT_INPUT_UPDATE_POST_LIFECYCLE,
                "Runs code to update IME state at the end of a lifecycle update "
                        + "rather than the beginning."),
        Flag.baseFeature(
                CcFeatures.NON_BLOCKING_COMMIT,
                "Don't block the renderer main thread unconditionally while waiting "
                        + "for commit to finish on the compositor thread."),
        Flag.baseFeature(
                CcFeatures.USE_DMSAA_FOR_TILES,
                "Switches skia to use DMSAA instead of MSAA for tile raster"),
        Flag.baseFeature(
                CcFeatures.USE_DMSAA_FOR_TILES_ANDROID_GL,
                "Switches skia to use DMSAA instead of MSAA for tile raster"
                        + " on Android GL backend."),
        Flag.baseFeature(
                BlinkFeatures.CSS_SPELLING_GRAMMAR_ERRORS,
                "Enables new CSS spelling and grammar features"),
        Flag.baseFeature(
                BlinkFeatures.WEB_RTC_INITIALIZE_ENCODER_ON_FIRST_FRAME,
                "Initialize VideoEncodeAccelerator on the first encode."),
        Flag.baseFeature(
                BlinkFeatures.WEB_RTC_METRONOME,
                "Inject a metronome into webrtc to allow task coalescing, "
                        + " including synchronized decoding."),
        Flag.baseFeature(
                BlinkFeatures.THREADED_BODY_LOADER,
                "If enabled, reads and decodes navigation body data off the main thread."),
        Flag.baseFeature(BlinkFeatures.SPARSE_OBJECT_PAINT_PROPERTIES),
        Flag.baseFeature(BlinkFeatures.SVG_RASTER_OPTIMIZATIONS),
        Flag.baseFeature(BlinkFeatures.HIT_TEST_OPAQUENESS),
        Flag.baseFeature(BlinkFeatures.DYNAMIC_SCROLL_CULL_RECT_EXPANSION),
        Flag.baseFeature(BlinkFeatures.INTERSECTION_OPTIMIZATION),
        Flag.baseFeature(BlinkFeatures.SOLID_COLOR_LAYERS),
        Flag.baseFeature(BlinkFeatures.EXPAND_COMPOSITED_CULL_RECT),
        Flag.baseFeature(BlinkFeatures.SCROLLBAR_COLOR),
        Flag.baseFeature(BlinkFeatures.ONE_PASS_RASTER_INVALIDATION),
        Flag.baseFeature(
                ContentFeatures.SURFACE_SYNC_FULLSCREEN_KILLSWITCH,
                "Disable to turn off the new SurfaceSync Fullscreen path."),
        Flag.baseFeature(
                ContentFeatures.SYNCHRONOUS_COMPOSITOR_BACKGROUND_SIGNAL,
                "Send foreground / background signal to GPU stack."),
        Flag.baseFeature(
                ContentFeatures.PERSISTENT_ORIGIN_TRIALS,
                "If enabled, servers will be able to use persistent origin trials "
                        + "on this device."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_IMAGE_DRAG,
                "If enabled, images can be dragged out from Webview"),
        Flag.baseFeature(
                BlinkFeatures.WEB_RTC_COMBINED_NETWORK_AND_WORKER_THREAD,
                "Combines WebRTC's worker thread and network thread onto a single thread."),
        Flag.baseFeature(
                BlinkFeatures.V_SYNC_DECODING, "Runs the WebRTC metronome off the VSync signal."),
        Flag.baseFeature(
                BlinkFeatures.WEB_RTC_SEND_PACKET_BATCH,
                "Sends outgoing WebRTC Video RTP packets in batches."),
        Flag.baseFeature(
                "WebRtcEncodedTransformsPerStreamCreation",
                "Allows creating WebRTC Encoded Transforms without the "
                        + "encodedInsertableStreams RTCPeerConnection Parameter."),
        Flag.baseFeature(
                ContentSwitches.DISABLE_DOMAIN_BLOCKING_FOR3DAP_IS,
                "Disable the per-domain blocking for 3D APIs after GPU reset. "
                        + "This switch is intended only for tests."),
        Flag.baseFeature(
                MetricsFeatures.METRICS_SERVICE_ALLOW_EARLY_LOG_CLOSE,
                "Controls whether a log is allowed to be closed when Chrome"
                        + " is backgrounded/foregrounded early."),
        Flag.baseFeature(
                MetricsFeatures.FLUSH_PERSISTENT_SYSTEM_PROFILE_ON_WRITE,
                "Controls whether to schedule a flush of persistent histogram memory "
                        + "immediately after writing a system profile to it."),
        Flag.baseFeature(
                MetricsFeatures.METRICS_SERVICE_DELTA_SNAPSHOT_IN_BG,
                "Controls whether to perform histogram delta snapshots in a background "
                        + "thread (in contrast to snapshotting unlogged samples in the "
                        + "background, then marking them as logged on the main thread)."),
        Flag.baseFeature(
                MetricsFeatures.REPORTING_SERVICE_ALWAYS_FLUSH,
                "Determines whether to always flush Local State immediately after an UMA/UKM "
                        + "log upload."),
        Flag.baseFeature(
                ContentFeatures.MAIN_THREAD_COMPOSITING_PRIORITY,
                "When enabled runs the main thread at compositing priority."),
        Flag.baseFeature(
                ContentFeatures.REDUCE_SUBRESOURCE_RESPONSE_STARTED_IPC,
                "When enabled, reduces SubresourceResponseStarted IPC by sending"
                        + "subresource notifications only if the user has allowed"
                        + "HTTPS-related exceptions."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_UMA_UPLOAD_QUALITY_OF_SERVICE_SET_TO_DEFAULT,
                "If enabled, the frequency to upload UMA is increased."),
        Flag.baseFeature("CanvasColorCache"),
        Flag.baseFeature(
                BlinkFeatures.KEYBOARD_FOCUSABLE_SCROLLERS,
                "When enabled, can focus on a scroller element using the keyboard."),
        Flag.commandLine(
                AwSwitches.WEBVIEW_ENABLE_TRUST_TOKENS_COMPONENT,
                "Enables downloading TrustTokenKeyCommitmentsComponent by the component"
                        + " updater downloading service in nonembedded WebView."
                        + " See https://crbug.com/1170468."),
        Flag.baseFeature(
                BlinkFeatures.REPORT_VISIBLE_LINE_BOUNDS,
                "When enabled, WebView reports rectangles which surround each line of"
                        + " text in the currently focused element to Android. These rectangles "
                        + "are sent for <input> and <textarea> elements."),
        Flag.baseFeature(
                BlinkFeatures.STYLUS_POINTER_ADJUSTMENT,
                "When enabled, a hover icon is shown over editable HTML elements when"
                        + " using a stylus and the rectangle to trigger stylus writing on"
                        + " editable elements is expanded."),
        Flag.baseFeature(
                BlinkFeatures.STYLUS_RICH_GESTURES,
                "When enabled, stylus input can be used to draw rich gestures which "
                        + "affect text in editable web content."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_ZOOM_KEYBOARD_SHORTCUTS,
                "Enables WebView to use zoom keyboard shortcuts on hardware keyboards."),
        Flag.baseFeature(
                ContentFeatures.PRIVACY_SANDBOX_ADS_AP_IS_OVERRIDE,
                "When enabled, the following ads APIs will be available: Attribution Reporting,"
                        + "FLEDGE, Topics."),
        Flag.baseFeature(
                BlinkFeatures.RENDER_BLOCKING_FONTS,
                "When enabled, blocks rendering on font preloads to reduce CLS. "
                        + "See go/critical-font-analysis"),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_METRICS_FILTERING,
                "If enabled, clients used to be out-sampled will report filtered metrics."
                        + " This has no effect if metrics reporting is disabled"),
        Flag.baseFeature(
                SafeBrowsingFeatures.SAFE_BROWSING_SKIP_SUBRESOURCES,
                "When enabled, Safe Browsing will skip subresources"),
        Flag.baseFeature(
                "SafeBrowsingSkipSubResources2",
                "When enabled, Safe Browsing will skip WebTransport and WebSockets"),
        Flag.baseFeature(
                "AddWarningShownTSToClientSafeBrowsingReport",
                "When enabled, client reports will include a timestamp of when the warning was "
                        + "shown to the user"),
        Flag.baseFeature(
                "CreateWarningShownClientSafeBrowsingReports",
                "When enabled, WARNING_SHOWN client reports will be sent when a warning is "
                        + "shown to the user"),
        Flag.baseFeature("SafeBrowsingOnUIThread"),
        Flag.baseFeature(
                BlinkFeatures.ANDROID_EXTENDED_KEYBOARD_SHORTCUTS,
                "Enables WebView to use the extended keyboard shortcuts added for Android U"),
        Flag.baseFeature(
                BlinkFeatures.AUTOFILL_USE_DOM_NODE_ID_FOR_RENDERER_ID,
                "Enables Autofill to detect use DOM Node IDs for renderer IDs"),
        Flag.baseFeature(
                NetFeatures.PARTITIONED_COOKIES, "Enables the Partitioned cookie attribute"),
        Flag.baseFeature(
                NetFeatures.SUPPORT_PARTITIONED_BLOB_URL,
                "Enables the new Blob URL implementation needed for third-party storage"
                        + " partitioning"),
        Flag.baseFeature(
                NetFeatures.THIRD_PARTY_STORAGE_PARTITIONING,
                "Enables partitioning of third-party storage by top-level site. Note: this is under"
                    + " active development and may result in unexpected behavior. Please file bugs"
                    + " at https://bugs.chromium.org/p/chromium/issues/"
                    + "entry?labels=StoragePartitioning-trial-bugs&components=Blink%3EStorage."),
        Flag.baseFeature(
                NetFeatures.ASYNC_QUIC_SESSION, "Enables asynchronous QUIC session creation"),
        Flag.baseFeature(
                NetFeatures.BLOCK_TRUNCATED_COOKIES,
                "When enabled, cookies containing '\\0', '\\r', and '\\n' characters will be "
                        + "deemed invalid and the cookie won't be set."),
        Flag.baseFeature(
                NetFeatures.SPDY_HEADERS_TO_HTTP_RESPONSE_USE_BUILDER,
                "Enables new optimized implementation of SpdyHeadersToHttpResponse. No behavior"
                        + " change."),
        Flag.baseFeature(
                BlinkFeatures.NEW_BASE_URL_INHERITANCE_BEHAVIOR,
                "Enables the new base-url inheritance behavior for about:blank and "
                        + "about:srcdoc pages loaded in a webview."),
        Flag.baseFeature("MojoIpcz"),
        Flag.baseFeature(
                TracingServiceFeatures.ENABLE_PERFETTO_SYSTEM_TRACING,
                "When enabled, WebView exports trace events to the Android Perfetto service."
                        + " This works only for Android Q+."),
        Flag.baseFeature(
                UiAndroidFeatures.CONVERT_TRACKPAD_EVENTS_TO_MOUSE,
                "Enables converting trackpad click gestures to mouse events"
                        + " in order for them to be interpreted similar to a desktop"
                        + " experience (i.e. double-click to select word.)"),
        Flag.baseFeature(UiAndroidFeatures.ANDROID_HDR, "Enables HDR support"),
        Flag.baseFeature(
                NetworkServiceFeatures.ATTRIBUTION_REPORTING_CROSS_APP_WEB,
                "Enable attribution reporting to cross the app/web barrier by letting "
                        + "the WebView use OS-level attribution."),
        Flag.baseFeature(BaseFeatures.THREAD_POOL_CAP2, "Sets a fixed thread pool cap"),
        Flag.baseFeature(
                BlinkFeatures.BEFOREUNLOAD_EVENT_CANCEL_BY_PREVENT_DEFAULT,
                "Enables showing the cancel dialog by calling preventDefault() "
                        + "on beforeunload event."),
        Flag.baseFeature(
                ContentFeatures.QUEUE_NAVIGATIONS_WHILE_WAITING_FOR_COMMIT,
                "If enabled, allows navigations to be queued when there is "
                        + "an existing pending commit navigation in progress."),
        Flag.baseFeature("NetworkServiceCookiesHighPriorityTaskRunner"),
        Flag.baseFeature("IncreaseCoookieAccesCacheSize"),
        Flag.baseFeature(
                VizFeatures.ON_BEGIN_FRAME_THROTTLE_VIDEO,
                "Enables throttling OnBeginFrame for video frame sinks"
                        + "with a preferred framerate defined."),
        Flag.baseFeature(
                BaseFeatures.COLLECT_ANDROID_FRAME_TIMELINE_METRICS,
                "Report frame metrics to Google, if metrics reporting has been enabled."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_CLEAR_FUNCTOR_IN_BACKGROUND,
                "Clear the draw functor after some time in background."),
        Flag.baseFeature(
                PermissionsAndroidFeatureList.BLOCK_MIDI_BY_DEFAULT,
                "This flag won't block MIDI by default in WebView. In fact "
                        + "it makes sure the changes made to do so in "
                        + "Chromium won't affect WebView."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_PROPAGATE_NETWORK_SIGNALS,
                "This flag will allow webView to propagate networking signals to the networking"
                    + " stack. Only onNetwork(Connected|Disconnected|SoonToDisconnect|MadeDefault)"
                    + " signals are propagated."),
        Flag.baseFeature(
                ContentFeatures.PREFETCH_NEW_LIMITS,
                "Enables new limits policy for SpeculationRules Prefetch."),
        Flag.baseFeature(
                BlinkFeatures.FORM_CONTROLS_VERTICAL_WRITING_MODE_SUPPORT,
                "Enables support for CSS vertical writing mode on non-text-based form"
                        + " controls."),
        Flag.baseFeature(
                BlinkFeatures.FIX_GESTURE_SCROLL_QUEUING_BUG,
                "Queues gesture scrolls that do not hit a blocking handler, "
                        + "while handling events that hit a blocking handler instantly"
                        + " as this behaviour was flipped before this fix."),
        Flag.baseFeature(
                BlinkFeatures.QUEUE_BLOCKING_GESTURE_SCROLLS,
                "Queues all gesture scrolls regardless of blocking status on the"
                        + "compositor for more consistency and scrolling performance"
                        + "improvement"),
        Flag.baseFeature(
                BlinkFeatures.SERIALIZE_ACCESSIBILITY_POST_LIFECYCLE,
                "When enabled, the serialization of accessibility information"
                        + " for the browser process will be done during"
                        + " LocalFrameView::RunPostLifecycleSteps, rather than"
                        + " from a stand-alone task."),
        Flag.baseFeature(
                BaseFeatures.PARTITION_ALLOC_MEMORY_RECLAIMER,
                "Enables PartitionAlloc's MemoryReclaimer, which tries decommitting unused "
                        + "system pages as much as possible so that other applications can "
                        + "reuse the memory pages."),
        Flag.baseFeature(VizFeatures.EVICT_SUBTREE, "Enables evicting entire tree of surfaces."),
        Flag.baseFeature(
                ContentFeatures.NAVIGATION_UPDATES_CHILD_VIEWS_VISIBILITY,
                "Enables notifying children of the top-most RenderWidgetHostView that they "
                        + "were hidden during a navigation."),
        Flag.baseFeature(
                BlinkFeatures.FORM_CONTROLS_VERTICAL_WRITING_MODE_TEXT_SUPPORT,
                "Enables support for CSS vertical writing mode on text-based form controls."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_CHECK_PAK_FILE_DESCRIPTORS,
                "Crash on failing to load pak file fds."),
        Flag.baseFeature(
                BlinkFeatures.LOADING_PHASE_BUFFER_TIME_AFTER_FIRST_MEANINGFUL_PAINT,
                "Enables extending the loading phase by some buffer time after "
                        + "First Meaningful Paint is signaled."),
        Flag.baseFeature(
                BlinkFeatures.NON_STANDARD_APPEARANCE_VALUES_HIGH_USAGE,
                "This flag allows non-standard CSS appearance values with page load "
                        + "usage >= 0.001% and shows a deprecation warning."),
        Flag.baseFeature(
                BlinkFeatures.NON_STANDARD_APPEARANCE_VALUES_LOW_USAGE,
                "This flag allows non-standard CSS appearance values with page load "
                        + "usage < 0.001% and shows a deprecation warning."),
        Flag.baseFeature(
                BlinkFeatures.DISCARD_INPUT_EVENTS_TO_RECENTLY_MOVED_FRAMES,
                "Enables a browser intervention which silently ignores input events "
                        + "targeting a cross-origin iframe which has moved within its "
                        + "embedding page recently."),
        Flag.baseFeature(
                ContentFeatures.SERVICE_WORKER_STATIC_ROUTER,
                "Enables Service Worker static routing API."),
        Flag.baseFeature(
                ContentFeatures.BACK_FORWARD_CACHE_MEDIA_SESSION_SERVICE,
                "Enables media session usage when bfcache is enabled"),
        Flag.baseFeature(
                ContentFeatures.SELECTION_MENU_ITEM_MODIFICATION,
                "Enables text selection menu item modification based on "
                        + "embedder implementation."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_SUPERVISED_USER_SITE_DETECTION,
                "Enable detection of the loading of mature sites on "
                        + "WebViews running on supervised user accounts"),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_SUPERVISED_USER_SITE_BLOCK,
                "Enable blocking the loading of mature sites on "
                        + "WebViews running on supervised user accounts"),
        Flag.baseFeature(GwpAsanFeatures.GWP_ASAN_MALLOC, "GWP-ASan for `malloc()`."),
        Flag.baseFeature(GwpAsanFeatures.GWP_ASAN_PARTITION_ALLOC, "GWP-ASan for PartitionAlloc."),
        Flag.baseFeature(
                CcFeatures.USE_MAP_RECT_FOR_PIXEL_MOVEMENT,
                "Enables the usage of MapRect for computing filter pixel movement."),
        Flag.baseFeature(
                "UseAAudioInput",
                "Enables the use of AAudio for capturing audio input. (Android Q+ only)"),
        Flag.baseFeature("UseRustJsonParser"),
        Flag.baseFeature("V8FlushCodeBasedOnTime"),
        Flag.baseFeature("V8FlushCodeBasedOnTabVisibility"),
        Flag.baseFeature("V8SingleThreadedGCInBackground"),
        Flag.baseFeature("V8MemoryReducer"),
        Flag.baseFeature("V8MinorMS"),
        Flag.baseFeature("WebAssemblyMoreAggressiveCodeCaching"),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_INJECT_PLATFORM_JS_APIS,
                "Inject platform-specific JavaScript APIs."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_MEDIA_INTEGRITY_API,
                "Enable the WebView Media Integrity API. Requires injection of platform-specific"
                        + " JavaScript APIs to be enabled."),
        Flag.baseFeature(
                "PMProcessPriorityPolicy",
                "Controls whether the priority of renderers is controlled by the performance "
                        + "manager."),
        Flag.baseFeature(
                BlinkFeatures.BACKGROUND_RESOURCE_FETCH,
                "Process resource requests in a background thread inside Blink."),
        Flag.baseFeature(
                BlinkFeatures.THROTTLE_UNIMPORTANT_FRAME_TIMERS,
                "Throttles Javascript timer wake ups of unimportant frames."),
        Flag.baseFeature(
                NetworkServiceFeatures.REDUCE_TRANSFER_SIZE_UPDATED_IPC,
                "When enabled, the network service will send TransferSizeUpdatedIPC IPC only when"
                        + " DevTools is attached or the request is for an ad request."),
        Flag.baseFeature(
                BaseFeatures.USE_NEW_JOB_IMPLEMENTATION,
                "Uses a thread pool job implementation which leverages atomics to minimize lock"
                        + " contention."),
        Flag.baseFeature(
                ContentFeatures.BACK_FORWARD_CACHE, "Controls if back/forward cache is enabled."),
        Flag.baseFeature(
                VizFeatures.INVALIDATE_LOCAL_SURFACE_ID_PRE_COMMIT,
                "When enabled, invalidates the LocalSurfaceId of the DelegatedFrameHostAndroid when"
                        + " the old page is about to be unloaded."),
        Flag.baseFeature(
                BaseFeatures.PARTITION_ALLOC_SCHEDULER_LOOP_QUARANTINE,
                "Enables Partition Allocator's FreeFlags::kSchedulerLoopQuarantine"),
        Flag.baseFeature(
                BaseFeatures.PARTITION_ALLOC_ZAPPING_BY_FREE_FLAGS,
                "Enables Partition Allocator's FreeFlags::kZap"),
        // Add new commandline switches and features above. The final entry should have a
        // trailing comma for cleaner diffs.
    };
}
