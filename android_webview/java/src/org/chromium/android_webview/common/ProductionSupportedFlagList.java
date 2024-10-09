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
import org.chromium.components.sensitive_content.SensitiveContentFeatures;
import org.chromium.components.variations.VariationsSwitches;
import org.chromium.components.viz.common.VizFeatures;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.gpu.config.GpuFeatures;
import org.chromium.gpu.config.GpuSwitches;
import org.chromium.media.MediaFeatures;
import org.chromium.net.CookieSwitches;
import org.chromium.net.NetFeatures;
import org.chromium.services.network.NetworkServiceFeatures;
import org.chromium.services.tracing.TracingServiceFeatures;
import org.chromium.ui.accessibility.AccessibilityFeatures;
import org.chromium.ui.base.UiAndroidFeatures;
import org.chromium.ui.gfx.GfxSwitches;

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
                AwSwitches.NET_LOG,
                "Enables net logs for WebViews within an application, all network activity"
                        + " will be recorded to a JSON file. Can only"
                        + " be used when WebContentsDebuggingEnabled is enabled."),
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
                "Enables fenced frames. Also enables PrivacySandboxAdsAPIsOverride."),
        Flag.commandLine(
                AwSwitches.DEBUG_BSA,
                "Override and enable features useful for BSA library testing/debugging."),
        Flag.baseFeature(
                "DefaultPassthroughCommandDecoder", "Use the passthrough GLES2 command decoder."),
        Flag.baseFeature(
                GpuFeatures.WEBVIEW_SURFACE_CONTROL,
                "Use SurfaceControl. Requires WebViewThreadSafeMedia and Android device and OS "
                        + "support. Is not supported for TV, see WebViewSurfaceControlForTV."),
        Flag.baseFeature(
                GpuFeatures.WEBVIEW_SURFACE_CONTROL_FOR_TV,
                "Use SurfaceControl. Requires WebViewThreadSafeMedia and Android device and OS "
                        + "support. Only supported on TV."),
        Flag.baseFeature(
                GpuFeatures.RELAX_LIMIT_A_IMAGE_READER_MAX_SIZE_TO_ONE,
                "Allow more than 1 buffer from AImageReader on the specific set of devices. "
                        + "Only supported on TV."),
        Flag.baseFeature(
                GpuFeatures.WEBVIEW_THREAD_SAFE_MEDIA,
                "Use thread-safe media path, requires Android P."),
        Flag.baseFeature(
                GpuFeatures.PRUNE_OLD_TRANSFER_CACHE_ENTRIES,
                "Prune old transfer cache entries and disable pruning from client"),
        Flag.baseFeature(
                VizFeatures.WEBVIEW_NEW_INVALIDATE_HEURISTIC,
                "More robust heuristic for calling Invalidate"),
        Flag.baseFeature(VizFeatures.WEBVIEW_VULKAN_INTERMEDIATE_BUFFER, "For debugging vulkan"),
        Flag.baseFeature(
                VizFeatures.WEBVIEW_ENABLE_ADPF, "Pass WebView threads to HWUI ADPF session"),
        Flag.baseFeature(
                VizFeatures.WEBVIEW_ENABLE_ADPF_RENDERER_MAIN,
                "Include Renderer Main into ADPF session"),
        Flag.baseFeature(
                VizFeatures.WEBVIEW_FRAME_RATE_HINTS,
                "Provide frame rate hints to View system if supported by OS"),
        Flag.baseFeature(
                VizFeatures.ALLOW_UNDAMAGED_NONROOT_RENDER_PASS_TO_SKIP,
                "Enable optimization for skipping undamaged nonroot render passes."),
        Flag.baseFeature(
                VizFeatures.DRAW_IMMEDIATELY_WHEN_INTERACTIVE,
                "Enable optimization for immediate activation and draw when interactive."),
        Flag.baseFeature(GpuFeatures.USE_GLES2_FOR_OOP_R, "Force Skia context to use es2 only."),
        Flag.baseFeature(NetFeatures.PRIORITY_HEADER, "Enables the HTTP priority header."),
        Flag.baseFeature(
                NetFeatures.ZSTD_CONTENT_ENCODING,
                "Enables zstd content-encoding support in the browser."),
        Flag.baseFeature(
                NetFeatures.USE_NEW_ALPS_CODEPOINT_QUIC,
                "Enables using the new ALPS codepoint to negotiate application settings for QUIC."),
        Flag.baseFeature(
                NetFeatures.USE_NEW_ALPS_CODEPOINT_HTTP2,
                "Enables using the new ALPS codepoint to negotiate application settings for"
                        + " HTTP2."),
        Flag.baseFeature(
                BlinkFeatures.SIMPLIFY_LOADING_TRANSPARENT_PLACEHOLDER_IMAGE,
                "Enables simplifying loading known transparent placeholder images."),
        Flag.baseFeature(
                BlinkFeatures.OPTIMIZE_LOADING_DATA_URLS, "Enables optimizing loading data: URLs."),
        Flag.baseFeature(
                NetFeatures.OPTIMIZE_PARSING_DATA_URLS, "Enables optimizing parsing data: URLs."),
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
                BlinkFeatures.GMS_CORE_EMOJI,
                "Enables retrieval of the emoji font through GMS Core "
                        + "improving emoji glyph coverage."),
        Flag.baseFeature(
                AndroidAutofillFeatures.ANDROID_AUTOFILL_BOTTOM_SHEET_WORKAROUND_NAME,
                "Enable the workaround for autofill bottom sheet platform bug."),
        Flag.baseFeature(
                AndroidAutofillFeatures.ANDROID_AUTOFILL_DIRECT_FORM_SUBMISSION,
                "When enabled, submission is directly fired to the provider upon receiving the "
                        + "renderer's signal."),
        Flag.baseFeature(
                AndroidAutofillFeatures.ANDROID_AUTOFILL_PREFILL_REQUEST_FOR_CHANGE_PASSWORD_NAME,
                "Enables sending prefill requests for Change Password forms."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_ACCEPT_DOM_MUTATION_AFTER_AUTOFILL_SUBMISSION,
                "Accepts DOM_MUTATION_AFTER_AUTOFILL submissions detected on password forms."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_ENABLE_EXPIRATION_DATE_IMPROVEMENTS,
                "Enables various improvements to handling expiration dates."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_ENABLE_SUPPORT_FOR_PHONE_NUMBER_TRUNK_TYPES,
                "Rationalizes city-and-number and city-code fields to the "
                        + "correct trunk-prefix types."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_FIX_CACHING_ON_JAVA_SCRIPT_CHANGES,
                "When enabled, Autofill will reset the autofill state of fields modified by JS"),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_FIX_FORM_TRACKING,
                "Improves form submission tracking and duplicate submission handling"),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_FIX_VALUE_SEMANTICS,
                "Fixes the overloaded meaning of FormFieldData::value"),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_FIX_INITIAL_VALUE_OF_SELECT,
                "Sets the AutofillField's initial value for select elements"),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_FIX_CURRENT_VALUE_IN_IMPORT,
                "Prevents the AutofillField's current value from being reset for import"),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_INFER_COUNTRY_CALLING_CODE,
                "Infers the country calling code from the profile's country, if available."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_DECOUPLE_AUTOFILL_COUNT_FROM_CACHE,
                "Makes AutofillManager::GetCachedFormAndField return a form even if"
                        + " form->autofill_count() == 0"),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_DETECT_REMOVED_FORM_CONTROLS,
                "Enables Autofill to detect if form controls are removed from the DOM"),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_PAGE_LANGUAGE_DETECTION,
                "Enables Autofill to retrieve the page language for form parsing."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_PARSE_EMAIL_LABEL_AND_PLACEHOLDER,
                "Classifies fields as email fields if their label or placeholder have valid email"
                        + " format."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_PREFER_PARSED_PHONE_NUMBER,
                "When enabled, Autofill will always prefer the phone number parsed using "
                        + "libphonenumber over the format provided by the field during imports."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_REPLACE_CACHED_WEB_ELEMENTS_BY_RENDERER_IDS,
                "When enabled, AutofillAgent will store its cached form and fields as renderer ids "
                        + "instead of holding strong references to blink::WebElement objects."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_ALWAYS_PARSE_PLACEHOLDERS,
                "When enabled, Autofill local heuristics consider the placeholder attribute "
                        + "for determining field types."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_UNIFY_AND_FIX_FORM_TRACKING,
                "When enabled, AutofillAgent and FormTracker track the same elements."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_USE_AU_ADDRESS_MODEL,
                "When enabled, Autofill uses a custom address model for Australia."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_USE_CA_ADDRESS_MODEL,
                "When enabled, Autofill uses a custom address model for Canada."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_USE_DE_ADDRESS_MODEL,
                "When enabled, Autofill uses a custom address model for Germany."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_USE_FR_ADDRESS_MODEL,
                "When enabled, Autofill uses a custom address model for France."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_USE_IN_ADDRESS_MODEL,
                "When enabled, Autofill uses a custom address model for India."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_USE_IT_ADDRESS_MODEL,
                "When enabled, Autofill uses a custom address model for Italy."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_USE_PL_ADDRESS_MODEL,
                "When enabled, Autofill uses a custom address model for Poland."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_GREEK_REGEXES,
                "When enabled, Greek regexes are used for parsing in branded builds."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_ENABLE_EMAIL_HEURISTIC_ONLY_ADDRESS_FORMS,
                "When enabled, Autofill supports forms consisting of only email fields."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_CARET_EXTRACTION,
                "When enabled, autofill extracts the caret position on certain events."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_CONSIDER_PHONE_NUMBER_SEPARATORS_VALID_LABELS,
                "Makes label inference accept strings made up of  '(', ')', and '-' as labels."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_ENABLE_CACHE_FOR_REGEX_MATCHING,
                "When enabled, autofill uses an extra cache for matching regular expressions "
                        + "while executing local heuristics."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_STRUCTURED_FIELDS_DISABLE_ADDRESS_LINES,
                "When enabled, Autofill disable address lines on forms with structured address"
                        + " fields."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_ENABLE_LABEL_PRECEDENCE_FOR_TURKISH_ADDRESSES,
                "When enabled, the precedence is given to the field label over the name when they"
                        + " match different types. Applied only for parsing of address forms in"
                        + " Turkish."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_UKM_EXPERIMENTAL_FIELDS,
                "Enables UKM collection for experimental fields"),
        Flag.baseFeature(
                FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_VIRTUAL_CARD_FEATURE,
                "When enabled, merchant bound virtual cards will be offered in the keyboard "
                        + "accessory."),
        Flag.baseFeature(
                NetworkServiceFeatures.PRIVATE_STATE_TOKENS,
                "Enables the prototype Private State Tokens API."),
        Flag.baseFeature(
                NetworkServiceFeatures.MASKED_DOMAIN_LIST,
                "When enabled, the masked domain list required for IP Protection is loaded."),
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
                BlinkFeatures.PAINT_HOLDING_FOR_IFRAMES,
                "Show stale paint from old Document until new Document is ready for subframe"
                        + " navigations."),
        Flag.baseFeature(
                ContentFeatures.NAVIGATION_NETWORK_RESPONSE_QUEUE,
                "Schedules tasks related to the navigation network responses on a higher "
                        + "priority task queue."),
        Flag.baseFeature(
                ContentFeatures.EARLY_ESTABLISH_GPU_CHANNEL,
                "Enable establishing the GPU channel early in renderer startup."),
        Flag.baseFeature(
                ContentFeatures.GIN_JAVA_BRIDGE_MOJO_SKIP_CLEAR_OBJECTS_ON_MAIN_DOCUMENT_READY,
                "Skips clearing objects on main document ready."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_X_REQUESTED_WITH_HEADER_CONTROL,
                "Restricts insertion of XRequestedWith header on outgoing requests "
                        + "to those that have been allow-listed through the appropriate "
                        + "developer API."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_REDUCE_UA_ANDROID_VERSION_DEVICE_MODEL,
                "Enables reduce webview user-agent android version and device model."),
        Flag.baseFeature(
                BlinkFeatures.VIEWPORT_HEIGHT_CLIENT_HINT_HEADER,
                "Enables the use of sec-ch-viewport-height client hint."),
        Flag.baseFeature(
                BlinkFeatures.UACH_OVERRIDE_BLANK,
                "Changes behavior of User-Agent Client Hints to send blank headers "
                        + "when the User-Agent string is overriden"),
        Flag.baseFeature(
                BlinkFeatures.ESTABLISH_GPU_CHANNEL_ASYNC,
                "Enables establishing the GPU channel asnchronously when requesting a new "
                        + "layer tree frame sink."),
        Flag.baseFeature(
                BlinkFeatures.ELEMENT_GET_INNER_HTML,
                "Enables the getInnerHTML() function on elements."),
        Flag.baseFeature(BlinkFeatures.TEXT_SIZE_ADJUST_IMPROVEMENTS, "Improved text-size-adjust."),
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
                BlinkFeatures.CRABBY_AVIF,
                "If enabled, CrabbyAvif will be used instead of libavif for decoding AVIF images."),
        Flag.baseFeature(
                BlinkFeatures.DEPRECATE_UNLOAD,
                "If false prevents the gradual deprecation of the unload event."),
        Flag.baseFeature(
                BlinkFeatures.DEPRECATE_UNLOAD_BY_ALLOW_LIST,
                "Unload Deprecation respects a list of allowed origins."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_RECORD_APP_DATA_DIRECTORY_SIZE,
                "Record the size of the embedding app's data directory"),
        Flag.baseFeature(
                BlinkFeatures.THREADED_PRELOAD_SCANNER,
                "If enabled, the HTMLPreloadScanner will run on a worker thread."),
        Flag.baseFeature(
                BlinkFeatures.TIMED_HTML_PARSER_BUDGET,
                "If enabled, the HTMLDocumentParser will use a budget based on elapsed time"
                        + " rather than token count."),
        Flag.baseFeature(
                BlinkFeatures.CHECK_HTML_PARSER_BUDGET_LESS_OFTEN,
                "If enabled, avoids calling the clock for every token in the HTML parser."),
        Flag.baseFeature(
                BlinkFeatures.DETAILS_STYLING,
                "Enables support for improved styling of HTML details element."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_HIT_TEST_IN_BLINK_ON_TOUCH_START,
                "Hit test on touch start in blink"),
        Flag.baseFeature(BaseFeatures.ALIGN_WAKE_UPS, "Align delayed wake ups at 125 Hz"),
        Flag.baseFeature(
                BlinkFeatures.THREADED_SCROLL_PREVENT_RENDERING_STARVATION,
                "Enable rendering starvation-prevention during threaded scrolling."
                        + " See https://crbug.com/40833407."),
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
                BlinkFeatures.REPORT_EVENT_TIMING_AT_VISIBILITY_CHANGE,
                "Report event timing to UKM at visibility change."),
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
                BlinkFeatures.THREADED_BODY_LOADER,
                "If enabled, reads and decodes navigation body data off the main thread."),
        Flag.baseFeature(BlinkFeatures.HIT_TEST_OPAQUENESS),
        Flag.baseFeature(BlinkFeatures.DYNAMIC_SCROLL_CULL_RECT_EXPANSION),
        Flag.baseFeature(BlinkFeatures.INTERSECTION_OPTIMIZATION),
        Flag.baseFeature(BlinkFeatures.EXPAND_COMPOSITED_CULL_RECT),
        Flag.baseFeature(BlinkFeatures.RASTER_INDUCING_SCROLL),
        Flag.baseFeature(BlinkFeatures.SCROLLBAR_COLOR),
        Flag.baseFeature(BlinkFeatures.UNBLOCK_TOUCH_MOVE_EARLIER),
        Flag.baseFeature(
                ContentFeatures.PERSISTENT_ORIGIN_TRIALS,
                "If enabled, servers will be able to use persistent origin trials "
                        + "on this device."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_IMAGE_DRAG,
                "If enabled, images can be dragged out from Webview"),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_DRAG_DROP_FILES,
                "If enabled, files can be dropped into WebView"),
        Flag.baseFeature(
                BlinkFeatures.WEB_RTC_COMBINED_NETWORK_AND_WORKER_THREAD,
                "Combines WebRTC's worker thread and network thread onto a single thread."),
        Flag.baseFeature(
                BlinkFeatures.V_SYNC_DECODING, "Runs the WebRTC metronome off the VSync signal."),
        Flag.baseFeature(
                "WebRtcEncodedTransformsPerStreamCreation",
                "Allows creating WebRTC Encoded Transforms without the "
                        + "encodedInsertableStreams RTCPeerConnection Parameter."),
        Flag.baseFeature(
                "WebRtcEncodedTransformDirectCallback",
                "Directly invoke WebRTC Encoded Transform callbacks in a worker."),
        Flag.baseFeature(
                "RTCAlignReceivedEncodedVideoTransforms",
                "Aligns the JS calls by WebRTC Encoded Transforms on Video Frames with a Metronome"
                        + " to save power."),
        Flag.baseFeature(
                "WebRtcAudioSinkUseTimestampAligner",
                "Align WebRTC and Chrome clocks using a timestamp aligner for absolute capture"
                        + " times in Audio RTP packets."),
        Flag.baseFeature(
                ContentSwitches.DISABLE_DOMAIN_BLOCKING_FOR3DAP_IS,
                "Disable the per-domain blocking for 3D APIs after GPU reset. "
                        + "This switch is intended only for tests."),
        Flag.baseFeature(
                BlinkFeatures.MEDIA_RECORDER_USE_MEDIA_VIDEO_ENCODER,
                "When enabled, media::VideoEncoder implementation is used in MediaRecorder API"
                        + " instead of using MediaRecorder own video encoder implementation."),
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
                MetricsFeatures.METRICS_LOG_TRIMMING, "Controls trimming for metrics logs."),
        Flag.baseFeature(
                ContentFeatures.MAIN_THREAD_COMPOSITING_PRIORITY,
                "When enabled runs the main thread at compositing priority."),
        Flag.baseFeature(
                ContentFeatures.REDUCE_SUBRESOURCE_RESPONSE_STARTED_IPC,
                "When enabled, reduces SubresourceResponseStarted IPC by sending"
                        + "subresource notifications only if the user has allowed"
                        + "HTTPS-related exceptions."),
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
                SafeBrowsingFeatures.HASH_PREFIX_REAL_TIME_LOOKUPS,
                "When enabled, Safe Browsing checks will happen in real time"),
        Flag.baseFeature(
                SafeBrowsingFeatures.SAFE_BROWSING_ASYNC_REAL_TIME_CHECK,
                "When enabled, the real time Safe Browsing check will be called asynchronously,"
                        + " along with an additional v4 check which will be synchronous."),
        Flag.baseFeature(
                "AddWarningShownTSToClientSafeBrowsingReport",
                "When enabled, client reports will include a timestamp of when the warning was "
                        + "shown to the user"),
        Flag.baseFeature(
                "CreateWarningShownClientSafeBrowsingReports",
                "When enabled, WARNING_SHOWN client reports will be sent when a warning is "
                        + "shown to the user"),
        Flag.baseFeature(
                BlinkFeatures.ANDROID_EXTENDED_KEYBOARD_SHORTCUTS,
                "Enables WebView to use the extended keyboard shortcuts added for Android U"),
        Flag.baseFeature(
                NetFeatures.THIRD_PARTY_STORAGE_PARTITIONING,
                "Enables partitioning of third-party storage by top-level site. Note: this is under"
                    + " active development and may result in unexpected behavior. Please file bugs"
                    + " at https://bugs.chromium.org/p/chromium/issues/"
                    + "entry?labels=StoragePartitioning-trial-bugs&components=Blink%3EStorage."),
        Flag.baseFeature(
                NetFeatures.ASYNC_QUIC_SESSION, "Enables asynchronous QUIC session creation"),
        Flag.baseFeature(
                NetFeatures.SPDY_HEADERS_TO_HTTP_RESPONSE_USE_BUILDER,
                "Enables new optimized implementation of SpdyHeadersToHttpResponse. No behavior"
                        + " change."),
        Flag.baseFeature("MojoIpcz"),
        Flag.baseFeature(
                "FixDataPipeTrapBug",
                "Used to disable a specific bug fix for a long-standing bug that may"
                        + " have affected performance. Brief experiment for data collection"),
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
        Flag.baseFeature("ThreadGroupSemaphore"),
        Flag.baseFeature(
                BlinkFeatures.BEFOREUNLOAD_EVENT_CANCEL_BY_PREVENT_DEFAULT,
                "Enables showing the cancel dialog by calling preventDefault() "
                        + "on beforeunload event."),
        Flag.baseFeature(
                BlinkFeatures.CSS_LAZY_PARSING_FAST_PATH,
                "Enables a fast-path for skipping lazily-parsed CSS declaration blocks"),
        Flag.baseFeature(
                ContentFeatures.QUEUE_NAVIGATIONS_WHILE_WAITING_FOR_COMMIT,
                "If enabled, allows navigations to be queued when there is "
                        + "an existing pending commit navigation in progress."),
        Flag.baseFeature(
                ContentFeatures.RENDER_DOCUMENT,
                "If enabled, same-site navigations will change RenderFrameHosts"),
        Flag.baseFeature(
                ContentFeatures.RENDER_DOCUMENT_COMPOSITOR_REUSE,
                "If enabled, allows compositor to be reused on cross-RenderFrameHost navigations"),
        Flag.baseFeature(GpuFeatures.CONDITIONALLY_SKIP_GPU_CHANNEL_FLUSH),
        Flag.baseFeature("ReduceCpuUtilization2"),
        Flag.baseFeature("NetworkServiceCookiesHighPriorityTaskRunner"),
        Flag.baseFeature("IncreaseCoookieAccesCacheSize"),
        Flag.baseFeature("AvoidScheduleWorkDuringNativeEventProcessing"),
        Flag.baseFeature("AvoidEntryCreationForNoStore"),
        Flag.baseFeature("ChangeDiskCacheSize"),
        Flag.baseFeature("BatchNativeEventsInMessagePumpEpoll"),
        Flag.baseFeature(
                VizFeatures.ON_BEGIN_FRAME_THROTTLE_VIDEO,
                "Enables throttling OnBeginFrame for video frame sinks"
                        + "with a preferred framerate defined."),
        Flag.baseFeature(
                BaseFeatures.COLLECT_ANDROID_FRAME_TIMELINE_METRICS,
                "Report frame metrics to Google, if metrics reporting has been enabled."),
        Flag.baseFeature(
                PermissionsAndroidFeatureList.BLOCK_MIDI_BY_DEFAULT,
                "This flag won't block MIDI by default in WebView. In fact "
                        + "it makes sure the changes made to do so in "
                        + "Chromium won't affect WebView."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_PROPAGATE_NETWORK_CHANGE_SIGNALS,
                "This flag will allow webView to propagate networking change signals to the"
                    + " networking stack. Only"
                    + " onNetwork(Connected|Disconnected|SoonToDisconnect|MadeDefault) signals are"
                    + " propagated."),
        Flag.baseFeature(
                BaseFeatures.PARTITION_ALLOC_MEMORY_RECLAIMER,
                "Enables PartitionAlloc's MemoryReclaimer, which tries decommitting unused "
                        + "system pages as much as possible so that other applications can "
                        + "reuse the memory pages."),
        Flag.baseFeature(
                BaseFeatures.PARTITION_ALLOC_SORT_ACTIVE_SLOT_SPANS,
                "Sorts the active slot spans in PartitionRoot::PurgeMemory()."),
        Flag.baseFeature(
                BaseFeatures.PARTITION_ALLOC_SORT_SMALLER_SLOT_SPAN_FREE_LISTS,
                "sort free lists for smaller slot spans in PartitionRoot::PurgeMemory()."),
        Flag.baseFeature(
                BaseFeatures.PARTITION_ALLOC_STRAIGHTEN_LARGER_SLOT_SPAN_FREE_LISTS,
                "Straightens free lists for larger slot spans in PartitionRoot::PurgeMemory() -> "
                        + "... -> PartitionPurgeSlotSpan()."),
        Flag.baseFeature(
                "PartitionAllocUsePoolOffsetFreelists",
                "Activates an alternative freelist implementation in PartitionAlloc."),
        Flag.baseFeature(
                "PartitionAllocUseSmallSingleSlotSpans",
                "Uses a more nuanced heuristic to classify small single-slot spans."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_CHECK_PAK_FILE_DESCRIPTORS,
                "Crash on failing to load pak file fds."),
        Flag.baseFeature(
                BlinkFeatures.LOADING_PHASE_BUFFER_TIME_AFTER_FIRST_MEANINGFUL_PAINT,
                "Enables extending the loading phase by some buffer time after "
                        + "First Meaningful Paint is signaled."),
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
                AwFeatures.WEBVIEW_AUTO_SAA,
                "Enable auto granting storage access API requests. This will be done "
                        + "if a relationship is detected between the app and the website."),
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
                GwpAsanFeatures.EXTREME_LIGHTWEIGHT_UAF_DETECTOR,
                "Enables the Extreme Lightweight UAF Detector."),
        Flag.baseFeature(
                CcFeatures.USE_MAP_RECT_FOR_PIXEL_MOVEMENT,
                "Enables the usage of MapRect for computing filter pixel movement."),
        Flag.baseFeature(
                "UseAAudioInput",
                "Enables the use of AAudio for capturing audio input. (Android Q+ only)"),
        Flag.baseFeature("UseRustJsonParser"),
        Flag.baseFeature("V8BaselineBatchCompilation"),
        Flag.baseFeature("V8ConcurrentSparkplug"),
        Flag.baseFeature("V8CppGCEnableLargerCage"),
        Flag.baseFeature("V8FlushCodeBasedOnTabVisibility"),
        Flag.baseFeature("V8FlushCodeBasedOnTime"),
        Flag.baseFeature("V8MemoryReducer"),
        Flag.baseFeature("V8MinorMS"),
        Flag.baseFeature("V8ScavengerHigherCapacity"),
        Flag.baseFeature("V8SeparateGCPhases"),
        Flag.baseFeature("V8SingleThreadedGCInBackground"),
        Flag.baseFeature("V8SingleThreadedGCInBackgroundNoIncrementalMarking"),
        Flag.baseFeature("V8SingleThreadedGCInBackgroundParallelPause"),
        Flag.baseFeature("V8UpdateLimitAfterLoading"),
        Flag.baseFeature("V8IncrementalMarkingStartUserVisible"),
        Flag.baseFeature("V8ExternalMemoryAccountedInGlobalLimit"),
        Flag.baseFeature("WebAssemblyMoreAggressiveCodeCaching"),
        Flag.baseFeature("WebAssemblyTurboshaft"),
        Flag.baseFeature("WebAssemblyTurboshaftInstructionSelection"),
        Flag.baseFeature("WebAssemblyInlining"),
        Flag.baseFeature("WebAssemblyLiftoffCodeFlushing"),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_MEDIA_INTEGRITY_API_BLINK_EXTENSION,
                "Enable the WebView Media Integrity API as a Blink extension. Only works if"
                        + " WebViewMediaIntegrityApi is disabled."),
        Flag.baseFeature(
                "PMProcessPriorityPolicy",
                "Controls whether the priority of renderers is controlled by the performance "
                        + "manager."),
        Flag.baseFeature(
                "RunPerformanceManagerOnMainThreadSync",
                "Controls whether the performance manager runs on the main thread."),
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
                AwFeatures.WEBVIEW_BACK_FORWARD_CACHE,
                "Controls if back/forward cache is enabled. Note that it's also possible"
                        + " to enable BFCache through AwSettings as well. If either of"
                        + " the flag / setting is enabled, BFCache will be enabled"),
        Flag.baseFeature(
                ContentFeatures.WEBVIEW_SUPPRESS_TAP_DURING_FLING, "Supress tap during fling."),
        Flag.baseFeature(
                ContentFeatures.ACCESSIBILITY_MANAGE_BROADCAST_RECEIVER_ON_BACKGROUND,
                "Register, un-register Accessibility broadcast receiver on a background thread."),
        Flag.baseFeature(
                BlinkFeatures.INCREMENT_LOCAL_SURFACE_ID_FOR_MAINFRAME_SAME_DOC_NAVIGATION,
                "When enabled, every mainframe same-doc navigation will increment the"
                        + " `viz::LocalSurfaceId` from the impl thread."),
        Flag.baseFeature(
                BaseFeatures.PARTITION_ALLOC_SCHEDULER_LOOP_QUARANTINE,
                "Enables PartitionAlloc's FreeFlags::kSchedulerLoopQuarantine"),
        Flag.baseFeature(
                BaseFeatures.PARTITION_ALLOC_ZAPPING_BY_FREE_FLAGS,
                "Enables PartitionAlloc's FreeFlags::kZap"),
        Flag.baseFeature(
                BaseFeatures.POST_POWER_MONITOR_BROADCAST_RECEIVER_INIT_TO_BACKGROUND,
                "If enabled, it posts PowerMonitor broadcast receiver init to a background"
                        + " thread."),
        Flag.baseFeature(
                BaseFeatures.POST_GET_MY_MEMORY_STATE_TO_BACKGROUND,
                "If enabled, getMyMemoryState IPC will be posted to background."),
        Flag.baseFeature(
                BlinkFeatures.REGISTER_JS_SOURCE_LOCATION_BLOCKING_BF_CACHE,
                "Starts capturing bfcache blocking details"),
        Flag.baseFeature(
                "MojoChannelAssociatedSendUsesRunOrPostTask",
                "Enables optimization for sending messages on channel-associated interfaces"),
        Flag.baseFeature(
                "MojoChannelAssociatedCrashesOnSendError",
                "Enable a CHECK to verify if there are Mojo send errors in the field"),
        Flag.baseFeature(
                "MojoBindingsInlineSLS",
                "Enable small value optimization for current Mojo dispatch context storage"),
        Flag.baseFeature(
                BlinkFeatures.FORM_CONTROLS_VERTICAL_WRITING_MODE_DIRECTION_SUPPORT,
                "Enables support for CSS direction ltr and rtl on vertical slider elements"
                        + " progress, meter and range."),
        Flag.baseFeature(
                BlinkFeatures.BOOST_IMAGE_SET_LOADING_TASK_PRIORITY,
                "If enabled, image set loading tasks have higher priority on visible pages"),
        Flag.baseFeature(
                BlinkFeatures.BOOST_FONT_LOADING_TASK_PRIORITY,
                "If enabled, font loading tasks have higher priority on visible pages"),
        Flag.baseFeature(
                BlinkFeatures.BOOST_VIDEO_LOADING_TASK_PRIORITY,
                "If enabled, video loading tasks have higher priority on visible pages"),
        Flag.baseFeature(
                BlinkFeatures.BOOST_RENDER_BLOCKING_STYLE_LOADING_TASK_PRIORITY,
                "If enabled, render-blocking style loading tasks have higher priority on visible"
                        + " pages"),
        Flag.baseFeature(
                BlinkFeatures.BOOST_NON_RENDER_BLOCKING_STYLE_LOADING_TASK_PRIORITY,
                "If enabled, non-render-blocking style loading tasks have higher priority on"
                        + " visible pages"),
        Flag.baseFeature(
                MediaFeatures.BUILT_IN_HLS_PLAYER,
                "Switches the HLS demuxer implementation from MediaPlayer to an internal one"),
        Flag.baseFeature(
                MediaFeatures.LIBVPX_USE_CHROME_THREADS,
                "Attaches libvpx threads to the chromium thread system."),
        Flag.baseFeature(
                MediaFeatures.LIBAOM_USE_CHROME_THREADS,
                "Attaches libaom threads to the chromium thread system."),
        Flag.baseFeature(
                BlinkFeatures.BACK_FORWARD_CACHE_SEND_NOT_RESTORED_REASONS,
                "Expose NotRestoredReasons via PerformanceNavigationTiming API."),
        Flag.baseFeature("SkipUnnecessaryThreadHopsForParseHeaders"),
        Flag.commandLine(
                AwSwitches.WEBVIEW_FPS_COMPONENT,
                "Enables installing the first party sets component to WebViews."),
        Flag.commandLine(
                AwSwitches.WEBVIEW_FORCE_DISABLE3PCS,
                "Force disables 3rd party cookies for all apps."),
        Flag.baseFeature(
                "DoNotEvictOnAXLocationChange",
                "When enabled, do not evict the bfcache entry even when AXLocationChange happens."),
        Flag.baseFeature("PassHistogramSharedMemoryOnLaunch"),
        Flag.baseFeature("PumpFastToSleepAndroid"),
        Flag.baseFeature(
                BlinkFeatures.NO_THROTTLING_VISIBLE_AGENT,
                "Do not throttle Javascript timers to 1Hz on hidden cross-origin frames that are"
                        + " same-agent with a visible frame."),
        Flag.baseFeature("CreateSpareRendererOnBrowserContextCreation"),
        Flag.baseFeature(
                "AllowDatapipeDrainedAsBytesConsumerInBFCache",
                "When enabled, allow pages with drained datapipe into bfcache."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_USE_INITIAL_NETWORK_STATE_AT_STARTUP,
                "Use initial network state at startup"),
        Flag.baseFeature(
                BlinkFeatures.ALLOW_JAVA_SCRIPT_TO_RESET_AUTOFILL_STATE,
                "When enabled, Autofill will reset the autofill state of fields modified by JS"),
        Flag.baseFeature("StandardCompliantNonSpecialSchemeURLParsing"),
        Flag.baseFeature(
                BlinkFeatures.BLINK_SCHEDULER_DISCRETE_INPUT_MATCHES_RESPONSIVENESS_METRICS,
                "If enabled, the scheduler filters discrete input based on responsivness metrics"
                        + " definitions"),
        Flag.baseFeature(
                BlinkFeatures.CURSOR_ANCHOR_INFO_MOJO_PIPE,
                "If enabled, CursorAnchorInfo is sent from Blink to the browser using a single"
                        + " IPC."),
        Flag.baseFeature(
                NetworkServiceFeatures.AVOID_RESOURCE_REQUEST_COPIES,
                "Avoids copying ResourceRequest when possible."),
        Flag.baseFeature(
                BlinkFeatures.LOWER_HIGH_RESOLUTION_TIMER_THRESHOLD,
                "Schedule DOM Timers with high precision only if their deadline is <4ms."),
        Flag.baseFeature(
                "InputStreamOptimizations", "Enables optimizations to input stream handling."),
        Flag.baseFeature("WebViewOptimizeXrwNavigationFlow"),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_ASYNC_DNS, "Enables the built-in DNS resolver (Async DNS)."),
        Flag.baseFeature(
                "UseMoveNotCopyInAXTreeCombiner",
                "Enables moves instead of copies of snapshot tree data when combining updates."),
        Flag.baseFeature(
                "UseMoveNotCopyInMergeTreeUpdate",
                "Enables moves instead of copies of snapshot tree data when merging udpates."),
        Flag.baseFeature(
                "EnableHangWatcher", "Controls whether hooks for hang detection are active"),
        Flag.baseFeature(
                "MojoPredictiveAllocation",
                "Predictively allocate some serialization buffers for Mojo"),
        Flag.baseFeature("EnsureExistingRendererAlive"),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_PRELOAD_CLASSES,
                "Preloads expensive classes during WebView startup."),
        Flag.baseFeature(
                GfxSwitches.USE_SMART_REF_FOR_GPU_FENCE_HANDLE,
                "Avoids cloning of gpu fences when possible"),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_DO_NOT_SEND_ACCESSIBILITY_EVENTS_ON_GSU,
                "Do not send TYPE_VIEW_SCROLLED accessibility events on kGestureScrollUpdate acks,"
                        + " instead send them every 100ms when in a scroll gesture."),
        Flag.baseFeature(
                CcFeatures.METRICS_TRACING_CALCULATION_REDUCTION,
                "Reduces Renderer event latency attribution to only during tracing."),
        Flag.baseFeature(BlinkFeatures.STREAMLINE_RENDERER_INIT),
        Flag.baseFeature(
                BlinkFeatures.STATIC_ANIMATION_OPTIMIZATION,
                "Optimize handling of static properties during animations."),
        Flag.baseFeature("LazyBindJsInjection"),
        Flag.baseFeature(AwFeatures.WEBVIEW_MUTE_AUDIO, "Enables WebView audio to be muted."),
        Flag.baseFeature(
                BlinkFeatures.CONCURRENT_VIEW_TRANSITIONS_SPA,
                "Allows concurrent transitions in local frames rendered in the same process"),
        Flag.baseFeature("WebViewVizUseThreadPool"),
        Flag.baseFeature("InProcessGpuUseIOThread"),
        Flag.baseFeature("EnableCustomInputStreamBufferSize"),
        Flag.baseFeature("NetworkServiceDedicatedThread"),
        Flag.baseFeature("BrowserThreadPoolAdjustment"),
        Flag.commandLine(
                CookieSwitches.DISABLE_PARTITIONED_COOKIES_SWITCH,
                "Disables paritioned cookies in WebView"),
        Flag.baseFeature(ContentFeatures.DIPS, "Enables the Bounce Tracking Mitigations feature."),
        Flag.baseFeature(
                "LevelDBProtoAsyncWrite",
                "Makes writes to leveldb_proto databases asynchronous. This should reduce disk"
                    + " contention at the cost of potential lost writes on OS or power failure."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_SEPARATE_RESOURCE_CONTEXT,
                "Use WebView's own Context for Resources rather than the embedding app's"),
        Flag.baseFeature(
                BlinkFeatures.STANDARDIZED_BROWSER_ZOOM,
                "Enable conformance to the new HTML specification for CSS zoom."),
        Flag.baseFeature("UseContextSnapshot"),
        Flag.baseFeature(
                CcFeatures.WAIT_FOR_LATE_SCROLL_EVENTS,
                "While scrolling, attempts to wait for late arriving input events before"
                        + " rendering."),
        Flag.baseFeature(
                CcFeatures.EVICTION_THROTTLES_DRAW,
                "Enables Renderers to not draw and submit frames when they've been evicted by the"
                        + " GPU process."),
        Flag.baseFeature(
                CcFeatures.DONT_ALWAYS_PUSH_PICTURE_LAYER_IMPLS,
                "Stop always pushing PictureLayerImpl properties on tree Activation."),
        Flag.baseFeature(
                AccessibilityFeatures.ACCESSIBILITY_PRUNE_REDUNDANT_INLINE_TEXT,
                "Prune redundant text for AX inline text boxes during serialization"),
        Flag.baseFeature(
                ContentFeatures.DEFER_SPECULATIVE_RFH_CREATION,
                "Enables deferring the speculative render frame host creation when the"
                        + "navigation starts"),
        Flag.baseFeature(ContentFeatures.PWA_NAVIGATION_CAPTURING),
        Flag.baseFeature("TransportSecurityFileWriterSchedule"),
        Flag.commandLine(
                AwSwitches.WEBVIEW_INTERCEPTED_COOKIE_HEADER,
                "When enabled, the cookie header will be included in the request headers"
                        + " for shouldInterceptRequest"),
        Flag.baseFeature(
                VizFeatures.RENDER_PASS_DRAWN_RECT,
                "Enable optimization for tracking damage in a drawn rect for each render pass."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_HYPERLINK_CONTEXT_MENU,
                "Enables hyperlink context menu in WebView"),
        Flag.baseFeature("MojoUseBinder"),
        Flag.baseFeature(
                ContentFeatures.WEB_PERMISSIONS_API, "Enables navigator.permissions.query()"),
        Flag.baseFeature(
                MediaFeatures.BUILT_IN_H264_DECODER, "Controls use of FFmpeg for H.264 decoding"),
        Flag.baseFeature(
                BlinkFeatures.DEFER_RENDERER_TASKS_AFTER_INPUT,
                "If enabled, some renderer tasks will be deferred after discrete input events, e.g."
                        + " keypress, and the subsequent frame"),
        Flag.baseFeature(
                SensitiveContentFeatures.SENSITIVE_CONTENT,
                "Redact sensitive content during screen sharing, screen recording, and similar"
                        + " actions"),
        Flag.baseFeature(
                BlinkFeatures.PLZ_DEDICATED_WORKER,
                "Enable PlzDedicatedWorker. This affects how some URLs are sent to"
                        + " WebViewClient.shouldInterceptRequest()"),
        Flag.baseFeature(
                "BlinkUseLargeEmptySlotSpanRingForBufferRoot",
                "Tuning memory allocator for speed - large empty slot span ring for Blink buffer"
                        + " root"),
        Flag.baseFeature(
                "PartitionAllocAdjustSizeWhenInForeground",
                "Tuning memory allocator for speed - adjustments for foreground vs. background"
                        + " use"),
        Flag.baseFeature(
                "PartitionAllocLargeEmptySlotSpanRing",
                "Tuning memory allocator for speed - large empty slot span ring"),
        Flag.baseFeature(
                "UsePollForMessagePumpEpoll",
                "Uses poll() instead of epoll() for MessagePumpEpoll"),
        Flag.baseFeature(
                "SqlWALModeOnWebDatabase",
                "Enables Write-Ahead Logging (WAL) mode for the SQLite database used by the"
                        + " Chromium components that WebView relies on"),
        Flag.baseFeature("ServiceWorkerAvoidMainThreadForInitialization"),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_DIGITAL_ASSET_LINKS_LOAD_INCLUDES,
                "Enable loading include statements when checking digital asset links."),
        Flag.baseFeature("PrefetchNewWaitLoop"),
        Flag.baseFeature("DirectCompositorThreadIpc"),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_WEBAUTHN,
                "Enable WebAuthn setWebAuthenticationSupport / getWebAuthenticationSupport APIs."),
        Flag.baseFeature(
                CcFeatures.THROTTLE_FRAME_RATE_ON_MANY_DID_NOT_PRODUCE_FRAME,
                "Reduce frame rate when pixels aren't updated for many frames"),
        Flag.baseFeature(
                "MojoMessageAlwaysUseLatestVersion",
                "Performance experiment to always use the latest (largest) message version."),
        Flag.baseFeature(
                BlinkFeatures.BF_CACHE_OPEN_BROADCAST_CHANNEL,
                "Start putting pages with broadcast channel into bfcache."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_LAZY_FETCH_HAND_WRITING_ICON, "Fetch Hand Writing icon lazily"),
        Flag.baseFeature(
                ContentFeatures.IGNORE_DUPLICATE_NAVS,
                "Ignore duplicate navigations, keeping the older navigations instead."),
        Flag.baseFeature(
                ContentFeatures.USE_BROWSER_CALCULATED_ORIGIN,
                "Use origin calculated in the browser process rather than renderer process for"
                        + " navigations."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_AUTO_GRANT_SANITIZED_CLIPBOARD_WRITE,
                "Auto-grant clipboard sanitized text write permission with user gesture. "
                        + "Enabled by default."),
        Flag.baseFeature(
                "AllowSensorsToEnterBfcache",
                "Allow pages with sensors to enter back/forward cache."),
        Flag.baseFeature(
                BlinkFeatures.FONTATIONS_FONT_BACKEND,
                "Enables the Fontations font backend for web fonts."),
        // Add new commandline switches and features above. The final entry should have a
        // trailing comma for cleaner diffs.
    };
}
