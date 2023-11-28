// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/flag_descriptions.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "media/gpu/buildflags.h"
#include "pdf/buildflags.h"

// Keep in identical order as the header file, see the comment at the top
// for formatting rules.

namespace flag_descriptions {

const char kAccelerated2dCanvasName[] = "Accelerated 2D canvas";
const char kAccelerated2dCanvasDescription[] =
    "Enables the use of the GPU to perform 2d canvas rendering instead of "
    "using software rendering.";

const char kCanvasOopRasterizationName[] =
    "Out-of-process 2D canvas rasterization.";
const char kCanvasOopRasterizationDescription[] =
    "The rasterization of 2d canvas contents is performed in the GPU process. "
    "Requires that out-of-process rasterization be enabled.";

const char kAcceleratedVideoDecodeName[] = "Hardware-accelerated video decode";
const char kAcceleratedVideoDecodeDescription[] =
    "Hardware-accelerated video decode where available.";

const char kAcceleratedVideoEncodeName[] = "Hardware-accelerated video encode";
const char kAcceleratedVideoEncodeDescription[] =
    "Hardware-accelerated video encode where available.";

#if BUILDFLAG(ENABLE_PDF)
const char kAccessiblePDFFormName[] = "Accessible PDF Forms";
const char kAccessiblePDFFormDescription[] =
    "Enables accessibility support for PDF forms.";

const char kPdfOopifName[] = "OOPIF for PDF Viewer";
const char kPdfOopifDescription[] =
    "Use an OOPIF for the PDF Viewer, instead of a GuestView.";

const char kPdfPortfolioName[] = "PDF portfolio";
const char kPdfPortfolioDescription[] = "Enable PDF portfolio feature.";

const char kPdfUseSkiaRendererName[] = "Use Skia Renderer";
const char kPdfUseSkiaRendererDescription[] =
    "Use Skia as the PDF renderer. This flag will have no effect if the "
    "renderer choice is controlled by an enterprise policy.";
#endif

const char kAdvancedPeripheralsSupportName[] =
    "Advanced peripherals support on Android.";
const char kAdvancedPeripheralsSupportDescription[] =
    "Advanced keyboard, mouse and trackpad support for increased productivity.";

const char kAdvancedPeripheralsSupportTabStripName[] =
    "Advanced peripherals support for the tab strip on Android.";
const char kAdvancedPeripheralsSupportTabStripDescription[] =
    "Advanced keyboard, mouse and trackpad support for the tab strip UI.";

const char kAppDeduplicationServiceFondueName[] =
    "Identify duplicate app groups.";
const char kAppDeduplicationServiceFondueDescription[] =
    "Enables pulling app duplicate data from a Google server to allow clients "
    "to determine app duplicates.";

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

const char kAndroidAppIntegrationSafeSearchName[] =
    "SafeSearch in Android App Search";
const char kAndroidAppIntegrationSafeSearchDescription[] =
    "If enabled, allows Chrome to filter out sensitive urls";

const char kAndroidExtendedKeyboardShortcutsName[] =
    "Android Extended Keyboard Shortcuts";
const char kAndroidExtendedKeyboardShortcutsDescription[] =
    "If enabled, allows for extended keyboard shortcuts (i.e. Alt + Backspace "
    "to delete line).";

const char kAnimatedImageResumeName[] = "Use animated image resume behavior";
const char kAnimatedImageResumeDescription[] =
    "Resumes animated images from the last frame drawn rather than attempt "
    "to catch up to the frame that should be drawn based on current time.";

const char kAriaElementReflectionName[] = "Enable ARIA element reflection";
const char kAriaElementReflectionDescription[] =
    "Enable setting ARIA relationship attributes that reference other elements "
    "directly without an IDREF";

const char kAttributionReportingDebugModeName[] =
    "Attribution Reporting Debug Mode";
const char kAttributionReportingDebugModeDescription[] =
    "Enables debug mode for the Attribution Reporting API. This removes all "
    "reporting delays and noise. Only works if the Attribution Reporting API "
    "is already enabled.";

const char kAuxiliarySearchDonationName[] = "Auxiliary Search Donation";
const char kAuxiliarySearchDonationDescription[] =
    "If enabled, override Auxiliary Search donation cap.";

const char kBackgroundResourceFetchName[] = "Background Resource Fetch";
const char kBackgroundResourceFetchDescription[] =
    "Process resource requests in a background thread inside Blink.";

const char kCdmStorageDatabaseName[] = "Cdm Storage Database";
const char kCdmStorageDatabaseDescription[] =
    "Start to use the CdmStorageDatabase to store data alongside the "
    "MediaLicenseDatabase. If disabled, we will not use CdmStorage* at all, "
    "even in MediaLicense* code.";

const char kCdmStorageDatabaseMigrationName[] =
    "Cdm Storage Database Migration";
const char kCdmStorageDatabaseMigrationDescription[] =
    "Use the Cdm Storage Database over the MediaLicenseDatabase for Cdm "
    "storage operations.";

const char kClickToCallName[] = "Click-To-Call";
const char kClickToCallDescription[] = "Enable the click-to-call feature.";

const char kClipboardMaximumAgeName[] = "Clipboard maximum age";
const char kClipboardMaximumAgeDescription[] =
    "Limit the maximum age for recent clipboard content";

const char kClipboardUnsanitizedContentName[] = "Clipboard unsanitized read";
const char kClipboardUnsanitizedContentDescription[] =
    "Allows reading unsanitized content from the clipboard. "
    "Currently, it is only applicable to HTML format. See crbug.com/1268679.";

const char kClipboardWellFormedHtmlSanitizationWriteName[] =
    "Clipboard well-formed HTML sanitized write";
const char kClipboardWellFormedHtmlSanitizationWriteDescription[] =
    "New sanitization routine when writing HTML to clipboard with the async "
    "clipboard web API.";

const char kContentLanguagesInLanguagePickerName[] =
    "Content languages in language picker";
const char kContentLanguagesInLanguagePickerDescription[] =
    "Enables bringing user's content languages that are translatable to the "
    "top of the list with all languages shown in the translate menu";

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kCoralFeatureKeyName[] = "Secret key for Coral feature.";
const char kCoralFeatureKeyDescription[] =
    "Secret key for Coral feature. Incorrect values will cause chrome crashes.";
#endif  // IS_CHROMEOS_ASH

#if BUILDFLAG(IS_CHROMEOS)
const char kCrOSLegacyMediaFormatsName[] = "Legacy media format support";
const char kCrOSLegacyMediaFormatsDescription[] =
    "Controls support for uncommon legacy media formats: AVI containers and "
    "MPEG4 video streams.";
#endif

const char kCustomizeChromeColorExtractionName[] =
    "Customize Chrome Color Extraction";
const char kCustomizeChromeColorExtractionDescription[] =
    "Enables setting theme color based on background image color when "
    "background image is changed in New Tab Page Customize Chrome.";

const char kCustomizeChromeSidePanelName[] = "Customize Chrome Side Panel";
const char KCustomizeChromeSidePanelDescription[] =
    "Enables the ability to use Customize Chrome functionality from the "
    "unified side panel on the New Tab Page.";

const char kCustomizeChromeSidePanelExtensionsCardName[] =
    "Customize Chrome Side Panel Extension Card";
const char kCustomizeChromeSidePanelExtensionsCardDescription[] =
    "If enabled, shows an extension card within the Customize Chrome Side "
    "Panel for access to the Chrome Web Store extensions.";

const char kCustomizeChromeWallpaperSearchName[] =
    "Customize Chrome Wallpaper Search";
const char kCustomizeChromeWallpaperSearchDescription[] =
    "Enables wallpaper search in Customize Chrome Side Panel.";

const char kDeprecateUnloadName[] = "Deprecate the unload event";
const char kDeprecateUnloadDescription[] =
    "Controls the default for Permissions-Policy unload. If enabled, unload "
    "handlers are deprecated and will not receive the unload event unless a "
    "Permissions-Policy to enable them has been explicitly set. If  disabled, "
    "unload handlers will continue to receive the unload event unless "
    "explicity disabled by Permissions-Policy, even during the gradual "
    "rollout of their deprecation.";

const char kForceStartupSigninPromoName[] = "Force Start-up Signin Promo";
const char kForceStartupSigninPromoDescription[] =
    "If enabled, the full screen signin promo will be forced to show up at "
    "Chrome start-up.";

#if BUILDFLAG(USE_FONTATIONS_BACKEND)
const char kFontationsFontBackendName[] = "Enable Fontations font backend";
const char kFontationsFontBackendDescription[] =
    "If enabled, the Fontations font backend will be used for web fonts where "
    "otherwise FreeType would have been used.";
#endif

const char kGainmapHdrImagesName[] = "Gainmap HDR image rendering";
const char kGainmapHdrImagesDescription[] =
    "If enabled, renders images that include an gainmap in HDR";

const char kAvifGainmapHdrImagesName[] = "AVIF gainmap HDR image rendering";
const char kAvifGainmapHdrImagesDescription[] =
    "If enabled, and the 'Gainmap HDR image rendering' flag is also enabled, "
    "Chrome uses the gainmap (if present) in AVIF images to render the HDR "
    "version on HDR displays and the SDR version on SDR displays.";

const char kTangibleSyncName[] = "Tangible Sync";
const char kTangibleSyncDescription[] =
    "Enables the tangible sync when a user starts the sync consent flow";

const char kDIPSName[] = "Bounce Tracking Mitigations";
const char kDIPSDescription[] =
    "This flag controls bounce tracking mitigations.";

const char kDocumentPictureInPictureApiName[] =
    "Document Picture-in-Picture API";
const char kDocumentPictureInPictureApiDescription[] =
    "Enables API to open an always-on-top window with a full HTML document";

const char kDownloadWarningImprovementsName[] = "Download Warning Improvements";
const char kDownloadWarningImprovementsDescription[] =
    "Enable UI improvements for downloads, download scanning, and download "
    "warnings. The enabled features are subject to change at any time.";

const char kEnableBenchmarkingName[] = "Enable benchmarking";
const char kEnableBenchmarkingDescription[] =
    "Sets all features to their default state; that is, disables randomization "
    "for feature states. This is used by developers and testers to "
    "diagnose whether an observed problem is caused by a non-default "
    "base::Feature configuration. This flag is automatically reset "
    "after 3 restarts. On the third restart, the flag will appear to be off "
    "but the effect is still active.";

const char kPreloadingOnPerformancePageName[] =
    "Preloading Settings on Performance Page";
const char kPreloadingOnPerformancePageDescription[] =
    "Moves preloading settings to the performance page.";

const char kPrivacyIndicatorsName[] = "Enable Privacy Indicators";
const char kPrivacyIndicatorsDescription[] =
    "While screen sharing or camera/microphone is being accessed, show a green "
    "icon in the status area as well as add a silent notification to the tray.";

const char kEnableDrDcName[] =
    "Enables Display Compositor to use a new gpu thread.";
const char kEnableDrDcDescription[] =
    "When enabled, chrome uses 2 gpu threads instead of 1. "
    " Display compositor uses new dr-dc gpu thread and all other clients "
    "(raster, webgl, video) "
    " continues using the gpu main thread.";

const char kForceGpuMainThreadToNormalPriorityDrDcName[] =
    "Force GPU main thread priority to normal for DrDc.";
const char kForceGpuMainThreadToNormalPriorityDrDcDescription[] =
    "When enabled, force GPU main thread priority to be normal for DrDc mode. "
    "In that case DrDc thread continues to use DISPLAY thread priority and "
    "hence have higher thread priority than GPU main. Note that this flag will "
    "be a no-op when DrDc is disabled.";

const char kUseClientGmbInterfaceName[] =
    "Use ClientGmb interface to create GpuMemoryBuffers.";
const char kUseClientGmbInterfaceDescription[] =
    " Use new ClientGmb interface to create GpuMemoryBuffers when enabled."
    " This is expected to reduce number of IPCs happening while creating "
    " GpuMemoryBuffers.";

const char kTextBasedAudioDescriptionName[] = "Enable audio descriptions.";
const char kTextBasedAudioDescriptionDescription[] =
    "When enabled, HTML5 video elements with a 'descriptions' WebVTT track "
    "will speak the audio descriptions aloud as the video plays.";

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)

const char kFilterWebsitesForSupervisedUsersOnDesktopName[] =
    "Enable website filtering for supervised users";
const char kFilterWebsitesForSupervisedUsersOnDesktopDescription[] =
    "Enable filtering of web content for Family Link supervised users. "
    "The enable-family-link-supervision flag must also be enabled.";

const char kEnableExtensionsPermissionsForSupervisedUsersOnDesktopName[] =
    "Require parent permissions for extensions";
const char
    kEnableExtensionsPermissionsForSupervisedUsersOnDesktopDescription[] =
        "Apply Family Link parental control settings for extension "
        "installation. "
        "The enable-family-link-supervision flag must also be enabled.";

const char kSupervisedPrefsControlledBySupervisedStoreName[] =
    "Display updated UI for preferences managed by Family Link";
const char kSupervisedPrefsControlledBySupervisedStoreDescription[] =
    "Display updated UI for preferences managed by Family Link. "
    "The enable-family-link-supervision flag must also be enabled.";

const char kEnableManagedByParentUiName[] = "Enable Family Link management UI";
const char kEnableManagedByParentUiDescription[] =
    "Enables UI indicating if a Profile is managed by Family Link parental "
    "controls."
    "The enable-family-link-supervision flag must also be enabled.";

const char kClearingCookiesKeepsSupervisedUsersSignedInName[] =
    "Clearing cookies keep supervised users signed in";
const char kClearingCookiesKeepsSupervisedUsersSignedInDescription[] =
    "Supervised users will remain signed in when cookies are cleared. Display "
    "UI is updated accordingly"
    "The enable-family-link-supervision flag must also be enabled.";

#endif  // ENABLE_SUPERVISED_USERS

const char kUpcomingFollowFeaturesName[] = "Enable upcoming follow features.";
const char kUpcomingFollowFeaturesDescription[] =
    "This flag enables all upcoming follow features, in the experiment "
    "arms that are most likely to be shipped. This is a meta-flag to which "
    "features are upcoming at any given time may change.";

const char kUseAndroidStagingSmdsName[] = "Use Android staging SM-DS";
const char kUseAndroidStagingSmdsDescription[] =
    "Use the Android staging address when fetching pending eSIM profiles.";

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

const char kEnableFileBackedBlobFactoryName[] =
    "Enable registration of file backed blobs through the "
    "FileBackedBlobFactory interface";
const char kEnableFileBackedBlobFactoryDescription[] =
    "Use the FileBackedBlobFactory interface to register file backed blobs. "
    "This allows to identify the URL where the blob is uploaded and it enables "
    "Data Leak Prevention checks for managed users having file restrictions.";

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

const char kSHA1ServerSignatureName[] = "Allow SHA-1 server signatures in TLS.";
const char kSHA1ServerSignatureDescription[] =
    "When enabled, Chrome will allow the use of SHA-1 in signatures from the "
    "server during a TLS handshake";

const char kEncryptedClientHelloName[] = "Encrypted ClientHello";
const char kEncryptedClientHelloDescription[] =
    "When enabled, Chrome will enable Encrypted ClientHello support. This will "
    "encrypt TLS ClientHello if the server enables the extension via the HTTPS "
    "DNS record.";

const char kIsolatedSandboxedIframesName[] = "Isolated sandboxed iframes";
const char kIsolatedSandboxedIframesDescription[] =
    "When enabled, applies process isolation to iframes with the 'sandbox' "
    "attribute and without the 'allow-same-origin' permission set on that "
    "attribute. This also applies to documents with a similar CSP sandbox "
    "header, even in the main frame. The affected sandboxed documents can be "
    "grouped into processes based on their URL's site or origin. The default "
    "grouping when enabled is per-site.";

const char kAutofillEnableCvcStorageAndFillingName[] =
    "Enable CVC storage and filling for payments autofill";
const char kAutofillEnableCvcStorageAndFillingDescription[] =
    "When enabled, we will store CVC for both local and server credit cards. "
    "This will also allow the users to autofill their CVCs on checkout pages.";

const char kAutofillEnableAndroidNKeyForFidoAuthenticationName[] =
    "Enable Android N Key for FIDO authentication";
const char kAutofillEnableAndroidNKeyForFidoAuthenticationDescription[] =
    "When enabled, Android N+ devices will be supported for FIDO "
    "authentication when autofilling server credit cards.";

const char kAutofillEnableFIDOProgressDialogName[] =
    "Show FIDO progress dialog on Android";
const char kAutofillEnableFIDOProgressDialogDescription[] =
    "When enabled, a progress dialog is displayed while authenticating with "
    "FIDO on Android.";

const char kAutofillEnableFpanRiskBasedAuthenticationName[] =
    "Enable risk-based authentication for FPAN retrieval";
const char kAutofillEnableFpanRiskBasedAuthenticationDescription[] =
    "When enabled, server card retrieval will begin with a risk-based check "
    "instead of jumping straight to CVC or biometric auth.";

const char kAutofillEnableManualFallbackForVirtualCardsName[] =
    "Show manual fallback for virtual cards";
const char kAutofillEnableManualFallbackForVirtualCardsDescription[] =
    "When enabled, manual fallback will be enabled for virtual cards on "
    "Android.";

const char kAutofillEnableMerchantDomainInUnmaskCardRequestName[] =
    "Enable sending merchant domain in server card unmask requests";
const char kAutofillEnableMerchantDomainInUnmaskCardRequestDescription[] =
    "When enabled, requests to unmask cards will include a top-level "
    "merchant_domain parameter populated with the last origin of the main "
    "frame.";

const char kAutofillEnableMerchantOptOutClientSideUrlFilteringName[] =
    "Enable Autofill merchant opt-out client side URL filtering";
const char kAutofillEnableMerchantOptOutClientSideUrlFilteringDescription[] =
    "When enabled, client side URL filtering will be triggered for the "
    "merchant opt-out use-case, so that virtual card suggestions are not shown "
    "on websites that are opted-out of virtual cards.";

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

const char kAutofillEnableCardProductNameName[] =
    "Enable showing card product name";
const char kAutofillEnableCardProductNameDescription[] =
    "When enabled, card product name (instead of issuer network) will be shown "
    "in Payments Autofill UI.";

const char kAutofillEnableEmailOtpForVcnYellowPathName[] =
    "Enable email OTP authentication in the yellow path of the VCN retrieval "
    "flow";
const char kAutofillEnableEmailOtpForVcnYellowPathDescription[] =
    "When enabled, if the user encounters the yellow path (challenge path) in "
    "the VCN retrieval flow and the server denotes that the card is eligible "
    "for email OTP authentication, email OTP authentication will be offered as "
    "one of the challenge options.";

const char kAutofillEnableNewCardArtAndNetworkImagesName[] =
    "Enable showing new card art and network images";
const char kAutofillEnableNewCardArtAndNetworkImagesDescription[] =
    "When enabled, new and larger card art and network icons will be shown.";

const char kAutofillEnableOffersInClankKeyboardAccessoryName[] =
    "Enable Autofill offers in keyboard accessory";
const char kAutofillEnableOffersInClankKeyboardAccessoryDescription[] =
    "When enabled, offers will be displayed in the keyboard accessory when "
    "available.";

const char kAutofillEnablePaymentsAndroidBottomSheetName[] =
    "Autofill save card and VCN enrollment Bottom Sheets";
const char kAutofillEnablePaymentsAndroidBottomSheetDescription[] =
    "Displays save card and VCN enrollment in bottom sheets instead of info "
    "bars on Android.";

const char kAutofillEnablePaymentsMandatoryReauthName[] =
    "Enable mandatory re-auth for payments autofill";
const char kAutofillEnablePaymentsMandatoryReauthDescription[] =
    "When enabled, in use-cases where we would not have triggered any "
    "interactive authentication to autofill payment methods, we will trigger "
    "a device authentication.";

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

const char kAutofillEnableRemadeDownstreamMetricsName[] =
    "Enable remade Autofill Downstream metrics logging";
const char kAutofillEnableRemadeDownstreamMetricsDescription[] =
    "When enabled, some extra metrics logging for Autofill Downstream will "
    "start.";

const char kAutofillEnableServerIbanName[] =
    "Enable server-based IBAN uploading and autofilling";
const char kAutofillEnableServerIbanDescription[] =
    "When enabled, Autofill will attempt to offer upload save for IBANs "
    "(International Bank Account Numbers) and autofill server-based IBANs.";

const char kAutofillEnableStickyManualFallbackForCardsName[] =
    "Make manual fallback sticky for credit cards";
const char kAutofillEnableStickyManualFallbackForCardsDescription[] =
    "When enabled, if the user interacts with the manual fallback bottom "
    "sheet, it'll remain sticky until the user dismisses it.";

const char kAutofillEnableUpdateVirtualCardEnrollmentName[] =
    "Enable Update Virtual Card Enrollment";
const char kAutofillEnableUpdateVirtualCardEnrollmentDescription[] =
    "When enabled, the user will have the ability to update the virtual card "
    "enrollment of a credit card through their chrome browser after certain "
    "autofill flows (for example, downstream and upstream), and from the "
    "settings page.";

const char kAutofillEnableVirtualCardFidoEnrollmentName[] =
    "Enable FIDO enrollment for virtual cards";
const char kAutofillEnableVirtualCardFidoEnrollmentDescription[] =
    "When enabled, after a successful authentication to autofill a virtual "
    "card, the user will be prompted to opt-in to FIDO if the user is not "
    "currently opted-in, and if the user is opted-in already and the virtual "
    "card is FIDO eligible the user will be prompted to register the virtual "
    "card into FIDO.";

const char kAutofillEnableNewSaveCardBubbleUiName[] =
    "Update UI messaging and banner image for credit card upload save";
const char kAutofillEnableNewSaveCardBubbleUiDescription[] =
    "When enabled, the user will see a new banner logo and text in the bubble "
    "offering to upload their cards to Google Pay.";

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

const char kAutofillMoveLegalTermsAndIconForNewCardEnrollmentName[] =
    "Move legal terms for new card enrollment";
const char kAutofillMoveLegalTermsAndIconForNewCardEnrollmentDescription[] =
    "When enabled, legal terms will be moved before action buttons in autofill "
    "save card and virtual card enrollment bubbles and dialogs.";

const char kAutofillParseVcnCardOnFileStandaloneCvcFieldsName[] =
    "Parse standalone CVC fields for VCN card on file in forms";
const char kAutofillParseVcnCardOnFileStandaloneCvcFieldsDescription[] =
    "When enabled, Autofill will attempt to find standalone CVC fields for VCN "
    "card on file when parsing forms.";

const char kAutofillPreventOverridingPrefilledValuesName[] =
    "Prevent Autofill from overriding prefilled field values";
const char kAutofillPreventOverridingPrefilledValuesDescription[] =
    "When enabled, Autofill won't override any field values that have not been "
    "filled by Autofill";

const char kAutofillMoreProminentPopupName[] = "More prominent Autofill popup";
const char kAutofillMoreProminentPopupDescription[] =
    "If enabled Autofill's popup becomes more prominent, i.e. its shadow "
    "becomes more emphasized, position is also updated";

const char kAutofillShowAutocompleteDeleteButtonName[] =
    "Show a delete button for Autocomplete entries";
const char kAutofillShowAutocompleteDeleteButtonDescription[] =
    "When enabled, Autocomplete entries in filling popups will contain a "
    "delete button";

const char kAutofillSuggestServerCardInsteadOfLocalCardName[] =
    "Suggest Server card instead of Local card for deduped cards";
const char kAutofillSuggestServerCardInsteadOfLocalCardDescription[] =
    "When enabled, Autofill suggestions that consist of a local and server "
    "version of the same card will attempt to fill the server card upon "
    "selection instead of the local card.";

const char kAutofillTouchToFillForCreditCardsAndroidName[] =
    "Enable Touch To Fill bottomsheet for Autofill credit card suggestions";
const char kAutofillTouchToFillForCreditCardsAndroidDescription[] =
    "When enabled, Autofill credit card suggestions are shown on the "
    "Touch To Fill bottomsheet";

const char kAutofillUpdateChromeSettingsLinkToGPayWebName[] =
    "Update Chrome Settings Link to GPay Web";
const char kAutofillUpdateChromeSettingsLinkToGPayWebDescription[] =
    "When enabled, Chrome Settings link directs to GPay Web rather than "
    "Payments Center for payment methods management.";

const char kAutofillUpstreamAllowAdditionalEmailDomainsName[] =
    "Allow Autofill credit card upload save for select non-Google-based "
    "accounts";
const char kAutofillUpstreamAllowAdditionalEmailDomainsDescription[] =
    "When enabled, credit card upload is offered if the user's logged-in "
    "account's domain is from a common email provider.";

const char kAutofillUpstreamAllowAllEmailDomainsName[] =
    "Allow Autofill credit card upload save for all non-Google-based accounts";
const char kAutofillUpstreamAllowAllEmailDomainsDescription[] =
    "When enabled, credit card upload is offered without regard to the user's "
    "logged-in account's domain.";

const char kAutofillUseImprovedLabelDisambiguationName[] =
    "Autofill Uses Improved Label Disambiguation";
const char kAutofillUseImprovedLabelDisambiguationDescription[] =
    "When enabled, the Autofill dropdown's suggestions' labels are displayed "
    "using the improved disambiguation format.";

const char kAutofillVirtualCardsOnTouchToFillAndroidName[] =
    "Enable virtual cards on Touch To Fill bottomsheet for credit cards";
const char kAutofillVirtualCardsOnTouchToFillAndroidDescription[] =
    "When enabled, virtual credit card suggestions are shown on the Touch To "
    "Fill bottomsheet for credit cards.";

const char kAutofillVirtualViewStructureAndroidName[] =
    "Enable the setting to provide a virtual view structure for Autofill";
const char kAutofillVirtualViewStructureAndroidDescription[] =
    "When enabled, a setting allows to switch to using Android Autofill. Chrome"
    " then provides a virtual view structure but no own suggestions.";

const char kBackForwardCacheName[] = "Back-forward cache";
const char kBackForwardCacheDescription[] =
    "If enabled, caches eligible pages after cross-site navigations."
    "To enable caching pages on same-site navigations too, choose 'enabled "
    "same-site support'.";

const char kBiometricReauthForPasswordFillingName[] =
    "Biometric reauth for password filling";
const char kBiometricReauthForPasswordFillingDescription[] =
    "Enables biometric"
    "re-authentication before password filling";

const char kFailFastQuietChipName[] = "Fail fast quiet chip";
const char kFailFastQuietChipDescription[] =
    "Enables fast finalization of a permission request if it is displayed as a "
    "quiet chip.";

const char kBorealisBigGlName[] = "Borealis Big GL";
const char kBorealisBigGlDescription[] = "Enable Big GL when running Borealis.";

const char kBorealisDGPUName[] = "Borealis dGPU";
const char kBorealisDGPUDescription[] = "Enable dGPU when running Borealis.";

const char kBorealisForceBetaClientName[] = "Borealis Force Beta Client";
const char kBorealisForceBetaClientDescription[] =
    "Force the client to run its beta version.";

const char kBorealisForceDoubleScaleName[] = "Borealis Force Double Scale";
const char kBorealisForceDoubleScaleDescription[] =
    "Force the client to run in 2x visual zoom.";

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

const char kBorealisWebUIInstallerName[] = "Borealis WebUI Installer";
const char kBorealisWebUIInstallerDescription[] =
    "Use the new WebUI installer instead of views installer.";

const char kBypassAppBannerEngagementChecksName[] =
    "Bypass user engagement checks";
const char kBypassAppBannerEngagementChecksDescription[] =
    "Bypasses user engagement checks for displaying app banners, such as "
    "requiring that users have visited the site before and that the banner "
    "hasn't been shown recently. This allows developers to test that other "
    "eligibility requirements for showing app banners, such as having a "
    "manifest, are met.";

const char kServiceWorkerBypassFetchHandlerName[] =
    "Bypass Service Worker Fetch Handler";
