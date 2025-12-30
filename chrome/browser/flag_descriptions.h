// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FLAG_DESCRIPTIONS_H_
#define CHROME_BROWSER_FLAG_DESCRIPTIONS_H_

#include "base/debug/debugging_buildflags.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/common/buildflags.h"
#include "components/compose/buildflags.h"
#include "components/paint_preview/buildflags/buildflags.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/webui/flags/feature_entry.h"
#include "content/public/common/content_features.h"
#include "device/vr/buildflags/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "media/gpu/buildflags.h"
#include "media/media_buildflags.h"
#include "net/net_buildflags.h"
#include "pdf/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/blink/public/common/buildflags.h"

// This file declares strings used in chrome://flags. These messages are not
// translated, because instead of end-users they target Chromium developers and
// testers. See https://crbug.com/587272 and https://crbug.com/703134 for more
// details.
//
// Comments are not necessary. The contents of the strings (which appear in the
// UI) should be good enough documentation for what flags do and when they
// apply. If they aren't, fix them.
//
// Sort flags in each section alphabetically by the k...Name constant. Follow
// that by the k...Description constant and any special values associated with
// that.
//
// Please do NOT include preprocessor directives - these are no longer required.

namespace flag_descriptions {

inline constexpr char kAccelerated2dCanvasName[] = "Accelerated 2D canvas";
inline constexpr char kAccelerated2dCanvasDescription[] =
    "Enables the use of the GPU to perform 2d canvas rendering instead of "
    "using software rendering.";

inline constexpr char kAiModeOmniboxEntryPointName[] =
    "AI Mode Omnibox entrypoint";
inline constexpr char kAiModeOmniboxEntryPointDescription[] =
    "Enables icon button for AI Mode entrypoint in the Omnibox.";

inline constexpr char kAiModeEntryPointAlwaysNavigatesName[] =
    "AI Mode Omnibox Entrypoint always navigates";
inline constexpr char kAiModeEntryPointAlwaysNavigatesDescription[] =
    "If enabled, clicking aim button in omnibox always navigates directly to "
    "google.com/aimode.";

inline constexpr char kOmniboxAimServerEligibilityName[] =
    "AIM Server Eligibility";
inline constexpr char kOmniboxAimServerEligibilityDescription[] =
    "Enable AIM server eligibility checks.";

inline constexpr char kCanvasHibernationName[] = "Hibernation for 2D canvas";
inline constexpr char kCanvasHibernationDescription[] =
    "Enables canvas hibernation for 2D canvas.";

inline constexpr char kCapturedSurfaceControlName[] =
    "Captured Surface Control";
inline constexpr char kCapturedSurfaceControlDescription[] =
    "Enables an API that allows an application to control scroll and zoom on "
    "the tab which it is capturing.";

inline constexpr char kCrossTabElementCaptureName[] =
    "Element Capture cross-tab";
inline constexpr char kCrossTabElementCaptureDescription[] =
    "Allows the Element Capture API to be used cross-tab. (Only has an effect "
    "if Element Capture is generally enabled.)";

inline constexpr char kCrossTabRegionCaptureName[] = "Region Capture cross-tab";
inline constexpr char kCrossTabRegionCaptureDescription[] =
    "Allows the Region Capture API to be used cross-tab. (Only has an effect "
    "if Region Capture is generally enabled.)";

inline constexpr char kAcceleratedVideoDecodeName[] =
    "Hardware-accelerated video decode";
inline constexpr char kAcceleratedVideoDecodeDescription[] =
    "Hardware-accelerated video decode where available.";

inline constexpr char kAcceleratedVideoEncodeName[] =
    "Hardware-accelerated video encode";
inline constexpr char kAcceleratedVideoEncodeDescription[] =
    "Hardware-accelerated video encode where available.";

inline constexpr char kAlignWakeUpsName[] = "Align delayed wake ups at 125 Hz";
inline constexpr char kAlignWakeUpsDescription[] =
    "Run most delayed tasks with a non-zero delay (including DOM Timers) on a "
    "periodic 125Hz tick, instead of as soon as their delay has passed.";

inline constexpr char kAllowInsecureLocalhostName[] =
    "Allow invalid certificates for resources loaded from localhost.";
inline constexpr char kAllowInsecureLocalhostDescription[] =
    "Allows requests to localhost over HTTPS even when an invalid certificate "
    "is presented.";

inline constexpr char kAndroidAppIntegrationModuleName[] =
    "Integrate with Android App Search and shows a notice card";
inline constexpr char kAndroidAppIntegrationModuleDescription[] =
    "If enabled, allows Chrome to show a notice card on the magic stack for "
    "Android App Search integration";

inline constexpr char kNewContentForCheckerboardedScrollsName[] =
    "Change scrolling scheduling to reduce checkerboarding";
inline constexpr char kNewContentForCheckerboardedScrollsDescription[] =
    "If enabled, scrolling that would generate blank frames will now "
    "prioritize the new content over scrolling with the intention of "
    "decreasing the amount of checkerboarded frames.";

inline constexpr char kNewTabAddsToActiveGroupName[] =
    "Add new tabs to active tab group.";

inline constexpr char kNewTabAddsToActiveGroupDescription[] =
    "If enabled, and there is a tab group is focused, then new tabs "
    "will be added to the focused tab group.";

inline constexpr char kAllowNonFamilyLinkUrlFilterModeName[] =
    "Allow non-family link URL filter mode";
inline constexpr char kAllowNonFamilyLinkUrlFilterModeDescription[] =
    "Allows the URL classification mode without credentials, even if the "
    "profile is not managed by the family link System.";

inline constexpr char kAndroidAdaptiveFrameRateName[] =
    "Android Adaptive Refresh Rate features";
inline constexpr char kAndroidAdaptiveFrameRateDescription[] =
    "Enable adaptive  refresh rate features on supported devices. Feature "
    "include lowering frame rate for low speed scroll. Has no effect if device "
    "does not support adaptive refresh rate.";

inline constexpr char kHistoryOptInEducationalTipName[] =
    "History sync educational tip";
inline constexpr char kHistoryOptInEducationalTipDescription[] =
    "Enables a history sync promo in the magic stack on NTP";

inline constexpr char kThirdPartyDisableChromeAutofillSettingsScreenName[] =
    "Chrome Autofill Settings Screen in 3P Mode";
inline constexpr char
    kThirdPartyDisableChromeAutofillSettingsScreenDescription[] =
        "Chrome's Address and Payments Autofill are disabled in third party "
        "mode.";
inline constexpr char kGroupPromoPrototypeCpaName[] =
    "Group Promo Prototype - Contextual page action";
inline constexpr char kGroupPromoPrototypeCpaDescription[] =
    "Enables contextual toolbar button for group promo prototype.";

inline constexpr char kTaskManagerClankName[] = "Task Manager on Clank";
inline constexpr char kTaskManagerClankDescription[] =
    "Enables the Task Manager for Clank (Chrome on Android).";

inline constexpr char kShowNewTabAnimationsName[] = "Show New Tab Animations";
inline constexpr char kShowNewTabAnimationsDescription[] =
    "Shows new animations for creating tabs.";

inline constexpr char kNewTabPageCustomizationName[] =
    "Customize the new tab page";
inline constexpr char kNewTabPageCustomizationDescription[] =
    "If enabled, allows users to customize the new tab page";

inline constexpr char kNewTabPageCustomizationV2Name[] =
    "Customize the new tab page V2";
inline constexpr char kNewTabPageCustomizationV2Description[] =
    "Allows users to customize the new tab page, like appearance.";

inline constexpr char kNewTabPageCustomizationToolbarButtonName[] =
    "New tab page customization toolbar button";
inline constexpr char kNewTabPageCustomizationToolbarButtonDescription[] =
    "Add the new tab page customization button on the toolbar (mobile only).";

inline constexpr char kNewTabPageCustomizationForMvtName[] =
    "Customize the new tab page for Most Visiteid Tiles";
inline constexpr char kNewTabPageCustomizationForMvtDescription[] =
    "Allows users to enable or disable the Most Visiteid Tiles section";

inline constexpr char kAndroidAppearanceSettingsName[] = "Appearance Settings";
inline constexpr char kAndroidAppearanceSettingsDescription[] =
    "Enables the Appearance Settings preference screen.";

inline constexpr char kAndroidBookmarkBarName[] = "Bookmark Bar";
inline constexpr char kAndroidBookmarkBarDescription[] =
    "Enables the bookmark bar which provides users with bookmark access from "
    "top chrome. Note that device form factor restrictions also apply.";

inline constexpr char kAndroidBookmarkBarFastFollowName[] =
    "Android Bookmark Bar Fast Follow";
inline constexpr char kAndroidBookmarkBarFastFollowDescription[] =
    "Enables fast follow for the bookmark bar which adds addition "
    "functionality. This flag requires having the Android Bookmark Bar flag "
    "enabled as well.";

inline constexpr char kAndroidOpenIncognitoAsWindowName[] =
    "Open incognito tabs in new window";
inline constexpr char kAndroidOpenIncognitoAsWindowDescription[] =
    "Open regular and incognito tabs in separate windows.";

inline constexpr char kAndroidProgressBarVisualUpdateName[] =
    "Enable updated progress bar";
inline constexpr char kAndroidProgressBarVisualUpdateDescription[] =
    "Enable the new updated progress bar";

inline constexpr char kAndroidSmsOtpFillingName[] = "Enable SMS OTP filling";
inline constexpr char kAndroidSmsOtpFillingDescription[] =
    "Enables filling of OTPs received via SMS on Android";

inline constexpr char kAndroidWebAppLaunchHandlerName[] =
    "Android Web App Launch Handler";
inline constexpr char kAndroidWebAppLaunchHandlerDescription[] =
    "Enables support of launch_handler and file_handlers that allows web app "
    "developers to control how it's launched — for example if it uses an "
    "existing window or creates a new one, and to specify types of files a web "
    "app can handle.";

inline constexpr char kApproximateGeolocationPermissionName[] =
    "Approximate Geolocation Permission";
inline constexpr char kApproximateGeolocationPermissionDescription[] =
    "Enables the approximate geolocation permission prompt, with options to "
    "control prompt arm variations.";

inline constexpr char kAndroidDesktopDensityName[] = "Android Desktop Density";
inline constexpr char kAndroidDesktopDensityDescription[] =
    "Enables desktop density for some surfaces on Android.";

inline constexpr char kAndroidAppIntegrationMultiDataSourceName[] =
    "Integrate with Android App Search with multiple data sources.";
inline constexpr char kAndroidAppIntegrationMultiDataSourceDescription[] =
    "If enabled, allows Chrome to integrate with the Android App Search with "
    "multiple data sources, e.g. custom Tabs.";

inline constexpr char kAndroidBcivBottomControlsName[] =
    "Browser controls in viz for bottom controls";
inline constexpr char kAndroidBcivBottomControlsDescription[] =
    "Let viz move bottom browser controls when scrolling. If this flag is "
    "enabled, AndroidBrowserControlsInViz must also be enabled.";

inline constexpr char kAndroidBottomToolbarName[] = "Bottom Toolbar";
inline constexpr char kAndroidBottomToolbarDescription[] =
    "If enabled, displays the toolbar at the bottom.";

inline constexpr char kAndroidBottomToolbarV2Name[] = "Bottom Toolbar V2";
inline constexpr char kAndroidBottomToolbarV2Description[] =
    "If enabled, allows the Omnibox to be persistently anchored to the bottom "
    "of the screen.";

inline constexpr char kAndroidBrowserControlsInVizName[] =
    "Android Browser Controls in Viz";
inline constexpr char kAndroidBrowserControlsInVizDescription[] =
    "Let viz move browser controls when scrolling. For now, this applies only "
    "to top controls.";

inline constexpr char kAnnotatorModeName[] = "Enable annotator tool";
inline constexpr char kAnnotatorModeDescription[] =
    "Enables the tool for annotating across the OS.";


inline constexpr char kAutoRevokeSuspiciousNotificationName[] =
    "Auto-revoke suspicious notification";
inline constexpr char kAutoRevokeSuspiciousNotificationDescription[] =
    "Auto-revoke notification permission with suspicious content.";

inline constexpr char kAutomaticUsbDetachName[] =
    "Automatically detach USB kernel drivers";
inline constexpr char kAutomaticUsbDetachDescription[] =
    "Automatically detach kernel drivers when a USB interface is busy.";

inline constexpr char kAuxiliarySearchDonationName[] =
    "Auxiliary Search Donation";
inline constexpr char kAuxiliarySearchDonationDescription[] =
    "If enabled, override Auxiliary Search donation cap.";

inline constexpr char kAuxiliarySearchHistoryDonationName[] =
    "Auxiliary Search History Donation";
inline constexpr char kAuxiliarySearchHistoryDonationDescription[] =
    "If enabled, Auxiliary Search donates browsing history to AppSearch.";

inline constexpr char kBackgroundResourceFetchName[] =
    "Background Resource Fetch";
inline constexpr char kBackgroundResourceFetchDescription[] =
    "Process resource requests in a background thread inside Blink.";

inline constexpr char kByDateHistoryInSidePanelName[] =
    "By Date History in Side Panel";
inline constexpr char kByDateHistoryInSidePanelDescription[] =
    "If enabled, shows the 'By Date' History in Side Panel";

inline constexpr char kBlockV8OptimizerOnUnfamiliarSitesSettingName[] =
    "Automatic JS Optimizer Control";
inline constexpr char kBlockV8OptimizerOnUnfamiliarSitesSettingDescription[] =
    "Adds an option to the V8 optimizer content setting that disables the "
    "JavaScript optimizer on sites that are unfamiliar to the user.";

inline constexpr char kBookmarksTreeViewName[] =
    "Top Chrome Bookmarks Tree View";
inline constexpr char kBookmarksTreeViewDescription[] =
    "Show the bookmarks side panel in a tree view while in compact mode.";

inline constexpr char kBrowsingHistoryActorIntegrationM1Name[] =
    "Browsing History Actor Integration M1";
inline constexpr char kBrowsingHistoryActorIntegrationM1Description[] =
    "Enables the browsing history glic actor integration M1";

inline constexpr char kBrowsingHistoryActorIntegrationM2Name[] =
    "Browsing History Actor Integration M2";
inline constexpr char kBrowsingHistoryActorIntegrationM2Description[] =
    "Enables the browsing history glic actor integration M2";

inline constexpr char kBundledSecuritySettingsName[] =
    "Bundled Security Settings";
inline constexpr char kBundledSecuritySettingsDescription[] =
    "Enables new Bundled Security Settings UI on chrome://settings/security. "
    "This new UI bundles all security settings into either an enhanced or "
    "standard bundle which should simplify the security settings page and also "
    "help simplify the user's decision.";

inline constexpr char kCanvasDrawElementName[] = "HTML-in-Canvas";
inline constexpr char kCanvasDrawElementDescription[] =
    "Enables the Canvas 2D drawElement API and the WebGL texElement2D API for "
    "drawing HTML content into a canvas. "
    "See: https://github.com/WICG/html-in-canvas";

inline constexpr char kCertVerificationNetworkTimeName[] =
    "Network Time for Certificate Verification";
inline constexpr char kCertVerificationNetworkTimeDescription[] =
    "Use time fetched from the network for certificate verification decisions. "
    "If certificate verification fails with the network time, it will fall back"
    " to system time.";

inline constexpr char kClickToCallName[] = "Click-To-Call";
inline constexpr char kClickToCallDescription[] =
    "Enable the click-to-call feature.";

inline constexpr char kClipboardChangeEventName[] = "ClipboardChangeEvent";
inline constexpr char kClipboardChangeEventDescription[] =
    "Enables the `clipboardchange` event API. See: "
    "https://chromestatus.com/feature/5085102657503232";

inline constexpr char kClipboardMaximumAgeName[] = "Clipboard maximum age";
inline constexpr char kClipboardMaximumAgeDescription[] =
    "Limit the maximum age for recent clipboard content";

inline constexpr char kConnectionAllowlistsName[] = "Connection Allowlists";
inline constexpr char kConnectionAllowlistsDescription[] =
    "Enables a prototype implementation of `Connection-Allowlist` header "
    "parsing and enforcement. See https://github.com/mikewest/anti-exfil/";

inline constexpr char kCrosSwitcherName[] = "ChromeOS Switcher feature.";
inline constexpr char kCrosSwitcherDescription[] =
    "Enable/Disable ChromeOS Switcher feature.";

inline constexpr char kStylusHandwritingWinName[] =
    "Stylus Handwriting for Windows.";
inline constexpr char kStylusHandwritingWinDescription[] =
    "Enables an experimental feature that lets users handwrite into text "
    "fields using a compatible stylus. Only supported on Windows builds 22621 "
    "(patch 5126 and newer), 22631 (patch 5126 and newer) and all builds equal "
    "to or newer than 26100.3624";

inline constexpr char kPermissionsAndroidClapperLoudName[] = "Clapper Loud";
inline constexpr char kPermissionsAndroidClapperLoudDescription[] =
    "Enables the loud version of the Clapper permission prompt.";

inline constexpr char kPermissionsAndroidClapperQuietName[] = "Clapper Quiet";
inline constexpr char kPermissionsAndroidClapperQuietDescription[] =
    "Enables the quiet version of the Clapper permission prompt.";

inline constexpr char kCryptographyComplianceCnsaName[] =
    "Cryptography Compliance (CNSA)";
inline constexpr char kCryptographyComplianceCnsaDescription[] =
    "If enabled, Chrome will configure its preferred algorithms for TLS to "
    "prefer algorithms that satisfy the requirements of the Commercial "
    "National Security Algorithm Suite (CNSA) versions 1.0 and 2.0. Enabling "
    "this flag does not guarantee that any specific algorithms will be "
    "negotiated. This flag is not required for security.";

inline constexpr char kCssGamutMappingName[] = "CSS Gamut Mapping";
inline constexpr char kCssGamutMappingDescription[] =
    "Enable experimental CSS gamut mapping implementation.";

inline constexpr char kCssMasonryLayoutName[] = "CSS Masonry Layout";
inline constexpr char kCssMasonryLayoutDescription[] =
    "Enable experimental CSS Masonry Layout implementation. Simple layouts "
    "with masonry are supported. Subgrid, fragmentation, and out-of-flow items "
    "are not supported yet. The syntax to use CSS Masonry is `display: "
    "masonry` together with grid properties (i.e. `grid-column`, `grid-row`, "
    "etc.). More details on masonry syntax can be found at "
    "https://www.w3.org/TR/css-grid-3/#masonry-model.";

inline constexpr char kCustomizeChromeSidePanelExtensionsCardName[] =
    "Customize Chrome Side Panel Extension Card";
inline constexpr char kCustomizeChromeSidePanelExtensionsCardDescription[] =
    "If enabled, shows an extension card within the Customize Chrome Side "
    "Panel for access to the Chrome Web Store extensions.";

inline constexpr char kCustomizeChromeWallpaperSearchName[] =
    "Customize Chrome Wallpaper Search";
inline constexpr char kCustomizeChromeWallpaperSearchDescription[] =
    "Enables wallpaper search in Customize Chrome Side Panel.";

inline constexpr char kCustomizeChromeWallpaperSearchButtonName[] =
    "Customize Chrome Wallpaper Search Button";
inline constexpr char kCustomizeChromeWallpaperSearchButtonDescription[] =
    "Enables entry point on Customize Chrome Side Panel's Appearance page for "
    "Wallpaper Search.";

inline constexpr char kCustomizeChromeWallpaperSearchInspirationCardName[] =
    "Customize Chrome Wallpaper Search Inspiration Card";
inline constexpr char
    kCustomizeChromeWallpaperSearchInspirationCardDescription[] =
        "Shows inspiration card in Customize Chrome Side Panel Wallpaper "
        "Search. "
        "Requires #customize-chrome-wallpaper-search to be enabled too.";

inline constexpr char kCustomizeTabGroupColorPaletteName[] =
    "Customize tab group color palette";
inline constexpr char kCustomizeTabGroupColorPaletteDescription[] =
    "Enables parsing of the `tab_group_color_palette` key in the "
    "manifest.json file, which allows customization of the tab group color "
    "palette. Disabling this flag will cause the key to be ignored.";

inline constexpr char kDataSharingName[] = "Data Sharing";
inline constexpr char kDataSharingDescription[] =
    "Enabled all Data Sharing related UI and features.";

inline constexpr char kDataSharingJoinOnlyName[] = "Data Sharing Join Only";
inline constexpr char kDataSharingJoinOnlyDescription[] =
    "Enabled Data Sharing Joining flow related UI and features.";

inline constexpr char kDataSharingNonProductionEnvironmentName[] =
    "Data Sharing server environment";
inline constexpr char kDataSharingNonProductionEnvironmentDescription[] =
    "Sets data sharing server environment.";

// LINT.IfChange(DataSharingVersioning)
// Data Sharing versioning test scenarios.
inline constexpr char kDataSharingVersioningStatesName[] =
    "Data Sharing Versioning Test Scenarios";
inline constexpr char kDataSharingVersioningStatesDescription[] =
    "Testing multiple scenarios for versioning.";
inline constexpr char kDataSharingSharedDataTypesEnabled[] =
    "Version out-of-date, no UI";
inline constexpr char kDataSharingSharedDataTypesEnabledWithUi[] =
    "Version out-of-date, show UI ";
// LINT.ThenChange(//ios/chrome/browser/flags/ios_chrome_flag_descriptions.cc:DataSharingVersioning)

inline constexpr char kDbdRevampDesktopName[] =
    "Revamped Delete Browsing Data dialog";
inline constexpr char kDbdRevampDesktopDescription[] =
    "Enables a revamped Delete Browsing Data dialog on Desktop. This includes "
    "UI changes and removal of the bulk password deletion option from the "
    "dialog.";

inline constexpr char kDefaultSearchEnginePrewarmName[] =
    "Default search engine prewarm";
inline constexpr char kDefaultSearchEnginePrewarmDescription[] =
    "Performance optimization to prewarm the default search engine used in the "
    "Omnibox";

inline constexpr char kDisableAutofillStrikeSystemName[] =
    "Disable the Autofill strike system";
inline constexpr char kDisableAutofillStrikeSystemDescription[] =
    "When enabled, the Autofill strike system will not block a feature from "
    "being offered.";

inline constexpr char kDisableFacilitatedPaymentsMerchantAllowlistName[] =
    "Disable the merchant allowlist check for facilitated payments";
inline constexpr char
    kDisableFacilitatedPaymentsMerchantAllowlistDescription[] =
        "When enabled, disable the merchant allowlist check for facilitated "
        "payments, so that merchants that are not on the allowlist can also be "
        "tested for the supported features.";

inline constexpr char kDropInputEventsWhilePaintHoldingName[] =
    "Drop input events while paint-holding is active";
inline constexpr char kDropInputEventsWhilePaintHoldingDescription[] =
    "Drop input events at the browser process until the process receives the "
    "first signal that the renderer has sent a frame to GPU.  This prevents "
    "accidental interaction with a page the user has not seen yet.";

inline constexpr char kFieldClassificationModelCachingName[] =
    "Enable caching field classification predictions";
inline constexpr char kFieldClassificationModelCachingDescription[] =
    "When enabled, the field classification model uses runtime caching to not "
    "run models on the same inputs multiple times.";

inline constexpr char kHdrAgtmName[] = "Adaptive global tone mapping";
inline constexpr char kHdrAgtmDescription[] =
    "Enables parsing and rendering of adaptive global tone mapping (AGTM) aka "
    "SMTPE ST 2094-50 HDR metadata";

inline constexpr char kHistorySyncAlternativeIllustrationName[] =
    "History Sync Alternative Illustration";
inline constexpr char kHistorySyncAlternativeIllustrationDescription[] =
    "Enables history sync alternative illustration.";

inline constexpr char kLeftClickOpensTabGroupBubbleName[] =
    "Left Click to Open TabGroup Editor Bubble";
inline constexpr char kLeftClickOpensTabGroupBubbleDescription[] =
    "Swaps the mouse action for opening a tab group editor bubble to left "
    "click";

inline constexpr char kDeprecateUnloadName[] = "Deprecate the unload event";
inline constexpr char kDeprecateUnloadDescription[] =
    "Controls the default for Permissions-Policy unload. If enabled, unload "
    "handlers are deprecated and will not receive the unload event unless a "
    "Permissions-Policy to enable them has been explicitly set. If  disabled, "
    "unload handlers will continue to receive the unload event unless "
    "explicitly disabled by Permissions-Policy, even during the gradual "
    "rollout of their deprecation.";

inline constexpr char kDesktopUAOnConnectedDisplayName[] =
    "Request Desktop User-Agent on external displays. Android only.";
inline constexpr char kDesktopUAOnConnectedDisplayDescription[] =
    "When enabled, this feature will request a desktop user agent on external "
    "displays.";

inline constexpr char kDevToolsPrivacyUIName[] = "DevTools Privacy UI";
inline constexpr char kDevToolsPrivacyUIDescription[] =
    "Enables the Privacy UI in the current 'Security' panel in DevTools.";

inline constexpr char kDevToolsProjectSettingsName[] =
    "DevTools Project Settings";
inline constexpr char kDevToolsProjectSettingsDescription[] =
    "If enabled, DevTools will try to fetch project settings in the "
    "form of a `com.chrome.devtools.json` file from a well-known URI "
    "on local debugging targets.";

inline constexpr char kDevToolsStartingStyleDebuggingName[] =
    "DevTools @starting-style debugging";
inline constexpr char kDevToolsStartingStyleDebuggingDescription[] =
    "Enables the debugging of @starting-style in the elements panel.";

inline constexpr char kEnableSeamlessSigninName[] = "Enable Seamless Sign-in";
inline constexpr char kEnableSeamlessSigninDescription[] =
    "Enables the Seamless Sign-in flow that signs in the user without showing "
    "an additional bottom sheet when the sign-in promo button is clicked.";

inline constexpr char kForceHistoryOptInScreenName[] =
    "Force history opt-in screen";
inline constexpr char kForceHistoryOptInScreenDescription[] =
    "If enabled, the history opt-in screen will be forced to show up even if "
    "the user declined history sync too recently or too often";

inline constexpr char kFRESignInAlternativeSecondaryButtonTextName[] =
    "Use alternative secondary button text on FRE sign-in promo screen";
inline constexpr char kFRESignInAlternativeSecondaryButtonTextDescription[] =
    "If enabled, the FRE sign-in promo will use the alternative secondary "
    "button text.";

inline constexpr char kFluidResizeName[] = "Enable AL device fluid resize";
inline constexpr char kFluidResizeDescription[] =
    "Enable AL device fluid resize to improve UX.";

inline constexpr char kForceStartupSigninPromoName[] =
    "Force Start-up Signin Promo";
inline constexpr char kForceStartupSigninPromoDescription[] =
    "If enabled, the full screen signin promo will be forced to show up at "
    "Chrome start-up.";

inline constexpr char kFwupdDeveloperModeName[] = "Enable fwupd developer mode";
inline constexpr char kFwupdDeveloperModeDescription[] =
    "Allows display and installation in UI of unauthenticated firmware by "
    "disabling all checks.";

inline constexpr char kDisableInstanceLimitName[] = "Disable Instance Limit";
inline constexpr char kDisableInstanceLimitDescription[] =
    "Disable limit on number of app instances allowed (current limit is 5).";

inline constexpr char kDisplayEdgeToEdgeFullscreenName[] =
    "Enable Display Edge to Edge Fullscreen";
inline constexpr char kDisplayEdgeToEdgeFullscreenDescription[] =
    "Enable Display Edge to Edge Fullscreen when Chrome on Android is running "
    "in a windowing mode.";

inline constexpr char kClearInstanceInfoWhenClosedIntentionallyName[] =
    "Clear Instance Info When Closed Intentionally";
inline constexpr char kClearInstanceInfoWhenClosedIntentionallyDescription[] =
    "When enabled, permanently cleanup and remove the browser instance when a "
    "window is explicitly closed by the user (eg: via the Close button).";

inline constexpr char kPermissionPromiseLifetimeModulationName[] =
    "PermissionPromiseLifetimeModulation";
inline constexpr char kPermissionPromiseLifetimeModulationDescription[] =
    "Modulates the lifetime of a permission promise based on the prompt's UI "
    "treatment. When the prompt is non-prominent, the promise settlement is "
    "expedited to synchronize with the request manager state.";

inline constexpr char kEnableBenchmarkingName[] = "Enable benchmarking";
inline constexpr char kEnableBenchmarkingDescription[] =
    "Sets all features to a fixed state; that is, disables randomization for "
    "feature states. If '(Default Feature States)' is selected, sets all "
    "features to their default state. If '(Match Field Trial Testing Config)' "
    "is selected, sets all features to the state configured in the field trial "
    "testing config. This is used by developers and testers "
    "to diagnose whether an observed problem is caused by a non-default "
    "base::Feature configuration. This flag is automatically reset "
    "after 3 restarts and will be off from the 4th restart. On the 3rd "
    "restart, the flag will appear to be off but the effect is still active.";
inline constexpr char kEnableBenchmarkingChoiceDisabled[] = "Disabled";
inline constexpr char kEnableBenchmarkingChoiceDefaultFeatureStates[] =
    "Default Feature States";
inline constexpr char kEnableBenchmarkingChoiceMatchFieldTrialTestingConfig[] =
    "Match Field Trial Testing Config";

inline constexpr char kUseUnexportableKeyServiceInBrowserProcessName[] =
    "Enable UnexportableKeyService mojo service in the browser process.";
inline constexpr char kUseUnexportableKeyServiceInBrowserProcessDescription[] =
    "When enabled, the browser process will create an Unexportable Key "
    "Service which can be used by other, less privileged processes. This "
    "enables DBSC in platforms where access to TPM-like features is "
    "privileged.";

inline constexpr char kEnableCrossDevicePrefTrackerName[] =
    "Enable Cross-Device Pref Tracker";
inline constexpr char kEnableCrossDevicePrefTrackerDescription[] =
    "Enables the tracking and sharing of select non-syncing preference values "
    "across a user's signed-in devices.";

inline constexpr char kEnableExtensionInstallPolicyFetchingName[] =
    "Enable Extension Install Policy Fetching";
inline constexpr char kEnableExtensionInstallPolicyFetchingDescription[] =
    "Enables fetching of extension install policies from the cloud for the "
    "ExtensionInstallCloudPolicyChecksEnabled policy.";

inline constexpr char kD3D12VideoEncoderName[] = "Use D3D12 video encoder";
inline constexpr char kD3D12VideoEncoderDescription[] =
    "Enables D3D12 video encoding. The system might still fall back to "
    "Media Foundation video encoder if D3D12 encoder is not available "
    "or fails to initialize.";

inline constexpr char kPreinstalledWebAppAlwaysMigrateCalculatorName[] =
    "Preinstalled web app always migrate - Calculator";
inline constexpr char kPreinstalledWebAppAlwaysMigrateCalculatorDescription[] =
    "Whether the calculator web app preinstall should always attempt to migrate"
    " the Calculator Chrome app if it is detected as present.";

inline constexpr char kPrerender2Name[] = "Prerendering";
inline constexpr char kPrerender2Description[] =
    "If enabled, browser features and the speculation rules API can trigger "
    "prerendering. If disabled, all prerendering APIs still exist, but a "
    "prerender will never successfully take place.";

inline constexpr char kPrerender2ReuseHostName[] = "Prerender Reuse Host";
inline constexpr char kPrerender2ReuseHostDescription[] =
    "If enabled, the browser will reuse the prerender host and the underlying"
    "process for future prerendered pages when possible.";

inline constexpr char kPrerenderUntilScriptName[] = "Prerender Until Script";
inline constexpr char kPrerenderUntilScriptDescription[] =
    "Prerenders pages until a script is about to be executed. The script "
    "execution and the page parsing will be deferred until action.";

inline constexpr char kBookmarkBarPrefetchName[] = "BookmarkBarPrefetch";
inline constexpr char kBookmarkBarPrefetchDescription[] =
    "If enabled, bookmarkbar can trigger prefetch";

inline constexpr char kNewTabPagePrefetchName[] = "NewTabPagePrefetch";
inline constexpr char kNewTabPagePrefetchDescription[] =
    "If enabled, NewTabPage can trigger prefetch";

inline constexpr char kEnableDrDcName[] =
    "Enables Display Compositor to use a new gpu thread.";
inline constexpr char kEnableDrDcDescription[] =
    "When enabled, chrome uses 2 gpu threads instead of 1. "
    " Display compositor uses new dr-dc gpu thread and all other clients "
    "(raster, webgl, video) "
    " continues using the gpu main thread.";

inline constexpr char kEnableFullscreenToAnyScreenAndroidName[] =
    "Enables use of the screen parameter for requestFullscreen.";
inline constexpr char kEnableFullscreenToAnyScreenAndroidDescription[] =
    "When enabled the user can request an HTML element to be expanded to "
    "full screen on another screen using the Element.requestFullscreen web "
    "API. This enables usage of the screen parameter in the "
    "Element.requestFullscreen web API.";

inline constexpr char kTextBasedAudioDescriptionName[] =
    "Enable audio descriptions.";
inline constexpr char kTextBasedAudioDescriptionDescription[] =
    "When enabled, HTML5 video elements with a 'descriptions' WebVTT track "
    "will speak the audio descriptions aloud as the video plays.";

inline constexpr char kUseAndroidStagingSmdsName[] =
    "Use Android staging SM-DS";
inline constexpr char kUseAndroidStagingSmdsDescription[] =
    "Use the Android staging address when fetching pending eSIM profiles.";

inline constexpr char kUseStorkSmdsServerAddressName[] =
    "Use Stork SM-DS address";
inline constexpr char kUseStorkSmdsServerAddressDescription[] =
    "Use the Stork SM-DS address to fetch pending eSIM profiles managed by the "
    "Stork prod server. Note that Stork profiles can be created with an EID at "
    "go/stork-profile, and managed at go/stork-batch > View Profiles. Also "
    "note that an test eUICC card is required to use this feature, usually "
    "that requires the CellularUseSecond flag to be enabled. Go to "
    "go/cros-connectivity > Dev Tips for more instructions.";

inline constexpr char kUseWallpaperStagingUrlName[] =
    "Use Wallpaper staging URL";
inline constexpr char kUseWallpaperStagingUrlDescription[] =
    "Use the staging server as part of the Wallpaper App to verify "
    "additions/removals of wallpapers.";

inline constexpr char kUseDMSAAForTilesName[] = "Use DMSAA for tiles";
inline constexpr char kUseDMSAAForTilesDescription[] =
    "Switches skia to use DMSAA instead of MSAA for tile raster";

inline constexpr char kIsolatedSandboxedIframesName[] =
    "Isolated sandboxed iframes";
inline constexpr char kIsolatedSandboxedIframesDescription[] =
    "When enabled, applies process isolation to iframes with the 'sandbox' "
    "attribute and without the 'allow-same-origin' permission set on that "
    "attribute. This also applies to documents with a similar CSP sandbox "
    "header, even in the main frame. The affected sandboxed documents can be "
    "grouped into processes based on their URL's site or origin. The default "
    "grouping when enabled is per-site.";

inline constexpr char kAutofillAndPasswordsInSameSurfaceName[] =
    "Allow Autofill and Passwords in the same dropdown";
inline constexpr char kAutofillAndPasswordsInSameSurfaceDescription[] =
    "Enables a refactoring allowing to add password/passkey suggestions into "
    "Autofill dropdowns alongside addresses, etc.";

inline constexpr char kAutofillAndroidDesktopSuppressAccessoryOnEmptyName[] =
    "Enable suppressing keyboard accessory on android desktop";
inline constexpr char
    kAutofillAndroidDesktopSuppressAccessoryOnEmptyDescription[] =
        "When enabled, Autofill will suppress keyboard accessory when the form "
        "field is not a username/password field and does not have any autofill "
        "suggestions. ";

inline constexpr char kAutofillDisableBnplCountryCheckForTestingName[] =
    "Disable the country check for BNPL testing";
inline constexpr char kAutofillDisableBnplCountryCheckForTestingDescription[] =
    "Enables testing BNPL in countries where it would otherwise be disabled.";

inline constexpr char kAutofillEnableAiBasedAmountExtractionName[] =
    "Enable AI-based checkout amount extraction on Chrome";
inline constexpr char kAutofillEnableAiBasedAmountExtractionDescription[] =
    "When enabled, Chrome will extract the checkout amount from the checkout "
    "page using server-side AI.";

inline constexpr char kAutofillEnableAllowlistForBmoCardCategoryBenefitsName[] =
    "Enable allowlist for showing category benefits for BMO cards";
inline constexpr char
    kAutofillEnableAllowlistForBmoCardCategoryBenefitsDescription[] =
        "When enabled, card category benefits offered by BMO will be shown in "
        "Autofill suggestions on the allowlisted merchant websites.";

inline constexpr char kAutofillEnableAmountExtractionName[] =
    "Enable checkout amount extraction.";
inline constexpr char kAutofillEnableAmountExtractionDescription[] =
    "When enabled, Chrome will extract the checkout amount from the checkout "
    "page of the allowlisted merchant websites.";
inline constexpr char kAutofillEnableAmountExtractionTestingName[] =
    "Enable amount extraction testing";
inline constexpr char kAutofillEnableAmountExtractionTestingDescription[] =
    "Enables testing of the result of checkout amount extraction. This flag "
    "will allow amount extraction to run on any website when a CC form is "
    "clicked.";

inline constexpr char kAutofillEnableBuyNowPayLaterName[] =
    "Enable buy now pay later on Autofill";
inline constexpr char kAutofillEnableBuyNowPayLaterDescription[] =
    "When enabled, users will have the option to pay with buy now pay later on "
    "specific merchant webpages.";

inline constexpr char kAutofillEnableBuyNowPayLaterForExternallyLinkedName[] =
    "Enable buy now pay later for externally linked BNPL issuer";
inline constexpr char
    kAutofillEnableBuyNowPayLaterForExternallyLinkedDescription[] =
        "When enabled, users will have the option to pay with buy now pay "
        "later "
        "with externally linked issuer on specific merchant webpages.";

inline constexpr char kAutofillEnableBuyNowPayLaterForKlarnaName[] =
    "Enable buy now pay later on Autofill for Klarna";
inline constexpr char kAutofillEnableBuyNowPayLaterForKlarnaDescription[] =
    "When enabled, users will have the option to pay with buy now pay later "
    "with Klarna on specific merchant webpages.";

inline constexpr char kAutofillEnableBuyNowPayLaterSyncingName[] =
    "Enable syncing buy now pay later user data.";
inline constexpr char kAutofillEnableBuyNowPayLaterSyncingDescription[] =
    "When enabled, Chrome will sync user data related to buy now pay later.";

inline constexpr char
    kAutofillEnableBuyNowPayLaterUpdatedSuggestionSecondLineStringName[] =
        "Enable issuer names in the second line of a BNPL suggestion";
inline constexpr char
    kAutofillEnableBuyNowPayLaterUpdatedSuggestionSecondLineStringDescription
        [] = "When enabled, the second line of a BNPL suggestion is updated to "
             "include the issuer names for better brand recognition.";

inline constexpr char kAutofillEnableCvcStorageAndFillingName[] =
    "Enable CVC storage and filling for payments autofill";
inline constexpr char kAutofillEnableCvcStorageAndFillingDescription[] =
    "When enabled, we will store CVC for both local and server credit cards. "
    "This will also allow the users to autofill their CVCs on checkout pages.";

inline constexpr char kAutofillEnableCvcStorageAndFillingEnhancementName[] =
    "Enable CVC storage and filling enhancement for payments autofill";
inline constexpr char
    kAutofillEnableCvcStorageAndFillingEnhancementDescription[] =
        "When enabled, will enhance CVV storage project. Provide better "
        "suggestion, resolve conflict with COF project and add logging.";

inline constexpr char
    kAutofillEnableCvcStorageAndFillingStandaloneFormEnhancementName[] =
        "Enable CVC storage and filling standalone form enhancement for "
        "payments "
        "autofill";
inline constexpr char
    kAutofillEnableCvcStorageAndFillingStandaloneFormEnhancementDescription[] =
        "When enabled, this will enhance the CVV storage project. The "
        "enhancement will enable CVV storage suggestions for standalone CVC "
        "fields.";

inline constexpr char kAutofillEnableFpanRiskBasedAuthenticationName[] =
    "Enable risk-based authentication for FPAN retrieval";
inline constexpr char kAutofillEnableFpanRiskBasedAuthenticationDescription[] =
    "When enabled, server card retrieval will begin with a risk-based check "
    "instead of jumping straight to CVC or biometric auth.";

inline constexpr char kIPHAutofillCreditCardBenefitFeatureName[] =
    "Enable Card Benefits in-product help bubble";
inline constexpr char kIPHAutofillCreditCardBenefitFeatureDescription[] =
    "Enables In-Product-Help that appears when at least one autofill credit "
    "card suggestion includes card benefits.";

inline constexpr char kAutofillEnableCardBenefitsForAmericanExpressName[] =
    "Enable showing card benefits for American Express cards";
inline constexpr char
    kAutofillEnableCardBenefitsForAmericanExpressDescription[] =
        "When enabled, card benefits offered by American Express will be shown "
        "in "
        "Autofill suggestions.";

inline constexpr char kAutofillEnableCardBenefitsForBmoName[] =
    "Enable showing card benefits for BMO cards";
inline constexpr char kAutofillEnableCardBenefitsForBmoDescription[] =
    "When enabled, card benefits offered by BMO will be shown in Autofill "
    "suggestions.";

inline constexpr char kAutofillEnableCardBenefitsIphName[] =
    "Enable showing in-process help UI for card benefits";
inline constexpr char kAutofillEnableCardBenefitsIphDescription[] =
    "When enabled, in-process help UI will be shown for Autofill card "
    "suggestions with benefits.";

inline constexpr char kAutofillEnableCardBenefitsSyncName[] =
    "Enable syncing card benefits";
inline constexpr char kAutofillEnableCardBenefitsSyncDescription[] =
    "When enabled, card benefits offered by issuers will be synced from the "
    "Payments server.";

inline constexpr char kAutofillEnableCardInfoRuntimeRetrievalName[] =
    "Enable retrieval of card info(with CVC) from issuer for enrolled cards";
inline constexpr char kAutofillEnableCardInfoRuntimeRetrievalDescription[] =
    "When enabled, runtime retrieval of CVC along with card number and expiry "
    "from issuer for enrolled cards will be enabled during form fill.";

inline constexpr char kAutofillEnableDownstreamCardAwarenessIphName[] =
    "Enable showing in-product help UI for downstream card awareness";
inline constexpr char kAutofillEnableDownstreamCardAwarenessIphDescription[] =
    "When enabled, in-product help UI will be shown the first time a card "
    "added outside of Chrome appears in Autofill card suggestions.";

inline constexpr char kAutofillEnableEmailOrLoyaltyCardsFillingName[] =
    "Enable Autofill support for filling email or loyalty card fields";
inline constexpr char kAutofillEnableEmailOrLoyaltyCardsFillingDescription[] =
    "When enabled, Autofill will offer support for filling the user's loyalty "
    "cards on email or loyalty card fields.";

inline constexpr char kAutofillEnableFlatRateCardBenefitsFromCurinosName[] =
    "Enable showing flat rate card benefits sourced from Curinos";
inline constexpr char
    kAutofillEnableFlatRateCardBenefitsFromCurinosDescription[] =
        "When enabled, flat rate card benefits sourced from Curinos will be "
        "shown "
        "in Autofill suggestions.";

inline constexpr char kAutofillEnableKeyboardAccessoryChipRedesignName[] =
    "Enable 2 line chips in the Chrome Keyboard Accessory";
inline constexpr char
    kAutofillEnableKeyboardAccessoryChipRedesignDescription[] =
        "When enabled, Autofill information is displayed on 2 lines in the "
        "Chrome "
        "KeyboardAccessory";

inline constexpr char
    kAutofillEnableKeyboardAccessoryChipWidthAdjustmentName[] =
        "Enable Keyboard Accessory chip width adjustment";
inline constexpr char
    kAutofillEnableKeyboardAccessoryChipWidthAdjustmentDescription[] =
        "When enabled, Keyboard accessory limits the width of the first chip "
        "or "
        "the first 2 chips to display a part of the next chip.";

inline constexpr char kAutofillEnableLoyaltyCardsFillingName[] =
    "Enable Autofill support for filling loyalty cards";
inline constexpr char kAutofillEnableLoyaltyCardsFillingDescription[] =
    "When enabled, Autofill will offer support for filling the user's loyalty "
    "cards stored in Google Wallet.";

inline constexpr char kAutofillEnableLoyaltyCardSyncName[] =
    "Sync Autofill Loyalty Cards";
inline constexpr char kAutofillEnableLoyaltyCardSyncDescription[] =
    "When enabled, allows syncing of Google Wallet loyalty cards.";

inline constexpr char
    kAutofillEnableMultipleRequestInVirtualCardDownstreamEnrollmentName[] =
        "Enable multiple server request support for virtual card downstream "
        "enrollment";
inline constexpr char
    kAutofillEnableMultipleRequestInVirtualCardDownstreamEnrollmentDescription
        [] = "When enabled, Chrome will be able to send preflight call for "
             "enrollment earlier in the flow with the multiple server request "
             "support.";

inline constexpr char kAutofillEnableNewFopDisplayAndroidName[] =
    "Enable Autofill new FOP display on Android";
inline constexpr char kAutofillEnableNewFopDisplayAndroidDescription[] =
    "When enabled, updates payment method Autofill suggestions and settings "
    "UI.";

inline constexpr char kAutofillEnableNewFopDisplayDesktopName[] =
    "Enable Autofill new FOP display on Desktop";
inline constexpr char kAutofillEnableNewFopDisplayDesktopDescription[] =
    "When enabled, updates payment method Autofill suggestions and settings "
    "UI.";

inline constexpr char kAutofillEnableOffersInClankKeyboardAccessoryName[] =
    "Enable Autofill offers in keyboard accessory";
inline constexpr char
    kAutofillEnableOffersInClankKeyboardAccessoryDescription[] =
        "When enabled, offers will be displayed in the keyboard accessory when "
        "available.";

inline constexpr char kAutofillEnablePaymentsMandatoryReauthChromeOsName[] =
    "Enable mandatory re-auth for payments autofill on ChromeOS";
inline constexpr char
    kAutofillEnablePaymentsMandatoryReauthChromeOsDescription[] =
        "When enabled, in use-cases where we would not have triggered any "
        "interactive authentication to autofill payment methods, we will "
        "trigger a device authentication on ChromeOS.";

inline constexpr char kKeyboardLockApiOnAndroidName[] =
    "Keyboard Lock API on Android";
inline constexpr char kKeyboardLockApiOnAndroidDescription[] =
    "When enabled, allows websites to use keyboard.lock() on Android";

inline constexpr char kAutofillEnablePrefetchingRiskDataForRetrievalName[] =
    "Enable prefetching of risk data during payments autofill retrieval";
inline constexpr char
    kAutofillEnablePrefetchingRiskDataForRetrievalDescription[] =
        "When enabled, risk data is prefetched during payments autofill flows "
        "to reduce user-perceived latency.";

inline constexpr char kAutofillEnableSaveAndFillName[] = "Enable Save and Fill";
inline constexpr char kAutofillEnableSaveAndFillDescription[] =
    "When enabled, show an option to offer saving and filling a credit card "
    "with a single click when users don't have any cards saved in Autofill.";

inline constexpr char kAutofillEnableSeparatePixPreferenceItemName[] =
    "Enable Pix settings to be shown as a separate preference menu item.";
inline constexpr char kAutofillEnableSeparatePixPreferenceItemDescription[] =
    "When enabled, show Pix settings as a separate preference menu item "
    "instead of bundling them together with the non-card payment preference "
    "menu "
    "item.";

inline constexpr char kAutofillEnableSupportForHomeAndWorkName[] =
    "Support for Home and Work addresses in Autofill";
inline constexpr char kAutofillEnableSupportForHomeAndWorkDescription[] =
    "When enabled, Home and Work addresses from MyAccount are available for "
    "autofilling";

inline constexpr char kAutofillEnableSupportForNameAndEmailName[] =
    "Support for name and email addresses in Autofill";
inline constexpr char kAutofillEnableSupportForNameAndEmailDescription[] =
    "When enabled, a name and email profile with data comming from the account "
    "will be created for autofilling.";

inline constexpr char kAutofillEnableTouchToFillReshowForBnplName[] =
    "Enable the Touch To Fill bottom sheet to be reshown on Android for BNPL.";
inline constexpr char kAutofillEnableTouchToFillReshowForBnplDescription[] =
    "When enabled, the Touch To Fill bottom sheet on Android can be reshown "
    "after a BNPL flow is dismissed by a user.";

inline constexpr char kAutofillEnableVcn3dsAuthenticationName[] =
    "Enable 3DS authentication for virtual cards";
inline constexpr char kAutofillEnableVcn3dsAuthenticationDescription[] =
    "When enabled, Chrome will trigger 3DS authentication during a virtual "
    "card retrieval if a challenge is required, 3DS authentication is "
    "available for the card, and FIDO is not.";

inline constexpr char kAutofillImprovedLabelsName[] =
    "Autofill suggestions with improved labels";
inline constexpr char kAutofillImprovedLabelsDescription[] =
    "When enabled, the autofill suggestion labels are more more descriptive "
    "and relevant.";

inline constexpr char kAutofillManualTestingDataName[] =
    "Autofill manual testing data";
inline constexpr char kAutofillManualTestingDataDescription[] =
    "When set, imports the addresses and cards specified on startup. WARNING: "
    "If at least one address/card is specified, all other existing "
    "addresses/cards are overwritten.";

inline constexpr char kAutofillMoreProminentPopupName[] =
    "More prominent Autofill popup";
inline constexpr char kAutofillMoreProminentPopupDescription[] =
    "If enabled Autofill's popup becomes more prominent, i.e. its shadow "
    "becomes more emphasized, position is also updated";

inline constexpr char kAutofillPaymentsFieldSwappingName[] =
    "Swap credit card suggestions";
inline constexpr char kAutofillPaymentsFieldSwappingDescription[] =
    "When enabled, swapping autofilled payment suggestions would result"
    "in overriding all of the payments fields with the swapped profile data";

inline constexpr char kAutofillPreferBuyNowPayLaterBlocklistsName[] =
    "Prefer blocklists instead of allowlists for Payments Autofill Buy Now Pay "
    "Later (BNPL)";
inline constexpr char kAutofillPreferBuyNowPayLaterBlocklistsDescription[] =
    "When enabled, Payments Autofill Buy Now Pay Later (BNPL) will use each "
    "corresponding issuer's blocklist instead of allowlist to check for "
    "website eligibility.";

inline constexpr char kAutofillPrioritizeSaveCardOverMandatoryReauthName[] =
    "Prioritize save card bubble over mandatory re-auth";
inline constexpr char
    kAutofillPrioritizeSaveCardOverMandatoryReauthDescription[] =
        "When enabled, this flag prioritizes showing the save card bubble over "
        "the mandatory re-auth bubble when both are applicable.";

inline constexpr char kAutofillShowBubblesBasedOnPrioritiesName[] =
    "Show bubbles based on priorities";
inline constexpr char kAutofillShowBubblesBasedOnPrioritiesDescription[] =
    "When enabled, the autofill and the password manager bubbles would be"
    "shown based on their respective priorities compared to each other";

inline constexpr char kAutofillSharedStorageServerCardDataName[] =
    "Enable storing autofill server card data in the shared storage database";
inline constexpr char kAutofillSharedStorageServerCardDataDescription[] =
    "When enabled, the cached server credit card data from autofill will be "
    "pushed into the shared storage database for the payments origin.";

inline constexpr char kAutofillUnmaskCardRequestTimeoutName[] =
    "Timeout for the credit card unmask request";
inline constexpr char kAutofillUnmaskCardRequestTimeoutDescription[] =
    "When enabled, sets a client-side timeout on the Autofill credit card "
    "unmask request. Upon timeout, the client will terminate the current "
    "unmask server call, which may or may not terminate the ongoing unmask UI.";

inline constexpr char kAutofillVcnEnrollStrikeExpiryTimeName[] =
    "Expiry duration for VCN enrollment strikes";
inline constexpr char kAutofillVcnEnrollStrikeExpiryTimeDescription[] =
    "When enabled, changes the amount of time required for VCN enrollment "
    "prompt strikes to expire.";

inline constexpr char kAutoPictureInPictureAndroidName[] =
    "Auto picture in picture on Android";
inline constexpr char kAutoPictureInPictureAndroidDescription[] =
    "Enables auto picture in picture on Android";

inline constexpr char kAutoPictureInPictureForVideoPlaybackName[] =
    "Auto picture in picture for video playback";
inline constexpr char kAutoPictureInPictureForVideoPlaybackDescription[] =
    "Enables auto picture in picture for video playback";

inline constexpr char kBackForwardTransitionsCrossDocSharedImageName[] =
    "Back-forward transitions on cross-document navigations use SharedImage";
inline constexpr char kBackForwardTransitionsCrossDocSharedImageDescription[] =
    "When enabled, use a SharedImage for capturing screenshots for "
    "back/forward transitions on cross-document navigations.";

inline constexpr char kBiometricReauthForPasswordFillingName[] =
    "Biometric reauth for password filling";
inline constexpr char kBiometricReauthForPasswordFillingDescription[] =
    "Enables biometric"
    "re-authentication before password filling";

inline constexpr char kBindCookiesToPortName[] =
    "Bind cookies to their setting origin's port";
inline constexpr char kBindCookiesToPortDescription[] =
    "If enabled, cookies will only be accessible by origins with the same port "
    "as the one that originally set the cookie.";

inline constexpr char kBindCookiesToSchemeName[] =
    "Bind cookies to their setting origin's scheme";
inline constexpr char kBindCookiesToSchemeDescription[] =
    "If enabled, cookies will only be accessible by origins with the same "
    "scheme as the one that originally set the cookie";

inline constexpr char kBackgroundListeningName[] = "BackgroundListening";
inline constexpr char kBackgroundListeningDescription[] =
    "Enables the new media player features optimized for background listening.";

inline constexpr char kBlockCrossPartitionBlobUrlFetchingName[] =
    "Block Cross Partition Blob URL Fetching";
inline constexpr char kBlockCrossPartitionBlobUrlFetchingDescription[] =
    "Blocks fetching of cross-partitioned Blob URL.";

inline constexpr char kBookmarkTabGroupConversionName[] =
    "Bookmark and tab group conversion";
inline constexpr char kBookmarkTabGroupConversionDescription[] =
    "Enable conversion between bookmark and tab group";

inline constexpr char kBorealisBigGlName[] = "Borealis Big GL";
inline constexpr char kBorealisBigGlDescription[] =
    "Enable Big GL when running Borealis.";

inline constexpr char kBorealisDGPUName[] = "Borealis dGPU";
inline constexpr char kBorealisDGPUDescription[] =
    "Enable dGPU when running Borealis.";

inline constexpr char kBorealisEnableUnsupportedHardwareName[] =
    "Borealis Enable Unsupported Hardware";
inline constexpr char kBorealisEnableUnsupportedHardwareDescription[] =
    "Allow Borealis to run on hardware that does not meet the minimum spec "
    "requirements. Be aware: Games may crash, or perform below expectations.";

inline constexpr char kBorealisForceBetaClientName[] =
    "Borealis Force Beta Client";
inline constexpr char kBorealisForceBetaClientDescription[] =
    "Force the client to run its beta version.";

inline constexpr char kBorealisForceDoubleScaleName[] =
    "Borealis Force Double Scale";
inline constexpr char kBorealisForceDoubleScaleDescription[] =
    "Force the client to run in 2x visual zoom. the scale client by DPI flag "
    "needs to be off for this to take effect.";

inline constexpr char kBorealisLinuxModeName[] = "Borealis Linux Mode";
inline constexpr char kBorealisLinuxModeDescription[] =
    "Do not run ChromeOS-specific code in the client.";

// For UX reasons we prefer "enabled", but that is used internally to refer to
// whether borealis is installed or not, so the name of the variable is a bit
// different to the user-facing name.
inline constexpr char kBorealisPermittedName[] = "Borealis Enabled";
inline constexpr char kBorealisPermittedDescription[] =
    "Allows Borealis to run on your device. Borealis may still be blocked for "
    "other reasons, including: administrator settings, device hardware "
    "capabilities, or other security measures.";

inline constexpr char kBorealisProvisionName[] = "Borealis Provision";
inline constexpr char kBorealisProvisionDescription[] =
    "Uses the experimental 'provision' option when mounting borealis stateful. "
    "The feature causes allocations on thinly provisioned storage, such as "
    "sparse vm images, to be passed to the underlying storage layers. "
    "Resulting in allocations in the Borealis being backed by physical "
    "storage.";

inline constexpr char kBorealisScaleClientByDPIName[] =
    "Borealis Scale Client By DPI";
inline constexpr char kBorealisScaleClientByDPIDescription[] =
    "Enable scaling the Steam client according to device DPI. "
    "If enabled this will override the force double scale flag.";

inline constexpr char kBorealisZinkGlDriverName[] = "Borealis Zink GL Driver";
inline constexpr char kBorealisZinkGlDriverDescription[] =
    "Enables zink driver for GL rendering in Borealis. Can be enabled for "
    "recommended GL apps only or for all GL apps. Defaults to recommended.";

inline constexpr char kAndroidSettingsContainmentName[] =
    "Android Settings Containment";
inline constexpr char kAndroidSettingsContainmentDescription[] =
    "Enables the Android Settings Containment feature.";

inline constexpr char kCCTNavigationMetricsName[] = "CCT Navigation Metrics";
inline constexpr char kCCTNavigationMetricsDescription[] =
    "Enables detailed navigation-related metrics in CustomTabsCallback.";

inline constexpr char kCCTResetTimeoutEnabledName[] =
    "CCT Reset Timeout Enabled";
inline constexpr char kCCTResetTimeoutEnabledDescription[] =
    "Enables the reset timeout for CCTs. This flag allows embedder to "
    "close CCT after a specified time in mins.";

inline constexpr char kSearchInCCTName[] = "Search in Chrome Custom Tabs";
inline constexpr char kSearchInCCTDescription[] =
    "Permits apps to create searchable and "
    "navigable custom tabs.";
inline constexpr char kSearchInCCTAlternateTapHandlingName[] =
    "Search in Chrome Custom Tabs Alternate Tap Handling";
inline constexpr char kSearchInCCTAlternateTapHandlingDescription[] =
    "Search in Chrome Custom Tabs Alternate Tap Handling";

inline constexpr char kSettingsMultiColumnName[] =
    "Use MultiColumn mode in Chrome settings";
inline constexpr char kSettingsMultiColumnDescription[] =
    "If the window of the clank is large enough, settings page will have two "
    "column style, the main menu will be put at the left pane, and detailed "
    "page will be shown at the right pane. This is expected to be used with "
    "settings-single-activity mode.";

inline constexpr char kSettingsSingleActivityName[] =
    "Use SingleActivity mode in Chrome settings";
inline constexpr char kSettingsSingleActivityDescription[] =
    "On transition of the page, instead of stacking a new Activity as a task, "
    "reuse the Activity and switch the contained fragment.";

inline constexpr char kSeparateWebAppShortcutBadgeIconName[] =
    "Separate Web App Shortcut Badge Icon";
inline constexpr char kSeparateWebAppShortcutBadgeIconDescription[] =
    "The shortcut app badge is painted in the UI instead of being part of the "
    "shortcut app icon, and more effects are added for the icon.";

inline constexpr char kSeparateLocalAndAccountSearchEnginesName[] =
    "Separate local and account search engines";
inline constexpr char kSeparateLocalAndAccountSearchEnginesDescription[] =
    "Keeps the local and the account search engines separate. If the user "
    "signs out or sync is turned off, the account search engines are removed "
    "while the pre-existing/local search engines are left behind.";

inline constexpr char kSeparateLocalAndAccountThemesName[] =
    "Separate local and account themes";
inline constexpr char kSeparateLocalAndAccountThemesDescription[] =
    "Keeps the local and the account theme separate. If the user signs out or "
    "sync is turned off, only the account theme is removed and the "
    "pre-existing local theme is restored.";

inline constexpr char kGetDisplayMediaConfersActivationName[] =
    "getDisplayMedia() confers transient activation.";
inline constexpr char kGetDisplayMediaConfersActivationDescription[] =
    "When getDisplay() is invoked by the application, the user is shown a "
    "dialog which allows them to share a tab, a window or a screen. If this "
    "flag is enabled, then after the user chooses what to share, transient "
    "activation is conferred on the Web application.";

inline constexpr char kDevicePostureName[] = "Device Posture API";
inline constexpr char kDevicePostureDescription[] =
    "Enables Device Posture API (foldable devices)";

inline constexpr char kDocumentPictureInPictureAnimateResizeName[] =
    "Document Picture-in-Picture Animate Resize";
inline constexpr char kDocumentPictureInPictureAnimateResizeDescription[] =
    "Use an animation when programmatically resizing a document"
    "picture-in-picture window";

inline constexpr char kAudioDuckingName[] = "Audio Ducking";
inline constexpr char kAudioDuckingDescription[] =
    "Allows Chrome to duck (attenuate) "
    "audio from other tabs.";

inline constexpr char kDsePreload2Name[] = "Default Search Engine preload 2";
inline constexpr char kDsePreload2Description[] =
    "Enables new DSE preload instead of existing one, which uses //content "
    "prefetch";

inline constexpr char kDsePreload2OnPressName[] =
    "Default Search Engine preload 2, on-press triggers";
inline constexpr char kDsePreload2OnPressDescription[] =
    "Enables on-press triggers of DsePreload2";

inline constexpr char kHttpCacheCustomBackendName[] =
    "Use custom disk cache backend for HTTP Cache";
inline constexpr char kHttpCacheCustomBackendDescription[] =
    "Enables the experimental disk cache backend for HTTP Cache";

inline constexpr char kHttpCacheNoVarySearchName[] =
    "No Vary Search in Disk Cache";
inline constexpr char kHttpCacheNoVarySearchDescription[] =
    "Enables the No-Vary-Search header in the disk cache";

inline constexpr char kViewportSegmentsName[] = "Viewport Segments API";
inline constexpr char kViewportSegmentsDescription[] =
    "Enable the viewport segment API, giving information about the logical "
    "segments of the device (dual screen and foldable devices)";

inline constexpr char kVisitedURLRankingServiceDeduplicationName[] =
    "Visited URL ranking deduplication strategy";
inline constexpr char kVisitedURLRankingServiceDeduplicationDescription[] =
    "Enables visited url ranking service to use one of various deduplication "
    "strategies.";

inline constexpr char
    kVisitedURLRankingServiceHistoryVisibilityScoreFilterName[] =
        "Enable visited URL aggregates visibility score based filtering";
inline constexpr char
    kVisitedURLRankingServiceHistoryVisibilityScoreFilterDescription[] =
        "Enables filtering of visited URL aggregates based on history URL "
        "visibility scores.";

inline constexpr char kDoubleBufferCompositingName[] =
    "Double buffered compositing";
inline constexpr char kDoubleBufferCompositingDescription[] =
    "Use double buffer for compositing (instead of triple-buffering). "
    "Latency should be reduced in some cases. On the other hand, more skipped "
    "frames are expected.";

inline constexpr char kMagicBoostUpdateForQuickAnswersName[] =
    "Magic Boost Update for Quick Answers";
inline constexpr char kMagicBoostUpdateForQuickAnswersDescription[] =
    "Enables to show the new Quick Answers card with chips in the revamped "
    "Magic Boost opt-in flow";

inline constexpr char kMediaPlaybackWhileNotVisiblePermissionPolicyName[] =
    "media-playback-while-not-visible permission policy";
inline constexpr char
    kMediaPlaybackWhileNotVisiblePermissionPolicyDescription[] =
        "Enables the media-playback-while-not-visible permission policy. This "
        "permission policy will pause any media being played by any disallowed "
        "iframes which are not currently rendered. See"
        "https://github.com/MicrosoftEdge/MSEdgeExplainers/blob/main/"
        "IframeMediaPause/iframe_media_pausing.md for more information.";

inline constexpr char kCollaborationEntrepriseV2Name[] =
    "Collaboration Entreprise V2";
inline constexpr char kCollaborationEntrepriseV2Description[] =
    "Enables the collaboration feature for entreprise users within the same "
    "domain.";

inline constexpr char kCollaborationMessagingName[] = "Collaboration Messaging";
inline constexpr char kCollaborationMessagingDescription[] =
    "Enables the messaging framework within the collaboration feature, "
    "including features such as recent activity, dirty dots, and description "
    "action chips.";

inline constexpr char kCollaborationSharedTabGroupAccountDataName[] =
    "Shared Tab Group messaging sync";
inline constexpr char kCollaborationSharedTabGroupAccountDataDescription[] =
    "Enable the messaging sync backend for shared tab groups.";

inline constexpr char kCompressionDictionaryTransportName[] =
    "Compression dictionary transport";
inline constexpr char kCompressionDictionaryTransportDescription[] =
    "Enables compression dictionary transport.";

inline constexpr char kCompressionDictionaryTTLName[] =
    "Compression dictionary transport ttl";
inline constexpr char kCompressionDictionaryTTLDescription[] =
    "Enables support for the 'ttl' parameter in the 'use-as-dictionary' HTTP "
    "response header.";

inline constexpr char kContextMenuEmptySpaceName[] =
    "Context menu at empty space";
inline constexpr char kContextMenuEmptySpaceDescription[] =
    "When this is enabled, on right click (or equivalent gestures) at empty "
    "space, a context menu containing page-related items will be shown.";

inline constexpr char kContextualCueingName[] = "Contextual cueing";
inline constexpr char kContextualCueingDescription[] =
    "Enables the contextual cueing system to support showing actions.";
inline constexpr char kGlicActorName[] = "Glic actor";
inline constexpr char kGlicActorDescription[] = "Enables the Glic actor.";
inline constexpr char kGlicActorAutofillName[] = "Glic actor autofill";
inline constexpr char kGlicActorAutofillDescription[] =
    "Enables autofill actions for the Glic actor. Specific fillable types may "
    "also need to be enabled.";
inline constexpr char kGlicCaptureRegionDescription[] =
    "Enables Glic to capture a region of the screen.";
inline constexpr char kGlicCaptureRegionName[] = "Glic Capture Region";
inline constexpr char kGlicDetachedName[] = "Glic detached-only mode";
inline constexpr char kGlicDetachedDescription[] =
    "Detach only mode forces the Glic UI to always be floating";
inline constexpr char kGlicSidePanelName[] = "Glic side panel";
inline constexpr char kGlicSidePanelDescription[] =
    "Enable mulitple side panels for Glic";
inline constexpr char kGlicPanelResetTopChromeButtonName[] =
    "Glic Panel Reset With Top Chrome Button";
inline constexpr char kGlicPanelResetTopChromeButtonDescription[] =
    "Configure how the tab strip button can be used to reset the glic panel "
    "location.";
inline constexpr char kGlicPanelResetOnStartName[] =
    "Glic Panel Reset On Start";
inline constexpr char kGlicPanelResetOnStartDescription[] =
    "Enables resetting the glic panel position on startup.";
inline constexpr char kGlicPanelSetPositionOnDragName[] =
    "Glic Panel Set Position On Drag";
inline constexpr char kGlicPanelSetPositionOnDragDescription[] =
    "Enables only saving the glic panel position after a drag.";
inline constexpr char kGlicPanelResetOnSessionTimeoutName[] =
    "Glic Panel Reset On Session Timeout";
inline constexpr char kGlicPanelResetOnSessionTimeoutDescription[] =
    "Enables resetting the panel position after a session timeout.";
inline constexpr char kGlicPanelResetSizeAndLocationName[] =
    "Glic Panel Reset Size and Location";
inline constexpr char kGlicPanelResetSizeAndLocationDescription[] =
    "Enables resetting the panel size and position on every open.";
inline constexpr char kGlicWarmingName[] = "Glic Pre-Warming";
inline constexpr char kGlicWarmingDescription[] =
    "Enables the pre-warming of the Glic panel's web client.";
inline constexpr char kGlicFreWarmingName[] = "Glic FRE Pre-Warming";
inline constexpr char kGlicFreWarmingDescription[] =
    "Enables the pre-warming of Glic's FRE web page.";
inline constexpr char kGlicEntrypointVariationsName[] =
    "Glic Entrypoint Variations";
inline constexpr char kGlicEntrypointVariationsDescription[] =
    "Enables visual tweaks to the Glic entry button in the tab strip.";
inline constexpr char kGlicBindPinnedUnboundTabName[] =
    "Glic Bind a Shared Tab If Unbound";
inline constexpr char kGlicBindPinnedUnboundTabDescription[] =
    "When a tab is shared with conversation and not yet bound to any "
    "conversation, bind it to the current one";
inline constexpr char kGlicDefaultToLastActiveConversationName[] =
    "Glic Default To Last Active Conversation";
inline constexpr char kGlicDefaultToLastActiveConversationDescription[] =
    "Enables the last active conversation as the default conversation when "
    "opening a new Glic side panel instance.";
inline constexpr char kGlicButtonPressedStateName[] =
    "Glic Button Pressed State";
inline constexpr char kGlicButtonPressedStateDescription[] =
    "Enables visual changes to the Glic entry button when Glic is open.";

inline constexpr char kGlicButtonAltLabelName[] = "Glic Button Alt Label";
inline constexpr char kGlicButtonAltLabelDescription[] =
    "Enables an alternative label for the Glic button.";

inline constexpr char kGlicDaisyChainNewTabsName[] =
    "Glic Daisy chain new tabs";
inline constexpr char kGlicDaisyChainNewTabsDescription[] =
    "Daisy chains new tabs if the active tab when the new tab was create has an"
    " open glic side panel";
inline constexpr char kGlicUseToolbarHeightSidePanelName[] =
    "Glic Use Toolbar Height Side Panel";
inline constexpr char kGlicUseToolbarHeightSidePanelDescription[] =
    "Enables Glic to use the toolbar height side panel instead of content "
    "height side panel when enabled to use side panel";
inline constexpr char kGlicUseNonClientName[] = "Glic Use NonClientView";
inline constexpr char kGlicUseNonClientDescription[] =
    "Renders the window using NonClientView/FrameView which grants the window "
    "access to standard window management features on ChromeOS.";

inline constexpr char kContextualSearchWithCredentialsForDebugName[] =
    "Contextual Search within credentials for debug";
inline constexpr char kContextualSearchWithCredentialsForDebugDescription[] =
    "When this is enabled, if a user do the contextual search, the credentials "
    "mode will be include.";
inline constexpr char kFacilitatedPaymentsEnableA2APaymentName[] =
    "Enable Account to Account payments";
inline constexpr char kFacilitatedPaymentsEnableA2APaymentDescription[] =
    "When enabled, Chrome will offer an app list when a supported "
    "payment link is detected. Users can choose the payment app they want to "
    "use and be redirected to the chosen app to complete the payment flow";

inline constexpr char kForceColorProfileSRGB[] = "sRGB";
inline constexpr char kForceColorProfileP3[] = "Display P3 D65";
inline constexpr char kForceColorProfileRec2020[] = "ITU-R BT.2020";
inline constexpr char kForceColorProfileColorSpin[] =
    "Color spin with gamma 2.4";
inline constexpr char kForceColorProfileSCRGBLinear[] =
    "scRGB linear (HDR where available)";
inline constexpr char kForceColorProfileHDR10[] = "HDR10 (HDR where available)";

inline constexpr char kForceColorProfileName[] = "Force color profile";
inline constexpr char kForceColorProfileDescription[] =
    "Forces Chrome to use a specific color profile instead of the color "
    "of the window's current monitor, as specified by the operating system.";

inline constexpr char kDarkenWebsitesCheckboxInThemesSettingName[] =
    "Darken websites checkbox in themes setting";
inline constexpr char kDarkenWebsitesCheckboxInThemesSettingDescription[] =
    "Show a darken websites checkbox in themes settings when system default or "
    "dark is selected. The checkbox can toggle the auto-darkening web contents "
    "feature";

inline constexpr char kDebugShortcutsName[] = "Debugging keyboard shortcuts";
inline constexpr char kDebugShortcutsDescription[] =
    "Enables additional keyboard shortcuts that are useful for debugging Ash.";

inline constexpr char kDisableProcessReuse[] = "Disable subframe process reuse";
inline constexpr char kDisableProcessReuseDescription[] =
    "Prevents out-of-process iframes from reusing compatible processes from "
    "unrelated tabs. This is an experimental mode that will result in more "
    "processes being created.";

inline constexpr char kDisableSystemBlur[] = "Disable system blur";
inline constexpr char kDisableSystemBlurDescription[] =
    "Removes background blur from system UI";

inline constexpr char kDisallowDocWrittenScriptsUiName[] =
    "Block scripts loaded via document.write";
inline constexpr char kDisallowDocWrittenScriptsUiDescription[] =
    "Disallows fetches for third-party parser-blocking scripts inserted into "
    "the main frame via document.write.";

inline constexpr char kEnableAudioMonitoringOnAndroidName[] =
    "Enable Audio Levels Monitoring on Android";
inline constexpr char kEnableAudioMonitoringOnAndroidDescription[] =
    "Enables audio power level analysis on Android to determine webcontents "
    "audibility changes. This modifies the behavior of the "
    "#media-indicators-android flag to achieve a more responsive UI "
    "update when audio starts or stops.";

inline constexpr char kEnableAutoDisableAccessibilityName[] =
    "Auto-disable Accessibility";
inline constexpr char kEnableAutoDisableAccessibilityDescription[] =
    "When accessibility APIs are no longer being requested, automatically "
    "disables accessibility. This might happen if an assistive technology is "
    "turned off or if an extension which uses accessibility APIs no longer "
    "needs them.";

inline constexpr char kImageDescriptionsAlternateRoutingName[] =
    "Use alternative route for image descriptions.";
inline constexpr char kImageDescriptionsAlternateRoutingDescription[] =
    "When adding automatic captions to images, use a different route to "
    "acquire descriptions.";

inline constexpr char kEnterpriseBadgingForNtpFooterName[] =
    "Enable enterprise badging on the New Tab Page";
inline constexpr char kEnterpriseBadgingForNtpFooterDescription[] =
    "Enable enterprise profile badging in the footer on the New Tab Page. This "
    "includes showing the enterprise logo and the management disclaimer";

inline constexpr char kEnableClientCertificateProvisioningOnAndroidName[] =
    "Enable client certificate provisioning on Android";
inline constexpr char
    kEnableClientCertificateProvisioningOnAndroidDescription[] =
        "When enabled, client certificate provisioning from the cloud is "
        "allowed "
        "for enterprise users on Android.";

inline constexpr char kEnableExperimentalCookieFeaturesName[] =
    "Enable experimental cookie features";
inline constexpr char kEnableExperimentalCookieFeaturesDescription[] =
    "Enable new features that affect setting, sending, and managing cookies. "
    "The enabled features are subject to change at any time.";

inline constexpr char kEnableDelegatedCompositingName[] =
    "Enable delegated compositing";
inline constexpr char kEnableDelegatedCompositingDescription[] =
    "When enabled and applicable, the act of compositing is delegated to the "
    "system compositor.";

inline constexpr char kEnablePixAccountLinkingName[] =
    "Enable Pix account linking";
inline constexpr char kEnablePixAccountLinkingDescription[] =
    "When enabled, users without linked Pix accounts will be prompted to link "
    "their Pix accounts to Google Wallet.";

inline constexpr char kEnablePixPaymentsInLandscapeModeName[] =
    "Enable Pix payments in landscape mode";
inline constexpr char kEnablePixPaymentsInLandscapeModeDescription[] =
    "When enabled, users using their devices in landscape mode also will be "
    "offered to pay using their Pix accounts. Users using their devices in "
    "portrait mode are always offered to pay using their Pix accounts.";

inline constexpr char kEnableStaticQrCodeForPixName[] =
    "Enable Static Qr Code For Pix";
inline constexpr char kEnableStaticQrCodeForPixDescription[] =
    "When enabled, pix pay flow will be triggered when users click the copy "
    "button of static qr code.";

inline constexpr char kDesktopPWAsAdditionalWindowingControlsName[] =
    "Desktop PWA Additional Windowing Controls";
inline constexpr char kDesktopPWAsAdditionalWindowingControlsDescription[] =
    "Enable PWAs to: (1) manually recreate the minimize, maximize and restore "
    "window functionalities, (2) set windows (non-/)resizable and (3) listen "
    "to window's move events with respective APIs.";

inline constexpr char kDesktopPWAsAppTitleName[] =
    "Desktop PWA Application Title";
inline constexpr char kDesktopPWAsAppTitleDescription[] =
    "Enable PWAs to set a custom title for their windows.";

inline constexpr char kDesktopPWAsElidedExtensionsMenuName[] =
    "Desktop PWAs elided extensions menu";
inline constexpr char kDesktopPWAsElidedExtensionsMenuDescription[] =
    "Moves the Extensions \"puzzle piece\" icon from the title bar into the "
    "app menu for web app windows.";

inline constexpr char kDesktopPWAsTabStripName[] = "Desktop PWA tab strips";
inline constexpr char kDesktopPWAsTabStripDescription[] =
    "Tabbed application mode - enables the `tabbed` display mode which allows "
    "web apps to add a tab strip to their app.";

inline constexpr char kDesktopPWAsTabStripSettingsName[] =
    "Desktop PWA tab strips settings";
inline constexpr char kDesktopPWAsTabStripSettingsDescription[] =
    "Experimental UI for selecting whether a PWA should open in tabbed mode.";

inline constexpr char kDesktopPWAsTabStripCustomizationsName[] =
    "Desktop PWA tab strip customizations";
inline constexpr char kDesktopPWAsTabStripCustomizationsDescription[] =
    "Enable PWAs to customize their tab strip when in tabbed mode by adding "
    "the `tab_strip` manifest field.";

inline constexpr char kDesktopPWAsSubAppsName[] = "Desktop PWA Sub Apps";
inline constexpr char kDesktopPWAsSubAppsDescription[] =
    "Enable installed PWAs to create shortcuts by installing their sub apps. "
    "Prototype implementation of: "
    "https://github.com/ivansandrk/multi-apps/blob/main/explainer.md";

inline constexpr char kDevToolsIndividualRequestThrottlingName[] =
    "Enable individual request throttling in DevTools";
inline constexpr char kDevToolsIndividualRequestThrottlingDescription[] =
    "Enables a new feature in DevTools' network panel to apply network "
    "conditions to individual requests, extending the per-request blocking "
    "behavior.";

inline constexpr char kDevToolsLiveEditName[] =
    "Enable JavaScript live editing in DevTools";
inline constexpr char kDevToolsLiveEditDescription[] =
    "Re-enable the deprecated feature in DevTools' Sources panel to apply code "
    "edits to the target page live.";

inline constexpr char kDesktopPWAsBorderlessName[] = "Desktop PWA Borderless";
inline constexpr char kDesktopPWAsBorderlessDescription[] =
    "Enable web app manifests to declare borderless mode as a display "
    "override. Prototype implementation of: go/borderless-mode.";

inline constexpr char kEnableTLS13EarlyDataName[] = "TLS 1.3 Early Data";
inline constexpr char kEnableTLS13EarlyDataDescription[] =
    "This option enables TLS 1.3 Early Data, allowing GET requests to be sent "
    "during the handshake when resuming a connection to a compatible TLS 1.3 "
    "server.";

inline constexpr char kAccessibilityAcceleratorName[] =
    "Experimental Accessibility accelerator";
inline constexpr char kAccessibilityAcceleratorDescription[] =
    "This option enables the Accessibility accelerator.";

inline constexpr char kAccessibilityDisableTouchpadName[] =
    "Accessibility disable trackpad";
inline constexpr char kAccessibilityDisableTouchpadDescription[] =
    "Adds a setting that allows the user to disable the built-in trackpad.";

inline constexpr char kAccessibilityFlashScreenFeatureName[] =
    "Accessibility feature to flash the screen for each notification";
inline constexpr char kAccessibilityFlashScreenFeatureDescription[] =
    "Allows the user to use a feature which flashes the screen for each "
    "notification.";

inline constexpr char kAccessibilityShakeToLocateName[] =
    "Adds shake cursor to locate feature";
inline constexpr char kAccessibilityShakeToLocateDescription[] =
    "This option enables the experimental Accessibility feature to make the "
    "mouse cursor more visible when a shake is detected.";

inline constexpr char kAccessibilityReducedAnimationsName[] =
    "Experimental Reduced Animations";
inline constexpr char kAccessibilityReducedAnimationsDescription[] =
    "This option enables the setting to limit movement on the screen.";

inline constexpr char kAccessibilityReducedAnimationsInKioskName[] =
    "Reduced Animations feature toggle available in Kiosk quick settings";
inline constexpr char kAccessibilityReducedAnimationsInKioskDescription[] =
    "This option enables the quick settings option to toggle reduced "
    "animations.";

inline constexpr char kAccessibilityMagnifierFollowsChromeVoxName[] =
    "Magnifier follows ChromeVox focus";
inline constexpr char kAccessibilityMagnifierFollowsChromeVoxDescription[] =
    "This option enables the fullscreen magnifier to follow ChromeVox's focus.";

inline constexpr char kAccessibilityMouseKeysName[] = "Mouse Keys";
inline constexpr char kAccessibilityMouseKeysDescription[] =
    "This option enables you to control the mouse with the keyboard.";

inline constexpr char kAccessibilityCaptionsOnBrailleDisplayName[] =
    "Captions on Braille Display";
inline constexpr char kAccessibilityCaptionsOnBrailleDisplayDescription[] =
    "This option allows access to captions for media via a braille display.";

inline constexpr char kApplyClientsideModelPredictionsForPasswordTypesName[] =
    "Apply clientside model predictions for password forms.";
inline constexpr char
    kApplyClientsideModelPredictionsForPasswordTypesDescription[] =
        "Enable using clientside model predictions to fill password forms.";

inline constexpr char kNewMacNotificationAPIName[] =
    "Determines which notification API to use on macOS devices";
inline constexpr char kNewMacNotificationAPIDescription[] =
    "Enables the usage of Apple's new notification API";

inline constexpr char kEnableFencedFramesDeveloperModeName[] =
    "Enable the `FencedFrameConfig` constructor.";
inline constexpr char kEnableFencedFramesDeveloperModeDescription[] =
    "The `FencedFrameConfig` constructor allows you to test the <fencedframe> "
    "element without running an ad auction, as you can manually supply a URL "
    "to navigate the fenced frame to.";

inline constexpr char kEnableGamepadMultitouchName[] = "Gamepad Multitouch";
inline constexpr char kEnableGamepadMultitouchDescription[] =
    "Enables the ability to receive input from multitouch surface "
    "on the gamepad object.";

inline constexpr char kEnableGpuServiceLoggingName[] =
    "Enable gpu service logging";
inline constexpr char kEnableGpuServiceLoggingDescription[] =
    "Enable printing the actual GL driver calls.";

inline constexpr char kEnableIsolatedWebAppsName[] = "Enable Isolated Web Apps";
inline constexpr char kEnableIsolatedWebAppsDescription[] =
    "Enables experimental support for Isolated Web Apps. "
    "See https://github.com/reillyeon/isolated-web-apps for more information.";

inline constexpr char kDirectSocketsInServiceWorkersName[] =
    "Direct Sockets API in Service Workers";
inline constexpr char kDirectSocketsInServiceWorkersDescription[] =
    "Enables access to the Direct Sockets API in service workers. See "
    "https://github.com/WICG/direct-sockets for details.";

inline constexpr char kDirectSocketsInSharedWorkersName[] =
    "Direct Sockets API in Shared Workers";
inline constexpr char kDirectSocketsInSharedWorkersDescription[] =
    "Enables access to the Direct Sockets API in shared workers. See "
    "https://github.com/WICG/direct-sockets for details.";

inline constexpr char kEnableIsolatedWebAppUnmanagedInstallName[] =
    "Enable Isolated Web App unmanaged installation";
inline constexpr char kEnableIsolatedWebAppUnmanagedInstallDescription[] =
    "Enables the installation of Isolated Web Apps on devices that are not "
    "managed by an enterprise.";

inline constexpr char kEnableIsolatedWebAppManagedGuestSessionInstallName[] =
    "Enable Isolated Web App installation in managed guest sessions";
inline constexpr char
    kEnableIsolatedWebAppManagedGuestSessionInstallDescription[] =
        "Enables the installation of Isolated Web Apps for users that are "
        "logged "
        "into a managed guest session.";

inline constexpr char kWebAppManifestProtocolHandlersName[] =
    "Enable web app manifest protocol handlers";
inline constexpr char kWebAppManifestProtocolHandlersDescription[] =
    "Enables support for protocol handlers registered via the "
    "`protocol_handlers` web app manifest field.";

inline constexpr char kEnableIsolatedWebAppAllowlistName[] =
    "Enable an allowlist for Isolated Web Apps";
inline constexpr char kEnableIsolatedWebAppAllowlistDescription[] =
    "Enables an allowlist for Isolated Web Apps, restricting installation and "
    "updates to only those apps that are allowlisted.";

inline constexpr char kEnableIsolatedWebAppDevModeName[] =
    "Enable Isolated Web App Developer Mode";
inline constexpr char kEnableIsolatedWebAppDevModeDescription[] =
    "Enables the installation of unverified Isolated Web Apps";

inline constexpr char kEnableIwaKeyDistributionComponentName[] =
    "Enable the Iwa Key Distribution component";
inline constexpr char kEnableIwaKeyDistributionComponentDescription[] =
    "Enables the Iwa Key Distribution component that supplies key rotation "
    "data for Isolated Web Apps.";

inline constexpr char kIwaKeyDistributionComponentExpCohortName[] =
    "Experimental cohort for the Iwa Key Distribution component";
inline constexpr char kIwaKeyDistributionComponentExpCohortDescription[] =
    "Specifies the experimental cohort for the Iwa Key Distribution component.";

inline constexpr char kEnableControlledFrameName[] = "Enable Controlled Frame";
inline constexpr char kEnableControlledFrameDescription[] =
    "Enables experimental support for Controlled Frame. See "
    "https://github.com/WICG/controlled-frame/blob/main/EXPLAINER.md "
    "for more information.";

inline constexpr char kEnablePeripheralCustomizationName[] =
    "Enable peripheral customization";
inline constexpr char kEnablePeripheralCustomizationDescription[] =
    "Enable peripheral customization to allow users to customize buttons on "
    "their peripherals.";

inline constexpr char kEnablePeripheralNotificationName[] =
    "Enable peripheral notification";
inline constexpr char kEnablePeripheralNotificationDescription[] =
    "Enable peripheral notification to notify users when a input device is "
    "connected to the user's Chromebook for the first time.";

inline constexpr char kEnablePeripheralsLoggingName[] =
    "Enable peripherals logging";
inline constexpr char kEnablePeripheralsLoggingDescription[] =
    "Enable peripherals logging to get detailed logs of peripherals";

inline constexpr char kExperimentalRgbKeyboardPatternsName[] =
    "Enable experimental RGB Keyboard patterns support";
inline constexpr char kExperimentalRgbKeyboardPatternsDescription[] =
    "Enable experimental RGB Keyboard patterns support on supported devices.";

inline constexpr char kEnableNetworkLoggingToFileName[] =
    "Enable network logging to file";
inline constexpr char kEnableNetworkLoggingToFileDescription[] =
    "Enables network logging to a file named netlog.json in the user data "
    "directory. The file can be imported into chrome://net-internals.";

inline constexpr char kDownloadNotificationServiceUnifiedAPIName[] =
    "Migrate download notification service to use new API";
inline constexpr char kDownloadNotificationServiceUnifiedAPIDescription[] =
    "Migrate download notification service to use new unified API based on "
    "offline item and native persistence";

inline constexpr char kEnableNtpEnterpriseShortcutsName[] =
    "Enables enterprise shortcuts for the New Tab Page";
inline constexpr char kEnableNtpEnterpriseShortcutsDescription[] =
    "Enables enterprise shortcuts for the New Tab Page set by the NTPShortcuts "
    "policy.";

inline constexpr char kEnablePerfettoSystemTracingName[] =
    "Enable Perfetto system tracing";
inline constexpr char kEnablePerfettoSystemTracingDescription[] =
    "When enabled, Chrome will attempt to connect to the system tracing "
    "service";

inline constexpr char kWebRequestSecurityInfoName[] =
    "Enable SecurityInfo in WebRequest API";
inline constexpr char kWebRequestSecurityInfoDescription[] =
    "Enables SecurityInfo in WebRequest API for extensions, allowing "
    "listeners to retrieve certificate details of web requests.";

inline constexpr char kEnableWindowsGamingInputDataFetcherName[] =
    "Enable Windows.Gaming.Input";
inline constexpr char kEnableWindowsGamingInputDataFetcherDescription[] =
    "Enable Windows.Gaming.Input by default to provide game controller "
    "support on Windows 10 desktop.";

inline constexpr char kDeprecateAltClickName[] =
    "Enable Alt+Click deprecation notifications";
inline constexpr char kDeprecateAltClickDescription[] =
    "Start providing notifications about Alt+Click deprecation and enable "
    "Search+Click as an alternative.";

inline constexpr char kExperimentalAccessibilityLanguageDetectionName[] =
    "Experimental accessibility language detection";
inline constexpr char kExperimentalAccessibilityLanguageDetectionDescription[] =
    "Enable language detection for in-page content which is then exposed to "
    "assistive technologies such as screen readers.";

inline constexpr char kExperimentalAccessibilityLanguageDetectionDynamicName[] =
    "Experimental accessibility language detection for dynamic content";
inline constexpr char
    kExperimentalAccessibilityLanguageDetectionDynamicDescription[] =
        "Enable language detection for dynamic content which is then exposed "
        "to "
        "assistive technologies such as screen readers.";

inline constexpr char kFillRecoveryPasswordName[] = "Fill recovery password";
inline constexpr char kFillRecoveryPasswordDescription[] =
    "Offers the previously saved recovery password for filling if one exists.";

inline constexpr char kMemlogName[] = "Chrome heap profiler start mode.";
inline constexpr char kMemlogDescription[] =
    "Starts heap profiling service that records sampled memory allocation "
    "profile having each sample attributed with a callstack. "
    "The sampling resolution is controlled with --memlog-sampling-rate flag. "
    "Recorded heap dumps can be obtained at chrome://tracing "
    "[category:memory-infra] and chrome://memory-internals. This setting "
    "controls which processes will be profiled since their start. To profile "
    "any given process at a later time use chrome://memory-internals page.";
inline constexpr char kMemlogModeMinimal[] = "Browser and GPU";
inline constexpr char kMemlogModeAll[] = "All processes";
inline constexpr char kMemlogModeAllRenderers[] = "All renderers";
inline constexpr char kMemlogModeRendererSampling[] = "Single renderer";
inline constexpr char kMemlogModeBrowser[] = "Browser only";
inline constexpr char kMemlogModeGpu[] = "GPU only";
inline constexpr char kMemlogModeUtilitySampling[] = "Single utility";
inline constexpr char kMemlogModeAllUtilities[] = "All utilities";

inline constexpr char kMemlogSamplingRateName[] =
    "Heap profiling sampling interval (in bytes).";
inline constexpr char kMemlogSamplingRateDescription[] =
    "Heap profiling service uses Poisson process to sample allocations. "
    "Default value for the interval between samples is 1000000 (1MB). "
    "This results in low noise for large and/or frequent allocations "
    "[size * frequency >> 1MB]. This means that aggregate numbers [e.g. "
    "total size of malloc-ed objects] and large and/or frequent allocations "
    "can be trusted with high fidelity. "
    "Lower intervals produce higher samples resolution, but come at a cost of "
    "higher performance overhead.";
inline constexpr char kMemlogSamplingRate10KB[] = "10KB";
inline constexpr char kMemlogSamplingRate50KB[] = "50KB";
inline constexpr char kMemlogSamplingRate100KB[] = "100KB";
inline constexpr char kMemlogSamplingRate500KB[] = "500KB";
inline constexpr char kMemlogSamplingRate1MB[] = "1MB";
inline constexpr char kMemlogSamplingRate5MB[] = "5MB";

inline constexpr char kMemlogStackModeName[] =
    "Heap profiling stack traces type.";
inline constexpr char kMemlogStackModeDescription[] =
    "By default heap profiling service records native stacks. "
    "A post-processing step is required to symbolize the stacks. "
    "'Native with thread names' adds the thread name as the first frame of "
    "each native stack. It's also possible to record a pseudo stack using "
    "trace events as identifiers. It's also possible to do a mix of both.";
inline constexpr char kMemlogStackModeNative[] = "Native";
inline constexpr char kMemlogStackModeNativeWithThreadNames[] =
    "Native with thread names";

inline constexpr char kEnableDevtoolsDeepLinkViaExtensibilityApiName[] =
    "Extensibility API support for deep-links within DevTools";
inline constexpr char kEnableDevtoolsDeepLinkViaExtensibilityApiDescription[] =
    "Extends console.timestamp to support adding deep-links into the DevTools "
    "Performance Panel, which (when clicked) call into a DevTools extension";

inline constexpr char kEnableLazyLoadImageForInvisiblePageName[] =
    "Enable lazy load image for invisible page";
inline constexpr char kEnableLazyLoadImageForInvisiblePageDescription[] =
    "Respect the loading = lazy attribute for images even on invisible pages.";

inline constexpr char kEnableNtpBrowserPromosName[] =
    "Enable new tab page browser feature suggestions";
inline constexpr char kEnableNtpBrowserPromosDescription[] =
    "Shows suggestions to explore browser capabilities (eg. signing in) on the "
    "new tab page.";

inline constexpr char kSoftNavigationHeuristicsName[] =
    "Soft Navigation Heuristics";
inline constexpr char kSoftNavigationHeuristicsDescription[] =
    "Enables the soft navigation heuristics, including support for "
    "PerformanceObserver. This setting overrides other experimental settings. "
    "See the documentation for our earlier experiment at "
    "https://developer.chrome.com/docs/web-platform/soft-navigations-experiment"
    " (to be updated soon).";

inline constexpr char kEnableSiteSearchAllowUserOverridePolicyName[] =
    "Enable allow_user_override field for SiteSearchSettings policy";
inline constexpr char kEnableSiteSearchAllowUserOverridePolicyDescription[] =
    "Enable the field that allows organizations to set a Site Search engine "
    "that can be overridden by the user.";

inline constexpr char kEnableLensStandaloneFlagId[] = "enable-lens-standalone";
inline constexpr char kEnableLensStandaloneName[] =
    "Enable Lens features in Chrome.";
inline constexpr char kEnableLensStandaloneDescription[] =
    "Enables Lens image and region search to learn about the visual content "
    "you see while you browse and shop on the web.";

inline constexpr char kEnableManagedConfigurationWebApiName[] =
    "Enable Managed Configuration Web API";
inline constexpr char kEnableManagedConfigurationWebApiDescription[] =
    "Allows website to access a managed configuration provided by the device "
    "administrator for the origin.";

inline constexpr char kEnablePixelCanvasRecordingName[] =
    "Enable pixel canvas recording";
inline constexpr char kEnablePixelCanvasRecordingDescription[] =
    "Pixel canvas recording allows the compositor to raster contents aligned "
    "with the pixel and improves text rendering. This should be enabled when a "
    "device is using fractional scale factor.";

inline constexpr char kEnableProcessPerSiteUpToMainFrameThresholdName[] =
    "Enable ProcessPerSite up to main frame threshold";
inline constexpr char kEnableProcessPerSiteUpToMainFrameThresholdDescription[] =
    "Proactively reuses same-site renderer processes to host multiple main "
    "frames, up to a certain threshold.";

inline constexpr char kEnablePrintingMarginsAndScale[] =
    "Enable printing margins and scale support in chrome.printing API.";
inline constexpr char kEnablePrintingMarginsAndScaleDescription[] =
    "Allows extensions to specify margins and scale in chrome.printing API "
    "based on supported values provided by the printer.";

inline constexpr char kAlignPdfDefaultPrintSettingsWithHTMLName[] =
    "Align PDF default print settings with HTML";
inline constexpr char kAlignPdfDefaultPrintSettingsWithHTMLDescription[] =
    "Align the default print settings for PDFs with those for HTML, "
    "including scaling and centering.";

inline constexpr char kBoundaryEventDispatchTracksNodeRemovalName[] =
    "Boundary Event Dispatch Tracks Node Removal";
inline constexpr char kBoundaryEventDispatchTracksNodeRemovalDescription[] =
    "Mouse and Pointer boundary event dispatch (i.e. dispatch of enter, leave, "
    "over, out events) tracks DOM node removal to fix event pairing on "
    "ancestor nodes.";

inline constexpr char kEnablePreferencesAccountStorageName[] =
    "Enable the account data storage for preferences for syncing users";
inline constexpr char kEnablePreferencesAccountStorageDescription[] =
    "Enables storing preferences in a second, Gaia-account-scoped storage for "
    "syncing users";

inline constexpr char
    kEnableResamplingScrollEventsExperimentalPredictionName[] =
        "Enable experimental prediction for scroll events";
inline constexpr char
    kEnableResamplingScrollEventsExperimentalPredictionDescription[] =
        "Predicts the scroll amount after the vsync time to more closely match "
        "when the frame is visible.";

inline constexpr char kEnableWebAppPredictableAppUpdatingName[] =
    "Enable predictable app updating for PWAs";
inline constexpr char kEnableWebAppPredictableAppUpdatingDescription[] =
    "Enables PWA updates to be more predictable by considering changes in icon "
    "urls specified in the manifest";

inline constexpr char kEnableZeroCopyTabCaptureName[] = "Zero-copy tab capture";
inline constexpr char kEnableZeroCopyTabCaptureDescription[] =
    "Enable zero-copy content tab for getDisplayMedia() APIs.";

inline constexpr char kExcludePipFromScreenCaptureName[] =
    "Exclude Picture-in-Picture windows from screen capture";
inline constexpr char kExcludePipFromScreenCaptureDescription[] =
    "When enabled, Picture-in-Picture windows will be excluded from screen "
    "captures.";

inline constexpr char kExperimentalWebAssemblyFeaturesName[] =
    "Experimental WebAssembly";
inline constexpr char kExperimentalWebAssemblyFeaturesDescription[] =
    "Enable web pages to use experimental WebAssembly features.";

inline constexpr char kExperimentalWebAssemblySharedEverythingName[] =
    "Experimental WebAssembly Shared-Everything Threads";
inline constexpr char kExperimentalWebAssemblySharedEverythingDescription[] =
    "Enable web pages to use the experimental WebAssembly Shared-Everything "
    "Threads feature. Note that this only covers a subset of the proposal.";

inline constexpr char kEnableUnrestrictedUsbName[] =
    "Enable Isolated Web Apps to bypass USB restrictions";
inline constexpr char kEnableUnrestrictedUsbDescription[] =
    "When enabled, allows Isolated Web Apps to access blocklisted "
    "devices and protected interfaces through WebUSB API.";

inline constexpr char kEnableWasmBaselineName[] =
    "WebAssembly baseline compiler";
inline constexpr char kEnableWasmBaselineDescription[] =
    "Enables WebAssembly baseline compilation and tier up.";

inline constexpr char kEnableWasmLazyCompilationName[] =
    "WebAssembly lazy compilation";
inline constexpr char kEnableWasmLazyCompilationDescription[] =
    "Enables lazy (JIT on first call) compilation of WebAssembly modules.";

inline constexpr char kEnableWasmTieringName[] = "WebAssembly tiering";
inline constexpr char kEnableWasmTieringDescription[] =
    "Enables tiered compilation of WebAssembly (will tier up to TurboFan if "
    "#enable-webassembly-baseline is enabled).";

inline constexpr char kExperimentalWebPlatformFeaturesName[] =
    "Experimental Web Platform features";
inline constexpr char kExperimentalWebPlatformFeaturesDescription[] =
    "Enables experimental Web Platform features that are in development.";

inline constexpr char kSafeBrowsingLocalListsUseSBv5Name[] =
    "Safe Browsing Local Lists use v5 API";
inline constexpr char kSafeBrowsingLocalListsUseSBv5Description[] =
    "Fetch and check local lists using the Safe Browsing v5 API instead of the "
    "v4 Update API.";

inline constexpr char kXSLTName[] = "XSLT";
inline constexpr char kXSLTDescription[] =
    "Toggles whether or not XSLT is supported by the browser.";

inline constexpr char kSymphoniaAudioDecodingName[] =
    "Symphonia Audio Decoding";
inline constexpr char kSymphoniaAudioDecodingDescription[] =
    "Enables using the experimental Symphonia audio decoder instead of using "
    "FFMPEG for decoding audio.";

inline constexpr char kEnableWebHidInWebViewName[] = "Web HID in WebView";
inline constexpr char kEnableWebHidInWebViewDescription[] =
    "Enable WebViews to access Web HID upon embedder's permission.";

inline constexpr char kExperimentalOmniboxLabsName[] =
    "Enable extension permission omnibox.directInput";
inline constexpr char kExperimentalOmniboxLabsDescription[] =
    "Allows extensions to request permission omnibox.directInput, which "
    "enables unscoped mode in the Omnibox";
inline constexpr char kExtensionAiDataCollectionName[] =
    "Enables AI Data collection via extension";
inline constexpr char kExtensionAiDataCollectionDescription[] =
    "Enables an extension API to allow specific extensions to collect data "
    "from browser process. This data may contain profile specific information "
    " and may be otherwise unavailable to an extension.";
inline constexpr char kExtensionsCollapseMainMenuName[] =
    "Collapse Extensions Submenu";
inline constexpr char kExtensionsCollapseMainMenuDescription[] =
    "Enables a mode where if the current profile has no extensions, the "
    "extensions submenu in the application menu is replaced by a single item, "
    "e.g. \"Explore Extensions\".";
inline constexpr char kExtensionsMenuAccessControlName[] =
    "Extensions Menu Access Control";
inline constexpr char kExtensionsMenuAccessControlDescription[] =
    "Enables a redesigned extensions menu that allows the user to control "
    "extensions site access.";
inline constexpr char kIPHExtensionsMenuFeatureName[] = "IPH Extensions Menu";
inline constexpr char kIPHExtensionsMenuFeatureDescription[] =
    "Enables In-Product-Help that appears when at least one extension has "
    "access to the current page. This feature is gated by "
    "extensions-menu-access-control.";
inline constexpr char kIPHExtensionsRequestAccessButtonFeatureName[] =
    "IPH Extensions Request Access Button Feature";
inline constexpr char kIPHExtensionsRequestAccessButtonFeatureDescription[] =
    "Enables In-Product-Help that appears when at least one extension is "
    "requesting access to the current page. This feature is gated by "
    "extensions-menu-access-control.";
inline constexpr char kExtensionManifestV2DeprecationDisabledName[] =
    "Extension Manifest V2 Deprecation Disabled Stage";
inline constexpr char kExtensionManifestV2DeprecationDisabledDescription[] =
    "Displays a warning that affected MV2 extensions were turned off due to "
    "the Manifest V2 deprecation.";
inline constexpr char kExtensionManifestV2DeprecationUnsupportedName[] =
    "Extension Manifest V2 Deprecation Unsupported Stage";
inline constexpr char kExtensionManifestV2DeprecationUnsupportedDescription[] =
    "Displays a warning that affected MV2 extensions were turned off due to "
    "the Manifest V2 deprecation and cannot be re-enabled.";

inline constexpr char kCWSInfoFastCheckName[] = "CWS Info Fast Check";
inline constexpr char kCWSInfoFastCheckDescription[] =
    "When enabled, Chrome checks and fetches metadata for installed extensions "
    "more frequently.";

inline constexpr char kExtensionDisableUnsupportedDeveloperName[] =
    "Extension Disable Unsupported Developer";
inline constexpr char kExtensionDisableUnsupportedDeveloperDescription[] =
    "When enabled, disable unpacked extensions if developer mode is off.";

inline constexpr char kExtensionsToolbarZeroStateName[] =
    "Extensions Toolbar Zero State";
inline constexpr char kExtensionsToolbarZeroStateDescription[] =
    "When enabled, show an IPH to prompt users with zero extensions installed "
    "to interact with the Extensions Toolbar Button. Upon the user clicking "
    "the toolbar button, display a submenu that suggests exploring the Chrome "
    "Web Store.";
inline constexpr char kExtensionsToolbarZeroStateChoicesDisabled[] = "Disabled";
inline constexpr char kExtensionsToolbarZeroStateVistWebStore[] =
    "Visit Chrome Web Store";
inline constexpr char kExtensionsToolbarZeroStateExploreExtensionsByCategory[] =
    "Explore CWS extensions by category";

inline constexpr char kExtensionsOnChromeUrlsName[] =
    "Extensions on chrome:// URLs";
inline constexpr char kExtensionsOnChromeUrlsDescription[] =
    "Enables running extensions on chrome:// URLs, where extensions explicitly "
    "request this permission.";

inline constexpr char kExtensionsOnExtensionUrlsName[] =
    "Extensions on chrome-extension:// URLs";
inline constexpr char kExtensionsOnExtensionUrlsDescription[] =
    "Enables running extensions on chrome-extension:// URLs, where extensions "
    "explicitly request this permission.";

inline constexpr char kFractionalScrollOffsetsName[] =
    "Fractional Scroll Offsets";
inline constexpr char kFractionalScrollOffsetsDescription[] =
    "Enables fractional scroll offsets inside Blink, exposing non-integer "
    "offsets to web APIs.";

inline constexpr char kFedCmAlternativeIdentifiersName[] =
    "FedCmAlternativeIdentifiers";
inline constexpr char kFedCmAlternativeIdentifiersDescription[] =
    "Supports usernames and phone numbers as account identifiers.";

inline constexpr char kFedCmAutofillName[] = "FedCmAutofill";
inline constexpr char kFedCmAutofillDescription[] =
    "Allows RPs to enhance autofill with FedCM.";

inline constexpr char kFedCmDelegationName[] = "FedCM with delegation support";
inline constexpr char kFedCmDelegationDescription[] =
    "Enables IdPs to delegate presentation to the browser.";

inline constexpr char kFedCmErrorAttributeName[] = "FedCmErrorAttribute";
inline constexpr char kFedCmErrorAttributeDescription[] =
    "Enables the spec-compliant 'error' attribute in IdentityCredentialError "
    "while deprecating the legacy 'code' attribute.";

inline constexpr char kFedCmIdPRegistrationName[] =
    "FedCM with IdP Registration support";
inline constexpr char kFedCmIdPRegistrationDescription[] =
    "Enables RPs to get identity credentials from registered IdPs.";

inline constexpr char kFedCmIframeOriginName[] = "FedCmIframeOrigin";
inline constexpr char kFedCmIframeOriginDescription[] =
    "Allows showing iframe origins in the FedCM UI, if requested by the IDP.";

inline constexpr char kFedCmLightweightModeName[] = "FedCmLightweightMode";
inline constexpr char kFedCmLightweightModeDescription[] =
    "Enables IdPs to store user profile information using the login status "
    "API.";

inline constexpr char kFedCmMetricsEndpointName[] = "FedCmMetricsEndpoint";
inline constexpr char kFedCmMetricsEndpointDescription[] =
    "Allows the FedCM API to send performance measurement to the metrics "
    "endpoint on the identity provider side. Requires FedCM to be enabled.";

inline constexpr char kFedCmNonceInParamsName[] = "FedCmNonceInParams";
inline constexpr char kFedCmNonceInParamsDescription[] =
    "Removes nonce as an explicit parameter of the FedCM API. When enabled, a "
    "nonce may be passed in params.";

inline constexpr char kFedCmWellKnownEndpointValidationName[] =
    "FedCmWellKnownEndpointValidation";
inline constexpr char kFedCmWellKnownEndpointValidationDescription[] =
    "When enabled, accounts_endpoint and login_url must be present in "
    ".well-known/web-identity if client_metadata is used.";

inline constexpr char kFedCmWithoutWellKnownEnforcementName[] =
    "FedCmWithoutWellKnownEnforcement";
inline constexpr char kFedCmWithoutWellKnownEnforcementDescription[] =
    "Supports configURL that's not in the IdP's .well-known file.";

inline constexpr char kFedCmSegmentationPlatformName[] =
    "FedCmSegmentationPlatform";
inline constexpr char kFedCmSegmentationPlatformDescription[] =
    "Enables the segmentation platform service to provide UI volume "
    "recommendations to FedCM.";

inline constexpr char kFedCmNavigationInterceptionName[] =
    "FedCmNavigationInterception";
inline constexpr char kFedCmNavigationInterceptionDescription[] =
    "Allows IdP to intercept navigations by initiating a FedCM request.";

inline constexpr char kWebIdentityDigitalCredentialsName[] =
    "DigitalCredentials";
inline constexpr char kWebIdentityDigitalCredentialsDescription[] =
    "Enables the three-party verifier/holder/issuer identity model.";

inline constexpr char kWebIdentityDigitalCredentialsCreationName[] =
    "DigitalCredentialsCreation";
inline constexpr char kWebIdentityDigitalCredentialsCreationDescription[] =
    "Enables the Digital Credentials Creation API.";

inline constexpr char kFileHandlingIconsName[] = "File Handling Icons";
inline constexpr char kFileHandlingIconsDescription[] =
    "Allows websites using the file handling API to also register file type "
    "icons. See https://github.com/WICG/file-handling/blob/main/explainer.md "
    "for more information.";

inline constexpr char kFileSystemObserverName[] = "FileSystemObserver";
inline constexpr char kFileSystemObserverDescription[] =
    "Enables the FileSystemObserver interface, which allows websites to be "
    "notified of changes to the file system. See "
    "https://github.com/whatwg/fs/blob/main/proposals/FileSystemObserver.md "
    "for more information.";

inline constexpr char kAckCopyOutputRequestEarlyForViewTransitionName[] =
    "Ack CopyOutputRequest early for View Transition";
inline constexpr char kAckCopyOutputRequestEarlyForViewTransitionDescription[] =
    "If enabled, send acks for CopyOutputRequest completion immediately to "
    "unblock navigation for ViewTransitions while CopyOutputRequests are in "
    "progress. This is a fast-path for ViewTransitions.";

inline constexpr char kAckOnSurfaceActivationWhenInteractiveName[] =
    "Ack On Surface Activation When Interactive";
inline constexpr char kAckOnSurfaceActivationWhenInteractiveDescription[] =
    "If enabled, immediately send acks to clients when a viz surface "
    "activates and when that surface is a dependency of an interactive frame "
    "(i.e., when there is an active scroll or a touch interaction). This "
    "effectively removes back-pressure in this case. This can result in "
    "wasted work and contention, but should regularize the timing of client "
    "rendering.";

inline constexpr char kFluentOverlayScrollbarsName[] =
    "Fluent Overlay scrollbars.";
inline constexpr char kFluentOverlayScrollbarsDescription[] =
    "Stylizes scrollbars with Microsoft Fluent design and makes them overlay "
    "over the web's content.";

inline constexpr char kFluentScrollbarsName[] = "Fluent scrollbars.";
inline constexpr char kFluentScrollbarsDescription[] =
    "Stylizes scrollbars with Microsoft Fluent design.";

inline constexpr char kFillOnAccountSelectName[] =
    "Fill passwords on account selection";
inline constexpr char kFillOnAccountSelectDescription[] =
    "Filling of passwords when an account is explicitly selected by the user "
    "rather than autofilling credentials on page load.";

inline constexpr char kForceTextDirectionName[] = "Force text direction";
inline constexpr char kForceTextDirectionDescription[] =
    "Explicitly force the per-character directionality of UI text to "
    "left-to-right (LTR) or right-to-left (RTL) mode, overriding the default "
    "direction of the character language.";
inline constexpr char kForceDirectionLtr[] = "Left-to-right";
inline constexpr char kForceDirectionRtl[] = "Right-to-left";

inline constexpr char kForceUiDirectionName[] = "Force UI direction";
inline constexpr char kForceUiDirectionDescription[] =
    "Explicitly force the UI to left-to-right (LTR) or right-to-left (RTL) "
    "mode, overriding the default direction of the UI language.";

inline constexpr char kPolicyRegistrationDelayName[] =
    "Policy Registration Delay";
inline constexpr char kPolicyRegistrationDelayDescription[] =
    "Enables a configurable delay for policy registration.";

inline constexpr char kInitializePoliciesForSignedInUserInNewEntryPointsName[] =
    "Initialize policy for signed in user in new entry points";
inline constexpr char
    kInitializePoliciesForSignedInUserInNewEntryPointsDescription[] =
        "Enables policy initialization for signed in users in new entry "
        "points.";

inline constexpr char kGlobalMediaControlsUpdatedUIName[] =
    "Global Media Controls updated UI";
inline constexpr char kGlobalMediaControlsUpdatedUIDescription[] =
    "Show updated UI for Global Media Controls in all the non-CrOS desktop "
    "platforms.";

inline constexpr char kGoogleOneOfferFilesBannerName[] =
    "Google One offer Files banner";
inline constexpr char kGoogleOneOfferFilesBannerDescription[] =
    "Shows a Files banner about Google One offer.";

inline constexpr char kCastMessageLoggingName[] =
    "Enables logging of all Cast messages.";
inline constexpr char kCastMessageLoggingDescription[] =
    "Enables logging of all messages exchanged between websites, Chrome, "
    "and Cast receivers in chrome://media-router-internals.";

inline constexpr char kCastStreamingAv1Name[] =
    "Enable AV1 video encoding for Cast Streaming";
inline constexpr char kCastStreamingAv1Description[] =
    "Offers the AV1 video codec when negotiating Cast Streaming, and uses AV1 "
    "if selected for the session.";

inline constexpr char kCastStreamingHardwareH264Name[] =
    "Toggle hardware accelerated H.264 video encoding for Cast Streaming";
inline constexpr char kCastStreamingHardwareH264Description[] =
    "The default is to allow hardware H.264 encoding when recommended for the "
    "platform. If enabled, hardware H.264 encoding will always be allowed when "
    "supported by the platform. If disabled, hardware H.264 encoding will "
    "never be used.";

inline constexpr char kCastStreamingHardwareHevcName[] =
    "Toggle hardware accelerated HEVC video encoding for Cast Streaming";
inline constexpr char kCastStreamingHardwareHevcDescription[] =
    "The default is to allow hardware HEVC encoding when recommended for the "
    "platform. If enabled, hardware HEVC encoding will always be allowed when "
    "supported by the platform. If disabled, hardware HEVC encoding will "
    "never be used.";

inline constexpr char kCastStreamingHardwareVp8Name[] =
    "Toggle hardware accelerated VP8 video encoding for Cast Streaming";
inline constexpr char kCastStreamingHardwareVp8Description[] =
    "The default is to allow hardware VP8 encoding when recommended for the "
    "platform. If enabled, hardware VP8 encoding will always be allowed when "
    "supported by the platform (regardless of recommendation). If disabled, "
    "hardware VP8 encoding will never be used.";

inline constexpr char kCastStreamingHardwareVp9Name[] =
    "Toggle hardware accelerated VP9 video encoding for Cast Streaming";
inline constexpr char kCastStreamingHardwareVp9Description[] =
    "The default is to allow hardware VP9 encoding when recommended for the "
    "platform. If enabled, hardware VP9 encoding will always be allowed when "
    "supported by the platform (regardless of recommendation). If disabled, "
    "hardware VP9 encoding will never be used.";

inline constexpr char kCastStreamingMediaVideoEncoderName[] =
    "Toggles using the media::VideoEncoder implementation for Cast Streaming";
inline constexpr char kCastStreamingMediaVideoEncoderDescription[] =
    "When enabled, the media base VideoEncoder implementation is used instead "
    "of the media cast implementation.";

inline constexpr char kCastStreamingPerformanceOverlayName[] =
    "Toggle a performance metrics overlay while Cast Streaming";
inline constexpr char kCastStreamingPerformanceOverlayDescription[] =
    "When enabled, a text overlay is rendered on top of each frame sent while "
    "Cast Streaming that includes frame duration, resolution, timestamp, "
    "low latency mode, capture duration, target playout delay, target bitrate, "
    "and encoder utilization.";

inline constexpr char kCastStreamingVp8Name[] =
    "Enable VP8 video encoding for Cast Streaming";
inline constexpr char kCastStreamingVp8Description[] =
    "Offers the VP8 video codec when negotiating Cast Streaming, and uses VP8 "
    "if selected for the session. If true, software VP8 encoding will be "
    "offered and hardware VP8 encoding may be offered if enabled and available "
    "on this platform. If false, software VP8 will not be offered and hardware "
    "VP8 will only be offered if #cast-streaming-hardware-vp8 is explicitly "
    "set to true.";

inline constexpr char kCastStreamingVp9Name[] =
    "Enable VP9 video encoding for Cast Streaming";
inline constexpr char kCastStreamingVp9Description[] =
    "Offers the VP9 video codec when negotiating Cast Streaming, and uses VP9 "
    "if selected for the session.";

inline constexpr char kCastStreamingMacHardwareH264Name[] =
    "Enable hardware H264 video encoding on for Cast Streaming on macOS";
inline constexpr char kCastStreamingMacHardwareH264Description[] =
    "Offers the H264 video codec when negotiating Cast Streaming, and uses "
    "hardware-accelerated H264 encoding if selected for the session";
inline constexpr char kUseNetworkFrameworkForLocalDiscoveryName[] =
    "Use the Network Framework for local device discovery on Mac";
inline constexpr char kUseNetworkFrameworkForLocalDiscoveryDescription[] =
    "Use the Network Framework to replace the Bonjour API for local device "
    "discovery on Mac.";

inline constexpr char kCastStreamingWinHardwareH264Name[] =
    "Enable hardware H264 video encoding on for Cast Streaming on Windows";
inline constexpr char kCastStreamingWinHardwareH264Description[] =
    "Offers the H264 video codec when negotiating Cast Streaming, and uses "
    "hardware-accelerated H264 encoding if selected for the session";

inline constexpr char kCastEnableStreamingWithHiDPIName[] =
    "HiDPI tab capture support for Cast Streaming";
inline constexpr char kCastEnableStreamingWithHiDPIDescription[] =
    "Enables HiDPI tab capture during Cast Streaming mirroring sessions. May "
    "reduce performance on some platforms and also improve quality of video "
    "frames.";

inline constexpr char kChromeWebStoreNavigationThrottleName[] =
    "Chrome Web Store navigation throttle";
inline constexpr char kChromeWebStoreNavigationThrottleDescription[] =
    "When enabled, passes DM Token to the Chrome Web Store.";

inline constexpr char kFlexFirmwareUpdateName[] =
    "ChromeOS Flex Firmware Updates";
inline constexpr char kFlexFirmwareUpdateDescription[] =
    "Allow firmware updates from LVFS to be installed on ChromeOS Flex.";

inline constexpr char kGpuRasterizationName[] = "GPU rasterization";
inline constexpr char kGpuRasterizationDescription[] =
    "Use GPU to rasterize web content.";

inline constexpr char kHappyEyeballsV3Name[] = "Happy Eyeballs Version 3";
inline constexpr char kHappyEyeballsV3Description[] =
    "Enables the Happy Eyeballs Version 3 algorithm. See "
    "https://datatracker.ietf.org/doc/draft-pauly-v6ops-happy-eyeballs-v3/";

inline constexpr char kHardwareMediaKeyHandling[] =
    "Hardware Media Key Handling";
inline constexpr char kHardwareMediaKeyHandlingDescription[] =
    "Enables using media keys to control the active media session. This "
    "requires MediaSessionService to be enabled too";

inline constexpr char kHeadlessTabModelName[] = "Headless tab model";
inline constexpr char kHeadlessTabModelDescription[] =
    "Enables loading and mutating tab models on Android without an activity";

inline constexpr char kHeavyAdPrivacyMitigationsName[] =
    "Heavy ad privacy mitigations";
inline constexpr char kHeavyAdPrivacyMitigationsDescription[] =
    "Enables privacy mitigations for the heavy ad intervention. Disabling "
    "this makes the intervention deterministic. Defaults to enabled.";

inline constexpr char kHideAimOmniboxEntrypointOnUserInputName[] =
    "AI Entrypoint Disabled on User Input";
inline constexpr char kHideAimOmniboxEntrypointOnUserInputDescription[] =
    "Hide the Omnibox entrypoint for AI Mode while user is typing.";

inline constexpr char kHistoryEmbeddingsName[] = "History Embeddings";
inline constexpr char kHistoryEmbeddingsDescription[] =
    "When enabled, the history embeddings feature may operate.";

inline constexpr char kHistoryEmbeddingsAnswersName[] =
    "History Embeddings Answers";
inline constexpr char kHistoryEmbeddingsAnswersDescription[] =
    "When enabled, the history embeddings feature may answer some queries. "
    "Has no effect if the History Embeddings feature is disabled.";

inline constexpr char kTabAudioMutingName[] = "Tab audio muting UI control";
inline constexpr char kTabAudioMutingDescription[] =
    "When enabled, the audio indicators in the tab strip double as tab audio "
    "mute controls.";

inline constexpr char kCrasOutputPluginProcessorName[] =
    "Enable audio output plugin processor in CRAS";
inline constexpr char kCrasOutputPluginProcessorDescription[] =
    "When enabled, and the configuration files are properly set, the audio "
    "output will be processed by the output plugin processor.";

inline constexpr char kCrasProcessorWavDumpName[] =
    "Enable CrasProcessor WAVE file dumps";
inline constexpr char kCrasProcessorWavDumpDescription[] =
    "Make CrasProcessor produce WAVE file dumps for the audio processing "
    "pipeline";

inline constexpr char kPwaRestoreBackendName[] =
    "Enable the PWA Restore Backend";
inline constexpr char kPwaRestoreBackendDescription[] =
    "When enabled, PWA data will be sync to the backend, to support the PWA "
    "Restore UI.";

inline constexpr char kPwaRestoreUiName[] = "Enable the PWA Restore UI";
inline constexpr char kPwaRestoreUiDescription[] =
    "When enabled, the PWA Restore UI can be shown";

inline constexpr char kPwaRestoreUiAtStartupName[] =
    "Force-shows the PWA Restore UI at startup";
inline constexpr char kPwaRestoreUiAtStartupDescription[] =
    "When enabled, the PWA Restore UI will be forced to show on startup (even "
    "if the PwaRestoreUi flag is disabled and there are no apps to restore)";

inline constexpr char kStartSurfaceReturnTimeName[] =
    "Start surface return time";
inline constexpr char kStartSurfaceReturnTimeDescription[] =
    "Enable showing start surface at startup after specified time has elapsed";

inline constexpr char kHttpsFirstBalancedModeName[] =
    "Allow enabling Balanced Mode for HTTPS-First Mode.";
inline constexpr char kHttpsFirstBalancedModeDescription[] =
    "Enable tri-state HTTPS-First Mode setting in chrome://settings/security.";

inline constexpr char kHttpsFirstDialogUiName[] =
    "Dialog UI for HTTPS-First Modes";
inline constexpr char kHttpsFirstDialogUiDescription[] =
    "Use a dialog-based UI for HFM";

inline constexpr char kHttpsFirstModeIncognitoName[] =
    "HTTPS-First Mode in Incognito";
inline constexpr char kHttpsFirstModeIncognitoDescription[] =
    "Enable HTTPS-First Mode in Incognito as default setting.";

inline constexpr char kHttpsFirstModeIncognitoNewSettingsName[] =
    "HTTPS-First Mode in Incognito new Settings UI";
inline constexpr char kHttpsFirstModeIncognitoNewSettingsDescription[] =
    "Enable new HTTPS-First Mode settings UI for HTTPS-First Mode in "
    "Incognito. Must also enable #https-first-mode-incognito.";

inline constexpr char kHttpsFirstModeV2ForEngagedSitesName[] =
    "HTTPS-First Mode V2 For Engaged Sites";
inline constexpr char kHttpsFirstModeV2ForEngagedSitesDescription[] =
    "Enable Site-Engagement based HTTPS-First Mode. Shows HTTPS-First Mode "
    "interstitial on sites whose HTTPS URLs have high Site Engagement scores. "
    "Requires #https-upgrades feature to be enabled";

inline constexpr char kHttpsFirstModeForTypicallySecureUsersName[] =
    "HTTPS-First Mode For Typically Secure Users";
inline constexpr char kHttpsFirstModeForTypicallySecureUsersDescription[] =
    "Automatically enables HTTPS-First Mode if the user has a typically secure "
    "browsing pattern.";

inline constexpr char kHttpsUpgradesName[] = "HTTPS Upgrades";
inline constexpr char kHttpsUpgradesDescription[] =
    "Enable automatically upgrading all top-level navigations to HTTPS with "
    "fast fallback to HTTP.";

inline constexpr char kIgnoreGpuBlocklistName[] =
    "Override software rendering list";
inline constexpr char kIgnoreGpuBlocklistDescription[] =
    "Overrides the built-in software rendering list and enables "
    "GPU-acceleration on unsupported system configurations.";

inline constexpr char kInfobarRefreshName[] = "Infobar Refresh";
inline constexpr char kInfobarRefreshDescription[] =
    "Renders infobars with a refreshed UI.";

inline constexpr char kIncognitoScreenshotName[] = "Incognito Screenshot";
inline constexpr char kIncognitoScreenshotDescription[] =
    "Enables Incognito screenshots on Android. It will also make Incognito "
    "thumbnails visible.";

inline constexpr char kIncognitoThemeOverlayTestingName[] =
    "Incognito theme overlay for testing";
inline constexpr char kIncognitoThemeOverlayTestingDescription[] =
    "Enables incognito theme overlay for testing on the current window.";

inline constexpr char kInstanceSwitcherV2Name[] = "Instance switcher v2";
inline constexpr char kInstanceSwitcherV2Description[] =
    "Enables the updated instance switcher dialog, that uses a new layout and "
    "displays additional instance information like last access time and "
    "active/inactive status.";

inline constexpr char kInProductHelpDemoModeChoiceName[] =
    "In-Product Help Demo Mode";
inline constexpr char kInProductHelpDemoModeChoiceDescription[] =
    "Selects the In-Product Help demo mode.";

inline constexpr char kInputOnVizName[] = "Enable InputOnViz";
inline constexpr char kInputOnVizDescription[] =
    "The Flag only has affect on Android V(15)+. It enables input on "
    "web contents to be handled by Viz process in most scenarios.";

inline constexpr char kInstantHotspotRebrandName[] =
    "Instant Hotspot Improvements";

inline constexpr char kInstantHotspotRebrandDescription[] =
    "Enables Instant Hotspot rebrand/feature improvements.";

inline constexpr char kInstantHotspotOnNearbyName[] =
    "Instant Hotspot on Nearby";

inline constexpr char kInstantHotspotOnNearbyDescription[] =
    "Switches Instant Hotspot to use Nearby Presence for device discovery, as "
    "well as Nearby Connections for device communication.";

inline constexpr char
    kInvalidateSearchEngineChoiceOnDeviceRestoreDetectionName[] =
        "Invalidate search engine choice after the install detects it has been "
        "transferred to a new device";
inline constexpr char
    kInvalidateSearchEngineChoiceOnDeviceRestoreDetectionDescription[] =
        "When enabled, search engine choices made on what we assume was a "
        "different device will not be considered valid, leading to the choice "
        "screen potentially retriggering.";

inline constexpr char kJavascriptHarmonyName[] = "Experimental JavaScript";
inline constexpr char kJavascriptHarmonyDescription[] =
    "Enable web pages to use experimental JavaScript features.";

inline constexpr char kJavascriptHarmonyShippingName[] =
    "Latest stable JavaScript features";
inline constexpr char kJavascriptHarmonyShippingDescription[] =
    "Some web pages use legacy or non-standard JavaScript extensions that may "
    "conflict with the latest JavaScript features. This flag allows disabling "
    "support of those features for compatibility with such pages.";

inline constexpr char kJourneysName[] = "History Journeys";
inline constexpr char kJourneysDescription[] =
    "Enables the History Journeys UI.";

inline constexpr char kJumpStartOmniboxName[] = "Jump-start Omnibox";
inline constexpr char kJumpStartOmniboxDescription[] =
    "Modifies cold- and warm start-up "
    "process on low-end devices to reduce the time to active Omnibox, while "
    "completing core systems initialization in the background.";

inline constexpr char kAnnotatedPageContentExtractionName[] =
    "Enables annotated page content extraction";
inline constexpr char kAnnotatedPageContentExtractionDescription[] =
    "Enables annotated page content to be extracted";

inline constexpr char kExtractRelatedSearchesFromPrefetchedZPSResponseName[] =
    "Extract Related Searches from Prefetched ZPS Response";
inline constexpr char
    kExtractRelatedSearchesFromPrefetchedZPSResponseDescription[] =
        "Enables page annotation logic to source related searches data from "
        "prefetched ZPS responses";

inline constexpr char kLanguageDetectionAPIName[] =
    "Language detection web platform API";
inline constexpr char kLanguageDetectionAPIDescription[] =
    "When enabled, JS can use the web platform's language detection API";

inline constexpr char kLensOverlayName[] = "Lens overlay";
inline constexpr char kLensOverlayDescription[] =
    "Enables Lens search via an overlay on any page.";

inline constexpr char kLensOverlayEduActionChipName[] =
    "Lens Overlay EDU action chip";
inline constexpr char kLensOverlayEduActionChipDescription[] =
    "Enables Lens Overlay EDU action chip. Intended for testing the chip "
    "itself, not its triggering criteria.";

inline constexpr char kLensOverlayEntrypointLabelAltName[] =
    "Lens overlay entrypoint label";
inline constexpr char kLensOverlayEntrypointLabelAltDescription[] =
    "Replaces the string used for the Lens overlay entrypoint label.";

inline constexpr char kLensOverlayForceEmptyCsbQueryName[] =
    "Lens overlay force empty CSB query";
inline constexpr char kLensOverlayForceEmptyCsbQueryDescription[] =
    "Forces Lens overlay to issue an empty CSB query on invocation.";

inline constexpr char kLensOverlayImageContextMenuActionsName[] =
    "Lens overlay image context menu actions";
inline constexpr char kLensOverlayImageContextMenuActionsDescription[] =
    "Enables image context menu actions in the Lens overlay.";

inline constexpr char kLensOverlayNonBlockingPrivacyNoticeName[] =
    "Lens overlay non-blocking privacy notice";
inline constexpr char kLensOverlayNonBlockingPrivacyNoticeDescription[] =
    "Enables non-blocking privacy notice in the Lens overlay.";

inline constexpr char kLensOverlayOmniboxEntryPointName[] =
    "Lens Overlay Omnibox entrypoint";
inline constexpr char kLensOverlayOmniboxEntryPointDescription[] =
    "Enables icon button for Lens entrypoint in the Omnibox.";

inline constexpr char kLensOverlayOptimizationFilterName[] =
    "Lens Overlay optimization filter";
inline constexpr char kLensOverlayOptimizationFilterDescription[] =
    "Enables using the optimization filter for triggering the action chip.";

inline constexpr char kLensOverlaySidePanelOpenInNewTabName[] =
    "Lens overlay side panel open in new tab";
inline constexpr char kLensOverlaySidePanelOpenInNewTabDescription[] =
    "Enables open in new tab in the Lens overlay side panel.";

inline constexpr char kLensOverlayStraightToSrpName[] =
    "Lens overlay straight to SRP";
inline constexpr char kLensOverlayStraightToSrpDescription[] =
    "Enables straight to SRP flows for the Lens overlay.";

inline constexpr char kLensOverlayPermissionBubbleAltName[] =
    "Lens overlay permission bubble alt appearance";
inline constexpr char kLensOverlayPermissionBubbleAltDescription[] =
    "Enables Lens overlay permission bubble alt appearance.";

inline constexpr char kLensOverlayTextSelectionContextMenuEntrypointName[] =
    "Lens overlay text selection context menu entrypoint";
inline constexpr char
    kLensOverlayTextSelectionContextMenuEntrypointDescription[] =
        "Enables invoking the Lens overlay from the selected text context "
        "menu.";

inline constexpr char kLensOverlayTranslateButtonName[] =
    "Lens overlay translate button";
inline constexpr char kLensOverlayTranslateButtonDescription[] =
    "Enables translate button via the Lens overlay.";

inline constexpr char kLensOverlayTranslateLanguagesName[] =
    "More Lens overlay translate languages";
inline constexpr char kLensOverlayTranslateLanguagesDescription[] =
    "Enables more translate languages in the Lens Overlay.";

inline constexpr char kLensOverlayLatencyOptimizationsName[] =
    "Lens overlay latency optimizations";
inline constexpr char kLensOverlayLatencyOptimizationsDescription[] =
    "Enables latency optimizations for the Lens overlay.";

inline constexpr char kLensOverlayUpdatedVisualsName[] =
    "Lens overlay updated visuals";
inline constexpr char kLensOverlayUpdatedVisualsDescription[] =
    "Enables updated visuals in the Lens selection overlay.";

inline constexpr char kLensSearchAimM3Name[] =
    "Enables AIM in Lens side panel.";
inline constexpr char kLensSearchAimM3Description[] =
    "Enables AIM follow ups with the Lens overlay results side panel.";

inline constexpr char kLensAimSuggestionsName[] =
    "Lens AIM M3 Side Panel Suggestions";
inline constexpr char kLensAimSuggestionsDescription[] =
    "Enables suggestions in the Lens composebox. This will have an effect "
    "only when the Lens search AIM M3 flag is also enabled.";

inline constexpr char kLensAimSuggestionsGradientBackgroundName[] =
    "Lens AIM M3 Side Panel Suggestions Gradient Background";
inline constexpr char kLensAimSuggestionsGradientBackgroundDescription[] =
    "Enables a gradient background for the Lens composebox dropdown that does "
    "not cover the entire side panel.";

inline constexpr char kLensSearchReinvocationAffordanceName[] =
    "Lens search reinvocation affordance";
inline constexpr char kLensSearchReinvocationAffordanceDescription[] =
    "Enables the Lens button in the AIM Searchbox for reinvocation of "
    "selection overlay.";

inline constexpr char kLensSearchSidePanelNewFeedbackName[] =
    "Lens side panel new feedback";
inline constexpr char kLensSearchSidePanelNewFeedbackDescription[] =
    "Enables a new feedback entry point in the Lens side panel.";

inline constexpr char kLensSearchZeroStateCsbName[] =
    "Lens search zero state CSB";
inline constexpr char kLensSearchZeroStateCsbDescription[] =
    "Enables a zero state CSB query in Lens.";

inline constexpr char kLensVideoCitationsName[] = "Lens video citations";
inline constexpr char kLensVideoCitationsDescription[] =
    "Enables special handling for video citations in Lens.";

inline constexpr char kLensUpdatedFeedbackEntrypointName[] =
    "Lens updated feedback entrypoint";
inline constexpr char kLensUpdatedFeedbackEntrypointDescription[] =
    "Enables an updated feedback entry point in the Lens side panel.";

inline constexpr char kLoadAllTabsAtStartupName[] = "Load all tabs at startup";
inline constexpr char kLoadAllTabsAtStartupDescription[] =
    "Creates WebContents without renderers for all tabs at startup. Warning: "
    "this may have significant overhead and degrade performance.";

inline constexpr char kLockTopControlsOnLargeTabletsName[] =
    "Lock top controls on tablets";
inline constexpr char kLockTopControlsOnLargeTabletsDescription[] =
    "Disalllow scrolling off the top browser controls on large tablets";

inline constexpr char kLockTopControlsOnLargeTabletsV2Name[] =
    "Lock top controls on tablets - v2";
inline constexpr char kLockTopControlsOnLargeTabletsV2Description[] =
    "Second version of the lock top controls on tablets feature to prevent "
    "scrolling of top controls on large tablets.";

inline constexpr char kLogJsConsoleMessagesName[] =
    "Log JS console messages in system logs";
inline constexpr char kLogJsConsoleMessagesDescription[] =
    "Enable logging JS console messages in system logs, please note that they "
    "may contain PII.";

inline constexpr char kMediaRouterCastAllowAllIPsName[] =
    "Connect to Cast devices on all IP addresses";
inline constexpr char kMediaRouterCastAllowAllIPsDescription[] =
    "Have the Media Router connect to Cast devices on all IP addresses, not "
    "just RFC1918/RFC4193 private addresses.";

inline constexpr char kMigrateSyncingUserToSignedInName[] =
    "Migrate syncing user to signed in state";
inline constexpr char kMigrateSyncingUserToSignedInDescription[] =
    "When enabled, a syncing user is migrated to the signed in non-syncing "
    "state on the next browser startup.";

inline constexpr char kMobilePromoOnDesktopName[] = "Mobile Promo On Desktop";
inline constexpr char kMobilePromoOnDesktopDescription[] =
    "When enabled, shows a mobile promo on the desktop new tab page.";

inline constexpr char kMojoUseEventFdName[] =
    "Notify about new Mojo Channel messages using eventfd";
inline constexpr char kMojoUseEventFdDescription[] =
    "When enabled, prefers to use eventfd for mojo Channel over socket. "
    "Falls back to socket communication when writing to shared memory is "
    "not possible";

inline constexpr char kMostVisitedTilesCustomizationName[] =
    "Customize Most Visiteid Tiles";
inline constexpr char kMostVisitedTilesCustomizationDescription[] =
    "Adds long-click menu to fix the title and URL of a Most Visited Tile; "
    "enables MVT reordering.";

inline constexpr char kMostVisitedTilesReselectName[] =
    "Most Visited Tiles Reselect";
inline constexpr char kMostVisitedTilesReselectDescription[] =
    "When MV tiles is clicked, scans for a tab with a matching URL. "
    "If found, selects the tab and closes the NTP. Else opens into NTP.";

inline constexpr char kMostVisitedTilesNewScoringName[] =
    "Most Visited Tile: New scoring function";
inline constexpr char kMostVisitedTilesNewScoringDescription[] =
    "When showing MV tiles, use a new scoring function to compute the score of "
    "each segment.";

inline constexpr char kMulticastInDirectSocketsName[] =
    "Multicast in Direct Sockets API";
inline constexpr char kMulticastInDirectSocketsDescription[] =
    "Enables access Multicast in Direct Sockets API. See "
    "https://github.com/WICG/direct-sockets/blob/main/docs/"
    "multicast-explainer.md for "
    "details.";

inline constexpr char kCanvas2DLayersName[] =
    "Enables canvas 2D methods BeginLayer and EndLayer";
inline constexpr char kCanvas2DLayersDescription[] =
    "Enables the canvas 2D methods BeginLayer and EndLayer.";

inline constexpr char kWebMachineLearningNeuralNetworkName[] =
    "Enables WebNN API";
inline constexpr char kWebMachineLearningNeuralNetworkDescription[] =
    "Enables the Web Machine Learning Neural Network (WebNN) API. Spec at "
    "https://www.w3.org/TR/webnn/";

inline constexpr char kExperimentalWebMachineLearningNeuralNetworkName[] =
    "Enables experimental WebNN API features";
inline constexpr char
    kExperimentalWebMachineLearningNeuralNetworkDescription[] =
        "Enables additional, experimental features in Web Machine Learning "
        "Neural "
        "Network (WebNN) API. Requires the \"WebNN API\" flag to be enabled.";

inline constexpr char kWebNNCoreMLName[] = "Core ML backend for WebNN";
inline constexpr char kWebNNCoreMLDescription[] =
    "Enables using Core ML for GPU and "
    "NPU inference with the WebNN API. Disabling this flag enables a "
    "fallback to TFLite.";

inline constexpr char kWebNNCoreMLExplicitGPUOrNPUName[] =
    "Instruct Core ML to use GPU or Neural Engine explicitly";
inline constexpr char kWebNNCoreMLExplicitGPUOrNPUDescription[] =
    "Maps the WebNN \"gpu\" and \"npu\" device types to "
    "MLComputeUnitsCPUAndGPU and MLComputeUnitsCPUAndNeuralEngine instead of "
    "MLComputeUnitsAll. Disabled by default due to crashes.";

inline constexpr char kWebNNDirectMLName[] = "DirectML backend for WebNN";
inline constexpr char kWebNNDirectMLDescription[] =
    "Enables using DirectML for GPU and "
    "NPU inference with the WebNN API. Disabling this flag enables a "
    "fallback to TFLite.";

inline constexpr char kWebNNOnnxRuntimeName[] =
    "ONNX Runtime backend for WebNN";
inline constexpr char kWebNNOnnxRuntimeDescription[] =
    "Enables using ONNX Runtime for CPU, GPU and NPU inference with the WebNN "
    "API. Disabling this flag enables a fallback to DirectML or TFLite.";

inline constexpr char kSystemProxyForSystemServicesName[] =
    "Enable system-proxy for selected system services";
inline constexpr char kSystemProxyForSystemServicesDescription[] =
    "Enabling this flag will allow ChromeOS system service which require "
    "network connectivity to use the system-proxy daemon for authentication to "
    "remote HTTP web proxies.";

inline constexpr char kSystemShortcutBehaviorName[] =
    "Modifies the default behavior of system shortcuts.";
inline constexpr char kSystemShortcutBehaviorDescription[] =
    "This flag controls the default behavior of ChromeOS system shortcuts "
    "(Launcher key shortcuts).";

inline constexpr char kNewEtc1EncoderName[] = "Enable new ETC1 encoder";
inline constexpr char kNewEtc1EncoderDescription[] =
    "Enables the new ETC1 encoder implementation for tab and back/forward "
    "thumbnails.";

inline constexpr char kNotebookLmAppPreinstallName[] = "NotebookLM app preload";
inline constexpr char kNotebookLmAppPreinstallDescription[] =
    "Preloads the NotebookLM app.";

inline constexpr char kNotebookLmAppShelfPinName[] = "NotebookLM app shelf pin";
inline constexpr char kNotebookLmAppShelfPinDescription[] =
    "Pins the NotebookLM app preload to the shelf";

inline constexpr char kNotebookLmAppShelfPinResetName[] =
    "NotebookLM app shelf pin reset";
inline constexpr char kNotebookLmAppShelfPinResetDescription[] =
    "Clears state relating to pinning the NotebookLM app preload to the shelf";

inline constexpr char kNotificationSchedulerName[] = "Notification scheduler";
inline constexpr char kNotificationSchedulerDescription[] =
    "Enable notification scheduler feature.";

inline constexpr char kNotificationSchedulerDebugOptionName[] =
    "Notification scheduler debug options";
inline constexpr char kNotificationSchedulerDebugOptionDescription[] =
    "Enable debugging mode to override certain behavior of notification "
    "scheduler system for easier manual testing.";
inline constexpr char
    kNotificationSchedulerImmediateBackgroundTaskDescription[] =
        "Show scheduled notification right away.";

inline constexpr char kNotificationsSystemFlagName[] =
    "Enable system notifications.";
inline constexpr char kNotificationsSystemFlagDescription[] =
    "Enable support for using the system notification toasts and notification "
    "center on platforms where these are available.";

inline constexpr char kEnforceManagementDisclaimerName[] =
    "Enforce management disclaimer";
inline constexpr char kEnforceManagementDisclaimerDescription[] =
    "When enabled, all signed in profiles that never saw the management "
    "disclaimer will be shown the management disclaimer when they open Chrome. "
    "Every time the primary signed in account changes to a managed account, "
    "the management disclaimer will be shown.";

inline constexpr char kOfferMigrationToDiceUsersName[] =
    "Offer migration to Dice users";
inline constexpr char kOfferMigrationToDiceUsersDescription[] =
    "When enabled, offers the implicitly signed-in users a dialog to migrate "
    "to explicitly signed-in state.";

inline constexpr char kOmitCorsClientCertName[] =
    "Omit TLS client certificates if credential mode disallows";
inline constexpr char kOmitCorsClientCertDescription[] =
    "Strictly conform the Fetch spec to omit TLS client certificates if "
    "credential mode disallows. Without this flag enabled, Chrome will always "
    "try sending client certificates regardless of the credential mode.";

inline constexpr char kOmniboxAdjustIndentationName[] =
    "Adjust Indentation for Omnibox Text and Suggestions";
inline constexpr char kOmniboxAdjustIndentationDescription[] =
    "Adjusts the indentation of the omnibox and the suggestions to eliminate "
    "the visual shift when the popup opens.";

inline constexpr char kOmniboxAllowAiModeMatchesName[] =
    "Omnibox Allow AI Mode Matches";
inline constexpr char kOmniboxAllowAiModeMatchesDescription[] =
    "Allow showing AI mode matches if returned from the search server.";

inline constexpr char kOmniboxAsyncViewInflationName[] =
    "Async Omnibox view inflation";
inline constexpr char kOmniboxAsyncViewInflationDescription[] =
    "Inflate Omnibox and Suggestions views off the UI thread.";

inline constexpr char kOmniboxCalcProviderName[] = "Omnibox calc provider";
inline constexpr char kOmniboxCalcProviderDescription[] =
    "When enabled, suggests recent calculator results in the omnibox.";

inline constexpr char kOmniboxDiagnosticsName[] =
    "Omnibox Diagnostics (restart twice)";
inline constexpr char kOmniboxDiagnosticsDescription[] =
    "Allows controlling various diagnostic facilities of the Omnibox component."
    " Use sparingly, as this may produce significant amount of log output. "
    " Restart twice when changing this option.";

inline constexpr char kOmniboxForceAllowedToBeDefaultName[] =
    "Omnibox Force Allowed To Be Default";
inline constexpr char kOmniboxForceAllowedToBeDefaultDescription[] =
    "If enabled, all omnibox suggestions pretend to be inlineable. This likely "
    "has a bunch of problems.";

inline constexpr char kOmniboxGroupingFrameworkNonZPSName[] =
    "Omnibox Grouping Framework for Typed Suggestions";
inline constexpr char kOmniboxGroupingFrameworkDescription[] =
    "Enables an alternative grouping implementation for omnibox "
    "autocompletion.";

inline constexpr char kOmniboxMobileParityUpdateV2Name[] =
    "Omnibox Mobile parity update V2";
inline constexpr char kOmniboxMobileParityUpdateV2Description[] =
    "When set, applies certain assets to match Desktop visuals and "
    "descriptions, version V2";

inline constexpr char kOmniboxMultilineEditFieldName[] =
    "Omnibox Multiline edit field";
inline constexpr char kOmniboxMultilineEditFieldDescription[] =
    "When enabled, allows Omnibox input to span across multiple lines";

inline constexpr char kOmniboxMultimodalInputName[] =
    "Omnibox Multimodal Input";
inline constexpr char kOmniboxMultimodalInputDescription[] =
    "When enabled, the multimodal input toolbar is shown in the Omnibox.";

inline constexpr char kOmniboxNumNtpZpsRecentSearchesName[] =
    "Omnibox: Recent Searches on new tab page ZPS";
inline constexpr char kOmniboxNumNtpZpsRecentSearchesDescription[] =
    "Controls presence/volume of Recent Searches shown in zero-prefix context "
    "on the New Tab Page";

inline constexpr char kOmniboxNumNtpZpsTrendingSearchesName[] =
    "Omnibox: Trending Searches on new tab page ZPS";
inline constexpr char kOmniboxNumNtpZpsTrendingSearchesDescription[] =
    "Controls presence/volume of Trending Searches shown in zero-prefix "
    "context on the New Tab Page";

inline constexpr char kOmniboxNumSrpZpsRecentSearchesName[] =
    "Omnibox: Recent Searches on the SRP ZPS";
inline constexpr char kOmniboxNumSrpZpsRecentSearchesDescription[] =
    "Controls presence/volume of Recent Searches shown in zero-prefix "
    "context on the Search Results Page";

inline constexpr char kOmniboxNumSrpZpsRelatedSearchesName[] =
    "Omnibox: Related Searches on the SRP ZPS";
inline constexpr char kOmniboxNumSrpZpsRelatedSearchesDescription[] =
    "Controls presence/volume of Related Searches shown in zero-prefix "
    "context on the Search Results Page";

inline constexpr char kOmniboxNumWebZpsRecentSearchesName[] =
    "Omnibox: Recent Searches on the web ZPS";
inline constexpr char kOmniboxNumWebZpsRecentSearchesDescription[] =
    "Controls presence/volume of Recent Searches shown in zero-prefix "
    "context on the Web";

inline constexpr char kOmniboxNumWebZpsRelatedSearchesName[] =
    "Omnibox: Related Searches on the web ZPS";
inline constexpr char kOmniboxNumWebZpsRelatedSearchesDescription[] =
    "Controls presence/volume of Related Searches shown in zero-prefix "
    "context on the Web";

inline constexpr char kOmniboxNumWebZpsMostVisitedUrlsName[] =
    "Omnibox: Most Visited URLs on the web ZPS";
inline constexpr char kOmniboxNumWebZpsMostVisitedUrlsDescription[] =
    "Controls presence/volume of Most Visited URLs shown in zero-prefix "
    "context on the Web";

inline constexpr char kOmniboxToolbeltName[] = "Omnibox toolbelt";
inline constexpr char kOmniboxToolbeltDescription[] =
    "Adds a row of buttons at the bottom of the omnibox.";

inline constexpr char kOmniboxZeroSuggestPrefetchDebouncingName[] =
    "Omnibox Zero Prefix Suggest Prefetch Request Debouncing";
inline constexpr char kOmniboxZeroSuggestPrefetchDebouncingDescription[] =
    "Enables the use of a request debouncer to throttle the volume of ZPS "
    "prefetch requests issued to the remote Suggest service.";

inline constexpr char kOmniboxZeroSuggestPrefetchingName[] =
    "Omnibox Zero Prefix Suggestion Prefetching on NTP";
inline constexpr char kOmniboxZeroSuggestPrefetchingDescription[] =
    "Enables prefetching of the zero prefix suggestions for eligible users "
    "on the New Tab page.";

inline constexpr char kOmniboxZeroSuggestPrefetchingOnSRPName[] =
    "Omnibox Zero Prefix Suggestion Prefetching on SRP";
inline constexpr char kOmniboxZeroSuggestPrefetchingOnSRPDescription[] =
    "Enables prefetching of the zero prefix suggestions for eligible users "
    "on the Search Results page.";

inline constexpr char kOmniboxZeroSuggestPrefetchingOnWebName[] =
    "Omnibox Zero Prefix Suggestion Prefetching on the Web";
inline constexpr char kOmniboxZeroSuggestPrefetchingOnWebDescription[] =
    "Enables prefetching of the zero prefix suggestions for eligible users "
    "on the Web (i.e. non-NTP and non-SRP URLs).";

inline constexpr char kOmniboxZeroSuggestInMemoryCachingName[] =
    "Omnibox Zero Prefix Suggestion in-memory caching";
inline constexpr char kOmniboxZeroSuggestInMemoryCachingDescription[] =
    "Enables in-memory caching of zero prefix suggestions.";

inline constexpr char kOmniboxOnDeviceHeadSuggestionsName[] =
    "Omnibox on device head suggestions (non-incognito only)";
inline constexpr char kOmniboxOnDeviceHeadSuggestionsDescription[] =
    "Google head non personalized search suggestions provided by a compact on "
    "device model for non-incognito. Turn off this feature if you have other "
    "apps running which affects local file access (e.g. anti-virus software) "
    "and are experiencing searchbox typing lag.";
inline constexpr char kOmniboxOnDeviceHeadSuggestionsIncognitoName[] =
    "Omnibox on device head suggestions (incognito only)";
inline constexpr char kOmniboxOnDeviceHeadSuggestionsIncognitoDescription[] =
    "Google head non personalized search suggestions provided by a compact on "
    "device model for incognito. Turn off this feature if you have other "
    "apps running which affects local file access (e.g. anti-virus software) "
    "and are experiencing searchbox typing lag.";
inline constexpr char kOmniboxOnDeviceTailSuggestionsName[] =
    "Omnibox on device tail suggestions";
inline constexpr char kOmniboxOnDeviceTailSuggestionsDescription[] =
    "Google tail non personalized search suggestions provided by a compact on "
    "device model.";

inline constexpr char kOmniboxRichAutocompletionPromisingName[] =
    "Omnibox Rich Autocompletion Promising Combinations";
inline constexpr char kOmniboxRichAutocompletionPromisingDescription[] =
    "Allow autocompletion for titles and non-prefixes. Suggestions whose "
    "titles or URLs contain the user input as a continuous chunk, but not "
    "necessarily a prefix, can be the default suggestion. Otherwise, only "
    "suggestions whose URLs are prefixed by the user input can be.";

inline constexpr char kOmniboxLocalHistoryZeroSuggestBeyondNTPName[] =
    "Allow local history zero-prefix suggestions beyond NTP";
inline constexpr char kOmniboxLocalHistoryZeroSuggestBeyondNTPDescription[] =
    "Enables local history zero-prefix suggestions in every context in which "
    "the remote zero-prefix suggestions are enabled.";

inline constexpr char kOmniboxMiaZps[] = "Omnibox Mia ZPS on NTP";
inline constexpr char kOmniboxMiaZpsDescription[] =
    "Enables Mia ZPS suggestions in NTP omnibox";

inline constexpr char kOmniboxAimShortcutTypedStateName[] =
    "AIM shortcut in typed state of omnibox";
inline constexpr char kOmniboxAimShortcutTypedStateDescription[] =
    "Enables AIM shortcut in typed state of omnibox";

inline constexpr char kOmniboxMlLogUrlScoringSignalsName[] =
    "Log Omnibox URL Scoring Signals";
inline constexpr char kOmniboxMlLogUrlScoringSignalsDescription[] =
    "Enables Omnibox to log scoring signals of URL suggestions.";

inline constexpr char kOmniboxMlUrlPiecewiseMappedSearchBlendingName[] =
    "Omnibox ML Scoring with Piecewise Score Mapping";
inline constexpr char kOmniboxMlUrlPiecewiseMappedSearchBlendingDescription[] =
    "Specifies how to blend URL ML scores and search traditional scores using "
    "a piecewise ML score mapping function.";

inline constexpr char kOmniboxMlUrlScoreCachingName[] =
    "Omnibox ML URL Score Caching";
inline constexpr char kOmniboxMlUrlScoreCachingDescription[] =
    "Enables in-memory caching of ML URL scores.";

inline constexpr char kOmniboxMlUrlScoringName[] = "Omnibox ML URL Scoring";
inline constexpr char kOmniboxMlUrlScoringDescription[] =
    "Enables ML-based relevance scoring for Omnibox URL Suggestions.";

inline constexpr char kOmniboxMlUrlScoringModelName[] =
    "Omnibox URL Scoring Model";
inline constexpr char kOmniboxMlUrlScoringModelDescription[] =
    "Enables ML scoring model for Omnibox URL suggestions.";

inline constexpr char kOmniboxMlUrlSearchBlendingName[] =
    "Omnibox ML URL Search Blending";
inline constexpr char kOmniboxMlUrlSearchBlendingDescription[] =
    "Specifies how to blend URL ML scores and search traditional scores.";

inline constexpr char kOmniboxSuggestionAnswerMigrationName[] =
    "Omnibox SuggestionAnswer Migration";
inline constexpr char kOmniboxSuggestionAnswerMigrationDescription[] =
    "Uses protos instead of SuggestionAnswer to hold answer data.";

inline constexpr char kOmniboxMaxZeroSuggestMatchesName[] =
    "Omnibox Max Zero Suggest Matches";
inline constexpr char kOmniboxMaxZeroSuggestMatchesDescription[] =
    "Changes the maximum number of autocomplete matches displayed when zero "
    "suggest is active (i.e. displaying suggestions without input).";

inline constexpr char kOmniboxUIMaxAutocompleteMatchesName[] =
    "Omnibox UI Max Autocomplete Matches";
inline constexpr char kOmniboxUIMaxAutocompleteMatchesDescription[] =
    "Changes the maximum number of autocomplete matches displayed in the "
    "Omnibox UI.";

inline constexpr char kOmniboxRemoveSearchReadyOmniboxName[] =
    "Remove Search Ready Omnibox";
inline constexpr char kOmniboxRemoveSearchReadyOmniboxDescription[] =
    "When enabled, removes the Search Ready Omnibox feature.";

inline constexpr char kOmniboxStarterPackExpansionName[] =
    "Expansion pack for the Site search starter pack";
inline constexpr char kOmniboxStarterPackExpansionDescription[] =
    "Enables additional providers for the Site search starter pack feature";

inline constexpr char kOmniboxStarterPackIPHName[] =
    "IPH message for the Site search starter pack";
inline constexpr char kOmniboxStarterPackIPHDescription[] =
    "Enables an informational IPH message for the  Site search starter pack "
    "feature";

inline constexpr char kOmniboxSearchAggregatorName[] =
    "Omnibox search aggregator";
inline constexpr char kOmniboxSearchAggregatorDescription[] =
    "Enables omnibox suggestions from the search aggregator provider";

inline constexpr char kOmniboxSiteSearchName[] = "Omnibox Site Search";
inline constexpr char kOmniboxSiteSearchDescription[] =
    "Enables keyword-based site search functionality on Android devices";

inline constexpr char kOmniboxImprovementForLFFName[] =
    "Omnibox Improvement for LFF";
inline constexpr char kOmniboxImprovementForLFFDescription[] =
    "Enables desktop-like omnibox UI enhancement for large form factors";

inline constexpr char kContextualSearchBoxUsesContextualSearchProviderName[] =
    "Contextual search box uses contextual search provider";
inline constexpr char
    kContextualSearchBoxUsesContextualSearchProviderDescription[] =
        "Enables the contextual search box to use the ContextualSearchProvider "
        "instead of the ZeroSuggestProvider as the source for suggestions.";

inline constexpr char kControlledFrameWebRequestSecurityInfoName[] =
    "Enable SecurityInfo in WebRequest API for ControlledFrame";
inline constexpr char kControlledFrameWebRequestSecurityInfoDescription[] =
    "Enables SecurityInfo in WebRequest API for ControlledFrames, allowing "
    "listeners to retrieve certificate details of web requests.";

inline constexpr char kContextualSearchOpenLensActionUsesThumbnailName[] =
    "Contextual search open Lens action uses thumbnail";
inline constexpr char
    kContextualSearchOpenLensActionUsesThumbnailDescription[] =
        "Enables web content thumbnail image to override the Lens icon "
        "for the omnibox entry point action match.";

inline constexpr char kContextualSuggestionsAblateOthersWhenPresentName[] =
    "Contextual suggestions ablate others when present";
inline constexpr char
    kContextualSuggestionsAblateOthersWhenPresentDescription[] =
        "Makes contextual search suggestions exclusive in zero suggest.";

inline constexpr char kContextualSuggestionsUiImprovementsName[] =
    "Contextual suggestions UI improvements";
inline constexpr char kContextualSuggestionsUiImprovementsDescription[] =
    "Enables UI improvements for contextual suggestions (e.g. icon and "
    "animation).";

inline constexpr char kOmniboxContextualSearchOnFocusSuggestionsName[] =
    "Omnibox contextual search on focus suggestions";
inline constexpr char kOmniboxContextualSearchOnFocusSuggestionsDescription[] =
    "Enables omnibox contextual search suggestions in zero prefix suggest.";

inline constexpr char kOmniboxContextualSuggestionsName[] =
    "Omnibox contextual suggestions";
inline constexpr char kOmniboxContextualSuggestionsDescription[] =
    "Enables omnibox contextual suggestions.";

inline constexpr char kOmniboxFocusTriggersWebAndSRPZeroSuggestName[] =
    "Omnibox on-focus suggestions on web and SRP";
inline constexpr char kOmniboxFocusTriggersWebAndSRPZeroSuggestDescription[] =
    "Enables zero-prefix suggestions on web and SRP when the omnibox is "
    "focused, subject to the same conditions and restrictions as on-clobber "
    "suggestions.";

inline constexpr char kOmniboxHideSuggestionGroupHeadersName[] =
    "Hide suggestion group headers in the Omnibox popup";
inline constexpr char kOmniboxHideSuggestionGroupHeadersDescription[] =
    "If enabled, suggestion group headers will be hidden in the Omnibox popup "
    "(e.g. to minimize visual clutter in the zero-prefix state)";

inline constexpr char kOmniboxUrlSuggestionsOnFocus[] =
    "Omnibox on-focus URL suggestions on web and SRP";
inline constexpr char kOmniboxUrlSuggestionsOnFocusDescription[] =
    "Enables zero-prefix URL suggestions on web and SRP when the omnibox is "
    "focused.";

inline constexpr char kOmniboxShowPopupOnMouseReleasedName[] =
    "Show omnibox suggestions popup on mouse released";
inline constexpr char kOmniboxShowPopupOnMouseReleasedDescription[] =
    "Enables delaying presentation of the omnibox suggestions popup until the "
    "mouse is released.";

inline constexpr char kOmniboxZpsSuggestionLimit[] =
    "Omnibox suggestion limit for zero prefix suggestions";
inline constexpr char kOmniboxZpsSuggestionLimitDescription[] =
    "Enables limits on the total number of suggestions, as well as separate "
    "limits for search and URL suggestions in the omnibox.";

inline constexpr char kWebUIOmniboxAimPopupName[] = "WebUI Omnibox AIM Popup";
inline constexpr char kWebUIOmniboxAimPopupDescription[] =
    "If enabled, using certain trigger operations the omnibox suggestions "
    "popup transition to showing the AI-Mode "
    "compose-box input and suggestions.";

inline constexpr char kWebUIOmniboxFullPopupName[] = "WebUI Omnibox Full Popup";
inline constexpr char kWebUIOmniboxFullPopupDescription[] =
    "If enabled, shows the omnibox suggestions and the search input in the "
    "popup in WebUI.";

inline constexpr char kWebUIOmniboxPopupName[] = "WebUI Omnibox Popup";
inline constexpr char kWebUIOmniboxPopupDescription[] =
    "If enabled, shows the omnibox suggestions in the popup in WebUI.";

inline constexpr char kWebUIOmniboxPopupDebugName[] =
    "WebUI Omnibox Popup Debug Mode";
inline constexpr char kWebUIOmniboxPopupDebugDescription[] =
    "Enables the WebUI for omnibox suggestions without modifying the popup UI.";

inline constexpr char kOmniboxDynamicMaxAutocompleteName[] =
    "Omnibox Dynamic Max Autocomplete";
inline constexpr char kOmniboxDynamicMaxAutocompleteDescription[] =
    "Configures the maximum number of autocomplete matches displayed in the "
    "Omnibox UI dynamically based on the number of URL matches.";

inline constexpr char kOptimizationGuideDebugLogsName[] =
    "Enable optimization guide debug logs";
inline constexpr char kOptimizationGuideDebugLogsDescription[] =
    "Enables the optimization guide to log and save debug messages that can be "
    "shown in the internals page.";

inline constexpr char kOptimizationGuideModelExecutionName[] =
    "Enables optimization guide model execution";
inline constexpr char kOptimizationGuideModelExecutionDescription[] =
    "Enables the optimization guide to execute models.";

inline constexpr char kOptimizationGuideEnableDogfoodLoggingName[] =
    "Enable optimization guide dogfood logging";
inline constexpr char kOptimizationGuideEnableDogfoodLoggingDescription[] =
    "If this client is a Google-internal dogfood client, overrides enterprise "
    "policy to enable model quality logs. Googlers: See "
    "go/chrome-mqls-debug-logging for details.";

inline constexpr char kOptimizationGuideOnDeviceModelName[] =
    "Enables optimization guide on device";
inline constexpr char kOptimizationGuideOnDeviceModelDescription[] =
    "Enables the optimization guide to execute models on device.";

inline constexpr char kOptimizationGuideOnDeviceModelAndroidName[] =
    "Enables optimization guide on device on Android";
inline constexpr char kOptimizationGuideOnDeviceModelAndroidDescription[] =
    "Enables the optimization guide to execute models on device on Android.";

inline constexpr char kOrganicRepeatableQueriesName[] =
    "Organic repeatable queries in Most Visited tiles";
inline constexpr char kOrganicRepeatableQueriesDescription[] =
    "Enables showing the most repeated queries, from the device browsing "
    "history, organically among the most visited sites in the MV tiles.";

inline constexpr char kOriginAgentClusterDefaultName[] =
    "Origin-keyed Agent Clusters by default";
inline constexpr char kOriginAgentClusterDefaultDescription[] =
    "Select the default behaviour for the Origin-Agent-Cluster http header. "
    "If enabled, an absent header will cause pages to be assigned to an "
    "origin-keyed agent cluster, and to a site-keyed agent cluster when "
    "disabled. Documents whose agent clusters are origin-keyed cannot set "
    "document.domain to relax the same-origin policy.";

inline constexpr char kOriginKeyedProcessesByDefaultName[] =
    "Origin-keyed Processes by default";
inline constexpr char kOriginKeyedProcessesByDefaultDescription[] =
    "Enables origin-keyed process isolation for most pages (i.e., those "
    "assigned to an origin-keyed agent cluster by default). This improves "
    "security but also increases the number of processes created. Note: "
    "enabling this feature also enables 'Origin-keyed Agent Clusters by "
    "default'.";

inline constexpr char kOverlayScrollbarsName[] = "Overlay Scrollbars";
inline constexpr char kOverlayScrollbarsDescription[] =
    "Enable the experimental overlay scrollbars implementation. You must also "
    "enable threaded compositing to have the scrollbars animate.";

inline constexpr char kOverlayScrollbarsFlashWhenMouseEnterName[] =
    "Flash Overlay Scrollbars When Mouse Enter";
inline constexpr char kOverlayScrollbarsFlashWhenMouseEnterDescription[] =
    "Flash Overlay Scrollbars When Mouse Enter a scrollable area. You must also"
    " enable Overlay Scrollbars.";

inline constexpr char kOverlayScrollbarsFlashOnceVisibleOnViewportName[] =
    "Flash Overlay Scrollbars Once When Visible";
inline constexpr char
    kOverlayScrollbarsFlashOnceVisibleOnViewportDescription[] =
        "Flash Overlay Scrollbars only once per scrollbar and when they become "
        "visible on the viewport. You must also enable Overlay Scrollbars.";

inline constexpr char kOverlayStrategiesName[] = "Select HW overlay strategies";
inline constexpr char kOverlayStrategiesDescription[] =
    "Select strategies used to promote quads to HW overlays. Note that "
    "strategies other than Default may break playback of protected content.";
inline constexpr char kOverlayStrategiesDefault[] = "Default";
inline constexpr char kOverlayStrategiesNone[] = "None";
inline constexpr char kOverlayStrategiesUnoccludedFullscreen[] =
    "Unoccluded fullscreen buffers (single-fullscreen)";
inline constexpr char kOverlayStrategiesUnoccluded[] =
    "Unoccluded buffers (single-fullscreen,single-on-top)";
inline constexpr char kOverlayStrategiesOccludedAndUnoccluded[] =
    "Occluded and unoccluded buffers "
    "(single-fullscreen,single-on-top,underlay)";

inline constexpr char kOverscrollEffectOnNonRootScrollersName[] =
    "Overscroll effect on non-root scrollers";
inline constexpr char kOverscrollEffectOnNonRootScrollersDescription[] =
    "Enables elastic overscroll effect on scrollers other than the root "
    "document (e.g. iframes and overflow areas).";

inline constexpr char kOverscrollHistoryNavigationName[] =
    "Overscroll history navigation";
inline constexpr char kOverscrollHistoryNavigationDescription[] =
    "History navigation in response to horizontal overscroll.";

inline constexpr char kPageActionsMigrationName[] = "Page actions migration";
inline constexpr char kPageActionsMigrationDescription[] =
    "Enables a new internal framework for driving page actions behavior.";

inline constexpr char kPageContentAnnotationsName[] =
    "Page content annotations";
inline constexpr char kPageContentAnnotationsDescription[] =
    "Enables page content to be annotated on-device.";

inline constexpr char kPageContentAnnotationsRemotePageMetadataName[] =
    "Page content annotations - Remote page metadata";
inline constexpr char kPageContentAnnotationsRemotePageMetadataDescription[] =
    "Enables fetching of page load metadata to be persisted on-device.";

inline constexpr char kPageContentCacheName[] = "Page content cache";
inline constexpr char kPageContentCacheDescription[] =
    "Enables caching of the annotated page content and screenshot";

inline constexpr char kPageEmbeddedPermissionControlName[] =
    "Page embedded permission control (permission element)";
inline constexpr char kPageEmbeddedPermissionControlDescription[] =
    "Enables the Page Embedded Permission Control feature, which allows the "
    "use of the HTML 'permission' element.";

inline constexpr char kGeolocationPermissionControlName[] =
    "Geolocation permission control (geolocation element)";
inline constexpr char kGeolocationPermissionControlDescription[] =
    "Enables the Geolocation Permission Control feature, which allows the "
    "use of the HTML 'geolocation' element.";

inline constexpr char kPageVisibilityPageContentAnnotationsName[] =
    "Page visibility content annotations";
inline constexpr char kPageVisibilityPageContentAnnotationsDescription[] =
    "Enables annotating the page visibility model for each page load "
    "on-device.";

inline constexpr char kParallelDownloadingName[] = "Parallel downloading";
inline constexpr char kParallelDownloadingDescription[] =
    "Enable parallel downloading to accelerate download speed.";

inline constexpr char kPartitionAllocMemoryTaggingName[] =
    "PartitionAlloc Memory Tagging";
inline constexpr char kPartitionAllocMemoryTaggingDescription[] =
    "Enable memory tagging in PartitionAlloc.";
inline constexpr char kPartitionAllocWithAdvancedChecksName[] =
    "PartitionAlloc with Advanced Checks";
inline constexpr char kPartitionAllocWithAdvancedChecksDescription[] =
    "Enables an extra security layer on PartitionAlloc.";

inline constexpr char kPartitionVisitedLinkDatabaseWithSelfLinksName[] =
    "Partition the Visited Link Database, including 'self-links'";
inline constexpr char kPartitionVisitedLinkDatabaseWithSelfLinksDescription[] =
    "Style links as visited only if they have been clicked from this top-level "
    "site and frame origin before. Additionally, style links pointing to the "
    "same URL as the page it is displayed on, which have been :visited from "
    "any top-level site and frame origin, if they are displayed in a top-level "
    "frame or same-origin subframe.";

inline constexpr char kPasskeyUnlockErrorUiName[] = "Passkey Unlock Error UI";
inline constexpr char kPasskeyUnlockErrorUiDescription[] =
    "Enables showing the passkey unlock error UI to passkey users in case when "
    "their access to passkeys is “locked” and when they have an available user "
    "verification mechanism (either a system UV or a GPM PIN). This flag "
    "requires the flag `PasskeyUnlockManager` to be active.";

inline constexpr char kPasskeyUnlockManagerName[] = "Passkey Unlock Manager";
inline constexpr char kPasskeyUnlockManagerDescription[] =
    "Enables the Passkey Unlock Manager, which tracks the state of passkeys "
    "and publishes the corresponding metrics.";

inline constexpr char kPasswordFormClientsideClassifierName[] =
    "Clientside password form classifier.";
inline constexpr char kPasswordFormClientsideClassifierDescription[] =
    "Enable usage of new password form classifier on the client.";

inline constexpr char kPasswordFormGroupedAffiliationsName[] =
    "Grouped affiliation password suggestions";
inline constexpr char kPasswordFormGroupedAffiliationsDescription[] =
    "Enables offering credentials coming from grouped domains for "
    "filling";

inline constexpr char kPasswordManagerShowSuggestionsOnAutofocusName[] =
    "Showing password suggestions on autofocused password forms";
inline constexpr char kPasswordManagerShowSuggestionsOnAutofocusDescription[] =
    "Enables showing password suggestions without requiring the user to "
    "click on the already focused field if the field was autofocused on "
    "the page load.";

inline constexpr char kPasswordManualFallbackAvailableName[] =
    "Password manual fallback";
inline constexpr char kPasswordManualFallbackAvailableDescription[] =
    "Enables triggering password suggestions through the context menu";

inline constexpr char kPdfXfaFormsName[] = "PDF XFA support";
inline constexpr char kPdfXfaFormsDescription[] =
    "Enables support for XFA forms in PDFs. "
    "Has no effect if Chrome was not built with XFA support, or if controlled "
    "by an enterprise policy.";

inline constexpr char kAutoWebContentsDarkModeName[] =
    "Auto Dark Mode for Web Contents";
inline constexpr char kAutoWebContentsDarkModeDescription[] =
    "Automatically render all web contents using a dark theme.";

inline constexpr char kForcedColorsName[] = "Forced Colors";
inline constexpr char kForcedColorsDescription[] =
    "Enables forced colors mode for web content.";

inline constexpr char kLeftHandSideActivityIndicatorsName[] =
    "Left-hand side activity indicators";
inline constexpr char kLeftHandSideActivityIndicatorsDescription[] =
    "Moves activity indicators to the left-hand side of location bar.";

inline constexpr char kMerchantTrustName[] = "Merchant Trust";
inline constexpr char kMerchantTrustDescription[] =
    "Enables the merchant trust UI in page info.";

inline constexpr char kPrivacyPolicyInsightsName[] = "Privacy Policy Insights";
inline constexpr char kPrivacyPolicyInsightsDescription[] =
    "Enables the privacy policy insights UI in page info.";

inline constexpr char kCrosSystemLevelPermissionBlockedWarningsName[] =
    "Chrome OS block warnings";
inline constexpr char kCrosSystemLevelPermissionBlockedWarningsDescription[] =
    "Displays warnings in browser if camera, microphone or geolocation is "
    "disabled in the OS.";

inline constexpr char kPermissionsAIv3Name[] = "PermissionsAIv3";
inline constexpr char kPermissionsAIv3Description[] =
    "Use the Permission Predictions Service and the AIv3 model to surface "
    "permission notification requests using a quieter UI when the likelihood "
    "of the user granting the permission is predicted to be low. Requires "
    "`Make Searches and Browsing Better` to be enabled.";

inline constexpr char kPermissionsAIv4Name[] = "PermissionsAIv4";
inline constexpr char kPermissionsAIv4Description[] =
    "Use the Permission Predictions Service and the AIv4 model to surface "
    "permission notification requests using a quieter UI when the likelihood "
    "of the user granting the permission is predicted to be low. Requires "
    "`Make Searches and Browsing Better` to be enabled.";

inline constexpr char kPermissionsAIP92Name[] = "PermissionsAIP92";
inline constexpr char kPermissionsAIP92Description[] =
    "Use the Permission Predictions Service and with P92 adjustments to "
    "surface permission notification requests using a quieter UI when the "
    "likelihood of the user granting the permission is predicted to be low. "
    "Requires `Make Searches and Browsing Better` to be enabled.";

inline constexpr char kPermissionSiteSettingsRadioButtonName[] =
    "Permission radio buttons in Site Settings";
inline constexpr char kPermissionSiteSettingsRadioButtonDescription[] =
    "Enables radio buttons for permissions in SiteSettings";

inline constexpr char kReportNotificationContentDetectionDataName[] =
    "Option to report notifications to Google";
inline constexpr char kReportNotificationContentDetectionDataDescription[] =
    "Enables reporting a notification's contents to Google, when the user taps "
    "the `Report` button on the notification.";

inline constexpr char kReportOmniboxAutofocusHeaderName[] =
    "Option to report 'X-Omnibox-Autofocus' header";
inline constexpr char kReportOmniboxAutofocusHeaderDescription[] =
    "Enables reporting 'X-Omnibox-Autofocus' header to Google.";

inline constexpr char kShowRelatedWebsiteSetsPermissionGrantsName[] =
    "Show permission grants from Related Website Sets";
inline constexpr char kShowRelatedWebsiteSetsPermissionGrantsDescription[] =
    "Shows permission grants created by Related Website Sets in Chrome "
    "Settings UI and Page Info Bubble, "
    "default is hidden";

inline constexpr char kShowWarningsForSuspiciousNotificationsName[] =
    "Show Warnings for Suspicious Notifications";
inline constexpr char kShowWarningsForSuspiciousNotificationsDescription[] =
    "Enables replacing notification contents with a warning when the on-device "
    "notification content detection model returns a suspicious verdict.";

inline constexpr char kSearchInSettingsName[] = "Search in Settings";
inline constexpr char kSearchInSettingsDescription[] =
    "Enable search in settings";

inline constexpr char kGlobalCacheListForGatingNotificationProtectionsName[] =
    "Global cache list for gating notification protections";
inline constexpr char
    kGlobalCacheListForGatingNotificationProtectionsDescription[] =
        "Enables using the global cache list, rather than using the Safe "
        "Browsing "
        "allowlist, to gate notification content warnings and behavior-based "
        "telemetry.";

inline constexpr char kAnnotatedPageContentsForVirtualStructureName[] =
    "Use annotated page contents to populate virtual structure";
inline constexpr char kAnnotatedPageContentsForVirtualStructureDescription[] =
    "Use annotated page content proto instead of accessibility snapshot to "
    "populate virtual structure on tabbed activity.";

inline constexpr char kPowerBookmarkBackendName[] = "Power bookmark backend";
inline constexpr char kPowerBookmarkBackendDescription[] =
    "Enables storing additional metadata to support power bookmark features.";

inline constexpr char kPrerender2EarlyDocumentLifecycleUpdateName[] =
    "Prerender more document lifecycle phases";
inline constexpr char kPrerender2EarlyDocumentLifecycleUpdateDescription[] =
    "Allows prerendering pages to execute more lifecycle updates, such as "
    "prepaint, before activation";

inline constexpr char kTreesInVizName[] = "Trees in viz";
inline constexpr char kTreesInVizDescription[] =
    "Enables the renderer to send a CC LayerTree to the viz/gpu process "
    "instead of a CompositorFrame. This allows viz to generate and submit "
    "the CompositorFrame directly.";

inline constexpr char kEnableOmniboxSearchPrefetchName[] =
    "Omnibox prefetch Search";
inline constexpr char kEnableOmniboxSearchPrefetchDescription[] =
    "Allows omnibox to prefetch likely search suggestions provided by the "
    "Default Search Engine";

inline constexpr char kEnableOmniboxClientSearchPrefetchName[] =
    "Omnibox client prefetch Search";
inline constexpr char kEnableOmniboxClientSearchPrefetchDescription[] =
    "Allows omnibox to prefetch search suggestions provided by the Default "
    "Search Engine that the client thinks are likely to be navigated. Requires "
    "chrome://flags/#omnibox-search-prefetch";

inline constexpr char kNtpComposeboxUsesChromeComposeClientName[] =
    "Composebox uses chrome-compose client";
inline constexpr char kNtpComposeboxUsesChromeComposeClientDescription[] =
    "Composebox will use chrome-compose client when querying suggest for "
    "unimodal typed inputs instead of chrome-omni.";

inline constexpr char kPrivacySandboxAdTopicsContentParityName[] =
    "Privacy Sandbox Ad Topics Content Parity";
inline constexpr char kPrivacySandboxAdTopicsContentParityDescription[] =
    "Enables the Ad Topics card in the Privacy Guide to be displayed. This "
    "flag also updates UI and text of the Ad Topics settings page and Topics "
    "Consent Dialog. All of these changes are subject to regional "
    "availability.";

inline constexpr char kPrivacySandboxAdsApiUxEnhancementsName[] =
    "Privacy Sandbox Ads API UX Enhancements";
inline constexpr char kPrivacySandboxAdsApiUxEnhancementsDescription[] =
    "Enables UI and text updates to the Privacy Sandbox Ads APIs Notice and "
    "Consent UX, and settings pages to improve user comprehension";

inline constexpr char kPrivacySandboxEnrollmentOverridesName[] =
    "Privacy Sandbox Enrollment Overrides";
inline constexpr char kPrivacySandboxEnrollmentOverridesDescription[] =
    "Allows a list of sites to use Privacy Sandbox features without them being "
    "enrolled and attested into the Privacy Sandbox experiment. See: "
    "https://developer.chrome.com/en/docs/privacy-sandbox/enroll/";

inline constexpr char kPrivacySandboxInternalsName[] =
    "Privacy Sandbox Internals Page";
inline constexpr char kPrivacySandboxInternalsDescription[] =
    "Enables the chrome://privacy-sandbox-internals debugging page.";

inline constexpr char kPrivateMetricsEnablePumaName[] =
    "Enable Private User Metrics";
inline constexpr char kPrivateMetricsEnablePumaDescription[] =
    "Enables collection of Private User Metrics.";

inline constexpr char kPrivateMetricsEnablePumaRcName[] =
    "Enable Private User Metrics for Regional Capabilities";
inline constexpr char kPrivateMetricsEnablePumaRcDescription[] =
    "Enables collection of Private User Metrics for Regional Capabilities. For "
    "it to work, Private User Metrics need to be enabled too.";

inline constexpr char kProfileSignalsReportingEnabledName[] =
    "Profile Signals Reporting Enabled";
inline constexpr char kProfileSignalsReportingEnabledDescription[] =
    "Enables the profile signals reporting flow for Chrome Enterprise.";

inline constexpr char kPropagateDeviceContentFiltersToSupervisedUserName[] =
    "Propagate device content filters to supervised user";
inline constexpr char
    kPropagateDeviceContentFiltersToSupervisedUserDescription[] =
        "Propagates the device settings about content filters to supervised "
        "user features.";

inline constexpr char kProtectedAudiencesConsentedDebugTokenName[] =
    "Protected Audiences Consented Debug Token";
inline constexpr char kProtectedAudiencesConsentedDebugTokenDescription[] =
    "Enables Protected Audience Consented Debugging with the provided token. "
    "Protected Audience auctions running on a Bidding and Auction API trusted "
    "server with a matching token will be able to log information about the "
    "auction to enable debugging. Note that this logging may include "
    "information about the user's browsing history normally kept private.";

inline constexpr char kPullToRefreshName[] = "Pull-to-refresh gesture";
inline constexpr char kPullToRefreshDescription[] =
    "Pull-to-refresh gesture in response to vertical overscroll.";
inline constexpr char kPullToRefreshEnabledTouchscreen[] =
    "Enabled for touchscreen only";

inline constexpr char kPwaUpdateDialogForAppIconName[] =
    "Enable PWA install update dialog for icon changes";
inline constexpr char kPwaUpdateDialogForAppIconDescription[] =
    "Enable a confirmation dialog that shows up when a PWA changes its icon";

inline constexpr char kRenderDocumentName[] = "Enable RenderDocument";
inline constexpr char kRenderDocumentDescription[] =
    "Enable swapping RenderFrameHosts on same-site navigations";

inline constexpr char kRendererSideContentDecodingName[] =
    "Renderer-side content decoding";
inline constexpr char kRendererSideContentDecodingDescription[] =
    "Enables renderer-side content decoding (decompression). When enabled, the "
    "network service sends compressed HTTP response bodies to the renderer "
    "process.";

inline constexpr char kDeviceBoundSessionAccessObserverSharedRemoteName[] =
    "Reduce device bound session access observer IPC";
inline constexpr char
    kDeviceBoundSessionAccessObserverSharedRemoteDescription[] =
        "Enables the optimization of reducing unnecessary IPC for cloning "
        "DeviceBoundSessionAccessObserver.";

inline constexpr char kBackgroundCompactMessageName[] =
    "Enable Background Compaction";
inline constexpr char kBackgroundCompactDescription[] =
    "Compact memory for all tabs while chrome is backgrounded";
inline constexpr char kRunningCompactMessageName[] =
    "Enable Running Compaction";
inline constexpr char kRunningCompactDescription[] =
    "Compact memory tabs that haven't been used in a while while chrome "
    "is running.";

inline constexpr char kRcapsDynamicProfileCountryName[] =
    "Dynamic Profile Country";
inline constexpr char kRcapsDynamicProfileCountryDescription[] =
    "When enabled, Chrome updates the country associated with "
    "the profile on open";

inline constexpr char kQuicName[] = "Experimental QUIC protocol";
inline constexpr char kQuicDescription[] =
    "Enable experimental QUIC protocol support.";

inline constexpr char kQuickAppAccessTestUIName[] =
    "Internal test: quick app access";
inline constexpr char kQuickAppAccessTestUIDescription[] =
    "Show an app in the quick app access area at the start of the session";

inline constexpr char kQuickShareV2Name[] = "Quick Share v2";
inline constexpr char kQuickShareV2Description[] =
    "Enables Quick Share v2, which defaults Quick Share to 'Your Devices' "
    "visibility, removes the 'Selected Contacts' visibility, removes the Quick "
    "Share On/Off toggle.";

inline constexpr char kSendTabToSelfIOSPushNotificationsName[] =
    "Send tab to self iOS push notifications";
inline constexpr char kSendTabToSelfIOSPushNotificationsDescription[] =
    "Feature to allow users to send tabs to their iOS device through a system "
    "push notification.";

inline constexpr char kSensitiveContentName[] =
    "Redact sensitive content during screen sharing, screen recording, "
    "and similar actions";

inline constexpr char kSensitiveContentDescription[] =
    "When enabled, if sensitive form fields (such as credit cards, passwords) "
    "are present on the page, the entire content area is redacted during "
    "screen sharing, screen recording, and similar actions. This feature "
    "works only on Android V or above.";

inline constexpr char kSensitiveContentWhileSwitchingTabsName[] =
    "Redact sensitive content while switching tabs during screen sharing, "
    "screen recording, and similar actions";

inline constexpr char kSensitiveContentWhileSwitchingTabsDescription[] =
    "When enabled, if a tab switching surface provides a preview of a tab that "
    "contains sensitive content, the screen is redacted during screen sharing, "
    "screen recording, and similar actions. This feature works only on Android "
    "V or above, and if #sensitive-content is also enabled.";

inline constexpr char kSettingsAppNotificationSettingsName[] =
    "Split notification permission settings";
inline constexpr char kSettingsAppNotificationSettingsDescription[] =
    "Remove per-app notification permissions settings from the quick settings "
    "menu. Notification permission settings will be moved to the ChromeOS "
    "settings app.";

inline constexpr char kRecordWebAppDebugInfoName[] =
    "Record web app debug info";
inline constexpr char kRecordWebAppDebugInfoDescription[] =
    "Enables recording additional web app related debugging data to be "
    "displayed in: chrome://web-app-internals";

inline constexpr char kReduceIPAddressChangeNotificationName[] =
    "Reduce IP address change notification";
inline constexpr char kReduceIPAddressChangeNotificationDescription[] =
    "Reduce the frequency of IP address change notifications that result in "
    "TCP and QUIC connection resets.";

inline constexpr char kReduceAcceptLanguageHTTPName[] =
    "Reduce Accept-Language request header only";
inline constexpr char kReduceAcceptLanguageHTTPDescription[] =
    "Reduce the amount of information available in the Accept-Language request "
    "header only. chrome://flags/#reduce-accept-language overrides this flag, "
    "and if enabled, the changes will take effect for Javascript as well. See "
    "https://github.com/explainers-by-googlers/reduce-accept-language for more "
    "information.";

inline constexpr char kReduceAcceptLanguageName[] =
    "Reduce Accept-Language request header and JavaScript navigator.languages.";
inline constexpr char kReduceAcceptLanguageDescription[] =
    "Reduce the amount of information in the Accept-Language request header "
    "and JavaScript navigator.languages. Enabling this flag overrides the "
    "behavior of chrome://flags/#reduce-accept-language-http, which by itself "
    "only reduces the Accept-Language request header when enabled. For more "
    "information, see "
    "https://github.com/explainers-by-googlers/reduce-accept-language.";

inline constexpr char kReduceTransferSizeUpdatedIPCName[] =
    "Reduce TransferSizeUpdated IPC";
inline constexpr char kReduceTransferSizeUpdatedIPCDescription[] =
    "When enabled, the network service will send TransferSizeUpdatedIPC IPC "
    "only when DevTools is attached or the request is for an ad request.";

inline constexpr char kReduceUserAgentDataLinuxPlatformVersionName[] =
    "Reduce Linux platform version Client Hint";
inline constexpr char kReduceUserAgentDataLinuxPlatformVersionDescription[] =
    "Set platform version Client Hint on Linux to empty string.";

inline constexpr char kReplaceSyncPromosWithSignInPromosName[] =
    "Replace all sync-related UI with sign-in ones";
inline constexpr char kReplaceSyncPromosWithSignInPromosDescription[] =
    "When enabled, all sync-related UIs will be replaced by sign-in ones.";

inline constexpr char kResetShortcutCustomizationsName[] =
    "Reset all shortcut customizations";
inline constexpr char kResetShortcutCustomizationsDescription[] =
    "Resets all shortcut customizations on startup.";

inline constexpr char kResponsiveIframesName[] = "Responsive Iframes";
inline constexpr char kResponsiveIframesDescription[] =
    "Enable responsively-sized iframes.";

inline constexpr char kRobustWindowManagementName[] =
    "Robust window management";
inline constexpr char kRobustWindowManagementDescription[] =
    "Enables robust window management which includes being able to easily find "
    "switch between, and resume specific Chrome windows. Essentially, "
    "experiencing predictable and reliable window behavior similar to desktop "
    "browsers.";

inline constexpr char kRobustWindowManagementExperimentalName[] =
    "Robust window management experimental";
inline constexpr char kRobustWindowManagementExperimentalDescription[] =
    "Enables more experimental features for robust window managements. This "
    "enables users to effortlessly manage multiple tasks with reliable window "
    "switching and restoration, ensuring they never lose their work or "
    "context.";

inline constexpr char kRootScrollbarFollowsTheme[] =
    "Make scrollbar follow theme";
inline constexpr char kRootScrollbarFollowsThemeDescription[] =
    "If enabled makes the root scrollbar follow the browser's theme color.";

inline constexpr char kMBIModeName[] = "MBI Scheduling Mode";
inline constexpr char kMBIModeDescription[] =
    "Enables independent agent cluster scheduling, via the "
    "AgentSchedulingGroup infrastructure.";

inline constexpr char kSafetyCheckUnusedSitePermissionsName[] =
    "Permission Module for unused sites in Safety Check";
inline constexpr char kSafetyCheckUnusedSitePermissionsDescription[] =
    "When enabled, adds the unused sites permission module to Safety Check on "
    "desktop. The module will be shown depending on the browser state.";

inline constexpr char kSafetyHubDisruptiveNotificationRevocationName[] =
    "Safety Hub - Disruptive notification revocation";
inline constexpr char kSafetyHubDisruptiveNotificationRevocationDescription[] =
    "Enables autorevoking notifications with high volume and low site "
    "engagement score";

inline constexpr char kSafetyHubUnusedPermissionRevocationForAllSurfacesName[] =
    "Safety Hub -  unused permission revocation from all surfaces";
inline constexpr char
    kSafetyHubUnusedPermissionRevocationForAllSurfacesDescription[] =
        "Enables autorevoking of unused permissions granted from all UI "
        "surfaces.";

inline constexpr char kSafetyHubLocalPasswordsModuleName[] =
    "Enables the local passwords module in Safety Hub";
inline constexpr char kSafetyHubLocalPasswordsModuleDescription[] =
    "Enables showing the local passwords module in Safety Hub.";

inline constexpr char kSafetyHubUnifiedPasswordsModuleName[] =
    "Enables the unified passwords module in Safety Hub";
inline constexpr char kSafetyHubUnifiedPasswordsModuleDescription[] =
    "Enables the unified passwords module in Safety Hub, which includes "
    "account and local passwords.";

inline constexpr char kSafetyHubWeakAndReusedPasswordsName[] =
    "Enables Weak and Reused passwords in Safety Hub";
inline constexpr char kSafetyHubWeakAndReusedPasswordsDescription[] =
    "Enables showing weak and reused passwords in the password module of "
    "Safety Hub.";

inline constexpr char kSameAppWindowCycleName[] =
    "Cros Labs: Same App Window Cycling";
inline constexpr char kSameAppWindowCycleDescription[] =
    "Use Alt+` to cycle through the windows of the active application.";

inline constexpr char kTestThirdPartyCookiePhaseoutName[] =
    "Test Third Party Cookie Phaseout";
inline constexpr char kTestThirdPartyCookiePhaseoutDescription[] =
    "Enable to test third-party cookie phaseout. "
    "Learn more: https://goo.gle/3pcd-flags";

inline constexpr char kAppBrowserUseNewLayoutId[] =
    "app-browser-use-new-layout";
inline constexpr char kAppBrowserUseNewLayoutName[] =
    "App Browser Use New Layout";
inline constexpr char kAppBrowserUseNewLayoutDescription[] =
    "Use the new App Browser Layout. Visually nothing should change.";

inline constexpr char kPopupBrowserUseNewLayoutId[] =
    "popup-browser-use-new-layout";
inline constexpr char kPopupBrowserUseNewLayoutName[] =
    "Popup Browser Use New Layout";
inline constexpr char kPopupBrowserUseNewLayoutDescription[] =
    "Use the new Popup Browser Layout. Visually nothing should change.";

inline constexpr char kTabbedBrowserUseNewLayoutId[] =
    "tabbed-browser-use-new-layout";
inline constexpr char kTabbedBrowserUseNewLayoutName[] =
    "Tabbed Browser Use New Layout";
inline constexpr char kTabbedBrowserUseNewLayoutDescription[] =
    "Use the new Tabbed Browser Layout. Visually nothing should change.";

inline constexpr char kTabstripComboButtonFlagId[] = "tabstrip-combo-button";
inline constexpr char kTabstripComboButtonName[] = "Tabstrip Combo Button";
inline constexpr char kTabstripComboButtonDescription[] =
    "Combines tab search and the new tab button into a single combo button. "
    "Might require tab search toolbar flag to be disabled to take effect in "
    "specific regions.";

inline constexpr char kLaunchedTabSearchToolbarName[] =
    "Tab Search Toolbar Button";
inline constexpr char kLaunchedTabSearchToolbarDescription[] =
    "Enables tab search button to be in toolbar area. "
    "Might require enabling the tab strip combo button configuration to also "
    "match to toolbar in specific regions.";

inline constexpr char kSidePanelRelativeAlignmentName[] =
    "Side Panel Relative Alignment";
inline constexpr char kSidePanelRelativeAlignmentDescription[] =
    "Set the relative alignment between the toolbar height side panel and the "
    "content height side panel";

inline constexpr char kTabGroupsFocusingName[] = "Tab Groups Focusing";
inline constexpr char kTabGroupsFocusingDescription[] =
    "When a tab group is focused, the tabstrip constrains visiblity to the "
    "tabs in that group.";

inline constexpr char kTabStorageSqlitePrototypeName[] =
    "Tab Storage SQLite Prototype";
inline constexpr char kTabStorageSqlitePrototypeDescription[] =
    "Enables a prototype for using SQLite for tab storage.";

inline constexpr char kDynamicSearchUpdateAnimationName[] =
    "Dynamic Search Result Update Animation";
inline constexpr char kDynamicSearchUpdateAnimationDescription[] =
    "Dynamically adjust the search result update animation when those update "
    "animations are preempted. Shortened animation durations configurable "
    "(unit: milliseconds).";

inline constexpr char kSecurePaymentConfirmationAvailabilityAPIName[] =
    "securePaymentConfirmationAvailability API";
inline constexpr char kSecurePaymentConfirmationAvailabilityAPIDescription[] =
    "Enables the PaymentRequest.securePaymentConfirmationAvailability web API, "
    "which allows for more ergonomic feature detection of Secure Payment "
    "Confirmation";

inline constexpr char kSecurePaymentConfirmationBrowserBoundKeysName[] =
    "Secure Payment Confirmation Browser Bound Key";
inline constexpr char kSecurePaymentConfirmationBrowserBoundKeysDescription[] =
    "This flag enables an additional browser-bound signature in secure payment "
    "confirmation in PaymentRequest and for WebAuthn payment credentials.";

inline constexpr char kSecurePaymentConfirmationFallbackName[] =
    "Secure Payment Confirmation Fallback UX";
inline constexpr char kSecurePaymentConfirmationFallbackDescription[] =
    "Enable the fallback experience in Secure Payment Confirmation, where a "
    "transaction dialog-like UX is shown even if no credentials match.";

inline constexpr char kSecurePaymentConfirmationUxRefreshName[] =
    "Secure Payment Confirmation UX Refresh";
inline constexpr char kSecurePaymentConfirmationUxRefreshDescription[] =
    "This flag enables new UX in the secure payment confirmation dialog "
    "including new output states, payment instrument details and payment "
    "entities logos.";

inline constexpr char kSegmentationSurveyPageName[] =
    "Segmentation survey internals page and model";
inline constexpr char kSegmentationSurveyPageDescription[] =
    "Enable internals page for survey and fetching model";

inline constexpr char kServiceWorkerAutoPreloadName[] =
    "ServiceWorkerAutoPreload";
inline constexpr char kServiceWorkerAutoPreloadDescription[] =
    "Dispatches a preload request for navigation before starting the service "
    "worker. See "
    "https://github.com/explainers-by-googlers/service-worker-auto-preload";

inline constexpr char kServiceWorkerSyntheticResponseName[] =
    "ServiceWorkerSyntheticResponse";
inline constexpr char kServiceWorkerSyntheticResponseDescription[] =
    "Enable service worker synthetic response feature.";

inline constexpr char kSharingDesktopScreenshotsName[] = "Desktop Screenshots";
inline constexpr char kSharingDesktopScreenshotsDescription[] =
    "Enables taking"
    " screenshots from the desktop sharing hub.";

inline constexpr char kShowAutofillSignaturesName[] =
    "Show autofill signatures.";
inline constexpr char kShowAutofillSignaturesDescription[] =
    "Annotates web forms with Autofill signatures as HTML attributes. Also "
    "marks password fields suitable for password generation.";

inline constexpr char kShowAutofillTypePredictionsName[] =
    "Show Autofill predictions";
inline constexpr char kShowAutofillTypePredictionsDescription[] =
    "Annotates web forms with Autofill field type predictions as placeholder "
    "text.";

inline constexpr char kShowOverdrawFeedbackName[] = "Show overdraw feedback";
inline constexpr char kShowOverdrawFeedbackDescription[] =
    "Visualize overdraw by color-coding elements based on if they have other "
    "elements drawn underneath.";

inline constexpr char kAccessibilityOnScreenModeName[] =
    "On-Screen Only Accessibility Nodes";
inline constexpr char kAccessibilityOnScreenModeDescription[] =
    "Enable experimental accessibility mode to improve performance which "
    "allows assistive technologies to access only accessibility nodes that are "
    "on-screen";

inline constexpr char kFeedbackIncludeVariationsName[] =
    "Feedback include variations";
inline constexpr char kFeedbackIncludeVariationsDescription[] =
    "In Chrome feedback report, include commandline variations.";

inline constexpr char kSideBySideName[] = "Split View";
inline constexpr char kSideBySideDescription[] =
    "Allows users to view two tabs "
    "simultaneously in a split view.";

inline constexpr char kSideBySideSessionRestoreName[] =
    "Split View Session Restore";
inline constexpr char kSideBySideSessionRestoreDescription[] =
    "Allows users to restore tabs in split view "
    "from previous session when the browser restarts.";

inline constexpr char kOpenDraggedLinksSameTabName[] =
    "Open Dragged Links in the Same Tab";
inline constexpr char kOpenDraggedLinksSameTabDescription[] =
    "Allows users to drag a single link to a tab to open in that tab.";

inline constexpr char kDefaultSiteInstanceGroupsName[] =
    "Default SiteInstanceGroups";
inline constexpr char kDefaultSiteInstanceGroupsDescription[] =
    "Put sites that don't need isolation in their own SiteInstance in a default"
    "SiteInstanceGroup (per BrowsingContextGroup) instead of in a default "
    "SiteInstance.";

inline constexpr char kPwaNavigationCapturingName[] =
    "Desktop PWA Link Capturing";
inline constexpr char kPwaNavigationCapturingDescription[] =
    "Enables opening links from Chrome in an installed PWA. Currently under "
    "reimplementation.";

inline constexpr char kIsolateOriginsName[] = "Isolate additional origins";
inline constexpr char kIsolateOriginsDescription[] =
    "Requires dedicated processes for an additional set of origins, "
    "specified as a comma-separated list.";

inline constexpr char kSiteIsolationOptOutName[] = "Disable site isolation";
inline constexpr char kSiteIsolationOptOutDescription[] =
    "Disables site isolation "
    "(SitePerProcess, IsolateOrigins, etc). Intended for diagnosing bugs that "
    "may be due to out-of-process iframes. Opt-out has no effect if site "
    "isolation is force-enabled using a command line switch or using an "
    "enterprise policy. "
    "Caution: this disables important mitigations for the Spectre CPU "
    "vulnerability affecting most computers.";
inline constexpr char kSiteIsolationOptOutChoiceDefault[] = "Default";
inline constexpr char kSiteIsolationOptOutChoiceOptOut[] =
    "Disabled (not recommended)";

inline constexpr char kSkiaGraphiteName[] = "Skia Graphite";
inline constexpr char kSkiaGraphiteDescription[] =
    "Enable Skia Graphite. This will use the Dawn backend by default, but can "
    "be overridden with command line flags for testing on non-official "
    "developer builds. See --skia-graphite-backend flag in gpu_switches.h.";

inline constexpr char kSkiaGraphitePrecompilationName[] =
    "Skia Graphite Precompilation";
inline constexpr char kSkiaGraphitePrecompilationDescription[] =
    "Enable Skia Graphite Precompilation. This is only relevant when Graphite "
    "is enabled "
    "but can then be overridden via the "
    "--enable-skia-graphite-precompilation and "
    "--disable-skia-graphite-precompilation "
    "command line flags";

inline constexpr char kProfileCreationDeclineSigninCTAExperimentName[] =
    "Enable CTA experiment for sign-in level up";
inline constexpr char kProfileCreationDeclineSigninCTAExperimentDescription[] =
    "As part of the Sign In Level Up experiment, changes the decline "
    "sign in CTA string in profile creation entry points";

inline constexpr char
    kProfileCreationFrictionReductionExperimentPrefillNameRequirementName[] =
        "Enable prefill name requirement for profile creation for friction "
        "reduction experiment";
inline constexpr char
    kProfileCreationFrictionReductionExperimentPrefillNameRequirementDescription
        [] = "As part of the profile creation friction reduction experiment, "
             "prefills the name requirement in profile customization bubble";

inline constexpr char
    kProfileCreationFrictionReductionExperimentRemoveSigninStepName[] =
        "Remove sign-in step from profile creation for friction reduction "
        "experiment";
inline constexpr char
    kProfileCreationFrictionReductionExperimentRemoveSigninStepDescription[] =
        "As part of the profile creation friction reduction experiment, "
        "removes the sign-in step";

inline constexpr char
    kProfileCreationFrictionReductionExperimentSkipCustomizeProfileName[] =
        "Skip customize profile step for friction reduction experiment";
inline constexpr char
    kProfileCreationFrictionReductionExperimentSkipCustomizeProfileDescription
        [] = "As part of the profile creation friction reduction experiment, "
             "skips the customize profile bubble";

inline constexpr char kProfilePickerTextVariationsName[] =
    "Profile Picker Text Variations";
inline constexpr char kProfilePickerTextVariationsDescription[] =
    "As part of the Profile experiments, enables variations of the profile "
    "picker text.";

inline constexpr char kShowProfilePickerToAllUsersExperimentName[] =
    "Show profile picker to all users";
inline constexpr char kShowProfilePickerToAllUsersExperimentDescription[] =
    "As part of the Growth experiments, show the profile picker to users who "
    "only have one profile";

inline constexpr char kOpenAllProfilesFromProfilePickerExperimentName[] =
    "Add button to open all profiles from profile picker";
inline constexpr char kOpenAllProfilesFromProfilePickerExperimentDescription[] =
    "As part of the Growth experiments, add a button to open all profiles from "
    "the profile picker.";

inline constexpr char kBackdropFilterMirrorEdgeName[] =
    "Backdrop Filter Mirror Edge";
inline constexpr char kBackdropFilterMirrorEdgeDescription[] =
    "When sampling being the backdrop edge for backdrop-filter, samples "
    "beyond the edge are mirrored back into the backdrop rather than "
    "duplicating the pixels at the edge.";

inline constexpr char kSmoothScrollingName[] = "Smooth Scrolling";
inline constexpr char kSmoothScrollingDescription[] =
    "Animate smoothly when scrolling page content.";

inline constexpr char kStrictOriginIsolationName[] = "Strict-Origin-Isolation";
inline constexpr char kStrictOriginIsolationDescription[] =
    "Experimental security mode that strengthens the site isolation policy. "
    "Controls whether site isolation should use origins instead of scheme and "
    "eTLD+1.";

inline constexpr char kSupportToolScreenshot[] = "Support Tool Screenshot";
inline constexpr char kSupportToolScreenshotDescription[] =
    "Enables the Support Tool to capture and include a screenshot in the "
    "exported packet.";

inline constexpr char kSyncAutofillWalletCredentialDataName[] =
    "Sync Autofill Wallet Credential Data";
inline constexpr char kSyncAutofillWalletCredentialDataDescription[] =
    "When enabled, allows syncing of the autofill wallet credential data type.";

inline constexpr char kSyncSandboxName[] = "Use Chrome Sync sandbox";
inline constexpr char kSyncSandboxDescription[] =
    "Connects to the testing server for Chrome Sync.";

inline constexpr char kSystemKeyboardLockName[] =
    "Experimental system keyboard lock";
inline constexpr char kSystemKeyboardLockDescription[] =
    "Enables websites to use the keyboard.lock() API to intercept system "
    "keyboard shortcuts and have the events routed directly to the website "
    "when in fullscreen mode.";

inline constexpr char kTabArchivalDragDropAndroidName[] =
    "Drag and Drop to Archive Tabs";
inline constexpr char kTabArchivalDragDropAndroidDescription[] =
    "Enables drag-and-drop tabs in the tab switcher to archive tabs.";

inline constexpr char kTabCollectionAndroidName[] = "Tab Collection Android";
inline constexpr char kTabCollectionAndroidDescription[] =
    "A data layer refactoring to use tab collections rather than a list to "
    "store tabs on Chrome Android.";

inline constexpr char kTabFreezingUsesDiscardName[] =
    "Tab Freezing Uses Discard";
inline constexpr char kTabFreezingUsesDiscardDescription[] =
    "When enabled, tab freezing will use discarding instead of freezing.";

inline constexpr char kTabGroupEntryPointsAndroidName[] =
    "Tab Group Entry Points";
inline constexpr char kTabGroupEntryPointsAndroidDescription[] =
    "Enables additional entry points for creating tab groups";

inline constexpr char kTabGroupParityBottomSheetAndroidName[] =
    "Tab Group Parity Bottom Sheet";
inline constexpr char kTabGroupParityBottomSheetAndroidDescription[] =
    "Enables adding Tabs to Tab Groups via the Tab Group Parity Bottom Sheet";

inline constexpr char kTabGroupAndroidVisualDataCleanupName[] =
    "Tab Group Visual Data Cleanup";
inline constexpr char kTabGroupAndroidVisualDataCleanupDescription[] =
    "Cleanup tab group visual data that is no longer associated with an "
    "existing tab group.";

inline constexpr char kTabModelInitFixesName[] = "Tab Model Init Fixes";
inline constexpr char kTabModelInitFixesDescription[] =
    "A grab bag of simple and miscellaneous improvements for tab model "
    "initialization on Android. Should speed up initialization, as well as "
    "have better handling for app menu tab model operations during "
    "initialization.";

inline constexpr char kTabSwitcherDragDropName[] =
    "Tab Drag and Drop via Tab Switcher";
inline constexpr char kTabSwitcherDragDropDescription[] =
    "Enables long-pressing on tab switcher tab to start drag-and-drop. Users "
    "can drag the tab and drop it into another instance of Chrome or to create "
    "new instance of Chrome.";

inline constexpr char kTabSwitcherGroupSuggestionsAndroidName[] =
    "Tab Switcher Group Suggestions";
inline constexpr char kTabSwitcherGroupSuggestionsAndroidDescription[] =
    "Enables group suggestions in the tab switcher.";

inline constexpr char kTabSwitcherGroupSuggestionsTestModeAndroidName[] =
    "Tab Switcher Group Suggestions Test Mode";
inline constexpr char kTabSwitcherGroupSuggestionsTestModeAndroidDescription[] =
    "Helper flag for testing that shows group suggestions for the last 3 tabs "
    "in the tab switcher (if present).";

inline constexpr char kChromeNativeUrlOverridingName[] =
    "Chrome Native Url Overriding";
inline constexpr char kChromeNativeUrlOverridingDescription[] =
    "Allows for URL overriding for chrome-native:// pages";

inline constexpr char kTabletTabStripAnimationName[] =
    "Tablet Tab Strip Animation";
inline constexpr char kTabletTabStripAnimationDescription[] =
    "Enables new tablet tab strip animations.";

inline constexpr char kDataSharingDebugLogsName[] =
    "Enable data sharing debug logs";
inline constexpr char kDataSharingDebugLogsDescription[] =
    "Enables the data sharing infrastructure to log and save debug messages "
    "that can be shown in the internals page.";

inline constexpr char kTabGroupMenuImprovementsName[] =
    "Add context menu when left-clicking a tab group";
inline constexpr char kTabGroupMenuImprovementsDescription[] =
    "When clicking a tab group in the bookmarks bar, the left click will be "
    "given a context menu, similar to the one that appears when right clicking "
    "the tab group.";

inline constexpr char kTabGroupMenuMoreEntryPointsName[] =
    "Make options menus to include more tab group actions";
inline constexpr char kTabGroupMenuMoreEntryPointsDescription[] =
    "Add options to menus to facilitate tab group creation and interaction";

inline constexpr char kTabSearchPositionSettingId[] =
    "tab-search-position-setting";
inline constexpr char kTabSearchPositionSettingName[] =
    "Tab Search Position Setting";
inline constexpr char kTabSearchPositionSettingDescription[] =
    "Whether to show the tab search position options in the settings page.";

inline constexpr char kTearOffWebAppAppTabOpensWebAppWindowName[] =
    "Tear Off Web App Tab";
inline constexpr char kTearOffWebAppAppTabOpensWebAppWindowDescription[] =
    "Open Web App window when tearing off a tab that's displaying a url "
    "handled by an installed Web App.";

inline constexpr char kTextSafetyClassifierName[] = "Text Safety Classifier";
inline constexpr char kTextSafetyClassifierDescription[] =
    "Enables text safety classifier for on-device models";

inline constexpr char kAutofillThirdPartyModeContentProviderName[] =
    "Autofill Third Party Mode Content Provider";
inline constexpr char kAutofillThirdPartyModeContentProviderDescription[] =
    "Enables querying the third party autofill mode state from the Chrome app.";

inline constexpr char kThreeButtonPasswordSaveDialogName[] =
    "Three Button Password Save Dialog";
inline constexpr char kThreeButtonPasswordSaveDialogDescription[] =
    "Provides a 'not now' button alongside the 'never' button on the save "
    "password dialog.";

inline constexpr char kThrottleMainTo60HzName[] =
    "throttle-main-thread-to-60hz";
inline constexpr char kThrottleMainTo60HzDescription[] =
    "Throttle main thread updates to 60fps, even when VSync rate is higher.";

inline constexpr char kTintCompositedContentName[] = "Tint composited content";
inline constexpr char kTintCompositedContentDescription[] =
    "Tint contents composited using Viz with a shade of red to help debug and "
    "study overlay support.";

inline constexpr char kTLSTrustAnchorIDsName[] = "TLS Trust Anchor IDs";
inline constexpr char kTLSTrustAnchorIDsDescription[] =
    "This option configures TLS Trust Anchor IDs, allowing compatible servers "
    "to select between available certificates issued by different CAs.";

inline constexpr char kTopControlsRefactorName[] = "Top Controls Refactor";
inline constexpr char kTopControlsRefactorDescription[] =
    "Enables the alternative code path in Android for the top controls layout "
    "control.";

inline constexpr char kTopControlsRefactorV2Name[] = "Top Controls Refactor V2";
inline constexpr char kTopControlsRefactorV2Description[] =
    "Enables the alternative code path in Android for the top controls layout "
    "control, v2, including y-offsets.";

inline constexpr char kToolbarPhoneAnimationRefactorName[] =
    "Toolbar Phone Animation Refactor";
inline constexpr char kToolbarPhoneAnimationRefactorDescription[] =
    "Enables the refactored animation code path in Android for the toolbar "
    "phone class.";

inline constexpr char kRefactorMinWidthContextOverrideName[] =
    "Refactor the min width context override";
inline constexpr char kRefactorMinWidthContextOverrideDescription[] =
    "Refactor the min width context override from individual activities to "
    "parent "
    "ChromeBaseAppCompatActivity";

inline constexpr char kToolbarStaleCaptureBugFixName[] =
    "Fix for stale toolbar captures";
inline constexpr char kToolbarStaleCaptureBugFixDescription[] =
    "When enabled, this flag fixes a bug where the toolbar capture can become "
    "stale.";

inline constexpr char kToolbarTabletResizeRefactorName[] =
    "Toolbar Tablet Resize Refactor";
inline constexpr char kToolbarTabletResizeRefactorDescription[] =
    "Enables the refactored logic in Android for the toolbar tablet class for "
    "new animations and what buttons to show on window resizing.";

inline constexpr char kTouchToSearchCalloutName[] = "Touch To Search Callout";
inline constexpr char kTouchToSearchCalloutDescription[] =
    "Enables a callout in the touch to search panel.";

inline constexpr char kTopChromeTouchUiName[] = "Touch UI Layout";
inline constexpr char kTopChromeTouchUiDescription[] =
    "Enables touch UI layout in the browser's top chrome.";

inline constexpr char kTouchDragDropName[] = "Touch initiated drag and drop";
inline constexpr char kTouchDragDropDescription[] =
    "Touch drag and drop can be initiated through long press on a draggable "
    "element.";

inline constexpr char kTouchSelectionStrategyName[] =
    "Touch text selection strategy";
inline constexpr char kTouchSelectionStrategyDescription[] =
    "Controls how text selection granularity changes when touch text selection "
    "handles are dragged. Non-default behavior is experimental.";
inline constexpr char kTouchSelectionStrategyCharacter[] = "Character";
inline constexpr char kTouchSelectionStrategyDirection[] = "Direction";

inline constexpr char kTouchTextEditingRedesignName[] =
    "Touch Text Editing Redesign";
inline constexpr char kTouchTextEditingRedesignDescription[] =
    "Enables new touch text editing features.";

inline constexpr char kTranslationAPIName[] = "Experimental translation API";
inline constexpr char kTranslationAPIDescription[] =
    "Enables the on-device language translation API. "
    "See https://github.com/WICG/translation-api/blob/main/README.md";

inline constexpr char kTranslationAPIStreamingBySentenceName[] =
    "Translation API streaming split by sentence";
inline constexpr char kTranslationAPIStreamingBySentenceDescription[] =
    "Enables sentence-split streaming for on-device translation API.";

inline constexpr char kAvatarButtonSyncPromoName[] = "Avatar Sync Promo";
inline constexpr char kAvatarButtonSyncPromoDescription[] =
    "Enables the avatar button sync promo for eligible users. Only available "
    "on Windows.";

inline constexpr char kTreatInsecureOriginAsSecureName[] =
    "Insecure origins treated as secure";
inline constexpr char kTreatInsecureOriginAsSecureDescription[] =
    "Treat given (insecure) origins as secure origins. Multiple origins can be "
    "supplied as a comma-separated list. Origins must have their protocol "
    "specified e.g. \"http://example.com\". For the definition of secure "
    "contexts, see https://w3c.github.io/webappsec-secure-contexts/";

inline constexpr char kUnsafeWebGPUName[] = "Unsafe WebGPU Support";
inline constexpr char kUnsafeWebGPUDescription[] =
    "Convenience flag for WebGPU development. Enables best-effort WebGPU "
    "support on unsupported configurations and more! Note that this flag could "
    "expose security issues to websites so only use it for your own "
    "development.";

inline constexpr char kForceHighPerformanceGPUName[] =
    "Force High Performance GPU";
inline constexpr char kForceHighPerformanceGPUDescription[] =
    "Forces use of high performance GPU if available. Warning: this flag may "
    "increase power consumption leading to shorter battery time.";

inline constexpr char kUiaProviderName[] = "UI Automation";
inline constexpr char kUiaProviderDescription[] =
    "Enables native support of the UI Automation provider.";

inline constexpr char kUiPartialSwapName[] = "Partial swap";
inline constexpr char kUiPartialSwapDescription[] =
    "Sets partial swap behavior.";

inline constexpr char kTPCPhaseOutFacilitatedTestingName[] =
    "Third-party Cookie Phase Out Facilitated Testing";
inline constexpr char kTPCPhaseOutFacilitatedTestingDescription[] =
    "Enables third-party cookie phase out for facilitated testing described in "
    "https://developer.chrome.com/en/docs/privacy-sandbox/chrome-testing/";

inline constexpr char kTpcdHeuristicsGrantsName[] =
    "Third-party Cookie Grants Heuristics Testing";
inline constexpr char kTpcdHeuristicsGrantsDescription[] =
    "Enables temporary storage access grants for certain user behavior "
    "heuristics. See "
    "https://github.com/amaliev/3pcd-exemption-heuristics/blob/main/"
    "explainer.md for more details.";

inline constexpr char kTpcdMetadataGrantsName[] =
    "Third-Party Cookie Deprecation Metadata Grants for Testing";
inline constexpr char kTpcdMetadataGrantsDescription[] =
    "Provides a control for enabling/disabling Third-Party Cookie Deprecation "
    "Metadata Grants (WRT its default state) for testing.";

inline constexpr char kTrackingProtection3pcdName[] =
    "Tracking Protection for 3PCD";
inline constexpr char kTrackingProtection3pcdDescription[] =
    "Enables the tracking protection UI + prefs that will be used for the 3PCD "
    "1%.";

inline constexpr char kUndoMigrationOfSyncingUserToSignedInName[] =
    "Undo the migration of syncing users to signed-in state";
inline constexpr char kUndoMigrationOfSyncingUserToSignedInDescription[] =
    "When enabled, reverts the migration of syncing users who were previously "
    "migrated to the signed-in, non-syncing state.";

inline constexpr char kUseSearchClickForRightClickName[] =
    "Use Search+Click for right click";
inline constexpr char kUseSearchClickForRightClickDescription[] =
    "When enabled search+click will be remapped to right click, allowing "
    "webpages and apps to consume alt+click. When disabled the legacy "
    "behavior of remapping alt+click to right click will remain unchanged.";

inline constexpr char kNotificationOneTapUnsubscribeOnDesktopName[] =
    "Notification one-tap unsubscribe on Desktop";
inline constexpr char kNotificationOneTapUnsubscribeOnDesktopDescription[] =
    "Enables an experimental UX  on Desktop that replaces the [Site settings]"
    "button on web push notifications with an [Unsubscribe] button.";

inline constexpr char kUseAndroidBufferedInputDispatchName[] =
    "Use Android buffered input dispatch";
inline constexpr char kUseAndroidBufferedInputDispatchDescription[] =
    "Enables using Android's buffered input dispatch, which will generally "
    "deliver batched resampled input events to Chrome once per VSync.";

inline constexpr char kVcBackgroundReplaceName[] =
    "Enable vc background replacement";
inline constexpr char kVcBackgroundReplaceDescription[] =
    "Enables background replacement feature for video conferencing on "
    "Chromebooks. THIS WILL OVERRIDE BACKGROUND BLUR.";

inline constexpr char kVcRelightingInferenceBackendName[] =
    "Select relighting backend for video conferencing";
inline constexpr char kVcRelightingInferenceBackendDescription[] =
    "Select relighting backend to be used for running model inference during "
    "video conferencing, which may offload work from GPU.";

inline constexpr char kVcRetouchInferenceBackendName[] =
    "Select retouch backend for video conferencing";
inline constexpr char kVcRetouchInferenceBackendDescription[] =
    "Select retouch backend to be used for running model inference during "
    "video conferencing, which may offload work from GPU.";

inline constexpr char kVcSegmentationInferenceBackendName[] =
    "Select segmentation backend for video conferencing";
inline constexpr char kVcSegmentationInferenceBackendDescription[] =
    "Select segmentation backend to be used for running model inference "
    "during video conferencing, which may offload work from GPU.";

inline constexpr char kVcStudioLookName[] =
    "Enables Studio Look for video conferencing";
inline constexpr char kVcStudioLookDescription[] =
    "Enables Studio Look and VC settings UI, which contains settings for Studio"
    "Look.";

inline constexpr char kVcSegmentationModelName[] =
    "Use a different segmentation model";
inline constexpr char kVcSegmentationModelDescription[] =
    "Allows a different segmentation model to be used for blur and relighting, "
    "which may reduce the workload on the GPU.";

inline constexpr char kVcTrayMicIndicatorName[] =
    "Adds a mic indicator in VC tray";
inline constexpr char kVcTrayMicIndicatorDescription[] =
    "Displays a pulsing mic indicator that indicates how loud the audio is "
    "captured by the microphone, after some effects like noise cancellation "
    "is applied.";

inline constexpr char kVcTrayTitleHeaderName[] =
    "Adds a sidetone toggle in VC tray";
inline constexpr char kVcTrayTitleHeaderDescription[] =
    "Displays a sidetone toggle in VC Tray Title header";

inline constexpr char kVcLightIntensityName[] = "VC relighting intensity";
inline constexpr char kVcLightIntensityDescription[] =
    "Allows different light intensity to be used for relighting.";

inline constexpr char kVcWebApiName[] = "VC web API";
inline constexpr char kVcWebApiDescription[] =
    "Allows web API support for video conferencing on Chromebooks.";

inline constexpr char kVerifyQWACsName[] = "Verify QWACs";
inline constexpr char kVerifyQWACsDescription[] =
    "Enables verification of qualified certificates for website authentication "
    "as described in ETSI TS 119 411-5 V2.1.1 (2025-02).";

inline constexpr char kVidsAppPreinstallName[] = "Vids app preinstall";
inline constexpr char kVidsAppPreinstallDescription[] =
    "Preinstalls the Vids app on ChromeOS.";

inline constexpr char kV8VmFutureName[] = "Future V8 VM features";
inline constexpr char kV8VmFutureDescription[] =
    "This enables upcoming and experimental V8 VM features. "
    "This flag does not enable experimental JavaScript features.";

inline constexpr char kTaiyakiName[] = "Taiyaki";
inline constexpr char kTaiyakiDescription[] = "Enables Taiyaki.";

inline constexpr char kGlobalVaapiLockName[] =
    "Global lock on the VA-API wrapper.";
inline constexpr char kGlobalVaapiLockDescription[] =
    "Enable or disable the global VA-API lock for platforms and paths that "
    "support controlling this.";

inline constexpr char kWalletServiceUseSandboxName[] =
    "Use Google Payments sandbox servers";
inline constexpr char kWalletServiceUseSandboxDescription[] =
    "For developers: use the sandbox service for Google Payments API calls.";

inline constexpr char kWallpaperFastRefreshName[] =
    "Enable shortened wallpaper daily refresh interval for manual testing";
inline constexpr char kWallpaperFastRefreshDescription[] =
    "Allows developers to see a new wallpaper once every ten seconds rather "
    "than once per day when using the daily refresh feature.";

inline constexpr char kWallpaperGooglePhotosSharedAlbumsName[] =
    "Enable Google Photos shared albums for wallpaper";
inline constexpr char kWallpaperGooglePhotosSharedAlbumsDescription[] =
    "Allow users to set shared Google Photos albums as the source for their "
    "wallpaper.";

inline constexpr char kWallpaperSearchSettingsVisibilityName[] =
    "Wallpaper Search Settings Visibility";
inline constexpr char kWallpaperSearchSettingsVisibilityDescription[] =
    "Shows wallpaper search settings in settings UI.";

inline constexpr char kWebAppInstallationApiName[] = "Web App Installation API";
inline constexpr char kWebAppInstallationApiDescription[] =
    "Enables the Web App Installation API which allows web apps to be "
    "installed programmatically using navigator.install().";

inline constexpr char kWebAppMigratePreinstalledChatName[] =
    "Migrate preinstalled Chat app";
inline constexpr char kWebAppMigratePreinstalledChatDescription[] =
    "When enabled, the preinstalled Chat web app will be migrated from its old "
    "URL to a new URL. This migration only applies to users who have not "
    "manually installed the Chat app.";

inline constexpr char kWebAuthnUsePasskeyFromAnotherDeviceInContextMenuName[] =
    "Use passkey from another device in the context menu";
inline constexpr char
    kWebAuthnUsePasskeyFromAnotherDeviceInContextMenuDescription[] =
        "Hides the \"Use a passkey\" entry from the autofill popup for "
        "conditional "
        "WebAuthn requests. Moves the entry point to the context menu.";

inline constexpr char kAutofillReintroduceHybridPasskeyDropdownItemName[] =
    "Reintroduce hybrid passkey entry point";
inline constexpr char
    kAutofillReintroduceHybridPasskeyDropdownItemDescription[] =
        "Reintroduces the hybrid passkey entry point to the Autofill dropdown "
        "menu.";

inline constexpr char kWebAuthnPasskeyUpgradeName[] =
    "Enable automatic passkey upgrades in Google Password Manager";
inline constexpr char kWebAuthnPasskeyUpgradeDescription[] =
    "Enable the WebAuthn Conditional Create feature and let websites "
    "automatically create passkeys in GPM if there is a matching password "
    "credential for the same user.";

inline constexpr char kWebAuthnImmediateGetName[] =
    "Enable immediate mediation for WebAuthn get requests";
inline constexpr char kWebAuthnImmediateGetDescription[] =
    "Enables immediate mediation for WebAuthn and passwords for a "
    "navigator.credentials.get() request. This will return a NotAllowedError "
    "if there are no credentials for a given get request. The request can also "
    "request passwords.";

inline constexpr char kWebBluetoothName[] = "Web Bluetooth";
inline constexpr char kWebBluetoothDescription[] =
    "Enables the Web Bluetooth API on platforms without official support";

inline constexpr char kWebBluetoothNewPermissionsBackendName[] =
    "Use the new permissions backend for Web Bluetooth";
inline constexpr char kWebBluetoothNewPermissionsBackendDescription[] =
    "Enables the new permissions backend for Web Bluetooth. This will enable "
    "persistent storage of device permissions and Web Bluetooth features such "
    "as BluetoothDevice.watchAdvertisements() and Bluetooth.getDevices()";

inline constexpr char kWebiumName[] = "Webium";
inline constexpr char kWebiumDescription[] = "Webium Prototype Browser.";

inline constexpr char kWebOtpBackendName[] = "Web OTP";
inline constexpr char kWebOtpBackendDescription[] =
    "Enables Web OTP API that uses the specified backend.";
inline constexpr char kWebOtpBackendSmsVerification[] = "Code Browser API";
inline constexpr char kWebOtpBackendUserConsent[] = "User Consent API";
inline constexpr char kWebOtpBackendAuto[] = "Automatically select the backend";

inline constexpr char kWebglDeveloperExtensionsName[] =
    "WebGL Developer Extensions";
inline constexpr char kWebglDeveloperExtensionsDescription[] =
    "Enabling this option allows web applications to access WebGL extensions "
    "intended only for use during development time.";

inline constexpr char kWebglDraftExtensionsName[] = "WebGL Draft Extensions";
inline constexpr char kWebglDraftExtensionsDescription[] =
    "Enabling this option allows web applications to access the WebGL "
    "extensions that are still in draft status.";

inline constexpr char kWebGpuDeveloperFeaturesName[] =
    "WebGPU Developer Features";
inline constexpr char kWebGpuDeveloperFeaturesDescription[] =
    "Enables web applications to access WebGPU features intended only for use "
    "during development.";

inline constexpr char kWebPaymentsExperimentalFeaturesName[] =
    "Experimental Web Payments API features";
inline constexpr char kWebPaymentsExperimentalFeaturesDescription[] =
    "Enable experimental Web Payments API features";

inline constexpr char kAppStoreBillingDebugName[] =
    "Web Payments App Store Billing Debug Mode";
inline constexpr char kAppStoreBillingDebugDescription[] =
    "App-store purchases (e.g., Google Play Store) within a TWA can be "
    "requested using the Payment Request API. This flag removes the "
    "restriction that the TWA has to be installed from the app-store.";

inline constexpr char kWebrtcHideLocalIpsWithMdnsName[] =
    "Anonymize local IPs exposed by WebRTC.";
inline constexpr char kWebrtcHideLocalIpsWithMdnsDecription[] =
    "Conceal local IP addresses with mDNS hostnames.";

inline constexpr char kWebRtcAllowInputVolumeAdjustmentName[] =
    "Allow WebRTC to adjust the input volume.";
inline constexpr char kWebRtcAllowInputVolumeAdjustmentDescription[] =
    "Allow the Audio Processing Module in WebRTC to adjust the input volume "
    "during a real-time call. Disable if microphone muting or clipping issues "
    "are observed when the browser is running and used for a real-time call. "
    "This flag is experimental and may be removed at any time.";

inline constexpr char kWebRtcApmDownmixCaptureAudioMethodName[] =
    "WebRTC downmix capture audio method.";
inline constexpr char kWebRtcApmDownmixCaptureAudioMethodDescription[] =
    "Override the method that the Audio Processing Module in WebRTC uses to "
    "downmix the captured audio to mono (when needed) during a real-time call. "
    "This flag is experimental and may be removed at any time.";

inline constexpr char kWebrtcHwDecodingName[] =
    "WebRTC hardware video decoding";
inline constexpr char kWebrtcHwDecodingDescription[] =
    "Support in WebRTC for decoding video streams using platform hardware.";

inline constexpr char kWebrtcHwEncodingName[] =
    "WebRTC hardware video encoding";
inline constexpr char kWebrtcHwEncodingDescription[] =
    "Support in WebRTC for encoding video streams using platform hardware.";

inline constexpr char kWebRtcPqcForDtlsName[] = "WebRTC PQC for DTLS";
inline constexpr char kWebRtcPqcForDtlsDescription[] =
    "Support in WebRTC to enable PQC for DTLS";

inline constexpr char kWebrtcUseMinMaxVEADimensionsName[] =
    "WebRTC Min/Max Video Encode Accelerator dimensions";
inline constexpr char kWebrtcUseMinMaxVEADimensionsDescription[] =
    "When enabled, WebRTC will only use the Video Encode Accelerator for "
    "video resolutions inside those published as supported.";

inline constexpr char kWebSigninLeadsToImplicitlySignedInStateName[] =
    "Web Signin leads To implicitly signed-in state";
inline constexpr char kWebSigninLeadsToImplicitlySignedInStateDescription[] =
    "When enabled, web sign-in will implicitly sign the user in.";

inline constexpr char kWebTransportDeveloperModeName[] =
    "WebTransport Developer Mode";
inline constexpr char kWebTransportDeveloperModeDescription[] =
    "When enabled, removes the requirement that all certificates used for "
    "WebTransport over HTTP/3 are issued by a known certificate root.";

inline constexpr char kWebUsbDeviceDetectionName[] =
    "Automatic detection of WebUSB-compatible devices";
inline constexpr char kWebUsbDeviceDetectionDescription[] =
    "When enabled, the user will be notified when a device which advertises "
    "support for WebUSB is connected. Disable if problems with USB devices are "
    "observed when the browser is running.";

inline constexpr char kWebXrForceRuntimeName[] = "Force WebXr Runtime";
inline constexpr char kWebXrForceRuntimeDescription[] =
    "Force the browser to use a particular runtime, even if it would not "
    "usually be enabled or would otherwise not be selected based on the "
    "attached hardware.";

inline constexpr char kWebXrRuntimeChoiceNone[] = "No Runtime";
inline constexpr char kWebXrRuntimeChoiceArCore[] = "ARCore";
inline constexpr char kWebXrRuntimeChoiceCardboard[] = "Cardboard";
inline constexpr char kWebXrRuntimeChoiceOpenXR[] = "OpenXR";
inline constexpr char kWebXrRuntimeChoiceOrientationSensors[] =
    "Orientation Sensors";

inline constexpr char kWebXrHandAnonymizationStrategyName[] =
    "WebXr Hand Anonymization Strategy";
inline constexpr char kWebXrHandAnonymizationStrategyDescription[] =
    "Force the browser to use a particular strategy for anonymizing hand data, "
    "the default order has a hierarchy of strategies to try and if all of them "
    "fail, then no data will be returned, while this choice does allow the "
    "(not recommended) alternative of bypassing these algorithms all together.";

inline constexpr char kWebXrHandAnonymizationChoiceNone[] =
    "None (Not Recommended)";
inline constexpr char kWebXrHandAnonymizationChoiceRuntime[] =
    "Runtime Provided";
inline constexpr char kWebXrHandAnonymizationChoiceFallback[] =
    "Chrome Fallback";

inline constexpr char kWebXrIncubationsName[] = "WebXR Incubations";
inline constexpr char kWebXrIncubationsDescription[] =
    "Enables experimental features for WebXR.";

inline constexpr char kYourSavedInfoSettingsPageName[] =
    "Your Saved Info settings page";
inline constexpr char kYourSavedInfoSettingsPageDescription[] =
    "Enables the experimental \"Your saved info\" settings page, replacing "
    "the existing \"Autofill and passwords\" page.";

inline constexpr char kZeroCopyName[] = "Zero-copy rasterizer";
inline constexpr char kZeroCopyDescription[] =
    "Raster threads write directly to GPU memory associated with tiles.";

inline constexpr char kZeroCopyVideoEncodingName[] = "Zero copy video encoding";
inline constexpr char kZeroCopyVideoEncodingDescription[] =
    "Enables zero-copy video encoding via GL rendering on the input surface.";

inline constexpr char kEnableVulkanName[] = "Vulkan";
inline constexpr char kEnableVulkanDescription[] =
    "Use vulkan as the graphics backend.";

inline constexpr char kDefaultAngleVulkanName[] = "Default ANGLE Vulkan";
inline constexpr char kDefaultAngleVulkanDescription[] =
    "Use the Vulkan backend for ANGLE by default.";

inline constexpr char kVulkanFromAngleName[] = "Vulkan from ANGLE";
inline constexpr char kVulkanFromAngleDescription[] =
    "Initialize Vulkan from inside ANGLE and share the instance with Chrome.";

inline constexpr char kShowTabGroupsMacSystemMenuName[] =
    "Show tab group colours of tabs in Mac top bar menu";
inline constexpr char kShowTabGroupsMacSystemMenuDescription[] =
    "Show tab group colours of tabs that are in tab groups in the 'tabs' and"
    "'windows' menu' of the Mac OS menu bar";

inline constexpr char kUsePassthroughCommandDecoderName[] =
    "Use passthrough command decoder";
inline constexpr char kUsePassthroughCommandDecoderDescription[] =
    "Use chrome passthrough command decoder instead of validating command "
    "decoder.";

inline constexpr char kUserValueDefaultBrowserStringsName[] =
    "Default Browser settings page - updated strings";
inline constexpr char kUserValueDefaultBrowserStringsDescription[] =
    "Improves the flow and the wording on the Default Browser settings page.";

inline constexpr char kEnableUnsafeSwiftShaderName[] =
    "Enable unsafe SwiftShader fallback";
inline constexpr char kEnableUnsafeSwiftShaderDescription[] =
    "Allow SwiftShader to be used as a fallback for software WebGL. Using this "
    "flag is unsafe and should only be used for local development.";

inline constexpr char kPredictableReportedQuotaName[] =
    "Predictable Reported Quota";
inline constexpr char kPredictableReportedQuotaDescription[] =
    "Enables reporting of a predictable quota from the StorageManager's "
    "estimate API. This flag is intended only for validating if this change "
    "caused an unforeseen bug.";

inline constexpr char kRunVideoCaptureServiceInBrowserProcessName[] =
    "Run video capture service in browser";
inline constexpr char kRunVideoCaptureServiceInBrowserProcessDescription[] =
    "Run the video capture service in the browser process.";

inline constexpr char kPromptAPIForGeminiNanoName[] =
    "Prompt API for Gemini Nano";
inline constexpr char kPromptAPIForGeminiNanoDescription[] =
    "Enables the exploratory Prompt API, allowing you to send natural language "
    "instructions to a built-in large language model (Gemini Nano in Chrome). "
    "Exploratory APIs are designed for local prototyping to help discover "
    "potential use cases, and may never launch. These explorations will inform "
    "the built-in AI roadmap [1]. "
    "This API is primarily intended for natural language processing tasks such "
    "as summarizing, classifying, or rephrasing text. It is NOT suitable for "
    "use cases that require factual accuracy (e.g. answering knowledge "
    "questions). "
    "You must comply with our Prohibited Use Policy [2] which provides "
    "additional details about appropriate use of Generative AI.";
inline constexpr const char* kAIAPIsForGeminiNanoLinks[2] = {
    "https://goo.gle/chrome-ai-dev-preview",
    "https://policies.google.com/terms/generative-ai/use-policy"};

inline constexpr char kPromptAPIForGeminiNanoMultimodalInputName[] =
    "Prompt API for Gemini Nano with Multimodal Input";
inline constexpr char kPromptAPIForGeminiNanoMultimodalInputDescription[] =
    "Extends the exploratory Prompt API with image and audio input types. "
    "Allows you to supplement natural language instructions for a built-in "
    "large language model (Gemini Nano in Chrome) with image and audio inputs. "
    "Exploratory APIs are designed for local prototyping to help discover "
    "potential use cases, and may never launch. These explorations will inform "
    "the built-in AI roadmap [1]. "
    "This API enhancement is primarily intended for natural language "
    "processing tasks associated with visual and auditory data, such as "
    "generating rough descriptions of pictures and sounds. It is NOT suitable "
    "for use cases that require factual accuracy (e.g. answering knowledge "
    "questions). "
    "You must comply with our Prohibited Use Policy [2] which provides "
    "additional details about appropriate use of Generative AI.";

inline constexpr char kSummarizationAPIForGeminiNanoName[] =
    "Summarization API for Gemini Nano";
inline constexpr char kSummarizationAPIForGeminiNanoDescription[] =
    "Enables the Summarization API, allowing you to summarize a piece "
    "of text with a built-in large language model (Gemini Nano in Chrome)."
    "The API may be subject to changes including the supported options."
    "Please refer to the built-in AI article [1] for details. "
    "This API It is NOT suitable for use cases that require factual accuracy "
    "(e.g. answering knowledge questions). "
    "You must comply with our Prohibited Use Policy [2] which provides "
    "additional details about appropriate use of Generative AI.";

inline constexpr char kWriterAPIForGeminiNanoName[] =
    "Writer API for Gemini Nano";
inline constexpr char kWriterAPIForGeminiNanoDescription[] =
    "Enables the Writer API, allowing you to write a piece "
    "of text with a built-in large language model (Gemini Nano in Chrome)."
    "The API may be subject to changes including the supported options."
    "Please refer to the built-in AI article [1] for details. "
    "You must comply with our Prohibited Use Policy [2] which provides "
    "additional details about appropriate use of Generative AI.";

inline constexpr char kRewriterAPIForGeminiNanoName[] =
    "Rewriter API for Gemini Nano";
inline constexpr char kRewriterAPIForGeminiNanoDescription[] =
    "Enables the Rewriter API, allowing you to rewrite a piece "
    "of text with a built-in large language model (Gemini Nano in Chrome)."
    "The API may be subject to changes including the supported options."
    "Please refer to the built-in AI article [1] for details. "
    "You must comply with our Prohibited Use Policy [2] which provides "
    "additional details about appropriate use of Generative AI.";

inline constexpr char kProofreaderAPIForGeminiNanoName[] =
    "Proofreader API for Gemini Nano";
inline constexpr char kProofreaderAPIForGeminiNanoDescription[] =
    "Enables the Proofreader API, allowing you to proofread a piece of text"
    "with a built-in large language model (Gemini Nano in Chrome)."
    "The API may be subject to changes including the supported options."
    "Please refer to the built-in AI article [1] for details. "
    "You must comply with our Prohibited Use Policy [2] which provides "
    "additional details about appropriate use of Generative AI.";

// Android ---------------------------------------------------------------------
// FLAG_DESCRIPTIONS_ANDROID_START

inline constexpr char kAAudioPerStreamDeviceSelectionName[] =
    "AAudio per-stream device selection";
inline constexpr char kAAudioPerStreamDeviceSelectionDescription[] =
    "Enables per-stream device selection for AAudio streams. No effect on "
    "versions of Android prior to Android Q.";

inline constexpr char kAccessibilityDeprecateTypeAnnounceName[] =
    "Accessibility Deprecate TYPE_ANNOUNCE";
inline constexpr char kAccessibilityDeprecateTypeAnnounceDescription[] =
    "When enabled, TYPE_ANNOUNCE events will no longer be sent for live "
    "regions in the web contents.";

inline constexpr char kAccessibilityImproveLiveRegionAnnounceName[] =
    "Accessibility Improve Live Region Announcement";
inline constexpr char kAccessibilityImproveLiveRegionAnnounceDescription[] =
    "When enabled, live region announcements will be sent to Android via "
    "WINDOW_CONTENT_CHANGED events corresponding to each live region element "
    "change rather than via TYPE_ANNOUNCEMENT.";

inline constexpr char kAccessibilitySetSelectableOnAllNodesWithTextName[] =
    "AccessibilitySetSelectableOnAllNodesWithTextName";
inline constexpr char
    kAccessibilitySetSelectableOnAllNodesWithTextDescription[] =
        "When enabled, the accessibility tree for the web contents will "
        "include "
        "isTextSelectable and the ACTION_SET_SELECTION action on all relevant "
        "nodes.";

inline constexpr char kAccessibilityManageBroadcastReceiverOnBackgroundName[] =
    "Manage accessibility Broadcast Receiver on a background thread";
inline constexpr char
    kAccessibilityManageBroadcastReceiverOnBackgroundDescription[] =
        "When enabled, registering and un-registering the broadcast "
        "receiver will be on the background thread.";

inline constexpr char kAccessibilityPopulateSupplementalDescriptionApiName[] =
    "Accessibility populate supplemental description";
inline constexpr char
    kAccessibilityPopulateSupplementalDescriptionApiDescription[] =
        "When enabled, the supplemental description information will be "
        "populated "
        "using the Android supplemental description API.";

inline constexpr char kAccessibilitySequentialFocusName[] =
    "Accessibility Sequential Focus Navigation";
inline constexpr char kAccessibilitySequentialFocusDescription[] =
    "Enables synchronization of keyboard focus starting point with "
    "accessibility focus.";

inline constexpr char kAccessibilityTextFormattingName[] =
    "Accessibility Text Formatting";
inline constexpr char kAccessibilityTextFormattingDescription[] =
    "When enabled, text formatting information will be included in the "
    "AccessibilityNodeInfo tree on Android";

inline constexpr char kAccessibilityUnifiedSnapshotsName[] =
    "Accessibility Unified Snapshots";
inline constexpr char kAccessibilityUnifiedSnapshotsDescription[] =
    "When enabled, use the experimental unified code path for AXTree "
    "snapshots.";

inline constexpr char kAdaptiveButtonInTopToolbarPageSummaryName[] =
    "Adaptive button in top toolbar - Page Summary";
inline constexpr char kAdaptiveButtonInTopToolbarPageSummaryDescription[] =
    "Enables a summary button in the top toolbar. Must be selected in "
    "Settings > Toolbar Shortcut.";

inline constexpr char kAndroidAnimatedProgressBarInBrowserName[] =
    "Animate composited progress bar with browser frames.";
inline constexpr char kAndroidAnimatedProgressBarInBrowserDescription[] =
    "Hides the android progress bar and enables animating load progress "
    "updates for the composited progress bar in slim.";

inline constexpr char kAndroidAudioDeviceListenerName[] =
    "Android Audio Device Listener";
inline constexpr char kAndroidAudioDeviceListenerDescription[] =
    "Enables listening to audio device list change events, allowing web apps "
    "to react to audio devices being connected and disconnected.";

inline constexpr char kAndroidAutofillUpdateContextForWebContentsName[] =
    "Android Autofill updates context for WebContents";
inline constexpr char kAndroidAutofillUpdateContextForWebContentsDescription[] =
    "When enabled, the Autofill provider is updated whenever the context of "
    "the WebContents changes.";

inline constexpr char kAndroidAutoMintedTWAName[] = "Auto-minted TWA";
inline constexpr char kAndroidAutoMintedTWADescription[] =
    "Installs Web apps locally as an auto-minted Trusted Web Activity-based "
    "Android package instead of a server-minted WebAPK. This feature "
    "additionally requires WebApp Mainline module enabled.";

inline constexpr char kAndroidCaretBrowsingName[] = "Enable Caret Browsing.";
inline constexpr char kAndroidCaretBrowsingDescription[] =
    "Allows users to interact with a webpage using a keyboard.";

inline constexpr char kAndroidComposeplateName[] =
    "Enable composeplate on New Tab Page";
inline constexpr char kAndroidComposeplateDescription[] =
    "Show a composeplate on New Tab Page.";

inline constexpr char kAndroidComposeplateLFFName[] =
    "Enable composeplate button on New Tab Page for large form factors";
inline constexpr char kAndroidComposeplateLFFDescription[] =
    "Show a composeplate button on New Tab Page for large form factors.";

inline constexpr char kAndroidContextMenuDuplicateTabsName[] =
    "Android context menu duplicate tabs";
inline constexpr char kAndroidContextMenuDuplicateTabsDescription[] =
    "Adds a new context menu option allowing users to duplicate the"
    "selected tabs";

inline constexpr char kAndroidDataImporterServiceName[] =
    "Data Importer Service";
inline constexpr char kAndroidDataImporterServiceDescription[] =
    "Enables the service for importing user data from other browsers.";

inline constexpr char kAndroidDesktopWebPrefsLargeDisplaysName[] =
    "Android Desktop WebPrefs for Large Displays";
inline constexpr char kAndroidDesktopWebPrefsLargeDisplaysDescription[] =
    "Enables large display specific layout/renderer settings for "
    "Android large form factor devices";

inline constexpr char kAndroidDesktopZoomScalingName[] =
    "Android Desktop Zoom Scaling";
inline constexpr char kAndroidDesktopZoomScalingDescription[] =
    "When enabled, this feature will scale the Desktop Android web content up "
    "by some percentage, transparent to the user.";

inline constexpr char kAndroidDocumentPictureInPictureName[] =
    "Enable Document Picture-In-Picture in desktop windowing on Android.";
inline constexpr char kAndroidDocumentPictureInPictureDescription[] =
    "Enables Document Picture-In-Picture API allowing websites to open "
    "new floating always-on-top window with arbitrary HTML content.";

inline constexpr char kAndroidElegantTextHeightName[] =
    "Android Elegant Text Height";
inline constexpr char kAndroidElegantTextHeightDescription[] =
    "Enables elegant text height in core BrowserUI theme.";

inline constexpr char kAndroidGrammarCheckName[] =
    "Enable grammar checks on text input";
inline constexpr char kAndroidGrammarCheckDescription[] =
    "When typing, allows spellcheckers to highlight grammar errors and suggest "
    "corrections on browser text input.";

inline constexpr char kAndroidHubSearchTabGroupsName[] =
    "Android Hub Tab Group Search";
inline constexpr char kAndroidHubSearchTabGroupsDescription[] =
    "Enables searching through tab groups in the hub.";

inline constexpr char kAndroidMediaInsertionName[] =
    "Enable IME media insertion";
inline constexpr char kAndroidMediaInsertionDescription[] =
    "Enables IMEs to insert media content such as images, gifs and stickers.";

inline constexpr char kAndroidMinimalUiLargeScreenName[] =
    "Enable new minimal ui in desktop windowing";
inline constexpr char kAndroidMinimalUiLargeScreenDescription[] =
    "Display new minimal ui for PWAs on devices that support "
    "desktop windowing.";

inline constexpr char kAndroidOpenPdfInlineBackportName[] =
    "Open PDF Inline on Android pre-V";
inline constexpr char kAndroidOpenPdfInlineBackportDescription[] =
    "Enable Open PDF Inline on Android pre-V.";

inline constexpr char kAndroidOpenPdfInlineName[] =
    "Open PDF Inline on Android";
inline constexpr char kAndroidOpenPdfInlineDescription[] =
    "Enable Open PDF Inline on Android.";

inline constexpr char kAndroidNewMediaPickerName[] =
    "Enable new media capture picker on Android";
inline constexpr char kAndroidNewMediaPickerDescription[] =
    "Enables the new media capture picker UI on Android.";

inline constexpr char kAndroidPbDisablePulseAnimationName[] =
    "Android progress bar disable pulse animation";
inline constexpr char kAndroidPbDisablePulseAnimationDescription[] =
    "When enabled, there will no longer be a pulse on android progress bar for "
    "slow loading pages";

inline constexpr char kAndroidPbDisableSmoothAnimationName[] =
    "Android Progress Bar Disable Smooth Animation";
inline constexpr char kAndroidPbDisableSmoothAnimationDescription[] =
    "When enabled, there will no longer be a smooth progress animation for the "
    "android progress bar for slow loading pages";

inline constexpr char kAndroidPdfAssistContentName[] =
    "Provide assist content for PDF";
inline constexpr char kAndroidPdfAssistContentDescription[] =
    "Provide assist content for PDF on Android.";

inline constexpr char kAndroidPinnedTabsName[] = "Android pinned tabs";
inline constexpr char kAndroidPinnedTabsDescription[] =
    "Enables the ability to pin tabs through various entry points like context "
    "menus and overflow menu items.";

inline constexpr char kAndroidPinnedTabsTabletTabStripName[] =
    "Android pinned tabs on tablet tab strip in the tabbed layout";
inline constexpr char kAndroidPinnedTabsTabletTabStripDescription[] =
    "Enables the ability to pin tabs through tab context menu on tablet tab "
    "strip in the tabbed layout. This is M1 of Android Pinned Tabs";

inline constexpr char
    kAndroidShowRestoreTabsPromoOnFREBypassedKillSwitchName[] =
        "Android Show Restore Tab Promo On FRE Bypassed Kill Switch";
inline constexpr char
    kAndroidShowRestoreTabsPromoOnFREBypassedKillSwitchDescription[] =
        "Allows Restore Tabs Promo to run even FRE is bypassed, based on a "
        "time stamp set when the user had first initialized the app.";

inline constexpr char kAndroidSpellcheckFullApiBlinkName[] =
    "Enable full Android Spellchecker API support for Blink";
inline constexpr char kAndroidSpellcheckFullApiBlinkDescription[] =
    "If enabled, provides API support for custom spell check menus that are "
    "rendered by Android applications.";

inline constexpr char kAndroidSpellcheckNativeUiName[] =
    "Enable native-like spellcheck underline UI";
inline constexpr char kAndroidSpellcheckNativeUiDescription[] =
    "Makes the spellcheck underline style the same as native applications";

inline constexpr char kAndroidSurfaceColorUpdateName[] =
    "Android surface color update.";
inline constexpr char kAndroidSurfaceColorUpdateDescription[] =
    "If enabled, updates the android surface colors for toolbar/omnibox.";

inline constexpr char kAndroidTabDeclutterArchiveAllButActiveTabName[] =
    "Archive all tabs except active";
inline constexpr char kAndroidTabDeclutterArchiveAllButActiveTabDescription[] =
    "Causes all tabs in model (except the current active one) to be archived. "
    "Used for manual testing.";

inline constexpr char kAndroidTabDeclutterArchiveTabGroupsName[] =
    "Archive all inactive tab groups.";
inline constexpr char kAndroidTabDeclutterArchiveTabGroupsDescription[] =
    "Enables auto-archival of inactive tab groups and their inactive tabs.";

inline constexpr char kAndroidTabDeclutterPerformanceImprovementsName[] =
    "Android Tab Declutter performance improvements";
inline constexpr char kAndroidTabDeclutterPerformanceImprovementsDescription[] =
    "Enables performance improvements to the android tab declutter process.";

inline constexpr char kAndroidTabGroupsColorUpdateGM3Name[] =
    "Android tab groups color update";
inline constexpr char kAndroidTabGroupsColorUpdateGM3Description[] =
    "If enabled, allows tab groups to have display the color chosen by the "
    "user.";

inline constexpr char kAndroidTabHighlightingName[] =
    "Android tab strip tab highlighting";
inline constexpr char kAndroidTabHighlightingDescription[] =
    "If enabled, allows users to perform bulk actions on tab by using shift "
    "click or ctrl click to highlight them.";

inline constexpr char kAndroidThemeModuleName[] = "Android Theme Module";
inline constexpr char kAndroidThemeModuleDescription[] =
    "Enables external theme overlays for Chrome activities when available.";

inline constexpr char kAndroidThemeResourceProviderName[] =
    "Android Theme Resource Provider";
inline constexpr char kAndroidThemeResourceProviderDescription[] =
    "Enables the Android theme resource provider.";

inline constexpr char kAndroidTipsNotificationsName[] =
    "Android Tips Notifications";
inline constexpr char kAndroidTipsNotificationsDescription[] =
    "Enable tips notifications for supported features on Android.";

inline constexpr char kAndroidEnableTWAOriginDisplayName[] =
    "Enable TWA origin display";
inline constexpr char kAndroidEnableTWAOriginDisplayDescription[] =
    "For Trusted Web Apps (TWAs), display origin on web app header.";

inline constexpr char kAndroidUseCorrectDisplayWorkAreaName[] =
    "Enable accounting system UI for computing the display work area";
inline constexpr char kAndroidUseCorrectDisplayWorkAreaDescription[] =
    "Enable accounting system's bars and display cutouts for the correct "
    "computation of the display work area. The Web API Screen properties "
    "availLeft / availTop / availHeight / availWidth accurately reflect the "
    "accessible content display area.";

inline constexpr char kAndroidUseCorrectWindowBoundsName[] =
    "Use accurate top-level browser window bounds reported by Android.";
inline constexpr char kAndroidUseCorrectWindowBoundsDescription[] =
    "Use Android WindowManager as the data source for top-level browser window "
    "bounds in Blink. Impacts values reported by the window.screenX, "
    "window.screenY, window.outerWidth, and window.outerHeight web APIs.";

inline constexpr char kAndroidUseDisplayTopologyName[] =
    "Enable usage of display topology.";
inline constexpr char kAndroidUseDisplayTopologyDescription[] =
    "Enables usage of the display topology API to obtain information about all "
    "displays. The browser obtains this infromation on startup and uses it for "
    "Window Management API and better window placement on other screens.";

inline constexpr char kAndroidWindowControlsOverlayName[] =
    "Enable window controls overlay in PWAs";
inline constexpr char kAndroidWindowControlsOverlayDescription[] =
    "Allow window-controls-overlay display mode for PWAs.";

inline constexpr char kAndroidWindowManagementWebApiName[] =
    "Window Management Web API";
inline constexpr char kAndroidWindowManagementWebApiDescription[] =
    "Enable Window Management Web API. Websites can obtain information about "
    "displays and display topology. For proper work "
    "android-use-display-topology and android-use-correct-display-work-area "
    "should be enabled.";

inline constexpr char kAndroidWindowOcclusionName[] =
    "Enable occlusion tracking on Android.";
inline constexpr char kAndroidWindowOcclusionDescription[] =
    "Enables occlusion tracking on Android, which can save CPU and memory in "
    "multi-window environments.";

inline constexpr char kAndroidWindowPopupCustomTabUiName[] =
    "Enable new UI mode in Custom Tabs for contextual popups.";
inline constexpr char kAndroidWindowPopupCustomTabUiDescription[] =
    "Show the title of a tab opened in a pop-up window in caption bar of the "
    "top-level window, if exists and has sufficient dimensions.";

inline constexpr char kAndroidWindowPopupLargeScreenName[] =
    "Enable desktop-like behavior of window popup web API in desktop windowing "
    "on Android.";
inline constexpr char kAndroidWindowPopupLargeScreenDescription[] =
    "Open an actual new window instead of new tab on window.open() Javascript "
    "call and make moving windows with window.{move|resize}{By|To}() "
    "possible.";

inline constexpr char kAndroidWindowPopupPredictFinalBoundsName[] =
    "Try to predict the displacement between top-level window and website "
    "viewport before creating a popup.";
inline constexpr char kAndroidWindowPopupPredictFinalBoundsDescription[] =
    "Size of the website viewport of a new contextual popup may be requested "
    "as a parameter in a window.open() Javascript call. If this flag is "
    "enabled, then the final bounds of the popup will be predicted before its "
    "creation in hope the resizing action won't be needed. See also the "
    "enable-android-window-popup-resize-after-spawn flag that regulates "
    "post-creation bounds adjustments.";

inline constexpr char kAndroidWindowPopupResizeAfterSpawnName[] =
    "Resize a contextual popup after spawning it to compensate for UI elements "
    "so that its website viewport dimensions match requested ones.";
inline constexpr char kAndroidWindowPopupResizeAfterSpawnDescription[] =
    "Size of the website viewport of a new contextual popup may be requested "
    "as a parameter in a window.open() Javascript call. If this flag is "
    "enabled, then the popup will be resized after its creation to ensure that "
    "this web API contract is satisfied. See also the "
    "enable-android-window-popup-predict-final-bounds flag that regulates "
    "pre-creation bounds adjustments.";

inline constexpr char kAndroidWebAppMenuButtonName[] =
    "Enable minimal ui menu button";
inline constexpr char kAndroidWebAppMenuButtonDescription[] =
    "Display minimal ui menu button for PWAs on devices that support "
    "desktop windowing.";

inline constexpr char kAndroidZoomIndicatorName[] = "Android Zoom Indicator";
inline constexpr char kAndroidZoomIndicatorDescription[] =
    "Enable zoom indicator on Android.";

inline constexpr char kAnimatedImageDragShadowName[] =
    "Enable animated image drag shadow on Android.";
inline constexpr char kAnimatedImageDragShadowDescription[] =
    "Animate the shadow image from its original bound to the touch point. ";

inline constexpr char kAnimateSuggestionsListAppearanceName[] =
    "Animate appearance of the omnibox suggestions list";
inline constexpr char kAnimateSuggestionsListAppearanceDescription[] =
    "Animate the omnibox suggestions list when it appears instead of "
    "immediately setting it to visible";

inline constexpr char kAppSpecificHistoryName[] = "Allow app specific history";
inline constexpr char kAppSpecificHistoryDescription[] =
    "If enabled, history results will also be categorized by application.";

inline constexpr char kAutofillAndroidDesktopKeyboardAccessoryRevampName[] =
    "Move keyboard accessory to top on devices with large form factors";
inline constexpr char
    kAutofillAndroidDesktopKeyboardAccessoryRevampDescription[] =
        "When enabled, a new keyboard accessory design will be applied. It "
        "moves "
        "the entire bar to the top and changes the style to be more convenient "
        "for "
        "devices with large screens.";

inline constexpr char kAutomotiveBackButtonBarStreamlineName[] =
    "AutomotiveBackButtonBarStreamline";
inline constexpr char kAutomotiveBackButtonBarStreamlineDescription[] =
    "If enabled, streamline the Android Automotive back button bar on CaRMA "
    "devices when not in full screen.";

inline constexpr char kBackgroundNotPerceptibleBindingName[] =
    "Enable not perceptible binding without cpu priority boosting";
inline constexpr char kBackgroundNotPerceptibleBindingDescription[] =
    "If enabled, not perceptible binding put processes to the background cpu "
    "cgroup";

inline constexpr char kBoardingPassDetectorName[] = "Boarding Pass Detector";
inline constexpr char kBoardingPassDetectorDescription[] =
    "Enable Boarding Pass Detector";

inline constexpr char kBookmarkPaneAndroidName[] = "Bookmark hub pane";
inline constexpr char kBookmarkPaneAndroidDescription[] =
    "Enables a bookmark hub pane.";

inline constexpr char kBrowserControlsDebuggingName[] =
    "Browser controls debugging";
inline constexpr char kBrowserControlsDebuggingDescription[] =
    "Enables logs to debug Android browser controls.";

inline constexpr char kBrowsingDataModelName[] = "Browsing Data Model";
inline constexpr char kBrowsingDataModelDescription[] =
    "Enables BDM on Android.";

inline constexpr char kCCTAdaptiveButtonName[] =
    "Adaptive button in Custom Tabs";
inline constexpr char kCCTAdaptiveButtonDescription[] =
    "Enables adaptive action button in Custom Tabs toolbar";

inline constexpr char kCCTAdaptiveButtonTestSwitchName[] =
    "Test flags for adaptive button in Custom Tabs";
inline constexpr char kCCTAdaptiveButtonTestSwitchDescription[] =
    "Enables adaptive action button in Custom Tabs toolbar, with some tweaks "
    "to facilitate testing 1) simulate narrow toolbar to hide MTB 2) Always "
    "show static action MTB chip animation";

inline constexpr char kCCTAuthTabName[] = "CCT Auth Tab";
inline constexpr char kCCTAuthTabDescription[] =
    "Enable AuthTab used for authentication";

inline constexpr char kCCTAuthTabDisableAllExternalIntentsName[] =
    "Disable all external intents in Auth Tab";
inline constexpr char kCCTAuthTabDisableAllExternalIntentsDescription[] =
    "Disables all external intents in Auth Tab";

inline constexpr char kCCTAuthTabEnableHttpsRedirectsName[] =
    "Enable HTTPS redirect scheme in Auth Tab";
inline constexpr char kCCTAuthTabEnableHttpsRedirectsDescription[] =
    "Enables HTTPS redirect scheme in Auth Tab";

inline constexpr char kCCTContextualMenuItemsName[] =
    "Enable Contextual Menu Items in CCT";
inline constexpr char kCCTContextualMenuItemsDescription[] =
    "When enabled, contextual menu items passed by developers will be visible "
    "in CCTs.";

inline constexpr char kCCTIncognitoAvailableToThirdPartyName[] =
    "Allow third party to open Custom Tabs Incognito mode";
inline constexpr char kCCTIncognitoAvailableToThirdPartyDescription[] =
    "Enabling it would allow third party apps to open incognito mode for "
    "Chrome Custom Tabs, on Android.";

inline constexpr char kCCTNestedSecurityIconName[] =
    "Nest the CCT security icon under the title.";
inline constexpr char kCCTNestedSecurityIconDescription[] =
    "When enabled, the CCT toolbar security icon will be nested under the "
    "title.";

inline constexpr char kCCTGoogleBottomBarName[] = "Google Bottom Bar";
inline constexpr char kCCTGoogleBottomBarDescription[] =
    "Show bottom bar on Custom Tabs opened by the Android Google App.";

inline constexpr char kCCTGoogleBottomBarVariantLayoutsName[] =
    "Google Bottom Bar Variant Layouts";
inline constexpr char kCCTGoogleBottomBarVariantLayoutsDescription[] =
    "Show different layouts on Google Bottom Bar.";

inline constexpr char kCCTOpenInBrowserButtonIfAllowedByEmbedderName[] =
    "Open in Browser Button in CCT if allowed by Embedder";
inline constexpr char kCCTOpenInBrowserButtonIfAllowedByEmbedderDescription[] =
    "Open in Browser Button in CCT if allowed by Embedder";

inline constexpr char kCCTOpenInBrowserButtonIfEnabledByEmbedderName[] =
    "Open in Browser Button in CCT if enabled by Embedder";
inline constexpr char kCCTOpenInBrowserButtonIfEnabledByEmbedderDescription[] =
    "Open in Browser Button in CCT if enabled by Embedder";

inline constexpr char kCCTResizableForThirdPartiesName[] =
    "Bottom sheet Custom Tabs (third party)";
inline constexpr char kCCTResizableForThirdPartiesDescription[] =
    "Enable bottom sheet Custom Tabs for third party apps.";

inline constexpr char kCCTToolbarRefactorName[] = "CCT Toolbar Refactor";
inline constexpr char kCCTToolbarRefactorDescription[] = "CCT Toolbar Refactor";

inline constexpr char kChangeUnfocusedPriorityName[] =
    "Change Unfocused Priority";
inline constexpr char kChangeUnfocusedPriorityDescription[] =
    "Lower process priority for processes with only unfocused windows, "
    "allowing them to be discarded sooner.";

inline constexpr char kChromeItemPickerUiName[] = "Chrome Item Picker Ui";
inline constexpr char kChromeItemPickerUiDescription[] =
    "Enable the Chrome item picker to show";

inline constexpr char kChimeAlwaysShowNotificationDescription[] =
    "A debug flag to always show Chime notification after receiving a payload.";
inline constexpr char kChimeAlwaysShowNotificationName[] =
    "Always show Chime notification";

inline constexpr char kChimeAndroidSdkDescription[] =
    "Enable Chime SDK to receive push notification.";
inline constexpr char kChimeAndroidSdkName[] = "Use Chime SDK";

inline constexpr char kClankDefaultBrowserPromoName[] =
    "Clank default browser promo 2";
inline constexpr char kClankDefaultBrowserPromoDescription[] =
    "When enabled, show additional non-intrusive entry points to allow users "
    "to set Chrome as their default browser, if the trigger conditions are "
    "met.";

inline constexpr char kClankDefaultBrowserPromoRoleManagerName[] =
    "Clank default browser Promo Role Manager ";
inline constexpr char kClankDefaultBrowserPromoRoleManagerDescription[] =
    "Sets the Role Manager Default Browser Promo for testing the new "
    "Default Browser Promo Feature";

inline constexpr char kClientSideDetectionSendIntelligentScanInfoAndroidName[] =
    "Client Side Detection Send Brand and Intent on Android";
inline constexpr char
    kClientSideDetectionSendIntelligentScanInfoAndroidDescription[] =
        "Enables on device LLM output on pages to inquire for brand and intent "
        "of "
        "the page on Android.";

inline constexpr char kClientSideDetectionShowScamVerdictWarningAndroidName[] =
    "Client Side Detection Show Scam Verdict Warning on Android";
inline constexpr char
    kClientSideDetectionShowScamVerdictWarningAndroidDescription[] =
        "Show warnings based on the scam verdict field in Client Side "
        "Detection "
        "response on Android.";

inline constexpr char kContextualSearchSuppressShortViewName[] =
    "Contextual Search suppress short view";
inline constexpr char kContextualSearchSuppressShortViewDescription[] =
    "Contextual Search suppress when the base page view is too short";

inline constexpr char
    kCredentialManagementThirdPartyWebApiRequestForwardingName[] =
        "Credential Management Third Party Web API Request Forwarding";
inline constexpr char
    kCredentialManagementThirdPartyWebApiRequestForwardingDescription[] =
        "Forwards the requests from web pages that use the Credential "
        "Management "
        "API to 3P password managers if 3P mode autofill is on.";

inline constexpr char kCpaSpecUpdateName[] = "CpaSpecUpdate";
inline constexpr char kCpaSpecUpdateDescription[] =
    "Updates the Cpa button animation and changes the shape of the checked "
    "state button for stateful CPAs.";

inline constexpr char kDeprecatedExternalPickerFunctionName[] =
    "Use deprecated External Picker method";
inline constexpr char kDeprecatedExternalPickerFunctionDescription[] =
    "Use the old-style opening of an External Picker when uploading files";

inline constexpr char kDrawChromePagesEdgeToEdgeName[] =
    "Draw Chrome Pages Edge-to-Edge";
inline constexpr char kDrawChromePagesEdgeToEdgeDescription[] =
    "Enables drawing more native pages and secondary activities edge-to-edge.";

inline constexpr char kDrawCutoutEdgeToEdgeName[] = "DrawCutoutEdgeToEdge";
inline constexpr char kDrawCutoutEdgeToEdgeDescription[] =
    "Enables the Android feature Edge-to-Edge Feature to coordinate with the "
    "Display Cutout for the notch when drawing below the Nav Bar.";

inline constexpr char kEdgeToEdgeBottomChinName[] = "EdgeToEdgeBottomChin";
inline constexpr char kEdgeToEdgeBottomChinDescription[] =
    "Enables the scrollable bottom chin for an intermediate Edge-to-Edge "
    "experience.";

inline constexpr char kEdgeToEdgeEverywhereName[] = "EdgeToEdgeEverywhere";
inline constexpr char kEdgeToEdgeEverywhereDescription[] =
    "Enables Chrome to draw below the system bars, all the time. This is "
    "intended "
    "to facilitate the transition to edge-to-edge being enforced at the system "
    "level.";

inline constexpr char kEdgeToEdgeTabletName[] = "EdgeToEdgeTablet";
inline constexpr char kEdgeToEdgeTabletDescription[] =
    "Enables the Android feature Edge-to-Edge on tablets";

inline constexpr char kEnableAccessibilityLabeledByName[] =
    "Enable Accessibility LabeledBy";
inline constexpr char kEnableAccessibilityLabeledByDescription[] =
    "Enables the experimental support for aria-labelledby list format for "
    "relationships in "
    "the accessibility tree for android.";

inline constexpr char kEnableExclusiveAccessManagerName[] =
    "Enable Exclusive Access Manager on Android builds";
inline constexpr char kEnableExclusiveAccessManagerDescription[] =
    "Enables the integrated handling of the fullscreen, pointer and keyboard "
    "locks. Unifies the UI for the mentioned features.";

inline constexpr char kEducationalTipDefaultBrowserPromoCardName[] =
    "Educational Tip Default Browser Promo Card";
inline constexpr char kEducationalTipDefaultBrowserPromoCardDescription[] =
    "Show the default browser promo card of the educational tip module on "
    "magic stack in clank";

inline constexpr char kEducationalTipModuleName[] = "Educational Tip Module";
inline constexpr char kEducationalTipModuleDescription[] =
    "Show educational tip module on magic stack in clank";

inline constexpr char kEnableCommandLineOnNonRootedName[] =
    "Enable command line on non-rooted devices";
inline constexpr char kEnableCommandLineOnNoRootedDescription[] =
    "Enable reading command line file on non-rooted devices (DANGEROUS).";

inline constexpr char kEnableClipboardDataControlsAndroidName[] =
    "Enable enterprise data controls.";
inline constexpr char kEnableClipboardDataControlsAndroidDescription[] =
    "Enables the enterprise data controls on Android for restricting copy and "
    "paste actions for the clipboard.";

inline constexpr char kEnableEscapeHandlingForSecondaryActivitiesName[] =
    "Enable escape handling for secondary activities and native pages.";
inline constexpr char kEnableEscapeHandlingForSecondaryActivitiesDescription[] =
    "Enables handling escape events on secondary activities and native pages.";

inline constexpr char kExternalNavigationDebugLogsName[] =
    "External Navigation Debug Logs";
inline constexpr char kExternalNavigationDebugLogsDescription[] =
    "Enables detailed logging to logcat about why Chrome is making decisions "
    "about whether to allow or block navigation to other apps";

inline constexpr char kFeedAudioOverviewsName[] = "Feed audio overviews";
inline constexpr char kFeedAudioOverviewsDescription[] =
    "Enables audio overviews in the feed";

inline constexpr char kFeedContainmentName[] = "Feed containment";
inline constexpr char kFeedContainmentDescription[] =
    "Enables putting the feed in a container.";

inline constexpr char kFeedDiscoFeedEndpointName[] =
    "Feed using the DiscoFeed backend endpoint";
inline constexpr char kFeedDiscoFeedEndpointDescription[] =
    "Uses the DiscoFeed endpoint for serving the feed instead of GWS.";

inline constexpr char kFeedFollowUiUpdateName[] =
    "UI Update for the Following Feed";
inline constexpr char kFeedFollowUiUpdateDescription[] =
    "Enables showing the updated UI for the following feed.";

inline constexpr char kFeedHeaderRemovalName[] = "Removing feed header";
inline constexpr char kFeedHeaderRemovalDescription[] =
    "Stops showing the feed header.";

inline constexpr char kFeedLoadingPlaceholderName[] =
    "Feed loading placeholder";
inline constexpr char kFeedLoadingPlaceholderDescription[] =
    "Enables a placeholder UI in "
    "the feed instead of the loading spinner at first load.";

inline constexpr char kFeedSignedOutViewDemotionName[] =
    "Feed signed-out view demotion";
inline constexpr char kFeedSignedOutViewDemotionDescription[] =
    "Enables signed-out view demotion for the Discover Feed.";

inline constexpr char kFloatingSnackbarName[] = "FloatingSnackbar";
inline constexpr char kFloatingSnackbarDescription[] =
    "Enables the snackbar to float on top of the web content.";

inline constexpr char kForceOffTextAutosizingName[] =
    "Force off heuristics for inflating text sizes on devices with small "
    "screens.";
inline constexpr char kForceOffTextAutosizingDescription[] =
    "Disable text autosizing.";

inline constexpr char kFullscreenInsetsApiMigrationName[] =
    "Migrate to the new fullscreen insets APIs";
inline constexpr char kFullscreenInsetsApiMigrationDescription[] =
    "Migration from View#setSystemUiVisibility to WindowInsetsController.";

inline constexpr char kFullscreenInsetsApiMigrationOnAutomotiveName[] =
    "Migrate to the new fullscreen insets APIs on automotive";
inline constexpr char kFullscreenInsetsApiMigrationOnAutomotiveDescription[] =
    "Migration from View#setSystemUiVisibility to WindowInsetsController on "
    "automotive.";

inline constexpr char kGridTabSwitcherSurfaceColorUpdateName[] =
    "Grid tab switcher surface color update";
inline constexpr char kGridTabSwitcherSurfaceColorUpdateDescription[] =
    "Enables grid tab switcher surface color update";

inline constexpr char kGridTabSwitcherUpdateName[] = "Grid tab switcher update";
inline constexpr char kGridTabSwitcherUpdateDescription[] =
    "Enables the visual changes in the grid tab switcher.";

inline constexpr char kHistoryPaneAndroidName[] = "History Pane Android";
inline constexpr char kHistoryPaneAndroidDescription[] =
    "Enables showing a new pane in the hub that displays History.";

inline constexpr char kHomeModulePrefRefactorName[] =
    "Home module pref refactor";
inline constexpr char kHomeModulePrefRefactorDescription[] =
    "Use UserPrefs for home module customization settings (for the "
    "NTP).";

inline constexpr char kHubBackButtonName[] = "Hub back button";
inline constexpr char kHubBackButtonDescription[] =
    "Enables a back button on the hub for large screen devices.";

inline constexpr char kHubSlideAnimationName[] = "Hub Slide Animation";
inline constexpr char kHubSlideAnimationDescription[] =
    "Enables the slide animation on the hub when panes are switched.";

inline constexpr char kMagicStackAndroidName[] = "Magic Stack Android";
inline constexpr char kMagicStackAndroidDescription[] =
    "Show a magic stack which contains a list of modules on Start surface and "
    "NTPs on Android.";

inline constexpr char kMaliciousApkDownloadCheckName[] =
    "Malicious APK download check";
inline constexpr char kMaliciousApkDownloadCheckDescription[] =
    "Check APK downloads on Android for malware.";

inline constexpr char kMayLaunchUrlUsesSeparateStoragePartitionName[] =
    "MayLaunchUrl Uses Separate Storage Partition";
inline constexpr char kMayLaunchUrlUsesSeparateStoragePartitionDescription[] =
    "Forces MayLaunchUrl to use a new, ephemeral, storage partition for the "
    "url given to it. This is an experimental feature and may reduce "
    "performance.";

inline constexpr char kMediaCodecLowDelayModeName[] =
    "MediaCodec low delay mode";
inline constexpr char kMediaCodecLowDelayModeDescription[] =
    "Allows selection of low latency MediaCodec instances for video "
    "decoding when low delay is requested by the underlying stream.";

inline constexpr char kMediaIndicatorsAndroidName[] =
    "Media Indicators Android";
inline constexpr char kMediaIndicatorsAndroidDescription[] =
    "Enables media indicators on Android.";

inline constexpr char kMediaPickerAdoptionStudyName[] =
    "Android Media Picker Adoption";
inline constexpr char kMediaPickerAdoptionStudyDescription[] =
    "Controls how to launch the Android Media Picker (note: This flag is "
    "ignored as of Android U)";

inline constexpr char kMigrateAccountManagerDelegateName[] =
    "Migrate Account Manager Delegate";
inline constexpr char kMigrateAccountManagerDelegateDescription[] =
    "Enables a refactoring of the Account Manager Delegate to use "
    "PlatformAccounts";

inline constexpr char kMigrateAccountPrefsOnMobileName[] =
    "Migrate account prefs on mobile";
inline constexpr char kMigrateAccountPrefsOnMobileDescription[] =
    "Migrate account prefs on Mobile to the single-json implementation.";

inline constexpr char kMiniOriginBarName[] = "Mini Origin Bar";
inline constexpr char kMiniOriginBarDescription[] =
    "Show a mini origin bar above the keyboard when focusing a form field. "
    "Applicable to bottom toolbar on Android only.";

inline constexpr char kNavBarColorAnimationName[] = "NavBarColorAnimation";
inline constexpr char kNavBarColorAnimationDescription[] =
    "Enables animations for color changes to the OS navigation bar.";

inline constexpr char kNavigationCaptureRefactorAndroidName[] =
    "Navigation Capture refactoring for Chrome on Android";
inline constexpr char kNavigationCaptureRefactorAndroidDescription[] =
    "Prevents UI jank when a navigation is 'captured', causing a new "
    "app to be opened.";

inline constexpr char kNotificationPermissionRationaleName[] =
    "Notification Permission Rationale UI";
inline constexpr char kNotificationPermissionRationaleDescription[] =
    "Configure the dialog shown before requesting notification permission. "
    "Only works with builds targeting Android T.";

inline constexpr char kNotificationPermissionRationaleBottomSheetName[] =
    "Notification Permission Rationale Bottom Sheet UI";
inline constexpr char kNotificationPermissionRationaleBottomSheetDescription[] =
    "Enable the alternative bottom sheet UI for the notification permission "
    "flow. "
    "Only works with builds targeting Android T+.";

inline constexpr char kOfflineAutoFetchName[] = "Offline Auto Fetch";
inline constexpr char kOfflineAutoFetchDescription[] =
    "Enables auto fetch of content when Chrome is online";

inline constexpr char kOmahaMinSdkVersionAndroidName[] =
    "Forces the minimum Android SDK version to a particular value.";
inline constexpr char kOmahaMinSdkVersionAndroidDescription[] =
    "When set, the minimum Android minimum SDK version is set to a particular "
    "value which impact the app menu badge, menu items, and settings about "
    "screen regarding whether Chrome can be updated.";
inline constexpr char kOmahaMinSdkVersionAndroidMinSdk1Description[] =
    "Minimum SDK = 1";
inline constexpr char kOmahaMinSdkVersionAndroidMinSdk1000Description[] =
    "Minimum SDK = 1000";

inline constexpr char kOmniboxAutofocusOnIncognitoNtpName[] =
    "Omnibox Autofocus on Incognito New Tab Page";
inline constexpr char kOmniboxAutofocusOnIncognitoNtpDescription[] =
    "Enables the Omnibox to automatically gain focus when the New "
    "Tab Page in Incognito mode is opened, allowing immediate typing.";

inline constexpr char kOmniboxShortcutsAndroidName[] =
    "Omnibox shortcuts on Android";
inline constexpr char kOmniboxShortcutsAndroidDescription[] =
    "Enables storing successful query/match in the omnibox shortcut database "
    "on Android";

inline constexpr char kRecentlyClosedTabsAndWindowsName[] =
    "Recently Closed Tabs And Windows";
inline constexpr char kRecentlyClosedTabsAndWindowsDescription[] =
    "Enables the new Recently Closed feature to restore both closed tabs and "
    "windows from the Recent Tabs surface, or via a keyboard shortcut.";

inline constexpr char kRefreshFeedOnRestartName[] =
    "Enable refreshing feed on restart";
inline constexpr char kRefreshFeedOnRestartDescription[] =
    "Refresh feed when Chrome restarts.";

inline constexpr char kPCCTMinimumHeightName[] =
    "Change the minimum height of pCCT to 30%.";
inline constexpr char kPCCTMinimumHeightDescription[] =
    "When enabled, this sets the minimum "
    "height to 30% or 220dp, whichever is greater, for ephemeral pCCTs.";

inline constexpr char kProcessRankPolicyAndroidName[] =
    "Enable performance manager rank policy for Android";
inline constexpr char kProcessRankPolicyAndroidDescription[] =
    "Enables performance manager ranking policy to update memory priority of "
    "renderer processes";

inline constexpr char kProtectedTabsAndroidName[] =
    "Enable protected tab for Android";
inline constexpr char kProtectedTabsAndroidDescription[] =
    "Ensures that renderer processes for protected tabs will be killed after "
    "other discard-eligible tabs. Requires #process-rank-policy-android to "
    "also be enabled";

inline constexpr char kReadAloudName[] = "Read Aloud";
inline constexpr char kReadAloudDescription[] =
    "Controls the Read Aloud feature";

inline constexpr char kReadAloudBackgroundPlaybackName[] =
    "Read Aloud Background Playback";
inline constexpr char kReadAloudBackgroundPlaybackDescription[] =
    "Controls background playback for the Read Aloud feature";

inline constexpr char kReadAloudInCCTName[] = "Read Aloud entrypoint in CCT";
inline constexpr char kReadAloudInCCTDescription[] =
    "Controls the Read Aloud entrypoint in the overflow menu for CCT";

inline constexpr char kReadAloudTapToSeekName[] = "Read Aloud Tap to Seek";
inline constexpr char kReadAloudTapToSeekDescription[] =
    "Controls the Read Aloud Tap to Seek feature";

inline constexpr char kReaderModeDistillInAppName[] =
    "Reader Mode distillation in app";
inline constexpr char kReaderModeDistillInAppDescription[] =
    "Distills the web page in brapp instead of a custom tab.";
inline constexpr char kReaderModeHeuristicsName[] = "Reader Mode triggering";
inline constexpr char kReaderModeHeuristicsDescription[] =
    "Determines what pages the Reader Mode infobar is shown on.";
inline constexpr char kReaderModeHeuristicsMarkup[] =
    "With article structured markup";
inline constexpr char kReaderModeHeuristicsAdaboost[] =
    "Non-mobile-friendly articles";
inline constexpr char kReaderModeHeuristicsAllArticles[] = "All articles";
inline constexpr char kReaderModeHeuristicsAlwaysOff[] = "Never";
inline constexpr char kReaderModeHeuristicsAlwaysOn[] = "Always";
inline constexpr char kReaderModeImprovementsName[] =
    "Reader Mode improvements";
inline constexpr char kReaderModeImprovementsDescription[] =
    "Collection of improvements to reader mode for android.";
inline constexpr char kReaderModeUseReadabilityName[] =
    "Reader Mode use readability";
inline constexpr char kReaderModeUseReadabilityDescription[] =
    "Use readability as the primary distiller and/or triggering mechanism.";

inline constexpr char kReengagementNotificationName[] =
    "Enable re-engagement notifications";
inline constexpr char kReengagementNotificationDescription[] =
    "Enables Chrome to use the in-product help system to decide when "
    "to show re-engagement notifications.";

inline constexpr char kRelatedSearchesAllLanguageName[] =
    "Enables all the languages for Related Searches on Android";
inline constexpr char kRelatedSearchesAllLanguageDescription[] =
    "Enables requesting related searches suggestions for all the languages.";

inline constexpr char kRelatedSearchesSwitchName[] =
    "Enables an experiment for Related Searches on Android";
inline constexpr char kRelatedSearchesSwitchDescription[] =
    "Enables requesting related searches suggestions.";

inline constexpr char kRightEdgeGoesForwardGestureNavName[] =
    "RightEdgeGoesForwardGestureNav";
inline constexpr char kRightEdgeGoesForwardGestureNavDescription[] =
    "Enables the right edge to navigate forward in OS gesture navigation mode.";

inline constexpr char
    kSafeBrowsingScamDetectionKeyboardLockTriggerAndroidName[] =
        "Scam Detection Keyboard Lock Trigger Android";
inline constexpr char
    kSafeBrowsingScamDetectionKeyboardLockTriggerAndroidDescription[] =
        "Enable the keyboard lock trigger of Scam Detection via command line "
        "for "
        "easier "
        "testing.";

inline constexpr char kSafeBrowsingSyncCheckerCheckAllowlistName[] =
    "Safe Browsing Sync Checker Check Allowlist";
inline constexpr char kSafeBrowsingSyncCheckerCheckAllowlistDescription[] =
    "Enables Safe Browsing sync checker to check the allowlist before checking "
    "the blocklist.";

inline constexpr char kSegmentationPlatformAndroidHomeModuleRankerV2Name[] =
    "Segmentation platform Android home module ranker V2";
inline constexpr char
    kSegmentationPlatformAndroidHomeModuleRankerV2Description[] =
        "Enable on-demand segmentation platform service to rank home modules "
        "on "
        "Android.";

inline constexpr char kSegmentationPlatformEphemeralCardRankerName[] =
    "Segmentation platform ephemeral card ranker";
inline constexpr char kSegmentationPlatformEphemeralCardRankerDescription[] =
    "Enable the Ephemeral Card ranker for the segmentation platform service "
    "to rank home modules on Android.";

inline constexpr char kSetMarketUrlForTestingName[] =
    "Set market URL for testing";
inline constexpr char kSetMarketUrlForTestingDescription[] =
    "When enabled, sets the market URL for use in testing the update menu "
    "item.";

inline constexpr char kShareCustomActionsInCCTName[] = "Custom Actions in CCT";
inline constexpr char kShareCustomActionsInCCTDescription[] =
    "Display share custom actions Chrome Custom Tabs.";

inline constexpr char kShowReadyToPayDebugInfoName[] =
    "Show debug information about IS_READY_TO_PAY intents";
inline constexpr char kShowReadyToPayDebugInfoDescription[] =
    "Display an alert dialog with the contents of IS_READY_TO_PAY intents "
    "that Chrome sends to Android payment applications: app's package name, "
    "service name, payment method name, and method specific data.";

inline constexpr char kShowTabListAnimationsName[] =
    "Show Tab List Animations (Android XR)";
inline constexpr char kShowTabListAnimationsDescription[] =
    "Shows animations for each tab on the tab switcher on Android XR.";

inline constexpr char kSiteIsolationForPasswordSitesName[] =
    "Site Isolation For Password Sites";
inline constexpr char kSiteIsolationForPasswordSitesDescription[] =
    "Security mode that enables site isolation for sites based on "
    "password-oriented heuristics, such as a user typing in a password.";

inline constexpr char kSpatialEntitesDepthHitTestName[] =
    "Spatial Entities Depth-based Hit Test";
inline constexpr char kSpatialEntitesDepthHitTestDescription[] =
    "Controls whether the spatial entities framework is allowed to use "
    "depth-based hit tests or only plane-based ones.";

inline constexpr char kSmartSuggestionForLargeDownloadsName[] =
    "Smart suggestion for large downloads";
inline constexpr char kSmartSuggestionForLargeDownloadsDescription[] =
    "Smart suggestion that offers download locations for large files.";

inline constexpr char kSmartZoomName[] = "Smart Zoom";
inline constexpr char kSmartZoomDescription[] =
    "Enable the Smart Zoom accessibility feature as an alternative approach "
    "to zooming web contents.";

inline constexpr char kSearchResumptionModuleAndroidName[] =
    "Search Resumption Module";
inline constexpr char kSearchResumptionModuleAndroidDescription[] =
    "Enable showing search suggestions on NTP";

inline constexpr char kStrictSiteIsolationName[] = "Strict site isolation";
inline constexpr char kStrictSiteIsolationDescription[] =
    "Security mode that enables site isolation for all sites (SitePerProcess). "
    "In this mode, each renderer process will contain pages from at most one "
    "site, using out-of-process iframes when needed. "
    "Check chrome://process-internals to see the current isolation mode. "
    "Setting this flag to 'Enabled' turns on site isolation regardless of the "
    "default. Here, 'Disabled' is a legacy value that actually means "
    "'Default,' in which case site isolation may be already enabled based on "
    "platform, enterprise policy, or field trial. See also "
    "#site-isolation-trial-opt-out for how to disable site isolation for "
    "testing.";

inline constexpr char kSubmenusInAppMenuName[] = "Enable submenus in App Menu";
inline constexpr char kSubmenusInAppMenuDescription[] =
    "Enables displaying submenus in the app menu, using drilldown or flyout "
    "depending on conditions.";

inline constexpr char kSubmenusTabContextMenuLffTabStripName[] =
    "Submenus in LFF Tab Context Menu on LFF Tab Strip";
inline constexpr char kSubmenusTabContextMenuLffTabStripDescription[] =
    "Enables submenus (for moving tabs to groups or windows) in the tab "
    "context menu on LFF tab strip";

inline constexpr char kSupervisedUserInterstitialWithoutApprovalsName[] =
    "Supervisded user interstitial without approvals for content filters";
inline constexpr char kSupervisedUserInterstitialWithoutApprovalsDescription[] =
    "Enables the supervised user interstitial without approvals nor custodian "
    "information for content filters. Strictly requires "
    "#propagate-device-content-filters-to-supervised-user to be enabled. "
    "Enabling #allow-non-family-link-url-filter-mode is also required for "
    "users who do not sign-in.";

inline constexpr char kTabClosureMethodRefactorName[] =
    "Tab closure method refactor";
inline constexpr char kTabClosureMethodRefactorDescription[] =
    "Enables the refactored changes for tab closure methods where existing "
    "methods usages are switched off and newly introduced are made active.";

inline constexpr char kTabStripDensityChangeAndroidName[] =
    "Tab Strip Density Change";
inline constexpr char kTabStripDensityChangeAndroidDescription[] =
    "Enables tab UI to switch to a denser layout when a peripheral(keyboard, "
    "mouse, touchpad, etc.) is connected, including reducing minimum tab "
    "width and button touch target to better support click-first interactions.";

inline constexpr char kTabStripGroupDragDropAndroidName[] =
    "Tab Strip Group Drag Drop Android";
inline constexpr char kTabStripGroupDragDropAndroidDescription[] =
    "Enables long-pressing on tab strip tab group indicators to start "
    "drag-and-drop. Users can drag the tab group off the tab strip and drop it "
    "into another window in split-screen mode or create a new window by "
    "dropping it on the edge of Chrome.";

inline constexpr char kTabStripIncognitoMigrationName[] =
    "Tab Strip Incognito switcher migration to toolbar";
inline constexpr char kTabStripIncognitoMigrationDescription[] =
    "Migrates tab strip incognito switcher to toolbar and adds options to tab "
    "switcher context menu.";

inline constexpr char kTabStripMouseCloseResizeDelayName[] =
    "Tab Strip Mouse Close Resize Delay";
inline constexpr char kTabStripMouseCloseResizeDelayDescription[] =
    "Delays resizing the tab strip when closing a tab with the mouse.";

inline constexpr char kToolbarSnapshotRefactorName[] =
    "Toolbar Snapshot Refactor";
inline constexpr char kToolbarSnapshotRefactorDescription[] =
    "Updates the margin and snapshotting of the Toolbar on Android.";

inline constexpr char kTrustedWebActivityContactsDelegationName[] =
    "TWA contact picker delegation";
inline constexpr char kTrustedWebActivityContactsDelegationDescription[] =
    "When enabled, contacts information requests for Trusted Web Activities "
    "will be delegated to the app.";

inline constexpr char kUpdateMenuBadgeName[] = "Force show update menu badge";
inline constexpr char kUpdateMenuBadgeDescription[] =
    "When enabled, a badge will be shown on the app menu button if the update "
    "type is Update Available or Unsupported OS Version.";

inline constexpr char kUpdateMenuItemCustomSummaryDescription[] =
    "When this flag and the force show update menu item flag are enabled, a "
    "custom summary string will be displayed below the update menu item.";
inline constexpr char kUpdateMenuItemCustomSummaryName[] =
    "Update menu item custom summary";

inline constexpr char kUpdateMenuTypeName[] =
    "Forces the update menu type to a specific type";
inline constexpr char kUpdateMenuTypeDescription[] =
    "When set, forces the update type to be a specific one, which impacts "
    "the app menu badge and menu item for updates.";
inline constexpr char kUpdateMenuTypeNone[] = "None";
inline constexpr char kUpdateMenuTypeUpdateAvailable[] = "Update Available";
inline constexpr char kUpdateMenuTypeUnsupportedOSVersion[] =
    "Unsupported OS Version";

inline constexpr char kUseAngleDescriptionAndroid[] =
    "Choose the graphics backend for ANGLE. The Vulkan backend is still "
    "experimental, and may contain bugs that "
    "are still being worked on.";

inline constexpr char kUseAngleGLES[] = "OpenGL ES";
inline constexpr char kUseAngleVulkan[] = "Vulkan";

inline constexpr char kUseHardwareBufferUsageFlagsFromVulkanName[] =
    "Use recommended AHardwareBuffer usage flags from Vulkan";
inline constexpr char kUseHardwareBufferUsageFlagsFromVulkanDescription[] =
    "Allows querying recommended AHardwareBuffer usage flags from Vulkan API";

inline constexpr char kWebFeedAwarenessName[] = "Web Feed Awareness";
inline constexpr char kWebFeedAwarenessDescription[] =
    "Helps the user discover the web feed.";

inline constexpr char kWebFeedDeprecationName[] = "Web feed deprecation";
inline constexpr char kWebFeedDeprecationDescription[] =
    "Deprecate the web feed.";

inline constexpr char kWebFeedOnboardingName[] = "Web Feed Onboarding";
inline constexpr char kWebFeedOnboardingDescription[] =
    "Helps the user understand how to use the web feed.";

inline constexpr char kWebFeedSortName[] = "Web Feed Sort";
inline constexpr char kWebFeedSortDescription[] =
    "Allows users to sort their web content in the web feed. "
    "Only works if Web Feed is also enabled.";

inline constexpr char kWebSerialWiredDevicesAndroidName[] =
    "Web Serial API for Wired Devices";
inline constexpr char kWebSerialWiredDevicesAndroidDescription[] =
    "Provides a way for websites to interact with wired serial devices";

inline constexpr char kXplatSyncedSetupName[] = "Cross-platform synced setup";
inline constexpr char kXplatSyncedSetupDescription[] =
    "Enables the Cross-platform synced setup feature.";

inline constexpr char kXsurfaceMetricsReportingName[] =
    "Xsurface Metrics Reporting";
inline constexpr char kXsurfaceMetricsReportingDescription[] =
    "Allows metrics reporting state to be passed to Xsurface";

inline constexpr char kOpenXRExtendedFeaturesName[] =
    "WebXR OpenXR Runtime Extended Features";
inline constexpr char kOpenXRExtendedFeaturesDescription[] =
    "Enables the use of the OpenXR runtime to create WebXR sessions with a "
    "broader feature set (e.g. features not currently supported on Desktop).";

inline constexpr char kOpenXRName[] = "Enable OpenXR WebXR Runtime";
inline constexpr char kOpenXRDescription[] =
    "Enables the use of the OpenXR runtime to create WebXR sessions.";

inline constexpr char kOpenXRAndroidSmoothDepthName[] =
    "Enable OpenXR Smooth Depth";
inline constexpr char kOpenXRAndroidSmoothDepthDescription[] =
    "Forces the OpenXR Android runtime to use the Smooth depth image. When "
    "Disabled, the raw depth image will be used instead.";

// FLAG_DESCRIPTIONS_ANDROID_END
// Non-Android -----------------------------------------------------------------

inline constexpr char kAccountStoragePrefsThemesAndSearchEnginesName[] =
    "Account storage of preferences, themes and search engines";
inline constexpr char kAccountStoragePrefsThemesAndSearchEnginesDescription[] =
    "When enabled, keeps account preferences, themes and search-engines "
    "separate from the local data. If the user signs out or sync is turned "
    "off, only the account data is removed while the pre-existing/local data "
    "is left behind.";

inline constexpr char kAllowAllSitesToInitiateMirroringName[] =
    "Allow all sites to initiate mirroring";
inline constexpr char kAllowAllSitesToInitiateMirroringDescription[] =
    "When enabled, allows all websites to request to initiate tab mirroring "
    "via Presentation API. Requires #cast-media-route-provider to also be "
    "enabled";

inline constexpr char kAXTreeFixingName[] = "AXTree Fixing";
inline constexpr char kAXTreeFixingDescription[] =
    "When enabled, allows Chrome to dynamically fix the AXTree of sites. This "
    "is experimental and may cause breaking changes to users of assistive "
    "technology.";

inline constexpr char kBrowserInitiatedAutomaticPictureInPictureName[] =
    "Browser initiated automatic picture in picture";
inline constexpr char kBrowserInitiatedAutomaticPictureInPictureDescription[] =
    "When enabled, allows the browser to automatically enter picture in "
    "picture when a series of conditions are met.";

inline constexpr char kCreateNewTabGroupAppMenuTopLevelName[] =
    "Create new tab group menu option at the top level of the app menu";
inline constexpr char kCreateNewTabGroupAppMenuTopLevelDescription[] =
    "In the app menu, add an option to create a new tab group at the top level";

inline constexpr char kDialMediaRouteProviderName[] =
    "Allow cast device discovery with DIAL protocol";
inline constexpr char kDialMediaRouteProviderDescription[] =
    "Enable/Disable the browser discovery of the DIAL support cast device."
    "It sends a discovery SSDP message every 120 seconds";

inline constexpr char kPictureInPictureShowWindowAnimationName[] =
    "Picture-in-Picture show window animation";
inline constexpr char kPictureInPictureShowWindowAnimationDescription[] =
    "When enabled, Picture-in-Picture windows will use a fade-in show "
    "animation. On Windows OS this is a no-op.";

inline constexpr char kCastMirroringTargetPlayoutDelayName[] =
    "Changes the target playout delay for Cast mirroring.";
inline constexpr char kCastMirroringTargetPlayoutDelayDescription[] =
    "Choose a target playout delay for Cast mirroring. A lower delay will "
    "decrease latency, but may impact other quality indicators.";
inline constexpr char kCastMirroringTargetPlayoutDelayDefault[] =
    "Default (200ms)";
inline constexpr char kCastMirroringTargetPlayoutDelay100ms[] = "100ms.";
inline constexpr char kCastMirroringTargetPlayoutDelay150ms[] = "150ms.";
inline constexpr char kCastMirroringTargetPlayoutDelay250ms[] = "250ms.";
inline constexpr char kCastMirroringTargetPlayoutDelay300ms[] = "300ms.";
inline constexpr char kCastMirroringTargetPlayoutDelay350ms[] = "350ms.";
inline constexpr char kCastMirroringTargetPlayoutDelay400ms[] = "400ms.";

inline constexpr char kEnableHeadlessLiveCaptionName[] =
    "Headless Live Captions";
inline constexpr char kEnableHeadlessLiveCaptionDescription[] =
    "Enable features related to headless captions exploration. These are "
    "very likely unstable.";

inline constexpr char kEnableMediaLinkHelpersName[] = "Media Link Helpers";
inline constexpr char kEnableMediaLinkHelpersDescription[] =
    "Enable customized per-site media link processing.";

inline constexpr char kEnableCrOSLiveTranslateName[] = "Live Translate CrOS";
inline constexpr char kEnableCrOSLiveTranslateDescription[] =
    "Enables the live translate feature on ChromeOS which allows for live "
    "translation of captions into a target language.";

inline constexpr char kEnableCrOSSodaConchLanguagesName[] =
    "SODA Conch Languages.";
inline constexpr char kEnableCrOSSodaConchLanguagesDescription[] =
    "Enable Conch specific SODA language models.";

inline constexpr char kFreezingOnEnergySaverName[] =
    "Freeze CPU intensive background tabs on Energy Saver";
inline constexpr char kFreezingOnEnergySaverDescription[] =
    "When Energy Saver is active, freeze eligible background tabs that use a "
    "lot of CPU. A tab is eligible if it's silent, doesn't provide audio- or "
    "video- conference functionality and doesn't use WebUSB or Web Bluetooth.";

inline constexpr char kFreezingOnEnergySaverTestingName[] =
    "Freeze CPU intensive background tabs on Energy Saver - Testing Mode";
inline constexpr char kFreezingOnEnergySaverTestingDescription[] =
    "Similar to #freezing-on-energy-saver, with changes to facilitate testing: "
    "1) pretend that Energy Saver is active even when it's not and 2) pretend "
    "that all tabs use a lot of CPU.";

inline constexpr char kImprovedPasswordChangeServiceName[] =
    "Improved password change service";
inline constexpr char kImprovedPasswordChangeServiceDescription[] =
    "Experimental feature, which offers automatic password change to the user "
    "when they sign in with a credential known to be leaked.";

inline constexpr char kInfiniteTabsFreezingName[] = "Infinite Tabs Freezing";
inline constexpr char kInfiniteTabsFreezingDescription[] =
    "Freezes eligible tabs which are not in the 5 most recently used ones, to "
    "preserve Chrome speed as new tabs are created. Tabs providing background "
    "functionality (e.g. playing audio, handling a video call) are not "
    "eligible for freezing.";

inline constexpr char kMemoryPurgeOnFreezeLimitName[] =
    "Memory Purge on Freeze Limit";
inline constexpr char kMemoryPurgeOnFreezeLimitDescription[] =
    "Do not purge memory in renderers with frozen pages more than once per "
    "backgrounded interval, to minimize overhead when pages are periodically "
    "unfrozen. To be enabled with memory-purge-on-freeze-limit.";

inline constexpr char kReadAnythingImagesViaAlgorithmName[] =
    "Reading Mode with images added via algorithm";
inline constexpr char kReadAnythingImagesViaAlgorithmDescription[] =
    "Have Reading Mode use a local rules based algorithm to include images "
    "from webpages.";

inline constexpr char kReadAnythingImmersiveReadingModeName[] =
    "Reading Mode Experimental Immersive Mode";
inline constexpr char kReadAnythingImmersiveReadingModeDescription[] =
    "Enables the infrastructure for Immersive Reading Mode. No visual "
    "changes currently included.";

inline constexpr char kReadAnythingReadAloudName[] = "Reading Mode Read Aloud";
inline constexpr char kReadAnythingReadAloudDescription[] =
    "Enables the experimental Read Aloud feature in Reading Mode.";

inline constexpr char kReadAnythingReadAloudTsTextSegmentationName[] =
    "Reading Mode Read Aloud Experimental Text Segmentation";
inline constexpr char kReadAnythingReadAloudTsTextSegmentationDescription[] =
    "Enables the experimental text segmentation method for reading "
    "mode.";

inline constexpr char kReadAnythingOmniboxChipName[] =
    "Reading Mode Omnibox Chip";
inline constexpr char kReadAnythingOmniboxChipDescription[] =
    "Enables the omnibox chip entry point for Reading mode";

inline constexpr char kReadAnythingReadAloudPhraseHighlightingName[] =
    "Reading Mode Read Aloud Phrase Highlighting";
inline constexpr char kReadAnythingReadAloudPhraseHighlightingDescription[] =
    "Enables the experimental Reading Mode feature that highlights by phrases "
    "when reading aloud, when the phrase option is selected from the highlight "
    "menu.";

inline constexpr char kReadAnythingDocsIntegrationName[] =
    "Reading Mode Google Docs Integration";
inline constexpr char kReadAnythingDocsIntegrationDescription[] =
    "Allows Reading Mode to work on Google Docs.";

inline constexpr char kReadAnythingDocsLoadMoreButtonName[] =
    "Reading Mode Google Docs Load More Button";
inline constexpr char kReadAnythingDocsLoadMoreButtonDescription[] =
    "Adds a button to the end of the Reading Mode UI. When clicked, "
    "the main page scrolls to show the next page's content.";

inline constexpr char kReadAnythingWithReadabilityName[] =
    "Reading Mode Experimental Webpage Distilation";
inline constexpr char kReadAnythingWithReadabilityDescription[] =
    "Enables the experimental text webpage distillation using readability.js "
    "method for reading mode.";

inline constexpr char kLinkPreviewName[] = "Link Preview";
inline constexpr char kLinkPreviewDescription[] =
    "When enabled, Link Preview feature gets to be available to preview a "
    "linked page in a dedicated small window before navigating to the linked "
    "page. The feature can be triggered from a context menu item, or users' "
    "actions. We are evaluating multiple actions in our experiment to "
    "understand what's to be the best for users from the viewpoint of "
    "security, privacy, and usability. The feature might be unstable and "
    "unusable on some platforms, e.g. macOS or touch devices.";

inline constexpr char kMarkAllCredentialsAsLeakedName[] =
    "Mark all credential as leaked";
inline constexpr char kMarkAllCredentialsAsLeakedDescription[] =
    "Will pop up the leaked check dialog on every password form submission. "
    "This should be used "
    "in combination with #improved-password-change-service to better test the "
    "improved password change service";

inline constexpr char kMuteNotificationSnoozeActionName[] =
    "Snooze action for mute notifications";
inline constexpr char kMuteNotificationSnoozeActionDescription[] =
    "Adds a Snooze action to mute notifications shown while sharing a screen.";

inline constexpr char kNtpAlphaBackgroundCollectionsName[] =
    "NTP Alpha Background Collections";
inline constexpr char kNtpAlphaBackgroundCollectionsDescription[] =
    "Shows alpha NTP background collections in Customize Chrome.";

inline constexpr char kNtpBackgroundImageErrorDetectionName[] =
    "NTP Background Image Error Detection";
inline constexpr char kNtpBackgroundImageErrorDetectionDescription[] =
    "Checks NTP background image links for HTTP status errors.";

inline constexpr char kNtpCalendarModuleName[] = "NTP Calendar Module";
inline constexpr char kNtpCalendarModuleDescription[] =
    "Shows the Google Calendar module on the New Tab Page.";

inline constexpr char kNtpComposeboxName[] = "NTP Composebox";
inline constexpr char kNtpComposeboxDescription[] =
    "Shows the Composebox on the New Tab Page Searchbox upon clicking the "
    "entrypoint.";

inline constexpr char kNtpCustomizeChromeAutoOpenName[] =
    "NTP Customize Chrome Auto Promo";
inline constexpr char kNtpCustomizeChromeAutoOpenDescription[] =
    "Shows the Customize Chrome on the New Tab Page automatically.";

inline constexpr char kNtpRealboxNextName[] = "NTP Realbox Next";
inline constexpr char kNtpRealboxNextDescription[] =
    "Enables the Realbox 'Next' experience.";

inline constexpr char kNtpDriveModuleName[] = "NTP Drive Module";
inline constexpr char kNtpDriveModuleDescription[] =
    "Shows the Google Drive module on the New Tab Page";

inline constexpr char kNtpDriveModuleSegmentationName[] =
    "NTP Drive Module Segmentation";
inline constexpr char kNtpDriveModuleSegmentationDescription[] =
    "Uses segmentation data to decide whether to show the Drive module on the "
    "New Tab Page.";

inline constexpr char kNtpDriveModuleShowSixFilesName[] =
    "NTP Drive Module Show Six Files";
inline constexpr char kNtpDriveModuleShowSixFilesDescription[] =
    "Shows six files in the NTP Drive module, instead of three.";

inline constexpr char kNtpDummyModulesName[] = "NTP Dummy Modules";
inline constexpr char kNtpDummyModulesDescription[] =
    "Adds dummy modules to New Tab Page when 'NTP Modules Redesigned' is "
    "enabled.";

inline constexpr char kNtpFeatureOptimizationDismissModulesRemovalName[] =
    "NTP Feature Optimization Dismiss Modules Removal";
inline constexpr char
    kNtpFeatureOptimizationDismissModulesRemovalDescription[] =
        "Removes the dismiss module buttons from the NTP modules.";

inline constexpr char kNtpFeatureOptimizationModuleRemovalName[] =
    "NTP Feature Optimization Module Removal";
inline constexpr char kNtpFeatureOptimizationModuleRemovalDescription[] =
    "Enables auto-removal of stale modules from the NTP.";

inline constexpr char kNtpFeatureOptimizationShortcutsRemovalName[] =
    "NTP Feature Optimization Shortcuts Removal";
inline constexpr char kNtpFeatureOptimizationShortcutsRemovalDescription[] =
    "Enables auto-removal of stale shortcuts from the NTP.";

inline constexpr char kNtpFooterName[] = "NTP Footer";
inline constexpr char kNtpFooterDescription[] =
    "Adds footer to New Tab Page that encapsulates customize buttons and "
    "background/theme attributions.";

inline constexpr char kNtpMicrosoftAuthenticationModuleName[] =
    "NTP Microsoft Authentication Module";
inline constexpr char kNtpMicrosoftAuthenticationModuleDescription[] =
    "Shows the Microsoft Authentication Module on the New Tab Page.";

inline constexpr char kNtpMostRelevantTabResumptionModuleName[] =
    "NTP Most Relevant Tab Resumption Module";
inline constexpr char kNtpMostRelevantTabResumptionModuleDescription[] =
    "Shows the Most Relevant Tab Resumption Module on the New Tab Page.";

inline constexpr char kNtpMostRelevantTabResumptionModuleFallbackToHostName[] =
    "NTP Most Relevant Tab Resumption Module uses fallback to host for favicon";
inline constexpr char
    kNtpMostRelevantTabResumptionModuleFallbackToHostDescription[] =
        "Shows the host fallback icon instead of server fallback on Most "
        "Relevant "
        "Tab Resumption Module on the New Tab Page.";

inline constexpr char kNtpMiddleSlotPromoDismissalName[] =
    "NTP Middle Slot Promo Dismissal";
inline constexpr char kNtpMiddleSlotPromoDismissalDescription[] =
    "Allows middle slot promo to be dismissed from New Tab Page until "
    "new promo message is populated.";

inline constexpr char kNtpMobilePromoName[] = "NTP Mobile Promo";
inline constexpr char kNtpMobilePromoDescription[] =
    "Shows a promo for installing on mobile to the New Tab Page.";

inline constexpr char kForceNtpMobilePromoName[] = "Force NTP Mobile Promo";
inline constexpr char kForceNtpMobilePromoDescription[] =
    "Forces a promo for installing on mobile to the New Tab Page to show "
    "without preconditions.";

inline constexpr char kNtpModulesDragAndDropName[] =
    "NTP Modules Drag and Drop";
inline constexpr char kNtpModulesDragAndDropDescription[] =
    "Enables modules to be reordered via dragging and dropping on the "
    "New Tab Page.";

inline constexpr char kNtpModuleSignInRequirementName[] =
    "NTP Modules Sign-in Requirement";
inline constexpr char kNtpModuleSignInRequirementDescription[] =
    "Makes NTP Sign-in Requirement per module, removing the requirement for "
    "Microsoft Modules";

inline constexpr char kNtpNextFeaturesName[] = "NTP Next Features";
inline constexpr char kNtpNextFeaturesDescription[] =
    "Enables features (e.g., AI action chips) in NTP Next";

inline constexpr char kNtpOneGoogleBarAsyncBarPartsName[] =
    "NTP OneGoogleBar Async Bar Parts";
inline constexpr char kNtpOneGoogleBarAsyncBarPartsDescription[] =
    "Enables the OneGoogleBar async bar parts API on the New Tab Page.";

inline constexpr char kNtpOutlookCalendarModuleName[] =
    "NTP Outlook Calendar Module";
inline constexpr char kNtpOutlookCalendarModuleDescription[] =
    "Shows the Outlook Calendar module on the New Tab Page.";

inline constexpr char kNtpRealboxContextualAndTrendingSuggestionsName[] =
    "NTP Realbox Contextual and Trending Suggestions";
inline constexpr char kNtpRealboxContextualAndTrendingSuggestionsDescription[] =
    "Allows NTP Realbox's second column to display contextual and trending "
    "text suggestions.";

inline constexpr char kNtpRealboxCr23ThemingName[] =
    "Chrome Refresh Themed Realbox";
inline constexpr char kNtpRealboxCr23ThemingDescription[] =
    "CR23 theming will be applied in Realbox when enabled.";

inline constexpr char kNtpRealboxMatchSearchboxThemeName[] =
    "NTP Realbox Matches Searchbox Theme";
inline constexpr char kNtpRealboxMatchSearchboxThemeDescription[] =
    "Makes NTP Realbox drop shadow match that of the Searchbox when enabled.";

inline constexpr char kNtpRealboxUseGoogleGIconName[] =
    "NTP Realbox Google G Icon";
inline constexpr char kNtpRealboxUseGoogleGIconDescription[] =
    "Shows Google G icon "
    "instead of Search Loupe in realbox when enabled";

inline constexpr char kNtpSafeBrowsingModuleName[] = "NTP Safe Browsing Module";
inline constexpr char kNtpSafeBrowsingModuleDescription[] =
    "Shows the safe browsing module on the New Tab Page.";

inline constexpr char kNtpSharepointModuleName[] = "NTP Sharepoint Module";
inline constexpr char kNtpSharepointModuleDescription[] =
    "Shows the Sharepoint module on the New Tab Page.";

inline constexpr char kNtpTabGroupsModuleName[] = "NTP Tab Groups Module";
inline constexpr char kNtpTabGroupsModuleDescription[] =
    "Shows the Tab Groups module on the New Tab Page.";

inline constexpr char kNtpTabGroupsModuleZeroStateName[] =
    "NTP Tab Groups Zero State Card";
inline constexpr char kNtpTabGroupsModuleZeroStateDescription[] =
    "Enables the zero-state card for the Tab Groups Module on the New Tab "
    "Page.";

inline constexpr char kNtpWallpaperSearchButtonName[] =
    "NTP Wallpaper Search Button";
inline constexpr char kNtpWallpaperSearchButtonDescription[] =
    "Enables entry point on New Tab Page for Customize Chrome Side Panel "
    "Wallpaper Search.";

inline constexpr char kNtpWallpaperSearchButtonAnimationName[] =
    "NTP Wallpaper Search Button Animation";
inline constexpr char kNtpWallpaperSearchButtonAnimationDescription[] =
    "Enables animation for New Tab Page's Wallpaper Search button. Requires "
    "#ntp-wallpaper-search-button to be enabled too.";

inline constexpr char kHappinessTrackingSurveysForDesktopDemoName[] =
    "Happiness Tracking Surveys Demo";
inline constexpr char kHappinessTrackingSurveysForDesktopDemoDescription[] =
    "Enable showing Happiness Tracking Surveys Demo to users on Desktop";

inline constexpr char kMainNodeAnnotationsName[] = "Main Node Annotations";
inline constexpr char kMainNodeAnnotationsDescription[] =
    "Uses Screen2x main content extractor to annotate the accessibility tree "
    "with the main landmark on the node identified as main.";

inline constexpr char kOmniboxDriveSuggestionsNoSyncRequirementName[] =
    "Omnibox Google Drive Document suggestions don't require Chrome Sync";
inline constexpr char kOmniboxDriveSuggestionsNoSyncRequirementDescription[] =
    "Omnibox Drive suggestions don't require the user to have enabled Chrome "
    "Sync and are available when all other requirements are met.";

inline constexpr char kSavePasswordsContextualUiName[] =
    "Save Password Contextual UI";
inline constexpr char kSavePasswordsContextualUiDescription[] =
    "Improved page action indicator and dialog UI when the user has "
    "blocklisted the current site for password saving.";

inline constexpr char kSCTAuditingName[] = "SCT auditing";
inline constexpr char kSCTAuditingDescription[] =
    "Enables SCT auditing for users who have opted in to Safe Browsing "
    "Extended Reporting.";

inline constexpr char kSmartCardWebApiName[] = "Smart Card API";
inline constexpr char kSmartCardWebApiDescription[] =
    "Enable access to the Smart Card API. See "
    "https://github.com/WICG/web-smart-card#readme for more information.";

inline constexpr char kTabCaptureInfobarLinksName[] =
    "Navigation links in the tab-sharing bar";
inline constexpr char kTabCaptureInfobarLinksDescription[] =
    "Enables quick-navigation links to the captured and capturing tab in the "
    "tab-sharing bar.";

inline constexpr char kTranslateOpenSettingsName[] = "Translate Open Settings";
inline constexpr char kTranslateOpenSettingsDescription[] =
    "Add an option to the translate bubble menu to open language settings.";

inline constexpr char kWebAuthenticationPermitEnterpriseAttestationName[] =
    "Web Authentication Enterprise Attestation";
inline constexpr char
    kWebAuthenticationPermitEnterpriseAttestationDescription[] =
        "Permit a set of origins to request a uniquely identifying enterprise "
        "attestation statement from a security key when creating a Web "
        "Authentication credential.";

// Windows ---------------------------------------------------------------------

inline constexpr char kCalculateNativeWinOcclusionName[] =
    "Calculate window occlusion on Windows";
inline constexpr char kCalculateNativeWinOcclusionDescription[] =
    "Calculate window occlusion on Windows will be used in the future "
    "to throttle and potentially unload foreground tabs in occluded windows";

inline constexpr char kEnableMediaFoundationVideoCaptureName[] =
    "MediaFoundation Video Capture";
inline constexpr char kEnableMediaFoundationVideoCaptureDescription[] =
    "Enable/Disable the usage of MediaFoundation for video capture. Fall back "
    "to DirectShow if disabled.";

inline constexpr char kHardwareSecureDecryptionName[] =
    "Hardware Secure Decryption";
inline constexpr char kHardwareSecureDecryptionDescription[] =
    "Enable/Disable the use of hardware secure Content Decryption Module (CDM) "
    "for protected content playback.";

inline constexpr char kHidGetFeatureReportFixName[] =
    "Adjust feature reports received with WebHID";
inline constexpr char kHidGetFeatureReportFixDescription[] =
    "Enable/Disable a fix for a bug that caused feature reports to be offset "
    "by one byte when received from devices that do not use numbered reports.";

inline constexpr char kHardwareSecureDecryptionExperimentName[] =
    "Hardware Secure Decryption Experiment";
inline constexpr char kHardwareSecureDecryptionExperimentDescription[] =
    "Enable/Disable the use of hardware secure Content Decryption Module (CDM) "
    "for experimental protected content playback.";

inline constexpr char kHardwareSecureDecryptionFallbackName[] =
    "Hardware Secure Decryption Fallback";
inline constexpr char kHardwareSecureDecryptionFallbackDescription[] =
    "Allows automatically disabling hardware secure Content Decryption Module "
    "(CDM) after failures or crashes. Subsequent playback may use software "
    "secure CDMs. If this feature is disabled, the fallback will never happen "
    "and users could be stuck with playback failures.";

inline constexpr char kMediaFoundationCameraUsageMonitoringName[] =
    "Media Foundation Camera Usage Monitoring";
inline constexpr char kMediaFoundationCameraUsageMonitoringDescription[] =
    "Enables the use of Media Foundation for camera usage monitoring. "
    "This allows detecting if a camera is being used by another application.";

inline constexpr char kUseAngleDescriptionWindows[] =
    "Choose the graphics backend for ANGLE. D3D11 is used on most Windows "
    "computers by default.";

inline constexpr char kUseAngleD3D11[] = "D3D11";
inline constexpr char kUseAngleD3D9[] = "D3D9";
inline constexpr char kUseAngleD3D11Warp[] = "D3D11 WARP";

inline constexpr char kUseWaitableSwapChainName[] = "Use waitable swap chains";
inline constexpr char kUseWaitableSwapChainDescription[] =
    "Use waitable swap chains to reduce presentation latency (effective only "
    "Windows 8.1 or later). If enabled, specify the maximum number of frames "
    "that can be queued, ranging from 1-3. 1 has the lowest delay but is most "
    "likely to drop frames, while 3 has the highest delay but is least likely "
    "to drop frames.";

inline constexpr char kAndroidWebAppHeaderForStandaloneModeName[] =
    "Use Web App Header for Standalone mode";
inline constexpr char kAndroidWebAppHeaderForStandaloneModeDescription[] =
    "For Trusted Web Apps (TWAs), use Web App Header for Standalone mode";

inline constexpr char kUseWinrtMidiApiName[] = "Use Windows Runtime MIDI API";
inline constexpr char kUseWinrtMidiApiDescription[] =
    "Use Windows Runtime MIDI API for WebMIDI (effective only on Windows 10 or "
    "later).";

inline constexpr char kWebRtcAllowWgcScreenCapturerName[] =
    "Use Windows WGC API for screen capture";
inline constexpr char kWebRtcAllowWgcScreenCapturerDescription[] =
    "Use Windows.Graphics.Capture API based screen capturer in combination "
    "with the WebRTC based Web API getDisplayMedia. Requires  Windows 10, "
    "version 1803 or higher. Adds a thin yellow border around the captured "
    "screen area. The DXGI API is used as screen capture API when this flag is "
    "disabled.";

inline constexpr char kWebRtcWgcRequireBorderName[] =
    "Border around WGC captures";
inline constexpr char kWebRtcWgcRequireBorderDescription[] =
    "When using WGC to capture a window or a screen, draw a border around the "
    "captured surface.";

inline constexpr char kWindows11MicaTitlebarName[] = "Windows 11 Mica titlebar";
inline constexpr char kWindows11MicaTitlebarDescription[] =
    "Use the DWM system-drawn Mica titlebar on Windows 11, version 22H2 (build "
    "22621) and above.";

inline constexpr char kWindowsSystemTracingName[] = "System tracing";
inline constexpr char kWindowsSystemTracingDescription[] =
    "When enabled, the system tracing service is started along with Chrome's "
    "tracing service (if the system tracing service is registered).";

inline constexpr char kPrintWithPostScriptType42FontsName[] =
    "Print with PostScript Type 42 fonts";
inline constexpr char kPrintWithPostScriptType42FontsDescription[] =
    "When using PostScript level 3 printing, render text with Type 42 fonts if "
    "possible.";

inline constexpr char kPrintWithReducedRasterizationName[] =
    "Print with reduced rasterization";
inline constexpr char kPrintWithReducedRasterizationDescription[] =
    "When using GDI printing, avoid rasterization if possible.";

inline constexpr char kReadPrinterCapabilitiesWithXpsName[] =
    "Read printer capabilities with XPS";
inline constexpr char kReadPrinterCapabilitiesWithXpsDescription[] =
    "When enabled, utilize XPS interface to read printer capabilities.";

inline constexpr char kUseXpsForPrintingName[] = "Use XPS for printing";
inline constexpr char kUseXpsForPrintingDescription[] =
    "When enabled, use XPS printing API instead of the GDI print API.";

inline constexpr char kUseXpsForPrintingFromPdfName[] =
    "Use XPS for printing from PDF";
inline constexpr char kUseXpsForPrintingFromPdfDescription[] =
    "When enabled, use XPS printing API instead of the GDI print API when "
    "printing PDF documents.";

// Mac -------------------------------------------------------------------------

inline constexpr char kImmersiveFullscreenName[] =
    "Immersive Fullscreen Toolbar";
inline constexpr char kImmersiveFullscreenDescription[] =
    "Automatically hide and show the toolbar in fullscreen.";

inline constexpr char kMacAccessibilityAPIMigrationName[] =
    "Mac A11y API Migration";
inline constexpr char kMacAccessibilityAPIMigrationDescription[] =
    "Enables the migration to the new Cocoa accessibility API.";

inline constexpr char kMacCatapLoopbackAudioForCastName[] =
    "Mac Core Audio Tap System Loopback Capture for Cast";
inline constexpr char kMacCatapLoopbackAudioForCastDescription[] =
    "Enable system audio loopback capture for Cast using the macOS CoreAudio "
    "tap API on macOS 14.2+.";

inline constexpr char kMacCatapLoopbackAudioForScreenShareName[] =
    "Mac Core Audio Tap System Loopback Capture for Screen Sharing";
inline constexpr char kMacCatapLoopbackAudioForScreenShareDescription[] =
    "Enable system audio loopback capture for screen share using the macOS "
    "CoreAudio tap API on macOS 14.2+.";

inline constexpr char kMacPWAsNotificationAttributionName[] =
    "Mac PWA notification attribution";
inline constexpr char kMacPWAsNotificationAttributionDescription[] =
    "Route notifications for PWAs on Mac through the app shim, attributing "
    "notifications to the correct apps.";

inline constexpr char kRetryGetVideoCaptureDeviceInfosName[] =
    "Retry capture device enumeration on crash";
inline constexpr char kRetryGetVideoCaptureDeviceInfosDescription[] =
    "Enables retries when enumerating the available video capture devices "
    "after a crash. The capture service is restarted without loading external "
    "DAL plugins which could have caused the crash.";

inline constexpr char kUnexportableKeyDeletionName[] =
    "Enable Unexportable Key Deletion";
inline constexpr char kUnexportableKeyDeletionDescription[] =
    "Enables the garbage collection and deletion of obsolete cryptographic "
    "keys used for Device Bound Session Credentials.";

inline constexpr char kUseAdHocSigningForWebAppShimsName[] =
    "Use Ad-hoc Signing for Web App Shims";
inline constexpr char kUseAdHocSigningForWebAppShimsDescription[] =
    "Ad-hoc code signing ensures that each PWA app shim has a unique identity. "
    "This allows macOS subsystems to correctly distinguish between multiple "
    "PWAs.";

inline constexpr char kUseSCContentSharingPickerName[] =
    "Use ScreenCaptureKit picker for stream selection";
inline constexpr char kUseSCContentSharingPickerDescription[] =
    "This feature opens a native picker in macOS 15+ to allow the selection "
    "of a window or screen that will be captured.";

inline constexpr char kBlockRootWindowAccessibleNameChangeEventName[] =
    "Block Root Window Accessible Name Change Event";
inline constexpr char kBlockRootWindowAccessibleNameChangeEventDescription[] =
    "This feature prevents the firing of accessible name change events on the "
    "Root Window of MacOS applications. By blocking these events, it ensures "
    "that changes to the accessible name of Root Window do not trigger "
    "notifications to assistive technologies. This can be useful in scenarios "
    "where frequent or unnecessary name change events could lead to "
    "performance issues or unwanted behavior in assistive applications.";

// Windows and Mac -------------------------------------------------------------

inline constexpr char kLocationProviderManagerName[] =
    "Enable location provider manager for Geolocation API";
inline constexpr char kLocationProviderManagerDescription[] =
    "Enables usage of the location provider manager to select between "
    "the operating system's location API or the network-based provider "
    "as the data source for Geolocation API.";

// Windows, Mac and Android  --------------------------------------------------

inline constexpr char kUseAngleName[] = "Choose ANGLE graphics backend";

inline constexpr char kUseAngleDefault[] = "Default";

// ChromeOS -------------------------------------------------------------------

inline constexpr char kAcceleratedMjpegDecodeName[] =
    "Hardware-accelerated mjpeg decode for captured frame";
inline constexpr char kAcceleratedMjpegDecodeDescription[] =
    "Enable hardware-accelerated MJPEG decode for captured frame where "
    "available.";

inline constexpr char kAllowApnModificationPolicyName[] =
    "Allow APN Modification by Policy";
inline constexpr char kAllowApnModificationPolicyDescription[] =
    "Enables the ChromeOS APN Allow APN Modification policy, which gives "
    "admins the ability to allow or prohibit managed users from modifying "
    "APNs.";

inline constexpr char kAllowCrossDeviceFeatureSuiteName[] =
    "Allow the use of Cross-Device features";
inline constexpr char kAllowCrossDeviceFeatureSuiteDescription[] =
    "Allow features such as Nearby Share, PhoneHub, Fast Pair, and Smart Lock, "
    "that require communication with a nearby device. This should be enabled "
    "by default on most platforms, and only disabled in cases where we cannot "
    "guarantee a good experience with the stock Bluetooth hardware (e.g. "
    "ChromeOS Flex). If disabled, this removes all Cross-Device features and "
    "their entries in the Settings app.";

inline constexpr char kLinkCrossDeviceInternalsName[] =
    "Link Cross-Device internals logging to Feedback reports.";
inline constexpr char kLinkCrossDeviceInternalsDescription[] =
    "Improves debugging of Cross-Device features by recording more verbose "
    "logs and attaching these logs to filed Feedback reports.";

inline constexpr char kAltClickAndSixPackCustomizationName[] =
    "Allow users to customize Alt-Click and 6-pack key remapping.";

inline constexpr char kAltClickAndSixPackCustomizationDescription[] =
    "Shows settings to customize Alt-Click and 6-pack key remapping in the "
    "keyboard settings page.";

inline constexpr char kAlwaysEnableHdcpName[] =
    "Always enable HDCP for external displays";
inline constexpr char kAlwaysEnableHdcpDescription[] =
    "Enables the specified type for HDCP whenever an external display is "
    "connected. By default, HDCP is only enabled when required.";
inline constexpr char kAlwaysEnableHdcpDefault[] = "Default";
inline constexpr char kAlwaysEnableHdcpType0[] = "Type 0";
inline constexpr char kAlwaysEnableHdcpType1[] = "Type 1";

inline constexpr char kApnRevampName[] = "APN Revamp";
inline constexpr char kApnRevampDescription[] =
    "Enables the ChromeOS APN Revamp, which updates cellular network APN "
    "system UI and related infrastructure.";

inline constexpr char kArcEnableAttestationName[] = "Enable ARC attestation";
inline constexpr char kArcEnableAttestationDescription[] =
    "Allow key and ID attestation to run for keymint";

inline constexpr char kArcExtendIntentAnrTimeoutName[] =
    "Extend broadcast of intent ANR timeout time";
inline constexpr char kArcExtendIntentAnrTimeoutDescription[] =
    "When enabled, the default broadcast of intent ANR timeout time will be"
    " extended from 10 seconds to 15 seconds for foreground broadcasts, 60"
    " seconds to 90 seconds for background broadcasts.";

inline constexpr char kArcExtendServiceAnrTimeoutName[] =
    "Extend executing service ANR timeout time";
inline constexpr char kArcExtendServiceAnrTimeoutDescription[] =
    "When enabled, the default executing service ANR timeout time will be"
    " extended from 20 seconds to 30 seconds for foreground services, 200"
    " seconds to 300 seconds for background services.";

inline constexpr char kArcFriendlierErrorDialogName[] =
    "Enable friendlier error dialog for ARC";
inline constexpr char kArcFriendlierErrorDialogDescription[] =
    "Replaces disruptive error dialogs with Chrome notifications for some ANR "
    "and crash events.";

inline constexpr char kArcIdleManagerName[] = "Enable ARC Idle Manager";
inline constexpr char kArcIdleManagerDescription[] =
    "ARC will turn on Android's doze mode when idle.";

inline constexpr char kArcNativeBridgeToggleName[] =
    "Toggle between native bridge implementations for ARC";
inline constexpr char kArcNativeBridgeToggleDescription[] =
    "Toggle between native bridge implementations for ARC.";

inline constexpr char kArcPerAppLanguageName[] =
    "Enable ARC Per-App Language setting integration";
inline constexpr char kArcPerAppLanguageDescription[] =
    "When enabled, ARC Per-App Language settings will be surfaced in ChromeOS "
    "settings.";

inline constexpr char kArcResizeCompatName[] =
    "Enable ARC Resize Compatibility features";
inline constexpr char kArcResizeCompatDescription[] =
    "Enable resize compatibility features for ARC++ apps";

inline constexpr char kArcRtVcpuDualCoreName[] =
    "Enable ARC real time vCPU on a device with 2 logical cores online.";
inline constexpr char kArcRtVcpuDualCoreDesc[] =
    "Enable ARC real time vCPU on a device with 2 logical cores online to "
    "reduce media playback glitch.";

inline constexpr char kArcRtVcpuQuadCoreName[] =
    "Enable ARC real time vCPU on a device with 3+ logical cores online.";
inline constexpr char kArcRtVcpuQuadCoreDesc[] =
    "Enable ARC real time vCPU on a device with 3+ logical cores online to "
    "reduce media playback glitch.";

inline constexpr char kArcSwitchToKeyMintDaemonName[] =
    "Switch to KeyMint Daemon.";
inline constexpr char kArcSwitchToKeyMintDaemonDesc[] =
    "Switch from Keymaster Daemon to KeyMint Daemon. Must be switched on/off "
    "at the same time with \"Switch To KeyMint on ARC-T\"";

inline constexpr char kArcSyncInstallPriorityName[] =
    "Enable supporting install priority for synced ARC apps.";
inline constexpr char kArcSyncInstallPriorityDescription[] =
    "Enable supporting install priority for synced ARC apps. Pass install "
    "priority to Play instead of using default install priority specified "
    "in Play";

inline constexpr char kArcVmMemorySizeName[] =
    "Enable custom ARCVM memory size";
inline constexpr char kArcVmMemorySizeDesc[] =
    "Enable custom ARCVM memory size, "
    "\"shift\" controls the amount to shift system RAM when sizing ARCVM.";

inline constexpr char kArcVmmSwapKBShortcutName[] =
    "Keyboard shortcut trigger for ARCVM"
    " vmm swap feature";
inline constexpr char kArcVmmSwapKBShortcutDesc[] =
    "Alt + Ctrl + Shift + O/P to enable / disable ARCVM vmm swap. Only for "
    "experimental usage.";

inline constexpr char kArcAAudioMMAPLowLatencyName[] =
    "Enable ARCVM AAudio MMAP low latency";
inline constexpr char kArcAAudioMMAPLowLatencyDescription[] =
    "When enabled, ARCVM AAudio MMAP will use low latency setting.";

inline constexpr char kArcEnableVirtioBlkForDataName[] =
    "Enable virtio-blk for ARCVM /data";
inline constexpr char kArcEnableVirtioBlkForDataDesc[] =
    "If enabled, ARCVM uses virtio-blk for /data in Android storage.";

inline constexpr char kArcExternalStorageAccessName[] =
    "External storage access by ARC";
inline constexpr char kArcExternalStorageAccessDescription[] =
    "Allow Android apps to access external storage devices like USB flash "
    "drives and SD cards";

inline constexpr char kArcUnthrottleOnActiveAudioV2Name[] =
    "Unthrottle ARC on active audio";
inline constexpr char kArcUnthrottleOnActiveAudioV2Description[] =
    "Do not throttle ARC when there is an active audio stream running.";

inline constexpr char kArcVideoEncodeUseMappableSIName[] =
    "ARC video encode use mappable SharedImage";
inline constexpr char kArcVideoEncodeUseMappableSIDescription[] =
    "Controls whether ARC video encoding uses mappable SharedImage.";

inline constexpr char kAshEnableUnifiedDesktopName[] = "Unified desktop mode";
inline constexpr char kAshEnableUnifiedDesktopDescription[] =
    "Enable unified desktop mode which allows a window to span multiple "
    "displays.";

inline constexpr char kAshModifierSplitName[] = "Modifier split feature";
inline constexpr char kAshModifierSplitDescription[] =
    "Enable new modifier split feature on ChromeOS.";

inline constexpr char kAshPickerGifsName[] = "Picker GIFs search";
inline constexpr char kAshPickerGifsDescription[] =
    "Enable GIf search for Picker.";

inline constexpr char kAshSplitKeyboardRefactorName[] =
    "Split keyboard refactor";
inline constexpr char kAshSplitKeyboardRefactorDescription[] =
    "Enable split keyboard refactor on ChromeOS.";

inline constexpr char kAshNullTopRowFixName[] = "Null top row fix";
inline constexpr char kAshNullTopRowFixDescription[] =
    "Enable the bugfix for keyboards with a null top row descriptor.";

inline constexpr char kAssistantIphName[] = "Assistant IPH";
inline constexpr char kAssistantIphDescription[] =
    "Enables showing Assistant IPH on ChromeOS.";

inline constexpr char kAudioSelectionImprovementName[] =
    "Enable audio selection improvement algorithm";
inline constexpr char kAudioSelectionImprovementDescription[] =
    "Enable set-based audio selection improvement algorithm.";

inline constexpr char kResetAudioSelectionImprovementPrefName[] =
    "Reset audio selection improvement user preference";
inline constexpr char kResetAudioSelectionImprovementPrefDescription[] =
    "Reset audio selection improvement user preference for testing purpose.";

inline constexpr char kAutoFramingOverrideName[] =
    "Auto-framing control override";
inline constexpr char kAutoFramingOverrideDescription[] =
    "Overrides the default to forcibly enable or disable the auto-framing "
    "feature";

inline constexpr char kAutocorrectByDefaultName[] =
    "CrOS autocorrect by default";
inline constexpr char kAutocorrectByDefaultDescription[] =
    "Enables autocorrect by default experiment on ChromeOS";

inline constexpr char kAutocorrectParamsTuningName[] =
    "CrOS autocorrect params tuning";
inline constexpr char kAutocorrectParamsTuningDescription[] =
    "Enables params tuning experiment for autocorrect on ChromeOS.";

inline constexpr char kBatteryChargeLimitName[] =
    "ChromeOS Battery Charge Limit";
inline constexpr char kBatteryChargeLimitDescription[] =
    "Enables an option in Power settings which allows the user to choose "
    "between Adaptive Charging and an explicit 80% charge limit.";

inline constexpr char kBlockTelephonyDevicePhoneMuteName[] =
    "Block Telephony Device Phone Mute";
inline constexpr char kBlockTelephonyDevicePhoneMuteDescription[] =
    "Block telephony device phone mute HID code so it does not toggle ChromeOS "
    "system microphone mute.";

inline constexpr char kBluetoothAudioLEAudioOnlyName[] =
    "Bluetooth Audio LE Audio Only";
inline constexpr char kBluetoothAudioLEAudioOnlyDescription[] =
    "Enable Bluetooth LE audio and disable classic profiles "
    "(A2DP, HFP, AVRCP). This is used for prototyping and demonstration "
    "purposes.";

inline constexpr char kBluetoothBtsnoopInternalsName[] =
    "Enables btsnoop collection in chrome://bluetooth-internals";
inline constexpr char kBluetoothBtsnoopInternalsDescription[] =
    "Enables bluetooth traffic (btsnoop) collection via the page "
    "chrome://bluetooth-internals. Btsnoop logs are essential for debugging "
    "bluetooth issues.";

inline constexpr char kBluetoothFlossTelephonyName[] =
    "Bluetooth Floss Telephony";
inline constexpr char kBluetoothFlossTelephonyDescription[] =
    "Enable Floss to create a Bluetooth HID device that allows applications to "
    "access Bluetooth telephony functions through WebHID.";

inline constexpr char kBluetoothUseFlossName[] = "Use Floss instead of BlueZ";
inline constexpr char kBluetoothUseFlossDescription[] =
    "Enables using Floss (also known as Fluoride, Android's Bluetooth stack) "
    "instead of BlueZ. This is meant to be used by developers and is not "
    "guaranteed to be stable";

inline constexpr char kBluetoothUseLLPrivacyName[] =
    "Enable LL Privacy in Floss";
inline constexpr char kBluetoothUseLLPrivacyDescription[] =
    "Enable address resolution offloading to Bluetooth Controller if "
    "supported. Modifying this flag will cause Bluetooth Controller to reset.";

inline constexpr char kCampbellGlyphName[] = "Enable glyph for Campbell";
inline constexpr char kCampbellGlyphDescription[] = "Enables a Campbell glyph.";

inline constexpr char kCampbellKeyName[] = "Key to enable glyph for Campbell";
inline constexpr char kCampbellKeyDescription[] =
    "Secret key to enable glyph for Campbell";

inline constexpr char kCaptureModeEducationName[] =
    "Enable Capture Mode Education";
inline constexpr char kCaptureModeEducationDescription[] =
    "Enables the Capture Mode Education nudges and tutorials that inform users "
    "of the screenshot keyboard shortcut and the screen capture tool in the "
    "quick settings menu.";

inline constexpr char kCaptureModeEducationBypassLimitsName[] =
    "Enable Capture Mode Education bypass limits";
inline constexpr char kCaptureModeEducationBypassLimitsDescription[] =
    "Enables bypassing the 3 times / 24 hours show limit for Capture Mode "
    "Education nudges and tutorials, so they can be viewed repeatedly for "
    "testing purposes.";

inline constexpr char kCrosContentAdjustedRefreshRateName[] =
    "Content Adjusted Refresh Rate";
inline constexpr char kCrosContentAdjustedRefreshRateDescription[] =
    "Allows the display to adjust the refresh rate in order to match content.";

inline constexpr char kDesksTemplatesName[] = "Desk Templates";
inline constexpr char kDesksTemplatesDescription[] =
    "Streamline workflows by saving a group of applications and windows as a "
    "launchable template in a new desk";

inline constexpr char kForceControlFaceAeName[] = "Force control face AE";
inline constexpr char kForceControlFaceAeDescription[] =
    "Control this flag to force enable or disable face AE for camera";

inline constexpr char kCellularBypassESimInstallationConnectivityCheckName[] =
    "Bypass eSIM installation connectivity check";
inline constexpr char
    kCellularBypassESimInstallationConnectivityCheckDescription[] =
        "Bypass the non-cellular internet connectivity check during eSIM "
        "installation.";

inline constexpr char kCellularUseSecondEuiccName[] = "Use second Euicc";
inline constexpr char kCellularUseSecondEuiccDescription[] =
    "When enabled Cellular Setup and Settings UI will use the second available "
    "eUICC that's exposed by Hermes.";

inline constexpr char kCloudGamingDeviceName[] = "Enable cloud game search";
inline constexpr char kCloudGamingDeviceDescription[] =
    "Enables cloud game search results in the launcher.";

inline constexpr char kCampaignsComponentUpdaterTestTagName[] =
    "Campaigns test tag";
inline constexpr char kCampaignsComponentUpdaterTestTagDescription[] =
    "Tags used for component updater to select Omaha cohort for Growth "
    "Campaigns.";
inline constexpr char kCampaignsOverrideName[] = "Campaigns override";
inline constexpr char kCampaignsOverrideDescription[] =
    "Base64 encoded Growth campaigns used for testing.";

inline constexpr char kComponentUpdaterTestRequestName[] =
    "Enable the component updater check 'test-request' parameter";
inline constexpr char kComponentUpdaterTestRequestDescription[] =
    "Enables the 'test-request' parameter for component updater check requests."
    " Overrides any other component updater check request parameters that may "
    "have been specified.";

inline constexpr char kEnableServiceWorkersForChromeUntrustedName[] =
    "Enable chrome-untrusted:// Service Workers";
inline constexpr char kEnableServiceWorkersForChromeUntrustedDescription[] =
    "When enabled, allows chrome-untrusted:// WebUIs to use service workers.";

inline constexpr char kEnterpriseReportingUIName[] =
    "Enable chrome://enterprise-reporting";
inline constexpr char kEnterpriseReportingUIDescription[] =
    "When enabled, allows for chrome://enterprise-reporting to be visited";

inline constexpr char kESimEmptyActivationCodeSupportedName[] =
    "Enable support for empty activation codes in eSIM activation dialog";
inline constexpr char kESimEmptyActivationCodeSupportedDescription[] =
    "When enabled, allows users to enter and submit empty activation codes in "
    "the eSIM dialog";

inline constexpr char kPermissiveUsbPassthroughName[] =
    "Enable more permissive passthrough for USB Devices";
inline constexpr char kPermissiveUsbPassthroughDescription[] =
    "When enabled, applies more permissive rules passthrough of USB devices.";

inline constexpr char kChromeboxUsbPassthroughRestrictionsName[] =
    "Limit primary mice/keyboards from USB passthrough on chromeboxes";
inline constexpr char kChromeboxUsbPassthroughRestrictionsDescription[] =
    "When enabled, attempts to prevent primary mice/keyboard from being passed "
    "through to guest environments on chromebox-style devices.  If you have "
    "issues with passing through a USB peripheral on a chromebox, you can "
    "try disabling this feature.";

inline constexpr char kDisableBruschettaInstallChecksName[] =
    "Disable Bruschetta Installer Checks";
inline constexpr char kDisableBruschettaInstallChecksDescription[] =
    "Disables the built-in checks the Bruschetta installer performs before "
    "running the install process.";

inline constexpr char kCrostiniContainerInstallName[] =
    "Debian version for new Crostini containers";
inline constexpr char kCrostiniContainerInstallDescription[] =
    "New Crostini containers will use this Debian version";

inline constexpr char kCrostiniGpuSupportName[] = "Crostini GPU Support";
inline constexpr char kCrostiniGpuSupportDescription[] =
    "Enable Crostini GPU support.";

inline constexpr char kCrostiniResetLxdDbName[] =
    "Crostini Reset LXD DB on launch";
inline constexpr char kCrostiniResetLxdDbDescription[] =
    "Recreates the LXD database every time we launch it";

inline constexpr char kCrostiniContainerlessName[] =
    "Crostini without LXD containers";
inline constexpr char kCrostiniContainerlessDescription[] =
    "Experimental support for Crostini without LXD containers (aka Baguette)";

inline constexpr char kCrostiniMultiContainerName[] =
    "Allow multiple Crostini containers";
inline constexpr char kCrostiniMultiContainerDescription[] =
    "Experimental UI for creating and managing multiple Crostini containers";

inline constexpr char kCrostiniQtImeSupportName[] =
    "Crostini IME support for Qt applications";
inline constexpr char kCrostiniQtImeSupportDescription[] =
    "Experimental support for IMEs (excluding VK) in Crostini for applications "
    "built with Qt.";

inline constexpr char kCrostiniVirtualKeyboardSupportName[] =
    "Crostini Virtual Keyboard Support";
inline constexpr char kCrostiniVirtualKeyboardSupportDescription[] =
    "Experimental support for the Virtual Keyboard on Crostini.";

inline constexpr char kConchName[] = "Conch feature";
inline constexpr char kConchDescription[] = "Enable Conch on ChromeOS.";

inline constexpr char kConchSystemAudioFromMicName[] =
    "System audio capture for Conch";
inline constexpr char kConchSystemAudioFromMicDescription[] =
    "Capture system audio from microphone for Conch on ChromeOS.";

inline constexpr char kDemoModeComponentUpdaterTestTagName[] =
    "Demo Mode test tag";
inline constexpr char kDemoModeComponentUpdaterTestTagDescription[] =
    "Tags used for component updater to select Omaha cohort for Demo Mode.";

inline constexpr char kDisableCancelAllTouchesName[] =
    "Disable CancelAllTouches()";
inline constexpr char kDisableCancelAllTouchesDescription[] =
    "If enabled, a canceled touch will not force all other touches to be "
    "canceled.";

inline constexpr char kDisableExplicitDmaFencesName[] =
    "Disable explicit dma-fences";
inline constexpr char kDisableExplicitDmaFencesDescription[] =
    "Always rely on implicit synchronization between GPU and display "
    "controller instead of using dma-fences explicitly when available.";

inline constexpr char kDisplayAlignmentAssistanceName[] =
    "Enable Display Alignment Assistance";
inline constexpr char kDisplayAlignmentAssistanceDescription[] =
    "Show indicators on shared edges of the displays when user is "
    "attempting to move their mouse over to another display. Show preview "
    "indicators when the user is moving a display in display layouts.";

inline constexpr char kEnableLibinputToHandleTouchpadName[] =
    "Enable libinput to handle touchpad.";
inline constexpr char kEnableLibinputToHandleTouchpadDescription[] =
    "Use libinput instead of the gestures library to handle touchpad."
    "Libgesures works very well on modern devices but fails on legacy"
    "devices. Use libinput if an input device doesn't work or is not working"
    "well.";

inline constexpr char kEnableFakeKeyboardHeuristicName[] =
    "Enable Fake Keyboard Heuristic";
inline constexpr char kEnableFakeKeyboardHeuristicDescription[] =
    "Enable heuristic to prevent non-keyboard devices from pretending "
    "to be keyboards. Primarily assists in preventing the virtual keyboard "
    "from being disabled unintentionally.";

inline constexpr char kEnableFakeMouseHeuristicName[] =
    "Enable Fake Mouse Heuristic";
inline constexpr char kEnableFakeMouseHeuristicDescription[] =
    "Enable heuristic to prevent non-mouse devices from pretending "
    "to be mice. Primarily assists in preventing fake entries "
    "appearing in the input settings menu.";

inline constexpr char kFastPairDebugMetadataName[] =
    "Enable Fast Pair Debug Metadata";
inline constexpr char kFastPairDebugMetadataDescription[] =
    "Enables Fast Pair to use Debug metadata when checking device "
    "advertisements, allowing notifications to pop up for debug-mode only "
    "devices.";

inline constexpr char kFaceRetouchOverrideName[] =
    "Enable face retouch using the relighting button in the VC panel";
inline constexpr char kFaceRetouchOverrideDescription[] =
    "Enables or disables the face retouch feature using the relighting button "
    "in the VC panel.";

inline constexpr char kFastPairHandshakeLongTermRefactorName[] =
    "Enable Fast Pair Handshake Long Term Refactor";
inline constexpr char kFastPairHandshakeLongTermRefactorDescription[] =
    "Enables long term refactored handshake logic for Google Fast Pair "
    "service.";

inline constexpr char kFastPairKeyboardsName[] = "Enable Fast Pair Keyboards";
inline constexpr char kFastPairKeyboardsDescription[] =
    "Enables prototype support for Fast Pair for keyboards.";

inline constexpr char kFastPairPwaCompanionName[] =
    "Enable Fast Pair Web Companion";
inline constexpr char kFastPairPwaCompanionDescription[] =
    "Enables Fast Pair Web Companion link after device pairing.";

inline constexpr char kUseHDRTransferFunctionName[] =
    "Monitor/Display HDR transfer function";
inline constexpr char kUseHDRTransferFunctionDescription[] =
    "Allows using the HDR transfer functions of any connected monitor that "
    "supports it";

inline constexpr char kEnableExternalDisplayHdr10Name[] =
    "Enable HDR10 support on external monitors";
inline constexpr char kEnableExternalDisplayHdr10Description[] =
    "Allows using HDR10 mode on any external monitor that supports it";

inline constexpr char kDriveFsMirroringName[] =
    "Enable local to Drive mirror sync";

inline constexpr char kDriveFsShowCSEFilesDescription[] =
    "Enable listing of CSE files in DriveFS, which will result in these files "
    "being visible in the Files App's Google Drive item.";

inline constexpr char kEnableBrightnessControlInSettingsName[] =
    "Enable brightness controls in Settings";
inline constexpr char kEnableBrightnessControlInSettingsDescription[] =
    "Enables brightness slider and auto-brightness toggle for internal display "
    "in Settings";

inline constexpr char kEnableDisplayPerformanceModeName[] =
    "Enable Display Performance Mode";
inline constexpr char kEnableDisplayPerformanceModeDescription[] =
    "This option enables toggling different display features based on user "
    "setting and power state";

inline constexpr char kDisableDnsProxyName[] =
    "Disable DNS proxy service for ChromeOS";
inline constexpr char kDisableDnsProxyDescription[] =
    "Turns off DNS proxying and SecureDNS for ChromeOS (only). Does not impact "
    "Chrome browser.";

inline constexpr char kDisconnectWiFiOnEthernetConnectedName[] =
    "Disconnect WiFi on Ethernet";
inline constexpr char kDisconnectWiFiOnEthernetConnectedDescription[] =
    "Automatically disconnect WiFi and prevent it from auto connecting when "
    "the device gets an Ethernet connection. User are still allowed to connect "
    "to WiFi manually.";

inline constexpr char kEnableRFC8925Name[] =
    "Enable RFC8925 (prefer IPv6-only on IPv6-only-capable network)";
inline constexpr char kEnableRFC8925Description[] =
    "Let ChromeOS DHCPv4 client voluntarily drop DHCPv4 lease and prefer to"
    "operate IPv6-only, if the network is also IPv6-only capable.";

inline constexpr char kEnableRootNsDnsProxyName[] =
    "Enable DNS proxy service running on the root network namespace for "
    "ChromeOS";
inline constexpr char kEnableRootNsDnsProxyDescription[] =
    "When enabled, DNS proxy service runs on the root network namespace "
    "instead of inside a specified network namespace";

inline constexpr char kEnableEdidBasedDisplayIdsName[] =
    "Enable EDID-based display IDs";
inline constexpr char kEnableEdidBasedDisplayIdsDescription[] =
    "When enabled, a display's ID will be produced by hashing certain values "
    "in the display's EDID blob. EDID-based display IDs allow ChromeOS to "
    "consistently identify previously connected displays, regardless of the "
    "physical port they were connected to, and load user display layouts more "
    "accurately.";

inline constexpr char kTiledDisplaySupportName[] =
    "Enable tile display support";
inline constexpr char kTiledDisplaySupportDescription[] =
    "When enabled, tiled displays will be represented by a single display in "
    "ChromeOS, rather than each tile being a separate display.";

inline constexpr char kEnableDozeModePowerSchedulerName[] =
    "Enable doze mode power scheduler";
inline constexpr char kEnableDozeModePowerSchedulerDescription[] =
    "Enable doze mode power scheduler.";

inline constexpr char kEnableExternalKeyboardsInDiagnosticsAppName[] =
    "Enable external keyboards in the Diagnostics App";
inline constexpr char kEnableExternalKeyboardsInDiagnosticsAppDescription[] =
    "Shows external keyboards in the Diagnostics App's input section. Requires "
    "#enable-input-in-diagnostics-app to be enabled.";

inline constexpr char kEnableFastInkForSoftwareCursorName[] =
    "Enable fast ink for software cursor";
inline constexpr char kEnableFastInkForSoftwareCursorDescription[] =
    "When enabled, software cursor will use fast ink to display cursor with "
    "minimal latency. "
    "However, it might also cause tearing artifacts.";

inline constexpr char kEnableHostnameSettingName[] =
    "Enable setting the device hostname";
inline constexpr char kEnableHostnameSettingDescription[] =
    "Enables the ability to set the ChromeOS hostname, the name of the device "
    "that is exposed to the local network";

inline constexpr char kEnableGesturePropertiesDBusServiceName[] =
    "Enable gesture properties D-Bus service";
inline constexpr char kEnableGesturePropertiesDBusServiceDescription[] =
    "Enable a D-Bus service for accessing gesture properties, which are used "
    "to configure input devices.";

inline constexpr char kEnableInputEventLoggingName[] =
    "Enable input event logging";
inline constexpr char kEnableInputEventLoggingDescription[] =
    "Enable detailed logging of input events from touchscreens, touchpads, and "
    "mice. These events include the locations of all touches as well as "
    "relative pointer movements, and so may disclose sensitive data. They "
    "will be included in feedback reports and system logs, so DO NOT ENTER "
    "SENSITIVE INFORMATION with this flag enabled.";

inline constexpr char kEnableKeyboardUsedPalmSuppressionName[] =
    "Use keyboard based palm suppression.";
inline constexpr char kEnableKeyboardUsedPalmSuppressionDescription[] =
    "Enable keyboard usage based palm suppression.";

inline constexpr char kEnableHeatmapPalmDetectionName[] =
    "Enable Heatmap Palm Detection";
inline constexpr char kEnableHeatmapPalmDetectionDescription[] =
    "Experimental: Enable Heatmap Palm detection. Not compatible with all "
    "devices.";

inline constexpr char kEnableNeuralStylusPalmRejectionName[] =
    "Enable Neural Palm Detection";
inline constexpr char kEnableNeuralStylusPalmRejectionDescription[] =
    "Experimental: Enable Neural Palm detection. Not compatible with all "
    "devices.";

inline constexpr char kEnablePalmSuppressionName[] =
    "Enable Palm Suppression with Stylus.";
inline constexpr char kEnablePalmSuppressionDescription[] =
    "If enabled, suppresses touch when a stylus is on a touchscreen.";

inline constexpr char kEnableFastTouchpadClickName[] =
    "Enable Fast Touchpad Click";
inline constexpr char kEnableFastTouchpadClickDescription[] =
    "If enabled, reduce the time after touchpad click before cursor can move.";

inline constexpr char kEnableSeamlessRefreshRateSwitchingName[] =
    "Seamless Refresh Rate Switching";
inline constexpr char kEnableSeamlessRefreshRateSwitchingDescription[] =
    "This option enables seamlessly changing the refresh rate based on power "
    "state on devices with supported hardware and drivers.";

inline constexpr char kEnableToggleCameraShortcutName[] =
    "Enable shortcut to toggle camera access";
inline constexpr char kEnableToggleCameraShortcutDescription[] =
    "Adds a shortcut to toggle the value of the top level 'Camera access' "
    "setting in the privacy controls section of the Settings app.";

inline constexpr char kEnableTouchpadsInDiagnosticsAppName[] =
    "Enable touchpad cards in the Diagnostics App";
inline constexpr char kEnableTouchpadsInDiagnosticsAppDescription[] =
    "Shows touchpad cards in the Diagnostics App's input section. Requires "
    "#enable-input-in-diagnostics-app to be enabled.";

inline constexpr char kEnableTouchscreensInDiagnosticsAppName[] =
    "Enable touchscreen cards in the Diagnostics App";
inline constexpr char kEnableTouchscreensInDiagnosticsAppDescription[] =
    "Shows touchscreen cards in the Diagnostics App's input section. Requires "
    "#enable-input-in-diagnostics-app to be enabled.";

inline constexpr char kEnableWifiQosName[] = "Enable WiFi QoS";
inline constexpr char kEnableWifiQosDescription[] =
    "If enabled the system will start automatic prioritization of egress "
    "traffic with WiFi QoS/WMM.";

inline constexpr char kEnableWifiQosEnterpriseName[] =
    "Enable WiFi QoS enterprise";
inline constexpr char kEnableWifiQosEnterpriseDescription[] =
    "If enabled the system will start automatic prioritization of egress "
    "traffic with WiFi QoS/WMM. This flag only affects Enterprise enrolled "
    "devices. Requires #enable-wifi-qos to be enabled.";

inline constexpr char kPanelSelfRefresh2Name[] = "Enable Panel Self Refresh 2";
inline constexpr char kPanelSelfRefresh2Description[] =
    "Enable Panel Self Refresh 2/Selective-Update where supported. "
    "Allows the display driver to only update regions of the screen that have "
    "damage.";

inline constexpr char kEnableVariableRefreshRateName[] =
    "Enable Variable Refresh Rate";
inline constexpr char kEnableVariableRefreshRateDescription[] =
    "Enable the variable refresh rate (Adaptive Sync) setting for capable "
    "displays.";

inline constexpr char kEapGtcWifiAuthenticationName[] =
    "EAP-GTC WiFi Authentication";
inline constexpr char kEapGtcWifiAuthenticationDescription[] =
    "Allows configuration of WiFi networks using EAP-GTC authentication";

inline constexpr char kEcheSWAName[] = "Enable Eche feature";
inline constexpr char kEcheSWADescription[] =
    "This is the main flag for enabling Eche.";

inline constexpr char kEcheSWADebugModeName[] = "Enable Eche Debug Mode";
inline constexpr char kEcheSWADebugModeDescription[] =
    "Save console logs of Eche in the system log";

inline constexpr char kEcheSWAMeasureLatencyName[] = "Measure Eche E2E Latency";
inline constexpr char kEcheSWAMeasureLatencyDescription[] =
    "Measure Eche E2E Latency and print all E2E latency logs of Eche in "
    "Console";

inline constexpr char kEcheSWASendStartSignalingName[] =
    "Enable Eche Send Start Signaling";
inline constexpr char kEcheSWASendStartSignalingDescription[] =
    "Allows sending start signaling action to establish Eche's WebRTC "
    "connection";

inline constexpr char kEcheSWADisableStunServerName[] =
    "Disable Eche STUN server";
inline constexpr char kEcheSWADisableStunServerDescription[] =
    "Allows disabling the stun servers when establishing a WebRTC connection "
    "to Eche";

inline constexpr char kEcheSWACheckAndroidNetworkInfoName[] =
    "Check Android network info";
inline constexpr char kEcheSWACheckAndroidNetworkInfoDescription[] =
    "Allows CrOS to analyze Android network information to provide more "
    "context on connection errors";

inline constexpr char kEnableOAuthIppName[] =
    "Enable OAuth when printing via the IPP protocol";
inline constexpr char kEnableOAuthIppDescription[] =
    "Enable OAuth when printing via the IPP protocol";

inline constexpr char kEnableOngoingProcessesName[] =
    "Enable Ongoing Processes";
inline constexpr char kEnableOngoingProcessesDescription[] =
    "Enables use of the new PinnedNotificationView for all ash pinned "
    "notifications, which are now referred to as Ongoing Processes";

inline constexpr char kEnterOverviewFromWallpaperName[] =
    "Enable entering overview from wallpaper";
inline constexpr char kEnterOverviewFromWallpaperDescription[] =
    "Experimental feature. Enable entering overview by clicking wallpaper with "
    "mouse click";

inline constexpr char kEolResetDismissedPrefsName[] =
    "Reset end of life notification prefs";
inline constexpr char kEolResetDismissedPrefsDescription[] =
    "Reset the end of life notification prefs to their default value, at the "
    "start of the user session. This is meant to make manual testing easier.";

inline constexpr char kEventBasedLogUpload[] = "Enable event based log uploads";
inline constexpr char kEventBasedLogUploadDescription[] =
    "Uploads relevant logs to device management server when unexpected events "
    "(e.g. crashes) occur on the device. The feature is guarded by "
    "LogUploadEnabled policy.";

inline constexpr char kExcludeDisplayInMirrorModeName[] =
    "Enable feature to exclude a display in mirror mode.";
inline constexpr char kExcludeDisplayInMirrorModeDescription[] =
    "Show toggles in Display Settings to exclude a display in mirror mode.";

inline constexpr char kExoGamepadVibrationName[] =
    "Gamepad Vibration for Exo Clients";
inline constexpr char kExoGamepadVibrationDescription[] =
    "Allow Exo clients like Android to request vibration events for gamepads "
    "that support it.";

inline constexpr char kExoOrdinalMotionName[] =
    "Raw (unaccelerated) motion for Linux applications";
inline constexpr char kExoOrdinalMotionDescription[] =
    "Send unaccelerated values as raw motion events to Linux applications.";

inline constexpr char kExperimentalAccessibilityDictationContextCheckingName[] =
    "Experimental accessibility dictation using context checking.";
inline constexpr char
    kExperimentalAccessibilityDictationContextCheckingDescription[] =
        "Enables experimental dictation context checking.";

inline constexpr char
    kExperimentalAccessibilityGoogleTtsHighQualityVoicesName[] =
        "Experimental accessibility Google TTS High Quality Voices.";
inline constexpr char
    kExperimentalAccessibilityGoogleTtsHighQualityVoicesDescription[] =
        "Enables downloading Google TTS High Quality Voices.";

inline constexpr char kExperimentalAccessibilityManifestV3Name[] =
    "Changes accessibility features from extension manifest v2 to v3.";
inline constexpr char kExperimentalAccessibilityManifestV3Description[] =
    "Experimental migration of accessibility features from extension manifest "
    "v2 to v3. Likely to break accessibility access while experimental.";

inline constexpr char kAccessibilityManifestV3AccessibilityCommonName[] =
    "Changes accessibility common extension manifest v2 to v3.";
inline constexpr char kAccessibilityManifestV3AccessibilityCommonDescription[] =
    "Experimental migration of accessibility common extension from manifest v2 "
    "to v3.";

inline constexpr char kAccessibilityManifestV3BrailleImeName[] =
    "Changes accessibility extension Braille IME manifest v2 to v3.";
inline constexpr char kAccessibilityManifestV3BrailleImeDescription[] =
    "Experimental migration of Braille IME from extension manifest v2 to v3.";

inline constexpr char kAccessibilityManifestV3ChromeVoxName[] =
    "Changes accessibility extension ChromeVox manifest v2 to v3.";
inline constexpr char kAccessibilityManifestV3ChromeVoxDescription[] =
    "Experimental migration of ChromeVox from extension manifest v2 to v3.";

inline constexpr char kAccessibilityManifestV3EnhancedNetworkTtsName[] =
    "Changes accessibility extension Enhanced Network TTS manifest v2 to v3.";
inline constexpr char kAccessibilityManifestV3EnhancedNetworkTtsDescription[] =
    "Experimental migration of Enhanced Network TTS from extension manifest "
    "v2 to v3.";

inline constexpr char kAccessibilityManifestV3EspeakNGName[] =
    "Changes accessibility extension EspeakNG TTS manifest v2 to v3.";
inline constexpr char kAccessibilityManifestV3EspeakNGDescription[] =
    "Experimental migration of EspeakNG TTS from extension manifest v2 to v3.";

inline constexpr char kAccessibilityManifestV3GoogleTtsName[] =
    "Changes accessibility extension Google TTS manifest v2 to v3.";
inline constexpr char kAccessibilityManifestV3GoogleTtsDescription[] =
    "Experimental migration of Google TTS from extension manifest v2 to v3.";

inline constexpr char kAccessibilityManifestV3SelectToSpeakName[] =
    "Changes accessibility extension Select to Speak manifest v2 to v3.";
inline constexpr char kAccessibilityManifestV3SelectToSpeakDescription[] =
    "Experimental migration of Select to Speak from extension manifest "
    "v2 to v3.";

inline constexpr char kAccessibilityManifestV3SwitchAccessName[] =
    "Changes accessibility extension Switch Access manifest v2 to v3.";
inline constexpr char kAccessibilityManifestV3SwitchAccessDescription[] =
    "Experimental migration of Switch Access from extension manifest "
    "v2 to v3.";

inline constexpr char kExperimentalAccessibilitySwitchAccessTextName[] =
    "Enable enhanced Switch Access text input.";
inline constexpr char kExperimentalAccessibilitySwitchAccessTextDescription[] =
    "Enable experimental or in-progress Switch Access features for improved "
    "text input";

inline constexpr char kFastDrmMasterDropName[] =
    "Drop DRM master tokens without disabling all the displays.";
inline constexpr char kFastDrmMasterDropDescription[] =
    "Drop DRM master tokens after detaching all the planes off of pipes,"
    "rather than disabling all the displays. Will not work on AMD devices as "
    "they are unable to accept commits without a primary plane.";

inline constexpr char kFileTransferEnterpriseConnectorName[] =
    "Enable Files Transfer Enterprise Connector.";
inline constexpr char kFileTransferEnterpriseConnectorDescription[] =
    "Enable the File Transfer Enterprise Connector.";

inline constexpr char kFileTransferEnterpriseConnectorUIName[] =
    "Enable UI for Files Transfer Enterprise Connector.";
inline constexpr char kFileTransferEnterpriseConnectorUIDescription[] =
    "Enable the UI for the File Transfer Enterprise Connector.";

inline constexpr char kFilesConflictDialogName[] = "Files app conflict dialog";
inline constexpr char kFilesConflictDialogDescription[] =
    "When enabled, the conflict dialog will be shown during file transfers "
    "if a file entry in the transfer exists at the destination.";

inline constexpr char kFilesLocalImageSearchName[] =
    "Search local images by query.";
inline constexpr char kFilesLocalImageSearchDescription[] =
    "Enable searching local images by query.";

inline constexpr char kFilesMaterializedViewsName[] =
    "Files app materialized views";
inline constexpr char kFilesMaterializedViewsDescription[] =
    "Enable materialized views in Files App.";

inline constexpr char kFilesSinglePartitionFormatName[] =
    "Enable Partitioning of Removable Disks.";
inline constexpr char kFilesSinglePartitionFormatDescription[] =
    "Enable partitioning of removable disks into single partition.";

inline constexpr char kFilesTrashAutoCleanupName[] = "Trash auto cleanup";
inline constexpr char kFilesTrashAutoCleanupDescription[] =
    "Enable background cleanup for old files in Trash.";

inline constexpr char kFilesTrashDriveName[] = "Enable Files Trash for Drive.";
inline constexpr char kFilesTrashDriveDescription[] =
    "Enable trash for Drive volume in Files App.";

inline constexpr char kFileSystemProviderCloudFileSystemName[] =
    "Enable CloudFileSystem for FileSystemProvider extensions.";
inline constexpr char kFileSystemProviderCloudFileSystemDescription[] =
    "Enable the ability for individual FileSystemProvider extensions to "
    "be serviced by a CloudFileSystem.";

inline constexpr char kFileSystemProviderContentCacheName[] =
    "Enable content caching for FileSystemProvider extensions.";
inline constexpr char kFileSystemProviderContentCacheDescription[] =
    "Enable the ability for individual FileSystemProvider extensions being "
    "serviced by CloudFileSystem to leverage a content cache.";

inline constexpr char kFirmwareUpdateUIV2Name[] =
    "Enables the v2 version of the Firmware Updates app";
inline constexpr char kFirmwareUpdateUIV2Description[] =
    "Enable the v2 version of the Firmware Updates App.";

inline constexpr char kFocusFollowsCursorName[] = "Focus follows cursor";
inline constexpr char kFocusFollowsCursorDescription[] =
    "Enable window focusing by moving the cursor.";

inline constexpr char kFuseBoxDebugName[] =
    "Debugging UI for ChromeOS FuseBox service";
inline constexpr char kFuseBoxDebugDescription[] =
    "Show additional debugging UI for ChromeOS FuseBox service.";

inline constexpr char kGameDashboardGamepadSupport[] =
    "Game Dashboard gamepad support.";

inline constexpr char kGameDashboardUtilities[] = "Game Dashboard Utilities";
inline constexpr char kGameDashboardUtilitiesDescription[] =
    "Enables utility features in the Game Dashboard.";

inline constexpr char kAppLaunchShortcut[] = "App launch keyboard shortcut";
inline constexpr char kAppLaunchShortcutDescription[] =
    "Enables a keyboard shortcut that launches a user specified app.";

inline constexpr char kGlanceablesTimeManagementClassroomStudentViewName[] =
    "Glanceables > Time Management > Classroom Student";
inline constexpr char
    kGlanceablesTimeManagementClassroomStudentViewDescription[] =
        "Enables Google Classroom integration for students on the Time "
        "Management "
        "Glanceables surface (via Calendar entry point).";

inline constexpr char kGlanceablesTimeManagementTasksViewName[] =
    "Glanceables > Time Management > Tasks";
inline constexpr char kGlanceablesTimeManagementTasksViewDescription[] =
    "Enables Google Tasks integration on the Time Management Glanceables "
    "surface (via Calendar entry point).";

inline constexpr char kHelpAppAppDetailPageName[] = "Help App app detail page";
inline constexpr char kHelpAppAppDetailPageDescription[] =
    "If enabled, the Help app will render the App Detail Page and entry point.";

inline constexpr char kHelpAppAppsListName[] = "Help App apps list";
inline constexpr char kHelpAppAppsListDescription[] =
    "If enabled, the Help app will render the Apps List page and entry point.";

inline constexpr char kHelpAppAutoTriggerInstallDialogName[] =
    "Help App Auto Trigger Install Dialog";
inline constexpr char kHelpAppAutoTriggerInstallDialogDescription[] =
    "Enables the logic that auto triggers the install dialog during the web "
    "app install flow initiated from the Help App.";

inline constexpr char kHelpAppHomePageAppArticlesName[] =
    "Help App home page app articles";
inline constexpr char kHelpAppHomePageAppArticlesDescription[] =
    "If enabled, the home page of the Help App will show a section containing"
    "articles about apps.";

inline constexpr char kHelpAppLauncherSearchName[] = "Help App launcher search";
inline constexpr char kHelpAppLauncherSearchDescription[] =
    "Enables showing search results from the help app in the launcher.";

inline constexpr char kHelpAppOnboardingRevampName[] =
    "Help App onboarding revamp";
inline constexpr char kHelpAppOnboardingRevampDescription[] =
    "Enables a new onboarding flow in the Help App";

inline constexpr char kHelpAppOpensInsteadOfReleaseNotesNotificationName[] =
    "Help App opens instead of release notes notification";
inline constexpr char
    kHelpAppOpensInsteadOfReleaseNotesNotificationDescription[] =
        "Enables opening the Help App's What's New page immediately instead of "
        "showing a notification to open the help app.";

inline constexpr char kHybridChargerNotificationsName[] =
    "Hybrid Charger Notifications";
inline constexpr char kHybridChargerNotificationsDescription[] =
    "Displays helpful notifications for devices with Hybrid Chargers.";

inline constexpr char kIdbSqliteBackingStoreName[] = "IDB SQLite Backing Store";
inline constexpr char kIdbSqliteBackingStoreDescription[] =
    "Uses a SQLite-powered backing store for IndexedDB. No data is migrated "
    "from existing backing stores, including LevelDB stores or SQLite stores "
    "with older schemas; use at your own peril.";

inline constexpr char kImeAssistMultiWordName[] =
    "Enable assistive multi word suggestions";
inline constexpr char kImeAssistMultiWordDescription[] =
    "Enable assistive multi word suggestions for native IME";

inline constexpr char kImeSwitchCheckConnectionStatusName[] =
    "Enable IME switching using global boolean";
inline constexpr char kImeSwitchCheckConnectionStatusDescription[] =
    "When enabled and swapping between input methods, this prevents a race "
    "condition.";

inline constexpr char kIppFirstSetupForUsbPrintersName[] =
    "Try to setup USB printers with IPP first";
inline constexpr char kIppFirstSetupForUsbPrintersDescription[] =
    "When enabled, ChromeOS attempts to setup USB printers via IPP Everywhere "
    "first, then falls back to PPD-based setup.";

inline constexpr char kImeSystemEmojiPickerGIFSupportName[] =
    "System emoji picker gif support";
inline constexpr char kImeSystemEmojiPickerGIFSupportDescription[] =
    "Emoji picker gif support allows users to select gifs to input.";

inline constexpr char kImeSystemEmojiPickerJellySupportName[] =
    "Enable jelly colors for the System Emoji Picker";
inline constexpr char kImeSystemEmojiPickerJellySupportDescription[] =
    "Enable jelly colors for the System Emoji Picker.";

inline constexpr char kImeSystemEmojiPickerMojoSearchName[] =
    "Enable mojo search for the System Emoji Picker";
inline constexpr char kImeSystemEmojiPickerMojoSearchDescription[] =
    "Enable mojo search for the System Emoji Picker.";

inline constexpr char kImeSystemEmojiPickerVariantGroupingName[] =
    "System emoji picker global variant grouping";
inline constexpr char kImeSystemEmojiPickerVariantGroupingDescription[] =
    "Emoji picker global variant grouping syncs skin tone and gender "
    "preferences across emojis in each group.";

inline constexpr char kImeUsEnglishModelUpdateName[] =
    "Enable US English IME model update";
inline constexpr char kImeUsEnglishModelUpdateDescription[] =
    "Enable updated US English IME language models for native IME";

inline constexpr char kJupiterScreensaverName[] = "Jupiter screensaver";
inline constexpr char kJupiterScreensaverDescription[] =
    "Enable Jupiter screensaver on more device types.";

inline constexpr char kCrosComponentsName[] = "Cros Components";
inline constexpr char kCrosComponentsDescription[] =
    "Enable cros-component UI elements, replacing other elements.";

inline constexpr char kLanguagePacksInSettingsName[] =
    "Language Packs in Settings";
inline constexpr char kLanguagePacksInSettingsDescription[] =
    "Enables the UI and logic to manage Language Packs in Settings. This is "
    "used for languages and input methods.";

inline constexpr char kLauncherContinueSectionWithRecentsName[] =
    "Launcher continue section with recent drive files";
inline constexpr char kLauncherContinueSectionWithRecentsDescription[] =
    "Adds Google Drive file suggestions based on users' recent activity to "
    "\"Continue where you left off\" section in Launcher.";

inline constexpr char kLauncherItemSuggestName[] = "Launcher ItemSuggest";
inline constexpr char kLauncherItemSuggestDescription[] =
    "Allows configuration of experiment parameters for ItemSuggest in the "
    "launcher.";

inline constexpr char kLimitShelfItemsToActiveDeskName[] =
    "Limit Shelf items to active desk";
inline constexpr char kLimitShelfItemsToActiveDeskDescription[] =
    "Limits items on the shelf to the ones associated with windows on the "
    "active desk";

inline constexpr char kListAllDisplayModesName[] = "List all display modes";
inline constexpr char kListAllDisplayModesDescription[] =
    "Enables listing all external displays' modes in the display settings.";

inline constexpr char kHindiInscriptLayoutName[] =
    "Hindi Inscript Layout on CrOS";
inline constexpr char kHindiInscriptLayoutDescription[] =
    "Enables Hindi Inscript Layout on ChromeOS.";

inline constexpr char kLockScreenNotificationName[] =
    "Lock screen notification";
inline constexpr char kLockScreenNotificationDescription[] =
    "Enable notifications on the lock screen.";

inline constexpr char kMahiName[] = "Mahi feature";
inline constexpr char kMahiDescription[] = "Enable Mahi feature on ChromeOS.";

inline constexpr char kMahiDebuggingName[] = "Mahi Debugging";
inline constexpr char kMahiDebuggingDescription[] =
    "Enable debugging for mahi.";

inline constexpr char kMahiPanelResizableName[] = "Mahi panel resizing";
inline constexpr char kMahiPanelResizableDescription[] =
    "Enable Mahi panel resizing on ChromeOS.";

inline constexpr char kMahiSummarizeSelectedName[] =
    "Mahi summarize selected text";
inline constexpr char kMahiSummarizeSelectedDescription[] =
    "Enable Mahi to summarize the selected text";

inline constexpr char kMediaAppImageMantisReimagineName[] =
    "Reimagine feature of Mantis";
inline constexpr char kMediaAppImageMantisReimagineDescription[] =
    "Enable the Reimagine feature of Mantis";

inline constexpr char kMediaAppPdfMahiName[] = "Mahi feature on Media App PDF";
inline constexpr char kMediaAppPdfMahiDescription[] =
    "Enable Mahi feature on PDF files in Gallery app.";

inline constexpr char kMicrophoneMuteSwitchDeviceName[] =
    "Microphone Mute Switch Device";
inline constexpr char kMicrophoneMuteSwitchDeviceDescription[] =
    "Support for detecting the state of hardware microphone mute toggle. Only "
    "effective on devices that have a microphone mute toggle. Enabling the "
    "flag does not affect the toggle functionality, it only affects how the "
    "System UI handles the mute toggle state.";

inline constexpr char kMultiCalendarSupportName[] =
    "Multi-Calendar Support in Quick Settings";
inline constexpr char kMultiCalendarSupportDescription[] =
    "Enables the Quick Settings Calendar to display Google Calendar events for "
    "up to 10 of the user's calendars.";

inline constexpr char kEnableNearbyBleV2Name[] = "Nearby BLE v2";
inline constexpr char kEnableNearbyBleV2Description[] =
    "Enables Nearby BLE v2.";

inline constexpr char kEnableNearbyBleV2ExtendedAdvertisingName[] =
    "Nearby BLE v2 Extended Advertising";
inline constexpr char kEnableNearbyBleV2ExtendedAdvertisingDescription[] =
    "Enables extended advertising functionality over BLE when using Nearby BLE "
    "v2.";

inline constexpr char kEnableNearbyBleV2GattServerName[] =
    "Nearby BLE v2 GATT Server";
inline constexpr char kEnableNearbyBleV2GattServerDescription[] =
    "Enables GATT server functionality over BLE when using Nearby BLE "
    "v2.";

inline constexpr char kEnableNearbyBluetoothClassicAdvertisingName[] =
    "Nearby Bluetooth Classic Advertising";
inline constexpr char kEnableNearbyBluetoothClassicAdvertisingDescription[] =
    "Enables Nearby advertising over Bluetooth Classic.";

inline constexpr char kEnableNearbyMdnsName[] = "Nearby mDNS Discovery";
inline constexpr char kEnableNearbyMdnsDescription[] =
    "Enables Nearby discovery over mDNS.";

inline constexpr char kNearbyPresenceName[] = "Nearby Presence";
inline constexpr char kNearbyPresenceDescription[] =
    "Enables Nearby Presence for scanning and discovery of nearby devices.";

inline constexpr char kNotificationsIgnoreRequireInteractionName[] =
    "Notifications always timeout";
inline constexpr char kNotificationsIgnoreRequireInteractionDescription[] =
    "Always timeout notifications, even if they are set with "
    "requireInteraction.";

inline constexpr char kOnDeviceAppControlsName[] =
    "On-device controls for apps";
inline constexpr char kOnDeviceAppControlsDescription[] =
    "Enables the on-device controls UI for blocking apps.";

inline constexpr char kPcieBillboardNotificationName[] =
    "PCIe billboard notification";
inline constexpr char kPcieBillboardNotificationDescription[] =
    "Enable PCIe peripheral billboard notification.";

inline constexpr char kPhoneHubCallNotificationName[] =
    "Incoming call notification in Phone Hub";
inline constexpr char kPhoneHubCallNotificationDescription[] =
    "Enables the incoming/ongoing call feature in Phone Hub.";

inline constexpr char kPompanoName[] = "Pompano feature";
inline constexpr char kPompanoDescritpion[] =
    "Enable Pompano feature on ChromeOS.";

inline constexpr char kPrintingPpdChannelName[] = "Printing PPD channel";
inline constexpr char kPrintingPpdChannelDescription[] =
    "The channel from which PPD index "
    "is loaded when matching PPD files during printer setup.";

inline constexpr char kPrintPreviewCrosAppName[] =
    "Enable ChromeOS print preview";
inline constexpr char kPrintPreviewCrosAppDescription[] =
    "Enables ChromeOS print preview app.";

inline constexpr char kProjectorAppDebugName[] = "Enable Projector app debug";
inline constexpr char kProjectorAppDebugDescription[] =
    "Adds more informative error messages to the Projector app for debugging";

inline constexpr char kProjectorServerSideSpeechRecognitionName[] =
    "Enable server side speech recognition for Projector";
inline constexpr char kProjectorServerSideSpeechRecognitionDescription[] =
    "Adds server side speech recognition capability to Projector.";

inline constexpr char kProjectorServerSideUsmName[] =
    "Enable USM for Projector server side speech recognition";
inline constexpr char kProjectorServerSideUsmDescription[] =
    "Allows Screencast to use the latest model for server side speech "
    "recognition.";

inline constexpr char kReleaseNotesNotificationAllChannelsName[] =
    "Release Notes Notification All Channels";
inline constexpr char kReleaseNotesNotificationAllChannelsDescription[] =
    "Enables the release notes notification for all ChromeOS channels";

inline constexpr char kReleaseNotesNotificationAlwaysEligibleName[] =
    "Release Notes Notification always eligible";
inline constexpr char kReleaseNotesNotificationAlwaysEligibleDescription[] =
    "Makes the release notes notification always appear regardless of channel, "
    "profile type, and whether or not the notification had already been shown "
    "this milestone. For testing.";

inline constexpr char kRenderArcNotificationsByChromeName[] =
    "Render ARC notifications by ChromeOS";
inline constexpr char kRenderArcNotificationsByChromeDescription[] =
    "Enables rendering ARC notifications using ChromeOS notification framework "
    "if supported";

inline constexpr char kArcWindowPredictorName[] = "Enable ARC window predictor";
inline constexpr char kArcWindowPredictorDescription[] =
    "Enables the window state and bounds predictor for ARC task windows";

inline constexpr char kShelfAutoHideSeparationName[] =
    "Enable separate shelf auto-hide preferences.";
inline constexpr char kShelfAutoHideSeparationDescription[] =
    "Allows for the shelf's auto-hide preference to be specified separately "
    "for clamshell and tablet mode.";

inline constexpr char kShimlessRMAOsUpdateName[] =
    "Enable OS updates in shimless RMA";
inline constexpr char kShimlessRMAOsUpdateDescription[] =
    "Turns on OS updating in Shimless RMA";

inline constexpr char kShimlessRMAHardwareValidationSkipName[] =
    "Enable Hardware Validation Skip in Shimless RMA";
inline constexpr char kShimlessRMAHardwareValidationSkipDescription[] =
    "Turns on Hardware Validation Skip in Shimless RMA";

inline constexpr char kShimlessRMADynamicDeviceInfoInputsName[] =
    "Enable Dynamic Device Info Inputs in Shimless RMA";
inline constexpr char kShimlessRMADynamicDeviceInfoInputsDescription[] =
    "Turns on Dynamic Device Info Inputs in Shimless RMA";

inline constexpr char kSchedulerConfigurationName[] = "Scheduler Configuration";
inline constexpr char kSchedulerConfigurationDescription[] =
    "Instructs the OS to use a specific scheduler configuration setting.";
inline constexpr char kSchedulerConfigurationConservative[] =
    "Disables Hyper-Threading on relevant CPUs.";
inline constexpr char kSchedulerConfigurationPerformance[] =
    "Enables Hyper-Threading on relevant CPUs.";

inline constexpr char kStructuredDnsErrorsName[] = "Structured DNS Errors";
inline constexpr char kStructuredDnsErrorsDescription[] =
    "When enabled, signals support for Structured DNS Errors when sending DNS "
    "requests, renders Extended DNS Error codes on the net error page when "
    "applicable, and interprets filtering details provided via "
    "draft-nottingham-public-resolver-errors-02";

inline constexpr char kMediaDynamicCgroupName[] = "Media Dynamic Cgroup";
inline constexpr char kMediaDynamicCgroupDescription[] =
    "Dynamic Cgroup allows tasks from media workload to be consolidated on "
    "limited cpuset";

inline constexpr char kMissiveStorageName[] =
    "Missive Daemon Storage Configuration";
inline constexpr char kMissiveStorageDescription[] =
    "Provides missive daemon with custom storage configuration parameters";

inline constexpr char kShowBluetoothDebugLogToggleName[] =
    "Show Bluetooth debug log toggle";
inline constexpr char kShowBluetoothDebugLogToggleDescription[] =
    "Enables a toggle which can enable debug (i.e., verbose) logs for "
    "Bluetooth";

inline constexpr char kShowTapsName[] = "Show taps";
inline constexpr char kShowTapsDescription[] =
    "Draws a circle at each touch point, which makes touch points more obvious "
    "when projecting or mirroring the display. Similar to the Android OS "
    "developer option.";

inline constexpr char kShowTouchHudName[] = "Show HUD for touch points";
inline constexpr char kShowTouchHudDescription[] =
    "Shows a trail of colored dots for the last few touch points. Pressing "
    "Ctrl-Alt-I shows a heads-up display view in the top-left corner. Helps "
    "debug hardware issues that generate spurious touch events.";

inline constexpr char kContinuousOverviewScrollAnimationName[] =
    "Makes the gesture for Overview continuous";
inline constexpr char kContinuousOverviewScrollAnimationDescription[] =
    "When a user does the Overview gesture (3 finger swipe), smoothly animates "
    "the transition into Overview as the gesture is done. Allows for the user "
    "to scrub (move forward and backward) through Overview.";

inline constexpr char kSpectreVariant2MitigationName[] =
    "Spectre variant 2 mitigation";
inline constexpr char kSpectreVariant2MitigationDescription[] =
    "Controls whether Spectre variant 2 mitigation is enabled when "
    "bootstrapping the Seccomp BPF sandbox. Can be overridden by "
    "#force-spectre-variant2-mitigation.";

inline constexpr char kSupportF11AndF12ShortcutsName[] = "F11/F12 Shortcuts";
inline constexpr char kSupportF11AndF12ShortcutsDescription[] =
    "Enables settings that "
    "allow users to use shortcuts to remap to the F11 and F12 keys in the "
    "Customize keyboard keys "
    "page.";

inline constexpr char kTerminalDevName[] = "Terminal dev";
inline constexpr char kTerminalDevDescription[] =
    "Enables Terminal System App to load from Downloads for developer testing. "
    "Only works in dev and canary channels.";

inline constexpr char kTetherName[] = "Instant Tethering";
inline constexpr char kTetherDescription[] =
    "Enables Instant Tethering. Instant Tethering allows your nearby Google "
    "phone to share its Internet connection with this device.";

inline constexpr char kTilingWindowResizeName[] =
    "CrOS Labs - Tiling Window Resize";
inline constexpr char kTilingWindowResizeDescription[] =
    "Enables tile-like resizing of windows.";

inline constexpr char kTouchscreenCalibrationName[] =
    "Enable/disable touchscreen calibration option in material design settings";
inline constexpr char kTouchscreenCalibrationDescription[] =
    "If enabled, the user can calibrate the touch screen displays in "
    "chrome://settings/display.";

inline constexpr char kTouchscreenMappingName[] =
    "Enable/disable touchscreen mapping option in material design settings";
inline constexpr char kTouchscreenMappingDescription[] =
    "If enabled, the user can map the touch screen display to the correct "
    "input device in chrome://settings/display.";

inline constexpr char kTrafficCountersEnabledName[] =
    "Traffic counters enabled";
inline constexpr char kTrafficCountersEnabledDescription[] =
    "If enabled, data usage will be visible in the Cellular Settings UI and "
    "traffic counters will be automatically reset if that setting is enabled.";

inline constexpr char kTrafficCountersForWiFiTestingName[] =
    "Traffic counters enabled for WiFi networks";
inline constexpr char kTrafficCountersForWiFiTestingDescription[] =
    "If enabled, data usage will be visible in the Settings UI for WiFi "
    "networks";

inline constexpr char kUploadOfficeToCloudName[] =
    "Enable Office files upload workflow.";

inline constexpr char kUseAnnotatedAccountIdName[] =
    "Use AccountId based mapping between User and BrowserContext";
inline constexpr char kUseAnnotatedAccountIdDescription[] =
    "Uses AccountId annotated for BrowserContext to look up between ChromeOS "
    "User and BrowserContext, a.k.a. Profile.";

inline constexpr char kUseDHCPCD10Name[] = "Use dhcpcd10 for IPv4";
inline constexpr char kUseDHCPCD10Description[] =
    "Use dhcpcd10 for IPv4 provisioning, otherwise the legacy dhcpcd7 "
    "will be used. Note that IPv6 (DHCPv6-PD) will always use dhcpcd10.";

inline constexpr char kUseFakeDeviceForMediaStreamName[] =
    "Use fake video capture device";
inline constexpr char kUseFakeDeviceForMediaStreamDescription[] =
    "Forces Chrome to use a fake video capture device (a rolling pacman with a "
    "timestamp) instead of the system audio/video devices, for debugging "
    "purposes.";

inline constexpr char kUiDevToolsName[] = "Enable native UI inspection";
inline constexpr char kUiDevToolsDescription[] =
    "Enables inspection of native UI elements. For local inspection use "
    "chrome://inspect#other";

inline constexpr char kUiSlowAnimationsName[] = "Slow UI animations";
inline constexpr char kUiSlowAnimationsDescription[] =
    "Makes all UI animations slow.";

inline constexpr char kUnicornChromeActivityReportingName[] =
    "Chrome app activity reporting for supervised users";
inline constexpr char kUnicornChromeActivityReportingDescription[] =
    "Enables reporting Chrome app activity for supervised users.";

inline constexpr char kVcDlcUiName[] = "VC DLC UI";
inline constexpr char kVcDlcUiDescription[] =
    "Enable UI for video conference effect toggle tiles in the video "
    "conference controls bubble that indicates when required DLC is "
    "downloading.";

inline constexpr char kVirtualKeyboardName[] = "Virtual Keyboard";
inline constexpr char kVirtualKeyboardDescription[] =
    "Always show virtual keyboard regardless of having a physical keyboard "
    "present";

inline constexpr char kVirtualKeyboardDisabledName[] =
    "Disable Virtual Keyboard";
inline constexpr char kVirtualKeyboardDisabledDescription[] =
    "Always disable virtual keyboard regardless of device mode. Workaround for "
    "virtual keyboard showing with some external keyboards.";

inline constexpr char kWakeOnWifiAllowedName[] =
    "Allow enabling wake on WiFi features";
inline constexpr char kWakeOnWifiAllowedDescription[] =
    "Allows wake on WiFi features in shill to be enabled.";

inline constexpr char kWelcomeExperienceName[] = "Welcome Experience";
inline constexpr char kWelcomeExperienceDescription[] =
    "Enables a new Welcome Experience for first-time peripheral connections.";

inline constexpr char kWelcomeExperienceTestUnsupportedDevicesName[] =
    "Welcome Experience test unsupported devices";
inline constexpr char kWelcomeExperienceTestUnsupportedDevicesDescription[] =
    "kWelcomeExperienceTestUnsupportedDevices enables the new device Welcome "
    "Experience to be tested on external devices that are not officially "
    "supported. When enabled, users will be able to initiate and complete "
    "the enhanced Welcome Experience flow using these unsupported external "
    "devices. This flag is intended for testing purposes and should be "
    "disabled in production environments.";

inline constexpr char kWelcomeTourName[] = "Welcome Tour";
inline constexpr char kWelcomeTourDescription[] =
    "Enables the Welcome Tour that walks new users through ChromeOS System UI.";

inline constexpr char kWelcomeTourForceUserEligibilityName[] =
    "Force Welcome Tour user eligibility";
inline constexpr char kWelcomeTourForceUserEligibilityDescription[] =
    "Forces user eligibility for the Welcome Tour that walks new users through "
    "ChromeOS System UI. Enabling this flag has no effect unless the Welcome "
    "Tour is also enabled.";

inline constexpr char kWifiConnectMacAddressRandomizationName[] =
    "MAC address randomization";
inline constexpr char kWifiConnectMacAddressRandomizationDescription[] =
    "Randomize MAC address when connecting to unmanaged (non-enterprise) "
    "WiFi networks.";

inline constexpr char kWifiConcurrencyName[] = "WiFi Concurrency";
inline constexpr char kWifiConcurrencyDescription[] =
    "When enabled, it uses new WiFi concurrency Shill APIs to start station "
    "WiFi and tethering.";

inline constexpr char kWindowSplittingName[] = "CrOS Labs - Window splitting";
inline constexpr char kWindowSplittingDescription[] =
    "Enables splitting windows by dragging one over another.";

inline constexpr char kLauncherKeyShortcutInBestMatchName[] =
    "Enable keyshortcut results in best match";
inline constexpr char kLauncherKeyShortcutInBestMatchDescription[] =
    "When enabled, it allows key shortcut results to appear in best match and "
    "answer card in launcher.";

inline constexpr char kLauncherKeywordExtractionScoring[] =
    "Query keyword extraction and scoring in launcher";
inline constexpr char kLauncherKeywordExtractionScoringDescription[] =
    "Enables extraction of keywords from query then calculate score from "
    "extracted keyword in the launcher.";

inline constexpr char kLauncherLocalImageSearchName[] =
    "Enable launcher local image search";
inline constexpr char kLauncherLocalImageSearchDescription[] =
    "Enables on-device local image search in the launcher.";

inline constexpr char kLauncherLocalImageSearchConfidenceName[] =
    "Launcher Local Image Search Confidence";
inline constexpr char kLauncherLocalImageSearchConfidenceDescription[] =
    "Allows configurations of the experiment parameters for local image search "
    "confidence threshold in the launcher.";

inline constexpr char kLauncherLocalImageSearchRelevanceName[] =
    "Launcher Local Image Search Relevance";
inline constexpr char kLauncherLocalImageSearchRelevanceDescription[] =
    "Allows configurations of the experiment parameters for local image search "
    "Relevance threshold in the launcher.";

inline constexpr char kLauncherLocalImageSearchOcrName[] =
    "Enable OCR for local image search";
inline constexpr char kLauncherLocalImageSearchOcrDescription[] =
    "Enables on-device Optical Character Recognition for local image search in "
    "the launcher.";

inline constexpr char kLauncherLocalImageSearchIcaName[] =
    "Enable ICA for local image search";
inline constexpr char kLauncherLocalImageSearchIcaDescription[] =
    "Enables on-device Image Content-based Annotation for local image search "
    "in the launcher.";

inline constexpr char kMacAddressRandomizationName[] =
    "MAC address randomization";
inline constexpr char kMacAddressRandomizationDescription[] =
    "Feature to allow MAC address randomization to be enabled for WiFi "
    "networks.";

inline constexpr char kSysUiShouldHoldbackDriveIntegrationName[] =
    "Holdback for Drive Integration on chromeOS";
inline constexpr char kSysUiShouldHoldbackDriveIntegrationDescription[] =
    "Enables holdback for Drive Integration.";

inline constexpr char kSysUiShouldHoldbackTaskManagementName[] =
    "Holdback for Task Management on chromeOS";
inline constexpr char kSysUiShouldHoldbackTaskManagementDescription[] =
    "Enables holdback for Task Management.";

inline constexpr char kTetheringExperimentalFunctionalityName[] =
    "Tethering Allow Experimental Functionality";
inline constexpr char kTetheringExperimentalFunctionalityDescription[] =
    "Feature to enable Chromebook hotspot functionality for experimental "
    "carriers, modem and modem FW.";

// Prefer keeping this section sorted to adding new definitions down here.

inline constexpr char kAddPrinterViaPrintscanmgrName[] =
    "Uses printscanmgr to add printers";
inline constexpr char kAddPrinterViaPrintscanmgrDescription[] =
    "Changes the daemon used to add printers from debugd to printscanmgr.";

inline constexpr char kCrOSDspBasedAecAllowedName[] =
    "Allow CRAS to use a DSP-based AEC if available";
inline constexpr char kCrOSDspBasedAecAllowedDescription[] =
    "Allows the system variant of the AEC in CRAS to be run on DSP ";

inline constexpr char kCrOSDspBasedNsAllowedName[] =
    "Allow CRAS to use a DSP-based NS if available";
inline constexpr char kCrOSDspBasedNsAllowedDescription[] =
    "Allows the system variant of the NS in CRAS to be run on DSP ";

inline constexpr char kCrOSDspBasedAgcAllowedName[] =
    "Allow CRAS to use a DSP-based AGC if available";
inline constexpr char kCrOSDspBasedAgcAllowedDescription[] =
    "Allows the system variant of the AGC in CRAS to be run on DSP ";

inline constexpr char kCrOSEnforceMonoAudioCaptureName[] =
    "Enforce mono audio capture for Chrome";
inline constexpr char kCrOSEnforceMonoAudioCaptureDescription[] =
    "Enforce mono audio capture instead of stereo capture for Chrome on "
    "ChromeOS";

inline constexpr char kCrOSEnforceSystemAecName[] =
    "Enforce using the system AEC in CrAS";
inline constexpr char kCrOSEnforceSystemAecDescription[] =
    "Enforces using the system variant in CrAS of the AEC";

inline constexpr char kCrOSEnforceSystemAecAgcName[] =
    "Enforce using the system AEC and AGC in CrAS";
inline constexpr char kCrOSEnforceSystemAecAgcDescription[] =
    "Enforces using the system variants in CrAS of the AEC and AGC.";

inline constexpr char kCrOSEnforceSystemAecNsName[] =
    "Enforce using the system AEC and NS in CrAS";
inline constexpr char kCrOSEnforceSystemAecNsDescription[] =
    "Enforces using the system variants in CrAS of the AEC and NS.";

inline constexpr char kCrOSEnforceSystemAecNsAgcName[] =
    "Enforce using the system AEC, NS and AGC in CrAS";
inline constexpr char kCrOSEnforceSystemAecNsAgcDescription[] =
    "Enforces using the system variants in CrAS of the AEC, NS and AGC.";

inline constexpr char kIgnoreUiGainsName[] =
    "Ignore UI Gains in system mic gain setting";
inline constexpr char kIgnoreUiGainsDescription[] =
    "Ignore UI Gains in system mic gain setting";

inline constexpr char kShowForceRespectUiGainsToggleName[] =
    "Enable a setting toggle to force respect UI gains";
inline constexpr char kShowForceRespectUiGainsToggleDescription[] =
    "Enable a setting toggle to force respect UI gains.";

inline constexpr char kCrOSSystemVoiceIsolationOptionName[] =
    "Enable the options of setting system voice isolation per stream";
inline constexpr char kCrOSSystemVoiceIsolationOptionDescription[] =
    "Enable the options of setting system voice isolation per stream.";

inline constexpr char kShowSpatialAudioToggleName[] =
    "Enable a setting toggle for spatial audio";
inline constexpr char kShowSpatialAudioToggleDescription[] =
    "Enable a setting toggle for spatial audio.";

inline constexpr char kSingleCaCertVerificationPhase0Name[] =
    "Use single CA cert for EAP networks if provided phase 0";
inline constexpr char kSingleCaCertVerificationPhase0Description[] =
    "Only collect data for server certificate verification failure.";

inline constexpr char kSingleCaCertVerificationPhase1Name[] =
    "Use single CA cert for EAP networks if provided phase 1";
inline constexpr char kSingleCaCertVerificationPhase1Description[] =
    "Use a single CA cert for server's cert verification with fallback to"
    "the old config.";

inline constexpr char kSingleCaCertVerificationPhase2Name[] =
    "Use single CA cert for EAP networks if provided phase 2";
inline constexpr char kSingleCaCertVerificationPhase2Description[] =
    "Use a single CA cert for server's cert verification, no fallback.";

inline constexpr char kCrosSeparateGeoApiKeyName[] =
    "Use ChromeOS-specific API keys for location resolution";
inline constexpr char kCrosSeparateGeoApiKeyDescription[] =
    "If enabled, ChromeOS system services and Chrome-on-ChromeOS will use "
    "different API keys and GCP endpoint to resolve location.";

inline constexpr char kCrosCachedLocationProviderName[] =
    "Use Caching in System Location Provider to optimize GCP utilization";
inline constexpr char kCrosCachedLocationProviderDescription[] =
    "If enabled, System Location Provider will cache last resolved location "
    "and reuse it in subsequent calls, when deemed necessary. Cache eviction "
    "algorithm will be based on elapsed time and proximity information such as"
    "wifi/cellular scan data. Enabling this feature will NOT incur extra power "
    "overhead.";

inline constexpr char kDisableIdleSocketsCloseOnMemoryPressureName[] =
    "Disable closing idle sockets on memory pressure";
inline constexpr char kDisableIdleSocketsCloseOnMemoryPressureDescription[] =
    "If enabled, idle sockets will not be closed when chrome detects memory "
    "pressure. This applies to web pages only and not to internal requests.";

inline constexpr char kLockedModeName[] = "Enable the Locked Mode API.";
inline constexpr char kLockedModeDescription[] =
    "Enabled the Locked Mode Web API which allows admin-allowlisted sites "
    "to enter a locked down fullscreen mode.";

inline constexpr char kOneGroupPerRendererName[] =
    "Use one cgroup for each foreground renderer";
inline constexpr char kOneGroupPerRendererDescription[] =
    "Places each Chrome foreground renderer into its own cgroup";

inline constexpr char kPlatformKeysChangesWave1Name[] =
    "Platform Keys Changes Wave 1";
inline constexpr char kPlatformKeysChangesWave1Description[] =
    "Enables the first wave of new features for the "
    "chrome.enterprise.platformKeys API. That includes supporting the "
    "\"RSA-OAEP\" key type with the \"unwrapKey\" key usage and adding the "
    "setKeyTag() API method to mark keys for future lookup.";

inline constexpr char kPrintPreviewCrosPrimaryName[] =
    "Enables the ChromeOS print preview to be the primary print preview.";
inline constexpr char kPrintPreviewCrosPrimaryDescription[] =
    "Allows the ChromeOS print preview to be opened instead of the browser "
    " print preview.";

inline constexpr char kDisableQuickAnswersV2TranslationName[] =
    "Disable Quick Answers Translation";
inline constexpr char kDisableQuickAnswersV2TranslationDescription[] =
    "Disable translation services of the Quick Answers.";

inline constexpr char kQuickAnswersRichCardName[] =
    "Enable Quick Answers Rich Card";
inline constexpr char kQuickAnswersRichCardDescription[] =
    "Enable rich card views of the Quick Answers feature.";

inline constexpr char kQuickAnswersMaterialNextUIName[] =
    "Enable Quick Answers Material Next UI";
inline constexpr char kQuickAnswersMaterialNextUIDescription[] =
    "Enable Material Next UI for the Quick Answers feature. This is effective "
    "only if Magic Boost flag is off. Note that this will be changed as this "
    "is effective only if a device is eligible to Magic Boost when the Magic "
    "Boost flag gets flipped.";

inline constexpr char kWebPrintingApiName[] = "Web Printing API";
inline constexpr char kWebPrintingApiDescription[] =
    "Enable access to the Web Printing API. See "
    "https://github.com/WICG/web-printing for details.";

inline constexpr char kChromeOSHWVBREncodingName[] =
    "ChromeOS Hardware Variable Bitrate Encoding";
inline constexpr char kChromeOSHWVBREncodingDescription[] =
    "Enables the hardware-accelerated variable bitrate (VBR) encoding on "
    "ChromeOS. If the hardware encoder supports VBR for a specified codec, a "
    "video is recorded in VBR encoding in MediaRecoder API automatically and "
    "WebCodecs API if configured so.";
inline constexpr char kUseGLForScalingName[] =
    "Use GL image processor for scaling";
inline constexpr char kUseGLForScalingDescription[] =
    "Use the GL image processor for scaling over libYUV implementations.";
inline constexpr char kPreferGLImageProcessorName[] =
    "Prefer GL image processor";
inline constexpr char kPreferGLImageProcessorDescription[] =
    "Prefers the GL image processor for format conversion of video frames over"
    " both the libYUV and hardware implementations";
inline constexpr char kPreferSoftwareMT21Name[] =
    "Prefer software MT21 conversion";
inline constexpr char kPreferSoftwareMT21Description[] =
    "Prefer using the software MT21 conversion instead of the MDP hardware "
    "conversion on MT8173 devices.";
inline constexpr char kEnableProtectedVulkanDetilingName[] =
    "Enable Protected Vulkan Detiling";
inline constexpr char kEnableProtectedVulkanDetilingDescription[] =
    "Use a Vulkan shader for protected Vulkan detiling.";
inline constexpr char kEnableArmHwdrm10bitOverlaysName[] =
    "Enable ARM HW DRM 10-bit Overlays";
inline constexpr char kEnableArmHwdrm10bitOverlaysDescription[] =
    "Enable 10-bit overlays for ARM HW DRM content. If disabled, 10-bit "
    "HW DRM content will be subsampled to 8-bit before scanout. This flag "
    "has no effect on 8-bit content.";
inline constexpr char kEnableArmHwdrmName[] = "Enable ARM HW DRM";
inline constexpr char kEnableArmHwdrmDescription[] =
    "Enable HW backed Widevine L1 DRM";

// Linux -----------------------------------------------------------------------

inline constexpr char kPulseaudioLoopbackForCastName[] =
    "Linux System Audio Loopback for Cast (pulseaudio)";
inline constexpr char kPulseaudioLoopbackForCastDescription[] =
    "Enable system audio mirroring when casting a screen on Linux with "
    "pulseaudio.";

inline constexpr char kPulseaudioLoopbackForScreenShareName[] =
    "Linux System Audio Loopback for Screen Sharing (pulseaudio)";
inline constexpr char kPulseaudioLoopbackForScreenShareDescription[] =
    "Enable system audio sharing when screen sharing on Linux with pulseaudio.";

inline constexpr char kWaylandLinuxDrmSyncobjName[] =
    "Wayland linux-drm-syncobj explicit sync";
inline constexpr char kWaylandLinuxDrmSyncobjDescription[] =
    "Enable Wayland's explicit sync support using linux-drm-syncobj."
    "Requires minimum kernel version v6.11.";

inline constexpr char kWaylandPerWindowScalingName[] =
    "Wayland per-window scaling";
inline constexpr char kWaylandPerWindowScalingDescription[] =
    "Enable Wayland's per-window scaling experimental support.";

inline constexpr char kWaylandSessionManagementName[] =
    "Wayland session management";
inline constexpr char kWaylandSessionManagementDescription[] =
    "Enable Wayland's xx/xdg-session-management-v1 experimental support.";

// Random platform combinations -----------------------------------------------

inline constexpr char kZeroCopyVideoCaptureName[] =
    "Enable Zero-Copy Video Capture";
inline constexpr char kZeroCopyVideoCaptureDescription[] =
    "Camera produces a gpu friendly buffer on capture and, if there is, "
    "hardware accelerated video encoder consumes the buffer";

inline constexpr char kLocalNetworkAccessChecksName[] =
    "Local Network Access Checks";
inline constexpr char kLocalNetworkAccessChecksDescription[] =
    "Enables Local Network Access checks. "
    "See: https://chromestatus.com/feature/5152728072060928";

inline constexpr char kLocalNetworkAccessChecksWebRTCName[] =
    "Local Network Access Checks for WebRTC";
inline constexpr char kLocalNetworkAccessChecksWebRTCDescription[] =
    "Enable Local Network Access checks for WebRTC. Requires the "
    "#local-network-access-check flag to also be enabled "
    "See: https://chromestatus.com/feature/5065884686876672";

inline constexpr char kLocalNetworkAccessChecksWebSocketsName[] =
    "Local Network Access Checks for WebSockets";
inline constexpr char kLocalNetworkAccessChecksWebSocketsDescription[] =
    "Enable Local Network Access checks for WebSockets. Requires the "
    "#local-network-access-check flag to also be enabled "
    "See: https://chromestatus.com/feature/5197681148428288";

inline constexpr char kLocalNetworkAccessChecksWebTransportName[] =
    "Local Network Access Checks for WebTransport";
inline constexpr char kLocalNetworkAccessChecksWebTransportDescription[] =
    "Enable Local Network Access checks for WebTransport. Requires the "
    "#local-network-access-check flag to also be enabled "
    "See: https://chromestatus.com/feature/5126430912544768";

inline constexpr char kTaskManagerDesktopRefreshName[] =
    "Task Manager Desktop Refresh";
inline constexpr char kTaskManagerDesktopRefreshDescription[] =
    "Enables a refreshed design for the Task Manager on Desktop platforms.";

inline constexpr char kGroupPromoPrototypeName[] = "Group Promo Prototype";
inline constexpr char kGroupPromoPrototypeDescription[] =
    "Enables prototype for group promo.";

inline constexpr char kEnableNetworkServiceSandboxName[] =
    "Enable the network service sandbox.";
inline constexpr char kEnableNetworkServiceSandboxDescription[] =
    "Enables a sandbox around the network service to help mitigate exploits in "
    "its process. This may cause crashes if Kerberos is used.";

inline constexpr char kUseOutOfProcessVideoDecodingName[] =
    "Use out-of-process video decoding (OOP-VD)";
inline constexpr char kUseOutOfProcessVideoDecodingDescription[] =
    "Start utility processes to do hardware video decoding.";

inline constexpr char kUseSharedImageInOOPVDName[] =
    "Use Shared Image in OOP-VD";
inline constexpr char kUseSharedImageInOOPVDDescription[] =
    "Use shared image interface to transport video frame resources in out of "
    "process video decoding.";

inline constexpr char kWebBluetoothConfirmPairingSupportName[] =
    "Web Bluetooth confirm pairing support";
inline constexpr char kWebBluetoothConfirmPairingSupportDescription[] =
    "Enable confirm-only and confirm-pin pairing mode support for Web "
    "Bluetooth";

inline constexpr char kCupsIppPrintingBackendName[] =
    "CUPS IPP Printing Backend";
inline constexpr char kCupsIppPrintingBackendDescription[] =
    "Use the CUPS IPP printing backend instead of the original CUPS backend "
    "that calls the PPD API.";

inline constexpr char kChromeWideEchoCancellationName[] =
    "Chrome-wide echo cancellation";
inline constexpr char kChromeWideEchoCancellationDescription[] =
    "Run WebRTC capture audio processing in the audio process instead of the "
    "renderer processes, thereby cancelling echoes from more audio sources.";

inline constexpr char kDcheckIsFatalName[] = "DCHECKs are fatal";
inline constexpr char kDcheckIsFatalDescription[] =
    "By default Chrome will evaluate in this build, but only log failures, "
    "rather than crashing. If enabled, DCHECKs will crash the calling process.";

inline constexpr char kDocumentPatchingName[] = "Document patching";
inline constexpr char kDocumentPatchingDescription[] =
    "Allow out-of-order streaming of HTML content using <template patchfor> "
    "and node.patchSelf(). "
    "See https://github.com/WICG/declarative-partial-updates";

inline constexpr char kRouteMatchingName[] = "Route matching";
inline constexpr char kRouteMatchingDescription[] =
    "Allow definition of routes as e.g. URLPattern. Special CSS rules can be "
    "used to match active routes. See "
    "https://github.com/WICG/declarative-partial-updates/blob/main/"
    "route-matching-explainer.md";

inline constexpr char kEnableOopPrintDriversName[] =
    "Enables Out-of-Process Printer Drivers";
inline constexpr char kEnableOopPrintDriversDescription[] =
    "Enables printing interactions with the operating system to be performed "
    "out-of-process.";

inline constexpr char kPaintPreviewDemoName[] = "Paint Preview Demo";
inline constexpr char kPaintPreviewDemoDescription[] =
    "If enabled a menu item is added to the Android main menu to demo paint "
    "previews.";

inline constexpr char kAccessiblePDFFormName[] = "Accessible PDF Forms";
inline constexpr char kAccessiblePDFFormDescription[] =
    "Enables accessibility support for PDF forms.";

inline constexpr char kPdfInk2Name[] = "PDF Ink Signatures";
inline constexpr char kPdfInk2Description[] =
    "Enables the ability to annotate PDFs using a new ink library.";

inline constexpr char kPdfSaveToDriveName[] = "Save PDF to Drive";
inline constexpr char kPdfSaveToDriveDescription[] =
    "Enables the ability to save PDFs to Google Drive.";

inline constexpr char kPdfOopifName[] = "OOPIF for PDF Viewer";
inline constexpr char kPdfOopifDescription[] =
    "Use an OOPIF for the PDF Viewer, instead of a GuestView.";

inline constexpr char kPdfPortfolioName[] = "PDF portfolio";
inline constexpr char kPdfPortfolioDescription[] =
    "Enable PDF portfolio feature.";

inline constexpr char kPdfUseSkiaRendererName[] = "Use Skia Renderer";
inline constexpr char kPdfUseSkiaRendererDescription[] =
    "Use Skia as the PDF renderer. This flag will have no effect if the "
    "renderer choice is controlled by an enterprise policy.";

inline constexpr char kWebXrProjectionLayersName[] = "WebXR Projection Layers";
inline constexpr char kWebXrProjectionLayersDescription[] =
    "Enables use of XRProjectionLayers.";
inline constexpr char kWebXrWebGpuBindingName[] = "WebXR/WebGPU Binding";
inline constexpr char kWebXrWebGpuBindingDescription[] =
    "Enables rendering with WebGPU for WebXR sessions. WebXR Projection "
    "Layers must be also be enabled to use this feature.";
inline constexpr char kWebXrInternalsName[] = "WebXR Internals Debugging Page";
inline constexpr char kWebXrInternalsDescription[] =
    "Enables the webxr-internals developer page which can be used to help "
    "debug issues with the WebXR Device API.";

inline constexpr char kOpenXrSpatialEntitiesName[] = "OpenXR Spatial Entities";
inline constexpr char kOpenXrSpatialEntitiesDescription[] =
    "Allows the OpenXR runtime to use the spatial entities set of extensions "
    "to understand the environment.";

inline constexpr char kAutofillCreditCardUploadName[] =
    "Enable offering upload of Autofilled credit cards";
inline constexpr char kAutofillCreditCardUploadDescription[] =
    "Enables a new option to upload credit cards to Google Payments for sync "
    "to all Chrome devices.";

inline constexpr char kElasticOverscrollName[] = "Elastic Overscroll";
inline constexpr char kElasticOverscrollDescription[] =
    "Enables Elastic Overscrolling on touchscreens and precision touchpads.";

inline constexpr char kElementCaptureName[] = "Element Capture";
inline constexpr char kElementCaptureDescription[] =
    "Enables Element Capture - an API allowing the mutation of a tab-capture "
    "media track into a track capturing just a specific DOM element.";

inline constexpr char kUIDebugToolsName[] = "Debugging tools for UI";
inline constexpr char kUIDebugToolsDescription[] =
    "Enables additional keyboard shortcuts to help debugging.";

inline constexpr char kWebrtcPipeWireCameraName[] = "PipeWire Camera support";
inline constexpr char kWebrtcPipeWireCameraDescription[] =
    "When enabled the PipeWire multimedia server will be used for cameras.";

inline constexpr char kEnableAudioFocusEnforcementName[] =
    "Audio Focus Enforcement";
inline constexpr char kEnableAudioFocusEnforcementDescription[] =
    "Enables enforcement of a single media session having audio focus at "
    "any one time. Requires #enable-media-session-service to be enabled too.";

inline constexpr char kComposeSelectionNudgeName[] = "Compose Selection Nudge";
inline constexpr char kComposeSelectionNudgeDescription[] =
    "Enables nudge on selection for Compose";

inline constexpr char kGlicName[] = "Glic";
inline constexpr char kGlicDescription[] = "Enables glic";

inline constexpr char kGlicZOrderChangesName[] = "Glic Z Order Changes";
inline constexpr char kGlicZOrderChangesDescription[] =
    "Enables glic z order changing";

inline constexpr char kDesktopPWAsUserLinkCapturingScopeExtensionsName[] =
    "Desktop PWA Link Capturing with Scope Extensions";
inline constexpr char
    kDesktopPWAsUserLinkCapturingScopeExtensionsDescription[] =
        "Allows the 'Desktop PWA Scope Extensions' feature to be used with the "
        "'Desktop PWA Link Capturing' feature. Both of those features are "
        "required "
        "to be turned on for this flag to have an effect.";

inline constexpr char kEnableGenericOidcAuthProfileManagementName[] =
    "Enable generic OIDC profile management";
inline constexpr char kEnableGenericOidcAuthProfileManagementDescription[] =
    "Enables profile management triggered by generic OIDC authentications.";

inline constexpr char kEnableOidcProfileRemoteCommandsName[] =
    "Enable OIDC profile remote commands";
inline constexpr char kEnableOidcProfileRemoteCommandsDescription[] =
    "Enables remote commands for OIDC profiles.";

inline constexpr char kEnableHlsPlaybackName[] =
    "Enable direct playback of HLS manifests";
inline constexpr char kEnableHlsPlaybackDescription[] =
    "Enables built-in HLS player for adaptive playback and live streams.";

inline constexpr char kProfilesReorderingName[] = "Profiles Reordering";
inline constexpr char kProfilesReorderingDescription[] =
    "Enables profiles reordering in the Profile Picker main view by drag and "
    "dropping the Profile Tiles. The order is saved when changed and "
    "persisted.";

inline constexpr char kEnableChromeRefreshTokenBindingName[] =
    "Chrome Refresh Token Binding";
inline constexpr char kEnableChromeRefreshTokenBindingDescription[] =
    "Enables binding of Chrome refresh tokens to cryptographic keys.";

inline constexpr char kEnableOAuthMultiloginCookiesBindingName[] =
    "Enable OAuthMultilogin Cookies Binding";
inline constexpr char kEnableOAuthMultiloginCookiesBindingDescription[] =
    "Enables binding of cookies returned from OAuthMultilogin to cryptographic "
    "keys.";

inline constexpr char
    kEnableOAuthMultiloginCookiesBindingServerExperimentName[] =
        "Enable OAuthMultilogin Cookies Binding Server Experiment";
inline constexpr char
    kEnableOAuthMultiloginCookiesBindingServerExperimentDescription[] =
        "When enabled, Chrome will send will send a specific URL parameter to "
        "Gaia "
        "to trigger the server-side experiment for binding the OAuthMultilogin "
        "cookies to cryptographic keys. This flag is meant to be used in "
        "conjunction with the 'Enable OAuthMultilogin Cookies Binding' flag.";

inline constexpr char kEnableBoundSessionCredentialsName[] =
    "Device Bound Session Credentials";
inline constexpr char kEnableBoundSessionCredentialsDescription[] =
    "Enables Google session credentials binding to cryptographic keys.";

inline constexpr char
    kEnableBoundSessionCredentialsSoftwareKeysForManualTestingName[] =
        "Device Bound Session Credentials with software keys";
inline constexpr char
    kEnableBoundSessionCredentialsSoftwareKeysForManualTestingDescription[] =
        "Enables mock software-backed cryptographic keys for Google session "
        "credentials binding and Chrome refresh tokens binding (not secure). "
        "This is intended to be used for manual testing only.";

inline constexpr char kEnableStandardBoundSessionCredentialsName[] =
    "Device Bound Session Credentials (Standard)";
inline constexpr char kEnableStandardBoundSessionCredentialsDescription[] =
    "Enables the official version of Device Bound Session Credentials. For "
    "more information see https://github.com/WICG/dbsc.";
inline constexpr char kEnableStandardBoundSessionPersistenceName[] =
    "Device Bound Session Credentials (Standard) Persistence";
inline constexpr char kEnableStandardBoundSessionPersistenceDescription[] =
    "Enables session persistence for the official version of "
    "Device Bound Session Credentials.";
inline constexpr char
    kEnableStandardBoundSessionCredentialsFederatedSessionsName[] =
        "Device Bound Session Credentials (Standard) - Federated Registrations";
inline constexpr char
    kEnableStandardBoundSessionCredentialsFederatedSessionsDescription[] =
        "Enables federated session registration for the official version of "
        "Device Bound Session Credentials.";

inline constexpr char kEnablePolicyPromotionBannerName[] =
    "Enable Policy Promotion Banner";
inline constexpr char kEnablePolicyPromotionBannerDescription[] =
    "Enables showing the policy promotion banner on chrome://policy page.";
inline constexpr char kEnableManagementPromotionBannerName[] =
    "Enable Management Promotion Banner";
inline constexpr char kEnableManagementPromotionBannerDescription[] =
    "Enables showing the management promotion banner on chrome://management "
    "page.";

inline constexpr char kSupervisedUserBlockInterstitialV3Name[] =
    "Enable URL filter interstitial V3";
inline constexpr char kSupervisedUserBlockInterstitialV3Description[] =
    "Enables URL filter interstitial V3 for Family Link users.";

inline constexpr char kAllowUserInstalledChromeAppsName[] =
    "Allow user installed Chrome Apps";
inline constexpr char kAllowUserInstalledChromeAppsDescription[] =
    "Enables users to override the Chrome Apps deprecation for apps installed "
    "by users.";

inline constexpr char kVariationsSeedCorpusName[] = "Variations seed corpus";
inline constexpr char kVariationsSeedCorpusDescription[] =
    "The value of the 'corpus' parameter in the variations seed request. "
    "If unspecified, the 'corpus' parameter is omitted from the request.";

inline constexpr char kHandleMdmErrorsForDasherAccountsName[] =
    "Mdm error handling for dasher accounts";
inline constexpr char kHandleMdmErrorsForDasherAccountsDescription[] =
    "Enables the mdm error handling feature for dasher accounts";

// ============================================================================
// Don't just add flags to the end, put them in the alphabetical order.
// ============================================================================

}  // namespace flag_descriptions

#endif  // CHROME_BROWSER_FLAG_DESCRIPTIONS_H_
