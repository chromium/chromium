// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

import org.chromium.base.BaseSwitches;
import org.chromium.blink_public.common.BlinkFeatures;
import org.chromium.cc.base.CcSwitches;
import org.chromium.components.metrics.MetricsSwitches;
import org.chromium.components.power_scheduler.PowerSchedulerFeatures;
import org.chromium.components.viz.common.VizFeatures;
import org.chromium.gpu.config.GpuFeatures;
import org.chromium.gpu.config.GpuSwitches;
import org.chromium.services.network.NetworkServiceFeatures;
import org.chromium.ui.base.UiFeatures;

/**
 * List of experimental features/flags supported for user devices. Add features/flags to this list
 * with scrutiny: any feature/flag in this list can be enabled for production Android devices, and
 * so it must not compromise the Android security model (i.e., WebView must still protect the app's
 * private data from being user visible).
 *
 * <p>
 * This lives in the common package so it can be shared by dev UI (to know which features/flags to
 * display) as well as the WebView implementation (so it knows which features/flags are safe to
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
            Flag.baseFeature(GpuFeatures.WEBVIEW_VULKAN,
                    "Use Vulkan for composite. Requires Android device and OS support. May crash "
                            + "if enabled on unsupported device."),
            Flag.baseFeature(GpuFeatures.WEBVIEW_SURFACE_CONTROL,
                    "Use SurfaceControl. Requires WebViewZeroCopyVideo and Android device and OS "
                            + "support."),
            Flag.baseFeature(GpuFeatures.WEBVIEW_ZERO_COPY_VIDEO,
                    "Avoid extra copy for video frames when possible"),
            Flag.baseFeature(
                    VizFeatures.WEBVIEW_VULKAN_INTERMEDIATE_BUFFER, "For debugging vulkan"),
            Flag.baseFeature(
                    GpuFeatures.USE_GLES2_FOR_OOP_R, "Force Skia context to use es2 only."),
            Flag.baseFeature(AwFeatures.WEBVIEW_CONNECTIONLESS_SAFE_BROWSING,
                    "Uses GooglePlayService's 'connectionless' APIs for Safe Browsing "
                            + "security checks."),
            Flag.baseFeature(AwFeatures.WEBVIEW_BROTLI_SUPPORT,
                    "Enables brotli compression support in WebView."),
            Flag.baseFeature(BlinkFeatures.APP_CACHE,
                    "Controls AppCache to facilitate testing against future removal."),
            Flag.baseFeature(AwFeatures.WEBVIEW_EXTRA_HEADERS_SAME_ORIGIN_ONLY,
                    "Only allow extra headers added via loadUrl() to be sent to the same origin "
                            + "as the original request."),
            Flag.baseFeature(AwFeatures.WEBVIEW_MEASURE_SCREEN_COVERAGE,
                    "Measure the number of pixels occupied by one or more WebViews as a proportion "
                            + "of the total screen size. Depending on the number of WebViews and "
                            + "the size of the screen this might be expensive so hidden behind a "
                            + "feature flag until the true runtime cost can be measured."),
            Flag.baseFeature(AwFeatures.WEBVIEW_DISPLAY_CUTOUT,
                    "Enables display cutout (notch) support in WebView for Android P and above."),
            Flag.baseFeature(PowerSchedulerFeatures.WEBVIEW_CPU_AFFINITY_RESTRICT_TO_LITTLE_CORES,
                    "Forces WebView to do rendering work on LITTLE CPU cores on big.LITTLE "
                            + "architectures"),
            Flag.baseFeature(PowerSchedulerFeatures.WEBVIEW_POWER_SCHEDULER_THROTTLE_IDLE,
                    "Restricts all of WebView's out-of-process renderer threads to use only LITTLE "
                            + "CPU cores on big.LITTLE architectures when the power mode is idle. "
                            + "WebViewCpuAffinityRestrictToLittleCores, if set, takes precedence "
                            + "over this flag."),
            Flag.baseFeature(PowerSchedulerFeatures.POWER_SCHEDULER,
                    "Enables the Power Scheduler. Defaults to throttling when idle or in no-op "
                            + "animations, if at least 250ms of CPU time were spent "
                            + "in the first 500ms after entering idle/no-op animation mode. "
                            + "Can be further configured via field trial parameters, "
                            + "see power_scheduler.h/cc for details."),
            Flag.baseFeature(BlinkFeatures.WEBVIEW_ACCELERATE_SMALL_CANVASES,
                    "Accelerate all canvases in webview."),
            Flag.baseFeature(AwFeatures.WEBVIEW_MIXED_CONTENT_AUTOUPGRADES,
                    "Enables autoupgrades for audio/video/image mixed content when mixed content "
                            + "mode is set to MIXED_CONTENT_COMPATIBILITY_MODE"),
            Flag.baseFeature(UiFeatures.SWIPE_TO_MOVE_CURSOR,
                    "Enables swipe to move cursor feature."
                            + "This flag will only take effect on Android 11 and above."),
            Flag.baseFeature(AwFeatures.WEBVIEW_JAVA_JS_BRIDGE_MOJO,
                    "Enables the new Java/JS Bridge code path with mojo implementation."),
            Flag.baseFeature(
                    AwFeatures.WEBVIEW_ORIGIN_TRIALS, "Enables Origin Trials support on WebView."),
            Flag.baseFeature(
                    BlinkFeatures.LAYOUT_NG_TABLE, "Enables Blink's next generation table layout."),
            Flag.baseFeature(
                    NetworkServiceFeatures.TRUST_TOKENS, "Enables the prototype Trust Tokens API."),
            Flag.baseFeature(AwFeatures.WEBVIEW_APPS_PACKAGE_NAMES_ALLOWLIST,
                    "Enables using a server-defined allowlist of apps whose name can be recorded "
                            + "in UMA logs. The allowlist is downloaded and fetched via component "
                            + "updater services in WebView."),
            Flag.commandLine(AwSwitches.WEBVIEW_DISABLE_APPS_PACKAGE_NAMES_ALLOWLIST_COMPONENT,
                    "Disable downloading the apps package names allowlist component by the "
                            + "component updater."),
    };
}