const char kServiceWorkerBypassFetchHandlerDescription[] =
    "Bypass starting a service worker and its fetch handler when the fetch "
    "event meets the conditions. The service worker may start after sending a "
    "resource request, or conduct a race between the network request and its "
    "fetch handler. If the resource could be handled in the fetch handler, the "
    "feature may affect the page load. This feature will override "
    "chrome://flags/#service-worker-bypass-fetch-handler-for-main-resource";

const char kServiceWorkerBypassFetchHandlerForMainResourceName[] =
    "Bypass Service Worker Fetch Handler for main resource";
const char kServiceWorkerBypassFetchHandlerForMainResourceDescription[] =
    "Bypass starting a service worker and its fetch handler for main resource "
    "requests. The service worker starts after sending a main resource request "
    "and handles subresources. If the main resource could be handled in the "
    "fetch handler, the feature may affect the page load. This feature will be "
    "overridden by chrome://flags/#service-worker-bypass-fetch-handler";

const char kServiceWorkerStaticRouterName[] = "Service Worker Static Router";
const char kServiceWorkerStaticRouterDescription[] =
    "When enabled, Chrome will enable the Service Worker Static Routing API. "
    "https://chromestatus.com/feature/5185352976826368";

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
const char kCameraMicEffectsName[] = "Camera and Mic Effects";
const char kCameraMicEffectsDescription[] =
    "Enables effects for camera and mic streams.";
const char kCameraMicPreviewName[] = "Camera and Mic Preview";
const char kCameraMicPreviewDescription[] =
    "Enables camera and mic preview in permission bubble and site settings.";

const char kCcoTest1Name[] = "Cco Test1";
const char kCcoTest1Description[] = "Enables cco test 1";

#endif

const char kChromeLabsName[] = "Chrome Labs";
const char kChromeLabsDescription[] =
    "Access Chrome Labs through the toolbar menu to see featured user-facing "
    "experimental features.";

const char kChromeRefresh2023Id[] = "chrome-refresh-2023";
const char kChromeRefresh2023Name[] = "Chrome Refresh 2023";
const char kChromeRefresh2023Description[] = "Enables the new desktop design.";

const char kChromeWebuiRefresh2023Name[] = "Chrome WebUI Refresh 2023";
const char kChromeWebuiRefresh2023Description[] =
    "Enables Chrome Refresh 2023 styles for various WebUI surfaces.";

const char kChromeRefresh2023NTBName[] = "Chrome Refresh 2023 New Tab Button";
const char kChromeRefresh2023NTBDescription[] =
    "Enables the variations of the new tab button for chrome refresh 2023.";

const char kChromeRefresh2023TopChromeFontName[] =
    "Chrome Refresh 2023 Top Chrome Font Style ";
const char kChromeRefresh2023TopChromeFontDescription[] =
    "Enables the bolder version of font styles for Top Chrome components.";

const char kCommerceHintAndroidName[] = "Commerce Hint Android";
const char kCommerceHintAndroidDescription[] =
    "Enables commerce hint detection on Android.";

const char kConsumerAutoUpdateToggleAllowedName[] =
    "Allow Consumer Auto Update Toggle";
const char kConsumerAutoUpdateToggleAllowedDescription[] =
    "Allow enabling the consumer auto update toggle in settings";

const char kContextMenuSearchWithGoogleLensName[] =
    "Google Lens powered image search in the context menu.";
const char kContextMenuSearchWithGoogleLensDescription[] =
    "Replaces default image search with an intent to Google Lens when "
    "supported.";

const char kContextMenuGoogleLensSearchOptimizationsName[] =
    "Google Lens powered image search string variations in the context menu.";
const char kContextMenuGoogleLensSearchOptimizationsDescription[] =
    "Replaces Google Lens string variations when Google Lens is supported.";

const char kClearCrossSiteCrossBrowsingContextGroupWindowNameName[] =
    "Clear window name in top-level cross-site cross-browsing-context-group "
    "navigation";
const char kClearCrossSiteCrossBrowsingContextGroupWindowNameDescription[] =
    "Clear the preserved window.name property when it's a top-level cross-site "
    "navigation that swaps BrowsingContextGroup.";

const char kCreateShortcutIgnoresManifestName[] =
    "Create Shortcut ignores manifest";
const char kCreateShortcutIgnoresManifestDescription[] =
    "When the 'Create Shortcut' menu item is selected, use information from "
    "the current page, ignoring any web app manifest information that may be "
    "available.";

const char kDeviceForceScheduledRebootName[] =
    "Enable DeviceScheduledReboot policy for all sessions.";
const char kDeviceForceScheduledRebootDescription[] =
    "Schedule recurring reboot for the device. Reboots are always executed at "
    "a scheduled time. If the session is active, user will be notified about "
    "the reboot, but the reboot will not be delayed.";

const char kDevicePostureName[] = "Device Posture API";
const char kDevicePostureDescription[] =
    "Enables Device Posture API (foldable devices)";

const char kViewportSegmentsName[] = "Viewport Segments API";
const char kViewportSegmentsDescription[] =
    "Enable the viewport segment API, giving information about the logical "
    "segments of the device (dual screen and foldable devices)";

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

const char kMerchantWidePromotionsName[] = "Merchant wide promotions";
const char kMerchantWidePromotionsDescription[] =
    "Enables the discount consent for all merchants, and show merchant wide "
    "promotions if they are available";

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

const char kEnableAutoDisableAccessibilityName[] = "Auto-disable Accessibility";
const char kEnableAutoDisableAccessibilityDescription[] =
    "When accessibility APIs are no longer being requested, automatically "
    "disables accessibility. This might happen if an assistive technology is "
    "turned off or if an extension which uses accessibility APIs no longer "
    "needs them.";

const char kEnableAutoDisableAccessibilityV2Name[] =
    "Auto-disable Accessibility V2";
const char kEnableAutoDisableAccessibilityV2Description[] =
    "Automatically disable accessibility when Android reports no assistive "
    "technologies are running. Might break accessibility for assistive "
    "technologies without isAccessibilityTool set.";

const char kEnableAutofillAddressSavePromptName[] =
    "Autofill Address Save Prompts";
const char kEnableAutofillAddressSavePromptDescription[] =
    "Enable the Autofill address save prompts.";

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
    "When enabled and applicable, the act of compositing is delegated to Ash.";

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

const char kDesktopPWAsWebBundlesName[] = "Desktop PWAs Web Bundles";
const char kDesktopPWAsWebBundlesDescription[] =
    "Adds support for web bundles, making web apps able to be launched "
    "offline.";

const char kEnableTLS13EarlyDataName[] = "TLS 1.3 Early Data";
const char kEnableTLS13EarlyDataDescription[] =
    "This option enables TLS 1.3 Early Data, allowing GET requests to be sent "
    "during the handshake when resuming a connection to a compatible TLS 1.3 "
    "server.";

const char kEnableTLS13KyberName[] = "TLS 1.3 hybridized Kyber support";
const char kEnableTLS13KyberDescription[] =
    "This option enables a combination of X25519 and Kyber in TLS 1.3.";

const char kAccessibilityAcceleratorNotificationsTimeoutName[] =
    "Allows accelerator notifications for accessibility features to time out";
const char kAccessibilityAcceleratorNotificationsTimeoutDescription[] =
    "Enables notifications for accessibility features turned on by keyboard "
    "shortcut (docked magnifier, screen magnifier and high contrast) to time "
    "out instead of remaining pinned.";

const char kAccessibilityServiceName[] = "Experimental Accessibility Service";
const char kAccessibilityServiceDescription[] =
    "This option enables the experimental Accessibility Service and runs some "
    "accessibility features in the service.";

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

const char kAccessibilityDictationKeyboardImprovementsName[] =
    "Dictation keyboard improvements";
const char kAccessibilityDictationKeyboardImprovementsDescription[] =
    "This option enables Dictation keyboard improvements to enable a more "
    "seemless experience when pressing Search + D or the Dictation key on "
    "supported keyboards. This feature also includes some UI enhancements for "
    "Dictation, including notifications during error states.";

const char kAccessibilityGameFaceIntegrationName[] =
    "Experimental GameFace integration";
const char kAccessibilityGameFaceIntegrationDescription[] =
    "This option enables the experimental GameFace ChromeOS integration";

const char kAccessibilitySelectToSpeakHoverTextImprovementsName[] =
    "Select-to-Speak Hover Text Improvements";
const char kAccessibilitySelectToSpeakHoverTextImprovementsDescription[] =
    "This option enables improvements in the text shown when hovering over the "
    "Select-to-Speak feature icon in the system tray.";

const char kMacCoreLocationBackendName[] = "Core Location Backend";
const char kMacCoreLocationBackendDescription[] =
    "Enables usage of the Core Location APIs as the backend for Geolocation "
    "API";

const char kNewMacNotificationAPIName[] =
    "Determines which notification API to use on macOS devices";
const char kNewMacNotificationAPIDescription[] =
    "Enables the usage of Apple's new notification API";

const char kWinrtGeolocationImplementationName[] =
    "WinRT Geolocation Implementation";
const char kWinrtGeolocationImplementationDescription[] =
    "Enables usage of the Windows.Devices.Geolocation WinRT APIs on Windows "
    "for geolocation";

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

const char kEnableFencedFramesReportingAttestationsChangeName[] =
    "Enable Fenced Frames reporting attestations changes";
const char kEnableFencedFramesReportingAttestationsChangeDescription[] =
    "Relax the attestation requirement of post-impression beacons from "
    "Protected Audience only to either Protected Audience or Attribution "
    "Reporting.";

const char kEnableGamepadButtonAxisEventsName[] =
    "Gamepad Button and Axis Events";
const char kEnableGamepadButtonAxisEventsDescription[] =
    "Enables the ability to subscribe to changes in buttons and/or axes "
    "on the gamepad object.";

const char kEnableGamepadMultitouchName[] = "Gamepad Multitouch";
const char kEnableGamepadMultitouchDescription[] =
    "Enables the ability to receive input from multitouch surface "
    "on the gamepad object.";

const char kEnableGamepadTriggerRumbleName[] = "Gamepad Trigger Rumble";
const char kEnableGamepadTriggerRumbleDescription[] =
    "Enables the Gamepad API extension for trigger rumble. See "
    "https://chromestatus.com/feature/5162940951953408";

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
#endif

const char kEnableIsolatedWebAppDevModeName[] =
    "Enable Isolated Web App Developer Mode";
const char kEnableIsolatedWebAppDevModeDescription[] =
    "Enables the installation of unverified Isolated Web Apps";

const char kEnableShortcutCustomizationAppName[] =
    "Enable shortcut customization app";
const char kEnableShortcutCustomizationAppDescription[] =
    "Enable the shortcut customization SWA, allowing users to customize system "
    "shortcuts.";

const char kEnableShortcutCustomizationName[] =
    "Enable customization in new shortcuts app";
const char kEnableShortcutCustomizationDescription[] =
    "Enable customization of shortcuts in the new shortcuts app.";

const char kEnableSearchCustomizableShortcutsInLauncherName[] =
    "Enable search for customizable shortcuts in launcher";
const char kEnableSearchCustomizableShortcutsInLauncherDescription[] =
    "Enable searching for customizable shortcuts in launcher.";

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

const char kExperimentalRgbKeyboardPatternsName[] =
    "Enable experimental RGB Keyboard patterns support";
const char kExperimentalRgbKeyboardPatternsDescription[] =
    "Enable experimental RGB Keyboard patterns support on supported devices.";

const char kDownloadRangeName[] = "Enable download range support";
const char kDownloadRangeDescription[] =
    "Enables arbitrary download range request support.";

const char kEarlyDocumentSwapForBackForwardTransitionsName[] =
    "Early document swap for back/forward navigations";
const char kEarlyDocumentSwapForBackForwardTransitionsDescription[] =
    "Enable early swapping of RenderFrameHosts during some back/forward "
    "navigations. "
    "This is a highly experimental feature intended to support new kinds of "
    "navigation transitions. When enabled, the old document will be unloaded "
    "shortly after starting some back/forward navigations to a new document, "
    "without waiting for the new navigation to complete.";

const char kEnableFriendlierSafeBrowsingSettingsEnhancedProtectionName[] =
    "Enable friendlier safe browsing settings enhanced protection";
const char
    kEnableFriendlierSafeBrowsingSettingsEnhancedProtectionDescription[] =
        "Updates the text, layout, icons, and links on both the privacy guide "
        "and the security settings page.";

const char kEnableFriendlierSafeBrowsingSettingsStandardProtectionName[] =
    "Enable friendlier safe browsing settings standard protection";
const char
    kEnableFriendlierSafeBrowsingSettingsStandardProtectionDescription[] =
        "Updates the text and layout on both the privacy guide and the "
        "security settings page.";

const char kEnableRedInterstitialFaceliftName[] = "Red interstitial facelift";
const char kEnableRedInterstitialFaceliftDescription[] =
    "Enables red interstitial facelift UI changes, including icon, string, and "
    "style changes.";

const char kEnableTailoredSecurityUpdatedMessagesName[] =
    "Enable tailored security updated messages";
const char kEnableTailoredSecurityUpdatedMessagesDescription[] =
    "Updates the tailored security dialog strings and icons for their "
    "respective platforms.";

const char kEnableNetworkLoggingToFileName[] = "Enable network logging to file";
const char kEnableNetworkLoggingToFileDescription[] =
    "Enables network logging to a file named netlog.json in the user data "
    "directory. The file can be imported into chrome://net-internals.";

const char kEnableNewDownloadBackendName[] = "Enable new download backend";
const char kEnableNewDownloadBackendDescription[] =
    "Enables the new download backend that uses offline content provider";

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

const char kEnableWebUsbOnExtensionServiceWorkerName[] =
    "Enable WebUSB on extension service workers";
const char kEnableWebUsbOnExtensionServiceWorkerDescription[] =
    "When enabled, WebUSB API is available on extension service workers.";

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

const char kPipPinchToResizeName[] = "Enable pinch to resize PiP windows";
const char kPipPinchToResizeDescription[] =
    "Enables pinch gesture on existing PiP windows to move and "
    "resize them.";

const char kPipTiltName[] = "Enable tilt for PiP windows";
const char kPipTiltDescription[] =
    "Enables window tilting using pinch gesture on existing PiP windows. "
    "This requires #enable-pip-pinch-to-resize to also be enabled.";

const char kPipTuckName[] = "Enable tuck for PiP windows";
const char kPipTuckDescription[] = "Enables window tucking for PiP windows. ";

const char kPrivateNetworkAccessSendPreflightsName[] =
    "Send Private Network Access preflights";
const char kPrivateNetworkAccessSendPreflightsDescription[] =
    "Enables sending Private Network Access preflights ahead of requests to "
    "more-private IP addresses. Failed preflights display warnings in DevTools "
    "without failing entire request. See also: "
    "https://developer.chrome.com/blog/private-network-access-preflight/";

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
    "milliseconds. The default timeout period for requests is 5 minutes."
    "See also: "
    "https://developer.chrome.com/blog/private-network-access-preflight/";

const char kPrivateNetworkAccessPermissionPromptName[] =
    "Enable Permission Prompt for Private Network Access";
const char kPrivateNetworkAccessPermissionPromptDescription[] =
    "Enable Permission Prompt for HTTPS public websites accessing HTTP "
    "more-private devices. Require to set a fetch option `targetAddressSpace` "
    "on the request to relax mixed content check."
    "See also: "
    "https://developer.chrome.com/blog/"
    "private-network-access-update-2023-02-02/";

const char kDeprecateAltClickName[] =
    "Enable Alt+Click deprecation notifications";
const char kDeprecateAltClickDescription[] =
    "Start providing notifications about Alt+Click deprecation and enable "
    "Search+Click as an alternative.";

const char kDeprecateAltBasedSixPackName[] =
    "Deprecate Alt based six-pack (PgUp, PgDn, Home, End, Delete, Insert)";
const char kDeprecateAltBasedSixPackDescription[] =
    "Show deprecation notifications and disable functionality for Alt based "
    "six pack deprecations. The Search based versions continue to work.";

const char kDeprecateOldKeyboardShortcutsAcceleratorName[] =
    "Enable deprecation notifications for Ctrl+Alt+/ to open Keyboard "
    "shortcuts app";
const char kDeprecateOldKeyboardShortcutsAcceleratorDescription[] =
    "Show deprecation notifications and disable functionality for Ctrl+Alt+/ "
    "as the shortcut to open the Keyboard shortcuts app. The new shortcut is "
    "Ctrl+Search+S.";

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

const char kEditContextName[] = "EditContext API";
const char kEditContextDescription[] =
    "Allows web pages to use the experimental EditContext API to better "
    "control text input.";

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

const char kEnableWebAuthenticationChromeOSAuthenticatorName[] =
    "ChromeOS platform Web Authentication support";
const char kEnableWebAuthenticationChromeOSAuthenticatorDescription[] =
    "Enable the ChromeOS platform authenticator for the Web Authentication "
    "API.";

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

const char kEnablePolicyTestPageName[] =
    "Enable access to the policy test page";
const char kEnablePolicyTestPageDescription[] =
    "When enabled, allows the policy test page to be accessed at "
    "chrome://policy/test.";

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

#if BUILDFLAG(ENABLE_EXTENSIONS)
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
const char kWebViewTagMPArchBehaviorName[] =
    "MPArch behavior change for <webview> tags";
const char kWebViewTagMPArchBehaviorDescription[] =
    "Enables a behavior change associated with the migration of "
    "<webview> tags to MPArch. See https://crbug.com/1261928. Specifically, "
    "cross-WebContents newwindow event usage invalidates some window.open "
    "usage within <webview>s. For enterprise, the "
    "ChromeAppsWebViewPermissiveBehaviorAllowed policy serves as an escape "
    "hatch during the roll out of this change.";

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

const char kSafetyCheckExtensionsName[] = "Extensions Module in Safety Check";
const char kSafetyCheckExtensionsDescription[] =
    "When enabled, adds the Extensions Module to Safety Check on "
    "desktop. The module will be shown if there are potentially unsafe "
    "extensions to review.";

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

const char kFedCmAutoSelectedFlagName[] = "FedCmAutoSelectedFlag";
const char kFedCmAutoSelectedFlagDescription[] =
    "Allows the browser to share whether an identity credential was "
    "auto-selected with developers post user permission to continue with the "
    "IdP.";

const char kFedCmErrorName[] = "FedCmError";
const char kFedCmErrorDescription[] =
    "Enables IDPs to show information about an error.";

const char kFedCmDomainHintName[] = "FedCmDomainHint";
const char kFedCmDomainHintDescription[] =
    "Enables RPs to request only FedCM invocations to only show accounts "
    "matching a given domain.";

const char kFedCmIdPRegistrationName[] = "FedCM with IdP Registration support";
const char kFedCmIdPRegistrationDescription[] =
    "Enables RPs to get identity credentials from registered IdPs.";

const char kFedCmLogoutRpsName[] = "FedCM with logoutRPs";
const char kFedCmLogoutRpsDescription[] =
    "Enables an IDP to declare itself logged out and request front-channel "
    "logout.";

const char kFedCmMetricsEndpointName[] = "FedCmMetricsEndpoint";
const char kFedCmMetricsEndpointDescription[] =
    "Allows the FedCM API to send performance measurement to the metrics "
    "endpoint on the identity provider side. Requires FedCM to be enabled.";

const char kFedCmMultiIdpName[] = "FedCmMultiIdp";
const char kFedCmMultiIdpDescription[] =
    "Allows the FedCM API to request multiple identity providers "
    "simultaneously. Requires FedCM to be enabled as well.";

const char kFedCmRevokeName[] = "FedCmRevoke";
const char kFedCmRevokeDescription[] =
    "Enables the IdentityCredential.revoke() API which allows revoking "
    "accounts created via federated login through FedCM.";

const char kFedCmSelectiveDisclosureName[] = "FedCmSelectiveDisclosure";
const char kFedCmSelectiveDisclosureDescription[] =
    "Allows a relying party to selectively request a set of identity "
    "attributes to be disclosed.";

const char kFedCmWithoutThirdPartyCookiesName[] =
    "FedCmWithoutThirdPartyCookies";
const char kFedCmWithoutThirdPartyCookiesDescription[] =
    "Allows the FedCM API to be enabled when third party cookies are disabled.";

const char kFedCmWithoutWellKnownEnforcementName[] =
    "FedCmWithoutWellKnownEnforcement";
const char kFedCmWithoutWellKnownEnforcementDescription[] =
    "Supports configURL that's not in the IdP's .well-known file.";

const char kFedCmIdpSigninStatusName[] = "FedCmIdpSigninStatus";
const char kFedCmIdpSigninStatusDescription[] =
    "Enables the FedCM IDP sign-in status API that allows IDPs to notify the "
    "browser about the user's sign-in status.";

const char kWebIdentityDigitalCredentialsName[] = "DigitalCredentials";
const char kWebIdentityDigitalCredentialsDescription[] =
    "Enables the three-party verifier/holder/issuer identity model.";

const char kFileHandlingIconsName[] = "File Handling Icons";
const char kFileHandlingIconsDescription[] =
    "Allows websites using the file handling API to also register file type "
    "icons. See https://github.com/WICG/file-handling/blob/main/explainer.md "
    "for more information.";

const char kFileSystemAccessLockingSchemeName[] = "File system lock modes";
const char kFileSystemAccessLockingSchemeDescription[] =
    "Allows the creation of FileSystemSyncAccessHandle and "
    "FileSystemWritableFileStream in new locking modes. See "
    "https://github.com/whatwg/fs/blob/main/proposals/"
    "MultipleReadersWriters.md for more information.";

const char kFileSystemAccessPersistentPermissionName[] =
    "Persistent Permission for File System Access API";
const char kFileSystemAccessPersistentPermissionDescription[] =
    "Allows users to opt in to keep the file system permission persistent "
    "across visits and to restore recently granted file permissions.";

const char kFileSystemObserverName[] = "FileSystemObserver";
const char kFileSystemObserverDescription[] =
    "Enables the FileSystemObserver interface, which allows websites to be "
    "notified of changes to the file system. See "
    "https://github.com/whatwg/fs/blob/main/proposals/FileSystemObserver.md "
    "for more information.";

#if BUILDFLAG(IS_ANDROID)
const char kFillingAcrossAffiliatedWebsitesName[] =
    "Fill passwords across affiliated websites.";
const char kFillingAcrossAffiliatedWebsitesDescription[] =
    "Enables filling password on a website when there is saved "
    "password on affiliated website.";
#endif

const char kDrawImmediatelyWhenInteractiveName[] =
    "Enable Immediate Draw When Interactive";
const char kDrawImmediatelyWhenInteractiveDescription[] =
    "Causes viz to activate and draw frames immediately during a touch "
    "interaction or scroll.";

const char kFillingAcrossGroupedSitesName[] =
    "Password filling across grouped websites";
const char kFillingAcrossGroupedSitesDescription[] =
    "This flag enables password filling across grouped websites. Information "
    "about website groups is provided by the affiliation service.";

const char kFluentScrollbarsName[] = "Windows Fluent scrollbars.";
const char kFluentScrollbarsDescription[] =
    "Stylizes scrollbars with Microsoft Fluent design.";

const char kMutationEventsName[] =
    "Enable (deprecated) synchronous mutation events";
const char kMutationEventsDescription[] =
    "Mutation Events are a deprecated set of events which cause performance "
    "issues. Disabling this feature turns off Mutation Events. NOTE: Disabling "
    "these events can cause breakage on some sites that are still reliant on "
    "these deprecated features.";

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

const char kForgotPasswordFormSupportName[] = "Forgot Password Form Support";
const char kForgotPasswordFormSupportDescription[] =
    "Detect and fill usernames in forgot password forms.";

const char kFullscreenPopupWindowsName[] = "Fullscreen popup windows";
const char kFullscreenPopupWindowsDescription[] =
    "Enables sites with Window Management permission to open fullscreen popup "
    "windows with a `fullscreen` window.open() features parameter. See "
    "https://chromestatus.com/feature/6002307972464640 for more information.";

const char kGalleryAppPdfEditNotificationName[] =
    "Gallery App Pdf Edit Notification";
const char kGalleryAppPdfEditNotificationDescription[] =
    "Shows a notification to provide an option to open Gallery app for a "
    "downloaded pdf file";

const char kMediaRemotingWithoutFullscreenName[] =
    "Media Remoting without videos in fullscreen mode";
const char kMediaRemotingWithoutFullscreenDescription[] =
    "Starts Media Remoting from Global Media Controls without making the "
    "videos fullscreen.";

const char kRemotePlaybackBackendName[] = "Remote Playback API implementation";
const char kRemotePlaybackBackendDescription[] =
    "Enables the Remote Playback API implementation.";

#if BUILDFLAG(IS_CHROMEOS)
const char kGlobalMediaControlsCrOSUpdatedUIName[] =
    "Global Media Controls CrOS updated UI";
const char kGlobalMediaControlsCrOSUpdatedUIDescription[] =
    "Show updated UI for Global Media Controls in CrOS.";
#endif

const char kGoogleOneOfferFilesBannerName[] = "Google One offer Files banner";
const char kGoogleOneOfferFilesBannerDescription[] =
    "Shows a Files banner about Google One offer.";

const char kObservableAPIName[] = "Observable API";
const char kObservableAPIDescription[] =
    "A reactive programming primitive for ergonomically handling streams of "
    "async data. See https://github.com/WICG/observable.";

const char kOpenscreenCastStreamingSessionName[] =
    "Enable Open Screen Library (libcast) as the Mirroring Service's Cast "
    "Streaming implementation";
const char kOpenscreenCastStreamingSessionDescription[] =
    "Enables Open Screen Library's (libcast) Cast Streaming implementation to "
    "be used for negotiating and executing mirroring and remoting sessions.";

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

const char kCastStreamingPerformanceOverlayName[] =
    "Toggle a performance metrics overlay while Cast Streaming";
const char kCastStreamingPerformanceOverlayDescription[] =
    "When enabled, a text overlay is rendered on top of each frame sent while "
    "Cast Streaming that includes frame duration, resolution, timestamp, "
    "low latency mode, capture duration, target playout delay, target bitrate, "
    "and encoder utilitization.";

const char kCastStreamingVp9Name[] =
    "Enable VP9 video encoding for Cast Streaming";
const char kCastStreamingVp9Description[] =
    "Offers the VP9 video codec when negotiating Cast Streaming, and uses VP9 "
    "if selected for the session.";

const char kCastEnableStreamingWithHiDPIName[] =
    "HiDPI tab capture support for Cast Streaming";
const char kCastEnableStreamingWithHiDPIDescription[] =
    "Enables HiDPI tab capture during Cast Streaming mirroring sessions. May "
    "reduce performance on some platforms and also improve quality of video "
    "frames.";

const char kGpuRasterizationName[] = "GPU rasterization";
const char kGpuRasterizationDescription[] = "Use GPU to rasterize web content.";

const char kContextualPageActionsName[] = "Contextual page actions";
const char kContextualPageActionsDescription[] =
    "Enables contextual page action feature.";

const char kContextualPageActionsPriceTrackingName[] =
    "Contextual page actions - price tracking";
const char kContextualPageActionsPriceTrackingDescription[] =
    "Enables price tracking as a contextual page action.";

const char kContextualPageActionsReaderModeName[] =
    "Contextual page actions - reader mode";
const char kContextualPageActionsReaderModeDescription[] =
    "Enables reader mode as a contextual page action.";

const char kContextualPageActionsShareModelName[] =
    "Contextual page actions - share model";
const char kContextualPageActionsShareModelDescription[] =
    "Enables share model data collection.";

const char kEnableOsIntegrationSubManagersName[] =
    "OS Integration sub managers";
const char kEnableOsIntegrationSubManagersDescription[] =
    "Enable OS integration sub managers to either just write new OS "
    "integration states to DB or execute on the OS integration states before "
    "writing to the DB";

const char kHandwritingGestureEditingName[] = "Handwriting Gestures Editing";
const char kHandwritingGestureEditingDescription[] =
    "Enables editing with handwriting gestures within the virtual keyboard.";

const char kHandwritingLegacyRecognitionName[] =
    "Handwriting Legacy Recognition";
const char kHandwritingLegacyRecognitionDescription[] =
    "Enables new on-device recognition for handwriting legacy paths.";

const char kHandwritingLibraryDlcName[] =
    "Handwriting recognition with library from DLC";
