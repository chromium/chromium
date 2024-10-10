// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/flag_descriptions.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "flag_descriptions.h"
#include "media/gpu/buildflags.h"
#include "pdf/buildflags.h"

// Keep in identical order as the header file, see the comment at the top
// for formatting rules.

namespace flag_descriptions {

const char kAccelerated2dCanvasName[] = "Accelerated 2D canvas";
const char kAccelerated2dCanvasDescription[] =
    "Enables the use of the GPU to perform 2d canvas rendering instead of "
    "using software rendering.";

const char kCanvasHibernationName[] = "Hibernation for 2D canvas";
const char kCanvasHibernationDescription[] =
    "Enables canvas hibernation for 2D canvas.";

const char kCanvasOopRasterizationName[] =
    "Out-of-process 2D canvas rasterization.";
const char kCanvasOopRasterizationDescription[] =
    "The rasterization of 2d canvas contents is performed in the GPU process. "
    "Requires that out-of-process rasterization be enabled.";

const char kCapturedSurfaceControlName[] = "Captured Surface Control";
const char kCapturedSurfaceControlDescription[] =
    "Enables an API that allows an application to control scroll and zoom on "
    "the tab which it is capturing.";

const char kAcceleratedVideoDecodeName[] = "Hardware-accelerated video decode";
const char kAcceleratedVideoDecodeDescription[] =
    "Hardware-accelerated video decode where available.";

const char kAcceleratedVideoEncodeName[] = "Hardware-accelerated video encode";
const char kAcceleratedVideoEncodeDescription[] =
    "Hardware-accelerated video encode where available.";

const char kAlignWakeUpsName[] = "Align delayed wake ups at 125 Hz";
const char kAlignWakeUpsDescription[] =
    "Run most delayed tasks with a non-zero delay (including DOM Timers) on a "
    "periodic 125Hz tick, instead of as soon as their delay has passed.";

const char kAllowInsecureLocalhostName[] =
    "Allow invalid certificates for resources loaded from localhost.";
const char kAllowInsecureLocalhostDescription[] =
    "Allows requests to localhost over HTTPS even when an invalid certificate "
    "is presented.";

const char kAndroidAppIntegrationName[] = "Integrate with Android App Search";
const char kAndroidAppIntegrationDescription[] =
    "If enabled, allows Chrome to integrate with the Android App Search.";

const char kAndroidAppIntegrationWithFaviconName[] =
    "Integrate with Android App Search with favicons";
const char kAndroidAppIntegrationWithFaviconDescription[] =
    "If enabled, allows Chrome to integrate with the Android App Search with "
    "favicons.";

const char kAndroidBottomToolbarName[] = "Bottom Toolbar";
const char kAndroidBottomToolbarDescription[] =
    "If enabled, displays the toolbar at the bottom.";

const char kAndroidExtendedKeyboardShortcutsName[] =
    "Android Extended Keyboard Shortcuts";
const char kAndroidExtendedKeyboardShortcutsDescription[] =
    "If enabled, allows for extended keyboard shortcuts (i.e. Alt + Backspace "
    "to delete line).";

const char kAndroidNoSurfaceSyncForBrowserControlsName[] =
    "Android No Surface Sync For Browser Controls";
const char kAndroidNoSurfaceSyncForBrowserControlsDescription[] =
    "Disable surface sync for top controls when scrolling";

const char kAndroidBrowserControlsInVizName[] =
    "Android Browser Controls in Viz";
const char kAndroidBrowserControlsInVizDescription[] =
    "Let viz move browser controls when scrolling. For now, this applies only "
    "to top controls.";

const char kAnimatedImageResumeName[] = "Use animated image resume behavior";
const char kAnimatedImageResumeDescription[] =
    "Resumes animated images from the last frame drawn rather than attempt "
    "to catch up to the frame that should be drawn based on current time.";

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kAnnotatorModeName[] = "Enable annotator tool";
const char kAnnotatorModeDescription[] =
    "Enables the tool for annotating across the OS.";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

const char kAriaElementReflectionName[] = "Enable ARIA element reflection";
const char kAriaElementReflectionDescription[] =
    "Enable setting ARIA relationship attributes that reference other elements "
    "directly without an IDREF";

const char kAutomotiveFullscreenToolbarImprovementsName[] =
    "Enable automotive fullscreen toolbar improvements.";
const char kAutomotiveFullscreenToolbarImprovementsDescription[] =
    "In automotive, hides the toolbar when entering fullscreen mode. Swipe on"
    "edge to reshow the toolbar.";

const char kAuxiliarySearchDonationName[] = "Auxiliary Search Donation";
const char kAuxiliarySearchDonationDescription[] =
    "If enabled, override Auxiliary Search donation cap.";

const char kBackgroundResourceFetchName[] = "Background Resource Fetch";
const char kBackgroundResourceFetchDescription[] =
    "Process resource requests in a background thread inside Blink.";

#if BUILDFLAG(IS_ANDROID)
const char kBiometricAuthIdentityCheckName[] =
    "Enables android identity check for eligible features";
const char kBiometricAuthIdentityCheckDescription[] =
    "The feature makes biometric reauthentication mandatory before passwords "
    "filling or before other actions that are or should be protected by "
    "biometric checks.";
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kBirchWeatherName[] = "Birch weather";
const char kBirchWeatherDescription[] =
    "Shows the weather suggestion chip in the birch suggestions UI. Note that "
    "the chip only shows certain times of day, so it may not immediately "
    "appear after enabling this feature.";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

const char kBlinkSchedulerDiscreteInputMatchesResponsivenessMetricsName[] =
    "Blink Scheduler Discrete Input Matches Responsiveness Metrics";
const char
    kBlinkSchedulerDiscreteInputMatchesResponsivenessMetricsDescription[] =
        "Uses Responsiveness Metrics definitions for discrete input in blink "
        "scheduler heuristics.";

const char kCertVerificationNetworkTimeName[] =
    "Network Time for Certificate Verification";
const char kCertVerificationNetworkTimeDescription[] =
    "Use time fetched from the network for certificate verification decisions. "
    "If certificate verification fails with the network time, it will fall back"
    " to system time.";

const char kClassifyUrlOnProcessResponseEventName[] =
    "Classify Url on process response event";
const char kClassifyUrlOnProcessResponseEventDescription[] =
    "Alters the behavior of a supervised user navigation throttle so that the"
    "decision whether to proceed or cancel is made when the response is ready"
    "to be rendered, rather than before the request (or any redirect)"
    "is issued.";

const char kClearUndecryptablePasswordsName[] =
    "Removes passwords that can no longer be decrypted";
const char kClearUndecryptablePasswordsDescription[] =
    "If enabled local passwords that current encyrption key cannot decrypt, "
    "will be deleted to restore the full functionality of password manager.";

const char kClickToCallName[] = "Click-To-Call";
const char kClickToCallDescription[] = "Enable the click-to-call feature.";

const char kClipboardMaximumAgeName[] = "Clipboard maximum age";
const char kClipboardMaximumAgeDescription[] =
    "Limit the maximum age for recent clipboard content";

const char kComputePressureRateObfuscationMitigationName[] =
    "Enable mitigation algorithm for rate obfuscation in compute pressure";
const char kComputePressureRateObfuscationMitigationDescription[] =
    "Rate Obfuscation Mitigation is used to avoid fingerprinting attacks. Its "
    "usage introduces some timing penalties to the compute pressure results."
    "This mitigation might introduce slight precision errors."
    "When disabled this helps to test how predictable and accurate compute "
    "pressure is, but the Compute Pressure API can be susceptible to "
    "fingerprinting attacks.";

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kContainerName[] = "Container";
const char kContainerDescription[] = "Enables container.";
const char kContainerDebugName[] = "Container Debug";
const char kContainerDebugDescription[] = "Enables container debug.";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

const char kComputePressureBreakCalibrationMitigationName[] =
    "Enable mitigation algorithm to break calibration attempt in compute "
    "pressure";
const char kComputePressureBreakCalibrationMitigationDescription[] =
    "In a calibration process an attacker tries to manipulate the CPU so that "
    "Compute Pressure API would report a transition into a certain pressure "
    "state with the highest probability in response to the pressure exerted "
    "by the fabricated workload."
    "Break Calibration Mitigation is used to avoid calibration attempts by "
    "introducing some randomness in the result of the platform collector."
    "This mitigation might introduce slight precision errors."
    "When disabled this helps to test how predictable and accurate compute "
    "pressure is, but the Compute Pressure API can be susceptible to "
    "calibration attempts.";

const char kContainerTypeNoLayoutContainmentName[] =
    "Enables the container-type property to have no layout containment";
const char kContainerTypeNoLayoutContainmentDescription[] =
    "The container-type property was recently changed to not add layout "
    "containment, this allows users to temporarily disable this change.";

const char kContentSettingsPartitioningName[] = "Content Settings Partitioning";
const char kContentSettingsPartitioningDescription[] =
    "Partition content settings by StoragePartitions";

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kCoralFeatureKeyName[] = "Secret key for Coral feature.";
const char kCoralFeatureKeyDescription[] =
    "Secret key for Coral feature. Incorrect values will cause chrome crashes.";

const char kCrosSwitcherName[] = "ChromeOS Switcher feature.";
const char kCrosSwitcherDescription[] =
    "Enable/Disable ChromeOS Switcher feature.";
#endif  // IS_CHROMEOS_ASH

const char kCssGamutMappingName[] = "CSS Gamut Mapping";
const char kCssGamutMappingDescription[] =
    "Enable experimental CSS gamut mapping implementation.";

const char kCssTextBoxTrimName[] = "CSS text-box-trim";
const char kCssTextBoxTrimDescription[] =
    "Enable experimental CSS text-box-trim property.";

const char kCustomizeChromeSidePanelExtensionsCardName[] =
    "Customize Chrome Side Panel Extension Card";
const char kCustomizeChromeSidePanelExtensionsCardDescription[] =
    "If enabled, shows an extension card within the Customize Chrome Side "
    "Panel for access to the Chrome Web Store extensions.";

const char kCustomizeChromeWallpaperSearchName[] =
    "Customize Chrome Wallpaper Search";
const char kCustomizeChromeWallpaperSearchDescription[] =
    "Enables wallpaper search in Customize Chrome Side Panel.";

const char kCustomizeChromeWallpaperSearchButtonName[] =
    "Customize Chrome Wallpaper Search Button";
const char kCustomizeChromeWallpaperSearchButtonDescription[] =
    "Enables entry point on Customize Chrome Side Panel's Appearance page for "
    "Wallpaper Search.";

const char kCustomizeChromeWallpaperSearchInspirationCardName[] =
    "Customize Chrome Wallpaper Search Inspiration Card";
const char kCustomizeChromeWallpaperSearchInspirationCardDescription[] =
    "Shows inspiration card in Customize Chrome Side Panel Wallpaper Search. "
    "Requires #customize-chrome-wallpaper-search to be enabled too.";

const char kDataSharingName[] = "Data Sharing";
const char kDataSharingDescription[] =
    "Enabled all Data Sharing related UI and features.";

const char kDataSharingJoinOnlyName[] = "Data Sharing Join Only";
const char kDataSharingJoinOnlyDescription[] =
    "Enabled Data Sharing Joining flow related UI and features.";

const char kHistorySyncAlternativeIllustrationName[] =
    "History Sync Alternative Illustration";
const char kHistorySyncAlternativeIllustrationDescription[] =
    "Enables history sync alternative illustration.";

const char kDeprecateUnloadName[] = "Deprecate the unload event";
const char kDeprecateUnloadDescription[] =
    "Controls the default for Permissions-Policy unload. If enabled, unload "
    "handlers are deprecated and will not receive the unload event unless a "
    "Permissions-Policy to enable them has been explicitly set. If  disabled, "
    "unload handlers will continue to receive the unload event unless "
    "explicity disabled by Permissions-Policy, even during the gradual "
    "rollout of their deprecation.";

const char kDeferRendererTasksAfterInputName[] =
    "Defer Renderer Tasks After Input";
const char kDeferRendererTasksAfterInputDescription[] =
    "If enabled, tasks in the renderer will be deferred after discrete input "
    "events, e.g. keypress, until after the subsequent frame. The associated "
    "policy controls which tasks types can be deferred.";

const char kDevToolsPrivacyUIName[] = "DevTools Privacy UI";
const char kDevToolsPrivacyUIDescription[] =
    "Enables the Privacy UI in the current 'Security' panel in DevTools.";

const char kForceStartupSigninPromoName[] = "Force Start-up Signin Promo";
const char kForceStartupSigninPromoDescription[] =
    "If enabled, the full screen signin promo will be forced to show up at "
    "Chrome start-up.";

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kForestFeatureName[] = "Forest";
const char kForestFeatureDescription[] = "Enable Forest experiment.";
#endif

const char kFontationsFontBackendName[] = "Enable Fontations font backend";
const char kFontationsFontBackendDescription[] =
    "If enabled, the Fontations font backend will be used for web fonts where "
    "otherwise FreeType would have been used.";

const char kAvifGainmapHdrImagesName[] = "AVIF gainmap HDR image rendering";
const char kAvifGainmapHdrImagesDescription[] =
    "If enabled, Chrome uses the gainmap (if present) in AVIF images to render "
    "the HDR version on HDR displays and the SDR version on SDR displays.";

const char kCrabbyAvifName[] = "CrabbyAvif for decoding AVIF images";
const char kCrabbyAvifDescription[] =
    "If enabled, CrabbyAvif will be used instead of libavif for decoding AVIF "
    "images";

const char kTangibleSyncName[] = "Tangible Sync";
const char kTangibleSyncDescription[] =
    "Enables the tangible sync when a user starts the sync consent flow";

#if BUILDFLAG(IS_ANDROID)
const char kDisableInstanceLimitName[] = "Disable Instance Limit";
const char kDisableInstanceLimitDescription[] =
    "Disable limit on number of app instances allowed (current limit is 5).";
#endif

const char kDownloadWarningImprovementsName[] = "Download Warning Improvements";
const char kDownloadWarningImprovementsDescription[] =
    "Enable UI improvements for downloads, download scanning, and download "
    "warnings. The enabled features are subject to change at any time.";

const char kEnableBenchmarkingName[] = "Enable benchmarking";
const char kEnableBenchmarkingDescription[] =
    "Sets all features to a fixed state; that is, disables randomization for "
    "feature states. If '(Default Feature States)' is selected, sets all "
    "features to their default state. If '(Match Field Trial Testing Config)' "
    "is selected, sets all features to the state configured in the field trial "
    "testing config. This is used by developers and testers "
    "to diagnose whether an observed problem is caused by a non-default "
    "base::Feature configuration. This flag is automatically reset "
    "after 3 restarts and will be off from the 4th restart. On the 3rd "
    "restart, the flag will appear to be off but the effect is still active.";
const char kEnableBenchmarkingChoiceDisabled[] = "Disabled";
const char kEnableBenchmarkingChoiceDefaultFeatureStates[] =
    "Default Feature States";
const char kEnableBenchmarkingChoiceMatchFieldTrialTestingConfig[] =
    "Match Field Trial Testing Config";

const char kEnableBookmarksSelectedTypeOnSigninForTestingName[] =
    "Enable bookmarks selected type on sign-in for testing";
const char kEnableBookmarksSelectedTypeOnSigninForTestingDescription[] =
    "Test-only flag to help with the development of "
    "sync-enable-bookmarks-in-transport-mode. Enables the bookmarks "
    "UserSelectableType upon sign-in";

const char kPreloadingOnPerformancePageName[] =
    "Preloading Settings on Performance Page";
const char kPreloadingOnPerformancePageDescription[] =
    "Moves preloading settings to the performance page.";

const char kPrerender2Name[] = "Prerendering";
const char kPrerender2Description[] =
    "If enabled, browser features and the speculation rules API can trigger "
    "prerendering. If disabled, all prerendering APIs still exist, but a "
    "prerender will never successfully take place.";

const char kEnableDrDcName[] =
    "Enables Display Compositor to use a new gpu thread.";
const char kEnableDrDcDescription[] =
    "When enabled, chrome uses 2 gpu threads instead of 1. "
    " Display compositor uses new dr-dc gpu thread and all other clients "
    "(raster, webgl, video) "
    " continues using the gpu main thread.";

const char kTextBasedAudioDescriptionName[] = "Enable audio descriptions.";
const char kTextBasedAudioDescriptionDescription[] =
    "When enabled, HTML5 video elements with a 'descriptions' WebVTT track "
    "will speak the audio descriptions aloud as the video plays.";

const char kEnableExtensionsPermissionsForSupervisedUsersOnDesktopName[] =
    "Require parent permissions for extensions";
const char
    kEnableExtensionsPermissionsForSupervisedUsersOnDesktopDescription[] =
        "Apply Family Link parental control settings for extension "
        "installation. ";

const char kEnableSupervisedUserSkipParentApprovalToInstallExtensionsName[] =
    "Skip parental approval for extension installation for supervised users.";
const char
    EnableSupervisedUserSkipParentApprovalToInstallExtensionsDescription[] =
        "Provides an extension installation mode where supervised users can "
        "install extensions without asking for parental approach on each "
        "install."
        "The flag enable-family-link-extensions-permissions must "
        "also "
        "be enabled on Windows/Linux/Mac.";

const char kEnableSupervisedUserProfileSignInIphName[] =
    "Enable IPH for supervised user sign-in into a new profile.";
const char kEnableSupervisedUserProfileSignInIphDescription[] =
    "Enables the IPH for shown to supervised user upon sign-in into a newly "
    "created profile.";

const char kUpdatedSupervisedUserExtensionApprovalStringsName[] =
    "Display informative strings in the extension parent approval dialog.";
const char kUpdatedSupervisedUserExtensionApprovalStringsDescription[] =
    "Adds information about the applicable parental controls "
    "in the parent approval dialog screen for extensions. "
    "Informs the parents that parental controls apply and can be configured "
    "in Family Link.";

const char kUseAndroidStagingSmdsName[] = "Use Android staging SM-DS";
const char kUseAndroidStagingSmdsDescription[] =
    "Use the Android staging address when fetching pending eSIM profiles.";

const char kUseFrameIntervalDeciderName[] =
    "Use rewritten display FrameIntervalDecider";
const char kUseFrameIntervalDeciderDescription[] =
    "Rewrite is meant to preserve existing behavior and enable new features.";

const char kUseSharedImagesForPepperVideoName[] =
    "Use SharedImages for PPAPI Video";
const char kUseSharedImagesForPepperVideoDescription[] =
    "Enables use of SharedImages for textures that are used by PPAPI "
    "VideoDecoder";

const char kUseStorkSmdsServerAddressName[] = "Use Stork SM-DS address";
const char kUseStorkSmdsServerAddressDescription[] =
    "Use the Stork SM-DS address to fetch pending eSIM profiles managed by the "
    "Stork prod server. Note that Stork profiles can be created with an EID at "
    "go/stork-profile, and managed at go/stork-batch > View Profiles. Also "
    "note that an test EUICC card is required to use this feature, usually "
    "that requires the kCellularUseSecond flag to be enabled. Go to "
    "go/cros-connectivity > Dev Tips for more instructions.";

const char kUseWallpaperStagingUrlName[] = "Use Wallpaper staging URL";
const char kUseWallpaperStagingUrlDescription[] =
    "Use the staging server as part of the Wallpaper App to verify "
    "additions/removals of wallpapers.";

const char kUseMessagesStagingUrlName[] = "Use Messages staging URL";
const char kUseMessagesStagingUrlDescription[] =
    "Use the staging server as part of the \"Messages\" feature under "
    "\"Connected Devices\" settings.";

const char kUseCustomMessagesDomainName[] = "Use custom Messages domain";
const char kUseCustomMessagesDomainDescription[] =
    "Use a custom URL as part of the \"Messages\" feature under "
    "\"Connected Devices\" settings.";

const char kUseDMSAAForTilesName[] = "Use DMSAA for tiles";
const char kUseDMSAAForTilesDescription[] =
    "Switches skia to use DMSAA instead of MSAA for tile raster";

const char kUseDMSAAForTilesAndroidGLName[] =
    "Use DMSAA for tiles on Android GL backend.";
const char kUseDMSAAForTilesAndroidGLDescription[] =
    "Switches skia to use DMSAA instead of MSAA for tile raster on Android GL "
    "backend.";

const char kUseDnsHttpsSvcbAlpnName[] = "Use DNS https alpn";
const char kUseDnsHttpsSvcbAlpnDescription[] =
    "When enabled, Chrome may try QUIC on the first connection using the ALPN"
    " information in the DNS HTTPS record.";

const char kIsolatedSandboxedIframesName[] = "Isolated sandboxed iframes";
const char kIsolatedSandboxedIframesDescription[] =
    "When enabled, applies process isolation to iframes with the 'sandbox' "
    "attribute and without the 'allow-same-origin' permission set on that "
    "attribute. This also applies to documents with a similar CSP sandbox "
    "header, even in the main frame. The affected sandboxed documents can be "
    "grouped into processes based on their URL's site or origin. The default "
    "grouping when enabled is per-site.";

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
const char kAutofillEnableAmountExtractionDesktopName[] =
    "Enable checkout amount extraction on Chrome desktop";
const char kAutofillEnableAmountExtractionDesktopDescription[] =
    "When enabled, Chrome will extract the checkout amount from the checkout "
    "page of the allowlisted merchant websites.";
#endif

const char kAutofillEnableAndroidNKeyForFidoAuthenticationName[] =
    "Enable Android N Key for FIDO authentication";
const char kAutofillEnableAndroidNKeyForFidoAuthenticationDescription[] =
    "When enabled, Android N+ devices will be supported for FIDO "
    "authentication when autofilling server credit cards.";

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
const char kAutofillEnableBuyNowPayLaterForAffirmName[] =
    "Enable buy now pay later on Autofill for Affirm";
const char kAutofillEnableBuyNowPayLaterForAffirmDescription[] =
    "When enabled, users will have the option to pay with buy now pay later "
    "with Affirm on specific merchant webpages.";

const char kAutofillEnableBuyNowPayLaterForZipName[] =
    "Enable buy now pay later on Autofill for Zip";
const char kAutofillEnableBuyNowPayLaterForZipDescription[] =
    "When enabled, users will have the option to pay with buy now pay later "
    "with Zip on specific merchant webpages.";

const char kAutofillEnableBuyNowPayLaterSyncingName[] =
    "Enable syncing buy now pay later user data.";
const char kAutofillEnableBuyNowPayLaterSyncingDescription[] =
    "When enabled, Chrome will sync user data related to buy now pay later.";
#endif

const char kAutofillEnableCvcStorageAndFillingName[] =
    "Enable CVC storage and filling for payments autofill";
const char kAutofillEnableCvcStorageAndFillingDescription[] =
    "When enabled, we will store CVC for both local and server credit cards. "
    "This will also allow the users to autofill their CVCs on checkout pages.";

const char kAutofillEnableCvcStorageAndFillingEnhancementName[] =
    "Enable CVC storage and filling enhancement for payments autofill";
const char kAutofillEnableCvcStorageAndFillingEnhancementDescription[] =
    "When enabled, will enhance CVV storage project. Provide better "
    "suggestion, resolve conflict with COF project and add logging.";

const char kAutofillEnableCvcStorageAndFillingStandaloneFormEnhancementName[] =
    "Enable CVC storage and filling standalone form enhancement for payments "
    "autofill";
const char
    kAutofillEnableCvcStorageAndFillingStandaloneFormEnhancementDescription[] =
        "When enabled, this will enhance the CVV storage project. The "
        "enhancement will enable CVV storage suggestions for standalone CVC "
        "fields.";

const char kAutofillEnableFpanRiskBasedAuthenticationName[] =
    "Enable risk-based authentication for FPAN retrieval";
const char kAutofillEnableFpanRiskBasedAuthenticationDescription[] =
    "When enabled, server card retrieval will begin with a risk-based check "
    "instead of jumping straight to CVC or biometric auth.";

const char kAutofillEnableManualFallbackIPHName[] =
    "Show autofill manual fallback IPH";
const char kAutofillEnableManualFallbackIPHDescription[] =
    "Show a promo on address fields which educates the users about the "
    "possibility to trigger autofill from the context menu.";

const char kAutofillEnableCardArtImageName[] = "Enable showing card art images";
const char kAutofillEnableCardArtImageDescription[] =
    "When enabled, card product images (instead of network icons) will be "
    "shown in Payments Autofill UI.";

const char kAutofillEnableCardArtServerSideStretchingName[] =
    "Enable server side stretching of card art images";
const char kAutofillEnableCardArtServerSideStretchingDescription[] =
    "When enabled, the server will stretch (if necessary) and return card art "
    "images of the exact required dimensions. The client side resizing of "
    "images will not be required.";

const char kAutofillEnableCardBenefitsForAmericanExpressName[] =
    "Enable showing card benefits for American Express cards";
const char kAutofillEnableCardBenefitsForAmericanExpressDescription[] =
    "When enabled, card benefits offered by American Express will be shown in "
    "Autofill suggestions.";

const char kAutofillEnableCardBenefitsForCapitalOneName[] =
    "Enable showing card benefits for Capital One cards";
const char kAutofillEnableCardBenefitsForCapitalOneDescription[] =
    "When enabled, card benefits offered by Capital One will be shown in "
    "Autofill suggestions.";

const char kAutofillEnableCardBenefitsIphName[] =
    "Enable showing in-process help UI for card benefits";
const char kAutofillEnableCardBenefitsIphDescription[] =
    "When enabled, in-process help UI will be shown for Autofill card "
    "suggestions with benefits.";

const char kAutofillEnableCardBenefitsSyncName[] =
    "Enable syncing card benefits";
const char kAutofillEnableCardBenefitsSyncDescription[] =
    "When enabled, card benefits offered by issuers will be synced from the "
    "Payments server.";

const char kAutofillEnableCardInfoRuntimeRetrievalName[] =
    "Enable retrieval of card info(with CVC) from issuer for enrolled cards";
const char kAutofillEnableCardInfoRuntimeRetrievalDescription[] =
    "When enabled, runtime retrieval of CVC along with card number and expiry "
    "from issuer for enrolled cards will be enabled during form fill.";

const char kAutofillEnableCardProductNameName[] =
    "Enable showing card product name";
const char kAutofillEnableCardProductNameDescription[] =
    "When enabled, card product name (instead of issuer network) will be shown "
    "in Payments Autofill UI.";

const char kAutofillGranularFillingAvailableName[] =
    "Enable autofill address granular filling";
const char kAutofillGranularFillingAvailableDescription[] =
    "When enabled, autofill address suggestions contain child suggestions for "
    "group and granular field filling.";

const char kAutofillForUnclassifiedFieldsAvailableName[] =
    "Enable address and payments autofill for unclassified fields";
const char kAutofillForUnclassifiedFieldsAvailableDescription[] =
    "When enabled, address and payments autofill can be triggered on "
    "unclassified fields via the Chrome context menu.";

const char kAutofillEnableLocalIbanName[] =
    "Enable local IBAN save and autofilling";
const char kAutofillEnableLocalIbanDescription[] =
    "When enabled, saving and autofilling local IBANs (International Bank "
    "Account Numbers) will be offered.";

const char kAutofillEnableLogFormEventsToAllParsedFormTypesName[] =
    "Enable logging form events to all parsed form on a web page.";
const char kAutofillEnableLogFormEventsToAllParsedFormTypesDescription[] =
    "When enabled, a form event will log to all of the parsed forms of the "
    "same type on a webpage. This means credit card form events will log to "
    "all credit card form types and address form events will log to all "
    "address form types.";

const char kAutofillEnableNewCardArtAndNetworkImagesName[] =
    "Enable showing new card art and network images";
const char kAutofillEnableNewCardArtAndNetworkImagesDescription[] =
    "When enabled, new and larger card art and network icons will be shown.";

const char kAutofillEnableOffersInClankKeyboardAccessoryName[] =
    "Enable Autofill offers in keyboard accessory";
const char kAutofillEnableOffersInClankKeyboardAccessoryDescription[] =
    "When enabled, offers will be displayed in the keyboard accessory when "
    "available.";

#if BUILDFLAG(IS_ANDROID)
const char kAutofillEnablePaymentSettingsCardPromoAndScanCardName[] =
    "Use the new card promo and allow for card scanning in the payment "
    "settings page";
const char kAutofillEnablePaymentSettingsCardPromoAndScanCardDescription[] =
    "When enabled, the new card promo UX will be shown on the payment "
    "settings page and the option for card scans will be available on the add "
    "card page.";

const char kAutofillEnablePaymentSettingsServerCardSaveName[] =
    "Save new credit cards in the payment settings page to the Google Payments "
    "server";
const char kAutofillEnablePaymentSettingsServerCardSaveDescription[] =
    "When enabled, new credit cards added in the payment settings page will be "
    "saved to the Google Payments server. The card will be saved locally "
    "if the server save fails.";
#endif

const char kAutofillEnablePrefetchingRiskDataForRetrievalName[] =
    "Enable prefetching of risk data during payments autofill retrieval";
const char kAutofillEnablePrefetchingRiskDataForRetrievalDescription[] =
    "When enabled, risk data is prefetched during payments autofill flows "
    "to reduce user-perceived latency.";

const char kAutofillEnableRankingFormulaAddressProfilesName[] =
    "Enable new Autofill suggestion ranking formula for profiles";
const char kAutofillEnableRankingFormulaAddressProfilesDescription[] =
    "When enabled, Autofill will use a new ranking formula to rank Autofill "
    "profile suggestions.";

const char kAutofillEnableRankingFormulaCreditCardsName[] =
    "Enable new Autofill suggestion ranking formula for credit cards";
const char kAutofillEnableRankingFormulaCreditCardsDescription[] =
    "When enabled, Autofill will use a new ranking formula to rank Autofill "
    "credit card suggestions.";

const char kAutofillEnableSaveCardLoadingAndConfirmationName[] =
    "Enable save card loading and confirmation UX";
const char kAutofillEnableSaveCardLoadingAndConfirmationDescription[] =
    "When enabled, a loading spinner will be shown when uploading a card to "
    "the server and a confirmation screen will be will be shown based on the "
    "result of the upload. If the upload is unsuccessful in being uploaded to "
    "the server, it will be saved locally.";

const char kAutofillEnableSaveCardLocalSaveFallbackName[] =
    "Enable save card local save fallback";
const char kAutofillEnableSaveCardLocalSaveFallbackDescription[] =
    "When enabled, if a card fails to be uploaded to the server, the card "
    "details will be saved locally instead. If a card with the same card "
    "number and expiration date already exists in the local database, this "
    "will be a no-op and the existing card will not be updated with any card "
    "details from the form.";

const char kAutofillEnableServerIbanName[] =
    "Enable server-based IBAN uploading and autofilling";
const char kAutofillEnableServerIbanDescription[] =
    "When enabled, Autofill will attempt to offer upload save for IBANs "
    "(International Bank Account Numbers) and autofill server-based IBANs.";

#if BUILDFLAG(IS_ANDROID)
const char kAutofillEnableSyncingOfPixBankAccountsName[] =
    "Sync Pix bank accounts from Google Payments";
const char kAutofillEnableSyncingOfPixBankAccountsDescription[] =
    "When enabled, Pix bank accounts are synced from Google Payments backend. "
    "These bank account will show up in Chrome settings.";
#endif  // BUILDFLAG(IS_ANDROID)

const char kAutofillEnableVcn3dsAuthenticationName[] =
    "Enable 3DS authentication for virtual cards";
const char kAutofillEnableVcn3dsAuthenticationDescription[] =
    "When enabled, Chrome will trigger 3DS authentication during a virtual "
    "card retrieval if a challenge is required, 3DS authentication is "
    "available for the card, and FIDO is not.";

const char kAutofillEnableVcnGrayOutForMerchantOptOutName[] =
    "Enable virtual card suggestion graying out for opted-out merchants";
const char kAutofillEnableVcnGrayOutForMerchantOptOutDescription[] =
    "When enabled, Chrome will display grayed out virtual card suggestions on "
    "merchant websites where the merchant has opted-out of virtual cards.";

const char kAutofillEnableVcnEnrollLoadingAndConfirmationName[] =
    "Enable showing loading and confirmation screens for virtual card "
    "enrollment";
const char kAutofillEnableVcnEnrollLoadingAndConfirmationDescription[] =
    "When enabled, the virtual card enrollment screen will present a loading "
    "spinner while enrolling the card to the server and present a confirmation "
    "screen with the result when completed.";

const char kAutofillEnableVerveCardSupportName[] =
    "Enable autofill support for Verve cards";
const char kAutofillEnableVerveCardSupportDescription[] =
    "When enabled, Verve-branded card art will be shown for Verve cards.";

const char kAutofillEnableVirtualCardMetadataName[] =
    "Enable showing metadata for virtual cards";
const char kAutofillEnableVirtualCardMetadataDescription[] =
    "When enabled, Chrome will show metadata together with other card "
    "information when the virtual card is presented to users.";

const char kAutofillHighlightOnlyChangedValuesInPreviewModeName[] =
    "Highlight only changed values in preview mode.";
const char kAutofillHighlightOnlyChangedValuesInPreviewModeDescription[] =
    "When Autofill is previewing filling a form, already autofilled values "
    "and other values that are not changed by accepting the preview should "
    "not be highlighted.";

const char kAutofillParseVcnCardOnFileStandaloneCvcFieldsName[] =
    "Parse standalone CVC fields for VCN card on file in forms";
const char kAutofillParseVcnCardOnFileStandaloneCvcFieldsDescription[] =
    "When enabled, Autofill will attempt to find standalone CVC fields for VCN "
    "card on file when parsing forms.";

const char kAutofillSkipAndroidBottomSheetForIbanName[] =
    "Skip Autofill IBAN as bottom sheet suggestions";
const char kAutofillSkipAndroidBottomSheetForIbanDescription[] =
    "When enabled, IBAN Autofill suggestions are shown via the keyboard "
    "accessory instead of the bottom sheet.";

const char kAutofillMoreProminentPopupName[] = "More prominent Autofill popup";
const char kAutofillMoreProminentPopupDescription[] =
    "If enabled Autofill's popup becomes more prominent, i.e. its shadow "
    "becomes more emphasized, position is also updated";

const char kAutofillRemovePaymentsButterDropdownName[] =
    "Remove \"Show cards from your Google Account\" dropdown";
const char kAutofillRemovePaymentsButterDropdownDescription[] =
    "When enabled, the \"Show cards from your Google Account\" Autofill "
    "suggestion will not be displayed, and Autofill will work as if it had "
    "been selected.";

const char kAutofillRequireValidLocalCardsInSettingsName[] =
    "Require local card numbers to be valid when adding or editing via "
    "chrome://settings";
const char kAutofillRequireValidLocalCardsInSettingsDescription[] =
    "When enabled, the chrome://settings dialog for adding or editing local "
    "cards will enforce that the card number is valid.";

const char kAutofillSharedStorageServerCardDataName[] =
    "Enable storing autofill server card data in the shared storage database";
const char kAutofillSharedStorageServerCardDataDescription[] =
    "When enabled, the cached server credit card data from autofill will be "
    "pushed into the shared storage database for the payments origin.";

#if BUILDFLAG(IS_ANDROID)
const char kAutofillSyncEwalletAccountsName[] =
    "Sync eWallet accounts from Google Payments";
const char kAutofillSyncEwalletAccountsDescription[] =
    "When enabled, eWallet accounts are synced from the Google Payments "
    "servers and displayed on the payment methods settings page.";
#endif  // BUILDFLAG(IS_ANDROID)

const char kAutofillUnmaskCardRequestTimeoutName[] =
    "Timeout for the credit card unmask request";
const char kAutofillUnmaskCardRequestTimeoutDescription[] =
    "When enabled, sets a client-side timeout on the Autofill credit card "
    "unmask request. Upon timeout, the client will terminate the current "
    "unmask server call, which may or may not terminate the ongoing unmask UI.";

const char kAutofillUploadCardRequestTimeoutName[] =
    "Timeout for the credit card upload request";
const char kAutofillUploadCardRequestTimeoutDescription[] =
    "When enabled, sets a client-side timeout on the Autofill credit card "
    "upload request. Upon timeout, the client will terminate the upload UI, "
    "but the request may still succeed server-side.";

const char kAutofillUpstreamUpdatedUiName[] =
    "Credit card upload save UI treatment";
const char kAutofillUpstreamUpdatedUiDescription[] =
    "When enabled, shows different text and images in the UI of the credit "
    "card upload save bubble.";

const char kAutofillVcnEnrollRequestTimeoutName[] =
    "Timeout for the credit card VCN enrollment request";
const char kAutofillVcnEnrollRequestTimeoutDescription[] =
    "When enabled, sets a client-side timeout on the Autofill credit card "
    "VCN enrollment request. Upon timeout, the client will terminate the VCN "
    "enrollment UI, but the request may still succeed server-side.";

const char kAutofillVirtualViewStructureAndroidName[] =
    "Enable the setting to provide a virtual view structure for Autofill";
const char kAutofillVirtualViewStructureAndroidDescription[] =
    "When enabled, a setting allows to switch to using Android Autofill. Chrome"
    " then provides a virtual view structure but no own suggestions.";

const char kAutoPictureInPictureForVideoPlaybackName[] =
    "Auto picture in picture for video playback";
const char kAutoPictureInPictureForVideoPlaybackDescription[] =
    "Enables auto picture in picture for video playback. Requires "
    "chrome://flags/#auto-picture-in-picture-video-heuristics to be enabled.";

const char kAutoPictureInPictureVideoHeuristicsName[] =
    "Auto picture in picture video heuristics";
const char kAutoPictureInPictureVideoHeuristicsDescription[] =
    "Enables the use of video heuristics for tracking HTMLVideoElement "
    "visibility.";

const char kBackForwardCacheName[] = "Back-forward cache";
const char kBackForwardCacheDescription[] =
    "If enabled, caches eligible pages after cross-site navigations."
    "To enable caching pages on same-site navigations too, choose 'enabled "
    "same-site support'.";

const char kBackForwardTransitionsName[] = "Back-forward visual transitions";
const char kBackForwardTransitionsDescription[] =
    "If enabled, adds animated gesture transitions for back/forward session "
    "history navigations. NOTE: enable "
    "increment-local-surface-id-for-mainframe-same-doc-navigation to enable "
    "the transition on same-doc navigations.";

const char kBiometricReauthForPasswordFillingName[] =
    "Biometric reauth for password filling";
const char kBiometricReauthForPasswordFillingDescription[] =
    "Enables biometric"
    "re-authentication before password filling";

const char kBindCookiesToPortName[] =
    "Bind cookies to their setting origin's port";
const char kBindCookiesToPortDescription[] =
    "If enabled, cookies will only be accessible by origins with the same port "
    "as the one that originally set the cookie.";

const char kBindCookiesToSchemeName[] =
    "Bind cookies to their setting origin's scheme";
const char kBindCookiesToSchemeDescription[] =
    "If enabled, cookies will only be accessible by origins with the same "
    "scheme as the one that originally set the cookie";

const char kFailFastQuietChipName[] = "Fail fast quiet chip";
const char kFailFastQuietChipDescription[] =
    "Enables fast finalization of a permission request if it is displayed as a "
    "quiet chip.";

const char kBackgroundListeningName[] = "BackgroundListening";
const char kBackgroundListeningDescription[] =
    "Enables the new media player features optimized for background listening.";

const char kBorealisBigGlName[] = "Borealis Big GL";
const char kBorealisBigGlDescription[] = "Enable Big GL when running Borealis.";

const char kBorealisDGPUName[] = "Borealis dGPU";
const char kBorealisDGPUDescription[] = "Enable dGPU when running Borealis.";

const char kBorealisEnableUnsupportedHardwareName[] =
    "Borealis Enable Unsupported Hardware";
const char kBorealisEnableUnsupportedHardwareDescription[] =
    "Allow Borealis to run on hardware that does not meet the minimum spec "
    "requirements. Be aware: Games may crash, or perform below expectations.";

const char kBorealisForceBetaClientName[] = "Borealis Force Beta Client";
const char kBorealisForceBetaClientDescription[] =
    "Force the client to run its beta version.";

const char kBorealisForceDoubleScaleName[] = "Borealis Force Double Scale";
const char kBorealisForceDoubleScaleDescription[] =
    "Force the client to run in 2x visual zoom. the scale client by DPI flag "
    "needs to be off for this to take effect.";

const char kBorealisLinuxModeName[] = "Borealis Linux Mode";
const char kBorealisLinuxModeDescription[] =
    "Do not run ChromeOS-specific code in the client.";

// For UX reasons we prefer "enabled", but that is used internally to refer to
// whether borealis is installed or not, so the name of the variable is a bit
// different to the user-facing name.
const char kBorealisPermittedName[] = "Borealis Enabled";
const char kBorealisPermittedDescription[] =
    "Allows Borealis to run on your device. Borealis may still be blocked for "
    "other reasons, including: administrator settings, device hardware "
    "capabilities, or other security measures.";

const char kBorealisProvisionName[] = "Borealis Provision";
const char kBorealisProvisionDescription[] =
    "Uses the experimental 'provision' option when mounting borealis stateful. "
    "The feature causes allocations on thinly provisioned storage, such as "
    "sparse vm images, to be passed to the underyling storage layers. "
    "Resulting in allocations in the Borealis being backed by physical "
    "storage.";

const char kBorealisScaleClientByDPIName[] = "Borealis Scale Client By DPI";
const char kBorealisScaleClientByDPIDescription[] =
    "Enable scaling the Steam client according to device DPI. "
    "If enabled this will override the force double scale flag.";

const char kBorealisZinkGlDriverName[] = "Borealis Zink GL Driver";
const char kBorealisZinkGlDriverDescription[] =
    "Enables zink driver for GL rendering in Borealis. Can be enabled for "
    "recommended GL apps only or for all GL apps. Defaults to recommended.";

const char kBypassAppBannerEngagementChecksName[] =
    "Bypass user engagement checks";
const char kBypassAppBannerEngagementChecksDescription[] =
    "Bypasses user engagement checks for displaying app banners, such as "
    "requiring that users have visited the site before and that the banner "
    "hasn't been shown recently. This allows developers to test that other "
    "eligibility requirements for showing app banners, such as having a "
    "manifest, are met.";

#if BUILDFLAG(IS_ANDROID)
const char kSearchInCCTName[] = "Search in Chrome Custom Tabs";
const char kSearchInCCTDescription[] =
    "Permits apps to create searchable and "
    "navigable custom tabs.";
const char kSearchInCCTAlternateTapHandlingName[] =
    "Search in Chrome Custom Tabs Alternate Tap Handling";
const char kSearchInCCTAlternateTapHandlingDescription[] =
    "Search in Chrome Custom Tabs Alternate Tap Handling";
#endif  // BUILDFLAG(IS_ANDROID)

const char kSeparateWebAppShortcutBadgeIconName[] =
    "Separate Web App Shorcut Badge Icon";
const char kSeparateWebAppShortcutBadgeIconDescription[] =
    "The shortcut app badge is painted in the UI instead of being part of the "
    "shortcut app icon, and more effects are added for the icon.";

const char kSeparateLocalAndAccountThemesName[] =
    "Separate local and account themes";
const char kSeparateLocalAndAccountThemesDescription[] =
    "Keeps the local and the account theme separate. If the user signs out or "
    "sync is turned off, only the account theme is removed and the "
    "pre-existing local theme is restored.";

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
const char kCameraMicEffectsName[] = "Camera and Mic Effects";
const char kCameraMicEffectsDescription[] =
    "Enables effects for camera and mic streams.";
const char kCameraMicPreviewName[] = "Camera and Mic Preview";
const char kCameraMicPreviewDescription[] =
    "Enables camera and mic preview in permission bubble and site settings.";
const char kGetUserMediaDeferredDeviceSettingsSelectionName[] =
    "getUserMedia deferred device settings selection";
const char kGetUserMediaDeferredDeviceSettingsSelectionDescription[] =
    "Enables deferring device settings selection for getUserMedia until after "
    "the user grants permission.";
#endif

#if BUILDFLAG(IS_CHROMEOS)
const char kCaptivePortalPopupWindowName[] =
    "Captive portal signin uses popup window.";
const char kCaptivePortalPopupWindowDescription[] =
    "Use a popup window with proxies disabled for captive portal signin.";
#endif

const char kChromeLabsName[] = "Chrome Labs";
const char kChromeLabsDescription[] =
    "Access Chrome Labs through the toolbar menu to see featured user-facing "
    "experimental features.";

const char kCommerceHintAndroidName[] = "Commerce Hint Android";
const char kCommerceHintAndroidDescription[] =
    "Enables commerce hint detection on Android.";

const char kClearCrossSiteCrossBrowsingContextGroupWindowNameName[] =
    "Clear window name in top-level cross-site cross-browsing-context-group "
    "navigation";
const char kClearCrossSiteCrossBrowsingContextGroupWindowNameDescription[] =
    "Clear the preserved window.name property when it's a top-level cross-site "
    "navigation that swaps BrowsingContextGroup.";

const char kDeviceForceScheduledRebootName[] =
    "Enable DeviceScheduledReboot policy for all sessions.";
const char kDeviceForceScheduledRebootDescription[] =
    "Schedule recurring reboot for the device. Reboots are always executed at "
    "a scheduled time. If the session is active, user will be notified about "
    "the reboot, but the reboot will not be delayed.";

const char kDevicePostureName[] = "Device Posture API";
const char kDevicePostureDescription[] =
    "Enables Device Posture API (foldable devices)";

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_CHROMEOS)
const char kDocumentPictureInPictureAnimateResizeName[] =
    "Document Picture-in-Picture Animate Resize";
const char kDocumentPictureInPictureAnimateResizeDescription[] =
    "Use an animation when programmatically resizing a document"
    "picture-in-picture window";
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_CHROMEOS)

const char kViewportSegmentsName[] = "Viewport Segments API";
const char kViewportSegmentsDescription[] =
    "Enable the viewport segment API, giving information about the logical "
    "segments of the device (dual screen and foldable devices)";

const char kVisitedURLRankingServiceHistoryVisibilityScoreFilterName[] =
    "Enable visited URL aggregates visibility score based filtering";
const char kVisitedURLRankingServiceHistoryVisibilityScoreFilterDescription[] =
    "Enables filtering of visited URL aggregates based on history URL "
    "visibility scores.";

const char kDiscountConsentV2Name[] = "Discount Consent V2";
const char kDiscountConsentV2Description[] = "Enables Discount Consent V2";

const char kDisruptiveNotificationPermissionRevocationName[] =
    "Disruptive notification permission revocation";
const char kDisruptiveNotificationPermissionRevocationDescription[] =
    "Enables revoking the notification permission on sites that send "
    "disruptive notifications unless the permission was granted through a "
    "prompt that informed the user about this possibility.";

const char kDoubleBufferCompositingName[] = "Double buffered compositing";
const char kDoubleBufferCompositingDescription[] =
    "Use double buffer for compositing (instead of triple-buffering). "
    "Latency should be reduced in some cases. On the other hand, more skipped "
    "frames are expected.";

const char kMainThreadCompositingPriorityName[] =
    "Main thread runs as compositing";
const char kMainThreadCompositingPriorityDescription[] =
    "Runs the main thread at compositing priority since it responds to input "
    "and is on the critical path.";

const char kMediaSessionEnterPictureInPictureName[] =
    "Media Session enterpictureinpicture action";
const char kMediaSessionEnterPictureInPictureDescription[] =
    "Enables the 'enterpictureinpicture' MediaSessionAction to allow websites "
    "to register an action handler for entering picture-in-picture.";

const char kCodeBasedRBDName[] = "Code-based RBD";
const char kCodeBasedRBDDescription[] = "Enables the Code-based RBD.";

const char kCompressionDictionaryTransportName[] =
    "Compression dictionary transport";
const char kCompressionDictionaryTransportDescription[] =
    "Enables compression dictionary transport features. Requires "
    "chrome://flags/#enable-compression-dictionary-transport-backend to be "
    "enabled.";

const char kCompressionDictionaryTransportBackendName[] =
    "Compression dictionary transport backend";
const char kCompressionDictionaryTransportBackendDescription[] =
    "Enables the backend of compression dictionary transport features. "
    "Requires chrome://flags/#enable-compression-dictionary-transport to be "
    "enabled for testing the feature.";

const char kCompressionDictionaryTransportOverHttp1Name[] =
    "Compression dictionary transport over HTTP/1";
const char kCompressionDictionaryTransportOverHttp1Description[] =
    "When this is enabled, Chromium can use stored shared dictionaries even "
    "when the connection is using HTTP/1 for non-localhost requests.";

const char kCompressionDictionaryTransportOverHttp2Name[] =
    "Compression dictionary transport over HTTP/2";
const char kCompressionDictionaryTransportOverHttp2Description[] =
    "When this is enabled, Chromium can use stored shared dictionaries even "
    "when the connection is using HTTP/2 for non-localhost requests.";

const char kCompressionDictionaryTransportRequireKnownRootCertName[] =
    "Compression dictionary transport require knwon root cert";
const char kCompressionDictionaryTransportRequireKnownRootCertDescription[] =
    "When this is enabled, Chromium can use stored shared dictionaries only "
    "when the connection is using a well known root cert or when the server is "
    "a localhost.";

#if BUILDFLAG(IS_ANDROID)
const char kContextualSearchWithCredentialsForDebugName[] =
    "Contextual Search within credentials for debug";
const char kContextualSearchWithCredentialsForDebugDescription[] =
    "When this is enabled, if a user do the contextual search, the credentials "
    "mode will be include.";
#endif  // BUILDFLAG(IS_ANDROID)

const char kForceColorProfileSRGB[] = "sRGB";
const char kForceColorProfileP3[] = "Display P3 D65";
const char kForceColorProfileRec2020[] = "ITU-R BT.2020";
const char kForceColorProfileColorSpin[] = "Color spin with gamma 2.4";
const char kForceColorProfileSCRGBLinear[] =
    "scRGB linear (HDR where available)";
const char kForceColorProfileHDR10[] = "HDR10 (HDR where available)";

const char kForceColorProfileName[] = "Force color profile";
const char kForceColorProfileDescription[] =
    "Forces Chrome to use a specific color profile instead of the color "
    "of the window's current monitor, as specified by the operating system.";

const char kDynamicColorGamutName[] = "Dynamic color gamut";
const char kDynamicColorGamutDescription[] =
    "Displays in wide color when the content is wide. When the content is "
    "not wide, displays sRGB";

const char kCooperativeSchedulingName[] = "Cooperative Scheduling";
const char kCooperativeSchedulingDescription[] =
    "Enables cooperative scheduling in Blink.";

const char kDarkenWebsitesCheckboxInThemesSettingName[] =
    "Darken websites checkbox in themes setting";
const char kDarkenWebsitesCheckboxInThemesSettingDescription[] =
    "Show a darken websites checkbox in themes settings when system default or "
    "dark is selected. The checkbox can toggle the auto-darkening web contents "
    "feature";

const char kDebugPackedAppName[] = "Debugging for packed apps";
const char kDebugPackedAppDescription[] =
    "Enables debugging context menu options such as Inspect Element for packed "
    "applications.";

const char kDebugShortcutsName[] = "Debugging keyboard shortcuts";
const char kDebugShortcutsDescription[] =
    "Enables additional keyboard shortcuts that are useful for debugging Ash.";

const char kDisableProcessReuse[] = "Disable subframe process reuse";
const char kDisableProcessReuseDescription[] =
    "Prevents out-of-process iframes from reusing compatible processes from "
    "unrelated tabs. This is an experimental mode that will result in more "
    "processes being created.";

const char kDisallowDocWrittenScriptsUiName[] =
    "Block scripts loaded via document.write";
const char kDisallowDocWrittenScriptsUiDescription[] =
    "Disallows fetches for third-party parser-blocking scripts inserted into "
    "the main frame via document.write.";

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
const char kDisallowManagedProfileSignoutName[] =
    "Disallow managed profile signout";
const char kDisallowManagedProfileSignoutDescription[] =
    "Disallows signing out from managed profiles.";
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)

const char kViewTransitionOnNavigationName[] =
    "viewTransition API for navigations";
const char kViewTransitionOnNavigationDescription[] =
    "Controls the availability of the viewTransition API on document "
    "navigations.";

const char kViewTransitionOnNavigationIframeName[] =
    "viewTransition API for sub-frame navigations";
const char kViewTransitionOnNavigationIframeDescription[] =
    "Controls the availability of the viewTransition API on document "
    "navigations in a subframe. Must be used with "
    "view-transition-on-navigation";

const char kEnableAutoDisableAccessibilityName[] = "Auto-disable Accessibility";
const char kEnableAutoDisableAccessibilityDescription[] =
    "When accessibility APIs are no longer being requested, automatically "
    "disables accessibility. This might happen if an assistive technology is "
    "turned off or if an extension which uses accessibility APIs no longer "
    "needs them.";

const char kImageDescriptionsAlternateRoutingName[] =
    "Use alternative route for image descriptions.";
const char kImageDescriptionsAlternateRoutingDescription[] =
    "When adding automatic captions to images, use a different route to aquire "
    "descriptions.";

const char kImprovedSigninUIOnDesktopName[] = "Improved signin UI";
const char kImprovedSigninUIOnDesktopDescription[] =
    "Enables redesign of profile menu and signin settings ; new signin promos "
    "and better signin errors.";

const char kEnableAutofillAddressSavePromptName[] =
    "Autofill Address Save Prompts";
const char kEnableAutofillAddressSavePromptDescription[] =
    "Enable the Autofill address save prompts.";

const char kEnterpriseProfileBadgingForAvatarName[] =
    "Enable enterprise profile badging on the avatar";
const char kEnterpriseProfileBadgingForAvatarDescription[] =
    "Enable enterprise profile badging on the toolbar avatar";

const char kEnterpriseProfileBadgingForMenuName[] =
    "Enable enterprise profile badging in the profile menu";
const char kEnterpriseProfileBadgingForMenuDescription[] =
    "Enable enterprise profile badging in the profile menu. This includes "
    "showing the enterprise logo and the management disclaimer under the email";

const char kEnterpriseUpdatedProfileCreationScreenName[] =
    "Enable enterprise updated profile creation screens";
const char kEnterpriseUpdatedProfileCreationScreenDescription[] =
    "Enable enterprise updated profile creation screens in the profile picker "
    "and forced profile creation.";

const char kEnableExperimentalCookieFeaturesName[] =
    "Enable experimental cookie features";
const char kEnableExperimentalCookieFeaturesDescription[] =
    "Enable new features that affect setting, sending, and managing cookies. "
    "The enabled features are subject to change at any time.";

const char kEnableRawDrawName[] = "Enable raw draw";
const char kEnableRawDrawDescription[] =
    "When enabled, web content will be rastered on output surface directly.";

const char kEnableDelegatedCompositingName[] = "Enable delegated compositing";
const char kEnableDelegatedCompositingDescription[] =
    "When enabled and applicable, the act of compositing is delegated to the "
    "system compositor.";

const char kEnableRenderPassDrawnRectName[] =
    "Enable drawn rect for render passes";
const char kEnableRenderPassDrawnRectDescription[] =
    "When enabled and applicable, render passes will track drawn area in a "
    "rect to allow for finer grain damage.";

#if BUILDFLAG(IS_ANDROID)
const char kEnablePixPaymentsName[] = "Enable Pix payments";
const char kEnablePixPaymentsDescription[] =
    "When enabled, users will be offered to pay for Pix transactions using "
    "their bank accounts stored with Google payments.";

const char kEnablePixPaymentsInLandscapeModeName[] =
    "Enable Pix payments in landscape mode";
const char kEnablePixPaymentsInLandscapeModeDescription[] =
    "When enabled, users using their devices in landscape mode also will be "
    "offered to pay using their Pix accounts. Users using their devices in "
    "portrait mode are always offered to pay using their Pix accounts.";
#endif  // BUILDFLAG(IS_ANDROID)

const char kEnableRemovingAllThirdPartyCookiesName[] =
    "Enable removing SameSite=None cookies";
const char kEnableRemovingAllThirdPartyCookiesDescription[] =
    "Enables UI on chrome://settings/siteData to remove all third-party "
    "cookies and site data.";

const char kDesktopPWAsAdditionalWindowingControlsName[] =
    "Desktop PWA Additional Windowing Controls";
const char kDesktopPWAsAdditionalWindowingControlsDescription[] =
    "Enable PWAs to: (1) manually recreate the minimize, maximize and restore "
    "window functionalities, (2) set windows (non-/)resizable and (3) listen "
    "to window's move events with respective APIs.";

const char kDesktopPWAsAppTitleName[] = "Desktop PWA App Title";
const char kDesktopPWAsAppTitleDescription[] =
    "Enable PWAs to set a custom title for their windows.";

const char kDesktopPWAsElidedExtensionsMenuName[] =
    "Desktop PWAs elided extensions menu";
const char kDesktopPWAsElidedExtensionsMenuDescription[] =
    "Moves the Extensions \"puzzle piece\" icon from the title bar into the "
    "app menu for web app windows.";

const char kDesktopPWAsLaunchHandlerName[] = "Desktop PWA launch handler";
const char kDesktopPWAsLaunchHandlerDescription[] =
    "Enable web app manifests to declare app launch behavior. Prototype "
    "implementation of: "
    "https://github.com/WICG/web-app-launch/blob/main/launch_handler.md";

const char kDesktopPWAsTabStripName[] = "Desktop PWA tab strips";
const char kDesktopPWAsTabStripDescription[] =
    "Tabbed application mode - enables the `tabbed` display mode which allows "
    "web apps to add a tab strip to their app.";

const char kDesktopPWAsTabStripSettingsName[] =
    "Desktop PWA tab strips settings";
const char kDesktopPWAsTabStripSettingsDescription[] =
    "Experimental UI for selecting whether a PWA should open in tabbed mode.";

const char kDesktopPWAsTabStripCustomizationsName[] =
    "Desktop PWA tab strip customizations";
const char kDesktopPWAsTabStripCustomizationsDescription[] =
    "Enable PWAs to customize their tab strip when in tabbed mode by adding "
    "the `tab_strip` manifest field.";

const char kDesktopPWAsSubAppsName[] = "Desktop PWA Sub Apps";
const char kDesktopPWAsSubAppsDescription[] =
    "Enable installed PWAs to create shortcuts by installing their sub apps. "
    "Prototype implementation of: "
    "https://github.com/ivansandrk/multi-apps/blob/main/explainer.md";

const char kDesktopPWAsScopeExtensionsName[] = "Desktop PWA Scope Extensions";
const char kDesktopPWAsScopeExtensionsDescription[] =
    "Enable web app manifests to declare scope extensions to extend app scope "
    "to other origins. Prototype implementation of: "
    "https://github.com/WICG/manifest-incubations/blob/gh-pages/"
    "scope_extensions-explainer.md";

const char kDesktopPWAsBorderlessName[] = "Desktop PWA Borderless";
const char kDesktopPWAsBorderlessDescription[] =
    "Enable web app manifests to declare borderless mode as a display "
    "override. Prototype implementation of: go/borderless-mode.";

const char kEnableTLS13EarlyDataName[] = "TLS 1.3 Early Data";
const char kEnableTLS13EarlyDataDescription[] =
    "This option enables TLS 1.3 Early Data, allowing GET requests to be sent "
    "during the handshake when resuming a connection to a compatible TLS 1.3 "
    "server.";

const char kEnableTLS13KyberName[] = "TLS 1.3 post-quantum key agreement";
const char kEnableTLS13KyberDescription[] =
    "This option enables a post-quantum key agreement in TLS 1.3. "
    "The #use-ml-kem flag controls whether ML-KEM or Kyber is used.";

const char kUseMLKEMName[] = "Use ML-KEM in TLS 1.3";
const char kUseMLKEMDescription[] =
    "This option uses the ML-KEM standard as the post-quantum key agreement in "
    "TLS 1.3, instead of the Kyber draft standard. Post-quantum key agreement "
    "must be enabled (see #enable-tls13-kyber) for this option to have an "
    "effect.";

const char kAccessibilityAcceleratorName[] =
    "Experimental Accessibility accelerator";
const char kAccessibilityAcceleratorDescription[] =
    "This option enables the Accessibility accelerator.";

const char kAccessibilityCaretBlinkIntervalSettingName[] =
    "Adds text caret blink rate setting";
const char kAccessibilityCaretBlinkIntervalSettingDescription[] =
    "Allows the user to change their text caret blink rate setting across Ash "
    "and Chrome.";

const char kAccessibilityDisableTrackpadName[] =
    "Accessibility disable trackpad";
const char kAccessibilityDisableTrackpadDescription[] =
    "Adds a setting that allows the user to disable the built-in trackpad.";

const char kAccessibilityFlashScreenFeatureName[] =
    "Accessibility feature to flash the screen for each notification";
const char kAccessibilityFlashScreenFeatureDescription[] =
    "Allows the user to use a feature which flashes the screen for each "
    "notification.";

const char kAccessibilityOverscrollSettingFeatureName[] =
    "Adds overscroll to navigate setting";
const char kAccessibilityOverscrollSettingFeatureDescription[] =
    "Allows the user to change the swipe to navigate setting on ChromeOS in "
    "Accessibility settings.";

const char kAccessibilityServiceName[] = "Experimental Accessibility Service";
const char kAccessibilityServiceDescription[] =
    "This option enables the experimental Accessibility Service and runs some "
    "accessibility features in the service.";

const char kAccessibilityShakeToLocateName[] =
    "Adds shake cursor to locate feature";
const char kAccessibilityShakeToLocateDescription[] =
    "This option enables the experimental Accessibility feature to make the "
    "mouse cursor more visible when a shake is detected.";

const char kExperimentalAccessibilityColorEnhancementSettingsName[] =
    "Experimental Accessibility color enhancement settings";
const char kExperimentalAccessibilityColorEnhancementSettingsDescription[] =
    "This option enables the experimental Accessibility color enhancement "
    "settings found in the OS Accessibility settings.";

const char kAccessibilityChromeVoxPageMigrationName[] =
    "ChromeVox Page Migration";
const char kAccessibilityChromeVoxPageMigrationDescription[] =
    "This option enables ChromeVox page migration from extension options page "
    "to a Chrome OS settings page.";

const char kAccessibilityReducedAnimationsName[] =
    "Experimental Reduced Animations";
const char kAccessibilityReducedAnimationsDescription[] =
    "This option enables the setting to limit movement on the screen.";

const char kAccessibilityReducedAnimationsInKioskName[] =
    "Reduced Animations feature toggle available in Kiosk quick settings";
const char kAccessibilityReducedAnimationsInKioskDescription[] =
    "This option enables the quick settings option to toggle reduced "
    "animations.";

const char kAccessibilityFaceGazeName[] = "Experimental FaceGaze integration";
const char kAccessibilityFaceGazeDescription[] =
    "This option enables the experimental FaceGaze ChromeOS integration";

const char kAccessibilityFaceGazeGravityWellsName[] =
    "Experimental FaceGaze gravity wells";
const char kAccessibilityFaceGazeGravityWellsDescription[] =
    "This option enables the experimental gravity wells behavior for FaceGaze";

const char kAccessibilityMagnifierFollowsChromeVoxName[] =
    "Magnifier follows ChromeVox focus";
const char kAccessibilityMagnifierFollowsChromeVoxDescription[] =
    "This option enables the fullscreen magnifier to follow ChromeVox's focus.";

const char kAccessibilityMagnifierFollowsStsName[] =
    "Magnifier follows Select to Speak focus";
const char kAccessibilityMagnifierFollowsStsDescription[] =
    "This option enables the fullscreen magnifier to follow Select to Speak's "
    "reading focus.";

const char kAccessibilityMagnifyAcceleratorDialogName[] =
    "Magnifier turns on with accelerator dialog";
const char kAccessibilityMagnifyAcceleratorDialogDescription[] =
    "This option enables the fullscreen magnifier to turn on as soon as the "
    "accelerator dialog is shown.";

const char kAccessibilityMouseKeysName[] = "Mouse Keys";
const char kAccessibilityMouseKeysDescription[] =
    "This option enables you to control the mouse with the keyboard.";

const char kAccessibilitySelectToSpeakShortcutName[] =
    "Select to Speak keyboard shortcut";
const char kAccessibilitySelectToSpeakShortcutDescription[] =
    "This option enables the keyboard shortcut to enable or activate Select to "
    "Speak.";

const char kNewMacNotificationAPIName[] =
    "Determines which notification API to use on macOS devices";
const char kNewMacNotificationAPIDescription[] =
    "Enables the usage of Apple's new notification API";

const char kEnableFencedFramesName[] = "Enable the <fencedframe> element.";
const char kEnableFencedFramesDescription[] =
    "Fenced frames are an experimental web platform feature that allows "
    "embedding an isolated top-level page. This requires "
    "#privacy-sandbox-ads-apis to also be enabled. See "
    "https://github.com/shivanigithub/fenced-frame";

const char kEnableFencedFramesDeveloperModeName[] =
    "Enable the `FencedFrameConfig` constructor.";
const char kEnableFencedFramesDeveloperModeDescription[] =
    "The `FencedFrameConfig` constructor allows you to test the <fencedframe> "
    "element without running an ad auction, as you can manually supply a URL "
    "to navigate the fenced frame to.";

const char kEnableFencedFramesM120FeaturesName[] =
    "Enable the Fenced Frames M120 features";
const char kEnableFencedFramesM120FeaturesDescription[] =
    "The Fenced Frames M120 features include: 1. Support leaving interest "
    "group from ad components. 2. Allow automatic beacons to send at "
    "navigation start.";

const char kEnableGamepadButtonAxisEventsName[] =
    "Gamepad Button and Axis Events";
const char kEnableGamepadButtonAxisEventsDescription[] =
    "Enables the ability to subscribe to changes in buttons and/or axes "
    "on the gamepad object.";

const char kEnableGamepadMultitouchName[] = "Gamepad Multitouch";
const char kEnableGamepadMultitouchDescription[] =
    "Enables the ability to receive input from multitouch surface "
    "on the gamepad object.";

const char kEnableGenericSensorExtraClassesName[] =
    "Generic Sensor Extra Classes";
const char kEnableGenericSensorExtraClassesDescription[] =
    "Enables an extra set of sensor classes based on Generic Sensor API, which "
    "expose previously unavailable platform features, i.e. AmbientLightSensor "
    "and Magnetometer interfaces.";

const char kEnableGpuServiceLoggingName[] = "Enable gpu service logging";
const char kEnableGpuServiceLoggingDescription[] =
    "Enable printing the actual GL driver calls.";

const char kEnableIsolatedWebAppsName[] = "Enable Isolated Web Apps";
const char kEnableIsolatedWebAppsDescription[] =
    "Enables experimental support for Isolated Web Apps. "
    "See https://github.com/reillyeon/isolated-web-apps for more information.";

#if BUILDFLAG(IS_CHROMEOS)
const char kEnableIsolatedWebAppAutomaticUpdatesName[] =
    "Enable automatic updates of Isolated Web Apps";
const char kEnableIsolatedWebAppAutomaticUpdatesDescription[] =
    "Enables experimental support for automatically updating Isolated Web "
    "Apps.";

const char kEnableIsolatedWebAppUnmanagedInstallName[] =
    "Enable Isolated Web App unmanaged installation";
const char kEnableIsolatedWebAppUnmanagedInstallDescription[] =
    "Enables the installation of Isolated Web Apps on devices that are not "
    "managed by an enterprise.";

const char kEnableIsolatedWebAppManagedGuestSessionInstallName[] =
    "Enable Isolated Web App installation in managed guest sessions";
const char kEnableIsolatedWebAppManagedGuestSessionInstallDescription[] =
    "Enables the installation of Isolated Web Apps for users that are logged "
    "into a managed guest session.";
#endif

const char kEnableIsolatedWebAppDevModeName[] =
    "Enable Isolated Web App Developer Mode";
const char kEnableIsolatedWebAppDevModeDescription[] =
    "Enables the installation of unverified Isolated Web Apps";

const char kEnableControlledFrameName[] = "Enable Controlled Frame";
const char kEnableControlledFrameDescription[] =
    "Enables experimental support for Controlled Frame. See "
    "https://github.com/WICG/controlled-frame/blob/main/EXPLAINER.md "
    "for more information.";

const char kEnableFingerprintingProtectionBlocklistName[] =
    "Enable Fingerprinting Protection Blocklist";
const char kEnableFingerprintingProtectionBlocklistDescription[] =
    "Enable Fingerprinting Protection which may block fingerprinting "
    "resources from loading in a 3p context.";

const char kEnableShortcutCustomizationName[] =
    "Enable customization in new shortcuts app";
const char kEnableShortcutCustomizationDescription[] =
    "Enable customization of shortcuts in the new shortcuts app.";

const char kEnableSearchCustomizableShortcutsInLauncherName[] =
    "Enable search for customizable shortcuts in launcher";
const char kEnableSearchCustomizableShortcutsInLauncherDescription[] =
    "Enable searching for customizable shortcuts in launcher.";

const char kEnableSuspendStateMachineName[] = "Enable suspend state machine";
const char kEnableSuspendStateMachineDescription[] =
    "Enables a fix for the suspend keyboard shortcut to more consistently "
    "execute.";

const char kEnableInputDeviceSettingsSplitName[] =
    "Enable input device settings split";
const char kEnableInputDeviceSettingsSplitDescription[] =
    "Enable input device settings to be split per-device.";

const char kEnablePeripheralCustomizationName[] =
    "Enable peripheral customization";
const char kEnablePeripheralCustomizationDescription[] =
    "Enable peripheral customization to allow users to customize buttons on "
    "their peripherals.";

const char kEnablePeripheralNotificationName[] =
    "Enable peripheral notification";
const char kEnablePeripheralNotificationDescription[] =
    "Enable peripheral notification to notify users when a input device is "
    "connected to the user's chromebook for the first time.";

const char kEnablePeripheralsLoggingName[] = "Enable peripherals logging";
const char kEnablePeripheralsLoggingDescription[] =
    "Enable peripherals logging to get detailed logs of peripherals";

const char kExperimentalRgbKeyboardPatternsName[] =
    "Enable experimental RGB Keyboard patterns support";
const char kExperimentalRgbKeyboardPatternsDescription[] =
    "Enable experimental RGB Keyboard patterns support on supported devices.";

const char kClayBlockingDialogName[] = "Clay blocking dialog";
const char kClayBlockingDialogDescription[] =
    "Enables the blocking dialog that directs users to complete their choice "
    "of default apps (for Browser & Search) in Android.";

const char kTemplateUrlReconciliationName[] = "Merge TemplateUrl definitions";
const char kTemplateUrlReconciliationDialogDescription[] =
    "Allows Chrome to reconcile TemplateUrl definitions coming from external "
    "sources with its internal database.";

const char kEnableNetworkLoggingToFileName[] = "Enable network logging to file";
const char kEnableNetworkLoggingToFileDescription[] =
    "Enables network logging to a file named netlog.json in the user data "
    "directory. The file can be imported into chrome://net-internals.";

const char kDownloadNotificationServiceUnifiedAPIName[] =
    "Migrate download notification service to use new API";
const char kDownloadNotificationServiceUnifiedAPIDescription[] =
    "Migrate download notification service to use new unified API based on "
    "offline item and native persistence";

const char kDownloadsMigrateToJobsAPIName[] = "Migrate downloads use jobs API";
const char kDownloadsMigrateToJobsAPIDescription[] =
    "Migrate downloads to use user-initiated jobs instead of foreground "
    "service on Android 14";

const char kEnablePerfettoSystemTracingName[] =
    "Enable Perfetto system tracing";
const char kEnablePerfettoSystemTracingDescription[] =
    "When enabled, Chrome will attempt to connect to the system tracing "
    "service";

const char kEnableWindowsGamingInputDataFetcherName[] =
    "Enable Windows.Gaming.Input";
const char kEnableWindowsGamingInputDataFetcherDescription[] =
    "Enable Windows.Gaming.Input by default to provide game controller "
    "support on Windows 10 desktop.";

const char kBlockInsecurePrivateNetworkRequestsName[] =
    "Block insecure private network requests.";
const char kBlockInsecurePrivateNetworkRequestsDescription[] =
    "Prevents non-secure contexts from making subresource requests to "
    "more-private IP addresses. See also: "
    "https://developer.chrome.com/blog/private-network-access-update/";

const char kPipDoubleTapToResizeName[] =
    "Enable double-tap to resize PiP windows";
const char kPipDoubleTapToResizeDescription[] =
    "Enables double-tapping on existing PiP windows to resize "
    "them depending on its current state, such as minimizng or maximizing.";

const char kPipTuckName[] = "Enable tuck for PiP windows";
const char kPipTuckDescription[] = "Enables window tucking for PiP windows. ";

const char kPrivateNetworkAccessRespectPreflightResultsName[] =
    "Respect the result of Private Network Access preflights";
const char kPrivateNetworkAccessRespectPreflightResultsDescription[] =
    "Enables sending Private Network Access preflights ahead of requests to "
    "more-private IP addresses. These preflight requests must succeed in order "
    "for the request to proceed. See also: "
    "https://developer.chrome.com/blog/private-network-access-preflight/";

const char kPrivateNetworkAccessPreflightShortTimeoutName[] =
    "Reduce waiting time for Private Network Access preflights response";
const char kPrivateNetworkAccessPreflightShortTimeoutDescription[] =
    "Reduce the waiting time for Private Network Access preflights to 200 "
    "milliseconds. The default timeout period for requests is 5 minutes. "
    "See also: "
    "https://developer.chrome.com/blog/private-network-access-preflight/";

const char kPrivateNetworkAccessPermissionPromptName[] =
    "Enable Permission Prompt for Private Network Access";
const char kPrivateNetworkAccessPermissionPromptDescription[] =
    "Enable Permission Prompt for HTTPS public websites accessing HTTP "
    "more-private devices. Require to set a fetch option `targetAddressSpace` "
    "on the request to relax mixed content check. "
    "See also: "
    "https://developer.chrome.com/blog/"
    "private-network-access-update-2023-02-02/";

const char kPrivateNetworkAccessIgnoreWorkerErrorsName[] =
    "Ignore Private Network Access errors for worker-related requests";
const char kPrivateNetworkAccessIgnoreWorkerErrorsDescription[] =
    "Ignore Private Network Access errors for worker-related requests, "
    "including worker script fetches, and fetches initiated in a worker. "
    "Instead of blocking the requests, the errors will be shown as an issue in "
    "DevTools. See "
    "also: https://developer.chrome.com/blog/private-network-access-update/ "
    "and https://developer.chrome.com/blog/private-network-access-preflight/";

const char kPrivateNetworkAccessIgnoreNavigationErrorsName[] =
    "Ignore Private Network Access errors for navigation-related requests";
const char kPrivateNetworkAccessIgnoreNavigationErrorsDescription[] =
    "Ignore Private Network Access errors for navigation-related requests, "
    "including iframes and popups. Instead of blocking the requests, the errors"
    "will be shown as an issue in DevTools. See also: "
    "https://developer.chrome.com/blog/private-network-access-update/ "
    "and https://developer.chrome.com/blog/private-network-access-preflight/";

const char kDeprecateAltClickName[] =
    "Enable Alt+Click deprecation notifications";
const char kDeprecateAltClickDescription[] =
    "Start providing notifications about Alt+Click deprecation and enable "
    "Search+Click as an alternative.";

const char kExperimentalAccessibilityLanguageDetectionName[] =
    "Experimental accessibility language detection";
const char kExperimentalAccessibilityLanguageDetectionDescription[] =
    "Enable language detection for in-page content which is then exposed to "
    "assistive technologies such as screen readers.";

const char kExperimentalAccessibilityLanguageDetectionDynamicName[] =
    "Experimental accessibility language detection for dynamic content";
const char kExperimentalAccessibilityLanguageDetectionDynamicDescription[] =
    "Enable language detection for dynamic content which is then exposed to "
    "assistive technologies such as screen readers.";

const char kMemlogName[] = "Chrome heap profiler start mode.";
const char kMemlogDescription[] =
    "Starts heap profiling service that records sampled memory allocation "
    "profile having each sample attributed with a callstack. "
    "The sampling resolution is controlled with --memlog-sampling-rate flag. "
    "Recorded heap dumps can be obtained at chrome://tracing "
    "[category:memory-infra] and chrome://memory-internals. This setting "
    "controls which processes will be profiled since their start. To profile "
    "any given process at a later time use chrome://memory-internals page.";
const char kMemlogModeMinimal[] = "Browser and GPU";
const char kMemlogModeAll[] = "All processes";
const char kMemlogModeAllRenderers[] = "All renderers";
const char kMemlogModeRendererSampling[] = "Single renderer";
const char kMemlogModeBrowser[] = "Browser only";
const char kMemlogModeGpu[] = "GPU only";

const char kMemlogSamplingRateName[] =
    "Heap profiling sampling interval (in bytes).";
const char kMemlogSamplingRateDescription[] =
    "Heap profiling service uses Poisson process to sample allocations. "
    "Default value for the interval between samples is 1000000 (1MB). "
    "This results in low noise for large and/or frequent allocations "
    "[size * frequency >> 1MB]. This means that aggregate numbers [e.g. "
    "total size of malloc-ed objects] and large and/or frequent allocations "
    "can be trusted with high fidelity. "
    "Lower intervals produce higher samples resolution, but come at a cost of "
    "higher performance overhead.";
const char kMemlogSamplingRate10KB[] = "10KB";
const char kMemlogSamplingRate50KB[] = "50KB";
const char kMemlogSamplingRate100KB[] = "100KB";
const char kMemlogSamplingRate500KB[] = "500KB";
const char kMemlogSamplingRate1MB[] = "1MB";
const char kMemlogSamplingRate5MB[] = "5MB";

const char kMemlogStackModeName[] = "Heap profiling stack traces type.";
const char kMemlogStackModeDescription[] =
    "By default heap profiling service records native stacks. "
    "A post-processing step is required to symbolize the stacks. "
    "'Native with thread names' adds the thread name as the first frame of "
    "each native stack. It's also possible to record a pseudo stack using "
    "trace events as identifiers. It's also possible to do a mix of both.";
const char kMemlogStackModeNative[] = "Native";
const char kMemlogStackModeNativeWithThreadNames[] = "Native with thread names";

const char kMirrorBackForwardGesturesInRTLName[] =
    "Mirror back forward gestures in RTL";
const char kMirrorBackForwardGesturesInRTLDescription[] =
    "When the OS UI language is right-to-left, the back-forward gesture "
    "directions are flipped so that the left edge is considered forward and "
    "right is considered back.";

const char kEnableLensStandaloneFlagId[] = "enable-lens-standalone";
const char kEnableLensStandaloneName[] = "Enable Lens features in Chrome.";
const char kEnableLensStandaloneDescription[] =
    "Enables Lens image and region search to learn about the visual content "
    "you see while you browse and shop on the web.";

const char kEnableManagedConfigurationWebApiName[] =
    "Enable Managed Configuration Web API";
const char kEnableManagedConfigurationWebApiDescription[] =
    "Allows website to access a managed configuration provided by the device "
    "administrator for the origin.";

const char kEnablePixelCanvasRecordingName[] = "Enable pixel canvas recording";
const char kEnablePixelCanvasRecordingDescription[] =
    "Pixel canvas recording allows the compositor to raster contents aligned "
    "with the pixel and improves text rendering. This should be enabled when a "
    "device is using fractional scale factor.";

const char kEnableProcessPerSiteUpToMainFrameThresholdName[] =
    "Enable ProcessPerSite up to main frame threshold";
const char kEnableProcessPerSiteUpToMainFrameThresholdDescription[] =
    "Proactively reuses same-site renderer processes to host multiple main "
    "frames, up to a certain threshold.";

const char kDropInputEventsBeforeFirstPaintName[] =
    "Drop Input Events Before First Paint";
const char kDropInputEventsBeforeFirstPaintDescription[] =
    "Before the user can see the first paint of a new page they cannot "
    "intentionally interact with elements on that page. By dropping the events "
    "we prevent accidental interaction with a page the user has not seen yet.";

const char kBoundaryEventDispatchTracksNodeRemovalName[] =
    "Boundary Event Dispatch Tracks Node Removal";
const char kBoundaryEventDispatchTracksNodeRemovalDescription[] =
    "Mouse and Pointer boundary event dispatch (i.e. dispatch of enter, leave, "
    "over, out events) tracks DOM node removal to fix event pairing on "
    "ancestor nodes.";

const char kEnableBubbleCornerRadiusUpdateName[] =
    "Enable corner radius update for system bubbles.";
const char kEnableBubbleCornerRadiusUpdateDescription[] =
    "When enabled, the launcher bubble and tray bubble corner radius is "
    "updated.";

const char kEnableCssSelectorFragmentAnchorName[] =
    "Enables CSS selector fragment anchors";
const char kEnableCssSelectorFragmentAnchorDescription[] =
    "Similar to text directives, CSS selector directives can be specified "
    "in a url which is to be scrolled into view and highlighted.";

const char kRetailCouponsName[] = "Enable to fetch for retail coupons";
const char kRetailCouponsDescription[] =
    "Allow to fetch retail coupons for consented users";

const char kEnablePreferencesAccountStorageName[] =
    "Enable the account data storage for preferences for syncing users";
const char kEnablePreferencesAccountStorageDescription[] =
    "Enables storing preferences in a second, Gaia-account-scoped storage for "
    "syncing users";

const char kEnableResamplingScrollEventsExperimentalPredictionName[] =
    "Enable experimental prediction for scroll events";
const char kEnableResamplingScrollEventsExperimentalPredictionDescription[] =
    "Predicts the scroll amount after the vsync time to more closely match "
    "when the frame is visible.";

const char kEnableSystemEntropyOnPerformanceNavigationTimingName[] =
    "Enable the systemEntropy property on PerformanceNavigationTiming";
const char kEnableSystemEntropyOnPerformanceNavigationTimingDescription[] =
    "Allows developers to discern if the top level navigation occured during "
    "while the user agent was under load. See "
    "https://chromestatus.com/feature/5186950448283648 for more information.";

const char kEnableZeroCopyTabCaptureName[] = "Zero-copy tab capture";
const char kEnableZeroCopyTabCaptureDescription[] =
    "Enable zero-copy content tab for getDisplayMedia() APIs.";

const char kExperimentalWebAssemblyFeaturesName[] = "Experimental WebAssembly";
const char kExperimentalWebAssemblyFeaturesDescription[] =
    "Enable web pages to use experimental WebAssembly features.";

const char kExperimentalWebAssemblyJSPIName[] =
    "Experimental WebAssembly JavaScript Promise Integration (JSPI)";
const char kExperimentalWebAssemblyJSPIDescription[] =
    "Enable web pages to use experimental WebAssembly JavaScript Promise "
    "Integration (JSPI) "
    "API.";

const char kEnableUnrestrictedUsbName[] =
    "Enable Isolated Web Apps to bypass USB restrictions";
const char kEnableUnrestrictedUsbDescription[] =
    "When enabled, allows Isolated Web Apps to access blocklisted "
    "devices and protected interfaces through WebUSB API.";

const char kEnableWasmBaselineName[] = "WebAssembly baseline compiler";
const char kEnableWasmBaselineDescription[] =
    "Enables WebAssembly baseline compilation and tier up.";

const char kEnableWasmLazyCompilationName[] = "WebAssembly lazy compilation";
const char kEnableWasmLazyCompilationDescription[] =
    "Enables lazy (JIT on first call) compilation of WebAssembly modules.";

const char kEnableWasmGarbageCollectionName[] =
    "WebAssembly Garbage Collection";
const char kEnableWasmGarbageCollectionDescription[] =
    "Enables the experimental Garbage Collection (GC) extensions to "
    "WebAssembly.";

const char kEnableWasmMemory64Name[] = "WebAssembly Memory64";
const char kEnableWasmMemory64Description[] =
    "Enables 64-bit memory indices on WebAssembly operations";

const char kEnableWasmRelaxedSimdName[] = "WebAssembly Relaxed SIMD";
const char kEnableWasmRelaxedSimdDescription[] =
    "Enables the use of WebAssembly vector operations with relaxed semantics";

const char kEnableWasmStringrefName[] = "WebAssembly Stringref";
const char kEnableWasmStringrefDescription[] =
    "Enables the experimental stringref (reference-typed strings) extensions "
    "to WebAssembly.";

const char kEnableWasmTieringName[] = "WebAssembly tiering";
const char kEnableWasmTieringDescription[] =
    "Enables tiered compilation of WebAssembly (will tier up to TurboFan if "
    "#enable-webassembly-baseline is enabled).";

const char kEvDetailsInPageInfoName[] = "EV certificate details in Page Info.";
const char kEvDetailsInPageInfoDescription[] =
    "Shows the EV certificate details in the Page Info bubble.";

const char kExperimentalWebPlatformFeaturesName[] =
    "Experimental Web Platform features";
const char kExperimentalWebPlatformFeaturesDescription[] =
    "Enables experimental Web Platform features that are in development.";

const char kSafeBrowsingAsyncRealTimeCheckName[] =
    "Safe Browsing Async Real Time Check";
const char kSafeBrowsingAsyncRealTimeCheckDescription[] =
    "Safe Browsing real-time checks are conducted asynchronously. They no "
    "longer delay page load.";

const char kSafeBrowsingHashPrefixRealTimeLookupsName[] =
    "Safe Browsing Hash Prefix Real Time Lookups";
const char kSafeBrowsingHashPrefixRealTimeLookupsDescription[] =
    "Enable checking URLs through Safe Browsing hash-prefix real time "
    "protocol.";

const char kSafeBrowsingLocalListsUseSBv5Name[] =
    "Safe Browsing Local Lists use v5 API";
const char kSafeBrowsingLocalListsUseSBv5Description[] =
    "Fetch and check local lists using the Safe Browsing v5 API instead of the "
    "v4 Update API.";

#if BUILDFLAG(ENABLE_EXTENSIONS)
const char kEnableWebHidInWebViewName[] = "Web HID in WebView";
const char kEnableWebHidInWebViewDescription[] =
    "Enable WebViews to access Web HID upon embedder's permission.";

const char kExtensionAiDataCollectionName[] =
    "Enables AI Data collection via extension";
const char kExtensionAiDataCollectionDescription[] =
    "Enables an extension API to allow specific extensions to collect data "
    "from browser process. This data may contain profile specific information "
    " and may be otherwise unavailable to an extension.";
const char kExtensionsMenuAccessControlName[] =
    "Extensions Menu Access Control";
const char kExtensionsMenuAccessControlDescription[] =
    "Enables a redesigned extensions menu that allows the user to control "
    "extensions site access.";
const char kIPHExtensionsMenuFeatureName[] = "IPH Extensions Menu";
const char kIPHExtensionsMenuFeatureDescription[] =
    "Enables In-Product-Help that appears when at least one extension has "
    "access to the current page. This feature is gated by "
    "extensions-menu-access-control.";
const char kIPHExtensionsRequestAccessButtonFeatureName[] =
    "IPH Extensions Request Access Button Feature";
const char kIPHExtensionsRequestAccessButtonFeatureDescription[] =
    "Enables In-Product-Help that appears when at least one extension is "
    "requesting access to the current page. This feature is gated by "
    "extensions-menu-access-control.";
const char kExtensionManifestV2DeprecationWarningName[] =
    "Extension Manifest V2 Deprecation Warning Stage";
const char kExtensionManifestV2DeprecationWarningDescription[] =
    "Displays a warning that affected MV2 extensions may no longer be "
    "supported due to the Manifest V2 deprecation.";
const char kExtensionManifestV2DeprecationDisabledName[] =
    "Extension Manifest V2 Deprecation Disabled Stage";
const char kExtensionManifestV2DeprecationDisabledDescription[] =
    "Displays a warning that affected MV2 extensions were turned off due to "
    "the Manifest V2 deprecation.";
const char kExtensionManifestV2DeprecationUnsupportedName[] =
    "Extension Manifest V2 Deprecation Unsupported Stage";
const char kExtensionManifestV2DeprecationUnsupportedDescription[] =
    "Displays a warning that affected MV2 extensions were turned off due to "
    "the Manifest V2 deprecation and cannot be re-enabled.";

const char kWebAuthFlowInBrowserTabName[] =
    "Web Authentication Flow in Browser Tab";
const char kWebAuthFlowInBrowserTabDescription[] =
    "Web authentication flows to be displayed in a Browser Tab instead of an "
    "App Window. The flows are used via the Chrome Extension API, using "
    "`chrome.identity` functions. Browser Tab can be displayed either in a New "
    "Tab or a Popup Window via the feature paramters.";

const char kCWSInfoFastCheckName[] = "CWS Info Fast Check";
const char kCWSInfoFastCheckDescription[] =
    "When enabled, Chrome checks and fetches metadata for installed extensions "
    "more frequently.";

const char kExtensionTelemetryForEnterpriseName[] =
    "Extension Telemetry for Enterprise";
const char kExtensionTelemetryForEnterpriseDescription[] =
    "When enabled, the extension telemetry service collects signals and "
    "generates reports to send for enterprise.";

const char kSafetyCheckExtensionsName[] = "Extensions Module in Safety Check";
const char kSafetyCheckExtensionsDescription[] =
    "When enabled, adds the Extensions Module to Safety Check on "
    "desktop. The module will be shown if there are potentially unsafe "
    "extensions to review.";

const char kExtensionsToolbarZeroStateName[] = "Extensions Toolbar Zero State";
const char kExtensionsToolbarZeroStateDescription[] =
    "When enabled, show an IPH to prompt users with zero extensions installed "
    "to interact with the Extensions Toolbar Button. Upon the user clicking "
    "the toolbar button, display a submenu that suggests exploring the Chrome "
    "Web Store.";
const char kExtensionsToolbarZeroStateChoicesDisabled[] = "Disabled";
const char kExtensionsToolbarZeroStateVistWebStore[] = "Visit Chrome Web Store";
const char kExtensionsToolbarZeroStateExploreExtensionsByCategory[] =
    "Explore CWS extensions by category";

#if BUILDFLAG(IS_CHROMEOS)
const char kExtensionWebFileHandlersName[] = "Extensions Web File Handlers";
const char kExtensionWebFileHandlersDescription[] =
    "Enable Extension Web File Handlers, which allows extensions to operate on "
    "the native file system. An extension can register to read and edit files, "
    "specified in the manifest, by their file extension or mime type.";
#endif  // IS_CHROMEOS
#endif  // ENABLE_EXTENSIONS

const char kExtensionsOnChromeUrlsName[] = "Extensions on chrome:// URLs";
const char kExtensionsOnChromeUrlsDescription[] =
    "Enables running extensions on chrome:// URLs, where extensions explicitly "
    "request this permission.";

const char kFractionalScrollOffsetsName[] = "Fractional Scroll Offsets";
const char kFractionalScrollOffsetsDescription[] =
    "Enables fractional scroll offsets inside Blink, exposing non-integer "
    "offsets to web APIs.";

const char kFedCmAuthzName[] = "FedCmAuthz";
const char kFedCmAuthzDescription[] =
    "Enables RPs to request authorization for custom IdP scopes.";

const char kFedCmButtonModeName[] = "FedCmButtonMode";
const char kFedCmButtonModeDescription[] =
    "Enables RPs specify whether they want to trigger the FedCM widget flow or "
    "the button flow.";

const char kFedCmIdPRegistrationName[] = "FedCM with IdP Registration support";
const char kFedCmIdPRegistrationDescription[] =
    "Enables RPs to get identity credentials from registered IdPs.";

const char kFedCmMetricsEndpointName[] = "FedCmMetricsEndpoint";
const char kFedCmMetricsEndpointDescription[] =
    "Allows the FedCM API to send performance measurement to the metrics "
    "endpoint on the identity provider side. Requires FedCM to be enabled.";

const char kFedCmMultiIdpName[] = "FedCmMultiIdp";
const char kFedCmMultiIdpDescription[] =
    "Allows the FedCM API to request multiple identity providers "
    "simultaneously. Requires FedCM to be enabled as well.";

const char kFedCmSelectiveDisclosureName[] = "FedCmSelectiveDisclosure";
const char kFedCmSelectiveDisclosureDescription[] =
    "Allows a relying party to selectively request a set of identity "
    "attributes to be disclosed.";

const char kFedCmWithStorageAccessAPIName[] = "FedCmWithStorageAccessAPI";
const char kFedCmWithStorageAccessAPIDescription[] =
    "Allows Storage Access API to auto-resolve if the user has allowed a FedCM "
    "connection.";

const char kFedCmWithoutWellKnownEnforcementName[] =
    "FedCmWithoutWellKnownEnforcement";
const char kFedCmWithoutWellKnownEnforcementDescription[] =
    "Supports configURL that's not in the IdP's .well-known file.";

const char kFedCmUseOtherAccountName[] = "FedCmUseOtherAccount";
const char kFedCmUseOtherAccountDescription[] =
    "Enables the \"Use a different account\" button on the FedCM account "
    "chooser to log in to another IdP account, for IdPs who have opted in.";

const char kWebIdentityDigitalCredentialsName[] = "DigitalCredentials";
const char kWebIdentityDigitalCredentialsDescription[] =
    "Enables the three-party verifier/holder/issuer identity model.";

const char kFileHandlingIconsName[] = "File Handling Icons";
const char kFileHandlingIconsDescription[] =
    "Allows websites using the file handling API to also register file type "
    "icons. See https://github.com/WICG/file-handling/blob/main/explainer.md "
    "for more information.";

const char kFileSystemAccessPersistentPermissionUpdatedPageInfoName[] =
    "Updated Page Info UI for the File System Access API Persistent "
    "Permissions";
const char kFileSystemAccessPersistentPermissionUpdatedPageInfoDescription[] =
    "Allows users to opt in to the updated Page Info UI component for the "
    "File System Access persistent permissions feature.";

const char kFileSystemObserverName[] = "FileSystemObserver";
const char kFileSystemObserverDescription[] =
    "Enables the FileSystemObserver interface, which allows websites to be "
    "notified of changes to the file system. See "
    "https://github.com/whatwg/fs/blob/main/proposals/FileSystemObserver.md "
    "for more information.";

const char kDrawImmediatelyWhenInteractiveName[] =
    "Enable Immediate Draw When Interactive";
const char kDrawImmediatelyWhenInteractiveDescription[] =
    "Causes viz to activate and draw frames immediately during a touch "
    "interaction or scroll.";

const char kAckOnSurfaceActivationWhenInteractiveName[] =
    "Ack On Surface Activation When Interactive";
const char kAckOnSurfaceActivationWhenInteractiveDescription[] =
    "If enabled, immediately send acks to clients when a viz surface "
    "activates and when that surface is a dependency of an interactive frame "
    "(i.e., when there is an active scroll or a touch interaction). This "
    "effectively removes back-pressure in this case. This can result in "
    "wasted work and contention, but should regularize the timing of client "
    "rendering.";

const char kFluentOverlayScrollbarsName[] = "Fluent Overlay scrollbars.";
const char kFluentOverlayScrollbarsDescription[] =
    "Stylizes scrollbars with Microsoft Fluent design and makes them overlay "
    "over the web's content.";

const char kFluentScrollbarsName[] = "Fluent scrollbars.";
const char kFluentScrollbarsDescription[] =
    "Stylizes scrollbars with Microsoft Fluent design.";

const char kMutationEventsName[] =
    "Enable (deprecated) synchronous mutation events";
const char kMutationEventsDescription[] =
    "Mutation Events are a deprecated set of events which cause performance "
    "issues. Disabling this feature turns off Mutation Events. NOTE: Disabling "
    "these events can cause breakage on some sites that are still reliant on "
    "these deprecated features.";

const char kKeyboardFocusableScrollersName[] =
    "Enables keyboard focusable scrollers";
const char kKeyboardFocusableScrollersDescription[] =
    "Scrollers without focusable children are keyboard-focusable by default.";

const char kFillOnAccountSelectName[] = "Fill passwords on account selection";
const char kFillOnAccountSelectDescription[] =
    "Filling of passwords when an account is explicitly selected by the user "
    "rather than autofilling credentials on page load.";

const char kForceTextDirectionName[] = "Force text direction";
const char kForceTextDirectionDescription[] =
    "Explicitly force the per-character directionality of UI text to "
    "left-to-right (LTR) or right-to-left (RTL) mode, overriding the default "
    "direction of the character language.";
const char kForceDirectionLtr[] = "Left-to-right";
const char kForceDirectionRtl[] = "Right-to-left";

const char kForceUiDirectionName[] = "Force UI direction";
const char kForceUiDirectionDescription[] =
    "Explicitly force the UI to left-to-right (LTR) or right-to-left (RTL) "
    "mode, overriding the default direction of the UI language.";

const char kMediaRemotingWithoutFullscreenName[] =
    "Media Remoting without videos in fullscreen mode";
const char kMediaRemotingWithoutFullscreenDescription[] =
    "Starts Media Remoting from Global Media Controls without making the "
    "videos fullscreen.";

const char kRemotePlaybackBackendName[] = "Remote Playback API implementation";
const char kRemotePlaybackBackendDescription[] =
    "Enables the Remote Playback API implementation.";

#if !BUILDFLAG(IS_CHROMEOS)
const char kGlobalMediaControlsUpdatedUIName[] =
    "Global Media Controls updated UI";
const char kGlobalMediaControlsUpdatedUIDescription[] =
    "Show updated UI for Global Media Controls in all the non-CrOS desktop "
    "platforms.";
#endif  // !BUILDFLAG(IS_CHROMEOS)

const char kGoogleOneOfferFilesBannerName[] = "Google One offer Files banner";
const char kGoogleOneOfferFilesBannerDescription[] =
    "Shows a Files banner about Google One offer.";

const char kObservableAPIName[] = "Observable API";
const char kObservableAPIDescription[] =
    "A reactive programming primitive for ergonomically handling streams of "
    "async data. See https://github.com/WICG/observable.";

const char kOutlineSilhouetteIconName[] = "Outline Silhouette Icon";
const char kOutlineSilhouetteIconDescription[] =
    "Enables the improved silhouette icon for signed out profiles";

const char kAtomicMoveAPIName[] = "Atomic DOM move (Node.moveBefore)";
const char kAtomicMoveAPIDescription[] =
    "Introduces a state-preserving atomic move primitive to the DOM, "
    "by calling Node.moveBefore. See "
    "https://github.com/whatwg/dom/issues/1255.";

const char kCastStreamingAv1Name[] =
    "Enable AV1 video encoding for Cast Streaming";
const char kCastStreamingAv1Description[] =
    "Offers the AV1 video codec when negotiating Cast Streaming, and uses AV1 "
    "if selected for the session.";

const char kCastStreamingHardwareH264Name[] =
    "Toggle hardware accelerated H.264 video encoding for Cast Streaming";
const char kCastStreamingHardwareH264Description[] =
    "The default is to allow hardware H.264 encoding when recommended for the "
    "platform. If enabled, hardware H.264 encoding will always be allowed when "
    "supported by the platform. If disabled, hardware H.264 encoding will "
    "never be used.";

const char kCastStreamingHardwareVp8Name[] =
    "Toggle hardware accelerated VP8 video encoding for Cast Streaming";
const char kCastStreamingHardwareVp8Description[] =
    "The default is to allow hardware VP8 encoding when recommended for the "
    "platform. If enabled, hardware VP8 encoding will always be allowed when "
    "supported by the platform (regardless of recommendation). If disabled, "
    "hardware VP8 encoding will never be used.";

const char kCastStreamingHardwareVp9Name[] =
    "Toggle hardware accelerated VP9 video encoding for Cast Streaming";
const char kCastStreamingHardwareVp9Description[] =
    "The default is to allow hardware VP9 encoding when recommended for the "
    "platform. If enabled, hardware VP9 encoding will always be allowed when "
    "supported by the platform (regardless of recommendation). If disabled, "
    "hardware VP9 encoding will never be used.";

const char kCastStreamingPerformanceOverlayName[] =
    "Toggle a performance metrics overlay while Cast Streaming";
const char kCastStreamingPerformanceOverlayDescription[] =
    "When enabled, a text overlay is rendered on top of each frame sent while "
    "Cast Streaming that includes frame duration, resolution, timestamp, "
    "low latency mode, capture duration, target playout delay, target bitrate, "
    "and encoder utilitization.";

const char kCastStreamingVp8Name[] =
    "Enable VP8 video encoding for Cast Streaming";
const char kCastStreamingVp8Description[] =
    "Offers the VP8 video codec when negotiating Cast Streaming, and uses VP8 "
    "if selected for the session. If true, software VP8 encoding will be "
    "offered and hardware VP8 encoding may be offered if enabled and available "
    "on this platform. If false, software VP8 will not be offered and hardware "
    "VP8 will only be offered if #cast-streaming-hardware-vp8 is explicitly "
    "set to true.";

const char kCastStreamingVp9Name[] =
    "Enable VP9 video encoding for Cast Streaming";
const char kCastStreamingVp9Description[] =
    "Offers the VP9 video codec when negotiating Cast Streaming, and uses VP9 "
    "if selected for the session.";

#if BUILDFLAG(IS_MAC)
const char kCastStreamingMacHardwareH264Name[] =
    "Enable hardware H264 video encoding on for Cast Streaming on macOS";
const char kCastStreamingMacHardwareH264Description[] =
    "Offers the H264 video codec when negotiating Cast Streaming, and uses "
    "hardware-accelerated H264 encoding if selected for the session";
const char kUseNetworkFrameworkForLocalDiscoveryName[] =
    "Use the Network Framework for local device discovery on Mac";
const char kUseNetworkFrameworkForLocalDiscoveryDescription[] =
    "Use the Network Framework to replace the Bonjour API for local device "
    "discovery on Mac.";
#endif

#if BUILDFLAG(IS_WIN)
const char kCastStreamingWinHardwareH264Name[] =
    "Enable hardware H264 video encoding on for Cast Streaming on Windows";
const char kCastStreamingWinHardwareH264Description[] =
    "Offers the H264 video codec when negotiating Cast Streaming, and uses "
    "hardware-accelerated H264 encoding if selected for the session";
#endif

const char kCastEnableStreamingWithHiDPIName[] =
    "HiDPI tab capture support for Cast Streaming";
const char kCastEnableStreamingWithHiDPIDescription[] =
    "Enables HiDPI tab capture during Cast Streaming mirroring sessions. May "
    "reduce performance on some platforms and also improve quality of video "
    "frames.";

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kFlexFirmwareUpdateName[] = "ChromeOS Flex Firmware Updates";
const char kFlexFirmwareUpdateDescription[] =
    "Allow firmware updates from LVFS to be installed on ChromeOS Flex.";
#endif

const char kGpuRasterizationName[] = "GPU rasterization";
const char kGpuRasterizationDescription[] = "Use GPU to rasterize web content.";

const char kContextualPageActionsName[] = "Contextual page actions";
const char kContextualPageActionsDescription[] =
    "Enables contextual page action feature.";

const char kContextualPageActionsReaderModeName[] =
    "Contextual page actions - reader mode";
const char kContextualPageActionsReaderModeDescription[] =
    "Enables reader mode as a contextual page action.";

const char kContextualPageActionsShareModelName[] =
    "Contextual page actions - share model";
const char kContextualPageActionsShareModelDescription[] =
    "Enables share model data collection.";

const char kHardwareMediaKeyHandling[] = "Hardware Media Key Handling";
const char kHardwareMediaKeyHandlingDescription[] =
    "Enables using media keys to control the active media session. This "
    "requires MediaSessionService to be enabled too";

const char kHeavyAdPrivacyMitigationsName[] = "Heavy ad privacy mitigations";
const char kHeavyAdPrivacyMitigationsDescription[] =
    "Enables privacy mitigations for the heavy ad intervention. Disabling "
    "this makes the intervention deterministic. Defaults to enabled.";

const char kHideIncognitoMediaMetadataName[] =
    "Hide media metadata when in Incognito";
const char kHideIncognitoMediaMetadataDescription[] =
    "When enabled, media metadata will be hidden from your OS' media player "
    "if you are in an Incognito session.";

const char kHistoryEmbeddingsName[] = "History Embeddings";
const char kHistoryEmbeddingsDescription[] =
    "When enabled, the history embeddings feature may operate.";

const char kHistoryEmbeddingsAnswersName[] = "History Embeddings Answers";
const char kHistoryEmbeddingsAnswersDescription[] =
    "When enabled, the history embeddings feature may answer some queries. "
    "Has no effect if the History Embeddings feature is disabled.";

const char kTabAudioMutingName[] = "Tab audio muting UI control";
const char kTabAudioMutingDescription[] =
    "When enabled, the audio indicators in the tab strip double as tab audio "
    "mute controls.";

const char kCrasProcessorDedicatedThreadName[] =
    "Run CrasProcessor in a dedicated thread";
const char kCrasProcessorDedicatedThreadDescription[] =
    "Run CrasProcessor in a separate thread out of the audio thread";

const char kCrasProcessorWavDumpName[] = "Enable CrasProcessor WAVE file dumps";
const char kCrasProcessorWavDumpDescription[] =
    "Make CrasProcessor produce WAVE file dumps for the audio processing "
    "pipeline";

const char kPwaRestoreBackendName[] = "Enable the PWA Restore Backend";
const char kPwaRestoreBackendDescription[] =
    "When enabled, PWA data will be sync to the backend, to support the PWA "
    "Restore UI.";

const char kPwaRestoreUiName[] = "Enable the PWA Restore UI";
const char kPwaRestoreUiDescription[] =
    "When enabled, the PWA Restore UI can be shown";

const char kPwaRestoreUiAtStartupName[] =
    "Force-shows the PWA Restore UI at startup";
const char kPwaRestoreUiAtStartupDescription[] =
    "When enabled, the PWA Restore UI will be forced to show on startup (even "
    "if the PwaRestoreUi flag is disabled and there are no apps to restore)";

const char kStartSurfaceReturnTimeName[] = "Start surface return time";
const char kStartSurfaceReturnTimeDescription[] =
    "Enable showing start surface at startup after specified time has elapsed";

const char kHttpsFirstBalancedModeName[] =
    "Allow enabling Balanced Mode for HTTPS-First Mode.";
const char kHttpsFirstBalancedModeDescription[] =
    "Enable tri-state HTTPS-First Mode setting in chrome://settings/security.";

const char kHttpsFirstDialogUiName[] = "Dialog UI for HTTPS-First Modes";
const char kHttpsFirstDialogUiDescription[] = "Use a dialog-based UI for HFM";

const char kHttpsFirstModeInterstitialAugust2024RefreshName[] =
    "Use the new interstitial for HTTPS-First Modes";
const char kHttpsFirstModeInterstitialAugust2024RefreshDescription[] =
    "Use the new interstitial with updated text and icons for HTTPS-First "
    "Modes";

const char kHttpsFirstModeIncognitoName[] = "HTTPS-First Mode in Incognito";
const char kHttpsFirstModeIncognitoDescription[] =
    "Enable HTTPS-First Mode in Incognito as default setting.";

const char kHttpsFirstModeIncognitoNewSettingsName[] =
    "HTTPS-First Mode in Incognito new Settings UI";
const char kHttpsFirstModeIncognitoNewSettingsDescription[] =
    "Enable new HTTPS-First Mode settings UI for HTTPS-First Mode in "
    "Incognito. Must also enable #https-first-mode-incognito.";

const char kHttpsFirstModeV2Name[] = "HTTPS-First Mode V2";
const char kHttpsFirstModeV2Description[] =
    "Enable rearchitected version of HTTPS-First Mode.";

const char kHttpsFirstModeV2ForEngagedSitesName[] =
    "HTTPS-First Mode V2 For Engaged Sites";
const char kHttpsFirstModeV2ForEngagedSitesDescription[] =
    "Enable Site-Engagement based HTTPS-First Mode. Shows HTTPS-First Mode "
    "interstitial on sites whose HTTPS URLs have high Site Engagement scores. "
    "Requires #https-upgrades feature to be enabled";

const char kHttpsFirstModeForTypicallySecureUsersName[] =
    "HTTPS-First Mode For Typically Secure Users";
const char kHttpsFirstModeForTypicallySecureUsersDescription[] =
    "Automatically enables HTTPS-First Mode if the user has a typically secure "
    "browsing pattern.";

const char kHttpsUpgradesName[] = "HTTPS Upgrades";
const char kHttpsUpgradesDescription[] =
    "Enable automatically upgrading all top-level navigations to HTTPS with "
    "fast fallback to HTTP.";

const char kIgnoreGpuBlocklistName[] = "Override software rendering list";
const char kIgnoreGpuBlocklistDescription[] =
    "Overrides the built-in software rendering list and enables "
    "GPU-acceleration on unsupported system configurations.";

const char kImprovedKeyboardShortcutsName[] =
    "Enable improved keyboard shortcuts";
const char kImprovedKeyboardShortcutsDescription[] =
    "Ensure keyboard shortcuts work consistently with international keyboard "
    "layouts and deprecate legacy shortcuts.";

const char kIncrementLocalSurfaceIdForMainframeSameDocNavigationName[] =
    "Increments LocalSurfaceId for main-frame same-doc navigations";
const char kIncrementLocalSurfaceIdForMainframeSameDocNavigationDescription[] =
    "If enabled, every same-document navigations in the main-frame will also "
    "increment the LocalSurfaceId.";

const char kIncognitoReauthenticationForAndroidName[] =
    "Enable device reauthentication for Incognito.";
const char kIncognitoReauthenticationForAndroidDescription[] =
    "When enabled, a setting appears in Settings > Privacy and Security, to "
    "enable reauthentication for accessing your existing Incognito tabs.";

const char kIncognitoScreenshotName[] = "Incognito Screenshot";
const char kIncognitoScreenshotDescription[] =
    "Enables Incognito screenshots on Android. It will also make Incognito "
    "thumbnails visible.";

const char kIndexedDBDefaultDurabilityRelaxed[] =
    "IndexedDB transactions relaxed durability by default";
const char kIndexedDBDefaultDurabilityRelaxedDescription[] =
    "IDBTransaction \"readwrite\" transaction durability defaults to relaxed "
    "when not specified";

const char kInProductHelpDemoModeChoiceName[] = "In-Product Help Demo Mode";
const char kInProductHelpDemoModeChoiceDescription[] =
    "Selects the In-Product Help demo mode.";

const char kInProductHelpSnoozeName[] = "In-Product Help Snooze";
const char kInProductHelpSnoozeDescription[] =
    "Enables the snooze button on In-Product Help.";

#if !BUILDFLAG(IS_ANDROID)
const char kUserEducationExperienceVersion2Name[] =
    "User Education Experience Version 2";
const char kUserEducationExperienceVersion2Description[] =
    "Enables enhancements to the User Education and In-Product Help systems "
    "such as startup grace period, more sophisticated rate limiting, etc.";
#endif

const char kInstallIsolatedWebAppFromUrl[] =
    "Install Isolated Web App from Proxy URL";
const char kInstallIsolatedWebAppFromUrlDescription[] =
    "Installs a new developer mode Isolated Web App whose contents are hosted "
    "at the provided HTTP(S) URL.";

const char kInstantHotspotRebrandName[] = "Instant Hotspot Improvements";

const char kInstantHotspotRebrandDescription[] =
    "Enables Instant Hotspot rebrand/feature improvements.";

const char kInstantHotspotOnNearbyName[] = "Instant Hotspot on Nearby";

const char kInstantHotspotOnNearbyDescription[] =
    "Switches Instant Hotspot to use Nearby Presence for device discovery, as "
    "well as Nearby Connections for device communication.";

const char kIpProtectionProxyOptOutName[] = "Disable IP Protection Proxy";
const char kIpProtectionProxyOptOutDescription[] =
    "When disabled, prevents use of the IP Protection proxy. This is intended "
    "to help with diagnosing any issues that could be caused by the feature "
    "being enabled. For the current status of this feature, see: "
    "https://chromestatus.com/feature/5111460239245312";
const char kIpProtectionProxyOptOutChoiceDefault[] = "Default";
const char kIpProtectionProxyOptOutChoiceOptOut[] = "Disabled";

const char kAutomaticFullscreenContentSettingName[] =
    "Automatic Fullscreen Content Setting";
const char kAutomaticFullscreenContentSettingDescription[] =
    "Enables a new Automatic Fullscreen content setting that lets allowlisted "
    "origins use the HTML Fullscreen API without transient activation.";

const char kJapaneseOSSettingsName[] = "Japanese OS Settings Page";
const char kJapaneseOSSettingsDescription[] =
    "Enable OS Settings Page for Japanese input methods";

const char kJavascriptHarmonyName[] = "Experimental JavaScript";
const char kJavascriptHarmonyDescription[] =
    "Enable web pages to use experimental JavaScript features.";

const char kJavascriptHarmonyShippingName[] =
    "Latest stable JavaScript features";
const char kJavascriptHarmonyShippingDescription[] =
    "Some web pages use legacy or non-standard JavaScript extensions that may "
    "conflict with the latest JavaScript features. This flag allows disabling "
    "support of those features for compatibility with such pages.";

const char kJourneysName[] = "History Journeys";
const char kJourneysDescription[] = "Enables the History Journeys UI.";

const char kJourneysShowAllClustersName[] =
    "History Journeys Show All Clusters";
const char kJourneysShowAllClustersDescription[] =
    "Enables all Journeys clusters to be shown on prominent UI surfaces";

const char kJourneysZeroStateFilteringName[] =
    "History Journeys Zero-State Filtering";
const char kJourneysZeroStateFilteringDescription[] =
    "Enables filtering of clusters in the zero state of the History Journeys "
    "WebUI.";

const char kJumpStartOmniboxName[] = "Jump-start Omnibox";
const char kJumpStartOmniboxDescription[] =
    "Modifies cold- and warm start-up "
    "process on low-end devices to reduce the time to active Omnibox, while "
    "completing core systems initialization in the background.";

const char kExtractRelatedSearchesFromPrefetchedZPSResponseName[] =
    "Extract Related Searches from Prefetched ZPS Response";
const char kExtractRelatedSearchesFromPrefetchedZPSResponseDescription[] =
    "Enables page annotation logic to source related searches data from "
    "prefetched ZPS responses";

const char kLacrosExtensionPrintingName[] =
    "Enable lacros extension printing on ash";
const char kLacrosExtensionPrintingDescription[] =
    "When enabled, lacros's extension printers will be used on ash when lacros "
    "is enabled.";

const char kLanguageDetectionAPIName[] = "Language detection web platform API";
const char kLanguageDetectionAPIDescription[] =
    "When enabled, JS can use the web platform's language detection API";

const char kLegacyTechReportEnableCookieIssueReportsName[] =
    "Enable reporting of Cookie Issues for legacy technology report";
const char kLegacyTechReportEnableCookieIssueReportsDescription[] =
    "When enabled, usage of third-party cookies (on allowlisted pages) is "
    "uploaded for enterprise users as part of a legacy technology report.";

const char kLegacyTechReportTopLevelUrlName[] =
    "Using top level navigation URL for legacy technology report";
const char kLegacyTechReportTopLevelUrlDescription[] =
    "When a legacy technology report is triggered and uploaded for enterprise "
    "users. By default, the URL of the report won't be same as the one in the "
    "Omnibox if the event is detected in a sub-frame. Enable this flag will "
    "allow browser trace back to the top level URL instead and populate the "
    "Frame URL in the `frame_url` field on the API.";

const char kLensOverlayName[] = "Lens overlay";
const char kLensOverlayDescription[] =
    "Enables Lens search via an overlay on any page.";

const char kLensOverlayContextualSearchboxName[] =
    "Lens overlay contextual search box";
const char kLensOverlayContextualSearchboxDescription[] =
    "Enables contextual search box in the Lens overlay.";

const char kLensOverlayTranslateButtonName[] = "Lens overlay translate button";
const char kLensOverlayTranslateButtonDescription[] =
    "Enables translate button via the Lens overlay.";

const char kLensImageTranslateName[] =
    "Translate text in images with Google Lens";
const char kLensImageTranslateDescription[] =
    "Enables a context menu item to translate text in images using Google "
    "Lens. The context menu item appears when the current page is being "
    "translated.";

const char kLinkedServicesSettingName[] = "Linked Services Setting";
const char kLinkedServicesSettingDescription[] =
    "Add Linked Services Setting to the Sync Settings page.";

const char kLogJsConsoleMessagesName[] =
    "Log JS console messages in system logs";
const char kLogJsConsoleMessagesDescription[] =
    "Enable logging JS console messages in system logs, please note that they "
    "may contain PII.";

const char kMediaRouterCastAllowAllIPsName[] =
    "Connect to Cast devices on all IP addresses";
const char kMediaRouterCastAllowAllIPsDescription[] =
    "Have the Media Router connect to Cast devices on all IP addresses, not "
    "just RFC1918/RFC4193 private addresses.";

const char kMojoLinuxChannelSharedMemName[] =
    "Enable Mojo Shared Memory Channel";
const char kMojoLinuxChannelSharedMemDescription[] =
    "If enabled Mojo on Linux based platforms can use shared memory as an "
    "alternate channel for most messages.";

const char kMostVisitedTilesReselectName[] = "Most Visited Tiles Reselect";
const char kMostVisitedTilesReselectDescription[] =
    "When MV tiles is clicked, scans for a tab with a matching URL. "
    "If found, selects the tab and closes the NTP. Else opens into NTP.";

const char kMostVisitedTilesNewScoringName[] =
    "Most Visited Tile: New scoring function";
const char kMostVisitedTilesNewScoringDescription[] =
    "When showing MV tiles, use a new scoring function to compute the score of "
    "each segment.";

const char kCanvas2DLayersName[] =
    "Enables canvas 2D methods BeginLayer and EndLayer";
const char kCanvas2DLayersDescription[] =
    "Enables the canvas 2D methods BeginLayer and EndLayer.";

const char kWebMachineLearningNeuralNetworkName[] = "Enables WebNN API";
const char kWebMachineLearningNeuralNetworkDescription[] =
    "Enables the Web Machine Learning Neural Network (WebNN) API. Spec at "
    "https://www.w3.org/TR/webnn/";

const char kExperimentalWebMachineLearningNeuralNetworkName[] =
    "Enables experimental WebNN API features";
const char kExperimentalWebMachineLearningNeuralNetworkDescription[] =
    "Enables additional, experimental features in Web Machine Learning Neural "
    "Network (WebNN) API. Requires the \"WebNN API\" flag to be enabled.";

const char kSystemProxyForSystemServicesName[] =
    "Enable system-proxy for selected system services";
const char kSystemProxyForSystemServicesDescription[] =
    "Enabling this flag will allow ChromeOS system service which require "
    "network connectivity to use the system-proxy daemon for authentication to "
    "remote HTTP web proxies.";

const char kSystemShortcutBehaviorName[] =
    "Modifies the default behavior of system shortcuts.";
const char kSystemShortcutBehaviorDescription[] =
    "This flag controls the default behavior of ChromeOS system shortcuts "
    "(Launcher key shortcuts).";

const char kNotificationSchedulerName[] = "Notification scheduler";
const char kNotificationSchedulerDescription[] =
    "Enable notification scheduler feature.";

const char kNotificationSchedulerDebugOptionName[] =
    "Notification scheduler debug options";
const char kNotificationSchedulerDebugOptionDescription[] =
    "Enable debugging mode to override certain behavior of notification "
    "scheduler system for easier manual testing.";
const char kNotificationSchedulerImmediateBackgroundTaskDescription[] =
    "Show scheduled notification right away.";

const char kNotificationsSystemFlagName[] = "Enable system notifications.";
const char kNotificationsSystemFlagDescription[] =
    "Enable support for using the system notification toasts and notification "
    "center on platforms where these are available.";

const char kOmitCorsClientCertName[] =
    "Omit TLS client certificates if credential mode disallows";
const char kOmitCorsClientCertDescription[] =
    "Strictly conform the Fetch spec to omit TLS client certificates if "
    "credential mode disallows. Without this flag enabled, Chrome will always "
    "try sending client certificates regardless of the credential mode.";

const char kOmniboxActionsInSuggestName[] = "Action in Suggest";
const char kOmniboxActionsInSuggestDescription[] =
    "Actions in Suggest permits optional Action Chips to be attached to "
    "Entity suggestions.";

const char kOmniboxAdaptiveSuggestionsCountName[] =
    "Adaptive Omnibox Suggestions count";
const char kOmniboxAdaptiveSuggestionsCountDescription[] =
    "Dynamically adjust number of presented Omnibox suggestions depending on "
    "available space. When enabled, this feature will increase (or decrease) "
    "amount of offered Omnibox suggestions to fill in the space between the "
    "Omnibox and soft keyboard (if any). See also Max Autocomplete Matches "
    "flag to adjust the limit of offered suggestions. The number of shown "
    "suggestions will be no less than the platform default limit.";

const char kOmniboxAnswerActionsName[] = "Answer Actions";
const char kOmniboxAnswerActionsDescription[] =
    "Answer Actions attaches related Action Chips to Answer suggestions.";

const char kOmniboxAsyncViewInflationName[] = "Async Omnibox view inflation";
const char kOmniboxAsyncViewInflationDescription[] =
    "Inflate Omnibox and Suggestions views off the UI thread.";

const char kOmniboxCalcProviderName[] = "Omnibox calc provider";
const char kOmniboxCalcProviderDescription[] =
    "When enabled, suggests recent calculator results in the omnibox.";

const char kOmniboxDomainSuggestionsName[] = "Omnibox Domain Suggestions";
const char kOmniboxDomainSuggestionsDescription[] =
    "If enabled, history URL suggestions from hosts visited often bypass the "
    "per provider limit.";

const char kOmniboxForceAllowedToBeDefaultName[] =
    "Omnibox Force Allowed To Be Default";
const char kOmniboxForceAllowedToBeDefaultDescription[] =
    "If enabled, all omnibox suggestions pretend to be inlineable. This likely "
    "has a bunch of problems.";

const char kOmniboxGroupingFrameworkZPSName[] =
    "Omnibox Grouping Framework for ZPS";
const char kOmniboxGroupingFrameworkNonZPSName[] =
    "Omnibox Grouping Framework for Typed Suggestions";
const char kOmniboxGroupingFrameworkDescription[] =
    "Enables an alternative grouping implementation for omnibox "
    "autocompletion.";

const char kOmniboxMostVisitedTilesHorizontalRenderGroupName[] =
    "Omnibox MV Tiles Horizontal Render Group";
const char kOmniboxMostVisitedTilesHorizontalRenderGroupDescription[] =
    "Updates the logic constructing MV tiles to use horizontal render group. "
    "No user-facing changes expected.";

const char kOmniboxZeroSuggestPrefetchDebouncingName[] =
    "Omnibox Zero Prefix Suggest Prefetch Request Debouncing";
const char kOmniboxZeroSuggestPrefetchDebouncingDescription[] =
    "Enables the use of a request debouncer to throttle the volume of ZPS "
    "prefetch requests issued to the remote Suggest service.";

const char kOmniboxZeroSuggestPrefetchingName[] =
    "Omnibox Zero Prefix Suggestion Prefetching on NTP";
const char kOmniboxZeroSuggestPrefetchingDescription[] =
    "Enables prefetching of the zero prefix suggestions for eligible users "
    "on the New Tab page.";

const char kOmniboxZeroSuggestPrefetchingOnSRPName[] =
    "Omnibox Zero Prefix Suggestion Prefetching on SRP";
const char kOmniboxZeroSuggestPrefetchingOnSRPDescription[] =
    "Enables prefetching of the zero prefix suggestions for eligible users "
    "on the Search Results page.";

const char kOmniboxZeroSuggestPrefetchingOnWebName[] =
    "Omnibox Zero Prefix Suggestion Prefetching on the Web";
const char kOmniboxZeroSuggestPrefetchingOnWebDescription[] =
    "Enables prefetching of the zero prefix suggestions for eligible users "
    "on the Web (i.e. non-NTP and non-SRP URLs).";

const char kOmniboxZeroSuggestInMemoryCachingName[] =
    "Omnibox Zero Prefix Suggestion in-memory caching";
const char kOmniboxZeroSuggestInMemoryCachingDescription[] =
    "Enables in-memory caching of zero prefix suggestions.";

const char kOmniboxOnDeviceHeadSuggestionsName[] =
    "Omnibox on device head suggestions (non-incognito only)";
const char kOmniboxOnDeviceHeadSuggestionsDescription[] =
    "Google head non personalized search suggestions provided by a compact on "
    "device model for non-incognito. Turn off this feature if you have other "
    "apps running which affects local file access (e.g. anti-virus software) "
    "and are experiencing searchbox typing lags.";
const char kOmniboxOnDeviceHeadSuggestionsIncognitoName[] =
    "Omnibox on device head suggestions (incognito only)";
const char kOmniboxOnDeviceHeadSuggestionsIncognitoDescription[] =
    "Google head non personalized search suggestions provided by a compact on "
    "device model for incognito. Turn off this feature if you have other "
    "apps running which affects local file access (e.g. anti-virus software) "
    "and are experiencing searchbox typing lags.";
const char kOmniboxOnDeviceTailSuggestionsName[] =
    "Omnibox on device tail suggestions";
const char kOmniboxOnDeviceTailSuggestionsDescription[] =
    "Google tail non personalized search suggestions provided by a compact on "
    "device model.";

const char kOmniboxPrefBasedDataCollectionConsentHelperName[] =
    "Pref Based Data Collection Consent Helper";
const char kOmniboxPrefBasedDataCollectionConsentHelperDescription[] =
    "Enables the pref based data collection consent helper";

const char kOmniboxRichAutocompletionPromisingName[] =
    "Omnibox Rich Autocompletion Promising Combinations";
const char kOmniboxRichAutocompletionPromisingDescription[] =
    "Allow autocompletion for titles and non-prefixes. Suggestions whose "
    "titles or URLs contain the user input as a continuous chunk, but not "
    "necessarily a prefix, can be the default suggestion. Otherwise, only "
    "suggestions whose URLs are prefixed by the user input can be.";

const char kOmniboxLocalHistoryZeroSuggestBeyondNTPName[] =
    "Allow local history zero-prefix suggestions beyond NTP";
const char kOmniboxLocalHistoryZeroSuggestBeyondNTPDescription[] =
    "Enables local history zero-prefix suggestions in every context in which "
    "the remote zero-prefix suggestions are enabled.";

const char kOmniboxMlLogUrlScoringSignalsName[] =
    "Log Omnibox URL Scoring Signals";
const char kOmniboxMlLogUrlScoringSignalsDescription[] =
    "Enables Omnibox to log scoring signals of URL suggestions.";

const char kOmniboxMlUrlPiecewiseMappedSearchBlendingName[] =
    "Omnibox ML Scoring with Piecewise Score Mapping";
const char kOmniboxMlUrlPiecewiseMappedSearchBlendingDescription[] =
    "Specifies how to blend URL ML scores and search traditional scores using "
    "a piecewise ML score mapping function.";

const char kOmniboxMlUrlScoreCachingName[] = "Omnibox ML URL Score Caching";
const char kOmniboxMlUrlScoreCachingDescription[] =
    "Enables in-memory caching of ML URL scores.";

const char kOmniboxMlUrlScoringName[] = "Omnibox ML URL Scoring";
const char kOmniboxMlUrlScoringDescription[] =
    "Enables ML-based relevance scoring for Omnibox URL Suggestions.";

const char kOmniboxMlUrlScoringModelName[] = "Omnibox URL Scoring Model";
const char kOmniboxMlUrlScoringModelDescription[] =
    "Enables ML scoring model for Omnibox URL sugestions.";

const char kOmniboxMlUrlSearchBlendingName[] = "Omnibox ML URL Search Blending";
const char kOmniboxMlUrlSearchBlendingDescription[] =
    "Specifies how to blend URL ML scores and search traditional scores.";

const char kOmniboxSuggestionAnswerMigrationName[] =
    "Omnibox SuggestionAnswer Migration";
const char kOmniboxSuggestionAnswerMigrationDescription[] =
    "Uses protos instead of SuggestionAnswer to hold answer data.";

const char kOmniboxShortcutBoostName[] = "Omnibox shortcut boosting";
const char kOmniboxShortcutBoostDescription[] =
    "Promote shortcuts to be default when available.";

const char kOmniboxMaxZeroSuggestMatchesName[] =
    "Omnibox Max Zero Suggest Matches";
const char kOmniboxMaxZeroSuggestMatchesDescription[] =
    "Changes the maximum number of autocomplete matches displayed when zero "
    "suggest is active (i.e. displaying suggestions without input).";

const char kOmniboxUIMaxAutocompleteMatchesName[] =
    "Omnibox UI Max Autocomplete Matches";
const char kOmniboxUIMaxAutocompleteMatchesDescription[] =
    "Changes the maximum number of autocomplete matches displayed in the "
    "Omnibox UI.";

const char kOmniboxStarterPackExpansionName[] =
    "Expansion pack for the Site search starter pack";
const char kOmniboxStarterPackExpansionDescription[] =
    "Enables additional providers for the Site search starter pack feature";

const char kOmniboxStarterPackIPHName[] =
    "IPH message for the Site search starter pack";
const char kOmniboxStarterPackIPHDescription[] =
    "Enables an informational IPH message for the  Site search starter pack "
    "feature";

#if BUILDFLAG(IS_ANDROID)
const char kOmnibox2023RefreshConnectionSecurityIndicatorsName[] =
    "Omnibox 2023 refresh connection security indicators";
const char kOmnibox2023RefreshConnectionSecurityIndicatorsDescription[] =
    "Use new connection security indicators for https pages in the omnibox. "
    "When enabled, the icon shown in the onmibox for secure pages is the same "
    "as shown on desktop when the Chrome 2023 refresh flag is active.";
#endif  // BUILDFLAG(IS_ANDROID)

const char kWebUIOmniboxPopupName[] = "WebUI Omnibox Popup";
const char kWebUIOmniboxPopupDescription[] =
    "If enabled, shows the omnibox suggestions popup in WebUI.";

const char kOmniboxMaxURLMatchesName[] = "Omnibox Max URL Matches";
const char kOmniboxMaxURLMatchesDescription[] =
    "The maximum number of URL matches to show, unless there are no "
    "replacements.";

const char kOmniboxDynamicMaxAutocompleteName[] =
    "Omnibox Dynamic Max Autocomplete";
const char kOmniboxDynamicMaxAutocompleteDescription[] =
    "Configures the maximum number of autocomplete matches displayed in the "
    "Omnibox UI dynamically based on the number of URL matches.";

const char kOneTimePermissionName[] = "One time permission";
const char kOneTimePermissionDescription[] =
    "Enables experimental one time permissions for Geolocation, Microphone and "
    "Camera.";

const char kOptimizationGuideDebugLogsName[] =
    "Enable optimization guide debug logs";
const char kOptimizationGuideDebugLogsDescription[] =
    "Enables the optimization guide to log and save debug messages that can be "
    "shown in the internals page.";

const char kOptimizationGuideModelExecutionName[] =
    "Enables optimization guide model execution";
const char kOptimizationGuideModelExecutionDescription[] =
    "Enables the optimization guide to execute models.";

const char kOptimizationGuideEnableDogfoodLoggingName[] =
    "Enable optimization guide dogfood logging";
const char kOptimizationGuideEnableDogfoodLoggingDescription[] =
    "If this client is a Google-internal dogfood client, overrides enterprise "
    "policy to enable model quality logs. Googlers: See "
    "go/chrome-mqls-debug-logging for details.";

const char kOptimizationGuideOnDeviceModelName[] =
    "Enables optimization guide on device";
const char kOptimizationGuideOnDeviceModelDescription[] =
    "Enables the optimization guide to execute models on device.";

const char kOptimizationGuidePersonalizedFetchingName[] =
    "Enable optimization guide personalized fetching";
const char kOptimizationGuidePersonalizedFetchingDescription[] =
    "Enables the optimization guide to fetch personalized results, by "
    "attaching Gaia.";

const char kOptimizationGuidePushNotificationName[] =
    "Enable optimization guide push notifications";
const char kOptimizationGuidePushNotificationDescription[] =
    "Enables the optimization guide to receive push notifications.";

const char kOrganicRepeatableQueriesName[] =
    "Organic repeatable queries in Most Visited tiles";
const char kOrganicRepeatableQueriesDescription[] =
    "Enables showing the most repeated queries, from the device browsing "
    "history, organically among the most visited sites in the MV tiles.";

const char kOriginAgentClusterDefaultName[] =
    "Origin-keyed Agent Clusters by default";
const char kOriginAgentClusterDefaultDescription[] =
    "Select the default behaviour for the Origin-Agent-Cluster http header. "
    "If enabled, an absent header will cause pages to be assigned to an "
    "origin-keyed agent cluster, and to a site-keyed agent cluster when "
    "disabled. Documents whose agent clusters are origin-keyed cannot set "
    "document.domain to relax the same-origin policy.";

const char kOriginKeyedProcessesByDefaultName[] =
    "Origin-keyed Processes by default";
const char kOriginKeyedProcessesByDefaultDescription[] =
    "Enables origin-keyed process isolation for most pages (i.e., those "
    "assigned to an origin-keyed agent cluster by default). This improves "
    "security but also increases the number of processes created. Note: "
    "enabling this feature also enables 'Origin-keyed Agent Clusters by "
    "default'.";

const char kOsSettingsAppNotificationsPageName[] =
    "CrOS Settings App Notifications Page";
const char kOsSettingsAppNotificationsPageDescription[] =
    "If enabled, a new App Notifications subpage will appear in the "
    "CrOS Settings Apps section.";

const char kOverlayScrollbarsName[] = "Overlay Scrollbars";
const char kOverlayScrollbarsDescription[] =
    "Enable the experimental overlay scrollbars implementation. You must also "
    "enable threaded compositing to have the scrollbars animate.";

const char kOverlayStrategiesName[] = "Select HW overlay strategies";
const char kOverlayStrategiesDescription[] =
    "Select strategies used to promote quads to HW overlays. Note that "
    "strategies other than Default may break playback of protected content.";
const char kOverlayStrategiesDefault[] = "Default";
const char kOverlayStrategiesNone[] = "None";
const char kOverlayStrategiesUnoccludedFullscreen[] =
    "Unoccluded fullscreen buffers (single-fullscreen)";
const char kOverlayStrategiesUnoccluded[] =
    "Unoccluded buffers (single-fullscreen,single-on-top)";
const char kOverlayStrategiesOccludedAndUnoccluded[] =
    "Occluded and unoccluded buffers "
    "(single-fullscreen,single-on-top,underlay)";

const char kOverscrollHistoryNavigationName[] = "Overscroll history navigation";
const char kOverscrollHistoryNavigationDescription[] =
    "History navigation in response to horizontal overscroll.";

const char kOverviewDeskNavigationName[] =
    "CrOS Labs: Overview Desk Navigation";
const char kOverviewDeskNavigationDescription[] =
    "Stay in overview when navigating between desks using a swipe gesture or "
    "keyboard shortcut.";

const char kPageContentAnnotationsName[] = "Page content annotations";
const char kPageContentAnnotationsDescription[] =
    "Enables page content to be annotated on-device.";

const char kPageContentAnnotationsPersistSalientImageMetadataName[] =
    "Page content annotations - Persist salient image metadata";
const char kPageContentAnnotationsPersistSalientImageMetadataDescription[] =
    "Enables salient image metadata per page load to be persisted on-device.";

const char kPageContentAnnotationsRemotePageMetadataName[] =
    "Page content annotations - Remote page metadata";
const char kPageContentAnnotationsRemotePageMetadataDescription[] =
    "Enables fetching of page load metadata to be persisted on-device.";

const char kPageImageServiceOptimizationGuideSalientImagesName[] =
    "Page Image Service - Optimization Guide Salient Images";
const char kPageImageServiceOptimizationGuideSalientImagesDescription[] =
    "Enables the PageImageService fetching images from the Optimization Guide "
    "Salient Images source.";

const char kPageImageServiceSuggestPoweredImagesName[] =
    "Page Image Service - Suggest Powered Images";
const char kPageImageServiceSuggestPoweredImagesDescription[] =
    "Enables the PageImageService fetching images from the Suggest source.";

const char kPageInfoAboutThisPagePersistentEntryName[] =
    "AboutThisPage persistent SidePanel entry";
const char kPageInfoAboutThisPagePersistentEntryDescription[] =
    "Registers a SidePanel entry on pageload if 'AboutThisPage' info is "
    "available";

const char kPageInfoCookiesSubpageName[] = "Cookies subpage in page info";
const char kPageInfoCookiesSubpageDescription[] =
    "Enable the Cookies subpage in page info for managing cookies and site "
    "data.";

const char kPageInfoHideSiteSettingsName[] = "Page info hide site settings";
const char kPageInfoHideSiteSettingsDescription[] =
    "Hides site settings row in the page info menu.";

const char kPageInfoHistoryDesktopName[] = "Page info history";
const char kPageInfoHistoryDesktopDescription[] =
    "Enable a history section in the page info.";

const char kPageVisibilityPageContentAnnotationsName[] =
    "Page visibility content annotations";
const char kPageVisibilityPageContentAnnotationsDescription[] =
    "Enables annotating the page visibility model for each page load "
    "on-device.";

const char kParallelDownloadingName[] = "Parallel downloading";
const char kParallelDownloadingDescription[] =
    "Enable parallel downloading to accelerate download speed.";

const char kPasswordGenerationSoftNudgeName[] =
    "Password generation soft nudge";
const char kPasswordGenerationSoftNudgeDescription[] =
    "Enables updated password generation UI with a prominent button and "
    "previewing the generated password on focus.";

const char kPasswordLeakToggleMoveName[] = "Password leak toggle move";
const char kPasswordLeakToggleMoveDescription[] =
    "Enables the new location of the password leak toggle option under "
    "standard protection on chrome://settings/security to the advanced "
    "section of the chrome://settings/security page.";

const char kPasswordManagerShowSuggestionsOnAutofocusName[] =
    "Showing password suggestions on autofocused password forms";
const char kPasswordManagerShowSuggestionsOnAutofocusDescription[] =
    "Enables showing password suggestions without requiring the user to "
    "click on the already focused field if the field was autofocused on "
    "the page load.";

const char kPasswordManualFallbackAvailableName[] = "Password manual fallback";
const char kPasswordManualFallbackAvailableDescription[] =
    "Enables triggering password suggestions through the context menu";

const char kPasswordParsingOnSaveUsesPredictionsName[] =
    "Use server predictions for password form parsing on saving";
const char kPasswordParsingOnSaveUsesPredictionsDescription[] =
    "Take server prediction into account when parsing password forms "
    "during saving.";

const char kPdfOcrName[] = "Performs OCR on inaccessible PDFs";
const char kPdfOcrDescription[] =
    "Enables a feature whereby inaccessible (i.e. untagged) PDFs are made "
    "accessible using an optical character recognition service.";

const char kPdfSearchifyName[] = "Make the text in PDF images interactable";
const char kPdfSearchifyDescription[] =
    "Enables a feature which runs OCR on PDF images and makes the recognized "
    "text searchable and editable.";

const char kPdfXfaFormsName[] = "PDF XFA support";
const char kPdfXfaFormsDescription[] =
    "Enables support for XFA forms in PDFs. "
    "Has no effect if Chrome was not built with XFA support.";

const char kAutoWebContentsDarkModeName[] = "Auto Dark Mode for Web Contents";
const char kAutoWebContentsDarkModeDescription[] =
    "Automatically render all web contents using a dark theme.";

const char kForcedColorsName[] = "Forced Colors";
const char kForcedColorsDescription[] =
    "Enables forced colors mode for web content.";

const char kWindowsScrollingPersonalityName[] = "Windows Scrolling Personality";
const char kWindowsScrollingPersonalityDescription[] =
    "If enabled, mousewheel and keyboard scrolls will scroll by a percentage "
    "of the scroller size and the default scroll animation is replaced with "
    "Impulse-style scroll animations.";

const char kImprovedSemanticsActivityIndicatorsName[] =
    "Improved semantics activity indicators";
const char kImprovedSemanticsActivityIndicatorsDescription[] =
    "Enables experimental improved semantics indicators in the location bar.";

const char kLeftHandSideActivityIndicatorsName[] =
    "Left-hand side activity indicators";
const char kLeftHandSideActivityIndicatorsDescription[] =
    "Moves activity indicators to the left-hand side of location bar.";

#if BUILDFLAG(IS_CHROMEOS)
const char kCrosSystemLevelPermissionBlockedWarningsName[] =
    "Chrome OS block warnings";
const char kCrosSystemLevelPermissionBlockedWarningsDescription[] =
    "Displays warnings in browser if camera, microphone or geolocation is "
    "disabled in the OS.";
#endif

const char kPerformanceInterventionUiName[] =
    "Enable performance intervention suggestions.";
const char kPerformanceInterventionUiDescription[] =
    "When enabled, performance intervention UI will automatically pop up and "
    "suggest tabs to take action on during times of poor performance.";

const char kPerformanceInterventionDemoModeName[] =
    "Enable performance intervention demo mode.";
const char kPerformanceInterventionDemoModeDescription[] =
    "When enabled, CPU thresholds and rate limiting "
    "will be ignored so performance intervention UI may be easily triggered "
    "through natural browser usage or force triggered in chrome://discards. "
    "All suggested actionable tabs will meet the normal discard criteria "
    "except for needing to stay in the background for a set minimum time. "
    "Requires chrome://flags/#performance-intervention-ui to be enabled.";

const char kPermissionPredictionsName[] = "Permission Predictions";
const char kPermissionPredictionsDescription[] =
    "Use the Permission Predictions Service to surface permission requests "
    "using a quieter UI when the likelihood of the user granting the "
    "permission is predicted to be low. Requires "
    "chrome://flags/#quiet-notification-prompts and `Safe Browsing` to be "
    "enabled.";

const char kPermissionPredictionsV3Name[] = "Permission Predictions V3";
const char kPermissionPredictionsV3Description[] =
    "Use the Permission Predictions Service V3 to surface permission requests "
    "using a quieter UI when the likelihood of the user granting the "
    "permission is predicted to be low. Requires "
    "and `Safe Browsing` to be enabled.";

const char kPermissionQuietChipName[] = "Quiet Permission Chip Experiment";
const char kPermissionQuietChipDescription[] =
    "Enables a permission prompt that uses the quiet chip instead of the "
    "right-hand side address bar icon for quiet permission prompts. Requires "
    "chrome://flags/#quiet-notification-prompts to be enabled.";

const char kShowRelatedWebsiteSetsPermissionGrantsName[] =
    "Show permission grants from Related Website Sets";
const char kShowRelatedWebsiteSetsPermissionGrantsDescription[] =
    "Shows permission grants created by Related Website Sets in Chrome "
    "Settings UI and Page Info Bubble, "
    "default is hidden";

const char kPowerBookmarkBackendName[] = "Power bookmark backend";
const char kPowerBookmarkBackendDescription[] =
    "Enables storing additional metadata to support power bookmark features.";

const char kSpeculationRulesPrerenderingTargetHintName[] =
    "Speculation Rules API target hint";
const char kSpeculationRulesPrerenderingTargetHintDescription[] =
    "Enable target_hint param on Speculation Rules API for prerendering.";

const char kSubframeProcessReuseThresholds[] =
    "Subframe process reuse thresholds";
const char kSubframeProcessReuseThresholdsDescription[] =
    "Enable thresholds for subframe process reuse. When "
    "out-of-process iframes attempt to reuse compatible processes from "
    "unrelated tabs, process reuse will only be allowed if the process stays "
    "below predefined thresholds (e.g., below a certain memory limit).";

const char kSupportSearchSuggestionForPrerender2Name[] =
    "Prerender search suggestions";
const char kSupportSearchSuggestionForPrerender2Description[] =
    "Allows Prerender2 to prerender search suggestions provided by the default "
    "search engine.";

const char kPrerender2EarlyDocumentLifecycleUpdateName[] =
    "Prerender more document lifecycle phases";
const char kPrerender2EarlyDocumentLifecycleUpdateDescription[] =
    "Allows prerendering pages to execute more lifecycle updates, such as "
    "prepaint, before activation";

const char kWarmUpCompositorName[] = "Warm up compositor";
const char kWarmUpCompositorDescription[] =
    "Allows compositor to start warming up on certain signals";

const char kPrerender2WarmUpCompositorName[] =
    "Warm up compositor on prerendering";
const char kPrerender2WarmUpCompositorDescription[] =
    "Enables compositor warming up on paticular loading events of prerender "
    "initial navigation. Requires chrome://flags/#compositor-warm-up to be "
    "enabled";

const char kPrerender2ForNewTabPageAndroidName[] =
    "Enable prerendering on New Tab Page Android";
const char kPrerender2ForNewTabPageAndroidDescription[] =
    "Enables prerendering for navigations initiated by New Tab Page on Android";

const char kEnableOmniboxSearchPrefetchName[] = "Omnibox prefetch Search";
const char kEnableOmniboxSearchPrefetchDescription[] =
    "Allows omnibox to prefetch likely search suggestions provided by the "
    "Default Search Engine";

const char kEnableOmniboxClientSearchPrefetchName[] =
    "Omnibox client prefetch Search";
const char kEnableOmniboxClientSearchPrefetchDescription[] =
    "Allows omnibox to prefetch search suggestions provided by the Default "
    "Search Engine that the client thinks are likely to be navigated. Requires "
    "chrome://flags/#omnibox-search-prefetch";

const char kCbdTimeframeRequiredName[] =
    "Clear Browsing Data Timeframe Experiment on Desktop";
const char kCbdTimeframeRequiredDescription[] =
    "This experiment requires users to interact with timeframe drop down menu "
    "in the clear browsing data dialog. It also adds a new 'Last 15 minutes' "
    "value to the list.";

const char kPriceChangeModuleName[] = "Price Change Module";
const char kPriceChangeModuleDescription[] =
    "Show a module with price drops of open tabs on new tab page.";

const char kPrivacyGuideAndroid3Name[] = "Privacy Guide 3 on Android";
const char kPrivacyGuideAndroid3Description[] =
    "Enables updates to the Privacy Guide flow on Android.";

const char kPrivacyGuidePreloadAndroidName[] =
    "Privacy Guide Preload on Android";
const char kPrivacyGuidePreloadAndroidDescription[] =
    "Adds the preload card to the Privacy Guide 3 flow on Android. "
    "This requires #privacy-guide-android-3 to also be enabled.";

const char kPrivacySandboxAdsAPIsOverrideName[] = "Privacy Sandbox Ads APIs";
const char kPrivacySandboxAdsAPIsOverrideDescription[] =
    "Enables Privacy Sandbox APIs: Attribution Reporting, Fledge, Topics, "
    "Fenced Frames, Shared Storage, Private Aggregation, and their associated "
    "features.";

const char kPrivacySandboxEnrollmentOverridesName[] =
    "Privacy Sandbox Enrollment Overrides";
const char kPrivacySandboxEnrollmentOverridesDescription[] =
    "Allows a list of sites to use Privacy Sandbox features without them being "
    "enrolled and attested into the Privacy Sandbox experiment. See: "
    "https://developer.chrome.com/en/docs/privacy-sandbox/enroll/";

const char kPrivacySandboxInternalsName[] = "Privacy Sandbox Internals Page";
const char kPrivacySandboxInternalsDescription[] =
    "Enables the chrome://privacy-sandbox-internals debugging page.";

const char kPrivateStateTokensDevUIName[] = "Private State Tokens Dev UI";
const char kPrivateStateTokensDevUIDescription[] =
    "Enables the chrome://privacy-sandbox-internals/private-state-tokens page."
    "Relies on privacy-sandbox-internals also being enabled";

const char kPrivacySandboxPrivacyGuideAdTopicsName[] =
    "Privacy Sandbox Privacy Guide Ad Topics";
const char kPrivacySandboxPrivacyGuideAdTopicsDescription[] =
    "Enables the Ad Topics card in the Privacy Guide to be displayed, subject "
    "to regional availability.";

const char kPrivacySandboxPrivacyPolicyName[] =
    "Privacy Sandbox Privacy Policy";
const char kPrivacySandboxPrivacyPolicyDescription[] =
    "Enables the Privacy Policy link to be displayed on the Privacy Sandbox "
    "Consent dialog, subject to regional availability.";

const char kProtectedAudiencesConsentedDebugTokenName[] =
    "Protected Audiences Consented Debug Token";
const char kProtectedAudiencesConsentedDebugTokenDescription[] =
    "Enables Protected Audience Consented Debugging with the provided token. "
    "Protected Audience auctions running on a Bidding and Auction API trusted "
    "server with a matching token will be able to log information about the "
    "auction to enable debugging. Note that this logging may include "
    "information about the user's browsing history normally kept private.";

const char kPullToRefreshName[] = "Pull-to-refresh gesture";
const char kPullToRefreshDescription[] =
    "Pull-to-refresh gesture in response to vertical overscroll.";
const char kPullToRefreshEnabledTouchscreen[] = "Enabled for touchscreen only";

const char kPwaUpdateDialogForAppIconName[] =
    "Enable PWA install update dialog for icon changes";
const char kPwaUpdateDialogForAppIconDescription[] =
    "Enable a confirmation dialog that shows up when a PWA changes its icon";

const char kRenderDocumentName[] = "Enable RenderDocument";
const char kRenderDocumentDescription[] =
    "Enable swapping RenderFrameHosts on same-site navigations";

const char kRelatedWebsiteSetsDevUIName[] = "Related Website Sets Dev UI";
const char kRelatedWebsiteSetsDevUIDescription[] =
    "Enables the chrome://privacy-sandbox-internals/related-website-sets page. "
    "Relies on privacy-sandbox-internals also being enabled.";

const char kQuicName[] = "Experimental QUIC protocol";
const char kQuicDescription[] = "Enable experimental QUIC protocol support.";

const char kQuickAppAccessTestUIName[] = "Internal test: quick app access";
const char kQuickAppAccessTestUIDescription[] =
    "Show an app in the quick app access area at the start of the session";

const char kQuickDeleteForAndroidName[] = "Enable quick delete";
const char kQuickDeleteForAndroidDescription[] =
    "When enabled, a new entrypoint to Clear Browsing Data appears in the "
    "three dots menu, allowing users to achieve their browsing data and tabs "
    "deletion needs in a quicker and more reassuring manner.";

const char kQuickDeleteAndroidFollowupName[] =
    "Followup for Quick Delete on Android";
const char kQuickDeleteAndroidFollowupDescription[] =
    "Enables some follow up work on Clear Browsing Data for Quick Delete, "
    "including an improved post delete experience and animations.";

const char kQuickDeleteAndroidSurveyName[] = "HaTS for Quick Delete on Android";
const char kQuickDeleteAndroidSurveyDescription[] =
    "Enables HaTS survey for Quick Delete on Android.";

const char kQuickShareV2Name[] = "Quick Share v2";
const char kQuickShareV2Description[] =
    "Enables Quick Share v2, which defaults Quick Share to 'Your Devices' "
    "visibility, removes the 'Selected Contacts' visibility, removes the Quick "
    "Share On/Off toggle.";

const char kSendTabToSelfIOSPushNotificationsName[] =
    "Send tab to self iOS push notifications";
const char kSendTabToSelfIOSPushNotificationsDescription[] =
    "Feature to allow users to send tabs to their iOS device through a system "
    "push notitification.";

#if BUILDFLAG(IS_ANDROID)
const char kSensitiveContentName[] =
    "Redact sensitive content during screen sharing, screen recording, "
    "and similar actions";

const char kSensitiveContentDescription[] =
    "When enabled, if sensitive form fields (such as credit cards, passwords) "
    "are present on the page, the entire content area is redacted during "
    "screen sharing, screen recording, and similar actions. This feature "
    "works only on Android V or above.";

const char kSensitiveContentWhileSwitchingTabsName[] =
    "Redact sensitive content while switching tabs during screen sharing, "
    "screen recording, and similar actions";

const char kSensitiveContentWhileSwitchingTabsDescription[] =
    "When enabled, if a tab switching surface provides a preview of a tab that "
    "contains sensitive content, the screen is redacted during screen sharing, "
    "screen recording, and similar actions. This feature works only on Android "
    "V or above, and if #sensitive-content is also enabled.";
#endif  // BUILDFLAG(IS_ANDROID)

const char kSettingsAppNotificationSettingsName[] =
    "Split notification permission settings";
const char kSettingsAppNotificationSettingsDescription[] =
    "Remove per-app notification permissions settings from the quick settings "
    "menu. Notification permission settings will be split between the "
    "lacros-chrome browser's notification permission page "
    "and the ChromeOS settings app.";

const char kRecordWebAppDebugInfoName[] = "Record web app debug info";
const char kRecordWebAppDebugInfoDescription[] =
    "Enables recording additional web app related debugging data to be "
    "displayed in: chrome://web-app-internals";

#if BUILDFLAG(IS_ANDROID)
const char kRecordPermissionExpirationTimestampsName[] =
    "Record expiration timestamps for permissions";
const char kRecordPermissionExpirationTimestampsDescription[] =
    "When enabled, it will record the expiration timestamp for all permissions "
    "granted through prompts.";
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC)
const char kReduceIPAddressChangeNotificationName[] =
    "Reduce IP address change notification";
const char kReduceIPAddressChangeNotificationDescription[] =
    "Reduce the frequency of IP address change notifications that result in "
    "TCP and QUIC connection resets.";
#endif  // BUILDFLAG(IS_MAC)

const char kReduceAcceptLanguageName[] =
    "Reduce Accept-Language request header";
const char kReduceAcceptLanguageDescription[] =
    "Reduce the amount of information available in the Accept-Language request "
    "header. See https://github.com/Tanych/accept-language for more info.";

const char kReduceTransferSizeUpdatedIPCName[] =
    "Reduce TransferSizeUpdated IPC";
const char kReduceTransferSizeUpdatedIPCDescription[] =
    "When enabled, the network service will send TransferSizeUpdatedIPC IPC "
    "only when DevTools is attached or the request is for an ad request.";

#if BUILDFLAG(IS_ANDROID)
const char kReplaceSyncPromosWithSignInPromosName[] =
    "Replace all sync-related UI with sign-in ones";
const char kReplaceSyncPromosWithSignInPromosDescription[] =
    "When enabled, all sync-related promos will be replaced by sign-in ones.";
#endif  // BUILDFLAG(IS_ANDROID)

const char kResetShortcutCustomizationsName[] =
    "Reset all shortcut customizations";
const char kResetShortcutCustomizationsDescription[] =
    "Resests all shortcut customizations on startup.";

const char kRestrictGamepadAccessName[] = "Restrict gamepad access";
const char kRestrictGamepadAccessDescription[] =
    "Enables Permissions Policy and Secure Context restrictions on the Gamepad "
    "API";

#if BUILDFLAG(IS_ANDROID)
const char kRetainOmniboxOnFocusName[] = "Retain omnibox on focus";
const char kRetainOmniboxOnFocusDescription[] =
    "Whether the contents of the omnibox should be retained on focus as "
    "opposed to being cleared. When this feature flag is enabled and the "
    "omnibox contents are retained, focus events will also result in the "
    "omnibox contents being fully selected so as to allow for easy replacement "
    "by the user. Note that even with this feature flag enabled, only large "
    "screen devices with an attached keyboard and precision pointer will "
    "exhibit a change in behavior.";
#endif  // BUILDFLAG(IS_ANDROID)

const char kRoundedDisplay[] = "Rounded display";
const char kRoundedDisplayDescription[] =
    "Enables rounded corners for the display";

const char kRoundedWindows[] = "Use rounded windows";
const char kRoundedWindowsDescription[] =
    "Specifies the radius of rounded windows in DIPs (Device Independent "
    "Pixels)";

const char kRubyShortHeuristicsName[] = "Short ruby heuristics";
const char kRubyShortHeuristicsDescription[] =
    "When enabled, line breaking doesn't happen inside <ruby>s with shorter "
    "contents even if `text-wrap: nowrap` is not specified.";

const char kMBIModeName[] = "MBI Scheduling Mode";
const char kMBIModeDescription[] =
    "Enables independent agent cluster scheduling, via the "
    "AgentSchedulingGroup infrastructure.";

const char kSafetyCheckUnusedSitePermissionsName[] =
    "Permission Module for unused sites in Safety Check";
const char kSafetyCheckUnusedSitePermissionsDescription[] =
    "When enabled, adds the unused sites permission module to Safety Check on "
    "desktop. The module will be shown depending on the browser state.";

const char kSafetyHubName[] = "Safety Check v2";
const char kSafetyHubDescription[] =
    "When enabled, Safety Check v2 will be visible in settings.";

#if BUILDFLAG(IS_ANDROID)
const char kSafetyHubMagicStackName[] = "Safety Check v2 - Magic Stack";
const char kSafetyHubMagicStackDescription[] =
    "When enabled, a magic stack card will be visible for Safety Check v2 if "
    "trigger conditions are met.";

const char kSafetyHubFollowupName[] = "Followup for Safety Check v2";
const char kSafetyHubFollowupDescription[] =
    "Enables some follow up work for Safety Check v2 if, this includes some "
    "enhancements to the passwords module on the Safety Check page and "
    "enabling the password card on magic stack.";

const char kSafetyHubAndroidSurveyName[] =
    "HaTS for Safety Check v2 on Android";
const char kSafetyHubAndroidSurveyDescription[] =
    "Enables control & proactive HaTS surveys for Safety Check v2 on Android.";
#else
const char kSafetyHubHaTSOneOffSurveyName[] =
    "HaTS for Safety Check v2 on Desktop";
const char kSafetyHubHaTSOneOffSurveyDescription[] =
    "Enables one-off HaTS surveys for Safety Check v2 on Desktop.";
#endif  // BUILDFLAG(IS_ANDROID)

const char kSafetyHubAbusiveNotificationRevocationName[] =
    "Include abusive notification sites in the Permissions Module of Safety "
    "Hub";
const char kSafetyHubAbusiveNotificationRevocationDescription[] =
    "When enabled, includes abusive notification permission revocation in the "
    "site permission module of Safety Hub on desktop.";

const char kSameAppWindowCycleName[] = "Cros Labs: Same App Window Cycling";
const char kSameAppWindowCycleDescription[] =
    "Use Alt+` to cycle through the windows of the active application.";

const char kTestThirdPartyCookiePhaseoutName[] =
    "Test Third Party Cookie Phaseout";
const char kTestThirdPartyCookiePhaseoutDescription[] =
    "Enable to test third-party cookie phaseout. "
    "Learn more: https://goo.gle/3pcd-flags";

const char kScrollableTabStripFlagId[] = "scrollable-tabstrip";
const char kScrollableTabStripName[] = "Tab Scrolling";
const char kScrollableTabStripDescription[] =
    "Enables tab strip to scroll left and right when full.";

const char kTabStripCollectionStorageFlagId[] = "tabstrip-collection-storage";
const char kTabStripCollectionStorageName[] =
    "TabStripModel Collection Based Storage";
const char kTabStripCollectionStorageDescription[] =
    "Enables the collection based storage of tabstrip model.";

const char kTabScrollingButtonPositionFlagId[] =
    "tab-scrolling-button-position";
const char kTabScrollingButtonPositionName[] = "Tab Scrolling Buttons";
const char kTabScrollingButtonPositionDescription[] =
    "Enables buttons on the tab strip to scroll left and right when full";

const char kScrollableTabStripWithDraggingFlagId[] =
    "scrollable-tabstrip-with-dragging";
const char kScrollableTabStripWithDraggingName[] =
    "Tab Scrolling With Dragging";
const char kScrollableTabStripWithDraggingDescription[] =
    "Scrolls the tabstrip while dragging tabs towards the end of the visible "
    "view.";

const char kScrollableTabStripOverflowFlagId[] = "scrollable-tabstrip-overflow";
const char kScrollableTabStripOverflowName[] =
    "Tab Scrolling Overflow Indicator";
const char kScrollableTabStripOverflowDescription[] =
    "Choices for overflow indicators shown when the tabstrip is in scrolling "
    "mode.";

const char kSplitTabStripName[] = "Split TabStrip";
const char kSplitTabStripDescription[] =
    "Splits pinned and unpinned tabs into separate TabStrips under the hood. "
    "Pure refactoring, no user-visible behavioral changes are included.";

const char kDynamicSearchUpdateAnimationName[] =
    "Dynamic Search Result Update Animation";
const char kDynamicSearchUpdateAnimationDescription[] =
    "Dynamically adjust the search result update animation when those update "
    "animations are preempted. Shortened animation durations configurable "
    "(unit: milliseconds).";

const char kSecurePaymentConfirmationDebugName[] =
    "Secure Payment Confirmation Debug Mode";
const char kSecurePaymentConfirmationDebugDescription[] =
    "This flag removes the restriction that PaymentCredential in WebAuthn and "
    "secure payment confirmation in PaymentRequest API must use user verifying "
    "platform authenticators.";

const char kSecurePaymentConfirmationNetworkAndIssuerIconsName[] =
    "Secure Payment Confirmation Network and Issuer Icons";
const char kSecurePaymentConfirmationNetworkAndIssuerIconsDescription[] =
    "Allow the passing in and display of card network and issuer icons for the "
    "Secure Payment Confirmation Web API.";

const char kSegmentationSurveyPageName[] =
    "Segmentation survey internals page and model";
const char kSegmentationSurveyPageDescription[] =
    "Enable internals page for survey and fetching model";

const char kSidePanelJourneysFlagId[] = "side-panel-journeys";
const char kSidePanelJourneysName[] = "Side panel journeys";
const char kSidePanelJourneysDescription[] =
    "Enables Journeys within the side panel.";

const char kSidePanelJourneysQuerylessFlagId[] =
    "side-panel-journeys-queryless";
const char kSidePanelJourneysQuerylessName[] = "Side panel journeys queryless";
const char kSidePanelJourneysQuerylessDescription[] =
    "Enables Journeys queryless view within the side panel.";

const char kSharedZstdName[] = "Shared Zstd";
const char kSharedZstdDescription[] =
    "Enables compression dictionary transport with Zstandard (aka Shared "
    "Zstd).";

const char kSharingDesktopScreenshotsName[] = "Desktop Screenshots";
const char kSharingDesktopScreenshotsDescription[] =
    "Enables taking"
    " screenshots from the desktop sharing hub.";

const char kShowAutofillSignaturesName[] = "Show autofill signatures.";
const char kShowAutofillSignaturesDescription[] =
    "Annotates web forms with Autofill signatures as HTML attributes. Also "
    "marks password fields suitable for password generation.";

const char kShowAutofillTypePredictionsName[] = "Show Autofill predictions";
const char kShowAutofillTypePredictionsDescription[] =
    "Annotates web forms with Autofill field type predictions as placeholder "
    "text.";

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
const char kShowFeaturedEnterpriseSiteSearchName[] =
    "Show featured Enterprise site search engines in Omnibox";
const char kShowFeaturedEnterpriseSiteSearchDescription[] =
    "Allow Enterprise site search engines created by the SiteSearchSettings "
    "policy to be featured in the Omnibox on @ state.";

const char kShowFeaturedEnterpriseSiteSearchIPHName[] =
    "Show IPH for featured Enterprise search";
const char kShowFeaturedEnterpriseSiteSearchIPHDescription[] =
    "Show an IPH on zero-suggest in the Omnibox for featured Enterprise search "
    "engines created by the SiteSearchSettings policy. ";
#endif

const char kShowOverdrawFeedbackName[] = "Show overdraw feedback";
const char kShowOverdrawFeedbackDescription[] =
    "Visualize overdraw by color-coding elements based on if they have other "
    "elements drawn underneath.";

const char kSidePanelResizingFlagId[] = "side-panel-resizing";
const char kSidePanelResizingName[] = "Side Panel Resizing";
const char kSidePanelResizingDescription[] =
    "Allows users to resize the side panel and persist the width across "
    "browser sessions.";

const char kSiteInstanceGroupsForDataUrlsName[] =
    "SiteInstanceGroups for data: URLs";
const char kSiteInstanceGroupsForDataUrlsDescription[] =
    "Put data: URL subframes in a separate SiteInstance from the initiator, "
    "but in the same SiteInstanceGroup, and thus the same process.";

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
const char kSiteSearchSettingsPolicyName[] = "Enable SiteSearchSettings policy";
const char kSiteSearchSettingsPolicyDescription[] =
    "Allow site search engines to be defined by the SiteSearchSettings policy.";

const char kPwaNavigationCapturingName[] = "Desktop PWA Link Capturing";
const char kPwaNavigationCapturingDescription[] =
    "Enables opening links from Chrome in an installed PWA. Currently under "
    "reimplementation.";
#endif

const char kIsolateOriginsName[] = "Isolate additional origins";
const char kIsolateOriginsDescription[] =
    "Requires dedicated processes for an additional set of origins, "
    "specified as a comma-separated list.";

const char kIsolationByDefaultName[] =
    "Change web-facing behaviors that prevent origin-level isolation";
const char kIsolationByDefaultDescription[] =
    "Change several web APIs that make it difficult to isolate origins into "
    "distinct processes. While these changes will ideally become new default "
    "behaviors for the web, this flag is likely to break your experience on "
    "sites you visit today.";

const char kSiteIsolationOptOutName[] = "Disable site isolation";
const char kSiteIsolationOptOutDescription[] =
    "Disables site isolation "
    "(SitePerProcess, IsolateOrigins, etc). Intended for diagnosing bugs that "
    "may be due to out-of-process iframes. Opt-out has no effect if site "
    "isolation is force-enabled using a command line switch or using an "
    "enterprise policy. "
    "Caution: this disables important mitigations for the Spectre CPU "
    "vulnerability affecting most computers.";
const char kSiteIsolationOptOutChoiceDefault[] = "Default";
const char kSiteIsolationOptOutChoiceOptOut[] = "Disabled (not recommended)";

const char kSkiaGraphiteName[] = "Skia Graphite";
const char kSkiaGraphiteDescription[] =
    "Enable Skia Graphite. This will use the Dawn backend by default, but can "
    "be overridden with command line flags for testing on non-official "
    "developer builds. See --skia-graphite-backend flag in gpu_switches.h.";

const char kBackdropFilterMirrorEdgeName[] = "Backdrop Filter Mirror Edge";
const char kBackdropFilterMirrorEdgeDescription[] =
    "When sampling being the backdrop edge for backdrop-filter, samples "
    "beyond the edge are mirrored back into the backdrop rather than "
    "duplicating the pixels at the edge.";

const char kSmoothScrollingName[] = "Smooth Scrolling";
const char kSmoothScrollingDescription[] =
    "Animate smoothly when scrolling page content.";

const char kSplitCacheByNetworkIsolationKeyName[] = "HTTP Cache Partitioning";
const char kSplitCacheByNetworkIsolationKeyDescription[] =
    "Partitions the HTTP Cache by (top-level site, current-frame site) to "
    "disallow cross-site tracking.";

const char kStorageAccessHeadersName[] = "Storage Access Headers";
const char kStorageAccessHeadersDescription[] =
    "Enables HTTP headers related to the Storage Access API.";

const char kStrictOriginIsolationName[] = "Strict-Origin-Isolation";
const char kStrictOriginIsolationDescription[] =
    "Experimental security mode that strengthens the site isolation policy. "
    "Controls whether site isolation should use origins instead of scheme and "
    "eTLD+1.";

const char kSupportTool[] = "Support Tool";
const char kSupportToolDescription[] =
    "Support Tool collects and exports logs to help debugging the issues. It's"
    " available in chrome://support-tool.";

const char kSupportToolScreenshot[] = "Support Tool Screenshot";
const char kSupportToolScreenshotDescription[] =
    "Enables the Support Tool to capture and include a screenshot in the "
    "exported packet.";

const char kSuppressToolbarCapturesName[] = "Suppress Toolbar Captures";
const char kSuppressToolbarCapturesDescription[] =
    "Suppress Toolbar Captures except when certain properties change.";

const char kSyncAutofillWalletCredentialDataName[] =
    "Sync Autofill Wallet Credential Data";
const char kSyncAutofillWalletCredentialDataDescription[] =
    "When enabled, allows syncing of the autofill wallet credential data type.";

const char kSyncAutofillWalletUsageDataName[] =
    "Sync Autofill Wallet Usage Data";
const char kSyncAutofillWalletUsageDataDescription[] =
    "When enabled, allows syncing of the autofill wallet usage data type.";

const char kSyncSandboxName[] = "Use Chrome Sync sandbox";
const char kSyncSandboxDescription[] =
    "Connects to the testing server for Chrome Sync.";

const char kSyncTrustedVaultPassphrasePromoName[] =
    "Enable promos for sync trusted vault passphrase.";
const char kSyncTrustedVaultPassphrasePromoDescription[] =
    "Enables promos for an experimental sync passphrase type, referred to as "
    "trusted vault.";

const char kSystemKeyboardLockName[] = "Experimental system keyboard lock";
const char kSystemKeyboardLockDescription[] =
    "Enables websites to use the keyboard.lock() API to intercept system "
    "keyboard shortcuts and have the events routed directly to the website "
    "when in fullscreen mode.";

const char kTabDragDropName[] = "Tab Drag and Drop via Strip";
const char kTabDragDropDescription[] =
    "Enables Tab drag and drop UI to move tab on tab-strip across windows.";

const char kToolbarPhoneCleanupName[] = "Toolbar Phone cleanup";
const char kToolbarPhoneCleanupDescription[] =
    "Enables cleanup on toolbar phone class.";

const char kCommerceDeveloperName[] = "Commerce developer mode";
const char kCommerceDeveloperDescription[] =
    "Allows users in the allowlist to enter the developer mode";

const char kTabGroupsSaveV2Id[] = "tab-groups-save-v2";
const char kTabGroupsSaveV2Name[] = "Tab Groups Save and Sync V2";
const char kTabGroupsSaveV2Description[] =
    "Enables saving and recalling of tab groups but enhanced. Highly "
    "experimental.";

const char kCompactModeId[] = "compact-mode";
const char kCompactModeName[] = "Compact mode for the browser - Prototype";
const char kCompactModeDescription[] =
    "This feature is a prototype. Reduces the height of the browser controls "
    "(tabstrip, toolbar, bookmarks bar, etc.) to allow more space for the web "
    "contents";

const char kTabGroupsSaveUIUpdateId[] = "tab-groups-save-ui-update";
const char kTabGroupsSaveUIUpdateName[] = "Tab Groups Save UI Update";
const char kTabGroupsSaveUIUpdateDescription[] =
    "Enables saved tab group pinning, everything menu and bookmarks bar change";

const char kTabGroupSyncServiceDesktopMigrationId[] =
    "tab-group-sync-service-desktop-migration";
const char kTabGroupSyncServiceDesktopMigrationName[] =
    "Tab Group Sync Service Desktop Migration";
const char kTabGroupSyncServiceDesktopMigrationDescription[] =
    "Enables use of the TabGroupSyncService. This is a backend only change.";

const char kTabHoverCardImagesName[] = "Tab Hover Card Images";
const char kTabHoverCardImagesDescription[] =
    "Shows a preview image in tab hover cards, if tab hover cards are enabled.";

#if !BUILDFLAG(IS_ANDROID)
const char kTabSearchPositionSettingId[] = "tab-search-position-setting";
const char kTabSearchPositionSettingName[] = "Tab Search Position Setting";
const char kTabSearchPositionSettingDescription[] =
    "Whether to show the tab search position options in the settings page.";
#endif

const char kTearOffWebAppAppTabOpensWebAppWindowName[] = "Tear Off Web App Tab";
const char kTearOffWebAppAppTabOpensWebAppWindowDescription[] =
    "Open Web App window when tearing off a tab that's displaying a url "
    "handled by an installed Web App.";

const char kTextInShelfName[] = "Internal test: text in shelf";
const char kTextInShelfDescription[] =
    "Extend text in shelf timeout to learn about user education";

const char kThreadedScrollPreventRenderingStarvationName[] =
    "threaded-scroll-prevent-rendering-starvation";
const char kThreadedScrollPreventRenderingStarvationDescription[] =
    "Prevents main thread rendering starvation during threaded scrolling based "
    "on a given threshold.";

const char kTintCompositedContentName[] = "Tint composited content";
const char kTintCompositedContentDescription[] =
    "Tint contents composited using Viz with a shade of red to help debug and "
    "study overlay support.";

#if !BUILDFLAG(IS_ANDROID)
const char kToolbarPinningFlagId[] = "toolbar-pinning";
const char kToolbarPinningName[] = "Toolbar pinning";
const char kToolbarPinningDescription[] =
    "Enables the ability to pin features to the toolbar via the Customize "
    "Chrome side panel.";
#endif

#if !BUILDFLAG(IS_ANDROID)
const char kTopChromeToastsName[] = "Top Chrome Toasts";
const char kTopChromeToastsDescription[] =
    "Enables the use of toasts to present confirmation of user actions.";
#endif

const char kTopChromeTouchUiName[] = "Touch UI Layout";
const char kTopChromeTouchUiDescription[] =
    "Enables touch UI layout in the browser's top chrome.";

const char kTouchDragDropName[] = "Touch initiated drag and drop";
const char kTouchDragDropDescription[] =
    "Touch drag and drop can be initiated through long press on a draggable "
    "element.";

const char kTouchSelectionStrategyName[] = "Touch text selection strategy";
const char kTouchSelectionStrategyDescription[] =
    "Controls how text selection granularity changes when touch text selection "
    "handles are dragged. Non-default behavior is experimental.";
const char kTouchSelectionStrategyCharacter[] = "Character";
const char kTouchSelectionStrategyDirection[] = "Direction";

const char kTouchTextEditingRedesignName[] = "Touch Text Editing Redesign";
const char kTouchTextEditingRedesignDescription[] =
    "Enables new touch text editing features.";

const char kTranslateForceTriggerOnEnglishName[] =
    "Select which language model to use to trigger translate on English "
    "content";
const char kTranslateForceTriggerOnEnglishDescription[] =
    "Force the Translate Triggering on English pages experiment to be enabled "
    "with the selected language model active.";

const char kTreatInsecureOriginAsSecureName[] =
    "Insecure origins treated as secure";
const char kTreatInsecureOriginAsSecureDescription[] =
    "Treat given (insecure) origins as secure origins. Multiple origins can be "
    "supplied as a comma-separated list. Origins must have their protocol "
    "specified e.g. \"http://example.com\". For the definition of secure "
    "contexts, see https://w3c.github.io/webappsec-secure-contexts/";

const char kUnifiedPasswordManagerAndroidReenrollmentName[] =
    "Automatic reenrollement of users who were evicted from using Google "
    "Mobile Services after experiencing errors.";
const char kUnifiedPasswordManagerAndroidReenrollmentDescription[] =
    "Requires UnifiedPasswordManagerAndroid flag enabled. Allows automatic "
    "reenrollment into Google Mobile Services if sync and backend "
    "communication work.";

const char kUnsafeWebGPUName[] = "Unsafe WebGPU Support";
const char kUnsafeWebGPUDescription[] =
    "Convenience flag for WebGPU development. Enables best-effort WebGPU "
    "support on unsupported configurations and more! Note that this flag could "
    "expose security issues to websites so only use it for your own "
    "development.";

const char kUiPartialSwapName[] = "Partial swap";
const char kUiPartialSwapDescription[] = "Sets partial swap behavior.";

const char kUIEnableSharedImageCacheForGpuName[] = "Shared GPUImageDecodeCache";
const char kUIEnableSharedImageCacheForGpuDescription[] =
    "Enables shared GPUImageDecodeCache for UI if gpu rasterization is "
    "enabled.";

const char kTPCPhaseOutFacilitatedTestingName[] =
    "Third-party Cookie Phase Out Facilitated Testing";
const char kTPCPhaseOutFacilitatedTestingDescription[] =
    "Enables third-party cookie phase out for facilitated testing described in "
    "https://developer.chrome.com/en/docs/privacy-sandbox/chrome-testing/";

const char kTpcdHeuristicsGrantsName[] =
    "Third-party Cookie Grants Heuristics Testing";
const char kTpcdHeuristicsGrantsDescription[] =
    "Enables temporary storage access grants for certain user behavior "
    "heuristics. See "
    "https://github.com/amaliev/3pcd-exemption-heuristics/blob/main/"
    "explainer.md for more details.";

const char kTpcdMetadataGrantsName[] =
    "Third-Party Cookie Deprecation Metadata Grants for Testing";
const char kTpcdMetadataGrantsDescription[] =
    "Provides a control for enabling/disabling Third-Party Cookie Deprecation "
    "Metadata Grants (WRT its default state) for testing.";

const char kTpcdTrialSettingsName[] =
    "Third-Party Cookie Deprecation Trial Grants for Testing";
const char kTpcdTrialSettingsDescription[] =
    "Controls creation of cookie access grants for the Third-Party Cookies "
    "Deprecation Trial. Please note that, even if enabled, this will still "
    "require #tracking-protection-3pcd to be enabled and a pref to be set, for "
    "it to have an effect. See "
    "https://developer.chrome.com/blog/cookie-countdown-2023oct/ for more "
    "details.";

const char kTopLevelTpcdTrialSettingsName[] =
    "Top-Level Third-Party Cookie Deprecation Trial Grants for Testing";
const char kTopLevelTpcdTrialSettingsDescription[] =
    "Controls creation of cookie access grants for the Top-Level Third-Party "
    "Cookies Deprecation Trial. Please note that, even if enabled, this will "
    "still require #tracking-protection-3pcd to be enabled and a pref to be "
    "set, for it to have an effect. See "
    "https://developer.chrome.com/blog/cookie-countdown-2023oct/ for more "
    "details.";

const char kTrackingProtection3pcdName[] = "Tracking Protection for 3PCD";
const char kTrackingProtection3pcdDescription[] =
    "Enables the tracking protection UI + prefs that will be used for the 3PCD "
    "1%.";

const char kUserNotesSidePanelName[] = "User notes side panel";
const char kUserNotesSidePanelDescription[] =
    "Enables the user notes feature in the side panel. "
    "Only works if Power bookmark backend is also enabled.";

const char kUsernameFirstFlowFallbackCrowdsourcingName[] =
    "Username first flow fallback crowdsourcing";
const char kUsernameFirstFlowFallbackCrowdsourcingDescription[] =
    "Support of sending additional votes on username first flow i.e. login "
    "flows where a user has to type username first on one page and then "
    "password on another page. These votes are sent on single password forms "
    "and contain information whether a 1-password form follows a 1-text form "
    "and the value's type(or pattern) in the latter (e.g. email-like, "
    "phone-like, arbitrary string).";

const char kUsernameFirstFlowWithIntermediateValuesPredictionsName[] =
    "Predictions on Username first flow with intermediate values";
const char kUsernameFirstFlowWithIntermediateValuesPredictionsDescription[] =
    "New single username predictions based on voting from Username First Flow "
    "with intermediate values.";

const char kUsernameFirstFlowWithIntermediateValuesVotingName[] =
    "Username first flow with intermediate values voting";
const char kUsernameFirstFlowWithIntermediateValuesVotingDescription[] =
    "Support voting on username first flow with intermediate values. Username "
    "first flow is login/sign-up flow where a user has to type username first "
    "on one page and then password on another page. Intermediate fields are "
    "usually an OTP field or CAPTCHA.";

const char kUseSearchClickForRightClickName[] =
    "Use Search+Click for right click";
const char kUseSearchClickForRightClickDescription[] =
    "When enabled search+click will be remapped to right click, allowing "
    "webpages and apps to consume alt+click. When disabled the legacy "
    "behavior of remapping alt+click to right click will remain unchanged.";

const char kVcBackgroundReplaceName[] = "Enable vc background replacement";
const char kVcBackgroundReplaceDescription[] =
    "Enables background replacement feature for video conferencing on "
    "chromebooks. THIS WILL OVERRIDE BACKGROUND BLUR.";

const char kVcRelightingInferenceBackendName[] =
    "Select relighting backend for video conferencing";
const char kVcRelightingInferenceBackendDescription[] =
    "Select relighting backend to be used for running model inference during "
    "video conferencing, which may offload work from GPU.";

const char kVcSegmentationInferenceBackendName[] =
    "Select segmentation backend for video conferencing";
const char kVcSegmentationInferenceBackendDescription[] =
    "Select segmentation backend to be used for running model inference "
    "during video conferencing, which may offload work from GPU.";

const char kVcStudioLookName[] = "Enables Studio Look for video conferencing";
const char kVcStudioLookDescription[] =
    "Enables Studio Look and VC settings UI, which contains settings for Studio"
    "Look.";

const char kVcSegmentationModelName[] = "Use a different segmentation model";
const char kVcSegmentationModelDescription[] =
    "Allows a different segmentation model to be used for blur and relighting, "
    "which may reduce the workload on the GPU.";

const char kVcTrayMicIndicatorName[] = "Adds a mic indicator in VC tray";
const char kVcTrayMicIndicatorDescription[] =
    "Displays a pulsing mic indicator that indicates how loud the audio is "
    "captured by the microphone, after some effects like noise cancellation "
    "is applied.";

const char kVcTrayTitleHeaderName[] = "Adds a sidetone toggle in VC tray";
const char kVcTrayTitleHeaderDescription[] =
    "Displays a sidetone toggle in VC Tray Title header";

const char kVcLightIntensityName[] = "VC relighting intensity";
const char kVcLightIntensityDescription[] =
    "Allows different light intenisty to be used for relighting.";

const char kVcWebApiName[] = "VC web API";
const char kVcWebApiDescription[] =
    "Allows web API support for video conferencing on chromebooks.";

const char kVideoPictureInPictureControlsUpdate2024Name[] =
    "Video picture-in-picture controls update 2024";
const char kVideoPictureInPictureControlsUpdate2024Description[] =
    "Displays an updated UI for video picture-in-picture controls from its 2024"
    "UI update";

const char kV8VmFutureName[] = "Future V8 VM features";
const char kV8VmFutureDescription[] =
    "This enables upcoming and experimental V8 VM features. "
    "This flag does not enable experimental JavaScript features.";

const char kGlobalVaapiLockName[] = "Global lock on the VA-API wrapper.";
const char kGlobalVaapiLockDescription[] =
    "Enable or disable the global VA-API lock for platforms and paths that "
    "support controlling this.";

const char kWalletServiceUseSandboxName[] =
    "Use Google Payments sandbox servers";
const char kWalletServiceUseSandboxDescription[] =
    "For developers: use the sandbox service for Google Payments API calls.";

const char kTaskManagerEndProcessDisabledForExtensionName[] =
    "Disable extension process termination through task manager";
const char kTaskManagerEndProcessDisabledForExtensionDescription[] =
    "Allows admnistrators to configure managed devices to prevent users from "
    "closing terminating the process for certain extensions";

const char kWallpaperFastRefreshName[] =
    "Enable shortened wallpaper daily refresh interval for manual testing";
const char kWallpaperFastRefreshDescription[] =
    "Allows developers to see a new wallpaper once every ten seconds rather "
    "than once per day when using the daily refresh feature.";

const char kWallpaperGooglePhotosSharedAlbumsName[] =
    "Enable Google Photos shared albums for wallpaper";
const char kWallpaperGooglePhotosSharedAlbumsDescription[] =
    "Allow users to set shared Google Photos albums as the source for their "
    "wallpaper.";

const char kWallpaperSearchSettingsVisibilityName[] =
    "Wallpaper Search Settings Visibility";
const char kWallpaperSearchSettingsVisibilityDescription[] =
    "Shows wallpaper search settings in settings UI.";

#if !BUILDFLAG(IS_ANDROID)
const char kWebAuthnEnclaveAuthenticatorName[] =
    "Enable the cloud enclave authenticator for GPM passkeys";
const char kWebAuthnEnclaveAuthenticatorDescription[] =
    "Allow users to create and use Google Password Manager passkeys using a "
    "cloud-based authenticator service.";
#endif

const char kWebBluetoothName[] = "Web Bluetooth";
const char kWebBluetoothDescription[] =
    "Enables the Web Bluetooth API on platforms without official support";

const char kWebBluetoothNewPermissionsBackendName[] =
    "Use the new permissions backend for Web Bluetooth";
const char kWebBluetoothNewPermissionsBackendDescription[] =
    "Enables the new permissions backend for Web Bluetooth. This will enable "
    "persistent storage of device permissions and Web Bluetooth features such "
    "as BluetoothDevice.watchAdvertisements() and Bluetooth.getDevices()";

const char kWebOtpBackendName[] = "Web OTP";
const char kWebOtpBackendDescription[] =
    "Enables Web OTP API that uses the specified backend.";
const char kWebOtpBackendSmsVerification[] = "Code Browser API";
const char kWebOtpBackendUserConsent[] = "User Consent API";
const char kWebOtpBackendAuto[] = "Automatically select the backend";

const char kWebglDeveloperExtensionsName[] = "WebGL Developer Extensions";
const char kWebglDeveloperExtensionsDescription[] =
    "Enabling this option allows web applications to access WebGL extensions "
    "intended only for use during development time.";

const char kWebglDraftExtensionsName[] = "WebGL Draft Extensions";
const char kWebglDraftExtensionsDescription[] =
    "Enabling this option allows web applications to access the WebGL "
    "extensions that are still in draft status.";

const char kWebGpuDeveloperFeaturesName[] = "WebGPU Developer Features";
const char kWebGpuDeveloperFeaturesDescription[] =
    "Enables web applications to access WebGPU features intended only for use "
    "during development.";

const char kWebPaymentsExperimentalFeaturesName[] =
    "Experimental Web Payments API features";
const char kWebPaymentsExperimentalFeaturesDescription[] =
    "Enable experimental Web Payments API features";

const char kAppStoreBillingDebugName[] =
    "Web Payments App Store Billing Debug Mode";
const char kAppStoreBillingDebugDescription[] =
    "App-store purchases (e.g., Google Play Store) within a TWA can be "
    "requested using the Payment Request API. This flag removes the "
    "restriction that the TWA has to be installed from the app-store.";

const char kWebrtcHideLocalIpsWithMdnsName[] =
    "Anonymize local IPs exposed by WebRTC.";
const char kWebrtcHideLocalIpsWithMdnsDecription[] =
    "Conceal local IP addresses with mDNS hostnames.";

const char kWebRtcAllowInputVolumeAdjustmentName[] =
    "Allow WebRTC to adjust the input volume.";
const char kWebRtcAllowInputVolumeAdjustmentDescription[] =
    "Allow the Audio Processing Module in WebRTC to adjust the input volume "
    "during a real-time call. Disable if microphone muting or clipping issues "
    "are observed when the browser is running and used for a real-time call. "
    "This flag is experimental and may be removed at any time.";

const char kWebRtcApmDownmixCaptureAudioMethodName[] =
    "WebRTC downmix capture audio method.";
const char kWebRtcApmDownmixCaptureAudioMethodDescription[] =
    "Override the method that the Audio Processing Module in WebRTC uses to "
    "downmix the captured audio to mono (when needed) during a real-time call. "
    "This flag is experimental and may be removed at any time.";

const char kWebrtcHwDecodingName[] = "WebRTC hardware video decoding";
const char kWebrtcHwDecodingDescription[] =
    "Support in WebRTC for decoding video streams using platform hardware.";

const char kWebrtcHwEncodingName[] = "WebRTC hardware video encoding";
const char kWebrtcHwEncodingDescription[] =
    "Support in WebRTC for encoding video streams using platform hardware.";

const char kWebRtcRemoteEventLogName[] = "WebRTC remote-bound event logging";
const char kWebRtcRemoteEventLogDescription[] =
    "Allow collecting WebRTC event logs and uploading them to Crash. "
    "Please note that, even if enabled, this will still require "
    "a policy to be set, for it to have an effect.";

const char kWebrtcUseMinMaxVEADimensionsName[] =
    "WebRTC Min/Max Video Encode Accelerator dimensions";
const char kWebrtcUseMinMaxVEADimensionsDescription[] =
    "When enabled, WebRTC will only use the Video Encode Accelerator for "
    "video resolutions inside those published as supported.";

const char kWebTransportDeveloperModeName[] = "WebTransport Developer Mode";
const char kWebTransportDeveloperModeDescription[] =
    "When enabled, removes the requirement that all certificates used for "
    "WebTransport over HTTP/3 are issued by a known certificate root.";

const char kWebUsbDeviceDetectionName[] =
    "Automatic detection of WebUSB-compatible devices";
const char kWebUsbDeviceDetectionDescription[] =
    "When enabled, the user will be notified when a device which advertises "
    "support for WebUSB is connected. Disable if problems with USB devices are "
    "observed when the browser is running.";

const char kWebXrForceRuntimeName[] = "Force WebXr Runtime";
const char kWebXrForceRuntimeDescription[] =
    "Force the browser to use a particular runtime, even if it would not "
    "usually be enabled or would otherwise not be selected based on the "
    "attached hardware.";

const char kWebXrRuntimeChoiceNone[] = "No Runtime";
const char kWebXrRuntimeChoiceArCore[] = "ARCore";
const char kWebXrRuntimeChoiceCardboard[] = "Cardboard";
const char kWebXrRuntimeChoiceOpenXR[] = "OpenXR";
const char kWebXrRuntimeChoiceOrientationSensors[] = "Orientation Sensors";

const char kWebXrIncubationsName[] = "WebXR Incubations";
const char kWebXrIncubationsDescription[] =
    "Enables experimental features for WebXR.";

const char kZeroCopyName[] = "Zero-copy rasterizer";
const char kZeroCopyDescription[] =
    "Raster threads write directly to GPU memory associated with tiles.";

const char kZstdContentEncodingName[] = "Zstd Content-Encoding";
const char kZstdContentEncodingDescription[] =
    "Enables Zstandard Content-Encoding support.";

const char kEnableVulkanName[] = "Vulkan";
const char kEnableVulkanDescription[] = "Use vulkan as the graphics backend.";

const char kDefaultAngleVulkanName[] = "Default ANGLE Vulkan";
const char kDefaultAngleVulkanDescription[] =
    "Use the Vulkan backend for ANGLE by default.";

const char kVulkanFromAngleName[] = "Vulkan from ANGLE";
const char kVulkanFromAngleDescription[] =
    "Initialize Vulkan from inside ANGLE and share the instance with Chrome.";

const char kResponsiveToolbarName[] = "Responsive toolbar";
const char kResponsiveToolbarDescription[] =
    "Toolbar icons overflow to a chevron icon when the browser width is "
    "resized small than normal";

const char kSharedHighlightingManagerName[] = "Refactoring Shared Highlighting";
const char kSharedHighlightingManagerDescription[] =
    "Refactors Shared Highlighting by centralizing the IPC calls in a Manager.";

const char kSanitizerApiName[] = "Sanitizer API";
const char kSanitizerApiDescription[] =
    "Enable the Sanitizer API. See: https://github.com/WICG/sanitizer-api";

const char kUsePassthroughCommandDecoderName[] =
    "Use passthrough command decoder";
const char kUsePassthroughCommandDecoderDescription[] =
    "Use chrome passthrough command decoder instead of validating command "
    "decoder.";

const char kUseIDNA2008NonTransitionalName[] =
    "Enable IDNA 2008 Non-Transitional Mode";
const char kUseIDNA2008NonTransitionalDescription[] =
    "Enables IDNA 2008 in Non-Transitional Mode in URL processing, allowing "
    "deviation characters in domain names.";

const char kEnablePasswordSharingName[] = "Enables password sharing";
const char kEnablePasswordSharingDescription[] =
    "Enables sharing of password between members of the same family.";

const char kEnableMantaServiceName[] = "Enable Manta Service";
const char kEnableMantaServiceDescription[] =
    "Enables the profile keyed Manta service at startup.";

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)
const char kRunVideoCaptureServiceInBrowserProcessName[] =
    "Run video capture service in browser";
const char kRunVideoCaptureServiceInBrowserProcessDescription[] =
    "Run the video capture service in the browser process.";
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)

const char kPromptAPIForGeminiNanoName[] = "Prompt API for Gemini Nano";
const char kPromptAPIForGeminiNanoDescription[] =
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
    "additional details about appropriate use of "
    "Generative AI.";
const char* const kAIAPIsForGeminiNanoLinks[2] = {
    "https://goo.gle/chrome-ai-dev-preview",
    "https://policies.google.com/terms/generative-ai/use-policy"};

const char kSummarizationAPIForGeminiNanoName[] =
    "Summarization API for Gemini Nano";
const char kSummarizationAPIForGeminiNanoDescription[] =
    "Enables the Summarization API, allowing you to summarize a piece "
    "of text with a built-in large language model (Gemini Nano in Chrome)."
    "The API may be subject to changes including the supported options."
    "Please refer to the built-in AI article [1] for details. "
    "This API It is NOT suitable for use cases that require factual accuracy "
    "(e.g. answering knowledge questions). "
    "You must comply with our Prohibited Use Policy [2] which provides "
    "additional details about appropriate use of Generative AI.";

const char kWriterAPIForGeminiNanoName[] = "Writer API for Gemini Nano";
const char kWriterAPIForGeminiNanoDescription[] =
    "Enables the Writer API, allowing you to write a piece "
    "of text with a built-in large language model (Gemini Nano in Chrome)."
    "The API may be subject to changes including the supported options."
    "Please refer to the built-in AI article [1] for details. "
    "You must comply with our Prohibited Use Policy [2] which provides "
    "additional details about appropriate use of Generative AI.";

const char kRewriterAPIForGeminiNanoName[] = "Rewriter API for Gemini Nano";
const char kRewriterAPIForGeminiNanoDescription[] =
    "Enables the Rewriter API, allowing you to rewrite a piece "
    "of text with a built-in large language model (Gemini Nano in Chrome)."
    "The API may be subject to changes including the supported options."
    "Please refer to the built-in AI article [1] for details. "
    "You must comply with our Prohibited Use Policy [2] which provides "
    "additional details about appropriate use of Generative AI.";

// Android ---------------------------------------------------------------------

#if BUILDFLAG(IS_ANDROID)

const char kAccessibilityIncludeLongClickActionName[] =
    "Accessibility Include Long Click Action";
const char kAccessibilityIncludeLongClickActionDescription[] =
    "When enabled, the accessibility tree for the web contents will include "
    "the ACTION_LONG_CLICK action on all relevant nodes.";

const char kAccessibilityPageZoomName[] = "Accessibility Page Zoom";
const char kAccessibilityPageZoomDescription[] =
    "Whether the UI and underlying code for page zoom should be enabled to"
    " allow a user to increase/decrease the web contents zoom factor.";

const char kAccessibilityPageZoomEnhancementsName[] =
    "Accessibility Page Zoom Enhancements";
const char kAccessibilityPageZoomEnhancementsDescription[] =
    "When enabled, will show enhancements to the Page Zoom feature based on "
    "user feedback from the v1 version (e.g. reset button, better IPH, etc).";

const char kAccessibilitySnapshotStressTestsName[] =
    "Accessibility AXTree Snapshot Stress Tests";
const char kAccessibilitySnapshotStressTestsDescription[] =
    "Enable experiment that will disable max node and timeout limits for the "
    "AXTreeSnapshotter, and track performance stats.";

const char kAccessibilityUnifiedSnapshotsName[] =
    "Accessibility Unified Snapshots";
const char kAccessibilityUnifiedSnapshotsDescription[] =
    "When enabled, use the experimental unified code path for AXTree "
    "snapshots.";

const char kAccessibilityManageBroadcastReceiverOnBackgroundName[] =
    "Manage accessibility Broadcast Receiver on a background thread";
const char kAccessibilityManageBroadcastReceiverOnBackgroundDescription[] =
    "When enabled, registering and un-registering the broadcast "
    "receiver will be on the background thread.";

const char kAccountBookmarksAndReadingListBehindOptInName[] =
    "Account bookmarks and reading list behind opt-in";
const char kAccountBookmarksAndReadingListBehindOptInDescription[] =
    "Make account bookmarks and reading lists available to users that sign in "
    "via promo in the bookmark manager.";

const char kAndroidSurfaceControlName[] = "Android SurfaceControl";
const char kAndroidSurfaceControlDescription[] =
    " Enables SurfaceControl to manage the buffer queue for the "
    " DisplayCompositor on Android. This feature is only available on "
    " android Q+ devices";

const char kAndroidElegantTextHeightName[] = "Android Elegant Text Height";
const char kAndroidElegantTextHeightDescription[] =
    "Enables elegant text height in core BrowserUI theme.";

const char kAndroidGoogleSansTextName[] = "Android Google Sans Text";
const char kAndroidGoogleSansTextDescription[] =
    "Enables Google Sans Text as the default font family.";

const char kAndroidHubFloatingActionButtonName[] =
    "Android Hub Floating Action Button";
const char kAndroidHubFloatingActionButtonDescription[] =
    "Makes the main action button of a pane a floating action button.";

const char kAndroidHubSearchName[] = "Android Hub Search";
const char kAndroidHubSearchDescription[] =
    "Enables searching through the hub.";

const char kAndroidHubV2Name[] = "Android Hub V2";
const char kAndroidHubV2Description[] = "Improvements to the Hub UI surface.";

const char kAndroidOpenPdfInlineName[] = "Open PDF Inline on Android";
const char kAndroidOpenPdfInlineDescription[] =
    "Enable Open PDF Inline on Android.";

const char kChromePageInfoSharingName[] = "Page Info Sharing";
const char kChromePageInfoSharingDescription[] =
    "Enable Page Info Sharing on Android.";

const char kAndroidTabDeclutterName[] = "Android Tab Declutter";
const char kAndroidTabDeclutterDescription[] =
    "Enables auto-archival and deletion of inactive tabs.";

const char kAndroidTabDeclutterArchiveAllButActiveTabName[] =
    "Archive all tabs except active";
const char kAndroidTabDeclutterArchiveAllButActiveTabDescription[] =
    "Causes all tabs in model (except the current active one) to be archived. "
    "Used for manual testing.";

const char kAnimatedImageDragShadowName[] =
    "Enable animated image drag shadow on Android.";
const char kAnimatedImageDragShadowDescription[] =
    "Animate the shadow image from its original bound to the touch point. "
    "Image drag on Android is available when flag touch-drag-and-context-menu "
    "is enabled.";

const char kAnimateSuggestionsListAppearanceName[] =
    "Animate appearance of the omnibox suggestions list";
const char kAnimateSuggestionsListAppearanceDescription[] =
    "Animate the omnibox suggestions list when it appears instead of "
    "immediately setting it to visible";

const char kAppSpecificHistoryName[] = "Allow app specific history";
const char kAppSpecificHistoryDescription[] =
    "If enabled, history results will also be categorized by application.";

const char kBackGestureActivityTabProviderName[] =
    "Back Gesture Refactor (Activity Tab Provider)";
const char kBackGestureActivityTabProviderDescription[] =
    "When enabled, ChromeTabActivity will use getActivityTabProvider to "
    "get current tab, rather than getActivityTab if predictive back gesture"
    "is disabled.";

const char kBackGestureRefactorAndroidName[] = "Back Gesture Refactor";
const char kBackGestureRefactorAndroidDescription[] =
    "Enable Back Gesture Refactor.";

const char kBackToHomeAnimationName[] = "Back to home animation";
const char kBackToHomeAnimationDescription[] =
    "Enable Back to home animation on supported devices.";

const char kBoardingPassDetectorName[] = "Boarding Pass Detector";
const char kBoardingPassDetectorDescription[] = "Enable Boarding Pass Detector";

const char kCCTAuthTabName[] = "CCT Auth Tab";
const char kCCTAuthTabDescription[] = "Enable AuthTab used for authentication";

const char kCCTAuthTabDisableAllExternalIntentsName[] =
    "Disable all external intents in Auth Tab";
const char kCCTAuthTabDisableAllExternalIntentsDescription[] =
    "Disables all external intents in Auth Tab";

const char kCCTBeforeUnloadName[] = "CCT before unload";
const char kCCTBeforeUnloadDescription[] =
    "Kill switch for new CCT before unload behavior during back press";

const char kCCTEphemeralModeName[] =
    "Allow CCT embedders to open CCTs in ephemeral mode";
const char kCCTEphemeralModeDescription[] =
    "Enabling it would allow apps to open ephemeral mode for "
    "Chrome Custom Tabs, on Android.";

const char kCCTIncognitoAvailableToThirdPartyName[] =
    "Allow third party to open Custom Tabs Incognito mode";
const char kCCTIncognitoAvailableToThirdPartyDescription[] =
    "Enabling it would allow third party apps to open incognito mode for "
    "Chrome Custom Tabs, on Android.";

const char kCCTMinimizedName[] = "Allow Custom Tabs to be minimized";
const char kCCTMinimizedDescription[] =
    "When enabled, CCTs can be minimized into picture-in-picture (PiP) mode.";

const char kCCTNestedSecurityIconName[] =
    "Nest the CCT security icon under the title.";
const char kCCTNestedSecurityIconDescription[] =
    "When enabled, the CCT toolbar security icon will be nested under the "
    "title.";

const char kCCTGoogleBottomBarName[] = "Google Bottom Bar";
const char kCCTGoogleBottomBarDescription[] =
    "Show bottom bar on Custom Tabs opened by the Android Google App.";

const char kCCTGoogleBottomBarVariantLayoutsName[] =
    "Google Bottom Bar Variant Layouts";
const char kCCTGoogleBottomBarVariantLayoutsDescription[] =
    "Show different layouts on Google Bottom Bar.";

const char kCCTResizableForThirdPartiesName[] =
    "Bottom sheet Custom Tabs (third party)";
const char kCCTResizableForThirdPartiesDescription[] =
    "Enable bottom sheet Custom Tabs for third party apps.";

const char kCCTRevampedBrandingName[] = "Revamped CCT toolbar branding.";
const char kCCTRevampedBrandingDescription[] =
    "Enables a revamped branding animation on the CCT toolbar.";

const char kCCTSignInPromptName[] = "Sign-in prompt in CCT.";
const char kCCTSignInPromptDescription[] =
    "Displays a sign-in prompt message in CCT opened by 1P apps when the user "
    "is signed out of Chrome but signed in to the 1P app.";

const char kAccountReauthenticationRecentTimeWindowName[] =
    "Account Reauthentication Recent Time Window";
const char kAccountReauthenticationRecentTimeWindowDescription[] =
    "Changes the time window after a successful account authentication during "
    "which reauthentication challenges are not needed.";
const char kAccountReauthenticationRecentTimeWindowDefault[] =
    "10 mins (default)";
const char kAccountReauthenticationRecentTimeWindow0mins[] = "0 mins";
const char kAccountReauthenticationRecentTimeWindow1mins[] = "1 mins";
const char kAccountReauthenticationRecentTimeWindow5mins[] = "5 mins";

const char kBottomBrowserControlsRefactorName[] =
    "BottomBrowserControlsRefactor";
const char kBottomBrowserControlsRefactorDescription[] =
    "Use BottomControlsStacker to position bottom controls layers.";

const char kBrowsingDataModelName[] = "Browsing Data Model";
const char kBrowsingDataModelDescription[] = "Enables BDM on Android.";

const char kChimeAlwaysShowNotificationDescription[] =
    "A debug flag to always show Chime notification after receiving a payload.";
const char kChimeAlwaysShowNotificationName[] =
    "Always show Chime notification";

const char kChimeAndroidSdkDescription[] =
    "Enable Chime SDK to receive push notification.";
const char kChimeAndroidSdkName[] = "Use Chime SDK";

const char kClankDefaultBrowserPromoName[] = "Clank default browser promo 2";
const char kClankDefaultBrowserPromoDescription[] =
    "When enabled, show additional non-intrusive entry points to allow users "
    "to set Chrome as their default browser, if the trigger conditions are "
    "met.";

const char kClankDefaultBrowserPromoRoleManagerName[] =
    "Clank default browser Promo Role Manager ";
const char kClankDefaultBrowserPromoRoleManagerDescription[] =
    "Sets the Role Manager Default Browser Promo for testing the new "
    "Default Browser Promo Feature";

const char kClearLoginDatabaseForAllMigratedUPMUsersDescription[] =
    "This feature clears LoginDatabase for all users when UPM is properly "
    "enabled and the credentials were migrated to GMS Core.";
const char kClearLoginDatabaseForAllMigratedUPMUsersName[] =
    "Clear LoginDatabase for all migrated UPM users";

const char kTabStateFlatBufferName[] = "Enable TabState FlatBuffer";
const char kTabStateFlatBufferDescription[] =
    "Migrates TabState from a pickle based schema to a FlatBuffer based "
    "schema.";

const char kContextualSearchSuppressShortViewName[] =
    "Contextual Search suppress short view";
const char kContextualSearchSuppressShortViewDescription[] =
    "Contextual Search suppress when the base page view is too short";

const char kConvertTrackpadEventsToMouseName[] =
    "Convert trackpad events to mouse events";
const char kConvertTrackpadEventsToMouseDescription[] =
    "Convert trackpad events to mouse events to improve gesture support";

const char kDefaultViewportIsDeviceWidthName[] =
    "Default viewport width is device width";
const char kDefaultViewportIsDeviceWidthDescription[] =
    "Sets the default viewport layout width to be equivalent to "
    "width=device-width";

const char kDeprecatedExternalPickerFunctionName[] =
    "Use deprecated External Picker method";
const char kDeprecatedExternalPickerFunctionDescription[] =
    "Use the old-style opening of an External Picker when uploading files";

const char kDragDropIntoOmniboxName[] =
    "Enable drag and drop into omnibox on Android";
const char kDragDropIntoOmniboxDescription[] =
    "Drag urls, text, and images into omnibox to start a search";

const char kDragDropTabTearingName[] = "Drag and drop tab tearing";
const char kDragDropTabTearingDescription[] =
    "Allow drag tab from tab strip to create new instances if the drag is not "
    "handled.";

const char kDragDropTabTearingEnableOEMName[] =
    "Drag and drop tab tearing enabled based on OEM allowlist";
const char kDragDropTabTearingEnableOEMDescription[] =
    "Allow drag tab from tab strip on specific OEMs to create new instances if "
    "the drag is not handled.";

const char kDrawCutoutEdgeToEdgeName[] = "DrawCutoutEdgeToEdge";
const char kDrawCutoutEdgeToEdgeDescription[] =
    "Enables the Android feature Edge-to-Edge Feature to coordinate with the "
    "Display Cutout for the notch when drawing below the Nav Bar.  Requires "
    "DrawEdgeToEdge to also be enabled.";

const char kDrawEdgeToEdgeName[] = "DrawEdgeToEdge";
const char kDrawEdgeToEdgeDescription[] =
    "Enables the Android feature Edge-to-Edge to draw below the Nav Bar. "
    "Requires DrawCutoutEdgeToEdge to also be enabled.";

const char kDrawKeyNativeEdgeToEdgeName[] = "DrawKeyNativeEdgeToEdge";
const char kDrawKeyNativeEdgeToEdgeDescription[] =
    "Enables the Android feature Edge-to-Edge and forces a draw ToEdge on "
    "select native pages.";

const char kDrawNativeEdgeToEdgeName[] = "DrawNativeEdgeToEdge";
const char kDrawNativeEdgeToEdgeDescription[] =
    "Enables the Android feature Edge-to-Edge and forces a draw ToEdge on all "
    "native pages.";

const char kEdgeToEdgeBottomChinName[] = "EdgeToEdgeBottomChin";
const char kEdgeToEdgeBottomChinDescription[] =
    "Enables the scrollable bottom chin for an intermediate Edge-to-Edge "
    "experience.";

const char kEdgeToEdgeEverywhereName[] = "EdgeToEdgeEverywhere";
const char kEdgeToEdgeEverywhereDescription[] =
    "Enables Chrome to draw below the system bars, all the time. This is "
    "intended "
    "to facilitate the transition to edge-to-edge being enforced at the system "
    "level.";

const char kEdgeToEdgeWebOptInName[] = "EdgeToEdgeWebOptIn";
const char kEdgeToEdgeWebOptInDescription[] =
    "Enables Chrome to draw below the Nav Bar on websites that have explicitly "
    "opted into Edge-to-Edge. Requires DrawCutoutEdgeToEdge to also be "
    "enabled.";

const char kDynamicSafeAreaInsetsName[] = "DynamicSafeAreaInsets";
const char kDynamicSafeAreaInsetsDescription[] =
    "Dynamically change the safe area insets based on the bottom browser "
    "controls visibility.";

const char kDynamicSafeAreaInsetsOnScrollName[] =
    "DynamicSafeAreaInsetsOnScroll";
const char kDynamicSafeAreaInsetsOnScrollDescription[] =
    "Dynamically change the safe area insets as browser controls scrolls. "
    "Requires DynamicSafeAreaInsets to also be enabled.";

const char kEducationalTipModuleName[] = "Educational Tip Module";
const char kEducationalTipModuleDescription[] =
    "Show educational tip module on magic stack in clank";

const char kEnableCommandLineOnNonRootedName[] =
    "Enable command line on non-rooted devices";
const char kEnableCommandLineOnNoRootedDescription[] =
    "Enable reading command line file on non-rooted devices (DANGEROUS).";

const char kEnablePixDetectionName[] = "Enable PIX detection";
const char kEnablePixDetectionDescription[] =
    "Enables PIX code detection on allow-listed merchant websites.";

const char kEnablePixDetectionOnDomContentLoadedName[] =
    "Enable triggering PIX code detection when "
    "`WebContentsObserver::DOMContentLoaded` is fired";
const char kEnablePixDetectionOnDomContentLoadedDescription[] =
    "When enabled, PIX detection will be triggered on allow-listed merchant "
    "websites when `WebContentsObserver::DOMContentLoaded` is fired instead of "
    "`WebContentsObserver::DidFinishLoad`.";

const char kEwalletPaymentsName[] = "Enable eWallet payments";
const char kEwalletPaymentsDescription[] =
    "When enabled, Chrome will offer to pay with eWallet accounts if a payment "
    "link is detected.";

const char kExternalNavigationDebugLogsName[] =
    "External Navigation Debug Logs";
const char kExternalNavigationDebugLogsDescription[] =
    "Enables detailed logging to logcat about why Chrome is making decisions "
    "about whether to allow or block navigation to other apps";

const char kFeedFollowUiUpdateName[] = "UI Update for the Following Feed";
const char kFeedFollowUiUpdateDescription[] =
    "Enables showing the updated UI for the following feed.";

const char kFeedLoadingPlaceholderName[] = "Feed loading placeholder";
const char kFeedLoadingPlaceholderDescription[] =
    "Enables a placeholder UI in "
    "the feed instead of the loading spinner at first load.";

const char kFeedSignedOutViewDemotionName[] = "Feed signed-out view demotion";
const char kFeedSignedOutViewDemotionDescription[] =
    "Enables signed-out view demotion for the Discover Feed.";

const char kFeedStampName[] = "StAMP cards in the feed";
const char kFeedStampDescription[] = "Enables StAMP cards in the feed.";

const char kFeedCloseRefreshName[] = "Feed-close refresh";
const char kFeedCloseRefreshDescription[] =
    "Enables scheduling a background refresh of the feed following feed use.";

const char kFeedContainmentName[] = "Feed containment";
const char kFeedContainmentDescription[] =
    "Enables putting the feed in a container.";

const char kFeedDiscoFeedEndpointName[] =
    "Feed using the DiscoFeed backend endpoint";
const char kFeedDiscoFeedEndpointDescription[] =
    "Uses the DiscoFeed endpoint for serving the feed instead of GWS.";

const char kFeedDynamicColorsName[] = "Enable dynamic colors in the feed";
const char kFeedDynamicColorsDescription[] =
    "Allows feed to fully respect dynamic colors if supported by the client.";

const char kFetchGaiaHashOnSignInName[] =
    "Enable Fetching Gaia credentials from SharedPreferences.";
const char kFetchGaiaHashOnSignInDescription[] =
    "Enable retrieving email, salt, and hashed password from SharedPreferences "
    "for password reuse detection when a user signs in.";

const char kForceListTabSwitcherName[] =
    "Force list tab switcher for low-end devices";
const char kForceListTabSwitcherDescription[] =
    "Forces the list mode of the tab switcher intended for low-end devices. "
    "This flag is intended for debugging only.";

const char kFullscreenInsetsApiMigrationName[] =
    "Migrate to the new fullscreen insets APIs";
const char kFullscreenInsetsApiMigrationDescription[] =
    "Migration from View#setSystemUiVisibility to WindowInsetsController.";

const char kFullscreenInsetsApiMigrationOnAutomotiveName[] =
    "Migrate to the new fullscreen insets APIs on automotive";
const char kFullscreenInsetsApiMigrationOnAutomotiveDescription[] =
    "Migration from View#setSystemUiVisibility to WindowInsetsController on "
    "automotive.";

const char kGtsCloseTabAnimationName[] =
    "Grid tab switcher close tab animation";
const char kGtsCloseTabAnimationDescription[] =
    "New grid tab switcher close tab animation.";

const char kRefreshFeedOnRestartName[] = "Enable refreshing feed on restart";
const char kRefreshFeedOnRestartDescription[] =
    "Refresh feed when Chrome restarts.";

const char kInfoCardAcknowledgementTrackingName[] =
    "Info card acknowledgement tracking";
const char kInfoCardAcknowledgementTrackingDescription[] =
    "Enable acknowledgement tracking for info cards.";

const char kInterestFeedV2Name[] = "Interest Feed v2";
const char kInterestFeedV2Description[] =
    "Show content suggestions on the New Tab Page and Start Surface using the "
    "new Feed Component.";

const char kInterestFeedV2HeartsName[] = "Interest Feed v2 Hearts";
const char kInterestFeedV2HeartsDescription[] = "Enable hearts on Feedv2.";

const char kLogoPolishName[] = "Logo Polish";
const char kLogoPolishDescription[] = "Enable bigger doodle size for NTP.";

const char kLogoPolishAnimationKillSwitchName[] =
    "Logo Polish Animation Kill Switch";
const char kLogoPolishAnimationKillSwitchDescription[] =
    "Enable bigger doodle size with an animation for NTP, this is a kill "
    "switch";

const char kMagicStackAndroidName[] = "Magic Stack Android";
const char kMagicStackAndroidDescription[] =
    "Show a magic stack which contains a list of modules on Start surface and "
    "NTPs on Android.";

const char kMayLaunchUrlUsesSeparateStoragePartitionName[] =
    "MayLaunchUrl Uses Separate Storage Partition";
const char kMayLaunchUrlUsesSeparateStoragePartitionDescription[] =
    "Forces MayLaunchUrl to use a new, ephemeral, storage partition for the "
    "url given to it. This is an experimental feature and may reduce "
    "performance.";

const char kSegmentationPlatformAndroidHomeModuleRankerV2Name[] =
    "Segmentation platform Android home module ranker V2";
const char kSegmentationPlatformAndroidHomeModuleRankerV2Description[] =
    "Enable on-demand segmentation platform service to rank home modules on "
    "Android.";

const char kMediaPickerAdoptionStudyName[] = "Android Media Picker Adoption";
const char kMediaPickerAdoptionStudyDescription[] =
    "Controls how to launch the Android Media Picker (note: This flag is "
    "ignored as of Android U)";

const char kMessagesForAndroidAdsBlockedName[] = "Ads Blocked Messages UI";
const char kMessagesForAndroidAdsBlockedDescription[] =
    "When enabled, ads blocked message will use the new Messages UI.";

const char kMessagesForAndroidSaveCardName[] = "Save Card Messages UI";
const char kMessagesForAndroidSaveCardDescription[] =
    "When enabled, save card prompt will use the new Messages UI.";

const char kMigrateSyncingUserToSignedInName[] =
    "Migrate syncing user to signed-in";
const char kMigrateSyncingUserToSignedInDescription[] =
    "Enables the migration of syncing users to the signed-in, non-syncing "
    "state.";

const char kMouseAndTrackpadDropdownMenuName[] =
    "Android Mouse & Trackpad Drop-down Text Selection Menu";
const char kMouseAndTrackpadDropdownMenuDescription[] =
    "When enabled, shows a dropdown menu for mouse and trackpad secondary "
    "clicks (i.e. right click) with respect to text selection.";

const char kNavBarColorMatchesTabBackgroundName[] =
    "Nav bar color matches tab background";
const char kNavBarColorMatchesTabBackgroundDescription[] =
    "Matches the OS navigation bar color to the background color of the "
    "active tab.";

const char kNotificationOneTapUnsubscribeName[] =
    "Notification one-tap unsubscribe";
const char kNotificationOneTapUnsubscribeDescription[] =
    "Enables an experimental UX that replaces the [Site settings] button on "
    "web push notifications with an [Unsubscribe] button.";

const char kNotificationPermissionRationaleName[] =
    "Notification Permission Rationale UI";
const char kNotificationPermissionRationaleDescription[] =
    "Configure the dialog shown before requesting notification permission. "
    "Only works with builds targeting Android T.";

const char kNotificationPermissionRationaleBottomSheetName[] =
    "Notification Permission Rationale Bottom Sheet UI";
const char kNotificationPermissionRationaleBottomSheetDescription[] =
    "Enable the alternative bottom sheet UI for the notification permission "
    "flow. "
    "Only works with builds targeting Android T+.";

const char kOfflineAutoFetchName[] = "Offline Auto Fetch";
const char kOfflineAutoFetchDescription[] =
    "Enables auto fetch of content when Chrome is online";

const char kOfflineContentOnNetErrorName[] =
    "Offline Content allowed on net errors";
const char kOfflineContentOnNetErrorDescription[] =
    "Enables use of Offline Content on network errors";

const char kOmniboxShortcutsAndroidName[] = "Omnibox shortcuts on Android";
const char kOmniboxShortcutsAndroidDescription[] =
    "Enables storing successful query/match in the omnibox shortcut database "
    "on Android";

const char kPaymentLinkDetectionName[] = "Enable payment link detection";
const char kPaymentLinkDetectionDescription[] =
    "Enables payment link detection in the DOM.";

const char kReadAloudName[] = "Read Aloud";
const char kReadAloudDescription[] = "Controls the Read Aloud feature";

const char kReadAloudBackgroundPlaybackName[] =
    "Read Aloud Background Playback";
const char kReadAloudBackgroundPlaybackDescription[] =
    "Controls background playback for the Read Aloud feature";

const char kReadAloudInCCTName[] = "Read Aloud entrypoint in CCT";
const char kReadAloudInCCTDescription[] =
    "Controls the Read Aloud entrypoint in the overflow menu for CCT";

const char kReadAloudTapToSeekName[] = "Read Aloud Tap to Seek";
const char kReadAloudTapToSeekDescription[] =
    "Controls the Read Aloud Tap to Seek feature";

const char kReadLaterFlagId[] = "read-later";
const char kReadLaterName[] = "Reading List";
const char kReadLaterDescription[] =
    "Allow users to save tabs for later. Enables a new button and menu for "
    "accessing tabs saved for later.";

const char kReaderModeHeuristicsName[] = "Reader Mode triggering";
const char kReaderModeHeuristicsDescription[] =
    "Determines what pages the Reader Mode infobar is shown on.";
const char kReaderModeHeuristicsMarkup[] = "With article structured markup";
const char kReaderModeHeuristicsAdaboost[] = "Non-mobile-friendly articles";
const char kReaderModeHeuristicsAllArticles[] = "All articles";
const char kReaderModeHeuristicsAlwaysOff[] = "Never";
const char kReaderModeHeuristicsAlwaysOn[] = "Always";

const char kReaderModeInCCTName[] = "Reader Mode in CCT";
const char kReaderModeInCCTDescription[] =
    "Open Reader Mode in Chrome Custom Tabs.";

const char kReengagementNotificationName[] =
    "Enable re-engagement notifications";
const char kReengagementNotificationDescription[] =
    "Enables Chrome to use the in-product help system to decide when "
    "to show re-engagement notifications.";

const char kRelatedSearchesAllLanguageName[] =
    "Enables all the languages for Related Searches on Android";
const char kRelatedSearchesAllLanguageDescription[] =
    "Enables requesting related searches suggestions for all the languages.";

const char kRelatedSearchesSwitchName[] =
    "Enables an experiment for Related Searches on Android";
const char kRelatedSearchesSwitchDescription[] =
    "Enables requesting related searches suggestions.";

const char kRevokeNotificationsPermissionIfDisabledOnAppLevelName[] =
    "Revoke site-level notification permission on Android";
const char kRevokeNotificationsPermissionIfDisabledOnAppLevelDescription[] =
    "Allow revoking site-level notification permission if Chrome has no "
    "app-level notification permission on Android.";

const char kRichAutocompletionAndroidName[] =
    "Omnibox rich inline autocompletion on Android";
const char kRichAutocompletionAndroidDescription[] =
    "Enables omnibox rich inline autocompletion. Expands inline autocomplete "
    "to any type of input that users repeatedly use to get to specific URLs.";

const char kSafeBrowsingSyncCheckerCheckAllowlistName[] =
    "Safe Browsing Sync Checker Check Allowlist";
const char kSafeBrowsingSyncCheckerCheckAllowlistDescription[] =
    "Enables Safe Browsing sync checker to check the allowlist before checking "
    "the blocklist.";

const char kShareCustomActionsInCCTName[] = "Custom Actions in CCT";
const char kShareCustomActionsInCCTDescription[] =
    "Display share custom actions Chrome Custom Tabs.";

const char kShowReadyToPayDebugInfoName[] =
    "Show debug information about IS_READY_TO_PAY intents";
const char kShowReadyToPayDebugInfoDescription[] =
    "Display an alert dialog with the contents of IS_READY_TO_PAY intents "
    "that Chrome sends to Android payment applications: app's package name, "
    "service name, payment method name, and method specific data.";

const char kSecurePaymentConfirmationAndroidName[] =
    "Secure Payment Confirmation on Android";
const char kSecurePaymentConfirmationAndroidDescription[] =
    "Enables Secure Payment Confirmation on Android.";

const char kSendTabToSelfV2Name[] = "Send tab to self 2.0";
const char kSendTabToSelfV2Description[] =
    "Enables new received tab "
    "UI shown next to the profile icon instead of using system notifications.";

const char kSetMarketUrlForTestingName[] = "Set market URL for testing";
const char kSetMarketUrlForTestingDescription[] =
    "When enabled, sets the market URL for use in testing the update menu "
    "item.";

const char kSiteIsolationForPasswordSitesName[] =
    "Site Isolation For Password Sites";
const char kSiteIsolationForPasswordSitesDescription[] =
    "Security mode that enables site isolation for sites based on "
    "password-oriented heuristics, such as a user typing in a password.";

const char kSmartZoomName[] = "Smart Zoom";
const char kSmartZoomDescription[] =
    "Enable the Smart Zoom accessibility feature as an alternative approach "
    "to zooming web contents.";

const char kSmartSuggestionForLargeDownloadsName[] =
    "Smart suggestion for large downloads";
const char kSmartSuggestionForLargeDownloadsDescription[] =
    "Smart suggestion that offers download locations for large files.";

const char kFeedPositionAndroidName[] = "Feed Position";
const char kFeedPositionAndroidDescription[] =
    "Enable pushing down or pulling up of Feeds on NTP";

const char kSearchResumptionModuleAndroidName[] = "Search Resumption Module";
const char kSearchResumptionModuleAndroidDescription[] =
    "Enable showing search suggestions on NTP";

const char kStrictSiteIsolationName[] = "Strict site isolation";
const char kStrictSiteIsolationDescription[] =
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

const char kSupportMultipleServerRequestsForPixPaymentsName[] =
    "Support multiple server requests for Pix payments";
const char kSupportMultipleServerRequestsForPixPaymentsDescription[] =
    "When enabled, the network interface with Google Payments supports "
    "handling multiple concurrent requests for Pix flows.";

const char kTabletToolbarReorderingAndroidName[] =
    "Tablet Toolbar Reordering Android.";
const char kTabletToolbarReorderingAndroidDescription[] =
    "Enable Tablet Toolbar Reordering on Android - Reorder the toolbar by "
    "placing the Home button from 1st to the 4th position after the Refresh"
    "button to match the Desktop toolbar";

const char kCrossDeviceTabPaneAndroidName[] = "Cross Device Tab Pane Android";
const char kCrossDeviceTabPaneAndroidDescription[] =
    "Enables showing a new pane in the hub that displays the pre-existing "
    "cross device tabs feature originally located in Recent Tabs.";

const char kTabGroupCreationDialogAndroidName[] =
    "Tab Group Creation Dialog Android";
const char kTabGroupCreationDialogAndroidDescription[] =
    "Enables showing a dialog for adjusting tab group settings on new tab "
    "group creations. The dialog will only show for groups created through "
    "drag and drop or bulk selection merge.";

const char kTabGroupPaneAndroidName[] = "Tab Group Pane Android";
const char kTabGroupPaneAndroidDescription[] =
    "Enables showing a new pane in the hub that displays and allows "
    "interactions with tab groups. These groups may be currently opened in the "
    "tab mode or currently hidden.";

const char kTabGroupParityAndroidName[] = "Tab Group Parity Android";
const char kTabGroupParityAndroidDescription[] =
    "Enables cross-platform tab group feature parity on Android, which "
    "includes colors and required titles.";

const char kTabGroupSyncAndroidName[] = "Tab Group Sync on Android";
const char kTabGroupSyncAndroidDescription[] =
    "Enables syncing of tab groups on Android with other devices.";

const char kTabGroupSyncDisableNetworkLayerName[] =
    "Tab Group Sync Disable Network Layer";
const char kTabGroupSyncDisableNetworkLayerDescription[] =
    "Disables network layer of tab group sync.";

const char kTabGroupSyncForceOffName[] = "Tab Group Sync Force Off";
const char kTabGroupSyncForceOffDescription[] =
    "Disables syncing of tab groups on the device even if user has enabled "
    "it on other devices.";

const char kTabResumptionModuleAndroidName[] = "Tab Resumption Module";
const char kTabResumptionModuleAndroidDescription[] =
    "Enable showing tab suggestions on Android";

const char kTabStripGroupCollapseAndroidName[] =
    "Tab Strip Group Collapse Android";
const char kTabStripGroupCollapseAndroidDescription[] =
    "Enables tab strip group indicators to be clicked to collapse, temporarily "
    "hiding the colored outline, the group title, and the grouped tabs. "
    "Collapsed groups can be clicked again to expand. Synced tab groups will "
    "immediately be collapsed.";

const char kTabStripGroupContextMenuAndroidName[] =
    "Tab Strip Group Context Menu Android";
const char kTabStripGroupContextMenuAndroidDescription[] =
    "Enables tab strip group context menu to show when long-pressing on group "
    "titles, allowing user to assign colors and names to tab groups, open new "
    "tab within the group, ungroup, and close group";

const char kTabStripIncognitoMigrationName[] =
    "Tab Strip Incognito switcher migration to toolbar";
const char kTabStripIncognitoMigrationDescription[] =
    "Migrates tab strip incognito switcher to toolbar and adds options to tab "
    "switcher context menu.";

const char kTabStripLayoutOptimizationName[] = "Tab Strip Layout Optimization";
const char kTabStripLayoutOptimizationDescription[] =
    "Allows adding horizontal and vertical margin to the tab strip.";

const char kTabStripStartupRefactoringName[] =
    "Refactor for tablet tab strip startup.";
const char kTabStripStartupRefactoringDescription[] =
    "Enables refactor for tablet tab strip startup. This creates placeholder "
    "tabs before the tab strip is initialized to prevent "
    "jank (tabs seeming to quickly flicker / scroll).";

const char kTabStripTransitionInDesktopWindowName[] =
    "Tab Strip Transition in Desktop Window";
const char kTabStripTransitionInDesktopWindowDescription[] =
    "Allows hiding / showing the tab strip with varying desktop window widths "
    "by initiating a fade transition.";

const char kDelayTempStripRemovalName[] =
    "Delay temp tab strip removal on startup";
const char kDelayTempStripRemovalDescription[] =
    "By delaying the removal of the placeholder tab strip, we mitigate the "
    "jank seen as tabs are being restored on startup.";

const char kTouchDragAndContextMenuName[] =
    "Simultaneous touch drag and context menu";
const char kTouchDragAndContextMenuDescription[] =
    "Enables touch dragging and a context menu to start simultaneously, with"
    "the assumption that the menu is non-modal.";

const char kUndoMigrationOfSyncingUserToSignedInName[] =
    "Undo the migration of syncing users to signed-in";
const char kUndoMigrationOfSyncingUserToSignedInDescription[] =
    "Enables the reverse-migration of syncing users who were previously "
    "migrated to the signed-in, non-syncing state.";

const char kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarningName[] =
    "Access loss warning for unmigrated password manager users";
const char
    kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarningDescription[] =
        "Chrome password manager users who are not enrolled into the new "
        "experience of storing local passwords in GMSCore will be warned via "
        "various communication channels about data/feature access loss unless "
        "they upgrade GMSCore.";

const char kUpdateMenuBadgeName[] = "Force show update menu badge";
const char kUpdateMenuBadgeDescription[] =
    "When enabled, a badge will be shown on the app menu button if the update "
    "type is Update Available or Unsupported OS Version.";

const char kUpdateMenuItemCustomSummaryDescription[] =
    "When this flag and the force show update menu item flag are enabled, a "
    "custom summary string will be displayed below the update menu item.";
const char kUpdateMenuItemCustomSummaryName[] =
    "Update menu item custom summary";

const char kUpdateMenuTypeName[] =
    "Forces the update menu type to a specific type";
const char kUpdateMenuTypeDescription[] =
    "When set, forces the update type to be a specific one, which impacts "
    "the app menu badge and menu item for updates.";
const char kUpdateMenuTypeNone[] = "None";
const char kUpdateMenuTypeUpdateAvailable[] = "Update Available";
const char kUpdateMenuTypeUnsupportedOSVersion[] = "Unsupported OS Version";

const char kOmahaMinSdkVersionAndroidName[] =
    "Forces the minumum Android SDK version to a particular value.";
const char kOmahaMinSdkVersionAndroidDescription[] =
    "When set, the minimum Android minimum SDK version is set to a particular "
    "value which impact the app menu badge, menu items, and settings about "
    "screen regarding whether Chrome can be updated.";
const char kOmahaMinSdkVersionAndroidMinSdk1Description[] = "Minimum SDK = 1";
const char kOmahaMinSdkVersionAndroidMinSdk1000Description[] =
    "Minimum SDK = 1000";

const char kVideoTutorialsName[] = "Enable video tutorials";
const char kVideoTutorialsDescription[] = "Show video tutorials in Chrome";

const char kAdaptiveButtonInTopToolbarPageSummaryName[] =
    "Adaptive button in top toolbar - Page Summary";
const char kAdaptiveButtonInTopToolbarPageSummaryDescription[] =
    "Enables a summary button in the top toolbar. Must be selected in "
    "Settings > Toolbar Shortcut.";

const char kAdaptiveButtonInTopToolbarCustomizationName[] =
    "Adaptive button in top toolbar customization";
const char kAdaptiveButtonInTopToolbarCustomizationDescription[] =
    "Enables UI for customizing the adaptive action button in the top toolbar";

const char kWebAuthnAndroidCredManName[] =
    "Android Credential Management for passkeys";
const char kWebAuthnAndroidCredManDescription[] =
    "Use Credential Management API for passkeys. Requires Android 14 or "
    "higher.";

const char kWebFeedAwarenessName[] = "Web Feed Awareness";
const char kWebFeedAwarenessDescription[] =
    "Helps the user discover the web feed.";

const char kWebFeedOnboardingName[] = "Web Feed Onboarding";
const char kWebFeedOnboardingDescription[] =
    "Helps the user understand how to use the web feed.";

const char kWebFeedSortName[] = "Web Feed Sort";
const char kWebFeedSortDescription[] =
    "Allows users to sort their web content in the web feed. "
    "Only works if Web Feed is also enabled.";

const char kWebXrSharedBuffersName[] = "WebXR Shared Buffers";
const char kWebXrSharedBuffersDescription[] =
    "Toggles whether or not WebXR attempts to use SharedBuffers for moving "
    "textures from the device to the renderer. When this flag is set to either "
    "enabled or default SharedBuffer support will be dependent on what the "
    "device can actually support.";

const char kXsurfaceMetricsReportingName[] = "Xsurface Metrics Reporting";
const char kXsurfaceMetricsReportingDescription[] =
    "Allows metrics reporting state to be passed to Xsurface";

const char kPasswordSuggestionBottomSheetV2Name[] =
    "Refactored password suggestions bottom sheet";
const char kPasswordSuggestionBottomSheetV2Description[] =
    "Enables the refactored version of the password suggestions bottom sheet. "
    "All the user facing functionality should stay the same.";

const char kEnableAndroidGamepadVibrationName[] = "Gamepad vibration";
const char kEnableAndroidGamepadVibrationDescription[] =
    "Enables the ability to play vibration effects on supported gamepads.";

#if BUILDFLAG(ENABLE_VR) && BUILDFLAG(ENABLE_OPENXR)
const char kOpenXRExtendedFeaturesName[] =
    "WebXR OpenXR Runtime Extended Features";
const char kOpenXRExtendedFeaturesDescription[] =
    "Enables the use of the OpenXR runtime to create WebXR sessions with a "
    "broader feature set (e.g. features not currently supported on Desktop).";

const char kOpenXRName[] = "Enable OpenXR WebXR Runtime";
const char kOpenXRDescription[] =
    "Enables the use of the OpenXR runtime to create WebXR sessions.";
#endif

// Non-Android -----------------------------------------------------------------

#else  // BUILDFLAG(IS_ANDROID)

const char kAllowAllSitesToInitiateMirroringName[] =
    "Allow all sites to initiate mirroring";
const char kAllowAllSitesToInitiateMirroringDescription[] =
    "When enabled, allows all websites to request to initiate tab mirroring "
    "via Presentation API. Requires #cast-media-route-provider to also be "
    "enabled";

const char kDialMediaRouteProviderName[] =
    "Allow cast device discovery with DIAL protocol";
const char kDialMediaRouteProviderDescription[] =
    "Enable/Disable the browser discovery of the DIAL support cast device."
    "It sends a discovery SSDP message every 120 seconds";

const char kDelayMediaSinkDiscoveryName[] =
    "Delay media sink discovery until explicit user interaction with cast";
const char kDelayMediaSinkDiscoveryDescription[] =
    "Delay the browser background discovery of Cast and DIAL devices until "
    "users have interacted with the Cast UI or visited a site supporting Cast "
    "SDK or Remote Playback API.";

const char kShowCastPermissionRejectedErrorName[] =
    "Show the permission rejected error message in the Cast/GMC UI.";
const char kShowCastPermissionRejectedErrorDescription[] =
    "Show an error message in the Cast/GMC UI to inform users when the network "
    "permission is rejected and Chrome's Cast feature is disabled.";

const char kCastMirroringTargetPlayoutDelayName[] =
    "Changes the target playout delay for Cast mirroring.";
const char kCastMirroringTargetPlayoutDelayDescription[] =
    "Choose a target playout delay for Cast mirroring. A lower delay will "
    "decrease latency, but may impact other quality indicators.";
const char kCastMirroringTargetPlayoutDelayDefault[] = "Default (200ms)";
const char kCastMirroringTargetPlayoutDelay100ms[] = "100ms.";
const char kCastMirroringTargetPlayoutDelay150ms[] = "150ms.";
const char kCastMirroringTargetPlayoutDelay250ms[] = "250ms.";
const char kCastMirroringTargetPlayoutDelay300ms[] = "300ms.";
const char kCastMirroringTargetPlayoutDelay350ms[] = "350ms.";
const char kCastMirroringTargetPlayoutDelay400ms[] = "400ms.";

const char kEnableLiveCaptionMultilangName[] = "Multilingual Live Caption";
const char kEnableLiveCaptionMultilangDescription[] =
    "Enables the multilingual Live Caption Feature which allows "
    "for many language choices and automated language choices.";

const char kEnableCrOSLiveTranslateName[] = "Live Translate CrOS";
const char kEnableCrOSLiveTranslateDescription[] =
    "Enables the live translate feature on ChromeOS which allows for live "
    "translation of captions into a target language.";

const char kEnableCrOSSodaLanguagesName[] = "SODA language expansion";
const char kEnableCrOSSodaLanguagesDescription[] =
    "Enable language expansion for SODA on device to "
    "impact dictation and Live Captions.";

const char kEnableCrOSSodaConchLanguagesName[] = "SODA Conch Languages.";
const char kEnableCrOSSodaConchLanguagesDescription[] =
    "Enable Conch specific SODA language models.";

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const char kEsbDownloadRowPromoFeatureName[] =
    "Enable ESB Download Row promotion";
const char kEsbDownloadRowPromoFeatureDescription[] =
    "Promotes Enhanced Safe Browsing (ESB) on a user's most recent "
    "dangerous/suspicious download by adding an extra row under the download "
    "that links to the chrome://settings/security page.";
#endif

const char kForceSigninFlowInProfilePickerName[] =
    "Force Signin Flows in Profile Picker";
const char kForceSigninFlowInProfilePickerDescription[] =
    "Migrates Force Signin flows from the old popup dialog on top of the "
    "Profile Picker, to the regular sign in flow within the Profile Picker "
    "with adaptation to the policy settings";

const char kFreezingOnEnergySaverName[] =
    "Freeze CPU intensive background tabs on Energy Saver";
const char kFreezingOnEnergySaverDescription[] =
    "When Energy Saver is active, freeze eligible background tabs that use a "
    "lot of CPU. A tab is eligible if it's silent, doesn't provide audio- or "
    "video- conference functionality and doesn't use WebUSB or Web Bluetooth.";

const char kFreezingOnEnergySaverTestingName[] =
    "Freeze CPU intensive background tabs on Energy Saver - Testing Mode";
const char kFreezingOnEnergySaverTestingDescription[] =
    "Similar to #freezing-on-energy-saver, with changes to facilitate testing: "
    "1) pretend that Energy Saver is active even when it's not and 2) pretend "
    "that all tabs use a lot of CPU.";

const char kKeyboardAndPointerLockPromptName[] =
    "Keyboard and Pointer Lock prompt";
const char kKeyboardAndPointerLockPromptDescription[] =
    "Requesting to use the keyboard or pointer lock API causes a permission"
    "prompt to be shown.";

const char kPressAndHoldEscToExitBrowserFullscreenName[] =
    "Holding Esc to exit browser fullscreen";
const char kPressAndHoldEscToExitBrowserFullscreenDescription[] =
    "Allows users to press and hold Esc key to exit browser fullscreen.";

const char kReadAnythingWithScreen2xName[] = "Reading Mode with Screen2x";
const char kReadAnythingWithScreen2xDescription[] =
    "Have Reading Mode use a local machine learning model for web page "
    "distillation.";

const char kReadAnythingWithAlgorithmName[] =
    "Reading Mode with rules based algorithm.";
const char kReadAnythingWithAlgorithmDescription[] =
    "Have Reading Mode use a local rules based algorithm for web page "
    "distillation.";

const char kReadAnythingImagesViaAlgorithmName[] =
    "Reading Mode with images added via algorithm";
const char kReadAnythingImagesViaAlgorithmDescription[] =
    "Have Reading Mode use a local rules based algorithm to include images "
    "from webpages.";

const char kReadAnythingReadAloudName[] = "Reading Mode Read Aloud";
const char kReadAnythingReadAloudDescription[] =
    "Enables the experimental Read Aloud feature in Reading Mode.";

const char kReadAloudAutoVoiceSwitchingName[] =
    "Reading Mode Automatic Voice Switching";
const char kReadAloudAutoVoiceSwitchingDescription[] =
    "Enables the experimental Reading Mode feature that automatically chooses "
    "the best voice in Read Aloud using page language.";

const char kReadAnythingReadAloudAutomaticWordHighlightingName[] =
    "Reading Mode Read Aloud Automatic Word Highlighting";
const char kReadAnythingReadAloudAutomaticWordHighlightingDescription[] =
    "Enables the experimental Reading Mode feature that switches between word "
    "and sentence highlighting when reading aloud, depending on speech rate.";

const char kReadAnythingReadAloudPhraseHighlightingName[] =
    "Reading Mode Read Aloud Phrase Highlighting";
const char kReadAnythingReadAloudPhraseHighlightingDescription[] =
    "Enables the experimental Reading Mode feature that highlights by phrases "
    "when reading aloud, when the phrase option is selected from the highlight "
    "menu.";

const char kReadAloudLanguagePackDownloadingName[] =
    "Reading Mode Language Downloading";
const char kReadAloudLanguagePackDownloadingDescription[] =
    "Enables the experimental Reading Mode feature that downloads Google TTS "
    "languages directly from the Read Aloud menus.";

const char kReadAnythingDocsIntegrationName[] =
    "Reading Mode Google Docs Integration";
const char kReadAnythingDocsIntegrationDescription[] =
    "Allows Reading Mode to work on Google Docs.";

const char kReadAnythingDocsLoadMoreButtonName[] =
    "Reading Mode Google Docs Load More Button";
const char kReadAnythingDocsLoadMoreButtonDescription[] =
    "Adds a button to the end of the Reading Mode UI. When clicked, "
    "the main page scrolls to show the next page's content.";

const char kGlobalMediaControlsCastStartStopName[] =
    "Global media controls control Cast start/stop";
const char kGlobalMediaControlsCastStartStopDescription[] =
    "Allows global media controls to control when a Cast session is started "
    "or stopped instead of relying on the Cast dialog.";

const char kLinkPreviewName[] = "Link Preview";
const char kLinkPreviewDescription[] =
    "When enabled, Link Preview feature gets to be available to preview a "
    "linked page in a dedicated small window before navigating to the linked "
    "page. The feature can be triggered from a context menu item, or users' "
    "actions. We are evaluating multiple actions in our experiment to "
    "understand what's to be the best for users from the viewpoint of "
    "security, privacy, and usability. The feature might be unstable and "
    "unusable on some platforms, e.g. macOS or touch devices.";

const char kMuteNotificationSnoozeActionName[] =
    "Snooze action for mute notifications";
const char kMuteNotificationSnoozeActionDescription[] =
    "Adds a Snooze action to mute notifications shown while sharing a screen.";

const char kNtpAlphaBackgroundCollectionsName[] =
    "NTP Alpha Background Collections";
const char kNtpAlphaBackgroundCollectionsDescription[] =
    "Shows alpha NTP background collections in Customize Chrome.";

const char kNtpBackgroundImageErrorDetectionName[] =
    "NTP Background Image Error Detection";
const char kNtpBackgroundImageErrorDetectionDescription[] =
    "Checks NTP background image links for HTTP status errors.";

const char kNtpCalendarModuleName[] = "NTP Calendar Module";
const char kNtpCalendarModuleDescription[] =
    "Shows the Google Calendar module on the New Tab Page.";

const char kNtpChromeCartModuleName[] = "NTP Chrome Cart Module";
const char kNtpChromeCartModuleDescription[] =
    "Shows the chrome cart module on the New Tab Page.";

const char kNtpDriveModuleName[] = "NTP Drive Module";
const char kNtpDriveModuleDescription[] =
    "Shows the Google Drive module on the New Tab Page";

const char kNtpDriveModuleSegmentationName[] = "NTP Drive Module Segmentation";
const char kNtpDriveModuleSegmentationDescription[] =
    "Uses segmentation data to decide whether to show the Drive module on the "
    "New Tab Page.";

const char kNtpDriveModuleShowSixFilesName[] =
    "NTP Drive Module Show Six Files";
const char kNtpDriveModuleShowSixFilesDescription[] =
    "Shows six files in the NTP Drive module, instead of three.";

#if !defined(OFFICIAL_BUILD)
const char kNtpDummyModulesName[] = "NTP Dummy Modules";
const char kNtpDummyModulesDescription[] =
    "Adds dummy modules to New Tab Page when 'NTP Modules Redesigned' is "
    "enabled.";
#endif

const char kNtpMostRelevantTabResumptionModuleName[] =
    "NTP Most Relevant Tab Resumption Module";
const char kNtpMostRelevantTabResumptionModuleDescription[] =
    "Shows the Most Relevant Tab Resumption Module on the New Tab Page.";

const char kNtpMostRelevantTabResumptionModuleDeviceIconName[] =
    "NTP Most Relevant Tab Resumption Module with Device Icon";
const char kNtpMostRelevantTabResumptionModuleDeviceIconDescription[] =
    "Shows the device icon instead of device name on Most Relevant Tab "
    "Resumption Module on the New Tab Page.";

const char kNtpMiddleSlotPromoDismissalName[] =
    "NTP Middle Slot Promo Dismissal";
const char kNtpMiddleSlotPromoDismissalDescription[] =
    "Allows middle slot promo to be dismissed from New Tab Page until "
    "new promo message is populated.";

const char kNtpMobilePromoName[] = "NTP Mobile Promo";
const char kNtpMobilePromoDescription[] =
    "Shows a promo for installing on mobile to the New Tab Page.";

const char kNtpModulesDragAndDropName[] = "NTP Modules Drag and Drop";
const char kNtpModulesDragAndDropDescription[] =
    "Enables modules to be reordered via dragging and dropping on the "
    "New Tab Page.";

const char kNtpModulesHeaderIconName[] = "NTP Modules Header Icon";
const char kNtpModulesHeaderIconDescription[] =
    "Shows icons in NTP module headers.";

const char kNtpModulesRedesignedName[] = "NTP Modules Redesigned";
const char kNtpModulesRedesignedDescription[] =
    "Shows the redesigned modules on the New Tab Page.";

const char kNtpPhotosModuleName[] = "NTP Photos Module";
const char kNtpPhotosModuleDescription[] =
    "Shows the Google Photos module on the New Tab Page";

const char kNtpPhotosModuleOptInArtWorkName[] =
    "NTP Photos Module Opt In ArtWork";
const char kNtpPhotosModuleOptInArtWorkDescription[] =
    "Determines the art work in the NTP Photos Opt-In card";

const char kNtpPhotosModuleOptInTitleName[] = "NTP Photos Module Opt In Title";
const char kNtpPhotosModuleOptInTitleDescription[] =
    "Determines the title of the NTP Photos Opt-In card";

const char kNtpPhotosModuleSoftOptOutName[] = "NTP Photos Module Soft Opt-Out";
const char kNtpPhotosModuleSoftOptOutDescription[] =
    "Enables soft opt-out option in Photos opt-in card";

const char kNtpOutlookCalendarModuleName[] = "NTP Outlook Calendar Module";
const char kNtpOutlookCalendarModuleDescription[] =
    "Shows the Outlook Calendar module on the New Tab Page.";

const char kNtpRealboxContextualAndTrendingSuggestionsName[] =
    "NTP Realbox Contextual and Trending Suggestions";
const char kNtpRealboxContextualAndTrendingSuggestionsDescription[] =
    "Allows NTP Realbox's second column to display contextual and trending "
    "text suggestions.";

const char kNtpRealboxCr23ThemingName[] = "Chrome Refresh Themed Realbox";
const char kNtpRealboxCr23ThemingDescription[] =
    "CR23 theming will be applied in Realbox when enabled.";

const char kNtpRealboxMatchSearchboxThemeName[] =
    "NTP Realbox Matches Searchbox Theme";
const char kNtpRealboxMatchSearchboxThemeDescription[] =
    "Makes NTP Realbox drop shadow match that of the Searchbox when enabled.";

const char kNtpRealboxPedalsName[] = "NTP Realbox Pedals";
const char kNtpRealboxPedalsDescription[] =
    "Shows pedals in the NTP Realbox when enabled.";

const char kNtpRealboxUseGoogleGIconName[] = "NTP Realbox Google G Icon";
const char kNtpRealboxUseGoogleGIconDescription[] =
    "Shows Google G icon "
    "instead of Search Loupe in realbox when enabled";

const char kNtpSafeBrowsingModuleName[] = "NTP Safe Browsing Module";
const char kNtpSafeBrowsingModuleDescription[] =
    "Shows the safe browsing module on the New Tab Page.";

const char kNtpSharepointModuleName[] = "NTP Sharepoint Module";
const char kNtpSharepointModuleDescription[] =
    "Shows the Sharepoint module on the New Tab Page.";

const char kNtpWallpaperSearchButtonName[] = "NTP Wallpaper Search Button";
const char kNtpWallpaperSearchButtonDescription[] =
    "Enables entry point on New Tab Page for Customize Chrome Side Panel "
    "Wallpaper Search.";

const char kNtpWallpaperSearchButtonAnimationName[] =
    "NTP Wallpaper Search Button Animation";
const char kNtpWallpaperSearchButtonAnimationDescription[] =
    "Enables animation for New Tab Page's Wallpaper Search button. Requires "
    "#ntp-wallpaper-search-button to be enabled too.";

const char kNtpWideModulesName[] = "NTP Wide Modules";
const char kNtpWideModulesDescription[] =
    "Shows wide NTP modules if NTP provides enough space.";

const char kHappinessTrackingSurveysForDesktopDemoName[] =
    "Happiness Tracking Surveys Demo";
const char kHappinessTrackingSurveysForDesktopDemoDescription[] =
    "Enable showing Happiness Tracking Surveys Demo to users on Desktop";

const char kHatsWebUIName[] = "HaTS in Chrome WebUI";
const char kHatsWebUIDescription[] =
    "Enables the Happiness Tracking Surveys being delivered via chrome webui, "
    "rather than a separate static website.";

const char kMainNodeAnnotationsName[] = "Main Node Annotations";
const char kMainNodeAnnotationsDescription[] =
    "Uses Screen2x main content extractor to annotate the accessibility tree "
    "with the main landmark on the node identified as main.";

const char kOmniboxDriveSuggestionsName[] =
    "Omnibox Google Drive Document suggestions";
const char kOmniboxDriveSuggestionsDescription[] =
    "Display suggestions for Google Drive documents in the omnibox when Google "
    "is the default search engine.";

const char kOmniboxDriveSuggestionsNoSettingName[] =
    "Omnibox Google Drive Document suggestions don't require separate setting";
const char kOmniboxDriveSuggestionsNoSettingDescription[] =
    "Omnibox Drive suggestions don't require a separate setting and are "
    "available when all other requirements are met. The existing 'Improve "
    "search suggestions' setting can still be used to disable all server-side "
    "suggestions altogether.";

const char kOmniboxDriveSuggestionsNoSyncRequirementName[] =
    "Omnibox Google Drive Document suggestions don't require Chrome Sync";
const char kOmniboxDriveSuggestionsNoSyncRequirementDescription[] =
    "Omnibox Drive suggestions don't require the user to have enabled Chrome "
    "Sync and are available when all other requirements are met.";

const char kProbabilisticMemorySaverName[] = "Probabilistic Memory Saver Mode";
const char kProbabilisticMemorySaverDescription[] =
    "Memory Saver uses some probability distributions to estimate the chance "
    "of tab revisit based on observations about the tab's state.";

const char kSCTAuditingName[] = "SCT auditing";
const char kSCTAuditingDescription[] =
    "Enables SCT auditing for users who have opted in to Safe Browsing "
    "Extended Reporting.";

const char kSmartCardWebApiName[] = "Smart Card API";
const char kSmartCardWebApiDescription[] =
    "Enable access to the Smart Card API. See "
    "https://github.com/WICG/web-smart-card#readme for more information.";

const char kTabOrganizationName[] = "Tab Organization";
const char kTabOrganizationDescription[] =
    "Enables the Auto Tab Groups feature, which can be found within the Tab "
    "Search bubble.";

const char kTabOrganizationSettingsVisibilityName[] =
    "Tab Organization Settings Visibility";
const char kTabOrganizationSettingsVisibilityDescription[] =
    "Shows Tab Organization in the settings UI";

const char kMultiTabOrganizationName[] = "Multi Tab Organization";
const char kMultiTabOrganizationDescription[] =
    "Enables Auto Tab Groups suggestions for multiple tab groups at once.";

const char kTabReorganizationName[] = "Tab Reorganization";
const char kTabReorganizationDescription[] =
    "Enables Auto Tab Groups suggestions for adding tabs to existing groups";

const char kTabReorganizationDividerName[] = "Tab Reorganization Divider";
const char kTabReorganizationDividerDescription[] =
    "Shows a divider in Auto Tab Groups suggestions for existing groups, "
    "indicating which tabs in the group are new.";

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const char kIOSPromoRefreshedPasswordBubbleName[] =
    "Chrome for iOS promo : passwords";
const char kIOSPromoRefreshedPasswordBubbleDescription[] =
    "When enabled, the new Contextual Chrome for iOS promo will be shown to"
    "the eligible users when they save/update their passwords.";

const char kIOSPromoAddressBubbleName[] = "Chrome for iOS promo : addresses";
const char kIOSPromoAddressBubbleDescription[] =
    "When enabled, the Contextual Chrome for iOS promo will be shown to"
    "the eligible users when they save/update their address.";

const char kIOSPromoPaymentBubbleName[] = "Chrome for iOS promo : payments";
const char kIOSPromoPaymentBubbleDescription[] =
    "When enabled, the Contextual Chrome for iOS promo will be shown to"
    "the eligible users when they save/update their payments.";

#endif

const char kWebAppSyncGeneratedIconBackgroundFixName[] =
    "Web App Sync Generated Icon Background Fix";
const char kWebAppSyncGeneratedIconBackgroundFixDescription[] =
    "Schedules attempts to fix generated icons for sync installed web apps in "
    "the background with exponential backoff within their permitted fix time "
    "window.";

const char kWebAppSyncGeneratedIconRetroactiveFixName[] =
    "Web App Sync Generated Icon Retroactive Fix";
const char kWebAppSyncGeneratedIconRetroactiveFixDescription[] =
    "Starts a time window for existing sync installed web apps with generated "
    "icons for background/update events to attempt fixes.";

const char kWebAppSyncGeneratedIconUpdateFixName[] =
    "Web App Sync Generated Icon Update Fix";
const char kWebAppSyncGeneratedIconUpdateFixDescription[] =
    "Allows web apps installed via sync to update their icons without prompting"
    "during a manifest update if the icons were generated, indictative of"
    "network errors during the sync install.";

const char kWebAppUniversalInstallName[] = "Web App Universal Install";
const char kWebAppUniversalInstallDescription[] =
    "Allows any site to be installable on Windows, Mac, and Linux.";

const char kShortcutsNotAppsName[] = "Shortcuts not Apps";
const char kShortcutsNotAppsDescription[] =
    "Changes the 'create shortcut' 3-dot menu option to put a fire-and-forget "
    "link on the desktop, instead of an app.";

#if !BUILDFLAG(IS_ANDROID)
const char kTranslateOpenSettingsName[] = "Translate Open Settings";
const char kTranslateOpenSettingsDescription[] =
    "Add an option to the translate bubble menu to open language settings.";
#endif

const char kUserDisplayModeSyncBrowserMitigationName[] =
    "Web App User Display Mode Sync Browser Mitigation";
const char kUserDisplayModeSyncBrowserMitigationDescription[] =
    "Enables a mitigation during web app install on CrOS for syncing "
    "user_display_mode: kBrowser to non-CrOS devices.";

const char kUserDisplayModeSyncStandaloneMitigationName[] =
    "Web App User Display Mode Sync Standalone Mitigation";
const char kUserDisplayModeSyncStandaloneMitigationDescription[] =
    "Enables a mitigation during web app install on CrOS for syncing "
    "user_display_mode: kStandalone to non-CrOS devices.";

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
const char kWebAppSystemMediaControlsName[] = "Web App System Media Controls";
const char kWebAppSystemMediaControlsDescription[] =
    "Enable instanced system media controls for web apps";
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

const char kWebAuthenticationPermitEnterpriseAttestationName[] =
    "Web Authentication Enterprise Attestation";
const char kWebAuthenticationPermitEnterpriseAttestationDescription[] =
    "Permit a set of origins to request a uniquely identifying enterprise "
    "attestation statement from a security key when creating a Web "
    "Authentication credential.";

#endif  // BUILDFLAG(IS_ANDROID)

// Windows ---------------------------------------------------------------------

#if BUILDFLAG(IS_WIN)

const char kAuthenticateUsingUserConsentVerifierInteropApiName[] =
    "OS authentication will use IUserConsentVerifierInterop api if available.";
const char kAuthenticateUsingUserConsentVerifierInteropApiDescription[] =
    "If user has Windows Hello configured, OS authentication will use "
    "IUserConsentVerifierInterop api with explicit window passing, otherwise "
    "Wincred will be used.";

const char kAuthenticateUsingUserConsentVerifierApiName[] =
    "OS authentication will use UserConsentVerifier api if available.";
const char kAuthenticateUsingUserConsentVerifierApiDescription[] =
    "If user has Windows Hello configured, OS authentication will use "
    "UserConsentVerifier api, otherwise Wincred will be used.";

const char kCalculateNativeWinOcclusionName[] =
    "Calculate window occlusion on Windows";
const char kCalculateNativeWinOcclusionDescription[] =
    "Calculate window occlusion on Windows will be used in the future "
    "to throttle and potentially unload foreground tabs in occluded windows";

const char kEnableMediaFoundationVideoCaptureName[] =
    "MediaFoundation Video Capture";
const char kEnableMediaFoundationVideoCaptureDescription[] =
    "Enable/Disable the usage of MediaFoundation for video capture. Fall back "
    "to DirectShow if disabled.";

const char kHardwareSecureDecryptionName[] = "Hardware Secure Decryption";
const char kHardwareSecureDecryptionDescription[] =
    "Enable/Disable the use of hardware secure Content Decryption Module (CDM) "
    "for protected content playback.";

const char kHidGetFeatureReportFixName[] =
    "Adjust feature reports received with WebHID";
const char kHidGetFeatureReportFixDescription[] =
    "Enable/Disable a fix for a bug that caused feature reports to be offset "
    "by one byte when received from devices that do not use numbered reports.";

const char kHardwareSecureDecryptionExperimentName[] =
    "Hardware Secure Decryption Experiment";
const char kHardwareSecureDecryptionExperimentDescription[] =
    "Enable/Disable the use of hardware secure Content Decryption Module (CDM) "
    "for experimental protected content playback.";

const char kHardwareSecureDecryptionFallbackName[] =
    "Hardware Secure Decryption Fallback";
const char kHardwareSecureDecryptionFallbackDescription[] =
    "Allows automatically disabling hardware secure Content Decryption Module "
    "(CDM) after failures or crashes. Subsequent playback may use software "
    "secure CDMs. If this feature is disabled, the fallback will never happen "
    "and users could be stuck with playback failures.";

const char kMediaFoundationClearName[] = "Media Foundation for Clear";
const char kMediaFoundationClearDescription[] =
    "Enable/Disable the use of MediaFoundation for non-protected content "
    "playback on supported systems.";

const char kMediaFoundationClearStrategyName[] =
    "Media Foundation for Clear Rendering Strategy";
const char kMediaFoundationClearStrategyDescription[] =
    "Sets the rendering strategy to be used when Media Foundation for Clear is "
    "in use. The "
    "Direct Composition rendering strategy enforces presentation to a Direct "
    "Composition surface "
    "from the Media Foundation Media Engine. The Frame Server rendering "
    "strategy produces video "
    "frames from the Media Foundation Media Engine which are fed through "
    "Chromium's frame painting "
    "pipeline. The Dynamic rendering strategy allows changing between the two "
    "modes based on the "
    "current operating conditions. Other options will result in a default "
    "rendering strategy.";

const char kRawAudioCaptureName[] = "Raw audio capture";
const char kRawAudioCaptureDescription[] =
    "Enable/Disable the usage of WASAPI raw audio capture. When enabled, the "
    "audio stream is a 'raw' stream that bypasses all signal processing except "
    "for endpoint specific, always-on processing in the Audio Processing Object"
    " (APO), driver, and hardware.";

const char kUseAngleDescriptionWindows[] =
    "Choose the graphics backend for ANGLE. D3D11 is used on most Windows "
    "computers by default. Using the OpenGL driver as the graphics backend may "
    "result in higher performance in some graphics-heavy applications, "
    "particularly on NVIDIA GPUs. It can increase battery and memory usage of "
    "video playback.";

const char kUseAngleD3D11[] = "D3D11";
const char kUseAngleD3D9[] = "D3D9";
const char kUseAngleD3D11on12[] = "D3D11on12";

const char kUseWaitableSwapChainName[] = "Use waitable swap chains";
const char kUseWaitableSwapChainDescription[] =
    "Use waitable swap chains to reduce presentation latency (effective only "
    "Windows 8.1 or later). If enabled, specify the maximum number of frames "
    "that can be queued, ranging from 1-3. 1 has the lowest delay but is most "
    "likely to drop frames, while 3 has the highest delay but is least likely "
    "to drop frames.";

const char kUseWinrtMidiApiName[] = "Use Windows Runtime MIDI API";
const char kUseWinrtMidiApiDescription[] =
    "Use Windows Runtime MIDI API for WebMIDI (effective only on Windows 10 or "
    "later).";

const char kWebRtcAllowWgcScreenCapturerName[] =
    "Use Windows WGC API for screen capture";
const char kWebRtcAllowWgcScreenCapturerDescription[] =
    "Use Windows.Graphics.Capture API based screen capturer in combination "
    "with the WebRTC based Web API getDisplayMedia. Requires  Windows 10, "
    "version 1803 or higher. Adds a thin yellow border around the captured "
    "screen area. The DXGI API is used as screen capture API when this flag is "
    "disabled.";

const char kWebRtcAllowWgcWindowCapturerName[] =
    "Use Windows WGC API for window capture";
const char kWebRtcAllowWgcWindowCapturerDescription[] =
    "Use Windows.Graphics.Capture API based windows capturer in combination "
    "with the WebRTC based Web API getDisplayMedia. Requires  Windows 10, "
    "version 1803 or higher. Adds a thin yellow border around the captured "
    "window area. The GDI API is used as window capture API when this flag is "
    "disabled.";

const char kWindows11MicaTitlebarName[] = "Windows 11 Mica titlebar";
const char kWindows11MicaTitlebarDescription[] =
    "Use the DWM system-drawn Mica titlebar on Windows 11, version 22H2 (build "
    "22621) and above.";

#if BUILDFLAG(ENABLE_EXTENSIONS)
const char kLaunchWindowsNativeHostsDirectlyName[] =
    "Force Native Host Executables to Launch Directly";
const char kLaunchWindowsNativeHostsDirectlyDescription[] =
    "Force Native Host executables to launch directly via CreateProcess.";
#endif  // ENABLE_EXTENSIONS

#if BUILDFLAG(ENABLE_PRINTING)
const char kPrintWithPostScriptType42FontsName[] =
    "Print with PostScript Type 42 fonts";
const char kPrintWithPostScriptType42FontsDescription[] =
    "When using PostScript level 3 printing, render text with Type 42 fonts if "
    "possible.";

const char kPrintWithReducedRasterizationName[] =
    "Print with reduced rasterization";
const char kPrintWithReducedRasterizationDescription[] =
    "When using GDI printing, avoid rasterization if possible.";

const char kReadPrinterCapabilitiesWithXpsName[] =
    "Read printer capabilities with XPS";
const char kReadPrinterCapabilitiesWithXpsDescription[] =
    "When enabled, utilize XPS interface to read printer capabilities.";

const char kUseXpsForPrintingName[] = "Use XPS for printing";
const char kUseXpsForPrintingDescription[] =
    "When enabled, use XPS printing API instead of the GDI print API.";

const char kUseXpsForPrintingFromPdfName[] = "Use XPS for printing from PDF";
const char kUseXpsForPrintingFromPdfDescription[] =
    "When enabled, use XPS printing API instead of the GDI print API when "
    "printing PDF documents.";
#endif  // BUILDFLAG(ENABLE_PRINTING)

#endif  // BUILDFLAG(IS_WIN)

// Mac -------------------------------------------------------------------------

#if BUILDFLAG(IS_MAC)

const char kEnableExtensibleEnterpriseSSOName[] = "Extensible Enterprise SSO";
const char kEnableExtensibleEnterpriseSSODescription[] =
    "Enables support for extensible enterprise SSO in Chrome";

const char kImmersiveFullscreenName[] = "Immersive Fullscreen Toolbar";
const char kImmersiveFullscreenDescription[] =
    "Automatically hide and show the toolbar in fullscreen.";

const char kMacFullscreenAnimateTabsName[] = "Animate Tabs in Fullscreen";
const char kMacFullscreenAnimateTabsDescription[] =
    "Slide tabs out of the way during the reveal of the close, minimize and "
    "maximize (traffic lights) buttons.";

const char kFullscreenAlwaysShowTrafficLightsName[] =
    "Always Show Traffic Lights in Fullscreen";
const char kFullscreenAlwaysShowTrafficLightsDescription[] =
    "The close, minimize and maximize (traffic lights) buttons are always "
    "shown in fullscreen.";

const char kMacImeLiveConversionFixName[] = "Mac IME Live Conversion";
const char kMacImeLiveConversionFixDescription[] =
    "A fix for the Live Conversion feature of Japanese IME.";

const char kMacLoopbackAudioForScreenShareName[] =
    "Mac System Audio Loopback for Screen Sharing";
const char kMacLoopbackAudioForScreenShareDescription[] =
    "Enable system audio sharing when screen sharing on macOS 13.0+.";

const char kMacPWAsNotificationAttributionName[] =
    "Mac PWA notification attribution";
const char kMacPWAsNotificationAttributionDescription[] =
    "Route notifications for PWAs on Mac through the app shim, attributing "
    "notifications to the correct apps.";

const char kMacSyscallSandboxName[] = "Mac Syscall Filtering Sandbox";
const char kMacSyscallSandboxDescription[] =
    "Controls whether the macOS sandbox filters syscalls.";

const char kRetryGetVideoCaptureDeviceInfosName[] =
    "Retry capture device enumeration on crash";
const char kRetryGetVideoCaptureDeviceInfosDescription[] =
    "Enables retries when enumerating the available video capture devices "
    "after a crash. The capture service is restarted without loading external "
    "DAL plugins which could have caused the crash.";

const char kSonomaAccessibilityActivationRefinementsName[] =
    "Sonoma Accessibility Activation Refinements";
const char kSonomaAccessibilityActivationRefinementsDescription[] =
    "Refines how Chrome responds to accessibility activation signals on macOS "
    "Sonoma.";

const char kUseAngleDescriptionMac[] =
    "Choose the graphics backend for ANGLE. The OpenGL backend is soon to be "
    "deprecated on Mac, and may contain driver bugs that are not planned to be "
    "fixed. The Metal backend is still experimental, and may contain bugs that "
    "are still being worked on. The Metal backend should be more performant, "
    "but may still be behind the OpenGL backend until fully released.";

const char kUseAngleMetal[] = "Metal";

const char kUseAdHocSigningForWebAppShimsName[] =
    "Use Ad-hoc Signing for Web App Shims";
const char kUseAdHocSigningForWebAppShimsDescription[] =
    "Ad-hoc code signing ensures that each PWA app shim has a unique identity. "
    "This allows macOS subsystems to correctly distinguish between multiple "
    "PWAs. Only enabled on macOS 11.7 and later.";

const char kUseSCContentSharingPickerName[] =
    "Use ScreenCaptureKit picker for stream selection";
const char kUseSCContentSharingPickerDescription[] =
    "This feature opens a native picker in MacOS 15+ to allow the  selection "
    "of a window or screen that will be captured.";

#endif

// Windows and Mac -------------------------------------------------------------

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

const char kLocationProviderManagerName[] =
    "Enable location provider manager for Geolocation API";
const char kLocationProviderManagerDescription[] =
    "Enables usage of the location provider manager to select between "
    "the operating system's location API or the network-based provider "
    "as the data source for Geolocation API.";

const char kUseAngleName[] = "Choose ANGLE graphics backend";
const char kUseAngleDefault[] = "Default";
const char kUseAngleGL[] = "OpenGL";

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

// ChromeOS -------------------------------------------------------------------

#if BUILDFLAG(IS_CHROMEOS_ASH)

const char kAcceleratedMjpegDecodeName[] =
    "Hardware-accelerated mjpeg decode for captured frame";
const char kAcceleratedMjpegDecodeDescription[] =
    "Enable hardware-accelerated mjpeg decode for captured frame where "
    "available.";

const char kAccessibilityFilterKeysName[] = "Filter keys";
const char kAccessibilityFilterKeysDescription[] =
    "Enables settings to filter key presses in various ways, such as slow keys "
    "and bounce keys.";

const char kAdaptiveChargingForTestingName[] =
    "Show adaptive charging notifications for testing";
const char kAdaptiveChargingForTestingDescription[] =
    "Show adaptive charging notifications and nudges for testing. This is "
    "meant to be used by developers to test the feature UI only. The "
    "notifications will be shown after the device is plugged in to the "
    "charger. Please DO NOT enable this if you're not a developer who wants to "
    "test the UI of the adaptive charging feature.";

const char kAdaptiveChargingName[] = "Enable adaptive charging feature";
const char kAdaptiveChargingDescription[] =
    "Show settings to enable/disable adaptive charging feature.";

const char kAllowApnModificationPolicyName[] =
    "Allow APN Modification by Policy";
const char kAllowApnModificationPolicyDescription[] =
    "Enables the ChromeOS APN Allow APN Modification policy, which gives "
    "admins the ability to allow or prohibit managed users from modifying "
    "APNs.";

const char kAllowCrossDeviceFeatureSuiteName[] =
    "Allow the use of Cross-Device features";
const char kAllowCrossDeviceFeatureSuiteDescription[] =
    "Allow features such as Nearby Share, PhoneHub, Fast Pair, and Smart Lock, "
    "that require communication with a nearby device. This should be enabled "
    "by default on most platforms, and only disabled in cases where we cannot "
    "guarantee a good experience with the stock Bluetooth hardware (e.g. "
    "ChromeOS Flex). If disabled, this removes all Cross-Device features and "
    "their entries in the Settings app.";

const char kLinkCrossDeviceInternalsName[] =
    "Link Cross-Device internals logging to Feedback reports.";
const char kLinkCrossDeviceInternalsDescription[] =
    "Improves debugging of Cross-Device features by recording more verbose "
    "logs and attaching these logs to filed Feedback reports.";

const char kAllowDevtoolsInSystemUIName[] = "Enable DevTools in System UI";
const char kAllowDevtoolsInSystemUIDescription[] =
    "Enable the developer tools (DevTools) including the page source viewer "
    "(view-source) in Ash. By default, these tools are disabled if Lacros is "
    "the only browser, so as not to confuse the user by opening an Ash window. "
    "By enabling this flag, you can access them via the context menu or "
    "shortcuts to debug the system UI.";

const char kAllowFpmcuBetaFirmwareName[] =
    "Allow using beta firmware for the FPMCU";
const char kAllowFpmcuBetaFirmwareDescription[] =
    "Allow using beta firmware for the Fingerprint MCU if available";

const char kAllowScrollSettingsName[] =
    "Allow changes to scroll acceleration/sensitivity for mice.";
const char kAllowScrollSettingsDescription[] =
    "Shows settings to enable/disable scroll acceleration and to adjust the "
    "sensitivity for scrolling.";

const char kAltClickAndSixPackCustomizationName[] =
    "Allow users to customize Alt-Click and 6-pack key remapping.";

const char kAltClickAndSixPackCustomizationDescription[] =
    "Shows settings to customize Alt-Click and 6-pack key remapping in the "
    "keyboard settings page.";

const char kAlwaysEnableHdcpName[] = "Always enable HDCP for external displays";
const char kAlwaysEnableHdcpDescription[] =
    "Enables the specified type for HDCP whenever an external display is "
    "connected. By default, HDCP is only enabled when required.";
const char kAlwaysEnableHdcpDefault[] = "Default";
const char kAlwaysEnableHdcpType0[] = "Type 0";
const char kAlwaysEnableHdcpType1[] = "Type 1";

const char kApnPoliciesName[] = "APN Policies";
const char kApnPoliciesDescription[] =
    "Enables the ChromeOS APN Policies, which gives admins the ability to set "
    "APN policies for managed eSIM networks and pSIMs. Note that the 'APN "
    "Revamp' flag should be enabled as well for this feature to work as "
    "expected.";

const char kApnRevampName[] = "APN Revamp";
const char kApnRevampDescription[] =
    "Enables the ChromeOS APN Revamp, which updates cellular network APN "
    "system UI and related infrastructure.";

const char kAppLaunchAutomationName[] = "Enable app launch automation";
const char kAppLaunchAutomationDescription[] =
    "Allows groups of apps to be launched.";

const char kArcArcOnDemandExperimentName[] = "Enable ARC on Demand";
const char kArcArcOnDemandExperimentDescription[] =
    "Delay ARC activation if no apps is installed.";

const char kArcCustomTabsExperimentName[] =
    "Enable Custom Tabs experiment for ARC";
const char kArcCustomTabsExperimentDescription[] =
    "Allow Android apps to use Custom Tabs."
    "This feature only works on the Canary and Dev channels.";

const char kArcDocumentsProviderUnknownSizeName[] =
    "Enable ARC DocumentsProvider unknown file size handling";
const char kArcDocumentsProviderUnknownSizeDescription[] =
    "Allow opening DocumentsProvider files where size is not reported.";

const char kArcEnableAttestationName[] = "Enable ARC attestation";
const char kArcEnableAttestationDescription[] =
    "Allow key and ID attestation to run for keymint";

const char kArcExtendInputAnrTimeoutName[] =
    "Extend input event ANR timeout time";
const char kArcExtendInputAnrTimeoutDescription[] =
    "When enabled, the default input event ANR timeout time will be extended"
    " from 5 seconds to 8 seconds.";

const char kArcExtendIntentAnrTimeoutName[] =
    "Extend broadcast of intent ANR timeout time";
const char kArcExtendIntentAnrTimeoutDescription[] =
    "When enabled, the default broadcast of intent ANR timeout time will be"
    " extended from 10 seconds to 15 seconds for foreground broadcasts, 60"
    " seconds to 90 seconds for background broadcasts.";

const char kArcExtendServiceAnrTimeoutName[] =
    "Extend executing service ANR timeout time";
const char kArcExtendServiceAnrTimeoutDescription[] =
    "When enabled, the default executing service ANR timeout time will be"
    " extended from 20 seconds to 30 seconds for foreground services, 200"
    " seconds to 300 seconds for background services.";

const char kArcFilePickerExperimentName[] =
    "Enable file picker experiment for ARC";
const char kArcFilePickerExperimentDescription[] =
    "Enables using ChromeOS file picker in ARC.";

const char kArcFriendlierErrorDialogName[] =
    "Enable friendlier error dialog for ARC";
const char kArcFriendlierErrorDialogDescription[] =
    "Replaces disruptive error dialogs with Chrome notifications for some ANR "
    "and crash events.";

const char kArcIdleManagerName[] = "Enable ARC Idle Manager";
const char kArcIdleManagerDescription[] =
    "ARC will turn on Android's doze mode when idle.";

const char kArcIgnoreHoverEventAnrName[] =
    "Ignore ANR triggered by hover event in ARC";
const char kArcIgnoreHoverEventAnrDescription[] =
    "When enabled, ANR triggered by hover input event will be ignored. Noted"
    " that ANR triggered by other types of input event will not be ignored.";

const char kArcInstantResponseWindowOpenName[] =
    "Enable Instance Response for ARC app window open";
const char kArcInstantResponseWindowOpenDescription[] =
    "In some devices the placeholder window will popup immediately after the "
    "user attempts to launch apps.";

const char kArcNativeBridgeToggleName[] =
    "Toggle between native bridge implementations for ARC";
const char kArcNativeBridgeToggleDescription[] =
    "Toggle between native bridge implementations for ARC.";

const char kArcPerAppLanguageName[] =
    "Enable ARC Per-App Language setting integration";
const char kArcPerAppLanguageDescription[] =
    "When enabled, ARC Per-App Language settings will be surfaced in ChromeOS "
    "settings.";

const char kArcResizeCompatName[] = "Enable ARC Resize Compatibility features";
const char kArcResizeCompatDescription[] =
    "Enable resize compatibility features for ARC++ apps";

const char kArcRoundedWindowCompatName[] = "ARC Rounded Window Compatibility";
const char kArcRoundedWindowCompatDescription[] =
    "Enable rounded window compatibility feature for ARC++ apps";

const char kArcRtVcpuDualCoreName[] =
    "Enable ARC real time vcpu on a device with 2 logical cores online.";
const char kArcRtVcpuDualCoreDesc[] =
    "Enable ARC real time vcpu on a device with 2 logical cores online to "
    "reduce media playback glitch.";

const char kArcRtVcpuQuadCoreName[] =
    "Enable ARC real time vcpu on a device with 3+ logical cores online.";
const char kArcRtVcpuQuadCoreDesc[] =
    "Enable ARC real time vcpu on a device with 3+ logical cores online to "
    "reduce media playback glitch.";

const char kArcSwitchToKeyMintDaemonName[] = "Switch to KeyMint Daemon.";
const char kArcSwitchToKeyMintDaemonDesc[] =
    "Switch from Keymaster Daemon to KeyMint Daemon. Must be switched on/off "
    "at the same time with \"Switch To KeyMint on ARC-T\"";

const char kArcSwitchToKeyMintOnTName[] = "Switch to KeyMint on ARC-T.";
const char kArcSwitchToKeyMintOnTDesc[] =
    "Switch from Keymaster to KeyMint on ARC-T. Must be switched on/off at the "
    "same time with \"Switch to KeyMint Daemon\"";

const char kArcSwitchToKeyMintOnTOverrideName[] =
    "Override switch to KeyMint on ARC-T.";
const char kArcSwitchToKeyMintOnTOverrideDesc[] =
    "Override the block on certain boards to switch from Keymaster to KeyMint";

const char kArcSyncInstallPriorityName[] =
    "Enable supporting install priority for synced ARC apps.";
const char kArcSyncInstallPriorityDescription[] =
    "Enable supporting install priority for synced ARC apps. Pass install "
    "priority to Play instead of using default install priority specified "
    "in Play";

const char kArcTouchscreenEmulationName[] =
    "Enable touchscreen emulation for compatibility on specific ARC apps.";
const char kArcTouchscreenEmulationDesc[] =
    "Enable touchscreen emulation for compatibility on specific ARC apps.";

const char kArcVmMemorySizeName[] = "Enable custom ARCVM memory size";
const char kArcVmMemorySizeDesc[] =
    "Enable custom ARCVM memory size, "
    "\"shift\" controls the amount to shift system RAM when sizing ARCVM.";

const char kArcVmmSwapKBShortcutName[] =
    "Keyboard shortcut trigger for ARCVM"
    " vmm swap feature";
const char kArcVmmSwapKBShortcutDesc[] =
    "Alt + Ctrl + Shift + O/P to enable / disable ARCVM vmm swap. Only for "
    "experimental usage.";

const char kArcAAudioMMAPLowLatencyName[] =
    "Enable ARCVM AAudio MMAP low latency";
const char kArcAAudioMMAPLowLatencyDescription[] =
    "When enabled, ARCVM AAudio MMAP will use low latency setting.";

const char kArcEnableVirtioBlkForDataName[] =
    "Enable virtio-blk for ARCVM /data";
const char kArcEnableVirtioBlkForDataDesc[] =
    "If enabled, ARCVM uses virtio-blk for /data in Android storage.";

const char kArcExternalStorageAccessName[] = "External storage access by ARC";
const char kArcExternalStorageAccessDescription[] =
    "Allow Android apps to access external storage devices like USB flash "
    "drives and SD cards";

const char kArcUnthrottleOnActiveAudioV2Name[] =
    "Unthrottle ARC on active audio";
const char kArcUnthrottleOnActiveAudioV2Description[] =
    "Do not throttle ARC when there is an active audio stream running.";

const char kAshEnableUnifiedDesktopName[] = "Unified desktop mode";
const char kAshEnableUnifiedDesktopDescription[] =
    "Enable unified desktop mode which allows a window to span multiple "
    "displays.";

const char kAshModifierSplitName[] = "Modifier split feature";
const char kAshModifierSplitDescription[] =
    "Enable new modifier split feature on ChromeOS.";

const char kAshModifierSplitFeatureKeyName[] =
    "Feature key for modifier split feature";
const char kAshModifierSplitFeatureKeyDescription[] =
    "Feature key to use the new modifier split feature on ChromeOS.";

const char kAshPickerName[] = "Picker feature";
const char kAshPickerDescription[] = "Enable new Picker feature on ChromeOS.";

const char kAshPickerAlwaysShowFeatureTourName[] = "Picker feature tour";
const char kAshPickerAlwaysShowFeatureTourDescription[] =
    "Always show the feature tour for debugging.";

const char kAshPickerCloudName[] = "Picker cloud search";
const char kAshPickerCloudDescription[] =
    "Enable cloud-based search for the Picker feature on ChromeOS.";

const char kAshPickerGifsName[] = "Picker GIFs search";
const char kAshPickerGifsDescription[] = "Enable GIf search for Picker.";

const char kAshPickerGridName[] = "Picker grid UI";
const char kAshPickerGridDescription[] = "Eanble the grid UI for Picker.";

const char kAshSplitKeyboardRefactorName[] = "Split keyboard refactor";
const char kAshSplitKeyboardRefactorDescription[] =
    "Enable split keyboard refactor on ChromeOS.";

const char kAssistantIphName[] = "Assistant IPH";
const char kAssistantIphDescription[] =
    "Enables showing Assistant IPH on ChromeOS.";

const char kAudioA2DPAdvancedCodecsName[] = "BT A2DP advanced codecs support";
const char kAudioA2DPAdvancedCodecsDescription[] =
    "Enable BT A2DP advanced codecs support";

const char kAudioAPNoiseCancellationName[] = "Audio noise cancellation on AP";
const char kAudioAPNoiseCancellationDescription[] =
    "Enable noise cancellation on AP";

const char kAudioHFPMicSRToggleName[] = "Audio toggle for hfp-mic-sr";
const char kAudioHFPMicSRToggleDescription[] =
    "Enable the ui to show the toggle for controlling hfp-mic-sr.";

const char kAudioHFPOffloadName[] =
    "Audio Bluetooth HFP offloaded to DSP if supported";
const char kAudioHFPOffloadDescription[] =
    "While enabled, HFP Audio data is transmitted via the offloaded path "
    "in DSP if supported by device.";

const char kAudioHFPSwbName[] = "Audio Bluetooth HFP Super-wide-band support";
const char kAudioHFPSwbDescription[] =
    "Enable Bluetooth HFP Super-wide-band codec if supported.";

const char kAudioOffloadCrasDSPToSOFName[] =
    "Offload CRAS DSP modules to Audio FW if supported";
const char kAudioOffloadCrasDSPToSOFDescription[] =
    "While enabled, CRAS DSP modules are offloaded to Audio Firmware (SOF) "
    "if supported by device.";

const char kAudioStyleTransferName[] = "Audio style transfer";
const char kAudioStyleTransferDescription[] = "Enable audio style transfer";

const char kAudioAecRequiredForCrasProcessorName[] =
    "Require AEC to be active for an audio stream to run CrasProcessor";
const char kAudioAecRequiredForCrasProcessorDescription[] =
    "Require AEC to be active for an audio stream to run CrasProcessor. For "
    "debug purpose only.";

const char kAudioSelectionImprovementName[] =
    "Enable audio selection improvement algorithm";
const char kAudioSelectionImprovementDescription[] =
    "Enable set-based audio selection improvement algorithm.";

const char kResetAudioSelectionImprovementPrefName[] =
    "Reset audio selection improvement user preference";
const char kResetAudioSelectionImprovementPrefDescription[] =
    "Reset audio selection improvement user preference for testing purpose.";

const char kAudioSuppressSetRTCAudioActiveName[] =
    "Suppress calling the SetRTCAudioActive D-Bus method";
const char kAudioSuppressSetRTCAudioActiveDescription[] =
    "Don't call the SetRTCAudioActive D-Bus method in CRAS.";

const char kAutoFramingOverrideName[] = "Auto-framing control override";
const char kAutoFramingOverrideDescription[] =
    "Overrides the default to forcibly enable or disable the auto-framing "
    "feature";

const char kAutocorrectByDefaultName[] = "CrOS autocorrect by default";
const char kAutocorrectByDefaultDescription[] =
    "Enables autocorrect by default experiment on ChromeOS";

const char kAutocorrectParamsTuningName[] = "CrOS autocorrect params tuning";
const char kAutocorrectParamsTuningDescription[] =
    "Enables params tuning experiment for autocorrect on ChromeOS.";

const char kAutocorrectToggleName[] = "CrOS autocorrect toggle";
const char kAutocorrectToggleDescription[] =
    "Enables using a toggle for enabling autocorrect on ChromeOS.";

const char kAutocorrectUseReplaceSurroundingTextName[] =
    "Use ReplaceSurroundingText API for autocorrect.";
const char kAutocorrectUseReplaceSurroundingTextDescription[] =
    "When autocorrect is enabled, use the ReplaceSurroundingText API "
    "for better app compatibility.";

const char kBatteryBadgeIconName[] = "Enables smaller battery badge icon";
const char kBatteryBadgeIconDescription[] =
    "Enables smaller battery badge icons for increased legibility of the "
    "battery percentage.";

const char kBlockTelephonyDevicePhoneMuteName[] =
    "Block Telephony Device Phone Mute";
const char kBlockTelephonyDevicePhoneMuteDescription[] =
    "Block telephony device phone mute HID code so it does not toggle ChromeOS "
    "system microphone mute.";

const char kBluetoothAudioLEAudioOnlyName[] = "Bluetooth Audio LE Audio Only";
const char kBluetoothAudioLEAudioOnlyDescription[] =
    "Enable Bluetooth LE audio and disable classic profiles "
    "(A2DP, HFP, AVRCP). This is used for prototyping and demonstration "
    "purposes.";

const char kBluetoothBtsnoopInternalsName[] =
    "Enables btsnoop collection in chrome://bluetooth-internals";
const char kBluetoothBtsnoopInternalsDescription[] =
    "Enables bluetooth traffic (btsnoop) collection via the page "
    "chrome://bluetooth-internals. Btsnoop logs are essential for debugging "
    "bluetooth issues.";

const char kBluetoothFlossTelephonyName[] = "Bluetooth Floss Telephony";
const char kBluetoothFlossTelephonyDescription[] =
    "Enable Floss to create a Bluetooth HID device that allows applications to "
    "access Bluetooth telephony functions through WebHID.";

const char kBluetoothDisconnectWarningName[] = "Bluetooth Disconnect Warning";
const char kBluetoothDisconnectWarningDescription[] =
    "Enables Bluetooth Disconnect Warning feature. When enabled, a "
    "warning dialog will be shown when a user tries to disable Bluetooth "
    "while only Bluetooth input devices are connected to the host.";

const char kBluetoothCoredumpName[] = "Enable Bluetooth Device Coredump";
const char kBluetoothCoredumpDescription[] =
    "Enable Bluetooth coredump collection if supported. Please note that "
    "coredumps are only collected when hardware exceptions occur and are "
    "used for debugging such exceptions.";

const char kBluetoothFlossCoredumpName[] =
    "Enable Bluetooth Device Coredump for Floss";
const char kBluetoothFlossCoredumpDescription[] =
    "Enable Bluetooth coredump collection if supported. Please note that "
    "coredumps are only collected when hardware exceptions occur and are "
    "used for debugging such exceptions.";

const char kBluetoothUseFlossName[] = "Use Floss instead of BlueZ";
const char kBluetoothUseFlossDescription[] =
    "Enables using Floss (also known as Fluoride, Android's Bluetooth stack) "
    "instead of Bluez. This is meant to be used by developers and is not "
    "guaranteed to be stable";

const char kBluetoothWifiQSPodRefreshName[] =
    "Enable better bluetooth and wifi UI";
const char kBluetoothWifiQSPodRefreshDescription[] =
    "Enables better quick settings UI for bluetooth and wifi error states";

const char kBluetoothFlossIsAvailabilityCheckNeededName[] =
    "Floss availability check";
const char kBluetoothFlossIsAvailabilityCheckNeededDescription[] =
    "Floss availability is determined by each machine. If the machine set "
    "Floss is unavailable, Floss won't be enabled no matter what "
    "bluetooth-use-floss is set. Enable this flag will bypass the check so "
    "that users can still enable Floss by bluetooth-use-floss.";

const char kBluetoothUseLLPrivacyName[] = "Enable LL Privacy in Floss";
const char kBluetoothUseLLPrivacyDescription[] =
    "Enable address resolution offloading to Bluetooth Controller if "
    "supported. Modifying this flag will cause Bluetooth Controller to reset.";

const char kCampbellGlyphName[] = "Enable glyph for Campbell";
const char kCampbellGlyphDescription[] = "Enables a Campbell glyph.";

const char kCampbellKeyName[] = "Key to enable glyph for Campbell";
const char kCampbellKeyDescription[] =
    "Secret key to enable glyph for Campbell";

const char kCaptureModeEducationName[] = "Enable Capture Mode Education";
const char kCaptureModeEducationDescription[] =
    "Enables the Capture Mode Education nudges and tutorials that inform users "
    "of the screenshot keyboard shortcut and the screen capture tool in the "
    "quick settings menu.";

const char kCaptureModeEducationBypassLimitsName[] =
    "Enable Capture Mode Education bypass limits";
const char kCaptureModeEducationBypassLimitsDescription[] =
    "Enables bypassing the 3 times / 24 hours show limit for Capture Mode "
    "Education nudges and tutorials, so they can be viewed repeatedly for "
    "testing purposes.";

const char kCaptureModeGifRecordingName[] =
    "Enable GIF recording in screen capture";
const char kCaptureModeGifRecordingDescription[] =
    "Enables the ability to record the screen into animated GIFs.";

const char kDeskButtonName[] = "Desk button in shelf";
const char kDeskButtonDescription[] =
    "Show a desk button that provides quick access to the desk menu in the "
    "shelf in clamshell mode when there is more than one desk.";

const char kCrosContentAdjustedRefreshRateName[] =
    "Content Adjusted Refresh Rate";
const char kCrosContentAdjustedRefreshRateDescription[] =
    "Allows the display to adjust the refresh rate in order to match content.";

const char kCrosSoulName[] = "CrOS SOUL";
const char kCrosSoulDescription[] = "Enable the CrOS SOUL feature.";

const char kCrosSoulGravediggerName[] = "CrOS SOUL Gravedigger";
const char kCrosSoulGravediggerDescription[] = "Use Gravedigger.";

const char kDesksTemplatesName[] = "Desk Templates";
const char kDesksTemplatesDescription[] =
    "Streamline workflows by saving a group of applications and windows as a "
    "launchable template in a new desk";

const char kPreferConstantFrameRateName[] = "Prefer Constant Frame Rate";
const char kPreferConstantFrameRateDescription[] =
    "Enables this flag to prefer using constant frame rate for camera when "
    "streaming";

const char kForceControlFaceAeName[] = "Force control face AE";
const char kForceControlFaceAeDescription[] =
    "Control this flag to force enable or disable face AE for camera";

const char kCellularBypassESimInstallationConnectivityCheckName[] =
    "Bypass eSIM installation connectivity check";
const char kCellularBypassESimInstallationConnectivityCheckDescription[] =
    "Bypass the non-cellular internet connectivity check during eSIM "
    "installation.";

const char kCellularUseSecondEuiccName[] = "Use second Euicc";
const char kCellularUseSecondEuiccDescription[] =
    "When enabled Cellular Setup and Settings UI will use the second available "
    "Euicc that's exposed by Hermes.";

const char kClipboardHistoryLongpressName[] =
    "Hold Ctrl+V to paste an item from clipboard history";
const char kClipboardHistoryLongpressDescription[] =
    "Enables an experimental behavior change where long-pressing Ctrl+V shows "
    "the clipboard history menu. If an item is selected to paste, it replaces "
    "the content initially pasted by Ctrl+V.";

const char kClipboardHistoryRefreshName[] = "Clipboard history refresh";
const char kClipboardHistoryRefreshDescription[] =
    "Enables the following updates to the clipboard history feature: a new "
    "educational nudge, refreshed menu UI, and a clipboard history submenu "
    "embedded in context menus.";

const char kClipboardHistoryUrlTitlesName[] =
    "Show page titles for copied URLs in the clipboard history menu";
const char kClipboardHistoryUrlTitlesDescription[] =
    "When clipboard-history-refresh is also enabled, this flag enables an "
    "annotation for copied URLs in the clipboard history menu: If the URL has "
    "been visited, its page title will appear as part of the URL's menu item.";

const char kCloudGamingDeviceName[] = "Enable cloud game search";
const char kCloudGamingDeviceDescription[] =
    "Enables cloud game search results in the launcher.";

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kCampaignsComponentUpdaterTestTagName[] = "Campaigns test tag";
const char kCampaignsComponentUpdaterTestTagDescription[] =
    "Tags used for component updater to selet Omaha cohort for Growth "
    "Campaigns.";
const char kCampaignsOverrideName[] = "Campaigns override";
const char kCampaignsOverrideDescription[] =
    "Base64 encoded Growth campaigns used for testing.";
#endif  // IS_CHROMEOS_ASH

const char kComponentUpdaterTestRequestName[] =
    "Enable the component updater check 'test-request' parameter";
const char kComponentUpdaterTestRequestDescription[] =
    "Enables the 'test-request' parameter for component updater check requests."
    " Overrides any other component updater check request parameters that may "
    "have been specified.";

const char kEnableServiceWorkersForChromeUntrustedName[] =
    "Enable chrome-untrusted:// Service Workers";
const char kEnableServiceWorkersForChromeUntrustedDescription[] =
    "When enabled, allows chrome-untrusted:// WebUIs to use service workers.";

const char kEnterpriseReportingUIName[] =
    "Enable chrome://enterprise-reporting";
const char kEnterpriseReportingUIDescription[] =
    "When enabled, allows for chrome://enterprise-reporting to be visited";

const char kPermissiveUsbPassthroughName[] =
    "Enable more permissive passthrough for USB Devices";
const char kPermissiveUsbPassthroughDescription[] =
    "When enabled, applies more permissive rules passthrough of USB devices.";

const char kCameraAngleBackendName[] = "Camera service ANGLE backend";
const char kCameraAngleBackendDescription[] =
    "When enabled, uses ANGLE as the GL driver in the camera service.";

const char kCameraSuperResOverrideName[] =
    "Camera super resolution control override";
const char kCameraSuperResOverrideDescription[] =
    "Overrides the default to enable or disable the single frame super "
    "resolution feature.";

const char kChromeboxUsbPassthroughRestrictionsName[] =
    "Limit primary mice/keyboards from USB passthrough on chromeboxes";
const char kChromeboxUsbPassthroughRestrictionsDescription[] =
    "When enabled, attempts to prevent primary mice/keyboard from being passed "
    "through to guest environments on chromebox-style devices.  If you have "
    "issues with passing through a USB peripheral on a chromebox, you can "
    "try disabling this feature.";

const char kDisableBruschettaInstallChecksName[] =
    "Disable Bruschetta Installer Checks";
const char kDisableBruschettaInstallChecksDescription[] =
    "Disables the built-in checks the Bruschetta installer performs before "
    "running the install process.";

const char kCrostiniContainerInstallName[] =
    "Debian version for new Crostini containers";
const char kCrostiniContainerInstallDescription[] =
    "New Crostini containers will use this debian version";

const char kCrostiniGpuSupportName[] = "Crostini GPU Support";
const char kCrostiniGpuSupportDescription[] = "Enable Crostini GPU support.";

const char kCrostiniResetLxdDbName[] = "Crostini Reset LXD DB on launch";
const char kCrostiniResetLxdDbDescription[] =
    "Recreates the LXD database every time we launch it";

const char kCrostiniMultiContainerName[] = "Allow multiple Crostini containers";
const char kCrostiniMultiContainerDescription[] =
    "Experimental UI for creating and managing multiple Crostini containers";

const char kCrostiniQtImeSupportName[] =
    "Crostini IME support for Qt applications";
const char kCrostiniQtImeSupportDescription[] =
    "Experimental support for IMEs (excluding VK) in Crostini for applications "
    "built with Qt.";

const char kCrostiniVirtualKeyboardSupportName[] =
    "Crostini Virtual Keyboard Support";
const char kCrostiniVirtualKeyboardSupportDescription[] =
    "Experimental support for the Virtual Keyboard on Crostini.";

const char kCameraAppDigitalZoomName[] = "Camera App Digital Zoom";
const char kCameraAppDigitalZoomDescription[] =
    "Enable this flag to use digital zoom in Camera app.";

const char kCameraAppPdfOcrName[] = "Camera App PDF OCR";
const char kCameraAppPdfOcrDescription[] =
    "Enable this flag to make text on PDFs saved from Camera app accessible.";

const char kCameraAppPreviewOcrName[] = "Camera App Preview OCR";
const char kCameraAppPreviewOcrDescription[] =
    "Enable this flag to use OCR features on preview in Camera app.";

const char kConchName[] = "Conch feature";
const char kConchDescription[] = "Enable Conch on ChromeOS.";

const char kConchFeatureKeyName[] = "Feature key for Conch";
const char kConchFeatureKeyDescription[] =
    "Feature key to use the Conch on ChromeOS.";

const char kConchSystemAudioFromMicName[] = "System audio capture for Conch";
const char kConchSystemAudioFromMicDescription[] =
    "Capture system audio from microphone for Conch on ChromeOS.";

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kDemoModeComponentUpdaterTestTagName[] = "Demo Mode test tag";
const char kDemoModeComponentUpdaterTestTagDescription[] =
    "Tags used for component updater to select Omaha cohort for Demo Mode.";
#endif  // IS_CHROMEOS_ASH

const char kDeskProfilesName[] = "Desk profiles";
const char kDeskProfilesDescription[] =
    "Enable association of lacros profiles with desks.";

const char kDisableCameraFrameRotationAtSourceName[] =
    "Disable camera frame rotation at source";
const char kDisableCameraFrameRotationAtSourceDescription[] =
    "Disable camera frame rotation to the upright display orientation in the "
    "video capture device";

const char kDisableCancelAllTouchesName[] = "Disable CancelAllTouches()";
const char kDisableCancelAllTouchesDescription[] =
    "If enabled, a canceled touch will not force all other touches to be "
    "canceled.";

const char kDisableExplicitDmaFencesName[] = "Disable explicit dma-fences";
const char kDisableExplicitDmaFencesDescription[] =
    "Always rely on implicit syncrhonization between GPU and display "
    "controller instead of using dma-fences explcitily when available.";

const char kDisplayAlignmentAssistanceName[] =
    "Enable Display Alignment Assistance";
const char kDisplayAlignmentAssistanceDescription[] =
    "Show indicators on shared edges of the displays when user is "
    "attempting to move their mouse over to another display. Show preview "
    "indicators when the user is moving a display in display layouts.";

const char kEnableLibinputToHandleTouchpadName[] =
    "Enable libinput to handle touchpad.";
const char kEnableLibinputToHandleTouchpadDescription[] =
    "Use libinput instead of the gestures library to handle touchpad."
    "Libgesures works very well on modern devices but fails on legacy"
    "devices. Use libinput if an input device doesn't work or is not working"
    "well.";

const char kEnableFakeKeyboardHeuristicName[] =
    "Enable Fake Keyboard Heuristic";
const char kEnableFakeKeyboardHeuristicDescription[] =
    "Enable heuristic to prevent non-keyboard devices from pretending "
    "to be keyboards. Primarily assists in preventing the virtual keyboard "
    "from being disabled unintentionally.";

const char kEnableFakeMouseHeuristicName[] = "Enable Fake Mouse Heuristic";
const char kEnableFakeMouseHeuristicDescription[] =
    "Enable heuristic to prevent non-mouse devices from pretending "
    "to be mice. Primarily assists in preventing fake entries "
    "appearing in the input settings menu.";

const char kEnableRuntimeCountersTelemetryName[] =
    "Enable Runtime Counters Telemetry";
const char kEnableRuntimeCountersTelemetryDescription[] =
    "Allow admins to collect runtime counters telemetry (Intel Gen 14+ only).";

const char kFastPairDebugMetadataName[] = "Enable Fast Pair Debug Metadata";
const char kFastPairDebugMetadataDescription[] =
    "Enables Fast Pair to use Debug metadata when checking device "
    "advertisements, allowing notifications to pop up for debug-mode only "
    "devices.";

const char kFaceRetouchOverrideName[] =
    "Enable face retouch using the relighting button in the VC panel";
const char kFaceRetouchOverrideDescription[] =
    "Enables or disables the face retouch feature using the relighting button "
    "in the VC panel.";

const char kFastPairHandshakeLongTermRefactorName[] =
    "Enable Fast Pair Handshake Long Term Refactor";
const char kFastPairHandshakeLongTermRefactorDescription[] =
    "Enables long term refactored handshake logic for Google Fast Pair "
    "service.";

const char kFastPairHIDName[] = "Enable Fast Pair HID";
const char kFastPairHIDDescription[] =
    "Enables prototype support for Fast Pair HID (non-keyboard).";

const char kFastPairKeyboardsName[] = "Enable Fast Pair Keyboards";
const char kFastPairKeyboardsDescription[] =
    "Enables prototype support for Fast Pair for keyboards.";

const char kFastPairPwaCompanionName[] = "Enable Fast Pair Web Companion";
const char kFastPairPwaCompanionDescription[] =
    "Enables Fast Pair Web Companion link after device pairing.";

const char kFastPairDevicesBluetoothSettingsName[] =
    "Enable Fast Pair Devices in Bluetooth Settings";
const char kFastPairDevicesBluetoothSettingsDescription[] =
    "Enables the Fast Pair Bluetooth Settings page to display a list of the "
    "user's devices available for Subsequent Pairing.";

const char kFrameSinkDesktopCapturerInCrdName[] =
    "Enable FrameSinkDesktopCapturer in CRD";
const char kFrameSinkDesktopCapturerInCrdDescription[] =
    "Enables the use of FrameSinkDesktopCapturer in the video streaming for "
    "CRD, "
    "replacing the use of AuraDesktopCapturer";

const char kUseHDRTransferFunctionName[] =
    "Monitor/Display HDR transfer function";
const char kUseHDRTransferFunctionDescription[] =
    "Allows using the HDR transfer functions of any connected monitor that "
    "supports it";

const char kEnableExternalDisplayHdr10Name[] =
    "Enable HDR10 support on external monitors";
const char kEnableExternalDisplayHdr10Description[] =
    "Allows using HDR10 mode on any external monitor that supports it";

const char kDoubleTapToZoomInTabletModeName[] =
    "Double-tap to zoom in tablet mode";
const char kDoubleTapToZoomInTabletModeDescription[] =
    "If Enabled, double tapping in webpages while in tablet mode will zoom the "
    "page.";

const char kQuickSettingsPWANotificationsName[] =
    "Enable setting of PWA notification permissions in quick settings ";
const char kQuickSettingsPWANotificationsDescription[] =
    "Replace website notification permissions with PWA notification "
    "permissions in the quick settings menu. Website notification permissions "
    "settings will be migrated to the lacros - chrome browser.";

const char kDriveFsMirroringName[] = "Enable local to Drive mirror sync";
const char kDriveFsMirroringDescription[] =
    "Enable mirror sync between local files and Google Drive";

const char kDriveFsShowCSEFilesName[] = "Enable listing of CSE files";
const char kDriveFsShowCSEFilesDescription[] =
    "Enable listing of CSE files in DriveFS, which will result in these files "
    "being visible in the Files App's Google Drive item.";

const char kEnableBackgroundBlurName[] = "Enable background blur.";
const char kEnableBackgroundBlurDescription[] =
    "Enables background blur for the Launcher, Shelf, Unified System Tray etc.";

const char kEnableBrightnessControlInSettingsName[] =
    "Enable brightness controls in Settings";
const char kEnableBrightnessControlInSettingsDescription[] =
    "Enables brightness slider and auto-brightness toggle for internal display "
    "in Settings";

const char kEnableDisplayPerformanceModeName[] =
    "Enable Display Performance Mode";
const char kEnableDisplayPerformanceModeDescription[] =
    "This option enables toggling different display features based on user "
    "setting and power state";

const char kDisableDnsProxyName[] = "Disable DNS proxy service for ChromeOS";
const char kDisableDnsProxyDescription[] =
    "Turns off DNS proxying and SecureDNS for ChromeOS (only). Does not impact "
    "Chrome browser.";

const char kDisconnectWiFiOnEthernetConnectedName[] =
    "Disconnect WiFi on Ethernet";
const char kDisconnectWiFiOnEthernetConnectedDescription[] =
    "Automatically disconnect WiFi and prevent it from auto connecting when "
    "the device gets an Ethernet connection. User are still allowed to connect "
    "to WiFi manually.";

const char kEnableRFC8925Name[] =
    "Enable RFC8925 (prefer IPv6-only on IPv6-only-capable network)";
const char kEnableRFC8925Description[] =
    "Let ChromeOS DHCPv4 client voluntarily drop DHCPv4 lease and prefer to"
    "operate IPv6-only, if the network is also IPv6-only capable.";

const char kEnableEdidBasedDisplayIdsName[] = "Enable EDID-based display IDs";
const char kEnableEdidBasedDisplayIdsDescription[] =
    "When enabled, a display's ID will be produced by hashing certain values "
    "in the display's EDID blob. EDID-based display IDs allow ChromeOS to "
    "consistently identify previously connected displays, regardless of the "
    "physical port they were connected to, and load user display layouts more "
    "accurately.";

const char kTiledDisplaySupportName[] = "Enable tile display support";
const char kTiledDisplaySupportDescription[] =
    "When enabled, tiled displays will be represented by a single display in "
    "ChromeOS, rather than each tile being a separate display.";

const char kEnableExternalKeyboardsInDiagnosticsAppName[] =
    "Enable external keyboards in the Diagnostics App";
const char kEnableExternalKeyboardsInDiagnosticsAppDescription[] =
    "Shows external keyboards in the Diagnostics App's input section. Requires "
    "#enable-input-in-diagnostics-app to be enabled.";

const char kEnableFastInkForSoftwareCursorName[] =
    "Enable fast ink for software cursor";
const char kEnableFastInkForSoftwareCursorDescription[] =
    "When enabled, software cursor will use fast ink to display cursor with "
    "minimal latency. "
    "However, it might also cause tearing artifacts.";

const char kEnableHostnameSettingName[] = "Enable setting the device hostname";
const char kEnableHostnameSettingDescription[] =
    "Enables the ability to set the ChromeOS hostname, the name of the device "
    "that is exposed to the local network";

const char kEnableGesturePropertiesDBusServiceName[] =
    "Enable gesture properties D-Bus service";
const char kEnableGesturePropertiesDBusServiceDescription[] =
    "Enable a D-Bus service for accessing gesture properties, which are used "
    "to configure input devices.";

const char kEnableGoogleAssistantDspName[] =
    "Enable Google Assistant with hardware-based hotword";
const char kEnableGoogleAssistantDspDescription[] =
    "Enable an experimental feature that uses hardware-based hotword detection "
    "for Assistant. Only a limited number of devices have this type of "
    "hardware support.";

const char kEnableGoogleAssistantStereoInputName[] =
    "Enable Google Assistant with stereo audio input";
const char kEnableGoogleAssistantStereoInputDescription[] =
    "Enable an experimental feature that uses stereo audio input for hotword "
    "and voice to text detection in Google Assistant.";

const char kEnableGoogleAssistantAecName[] = "Enable Google Assistant AEC";
const char kEnableGoogleAssistantAecDescription[] =
    "Enable an experimental feature that removes local feedback from audio "
    "input to help hotword and ASR when background audio is playing.";

const char kEnableInputEventLoggingName[] = "Enable input event logging";
const char kEnableInputEventLoggingDescription[] =
    "Enable detailed logging of input events from touchscreens, touchpads, and "
    "mice. These events include the locations of all touches as well as "
    "relative pointer movements, and so may disclose sensitive data. They "
    "will be included in feedback reports and system logs, so DO NOT ENTER "
    "SENSITIVE INFORMATION with this flag enabled.";

const char kEnableKeyboardBacklightControlInSettingsName[] =
    "Enable Keyboard Backlight Control In Settings.";
const char kEnableKeyboardBacklightControlInSettingsDescription[] =
    "Enable control of keyboard backlight directly from ChromeOS Settings";

const char kEnableKeyboardBacklightToggleName[] =
    "Enable Keyboard Backlight Toggle.";
const char kEnableKeyboardBacklightToggleDescription[] =
    "Enable toggling of the keyboard backlight. By "
    "default, this flag is enabled.";

const char kEnableKeyboardRewriterFixName[] = "Use new Keyboard Rewriter.";
const char kEnableKeyboardRewriterFixDescription[] =
    "Enable new Keyboard Rewriter.";

const char kEnableNeuralPalmAdaptiveHoldName[] = "Palm Rejection Adaptive Hold";
const char kEnableNeuralPalmAdaptiveHoldDescription[] =
    "Enable adaptive hold in palm rejection.  Not compatible with all devices.";

const char kEnableHeatmapPalmDetectionName[] = "Enable Heatmap Palm Detection";
const char kEnableHeatmapPalmDetectionDescription[] =
    "Experimental: Enable Heatmap Palm detection. Not compatible with all "
    "devices.";

const char kEnableNeuralStylusPalmRejectionName[] =
    "Enable Neural Palm Detection";
const char kEnableNeuralStylusPalmRejectionDescription[] =
    "Experimental: Enable Neural Palm detection. Not compatible with all "
    "devices.";

const char kEnablePalmSuppressionName[] =
    "Enable Palm Suppression with Stylus.";
const char kEnablePalmSuppressionDescription[] =
    "If enabled, suppresses touch when a stylus is on a touchscreen.";

const char kEnableEdgeDetectionName[] = "Enable Edge Detection.";
const char kEnableEdgeDetectionDescription[] =
    "If enabled, suppresses edge touch based on sensors' info.";

const char kEnableFastTouchpadClickName[] = "Enable Fast Touchpad Click";
const char kEnableFastTouchpadClickDescription[] =
    "If enabled, reduce the time after touchpad click before cursor can move.";

const char kEnableRemoveStalePolicyPinnedAppsFromShelfName[] =
    "Enable removing stale policy-pinned apps from shelf.";
const char kEnableRemoveStalePolicyPinnedAppsFromShelfDescription[] =
    "If enabled, allows the system to remove apps that were once pinned to the "
    "shelf by PinnedLauncherApps policy but are no longer listed in it.";

const char kEnableSeamlessRefreshRateSwitchingName[] =
    "Seamless Refresh Rate Switching";
const char kEnableSeamlessRefreshRateSwitchingDescription[] =
    "This option enables seamlessly changing the refresh rate based on power "
    "state on devices with supported hardware and drivers.";

const char kEnableTouchpadsInDiagnosticsAppName[] =
    "Enable touchpad cards in the Diagnostics App";
const char kEnableTouchpadsInDiagnosticsAppDescription[] =
    "Shows touchpad cards in the Diagnostics App's input section. Requires "
    "#enable-input-in-diagnostics-app to be enabled.";

const char kEnableTouchscreensInDiagnosticsAppName[] =
    "Enable touchscreen cards in the Diagnostics App";
const char kEnableTouchscreensInDiagnosticsAppDescription[] =
    "Shows touchscreen cards in the Diagnostics App's input section. Requires "
    "#enable-input-in-diagnostics-app to be enabled.";

const char kEnableWifiQosName[] = "Enable WiFi QoS";
const char kEnableWifiQosDescription[] =
    "If enabled the system will start automatic prioritization of egress "
    "traffic with WiFi QoS/WMM.";

const char kEnableWifiQosEnterpriseName[] = "Enable WiFi QoS enterprise";
const char kEnableWifiQosEnterpriseDescription[] =
    "If enabled the system will start automatic prioritization of egress "
    "traffic with WiFi QoS/WMM. This flag only affects Enterprise enrolled "
    "devices. Requires #enable-wifi-qos to be enabled.";

const char kPanelSelfRefresh2Name[] = "Enable Panel Self Refresh 2";
const char kPanelSelfRefresh2Description[] =
    "Enable Panel Self Refresh 2/Selective-Update where supported. "
    "Allows the display driver to only update regions of the screen that have "
    "damage.";

const char kEnableVariableRefreshRateName[] = "Enable Variable Refresh Rate";
const char kEnableVariableRefreshRateDescription[] =
    "Enable the variable refresh rate (Adaptive Sync) setting for capable "
    "displays.";

const char kEapGtcWifiAuthenticationName[] = "EAP-GTC WiFi Authentication";
const char kEapGtcWifiAuthenticationDescription[] =
    "Allows configuration of WiFi networks using EAP-GTC authentication";

const char kEcheSWAName[] = "Enable Eche feature";
const char kEcheSWADescription[] = "This is the main flag for enabling Eche.";

const char kEcheLauncherName[] = "Enable the Eche launcher";
const char kEcheLauncherDescription[] =
    "Enables the launcher for all apps for Eche.";

const char kEcheLauncherListViewName[] = "Enable Eche launcher list view";
const char kEcheLauncherListViewDescription[] =
    "Convert Eche launcher from grid view to list view";

const char kEcheLauncherIconsInMoreAppsButtonName[] =
    "Enable app icons in the Eche launcher more apps button";
const char kEcheLauncherIconsInMoreAppsButtonDescription[] =
    "Show app icons in the Eche launcher more apps button";

const char kEcheSWADebugModeName[] = "Enable Eche Debug Mode";
const char kEcheSWADebugModeDescription[] =
    "Save console logs of Eche in the system log";

const char kEcheSWAMeasureLatencyName[] = "Measure Eche E2E Latency";
const char kEcheSWAMeasureLatencyDescription[] =
    "Measure Eche E2E Latency and print all E2E latency logs of Eche in "
    "Console";

const char kEcheSWASendStartSignalingName[] =
    "Enable Eche Send Start Signaling";
const char kEcheSWASendStartSignalingDescription[] =
    "Allows sending start signaling action to establish Eche's WebRTC "
    "connection";

const char kEcheSWADisableStunServerName[] = "Disable Eche STUN server";
const char kEcheSWADisableStunServerDescription[] =
    "Allows disabling the stun servers when establishing a WebRTC connection "
    "to Eche";

const char kEcheSWACheckAndroidNetworkInfoName[] = "Check Android network info";
const char kEcheSWACheckAndroidNetworkInfoDescription[] =
    "Allows CrOS to analyze Android network information to provide more "
    "context on connection errors";

const char kEcheSWAProcessAndroidAccessibilityTreeName[] =
    "Process Android Application Accessibility Tree";
const char kEcheSWAProcessAndroidAccessibilityTreeDescription[] =
    "Allows CrOS to process the Android accessibility tree information of the "
    "currently streaming app.";

const char kEnableNotificationImageDragName[] =
    "Enable notification image drag";
const char kEnableNotificationImageDragDescription[] =
    "Enable users to drag the image shown on the notification and drop it to "
    "directly paste or share";

const char kEnableOAuthIppName[] =
    "Enable OAuth when printing via the IPP protocol";
const char kEnableOAuthIppDescription[] =
    "Enable OAuth when printing via the IPP protocol";

const char kEnableOngoingProcessesName[] = "Enable Ongoing Processes";
const char kEnableOngoingProcessesDescription[] =
    "Enables use of the new PinnedNotificationView for all ash pinned "
    "notifications, which are now referred to as Ongoing Processes";

const char kEnforceAshExtensionKeeplistName[] =
    "Enforce Ash extension keeplist";
const char kEnforceAshExtensionKeeplistDescription[] =
    "Enforce the Ash extension keeplist. Only the extensions and Chrome apps on"
    " the keeplist are enabled in Ash.";

const char kEnterOverviewFromWallpaperName[] =
    "Enable entering overview from wallpaper";
const char kEnterOverviewFromWallpaperDescription[] =
    "Experimental feature. Enable entering overview by clicking wallpaper with "
    "mouse click";

const char kEolResetDismissedPrefsName[] =
    "Reset end of life notification prefs";
const char kEolResetDismissedPrefsDescription[] =
    "Reset the end of life notification prefs to their default value, at the "
    "start of the user session. This is meant to make manual testing easier.";

const char kEolIncentiveName[] = "Enable end of life incentives";
const char kEolIncentiveDescription[] =
    "Allows end of life incentives to be shown within the system ui.";

const char kEventBasedLogUpload[] = "Enable event based log uploads";
const char kEventBasedLogUploadDescription[] =
    "Uploads relevant logs to device management server when unexpected events "
    "(e.g. crashes) occur on the device. The feature is guarded by "
    "LogUploadEnabled policy.";

const char kExcludeDisplayInMirrorModeName[] =
    "Enable feature to exclude a display in mirror mode.";
const char kExcludeDisplayInMirrorModeDescription[] =
    "Show toggles in Display Settings to exclude a display in mirror mode.";

const char kExoGamepadVibrationName[] = "Gamepad Vibration for Exo Clients";
const char kExoGamepadVibrationDescription[] =
    "Allow Exo clients like Android to request vibration events for gamepads "
    "that support it.";

const char kExoOrdinalMotionName[] =
    "Raw (unaccelerated) motion for Linux applications";
const char kExoOrdinalMotionDescription[] =
    "Send unaccelerated values as raw motion events to linux applications.";

const char kExperimentalAccessibilityDictationContextCheckingName[] =
    "Experimental accessibility dictation using context checking.";
const char kExperimentalAccessibilityDictationContextCheckingDescription[] =
    "Enables experimental dictation context checking.";

const char kExperimentalAccessibilityGoogleTtsHighQualityVoicesName[] =
    "Experimental accessibility Google TTS High Quality Voices.";
const char kExperimentalAccessibilityGoogleTtsHighQualityVoicesDescription[] =
    "Enables downloading Google TTS High Quality Voices.";

const char kExperimentalAccessibilityManifestV3Name[] =
    "Changes accessibility features from extension manifest v2 to v3.";
const char kExperimentalAccessibilityManifestV3Description[] =
    "Experimental migration of accessibility features from extension manifest "
    "v2 to v3. Likely to break accessibility access while experimental.";

const char kExperimentalAccessibilitySwitchAccessTextName[] =
    "Enable enhanced Switch Access text input.";
const char kExperimentalAccessibilitySwitchAccessTextDescription[] =
    "Enable experimental or in-progress Switch Access features for improved "
    "text input";

const char kExposeOutOfProcessVideoDecodingToLacrosName[] =
    "Expose out-of-process video decoding (OOP-VD) to LaCrOS.";
const char kExposeOutOfProcessVideoDecodingToLacrosDescription[] =
    "Accept media.stable.mojom.StableVideoDecoderFactory connection requests "
    "from LaCrOS and host said factories in utility processes.";

const char kFileTransferEnterpriseConnectorName[] =
    "Enable Files Transfer Enterprise Connector.";
const char kFileTransferEnterpriseConnectorDescription[] =
    "Enable the File Transfer Enterprise Connector.";

const char kFileTransferEnterpriseConnectorUIName[] =
    "Enable UI for Files Transfer Enterprise Connector.";
const char kFileTransferEnterpriseConnectorUIDescription[] =
    "Enable the UI for the File Transfer Enterprise Connector.";

const char kFilesConflictDialogName[] = "Files app conflict dialog";
const char kFilesConflictDialogDescription[] =
    "When enabled, the conflict dialog will be shown during file transfers "
    "if a file entry in the transfer exists at the destination.";

const char kFilesKernelDriversName[] = "Prefer in-kernel filesystem drivers";
const char kFilesKernelDriversDescription[] =
    "Prefer the in-kernel drivers to the FUSE drivers for filesystems such as "
    "exFAT or NTFS. "
    "Enabling this flag allows the kernel drivers to be used on systems that "
    "support them. "
    "Disabling this flag ensures that the FUSE drivers are used.";

const char kFilesExtractArchiveName[] = "Extract archive in Files app";
const char kFilesExtractArchiveDescription[] =
    "Enable the simplified archive extraction feature in Files app";

const char kFilesLocalImageSearchName[] = "Search local images by query.";
const char kFilesLocalImageSearchDescription[] =
    "Enable searching local images by query.";

const char kFilesMaterializedViewsName[] = "Files app materialized views";
const char kFilesMaterializedViewsDescription[] =
    "Enable materialized views in Files App.";

const char kFilesSinglePartitionFormatName[] =
    "Enable Partitioning of Removable Disks.";
const char kFilesSinglePartitionFormatDescription[] =
    "Enable partitioning of removable disks into single partition.";

const char kFilesTrashDriveName[] = "Enable Files Trash for Drive.";
const char kFilesTrashDriveDescription[] =
    "Enable trash for Drive volume in Files App.";

const char kFileSystemProviderCloudFileSystemName[] =
    "Enable CloudFileSystem for FileSystemProvider extensions.";
const char kFileSystemProviderCloudFileSystemDescription[] =
    "Enable the ability for individual FileSystemProvider extensions to "
    "be serviced by a CloudFileSystem.";

const char kFileSystemProviderContentCacheName[] =
    "Enable content caching for FileSystemProvider extensions.";
const char kFileSystemProviderContentCacheDescription[] =
    "Enable the ability for individual FileSystemProvider extensions being "
    "serviced by CloudFileSystem to leverage a content cache.";

const char kFirmwareUpdateUIV2Name[] =
    "Enables the v2 version of the Firmware Updates app";
const char kFirmwareUpdateUIV2Description[] =
    "Enable the v2 version of the Firmware Updates App.";

const char kFirstPartyVietnameseInputName[] =
    "First party Vietnamese Input Method";
const char kFirstPartyVietnameseInputDescription[] =
    "Use first party input method for Vietnamese VNI and Telex";

const char kFocusFollowsCursorName[] = "Focus follows cursor";
const char kFocusFollowsCursorDescription[] =
    "Enable window focusing by moving the cursor.";

const char kFocusModeName[] = "Focus Mode on ChromeOS";
const char kFocusModeDescription[] =
    "Focus mode on the OS with timer, music, and task management "
    "functionalities.";

const char kFocusModeYTMName[] = "Focus Mode YTM on ChromeOS";
const char kFocusModeYTMDescription[] = "Focus mode YouTube Music integration.";

const char kForceReSyncDriveName[] = "Force resync drive";
const char kForceReSyncDriveDescription[] =
    "Enable Drive to forcibly resync office files.";

const char kFuseBoxDebugName[] = "Debugging UI for ChromeOS FuseBox service";
const char kFuseBoxDebugDescription[] =
    "Show additional debugging UI for ChromeOS FuseBox service.";

const char kGameDashboard[] = "Game Dashboard";
const char kGameDashboardDescription[] = "Enables the Game Dashboard.";

const char kGameDashboardGamepadSupport[] = "Game Dashboard gamepad support.";
const char kGameDashboardGamepadSupportDescription[] =
    "Enables gamepad support in game controls.";

const char kGameDashboardGamePWAs[] = "Game Dashboard Game PWAs";
const char kGameDashboardGamePWAsDescription[] =
    "Enables Game Dashboard for an additional set of game PWAs.";

const char kGameDashboardGamesInTest[] = "Game Dashboard Games In Test";
const char kGameDashboardGamesInTestDescription[] =
    "Enables Game Dashboard for a set of games being further evaluated.";

const char kGameDashboardUtilities[] = "Game Dashboard Utilities";
const char kGameDashboardUtilitiesDescription[] =
    "Enables utility features in the Game Dashboard.";

const char kGlanceablesTimeManagementClassroomStudentViewName[] =
    "Glanceables > Time Management > Classroom Student";
const char kGlanceablesTimeManagementClassroomStudentViewDescription[] =
    "Enables Google Classroom integration for students on the Time Management "
    "Glanceables surface (via Calendar entry point).";

const char kGlanceablesTimeManagementTasksViewName[] =
    "Glanceables > Time Management > Tasks";
const char kGlanceablesTimeManagementTasksViewDescription[] =
    "Enables Google Tasks integration on the Time Management Glanceables "
    "surface (via Calendar entry point).";

const char kHelpAppAppDetailPageName[] = "Help App app detail page";
const char kHelpAppAppDetailPageDescription[] =
    "If enabled, the Help app will render the App Detail Page and entry point.";

const char kHelpAppAppsListName[] = "Help App apps list";
const char kHelpAppAppsListDescription[] =
    "If enabled, the Help app will render the Apps List page and entry point.";

const char kHelpAppAutoTriggerInstallDialogName[] =
    "Help App Auto Trigger Install Dialog";
const char kHelpAppAutoTriggerInstallDialogDescription[] =
    "Enables the logic that auto triggers the install dialog during the web "
    "app install flow initiated from the Help App.";

const char kHelpAppHomePageAppArticlesName[] =
    "Help App home page app articles";
const char kHelpAppHomePageAppArticlesDescription[] =
    "If enabled, the home page of the Help App will show a section containing"
    "articles about apps.";

const char kHelpAppLauncherSearchName[] = "Help App launcher search";
const char kHelpAppLauncherSearchDescription[] =
    "Enables showing search results from the help app in the launcher.";

const char kHelpAppOnboardingRevampName[] = "Help App onboarding revamp";
const char kHelpAppOnboardingRevampDescription[] =
    "Enables a new onboarding flow in the Help App";

const char kHelpAppOpensInsteadOfReleaseNotesNotificationName[] =
    "Help App opens instead of release notes notification";
const char kHelpAppOpensInsteadOfReleaseNotesNotificationDescription[] =
    "Enables opening the Help App's What's New page immediately instead of "
    "showing a notification to open the help app.";

const char kDiacriticsUseReplaceSurroundingTextName[] =
    "Use ReplaceSurroundingText API for longpress diacritics.";
const char kDiacriticsUseReplaceSurroundingTextDescription[] =
    "When longpress diacritics is enabled, use the ReplaceSurroundingText API "
    "for better app compatibility.";

const char kHoldingSpaceSuggestionsName[] = "Enable holding space suggestions";
const char kHoldingSpaceSuggestionsDescription[] =
    "Enables pinned file suggestions in holding space to help the user "
    "understand and discover the ability to pin.";

const char kHotspotName[] = "Hotspot";
const char kHotspotDescription[] =
    "Enables the Chromebook to share its cellular internet connection to other "
    "devices through WiFi. While this feature is under development, enabling "
    "this flag may cause your device's non-tethering traffic to use a "
    "tethering APN, which can result in carrier limits or fees.";

const char kImeAssistMultiWordName[] =
    "Enable assistive multi word suggestions";
const char kImeAssistMultiWordDescription[] =
    "Enable assistive multi word suggestions for native IME";

const char kImeFstDecoderParamsUpdateName[] =
    "Enable FST Decoder parameters update";
const char kImeFstDecoderParamsUpdateDescription[] =
    "Enable updated parameters for the FST decoder.";

const char kImeKoreanModeSwitchDebugName[] =
    "Korean input method's mode switch debug";
const char kImeKoreanModeSwitchDebugDescription[] =
    "Enables debug info UI for Korean input method's internal-mode switch";

const char kImeKoreanOnlyModeSwitchOnRightAltName[] =
    "Only internal-mode switch on right-Alt in Korean input method";
const char kImeKoreanOnlyModeSwitchOnRightAltDescription[] =
    "When enabled and in Korean input method, right-Alt key location solely "
    "toggles internal Korean/English mode, without Alt modifier functionality";

const char kImeSwitchCheckConnectionStatusName[] =
    "Enable Ime switching using global boolean";
const char kImeSwitchCheckConnectionStatusDescription[] =
    "When enabled and swapping between input methods, this prevents a race "
    "condition.";

const char kIppFirstSetupForUsbPrintersName[] =
    "Try to setup USB printers with IPP first";
const char kIppFirstSetupForUsbPrintersDescription[] =
    "When enabled, ChromeOS attempts to setup USB printers via IPP Everywhere "
    "first, then falls back to PPD-based setup.";

const char kImeSystemEmojiPickerGIFSupportName[] =
    "System emoji picker gif support";
const char kImeSystemEmojiPickerGIFSupportDescription[] =
    "Emoji picker gif support allows users to select gifs to input.";

const char kImeSystemEmojiPickerJellySupportName[] =
    "Enable jelly colors for the System Emoji Picker";
const char kImeSystemEmojiPickerJellySupportDescription[] =
    "Enable jelly colors for the System Emoji Picker. Requires "
    "jelly-colors flag to be enabled.";

const char kImeSystemEmojiPickerMojoSearchName[] =
    "Enable mojo search for the System Emoji Picker";
const char kImeSystemEmojiPickerMojoSearchDescription[] =
    "Enable mojo search for the System Emoji Picker.";

const char kImeSystemEmojiPickerVariantGroupingName[] =
    "System emoji picker global variant grouping";
const char kImeSystemEmojiPickerVariantGroupingDescription[] =
    "Emoji picker global variant grouping syncs skin tone and gender "
    "preferences across emojis in each group.";

const char kImeUsEnglishModelUpdateName[] =
    "Enable US English IME model update";
const char kImeUsEnglishModelUpdateDescription[] =
    "Enable updated US English IME language models for native IME";

const char kJellyColorsName[] = "Jelly Colors";
const char kJellyColorsDescription[] = "Enable Jelly coloring";

const char kCrosComponentsName[] = "Cros Components";
const char kCrosComponentsDescription[] =
    "Enable cros-component UI elements, replacing other elements.";

const char kLacrosAvailabilityIgnoreName[] =
    "Ignore lacros-availability policy";
const char kLacrosAvailabilityIgnoreDescription[] =
    "Makes the lacros-availability policy have no effect on Google-internal "
    "accounts. Instead, Lacros availability will be controlled by experiment "
    "and/or user flags for such accounts.";

const char kLacrosStabilityName[] = "Lacros stability";
const char kLacrosStabilityDescription[] = "Lacros update channel.";

const char kLacrosSelectionName[] = "Lacros selection";
const char kLacrosSelectionDescription[] =
    "Choosing between rootfs or stateful Lacros.";

const char kLacrosSelectionRootfsDescription[] = "Rootfs";
const char kLacrosSelectionStatefulDescription[] = "Stateful";

const char kLacrosSelectionPolicyIgnoreName[] =
    "Ignore lacros-selection policy";
const char kLacrosSelectionPolicyIgnoreDescription[] =
    "Makes the lacros-selection policy have no effect. Instead Lacros "
    "selection will be controlled by experiment and/or user flags.";

const char kLacrosWaylandLoggingName[] = "Lacros wayland logging";
const char kLacrosWaylandLoggingDescription[] =
    "Enables wayland logging for Lacros. This generates a significant amount "
    "of logs on disk. Logs are cleared after two restarts.";

const char kLacrosProfileMigrationForceOffName[] = "Disable profile migration";
const char kLacrosProfileMigrationForceOffDescription[] =
    "Disables lacros profile migration. Lacros profile migration is being "
    "rolled out to internal users first. Once lacros profile migration becomes "
    "available to the user, the completion of profile migration becomes a "
    "requirement to use lacros i.e. if profile migration gets rolled out to "
    "the user and the migration fails, then lacros becomes unavailable until "
    "the migration is completed. By enabling this flag, even if profile "
    "migration is rolled out to the user, the migration will not run and the "
    "user can continue to use lacros without profile migration.";

const char kLacrosProfileBackwardMigrationName[] =
    "Trigger Lacros profile backward migration";
const char kLacrosProfileBackwardMigrationDescription[] =
    "Trigger data migration back from the Lacros profile directory to the Ash "
    "profile directory on next restart. Set this flag only together with "
    "disabling the Lacros availability flag, otherwise it has no effect.";

const char kLanguagePacksInSettingsName[] = "Language Packs in Settings";
const char kLanguagePacksInSettingsDescription[] =
    "Enables the UI and logic to manage Language Packs in Settings. This is "
    "used for languages and input methods.";

const char kUseMlServiceForNonLongformHandwritingOnAllBoardsName[] =
    "Use ML Service for non-Longform handwriting on all boards";
const char kUseMlServiceForNonLongformHandwritingOnAllBoardsDescription[] =
    "Use ML Service (and DLC Language Packs) for non-Longform handwriting in "
    "Chrome OS 1P Virtual Keyboard on all boards. When this flag is OFF, such "
    "usage exists on certain boards only.";

const char kLauncherContinueSectionWithRecentsName[] =
    "Launcher continue section with recent drive files";
const char kLauncherContinueSectionWithRecentsDescription[] =
    "Adds Google Drive file suggestions based on users' recent activity to "
    "\"Continue where you left off\" section in Launcher.";

const char kLauncherItemSuggestName[] = "Launcher ItemSuggest";
const char kLauncherItemSuggestDescription[] =
    "Allows configuration of experiment parameters for ItemSuggest in the "
    "launcher.";

const char kLimitShelfItemsToActiveDeskName[] =
    "Limit Shelf items to active desk";
const char kLimitShelfItemsToActiveDeskDescription[] =
    "Limits items on the shelf to the ones associated with windows on the "
    "active desk";

const char kListAllDisplayModesName[] = "List all display modes";
const char kListAllDisplayModesDescription[] =
    "Enables listing all external displays' modes in the display settings.";

const char kEnableHardwareMirrorModeName[] = "Enable Hardware Mirror Mode";
const char kEnableHardwareMirrorModeDescription[] =
    "Enables hardware support when multiple displays are set to mirror mode.";

const char kHindiInscriptLayoutName[] = "Hindi Inscript Layout on CrOS";
const char kHindiInscriptLayoutDescription[] =
    "Enables Hindi Inscript Layout on ChromeOS.";

const char kLockScreenNotificationName[] = "Lock screen notification";
const char kLockScreenNotificationDescription[] =
    "Enable notifications on the lock screen.";

const char kMahiName[] = "Mahi feature";
const char kMahiDescription[] = "Enable Mahi feature on ChromeOS.";

const char kMahiDebuggingName[] = "Mahi Debugging";
const char kMahiDebuggingDescription[] = "Enable debugging for mahi.";

const char kMahiFeatureKeyName[] = "Feature key for Mahi feature";
const char kMahiFeatureKeyDescription[] =
    "Feature key to use the Mahi feature on ChromeOS.";

const char kMediaAppPdfMahiName[] = "Mahi feature on Media App PDF";
const char kMediaAppPdfMahiDescription[] =
    "Enable Mahi feature on PDF files in Gallery app.";

const char kMeteredShowToggleName[] = "Show Metered Toggle";
const char kMeteredShowToggleDescription[] =
    "Shows a Metered toggle in the Network settings UI for WiFI and Cellular. "
    "The toggle allows users to set whether a network should be considered "
    "metered for purposes of bandwith usage (e.g. for automatic updates).";

const char kMicrophoneMuteSwitchDeviceName[] = "Microphone Mute Switch Device";
const char kMicrophoneMuteSwitchDeviceDescription[] =
    "Support for detecting the state of hardware microphone mute toggle. Only "
    "effective on devices that have a microphone mute toggle. Enabling the "
    "flag does not affect the toggle functionality, it only affects how the "
    "System UI handles the mute toggle state.";

const char kMultiCalendarSupportName[] =
    "Multi-Calendar Support in Quick Settings";
const char kMultiCalendarSupportDescription[] =
    "Enables the Quick Settings Calendar to display Google Calendar events for "
    "up to 10 of the user's calendars.";

const char kMultiZoneRgbKeyboardName[] =
    "Enable multi-zone RGB keyboard customization";
const char kMultiZoneRgbKeyboardDescription[] =
    "Enable multi-zone RGB keyboard customization on supported devices.";

const char kNotificationWidthIncreaseName[] =
    "Notification Width Increase Feature";
const char kNotificationWidthIncreaseDescription[] =
    "Enables increased notification width for pop-up notifications and "
    "notifications in the message center.";

const char kEnableNearbyBleV2Name[] = "Nearby BLE v2";
const char kEnableNearbyBleV2Description[] = "Enables Nearby BLE v2.";

const char kEnableNearbyBleV2ExtendedAdvertisingName[] =
    "Nearby BLE v2 Extended Advertising";
const char kEnableNearbyBleV2ExtendedAdvertisingDescription[] =
    "Enables extended advertising functionality over BLE when using Nearby BLE "
    "v2.";

const char kEnableNearbyBleV2GattServerName[] = "Nearby BLE v2 GATT Server";
const char kEnableNearbyBleV2GattServerDescription[] =
    "Enables GATT server functionality over BLE when using Nearby BLE "
    "v2.";

const char kEnableNearbyBluetoothClassicAdvertisingName[] =
    "Nearby Bluetooth Classic Advertising";
const char kEnableNearbyBluetoothClassicAdvertisingDescription[] =
    "Enables Nearby advertising over Bluetooth Classic.";

const char kEnableNearbyMdnsName[] = "Nearby mDNS Discovery";
const char kEnableNearbyMdnsDescription[] =
    "Enables Nearby discovery over mDNS.";

const char kEnableNearbyWebRtcName[] = "Nearby WebRTC";
const char kEnableNearbyWebRtcDescription[] =
    "Enables Nearby transfers over WebRTC.";

const char kEnableNearbyWifiDirectName[] = "Nearby WiFi Direct";
const char kEnableNearbyWifiDirectDescription[] =
    "Enables Nearby transfers over WiFi Direct.";

const char kEnableNearbyWifiLanName[] = "Nearby WiFi LAN";
const char kEnableNearbyWifiLanDescription[] =
    "Enables Nearby transfers over WiFi LAN.";

const char kNearbyPresenceName[] = "Nearby Presence";
const char kNearbyPresenceDescription[] =
    "Enables Nearby Presence for scanning and discovery of nearby devices.";

const char kNotificationsIgnoreRequireInteractionName[] =
    "Notifications always timeout";
const char kNotificationsIgnoreRequireInteractionDescription[] =
    "Always timeout notifications, even if they are set with "
    "requireInteraction.";

const char kOnDeviceAppControlsName[] = "On-device controls for apps";
const char kOnDeviceAppControlsDescription[] =
    "Enables the on-device controls UI for blocking apps.";

const char kOrcaKeyName[] = "Secret key for Orca feature";
const char kOrcaKeyDescription[] =
    "Secret key for Orca feature. Incorrect values will cause chrome crashes.";

const char kOsFeedbackDialogName[] =
    "OS Feedback dialog on OOBE and login screen";
const char kOsFeedbackDialogDescription[] =
    "Enable the OS Feedback dialog on OOBE and login screen.";

const char kOsSettingsRevampWayfindingName[] =
    "ChromeOS Settings Revamp: Wayfinding Improvements";
const char kOsSettingsRevampWayfindingDescription[] =
    "Enables wayfinding improvements in the ChromeOS Settings UI.";

const char kPcieBillboardNotificationName[] = "Pcie billboard notification";
const char kPcieBillboardNotificationDescription[] =
    "Enable Pcie peripheral billboard notification.";

const char kPerformantSplitViewResizing[] = "Performant Split View Resizing";
const char kPerformantSplitViewResizingDescription[] =
    "If enabled, windows may be moved instead of scaled when resizing split "
    "view in tablet mode.";

const char kPhoneHubCallNotificationName[] =
    "Incoming call notification in Phone Hub";
const char kPhoneHubCallNotificationDescription[] =
    "Enables the incoming/ongoing call feature in Phone Hub.";

const char kPreferDcheckName[] = "Prefer DCHECK-enabled build";
const char kPreferDcheckDescription[] =
    "Use a DCHECK-enabled build when available.";

const char kPrintingPpdChannelName[] = "Printing PPD channel";
const char kPrintingPpdChannelDescription[] =
    "The channel from which PPD index "
    "is loaded when matching PPD files during printer setup.";

const char kPrintPreviewCrosAppName[] = "Enable ChromeOS print preview";
const char kPrintPreviewCrosAppDescription[] =
    "Enables ChromeOS print preview app.";

const char kProductivityLauncherName[] =
    "Productivity experiment: App Launcher";
const char kProductivityLauncherDescription[] =
    "To evaluate an enhanced Launcher experience that aims to improve app "
    "workflows by optimizing access to apps, app content, and app actions.";

const char kProductivityLauncherImageSearchName[] =
    "Productivity Launcher experiment: Launcher Image Search";
const char kProductivityLauncherImageSearchDescription[] =
    "To evaluate the viability of image search as part of Productivity "
    "Launcher Search.";

const char kProjectorAppDebugName[] = "Enable Projector app debug";
const char kProjectorAppDebugDescription[] =
    "Adds more informative error messages to the Projector app for debugging";

const char kProjectorGm3Name[] = "Enable Screencast GM3";
const char kProjectorGm3Description[] =
    "Adds updated styles and dynamic colors to the Screencast app.";

const char kProjectorServerSideSpeechRecognitionName[] =
    "Enable server side speech recognition for Projector";
const char kProjectorServerSideSpeechRecognitionDescription[] =
    "Adds server side speech recognition capability to Projector.";

const char kProjectorServerSideUsmName[] =
    "Enable USM for Projector server side speech recognition";
const char kProjectorServerSideUsmDescription[] =
    "Allows Screencast to ues the latest model for server side speech "
    "recognition.";

const char kReleaseNotesNotificationAllChannelsName[] =
    "Release Notes Notification All Channels";
const char kReleaseNotesNotificationAllChannelsDescription[] =
    "Enables the release notes notification for all ChromeOS channels";

const char kReleaseNotesNotificationAlwaysEligibleName[] =
    "Release Notes Notification always eligible";
const char kReleaseNotesNotificationAlwaysEligibleDescription[] =
    "Makes the release notes notification always appear regardless of channel, "
    "profile type, and whether or not the notification had already been shown "
    "this milestone. For testing.";

const char kRenderArcNotificationsByChromeName[] =
    "Render ARC notifications by ChromeOS";
const char kRenderArcNotificationsByChromeDescription[] =
    "Enables rendering ARC notifications using ChromeOS notification framework "
    "if supported";

const char kArcWindowPredictorName[] = "Enable ARC window predictor";
const char kArcWindowPredictorDescription[] =
    "Enables the window state and bounds predictor for ARC task windows";

const char kScalableIphDebugName[] = "Scalable Iph Debug";
const char kScalableIphDebugDescription[] =
    "Enables debug feature of Scalable Iph";

const char kSeaPenName[] = "SeaPen";
const char kSeaPenDescription[] = "Enable SeaPen Wallpaper";

const char kSeaPenEnterpriseName[] = "SeaPenEnterprise";
const char kSeaPenEnterpriseDescription[] =
    "Enable SeaPen Features for managed users";

const char kSealKeyName[] = "Secret key for Seal feature";
const char kSealKeyDescription[] = "Secret key for Seal feature.";

const char kShelfAutoHideSeparationName[] =
    "Enable separate shelf auto-hide preferences.";
const char kShelfAutoHideSeparationDescription[] =
    "Allows for the shelf's auto-hide preference to be specified separately "
    "for clamshell and tablet mode.";

const char kShimlessRMAAproUpdateRootfsName[] =
    "Enable APRO update from RootFS in Shimless RMA";
const char kShimlessRMAAproUpdateRootfsDescription[] =
    "Updates APRO from RootFS first in the Shimless RMA flow and fallbacks to "
    "USB drive when it fails.";

const char kShimlessRMAOsUpdateName[] = "Enable OS updates in shimless RMA";
const char kShimlessRMAOsUpdateDescription[] =
    "Turns on OS updating in Shimless RMA";

const char kSchedulerConfigurationName[] = "Scheduler Configuration";
const char kSchedulerConfigurationDescription[] =
    "Instructs the OS to use a specific scheduler configuration setting.";
const char kSchedulerConfigurationConservative[] =
    "Disables Hyper-Threading on relevant CPUs.";
const char kSchedulerConfigurationPerformance[] =
    "Enables Hyper-Threading on relevant CPUs.";

const char kSnapGroupsName[] = "Enable Snap Groups feature";
const char kSnapGroupsDescription[] =
    "Enables the ability to accelerate the window layout setup process and "
    "access the window layout as a group";

const char kMediaDynamicCgroupName[] = "Media Dynamic Cgroup";
const char kMediaDynamicCgroupDescription[] =
    "Dynamic Cgroup allows tasks from media workload to be consolidated on "
    "limited cpuset";

const char kMissiveStorageName[] = "Missive Daemon Storage Configuration";
const char kMissiveStorageDescription[] =
    "Provides missive daemon with custom storage configuration parameters";

const char kShowBluetoothDebugLogToggleName[] =
    "Show Bluetooth debug log toggle";
const char kShowBluetoothDebugLogToggleDescription[] =
    "Enables a toggle which can enable debug (i.e., verbose) logs for "
    "Bluetooth";

const char kShowTapsName[] = "Show taps";
const char kShowTapsDescription[] =
    "Draws a circle at each touch point, which makes touch points more obvious "
    "when projecting or mirroring the display. Similar to the Android OS "
    "developer option.";

const char kShowTouchHudName[] = "Show HUD for touch points";
const char kShowTouchHudDescription[] =
    "Shows a trail of colored dots for the last few touch points. Pressing "
    "Ctrl-Alt-I shows a heads-up display view in the top-left corner. Helps "
    "debug hardware issues that generate spurious touch events.";

const char kContinuousOverviewScrollAnimationName[] =
    "Makes the gesture for Overview continuous";
const char kContinuousOverviewScrollAnimationDescription[] =
    "When a user does the Overview gesture (3 finger swipe), smoothly animates "
    "the transition into Overview as the gesture is done. Allows for the user "
    "to scrub (move forward and backward) through Overview.";

const char kSparkyName[] = "Sparky feature";
const char kSparkyDescription[] = "Enable Sparky feature on ChromeOS.";

const char kSparkyFeatureKeyName[] = "Feature key for Sparky feature";
const char kSparkyFeatureKeyDescription[] =
    "Feature key to use the Sparky feature on ChromeOS.";

const char kSparkyServerUrlName[] = "Server Url for Sparky feature";
const char kSparkyServerUrlDescription[] =
    "The Url of the server which the Sparky feature will call on ChromeOS.";

const char kSpectreVariant2MitigationName[] = "Spectre variant 2 mitigation";
const char kSpectreVariant2MitigationDescription[] =
    "Controls whether Spectre variant 2 mitigation is enabled when "
    "bootstrapping the Seccomp BPF sandbox. Can be overridden by "
    "#force-spectre-variant2-mitigation.";

const char kSystemJapanesePhysicalTypingName[] =
    "Use system IME for Japanese typing";
const char kSystemJapanesePhysicalTypingDescription[] =
    "Use the system input engine instead of the Chrome extension for physical "
    "typing in Japanese. This also replaces the Japanese extension settings "
    "page with one built into the UI and migrates the data to a new location.";

const char kSupportF11AndF12ShortcutsName[] = "F11/F12 Shortcuts";
const char kSupportF11AndF12ShortcutsDescription[] =
    "Enables settings that "
    "allow users to use shortcuts to remap to the F11 and F12 keys in the "
    "Customize keyboard keys "
    "page.";

const char kTerminalDevName[] = "Terminal dev";
const char kTerminalDevDescription[] =
    "Enables Terminal System App to load from Downloads for developer testing. "
    "Only works in dev and canary channels.";

const char kTetherName[] = "Instant Tethering";
const char kTetherDescription[] =
    "Enables Instant Tethering. Instant Tethering allows your nearby Google "
    "phone to share its Internet connection with this device.";

const char kTilingWindowResizeName[] = "CrOS Labs - Tiling Window Resize";
const char kTilingWindowResizeDescription[] =
    "Enables tile-like resizing of windows.";

const char kTouchscreenCalibrationName[] =
    "Enable/disable touchscreen calibration option in material design settings";
const char kTouchscreenCalibrationDescription[] =
    "If enabled, the user can calibrate the touch screen displays in "
    "chrome://settings/display.";

const char kTouchscreenMappingName[] =
    "Enable/disable touchscreen mapping option in material design settings";
const char kTouchscreenMappingDescription[] =
    "If enabled, the user can map the touch screen display to the correct "
    "input device in chrome://settings/display.";

const char kTrafficCountersEnabledName[] = "Traffic counters enabled";
const char kTrafficCountersEnabledDescription[] =
    "If enabled, data usage will be visible in the Cellular Settings UI and "
    "traffic counters will be automatically reset if that setting is enabled.";

const char kTrafficCountersForWiFiTestingName[] =
    "Traffic counters enabled for WiFi networks";
const char kTrafficCountersForWiFiTestingDescription[] =
    "If enabeld, data usage will be visible in the Settings UI for WiFi "
    "networks";

const char kUploadOfficeToCloudName[] = "Enable Office files upload workflow.";
const char kUploadOfficeToCloudDescription[] =
    "Some file handlers for Microsoft Office files are only available on the "
    "the cloud. Enables the cloud upload workflow for Office file handling.";

const char kUpstreamTrustedReportsFirmwareName[] =
    "Enable firmware updates from upstream";
const char kUpstreamTrustedReportsFirmwareDescription[] =
    "Enables firmware updates for firmwares that have been uploaded to LVFS "
    "and have been tested with ChromeOS.";

const char kUseAnnotatedAccountIdName[] =
    "Use AccountId based mapping between User and BrowserContext";
const char kUseAnnotatedAccountIdDescription[] =
    "Uses AccountId annotated for BrowserContext to look up between ChromeOS "
    "User and BrowserContext, a.k.a. Profile.";

const char kUseFakeDeviceForMediaStreamName[] = "Use fake video capture device";
const char kUseFakeDeviceForMediaStreamDescription[] =
    "Forces Chrome to use a fake video capture device (a rolling pacman with a "
    "timestamp) instead of the system audio/video devices, for debugging "
    "purposes.";

const char kUseLegacyDHCPCDName[] = "Use legacy dhcpcd7 for IPv4";
const char kUseLegacyDHCPCDDescription[] =
    "Use legacy dhcpcd7 for IPv4 provisioning, otherwise the latest dhcpcd "
    "will be used. Note that IPv6 (DHCPv6-PD) will always use the latest "
    "dhcpcd.";

const char kUiDevToolsName[] = "Enable native UI inspection";
const char kUiDevToolsDescription[] =
    "Enables inspection of native UI elements. For local inspection use "
    "chrome://inspect#other";

const char kUiSlowAnimationsName[] = "Slow UI animations";
const char kUiSlowAnimationsDescription[] = "Makes all UI animations slow.";

const char kVcDlcUiName[] = "VC DLC UI";
const char kVcDlcUiDescription[] =
    "Enable UI for video conference effect toggle tiles in the video "
    "conference controls bubble that indicates when required DLC is "
    "downloading.";

const char kVirtualKeyboardName[] = "Virtual Keyboard";
const char kVirtualKeyboardDescription[] =
    "Always show virtual keyboard regardless of having a physical keyboard "
    "present";

const char kVirtualKeyboardDisabledName[] = "Disable Virtual Keyboard";
const char kVirtualKeyboardDisabledDescription[] =
    "Always disable virtual keyboard regardless of device mode. Workaround for "
    "virtual keyboard showing with some external keyboards.";

const char kVirtualKeyboardGlobalEmojiPreferencesName[] =
    "Virtual Keyboard Global Emoji Preferences";
const char kVirtualKeyboardGlobalEmojiPreferencesDescription[] =
    "Enable global preferences for skin tone and gender in the virtual "
    "keyboard emoji picker.";

const char kWakeOnWifiAllowedName[] = "Allow enabling wake on WiFi features";
const char kWakeOnWifiAllowedDescription[] =
    "Allows wake on WiFi features in shill to be enabled.";

const char kWelcomeExperienceName[] = "Welcome Experience";
const char kWelcomeExperienceDescription[] =
    "Enables a new Welcome Experience for first-time peripheral connections.";

const char kWelcomeExperienceTestUnsupportedDevicesName[] =
    "Welcome Experience test unsupported devices";
const char kWelcomeExperienceTestUnsupportedDevicesDescription[] =
    "kWelcomeExperienceTestUnsupportedDevices enables the new device Welcome "
    "Experience to be tested on external devices that are not officially "
    "supported. When enabled, users will be able to initiate and complete "
    "the enhanced Welcome Experience flow using these unsupported external "
    "devices. This flag is intended for testing purposes and should be "
    "disabled in production environments.";

const char kWelcomeTourName[] = "Welcome Tour";
const char kWelcomeTourDescription[] =
    "Enables the Welcome Tour that walks new users through ChromeOS System UI.";

const char kWelcomeTourForceUserEligibilityName[] =
    "Force Welcome Tour user eligibility";
const char kWelcomeTourForceUserEligibilityDescription[] =
    "Forces user eligibility for the Welcome Tour that walks new users through "
    "ChromeOS System UI. Enabling this flag has no effect unless the Welcome "
    "Tour is also enabled.";

const char kWifiConnectMacAddressRandomizationName[] =
    "MAC address randomization";
const char kWifiConnectMacAddressRandomizationDescription[] =
    "Randomize MAC address when connecting to unmanaged (non-enterprise) "
    "WiFi networks.";

const char kWifiConcurrencyName[] = "WiFi Concurrency";
const char kWifiConcurrencyDescription[] =
    "When enabled, it uses new WiFi concurrency Shill APIs to start station "
    "WiFi and tethering.";

const char kWifiDirectName[] = "Wifi Direct";
const char kWifiDirectDescription[] =
    "Enables the WiFi direct functionalities in ChromeOS";

const char kWindowSplittingName[] = "CrOS Labs - Window splitting";
const char kWindowSplittingDescription[] =
    "Enables splitting windows by dragging one over another.";

const char kLauncherGameSearchName[] = "Enable launcher game search";
const char kLauncherGameSearchDescription[] =
    "Enables cloud game search results in the launcher.";

const char kLauncherKeyShortcutInBestMatchName[] =
    "Enable keyshortcut results in best match";
const char kLauncherKeyShortcutInBestMatchDescription[] =
    "When enabled, it allows key shortcut results to appear in best match and "
    "answer card in launcher.";

const char kLauncherKeywordExtractionScoring[] =
    "Query keyword extraction and scoring in launcher";
const char kLauncherKeywordExtractionScoringDescription[] =
    "Enables extraction of keywords from query then calculate score from "
    "extracted keyword in the launcher.";

const char kLauncherLocalImageSearchName[] =
    "Enable launcher local image search";
const char kLauncherLocalImageSearchDescription[] =
    "Enables on-device local image search in the launcher.";

const char kLauncherLocalImageSearchOnCoreName[] =
    "Enable launcher local image search on core devices";
const char kLauncherLocalImageSearchOnCoreDescription[] =
    "Enables on-device local image search in the launcher on core devices.";

const char kLauncherLocalImageSearchConfidenceName[] =
    "Launcher Local Image Search Confidence";
const char kLauncherLocalImageSearchConfidenceDescription[] =
    "Allows configurations of the experiment parameters for local image search "
    "confidence threshold in the launcher.";

const char kLauncherLocalImageSearchRelevanceName[] =
    "Launcher Local Image Search Relevance";
const char kLauncherLocalImageSearchRelevanceDescription[] =
    "Allows configurations of the experiment parameters for local image search "
    "Relevance threshold in the launcher.";

const char kLauncherLocalImageSearchOcrName[] =
    "Enable OCR for local image search";
const char kLauncherLocalImageSearchOcrDescription[] =
    "Enables on-device Optical Character Recognition for local image search in "
    "the launcher.";

const char kLauncherLocalImageSearchIcaName[] =
    "Enable ICA for local image search";
const char kLauncherLocalImageSearchIcaDescription[] =
    "Enables on-device Image Content-based Annotation for local image search "
    "in the launcher.";

const char kLauncherFuzzyMatchForOmniboxName[] =
    "Omnibox Results Fuzzy match experiment";
const char kLauncherFuzzyMatchForOmniboxDescription[] =
    "To evaluate the viability of a Fuzzy match on Omnibox results to "
    "downweight search sugestions";

const char kLauncherSearchControlName[] = "Enable launcher search control";
const char kLauncherSearchControlDescription[] =
    "Enable search control in launcher so that users can custmize the result "
    "results provided.";

const char kLauncherNudgeSessionResetName[] =
    "Enable resetting launcher nudge data";
const char kLauncherNudgeSessionResetDescription[] =
    "When enabled, this will reset the launcher nudge shown data on every new "
    "user session, allowing the nudge to be shown again.";

const char kLauncherSystemInfoAnswerCardsName[] =
    "System Info Answer Cards in launcher";
const char kLauncherSystemInfoAnswerCardsDescription[] =
    "Enables System info answer cards in the launcher to provide system "
    "performance metrics";

const char kMacAddressRandomizationName[] = "MAC address randomization";
const char kMacAddressRandomizationDescription[] =
    "Feature to allow MAC address randomization to be enabled for WiFi "
    "networks.";

const char kSysUiShouldHoldbackDriveIntegrationName[] =
    "Holdback for Drive Integration on chromeOS";
const char kSysUiShouldHoldbackDriveIntegrationDescription[] =
    "Enables holdback for Drive Integration.";

const char kSysUiShouldHoldbackFocusModeName[] =
    "Holdback for Focus Mode on chromeOS";
const char kSysUiShouldHoldbackFocusModeDescription[] =
    "Enables holdback for Focus Mode.";

const char kSysUiShouldHoldbackForestName[] = "Holdback for Forest on chromeOS";
const char kSysUiShouldHoldbackForestDescription[] =
    "Enables holdback for Forest.";

const char kSysUiShouldHoldbackGifRecordingName[] =
    "Holdback for GIF recording on chromeOS";
const char kSysUiShouldHoldbackGifRecordingDescription[] =
    "Enables holdback for GIF recording.";

const char kSysUiShouldHoldbackTaskManagementName[] =
    "Holdback for Task Management on chromeOS";
const char kSysUiShouldHoldbackTaskManagementDescription[] =
    "Enables holdback for Task Management.";

const char kTetheringExperimentalFunctionalityName[] =
    "Tethering Allow Experimental Functionality";
const char kTetheringExperimentalFunctionalityDescription[] =
    "Feature to enable Chromebook hotspot functionality for experimental "
    "carriers, modem and modem FW.";

// Prefer keeping this section sorted to adding new definitions down here.

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
const char kExperimentalWebAppStoragePartitionIsolationName[] =
    "Enable experimental web app stroage partition isolation";
const char kExperimentalWebAppStoragePartitionIsolationDescription[] =
    "This is highly experimental. Enabling this flag could break things. And a "
    "factory reset might be needed to fully recover the state.";

const char kBlinkExtensionName[] = "Experimental Blink Extension";
const char kBlinkExtensionDescription[] =
    "Enable the experimental Blink Extension.";

const char kBlinkExtensionDiagnosticsName[] =
    "Experimental Diagnostics Blink Extension";
const char kBlinkExtensionDiagnosticsDescription[] =
    "Enable the experimental Diagnostics Blink Extension.";
#endif  // #if BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
const char kGetAllScreensMediaName[] = "GetAllScreensMedia API";
const char kGetAllScreensMediaDescription[] =
    "When enabled, the getAllScreensMedia API for capturing multiple screens "
    "at once, is available.";
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_CHROMEOS)

const char kAddPrinterViaPrintscanmgrName[] =
    "Uses printscanmgr to add printers";
const char kAddPrinterViaPrintscanmgrDescription[] =
    "Changes the daemon used to add printers from debugd to printscanmgr.";

const char kCrosAppsBackgroundEventHandlingName[] =
    "Experimental Background Events for CrOS Apps";
const char kCrosAppsBackgroundEventHandlingDescription[] =
    "Enable key events for CrOS Apps running in background.";

const char kRunOnOsLoginName[] = "Run on OS login";
const char kRunOnOsLoginDescription[] =
    "When enabled, allows PWAs to be automatically run on OS login.";

const char kPreventCloseName[] = "Prevent close";
const char kPreventCloseDescription[] =
    "When enabled, allow-listed PWAs cannot be closed manually.";

const char kFileSystemAccessGetCloudIdentifiersName[] =
    "Cloud identifiers for FileSystemAccess API";
const char kFileSystemAccessGetCloudIdentifiersDescription[] =
    "Enables the FileSystemHandle.getCloudIdentifiers() method. See"
    "https://github.com/WICG/file-system-access/blob/main/proposals/"
    "CloudIdentifier.md"
    "for more information.";

const char kCrOSDspBasedAecAllowedName[] =
    "Allow CRAS to use a DSP-based AEC if available";
const char kCrOSDspBasedAecAllowedDescription[] =
    "Allows the system variant of the AEC in CRAS to be run on DSP ";

const char kCrOSDspBasedNsAllowedName[] =
    "Allow CRAS to use a DSP-based NS if available";
const char kCrOSDspBasedNsAllowedDescription[] =
    "Allows the system variant of the NS in CRAS to be run on DSP ";

const char kCrOSDspBasedAgcAllowedName[] =
    "Allow CRAS to use a DSP-based AGC if available";
const char kCrOSDspBasedAgcAllowedDescription[] =
    "Allows the system variant of the AGC in CRAS to be run on DSP ";

const char kCrOSEnforceSystemAecName[] = "Enforce using the system AEC in CrAS";
const char kCrOSEnforceSystemAecDescription[] =
    "Enforces using the system variant in CrAS of the AEC";

const char kCrOSEnforceSystemAecAgcName[] =
    "Enforce using the system AEC and AGC in CrAS";
const char kCrOSEnforceSystemAecAgcDescription[] =
    "Enforces using the system variants in CrAS of the AEC and AGC.";

const char kCrOSEnforceSystemAecNsName[] =
    "Enforce using the system AEC and NS in CrAS";
const char kCrOSEnforceSystemAecNsDescription[] =
    "Enforces using the system variants in CrAS of the AEC and NS.";

const char kCrOSEnforceSystemAecNsAgcName[] =
    "Enforce using the system AEC, NS and AGC in CrAS";
const char kCrOSEnforceSystemAecNsAgcDescription[] =
    "Enforces using the system variants in CrAS of the AEC, NS and AGC.";

const char kIgnoreUiGainsName[] = "Ignore UI Gains in system mic gain setting";
const char kIgnoreUiGainsDescription[] =
    "Ignore UI Gains in system mic gain setting";

const char kShowForceRespectUiGainsToggleName[] =
    "Enable a setting toggle to force respect UI gains";
const char kShowForceRespectUiGainsToggleDescription[] =
    "Enable a setting toggle to force respect UI gains.";

const char kCrOSSystemVoiceIsolationOptionName[] =
    "Enable the options of setting system voice isolation per stream";
const char kCrOSSystemVoiceIsolationOptionDescription[] =
    "Enable the options of setting system voice isolation per stream.";

const char kAudioFlexibleLoopbackForSystemLoopbackName[] =
    "Use FLEXIBLE_LOOPBACK instead of POST_MIX_LOOPBACK";
const char kAudioFlexibleLoopbackForSystemLoopbackDescription[] =
    "Request a FLEXIBLE_LOOPBACK instead of POST_MIX_LOOPBACK for system "
    "loopback";

const char kCrosMallName[] = "ChromeOS App Mall";
const char kCrosMallDescription[] =
    "Enables an app to discover and install other apps.";

const char kCrosMallSwaName[] = "ChromeOS App Mall System App";
const char kCrosMallSwaDescription[] =
    "Use System Web App for ChromeOS App Mall.";

const char kCrosMallUrlName[] = "ChromeOS App Mall URL";
const char kCrosMallUrlDescription[] =
    "Customize the URL used for the ChromeOS App Mall.";

const char kCrosPrivacyHubName[] = "Enable ChromeOS Privacy Hub";
const char kCrosPrivacyHubDescription[] = "Enables ChromeOS Privacy Hub.";

const char kCrosPrivacyHubAppPermissionsName[] =
    "Enable app permissions view inside Priacy Hub";
const char kCrosPrivacyHubAppPermissionsDescription[] =
    "When enabled, the user will be able to see the list of apps, sites and "
    "system services affected by the privacy hub toggles.";

const char kCrosPrivacyHubAppPermissionsV2Name[] =
    "Add privacy controls to app specific subpages";
const char kCrosPrivacyHubAppPermissionsV2Description[] =
    "When enabled, the user will be able to manage system access of sensors "
    "from the app specific subpages under the OS Apps section.";

const char kDisableIdleSocketsCloseOnMemoryPressureName[] =
    "Disable closing idle sockets on memory pressure";
const char kDisableIdleSocketsCloseOnMemoryPressureDescription[] =
    "If enabled, idle sockets will not be closed when chrome detects memory "
    "pressure. This applies to web pages only and not to internal requests.";

const char kDisableOfficeEditingComponentAppName[] =
    "Disable Office Editing for Docs, Sheets & Slides";
const char kDisableOfficeEditingComponentAppDescription[] =
    "Disables Office Editing for Docs, Sheets & Slides component app so "
    "handlers won't be registered, making it possible to install another "
    "version for testing.";

const char kLacrosColorManagementName[] = "Enable Chrome Color Management.";
const char kLacrosColorManagementDescription[] =
    "Uses chrome-color-management wayland protocol to manage color spaces "
    "for lacros. This is necessary for enabling HDR on compatible devices.";

const char kLockedModeName[] = "Enable the Locked Mode API.";
const char kLockedModeDescription[] =
    "Enabled the Locked Mode Web API which allows admin-allowlisted sites "
    "to enter a locked down fullscreen mode.";

const char kOneGroupPerRendererName[] =
    "Use one cgroup for each foreground renderer";
const char kOneGroupPerRendererDescription[] =
    "Places each Chrome foreground renderer into its own cgroup";

const char kPlatformKeysAesEncryptionName[] = "Platform Keys AES encryption";
const char kPlatformKeysAesEncryptionDescription[] =
    "Enables AES keys support in the chrome.enterprise.platformKeys and "
    "chrome.platformKeys APIs. For instance, the new operations include sign, "
    "encrypt, and decrypt.";

const char kPrintPreviewCrosPrimaryName[] =
    "Enables the ChromeOS print preview to be the primary print preview.";
const char kPrintPreviewCrosPrimaryDescription[] =
    "Allows the ChromeOS print preview to be opened instead of the browser "
    " print preview.";

const char kDisableQuickAnswersV2TranslationName[] =
    "Disable Quick Answers Translation";
const char kDisableQuickAnswersV2TranslationDescription[] =
    "Disable translation services of the Quick Answers.";

const char kQuickAnswersRichCardName[] = "Enable Quick Answers Rich Card";
const char kQuickAnswersRichCardDescription[] =
    "Enable rich card views of the Quick Answers feature.";

const char kQuickAnswersMaterialNextUIName[] =
    "Enable Quick Answers Material Next UI";
const char kQuickAnswersMaterialNextUIDescription[] =
    "Enable Material Next UI for the Quick Answers feature. This is effective "
    "only if Magic Boost flag is off. Note that this will be changed as this "
    "is effective only if a device is eligible to Magic Boost when the Magic "
    "Boost flag gets flipped.";

const char kSyncChromeOSExplicitPassphraseSharingName[] =
    "Sync passphrase sharing";
const char kSyncChromeOSExplicitPassphraseSharingDescription[] =
    "Allows sharing custom sync passphrase between OS and Browser on ChromeOS";

const char kQuickOfficeForceFileDownloadName[] =
    "Basic Office Editor File Download";
const char kQuickOfficeForceFileDownloadDescription[] =
    "Forces the Basic Office Editor to download files instead of intercepting "
    "navigations to document types it can handle.";

const char kWebPrintingApiName[] = "Web Printing API";
const char kWebPrintingApiDescription[] =
    "Enable access to the Web Printing API. See "
    "https://github.com/WICG/web-printing for details.";
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
const char kChromeOSHWVBREncodingName[] =
    "ChromeOS Hardware Variable Bitrate Encoding";
const char kChromeOSHWVBREncodingDescription[] =
    "Enables the hardware-accelerated variable bitrate (VBR) encoding on "
    "ChromeOS. If the hardware encoder supports VBR for a specified codec, a "
    "video is recorded in VBR encoding in MediaRecoder API automatically and "
    "WebCodecs API if configured so.";
#if defined(ARCH_CPU_ARM_FAMILY)
const char kUseGLForScalingName[] = "Use GL image processor for scaling";
const char kUseGLForScalingDescription[] =
    "Use the GL image processor for scaling over libYUV implementations.";
const char kPreferGLImageProcessorName[] = "Prefer GL image processor";
const char kPreferGLImageProcessorDescription[] =
    "Prefers the GL image processor for format conversion of video frames over"
    " both the libYUV and hardware implementations";
const char kPreferSoftwareMT21Name[] = "Prefer software MT21 conversion";
const char kPreferSoftwareMT21Description[] =
    "Prefer using the software MT21 conversion instead of the MDP hardware "
    "conversion on MT8173 devices.";
const char kEnableProtectedVulkanDetilingName[] =
    "Enable Protected Vulkan Detiling";
const char kEnableProtectedVulkanDetilingDescription[] =
    "Use a Vulkan shader for protected Vulkan detiling.";
const char kEnableArmHwdrm10bitOverlaysName[] =
    "Enable ARM HW DRM 10-bit Overlays";
const char kEnableArmHwdrm10bitOverlaysDescription[] =
    "Enable 10-bit overlays for ARM HW DRM content. If disabled, 10-bit "
    "HW DRM content will be subsampled to 8-bit before scanout. This flag "
    "has no effect on 8-bit content.";
#if BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
const char kEnableArmHwdrmName[] = "Enable ARM HW DRM";
const char kEnableArmHwdrmDescription[] = "Enable HW backed Widevine L1 DRM";
#endif  // BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
#endif  // defined(ARCH_CPU_ARM_FAMILY)
#endif  // BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION) && defined(ARCH_CPU_ARM_FAMILY)
const char kV4L2FlatStatefulVideoDecoderName[] =
    "ChromeOS V4L2 Flat Video Decoder";
const char kV4L2FlatStatefulVideoDecoderDescription[] =
    "Enables the new (flat) hardware-accelerated ChromeOS media::VideoDecoder "
    "implementation. Note that this only has an effect on platforms supporting "
    "the V4L2 Stateful video decoding API, currently Hana MT8173 and Trogdor "
    "SC7180.";
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION) &&
        // defined(ARCH_CPU_ARM_FAMILY)

// Linux -----------------------------------------------------------------------

#if BUILDFLAG(IS_LINUX)
const char kOzonePlatformHintChoiceDefault[] = "Default";
const char kOzonePlatformHintChoiceAuto[] = "Auto";
const char kOzonePlatformHintChoiceX11[] = "X11";
const char kOzonePlatformHintChoiceWayland[] = "Wayland";

const char kOzonePlatformHintName[] = "Preferred Ozone platform";
const char kOzonePlatformHintDescription[] =
    "Selects the preferred platform backend used on Linux. \"Auto\" selects "
    "Wayland if possible, X11 otherwise. ";

const char kPulseaudioLoopbackForCastName[] =
    "Linux System Audio Loopback for Cast (pulseaudio)";
const char kPulseaudioLoopbackForCastDescription[] =
    "Enable system audio mirroring when casting a screen on Linux with "
    "pulseaudio.";

const char kPulseaudioLoopbackForScreenShareName[] =
    "Linux System Audio Loopback for Screen Sharing (pulseaudio)";
const char kPulseaudioLoopbackForScreenShareDescription[] =
    "Enable system audio sharing when screen sharing on Linux with pulseaudio.";

const char kSimplifiedTabDragUIName[] = "Simplified tab dragging UI mode";
const char kSimplifiedTabDragUIDescription[] =
    "Enable simplified tab dragging UI mode as a fallback if the graphical "
    "environment does not support the classic UI.";

const char kWaylandPerWindowScalingName[] = "Wayland per-window scaling";
const char kWaylandPerWindowScalingDescription[] =
    "Enable Wayland's per-window scaling experimental support.";

const char kWaylandTextInputV3Name[] = "Wayland text-input-v3";
const char kWaylandTextInputV3Description[] =
    "Enable Wayland's text-input-v3 experimental support.";

const char kWaylandUiScalingName[] = "Wayland UI scaling";
const char kWaylandUiScalingDescription[] =
    "Enable experimental support for text scaling in the Wayland backend "
    "backed by full UI scaling. Requires #wayland-per-window-scaling to be "
    "enabled too.";
#endif  // BUILDFLAG(IS_LINUX)

// All views-based platforms --------------------------------------------------

#if defined(TOOLKIT_VIEWS)
const char kSideSearchName[] = "Side search";
const char kSideSearchDescription[] =
    "Enables an easily accessible way to access your most recent Google search "
    "results page embedded in a browser side panel";

const char kSearchWebInSidePanelName[] = "Search web in side panel";
const char kSearchWebInSidePanelDescription[] =
    "Displays right-click search results of a highlighted text in side panel";

#endif  // defined(TOOLKIT_VIEWS)

// Random platform combinations -----------------------------------------------

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
const char kZeroCopyVideoCaptureName[] = "Enable Zero-Copy Video Capture";
const char kZeroCopyVideoCaptureDescription[] =
    "Camera produces a gpu friendly buffer on capture and, if there is, "
    "hardware accelerated video encoder consumes the buffer";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
const char kFollowingFeedSidepanelName[] = "Following feed in the sidepanel";
const char kFollowingFeedSidepanelDescription[] =
    "Enables the following feed in the sidepanel.";
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        //  BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
const char kTaskManagerClankName[] = "Task Manager on Clank";
const char kTaskManagerClankDescription[] =
    "Enables the Task Manager for Clank (Chrome on Android).";
#else
const char kTaskManagerDesktopRefreshName[] = "Task Manager Desktop Refresh";
const char kTaskManagerDesktopRefreshDescription[] =
    "Enables a refreshed design for the Task Manager on Desktop platforms.";
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
const char kEnableNetworkServiceSandboxName[] =
    "Enable the network service sandbox.";
const char kEnableNetworkServiceSandboxDescription[] =
    "Enables a sandbox around the network service to help mitigate exploits in "
    "its process. This may cause crashes if Kerberos is used.";

const char kUseOutOfProcessVideoDecodingName[] =
    "Use out-of-process video decoding (OOP-VD)";
const char kUseOutOfProcessVideoDecodingDescription[] =
    "Start utility processes to do hardware video decoding. Note: on LaCrOS, "
    "this task is delegated to ash-chrome by requesting a "
    "media.stable.mojom.StableVideoDecoderFactory through the crosapi (so "
    "chrome://flags#expose-out-of-process-video-decoding-to-lacros must be "
    "enabled in ash-chrome).";
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
const char kWebShareName[] = "Web Share";
const char kWebShareDescription[] =
    "Enables the Web Share (navigator.share) APIs on experimentally supported "
    "platforms.";
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
const char kWebBluetoothConfirmPairingSupportName[] =
    "Web Bluetooth confirm pairing support";
const char kWebBluetoothConfirmPairingSupportDescription[] =
    "Enable confirm-only and confirm-pin pairing mode support for Web "
    "Bluetooth";
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
const char kSkipUndecryptablePasswordsName[] =
    "Skip undecryptable passwords to use the available decryptable "
    "passwords.";
const char kSkipUndecryptablePasswordsDescription[] =
    "Makes the decryptable passwords available in the password manager when "
    "there are undecryptable ones.";

#if BUILDFLAG(ENABLE_PRINTING)
const char kCupsIppPrintingBackendName[] = "CUPS IPP Printing Backend";
const char kCupsIppPrintingBackendDescription[] =
    "Use the CUPS IPP printing backend instead of the original CUPS backend "
    "that calls the PPD API.";
#endif  // BUILDFLAG(ENABLE_PRINTING)
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kScreenlockReauthCardName[] =
    "Show screenlock reauth before filling password setting in password "
    "manager";
const char kScreenlockReauthCardDescription[] =
    "Enables setting for requiring reauth before filling passwords "
    "in password manager settings. The default for setting is turned off.";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)
const char kScreenlockReauthPromoCardName[] =
    "Enables screenlock reauth before filling promo card in settings";
const char kScreenlockReauthPromoCardDescription[] =
    "Enables promo card in settings encouraging users to enable screenlock "
    "reauth before filling passwords.";
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
const char kChromeWideEchoCancellationName[] = "Chrome-wide echo cancellation";
const char kChromeWideEchoCancellationDescription[] =
    "Run WebRTC capture audio processing in the audio process instead of the "
    "renderer processes, thereby cancelling echoes from more audio sources.";
#endif  // BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)

#if BUILDFLAG(DCHECK_IS_CONFIGURABLE)
const char kDcheckIsFatalName[] = "DCHECKs are fatal";
const char kDcheckIsFatalDescription[] =
    "By default Chrome will evaluate in this build, but only log failures, "
    "rather than crashing. If enabled, DCHECKs will crash the calling process.";
#endif  // BUILDFLAG(DCHECK_IS_CONFIGURABLE)

#if BUILDFLAG(ENABLE_NACL)
const char kNaclName[] = "Native Client";
const char kNaclDescription[] =
    "Support Native Client for all web applications, even those that were not "
    "installed from the Chrome Web Store.";
const char kVerboseLoggingInNaclName[] = "Verbose logging in Native Client";
const char kVerboseLoggingInNaclDescription[] =
    "Control the level of verbose logging in Native Client modules for "
    "debugging purposes.";
const char kVerboseLoggingInNaclChoiceDefault[] = "Default";
const char kVerboseLoggingInNaclChoiceLow[] = "Low";
const char kVerboseLoggingInNaclChoiceMedium[] = "Medium";
const char kVerboseLoggingInNaclChoiceHigh[] = "High";
const char kVerboseLoggingInNaclChoiceHighest[] = "Highest";
const char kVerboseLoggingInNaclChoiceDisabled[] = "Disabled";
#endif  // ENABLE_NACL

#if BUILDFLAG(ENABLE_OOP_PRINTING)
const char kEnableOopPrintDriversName[] =
    "Enables Out-of-Process Printer Drivers";
const char kEnableOopPrintDriversDescription[] =
    "Enables printing interactions with the operating system to be performed "
    "out-of-process.";
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

#if BUILDFLAG(ENABLE_PAINT_PREVIEW) && BUILDFLAG(IS_ANDROID)
const char kPaintPreviewDemoName[] = "Paint Preview Demo";
const char kPaintPreviewDemoDescription[] =
    "If enabled a menu item is added to the Android main menu to demo paint "
    "previews.";
#endif  // ENABLE_PAINT_PREVIEW && BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_PDF)
const char kAccessiblePDFFormName[] = "Accessible PDF Forms";
const char kAccessiblePDFFormDescription[] =
    "Enables accessibility support for PDF forms.";

const char kPdfCr23Name[] = "PDF Chrome Refresh 2023";
const char kPdfCr23Description[] =
    "Use new CR23 icons and colors for the PDF Viewer.";

#if BUILDFLAG(ENABLE_PDF_INK2)
const char kPdfInk2Name[] = "PDF Ink Signatures";
const char kPdfInk2Description[] =
    "Enables the ability to annotate PDFs using a new ink library.";
#endif  // BUILDFLAG(ENABLE_PDF_INK2)

const char kPdfOopifName[] = "OOPIF for PDF Viewer";
const char kPdfOopifDescription[] =
    "Use an OOPIF for the PDF Viewer, instead of a GuestView.";

const char kPdfPortfolioName[] = "PDF portfolio";
const char kPdfPortfolioDescription[] = "Enable PDF portfolio feature.";

const char kPdfUseSkiaRendererName[] = "Use Skia Renderer";
const char kPdfUseSkiaRendererDescription[] =
    "Use Skia as the PDF renderer. This flag will have no effect if the "
    "renderer choice is controlled by an enterprise policy.";
#endif  // BUILDFLAG(ENABLE_PDF)

#if BUILDFLAG(ENABLE_VR)
const char kWebXrHandInputName[] = "WebXR Hand Input";
const char kWebXrHandInputDescription[] =
    "Enables access to articulated hand tracking sensor input in WebXR.";
const char kWebXrInternalsName[] = "WebXR Internals Debugging Page";
const char kWebXrInternalsDescription[] =
    "Enables the webxr-internals developer page which can be used to help "
    "debug issues with the WebXR Device API.";
#endif  // #if defined(ENABLE_VR)

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
const char kWebUITabStripFlagId[] = "webui-tab-strip";
const char kWebUITabStripName[] = "WebUI tab strip";
const char kWebUITabStripDescription[] =
    "When enabled makes use of a WebUI-based tab strip.";

const char kWebUITabStripContextMenuAfterTapName[] =
    "WebUI tab strip context menu after tap";
const char kWebUITabStripContextMenuAfterTapDescription[] =
    "Enables the context menu to appear after a tap gesture rather than "
    "following a press gesture.";
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)

#if defined(TOOLKIT_VIEWS) || BUILDFLAG(IS_ANDROID)

const char kAutofillCreditCardUploadName[] =
    "Enable offering upload of Autofilled credit cards";
const char kAutofillCreditCardUploadDescription[] =
    "Enables a new option to upload credit cards to Google Payments for sync "
    "to all Chrome devices.";

#endif  // defined(TOOLKIT_VIEWS) || BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
const char kElasticOverscrollName[] = "Elastic Overscroll";
const char kElasticOverscrollDescription[] =
    "Enables Elastic Overscrolling on touchscreens and precision touchpads.";
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
const char kElementCaptureName[] = "Element Capture";
const char kElementCaptureDescription[] =
    "Enables Element Capture - an API allowing the mutation of a tab-capture "
    "media track into a track capturing just a specific DOM element.";
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) ||                                      \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) || \
    BUILDFLAG(IS_MAC)
const char kUIDebugToolsName[] = "Debugging tools for UI";
const char kUIDebugToolsDescription[] =
    "Enables additional keyboard shortcuts to help debugging.";
#endif

#if defined(WEBRTC_USE_PIPEWIRE)
const char kWebrtcPipeWireCameraName[] = "PipeWire Camera support";
const char kWebrtcPipeWireCameraDescription[] =
    "When enabled the PipeWire multimedia server will be used for cameras.";
#endif  // #if defined(WEBRTC_USE_PIPEWIRE)

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kChromeKioskEnableLacrosName[] =
    "Enables Lacros in the chrome app Kiosk";
const char kChromeKioskEnableLacrosDescription[] =
    "Uses Lacros-chrome as the web browser in the chrome app Kiosk session on "
    "ChromeOS. When disabled, the Ash-chrome will be used";

const char kWebKioskEnableLacrosName[] =
    "Enables Lacros in the web (PWA) Kiosk";
const char kWebKioskEnableLacrosDescription[] =
    "Uses Lacros-chrome as the web browser in the web (PWA) Kiosk session on "
    "ChromeOS. When disabled, the Ash-chrome will be used";

const char kDisableLacrosTtsSupportName[] = "Disable lacros tts support";
const char kDisableLacrosTtsSupportDescription[] =
    "Disable lacros support for text to speech.";

const char kPromiseIconsName[] = "Promise Icons";
const char kPromiseIconsDescription[] =
    "Enables promise icons in the Launcher and Shelf (if the app is pinned) "
    "for app installations.";

const char kEnableAudioFocusEnforcementName[] = "Audio Focus Enforcement";
const char kEnableAudioFocusEnforcementDescription[] =
    "Enables enforcement of a single media session having audio focus at "
    "any one time. Requires #enable-media-session-service to be enabled too.";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_COMPOSE)
const char kComposeId[] = "CCO";
const char kComposeName[] = "CCO Edits";
const char kComposeDescription[] = "Enables CCO editing feature";

const char kComposeTextSelectionName[] = "Compose Selection";
const char kComposeTextSelectionDescription[] =
    "Enables feature for different heuristics for selecting text for Compose";

const char kComposeProactiveNudgeName[] = "Compose Proactive Nudge";
const char kComposeProactiveNudgeDescription[] =
    "Enables proactive nudging for Compose";

const char kComposePoliteNudgeName[] = "Compose Polite a11y announcement";
const char kComposePoliteNudgeDescription[] =
    "Uses polite announcements to alert the user that a compose popup is "
    "available to interact with.";

const char kComposeSegmentationPromotionName[] =
    "Compose Segmentation Promotion";
const char kComposeSegmentationPromotionDescription[] =
    "Enables the segmentation platfrom for the Compose proactive nudge";

const char kComposeSelectionNudgeName[] = "Compose Selection Nudge";
const char kComposeSelectionNudgeDescription[] =
    "Enables nudge on selection for Compose";

const char kComposeUpfrontInputModesName[] = "Compose Upfront Input Modes";
const char kComposeUpfrontInputModesDescription[] =
    "Enables upfront input modes in the Compose dialog";
#endif  // BUILDFLAG(ENABLE_COMPOSE)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
const char kThirdPartyProfileManagementName[] =
    "Third party profile management";
const char kThirdPartyProfileManagementDescription[] =
    "Enables profile management triggered by third-party sign-ins.";

const char kOidcAuthProfileManagementName[] = "OIDC profile management";
const char kOidcAuthProfileManagementDescription[] =
    "Enables profile management triggered by OIDC authentications.";

const char kUnoDesktopName[] = "UNO Desktop";
const char kUnoDesktopDescription[] =
    "Enables the UNO model on Desktop. This is currently an experiment in a "
    "prototype stage in order to validate the model.";

const char kExplicitBrowserSigninUIOnDesktopName[] =
    "Explicit Browser Signin UI On Desktop";
const char kExplicitBrowserSigninUIOnDesktopDescription[] =
    "Enables the UNO model on Desktop. Used for the launch of the UNO model on "
    "Desktop, as well as for the later phases of the experiment.";

const char kDesktopPWAsUserLinkCapturingScopeExtensionsName[] =
    "Desktop PWA Link Capturing with Scope Extensions";
const char kDesktopPWAsUserLinkCapturingScopeExtensionsDescription[] =
    "Allows the 'Desktop PWA Scope Extensions' feature to be used with the "
    "'Desktop PWA Link Capturing' feature. Both of those features are required "
    "to be turned on for this flag to have an effect.";

const char kSyncEnableBookmarksInTransportModeName[] =
    "Enable bookmarks in transport mode";
const char kSyncEnableBookmarksInTransportModeDescription[] =
    "Enables account bookmarks for signed-in non-syncing users";

const char kSyncEnableContactInfoDataTypeInTransportModeName[] =
    "Sync Enable Contact Info Data Type in Transport Mode";
const char kSyncEnableContactInfoDataTypeInTransportModeDescription[] =
    "Allows the 'Sync Enable Contact Info Data Type in Transport Mode' feature "
    "to be used with the 'Explicit Browser Signin UI On Desktop' feature.";

const char kEnableGenericOidcAuthProfileManagementName[] =
    "Enable generic OIDC profile management";
const char kEnableGenericOidcAuthProfileManagementDescription[] =
    "Enables profile management triggered by generic OIDC authentications.";
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_HLS_DEMUXER)
const char kEnableBuiltinHlsName[] = "Builtin HLS player";
const char kEnableBuiltinHlsDescription[] =
    "Enables chrome's builtin HLS player instead of Android's MediaPlayer";
#endif  // BUILDFLAG(ENABLE_HLS_DEMUXER)

#if !BUILDFLAG(IS_CHROMEOS_ASH)
const char kProfilesReorderingName[] = "Profiles Reordering";
const char kProfilesReorderingDescription[] =
    "Enables profiles reordering in the Profile Picker main view by drag and "
    "dropping the Profile Tiles. The order is saved when changed and "
    "persisted.";
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
const char kBatchUploadDesktopName[] = "Batch Upload Desktop";
const char kBatchUploadDesktopDescription[] =
    "Enables the Batch Upload feature for Desktop. This allows to upload local "
    "data to the account data when signed in. Currently only supports "
    "Passwords and Addresses";
#endif

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
const char kEnableBoundSessionCredentialsName[] =
    "Device Bound Session Credentials";
const char kEnableBoundSessionCredentialsDescription[] =
    "Enables Google session credentials binding to cryptographic keys.";

const char kEnableBoundSessionCredentialsSoftwareKeysForManualTestingName[] =
    "Device Bound Session Credentials with software keys";
const char
    kEnableBoundSessionCredentialsSoftwareKeysForManualTestingDescription[] =
        "Enables mock software-backed cryptographic keys for Google session "
        "credentials binding (not secure). This is intented to be used for "
        "manual testing only.";
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

#if BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)
const char kEnableStandardBoundSessionCredentialsName[] =
    "Device Bound Session Credentials (Standard)";
const char kEnableStandardBoundSessionCredentialsDescription[] =
    "Enables the official version of Device Bound Session Credentials. For "
    "more information see https://github.com/WICG/dbsc.";
#endif  // BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)

#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
const char kEnableCertManagementV2UIName[] = "Cert Management V2 UI";
const char kEnableCertManagementV2UIDescription[] =
    "Enables the Cert Management V2 UI; accessible at "
    "chrome://settings/certificates";
#endif  // BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kOverlayScrollbarsOSSettingsName[] = "Overlay Scrollbar OS Settings";
const char kOverlayScrollbarsOSSettingsDescription[] =
    "Enable the os settings for overlay scrollbars on ChromeOS.";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
const char kSupervisedProfileHideGuestName[] = "Supervised Profile Hide Guest";
const char kSupervisedProfileHideGuestDescription[] =
    "Hides Guest Profile entry points for supervised users";

const char kSupervisedProfileSafeSearchName[] = "Supervised Profile SafeSearch";
const char kSupervisedProfileSafeSearchDescription[] =
    "Enables SafeSearch in Google Search for supervised users in the pending "
    "state.";

const char kSupervisedProfileReauthForYouTubeName[] =
    "Supervised Profile YouTube reauth";
const char kSupervisedProfileReauthForYouTubeDescription[] =
    "Ask supervised users to re-authenticate when attempting to access YouTube "
    "in the pending state.";

const char kSupervisedProfileReauthForBlockedSiteName[] =
    "Supervised Profile blocked site reauth";
const char kSupervisedProfileReauthForBlockedSiteDescription[] =
    "Ask supervised users to re-authenticate when attempting to navigate to a "
    "site blocked by parental controls.";

const char kSupervisedProfileSubframeReauthName[] =
    "Supervised Profile reauth in subframes";
const char kSupervisedProfileSubframeReauthDescription[] =
    "If \"Supervised Profile YouTube reauth\" or \"Supervised Profile blocked "
    "site reauth\" is enabled, require supervised users to re-authenticate "
    "before accessing embedded YouTube videos or blocked sites in subframes, "
    "respectively.";

const char kSupervisedProfileFilteringFallbackName[] =
    "Supervised Profile filtering fallback";
const char kSupervisedProfileFilteringFallbackDescription[] =
    "Applies website filters for supervised users in the pending state, if the "
    "Family Link website filtering setting is set to block explicit sites. "
    "If the Family Link website filtering setting is set to another value, it "
    "is applied in the pending regardless of this flag.";

const char kSupervisedProfileCustomStringsName[] =
    "Supervised Profile custom strings";
const char kSupervisedProfileCustomStringsDescription[] =
    "Displays modified strings on both the sign-in intercept UI and the "
    "pre-UNO sync opt out screen";

const char kSupervisedProfileSignInIphName[] = "Supervised Profile sign-in IPH";
const char kSupervisedProfileSignInIphDescription[] =
    "Displays an in-product help message when a Profile becomes owned by a "
    "supervised user (either on creation of the new profile, or after sign "
    "in).";

const char kSupervisedProfileShowKiteBadgeName[] =
    "Supervised Profile show kite badge";
const char kSupervisedProfileShowKiteBadgeDescription[] =
    "Shows a kite badge on the profile avatar for supervised users.";
#endif  // #if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

// ============================================================================
// Don't just add flags to the end, put them in the right section in
// alphabetical order just like the header file.
// ============================================================================

}  // namespace flag_descriptions
