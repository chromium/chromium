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
import org.chromium.components.metrics.AndroidMetricsFeatures;
import org.chromium.components.metrics.MetricsFeatures;
import org.chromium.components.metrics.MetricsSwitches;
import org.chromium.components.network_session_configurator.NetworkSessionSwitches;
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
     * A list of commandline flags supported on user devices. If updating this list, please also
     * update enums.xml. See android_webview/docs/developer-ui.md
     * (https://chromium.googlesource.com/chromium/src/+/HEAD/android_webview/docs/developer-ui.md#Adding-your-flags-and-features-to-the-UI).
     */
    public static final Flag[] sFlagList = {
            Flag.commandLine(AwSwitches.HIGHLIGHT_ALL_WEBVIEWS,
                    "Highlight the contents (including web contents) of all WebViews with a yellow "
                            + "tint. This is useful for identifying WebViews in an Android "
                            + "application."),
            Flag.commandLine(AwSwitches.WEBVIEW_VERBOSE_LOGGING,
                    "WebView will log additional debugging information to logcat, such as "
                            + "variations and commandline state."),
            Flag.commandLine(CcSwitches.SHOW_COMPOSITED_LAYER_BORDERS,
                    "Renders a border around compositor layers to help debug and study layer "
                            + "compositing."),
            Flag.commandLine(CcSwitches.ANIMATED_IMAGE_RESUME,
                    "Resumes animated images from where they were."),
            Flag.commandLine(AwSwitches.FINCH_SEED_EXPIRATION_AGE,
                    "Forces all variations seeds to be considered stale.", "0"),
            Flag.commandLine(AwSwitches.FINCH_SEED_IGNORE_PENDING_DOWNLOAD,
                    "Forces the WebView service to reschedule a variations seed download job even "
                            + "if one is already pending."),
            Flag.commandLine(AwSwitches.FINCH_SEED_NO_CHARGING_REQUIREMENT,
                    "Forces WebView's service to always schedule a new variations seed download "
                            + "job, even if the device is not charging. Note this switch may be "
                            + "necessary for testing on Android emulators as these are not always "
                            + "considered to be charging."),
            Flag.commandLine(AwSwitches.FINCH_SEED_MIN_DOWNLOAD_PERIOD,
                    "Disables throttling of variations seed download jobs.", "0"),
            Flag.commandLine(AwSwitches.FINCH_SEED_MIN_UPDATE_PERIOD,
                    "Disables throttling of new variations seed requests to the WebView service.",
                    "0"),
            Flag.commandLine(MetricsSwitches.FORCE_ENABLE_METRICS_REPORTING,
                    "Forces WebView's metrics reporting to be enabled. This overrides user "
                            + "settings and capacity sampling, but does not override the app's "
                            + "choice to opt-out."),
            Flag.commandLine(AwSwitches.WEBVIEW_LOG_JS_CONSOLE_MESSAGES,
                    "Mirrors JavaScript console messages to system logs."),
            Flag.commandLine(BaseSwitches.ENABLE_CRASH_REPORTER_FOR_TESTING,
                    "Used for turning on Breakpad crash reporting in a debug environment where "
                            + "crash reporting is typically compiled but disabled."),
            Flag.commandLine(GpuSwitches.DISABLE_GPU_RASTERIZATION,
                    "Disables GPU rasterization, i.e. rasterizes on the CPU only."),
            Flag.commandLine(GpuSwitches.IGNORE_GPU_BLOCKLIST,
                    "Overrides the built-in software rendering list and enables "
                            + "GPU acceleration on unsupported device configurations."),
            Flag.commandLine(AwSwitches.WEBVIEW_ENABLE_MODERN_COOKIE_SAME_SITE,
                    "Enables modern SameSite cookie behavior: 1) SameSite=Lax by default "
                            + "(cookies without a SameSite attribute are treated as SameSite=Lax); "
                            + "2) Schemeful Same-Site (site boundaries include the URL scheme)."),
            Flag.commandLine(ContentSwitches.SITE_PER_PROCESS,
                    "Security mode that enables site isolation for all sites inside WebView. In "
                            + "this mode, each renderer process will contain pages from at most "
                            + "one site, using out-of-process iframes when needed. Highly "
                            + "experimental."),
            Flag.commandLine(NetworkSessionSwitches.ENABLE_HTTP2_GREASE_SETTINGS,
                    "Enable sending HTTP/2 SETTINGS parameters with reserved identifiers."),
            Flag.commandLine(NetworkSessionSwitches.DISABLE_HTTP2_GREASE_SETTINGS,
                    "Disable sending HTTP/2 SETTINGS parameters with reserved identifiers."),
            Flag.commandLine(VariationsSwitches.ENABLE_FINCH_SEED_DELTA_COMPRESSION,
                    "Enables delta-compression when requesting a new seed from the server."),
            Flag.commandLine(AwSwitches.WEBVIEW_FENCED_FRAMES,
                    "Enables fenced frames. Also implies SharedStorageAPI, "
                            + "and PrivacySandboxAdsAPIsOverride"),
            Flag.commandLine(AwSwitches.WEBVIEW_DISABLE_APP_RECOVERY,
                    "Disables WebView from checking for app recovery mitigations."),
            Flag.commandLine(AwSwitches.WEBVIEW_ENABLE_APP_RECOVERY,
                    "Enables WebView to check for app recovery mitigations."),
            Flag.baseFeature(BlinkFeatures.USER_AGENT_CLIENT_HINT,
                    "Enables user-agent client hints in WebView."),
            Flag.baseFeature("DefaultPassthroughCommandDecoder",
                    "Use the passthrough GLES2 command decoder."),
            Flag.baseFeature(GpuFeatures.WEBVIEW_VULKAN,
                    "Use Vulkan for composite. Requires Android device and OS support. May crash "
                            + "if enabled on unsupported device."),
            Flag.baseFeature(GpuFeatures.WEBVIEW_SURFACE_CONTROL,
                    "Use SurfaceControl. Requires WebViewThreadSafeMedia and Android device and OS "
                            + "support."),
            Flag.baseFeature(GpuFeatures.WEBVIEW_THREAD_SAFE_MEDIA,
                    "Use thread-safe media path, requires Android P."),
            Flag.baseFeature(VizFeatures.ALLOW_BYPASS_RENDER_PASS_QUADS,
                    "Enable bypass render pass for RenderPassDrawQuads"),
            Flag.baseFeature(VizFeatures.WEBVIEW_NEW_INVALIDATE_HEURISTIC,
                    "More robust heuristic for calling Invalidate"),
            Flag.baseFeature(
                    VizFeatures.WEBVIEW_VULKAN_INTERMEDIATE_BUFFER, "For debugging vulkan"),
            Flag.baseFeature(VizFeatures.ALLOW_UNDAMAGED_NONROOT_RENDER_PASS_TO_SKIP,
                    "Enable optimization for skipping undamaged nonroot render passes."),
            Flag.baseFeature(
                    GpuFeatures.USE_GLES2_FOR_OOP_R, "Force Skia context to use es2 only."),
            Flag.baseFeature(AwFeatures.WEBVIEW_CONNECTIONLESS_SAFE_BROWSING,
                    "Uses GooglePlayService's 'connectionless' APIs for Safe Browsing "
                            + "security checks."),
            Flag.baseFeature(AwFeatures.WEBVIEW_APPS_PACKAGE_NAMES_SERVER_SIDE_ALLOWLIST,
                    "Enables usage of server-side allowlist filtering of"
                            + " app package names."),
            Flag.baseFeature(AwFeatures.WEBVIEW_BROTLI_SUPPORT,
                    "Enables brotli compression support in WebView."),
            Flag.baseFeature(AwFeatures.WEBVIEW_EXTRA_HEADERS_SAME_ORIGIN_ONLY,
                    "Only allow extra headers added via loadUrl() to be sent to the same origin "
                            + "as the original request."),
            Flag.baseFeature(AwFeatures.WEBVIEW_DISPLAY_CUTOUT,
                    "Enables display cutout (notch) support in WebView for Android P and above."),
            Flag.baseFeature(BlinkFeatures.WEBVIEW_ACCELERATE_SMALL_CANVASES,
                    "Accelerate all canvases in webview."),
            Flag.baseFeature(AwFeatures.WEBVIEW_MIXED_CONTENT_AUTOUPGRADES,
                    "Enables autoupgrades for audio/video/image mixed content when mixed content "
                            + "mode is set to MIXED_CONTENT_COMPATIBILITY_MODE"),
            Flag.baseFeature(AwFeatures.WEBVIEW_JAVA_JS_BRIDGE_MOJO,
                    "Enables the new Java/JS Bridge code path with mojo implementation."),
            Flag.baseFeature(
                    AwFeatures.WEBVIEW_ORIGIN_TRIALS, "Enables Origin Trials support on WebView."),
            Flag.baseFeature(BlinkFeatures.GMS_CORE_EMOJI,
                    "Enables retrieval of the emoji font through GMS Core "
                            + "improving emoji glyph coverage."),
            Flag.baseFeature(
                    AndroidAutofillFeatures
                            .ANDROID_AUTOFILL_VIEW_STRUCTURE_WITH_FORM_HIERARCHY_LAYER_NAME,
                    "When enabled, Android Autofill ViewStructures contain an additional "
                            + "hierarchy level."),
            Flag.baseFeature(AutofillFeatures.AUTOFILL_SPLIT_CREDIT_CARD_NUMBERS_CAUTIOUSLY,
                    "Split credit card numbers over multiple fields more cautiously."),
            Flag.baseFeature(AutofillFeatures.AUTOFILL_ENABLE_DEPENDENT_LOCALITY_PARSING,
                    "Enables parsing dependent locality fields (e.g. Bairros in Brazil)."),
            Flag.baseFeature(AutofillFeatures.AUTOFILL_ENABLE_EXPIRATION_DATE_IMPROVEMENTS,
                    "Enables various improvements to handling expiration dates."),
            Flag.baseFeature(AutofillFeatures.AUTOFILL_ENABLE_SELECT_MENU,
                    "Enables autofill of <selectmenu> elements."),
            Flag.baseFeature(AutofillFeatures.AUTOFILL_ENABLE_SUPPORT_FOR_PHONE_NUMBER_TRUNK_TYPES,
                    "Rationalizes city-and-number and city-code fields to the "
                            + "correct trunk-prefix types."),
            Flag.baseFeature(AutofillFeatures.AUTOFILL_ENFORCE_DELAYS_IN_STRIKE_DATABASE,
                    "Enforce delay between offering Autofill opportunities in the "
                            + "strike database."),
            Flag.baseFeature(AutofillFeatures.AUTOFILL_PARSE_ASYNC,
                    "Parse forms asynchronously outside of the UI thread."),
            Flag.baseFeature(AutofillFeatures.AUTOFILL_PARSING_PATTERN_PROVIDER,
                    "Enables Autofill to use its new method to retrieve parsing patterns."),
            Flag.baseFeature(AutofillFeatures.AUTOFILL_PAGE_LANGUAGE_DETECTION,
                    "Enables Autofill to retrieve the page language for form parsing."),
            Flag.baseFeature(AutofillFeatures.AUTOFILL_ALWAYS_PARSE_PLACEHOLDERS,
                    "When enabled, Autofill local heuristics consider the placeholder attribute "
                            + "for determining field types."),
            Flag.baseFeature(AutofillFeatures.AUTOFILL_SERVER_BEHAVIORS,
                    "When enabled, Autofill will request experimental "
                            + "predictions from the Autofill API."),
            Flag.baseFeature(FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_VIRTUAL_CARD_FEATURE,
                    "When enabled, merchant bound virtual cards will be offered in the keyboard "
                            + "accessory."),
            Flag.baseFeature(NetworkServiceFeatures.PRIVATE_STATE_TOKENS,
                    "Enables the prototype Private State Tokens API."),
            Flag.commandLine(AwSwitches.WEBVIEW_DISABLE_PACKAGE_ALLOWLIST_THROTTLING,
                    "Disables throttling querying apps package names allowlist components in"
                            + "WebView clients."),
            Flag.baseFeature(AwFeatures.WEBVIEW_EMPTY_COMPONENT_LOADER_POLICY,
                    "Enables loading a fake empty (no-op) component during WebView startup."),
            Flag.commandLine(AwSwitches.WEBVIEW_SELECTIVE_IMAGE_INVERSION_DARKENING,
                    "Enables use selective image inversion to automatically darken page, it will be"
                            + " used when WebView is in dark mode, but website doesn't provide dark"
                            + " style."),
            Flag.baseFeature(AwFeatures.WEBVIEW_FORCE_DARK_MODE_MATCH_THEME,
                    "Automatically darken page if"
                            + " WebView is set to FORCE_DARK_AUTO and the app has dark theme"),
            Flag.baseFeature(ContentFeatures.VERIFY_DID_COMMIT_PARAMS,
                    "Enables reporting of browser and renderer navigation inconsistencies on"
                            + "navigations"),
            Flag.baseFeature(ContentFeatures.USER_MEDIA_CAPTURE_ON_FOCUS,
                    "Enables GetUserMedia API will only resolve when the document calling it has"
                            + "focus"),
            Flag.baseFeature(ContentFeatures.COMPOSITE_BG_COLOR_ANIMATION,
                    "When enabled, the background-color animation runs on the compositor thread."),
            Flag.baseFeature(AwFeatures.WEBVIEW_USE_METRICS_UPLOAD_SERVICE,
                    "Upload UMA metrics logs through MetricsUploadService not via GMS-core"
                            + " directly."),
            Flag.baseFeature(AndroidMetricsFeatures.ANDROID_METRICS_ASYNC_METRIC_LOGGING,
                    "Initiate metric uploading on a background thread."),
            Flag.baseFeature(BlinkFeatures.SET_TIMEOUT_WITHOUT_CLAMP,
                    "Enables faster setTimeout(,0) by removing the 1 ms clamping."),
            Flag.baseFeature(BlinkFeatures.PAINT_HOLDING_CROSS_ORIGIN,
                    "Defers the first commit until FCP or timeout for cross-origin navigations."),
            Flag.baseFeature(ContentFeatures.NAVIGATION_NETWORK_RESPONSE_QUEUE,
                    "Schedules tasks related to the navigation network responses on a higher "
                            + "priority task queue."),
            Flag.baseFeature(ContentFeatures.EARLY_ESTABLISH_GPU_CHANNEL,
                    "Enable establishing the GPU channel early in renderer startup."),
            Flag.baseFeature(AwFeatures.WEBVIEW_X_REQUESTED_WITH_HEADER_CONTROL,
                    "Restricts insertion of XRequestedWith header on outgoing requests "
                            + "to those that have been allow-listed through the appropriate "
                            + "developer API."),
            Flag.baseFeature(BlinkFeatures.VIEWPORT_HEIGHT_CLIENT_HINT_HEADER,
                    "Enables the use of sec-ch-viewport-height client hint."),
            Flag.baseFeature(GpuFeatures.CANVAS_CONTEXT_LOST_IN_BACKGROUND,
                    "Free Canvas2D resources when the webview is in the background."),
            Flag.baseFeature(GpuFeatures.USE_CLIENT_GMB_INTERFACE,
                    "Uses the ClientGmbInetrface to create GpuMemoryBuffers for Renderers."),
            Flag.baseFeature(GpuFeatures.USE_GPU_SCHEDULER_DFS,
                    "Uses the new SchedulerDFS GPU job scheduler."),
            Flag.baseFeature(BlinkFeatures.AUTOFILL_SHADOW_DOM,
                    "Enables Autofill associate form elements with form "
                            + "control elements across shadow boundaries."),
            Flag.baseFeature(BlinkFeatures.UACH_OVERRIDE_BLANK,
                    "Changes behavior of User-Agent Client Hints to send blank headers "
                            + "when the User-Agent string is overriden"),
            Flag.baseFeature(BlinkFeatures.MAX_UNTHROTTLED_TIMEOUT_NESTING_LEVEL,
                    "Increases the nesting threshold before which "
                            + "setTimeout(..., <4ms) starts being clamped to 4 ms."),
            Flag.baseFeature(BlinkFeatures.ESTABLISH_GPU_CHANNEL_ASYNC,
                    "Enables establishing the GPU channel asnchronously when requesting a new "
                            + "layer tree frame sink."),
            Flag.baseFeature(BlinkFeatures.DECODE_SCRIPT_SOURCE_OFF_THREAD,
                    "If enabled, script source text will be decoded and hashed off the main"
                            + "thread."),
            Flag.baseFeature(BaseFeatures.OPTIMIZE_DATA_URLS,
                    "Optimizes parsing and loading of data: URLs."),
            Flag.baseFeature(BlinkFeatures.PREFETCH_FONT_LOOKUP_TABLES,
                    "If enabled, font lookup tables will be prefetched on renderer startup."),
            Flag.baseFeature(BlinkFeatures.PRECOMPILE_INLINE_SCRIPTS,
                    "If enabled, inline scripts will be stream compiled using a background HTML"
                            + " scanner."),
            Flag.baseFeature(BaseFeatures.RUN_TASKS_BY_BATCHES,
                    "Run tasks in queue for 8ms before before sending a system message."),
            Flag.baseFeature(BlinkFeatures.OFFSET_PARENT_NEW_SPEC_BEHAVIOR,
                    "Enables new HTMLElement.offsetParent behavior to match other browsers."),
            Flag.baseFeature(AwFeatures.WEBVIEW_RECORD_APP_DATA_DIRECTORY_SIZE,
                    "Record the size of the embedding app's data directory"),
            Flag.baseFeature(BlinkFeatures.EARLY_EXIT_ON_NOOP_CLASS_OR_STYLE_CHANGE,
                    "Early exit when the style or class attribute of a DOM element is set to the"
                            + " same value as before."),
            Flag.baseFeature(BlinkFeatures.MAIN_THREAD_HIGH_PRIORITY_IMAGE_LOADING,
                    "If enabled, image load tasks on visible pages have high priority."),
            Flag.baseFeature(BlinkFeatures.THREADED_PRELOAD_SCANNER,
                    "If enabled, the HTMLPreloadScanner will run on a worker thread."),
            Flag.baseFeature(BlinkFeatures.TIMED_HTML_PARSER_BUDGET,
                    "If enabled, the HTMLDocumentParser will use a budget based on elapsed time"
                            + " rather than token count."),
            Flag.baseFeature(AwFeatures.WEBVIEW_HIT_TEST_IN_BLINK_ON_TOUCH_START,
                    "Hit test on touch start in blink"),
            Flag.baseFeature(BaseFeatures.ALIGN_WAKE_UPS, "Align delayed wake ups at 125 Hz"),
            Flag.baseFeature(BlinkSchedulerFeatures.THREADED_SCROLL_PREVENT_RENDERING_STARVATION,
                    "Enable rendering starvation-prevention during threaded scrolling."
                            + " See https://crbug.com/1315279."),
            Flag.baseFeature(BlinkSchedulerFeatures.PRIORITIZE_COMPOSITING_AFTER_DELAY_TRIALS,
                    "Controls the delay after which main thread compositing tasks "
                            + "are prioritized over other non-input tasks."),
            Flag.baseFeature(BaseFeatures.NO_WAKE_UPS_FOR_CANCELED_TASKS,
                    "Controls whether wake ups are possible for canceled tasks."),
            Flag.baseFeature(BaseFeatures.REMOVE_CANCELED_TASKS_IN_TASK_QUEUE,
                    "Controls whether or not canceled delayed tasks are removed from task queues."),
            Flag.baseFeature(BlinkFeatures.VIEW_TRANSITION,
                    "Enables the experimental View Transitions API."
                            + " See https://github.com/WICG/view-transitions/blob/main/explainer.md."),
            Flag.baseFeature(BlinkFeatures.VIEW_TRANSITION_ON_NAVIGATION,
                    "Enables the experimental View Transitions API for navigations."
                            + " See https://github.com/WICG/view-transitions/blob/main/explainer.md."),
            Flag.baseFeature(BlinkFeatures.CSS_OVERFLOW_FOR_REPLACED_ELEMENTS,
                    "Enables respecting the CSS overflow property on replaced elements."
                            + " See https://chromestatus.com/feature/5137515594383360."),
            Flag.baseFeature(GpuFeatures.INCREASED_CMD_BUFFER_PARSE_SLICE,
                    "Enable the use of an increased parse slice size per command buffer before"
                            + " each forced context switch."),
            Flag.baseFeature(AccessibilityFeatures.ABLATE_SEND_PENDING_ACCESSIBILITY_EVENTS,
                    "Enable to increase the cost of SendPendingAccessibilityEvents"),
            Flag.baseFeature(BlinkFeatures.RUN_TEXT_INPUT_UPDATE_POST_LIFECYCLE,
                    "Runs code to update IME state at the end of a lifecycle update "
                            + "rather than the beginning."),
            Flag.baseFeature(CcFeatures.NON_BLOCKING_COMMIT,
                    "Don't block the renderer main thread unconditionally while waiting "
                            + "for commit to finish on the compositor thread."),
            Flag.baseFeature(CcFeatures.USE_DMSAA_FOR_TILES,
                    "Switches skia to use DMSAA instead of MSAA for tile raster"),
            Flag.baseFeature(BlinkFeatures.CSS_PAINTING_FOR_SPELLING_GRAMMAR_ERRORS,
                    "Use the new CSS-based painting for spelling and grammar errors"),
            Flag.baseFeature(BlinkFeatures.WEB_RTC_ENCODER_ASYNC_ENCODE,
                    "Make RTCVideoEncoder encode call asynchronous."),
            Flag.baseFeature(BlinkFeatures.WEB_RTC_INITIALIZE_ENCODER_ON_FIRST_FRAME,
                    "Initialize VideoEncodeAccelerator on the first encode."),
            Flag.baseFeature(BlinkFeatures.WEB_RTC_METRONOME,
                    "Inject a metronome into webrtc to allow task coalescing, "
                            + " including synchronized decoding."),
            Flag.baseFeature(BlinkFeatures.FAST_PATH_PAINT_PROPERTY_UPDATES,
                    "If enabled, some paint property updates (e.g., transform "
                            + "changes) will be applied directly instead of "
                            + "using the property tree builder."),
            Flag.baseFeature(BlinkFeatures.THREADED_BODY_LOADER,
                    "If enabled, reads and decodes navigation body data off the main thread."),
            Flag.baseFeature(BlinkFeatures.SVG_RASTER_OPTIMIZATIONS),
            Flag.baseFeature(BlinkFeatures.COMPOSITE_BACKGROUND_ATTACHMENT_FIXED),
            Flag.baseFeature(BlinkFeatures.COMPOSITE_SCROLL_AFTER_PAINT),
            Flag.baseFeature(BlinkFeatures.DELAY_OUT_OF_VIEWPORT_LAZY_IMAGES,
                    "Delays out-of-viewport lazy loaded images."),
            Flag.baseFeature(BlinkFeatures.SEND_MOUSE_EVENTS_DISABLED_FORM_CONTROLS,
                    "This changes event propagation for disabled form controls."),
            Flag.baseFeature(ContentFeatures.SURFACE_SYNC_FULLSCREEN_KILLSWITCH,
                    "Disable to turn off the new SurfaceSync Fullscreen path."),
            Flag.baseFeature(ContentFeatures.PERSISTENT_ORIGIN_TRIALS,
                    "If enabled, servers will be able to use persistent origin trials "
                            + "on this device."),
            Flag.baseFeature(AwFeatures.WEBVIEW_IMAGE_DRAG,
                    "If enabled, images can be dragged out from Webview"),
            Flag.baseFeature(BlinkFeatures.WEB_RTC_COMBINED_NETWORK_AND_WORKER_THREAD,
                    "Combines WebRTC's worker thread and network thread onto a single thread."),
            Flag.baseFeature(BlinkFeatures.V_SYNC_DECODING,
                    "Runs the WebRTC metronome off the VSync signal."),
            Flag.baseFeature(BlinkFeatures.WEB_RTC_SEND_PACKET_BATCH,
                    "Sends outgoing WebRTC Video RTP packets in batches."),
            Flag.baseFeature(ContentSwitches.DISABLE_DOMAIN_BLOCKING_FOR3DAP_IS,
                    "Disable the per-domain blocking for 3D APIs after GPU reset. "
                            + "This switch is intended only for tests."),
            Flag.baseFeature(MetricsFeatures.METRICS_SERVICE_ALLOW_EARLY_LOG_CLOSE,
                    "Controls whether a log is allowed to be closed when Chrome"
                            + " is backgrounded/foregrounded early."),
            Flag.baseFeature(MetricsFeatures.METRICS_CLEAR_LOGS_ON_CLONED_INSTALL,
                    "Controls whether UMA logs are cleared when a cloned "
                            + "install is detected."),
            Flag.baseFeature(MetricsFeatures.RESTORE_UMA_CLIENT_ID_INDEPENDENT_LOGS,
                    "Controls whether independent logs from PMA files will use the embedded "
                            + "client uuid as the log's client ID."),
            Flag.baseFeature(ContentFeatures.MAIN_THREAD_COMPOSITING_PRIORITY,
                    "When enabled runs the main thread at compositing priority."),
            Flag.baseFeature(ContentFeatures.REDUCE_SUBRESOURCE_RESPONSE_STARTED_IPC,
                    "When enabled, reduces SubresourceResponseStarted IPC by sending"
                            + "subresource notifications only if the user has allowed"
                            + "HTTPS-related exceptions."),
            Flag.baseFeature(AwFeatures.WEBVIEW_UMA_UPLOAD_QUALITY_OF_SERVICE_SET_TO_DEFAULT,
                    "If enabled, the frequency to upload UMA is increased."),
            Flag.baseFeature("CanvasColorCache"),
            Flag.baseFeature(BlinkFeatures.KEYBOARD_FOCUSABLE_SCROLLERS,
                    "When enabled, can focus on a scroller element using the keyboard."),
            Flag.commandLine(AwSwitches.WEBVIEW_ENABLE_TRUST_TOKENS_COMPONENT,
                    "Enables downloading TrustTokenKeyCommitmentsComponent by the component"
                            + " updater downloading service in nonembedded WebView."
                            + " See https://crbug.com/1170468."),
            Flag.baseFeature(BlinkFeatures.STYLUS_POINTER_ADJUSTMENT,
                    "When enabled, a hover icon is shown over editable HTML elements when"
                            + " using a stylus and the rectangle to trigger stylus writing on"
                            + " editable elements is expanded."),
            Flag.baseFeature(BlinkFeatures.STYLUS_RICH_GESTURES,
                    "When enabled, stylus input can be used to draw rich gestures which "
                            + "affect text in editable web content."),
            Flag.baseFeature(AwFeatures.WEBVIEW_ZOOM_KEYBOARD_SHORTCUTS,
                    "Enables WebView to use zoom keyboard shortcuts on hardware keyboards."),
            Flag.baseFeature(ContentFeatures.PRIVACY_SANDBOX_ADS_AP_IS_OVERRIDE,
                    "When enabled, the following ads APIs will be available: Attribution Reporting,"
                            + "FLEDGE, Topics."),
            Flag.baseFeature(BlinkFeatures.RENDER_BLOCKING_FONTS,
                    "When enabled, blocks rendering on font preloads to reduce CLS. "
                            + "See go/critical-font-analysis"),
            Flag.baseFeature(AwFeatures.WEBVIEW_SERVER_SIDE_SAMPLING,
                    "If enabled, the client side sampling for user metrics will be turned off."
                            + " This has no effect if metrics reporting is disabled"),
            Flag.baseFeature("SafeBrowsingOnUIThread"),
            Flag.baseFeature(BlinkFeatures.ANDROID_EXTENDED_KEYBOARD_SHORTCUTS,
                    "Enables WebView to use the extended keyboard shortcuts added for Android U"),
            Flag.baseFeature("LessChattyNetworkService"),
            Flag.baseFeature(BlinkFeatures.AUTOFILL_DETECT_REMOVED_FORM_CONTROLS,
                    "Enables Autofill to detect if form controls are removed from the DOM"),
            Flag.baseFeature(
                    NetFeatures.PARTITIONED_COOKIES, "Enables the Partitioned cookie attribute"),
            Flag.baseFeature(NetFeatures.SUPPORT_PARTITIONED_BLOB_URL,
                    "Enables the new Blob URL implementation needed for third-party storage"
                            + " partitioning"),
            Flag.baseFeature(NetFeatures.THIRD_PARTY_STORAGE_PARTITIONING,
                    "Enables partitioning of third-party storage by top-level site. "
                            + "Note: this is under active development and may result in unexpected "
                            + "behavior. Please file bugs at https://bugs.chromium.org/p/chromium/issues/"
                            + "entry?labels=StoragePartitioning-trial-bugs&components=Blink%3EStorage."),
            Flag.baseFeature(
                    NetFeatures.ASYNC_QUIC_SESSION, "Enables asynchronous QUIC session creation"),
            Flag.baseFeature(BaseFeatures.CRASH_BROWSER_ON_CHILD_MISMATCH_IF_BROWSER_CHANGED,
                    "Causes the browser process to crash if child processes are failing to launch"
                            + " due to a browser version change."),
            Flag.baseFeature(BlinkFeatures.NEW_BASE_URL_INHERITANCE_BEHAVIOR,
                    "Enables the new base-url inheritance behavior for about:blank and "
                            + "about:srcdoc pages loaded in a webview."),
            Flag.baseFeature("MojoIpcz"),
            Flag.baseFeature(TracingServiceFeatures.ENABLE_PERFETTO_SYSTEM_TRACING,
                    "When enabled, WebView exports trace events to the Android Perfetto service."
                            + " This works only for Android Q+."),
            Flag.baseFeature(AwFeatures.WEBVIEW_SAFE_BROWSING_SAFE_MODE,
                    "Enable doing a JNI call to check safe browsing safe mode status "
                            + "before doing a safe browsing check."),
            Flag.baseFeature(UiAndroidFeatures.CONVERT_TRACKPAD_EVENTS_TO_MOUSE,
                    "Enables converting trackpad click gestures to mouse events"
                            + " in order for them to be interpreted similar to a desktop"
                            + " experience (i.e. double-click to select word.)"),
            Flag.baseFeature(NetworkServiceFeatures.ATTRIBUTION_REPORTING_CROSS_APP_WEB,
                    "Enable attribution reporting to cross the app/web barrier by letting "
                            + "the WebView use OS-level attribution."),
            Flag.baseFeature(BaseFeatures.THREAD_POOL_CAP,
                    "Reduces the thread pool cap to use less threads"),
            Flag.baseFeature(BlinkFeatures.BEFOREUNLOAD_EVENT_CANCEL_BY_PREVENT_DEFAULT,
                    "Enables showing the cancel dialog by calling preventDefault() "
                            + "on beforeunload event."),
            Flag.baseFeature(ContentFeatures.QUEUE_NAVIGATIONS_WHILE_WAITING_FOR_COMMIT,
                    "If enabled, allows navigations to be queued when there is "
                            + "an existing pending commit navigation in progress."),
            Flag.baseFeature("NetworkServiceCookiesHighPriorityTaskRunner"),
            Flag.baseFeature(VizFeatures.ON_BEGIN_FRAME_THROTTLE_VIDEO,
                    "Enables throttling OnBeginFrame for video frame sinks"
                            + "with a preferred framerate defined."),
            Flag.baseFeature(AwFeatures.WEBVIEW_REPORT_FRAME_METRICS,
                    "Report frame metrics to Google, if metrics reporting has been enabled."),
            Flag.baseFeature(AwFeatures.WEBVIEW_CLEAR_FUNCTOR_IN_BACKGROUND,
                    "Clear the draw functor after some time in background."),
            // Add new commandline switches and features above. The final entry should have a
            // trailing comma for cleaner diffs.
    };
}