const char kHandwritingLibraryDlcDescription[] =
    "Enables new on-device recognition with the handwriting library installed "
    "from DLC";

const char kHardwareMediaKeyHandling[] = "Hardware Media Key Handling";
const char kHardwareMediaKeyHandlingDescription[] =
    "Enables using media keys to control the active media session. This "
    "requires MediaSessionService to be enabled too";

const char kHeavyAdPrivacyMitigationsName[] = "Heavy ad privacy mitigations";
const char kHeavyAdPrivacyMitigationsDescription[] =
    "Enables privacy mitigations for the heavy ad intervention. Disabling "
    "this makes the intervention deterministic. Defaults to enabled.";

const char kHeavyAdInterventionName[] = "Heavy Ad Intervention";
const char kHeavyAdInterventionDescription[] =
    "Unloads ads that use too many device resources.";

const char kHideIncognitoMediaMetadataName[] =
    "Hide media metadata when in Incognito";
const char kHideIncognitoMediaMetadataDescription[] =
    "When enabled, media metadata will be hidden from your OS' media player "
    "if you are in an Incognito session.";

const char kTabAudioMutingName[] = "Tab audio muting UI control";
const char kTabAudioMutingDescription[] =
    "When enabled, the audio indicators in the tab strip double as tab audio "
    "mute controls.";

const char kCrasSplitAlsaUsbInternalName[] =
    "CRAS Split USB/Internal refactor control";
const char kCrasSplitAlsaUsbInternalDescription[] =
    "When enable, CRAS will create different iodev with USB and internal "
    "device.";

const char kPwaRestoreUiName[] = "Enable the PWA Restore UI";
const char kPwaRestoreUiDescription[] =
    "When enabled, the PWA Restore UI can be shown";

const char kRestoreTabsOnFREName[] = "Restore tabs on FRE";
const char kRestoreTabsOnFREDescription[] =
    "Enable promo sheet to indicate tabs from synced devices can be restored";

const char kRestoreSyncedPlaceholderTabsName[] =
    "Restore synced placeholder tabs";
const char kRestoreSyncedPlaceholderTabsDescription[] =
    "When enabled, any placeholder tabs missing from the local session will be "
    "restored.";

const char kStartSurfaceReturnTimeName[] = "Start surface return time";
const char kStartSurfaceReturnTimeDescription[] =
    "Enable showing start surface at startup after specified time has elapsed";

const char kStorageBucketsName[] = "Storage Buckets API";
const char kStorageBucketsDescription[] =
    "Enable experimental Storage Buckets API, allowing websites to create "
    "multiple buckets of storage to organize their data, allowing user agents "
    "to delete each bucket independently of other buckets.";

const char kHttpsFirstModeV2Name[] = "HTTPS-First Mode V2";
const char kHttpsFirstModeV2Description[] =
    "Enable rearchitected version of HTTPS-First Mode.";

const char kHttpsFirstModeV2ForEngagedSitesName[] =
    "HTTPS-First Mode V2 For Engaged Sites";
const char kHttpsFirstModeV2ForEngagedSitesDescription[] =
    "Enable Site-Engagement based HTTPS-First Mode. Shows HTTPS-First Mode "
    "interstitial on sites whose HTTPS URLs have high Site Engagement scores. "
    "Requires #https-upgrades feature to be enabled";

const char kHttpsUpgradesName[] = "HTTPS Upgrades";
const char kHttpsUpgradesDescription[] =
    "Enable automatically upgrading all top-level navigations to HTTPS with "
    "fast fallback to HTTP.";

const char kIgnoreGpuBlocklistName[] = "Override software rendering list";
const char kIgnoreGpuBlocklistDescription[] =
    "Overrides the built-in software rendering list and enables "
    "GPU-acceleration on unsupported system configurations.";

const char kIgnoreSyncEncryptionKeysLongMissingName[] =
    "Ignore Chrome Sync encryption keys long missing";
const char kIgnoreSyncEncryptionKeysLongMissingDescription[] =
    "Drops pending encrypted updates if their key has been missing for a "
    "(configurable) number of consecutive GetUpdates. Restarting the browser "
    "resets the counter. The threshold is configurable via the "
    "MinGuResponsesToIgnoreKey feature parameter.";

const char kImprovedKeyboardShortcutsName[] =
    "Enable improved keyboard shortcuts";
const char kImprovedKeyboardShortcutsDescription[] =
    "Ensure keyboard shortcuts work consistently with international keyboard "
    "layouts and deprecate legacy shortcuts.";

const char kIncognitoDownloadsWarningName[] =
    "Enable Incognito downloads warning";
const char kIncognitoDownloadsWarningDescription[] =
    "When enabled, users will be warned that downloaded files are saved on the "
    "device and might be seen by other users even if they are in Incognito.";

const char kIncognitoReauthenticationForAndroidName[] =
    "Enable device reauthentication for Incognito.";
const char kIncognitoReauthenticationForAndroidDescription[] =
    "When enabled, a setting appears in Settings > Privacy and Security, to "
    "enable reauthentication for accessing your existing Incognito tabs.";

const char kIncognitoNtpRevampName[] = "Revamped Incognito New Tab Page";
const char kIncognitoNtpRevampDescription[] =
    "When enabled, Incognito new tab page will have an updated UI";

const char kIncognitoScreenshotName[] = "Incognito Screenshot";
const char kIncognitoScreenshotDescription[] =
    "Enables Incognito screenshots on Android. It will also make Incognito "
    "thumbnails visible. This flag will be ignored on API version >= 33.";

const char kIndexedDBCompressValuesWithSnappy[] = "Compress IndexedDB values";
const char kIndexedDBCompressValuesWithSnappyDescription[] =
    "Compress IndexedDB values in the renderer process using Snappy.";

const char kIndexedDBDefaultDurabilityRelaxed[] =
    "IndexedDB transactions relaxed durability by default";
const char kIndexedDBDefaultDurabilityRelaxedDescription[] =
    "IDBTransaction \"readwrite\" transaction durability defaults to relaxed "
    "when not specified";

const char kInfobarScrollOptimizationName[] = "Infobar scroll optimiaztion";
const char kInfobarScrollOptimizationDescription[] =
    "Optimize Infobar scroll on Android.";

const char kInProductHelpDemoModeChoiceName[] = "In-Product Help Demo Mode";
const char kInProductHelpDemoModeChoiceDescription[] =
    "Selects the In-Product Help demo mode.";

const char kInProductHelpSnoozeName[] = "In-Product Help Snooze";
const char kInProductHelpSnoozeDescription[] =
    "Enables the snooze button on In-Product Help.";

const char kInProductHelpUseClientConfigName[] = "IPH Use Client Config";
const char kInProductHelpUseClientConfigDescription[] =
    "Enable In-Product Help to use client side configuration.";

const char kInsecureDownloadWarningsName[] = "Warn on insecure downloads";
const char kInsecureDownloadWarningsDescription[] =
    "Enables insecure download warnings. Requires users to bypass a warning "
    "when they attempt to download a file over insecure transports (e.g. HTTP) "
    "either directly or via an insecure redirect.";

const char kInstallIsolatedWebAppFromUrl[] =
    "Install Isolated Web App from Proxy URL";
const char kInstallIsolatedWebAppFromUrlDescription[] =
    "Installs a new developer mode Isolated Web App whose contents are hosted "
    "at the provided HTTP(S) URL.";

const char kInstantHotspotRebrandName[] = "Instant Hotspot Improvements";

const char kInstantHotspotRebrandDescription[] =
    "Enables Instant Hotspot rebrand/feature improvements.";

const char kIpProtectionProxyOptOutName[] = "Disable IP Protection Proxy";
const char kIpProtectionProxyOptOutDescription[] =
    "When disabled, prevents use of the IP Protection proxy. This is intended "
    "to help with diagnosing any issues that could be caused by the feature "
    "being enabled. For the current status of this feature, see: "
    "https://chromestatus.com/feature/5111460239245312";
const char kIpProtectionProxyOptOutChoiceDefault[] = "Default";
const char kIpProtectionProxyOptOutChoiceOptOut[] = "Disabled";

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

const char kJavascriptExperimentalSharedMemoryName[] =
    "Experimental JavaScript shared memory features";
const char kJavascriptExperimentalSharedMemoryDescription[] =
    "Enable web pages to use non-standard, experimental JavaScript shared "
    "memory features. Their use requires the same HTTP headers required by "
    "cross-thread usage of SharedArrayBuffers (i.e. COOP and COEP).";

const char kJourneysName[] = "History Journeys";
const char kJourneysDescription[] = "Enables the History Journeys UI.";

const char kRenameJourneysName[] = "Rename History Journeys";
const char kRenameJourneysDescription[] = "Renames History Journeys in the UI.";

const char kJourneysLabelsName[] = "History Journeys Labels";
const char kJourneysLabelsDescription[] =
    "Enables labels for Journeys within the History Journeys UI.";

const char kJourneysShowAllClustersName[] =
    "History Journeys Show All Clusters";
const char kJourneysShowAllClustersDescription[] =
    "Enables all Journeys clusters to be shown on prominent UI surfaces";

const char kJourneysIncludeSyncedVisitsName[] =
    "History Journeys Include SyncedVisits";
const char kJourneysIncludeSyncedVisitsDescription[] =
    "Enabled synced visits to be included in History Journeys clusters.";

const char kJourneysZeroStateFilteringName[] =
    "History Journeys Zero-State Filtering";
const char kJourneysZeroStateFilteringDescription[] =
    "Enables filtering of clusters in the zero state of the History Journeys "
    "WebUI.";

const char kExtractRelatedSearchesFromPrefetchedZPSResponseName[] =
    "Extract Related Searches from Prefetched ZPS Response";
const char kExtractRelatedSearchesFromPrefetchedZPSResponseDescription[] =
    "Enables page annotation logic to source related searches data from "
    "prefetched ZPS responses";

const char kLargeFaviconFromGoogleName[] = "Large favicons from Google";
const char kLargeFaviconFromGoogleDescription[] =
    "Request large favicons from Google's favicon service";

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
    "allow browser trace back to the top level URL instead.";

const char kLensCameraAssistedSearchName[] =
    "Google Lens in Omnibox and New Tab Page";
const char kLensCameraAssistedSearchDescription[] =
    "Enable an entry point to Google Lens to allow users to search what they "
    "see using their mobile camera.";

const char kLensRegionSearchStaticPageName[] =
    "Use a static page with the Lens region search feature.";
const char kLensRegionSearchStaticPageDescription[] =
    "Enables use of a static page in a new tab when using the Lens region "
    "search feature.";

const char kLensImageFormatOptimizationsName[] = "Lens Optimized Image Formats";
const char kLensImageFormatOptimizationsDescription[] =
    "Enables the use of either WebP or JPEG on all Lens quries to reduce "
    "network load and improve latency";

const char kLensImageTranslateName[] =
    "Translate text in images with Google Lens";
const char kLensImageTranslateDescription[] =
    "Enables a context menu item to translate text in images using Google "
    "Lens. The context menu item appears when the current page is being "
    "translated.";

const char kEnableLensPingName[] =
    "Enable the ping to Lens before Lens requests";
const char kEnableLensPingDescription[] =
    "Enables a ping to the Lens Standalone server before a request is sent. "
    "This ping is used to proactively set cookies needed by Lens.";

const char kCscCompanionEnablePageContentName[] = "CSC Page Contents";
const char kCscCompanionEnablePageContentDescription[] =
    "Share the page contents with Chrome search companion.";

const char kCscForceCompanionPinnedStateName[] = "CSC Pin State";
const char kCscForceCompanionPinnedStateDescription[] = "";

const char kCscSidePanelCompanionName[] = "CSC";
const char kCscSidePanelCompanionDescription[] = "Chrome search companion.";

const char kCscVisualSearchSuggestionsName[] = "CSC-VSS";
const char kCscVisualSearchSuggestionsDescription[] = "";

const char kLogJsConsoleMessagesName[] =
    "Log JS console messages in system logs";
const char kLogJsConsoleMessagesDescription[] =
    "Enable logging JS console messages in system logs, please note that they "
    "may contain PII.";

const char kUndoAutofillName[] = "Undo Autofill";
const char kUndoAutofillDescription[] =
    "Allows reverting Autofill filling operations. Replaces Clear Form "
    "functionality";

const char kUnthrottledNestedTimeoutName[] =
    "Increase the nesting threshold before which setTimeout(..., <4ms) start "
    "being clamped.";
const char kUnthrottledNestedTimeoutDescription[] =
    "setTimeout(..., 0) is commonly used to break down long Javascript tasks. "
    "Under this flag, setTimeouts and setIntervals with an interval < 4ms are "
    "not clamped as aggressively. This improves short horizon performance, but "
    "websites abusing the API will still eventually have their setTimeouts "
    "clamped.";

const char kMediaRouterCastAllowAllIPsName[] =
    "Connect to Cast devices on all IP addresses";
const char kMediaRouterCastAllowAllIPsDescription[] =
    "Have the Media Router connect to Cast devices on all IP addresses, not "
    "just RFC1918/RFC4193 private addresses.";

const char kMetricsSettingsAndroidName[] = "Metrics Settings on Android";
const char kMetricsSettingsAndroidDescription[] =
    "Enables the new design of metrics settings.";

const char kMojoLinuxChannelSharedMemName[] =
    "Enable Mojo Shared Memory Channel";
const char kMojoLinuxChannelSharedMemDescription[] =
    "If enabled Mojo on Linux based platforms can use shared memory as an "
    "alternate channel for most messages.";

const char kCanvas2DLayersName[] =
    "Enables canvas 2D methods BeginLayer and EndLayer";
const char kCanvas2DLayersDescription[] =
    "Enables the canvas 2D methods BeginLayer and EndLayer.";

const char kEnableMachineLearningModelLoaderWebPlatformApiName[] =
    "Enables Machine Learning Model Loader Web Platform API";
const char kEnableMachineLearningModelLoaderWebPlatformApiDescription[] =
    "Enables the Machine Learning Model Loader Web Platform API.";

#if !BUILDFLAG(IS_ANDROID)
const char kEnableMantaServiceName[] = "Enable Manta Service";
const char kEnableMantaServiceDescription[] =
    "Enables the profile keyed Manta service at startup.";
#endif  // !BUILDFLAG(IS_ANDROID)

const char kSystemProxyForSystemServicesName[] =
    "Enable system-proxy for selected system services";
const char kSystemProxyForSystemServicesDescription[] =
    "Enabling this flag will allow ChromeOS system service which require "
    "network connectivity to use the system-proxy daemon for authentication to "
    "remote HTTP web proxies.";

const char kNotificationInteractionHistoryName[] =
    "Notification Interaction History";
const char kNotificationInteractionHistoryDescription[] =
    "Enable recording notification count and interaction.";

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

const char kOmniboxCacheSuggestionResourcesName[] =
    "Omnibox cache suggestion resources";
const char kOmniboxCacheSuggestionResourcesDescription[] =
    "When enabled, the omnibox will cache frequently used drawables and "
    "strings rather than loading them from Android every time they're needed.";

const char kOmniboxCalcProviderName[] = "Omnibox calc provider";
const char kOmniboxCalcProviderDescription[] =
    "When enabled, suggests recent calculator results in the omnibox.";

const char kOmniboxCompanyEntityIconAdjustmentName[] =
    "Omnibox Company Entity Icon Adjustment";
const char kOmniboxCompanyEntityIconAdjustmentDescription[] =
    "When enabled, company entity icons may be replaced based on the search "
    "suggestions and their corresponding order.";

const char kOmniboxCR23ActionChipsName[] = "Omnibox CR 2023 Action Chips";
const char kOmniboxCR23ActionChipsDescription[] =
    "Updates Omnibox Action Chips to comply with CR23 shape guidelines.";

const char kOmniboxCR23ActionChipsIconsName[] =
    "Omnibox CR 2023 Action Chips Icons";
const char kOmniboxCR23ActionChipsIconsDescription[] =
    "Updates Omnibox Action Chips to comply with CR23 icons design.";

const char kOmniboxCR23ExpandedStateColorsName[] =
    "Omnibox Expanded State Colors";
const char kOmniboxCR23ExpandedStateColorsDescription[] =
    "Updates colors in Omnibox expanded state to comply with CR23 guidelines.";

const char kOmniboxCR23ExpandedStateHeightName[] =
    "Omnibox Expanded State Height";
const char kOmniboxCR23ExpandedStateHeightDescription[] =
    "Updates Omnibox expanded state height to comply with CR23 guidelines.";

const char kOmniboxCR23ExpandedStateLayoutName[] =
    "Omnibox Expanded State Layout";
const char kOmniboxCR23ExpandedStateLayoutDescription[] =
    "Updates Omnibox expanded state layout to comply with CR23 guidelines.";

const char kOmniboxCR23ExpandedStateShapeName[] =
    "Omnibox Expanded State Shape";
const char kOmniboxCR23ExpandedStateShapeDescription[] =
    "Updates Omnibox expanded state shape to comply with CR23 guidelines.";

const char kOmniboxCR23ExpandedStateSuggestIconsName[] =
    "Omnibox Expanded State Suggest Icons";
const char kOmniboxCR23ExpandedStateSuggestIconsDescription[] =
    "Updates suggestion icons in Omnibox query row and expanded state to "
    "comply with CR23 guidelines.";

const char kOmniboxCR23SteadyStateIconsName[] = "Omnibox Steady State Icons";
const char kOmniboxCR23SteadyStateIconsDescription[] =
    "Updates Omnibox steady state icons to comply with CR23 guidelines.";

const char kOmniboxCR23SuggestionHoverFillShapeName[] =
    "Omnibox Suggestion Hover Fill Shape";
const char kOmniboxCR23SuggestionHoverFillShapeDescription[] =
    "Updates Omnibox suggestion hover fill shape to comply with CR23 "
    "guidelines.";

const char kOmniboxActionsUISimplificationName[] =
    "Omnibox Actions UI Simplification";
const char kOmniboxActionsUISimplificationDescription[] =
    "Simplifies omnibox actions UI design with inlined and separated actions.";

const char kOmniboxKeywordModeRefreshName[] = "Omnibox Keyword Mode Refresh";
const char kOmniboxKeywordModeRefreshDescription[] =
    "Changes suggestion behavior for keyword mode/site search/starter pack.";

const char kOmniboxDomainSuggestionsName[] = "Omnibox Domain Suggestions";
const char kOmniboxDomainSuggestionsDescription[] =
    "If enabled, history URL suggestions from hosts visited often bypass the "
    "per provider limit.";

const char kOmniboxGM3SteadyStateBackgroundColorName[] =
    "Omnibox Steady State Background Color";
const char kOmniboxGM3SteadyStateBackgroundColorDescription[] =
    "Updates Omnibox steady state background color to comply with GM3 "
    "guidelines.";

const char kOmniboxGM3SteadyStateHeightName[] = "Omnibox Steady State Height";
const char kOmniboxGM3SteadyStateHeightDescription[] =
    "Updates Omnibox steady state height to comply with GM3 guidelines.";

const char kOmniboxGM3SteadyStateTextStyleName[] =
    "Omnibox Steady State Text Style";
const char kOmniboxGM3SteadyStateTextStyleDescription[] =
    "Updates Omnibox steady state text style to comply with GM3 guidelines.";

const char kOmniboxGM3SteadyStateTextColorName[] =
    "Omnibox Steady State Text Color";
const char kOmniboxGM3SteadyStateTextColorDescription[] =
    "Updates Omnibox steady state text color to comply with GM3 guidelines.";

const char kOmniboxGroupingFrameworkZPSName[] =
    "Omnibox Grouping Framework for ZPS";
const char kOmniboxGroupingFrameworkNonZPSName[] =
    "Omnibox Grouping Framework for Typed Suggestions";
const char kOmniboxGroupingFrameworkDescription[] =
    "Enables an alternative grouping implementation for omnibox "
    "autocompletion.";

const char kOmniboxMatchToolbarAndStatusBarColorName[] =
    "Omnibox Omnibox Match Toolbar And Status Bar Color";
const char kOmniboxMatchToolbarAndStatusBarColorDescription[] =
    "When enabled, the color of the toolbar and the status bar will be "
    "synchronized.";

const char kOmniboxModernizeVisualUpdateName[] =
    "Omnibox Modernize Visual Update";
const char kOmniboxModernizeVisualUpdateDescription[] =
    "When enabled, Omnibox will show a new UI which is visually "
    "updated. This flag is for the step 1 in the Clank Omnibox revamp plan.";

const char kOmniboxMostVisitedTilesHorizontalRenderGroupName[] =
    "Omnibox MV Tiles Horizontal Render Group";
const char kOmniboxMostVisitedTilesHorizontalRenderGroupDescription[] =
    "Updates the logic constructing MV tiles to use horizontal render group. "
    "No user-facing changes expected.";

const char kOmniboxSuppressClipboardSuggestionAfterFirstUsedName[] =
    "Suppress clipboard suggestion after first used";
const char kOmniboxSuppressClipboardSuggestionAfterFirstUsedDescription[] =
    "Stops showing a clipboard suggestion for distinct clip data once it's "
    "been clicked on";

const char kOmniboxWarmRecycledViewPoolName[] =
    "Omnibox warm recycled view pool";
const char kOmniboxWarmRecycledViewPoolDescription[] =
    "Pre-warms the Android Omnibox's RecyclerView pool by inflating "
    "views before the omnibox is focused.";

const char kOmniboxReportAssistedQueryStatsName[] =
    "Omnibox Assisted Query Stats param";
const char kOmniboxReportAssistedQueryStatsDescription[] =
    "Enables reporting the Assisted Query Stats param in search destination "
    "URLs originated from the Omnibox.";

const char kOmniboxReportSearchboxStatsName[] =
    "Omnibox Searchbox Stats proto param";
const char kOmniboxReportSearchboxStatsDescription[] =
    "Enables reporting the serialized Searchbox Stats proto param in search "
    "destination URLs originated from the Omnibox.";

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

const char kOmniboxQueryTilesInZPSOnNTPName[] = "Query Tiles in ZPS on NTP";
const char kOmniboxQueryTilesInZPSOnNTPDesc[] =
    "Offer Query Tiles in Zero Prefix Suggestions on a New Tab Page.";

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

const char kOmniboxMlUrlScoringName[] = "Omnibox ML URL Scoring";
const char kOmniboxMlUrlScoringDescription[] =
    "Enables ML-based relevance scoring for Omnibox URL Suggestions.";

const char kOmniboxMlUrlScoringModelName[] = "Omnibox URL Scoring Model";
const char kOmniboxMlUrlScoringModelDescription[] =
    "Enables ML scoring model for Omnibox URL sugestions.";

const char kOmniboxOnClobberFocusTypeOnContentName[] =
    "Omnibox On Clobber Focus Type On Content";
const char kOmniboxOnClobberFocusTypeOnContentDescription[] =
    "Send ON_CLOBBER focus type for zero-prefix requests with an empty input "
    "on Web/SRP.";

const char kOmniboxShortcutBoostName[] = "Omnibox shortcut boosting";
const char kOmniboxShortcutBoostDescription[] =
    "Promote shortcuts to be default when available.";

const char kOmniboxSimplifiedUiUniformRowHeightName[] =
    "Omnibox Suggestion Row Height";
const char kOmniboxSimplifiedUiUniformRowHeightDescription[] =
    "Changes the row height of omnibox suggetions.";

const char kOmniboxSimplifiedUiSquareSuggestIconName[] =
    "Omnibox Square Suggest Icons";
const char kOmniboxSimplifiedUiSquareSuggestIconDescription[] =
    "Adds a grey square background to suggestion icons, and makes the answer "
    "icon square.";

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

const char kOnlyShowNewShortcutsAppName[] =
    "Only show the new Shortcut Viewer app";
const char kOnlyShowNewShortcutsAppDescription[] =
    "If enabled, the existing Shortcut Viewer app will be hidden and only the "
    "new Shortcut Customization app will be discoverable.";

const char kOneTimePermissionName[] = "One time permission";
const char kOneTimePermissionDescription[] =
    "Enables experimental one time permissions for Geolocation, Microphone and "
    "Camera.";

const char kOptimizationGuideDebugLogsName[] =
    "Enable optimization guide debug logs";
const char kOptimizationGuideDebugLogsDescription[] =
    "Enables the optimization guide to log and save debug messages that can be "
    "shown in the internals page.";

const char kOptimizationGuideInstallWideModelStoreName[] =
    "Enables the new optimization guide install-wide model store";
const char kOptimizationGuideInstallWideModelStoreDescription[] =
    "Enables the new model store that is per Chrome installation and can "
    "share models across user profiles.";

const char kOptimizationGuideModelExecutionName[] =
    "Enables optimization guide model execution";
const char kOptimizationGuideModelExecutionDescription[] =
    "Enables the optimization guide to execute models.";

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
const char kOverscrollHistoryNavigationSettingName[] =
    "Overscroll history navigation setting";
const char kOverscrollHistoryNavigationSettingDescription[] =
    "Whether to show a setting in chrome://settings/accessibility that "
    "controls whether horizontal overscroll triggers history navigation.";

const char kOverviewButtonName[] = "Overview button at the status area";
const char kOverviewButtonDescription[] =
    "If enabled, always show the overview button at the status area.";

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

const char kPageEntitiesPageContentAnnotationsName[] =
    "Page entities content annotations";
const char kPageEntitiesPageContentAnnotationsDescription[] =
    "Enables annotating the page entities model for each page load on-device.";

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

const char kPasswordGenerationExperimentName[] =
    "Password generation experiment";
const char kPasswordGenerationExperimentDescription[] =
    "Enables different experiments that modify content and behavior of the "
    "existing generated password suggestion dropdown.";

const char kPasswordGenerationStrongLabelExperimentName[] =
    "Password generation strong label experiment";
const char kPasswordGenerationStrongLabelExperimentDescription[] =
    "Enables adding a 'Strong password' label inside the sign-up password "
    "field in the password generation flow.";

const char kPasswordParsingOnSaveUsesPredictionsName[] =
    "Use server predictions for password form parsing on saving";
const char kPasswordParsingOnSaveUsesPredictionsDescription[] =
    "Take server prediction into account when parsing password forms "
    "during saving.";

const char kPdfOcrName[] = "Performs OCR on inaccessible PDFs";
const char kPdfOcrDescription[] =
    "Enables a feature whereby inaccessible (i.e. untagged) PDFs are made "
    "accessible using an optical character recognition service.";

const char kBacklightOcrName[] =
    "Performs OCR on PDFs displayed in the Media App";
const char kBacklightOcrDescription[] =
    "Enables a feature that makes PDFs displayed in the ChromeOS Media App "
    "(AKA Backlight) accessible by performing OCR on the images for each page.";

const char kPdfXfaFormsName[] = "PDF XFA support";
const char kPdfXfaFormsDescription[] =
    "Enables support for XFA forms in PDFs. "
    "Has no effect if Chrome was not built with XFA support.";

const char kVmPerBootShaderCacheName[] = "VM per-boot shader cache";
const char kVmPerBootShaderCacheDescription[] =
    "If enabled, VM shader cache is refreshed per boot. If disabled, VM shader "
    "cache is refreshed per OS build.";

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

const char kPermissionChipName[] = "Permissions Chip Experiment";
const char kPermissionChipDescription[] =
    "Enables an experimental permission prompt that uses a chip in the location"
    " bar.";

const char kChipLocationBarIconOverrideName[] =
    "Chip Location Bar Icon Override Experiment.";
const char kChipLocationBarIconOverrideDescription[] =
    "Enables an experimental location bar icon override while a chip is shown "
    "in the location bar. Takes effect when #permission-chip or "
    "#confirmation-chip are active.";

const char kConfirmationChipName[] = "Confirmation Chip Experiment";
const char kConfirmationChipNameDescription[] =
    "Enables an experimental confirmation chip in the location bar after a "
    "permission prompt shown has been decided by the user.";

const char kImprovedSemanticsActivityIndicatorsName[] =
    "Improved semantics activity indicators";
const char kImprovedSemanticsActivityIndicatorsDescription[] =
    "Enables experimental improved semantics indicators in the location bar.";

