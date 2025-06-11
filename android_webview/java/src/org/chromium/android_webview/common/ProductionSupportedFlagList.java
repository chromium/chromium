// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

import org.chromium.base.BaseFeatures;
import org.chromium.base.BaseSwitches;
import org.chromium.blink_public.common.BlinkFeatures;
import org.chromium.build.annotations.NullMarked;
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
import org.chromium.components.payments.PaymentFeatureList;
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
import org.chromium.net.NetFeatures;
import org.chromium.services.network.NetworkServiceFeatures;
import org.chromium.services.tracing.TracingServiceFeatures;
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
@NullMarked
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
                GpuFeatures.USE_HARDWARE_BUFFER_USAGE_FLAGS_FROM_VULKAN,
                "Allows querying recommeded AHardwareBuffer usage flags from Vulkan API. Has effect"
                        + " only if HWUI uses Vulkan."),
        Flag.baseFeature(
                VizFeatures.WEBVIEW_NEW_INVALIDATE_HEURISTIC,
                "More robust heuristic for calling Invalidate"),
        Flag.baseFeature(VizFeatures.WEBVIEW_VULKAN_INTERMEDIATE_BUFFER, "For debugging vulkan"),
        Flag.baseFeature(
                VizFeatures.WEBVIEW_ENABLE_ADPF, "Pass WebView threads to HWUI ADPF session"),
        Flag.baseFeature(
                VizFeatures.WEBVIEW_ENABLE_ADPF_GPU_MAIN, "Include GPU Main into ADPF session"),
        Flag.baseFeature(
                VizFeatures.WEBVIEW_ENABLE_ADPF_RENDERER_MAIN,
                "Include Renderer Main into ADPF session"),
        Flag.baseFeature(
                VizFeatures.ALLOW_UNDAMAGED_NONROOT_RENDER_PASS_TO_SKIP,
                "Enable optimization for skipping undamaged nonroot render passes."),
        Flag.baseFeature(
                VizFeatures.DRAW_IMMEDIATELY_WHEN_INTERACTIVE,
                "Enable optimization for immediate activation and draw when interactive."),
        Flag.baseFeature(
                VizFeatures.AVOID_DUPLICATE_DELAY_BEGIN_FRAME,
                "For epsilonic judder avoid sending duplicate (delay source) begin frames."),
        Flag.baseFeature(
                VizFeatures.ACK_ON_SURFACE_ACTIVATION_WHEN_INTERACTIVE,
                "Enable immediately sending acks to clients when a viz surface activates and when"
                        + " that surface is a dependency of an interactive frame (i.e., when there"
                        + " is an active scroll or a touch interaction). This effectively removes"
                        + " back-pressure in this case. This can result in wasted work and "
                        + " contention, but should regularize the timing of client rendering."),
        Flag.baseFeature(GpuFeatures.USE_GLES2_FOR_OOP_R, "Force Skia context to use es2 only."),
        Flag.baseFeature(
                NetFeatures.USE_NEW_ALPS_CODEPOINT_QUIC,
                "Enables using the new ALPS codepoint to negotiate application settings for QUIC."),
        Flag.baseFeature(
                NetFeatures.USE_NEW_ALPS_CODEPOINT_HTTP2,
                "Enables using the new ALPS codepoint to negotiate application settings for"
                        + " HTTP2."),
        Flag.baseFeature(
                BlinkFeatures.LAYOUT_NG_SHAPE_CACHE, "Cache shape results for short text blocks."),
        Flag.baseFeature(
                NetFeatures.SIMDUTF_BASE64_SUPPORT,
                "Use the simdutf library to base64 decode data: URLs."),
        Flag.baseFeature(
                NetFeatures.FURTHER_OPTIMIZE_PARSING_DATA_URLS,
                "Further optimize parsing data: URLs."),
        Flag.baseFeature(
                BlinkFeatures.PRELOAD_LINK_REL_DATA_URLS,
                "Allow preloading data: URLs with link rel=preload"),
        Flag.baseFeature(BlinkFeatures.OPTIMIZE_HTML_ELEMENT_URLS, "Optimize HTML Element URLs"),
        Flag.baseFeature(
                BlinkFeatures.DOCUMENT_POLICY_EXPECT_NO_LINKED_RESOURCES,
                "Enables the ability to use Document Policy header to control feature"
                        + " ExpectNoLinkedResources."),
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
                AndroidAutofillFeatures.ANDROID_AUTOFILL_VIRTUAL_VIEW_STRUCTURE_ANDROID_IN_CCT_NAME,
                "Disables checking AutofilManager#isEnabled too early. Mainly affects CCTs."),
        Flag.baseFeature(
                AndroidAutofillFeatures.ANDROID_AUTOFILL_LAZY_FRAMEWORK_WRAPPER_NAME,
                "Enable lazily initializing framework Autofill wrapper."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_ACCEPT_DOM_MUTATION_AFTER_AUTOFILL_SUBMISSION,
                "Accepts DOM_MUTATION_AFTER_AUTOFILL submissions detected on password forms."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_AND_PASSWORDS_IN_SAME_SURFACE,
                "Changes how password requests are passed to the embedder. Ideally a noop."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_BETTER_LOCAL_HEURISTIC_PLACEHOLDER_SUPPORT,
                "Treats placeholders as a separate signal for Autofill local heuristics"),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_ENABLE_EMAIL_HEURISTIC_OUTSIDE_FORMS,
                "Enables heuristics for detecting email fields outside of forms."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_ENABLE_EXPIRATION_DATE_IMPROVEMENTS,
                "Enables various improvements to handling expiration dates."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_ENABLE_SUPPORT_FOR_PARSING_WITH_SHARED_LABELS,
                "Splits Autofill labels among consecutive fields for better heuristic"
                        + " predictions."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_EXTRACT_INPUT_DATE, "Extracts <input type=date> fields."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_FIX_FORM_TRACKING,
                "Improves form submission tracking and duplicate submission handling"),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_IMPROVE_CITY_FIELD_CLASSIFICATION,
                "Reduces city field false positive classifications"),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_DISALLOW_SLASH_DOT_LABELS,
                "Disallows labels that only contain slashes, dots and other special characters."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_DETECT_REMOVED_FORM_CONTROLS,
                "Enables Autofill to detect if form controls are removed from the DOM"),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_OPTIMIZE_FORM_EXTRACTION,
                "Makes Autofill spend less time on extracting forms."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_ENABLE_LOYALTY_CARDS_FILLING,
                "When enabled, Autofill will offer support for filling the user's loyalty cards"
                        + " stored in Google Wallet."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_ENABLE_LOYALTY_CARDS_FILLING,
                "When enabled, Autofill will offer support for filling the user's loyalty cards"
                        + " stored in Google Wallet."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_ENABLE_EMAIL_OR_LOYALTY_CARDS_FILLING,
                "When enabled, Autofill will offer support for Autofill suggestions on fields "
                        + "requesting email or loyalty card values."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_PAGE_LANGUAGE_DETECTION,
                "Enables Autofill to retrieve the page language for form parsing."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_PARSE_EMAIL_LABEL_AND_PLACEHOLDER,
                "Classifies fields as email fields if their label or placeholder have valid email"
                        + " format."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_PREFER_SAVED_FORM_AS_SUBMITTED_FORM,
                "When enabled, Autofill will start preferring the saved form over performing form "
                        + "extraction at submission time, and only use the latter as a fallback."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_REPLACE_CACHED_WEB_ELEMENTS_BY_RENDERER_IDS,
                "When enabled, AutofillAgent will store its cached form and fields as renderer ids "
                        + "instead of holding strong references to blink::WebElement objects."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_UNIFY_RATIONALIZATION_AND_SECTIONING_ORDER,
                "When enabled, the same rationalization/sectioning order is used for heuristic and"
                        + " server predictions."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_USE_FR_ADDRESS_MODEL,
                "When enabled, Autofill uses a custom address model for France."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_USE_IN_ADDRESS_MODEL,
                "When enabled, Autofill uses a custom address model for India."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_USE_NL_ADDRESS_MODEL,
                "When enabled, Autofill uses a custom address model for the Netherlands."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_SUPPORT_LAST_NAME_PREFIX,
                "When enabled, Autofill uses a custom name hierarchy for parsing last names."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_USE_NEGATIVE_PATTERN_FOR_ALL_ATTRIBUTES,
                "When enabled, parser won't try to match other attributes if any of the negative"
                        + " patterns matched."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_USE_SUBMITTED_FORM_IN_HTML_SUBMISSION,
                "When enabled, Autofill will start falling back to the saved form when HTML"
                        + " submission happens and form extraction fails."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_GREEK_REGEXES,
                "When enabled, Greek regexes are used for parsing in branded builds."),
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
                AutofillFeatures.AUTOFILL_SUPPORT_PHONETIC_NAME_FOR_JP,
                "When enabled, Autofill will support phonetic name for Japan."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_THROTTLE_ASK_FOR_VALUES_TO_FILL,
                "When enabled, Autofill throttles duplicate AskForValuesToFill() events."),
        Flag.baseFeature(
                AutofillFeatures.AUTOFILL_UKM_EXPERIMENTAL_FIELDS,
                "Enables UKM collection for experimental fields"),
        Flag.baseFeature(
                FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_VIRTUAL_CARD_FEATURE,
                "When enabled, merchant bound virtual cards will be offered in the keyboard "
                        + "accessory."),
        Flag.baseFeature(
                NetworkServiceFeatures.MASKED_DOMAIN_LIST,
                "When enabled, the masked domain list required for IP Protection is loaded."),
        Flag.commandLine(
                AwSwitches.WEBVIEW_SELECTIVE_IMAGE_INVERSION_DARKENING,
                "Enables use selective image inversion to automatically darken page, it will be"
                        + " used when WebView is in dark mode, but website doesn't provide dark"
                        + " style."),
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
                BlinkFeatures.COMPOSITE_CLIP_PATH_ANIMATION,
                "When enabled, clip-path animations run on the compositor thread."),
        Flag.baseFeature(
                CcFeatures.DEFER_IMPL_INVALIDATION,
                "Allow main thread additional time to respond before creating a pending tree"),
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
                BlinkFeatures.SET_INTERVAL_WITHOUT_CLAMP,
                "Enables faster setInterval(,0) by removing the 1 ms clamping."),
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
                BlinkFeatures.REDUCE_USER_AGENT_MINOR_VERSION,
                "Enables reduce webview user-agent minor version."),
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
                NetworkServiceFeatures.DEPRECATE_UNLOAD,
                "If false prevents the gradual deprecation of the unload event."),
        Flag.baseFeature(
                NetworkServiceFeatures.DEPRECATE_UNLOAD_BY_ALLOW_LIST,
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
        Flag.baseFeature(BaseFeatures.ALIGN_WAKE_UPS, "Align delayed wake ups at 125 Hz"),
        Flag.baseFeature(
                GpuFeatures.INCREASED_CMD_BUFFER_PARSE_SLICE,
                "Enable the use of an increased parse slice size per command buffer before"
                        + " each forced context switch."),
        Flag.baseFeature(
                BlinkFeatures.REPORT_EVENT_TIMING_AT_VISIBILITY_CHANGE,
                "Report event timing to UKM at visibility change."),
        Flag.baseFeature(
                CcFeatures.USE_DMSAA_FOR_TILES,
                "Switches skia to use DMSAA instead of MSAA for tile raster"),
        Flag.baseFeature(
                BlinkFeatures.THREADED_BODY_LOADER,
                "If enabled, reads and decodes navigation body data off the main thread."),
        Flag.baseFeature(BlinkFeatures.EXPAND_COMPOSITED_CULL_RECT),
        Flag.baseFeature(CcFeatures.NEW_CONTENT_FOR_CHECKERBOARDED_SCROLLS),
        Flag.baseFeature(CcFeatures.PRESERVE_DISCARDABLE_IMAGE_MAP_QUALITY),
        Flag.baseFeature(BlinkFeatures.SCROLLBAR_COLOR),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_FILE_SYSTEM_ACCESS, "Enables JS File System Access API"),
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
                MetricsFeatures.FLUSH_PERSISTENT_SYSTEM_PROFILE_ON_WRITE,
                "Controls whether to schedule a flush of persistent histogram memory "
                        + "immediately after writing a system profile to it."),
        Flag.baseFeature(
                MetricsFeatures.REPORTING_SERVICE_ALWAYS_FLUSH,
                "Determines whether to always flush Local State immediately after an UMA/UKM "
                        + "log upload."),
        Flag.baseFeature(
                MetricsFeatures.METRICS_LOG_TRIMMING, "Controls trimming for metrics logs."),
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
                "AddWarningShownTSToClientSafeBrowsingReport",
                "When enabled, client reports will include a timestamp of when the warning was "
                        + "shown to the user"),
        Flag.baseFeature(
                "CreateWarningShownClientSafeBrowsingReports",
                "When enabled, WARNING_SHOWN client reports will be sent when a warning is "
                        + "shown to the user"),
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
        Flag.baseFeature(NetFeatures.HAPPY_EYEBALLS_V3, "Enables Happy Eyeballs V3"),
        Flag.baseFeature(NetFeatures.ENABLE_TLS13_EARLY_DATA, "Enables TLS 1.3 Early Data"),
        Flag.baseFeature(
                NetFeatures.HTTP_CACHE_NO_VARY_SEARCH,
                "Enables support for the No-Vary-Search response header in the HTTP disk cache"),
        Flag.baseFeature("MojoIpcz"),
        Flag.baseFeature(
                "FixDataPipeTrapBug",
                "Used to disable a specific bug fix for a long-standing bug that may"
                        + " have affected performance. Brief experiment for data collection"),
        Flag.baseFeature(
                TracingServiceFeatures.ENABLE_PERFETTO_SYSTEM_TRACING,
                "When enabled, WebView exports trace events to the Android Perfetto service."
                        + " This works only for Android Q+."),
        Flag.baseFeature(UiAndroidFeatures.ANDROID_HDR, "Enables HDR support"),
        Flag.baseFeature(
                UiAndroidFeatures.DEPRECATED_EXTERNAL_PICKER_FUNCTION,
                "Deprecates old external file picker function."),
        Flag.baseFeature(BaseFeatures.THREAD_POOL_CAP2, "Sets a fixed thread pool cap"),
        Flag.baseFeature("ThreadGroupSemaphore"),
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
        Flag.baseFeature(
                ContentFeatures.SITE_INSTANCE_GROUPS_FOR_DATA_URLS,
                "If enabled, puts data: URL subframes in a separate SiteInstance in the same"
                        + "SiteInstanceGroup and process as its initiator"),
        Flag.baseFeature(GpuFeatures.CONDITIONALLY_SKIP_GPU_CHANNEL_FLUSH),
        Flag.baseFeature(
                GpuFeatures.SYNC_POINT_GRAPH_VALIDATION,
                "If enabled, replaces synchronous GPU sync point validation with graph based"
                        + " validation"),
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
                "PartitionAllocUseSmallSingleSlotSpans",
                "Uses a more nuanced heuristic to classify small single-slot spans."),
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
                AwFeatures.WEBVIEW_AUTO_SAA,
                "Enable auto granting storage access API requests. This will be done "
                        + "if a relationship is detected between the app and the website."),
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
        Flag.baseFeature(
                "AudioInputConfirmReadsViaShmem",
                "Enables an audio input optimization that uses shared memory instead of"
                        + " socket messages for audio IPC read confirmations."),
        Flag.baseFeature("UseRustJsonParser"),
        Flag.baseFeature("V8BaselineBatchCompilation"),
        Flag.baseFeature("V8ConcurrentSparkplug"),
        Flag.baseFeature("V8Flag_minor_gc_task_with_lower_priority"),
        Flag.baseFeature("V8FlushCodeBasedOnTabVisibility"),
        Flag.baseFeature("V8FlushCodeBasedOnTime"),
        Flag.baseFeature("V8MemoryReducer"),
        Flag.baseFeature("V8MinorMS"),
        Flag.baseFeature("V8PreconfigureOldGen"),
        Flag.baseFeature("V8ScavengerHigherCapacity"),
        Flag.baseFeature("V8IncrementalMarkingStartUserVisible"),
        Flag.baseFeature("V8ExternalMemoryAccountedInGlobalLimit"),
        Flag.baseFeature("V8GCSpeedUsesCounters"),
        Flag.baseFeature("WebAssemblyTurboshaft"),
        Flag.baseFeature("WebAssemblyTurboshaftInstructionSelection"),
        Flag.baseFeature("WebAssemblyDeopt"),
        Flag.baseFeature("WebAssemblyInliningCallIndirect"),
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
                ContentFeatures.ACCESSIBILITY_MANAGE_BROADCAST_RECEIVER_ON_BACKGROUND,
                "Register, un-register Accessibility broadcast receiver on a background thread."),
        Flag.baseFeature(
                "BatteryStatusManagerBroadcastReceiverInBackground",
                "Register, unregister Battery Status Manager broadcast receiver on a background"
                        + " thread."),
        Flag.baseFeature(
                BlinkFeatures.INCREMENT_LOCAL_SURFACE_ID_FOR_MAINFRAME_SAME_DOC_NAVIGATION,
                "When enabled, every mainframe same-doc navigation will increment the"
                        + " `viz::LocalSurfaceId` from the impl thread."),
        Flag.baseFeature(
                BaseFeatures.BACKGROUND_NOT_PERCEPTIBLE_BINDING,
                "If enabled, not perceptible binding put processes to the background cpu cgroup"),
        Flag.baseFeature(
                BaseFeatures.PARTITION_ALLOC_WITH_ADVANCED_CHECKS,
                "Enables PartitionAlloc with advanced safety checks"),
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
                MediaFeatures.BUILT_IN_HLS_MP4,
                "Enabled the playback of HLS renditions which use the mp4 container"),
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
        Flag.baseFeature("StandardCompliantNonSpecialSchemeURLParsing"),
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
                "EnableHangWatcher", "Controls whether hooks for hang detection are active"),
        Flag.baseFeature(
                "MojoPredictiveAllocation",
                "Predictively allocate some serialization buffers for Mojo"),
        Flag.baseFeature("EnsureExistingRendererAlive"),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_PRELOAD_CLASSES,
                "Preloads expensive classes during WebView startup."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_PREFETCH_NATIVE_LIBRARY,
                "Prefetches the native WebView code to memory during startup."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_RECORD_APP_CACHE_HISTOGRAMS,
                "When enabled, records histograms relating to app's cache size."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_CACHE_SIZE_LIMIT_DERIVED_FROM_APP_CACHE_QUOTA,
                "When enabled, instead of using the 20MiB as the HTTP cache limit, derive the value"
                        + " from the cache quota allocated to the app by the Android framework."),
        Flag.baseFeature(
                GfxSwitches.USE_SMART_REF_FOR_GPU_FENCE_HANDLE,
                "Avoids cloning of gpu fences when possible"),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_DO_NOT_SEND_ACCESSIBILITY_EVENTS_ON_GSU,
                "Do not send TYPE_VIEW_SCROLLED accessibility events on kGestureScrollUpdate acks,"
                        + " instead send them every 100ms when in a scroll gesture."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_DRAIN_PREFETCH_QUEUE_DURING_INIT,
                "Drain any prefetches that were triggered on the background thread during WebView"
                        + " initialization."),
        Flag.baseFeature(
                CcFeatures.METRICS_TRACING_CALCULATION_REDUCTION,
                "Reduces Renderer event latency attribution to only during tracing."),
        Flag.baseFeature(BlinkFeatures.STREAMLINE_RENDERER_INIT),
        Flag.baseFeature("LazyBindJsInjection"),
        Flag.baseFeature(AwFeatures.WEBVIEW_MUTE_AUDIO, "Enables WebView audio to be muted."),
        Flag.baseFeature("WebViewVizUseThreadPool"),
        Flag.baseFeature("InProcessGpuUseIOThread"),
        Flag.baseFeature("EnableCustomInputStreamBufferSize"),
        Flag.baseFeature("NetworkServiceDedicatedThread"),
        Flag.baseFeature("BrowserThreadPoolAdjustment"),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_DISABLE_CHIPS,
                "Disables partitioned cookies in WebView by default. Will require an additional"
                        + " restart of WebView to take effect."),
        Flag.baseFeature(ContentFeatures.BTM, "Enables the Bounce Tracking Mitigations feature."),
        Flag.baseFeature(
                "LevelDBProtoAsyncWrite",
                "Makes writes to leveldb_proto databases asynchronous. This should reduce disk"
                    + " contention at the cost of potential lost writes on OS or power failure."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_SAFE_AREA_INCLUDES_SYSTEM_BARS,
                "Include system bars in safe-area-inset CSS environment values for WebViews"
                        + " that take up the entire screen."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_SEPARATE_RESOURCE_CONTEXT,
                "Use WebView's own Context for Resources rather than the embedding app's"),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_SKIP_INTERCEPTS_FOR_PREFETCH,
                "Skip shouldInterceptRequest and other checks for prefetch requests."),
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
        Flag.baseFeature(CcFeatures.CC_SLIMMING, "Reduce unnecessary work in CC frame updates."),
        Flag.baseFeature(
                ContentFeatures.DEFER_SPECULATIVE_RFH_CREATION,
                "Enables deferring the speculative render frame host creation when the"
                        + "navigation starts"),
        Flag.baseFeature(ContentFeatures.PWA_NAVIGATION_CAPTURING),
        Flag.baseFeature("TransportSecurityFileWriterSchedule"),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_HYPERLINK_CONTEXT_MENU,
                "Enables hyperlink context menu in WebView"),
        Flag.baseFeature("MojoUseBinder"),
        Flag.baseFeature(
                ContentFeatures.WEB_PERMISSIONS_API, "Enables navigator.permissions.query()"),
        Flag.baseFeature(
                BlinkFeatures.DEFER_RENDERER_TASKS_AFTER_INPUT,
                "If enabled, some renderer tasks will be deferred after discrete input events, e.g."
                        + " keypress, and the subsequent frame"),
        Flag.baseFeature(
                SensitiveContentFeatures.SENSITIVE_CONTENT,
                "Redact sensitive content during screen sharing, screen recording, and similar"
                        + " actions"),
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
        Flag.baseFeature("DirectCompositorThreadIpc"),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_WEBAUTHN,
                "Enable WebAuthn setWebAuthenticationSupport / getWebAuthenticationSupport APIs."),
        Flag.baseFeature(
                BlinkFeatures.BF_CACHE_OPEN_BROADCAST_CHANNEL,
                "Start putting pages with broadcast channel into bfcache."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_LAZY_FETCH_HAND_WRITING_ICON, "Fetch Hand Writing icon lazily"),
        Flag.baseFeature(
                ContentFeatures.IGNORE_DUPLICATE_NAVS,
                "Ignore duplicate navigations, keeping the older navigations instead."),
        Flag.baseFeature(
                "AllowSensorsToEnterBfcache",
                "Allow pages with sensors to enter back/forward cache."),
        Flag.baseFeature("OverrideAPIKey"),
        Flag.baseFeature(
                "RustyPng", "When enabled, uses Rust `png` crate to decode and encode PNG images."),
        Flag.baseFeature(
                BlinkFeatures.ESCAPE_LT_GT_IN_ATTRIBUTES,
                "When enabled, less-than and greater-than characters in attributes are escaped."),
        Flag.baseFeature("CacheStylusSettings", "Cache stylus related settings."),
        Flag.baseFeature(
                "AsyncFastCheckout", "When enabled, run FastCheckoutTabHelper asynchronously."),
        Flag.baseFeature("Prerender2FallbackPrefetchSpecRules"),
        Flag.baseFeature("PrefetchReusable"),
        Flag.baseFeature(
                "LCPTimingPredictorPrerender2",
                "When enabled, Prerender2 by Speculation Rules API is delayed until LCP is"
                        + " finished."),
        Flag.baseFeature(
                "SelectParserRelaxation",
                "Enables new HTML parser behavior for the <select> element."),
        Flag.baseFeature(
                "CSSReadingFlow",
                "Enables new CSS reading-flow property for focus navigation in visual order."),
        Flag.baseFeature(
                "SimpleCachePrioritizedCaching",
                "When enabled, main frame navigation resources will be prioritized in Simple"
                        + " Cache."),
        Flag.baseFeature(
                CcFeatures.PREVENT_DUPLICATE_IMAGE_DECODES,
                "De-duplicate and share image decode requests between raster tasks "
                        + "and javascript image decode requests."),
        Flag.baseFeature(
                CcFeatures.SEND_EXPLICIT_DECODE_REQUESTS_IMMEDIATELY,
                "Forward javascript image decode requests to cc right away, "
                        + "rather than bundling them into the next compositor commit."),
        Flag.baseFeature(
                BlinkFeatures.SPECULATIVE_IMAGE_DECODES,
                "Start decoding in-viewport images as soon as they have loaded, "
                        + "rather than waiting for them to appear in a raster task."),
        Flag.baseFeature(
                BlinkFeatures.STANDARDIZED_TIMER_CLAMPING,
                "Clamp nested timers according to the spec."),
        Flag.baseFeature(
                MediaFeatures.MEDIA_CODEC_BLOCK_MODEL,
                "Controls use of MediaCodec's LinearBlock mode."),
        Flag.baseFeature(BlinkFeatures.FETCH_LATER_API, "Enables FetchLater API."),
        Flag.baseFeature(
                ContentFeatures.WEB_PAYMENTS,
                "Enable the JavaScript PaymentRequest API for launching payment apps through"
                        + " Android intents."),
        Flag.baseFeature(
                PaymentFeatureList.UPDATE_PAYMENT_DETAILS_INTENT_FILTER_IN_PAYMENT_APP,
                "PaymentRequest looks up the dynamic price updates service in the payment app,"
                        + " via an intent filter."),
        Flag.baseFeature(
                PaymentFeatureList.ANDROID_PAYMENT_INTENTS_OMIT_DEPRECATED_PARAMETERS,
                "Omit the deprecated parameters from the intents that are sent to "
                        + "Android payment apps in the PaymentRequest API."),
        Flag.baseFeature(
                GpuFeatures.WEB_GPU_USE_VULKAN_MEMORY_MODEL,
                "Use the Vulkan Memory Model from WebGPU when available"),
        Flag.baseFeature("RunBeforeUnloadClosureOnStackInvestigation"),
        Flag.baseFeature(NetworkServiceFeatures.SHARED_STORAGE_API, "Enable Shared Storage API."),
        Flag.baseFeature(BlinkFeatures.FENCED_FRAMES, "Enable Fenced Frames HTML Element."),
        Flag.baseFeature(
                BlinkFeatures.FENCED_FRAMES_API_CHANGES,
                "Enable Fenced Frames HTML Element extra APIs."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_SHORT_CIRCUIT_SHOULD_INTERCEPT_REQUEST,
                "Short circuit shouldInterceptRequest calls when they're not overridden."),
        Flag.baseFeature(
                BlinkFeatures.MEMORY_SAVER_MODE_RENDER_TUNING,
                "Enables v8 memory saver mode on low memory thresholds."),
        Flag.baseFeature(
                NetworkServiceFeatures.RENDERER_SIDE_CONTENT_DECODING,
                "Enable renderer-side content decoding (decompression)."),
        Flag.baseFeature(
                NetworkServiceFeatures.DEVICE_BOUND_SESSION_ACCESS_OBSERVER_SHARED_REMOTE,
                "Enable the optimization of reducing unnecessary IPC for cloning"
                        + " DeviceBoundSessionAccessObserver."),
        Flag.commandLine(
                AwSwitches.WEBVIEW_USE_STARTUP_TASKS_LOGIC,
                "When enabled, webview chromium initialization uses the startup tasks logic where"
                        + " it:\n"
                        + " - runs the startup tasks asynchronously if startup is triggered from a"
                        + " background thread. Otherwise runs startup synchronously.\n"
                        + " - caches any chromium startup exception and rethrows it if startup is"
                        + " retried without a restart."),
        Flag.baseFeature(
                CcFeatures.EXPORT_FRAME_TIMING_AFTER_FRAME_DONE,
                "When enabled, moves the layer tree client's metric export call for from beginning"
                        + " of the subsequent frame to the end of the subsequent frame."),
        Flag.baseFeature(
                BlinkFeatures.ASYNC_SET_COOKIE,
                "When enabled, the communication between renderer and network service is "
                        + "asynchronous when setting cookies."),
        Flag.baseFeature(
                NetworkServiceFeatures.GET_COOKIES_ON_SET,
                "When enabled, the network service returns all the cookies when setting a new "
                        + "cookie, so that it can be cached."),
        Flag.baseFeature(
                NetworkServiceFeatures.INCREASE_COOKIE_ACCESS_CACHE_SIZE,
                "When enabled, keep more cookies in the cache to be able to skip redundant access"
                        + " notifications."),
        Flag.baseFeature(
                MediaFeatures.MULTI_BUFFER_NEVER_DEFER,
                "Controls behavior of network deferrals during media src=file playbacks."),
        Flag.baseFeature("PrefetchScheduler"),
        Flag.baseFeature(
                BlinkFeatures.RENDER_BLOCKING_FULL_FRAME_RATE,
                "Enable the <link blocking=\"full-frame-rate\"/> API to lower the frame rate during"
                        + " loading"),
        Flag.baseFeature("ProgressiveAccessibility"),
        Flag.baseFeature("PreloadingNoSamePageFragmentAnchorTracking"),
        Flag.baseFeature(
                NetFeatures.RESTRICT_ABUSE_PORTS_ON_LOCALHOST,
                "Used to restrict connections to specified ports on localhost."),
        Flag.baseFeature(
                AwFeatures.WEBVIEW_QUIC_CONNECTION_TIMEOUT,
                "Enables updating the QUIC connection timeout to a value set by the"
                        + " WebViewUpdateQuicConnectionTimeoutSeconds feature param."),
        Flag.baseFeature(
                NetworkServiceFeatures.SHARED_DICTIONARY_CACHE,
                "When enabled, keep recently-used compression dictionaries in a memory cache."),
        Flag.baseFeature(
                NetworkServiceFeatures.CACHE_SHARING_FOR_PERVASIVE_SCRIPTS,
                "When enabled, enables a singled-keyed HTTP cache for well-known privacy-safe"
                        + " resources."),
        Flag.baseFeature(
                "PrefetchServiceWorker",
                "Enables SpeculationRules prefetch to ServiceWorker-controlled URLs."),
        Flag.baseFeature("TimedHTMLParserBudget"),
        Flag.baseFeature("ServiceWorkerBackgroundUpdateForRegisteredStorageKeys"),
        Flag.baseFeature(
                "ServiceWorkerBackgroundUpdateForRegisteredStorageKeysFieldTrialControlled"),
        // Add new commandline switches and features above. The final entry should have a
        // trailing comma for cleaner diffs.
    };
}