const char kPermissionPredictionsName[] = "Permission Predictions";
const char kPermissionPredictionsDescription[] =
    "Use the Permission Predictions Service to surface permission requests "
    "using a quieter UI when the likelihood of the user granting the "
    "permission is predicted to be low. Requires "
    "chrome://flags/#quiet-notification-prompts and `Safe Browsing` to be "
    "enabled.";

const char kPermissionQuietChipName[] = "Quiet Permission Chip Experiment";
const char kPermissionQuietChipDescription[] =
    "Enables a permission prompt that uses the quiet chip instead of the "
    "right-hand side address bar icon for quiet permission prompts. Requires "
    "chrome://flags/#quiet-notification-prompts to be enabled.";

const char kPermissionStorageAccessAPIName[] =
    "Storage Access API permission UI";
const char kPermissionStorageAccessAPIDescription[] =
    "Enables the new Storage Access API permission UI on Desktop";

const char kShowRelatedWebsiteSetsPermissionGrantsName[] =
    "Show permission grants from Related Website Sets";
const char kShowRelatedWebsiteSetsPermissionGrantsDescription[] =
    "Shows permission grants created by Related Website Sets in Chrome "
    "Settings UI and Page Info Bubble, "
    "default is hidden";

const char kRecordPermissionExpirationTimestampsName[] =
    "Record permission expiration timestamps";
const char kRecordPermissionExpirationTimestampsDescription[] =
    "When enabled, permissions grants with a durable session model will have "
    "an expiration date set.";

const char kPowerBookmarkBackendName[] = "Power bookmark backend";
const char kPowerBookmarkBackendDescription[] =
    "Enables storing additional metadata to support power bookmark features.";

const char kBookmarksImprovedSaveFlowName[] = "Improved bookmarks save flow";
const char kBookmarksImprovedSaveFlowDescription[] =
    "Enabled an improved save flow for bookmarks.";

const char kBookmarksRefreshName[] = "Bookmarks refresh";
const char kBookmarksRefreshDescription[] =
    "Enable various changes to bookmarks.";

const char kSpeculationRulesPrerenderingTargetHintName[] =
    "Speculation Rules API target hint";
const char kSpeculationRulesPrerenderingTargetHintDescription[] =
    "Enable target_hint param on Speculation Rules API for prerendering.";

const char kSupportSearchSuggestionForPrerender2Name[] =
    "Prerender search suggestions";
const char kSupportSearchSuggestionForPrerender2Description[] =
    "Allows Prerender2 to prerender search suggestions provided by the default "
    "search engine.";

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

const char kPrivacyGuideAndroidName[] = "Privacy Guide on Android";
const char kPrivacyGuideAndroidDescription[] =
    "Shows a new subpage in Settings that helps the user to review various "
    "privacy settings.";

const char kPrivacyGuide3Name[] = "Privacy Guide V3";
const char kPrivacyGuide3Description[] =
    "Enables updates to the Privacy Guide flow.";

const char kPrivacyGuidePreloadName[] = "Privacy Guide Preload";
const char kPrivacyGuidePreloadDescription[] =
    "Adds the preload card to the Privacy Guide 3 flow. "
    "This requires #privacy-guide-3 to also be enabled.";

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

const char kPrivacySandboxSettings4Name[] = "Privacy Sandbox Settings V4";
const char kPrivacySandboxSettings4Description[] =
    "Enables updated Privacy Sandbox UI";

const char kPrivacySandboxProactiveTopicsBlockingName[] =
    "Privacy Sandbox Proactive Topics Blocking";
const char kPrivacySandboxProactiveTopicsBlockingDescription[] =
    "Enables Privacy Sandbox Proactive Topics Blocking";

const char kPrivateAggregationDeveloperModeName[] =
    "Private Aggregation developer mode";
const char kPrivateAggregationDeveloperModeDescription[] =
    "Enables the developer mode for the Private Aggregation API. This removes "
    "all reporting delays. Only works if the Private Aggregation API is "
    "already enabled.";

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

const char kPWAsDefaultOfflinePageName[] = "Default offline page for PWAs";
const char kPWAsDefaultOfflinePageDescription[] =
    "Shows customised default offline page when web app is offline.";

const char kPwaUpdateDialogForAppIconName[] =
    "Enable PWA install update dialog for icon changes";
const char kPwaUpdateDialogForAppIconDescription[] =
    "Enable a confirmation dialog that shows up when a PWA changes its icon";

const char kRenderDocumentName[] = "Enable RenderDocument";
const char kRenderDocumentDescription[] =
    "Enable swapping RenderFrameHosts on same-site navigations";

const char kQuicName[] = "Experimental QUIC protocol";
const char kQuicDescription[] = "Enable experimental QUIC protocol support.";

const char kQuickAppAccessTestUIName[] = "Internal test: quick app access";
const char kQuickAppAccessTestUIDescription[] =
    "Show an app in the quick app access area at the start of the session";

const char kQuickIntensiveWakeUpThrottlingAfterLoadingName[] =
    "Quick intensive throttling after loading";
const char kQuickIntensiveWakeUpThrottlingAfterLoadingDescription[] =
    "For pages that are loaded when backgrounded, activates intensive "
    "throttling after 1 minute instead of the default 5 minutes. Intensive "
    "throttling will limit wake ups, from setTimeout and setInterval tasks "
    "with a high nesting level and delayed scheduler.postTask tasks, to 1 per "
    "minute. See https://chromestatus.com/feature/5580139453743104 for more "
    "info.";

const char kQuickDeleteForAndroidName[] = "Enable quick delete";
const char kQuickDeleteForAndroidDescription[] =
    "When enabled, a new quick delete option appears in the three dots menu, "
    "allowing users to quickly delete their last 15 mins of data.";

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

const char kReduceAcceptLanguageName[] =
    "Reduce Accept-Language request header";
const char kReduceAcceptLanguageDescription[] =
    "Reduce the amount of information available in the Accept-Language request "
    "header. See https://github.com/Tanych/accept-language for more info.";

const char kResetShortcutCustomizationsName[] =
    "Reset all shortcut customizations";
const char kResetShortcutCustomizationsDescription[] =
    "Resests all shortcut customizations on startup.";

const char kRestrictGamepadAccessName[] = "Restrict gamepad access";
const char kRestrictGamepadAccessDescription[] =
    "Enables Permissions Policy and Secure Context restrictions on the Gamepad "
    "API";

const char kRoundedDisplay[] = "Rounded display";
const char kRoundedDisplayDescription[] =
    "Enables rounded corners for the display";

const char kRoundedWindows[] = "Use rounded windows";
const char kRoundedWindowsDescription[] =
    "Specifies the radius of rounded windows in DIPs (Device Independent "
    "Pixels)";

const char kMBIModeName[] = "MBI Scheduling Mode";
const char kMBIModeDescription[] =
    "Enables independent agent cluster scheduling, via the "
    "AgentSchedulingGroup infrastructure.";

const char kSafetyCheckNotificationPermissionsName[] =
    "Permission Module for notifications in Safety Check";
const char kSafetyCheckNotificationPermissionsDescription[] =
    "When enabled, adds the notification permission module to Safety Check on "
    "desktop. The module will be shown depending on the browser state.";

const char kSafetyCheckUnusedSitePermissionsName[] =
    "Permission Module for unused sites in Safety Check";
const char kSafetyCheckUnusedSitePermissionsDescription[] =
    "When enabled, adds the unused sites permission module to Safety Check on "
    "desktop. The module will be shown depending on the browser state.";

const char kSafetyHubName[] = "Safety Check v2";
const char kSafetyHubDescription[] =
    "When enabled, Safety Check v2 will be visible in settings.";

const char kSameAppWindowCycleName[] = "Cros Labs: Same App Window Cycling";
const char kSameAppWindowCycleDescription[] =
    "Use Alt+` to cycle through the windows of the active application.";

const char kTestThirdPartyCookiePhaseoutName[] =
    "Test Third Party Cookie Phaseout";
const char kTestThirdPartyCookiePhaseoutDescription[] =
    "Enable to test third-party cookie phaseout. "
    "Enabling this flag also enables FedCM and third-party storage "
    "partitioning.";

const char kThirdPartyStoragePartitioningName[] =
    "Third-party Storage Partitioning";
const char kThirdPartyStoragePartitioningDescription[] =
    "When disabled, prevents partitioning of third-party storage by top-level "
    "site. If any site issues are experienced as a result of the third-party "
    "storage partitioning feature being enabled, please file bugs at "
    "https://bugs.chromium.org/p/chromium/issues/entry"
    "?labels=Proj-StoragePartitioningTrial&components=Blink%3EStorage.";

const char kScreenSaverDurationName[] = "Screen saver duration settings";
const char kScreenSaverDurationDescription[] =
    "Allow users to customize screen saver running time.";

const char kScrollableTabStripFlagId[] = "scrollable-tabstrip";
const char kScrollableTabStripName[] = "Tab Scrolling";
const char kScrollableTabStripDescription[] =
    "Enables tab strip to scroll left and right when full.";

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

const char kSecurePaymentConfirmationExtensionsName[] =
    "Secure Payment Confirmation Extensions";
const char kSecurePaymentConfirmationExtensionsDescription[] =
    "Enables extensions in the Secure Payment Confirmation API that allows "
    "using webauthn extensions with Secure Payment Confirmation";

const char kSidePanelJourneysFlagId[] = "side-panel-journeys";
const char kSidePanelJourneysName[] = "Side panel journeys";
const char kSidePanelJourneysDescription[] =
    "Enables Journeys within the side panel.";

const char kSidePanelJourneysQuerylessFlagId[] =
    "side-panel-journeys-queryless";
const char kSidePanelJourneysQuerylessName[] = "Side panel journeys queryless";
const char kSidePanelJourneysQuerylessDescription[] =
    "Enables Journeys queryless view within the side panel.";

const char kSidePanelPinningFlagId[] = "side-panel-pinning";
const char kSidePanelPinningName[] =
    "Side panel navigation and pinning updates";
const char kSidePanelPinningDescription[] =
    "Enables support for side panel pinning and updates to navigation.";

const char kSharedZstdName[] = "Shared Zstd";
const char kSharedZstdDescription[] =
    "Enables compression dictionary transport with Zstandard (aka Shared "
    "Zstd).";

const char kSharingDesktopScreenshotsName[] = "Desktop Screenshots";
const char kSharingDesktopScreenshotsDescription[] =
    "Enables taking"
    " screenshots from the desktop sharing hub.";

const char kShelfStackedHotseatName[] = "Shelf Stacked Hotseat";
const char kShelfStackedHotseatDescription[] =
    "Stack the hotseat app bar above the shelf button panels/system tray when "
    "there is not enough space for the app bar.";

const char kShowAutofillSignaturesName[] = "Show autofill signatures.";
const char kShowAutofillSignaturesDescription[] =
    "Annotates web forms with Autofill signatures as HTML attributes. Also "
    "marks password fields suitable for password generation.";

const char kShowAutofillTypePredictionsName[] = "Show Autofill predictions";
const char kShowAutofillTypePredictionsDescription[] =
    "Annotates web forms with Autofill field type predictions as placeholder "
    "text.";

const char kShowPerformanceMetricsHudName[] = "Show performance metrics in HUD";
const char kShowPerformanceMetricsHudDescription[] =
    "Display the performance metrics of current page in a heads up display on "
    "the page.";

const char kShowOverdrawFeedbackName[] = "Show overdraw feedback";
const char kShowOverdrawFeedbackDescription[] =
    "Visualize overdraw by color-coding elements based on if they have other "
    "elements drawn underneath.";

const char kSiteInstanceGroupsForDataUrlsName[] =
    "SiteInstanceGroups for data: URLs";
const char kSiteInstanceGroupsForDataUrlsDescription[] =
    "Put data: URL subframes in a separate SiteInstance from the initiator, "
    "but in the same SiteInstanceGroup, and thus the same process.";

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

const char kSmoothScrollingName[] = "Smooth Scrolling";
const char kSmoothScrollingDescription[] =
    "Animate smoothly when scrolling page content.";

const char kSplitCacheByNetworkIsolationKeyName[] = "HTTP Cache Partitioning";
const char kSplitCacheByNetworkIsolationKeyDescription[] =
    "Partitions the HTTP Cache by (top-level site, current-frame site) to "
    "disallow cross-site tracking.";

const char kStrictOriginIsolationName[] = "Strict-Origin-Isolation";
const char kStrictOriginIsolationDescription[] =
    "Experimental security mode that strengthens the site isolation policy. "
    "Controls whether site isolation should use origins instead of scheme and "
    "eTLD+1.";

const char kStorageAccessAPIName[] = "Storage Access API";
const char kStorageAccessAPIDescription[] =
    "Enables the Storage Access API, allowing websites to request storage "
    "access when it would otherwise be restricted.";

const char kSupportTool[] = "Support Tool";
const char kSupportToolDescription[] =
    "Support Tool collects and exports logs to help debugging the issues. It's"
    " available in chrome://support-tool.";

const char kSupportToolScreenshot[] = "Support Tool Screenshot";
const char kSupportToolScreenshotDescription[] =
    "Enables the Support Tool to capture and include a screenshot in the "
    "exported packet.";

const char kSuppressTextMessagesName[] = "Suppress Text Messages";
const char kSuppressTextMessagesDescription[] =
    "Enables users and admins to configure suppressing text messages.";

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

const char kSyncSessionOnVisibilityChangedName[] =
    "Sync session when tab visibility changes";
const char kSyncSessionOnVisibilityChangedDescription[] =
    "This flag enables session syncing when the visibility of a tab changes.";

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

const char kSystemSoundsName[] = "Power Sounds";
const char kSystemSoundsDescription[] =
    "Enable device charging and low battery warning sounds.";

const char kStylusBatteryStatusName[] =
    "Show stylus battery stylus in the stylus tools menu";
const char kStylusBatteryStatusDescription[] =
    "Enables viewing the current stylus battery level in the stylus tools "
    "menu.";

const char kTabDragDropName[] = "Tab Drag and Drop via Strip";
const char kTabDragDropDescription[] =
    "Enables Tab drag and drop UI to move tab on tab-strip across windows.";

const char kTabAndLinkDragDropName[] = "Tab and Link - Drag and Drop";
const char kTabAndLinkDragDropDescription[] =
    "Enables Tab drag/drop UI to move tab across windows via strip and "
    "switcher UI. Enables link drag to open a tab.";

const char kTabEngagementReportingName[] = "Tab Engagement Metrics";
const char kTabEngagementReportingDescription[] =
    "Tracks tab engagement and lifetime metrics.";

const char kCommerceDeveloperName[] = "Commerce developer mode";
const char kCommerceDeveloperDescription[] =
    "Allows users in the allowlist to enter the developer mode";

const char kCommerceMerchantViewerAndroidName[] = "Merchant Viewer";
const char kCommerceMerchantViewerAndroidDescription[] =
    "Allows users to view merchant trust signals on eligible pages.";

const char kTabGroupsSaveId[] = "tab-groups-save";
const char kTabGroupsSaveName[] = "Tab Groups Save and Sync";
const char kTabGroupsSaveDescription[] =
    "Enable saving and recalling of tab groups. Right click a tab group to "
    "save it. Recall groups from the bookmarks bar.";

const char kTabHoverCardImageSettingsName[] = "Tab Hover Card Image Setting";
const char kTabHoverCardImageSettingsDescription[] =
    "Show a toggle in appearance settings to control if tab hover card preview "
    "images are shown.";

const char kTabHoverCardImagesName[] = "Tab Hover Card Images";
const char kTabHoverCardImagesDescription[] =
    "Shows a preview image in tab hover cards, if tab hover cards are enabled.";

const char kTabSearchFuzzySearchName[] = "Fuzzy search for Tab Search";
const char kTabSearchFuzzySearchDescription[] =
    "Enable fuzzy search for Tab Search.";

const char kTextInShelfName[] = "Internal test: text in shelf";
const char kTextInShelfDescription[] =
    "Extend text in shelf timeout to learn about user education";

const char kTintCompositedContentName[] = "Tint composited content";
const char kTintCompositedContentDescription[] =
    "Tint contents composited using Viz with a shade of red to help debug and "
    "study overlay support.";

const char kTopChromeTouchUiName[] = "Touch UI Layout";
const char kTopChromeTouchUiDescription[] =
    "Enables touch UI layout in the browser's top chrome.";

const char kThrottleDisplayNoneAndVisibilityHiddenCrossOriginIframesName[] =
    "Throttle non-visible cross-origin iframes";
const char
    kThrottleDisplayNoneAndVisibilityHiddenCrossOriginIframesDescription[] =
        "When enabled, all cross-origin iframes with zero visibility (either "
        "display:none or zero viewport intersection with non-zero area) will be"
        " throttled, regardless of whether they are same-process or "
        "cross-process. When disabled, throttling for cross-process iframes is "
        "the same, but for same-process iframes throttling only occurs when "
        "the frame has zero viewport intersection, a non-zero area, and is "
        "not display:none.";

const char kThumbnailCacheRefactorName[] = "Thumbnail Cache Refactor";
const char kThumbnailCacheRefactorDescription[] =
    "When enabled the thumbnail cache for Android is updated to improve "
    "memory usage and performance.";

const char kNewBaseUrlInheritanceBehaviorName[] =
    "Enable new base url inheritance behaviors for srcdoc and about:blank";
const char kNewBaseUrlInheritanceBehaviorDescription[] =
    "When enabled, about:blank and srcdoc frames will use newly proposed "
    "behaviors around inheriting their base urls, as discussed on  "
    "https://crbug.com/1356658. Note: this is automatically enabled when "
    "'isolate sandboxed iframes' is enabled.";

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

const char kTailoredSecurityRetryForSyncUsersName[] =
    "Enable Tailored Security retry for sync users";
const char kTailoredSecurityRetryForSyncUsersDescription[] =
    "Enables a stateful recovery mechanism for the Tailored Security feature";

const char kTrustedVaultFrequentDegradedRecoverabilityPollingName[] =
    "Enable frequent polling of trusted vault degraded recoverability state";
const char kTrustedVaultFrequentDegradedRecoverabilityPollingDescription[] =
    "Sets degraded recoverability polling interval to 1 minute.";

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

const char kUseNAT64ForIPv4LiteralName[] =
    "Use NAT64 translation for IPv4 literals";
const char kUseNAT64ForIPv4LiteralDescription[] =
    "Enables IPv4 to IPv6 address translation for IPv4 literals when chrome is "
    "on an IPv6 only network";

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

const char kTrackingProtection3pcdName[] = "Tracking Protection for 3PCD";
const char kTrackingProtection3pcdDescription[] =
    "Enables the tracking protection UI + prefs that will be used for the 3PCD "
    "1%. ";

const char kTrackingProtectionOnboardingRollbackName[] =
    "Tracking Protection Rollback Flow";
const char kTrackingProtectionOnboardingRollbackDescription[] =
    "Enables the tracking protection rollback flow";

const char kUserBypassUIName[] = "User Bypass UI";
const char kUserBypassUIDescription[] = "Enables the User Bypass UI. ";

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

const char kUsernameFirstFlowStoreSeveralValuesName[] =
    "Username First Flow store several possible username values";
const char kUsernameFirstFlowStoreSeveralValuesDescription[] =
    "Store several values as a possible username value instead of only one. "
    "This flag is part of Username First Flow with intermediate values "
    "feature.";

const char kUsernameFirstFlowWithIntermediateValuesName[] =
    "Username first flow with intermediate values";
const char kUsernameFirstFlowWithIntermediateValuesDescription[] =
    "Support username first flow with intermediate values. Username first flow "
    "is login/sign-up flow where a user has to type username first on one page "
    "and then password on another page. Intermediate fields are usually an OTP "
    "field or CAPTCHA.";

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

const char kVideoConferenceName[] = "Enable video conference features";
const char kVideoConferenceDescription[] =
    "Enables all features for ChromeOS built-in video conferencing UI.";

const char kVcBackgroundReplaceName[] = "Enable vc background replacement";
const char kVcBackgroundReplaceDescription[] =
    "Enables background replacement feature for video conferencing on "
    "chromebooks. THIS WILL OVERRIDE BACKGROUND BLUR.";

const char kVcSegmentationModelName[] = "Use a different segmentation model";
const char kVcSegmentationModelDescription[] =
    "Allows a different segmentation model to be used for blur and relighting, "
    "which may reduce the workload on the GPU.";

const char kVcLightIntensityName[] = "VC relighting intensity";
const char kVcLightIntensityDescription[] =
    "Allows different light intenisty to be used for relighting.";

const char kVcWebApiName[] = "VC web API";
const char kVcWebApiDescription[] =
    "Allows web API support for video conferencing on chromebooks.";

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

const char kWallpaperPerDeskName[] =
    "Enable setting different wallpapers per desk";
const char kWallpaperPerDeskDescription[] =
    "Allow users to set different wallpapers on each of their active desks";

const char kWebAuthnFilterGooglePasskeysName[] =
    "Filter passkeys for google.com";
const char kWebAuthnFilterGooglePasskeysDescription[] =
    "When servicing a webauthn request for google.com, filter webauthn "
    "credentials that do not match a user.id prefix identifying them as "
    "passkeys, e.g. because they are used for autofill auth.";

const char kWebAuthnScreenReaderModeName[] =
    "Enable screen reader mode for webauthn UI";
const char kWebAuthnScreenReaderModeDescription[] =
    "When displaying webauthn UI, try to detect if a screen reader is running "
    "to tailor the experience for blind and low vision users.";

const char kWebBluetoothName[] = "Web Bluetooth";
const char kWebBluetoothDescription[] =
    "Enables the Web Bluetooth API on platforms without official support";

const char kWebBluetoothNewPermissionsBackendName[] =
    "Use the new permissions backend for Web Bluetooth";
const char kWebBluetoothNewPermissionsBackendDescription[] =
    "Enables the new permissions backend for Web Bluetooth. This will enable "
    "persistent storage of device permissions and Web Bluetooth features such "
    "as BluetoothDevice.watchAdvertisements() and Bluetooth.getDevices()";

const char kWebContentsCaptureHiDpiName[] = "HiDPI Tab Capture";
const char kWebContentsCaptureHiDpiDescription[] =
    "Enables HiDPI rendering for tab capture if the displayed content's "
    "resolution is low compared to the capture size. This improves "
    "legibility for viewers with higher-resolution screens.";

const char kWebMidiName[] = "Web MIDI";
const char kWebMidiDescription[] =
    "Enables the implementation of the Web MIDI API. When disabled the "
    "interface will still be exposed by Blink.";

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

const char kAddIdentityInCanMakePaymentEventName[] =
    "Add identity to canmakepayment event";
const char kAddIdentityInCanMakePaymentEventDescription[] =
    "Temporarily re-enable the deprecated feature of sharing the merchant and "
    "user identity with the payment app when the merchant checks whether the "
    "payment app can make payments.";

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
const char kWebXrRuntimeChoiceCardboard[] = "Cardboard";
const char kWebXrRuntimeChoiceGVR[] = "Google VR Services";
const char kWebXrRuntimeChoiceOpenXR[] = "OpenXR";

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

const char kResponsiveToolbarName[] = "Responsive toolbar";
const char kResponsiveToolbarDescription[] =
    "Toolbar icons overflow to a chevron icon when the browser width is "
    "resized small than normal";

const char kSharedHighlightingManagerName[] = "Refactoring Shared Highlighting";
const char kSharedHighlightingManagerDescription[] =
    "Refactors Shared Highlighting by centralizing the IPC calls in a Manager.";

const char kDraw1PredictedPoint12Ms[] = "1 point 12ms ahead.";
const char kDraw2PredictedPoints6Ms[] = "2 points, each 6ms ahead.";
const char kDraw1PredictedPoint6Ms[] = "1 point 6ms ahead.";
const char kDraw2PredictedPoints3Ms[] = "2 points, each 3ms ahead.";
const char kDrawPredictedPointsDescription[] =
    "Draw predicted points when using the delegated ink trails API. Requires "
    "experimental web platform features to be enabled.";
const char kDrawPredictedPointsName[] = "Draw predicted delegated ink points";

const char kSanitizerApiName[] = "Sanitizer API";
const char kSanitizerApiDescription[] =
    "Enable the Sanitizer API. See: https://github.com/WICG/sanitizer-api";

const char kUsePassthroughCommandDecoderName[] =
    "Use passthrough command decoder";
const char kUsePassthroughCommandDecoderDescription[] =
    "Use chrome passthrough command decoder instead of validating command "
    "decoder.";

const char kUseMultiPlaneFormatForHardwareVideoName[] =
    "Enable multi-plane formats for hardware video decoder";
const char kUseMultiPlaneFormatForHardwareVideoDescription[] =
    "Enable single shared image and mailbox for multi-plane formats for "
    "hardware video decoder";

const char kUseMultiPlaneFormatForSoftwareVideoName[] =
    "Enable multi-plane formats for software video decoder";
const char kUseMultiPlaneFormatForSoftwareVideoDescription[] =
    "Enable single shared image and mailbox for multi-plane formats for "
    "software video decoder";

const char kSkipServiceWorkerFetchHandlerName[] =
    "Skip Service Worker Fetch Handler if skippable";
const char kSkipServiceWorkerFetchHandlerDescription[] =
    "Skips starting the service worker and run the fetch handler if the fetch "
    "handler is recognized as skippable.";

const char kWebSQLAccessName[] = "Allows access to WebSQL APIs";
const char kWebSQLAccessDescription[] =
    "The WebSQL API is disabled by default, but can be enabled here.";

const char kUseGpuSchedulerDfsName[] = "Use new gpu scheduler.";
const char kUseGpuSchedulerDfsDescription[] =
    "Enables using the new gpu "
    "scheduler called GpuSchedulerDfs.";

const char kUseIDNA2008NonTransitionalName[] =
    "Enable IDNA 2008 Non-Transitional Mode";
const char kUseIDNA2008NonTransitionalDescription[] =
    "Enables IDNA 2008 in Non-Transitional Mode in URL processing, allowing "
    "deviation characters in domain names.";

const char kEnablePasswordSharingName[] = "Enables password sharing";
const char kEnablePasswordSharingDescription[] =
    "Enables sharing of password between members of the same family.";

// Android ---------------------------------------------------------------------

#if BUILDFLAG(IS_ANDROID)

const char kAccessibilityPageZoomName[] = "Accessibility Page Zoom";
const char kAccessibilityPageZoomDescription[] =
    "Whether the UI and underlying code for page zoom should be enabled to"
    " allow a user to increase/decrease the web contents zoom factor.";

const char kAccessibilityPerformanceFilteringName[] =
    "Accessibility Performance Filtering";
const char kAccessibilityPerformanceFilteringDescription[] =
    "Enable experimental accessibility filters to improve performance by"
    " supporting different levels of the accessibility engine depending on what"
    " accessibility services and assistive technologies are running.";

const char kAddToHomescreenIPHName[] = "Add to homescreen IPH";
const char kAddToHomescreenIPHDescription[] =
    " Shows in-product-help messages educating users about add to homescreen "
    "option in chrome.";

const char kAImageReaderName[] = "Android ImageReader";
const char kAImageReaderDescription[] =
    " Enables MediaPlayer and MediaCodec to use AImageReader on Android. "
    " This feature is only available for android P+ devices. Disabling it also "
    " disables SurfaceControl.";

const char kAndroidSurfaceControlName[] = "Android SurfaceControl";
const char kAndroidSurfaceControlDescription[] =
    " Enables SurfaceControl to manage the buffer queue for the "
    " DisplayCompositor on Android. This feature is only available on "
    " android Q+ devices";

const char kAndroidImprovedBookmarksName[] = "Android Visual Bookmark Manager";
const char kAndroidImprovedBookmarksDescription[] =
    "More visual changes to the bookmarks surfaces, with more thumbnails and a "
    "focus on search instead of folders/hierarchy";

const char kAndroidHatsRefactorName[] = "Android Hats Refactor";
const char kAndroidHatsRefactorDescription[] =
    "Enables survey structure refactor.";

const char kAndroidHubName[] = "Android Hub";
const char kAndroidHubDescription[] =
    "Replaces the Tab Switcher with a UI surface containing more types of "
    "data.";

const char kAnimatedImageDragShadowName[] =
    "Enable animated image drag shadow on Android.";
const char kAnimatedImageDragShadowDescription[] =
    "Animate the shadow image from its original bound to the touch point. "
    "Image drag on Android is available when flag touch-drag-and-context-menu "
    "is enabled.";

const char kAutofillUseMobileLabelDisambiguationName[] =
    "Autofill Uses Mobile Label Disambiguation";
const char kAutofillUseMobileLabelDisambiguationDescription[] =
    "When enabled, Autofill suggestions' labels are displayed using a "
    "mobile-friendly format.";

const char kAppMenuMobileSiteOptionName[] =
    "Show Mobile Site option in app menu";
const char kAppMenuMobileSiteOptionDescription[] =
    "When enabled, app menu should show 'Mobile site' when showing desktop "
    "site, instead of showing 'Desktop Site' with checkbox";

const char kBackGestureActivityTabProviderName[] =
    "Back Gesture Refactor (Activity Tab Provider)";
const char kBackGestureActivityTabProviderDescription[] =
    "When enabled, ChromeTabActivity will use getActivityTabProvider to "
    "get current tab, rather than getActivityTab if predictive back gesture"
    "is disabled.";

const char kBackGestureRefactorActivityAndroidName[] =
    "Back Gesture Refactor (Secondary Activities)";
const char kBackGestureRefactorActivityAndroidDescription[] =
    "Enable Back Gesture Refactor for Secondary Activities to support in-app "
    "activity-to-activity predictive back gestures";

const char kBackGestureRefactorAndroidName[] = "Back Gesture Refactor";
const char kBackGestureRefactorAndroidDescription[] =
    "Enable Back Gesture Refactor.";

const char kCCTBrandTransparencyName[] =
    "Chrome Custom Tabs Brand Transparency";
const char kCCTBrandTransparencyDescription[] =
    "When enabled, CCT will show more Chrome branding information when start, "
    "giving user more transparency that the web page is running in Chrome.";

const char kCCTIncognitoAvailableToThirdPartyName[] =
    "Allow third party to open Custom Tabs Incognito mode";
const char kCCTIncognitoAvailableToThirdPartyDescription[] =
    "Enabling it would allow third party apps to open incognito mode for "
    "Chrome Custom Tabs, on Android.";

const char kCCTMinimizedName[] = "Allow Custom Tabs to be minimized";
const char kCCTMinimizedDescription[] =
    "When enabled, CCTs can be minimized into picture-in-picture (PiP) mode.";

const char kCCTPageInsightsHubName[] = "Page Insights Hub";
const char kCCTPageInsightsHubDescription[] =
    "Show Page Insights Hub on Chrome Custom Tabs.";

const char kCCTPreventTouchesName[] = "Prevent touches from overlay";
const char kCCTPreventTouchesDescription[] =
    "Prevent touches from being processed if they are coming from an overlay "
    "activity.";

const char kCCTResizable90MaximumHeightName[] =
    "Bottom sheet Custom Tabs maximum height";
const char kCCTResizable90MaximumHeightDescription[] =
    "When enabled, the bottom sheet Custom Tabs will have maximum height 90% "
    "of the screen height, otherwise the maximum height is 100% of the screen "
    "height. In both cases, Custom Tabs will yield to the top status bar when "
    "at full stop";
const char kCCTResizableForThirdPartiesName[] =
    "Bottom sheet Custom Tabs (third party)";
const char kCCTResizableForThirdPartiesDescription[] =
    "Enable bottom sheet Custom Tabs for third party apps.";
const char kCCTResizableSideSheetName[] = "Side sheet Custom Tabs";
const char kCCTResizableSideSheetDescription[] =
    "Enable side sheet Custom Tabs";
const char kCCTResizableSideSheetForThirdPartiesName[] =
    "Side sheet Custom Tabs (third party)";
const char kCCTResizableSideSheetForThirdPartiesDescription[] =
    "Enable side sheet Custom Tabs for third party apps.";
const char kCCTRealTimeEngagementSignalsName[] =
    "Enable CCT real-time engagement signals.";
const char kCCTRealTimeEngagementSignalsDescription[] =
    "Enables sending real-time engagement signals (e.g. scroll) through "
    "CustomTabsCallback.";
const char kCCTRealTimeEngagementSignalsAlternativeImplName[] =
    "Enable alternative implementation for CCT real-time engagement signals.";
const char kCCTRealTimeEngagementSignalsAlternativeImplDescription[] =
    "Enables an alternative implementation for sending real-time engagement "
    "signals (e.g. scroll) through CustomTabsCallback.";

const char kCCTTextFragmentLookupApiEnabledName[] =
    "Enable CCT API to lookup text fragments";
const char kCCTTextFragmentLookupApiEnabledDescription[] =
    "Enable CCT API to lookup text fragments";

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

const char kChimeAlwaysShowNotificationDescription[] =
    "A debug flag to always show Chime notification after receiving a payload.";
const char kChimeAlwaysShowNotificationName[] =
    "Always show Chime notification";

const char kChimeAndroidSdkDescription[] =
    "Enable Chime SDK to receive push notification.";
const char kChimeAndroidSdkName[] = "Use Chime SDK";

const char kCloseTabSuggestionsName[] = "Suggest to close Tabs";
const char kCloseTabSuggestionsDescription[] =
    "Suggests to the user to close Tabs that haven't been used beyond a "
    "configurable threshold or where duplicates of Tabs exist. "
    "The threshold is configurable.";

const char kCriticalPersistedTabDataName[] = "Enable CriticalPersistedTabData";
const char kCriticalPersistedTabDataDescription[] =
    "A new method of persisting Tab data across restarts has been devised "
    "and implemented. This actives the new approach.";

const char kTabStateFlatBufferName[] = "Enable TabState FlatBuffer";
const char kTabStateFlatBufferDescription[] =
    "Migrates TabState from a pickle based schema to a FlatBuffer based "
    "schema.";

const char kContextMenuPopupForAllScreenSizesName[] =
    "Context menu popup for all screen sizes";
const char kContextMenuPopupForAllScreenSizesDescription[] =
    "When disabled, context menu will be shown as pop-up window only for "
    "devices in tablet mode, while shown as a fullscreen dialog for mobile "
    "devices; when enabled, context menu will be shown as a pop-up window "
    "for all form factors regardless of the screen sizes.";

const char kContextualSearchForceCaptionName[] =
    "Contextual Search force a caption";
const char kContextualSearchForceCaptionDescription[] =
    "Forces a caption to always be shown in the Touch to Search Bar.";

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

const char kDeferTabSwitcherLayoutCreationName[] =
    "Defer TabSwitcherLayout creation";
const char kDeferTabSwitcherLayoutCreationDescription[] =
    "Lazily construct the TabSwitcherLayout when first shown.";

const char kDeprecatedExternalPickerFunctionName[] =
    "Use deprecated External Picker method";
const char kDeprecatedExternalPickerFunctionDescription[] =
    "Use the old-style opening of an External Picker when uploading files";

const char kDragDropIntoOmniboxName[] =
    "Enable drag and drop into omnibox on Android";
const char kDragDropIntoOmniboxDescription[] =
    "Drag urls, text, and images into omnibox to start a search";

const char kDrawCutoutEdgeToEdgeName[] = "DrawCutoutEdgeToEdge";
const char kDrawCutoutEdgeToEdgeDescription[] =
    "Enables the Android feature Edge-to-Edge Feature to coordinate with the "
    "Display Cutout for the notch when drawing below the Nav Bar.";

const char kDrawEdgeToEdgeName[] = "DrawEdgeToEdge";
const char kDrawEdgeToEdgeDescription[] =
    "Enables the Android feature Edge-to-Edge to draw below the Nav Bar.";

const char kDrawNativeEdgeToEdgeName[] = "DrawNativeEdgeToEdge";
const char kDrawNativeEdgeToEdgeDescription[] =
    "Enables the Android feature Edge-to-Edge and forces a draw ToEdge on all "
    "native pages.";

const char kDrawWebEdgeToEdgeName[] = "DrawWebEdgeToEdge";
const char kDrawWebEdgeToEdgeDescription[] =
    "Enables the Android feature Edge-to-Edge and forces a draw ToEdge on most "
    "web pages.";

const char kEnableCommandLineOnNonRootedName[] =
    "Enable command line on non-rooted devices";
const char kEnableCommandLineOnNoRootedDescription[] =
    "Enable reading command line file on non-rooted devices (DANGEROUS).";

const char kEnablePasswordsAccountStorageName[] =
    "Enable the account data storage for passwords";
const char kEnablePasswordsAccountStorageDescription[] =
    "Enables storing passwords in a second, Gaia-account-scoped storage for "
    "signed-in but not syncing users";

const char kExploreSitesName[] = "Explore websites";
const char kExploreSitesDescription[] =
    "Enables portal from new tab page to explore websites.";

const char kExternalNavigationDebugLogsName[] =
    "External Navigation Debug Logs";
const char kExternalNavigationDebugLogsDescription[] =
    "Enables detailed logging to logcat about why Chrome is making decisions "
    "about whether to allow or block navigation to other apps";

const char kFeatureNotificationGuideName[] = "Feature notification guide";
const char kFeatureNotificationGuideDescription[] =
    "Enables notifications about chrome features.";

const char kFeatureNotificationGuideSkipCheckForLowEngagedUsersName[] =
    "Feature notification guide - Skip check for low engaged users";
const char kFeatureNotificationGuideSkipCheckForLowEngagedUsersDescription[] =
    "Skips check for low engaged users.";

const char kFeedBackToTopName[] = "Back to top of the feeds";
const char kFeedBackToTopDescription[] =
    "Enables showing a callout to help users return to the top of the feeds "
    "quickly.";

const char kFeedFollowUiUpdateName[] = "UI Update for the Following Feed";
const char kFeedFollowUiUpdateDescription[] =
    "Enables showing the updated UI for the following feed.";

const char kFeedHeaderStickToTopName[] = "Feed header stick to top";
const char kFeedHeaderStickToTopDescription[] =
    "Stick feed header to top on scroll.";

const char kFeedLoadingPlaceholderName[] = "Feed loading placeholder";
const char kFeedLoadingPlaceholderDescription[] =
    "Enables a placeholder UI in "
    "the feed instead of the loading spinner at first load.";

const char kFeedSignedOutViewDemotionName[] = "Feed signed-out view demotion";
const char kFeedSignedOutViewDemotionDescription[] =
    "Enables signed-out view demotion for the Discover Feed.";

const char kFeedSportsCardName[] = "Sports cards in the feed";
const char kFeedSportsCardDescription[] =
    "Enables the live sports cards in the feed.";

const char kFeedStampName[] = "StAMP cards in the feed";
const char kFeedStampDescription[] = "Enables StAMP cards in the feed.";

const char kFeedCloseRefreshName[] = "Feed-close refresh";
const char kFeedCloseRefreshDescription[] =
    "Enables scheduling a background refresh of the feed following feed use.";

const char kFeedDiscoFeedEndpointName[] =
    "Feed using the DiscoFeed backend endpoint";
const char kFeedDiscoFeedEndpointDescription[] =
    "Uses the DiscoFeed endpoint for serving the feed instead of GWS.";

const char kFeedDynamicColorsName[] = "Enable dynamic colors in the feed";
const char kFeedDynamicColorsDescription[] =
    "Allows feed to fully respect dynamic colors if supported by the client.";

const char kInfoCardAcknowledgementTrackingName[] =
    "Info card acknowledgement tracking";
const char kInfoCardAcknowledgementTrackingDescription[] =
    "Enable acknowledgement tracking for info cards.";

const char kInstanceSwitcherName[] = "Enable instance switcher";
const char kInstanceSwitcherDescription[] =
    "Enable instance switcher dialog UI that helps users manage multiple "
    "instances of Chrome.";

const char kInstantStartName[] = "Instant start";
const char kInstantStartDescription[] =
    "Show start surface before native library is loaded.";

const char kInterestFeedV2Name[] = "Interest Feed v2";
const char kInterestFeedV2Description[] =
    "Show content suggestions on the New Tab Page and Start Surface using the "
    "new Feed Component.";

const char kInterestFeedV2HeartsName[] = "Interest Feed v2 Hearts";
const char kInterestFeedV2HeartsDescription[] = "Enable hearts on Feedv2.";

const char kInterestFeedV2AutoplayName[] = "Interest Feed v2 Autoplay";
const char kInterestFeedV2AutoplayDescription[] = "Enable autoplay on Feedv2.";

const char kMediaPickerAdoptionStudyName[] = "Android Media Picker Adoption";
const char kMediaPickerAdoptionStudyDescription[] =
    "Controls how to launch the Android Media Picker (note: This flag is "
    "ignored as of Android U)";

const char kMessagesForAndroidAdsBlockedName[] = "Ads Blocked Messages UI";
const char kMessagesForAndroidAdsBlockedDescription[] =
    "When enabled, ads blocked message will use the new Messages UI.";

const char kMessagesForAndroidInfrastructureName[] = "Messages infrastructure";
const char kMessagesForAndroidInfrastructureDescription[] =
    "When enabled, will initialize Messages UI infrastructure";

const char kMessagesForAndroidOfferNotificationName[] =
    "Offer Notification Messages UI";
const char kMessagesForAndroidOfferNotificationDescription[] =
    "When enabled, offer notification will use the new Messages UI";

const char kMessagesForAndroidPermissionUpdateName[] =
    "Permission Update Messages UI";
const char kMessagesForAndroidPermissionUpdateDescription[] =
    "When enabled, permission update prompt will use the new Messages UI.";

const char kMessagesForAndroidPopupBlockedName[] = "Popup Blocked Messages UI";
const char kMessagesForAndroidPopupBlockedDescription[] =
    "When enabled, popup blocked prompt will use the new Messages UI.";

const char kMessagesForAndroidPWAInstallName[] = "PWA Installation Messages UI";
const char kMessagesForAndroidPWAInstallDescription[] =
    "When enabled, PWA Installation prompt will use the new Messages UI.";

const char kMessagesForAndroidSaveCardName[] = "Save Card Messages UI";
const char kMessagesForAndroidSaveCardDescription[] =
    "When enabled, save card prompt will use the new Messages UI.";

const char kMessagesForAndroidStackingAnimationName[] =
    "Stacking Animation of Messages UI";
const char kMessagesForAndroidStackingAnimationDescription[] =
    "When enabled, Messages UI will use the new stacking animation.";

const char kMobilePWAInstallPromptMlName[] =
    "Use ML to show mobile PWA install prompt";
const char kMobilePWAInstallPromptMlDescription[] =
    "When enabled, will use ML result to decide whether mobile PWA install "
    "prompt should be shown.";

const char kMouseAndTrackpadDropdownMenuName[] =
    "Android Mouse & Trackpad Drop-down Text Selection Menu";
const char kMouseAndTrackpadDropdownMenuDescription[] =
    "When enabled, shows a dropdown menu for mouse and trackpad secondary "
    "clicks (i.e. right click) with respect to text selection.";

const char kNewTabSearchEngineUrlAndroidName[] =
    "Enable new Tab Urls of customized search engines";
const char kNewTabSearchEngineUrlAndroidDescription[] =
    "Swap out NTP and Start surface according to a user's default search "
    "engine.";

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

const char kOfflinePagesLivePageSharingName[] =
    "Enables live page sharing of offline pages";
const char kOfflinePagesLivePageSharingDescription[] =
    "Enables to share current loaded page as offline page by saving as MHTML "
    "first.";

const char kPageInfoHistoryName[] = "Page info history";
const char kPageInfoHistoryDescription[] =
    "Enable a history sub page to the page info menu, and a button to forget "
    "a site, removing all preferences and history.";

const char kPasswordGenerationBottomSheetName[] =
    "Password generation bottom sheet";
const char kPasswordGenerationBottomSheetDescription[] =
    "Enabled showing the password generation bottom sheet.";

const char kQueryTilesName[] = "Show query tiles";
const char kQueryTilesDescription[] = "Shows query tiles in Chrome";
const char kQueryTilesNTPName[] = "Show query tiles in NTP";
const char kQueryTilesNTPDescription[] = "Shows query tiles in NTP";
const char kQueryTilesOnStartName[] = "Query tiles on start";
const char kQueryTilesOnStartDescription[] =
    "Show query tiles on start surface";
const char kQueryTilesSingleTierName[] = "Show only one level of query tiles";
const char kQueryTilesSingleTierDescription[] =
    "Show only one level of query tiles";
const char kQueryTilesEnableQueryEditingName[] =
    "Query Tiles - Enable query edit mode";
const char kQueryTilesEnableQueryEditingDescription[] =
    "When a query tile is tapped, the query text will be shown in the omnibox "
    "and user will have a chance to edit the text before submitting";
const char kQueryTilesEnableTrendingName[] =
    "Query Tiles - Enable trending queries";
const char kQueryTilesEnableTrendingDescription[] =
    "Allow tiles of trending queries to show up in front of curated tiles";
const char kQueryTilesDisableCountryOverrideName[] =
    "Disable tre default country list for query tiles.";
const char kQueryTilesDisableCountryOverrideDescription[] =
    "Disable the default country list for query tiles. It is still "
    "possible to show query tiles through server experiments.";
const char kQueryTilesCountryCode[] = "Country code for getting tiles";
const char kQueryTilesCountryCodeDescription[] =
    "When query tiles are enabled, this value determines tiles for which "
    "country should be displayed.";
const char kQueryTilesCountryCodeUS[] = "US";
const char kQueryTilesCountryCodeIndia[] = "IN";
const char kQueryTilesCountryCodeBrazil[] = "BR";
const char kQueryTilesCountryCodeNigeria[] = "NG";
const char kQueryTilesCountryCodeIndonesia[] = "ID";
const char kQueryTilesInstantFetchName[] = "Query tile instant fetch";
const char kQueryTilesInstantFetchDescription[] =
    "Immediately schedule background task to fetch query tiles";
const char kQueryTilesRankTilesName[] = "Query Tiles - rank tiles on server";
const char kQueryTilesRankTilesDescription[] =
    "Rank tiles on server based on client context";
const char kQueryTilesSegmentationName[] =
    "Query Tiles - use segmentation rules";
const char kQueryTilesSegmentationDescription[] =
    "enable segmentation rules to decide whether to show query tiles";
const char kQueryTilesSwapTrendingName[] =
    "Query Tiles - Swap trending queries";
const char kQueryTilesSwapTrendingDescription[] =
    "Swap trending queries if user didn't click on them after several "
    "impressions";

const char kReadAloudName[] = "Read Aloud";
const char kReadAloudDescription[] = "Controls the Read Aloud feature";

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

const char kRecoverFromNeverSaveAndroidName[] =
    "UI to recover from never save passwords on Android";
const char kRecoverFromNeverSaveAndroidDescription[] =
    "Enables showing UI which allows for easy reverting of the decision to "
    "never save passwords on a certain webiste";

const char kReengagementNotificationName[] =
    "Enable re-engagement notifications";
const char kReengagementNotificationDescription[] =
    "Enables Chrome to use the in-product help system to decide when "
    "to show re-engagement notifications.";

const char kRelatedSearchesName[] =
    "Enables an experiment for Related Searches on Android";
const char kRelatedSearchesDescription[] =
    "Enables requesting related searches suggestions. These will be requested "
    "but not shown unless the UI flag is also enabled.";

const char kRequestDesktopSiteAdditionsName[] =
    "Secondary settings for request desktop site on Android.";
const char kRequestDesktopSiteAdditionsDescription[] =
    "Secondary options in `Site settings` to request the desktop version of "
    "websites based on external display or peripheral.";

const char kRequestDesktopSiteDefaultsName[] =
    "Default settings for request desktop site on Android.";
const char kRequestDesktopSiteDefaultsDescription[] =
    "Request the desktop version of websites by default based on device "
    "conditions.";

const char kRequestDesktopSiteDefaultsDowngradeName[] =
    "Downgrade default settings for request desktop site on Android.";
const char kRequestDesktopSiteDefaultsDowngradeDescription[] =
    "Disable the request desktop site global setting if it was enabled by "
    "default based on device conditions.";

const char kRequestDesktopSiteDefaultsLoggingName[] =
    "Silently report crashes for debugging request desktop site on Android.";
const char kRequestDesktopSiteDefaultsLoggingDescription[] =
    "Silently report crashes with display spec when ineligible device shows "
    "up in cohort or device screen size exceeds threshold.";

const char kRequestDesktopSiteWindowSettingName[] =
    "Window setting for request desktop site on Android.";
const char kRequestDesktopSiteWindowSettingDescription[] =
    "Secondary option in `Site settings` to request the desktop version of "
    "websites based on window width.";

const char kRequestDesktopSiteZoomName[] =
    "Default zoom for request desktop site on Android.";
const char kRequestDesktopSiteZoomDescription[] =
    "Apply default page zoom on the desktop version of websites.";

const char kForceOffTextAutosizingName[] =
    "Force off heuristics for inflating text sizes on devices with small "
    "screens.";
const char kForceOffTextAutosizingDescription[] = "Disable text autosizing.";

const char kGridTabSwitcherAndroidAnimationsName[] =
    "Grid tab switcher Android animations";
const char kGridTabSwitcherAndroidAnimationsDescription[] =
    "Run grid tab switcher shrink & expand animations in Android instead of "
    "using the compositor.";

const char kGridTabSwitcherLandscapeAspectRatioPhonesName[] =
    "Landscape aspect ratio for thumbnails on tab switcher";
const char kGridTabSwitcherLandscapeAspectRatioPhonesDescription[] =
    "Use a landscape aspect ratio for tab thumbnails on the grid tab switcher. "
    "Phone only.";

const char kHideTabOnTabSwitcherName[] = "Hide tab on tab switcher";
const char kHideTabOnTabSwitcherDescription[] =
    "Hide the web contents of the foreground tab when on the tab switcher.";

const char kRevokeNotificationsPermissionIfDisabledOnAppLevelName[] =
    "Revoke site-level notification permission on Android";
const char kRevokeNotificationsPermissionIfDisabledOnAppLevelDescription[] =
    "Allow revoking site-level notification permission if Chrome has no "
    "app-level notification permission on Android.";

const char kSafeBrowsingHashPrefixRealTimeLookupsName[] =
    "Safe Browsing Hash Prefix Real Time Lookups";
const char kSafeBrowsingHashPrefixRealTimeLookupsDescription[] =
    "Enable checking URLs through Safe Browsing hash-prefix real time "
    "protocol.";

const char kScreenshotsForAndroidV2Name[] = "Screenshots for Android V2";
const char kScreenshotsForAndroidV2Description[] =
    "Adds functionality to the share screenshot panel within Chrome Browser"
    " on Android";

const char kShowNtpAtStartupAndroidName[] = "Show a NewTabPage at startup";
const char kShowNtpAtStartupAndroidDescription[] =
    "Enable showing a NewTabPage at startup after leaving Chrome for a while.";

const char kShowScrollableMVTOnNTPAndroidName[] =
    "Show scrollable MVT on NTP on tablets";
const char kShowScrollableMVTOnNTPAndroidDescription[] =
    "Enable showing the scrollable most visited tiles on NTP on tablets.";

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

const char kShareSheetCustomActionsPolishName[] =
    "Share sheet custom actions polish";
const char kShareSheetCustomActionsPolishDescription[] =
    "Polish Chrome provided custom actions for share sheet including dropping "
    "low engagement actions, and shuffle the ordering. Android only.";

const char kShareSheetMigrationAndroidName[] = "Share sheet refactor Android";
const char kShareSheetMigrationAndroidDescription[] =
    "When enabled, use the Android OS share sheet.";

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

const char kStartSurfaceAndroidName[] = "Start Surface";
const char kStartSurfaceAndroidDescription[] =
    "Enable showing the start surface when launching Chrome via the "
    "launcher.";

const char kFeedPositionAndroidName[] = "Feed Position";
const char kFeedPositionAndroidDescription[] =
    "Enable pushing down or pulling up of Feeds on NTP";

const char kSearchResumptionModuleAndroidName[] = "Search Resumption Module";
const char kSearchResumptionModuleAndroidDescription[] =
    "Enable showing search suggestions on NTP";

const char kSiteDataImprovementsName[] = "Site data improvements";
const char kSiteDataImprovementsDescription[] =
    "Improved 'All sites' and 'Site settings' pages on Android.";

const char kStartSurfaceDisabledFeedImprovementName[] =
    "Start Surface Disabled Feed Improvement";
const char kStartSurfaceDisabledFeedImprovementDescription[] =
    "Enable improving Start surface when Feed is disabled";

const char kStartSurfaceOnTabletName[] = "Start Surface On Tablet";
const char kStartSurfaceOnTabletDescription[] =
    "Enable Start Surface On Tablet.";

const char kStartSurfaceRefactorName[] = "Start Surface Refactor";
const char kStartSurfaceRefactorDescription[] =
    "Enable splitting Tab switcher from Start surface";

const char kStartSurfaceSpareTabName[] = "Start Surface Spare Tab";
const char kStartSurfaceSpareTabDescription[] =
    "Enable using Spare Tab for navigations from Start Surface.";

const char kStartSurfaceWithAccessibilityName[] =
    "Start Surface With Accessibility";
const char kStartSurfaceWithAccessibilityDescription[] =
    "Enable Start Surface with Accessibility.";

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

const char kSurfaceControlMagnifierName[] = "Surface control magnifier";
const char kSurfaceControlMagnifierDescription[] =
    "Use magnifier built using SurfaceControl. Depends on SurfaceControl, "
    "Slim compositor, and Android OS support. No effect if enabled on "
    "unsupported environment.";

const char kSurfacePolishName[] = "Surface Polish";
const char kSurfacePolishDescription[] =
    "Enable clank home surface polish for Start surface and NTP.";

const char kThumbnailPlaceholderName[] = "Thumbnail placeholder";
const char kThumbnailPlaceholderDescription[] =
    "Display a placeholder image for missing thumbnails.";

const char kTabStripRedesignAndroidName[] = "Tab Strip Redesign Android.";
const char kTabStripRedesignAndroidDescription[] =
    "Enabled Tab Strip Redesign on Android - A visual redesign of Clank Tab "
    "Strip that is consistent with GM3.";

const char kTabletToolbarReorderingAndroidName[] =
    "Tablet Toolbar Reordering Android.";
const char kTabletToolbarReorderingAndroidDescription[] =
    "Enable Tablet Toolbar Reordering on Android - Reorder the toolbar by "
    "placing the Home button from 1st to the 4th position after the Refresh"
    "button to match the Desktop toolbar";

const char kEmptyStatesAndroidName[] = "Empty States Android.";
const char kEmptyStatesAndroidDescription[] =
    "Enabled Clank Empty States on Android - Add illustrations to Clank empty "
    "states to update tab switcher UI, recent tabs, bookmarks, reading list "
    "and history zero states";

const char kTabStripStartupRefactoringName[] =
    "Refactor for tablet tab strip startup.";
const char kTabStripStartupRefactoringDescription[] =
    "Enables refactor for tablet tab strip startup. This creates placeholder "
    "tabs before the tab strip is initialized to prevent "
    "jank (tabs seeming to quickly flicker / scroll).";

const char kBaselineGM3SurfaceColorsName[] = "Baseline GM3 Surface Colors";
const char kBaselineGM3SurfaceColorsDescription[] =
    "Updates baseline surface colors to match the GM3 formula.";

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

const char kTranslateMessageUIName[] = "Translate Message UI";
const char kTranslateMessageUIDescription[] =
    "Controls whether the Translate Message UI will be shown instead of the "
    "Translate InfoBar.";

const char kUnifiedPasswordManagerLocalPasswordsAndroidNoMigrationName[] =
    "Google Mobile Services for passwords for users with empty local password "
    "storage";
const char
    kUnifiedPasswordManagerLocalPasswordsAndroidNoMigrationDescription[] =
        "Uses Google Mobile Services to store and retrieve passwords."
        "This only applies for users with no passwords saved locally and "
        "default password settings."
        "Warning: Highly experimental. May lead to loss of passwords and "
        "impact performance.";

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

const char kAdaptiveButtonInTopToolbarName[] = "Adaptive button in top toolbar";
const char kAdaptiveButtonInTopToolbarDescription[] =
    "Enables showing an adaptive action button in the top toolbar";

const char kAdaptiveButtonInTopToolbarTranslateName[] =
    "Adaptive button in top toolbar - Translate button";
const char kAdaptiveButtonInTopToolbarTranslateDescription[] =
    "Enables a translate button in the top toolbar. Must be selected in "
    "Settings > Toolbar Shortcut.";
const char kAdaptiveButtonInTopToolbarAddToBookmarksName[] =
    "Adaptive button in top toolbar - Add to bookmarks button";
const char kAdaptiveButtonInTopToolbarAddToBookmarksDescription[] =
    "Enables an add to bookmarks button in the top toolbar. Must be selected "
    "in "
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

const char kWebApkInstallFailureNotificationName[] =
    "Web app install failure notification";
const char kWebApkInstallFailureNotificationDescription[] =
    "Enables showing a notification when web app install failed";

const char kWebApkInstallFailureRetryName[] = "Web app install retry";
const char kWebApkInstallFailureRetryDescription[] =
    "Allows user to retry failed web app installs with the failure "
    "notification";

const char kWebFeedName[] = "Web Feed";
const char kWebFeedDescription[] =
    "Allows users to keep up with and consume web content.";

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

const char kPasswordEditDialogWithDetailsName[] =
    "Password edit dialog with details UI";
const char kPasswordEditDialogWithDetailsDescription[] =
    "Enables UI which shows the dialog after clicking on save/update password"
    " with the functionality to choose user account and edit the password.";

const char kPasswordSuggestionBottomSheetV2Name[] =
    "Refactored password suggestions bottom sheet";
const char kPasswordSuggestionBottomSheetV2Description[] =
    "Enables the refactored version of the password suggestions bottom sheet. "
    "All the user facing functionality should stay the same.";

const char kEnableAndroidGamepadVibrationName[] = "Gamepad vibration";
const char kEnableAndroidGamepadVibrationDescription[] =
    "Enables the ability to play vibration effects on supported gamepads.";

#if BUILDFLAG(ENABLE_VR) && BUILDFLAG(ENABLE_OPENXR)
const char kOpenXRName[] = "Enable OpenXR WebXR Runtime";
const char kOpenXRDescription[] =
    "Enables the use of the OpenXR runtime to create WebXR sessions.";
#endif

// Non-Android -----------------------------------------------------------------

#else  // BUILDFLAG(IS_ANDROID)

const char kAppManagementAppDetailsName[] =
    "Enable App Details in App Management.";
const char kAppManagementAppDetailsDescription[] =
    "Show app details on an app's App Management page.";

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

const char kCastMirroringTargetPlayoutDelayName[] =
    "Changes the target playout delay for cast mirroring.";
const char kCastMirroringTargetPlayoutDelayDescription[] =
    "Choose a target playout delay for cast mirroring. A lower delay will "
    "decrease latency, but may come at the cost of other quality standards "
    "such as dropped frames or FPS.";
const char kCastMirroringTargetPlayoutDelayDefault[] = "400ms (default)";
const char kCastMirroringTargetPlayoutDelay100ms[] = "100ms.";
const char kCastMirroringTargetPlayoutDelay150ms[] = "150ms.";
const char kCastMirroringTargetPlayoutDelay200ms[] = "200ms.";
const char kCastMirroringTargetPlayoutDelay250ms[] = "250ms.";
const char kCastMirroringTargetPlayoutDelay300ms[] = "300ms.";
const char kCastMirroringTargetPlayoutDelay350ms[] = "3500ms.";

const char kCopyLinkToTextName[] = "Copy Link To Text";
const char kCopyLinkToTextDescription[] =
    "Adds an item to the context menu to allow a user to copy a link to the "
    "page with the selected text highlighted.";

const char kEnableAccessibilityLiveCaptionName[] = "Live Caption";
const char kEnableAccessibilityLiveCaptionDescription[] =
    "Enables the live caption feature which generates captions for "
    "media playing in Chrome. Turn the feature on in "
    "chrome://settings/accessibility.";

const char kReadAnythingName[] = "Reading Mode";
const char kReadAnythingDescription[] =
    "Enables the Reading Mode feature which generates a reader-friendly view "
    "of web pages. Open the side panel and select Reading Mode to try the "
    "feature.";

const char kReadAnythingWithScreen2xName[] = "Reading Mode with Screen2x";
const char kReadAnythingWithScreen2xDescription[] =
    "Have Reading Mode use a local machine learning model for web page "
    "distillation.";

const char kReadAnythingWebUIToolbarName[] = "Reading Mode WebUI Toolbar";
const char kReadAnythingWebUIToolbarDescription[] =
    "Enables the Reading Mode toolbar implemented with WebUI instead of with "
    "Views.";

const char kReadAnythingReadAloudName[] = "Reading Mode Read Aloud";
const char kReadAnythingReadAloudDescription[] =
    "Enables the experimental Read Aloud feature in Reading Mode.";

const char kEnableWebHidOnExtensionServiceWorkerName[] =
    "Enable WebHID on extension service workers";
const char kEnableWebHidOnExtensionServiceWorkerDescription[] =
    "When enabled, WebHID API is available on extension service workers.";

const char kGlobalMediaControlsCastStartStopName[] =
    "Global media controls control Cast start/stop";
const char kGlobalMediaControlsCastStartStopDescription[] =
    "Allows global media controls to control when a Cast session is started "
    "or stopped instead of relying on the Cast dialog.";

const char kHeuristicMemorySaverName[] =
    "Enable the heuristics-based policy for Memory Saver Mode.";
const char kHeuristicMemorySaverDescription[] =
    "When enabled, Memory Saver will take multiple signals into account before "
    "discarding a tab rather than doing it after a fixed amount of time in the "
    "background.";

const char kHighEfficiencyMultistateModeAvailableName[] =
    "Enable the multi-state option for Memory Saver Mode.";
const char kHighEfficiencyMultistateModeAvailableDescription[] =
    "When enabled, Memory Saver can take one of three options: enabled with a "
    "heuristic mode, enabled with a fixed timer, and discarded. Configure this "
    "through the settings page.";

const char kHighEfficiencyDiscardedTabTreatmentName[] =
    "Enable discarded tab treatment for Memory Saver Mode.";
const char kHighEfficiencyDiscardedTabTreatmentDescription[] =
    "When enabled, discarded tabs will have a modified favicon to indicate "
    "that state.";

const char kHighEfficiencyMemoryUsageInHovercardsName[] =
    "Show memory usage in hovercards.";
const char kHighEfficiencyMemoryUsageInHovercardsDescription[] =
    "When enabled, memory usage for active tabs can be found in their "
    "hovercards.";

const char kHighEfficiencyDiscardExceptionsImprovementsName[] =
    "Enable improvements to creating tab discard exceptions.";
const char kHighEfficiencyDiscardExceptionsImprovementsDescription[] =
    "When enabled, tab discard exceptions can be created from the Memory Saver "
    "page action chip dialog and they can be created from currently open tabs "
    "via the settings page.";

const char kHighEfficiencySavingsReportingImprovementsName[] =
    "Enable improvements to how memory savings are reported.";
const char kHighEfficiencySavingsReportingImprovementsDescription[] =
    "When enabled, the Memory Saver page action chip and dialog will be used "
    "to highlight memory savings.";

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const char kIOSPromoPasswordBubbleName[] =
    "Contextual Chrome for iOS promo in the password save/update bubble.";
const char kIOSPromoPasswordBubbleDecription[] =
    "When enabled, a contextual Chrome for iOS promo will be shown to eligible "
    "users. The different flag options are for the promo's activation.";
#endif

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

const char kNtpCacheOneGoogleBarName[] = "Cache OneGoogleBar";
const char kNtpCacheOneGoogleBarDescription[] =
    "Enables using the OneGoogleBar cached response in chrome://new-tab-page, "
    "when available.";

const char kNtpChromeCartModuleName[] = "NTP Chrome Cart Module";
const char kNtpChromeCartModuleDescription[] =
    "Shows the chrome cart module on the New Tab Page.";

const char kNtpComprehensiveThemeRealboxName[] =
    "NTP Comprehensive Theme Realbox";
const char kNtpComprehensiveThemeRealboxDescription[] =
    "Applies theme based colors to the NTP Realbox element";

const char kNtpDesktopLensName[] = "NTP Desktop Lens Entrypoint";
const char kNtpDesktopLensDescription[] =
    "Shows a Lens entrypoint and upload dialog in desktop NTP when enabled.";

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

const char kNtpHistoryClustersModuleName[] = "NTP Journeys Module";
const char kNtpHistoryClustersModuleDescription[] =
    "Shows the Journeys module on the New Tab Page.";

const char kNtpHistoryClustersModuleSuggestionChipHeaderName[] =
    "NTP Journeys Module Suggestion Chip Header ";
const char kNtpHistoryClustersModuleSuggestionChipHeaderDescription[] =
    "Shows the historical suggestion chip in the header if enabled.";

const char kNtpHistoryClustersModuleUseModelRankingName[] =
    "NTP Journeys Module Model Ranking";
const char kNtpHistoryClustersModuleUseModelRankingDescription[] =
    "Leverages a machine learning model to rank clusters for the Journeys "
    "module on the New Tab Page.";

const char kNtpHistoryClustersModuleTextOnlyName[] =
    "NTP Journeys Module Text Only";
const char kNtpHistoryClustersModuleTextOnlyDescription[] =
    "Shows only text (i.e. no images) for Journeys module visits on the New "
    "Tab Page.";

const char kNtpChromeCartInHistoryClustersModuleName[] =
    "NTP ChromeCart in Journeys Module";
const char kNtpChromeCartInHistoryClustersModuleDescription[] =
    "Shows ChromeCart tile in the Journeys module when available on the New "
    "Tab Page.";

const char kNtpChromeCartHistoryClusterCoexistName[] =
    "NTP ChromeCart and Journeys Module Coexist";
const char kNtpChromeCartHistoryClusterCoexistDescription[] =
    "Shows ChromeCart module and ChromeCart+Journeys module together when "
    "available on the New Tab Page.";

const char kNtpDiscountsInHistoryClustersModuleName[] =
    "NTP discounts in Journeys Module";
const char kNtpDiscountsInHistoryClustersModuleDescription[] =
    "Shows discounts on the visit tiles in the Journeys module when available "
    "on the New Tab Page.";

const char kNtpTabResumptionModuleName[] =
    "NTP Cross Device Tab Resumption Module";
const char kNtpTabResumptionModuleDescription[] =
    "Shows the Cross Device Tab Resumption Module on the New Tab Page.";

const char kNtpMiddleSlotPromoDismissalName[] =
    "NTP Middle Slot Promo Dismissal";
const char kNtpMiddleSlotPromoDismissalDescription[] =
    "Allows middle slot promo to be dismissed from New Tab Page until "
    "new promo message is populated.";

const char kNtpModulesDragAndDropName[] = "NTP Modules Drag and Drop";
const char kNtpModulesDragAndDropDescription[] =
    "Enables modules to be reordered via dragging and dropping on the "
    "New Tab Page.";

const char kNtpModulesFirstRunExperienceName[] =
    "NTP Modules First Run Experience";
const char kNtpModulesFirstRunExperienceDescription[] =
    "Shows first run experience for Modular NTP Desktop v1.";

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

const char kNtpRealboxIsTallName[] = "NTP Realbox Is Tall";
const char kNtpRealboxIsTallDescription[] =
    "Makes NTP Realbox taller when enabled.";

const char kNtpRealboxCr23AllName[] = "Realbox Chrome Refresh 2023";
const char kNtpRealboxCr23AllDescription[] =
    "Enables all NTP Realbox Chrome Refresh features";

const char kNtpRealboxMatchSearchboxThemeName[] =
    "NTP Realbox Matches Searchbox Theme";
const char kNtpRealboxMatchSearchboxThemeDescription[] =
    "Makes NTP Realbox drop shadow match that of the Searchbox when enabled.";

const char kNtpRealboxPedalsName[] = "NTP Realbox Pedals";
const char kNtpRealboxPedalsDescription[] =
    "Shows pedals in the NTP Realbox when enabled.";

const char kNtpRealboxWidthBehaviorName[] = "NTP Realbox Width Behavior";
const char kNtpRealboxWidthBehaviorDescription[] =
    "Determines the width of the NTP realbox.";

const char kNtpRealboxUseGoogleGIconName[] = "NTP Realbox Google G Icon";
const char kNtpRealboxUseGoogleGIconDescription[] =
    "Shows Google G icon "
    "instead of Search Loupe in realbox when enabled";

const char kNtpRecipeTasksModuleName[] = "NTP Recipe Tasks Module";
const char kNtpRecipeTasksModuleDescription[] =
    "Shows the recipe tasks module on the New Tab Page.";

const char kNtpReducedLogoSpaceName[] = "NTP Reduced Logo Space";
const char kNtpReducedLogoSpaceDescription[] =
    "Reduces space the logo fills up vertically.";

const char kNtpSafeBrowsingModuleName[] = "NTP Safe Browsing Module";
const char kNtpSafeBrowsingModuleDescription[] =
    "Shows the safe browsing module on the New Tab Page.";

const char kNtpSingleRowShortcutsName[] = "NTP Single Row Shortcuts";
const char kNtpSingleRowShortcutsDescription[] =
    "Shows shortcuts in a single wide row on the New Tab Page.";

const char kNtpWideModulesName[] = "NTP Wide Modules";
const char kNtpWideModulesDescription[] =
    "Shows wide NTP modules if NTP provides enough space.";

const char kEnableReaderModeName[] = "Enable Reader Mode";
const char kEnableReaderModeDescription[] =
    "Allows viewing of simplified web pages by selecting 'Customize and "
    "control Chrome'>'Distill page'";

const char kHappinessTrackingSurveysForDesktopDemoName[] =
    "Happiness Tracking Surveys Demo";
const char kHappinessTrackingSurveysForDesktopDemoDescription[] =
    "Enable showing Happiness Tracking Surveys Demo to users on Desktop";

const char kHatsWebUIName[] = "HaTS in Chrome WebUI";
const char kHatsWebUIDescription[] =
    "Enables the Happiness Tracking Surveys being delivered via chrome webui, "
    "rather than a separate static website.";

const char kLayoutExtractionName[] = "Layout Extraction";
const char kLayoutExtractionDescription[] =
    "Enables Layout Extraction local machine intelligence library to use "
    "screen snapshots to add metadata for accessibility tools.";

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

const char kRealboxSecondaryZeroSuggestName[] =
    "Enables showing secondary zero-prefix suggestions in NTP realbox.";
const char kRealboxSecondaryZeroSuggestDescription[] =
    "When enabled, allows showing secondary zero-prefix suggestions in NTP "
    "realbox.";

const char kSCTAuditingName[] = "SCT auditing";
const char kSCTAuditingDescription[] =
    "Enables SCT auditing for users who have opted in to Safe Browsing "
    "Extended Reporting.";

const char kSmartCardWebApiName[] = "Smart Card API";
const char kSmartCardWebApiDescription[] =
    "Enable access to the Smart Card API. See "
    "https://github.com/WICG/web-smart-card#readme for more information.";

const char kSyncWebauthnCredentialsName[] = "Sync WebAuthn credentials";
const char kSyncWebauthnCredentialsDescription[] =
    "Allow syncing, managing, and displaying Google Password Manager WebAuthn "
    "credential ('passkey') metadata";

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const char kSettingsEnableGetTheMostOutOfChromeName[] =
    "'Get the most out of Chrome' documentation";
const char kSettingsEnableGetTheMostOutOfChromeDescription[] =
    "When enabled, the 'Get the most out of Chrome' documentation section "
    "will be available.";
#endif

const char kWebAppDedupeInstallUrlsName[] = "Web App Dedupe Install URLs";
const char kWebAppDedupeInstallUrlsDescription[] =
    "Enables a clean up fix for web apps that were installed by external "
    "(non-user) sources that install the same URL but result in different web "
    "apps being installed.";

const char kWebAppManifestImmediateUpdatingName[] =
    "Web App Manifest Immediate Updating";
const char kWebAppManifestImmediateUpdatingDescription[] =
    "Enables web app manifest updates to apply to running web app windows as "
    "soon as a change has been detected instead of waiting for all app windows "
    "to be closed.";

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

const char kWebAppSystemMediaControlsWinName[] =
    "Web App System Media Controls on Windows";
const char kWebAppSystemMediaControlsWinDescription[] =
    "Enable instanced system media controls for web apps";

const char kWebAuthenticationNewPasskeyUIName[] = "Enable new passkey UI";
const char kWebAuthenticationNewPasskeyUIDescription[] =
    "Enable the new passkey UI that emphasizes individual passkeys instead of "
    "authenticators where possible.";

const char kWebAuthenticationPermitEnterpriseAttestationName[] =
    "Web Authentication Enterprise Attestation";
const char kWebAuthenticationPermitEnterpriseAttestationDescription[] =
    "Permit a set of origins to request a uniquely identifying enterprise "
    "attestation statement from a security key when creating a Web "
    "Authentication credential.";

const char kDevToolsTabTargetLiteralName[] = "DevTools using Tab Target";
const char kDevToolsTabTargetLiteralDescription[] =
    "Makes DevTools use an experimental CDP Tab target.";

const char kNewConfirmationBubbleForGeneratedPasswordsName[] =
    "New confirmation bubble for forms with generated password";
const char kNewConfirmationBubbleForGeneratedPasswordsDescription[] =
    "Enables new confirmation bubble flow for forms where generated password "
    "was used.";

#endif  // BUILDFLAG(IS_ANDROID)

// Windows ---------------------------------------------------------------------

#if BUILDFLAG(IS_WIN)

const char kCalculateNativeWinOcclusionName[] =
    "Calculate window occlusion on Windows";
const char kCalculateNativeWinOcclusionDescription[] =
    "Calculate window occlusion on Windows will be used in the future "
    "to throttle and potentially unload foreground tabs in occluded windows";

const char kCloudApAuthAttachAsHeaderName[] =
    "CloudAP authentication data headers";
const char kCloudApAuthAttachAsHeaderDescription[] =
    "Allows certain ambient authentication data to be added to HTTP requests "
    "as separate headers instead of being appended to the cookie "
    "header.";

const char kEnableMediaFoundationVideoCaptureName[] =
    "MediaFoundation Video Capture";
const char kEnableMediaFoundationVideoCaptureDescription[] =
    "Enable/Disable the usage of MediaFoundation for video capture. Fall back "
    "to DirectShow if disabled.";

const char kHardwareSecureDecryptionName[] = "Hardware Secure Decryption";
const char kHardwareSecureDecryptionDescription[] =
    "Enable/Disable the use of hardware secure Content Decryption Module (CDM) "
    "for protected content playback.";

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

const char kRunVideoCaptureServiceInBrowserProcessName[] =
    "Run video capture service in browser";
const char kRunVideoCaptureServiceInBrowserProcessDescription[] =
    "Run the video capture service in the browser process.";

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

#if BUILDFLAG(ENABLE_PRINTING)
const char kCupsIppPrintingBackendName[] = "CUPS IPP Printing Backend";
const char kCupsIppPrintingBackendDescription[] =
    "Use the CUPS IPP printing backend instead of the original CUPS backend "
    "that calls the PPD API.";
#endif  // BUILDFLAG(ENABLE_PRINTING)

const char kImmersiveFullscreenName[] = "Immersive Fullscreen Toolbar";
const char kImmersiveFullscreenDescription[] =
    "Automatically hide and show the toolbar in fullscreen.";

const char kMacLoopbackAudioForCastName[] =
    "Mac System Audio Loopback for Cast";
const char kMacLoopbackAudioForCastDescription[] =
    "Enable system audio mirroring when casting a screen on macOS 13.0+.";

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

const char kScreenTimeName[] = "Screen Time";
const char kScreenTimeDescription[] =
    "Integrate with the macOS Screen Time system. Only enabled on macOS 12.1 "
    "and later.";

const char kUseAngleDescriptionMac[] =
    "Choose the graphics backend for ANGLE. The OpenGL backend is soon to be "
    "deprecated on Mac, and may contain driver bugs that are not planned to be "
    "fixed. The Metal backend is still experimental, and may contain bugs that "
    "are still being worked on. The Metal backend should be more performant, "
    "but may still be behind the OpenGL backend until fully released.";

const char kUseAngleMetal[] = "Metal";

const char kSwapBackquoteKeysInISOKeyboardName[] =
    "Swap Backquote Keys In ISO Keyboard";
const char kSwapBackquoteKeysInISOKeyboardDescription[] =
    "Swap Backquote and IntlBackslash keys when using ISO keyboard on macOS";

const char kSystemColorChooserName[] = "System Color Chooser";
const char kSystemColorChooserDescription[] =
    "Enables a button that launches the macOS native color chooser.";

const char kVideoToolboxAv1DecodingName[] = "VideoToolbox AV1 decoding support";
const char kVideoToolboxAv1DecodingDescription[] =
    "Controls support for accelerated AV1 decoding through VideoToolbox.";

#endif

// Windows and Mac -------------------------------------------------------------

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

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

const char kAdvancedDocumentScanApiName[] =
    "Enable advanced chrome.documentScan APIs";
const char kAdvancedDocumentScanApiDescription[] =
    "Enable chrome.documentScan functions that provide full-featured access "
    "to SANE scanners.  Also enable AsynchronousScannerDiscovery to get the "
    "full enhanced functionality.";

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

const char kAllowEapDefaultCasWithoutSubjectVerificationName[] =
    "Allow EAP network configs with default server CAs without subject "
    "verification";
const char kAllowEapDefaultCasWithoutSubjectVerificationDescription[] =
    "Allows creating EAP network configs which use the default server CA certs "
    "without specifying subject or domain match options which validate the "
    "identity of the server.";

const char kAllowRepeatedUpdatesName[] =
    "Continue checking for updates before reboot and after initial update.";
const char kAllowRepeatedUpdatesDescription[] =
    "Continues checking to see if there is a more recent update, even if user"
    "has not rebooted to apply the previous update.";

const char kAllowScrollSettingsName[] =
    "Allow changes to scroll acceleration/sensitivity for mice.";
const char kAllowScrollSettingsDescription[] =
    "Shows settings to enable/disable scroll acceleration and to adjust the "
    "sensitivity for scrolling.";

const char kAlmanacGameMigrationName[] =
    "Use Almanac for games in App Discovery Service";
const char kAlmanacGameMigrationDescription[] =
    "Enables App Discovery Service to use the Almanac system for fetching "
    "games instead of the App Provisioning Component.";

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

const char kAmbientModeThrottleAnimationName[] =
    "Throttle the frame rate of Lottie animations in ambient mode";
const char kAmbientModeThrottleAnimationDescription[] =
    "The throttled frame rate and when to throttle are embedded within the "
    "Lottie animation file itself. It is chosen by the motion designer and "
    "varies depending on how much motion there is in the animation. This is "
    "done in the hopes of improving power consumption while maintaining the "
    "same user-visible smoothness. This flag applies to all ambient Lottie "
    "animations that have throttling specified in the file.";

const char kApnRevampName[] = "APN Revamp";
const char kApnRevampDescription[] =
    "Enables the ChromeOS APN Revamp, which updates cellular network APN "
    "system UI and related infrastructure.";

const char kAppInstallServiceUriName[] = "Enable app install service URI";
const char kAppInstallServiceUriDescription[] =
    "Allows app installs to be invoked from a specific URI.";

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

const char kArcFilePickerExperimentName[] =
    "Enable file picker experiment for ARC";
const char kArcFilePickerExperimentDescription[] =
    "Enables using ChromeOS file picker in ARC.";

const char kArcIdleManagerName[] = "Enable ARC Idle Manager";
const char kArcIdleManagerDescription[] =
    "ARC will turn on Android's doze mode when idle.";

const char kArcInstantResponseWindowOpenName[] =
    "Enable Instance Response for ARC app window open";
const char kArcInstantResponseWindowOpenDescription[] =
    "In some devices the placeholder window will popup immediately after the "
    "user attempts to launch apps.";

const char kArcKeyboardShortcutHelperIntegrationName[] =
    "Enable keyboard shortcut helper integration for ARC";
const char kArcKeyboardShortcutHelperIntegrationDescription[] =
    "Shows keyboard shortcuts from Android apps in ChromeOS Shortcut Viewer";

const char kArcNativeBridgeToggleName[] =
    "Toggle between native bridge implementations for ARC";
const char kArcNativeBridgeToggleDescription[] =
    "Toggle between native bridge implementations for ARC.";

const char kArcNearbyShareFuseBoxName[] = "ARC Nearby Sharing through FuseBox";
const char kArcNearbyShareFuseBoxDescription[] =
    "When enabled, files shared through ARC Nearby Sharing will be shared "
    "through the ChromeOS FuseBox Service.";

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

const char kArcVmmSwapKBShortcutName[] =
    "Keyboard shortcut trigger for ARCVM"
    " vmm swap feature";
const char kArcVmmSwapKBShortcutDesc[] =
    "Alt + Ctrl + Shift + O/P to enable / disable ARCVM vmm swap. Only for "
    "experimental usage.";

const char kArcXdgModeName[] = "Enable XDG mode for ARC apps.";
const char kArcXdgModeDesc[] =
    "Switch to XDG-based Wayland protocols for ARC apps.";

const char kArcEnableAAudioMMAPName[] = "Enable ARCVM AAudio MMAP";
const char kArcEnableAAudioMMAPDescription[] =
    "Enable AAudio MMAP support for ARCVM which provides low latency audio "
    "for supported apps.";

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

const char kAshEnableUnifiedDesktopName[] = "Unified desktop mode";
const char kAshEnableUnifiedDesktopDescription[] =
    "Enable unified desktop mode which allows a window to span multiple "
    "displays.";

const char kAsynchronousScannerDiscoveryName[] =
    "Asynchronous scanner discovery";
const char kAsynchronousScannerDiscoveryDescription[] =
    "Use the newer asynchronous document scanner discovery API for the Scan "
    "app and extension APIs.";

const char kAudioA2DPAdvancedCodecsName[] = "BT A2DP advanced codecs support";
const char kAudioA2DPAdvancedCodecsDescription[] =
    "Enable BT A2DP advanced codecs support";

const char kAudioAPNoiseCancellationName[] = "Audio noise cancellation on AP";
const char kAudioAPNoiseCancellationDescription[] =
    "Enable noise cancellation on AP";

const char kAudioFlexibleLoopbackName[] =
    "ChromeOS flexible loopback API support";
const char kAudioFlexibleLoopbackDescription[] =
    "Enable flexible loopback API support in ChromeOS.";

const char kAudioHFPMicSRName[] =
    "Audio super-resolution Bluetooth HFP microphone";
const char kAudioHFPMicSRDescription[] =
    "Enable super-resolution Bluetooth HFP microphone recording.";

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

const char kAvatarsCloudMigrationName[] =
    "Loading CrOS avatar images from the cloud";
const char kAvatarsCloudMigrationDescription[] =
    "Enables loading avatar images from the cloud on ChromeOS.";

const char kBluetoothFixA2dpPacketSizeName[] = "Bluetooth fix A2DP packet size";
const char kBluetoothFixA2dpPacketSizeDescription[] =
    "Fixes Bluetooth A2DP packet size to a smaller default value to improve "
    "audio quality and may fix audio stutter.";

const char kBluetoothFlossTelephonyName[] = "Bluetooth Floss Telephony";
const char kBluetoothFlossTelephonyDescription[] =
    "Enable Floss to create a Bluetooth HID device that allows applications to "
    "access Bluetooth telephony functions through WebHID.";

const char kBluetoothQualityReportName[] = "Bluetooth Quality Report";
const char kBluetoothQualityReportDescription[] =
    "Enables the Bluetooth Quality Report feature on Bluetooth controllers "
    "which will send the Bluetooth link quality statistics such as the "
    "signal strength, the lost packet count, etc. to the host.";

const char kBluetoothWbsDogfoodName[] = "Bluetooth WBS dogfood";
const char kBluetoothWbsDogfoodDescription[] =
    "Enables Bluetooth wideband speech mic as default audio option. "
    "Note that flipping this flag makes no difference on most of the "
    "ChromeOS models, because Bluetooth WBS is either unsupported "
    "or fully launched. Only on the few models that Bluetooth WBS is "
    "still stablizing this flag will take effect.";

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

const char kRobustAudioDeviceSelectLogicName[] =
    "Robust Audio Device Select Logic";
const char kRobustAudioDeviceSelectLogicDescription[] =
    "A more robust logic for automatic audio device selection which is more "
    "capable of remembering the user's preferences of audio devices.";

const char kBluetoothUseFlossName[] = "Use Floss instead of BlueZ";
const char kBluetoothUseFlossDescription[] =
    "Enables using Floss (also known as Fluoride, Android's Bluetooth stack) "
    "instead of Bluez. This is meant to be used by developers and is not "
    "guaranteed to be stable";

const char kBluetoothFlossIsAvailabilityCheckNeededName[] =
    "Floss availability check";
const char kBluetoothFlossIsAvailabilityCheckNeededDescription[] =
    "Floss availability is determined by each machine. If the machine set "
    "Floss is unavailable, Floss won't be enabled no matter what "
    "bluetooth-use-floss is set. Enable this flag will bypass the check so "
    "that users can still enable Floss by bluetooth-use-floss.";

const char kBluetoothUseLLPrivacyName[] = "Enable LL Privacy in BlueZ";
const char kBluetoothUseLLPrivacyDescription[] =
    "Enable address resolution offloading to Bluetooth Controller if "
    "supported. Modifying this flag will cause Bluetooth Controller to reset.";

const char kCaptureModeAudioMixingName[] =
    "Enable screen capture advanced audio settings";
const char kCaptureModeAudioMixingDescription[] =
    "Enables the ability to record the microphone, or system audio each "
    "separately, or mix them together in a single stream in the screen capture "
    "tool.";

const char kCaptureModeGifRecordingName[] =
    "Enable GIF recording in screen capture";
const char kCaptureModeGifRecordingDescription[] =
    "Enables the ability to record the screen into animated GIFs.";

const char kCrosShortstandName[] =
    "Differentiate behaviour between web apps and browser created shortcuts";
const char kCrosShortstandDescription[] =
    "Enables the behaviour difference between web apps and browser created "
    "shortcut backed by the web app system on Chrome OS.";

const char kCrosWebAppShortcutUiUpdateName[] =
    "New ChromeOS Web app Shortcut UI";
const char kCrosWebAppShortcutUiUpdateDescription[] =
    "Enables new UI for shortcuts created from browser that backed by web app"
    "system on ChromeOS.";

const char kDeskButtonName[] = "Desk button in shelf";
const char kDeskButtonDescription[] =
    "Show a desk button that provides quick access to the desk menu in the "
    "shelf in clamshell mode when there is more than one desk.";

const char kCrosBatterySaverAlwaysOnName[] =
    "Make ChromeOS Battery Saver on all the time";
const char kCrosBatterySaverAlwaysOnDescription[] =
    "Turns on ChomeOS Battery Saver all the time, even when charging or fully "
    "charged. Used for testing ChromeOS Battery Saver Mode.";

const char kCrosSoulName[] = "CrOS SOUL";
const char kCrosSoulDescription[] = "Enable the CrOS SOUL feature.";

const char kCrosBatterySaverName[] =
    "Enable ChromeOS Battery Saver Mode Support";
const char kCrosBatterySaverDescription[] =
    "Enables the ability to turn on battery saver mode in the ChromeOS Power "
    "Settings";

const char kDesksTemplatesName[] = "Desk Templates";
const char kDesksTemplatesDescription[] =
    "Streamline workflows by saving a group of applications and windows as a "
    "launchable template in a new desk";

const char kDnsOverHttpsWithIdentifiersReuseOldPolicyName[] =
    "Experiment: Allows using identifiers in the DoH template URI";
const char kDnsOverHttpsWithIdentifiersReuseOldPolicyDescription[] =
    "Enables early testing of the DoH template URI with identifiers by "
    "evaluating user identifiers in the existing policy DnsOverHttpsTemplates "
    "and hashing them using a hardcoded salt.";

const char kPreferConstantFrameRateName[] = "Prefer Constant Frame Rate";
const char kPreferConstantFrameRateDescription[] =
    "Enables this flag to prefer using constant frame rate for camera when "
    "streaming";

const char kMoreVideoCaptureBuffersName[] = "More Video Capture Buffers";
const char kMoreVideoCaptureBuffersDescription[] =
    "This flag enables using a larger amount Chrome-allocated buffers for "
    "video capture. This larger amount is needed for deeper pipelines, e.g. "
    "sophisticated camera effects.";

const char kForceControlFaceAeName[] = "Force control face AE";
const char kForceControlFaceAeDescription[] =
    "Control this flag to force enable or disable face AE for camera";

const char kCellularBypassESimInstallationConnectivityCheckName[] =
    "Bypass eSIM installation connectivity check";
const char kCellularBypassESimInstallationConnectivityCheckDescription[] =
    "Bypass the non-cellular internet connectivity check during eSIM "
    "installation.";

const char kCellularCarrierLockName[] =
    "Cellular Carrier Lock provisioning manager";
const char kCellularCarrierLockDescription[] =
    "Enable support for Carrier Lock configuration on cellular device.";

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

const char kClipboardHistoryWebContentsPasteName[] =
    "Explicitly paste into web contents from clipboard history";
const char kClipboardHistoryWebContentsPasteDescription[] =
    "Enables an experimental behavior where clipboard history explicitly "
    "pastes into web contents instead of using synthetic key events.";

const char kComponentUpdaterTestRequestName[] =
    "Enable the component updater check 'test-request' parameter";
const char kComponentUpdaterTestRequestDescription[] =
    "Enables the 'test-request' parameter for component updater check requests."
    " Overrides any other component updater check request parameters that may "
    "have been specified.";

const char kContextualNudgesName[] =
    "Contextual nudges for user gesture education";
const char kContextualNudgesDescription[] =
    "Enables contextual nudges, periodically showing the user a label "
    "explaining how to interact with a particular UI element using gestures.";

const char kCrosOnDeviceGrammarCheckName[] = "On-device Grammar Check";
const char kCrosOnDeviceGrammarCheckDescription[] =
    "Enable new on-device grammar check component.";

const char kSystemExtensionsName[] = "ChromeOS System Extensions";
const char kSystemExtensionsDescription[] =
    "Enable the ChromeOS System Extension platform.";

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

const char kCrostiniUseLxd5Name[] =
    "Use LXD 5 instead of the default - Irreversible";
const char kCrostiniUseLxd5Description[] =
    "Uses LXD version 5 instead of the default version. WARNING: Once this is "
    "set you can't unset it without deleting your entire container";

const char kBruschettaName[] = "Enable the third party VMs feature";
const char kBruschettaDescription[] =
    "Enables UI support for third party/generic VMs";

const char kBruschettaAlphaMigrateName[] = "Migration for Bruschetta Alpha";
const char kBruschettaAlphaMigrateDescription[] =
    "Enable this flag to migrate a Bruschetta installed during the alpha. "
    "Requires the bruschetta flag to be enabled.";

const char kDisableBufferBWCompressionName[] =
    "Disable buffer bandwidth compression";
const char kDisableBufferBWCompressionDescription[] =
    "Disable bandwidth compression when allocating buffers";

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

const char kDropdownPanel[] = "Enable this flag to see the dropdown panel";
const char kDropdownPanelDescription[] =
    "Show the dropdown panel to view more options";

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

const char kEnableRuntimeCountersTelemetryName[] =
    "Enable Runtime Counters Telemetry";
const char kEnableRuntimeCountersTelemetryDescription[] =
    "Allow admins to collect runtime counters telemetry (Intel Gen 14+ only).";

const char kFasterSplitScreenSetupName[] = "Enable Faster Split Screen Setup";
const char kFasterSplitScreenSetupDescription[] =
    "Enables faster split screen setup process by showing partial overview on "
    "window snapped";

const char kFastPairName[] = "Enable Fast Pair";
const char kFastPairDescription[] =
    "Enables Google Fast Pair service which uses BLE to discover supported "
    "nearby Bluetooth devices and surfaces a notification for quick pairing.";

const char kFastPairHandshakeLongTermRefactorName[] =
    "Enable Fast Pair Handshake Long Term Refactor";
const char kFastPairHandshakeLongTermRefactorDescription[] =
    "Enables long term refactored handshake logic for Google Fast Pair "
    "service.";

const char kFastPairHIDName[] = "Enable Fast Pair HID";
const char kFastPairHIDDescription[] =
    "Enables prototype support for Fast Pair HID.";

const char kFastPairLowPowerName[] = "Enable Fast Pair Low Power mode";
const char kFastPairLowPowerDescription[] =
    "Enables Fast Pair Low Power mode, which doesn't scan for devices "
    "continously. This results in lower power usage, but also higher latency "
    "for device discovery.";

const char kFastPairPwaCompanionName[] = "Enable Fast Pair Web Companion";
const char kFastPairPwaCompanionDescription[] =
    "Enables Fast Pair Web Companion link after device pairing.";

const char kFastPairSoftwareScanningName[] =
    "Enable Fast Pair Software Scanning";
const char kFastPairSoftwareScanningDescription[] =
    "Allow using Fast Pair on devices which don't support hardware offloading "
    "of BLE scans. For development use.";

const char kFastPairSavedDevicesName[] = "Enable Fast Pair Saved Devices";
const char kFastPairSavedDevicesDescription[] =
    "Enables the Fast Pair \"Saved Devices\" page to display a list of the "
    "user's devices and provide the option to opt in or out of saving devices "
    "to their account.";

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

const char kDriveFsShowCSEFilesName[] = "Enable listing of CSE files";
const char kDriveFsShowCSEFilesDescription[] =
    "Enable listing of CSE files in DriveFS, which will result in these files "
    "being visible in the Files App's Google Drive item.";

const char kEnableBackgroundBlurName[] = "Enable background blur.";
const char kEnableBackgroundBlurDescription[] =
    "Enables background blur for the Launcher, Shelf, Unified System Tray etc.";

const char kDisableDnsProxyName[] = "Disable DNS proxy service for ChromeOS";
const char kDisableDnsProxyDescription[] =
    "Turns off DNS proxying and SecureDNS for ChromeOS (only). Does not impact "
    "Chrome browser.";

const char kEnableRFC8925Name[] =
    "Enable RFC8925 (prefer IPv6-only on IPv6-only-capable network)";
const char kEnableRFC8925Description[] =
    "Let ChromeOS DHCPv4 client voluntarily drop DHCPv4 lease and prefer to"
    "operate IPv6-only, if the network is also IPv6-only capable.";

const char kPasspointARCSupportName[] = "Enable Passpoint ARC support";
const char kPasspointARCSupportDescription[] =
    "Feature to allow Android apps (running on ARC) to provision WiFi networks "
    "through Passpoint.";

const char kPasspointSettingsName[] = "Enable Passpoint settings";
const char kPasspointSettingsDescription[] =
    "Enables displaying Passpoint subscription information in network "
    "settings.";

const char kEnableEdidBasedDisplayIdsName[] = "Enable EDID-based display IDs";
const char kEnableEdidBasedDisplayIdsDescription[] =
    "When enabled, a display's ID will be produced by hashing certain values "
    "in the display's EDID blob. EDID-based display IDs allow ChromeOS to "
    "consistently identify previously connected displays, regardless of the "
    "physical port they were connected to, and load user display layouts more "
    "accurately.";

const char kEnableExternalKeyboardsInDiagnosticsAppName[] =
    "Enable external keyboards in the Diagnostics App";
const char kEnableExternalKeyboardsInDiagnosticsAppDescription[] =
    "Shows external keyboards in the Diagnostics App's input section. Requires "
    "#enable-input-in-diagnostics-app to be enabled.";

const char kEnableGetDebugdLogsInParallelName[] =
    "Enable getting debug daemon logs in parallel for feedback";
const char kEnableGetDebugdLogsInParallelDescription[] =
    "When enabled, the feedback app will use a new debug daemon method to get "
    "logs. The method collects different pieces of logs in parallel.";

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

const char kDiagnosticsAppJellyName[] =
    "Enable jelly colors for the Diagnostics App";
const char kDiagnosticsAppJellyDescription[] =
    "Enable jelly colors for the Diagnostics App. Requires "
    "jelly-colors flag to be enabled.";

const char kEnableKeyboardBacklightToggleName[] =
    "Enable Keyboard Backlight Toggle.";
const char kEnableKeyboardBacklightToggleDescription[] =
    "Enable toggling of the keyboard backlight. By "
    "default, this flag is enabled.";

const char kEnableNeuralPalmAdaptiveHoldName[] = "Palm Rejection Adaptive Hold";
const char kEnableNeuralPalmAdaptiveHoldDescription[] =
    "Enable adaptive hold in palm rejection.  Not compatible with all devices.";

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

const char kEnablePerDeskZOrderName[] =
    "Enable per-desk Z-order for all-desk windows.";
const char kEnablePerDeskZOrderDescription[] =
    "The Z-order of all-desk windows is maintained on a per-desk basis. This "
    "means that all-desk windows will not keep popping to the front when "
    "switching desks.";

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

const char kEnableZramWriteback[] = "Enable Zram Writeback";
const char kEnableZramWritebackDescription[] =
    "If enabled zram swap will be able to write back to disk increasing "
    "overall swap capacity";

const char kEnableSuspendToDisk[] = "Enable Suspend to Disk";
const char kEnableSuspendToDiskDescription[] =
    "If enabled the system will attempt to suspend to disk (hibernate) "
    "after 6 hours or more hours. This is a best effort and might fail, in "
    "which case the legacy suspend or shutdown behavior will remain";

const char kEnableSuspendToDiskAllowS4[] = "Enable Suspend to Disk: Allow S4";
const char kEnableSuspendToDiskAllowS4Description[] =
    "If enabled on systems with Keylocker hibernate will be to S4. On systems "
    "with keylocker hibernate would otherwise be unavailable. WARNING: Only "
    "enable this if you know what you are doing.";

const char kPanelSelfRefresh2Name[] = "Enable Panel Self Refresh 2";
const char kPanelSelfRefresh2Description[] =
    "Enable Panel Self Refresh 2/Selective-Update where supported. "
    "Allows the display driver to only update regions of the screen that have "
    "damage.";

const char kEnableVariableRefreshRateName[] = "Enable Variable Refresh Rate";
const char kEnableVariableRefreshRateDescription[] =
    "Enable the variable refresh rate (Adaptive Sync) setting for capable "
    "displays.";

const char kEnableVariableRefreshRateAlwaysOnName[] =
    "Enable Variable Refresh Rate Always On";
const char kEnableVariableRefreshRateAlwaysOnDescription[] =
    "Enable the variable refresh (Adaptive Sync) setting for capable displays "
    "at all times.";

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

const char kEcheNetworkConnectionStateName[] =
    "Enable loading and error states for Eche.";
const char kEcheNetworkConnectionStateDescription[] =
    "Shows loading and error states in Phone Hub's recent apps section based "
    "on the connection state with the Phone.";

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

const char kEnableNotifierCollisionName[] = "Enable notifier collision";
const char kEnableNotifierCollisionDescription[] =
    "Enable popup notifications, right-anchored tray bubbles, slider bubbles, "
    "and toasts to not overlap when displayed in the right corner of the "
    "screen";

const char kEnableOAuthIppName[] =
    "Enable OAuth when printing via the IPP protocol";
const char kEnableOAuthIppDescription[] =
    "Enable OAuth when printing via the IPP protocol";

const char kEnforceAshExtensionKeeplistName[] =
    "Enforce Ash extension keeplist";
const char kEnforceAshExtensionKeeplistDescription[] =
    "Enforce the Ash extension keeplist. Only the extensions and Chrome apps on"
    " the keeplist are enabled in Ash.";

const char kEolResetDismissedPrefsName[] =
    "Reset end of life notification prefs";
const char kEolResetDismissedPrefsDescription[] =
    "Reset the end of life notification prefs to their default value, at the "
    "start of the user session. This is meant to make manual testing easier.";

const char kEolIncentiveName[] = "Enable end of life incentives";
const char kEolIncentiveDescription[] =
    "Allows end of life incentives to be shown within the system ui.";

const char kExoConsumedByImeByFlagName[] =
    "Use the consumed bit from IME in exo";
const char kExoConsumedByImeByFlagDescription[] =
    "To see whether a key event is consumed or not, this let exo to use a bit"
    " from IME directly, instead of using heuristics based on key code etc.";

const char kExoGamepadVibrationName[] = "Gamepad Vibration for Exo Clients";
const char kExoGamepadVibrationDescription[] =
    "Allow Exo clients like Android to request vibration events for gamepads "
    "that support it.";

const char kExoOrdinalMotionName[] =
    "Raw (unaccelerated) motion for Linux applications";
const char kExoOrdinalMotionDescription[] =
    "Send unaccelerated values as raw motion events to linux applications.";

const char kExoSurroundingTextOffsetName[] =
    "Supports offset of surrounding_text in exosphere";
const char kExoSurroundingTextOffsetDescription[] =
    "On wayland protocol, surrounding text may be trimmed. Enabling this "
    "supports the cases.";

const char kExperimentalAccessibilityDictationContextCheckingName[] =
    "Experimental accessibility dictation using context checking.";
const char kExperimentalAccessibilityDictationContextCheckingDescription[] =
    "Enables experimental dictation context checking.";

const char kExperimentalAccessibilityGoogleTtsLanguagePacksName[] =
    "Experimental accessibility Google TTS Langauge Packs.";
const char kExperimentalAccessibilityGoogleTtsLanguagePacksDescription[] =
    "Enables downloading Google TTS voices using Langauge Packs.";

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

const char kFederatedServiceName[] =
    "Enable Federated Service on ChromeOS login";
const char kFederatedServiceDescription[] =
    "If disalbed, all federated service activities are stopped.";

const char kFileTransferEnterpriseConnectorName[] =
    "Enable Files Transfer Enterprise Connector.";
const char kFileTransferEnterpriseConnectorDescription[] =
    "Enable the File Transfer Enterprise Connector.";

const char kFileTransferEnterpriseConnectorUIName[] =
    "Enable UI for Files Transfer Enterprise Connector.";
const char kFileTransferEnterpriseConnectorUIDescription[] =
    "Enable the UI for the File Transfer Enterprise Connector.";

const char kFilesAppExperimentalName[] =
    "Experimental UI features for Files app";
const char kFilesAppExperimentalDescription[] =
    "Enable experimental UI features for Files app. Experimental features are "
    "expected to be non functional to end users.";

const char kFilesConflictDialogName[] = "Files app conflict dialog";
const char kFilesConflictDialogDescription[] =
    "When enabled, the conflict dialog will be shown during file transfers "
    "if a file entry in the transfer exists at the destination.";

const char kFilesDriveShortcutsName[] = "Files app Google Drive shortcut icons";
const char kFilesDriveShortcutsDescription[] =
    "When enabled, shows an icon for files in Google Drive that are shortcuts";

const char kFilesExtractArchiveName[] = "Extract archive in Files app";
const char kFilesExtractArchiveDescription[] =
    "Enable the simplified archive extraction feature in Files app";

const char kFilesInlineSyncStatusName[] =
    "Enable inline sync status in Files app.";
const char kFilesInlineSyncStatusDescription[] =
    "Enable displaying the sync status of each file next to its name in Files "
    "app.";

const char kFilesInlineSyncStatusProgressEventsName[] =
    "Enable inline sync status in Files app to work with a new source of "
    "progress events.";
const char kFilesInlineSyncStatusProgressEventsDescription[] =
    "An improvement for inline sync status that will eventually allow it to "
    "display progress for downsyncing operations.";

const char kFilesNewDirectoryTreeName[] =
    "New directory tree implementation in Files app";
const char kFilesNewDirectoryTreeDescription[] =
    "Enable the new directory tree implementation in Files app.";

const char kFilesLocalImageSearchName[] = "Search local images by query.";
const char kFilesLocalImageSearchDescription[] =
    "Enable searching local images by query.";

const char kFilesSinglePartitionFormatName[] =
    "Enable Partitioning of Removable Disks.";
const char kFilesSinglePartitionFormatDescription[] =
    "Enable partitioning of removable disks into single partition.";

const char kFilesTrashDriveName[] = "Enable Files Trash for Drive.";
const char kFilesTrashDriveDescription[] =
    "Enable trash for Drive volume in Files App.";

const char kFilesGoogleDriveSettingsPageName[] =
    "Enable Google Drive settings page";
const char kFilesGoogleDriveSettingsPageDescription[] =
    "Enable a new page for the Google Drive settings.";

const char kFirmwareUpdateJellyName[] =
    "Enable jelly colors for the Firmware Update App";
const char kFirmwareUpdateJellyDescription[] =
    "Enable jelly colors for the Firmware Update App. Requires "
    "jelly-colors flag to be enabled.";

const char kForceSpectreVariant2MitigationName[] =
    "Force Spectre variant 2 mitigagtion";
const char kForceSpectreVariant2MitigationDescription[] =
    "Forces Spectre variant 2 mitigation. Setting this to enabled will "
    "override #spectre-variant2-mitigation and any system-level setting that "
    "disables Spectre variant 2 mitigation.";

const char kFirstPartyVietnameseInputName[] =
    "First party Vietnamese Input Method";
const char kFirstPartyVietnameseInputDescription[] =
    "Use first party input method for Vietnamese VNI and Telex";

const char kFocusFollowsCursorName[] = "Focus follows cursor";
const char kFocusFollowsCursorDescription[] =
    "Enable window focusing by moving the cursor.";

const char kForceReSyncDriveName[] = "Force resync drive";
const char kForceReSyncDriveDescription[] =
    "Enable Drive to forcibly resync office files.";

const char kFrameThrottleFpsName[] = "Set frame throttling fps.";
const char kFrameThrottleFpsDescription[] =
    "Set the throttle fps for compositor frame submission.";
const char kFrameThrottleFpsDefault[] = "Default";
const char kFrameThrottleFps5[] = "5 fps";
const char kFrameThrottleFps10[] = "10 fps";
const char kFrameThrottleFps15[] = "15 fps";
const char kFrameThrottleFps20[] = "20 fps";
const char kFrameThrottleFps25[] = "25 fps";
const char kFrameThrottleFps30[] = "30 fps";

const char kFSPsInRecentsName[] =
    "Enable chrome.fileSystemProviders in Recents";
const char kFSPsInRecentsDescription[] =
    "Enable chrome.fileSystemProvider file systems in Files app Recents view";

const char kFuseBoxDebugName[] = "Debugging UI for ChromeOS FuseBox service";
const char kFuseBoxDebugDescription[] =
    "Show additional debugging UI for ChromeOS FuseBox service.";

const char kGlanceablesV2Name[] = "Glanceables";
const char kGlanceablesV2Description[] =
    "Enables Glanceables on the Calendar surface.";

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

const char kDiacriticsOnPhysicalKeyboardLongpressName[] =
    "Enable diacritics and variant character selection on PK longpress.";
const char kDiacriticsOnPhysicalKeyboardLongpressDescription[] =
    "Enable diacritics and other varient character selection on physical "
    "keyboard longpress.";

const char kDiacriticsUseReplaceSurroundingTextName[] =
    "Use ReplaceSurroundingText API for longpress diacritics.";
const char kDiacriticsUseReplaceSurroundingTextDescription[] =
    "When longpress diacritics is enabled, use the ReplaceSurroundingText API "
    "for better app compatibility.";

const char kHoldingSpacePredictabilityName[] =
    "Enable holding space predictability";
const char kHoldingSpacePredictabilityDescription[] =
    "Increases predictability of holding space by being ever present in the "
    "shelf and always reserving space for downloads and screen captures.";

const char kHoldingSpaceRefreshName[] = "Enable holding space refresh";
const char kHoldingSpaceRefreshDescription[] =
    "Enables a refresh of holding space which better conveys the relationship "
    "with the Files app.";

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

const char kImeAssistEmojiEnhancedName[] = "Enable enhanced assistive emojis";
const char kImeAssistEmojiEnhancedDescription[] =
    "Enable enhanced assistive emoji suggestion features for native IME";

const char kImeAssistMultiWordName[] =
    "Enable assistive multi word suggestions";
const char kImeAssistMultiWordDescription[] =
    "Enable assistive multi word suggestions for native IME";

const char kImeAssistMultiWordExpandedName[] =
    "Enable expanded assistive multi word suggestions";
const char kImeAssistMultiWordExpandedDescription[] =
    "Enable expanded assistive multi word suggestions for native IME";

const char kImeFstDecoderParamsUpdateName[] =
    "Enable FST Decoder parameters update";
const char kImeFstDecoderParamsUpdateDescription[] =
    "Enable updated parameters for the FST decoder.";

const char kImeTrayHideVoiceButtonName[] =
    "Hides redudant voice button in IME tray";
const char kImeTrayHideVoiceButtonDescription[] =
    "Hides voice button in IME tray when mic icon is shown in the shelf";

const char kImeKoreanModeSwitchDebugName[] =
    "Korean input method's mode switch debug";
const char kImeKoreanModeSwitchDebugDescription[] =
    "Enables debug info UI for Korean input method's internal-mode switch";

const char kIppFirstSetupForUsbPrintersName[] =
    "Try to setup USB printers with IPP first";
const char kIppFirstSetupForUsbPrintersDescription[] =
    "When enabled, ChromeOS attempts to setup USB printers via IPP Everywhere "
    "first, then falls back to PPD-based setup.";

const char kVirtualKeyboardNewHeaderName[] =
    "Enable new header for virtual keyboard";
const char kVirtualKeyboardNewHeaderDescription[] =
    "Enable new header for virtual keyboard to improve navigation.";

const char kImeSystemEmojiPickerClipboardName[] =
    "System emoji picker clipboard";
const char kImeSystemEmojiPickerClipboardDescription[] =
    "Emoji picker will insert emoji into clipboard if they can't be inserted "
    "into a text field";

const char kImeSystemEmojiPickerExtensionName[] =
    "System emoji picker extension";
const char kImeSystemEmojiPickerExtensionDescription[] =
    "Emoji picker extension allows users to select emoticons and symbols to "
    "input.";

const char kImeSystemEmojiPickerGIFSupportName[] =
    "System emoji picker gif support";
const char kImeSystemEmojiPickerGIFSupportDescription[] =
    "Emoji picker gif support allows users to select gifs to input.";

const char kImeSystemEmojiPickerJellySupportName[] =
    "Enable jelly colors for the System Emoji Picker";
const char kImeSystemEmojiPickerJellySupportDescription[] =
    "Enable jelly colors for the System Emoji Picker. Requires "
    "jelly-colors flag to be enabled.";

const char kImeSystemEmojiPickerSearchExtensionName[] =
    "System emoji picker search extension";
const char kImeSystemEmojiPickerSearchExtensionDescription[] =
    "Emoji picker search extension enhances current emoji search by "
    "introducing multi-word prefix search.";

const char kImeStylusHandwritingName[] = "Stylus Handwriting";
const char kImeStylusHandwritingDescription[] =
    "Enable VK UI for stylus in text fields";

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

const char kLacrosOnlyName[] = "Lacros is the only browser";
const char kLacrosOnlyDescription[] =
    "Use Lacros-chrome as the only web browser on ChromeOS. "
    "Please note that the first restart can take some time to setup "
    "lacros-chrome. Please DO NOT attempt to turn off the device during "
    "the restart.";

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

const char kMediaAppPdfA11yOcrName[] = "Media App PDF A11y via OCR";
const char kMediaAppPdfA11yOcrDescription[] =
    "Enable ChromeVox A11y support for PDF content in Gallery app, using OCR.";

const char kMeteredShowToggleName[] = "Show Metered Toggle";
const char kMeteredShowToggleDescription[] =
    "Shows a Metered toggle in the Network settings UI for WiFI and Cellular. "
    "The toggle allows users to set whether a network should be considered "
    "metered for purposes of bandwith usage (e.g. for automatic updates).";

const char kMicrophoneMuteNotificationsName[] = "Microphone Mute Notifications";
const char kMicrophoneMuteNotificationsDescription[] =
    "Enables notifications that are shown when an app tries to use microphone "
    "while audio input is muted.";

const char kMicrophoneMuteSwitchDeviceName[] = "Microphone Mute Switch Device";
const char kMicrophoneMuteSwitchDeviceDescription[] =
    "Support for detecting the state of hardware microphone mute toggle. Only "
    "effective on devices that have a microphone mute toggle. Enabling the "
    "flag does not affect the toggle functionality, it only affects how the "
    "System UI handles the mute toggle state.";

const char kMultiZoneRgbKeyboardName[] =
    "Enable multi-zone RGB keyboard customization";
const char kMultiZoneRgbKeyboardDescription[] =
    "Enable multi-zone RGB keyboard customization on supported devices.";

const char kMultilingualTypingName[] = "Multilingual typing on CrOS";
const char kMultilingualTypingDescription[] =
    "Enables support for multilingual assistive typing on ChromeOS.";

const char kNearbySharingSelfShareName[] = "Nearby Sharing Self Share";
const char kNearbySharingSelfShareDescription[] =
    "Enables Self Share auto-accept and UI features to allow seamless sharing "
    "between a user's own devices.";

const char kOobeHidDetectionRevampName[] = "OOBE HID Detection Revamp";
const char kOobeHidDetectionRevampDescription[] =
    "Enables the ChromeOS HID Detection Revamp, which updates OOBE HID "
    "detection screen UI and related infrastructure.";

const char kOrcaKeyName[] = "Secret key for Orca feature";
const char kOrcaKeyDescription[] =
    "Secret key for Orca feature. Incorrect values will cause chrome crashes.";

const char kOsFeedbackDialogName[] =
    "OS Feedback dialog on OOBE and login screen";
const char kOsFeedbackDialogDescription[] =
    "Enable the OS Feedback dialog on OOBE and login screen.";

const char kOsFeedbackJellyName[] =
    "Enable jelly colors for the OS Feedback app";
const char kOsFeedbackJellyDescription[] =
    "Enable jelly colors for the OS Feedback app. Requires jelly-colors flag "
    "to be enabled.";

const char kOsSettingsAppBadgingToggleName[] =
    "ChromeOS Settings App Badging Toggle";
const char kOsSettingsAppBadgingToggleDescription[] =
    "Enables app badging toggle to be displayed in app notification page in"
    "ChromeOS Settings.";

const char kOsSettingsDeprecateSyncMetricsToggleName[] =
    "ChromeOS Settings Deprecate Sync Metrics Toggle";
const char kOsSettingsDeprecateSyncMetricsToggleDescription[] =
    "If enabled, deprecate the metrics in sync settings page in "
    "ChromeOS Settings.";

const char kOsSettingsTestChromeRefreshName[] =
    "ChromeOS Settings Test Chrome Refresh Components";
const char kOsSettingsTestChromeRefreshDescription[] =
    "If enabled, uses new Chrome Refresh web components in "
    "ChromeOS Settings.";

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

const char kPhoneHubOnboardingNotifierRevampName[] =
    "Phone Hub onboarding notifier revamp";
const char kPhoneHubOnboardingNotifierRevampDescription[] =
    "Enables the revamp for Phone Hub onboarding notifier when eligible.";

const char kPolicyProvidedTrustAnchorsAllowedAtLockScreenName[] =
    "Policy-provided trust anchors at lock screen";
const char kPolicyProvidedTrustAnchorsAllowedAtLockScreenDescription[] =
    "Enables using the policy-provided trust anchors at lock screen";

const char kPreferDcheckName[] = "Prefer DCHECK-enabled build";
const char kPreferDcheckDescription[] =
    "Use a DCHECK-enabled build when available.";

const char kPrinterSettingsPrinterStatusName[] =
    "Enable Printer Settings printer statuses";
const char kPrinterSettingsPrinterStatusDescription[] =
    "Enables printer status querying and displaying from the OS Printer "
    "settings page.";

const char kPrinterSettingsRevampName[] = "Enable Printer Settings Revamped UI";
const char kPrinterSettingsRevampDescription[] =
    "Show the enhanced UI for the OS Printer settings page.";

const char kPrintPreviewDiscoveredPrintersName[] =
    "Enables showing discovered printers in the Print Preview dialog.";
const char kPrintPreviewDiscoveredPrintersDescription[] =
    "Shows discovered printers in the Print Preview dialog that get set up "
    "once selected.";

const char kPrintingPpdChannelName[] = "Printing PPD channel";
const char kPrintingPpdChannelDescription[] =
    "The channel from which PPD index "
    "is loaded when matching PPD files during printer setup.";

const char kPrintManagementJellyName[] =
    "Enable jelly colors for the Print Management App";
const char kPrintManagementJellyDescription[] =
    "Enable jelly colors for the Print Management App. Requires "
    "jelly-colors flag to be enabled.";

const char kPrintManagementSetupAssistanceName[] =
    "Enable improved printer setup experience for the Print Management App";
const char kPrintManagementSetupAssistanceDescription[] =
    "Enable improved printer setup experience for the Print Management App.";

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

const char kProjectorServerSideSpeechRecognitionName[] =
    "Enable server side speech recognition for Projector";
const char kProjectorServerSideSpeechRecognitionDescription[] =
    "Adds server side speech recognition capability to Projector.";

const char kReleaseNotesNotificationAllChannelsName[] =
    "Release Notes Notification All Channels";
const char kReleaseNotesNotificationAllChannelsDescription[] =
    "Enables the release notes notification for all ChromeOS channels";

const char kRenderArcNotificationsByChromeName[] =
    "Render ARC notifications by ChromeOS";
const char kRenderArcNotificationsByChromeDescription[] =
    "Enables rendering ARC notifications using ChromeOS notification framework "
    "if supported";

const char kArcWindowPredictorName[] = "Enable ARC window predictor";
const char kArcWindowPredictorDescription[] =
    "Enables the window state and bounds predictor for ARC task windows";

const char kArcInputOverlayNameBeta[] = "Enable ARC Input Overlay Beta";
const char kArcInputOverlayDescriptionBeta[] =
    "Enable full editor feature for Gaming Input Overlay including features in "
    "Alpha V2, so users can add and remove actions.";

const char kArcInputOverlayNameAlphaV2[] = "Enable ARC Input Overlay Alpha V2";
const char kArcInputOverlayDescriptionAlphaV2[] =
    "Enable menu and action reposition feature for Gaming Input Overlay based "
    "on Alpha.";

const char kScalableIphDebugName[] = "Scalable Iph Debug";
const char kScalableIphDebugDescription[] =
    "Enables debug feature of Scalable Iph";

const char kScanningAppJellyName[] =
    "Enable jelly colors for the Scanning App.";
const char kScanningAppJellyDescription[] =
    "Enable jelly colors for the Scanning App. Requires "
    "jelly-colors flag to be enabled.";

const char kShelfAutoHideSeparationName[] =
    "Enable separate shelf auto-hide preferences.";
const char kShelfAutoHideSeparationDescription[] =
    "Allows for the shelf's auto-hide preference to be specified separately "
    "for clamshell and tablet mode.";

const char kShimlessRMAOsUpdateName[] = "Enable OS updates in shimless RMA";
const char kShimlessRMAOsUpdateDescription[] =
    "Turns on OS updating in Shimless RMA";

const char kShimlessRMAComplianceCheckName[] =
    "Enable compliance check in Shimless RMA";
const char kShimlessRMAComplianceCheckDescription[] =
    "Enable device compliance check in the Shimless RMA flow";

const char kShimlessRMASkuDescriptionName[] =
    "Enable SKU description in Shimless RMA";
const char kShimlessRMASkuDescriptionDescription[] =
    "Enable device SKU description in the Shimless RMA flow";

const char kShortcutCustomizationJellyName[] =
    "Enable jelly colors for the Shortcut Customization App";
const char kShortcutCustomizationJellyDescription[] =
    "Enable jelly colors for the Shortcut Customization App. Requires "
    "jelly-colors flag to be enabled.";

const char kSchedulerConfigurationName[] = "Scheduler Configuration";
const char kSchedulerConfigurationDescription[] =
    "Instructs the OS to use a specific scheduler configuration setting.";
const char kSchedulerConfigurationConservative[] =
    "Disables Hyper-Threading on relevant CPUs.";
const char kSchedulerConfigurationPerformance[] =
    "Enables Hyper-Threading on relevant CPUs.";

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

const char kBluetoothSessionizedMetricsName[] =
    "Enable Bluetooth sessionized metrics";
const char kBluetoothSessionizedMetricsDescription[] =
    "Enables collecting and processing Bluetooth sessionized metrics.";

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

const char kSpeakOnMuteOptInNudgePrefsResetName[] =
    "Reset Speak-on-mute detection opt-in nudge prefs";
const char kSpeakOnMuteOptInNudgePrefsResetDescription[] =
    "Resets the prefs that prevent the speak-on-mute opt-in nudge from "
    "showing, so it can be shown again for debugging purposes. With this flag "
    "enabled, the speak-on-mute nudge will show after every login.";

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

const char kSystemLiveCaptionName[] = "System Live Caption";
const char kSystemLiveCaptionDescription[] =
    "Enables the live caption feature for non-Chrome (e.g. Android, linux) "
    "audio.";

const char kSystemNudgeV2Name[] = "New System Nudges";
const char kSystemNudgeV2Description[] =
    "Enables the use of the new System Nudges";

const char kSupportF11AndF12ShortcutsName[] = "F11/F12 Shortcuts";
const char kSupportF11AndF12ShortcutsDescription[] =
    "Enables settings that "
    "allow users to use shortcuts to remap to the F11 and F12 keys in the "
    "Customize keyboard keys "
    "page.";

const char kTerminalAlternativeEmulatorName[] = "Terminal alternative emulator";
const char kTerminalAlternativeEmulatorDescription[] =
    "Enable the alternative emulator for the Terminal app. You will also get "
    "an option in Terminal settings to change the default emulator.";

const char kTerminalDevName[] = "Terminal dev";
const char kTerminalDevDescription[] =
    "Enables Terminal System App to load from Downloads for developer testing. "
    "Only works in dev and canary channels.";

const char kTetherName[] = "Instant Tethering";
const char kTetherDescription[] =
    "Enables Instant Tethering. Instant Tethering allows your nearby Google "
    "phone to share its Internet connection with this device.";

const char kTimeOfDayScreenSaverName[] = "Time of Day Screen Saver";
const char kTimeOfDayScreenSaverDescription[] =
    "Enables Time of Day Screen Saver feature on supported devices. Requires "
    "Time Of Day Wallpaper feature to be enabled.";

const char kTimeOfDayWallpaperName[] = "Time of Day Wallpaper";
const char kTimeOfDayWallpaperDescription[] =
    "Enables Time of Day Wallpaper feature on supported devices.";

const char kTimeOfDayDlcName[] = "Time of Day Dlc";
const char kTimeOfDayDlcDescription[] =
    "Enables downloading Time of Day Screen Saver assets from DLC rather than "
    "using ones built into rootfs. This should have little to no user-visible "
    "impact. Requires Time of Day Screen Saver to be enabled.";

const char kTouchscreenCalibrationName[] =
    "Enable/disable touchscreen calibration option in material design settings";
const char kTouchscreenCalibrationDescription[] =
    "If enabled, the user can calibrate the touch screen displays in "
    "chrome://settings/display.";

const char kTrafficCountersEnabledName[] = "Traffic counters enabled";
const char kTrafficCountersEnabledDescription[] =
    "If enabled, data usage will be visible in the Cellular Settings UI and "
    "traffic counters will be automatically reset if that setting is enabled.";

const char kUploadOfficeToCloudName[] = "Enable Office files upload workflow.";
const char kUploadOfficeToCloudDescription[] =
    "Some file handlers for Microsoft Office files are only available on the "
    "the cloud. Enables the cloud upload workflow for Office file handling.";

const char kUpstreamTrustedReportsFirmwareName[] =
    "Enable firmware updates from upstream";
const char kUpstreamTrustedReportsFirmwareDescription[] =
    "Enables firmware updates for firmwares that have been uploaded to LVFS "
    "and have been tested with ChromeOS.";

const char kUseFakeDeviceForMediaStreamName[] = "Use fake video capture device";
const char kUseFakeDeviceForMediaStreamDescription[] =
    "Forces Chrome to use a fake video capture device (a rolling pacman with a "
    "timestamp) instead of the system audio/video devices, for debugging "
    "purposes.";

const char kUiDevToolsName[] = "Enable native UI inspection";
const char kUiDevToolsDescription[] =
    "Enables inspection of native UI elements. For local inspection use "
    "chrome://inspect#other";

const char kUiSlowAnimationsName[] = "Slow UI animations";
const char kUiSlowAnimationsDescription[] = "Makes all UI animations slow.";

const char kVirtualKeyboardName[] = "Virtual Keyboard";
const char kVirtualKeyboardDescription[] =
    "Always show virtual keyboard regardless of having a physical keyboard "
    "present";

const char kVirtualKeyboardDisabledName[] = "Disable Virtual Keyboard";
const char kVirtualKeyboardDisabledDescription[] =
    "Always disable virtual keyboard regardless of device mode. Workaround for "
    "virtual keyboard showing with some external keyboards.";

const char kVirtualKeyboardRoundCornersName[] =
    "Virtual Keyboard Round Corners";
const char kVirtualKeyboardRoundCornersDescription[] =
    "Enables round corners on the virtual keyboard.";

const char kVmMemoryManagementServiceName[] = "VM Memory Management Service";
const char kVmMemoryManagementServiceDescription[] =
    "Enables the VM Memory Management Service.";

const char kWakeOnWifiAllowedName[] = "Allow enabling wake on WiFi features";
const char kWakeOnWifiAllowedDescription[] =
    "Allows wake on WiFi features in shill to be enabled.";

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

const char kWindowSplittingName[] = "CrOS Labs - Window splitting";
const char kWindowSplittingDescription[] =
    "Enables splitting windows by dragging one over another.";

const char kLauncherGameSearchName[] = "Enable launcher game search";
const char kLauncherGameSearchDescription[] =
    "Enables cloud game search results in the launcher.";

const char kLauncherKeywordExtractionScoring[] =
    "Query keyword extraction and scoring in launcher";
const char kLauncherKeywordExtractionScoringDescription[] =
    "Enables extraction of keywords from query then calculate score from "
    "extracted keyword in the launcher.";

const char kLauncherLocalImageSearchName[] =
    "Enable launcher local image search";
const char kLauncherLocalImageSearchDescription[] =
    "Enables on-device local image search in the launcher.";

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

const char kLauncherFuzzyMatchAcrossProvidersName[] =
    "Enable fuzzy match for relevance scores";
const char kLauncherFuzzyMatchAcrossProvidersDescription[] =
    "Change relevance score in Drive Files, Local Files, Help App, Keyboard "
    "shortcuts, OS Settings and personalization app to all be based on a fuzzy "
    "match";

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

const char kSmdsSupportName[] = "SM-DS Support";
const char kSmdsSupportDescription[] =
    "Feature to enable the consumer and enterprise support for provisioning "
    "eSIM profiles using Subscription Manager Discovery Service (SM-DS). This "
    "flag is a no-op unless the smds-dbus-migration and "
    "smds-support-euicc-upload flags are enabled.";

const char kSmdsSupportEuiccUploadName[] = "SM-DS Support EUICC Upload";
const char kSmdsSupportEuiccUploadDescription[] =
    "Feature to enable tracking when a policy-defined cellular network "
    "configured to use SM-DS has already been applied and an eSIM profile for "
    "the network was installed.";

const char kSmdsDbusMigrationName[] = "SM-DS DBus Migration";
const char kSmdsDbusMigrationDescription[] =
    "Feature to enable the usage of DBus APIs that improve the stability"
    "around performing SM-DS scans.";

const char kOobeJellyName[] = "Jelly design for OOBE";
const char kOobeJellyDescription[] =
    "Feature to enable the Jelly design in out of box experience.";

const char kOobeJellyModalName[] = "Jelly modal feature for OOBE";
const char kOobeJellyModalDescription[] =
    "Feature to enable the Jelly modal feature in out of box experience.";

const char kOobeSimonName[] = "Simon features for OOBE";
const char kOobeSimonDescription[] =
    "Feature to enable the Simon features in out of box experience.";

const char kLacrosSharedComponentsDirName[] =
    "Place browser components in a shared location";
const char kLacrosSharedComponentsDirDescription[] =
    "When enabled, it causes Lacros to use a location shared across users for "
    "browser components.";

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

const char kLacrosMergeIcuDataFileName[] =
    "Enable merging of icudtl.dat in Lacros";
const char kLacrosMergeIcuDataFileDescription[] =
    "Enables sharing common areas of icudtl.dat between Ash and Lacros.";
#endif  // #if BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
const char kGetAllScreensMediaName[] = "GetAllScreensMedia API";
const char kGetAllScreensMediaDescription[] =
    "When enabled, the getAllScreensMedia API for capturing multiple screens "
    "at once, is available.";
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_CHROMEOS)

const char kAppToAppLinkCapturingName[] = "App-to-app link capturing";
const char kAppToAppLinkCapturingDescription[] =
    "Enables link capturing from one app to another, even when the target app "
    "does not have link capturing enabled in settings";

const char kAppToAppLinkCapturingWorkspaceAppsName[] =
    "App-to-app link capturing for Workspace apps";
const char kAppToAppLinkCapturingWorkspaceAppsDescription[] =
    "Enables link capturing from one app to another, even when the target app "
    "does not have link capturing enabled in settings. Only applies if the "
    "target app is a Workspace app (Google Drive/Docs/Sheets/Slides).";

const char kCrosAppsBackgroundEventHandlingName[] =
    "Experimental Background Events for CrOS Apps";
const char kCrosAppsBackgroundEventHandlingDescription[] =
    "Enable key events for CrOS Apps running in background.";

const char kCrosWebAppInstallDialogName[] = "Web app install dialog";
const char kCrosWebAppInstallDialogDescription[] =
    "Enables a more detailed, OS-level dialog for web app installs";

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

const char kCrosPrivacyHubName[] = "Enable ChromeOS Privacy Hub";
const char kCrosPrivacyHubDescription[] = "Enables ChromeOS Privacy Hub.";

const char kCrosPrivacyHubAppPermissionsName[] =
    "Enable app permissions view inside Priacy Hub";
const char kCrosPrivacyHubAppPermissionsDescription[] =
    "When enabled, the user will be able to see the list of apps, sites and "
    "system services affected by the privacy hub toggles.";

const char kCrosPrivacyHubV0Name[] =
    "Enable ChromeOS Privacy Hub without the location switch.";
const char kCrosPrivacyHubV0Description[] =
    "Enables ChromeOS Privacy Hub without the location switch.";

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

const char kEnableBorderlessPrintingName[] = "Borderless printing";
const char kEnableBorderlessPrintingDescription[] =
    "Enable borderless printing and paper type selection in the print preview "
    "dialog.";

const char kKioskEnableAppServiceName[] = "Enable App Service in Kiosk.";
const char kKioskEnableAppServiceDescription[] =
    "Uses App Service to install web apps and launch both Chrome apps and web "
    "apps in Kiosk sessions.";

const char kLacrosColorManagementName[] = "Enable Chrome Color Management.";
const char kLacrosColorManagementDescription[] =
    "Uses chrome-color-management wayland protocol to manage color spaces "
    "for lacros. This is necessary for enabling HDR on compatible devices.";

const char kOneGroupPerRendererName[] =
    "Use one cgroup for each foreground renderer";
const char kOneGroupPerRendererDescription[] =
    "Places each Chrome foreground renderer into its own cgroup";

const char kPreinstalledWebAppWindowExperimentName[] =
    "Preinstalled web app window experiment.";
const char kPreinstalledWebAppWindowExperimentDescription[] =
    "A ChromeOS experiment for new users that makes all preinstalled web apps "
    "open in windows with link capturing enabled, or tabs, instead of the "
    "default behavior.";

const char kPrintPreviewSetupAssistanceName[] =
    "Enable improved printer status and error messaging in Print Preview.";
const char kPrintPreviewSetupAssistanceDescription[] =
    "Enable improved printer status and error messaging in Print Preview.";

const char kLocalPrinterObservingName[] = "Enable Local Printer Observing";
const char kLocalPrinterObservingDescription[] =
    "Allows Print Preview and Printer settings to receive live updates from "
    "local printers.";

const char kDisableQuickAnswersV2TranslationName[] =
    "Disable Quick Answers Translation";
const char kDisableQuickAnswersV2TranslationDescription[] =
    "Disable translation services of the Quick Answers.";

const char kQuickAnswersRichCardName[] = "Enable Quick Answers Rich Card";
const char kQuickAnswersRichCardDescription[] =
    "Enable rich card views of the Quick Answers feature.";

const char kSyncChromeOSExplicitPassphraseSharingName[] =
    "Sync passphrase sharing";
const char kSyncChromeOSExplicitPassphraseSharingDescription[] =
    "Allows sharing custom sync passphrase between OS and Browser on ChromeOS";

const char kQuickOfficeForceFileDownloadName[] =
    "Basic Office Editor File Download";
const char kQuickOfficeForceFileDownloadDescription[] =
    "Forces the Basic Office Editor to download files instead of intercepting "
    "navigations to document types it can handle.";
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
#if !BUILDFLAG(USE_VAAPI)
const char kChromeOSDirectVideoDecoderName[] = "ChromeOS Direct Video Decoder";
const char kChromeOSDirectVideoDecoderDescription[] =
    "Enables the hardware-accelerated ChromeOS direct media::VideoDecoder "
    "implementation. Note that this might be entirely disallowed by the "
    "--platform-disallows-chromeos-direct-video-decoder command line switch "
    "which is added for platforms where said direct VideoDecoder does not work "
    "or is not well tested (see the disable_cros_video_decoder USE flag in "
    "ChromeOS). This flag is supported only on non-Intel and non-AMD devices.";
#endif  // !BUILDFLAG(USE_VAAPI)
const char kChromeOSHWVBREncodingName[] =
    "ChromeOS Hardware Variable Bitrate Encoding";
const char kChromeOSHWVBREncodingDescription[] =
    "Enables the hardware-accelerated variable bitrate (VBR) encoding on "
    "ChromeOS. If the hardware encoder supports VBR for a specified codec, a "
    "video is recorded in VBR encoding in MediaRecoder API automatically and "
    "WebCodecs API if configured so.";
#if defined(ARCH_CPU_ARM_FAMILY)
const char kPreferGLImageProcessorName[] = "Prefer GL image processor";
const char kPreferGLImageProcessorDescription[] =
    "Prefers the GL image processor for format conversion of video frames over"
    " both the libYUV and hardware implementations";
const char kPreferSoftwareMT21Name[] = "Prefer software MT21 conversion";
const char kPreferSoftwareMT21Description[] =
    "Prefer using the software MT21 conversion instead of the MDP hardware "
    "conversion on MT8173 devices.";
#endif  // defined(ARCH_CPU_ARM_FAMILY)
#endif  // BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
const char kZeroCopyVideoCaptureName[] = "Enable Zero-Copy Video Capture";
const char kZeroCopyVideoCaptureDescription[] =
    "Camera produces a gpu friendly buffer on capture and, if there is, "
    "hardware accelerated video encoder consumes the buffer";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_FUCHSIA)
const char kQuickCommandsName[] = "Quick Commands";
const char kQuickCommandsDescription[] =
    "Enable a text interface to browser features. Invoke with Ctrl-Space.";

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_CHROMEOS)
const char kFollowingFeedSidepanelName[] = "Following feed in the sidepanel";
const char kFollowingFeedSidepanelDescription[] =
    "Enables the following feed in the sidepanel.";
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
const char kEnableProtoApiForClassifyUrlName[] =
    "Enable Proto API for Classify URL";
const char kEnableProtoApiForClassifyUrlDescription[] =
    "Calls to Classify URL RPC will use Protocol Buffer format in resposnes, "
    "instead of JSON.";
#endif

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

#if BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS)
const char kOzonePlatformHintChoiceDefault[] = "Default";
const char kOzonePlatformHintChoiceAuto[] = "Auto";
const char kOzonePlatformHintChoiceX11[] = "X11";
const char kOzonePlatformHintChoiceWayland[] = "Wayland";

const char kOzonePlatformHintName[] = "Preferred Ozone platform";
const char kOzonePlatformHintDescription[] =
    "Selects the preferred platform backend used on Linux. The default one is "
    "\"X11\". \"Auto\" selects Wayland if possible, X11 otherwise. ";
#endif  // BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS)

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
const char kForcePasswordInitialSyncWhenDecryptionFailsName[] =
    "Force initial sync to clean local undecryptable passwords during startup";
const char kForcePasswordInitialSyncWhenDecryptionFailsDescription[] =
    "During startup checks if there are undecryptable passwords in the local "
    "storage and requests initial sync.";
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
const char kAsyncDnsName[] = "Async DNS resolver";
const char kAsyncDnsDescription[] = "Enables the built-in DNS resolver.";
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)

// Feature flags --------------------------------------------------------------

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

#if BUILDFLAG(ENABLE_CARDBOARD)
const char kEnableCardboardName[] = "Enable Cardboard VR WebXR Runtime";
const char kEnableCardboardDescription[] =
    "Enables the use of the Cardboard SDK runtime for WebXR instead of the"
    "Google VR Services (or GVR) runtime to start a WebXR-based immersive-vr"
    "session.";
#endif  // ENABLE_CARDBOARD

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

#if BUILDFLAG(ENABLE_VR)
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

#if BUILDFLAG(IS_WIN) ||                                      \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) || \
    BUILDFLAG(IS_MAC) || BUILDFLAG(IS_FUCHSIA)
const char kUIDebugToolsName[] = "Debugging tools for UI";
const char kUIDebugToolsDescription[] =
    "Enables additional keyboard shortcuts to help debugging.";

const char kSyncPollImmediatelyOnEveryStartupName[] =
    "Sync Poll Immediately On Every Startup";
const char kSyncPollImmediatelyOnEveryStartupDescription[] =
    "Sends a poll GetUpdates request on every browser startup.";
#endif

#if defined(WEBRTC_USE_PIPEWIRE)
const char kWebrtcPipeWireCapturerName[] = "WebRTC PipeWire support";
const char kWebrtcPipeWireCapturerDescription[] =
    "When enabled the WebRTC will use the PipeWire multimedia server for "
    "capturing the desktop content on the Wayland display server.";
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

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
const char kThirdPartyProfileManagementName[] =
    "Third party profile management";
const char kThirdPartyProfileManagementDescription[] =
    "Enables profile management triggered by third-party sign-ins.";

const char kUnoDesktopName[] = "UNO Desktop";
const char kUnoDesktopDescription[] =
    "Enables the UNO model on Desktop. This is currently an experiment in a "
    "prototype stage in order to validate the model.";

const char kDesktopPWAsUserLinkCapturingName[] = "Desktop PWA Link Capturing";
const char kDesktopPWAsUserLinkCapturingDescription[] =
    "Enables opening links from Chrome in an installed PWA";

const char kAttachLogsToAutofillRaterExtensionReportName[] =
    "Attach logs to Autofill Rater Extension Report";
const char kAttachLogsToAutofillRaterExtensionReportDescription[] =
    "When a report is started via the Autofill Rater Extension, the logs which "
    "are normally recorded in chrome://password-manager-internals and "
    "chrome://autofill-internals will be recorded on files on the disk. At the "
    "end of the report, these files will be attached to the report.";

const char kFillMultiLineName[] = "Multi Line Fill";
const char kFillMultiLineDescription[] = "Enables multi line fill feature";
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

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
const char kEnableBoundSessionCredentialsName[] =
    "Device Bound Session Credentials";
const char kEnableBoundSessionCredentialsDescription[] =
    "Enables Google session credentials binding to cryptographic keys that are "
    "practically impossible to extract from the user device. This will mostly "
    "prevent the usage of bound credentials outside of the user device.";
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

#if BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
const char kTheoraVideoCodecName[] = "Theora video codec support";
const char kTheoraVideoCodecDescription[] =
    "Controls support for the Theora video codec.";
#endif

// ============================================================================
// Don't just add flags to the end, put them in the right section in
// alphabetical order just like the header file.
// ============================================================================

}  // namespace flag_descriptions
