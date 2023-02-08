// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/flag_descriptions.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "flag_descriptions.h"
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

const char kPdfUseSkiaRendererName[] = "Use Skia Renderer";
const char kPdfUseSkiaRendererDescription[] = "Use Skia as the PDF renderer.";
#endif

extern const char kAddEduAccountFromAccountSettingsForSupervisedUsersName[] =
    "Add Edu Account From Account Settings For Supervised Users";
extern const char
    kAddEduAccountFromAccountSettingsForSupervisedUsersDescription[] =
        "Enables supervised users to add additional Edu accounts and "
        "simplifies settings UI";

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

const char kWindowsFollowCursorName[] =
    "Windows open on the display with the cursor";
const char kWindowsFollowCursorDescription[] =
    "When there are multiple displays, windows open on the display where "
    "cursor is located.";

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

const char kBlockInsecureDownloadsName[] = "Block insecure downloads";
const char kBlockInsecureDownloadsDescription[] =
    "Enables insecure download blocking. This shows a 'blocked' message if the "
    "user attempts to download a file over an insecure transport (e.g. HTTP) "
    "either directly or via an insecure redirect.";

const char kBrokerFileOperationsOnDiskCacheInNetworkServiceName[] =
    "Broker file operations on disk cache in the Network Service";
const char kBrokerFileOperationsOnDiskCacheInNetworkServiceDescription[] =
    "Broker file operations on disk cache running in the Network Service. This "
    "is no-op when the Network Service is running in the browser process.";

const char kCertDualVerificationEnabledName[] = "Cert Dual Verification";
const char kCertDualVerificationEnabledDescription[] =
    "Runs both platform Cert Verifer and Chrome Cert Verifier, reporting "
    "any differences to Chrome if Safe Browsing Extended Reporting is "
    "enabled.";

const char kClipboardUnsanitizedContentName[] =
    "Clipboard unsanitized read and write";
const char kClipboardUnsanitizedContentDescription[] =
    "Allows reading/writing unsanitized content from/to the clipboard. "
    "Currently, it is only applicable to HTML format. See crbug.com/1268679.";

const char kChromeRootStoreEnabledName[] = "Chrome Root Store";
const char kChromeRootStoreEnabledDescription[] =
    "Enable use of Chrome Root Store over platform roots. "
    "On some platforms this may also enable Chrome Cert Verifier.";

const char kContentLanguagesInLanguagePickerName[] =
    "Content languages in language picker";
const char kContentLanguagesInLanguagePickerDescription[] =
    "Enables bringing user's content languages that are translatable to the "
    "top of the list with all languages shown in the translate menu";

const char kCustomizeChromeColorExtractionName[] =
    "Customize Chrome Color Extraction";
const char kCustomizeChromeColorExtractionDescription[] =
    "Enables setting theme color based on background image color when "
    "background image is changed in New Tab Page Customize Chrome.";

const char kCustomizeChromeSidePanelName[] = "Customize Chrome Side Panel";
const char KCustomizeChromeSidePanelDescription[] =
    "Enables the ability to use Customize Chrome functionality from the "
    "unified side panel on the New Tab Page.";

const char kForceStartupSigninPromoName[] = "Force Start-up Signin Promo";
const char kForceStartupSigninPromoDescription[] =
    "If enabled, the full screen signin promo will be forced to show up at "
    "Chrome start-up.";

const char kGaiaIdCacheInAccountManagerFacadeName[] =
    "Gaia Id In AccountManagerFacade (AMF)";
const char kGaiaIdCacheInAccountManagerFacadeDescription[] =
    "If enabled, starts fetching gaia id from android accounts in "
    "AccountManagerFacade (AMF)";

const char kIdentityStatusConsistencyName[] = "Identity Status Consistency";
const char kIdentityStatusConsistencyDescription[] =
    "If enabled, always show identity status - even for signed-out users";

const char kTangibleSyncName[] = "Tangible Sync";
const char kTangibleSyncDescription[] =
    "Enables the tangible sync when a user starts the sync consent flow";

const char kDebugHistoryInterventionNoUserActivationName[] =
    "Debug flag for history intervention on no user activation";
const char kDebugHistoryInterventionNoUserActivationDescription[] =
    "This flag when enabled, will be used to debug an issue where a page that "
    "did not get user activation "
    "is able to work around the history intervention which is not the expected "
    "behavior";

extern const char kDefaultChromeAppsMigrationName[] =
    "Default Chrome apps policy migration";
extern const char kDefaultChromeAppsMigrationDescription[] =
    "Enable replacing policies to force install Chrome apps with policies to "
    "force install PWAs";

const char kDeferredFontShapingName[] = "Deferred Font Shaping";
const char kDeferredFontShapingDescription[] =
    "Defer text rendering in invisible CSS boxes until the boxes become "
    "visible.";

const char kDocumentPictureInPictureApiName[] =
    "Document Picture-in-Picture API";
const char kDocumentPictureInPictureApiDescription[] =
    "Enables API to open an always-on-top window with a full HTML document";

const char kPasswordNotesWithBackupName[] = "Password notes in settings";
const char kPasswordNotesWithBackupDescription[] =
    "Enables a note section for each password in the settings page.";

const char kPasswordViewPageInSettingsName[] = "Password view page in settings";
const char kPasswordViewPageInSettingsDescription[] =
    "Enables a new password details subpage in the settings password "
    "management UI.";

extern const char kPPAPISharedImagesSwapChainName[] =
    "Use SharedImages for PPAPI swapchain";
extern const char kPPAPISharedImagesSwapChainDescription[] =
    "Switches legacy swap chain code to SharedImages";

extern const char kSupportPepperVideoDecoderDevAPIName[] =
    "Controls whether the Pepper PPB_VideoDecoder(Dev) API is supported";
extern const char kSupportPepperVideoDecoderDevAPIDescription[] =
    "When disabled, Chrome will return 0 if asked to create the "
    "PPB_VideoDecoder(Dev) API by in-process callers";

const char kPrivacyIndicatorsName[] = "Enable Privacy Indicators";
const char kPrivacyIndicatorsDescription[] =
    "While screen sharing or camera/microphone is being accessed, show a green "
    "icon in the status area as well as add a silent notification to the tray.";

const char kEnableBluetoothSerialPortProfileInSerialApiName[] =
    "Enable Bluetooth Serial Port Profile in Serial API";
const char kEnableBluetoothSerialPortProfileInSerialApiDescription[] =
    "When enabled, Bluetooth Serial Port Profile devices will be enumerated "
    "for use with the Serial API.";

const char kEnableDrDcName[] =
    "Enables Display Compositor to use a new gpu thread.";
const char kEnableDrDcDescription[] =
    "When enabled, chrome uses 2 gpu threads instead of 1. "
    " Display compositor uses new dr-dc gpu thread and all other clients "
    "(raster, webgl, video) "
    " continues using the gpu main thread.";

const char kEnableDrDcVulkanName[] =
    " Use this flag along with flag enable-drdc to enable DrDc on Vulkan. "
    " Note that this flag will be a no-op if enable-drdc is disabled. ";

const char kForceGpuMainThreadToNormalPriorityDrDcName[] =
    "Force GPU main thread priority to normal for DrDc.";
const char kForceGpuMainThreadToNormalPriorityDrDcDescription[] =
    "When enabled, force GPU main thread priority to be normal for DrDc mode. "
    "In that case DrDc thread continues to use DISPLAY thread priority and "
    "hence have higher thread priority than GPU main. Note that this flag will "
    "be a no-op when DrDc is disabled.";

const char kTextBasedAudioDescriptionName[] = "Enable audio descriptions.";
const char kTextBasedAudioDescriptionDescription[] =
    "When enabled, HTML5 video elements with a 'descriptions' WebVTT track "
    "will speak the audio descriptions aloud as the video plays.";

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
const char kWebFilterInterstitialRefreshName[] =
    "Web filter interstitial refresh.";
const char kWebFilterInterstitialRefreshDescription[] =
    "Enable web filter interstitial refresh for Family Link users.";
#endif  // ENABLE_SUPERVISED_USERS

const char kUpcomingSharingFeaturesName[] = "Enable upcoming sharing features.";
const char kUpcomingSharingFeaturesDescription[] =
    "This flag enables all upcoming sharing features, in the experiment "
    "arms that are most likely to be shipped. This is a meta-flag so which "
    "features are upcoming at any given time may change.";

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

const char kUseMojoVideoDecoderForPepperName[] =
    "Use the MojoVideoDecoder for hardware video decoding in Pepper";
const char kUseMojoVideoDecoderForPepperDescription[] =
    "Switches Pepper to use the MojoVideoDecoder for hardware accelerated "
    "video decoding instead of the legacy video decoder.";

const char kUseDMSAAForTilesName[] = "Use DMSAA for tiles";
const char kUseDMSAAForTilesDescription[] =
    "Switches skia to use DMSAA instead of MSAA for tile raster";

const char kUseDnsHttpsSvcbAlpnName[] = "Use DNS https alpn";
const char kUseDnsHttpsSvcbAlpnDescription[] =
    "When enabled, Chrome may try QUIC on the first connection using the ALPN"
    " information in the DNS HTTPS record.";

const char kEnableFirstPartySetsName[] = "Enable First-Party Sets";
const char kEnableFirstPartySetsDescription[] =
    "When enabled, Chrome will apply First-Party Sets to features such as the "
    "SameParty cookie attribute.";

extern const char kEncryptedClientHelloName[] = "Encrypted ClientHello";
extern const char kEncryptedClientHelloDescription[] =
    "When enabled, Chrome will enable Encrypted ClientHello support. This will "
    "encrypt TLS ClientHello if the server enables the extension via the HTTPS "
    "DNS record.";

extern const char kIsolatedSandboxedIframesName[] =
    "Isolated sandboxed iframes";
extern const char kIsolatedSandboxedIframesDescription[] =
    "When enabled, applies process isolation to iframes with the 'sandbox' "
    "attribute and without the 'allow-same-origin' permission set on that "
    "attribute. This also applies to documents with a similar CSP sandbox "
    "header, even in the main frame. The affected sandboxed documents can be "
    "grouped into processes based on their URL's site or origin. The default "
    "grouping when enabled is per-site.";

const char kAssistantConsentSimplifiedTextName[] =
    "AssistantConsentSimplifiedText";
const char kAssistantConsentSimplifiedTextDescription[] =
    "Enables simplified consent copy in the Assistant voice search consent "
    "dialog.";

const char kAssistantNonPersonalizedVoiceSearchName[] =
    "AssistantNonPersonalizedVoiceSearch";
const char kAssistantNonPersonalizedVoiceSearchDescription[] =
    "Enables the Assistant voice recognition and search without any "
    "personalization.";

const char kAssistantConsentV2Name[] = "AssistanConsentV2";
const char kAssistantConsentV2Description[] =
    "Enables different strategies for handling backing off from the consent "
    "screen without explicitly clicking yes/no buttons, i.e. when a user taps "
    "outside of the sheet.";

const char kAutofillAlwaysReturnCloudTokenizedCardName[] =
    "Return cloud token details for server credit cards when possible";
const char kAutofillAlwaysReturnCloudTokenizedCardDescription[] =
    "When enabled and where available, forms filled using Google Payments "
    "server cards are populated with cloud token details, including CPAN "
    "(cloud tokenized version of the Primary Account Number) and dCVV (dynamic "
    "CVV).";

const char kAutofillAutoTriggerManualFallbackForCardsName[] =
    "Auto trigger manual fallback for credit card form-filling failure cases";
const char kAutofillAutoTriggerManualFallbackForCardsDescription[] =
    "When enabled, manual fallback will be auto-triggered on form interaction "
    "in the case where autofill failed to fill a credit card form accurately.";

const char kAutofillEnableFIDOProgressDialogName[] =
    "Show FIDO progress dialog on Android";
const char kAutofillEnableFIDOProgressDialogDescription[] =
    "When enabled, a progress dialog is displayed while authenticating with "
    "FIDO on Android.";

const char kAutofillEnableManualFallbackForVirtualCardsName[] =
    "Show manual fallback for virtual cards";
const char kAutofillEnableManualFallbackForVirtualCardsDescription[] =
    "When enabled, manual fallback will be enabled for virtual cards on "
    "Android.";

const char kAutofillEnableMerchantOptOutErrorDialogName[] =
    "Enable merchant opt-out error dialog";
const char kAutofillEnableMerchantOptOutErrorDialogDescription[] =
    "When enabled, if the user attempts to use a virtual card on a website "
    "where the merchant has opted out of virtual cards, a descriptive error "
    "message will appear letting the user know that the merchant has opted "
    "out.";

const char kAutofillEnableCardArtImageName[] = "Enable showing card art images";
const char kAutofillEnableCardArtImageDescription[] =
    "When enabled, card product images (instead of network icons) will be "
    "shown in Payments Autofill UI.";

const char kAutofillEnableCardProductNameName[] =
    "Enable showing card product name";
const char kAutofillEnableCardProductNameDescription[] =
    "When enabled, card product name (instead of issuer network) will be shown "
    "in Payments Autofill UI.";

const char kAutofillEnableCvcForVcnYellowPathName[] =
    "Enable CVC Authentication in the yellow path of the VCN retrieval flow";
const char kAutofillEnableCvcForVcnYellowPathDescription[] =
    "When enabled, if the user encounters the yellow path (challenge path) in "
    "the VCN retrieval flow and the server denotes that the card is eligible "
    "for CVC authentication, CVC authentication will be offered as one of the "
    "challenge options.";

const char kAutofillEnableOfferNotificationForPromoCodesName[] =
    "Extend Autofill offers and rewards notification to promo code offers";
const char kAutofillEnableOfferNotificationForPromoCodesDescription[] =
    "When enabled, a notification will be displayed on page navigation if the "
    "domain has an eligible merchant promo code offer or reward.";

const char kAutofillEnableOffersInClankKeyboardAccessoryName[] =
    "Enable Autofill offers in keyboard accessory";
const char kAutofillEnableOffersInClankKeyboardAccessoryDescription[] =
    "When enabled, offers will be displayed in the keyboard accessory when "
    "available.";

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

const char kAutofillEnableVirtualCardName[] =
    "Offer to use cloud token virtual card in Autofill";
const char kAutofillEnableVirtualCardDescription[] =
    "When enabled, if all requirements are met, Autofill will offer to use "
    "virtual credit cards in form filling.";

const char kAutofillEnableVirtualCardManagementInDesktopSettingsPageName[] =
    "Enable virtual card enrollment management in desktop payments settings "
    "page";
const char
    kAutofillEnableVirtualCardManagementInDesktopSettingsPageDescription[] =
        "When enabled, chrome://settings/payments will offer the option to "
        "enroll in virtual card if the card is eligible and to unenroll if the "
        "card has been enrolled.";

const char kAutofillEnableVirtualCardMetadataName[] =
    "Enable showing metadata for virtual cards";
const char kAutofillEnableVirtualCardMetadataDescription[] =
    "When enabled, Chrome will show metadata together with other card "
    "information when the virtual card is presented to users.";

const char kAutofillEnforceDelaysInStrikeDatabaseName[] =
    "Enforce delay between offering Autofill opportunities in the strike "
    "database";
const char kAutofillEnforceDelaysInStrikeDatabaseDescription[] =
    "When enabled, if previous Autofill feature offer was declined, "
    "Chrome will wait for some time before showing the offer again.";

extern const char kAutofillFillIbanFieldsName[] =
    "Enable Autofill of IBAN fields in forms";
extern const char kAutofillFillIbanFieldsDescription[] =
    "When enabled, Autofill will attempt to fill IBAN (International Bank "
    "Account Number) fields when data is available.";

const char kAutofillFillMerchantPromoCodeFieldsName[] =
    "Enable Autofill of promo code fields in forms";
const char kAutofillFillMerchantPromoCodeFieldsDescription[] =
    "When enabled, Autofill will attempt to fill merchant promo/coupon/gift "
    "code fields when data is available.";

const char kAutofillHighlightOnlyChangedValuesInPreviewModeName[] =
    "Highlight only changed values in preview mode.";
const char kAutofillHighlightOnlyChangedValuesInPreviewModeDescription[] =
    "When Autofill is previewing filling a form, already autofilled values "
    "and other values that are not changed by accepting the preview should "
    "not be highlighted.";

const char kAutofillOfferToSaveCardWithSameLastFourName[] =
    "Offer credit card save for cards with same last-4 but different "
    "expiration dates";
const char kAutofillOfferToSaveCardWithSameLastFourDescription[] =
    "Offer credit card save when Chrome detects a card number with the same "
    "last 4 digits as an existing server card, but a different expiration "
    "date.";

const char kAutofillParseIBANFieldsName[] = "Parse IBAN fields in forms";
const char kAutofillParseIBANFieldsDescription[] =
    "When enabled, Autofill will attempt to find International Bank Account "
    "Number (IBAN) fields when parsing forms.";

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

const char kAutofillRemoveCardExpirationAndTypeTitlesName[] =
    "Remove expiration and type titles from Chrome Payment Settings page on "
    "desktop";
const char kAutofillRemoveCardExpirationAndTypeTitlesDescription[] =
    "When enabled, expiration and type titles will be removed from the Chrome "
    "Payment Settings page on desktop platforms.";

const char kAutofillUseConsistentPopupSettingsIconsName[] =
    "Consistent Autofill settings icon";
const char kAutofillUseConsistentPopupSettingsIconsDescription[] =
    "If enabled, all Autofill data types including addresses, credit cards and "
    "passwords will use a consistent icon in the popup settings footer.";

const char kAutofillSaveAndFillVPAName[] =
    "Offer save and autofill of UPI/VPA values";
const char kAutofillSaveAndFillVPADescription[] =
    "If enabled, when autofill recognizes a UPI/VPA value in a payment form, "
    "it will offer to save it. If saved, it will be offered for filling in "
    "fields which expect a VPA.";

const char kAutofillSaveCardUiExperimentName[] =
    "Enable different UI variants for the upload credit card save bubble";
const char kAutofillSaveCardUiExperimentDescription[] =
    "When enabled, it will trigger slightly different UI variants along with "
    "notification texts, when the upload credit card save bubble is shown.";
const char kAutofillSaveCardUiExperimentCurrentWithUserAvatarAndEmail[] =
    "Current with Avatar and Email";
const char kAutofillSaveCardUiExperimentEncryptedAndSecure[] =
    "Encrypted and Secure";
const char kAutofillSaveCardUiExperimentFasterAndProtected[] =
    "Faster and Protected";

const char kAutofillShowManualFallbackInContextMenuName[] =
    "Show Autofill options in Context Menu";
const char kAutofillShowManualFallbackInContextMenuDescription[] =
    "When enabled, users would get address/credit cards/passwords autofilling "
    "options in the context menu if the context menu is opened on a text field";

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

const char kAutoScreenBrightnessName[] = "Auto Screen Brightness model";
const char kAutoScreenBrightnessDescription[] =
    "Uses Auto Screen Brightness ML model (if it exists) to adjust screen "
    "brightness based on ambient light. If disabled, screen brightness "
    "will be controlled by the heuristic model provided by powerd (and only "
    "on devices that have ambient light sensors).";

const char kBackForwardCacheName[] = "Back-forward cache";
const char kBackForwardCacheDescription[] =
    "If enabled, caches eligible pages after cross-site navigations."
    "To enable caching pages on same-site navigations too, choose 'enabled "
    "same-site support'.";

const char kEnableBackForwardCacheForScreenReaderName[] =
    "Enable Back-forward cache for screen readers";
const char kEnableBackForwardCacheForScreenReaderDescription[] =
    "If enabled, allow pages to enter back/forward cache even if a screen "
    "reader is in use. The page might still not be cached for other reasons.";

const char kBentoBarName[] = "Persistent desks bar";
const char kBentoBarDescription[] =
    "Showing a persistent desks bar at the top of the screen in clamshell mode "
    "when there are more than one desk.";

const char kDragWindowToNewDeskName[] = "Drag window to new desk";
const char kDragWindowToNewDeskDescription[] =
    "Enable dragging and dropping a window to the new desk button in overview "
    "when there are less than the maximum number of desks.";

const char kBiometricReauthForPasswordFillingName[] =
    "Biometric reauth for password filling";
const char kBiometricReauthForPasswordFillingDescription[] =
    "Enables biometric"
    "re-authentication before password filling";

const char kFastCheckoutName[] = "Fast Checkout";
const char kFastCheckoutDescription[] =
    "Enables Fast Checkout experiences in Chrome.";

const char kFailFastQuietChipName[] = "Fail fast quiet chip";
const char kFailFastQuietChipDescription[] =
    "Enables fast finalization of a permission request if it is displayed as a "
    "quiet chip.";

const char kBorealisBigGlName[] = "Borealis Big GL";
const char kBorealisBigGlDescription[] = "Enable Big GL when running Borealis.";

const char kBorealisDiskManagementName[] = "Borealis Disk management";
const char kBorealisDiskManagementDescription[] =
    "Enable experimental disk management settings.";

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

const char kBorealisStorageBallooningName[] = "Borealis Storage Ballooning";
const char kBorealisStorageBallooningDescription[] =
    "Enables storage balloning for Borealis. This takes precedence over the "
    "other Borealis Disk management flag.";

const char kBypassAppBannerEngagementChecksName[] =
    "Bypass user engagement checks";
const char kBypassAppBannerEngagementChecksDescription[] =
    "Bypasses user engagement checks for displaying app banners, such as "
    "requiring that users have visited the site before and that the banner "
    "hasn't been shown recently. This allows developers to test that other "
    "eligibility requirements for showing app banners, such as having a "
    "manifest, are met.";

const char kServiceWorkerBypassFetchHandlerForMainResourceName[] =
    "Bypass Service Worker Fetch Handler for main resource";
const char kServiceWorkerBypassFetchHandlerForMainResourceDescription[] =
    "Bypass starting a service worker and its fetch handler for main resource "
    "requests. The service worker starts after sending a main resource request "
    "and handles subresources. If the main resource could be handled in the "
    "fetch handler, the feature may affect the page load.";

const char kCheckOfflineCapabilityName[] = "Check offline capability for PWAs";
const char kCheckOfflineCapabilityDescription[] =
    "Use advanced offline capability check to decide whether the browser "
    "displays install prompts for PWAs.";

const char kChromeLabsName[] = "Chrome Labs";
const char kChromeLabsDescription[] =
    "Access Chrome Labs through the toolbar menu to see featured user-facing "
    "experimental features.";

const char kChromeRefresh2023Name[] = "Chrome Refresh 2023";
const char kChromeRefresh2023Description[] = "Refresh of Chrome Desktop UI";

const char kClosedTabCacheName[] = "Closed Tab Cache";
const char kClosedTabCacheDescription[] =
    "Enables closed tab cache to instantaneously restore recently closed tabs. "
    "NOTE: This feature is highly experimental and will lead to various "
    "breakages, enable at your own risk.";

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

const char kClientStorageAccessContextAuditingName[] =
    "Access contexts for client-side storage";
const char kClientStorageAccessContextAuditingDescription[] =
    "Record the first-party contexts in which client-side storage was accessed";

const char kClearCrossSiteCrossBrowsingContextGroupWindowNameName[] =
    "Clear window name in top-level cross-site cross-browsing-context-group "
    "navigation";
const char kClearCrossSiteCrossBrowsingContextGroupWindowNameDescription[] =
    "Clear the preserved window.name property when it's a top-level cross-site "
    "navigation that swaps BrowsingContextGroup.";

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const char kChromeTipsInMainMenuName[] =
    "Show 'Tips for Chrome' in Help portion of main menu.";
const char kChromeTipsInMainMenuDescription[] =
    "Enables 'Tips for Chrome' in main menu; the menu item will take users to "
    "an official Google site with information about the latest and most "
    "popular Chrome features.";

const char kChromeTipsInMainMenuNewBadgeName[] =
    "Show 'New' promo badge on 'Tips for Chrome' in Help portion of main menu.";
const char kChromeTipsInMainMenuNewBadgeDescription[] =
    "Enables 'New' promo badge on 'Tips for Chrome' in main menu; experiment to"
    " test the value of this user education feature.";
#endif

const char kChromeWhatsNewUIName[] =
    "Show Chrome What's New page at chrome://whats-new";
const char kChromeWhatsNewUIDescription[] =
    "Enables Chrome What's New page at chrome://whats-new.";

const char kDarkLightModeKMeansColorName[] =
    "K Means color algorithm for dark light mode";
const char kDarkLightModeKMeansColorDescription[] =
    "Enables the alternate K Means color algorithm for extracting color from "
    "user wallpaper to mix into system UI for dark light mode";

extern const char kDeviceForceScheduledRebootName[] =
    "Enable DeviceScheduledReboot policy for all sessions.";
extern const char kDeviceForceScheduledRebootDescription[] =
    "Schedule recurring reboot for the device. Reboots are always executed at "
    "a scheduled time. If the session is active, user will be notified about "
    "the reboot, but the reboot will not be delayed.";

const char kDevicePostureName[] = "Device Posture API";
const char kDevicePostureDescription[] =
    "Enables Device Posture API (foldable devices)";

const char kDiscountConsentV2Name[] = "Discount Consent V2";
const char kDiscountConsentV2Description[] = "Enables Discount Consent V2";

const char kDisruptiveNotificationPermissionRevocationName[] =
    "Disruptive notification permission revocation";
const char kDisruptiveNotificationPermissionRevocationDescription[] =
    "Enables revoking the notification permission on sites that send "
    "disruptive notifications unless the permission was granted through a "
    "prompt that informed the user about this possibility.";

const char kIsolatedAppOriginsName[] = "Isolated App Origins";
const char kIsolatedAppOriginsDescription[] =
    "Enables Isolated App policy enforcement and related APIs (e.g. Direct "
    "Sockets API) for development purposes for a set of origins, specified as "
    "a comma-separated list.";

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

const char kMerchantWidePromotionsName[] = "Merchant wide promotions";
const char kMerchantWidePromotionsDescription[] =
    "Enables the discount consent for all merchants, and show merchant wide "
    "promotions if they are available";

const char kCodeBasedRBDName[] = "Code-based RBD";
const char kCodeBasedRBDDescription[] = "Enables the Code-based RBD.";

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

const char kViewTransitionName[] = "viewTransition API";
const char kViewTransitionDescription[] =
    "Controls the availability of the viewTransition JavaScript API.";

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

const char kEnableAutofillCreditCardAuthenticationName[] =
    "Allow using platform authenticators to retrieve server cards";
const char kEnableAutofillCreditCardAuthenticationDescription[] =
    "When enabled, users will be given the option to use a platform "
    "authenticator (if available) to verify card ownership when retrieving "
    "credit cards from Google Payments.";

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

const char kEnableBrowsingDataLifetimeManagerName[] =
    "Enables the BrowsingDataLifetimeManager service to run.";
const char kEnableBrowsingDataLifetimeManagerDescription[] =
    "Enables the BrowsingDataLifetimeManager service to run and periodically "
    "delete browsing data as specified by the BrowsingDataLifetime policy.";

const char kDesktopPWAsAdditionalWindowingControlsName[] =
    "Desktop PWA Window Minimize/maximize/restore";
const char kDesktopPWAsAdditionalWindowingControlsDescription[] =
    "Enable PWAs to manually recreate the minimize, maximize and restore "
    "window functionalities with respective APIs.";

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)

const char kDesktopPWAsAppHomePageFlagId[] =
    "enable-desktop-pwas-app-home-page";
const char kDesktopPWAsAppHomePageName[] = "desktop PWAs app home page";
const char kDesktopPWAsAppHomePageDescription[] =
    "Use new chrome://apps page which has different UX on desktop";

#endif  // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)

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
    "Experimental UI for tabbed application mode - exploring what PWA windows "
    "would look like with a tab strip.";

const char kDesktopPWAsTabStripSettingsName[] =
    "Desktop PWA tab strips settings";
const char kDesktopPWAsTabStripSettingsDescription[] =
    "Experimental UI for selecting whether a PWA should open in tabbed mode.";

const char kDesktopPWAsSubAppsName[] = "Desktop PWA Sub Apps";
const char kDesktopPWAsSubAppsDescription[] =
    "Enable installed PWAs to create shortcuts by installing their sub apps. "
    "Prototype implementation of: "
    "https://github.com/ivansandrk/multi-apps/blob/main/explainer.md";

const char kDesktopPWAsBorderlessName[] = "Desktop PWA Borderless";
const char kDesktopPWAsBorderlessDescription[] =
    "Enable web app manifests to declare borderless mode as a display "
    "override. Prototype implementation of: go/borderless-mode.";

const char kDesktopPWAsWebBundlesName[] = "Desktop PWAs Web Bundles";
const char kDesktopPWAsWebBundlesDescription[] =
    "Adds support for web bundles, making web apps able to be launched "
    "offline.";

const char kDesktopPWAsDetailedInstallDialogName[] =
    "Desktop PWAs Detailed Install Dialog";
const char kDesktopPWAsDetailedInstallDialogDescription[] =
    "Enable PWAs with screenshots to show a detailed install dialog during "
    "installation";

const char kSkipServiceWorkerCheckInstallOnlyName[] =
    "Skip service worker check for PWA installs";
const char kSkipServiceWorkerCheckInstallOnlyDescription[] =
    "Allows PWAs to be installed without a service worker.";

const char kSkipServiceWorkerForInstallPromptName[] =
    "Promote PWA installation without a service worker";
const char kSkipServiceWorkerForInstallPromptDescription[] =
    "Allows PWAs that can be installed without a service worker to be "
    "promoted. Requires #enable-skip-service-worker-check-install-only flag to "
    "also be enabled.";

const char kEnablePreinstalledWebAppDuplicationFixerName[] =
    "Enable the app deduplication fix for migrated preinstalled web apps";
const char kEnablePreinstalledWebAppDuplicationFixerDescription[] =
    "The preinstalled web app migration encountered app duplication issues "
    "when it rolled out. This code path will attempt to re-migrate instances "
    "of app duplication where the old app failed to stay removed. See "
    "https://crbug.com/1290716.";

const char kEnableTLS13EarlyDataName[] = "TLS 1.3 Early Data";
const char kEnableTLS13EarlyDataDescription[] =
    "This option enables TLS 1.3 Early Data, allowing GET requests to be sent "
    "during the handshake when resuming a connection to a compatible TLS 1.3 "
    "server.";

const char kAccessibilityAcceleratorNotificationsTimeoutName[] =
    "Allows accelerator notifications for accessibility features to time out";
const char kAccessibilityAcceleratorNotificationsTimeoutDescription[] =
    "Enables notifications for accessibility features turned on by keyboard "
    "shortcut (docked magnifier, screen magnifier and high contrast) to time "
    "out instead of remaining pinned.";

const char kChromeVoxQ1FastTrackFeaturesName[] =
    "ChromeVox Q1 Fast-Track Features";
const char kChromeVoxQ1FastTrackFeaturesDescription[] =
    "This option enables experimental Accessibility ChromeVox improvements for "
    "Chrome OS.";

const char kAccessibilityServiceName[] = "Experimental Accessibility Service";
const char kAccessibilityServiceDescription[] =
    "This option enables the experimental Accessibility Service and runs some "
    "accessibility features in the service.";

const char kExperimentalAccessibilityColorEnhancementSettingsName[] =
    "Experimental Accessibility color enhancement settings";
const char kExperimentalAccessibilityColorEnhancementSettingsDescription[] =
    "This option enables the experimental Accessibility color enhancement "
    "settings found in the OS Accessibility settings.";

const char kAccessibilitySelectToSpeakPageMigrationName[] =
    "Select-to-Speak Page Migration";
const char kAccessibilitySelectToSpeakPageMigrationDescription[] =
    "This option enables Select-to-Speak page migration from extension options "
    "page to a Chrome OS settings page.";

const char kAccessibilitySelectToSpeakContextMenuOptionName[] =
    "Select-To-Speak Context menu option";
const char kAccessibilitySelectToSpeakContextMenuOptionDescription[] =
    "This option enables a context menu option that will speak the selected "
    "text";

const char kAccessibilitySelectToSpeakHoverTextImprovementsName[] =
    "Select-to-Speak Hover Text Improvements";
const char kAccessibilitySelectToSpeakHoverTextImprovementsDescription[] =
    "This option enables improvements in the text shown when hovering over the "
    "Select-to-Speak feature icon in the system tray.";

const char kPostQuantumCECPQ2Name[] = "TLS Post-Quantum Confidentiality";
const char kPostQuantumCECPQ2Description[] =
    "This option enables a post-quantum (i.e. resistent to quantum computers) "
    "key exchange algorithm in TLS (CECPQ2).";

const char kMacCoreLocationBackendName[] = "Core Location Backend";
const char kMacCoreLocationBackendDescription[] =
    "Enables usage of the Core Location APIs as the backend for Geolocation "
    "API";

const char kNewMacNotificationAPIName[] =
    "Determines which notification API to use on macOS devices";
const char kNewMacNotificationAPIDescription[] =
    "Enables the usage of Apple's new notification API which will run on macOS "
    "10.14+";

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

const char kEnableGamepadButtonAxisEventsName[] =
    "Gamepad Button and Axis Events";
const char kEnableGamepadButtonAxisEventsDescription[] =
    "Enables the ability to subscribe to changes in buttons and/or axes "
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
    "Enables experimental support for isolated web apps. "
    "See https://github.com/reillyeon/isolated-web-apps for more information.";

const char kEnableIsolatedWebAppDevModeName[] =
    "Enable Isolated Web App Developer Mode";
const char kEnableIsolatedWebAppDevModeDescription[] =
    "Enables the installation of unverified Isolated Web Apps";

const char kEnableRgbKeyboardName[] = "Enable RGB Keyboard Support";
const char kEnableRgbKeyboardDescription[] =
    "Enable RGB Keyboard support on supported devices.";

const char kEnableShortcutCustomizationAppName[] =
    "Enable shortcut customization app";
const char kEnableShortcutCustomizationAppDescription[] =
    "Enable the shortcut customization SWA, allowing users to customize system "
    "shortcuts.";

extern const char kEnableShortcutCustomizationName[] =
    "Enable customization in new shortcuts app";
extern const char kEnableShortcutCustomizationDescription[] =
    "Enable customization of shortcuts in the new shortcuts app.";

const char kEnableInputDeviceSettingsSplitName[] =
    "Enable input device settings split";
const char kEnableInputDeviceSettingsSplitDescription[] =
    "Enable input device settings to be split per-device.";

const char kExperimentalRgbKeyboardPatternsName[] =
    "Enable experimental RGB Keyboard patterns support";
const char kExperimentalRgbKeyboardPatternsDescription[] =
    "Enable experimental RGB Keyboard patterns support on supported devices.";

const char kDownloadAutoResumptionNativeName[] =
    "Enable download auto-resumption in native";
const char kDownloadAutoResumptionNativeDescription[] =
    "Enables download auto-resumption in native";

const char kDownloadBubbleName[] = "Enable download bubble";
const char kDownloadBubbleDescription[] =
    "Enables the download bubble instead of the download shelf.";

const char kDownloadBubbleV2Name[] = "Enable download bubble V2";
const char kDownloadBubbleV2Description[] =
    "Adds features to the download bubble not available on the download shelf. "
    "Only works if the base download bubble flag download-bubble is also "
    "enabled.";

const char kDownloadRangeName[] = "Enable download range support";
const char kDownloadRangeDescription[] =
    "Enables arbitrary download range request support.";

const char kEnableEnhancedSafeBrowsingSettingsImprovementsName[] =
    "Enable enhanced safe browsing settings improvements";
const char kEnableEnhancedSafeBrowsingSettingsImprovementsDescription[] =
    "Adds an in-product-help bubble to the ESB settings options when users are "
    "directed to the chrome security settings page through ESB promotions and "
    "also collapses the ESB option.";

const char kEnableNetworkLoggingToFileName[] = "Enable network logging to file";
const char kEnableNetworkLoggingToFileDescription[] =
    "Enables network logging to a file named netlog.json in the user data "
    "directory. The file can be imported into chrome://net-internals.";

const char kEnableNewDownloadBackendName[] = "Enable new download backend";
const char kEnableNewDownloadBackendDescription[] =
    "Enables the new download backend that uses offline content provider";

const char kEnablePerfettoSystemTracingName[] =
    "Enable Perfetto system tracing";
const char kEnablePerfettoSystemTracingDescription[] =
    "When enabled, Chrome will attempt to connect to the system tracing "
    "service";

const char kEnablePortalsName[] = "Enable Portals.";
const char kEnablePortalsDescription[] =
    "Portals are an experimental web platform feature that allows embedding"
    " and seamless transitions between pages."
    " See https://github.com/WICG/portals and https://wicg.github.io/portals/";

const char kEnablePortalsCrossOriginName[] = "Enable cross-origin Portals.";
const char kEnablePortalsCrossOriginDescription[] =
    "Allows portals to load cross-origin URLs in addition to same-origin ones."
    " Has no effect if Portals are not enabled.";

const char kEnableTranslateSubFramesName[] = "Translate sub frames";
const char kEnableTranslateSubFramesDescription[] =
    "Enable the translation of sub frames (as well as the main frame)";

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

const char kCompositorThreadedScrollbarScrollingName[] =
    "Compositor threaded scrollbar scrolling";
const char kCompositorThreadedScrollbarScrollingDescription[] =
    "Enables pointer-based scrollbar scrolling on the compositor thread "
    "instead of the main thread";

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

const char kEnablePenetratingImageSelectionName[] =
    "Penetrating Image Selection";
const char kEnablePenetratingImageSelectionDescription[] =
    "Enables image options to be surfaced in the context menu for nodes "
    "covered by transparent overlays.";

const char kEnablePixelCanvasRecordingName[] = "Enable pixel canvas recording";
const char kEnablePixelCanvasRecordingDescription[] =
    "Pixel canvas recording allows the compositor to raster contents aligned "
    "with the pixel and improves text rendering. This should be enabled when a "
    "device is using fractional scale factor.";

const char kReduceHorizontalFlingVelocityName[] =
    "Reduce horizontal fling velocity";
const char kReduceHorizontalFlingVelocityDescription[] =
    "Reduces the velocity of horizontal flings to 20% of their original"
    "velocity.";

extern const char kDropInputEventsBeforeFirstPaintName[] =
    "Drop Input Events Before First Paint";
extern const char kDropInputEventsBeforeFirstPaintDescription[] =
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

const char kEnableResamplingInputEventsName[] =
    "Enable resampling input events";
const char kEnableResamplingInputEventsDescription[] =
    "Predicts mouse and touch inputs position at rAF time based on previous "
    "input";
const char kEnableResamplingScrollEventsName[] =
    "Enable resampling scroll events";
const char kEnableResamplingScrollEventsDescription[] =
    "Predicts the scroll amount at vsync time based on previous input";
const char kEnableResamplingScrollEventsExperimentalPredictionName[] =
    "Enable experimental prediction for scroll events";
const char kEnableResamplingScrollEventsExperimentalPredictionDescription[] =
    "Predicts the scroll amount after the vsync time to more closely match "
    "when the frame is visible.";

const char kEnableRestrictedWebApisName[] =
    "Enable the restricted web APIs for high-trusted apps.";
const char kEnableRestrictedWebApisDescription[] =
    "Enable the restricted web APIs for dev trial. This will be replaced with "
    "permission policies to control the capabilities afterwards.";

const char kEnableWebAuthenticationChromeOSAuthenticatorName[] =
    "ChromeOS platform Web Authentication support";
const char kEnableWebAuthenticationChromeOSAuthenticatorDescription[] =
    "Enable the ChromeOS platform authenticator for the Web Authentication "
    "API.";

const char kEnableZeroCopyTabCaptureName[] = "Zero-copy tab capture";
const char kEnableZeroCopyTabCaptureDescription[] =
    "Enable zero-copy content tab for getDisplayMedia() APIs.";

const char kEnableRegionCaptureExperimentalSubtypesName[] =
    "Region capture experimental subtypes";
const char kEnableRegionCaptureExperimentalSubtypesDescription[] =
    "Enables experiment support for CropTarget.fromElement to use other "
    "Element subtypes than just <div> and <iframe>.";

const char kExperimentalWebAssemblyFeaturesName[] = "Experimental WebAssembly";
const char kExperimentalWebAssemblyFeaturesDescription[] =
    "Enable web pages to use experimental WebAssembly features.";

const char kExperimentalWebAssemblyJSPIName[] =
    "Experimental WebAssembly JavaScript Promise Integration (JSPI)";
const char kExperimentalWebAssemblyJSPIDescription[] =
    "Enable web pages to use experimental WebAssembly JavaScript Promise "
    "Integration (JSPI) "
    "API.";

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

const char kExtensionContentVerificationName[] =
    "Extension Content Verification";
const char kExtensionContentVerificationDescription[] =
    "This flag can be used to turn on verification that the contents of the "
    "files on disk for extensions from the webstore match what they're "
    "expected to be. This can be used to turn on this feature if it would not "
    "otherwise have been turned on, but cannot be used to turn it off (because "
    "this setting can be tampered with by malware).";
const char kExtensionContentVerificationBootstrap[] =
    "Bootstrap (get expected hashes, but do not enforce them)";
const char kExtensionContentVerificationEnforce[] =
    "Enforce (try to get hashes, and enforce them if successful)";
const char kExtensionContentVerificationEnforceStrict[] =
    "Enforce strict (hard fail if we can't get hashes)";

#if BUILDFLAG(ENABLE_EXTENSIONS)
const char kExtensionsMenuAccessControlName[] =
    "Extensions Menu Access Control";
const char kExtensionsMenuAccessControlDescription[] =
    "Enables a redesigned extensions menu that allows the user to control "
    "extensions site access.";
#endif

const char kExtensionsOnChromeUrlsName[] = "Extensions on chrome:// URLs";
const char kExtensionsOnChromeUrlsDescription[] =
    "Enables running extensions on chrome:// URLs, where extensions explicitly "
    "request this permission.";

const char kFilteringScrollPredictionName[] = "Filtering scroll prediction";
const char kFilteringScrollPredictionDescription[] =
    "Enable filtering of predicted scroll events";

const char kFractionalScrollOffsetsName[] = "Fractional Scroll Offsets";
const char kFractionalScrollOffsetsDescription[] =
    "Enables fractional scroll offsets inside Blink, exposing non-integer "
    "offsets to web APIs.";

const char kFedCmName[] = "FedCM";
const char kFedCmDescription[] =
    "Enables JavaScript API to intermediate federated identity requests.";

const char kFedCmAutoSigninName[] = "FedCmAutoSignin";
const char kFedCmAutoSigninDescription[] =
    "Enables auto sign-in in the FedCM API. Requires FedCM to be enabled.";

const char kFedCmIframeSupportName[] = "FedCM with iframe support";
const char kFedCmIframeSupportDescription[] =
    "Enables the 'identity-credentials-get' Permissions-Policy for the FedCM "
    "API.";

const char kFedCmLoginHintName[] = "FedCmLoginHint";
const char kFedCmLoginHintDescription[] =
    "Allows the FedCM API to be used with a login hint which specifies a "
    "specific IDP account that needs to be displayed in the FedCM UI.";

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

const char kFedCmUserInfoName[] = "FedCmUserInfo";
const char kFedCmUserInfoDescription[] =
    "Allows an identity provider to request user info instead of token from "
    "its own iframe. Requires FedCM to be enabled.";

const char kFedCmWithoutThirdPartyCookiesName[] =
    "FedCmWithoutThirdPartyCookies";
const char kFedCmWithoutThirdPartyCookiesDescription[] =
    "Allows the FedCM API to be enabled when third party cookies are disabled.";

const char kFedCmRpContextName[] = "FedCmRpContext";
const char kFedCmRpContextDescription[] =
    "Allows relying parties to describe which context (e.g. sign-up vs "
    "sign-in) the FedCM is executing on.";

const char kFileHandlingIconsName[] = "File Handling Icons";
const char kFileHandlingIconsDescription[] =
    "Allows websites using the file handling API to also register file type "
    "icons. See https://github.com/WICG/file-handling/blob/main/explainer.md "
    "for more information.";

const char kFillingAcrossAffiliatedWebsitesName[] =
    "Fill passwords across affiliated websites.";
const char kFillingAcrossAffiliatedWebsitesDescription[] =
    "Enables filling password on a website when there is saved "
    "password on affiliated website.";

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

const char kFullUserAgentName[] = "Full User-Agent request header";
const char kFullUserAgentDescription[] =
    "If set, use the full (non-reduced) user agent string for the User-Agent "
    "request header and the JS APIs.";

const char kGalleryAppPdfEditNotificationName[] =
    "Gallery App Pdf Edit Notification";
const char kGalleryAppPdfEditNotificationDescription[] =
    "Shows a notification to provide an option to open Gallery app for a "
    "downloaded pdf file";

const char kGlobalMediaControlsForCastName[] = "Global Media Controls for Cast";
const char kGlobalMediaControlsForCastDescription[] =
    "Shows Cast sessions in the Global Media Controls UI.";

const char kMediaRemotingWithoutFullscreenName[] =
    "Media Remoting without videos in fullscreen mode";
const char kMediaRemotingWithoutFullscreenDescription[] =
    "Starts Media Remoting from Global Media Controls without making the "
    "videos fullscreen.";

#if BUILDFLAG(IS_CHROMEOS)
const char kGlobalMediaControlsCrOSUpdatedUIName[] =
    "Global Media Controls CrOS updated UI";
const char kGlobalMediaControlsCrOSUpdatedUIDescription[] =
    "Show updated UI for Global Media Controls in CrOS.";
#endif

const char kGlobalMediaControlsModernUIName[] =
    "Global Media Controls Modern UI";
const char kGlobalMediaControlsModernUIDescription[] =
    "Use a redesigned version of the Global Media Controls UI. Requires "
    "#global-media-controls to also be enabled.";

const char kGoogleOneOfferFilesBannerName[] = "Google One offer Files banner";
const char kGoogleOneOfferFilesBannerDescription[] =
    "Shows a Files banner about Google One offer.";

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

const char kCastStreamingVp9Name[] =
    "Enable VP9 video encoding for Cast Streaming";
const char kCastStreamingVp9Description[] =
    "Offers the VP9 video codec when negotiating Cast Streaming, and uses VP9 "
    "if selected for the session.";

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

const char kHandwritingLegacyRecognitionAllLangName[] =
    "Handwriting Legacy Recognition All Languages";
const char kHandwritingLegacyRecognitionAllLangDescription[] =
    "Enables new on-device recognition for handwriting legacy paths in all "
    "supported languages.";

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

const char kHiddenNetworkMigrationName[] = "Hidden Network Migration";

const char kHiddenNetworkMigrationDescription[] =
    "Enables a privacy improvement that removes wrongly configured hidden"
    "networks and mitigates the creation of these networks.";

const char kHideShelfControlsInTabletModeName[] =
    "Hide shelf control buttons in tablet mode.";

const char kHideShelfControlsInTabletModeDescription[] =
    "Hides home, back, and overview button from the shelf while the device is "
    "in tablet mode. Predicated on shelf-hotseat feature being enabled.";

extern const char kHotspotName[] = "Hotspot";
extern const char kHotspotDescription[] =
    "Enables the Chromebook to share its cellular internet connection to other "
    "devices through WiFi.";

const char kTabAudioMutingName[] = "Tab audio muting UI control";
const char kTabAudioMutingDescription[] =
    "When enabled, the audio indicators in the tab strip double as tab audio "
    "mute controls.";

const char kTabSelectionEditorV2Name[] = "Tab selection editor V2";
const char kTabSelectionEditorV2Description[] =
    "Enable improved bulk tab editing capabilities.";

const char kStartSurfaceReturnTimeName[] = "Start surface return time";
const char kStartSurfaceReturnTimeDescription[] =
    "Enable showing start surface at startup after specified time has elapsed";

const char kStorageBucketsName[] = "Storage Buckets API";
const char kStorageBucketsDescription[] =
    "Enable experimental Storage Buckets API, allowing websites to create "
    "multiple buckets of storage to organize their data, allowing user agents "
    "to delete each bucket independently of other buckets.";

const char kHttpsOnlyModeName[] = "HTTPS-First Mode Setting";
const char kHttpsOnlyModeDescription[] =
    "Adds a setting under chrome://settings/security to opt-in to HTTPS-First "
    "Mode.";

const char kHttpsFirstModeV2Name[] = "HTTPS-First Mode V2";
const char kHttpsFirstModeV2Description[] =
    "Enable rearchitected version of HTTPS-First Mode.";

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

const char kUpdateHistoryEntryPointsInIncognitoName[] =
    "Update history entry points in Incognito.";
const char kUpdateHistoryEntryPointsInIncognitoDescription[] =
    "When enabled, the entry points to history UI from Incognito mode will be "
    "removed for iOS and Desktop. An educative placeholder will be shown for "
    "Android history page.";

const char kIncognitoNtpRevampName[] = "Revamped Incognito New Tab Page";
const char kIncognitoNtpRevampDescription[] =
    "When enabled, Incognito new tab page will have an updated UI";

const char kIncognitoScreenshotName[] = "Incognito Screenshot";
const char kIncognitoScreenshotDescription[] =
    "Enables Incognito screenshots on Android. It will also make Incognito "
    "thumbnails visible.";

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

const char kInstallIsolatedWebAppFromFile[] =
    "Install Isolated Web App from Unsigned Web Bundle";
const char kInstallIsolatedWebAppFromFileDescription[] =
    "Installs a new developer mode Isolated Web App backed by the unsigned "
    "Web Bundle at the provided path.";

const char kInstallIsolatedWebAppFromUrl[] =
    "Install Isolated Web App from Proxy URL";
const char kInstallIsolatedWebAppFromUrlDescription[] =
    "Installs a new developer mode Isolated Web App whose contents are hosted "
    "at the provided HTTP(S) URL.";

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

const char kJourneysContentClusteringName[] =
    "History Journeys Content Clustering";
const char kJourneysContentClusteringDescription[] =
    "Enables clustering page loads using content annotations.";

const char kJourneysImagesName[] = "History Journeys Images";
const char kJourneysImagesDescription[] =
    "Enables Images for the History Journeys UI.";

const char kJourneysLabelsName[] = "History Journeys Labels";
const char kJourneysLabelsDescription[] =
    "Enables labels for Journeys within the History Journeys UI.";

const char kJourneysNavigationContextClusteringName[] =
    "History Journeys Navigation Context Clustering";
const char kJourneysNavigationContextClusteringDescription[] =
    "Enables context clustering to be done at navigation time";

const char kJourneysOmniboxActionName[] = "History Journeys Omnibox Action";
const char kJourneysOmniboxActionDescription[] =
    "Enables the History Journeys Omnibox Action.";

const char kJourneysOmniboxHistoryClusterProviderName[] =
    "History Journeys Omnibox History Cluster Provider";
const char kJourneysOmniboxHistoryClusterProviderDescription[] =
    "Enables the History Journeys Omnibox History Cluster Provider to surface "
    "Journeys as a suggestion row instead of an action chip.";

const char kJourneysPersistedClustersName[] =
    "History Journeys Persisted Clusters";
const char kJourneysPersistedClustersDescription[] =
    "Enables the persistence of clusters into the history database.";

const char kJourneysShowAllClustersName[] =
    "History Journeys Show All Clusters";
const char kJourneysShowAllClustersDescription[] =
    "Enables all Journeys clusters to be shown on prominent UI surfaces";

const char kJourneysIncludeSyncedVisitsName[] =
    "History Journeys Include SyncedVisits";
const char kJourneysIncludeSyncedVisitsDescription[] =
    "Enabled synced visits to be included in History Journeys clusters.";

const char kExtractRelatedSearchesFromPrefetchedZPSResponseName[] =
    "Extract Related Searches from Prefetched ZPS Response";
const char kExtractRelatedSearchesFromPrefetchedZPSResponseDescription[] =
    "Enables page annotation logic to source related searches data from "
    "prefetched ZPS responses";

const char kLargeFaviconFromGoogleName[] = "Large favicons from Google";
const char kLargeFaviconFromGoogleDescription[] =
    "Request large favicons from Google's favicon service";

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

const char kLogJsConsoleMessagesName[] =
    "Log JS console messages in system logs";
const char kLogJsConsoleMessagesDescription[] =
    "Enable logging JS console messages in system logs, please note that they "
    "may contain PII.";

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

const char kMouseSubframeNoImplicitCaptureName[] =
    "Disable mouse implicit capture for iframe";
const char kMouseSubframeNoImplicitCaptureDescription[] =
    "When enable, mouse down does not implicit capture for iframe.";

const char kCanvas2DLayersName[] =
    "Enables canvas 2D methods BeginLayer and EndLayer";
const char kCanvas2DLayersDescription[] =
    "Enables the canvas 2D methods BeginLayer and EndLayer.";

const char kEnableMachineLearningModelLoaderWebPlatformApiName[] =
    "Enables Machine Learning Model Loader Web Platform API";
const char kEnableMachineLearningModelLoaderWebPlatformApiDescription[] =
    "Enables the Machine Learning Model Loader Web Platform API.";

const char kSystemProxyForSystemServicesName[] =
    "Enable system-proxy for selected system services";
const char kSystemProxyForSystemServicesDescription[] =
    "Enabling this flag will allow ChromeOS system service which require "
    "network connectivity to use the system-proxy daemon for authentication to "
    "remote HTTP web proxies.";

const char kDestroyProfileOnBrowserCloseName[] =
    "Destroy Profile on browser close";
const char kDestroyProfileOnBrowserCloseDescription[] =
    "Release memory and other resources when a Profile's last browser window "
    "is closed, rather than when Chrome closes completely.";

const char kDestroySystemProfilesName[] = "Destroy System Profile";
const char kDestroySystemProfilesDescription[] =
    "After you close the Profile Picker, release memory and other resources "
    "owned by the System Profile. This requires "
    "#destroy-profile-on-browser-close.";

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

const char kOmniboxAdaptiveSuggestionsCountName[] =
    "Adaptive Omnibox Suggestions count";
const char kOmniboxAdaptiveSuggestionsCountDescription[] =
    "Dynamically adjust number of presented Omnibox suggestions depending on "
    "available space. When enabled, this feature will increase (or decrease) "
    "amount of offered Omnibox suggestions to fill in the space between the "
    "Omnibox and soft keyboard (if any). See also Max Autocomplete Matches "
    "flag to adjust the limit of offered suggestions. The number of shown "
    "suggestions will be no less than the platform default limit.";

const char kOmniboxAssistantVoiceSearchName[] =
    "Omnibox Assistant Voice Search";
const char kOmniboxAssistantVoiceSearchDescription[] =
    "When enabled, use Assistant for omnibox voice query recognition instead of"
    " Android's built-in voice recognition service. Only works on Android.";

const char kOmniboxBookmarkPathsName[] = "Omnibox Bookmark Paths";
const char kOmniboxBookmarkPathsDescription[] =
    "Allows inputs to match with bookmark paths. E.g. 'planets jupiter' can "
    "suggest a bookmark titled 'Jupiter' with URL "
    "'en.wikipedia.org/wiki/Jupiter' located in a path containing 'planet.'";

const char kOmniboxClosePopupWithEscapeName[] =
    "Omnibox Close Popup with Escape";
const char kOmniboxClosePopupWithEscapeDescription[] =
    "When enabled, pressing escape when the omnibox popup is open and the "
    "default suggestion is selected will close the omnibox without removing "
    "its focus or clearing user input.";

const char kOmniboxDefaultBrowserPedalName[] = "Omnibox Default Browser Pedal";
const char kOmniboxDefaultBrowserPedalDescription[] =
    "Enables a pedal that helps the user 'set default browser'.";

const char kOmniboxDisableCGIParamMatchingName[] =
    "Disable CGI Param Name Matching";
const char kOmniboxDisableCGIParamMatchingDescription[] =
    "Disables using matches in CGI parameter names while scoring suggestions.";

const char kOmniboxDocumentProviderAsoName[] = "Omnibox Document Provider ASO";
const char kOmniboxDocumentProviderAsoDescription[] =
    "If document suggestions are enabled, swaps the backend from cloudsearch "
    "to ASO (Apps Search Overlay) search.";

const char kOmniboxDomainSuggestionsName[] = "Omnibox Domain Suggestions";
const char kOmniboxDomainSuggestionsDescription[] =
    "If enabled, history URL suggestions from hosts visited often bypass the "
    "per provider limit.";

const char kOmniboxExperimentalSuggestScoringName[] =
    "Omnibox Experimental Suggest Scoring";
const char kOmniboxExperimentalSuggestScoringDescription[] =
    "Enables an experimental scoring mode for suggestions when Google is the "
    "default search engine.";

const char kOmniboxFuzzyUrlSuggestionsName[] = "Omnibox Fuzzy URL Suggestions";
const char kOmniboxFuzzyUrlSuggestionsDescription[] =
    "Enables URL suggestions for inputs that may contain typos.";

const char kOmniboxGroupingFrameworkName[] = "Omnibox Grouping Framework";
const char kOmniboxGroupingFrameworkDescription[] =
    "Enables an alternative grouping implementation for omnibox "
    "autocompletion.";

const char kOmniboxHistoryQuickProviderSpecificityScoreCountUniqueHostsName[] =
    "Omnibox HQP Specificity";
const char
    kOmniboxHistoryQuickProviderSpecificityScoreCountUniqueHostsDescription[] =
        "When enabled, HQP doesn't demote same-host suggestions.";

const char kOmniboxKeepSecondaryZeroSuggestName[] =
    "Keeps all secondary zero-prefix suggestions.";
const char kOmniboxKeepSecondaryZeroSuggestDescription[] =
    "Keeps all zero-prefix suggestions in the second column and does not count "
    "them toward the overall zero-suggest limit.";

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

const char kOmniboxMostVisitedTilesName[] = "Omnibox Most Visited Tiles";
const char kOmniboxMostVisitedTilesDescription[] =
    "Display a list of frequently visited pages from history as a single row "
    "with a carousel instead of one URL per line.";

const char kOmniboxMostVisitedTilesAddRecycledViewPoolName[] =
    "Omnibox Most Visited Tiles Add Recycled View Pool";
const char kOmniboxMostVisitedTilesAddRecycledViewPoolDescription[] =
    "Add a recycled view pool to omnibox most visited tiles carousel to "
    "increase tile view reuse and reduce jackiness.";

const char kOmniboxRemoveExcessiveRecycledViewClearCallsName[] =
    "Omnibox Remove Excessive Recycled View Clear Calls";
const char kOmniboxRemoveExcessiveRecycledViewClearCallsDescription[] =
    "Remove excessive clear calls on RecycledViewPool in omnibox.";

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

const char kOmniboxRichAutocompletionPromisingName[] =
    "Omnibox Rich Autocompletion Promising Combinations";
const char kOmniboxRichAutocompletionPromisingDescription[] =
    "Allow autocompletion for titles and non-prefixes. Suggestions whose "
    "titles or URLs contain the user input as a continuous chunk, but not "
    "necessarily a prefix, can be the default suggestion. Otherwise, only "
    "suggestions whose URLs are prefixed by the user input can be.";

const char kOmniboxSiteSearchStarterPackName[] =
    "Omnibox Site Search Starter Pack";
const char kOmniboxSiteSearchStarterPackDescription[] =
    "Enables @history, @bookmarks, and @tabs scopes in Omnibox Site "
    "Search/Keyword Mode";

const char kOmniboxLocalHistoryZeroSuggestBeyondNTPName[] =
    "Allow local history zero-prefix suggestions beyond NTP";
const char kOmniboxLocalHistoryZeroSuggestBeyondNTPDescription[] =
    "Enables local history zero-prefix suggestions in every context in which "
    "the remote zero-prefix suggestions are enabled.";

const char kOmniboxFocusTriggersSRPZeroSuggestName[] =
    "Allow Omnibox contextual web on-focus suggestions on the SRP";
const char kOmniboxFocusTriggersSRPZeroSuggestDescription[] =
    "Enables on-focus suggestions on the Search Results page. "
    "Requires on-focus suggestions for the contextual web to be enabled. "
    "Will only work if user is signed-in and syncing.";

const char kOmniboxFocusTriggersContextualWebZeroSuggestName[] =
    "Omnibox on-focus suggestions for the contextual Web";
const char kOmniboxFocusTriggersContextualWebZeroSuggestDescription[] =
    "Enables on-focus suggestions on the Open Web, that are contextual to the "
    "current URL. Will only work if user is signed-in and syncing, or is "
    "otherwise eligible to send the current page URL to the suggest server.";

const char kOmniboxMlLogUrlScoringSignalsName[] =
    "Log Omnibox URL Scoring Signals";
const char kOmniboxMlLogUrlScoringSignalsDescription[] =
    "Enables Omnibox to log scoring signals of URL suggestions.";

const char kOmniboxMlRelevanceScoringName[] = "Omnibox ML Relevance Scoring";
const char kOmniboxMlRelevanceScoringDescription[] =
    "Enables ML-based relevance scoring of Omnibox URL Suggestions.";

const char kOmniboxMlUrlScoringModelName[] =
    "Omnibox autocomplete URL scoring model.";
const char kOmniboxMlUrlScoringModelDescription[] =
    "Enables machine learning scoring model for Omnibox URL sugestions.";

const char kOmniboxOnClobberFocusTypeOnContentName[] =
    "Omnibox On Clobber Focus Type On Content";
const char kOmniboxOnClobberFocusTypeOnContentDescription[] =
    "Send ON_CLOBBER focus type for zero-prefix requests with an empty input "
    "on Web/SRP.";

const char kOmniboxShortBookmarkSuggestionsName[] =
    "Omnibox short bookmark suggestions";
const char kOmniboxShortBookmarkSuggestionsDescription[] =
    "Match very short input words to beginning of words in bookmark "
    "suggestions.";

const char kOmniboxShortcutExpandingName[] = "Omnibox shortcut expanding";
const char kOmniboxShortcutExpandingDescription[] =
    "Expand the last word in the shortcut text to be a complete word from the "
    "suggestion text.";

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

const char kOmniboxUpdatedConnectionSecurityIndicatorsName[] =
    "Omnibox Updated connection security indicators";
const char kOmniboxUpdatedConnectionSecurityIndicatorsDescription[] =
    "Use new connection security indicators for https pages in the omnibox.";

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
    "Select strategies used to promote quads to HW overlays.";
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

const char kPageEntitiesPageContentAnnotationsName[] =
    "Page entities content annotations";
const char kPageEntitiesPageContentAnnotationsDescription[] =
    "Enables annotating the page entities model for each page load on-device.";

const char kPageInfoAboutThisSiteNonEnName[] =
    "'About this site' section in page info in all languages";
const char kPageInfoAboutThisSiteNonEnDescription[] =
    "Enable the 'About this site' section in the page info UI in languages "
    "other than English.";

const char kPageInfoboutThisPageDescriptionPlaceholderName[] =
    "AboutThisPage description placeholder";
const char kPageInfoboutThisPageDescriptionPlaceholderDescription[] =
    "Shows a placeholder when no description is availble instead of not "
    "showing an entry at all";

const char kPageInfoboutThisPagePersistentEntryName[] =
    "AboutThisPage persistent SidePanel entry";
const char kPageInfoboutThisPagePersistentEntryDescription[] =
    "Registers a SidePanel entry on pageload if 'AboutThisPage' info is "
    "available";

const char kPageInfoCookiesSubpageName[] = "Cookies subpage in page info";
const char kPageInfoCookiesSubpageDescription[] =
    "Enable the Cookies subpage in page info for managing cookies and site "
    "data.";

const char kPageInfoMoreAboutThisPageName[] =
    "'More about this page' link in page info";
const char kPageInfoMoreAboutThisPageDescription[] =
    "Enable the 'More about this page' link in the 'From the web' section of "
    "page info.";

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

const char kPasswordImportName[] = "Password import";
const char kPasswordImportDescription[] =
    "Import functionality in password settings.";

const char kPasswordStrengthIndicatorName[] = "Password strength indicator";
const char kPasswordStrengthIndicatorDescription[] =
    "Enables password strength indicator when typing a password during a "
    "sign-up and password change flows.";

const char kForceEnableFastCheckoutCapabilitiesName[] =
    "Force enable fast checkout capabilities";
const char kForceEnableFastCheckoutCapabilitiesDescription[] =
    "Force enables fast checkout capabilities for every domain, regardless "
    "of the server response.";

const char kPdfOcrName[] = "Performs OCR on inaccessible PDFs";
const char kPdfOcrDescription[] =
    "Enables a feature whereby inaccessible (i.e. untagged) PDFs are made "
    "accessible using an optical character recognition service.";

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

const char kRecordPermissionExpirationTimestampsName[] =
    "Record permission expiration timestamps";
const char kRecordPermissionExpirationTimestampsDescription[] =
    "When enabled, permissions grants with a durable session model will have "
    "an expiration date set.";

const char kPointerLockOptionsName[] = "Enables pointer lock options";
const char kPointerLockOptionsDescription[] =
    "Enables pointer lock unadjustedMovement. When unadjustedMovement is set "
    "to true, pointer movements wil not be affected by the underlying platform "
    "modications such as mouse accelaration.";

const char kPowerBookmarksSidePanel[] = "Power bookmarks side panel";
const char kPowerBookmarksSidePanelDescription[] =
    "Enables the power bookmarks version of the bookmarks side panel content.";

const char kBookmarksImprovedSaveFlowName[] = "Improved bookmarks save flow";
const char kBookmarksImprovedSaveFlowDescription[] =
    "Enabled an improved save flow for bookmarks.";

const char kBookmarksRefreshName[] = "Bookmarks refresh";
const char kBookmarksRefreshDescription[] =
    "Enable various changes to bookmarks.";

const char kOmniboxTriggerForPrerender2Name[] =
    "Omnibox trigger for Prerender2";
const char kOmniboxTriggerForPrerender2Description[] =
    "Enables the new omnibox trigger prerenderer implementation.";

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

const char kPrivacyGuideAndroidName[] = "Privacy Guide on Android";
const char kPrivacyGuideAndroidDescription[] =
    "Shows a new subpage in Settings that helps the user to review various "
    "privacy settings.";

const char kPrivacySandboxAdsAPIsOverrideName[] = "Privacy Sandbox Ads APIs";
const char kPrivacySandboxAdsAPIsOverrideDescription[] =
    "Enables Privacy Sandbox APIs: Attribution Reporting, Fledge, Topics, "
    "Fenced Frames, Shared Storage, Private Aggregation, and their associated "
    "features.";

const char kPrivacySandboxSettings4Name[] = "Privacy Sandbox Settings V4";
const char kPrivacySandboxSettings4Description[] =
    "Enables updated Privacy Sandbox UI";

const char kPrivateAggregationDeveloperModeName[] =
    "Private Aggregation developer mode";
const char kPrivateAggregationDeveloperModeDescription[] =
    "Enables the developer mode for the Private Aggregation API. This removes "
    "all reporting delays. Only works if the Private Aggregation API is "
    "already enabled.";

const char kProminentDarkModeActiveTabTitleName[] =
    "Prominent Dark Mode Active Tab Titles";
const char kProminentDarkModeActiveTabTitleDescription[] =
    "Makes the active tab title in dark mode bolder so the active tab is "
    "easier "
    "to identify.";

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

const char kPwaUpdateDialogForAppTitleName[] =
    "Enable PWA install update dialog for name changes";
const char kPwaUpdateDialogForAppTitleDescription[] =
    "Enable a confirmation dialog that shows up when a PWA changes its name";

const char kQuicName[] = "Experimental QUIC protocol";
const char kQuicDescription[] = "Enable experimental QUIC protocol support.";

const char kQuickIntensiveWakeUpThrottlingAfterLoadingName[] =
    "Quick intensive throttling after loading";
const char kQuickIntensiveWakeUpThrottlingAfterLoadingDescription[] =
    "For pages that are loaded when backgrounded, activates intensive "
    "throttling after 1 minute instead of the default 5 minutes. Intensive "
    "throttling will limit wake ups, from setTimeout and setInterval tasks "
    "with a high nesting level and delayed scheduler.postTask tasks, to 1 per "
    "minute. See https://chromestatus.com/feature/5580139453743104 for more "
    "info.";

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

const char kReduceUserAgentName[] = "Reduce User-Agent request header";
const char kReduceUserAgentDescription[] =
    "Reduce (formerly, \"freeze\") the amount of information available in "
    "the User-Agent request header. "
    "See https://www.chromium.org/updates/ua-reduction for more info.";

const char kRestrictGamepadAccessName[] = "Restrict gamepad access";
const char kRestrictGamepadAccessDescription[] =
    "Enables Permissions Policy and Secure Context restrictions on the Gamepad "
    "API";

const char kRoundedDisplay[] = "Rounded display";
const char kRoundedDisplayDescription[] =
    "Enables rounded corners for the display";

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

const char kSameAppWindowCycleName[] = "Cros Labs: Same App Window Cycling";
const char kSameAppWindowCycleDescription[] =
    "Use Alt+` to cycle through the windows of the active application.";

const char kPartitionedCookiesName[] = "Partitioned cookies";
const char kPartitionedCookiesDescription[] =
    "Controls if the Partitioned cookie attribute is enabled.";

const char kNoncedPartitionedCookiesName[] = "Nonced partitioned cookies only";
const char kNoncedPartitionedCookiesDescription[] =
    "When this flag is enabled, we allow partitioned cookies whose "
    "partition keys contain a nonce even if the \"Partitioned cookies\" "
    "feature is disabled. If \"Partitioned cookies\" are enabled, then "
    "enabling or disabling this feature does nothing.";

const char kThirdPartyStoragePartitioningName[] =
    "Experimental third-party storage partitioning.";
const char kThirdPartyStoragePartitioningDescription[] =
    "Enables partitioning of third-party storage by top-level site. "
    "Note: this is under active development and may result in unexpected "
    "behavior. Please file bugs at https://bugs.chromium.org/p/chromium/issues/"
    "entry?labels=StoragePartitioning-trial-bugs&components=Blink%3EStorage.";

const char kScreenSaverPreviewName[] = "Screen saver preview";
const char kScreenSaverPreviewDescription[] =
    "Enables the screen saver preview feature which allows the users to "
    "preview current screen saver.";

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

const char kScrollUnificationName[] = "Scroll Unification";
const char kScrollUnificationDescription[] =
    "Refactoring project that eliminates scroll handling code from Blink. "
    "Does not affect behavior or performance.";

extern const char kSearchResultInlineIconName[] = "Search Result Inline Icons";
extern const char kSearchResultInlineIconDescription[] =
    "Show iconified text and vector icons "
    "in launcher search results.";

extern const char kDynamicSearchUpdateAnimationName[] =
    "Dynamic Search Result Update Animation";
extern const char kDynamicSearchUpdateAnimationDescription[] =
    "Dynamically adjust the search result update animation when those update "
    "animations are preempted. Shortened animation durations configurable "
    "(unit: milliseconds).";

const char kSecurePaymentConfirmationDebugName[] =
    "Secure Payment Confirmation Debug Mode";
const char kSecurePaymentConfirmationDebugDescription[] =
    "This flag removes the restriction that PaymentCredential in WebAuthn and "
    "secure payment confirmation in PaymentRequest API must use user verifying "
    "platform authenticators.";

const char kSendTabToSelfSigninPromoName[] = "Send tab to self sign-in promo";
const char kSendTabToSelfSigninPromoDescription[] =
    "Enables a sign-in promo if the user attempts to share a tab while being "
    "signed out";

const char kSidePanelJourneysFlagId[] = "side-panel-journeys";
const char kSidePanelJourneysName[] = "Side panel journeys";
const char kSidePanelJourneysDescription[] =
    "Enables Journeys within the side panel.";

const char kSidePanelJourneysQuerylessFlagId[] =
    "side-panel-journeys-queryless";
const char kSidePanelJourneysQuerylessName[] = "Side panel journeys queryless";
const char kSidePanelJourneysQuerylessDescription[] =
    "Enables Journeys queryless view within the side panel.";

const char kSidePanelWebViewName[] = "Side panel webview";
const char kSidePanelWebViewDescription[] =
    "Adds a side panel option to load arbitrary web content, with a URL bar.";

const char kSharingDesktopScreenshotsName[] = "Desktop Screenshots";
const char kSharingDesktopScreenshotsDescription[] =
    "Enables taking"
    " screenshots from the desktop sharing hub.";

const char kShelfDragToPinName[] = "Pin apps in shelf using drag";

const char kShelfDragToPinDescription[] =
    "Enables pinning unpinned items in shelf by dragging them to the part of "
    "the shelf that contains pinned apps.";

const char kShelfGesturesWithVirtualKeyboardName[] =
    "Enable shelf gestures from virtual keyboard";
const char kShelfGesturesWithVirtualKeyboardDescription[] =
    "Enables shelf tablet mode gestures to show hotseat, got to home screen, "
    "or to go to overview while virtual keyboard is visible";

const char kShelfHoverPreviewsName[] =
    "Show previews of running apps when hovering over the shelf.";
const char kShelfHoverPreviewsDescription[] =
    "Shows previews of the open windows for a given running app when hovering "
    "over the shelf.";

const char kShelfPalmRejectionSwipeOffsetName[] =
    "Shelf Palm Rejection: Swipe Offset";
const char kShelfPalmRejectionSwipeOffsetDescription[] =
    "Enables palm rejection in the shelf by setting an offset for the swipe "
    "gesture that drags the hotseat to a extended state for certain stylus "
    "apps.";

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

extern const char kSimLockPolicyName[] = "SIM Lock Policy";
extern const char kSimLockPolicyDescription[] =
    "Enable the support for policy controlled enabling or disabling of PIN "
    "Locking SIMs on managed devices.";

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

const char kWebViewTagSiteIsolationName[] = "Site isolation for <webview> tags";
const char kWebViewTagSiteIsolationDescription[] =
    "Enables site isolation for content rendered inside <webview> tags. This "
    "increases security for Chrome Apps and WebUI pages that use <webview>.";

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

const char kSmoothScrollingName[] = "Smooth Scrolling";
const char kSmoothScrollingDescription[] =
    "Animate smoothly when scrolling page content.";

const char kSpeakOnMuteName[] = "Enable Speak On Mute Detection";
const char kSpeakOnMuteDescription[] =
    "Enable the speak-on-mute detector in CRAS. Reboot is required to let the "
    "flag take effect";

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

const char kStoragePressureEventName[] = "Enable storage pressure Event";
const char kStoragePressureEventDescription[] =
    "If enabled, Chrome will dispatch a DOM event, informing applications "
    "about storage pressure (low disk space)";

const char kSuggestionsWithSubStringMatchName[] =
    "Substring matching for Autofill suggestions";
const char kSuggestionsWithSubStringMatchDescription[] =
    "Match Autofill suggestions based on substrings (token prefixes) rather "
    "than just prefixes.";

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

const char kSyncAutofillWalletUsageDataName[] =
    "Sync Autofill Wallet Usage Data";
const char kSyncAutofillWalletUsageDataDescription[] =
    "When enabled, allows syncing of the autofill wallet usage data type.";

const char kSyncEnableHistoryDataTypeName[] = "Enable History sync data type";
const char kSyncEnableHistoryDataTypeDescription[] =
    "Enables the History sync data type instead of TypedURLs";

const char kSyncSandboxName[] = "Use Chrome Sync sandbox";
const char kSyncSandboxDescription[] =
    "Connects to the testing server for Chrome Sync.";

const char kSyncTrustedVaultPassphrasePromoName[] =
    "Enable promos for sync trusted vault passphrase.";
const char kSyncTrustedVaultPassphrasePromoDescription[] =
    "Enables promos for an experimental sync passphrase type, referred to as "
    "trusted vault.";

const char kSyncInvalidationsName[] = "Use Sync standalone invalidations";
const char kSyncInvalidationsDescription[] =
    "If enabled, Sync will use standalone invalidations instead of topic based "
    "invalidations (Wallet and Offer data types are enabled by a dedicated "
    "flag).";

const char kSyncInvalidationsWalletAndOfferName[] =
    "Use Sync standalone invalidations for Wallet and Offer";
const char kSyncInvalidationsWalletAndOfferDescription[] =
    "If enabled, Sync will use standalone invalidations for Wallet and Offer "
    "data types. Takes effect only when Sync standalone invalidations are "
    "enabled.";

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

const char kTabEngagementReportingName[] = "Tab Engagement Metrics";
const char kTabEngagementReportingDescription[] =
    "Tracks tab engagement and lifetime metrics.";

const char kTabGridLayoutAndroidName[] = "Tab Grid Layout";
const char kTabGridLayoutAndroidDescription[] =
    "Allows users to see their tabs in a grid layout in the tab switcher on "
    "phones.";

const char kCommerceCouponsName[] = "Coupon Annotations";
const char kCommerceCouponsDescription[] =
    "Allows users to view annotations for available coupons in the Tab "
    "Switching UI and on the Tab itself when a known coupon in available";

const char kCommerceDeveloperName[] = "Commerce developer mode";
const char kCommerceDeveloperDescription[] =
    "Allows users in the allowlist to enter the developer mode";

const char kCommerceMerchantViewerAndroidName[] = "Merchant Viewer";
const char kCommerceMerchantViewerAndroidDescription[] =
    "Allows users to view merchant trust signals on eligible pages.";

const char kTabGroupsAndroidName[] = "Tab Groups";
const char kTabGroupsAndroidDescription[] =
    "Allows users to create groups to better organize their tabs on phones.";

const char kTabGroupsContinuationAndroidName[] = "Tab Groups Continuation";
const char kTabGroupsContinuationAndroidDescription[] =
    "Allows users to access continuation features in Tab Group on phones.";

const char kTabGroupsUiImprovementsAndroidName[] = "Tab Groups UI Improvements";
const char kTabGroupsUiImprovementsAndroidDescription[] =
    "Allows users to access new features in Tab Group UI on phones.";

const char kTabToGTSAnimationAndroidName[] = "Enable Tab-to-GTS Animation";
const char kTabToGTSAnimationAndroidDescription[] =
    "Allows users to see an animation when entering or leaving the "
    "Grid Tab Switcher on phones.";

const char kTabGroupsNewBadgePromoName[] = "Tab Groups 'New' Badge Promo";
const char kTabGroupsNewBadgePromoDescription[] =
    "Causes a 'New' badge to appear on the entry point for creating a tab "
    "group in the tab context menu.";

const char kTabGroupsSaveName[] = "Tab Groups Save";
const char kTabGroupsSaveDescription[] =
    "Enables users to explicitly save and recall tab groups.";

const char kTabHoverCardImagesName[] = "Tab Hover Card Images";
const char kTabHoverCardImagesDescription[] =
    "Shows a preview image in tab hover cards, if tab hover cards are enabled.";

const char kTabOutlinesInLowContrastThemesName[] =
    "Tab Outlines in Low Contrast Themes";
const char kTabOutlinesInLowContrastThemesDescription[] =
    "Expands the range of situations in which tab outline strokes are "
    "displayed, improving accessiblity in dark and incognito mode.";

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

const char kThreadedScrollingName[] = "Threaded scrolling";
const char kThreadedScrollingDescription[] =
    "Threaded handling of scroll-related input events. Disabling this will "
    "force all such scroll events to be handled on the main thread. Note that "
    "this can dramatically hurt scrolling performance of most websites and is "
    "intended for testing purposes only.";

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

const char kTranslateAssistContentName[] = "Translate AssistContent";
const char kTranslateAssistContentDescription[] =
    "Enables populating translate details for the current page in "
    "AssistContent.";

const char kTranslateForceTriggerOnEnglishName[] =
    "Select which language model to use to trigger translate on English "
    "content";
const char kTranslateForceTriggerOnEnglishDescription[] =
    "Force the Translate Triggering on English pages experiment to be enabled "
    "with the selected language model active.";

const char kTranslateIntentName[] = "Translate intent";
const char kTranslateIntentDescription[] =
    "Enables an intent that allows Assistant to initiate a translation of the "
    "foreground tab.";

const char kTreatInsecureOriginAsSecureName[] =
    "Insecure origins treated as secure";
const char kTreatInsecureOriginAsSecureDescription[] =
    "Treat given (insecure) origins as secure origins. Multiple origins can be "
    "supplied as a comma-separated list. Origins must have their protocol "
    "specified e.g. \"http://example.com\". For the definition of secure "
    "contexts, see https://w3c.github.io/webappsec-secure-contexts/";

const char kPrivateStateTokensName[] = "Enable Private State Tokens";
const char kPrivateStateTokensDescription[] =
    "Enables the prototype Private State Token API "
    "(https://github.com/wicg/trust-token-api).";

const char kTurnOffStreamingMediaCachingOnBatteryName[] =
    "Turn off caching of streaming media to disk while on battery power.";
const char kTurnOffStreamingMediaCachingOnBatteryDescription[] =
    "Reduces disk activity during media playback, which can result in "
    "power savings.";

const char kTurnOffStreamingMediaCachingAlwaysName[] =
    "Turn off caching of streaming media to disk.";
const char kTurnOffStreamingMediaCachingAlwaysDescription[] =
    "Reduces disk activity during media playback, which can result in "
    "power savings.";

const char kUnifiedPasswordManagerAndroidName[] =
    "Google Mobile Services for passwords";
const char kUnifiedPasswordManagerAndroidDescription[] =
    "Uses Google Mobile Services to store and retrieve passwords."
    "Warning: Highly experimental. May lead to loss of passwords and "
    "impact performance.";

const char kUnifiedPasswordManagerErrorMessagesName[] =
    "Error Messages when using Google Mobile Services for passwords";
const char kUnifiedPasswordManagerErrorMessagesDescription[] =
    "Enables showing error messages that offer signing in again to fix the "
    "auth errors.";

const char kUnifiedPasswordManagerAndroidReenrollmentName[] =
    "Automatic reenrollement of users who were evicted from using Google "
    "Mobile Services after experiencing errors.";
const char kUnifiedPasswordManagerAndroidReenrollmentDescription[] =
    "Requires UnifiedPasswordManagerAndroid flag enabled. Allows automatic "
    "reenrollment into Google Mobile Services if sync and backend "
    "communication work.";

const char kUnsafeWebGPUName[] = "Unsafe WebGPU";
const char kUnsafeWebGPUDescription[] =
    "Enables access to the experimental WebGPU API. Warning: As GPU sandboxing "
    "isn't implemented yet for the WebGPU API, it is possible to read GPU data "
    "for other processes.";

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

const char kUsernameFirstFlowFallbackCrowdsourcingName[] =
    "Username first flow fallback crowdsourcing";
const char kUsernameFirstFlowFallbackCrowdsourcingDescription[] =
    "Support of sending additional votes on username first flow i.e. login "
    "flows where a user has to type username first on one page and then "
    "password on another page. These votes are sent on single password forms "
    "and contain information whether a 1-password form follows a 1-text form "
    "and the value's type(or pattern) in the latter (e.g. email-like, "
    "phone-like, arbitrary string).";

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

const char kV8VmFutureName[] = "Future V8 VM features";
const char kV8VmFutureDescription[] =
    "This enables upcoming and experimental V8 VM features. "
    "This flag does not enable experimental JavaScript features.";

const char kVariableCOLRV1Name[] = "Variable COLRv1 Fonts";
const char kVariableCOLRV1Description[] =
    "Enable rendering of COLRv1 glyphs with font variations applied. When this "
    "flag is off, variations to COLRv1 tables are ignored.";

const char kGlobalVaapiLockName[] = "Global lock on the VA-API wrapper.";
const char kGlobalVaapiLockDescription[] =
    "Enable or disable the global VA-API lock for platforms and paths that "
    "support controlling this.";

const char kVp9kSVCHWDecodingName[] =
    "Hardware decode acceleration for k-SVC VP9";
const char kVp9kSVCHWDecodingDescription[] =
    "Enable or disable k-SVC VP9 hardware decode acceleration";

const char kWalletServiceUseSandboxName[] =
    "Use Google Payments sandbox servers";
const char kWalletServiceUseSandboxDescription[] =
    "For developers: use the sandbox service for Google Payments API calls.";

const char kWallpaperFastRefreshName[] =
    "Enable shortened wallpaper daily refresh interval for manual testing";
const char kWallpaperFastRefreshDescription[] =
    "Allows developers to see a new wallpaper once every ten seconds rather "
    "than once per day when using the daily refresh feature.";

const char kWallpaperFullScreenPreviewName[] =
    "Enable wallpaper full screen preview UI";
const char kWallpaperFullScreenPreviewDescription[] =
    "Allows users to minimize all active windows to preview their current "
    "wallpaper";

const char kWallpaperGooglePhotosSharedAlbumsName[] =
    "Enable Google Photos shared albums for wallpaper";
const char kWallpaperGooglePhotosSharedAlbumsDescription[] =
    "Allow users to set shared Google Photos albums as the source for their "
    "wallpaper.";

const char kWallpaperPerDeskName[] =
    "Enable setting different wallpapers per desk";
const char kWallpaperPerDeskDescription[] =
    "Allow users to set different wallpapers on each of their active desks";

const char kWebBluetoothName[] = "Web Bluetooth";
const char kWebBluetoothDescription[] =
    "Enables the Web Bluetooth API on platforms without official support";

const char kWebBluetoothNewPermissionsBackendName[] =
    "Use the new permissions backend for Web Bluetooth";
const char kWebBluetoothNewPermissionsBackendDescription[] =
    "Enables the new permissions backend for Web Bluetooth. This will enable "
    "persistent storage of device permissions and Web Bluetooth features such "
    "as BluetoothDevice.watchAdvertisements() and Bluetooth.getDevices()";

const char kWebBundlesName[] = "Web Bundles";
const char kWebBundlesDescription[] =
    "Enables experimental supports for Web Bundles (Bundled HTTP Exchanges) "
    "navigation.";

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

const char kIgnoreCSPInWebPaymentAPIName[] = "Ignore CSP in Web Payment API";
const char kIgnoreCSPInWebPaymentAPIDescription[] =
    "Temporarily disable enforcing Content Security Policy connect-src "
    "directive for Web Payment API when fetching manifest files, app icons, "
    "and service worker JavaScript files.";

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

const char kWebrtcSrtpAesGcmName[] =
    "Negotiation with GCM cipher suites for SRTP in WebRTC";
const char kWebrtcSrtpAesGcmDescription[] =
    "When enabled, WebRTC will try to negotiate GCM cipher suites for SRTP.";

const char kWebrtcUseMinMaxVEADimensionsName[] =
    "WebRTC Min/Max Video Encode Accelerator dimensions";
const char kWebrtcUseMinMaxVEADimensionsDescription[] =
    "When enabled, WebRTC will only use the Video Encode Accelerator for "
    "video resolutions inside those published as supported.";

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
const char kWebXrRuntimeChoiceOpenXR[] = "OpenXR";

const char kWebXrIncubationsName[] = "WebXR Incubations";
const char kWebXrIncubationsDescription[] =
    "Enables experimental features for WebXR.";

const char kZeroCopyName[] = "Zero-copy rasterizer";
const char kZeroCopyDescription[] =
    "Raster threads write directly to GPU memory associated with tiles.";

const char kEnableVulkanName[] = "Vulkan";
const char kEnableVulkanDescription[] = "Use vulkan as the graphics backend.";

const char kSharedHighlightingAmpName[] = "Shared Highlighting for AMP Viewers";
const char kSharedHighlightingAmpDescription[] =
    "Enables Shared Highlighting for AMP Viwers.";

const char kSharedHighlightingManagerName[] = "Refactoring Shared Highlighting";
const char kSharedHighlightingManagerDescription[] =
    "Refactors Shared Highlighting by centralizing the IPC calls in a Manager.";

const char kSharedHighlightingRefinedBlocklistName[] =
    "Shared Highlighting Blocklist Refinement";
const char kSharedHighlightingRefinedBlocklistDescription[] =
    "Narrow the Blocklist for enabling Shared Highlighting.";

const char kSharedHighlightingRefinedMaxContextWordsName[] =
    "Shared Highlighting Max Context Words Refinement";
const char kSharedHighlightingRefinedMaxContextWordsDescription[] =
    "Experiment with different Max Context Words for Shared Highlighting.";

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

const char kDurableClientHintsCacheName[] = "Persistent client hints";
const char kDurableClientHintsCacheDescription[] =
    "Persist the client hints cache beyond browser restarts.";

const char kReduceUserAgentMinorVersionName[] =
    "Reduce the minor version in the User-Agent string";
const char kReduceUserAgentMinorVersionDescription[] =
    "Reduce the minor, build, and patch versions in the User-Agent string.  "
    "The Chrome version in the User-Agent string will be reported as "
    "Chrome/<major_version>.0.0.0.";

const char kReduceUserAgentPlatformOsCpuName[] =
    "Reduce the plaftform and oscpu in the desktop User-Agent string";
const char kReduceUserAgentPlatformOsCpuDescription[] =
    "Reduce the plaftform and oscpu in the desktop User-Agent string.  "
    "The platform and oscpu in the User-Agent string will be reported as "
    "<unifiedPlatform>";

const char kSkipServiceWorkerFetchHandlerName[] =
    "Skip Service Worker Fetch Handler if skippable";
const char kSkipServiceWorkerFetchHandlerDescription[] =
    "Skips starting the service worker and run the fetch handler if the fetch "
    "handler is recognized as skippable.";

const char kWebSQLAccessName[] = "Allows access to WebSQL APIs";
const char kWebSQLAccessDescription[] =
    "The WebSQL API is enabled by default, but can be disabled here.";

const char kUseIDNA2008NonTransitionalName[] =
    "Enable IDNA 2008 Non-Transitional Mode";
const char kUseIDNA2008NonTransitionalDescription[] =
    "Enables IDNA 2008 in Non-Transitional Mode in URL processing, allowing "
    "deviation characters in domain names.";

#if !BUILDFLAG(IS_CHROMEOS)
const char kDmTokenDeletionName[] = "DMToken deletion";
const char kDmTokenDeletionDescription[] =
    "Delete the corresponding DMToken when a managed browser is deleted in "
    "Chrome Browser Cloud Management.";
#endif  // !BUILDFLAG(IS_CHROMEOS)

// Android ---------------------------------------------------------------------

#if BUILDFLAG(IS_ANDROID)

const char kAccessibilityPageZoomName[] = "Accessibility Page Zoom";
const char kAccessibilityPageZoomDescription[] =
    "Whether the UI and underlying code for page zoom should be enabled to"
    " allow a user to increase/decrease the web contents zoom factor.";

const char kActivateMetricsReportingEnabledPolicyAndroidName[] =
    "Activate MetricsReportingEnabled policy on Android";
const char kActivateMetricsReportingEnabledPolicyAndroidDescription[] =
    " Allows admins to block metrics reporting by using "
    " MetricsReportingEnabled policy.";

const char kAddToHomescreenIPHName[] = "Add to homescreen IPH";
const char kAddToHomescreenIPHDescription[] =
    " Shows in-product-help messages educating users about add to homescreen "
    "option in chrome.";

const char kAImageReaderName[] = "Android ImageReader";
const char kAImageReaderDescription[] =
    " Enables MediaPlayer and MediaCodec to use AImageReader on Android. "
    " This feature is only available for android P+ devices. Disabling it also "
    " disables SurfaceControl.";

const char kAndroidForceAppLanguagePromptName[] =
    "Force second run app language prompt";
const char kAndroidForceAppLanguagePromptDescription[] =
    "When enabled the app language prompt to change the UI language will"
    "always be shown.";

const char kAndroidMediaPickerSupportName[] = "Android Media Picker";
const char kAndroidMediaPickerSupportDescription[] =
    "When enabled the Android Media picker is used instead of the Chrome one.";

const char kAndroidSurfaceControlName[] = "Android SurfaceControl";
const char kAndroidSurfaceControlDescription[] =
    " Enables SurfaceControl to manage the buffer queue for the "
    " DisplayCompositor on Android. This feature is only available on "
    " android Q+ devices";

const char kAndroidImprovedBookmarksName[] = "Android Visual Bookmark Manager";
const char kAndroidImprovedBookmarksDescription[] =
    "More visual changes to the bookmarks surfaces, with more thumbnails and a "
    "focus on search instead of folders/hierarchy";

const char kAnimatedImageDragShadowName[] =
    "Enable animated image drag shadow on Android.";
const char kAnimatedImageDragShadowDescription[] =
    "Animate the shadow image from its original bound to the touch point. "
    "Image drag on Android is available when flag touch-drag-and-context-menu "
    "is enabled.";

const char kAssistantIntentTranslateInfoName[] =
    "Translate info in Assistant intent";
const char kAssistantIntentTranslateInfoDescription[] =
    "Include page translation details in the Assistant voice transcription "
    "intent. This includes the page's URL and its original, current, and "
    "default target language.";

const char kAutofillAccessoryViewName[] =
    "Autofill suggestions as keyboard accessory view";
const char kAutofillAccessoryViewDescription[] =
    "Shows Autofill suggestions on top of the keyboard rather than in a "
    "dropdown.";

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

const char kBackGestureRefactorActivityAndroidName[] =
    "Back Gesture Refactor (Secondary Activities)";
const char kBackGestureRefactorActivityAndroidDescription[] =
    "Enable Back Gesture Refactor for Secondary Activities to support in-app "
    "activity-to-activity predictive back gestures";

const char kBackGestureRefactorAndroidName[] = "Back Gesture Refactor";
const char kBackGestureRefactorAndroidDescription[] =
    "Enable Back Gesture Refactor.";

const char kBindingManagerConnectionLimitName[] =
    "Binding manager connection limit";
const char kBindingManagerConnectionLimitDescription[] =
    "When enabled, limits the number of connections the binding manager can "
    "hold to background renderers. This results in lower priorities for the "
    "least important background renderers, which may cause them to be "
    "reclaimed by the OS if memory is needed.";

const char kBindingManagerUseNotPerceptibleBindingName[] =
    "Reduced background renderer binding";
const char kBindingManagerUseNotPerceptibleBindingDescription[] =
    "When enabled, uses a not perceptible binding for background renderers "
    "used in the current session if able.";

const char kCCTBrandTransparencyName[] =
    "Chrome Custom Tabs Brand Transparency";
const char kCCTBrandTransparencyDescription[] =
    "When enabled, CCT will show more Chrome branding information when start, "
    "giving user more transparency that the web page is running in Chrome.";

const char kCCTIncognitoName[] = "Chrome Custom Tabs Incognito mode";
const char kCCTIncognitoDescription[] =
    "Enables incognito mode for Chrome Custom Tabs, on Android.";

const char kCCTIncognitoAvailableToThirdPartyName[] =
    "Allow third party to open Custom Tabs Incognito mode";
const char kCCTIncognitoAvailableToThirdPartyDescription[] =
    "Enabling it would allow third party apps to open incognito mode for "
    "Chrome Custom Tabs, on Android.";

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

const char kCCTRetainingStateInMemoryName[] =
    "Enable restoring of previous Custom Tab session";
const char kCCTRetainingStateInMemoryDescription[] =
    "Enables ability to fully restore a recently engaged with Custom Tab.";
const char kCCTRealTimeEngagementSignalsName[] =
    "Enable CCT real-time engagement signals.";
const char kCCTRealTimeEngagementSignalsDescription[] =
    "Enables sending real-time engagement signals (e.g. scroll) through "
    "CustomTabsCallback.";

const char kChimeAlwaysShowNotificationDescription[] =
    "A debug flag to always show Chime notification after receiving a payload.";
const char kChimeAlwaysShowNotificationName[] =
    "Always show Chime notification";

const char kChimeAndroidSdkDescription[] =
    "Enable Chime SDK to receive push notification.";
const char kChimeAndroidSdkName[] = "Use Chime SDK";

const char kChromeShareLongScreenshotName[] = "Chrome Share Long Screenshots";
const char kChromeShareLongScreenshotDescription[] =
    "Enables UI to edit and share long screenshots on Android";

const char kChromeSharingCrowLaunchTabName[] =
    "Launch Thank Creator actions in standard tab";
const char kChromeSharingCrowLaunchTabDescription[] =
    "Launches Thank Creator actions in a traditional  new tab; default is a "
    "custom tab. For internal debugging.";

const char kChromeSharingHubLaunchAdjacentName[] =
    "Launch new share hub actions in adjacent window";
const char kChromeSharingHubLaunchAdjacentDescription[] =
    "In multi-window mode, launches share hub actions in an adjacent window. "
    "For internal debugging.";

const char kDiscardOccludedBitmapsName[] =
    "Discard occluded bitmaps in Android UI";
const char kDiscardOccludedBitmapsDescription[] =
    "Proactively discard cached bitmaps that are occluded/offscreen. Applies "
    "to several UI surfaces. May introduce jank if the bitmap is needed again.";

const char kCloseTabSuggestionsName[] = "Suggest to close Tabs";
const char kCloseTabSuggestionsDescription[] =
    "Suggests to the user to close Tabs that haven't been used beyond a "
    "configurable threshold or where duplicates of Tabs exist. "
    "The threshold is configurable.";

const char kCriticalPersistedTabDataName[] = "Enable CriticalPersistedTabData";
const char kCriticalPersistedTabDataDescription[] =
    "A new method of persisting Tab data across restarts has been devised "
    "and implemented. This actives the new approach.";

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

const char kAutofillManualFallbackAndroidName[] =
    "Enable Autofill manual fallback for Addresses and Payments (Android)";
const char kAutofillManualFallbackAndroidDescription[] =
    "If enabled, adds toggle for addresses and payments bottom sheet to the "
    "keyboard accessory.";

const char kEnableAutofillRefreshStyleName[] =
    "Enable Autofill refresh style (Android)";
const char kEnableAutofillRefreshStyleDescription[] =
    "Enable modernized style for Autofill on Android";

const char kEnableCommandLineOnNonRootedName[] =
    "Enable command line on non-rooted devices";
const char kEnableCommandLineOnNoRootedDescription[] =
    "Enable reading command line file on non-rooted devices (DANGEROUS).";

const char kEnablePasswordsAccountStorageName[] =
    "Enable the account data storage for passwords";
const char kEnablePasswordsAccountStorageDescription[] =
    "Enables storing passwords in a second, Gaia-account-scoped storage for "
    "signed-in but not syncing users";

const char kEnableOskResizesVisualViewportByDefaultName[] =
    "Enable OSK resizes visual viewport by default";
const char kEnableOskResizesVisualViewportByDefaultDescription[] =
    "Unless explicitly overriden by the page author, the Android "
    "On-Screen-Keyboard (OSK) will avoid changing the page height, resizing "
    "only the visual viewport.";

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

const char kFeedBottomSyncBannerName[] = "Show Sync Promotion";
const char kFeedBottomSyncBannerDescription[] =
    "Enables the Sync promo banner to be"
    " shown at bottom of Chrome Feed.";

const char kFeedHeaderStickToTopName[] = "Feed header stick to top";
const char kFeedHeaderStickToTopDescription[] =
    "Stick feed header to top on scroll.";

const char kFeedLoadingPlaceholderName[] = "Feed loading placeholder";
const char kFeedLoadingPlaceholderDescription[] =
    "Enables a placeholder UI in "
    "the feed instead of the loading spinner at first load.";

const char kFeedStampName[] = "StAMP cards in the feed";
const char kFeedStampDescription[] = "Enables StAMP cards in the feed.";

const char kFeedCloseRefreshName[] = "Feed-close refresh";
const char kFeedCloseRefreshDescription[] =
    "Enables scheduling a background refresh of the feed following feed use.";

const char kFeedVideoInlinePlaybackName[] = "Feed video inline playback";
const char kFeedVideoInlinePlaybackDescription[] =
    "Enable playing feed video in inline playback mode.";

const char kFeedDiscoFeedEndpointName[] =
    "Feed using the DiscoFeed backend endpoint";
const char kFeedDiscoFeedEndpointDescription[] =
    "Uses the DiscoFeed endpoint for serving the feed instead of GWS.";

const char kGridTabSwitcherForTabletsName[] = "Grid tab switcher for tablets";
const char kGridTabSwitcherForTabletsDescription[] =
    "Enable grid tab switcher for tablets, replacing the tab strip.";

const char kHighlightManagedPrefDisclaimerAndroidName[] =
    "Highlight managed disclaimer for preferences";
const char kHighlightManagedPrefDisclaimerAndroidDescription[] =
    "Enables new Settings UI on Android that makes it more prominent that a "
    "preference is managed by the administrator or a custodian.";

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

const char kMessagesForAndroidAdsBlockedName[] = "Ads Blocked Messages UI";
const char kMessagesForAndroidAdsBlockedDescription[] =
    "When enabled, ads blocked message will use the new Messages UI.";

const char kMessagesForAndroidInfrastructureName[] = "Messages infrastructure";
const char kMessagesForAndroidInfrastructureDescription[] =
    "When enabled, will initialize Messages UI infrastructure";

const char kMessagesForAndroidNotificationBlockedName[] =
    "Notification Blocked Messages UI";
const char kMessagesForAndroidNotificationBlockedDescription[] =
    "When enabled, notification blocked prompt will use the new Messages UI.";

extern const char kMessagesForAndroidOfferNotificationName[] =
    "Offer Notification Messages UI";
extern const char kMessagesForAndroidOfferNotificationDescription[] =
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

const char kNetworkServiceInProcessName[] =
    "Run the network service on the browser process";
const char kNetworkServiceInProcessDescription[] =
    "When enabled, the network service runs on the browser process. Otherwise, "
    "it runs on a dedicated process.";

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

const char kPageInfoStoreInfoName[] = "Page info store info";
const char kPageInfoStoreInfoDescription[] =
    "Enable a store info row to the page info menu on eligible pages.";

const char kPageInfoAboutThisSiteImprovedBottomSheetName[] =
    "Page info 'About this site' improved bottomsheet";
const char kPageInfoAboutThisSiteImprovedBottomSheetDescription[] =
    "Enable an improved 'About this site' bottomsheet, which includes a "
    "less cluttered toolbar view and opening navigations in a new tab.";

const char kPersistShareHubOnAppSwitchName[] = "Persist sharing hub";
const char kPersistShareHubOnAppSwitchDescription[] =
    "Persist the sharing hub across app pauses/resumes.";

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

const char kReadLaterReminderNotificationName[] =
    "Read later reminder notification";
const char kReadLaterReminderNotificationDescription[] =
    "Enables read later weekly reminder notification.";

const char kRecoverFromNeverSaveAndroidName[] =
    "UI to recover from never save passwords on Android";
const char kRecoverFromNeverSaveAndroidDescription[] =
    "Enables showing UI which allows for easy reverting of the decision to "
    "never save passwords on a certain webiste";

const char kReduceUserAgentAndroidVersionDeviceModelName[] =
    "Reduce the device model and Android version in the andriod User-Agent "
    "string";
const char kReduceUserAgentAndroidVersionDeviceModelDescription[] =
    "Enables reducing the device model and Android version in the andriod "
    "User-Agent string. The device model will be reported as `K` and the "
    "Android version will be reported as a static `10` string.";

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

const char kRelatedSearchesInBarName[] =
    "Enables showing Related Searches in the peeking bar.";
const char kRelatedSearchesInBarDescription[] =
    "Enables showing related searches suggestions in a carousel in the "
    "peeking bar of the bottom sheet on Android.";

const char kRelatedSearchesUiName[] =
    "Forces showing of the Related Searches UI on Android";
const char kRelatedSearchesUiDescription[] =
    "Forces the Related Searches UI and underlying requests to be enabled "
    "regardless of whether they are safe or useful. This requires the Related "
    "Searches feature flag to also be enabled.";

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

const char kRequestDesktopSiteExceptionsDowngradeName[] =
    "Downgrade request desktop site exceptions on Android.";
const char kRequestDesktopSiteExceptionsDowngradeDescription[] =
    "Downgrade request desktop site domain level settings to tab level "
    "settings.";

const char kRequestDesktopSiteExceptionsName[] =
    "Per-site setting to request desktop site on Android.";
const char kRequestDesktopSiteExceptionsDescription[] =
    "An option in `Site settings` to request the desktop version of websites "
    "based on site level settings.";

const char kRequestDesktopSitePerSiteIphName[] =
    "Contextual help for request desktop site site-level settings on Android.";
const char kRequestDesktopSitePerSiteIphDescription[] =
    "Provide in-product help for desktop site site-level settings.";

const char kRequestDesktopSiteZoomName[] =
    "Default zoom for request desktop site on Android.";
const char kRequestDesktopSiteZoomDescription[] =
    "Apply default page zoom on the desktop version of websites.";

const char kRevokeNotificationsPermissionIfDisabledOnAppLevelName[] =
    "Revoke site-level notification permission on Android";
const char kRevokeNotificationsPermissionIfDisabledOnAppLevelDescription[] =
    "Allow revoking site-level notification permission if Chrome has no "
    "app-level notification permission on Android.";

const char kSafeModeForCachedFlagsName[] = "Safe Mode for Cached Flags";
const char kSafeModeForCachedFlagsDescription[] =
    "Attempts recovery from startup crash loops caused by a bad field trial "
    "by rolling back to previous known safe flag values.";

const char kScreenshotsForAndroidV2Name[] = "Screenshots for Android V2";
const char kScreenshotsForAndroidV2Description[] =
    "Adds functionality to the share screenshot panel within Chrome Browser"
    " on Android";

const char kShowScrollableMVTOnNTPAndroidName[] = "Show scrollable MVT on NTP";
const char kShowScrollableMVTOnNTPAndroidDescription[] =
    "Enable showing the scrollable most visited tiles on NTP.";

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

const char kStartSurfaceRefactorName[] = "Start Surface Refactor";
const char kStartSurfaceRefactorDescription[] =
    "Enable splitting Tab switcher from Start surface";

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

const char kTabGroupsForTabletsName[] = "Tab groups on tablets";
const char kTabGroupsForTabletsDescription[] = "Enable tab groups on tablets.";

const char kTabStripImprovementsAndroidName[] =
    "Tab strip improvements for Android.";
const char kTabStripImprovementsAndroidDescription[] =
    "Enables scrollable tab strip with tab group indicators.";

const char kDiscoverFeedMultiColumnAndroidName[] =
    "Multi-column Discover feed Android.";
const char kDiscoverFeedMultiColumnAndroidDescription[] =
    "Enables single or multi-column Discover feed based on screen width on "
    "Android.";

const char kTabStripRedesignAndroidName[] = "Tab Strip Redesign Android.";
const char kTabStripRedesignAndroidDescription[] =
    "Enabled Tab Strip Redesign on Android - A visual redesign of Clank Tab "
    "Strip that is consistent with GM3.";

const char kFoldableJankFixAndroidName[] = "Fix for jank seen on foldables.";
const char kFoldableJankFixAndroidDescription[] =
    "Enables fix for jank seen during fold to unfold transition on foldables "
    "with Android 12+";

const char kBaselineGM3SurfaceColorsName[] = "Baseline GM3 Surface Colors";
const char kBaselineGM3SurfaceColorsDescription[] =
    "Updates baseline surface colors to match the GM3 formula.";

const char kTouchDragAndContextMenuName[] =
    "Simultaneous touch drag and context menu";
const char kTouchDragAndContextMenuDescription[] =
    "Enables touch dragging and a context menu to start simultaneously, with"
    "the assumption that the menu is non-modal.";

const char kTranslateMessageUIName[] = "Translate Message UI";
const char kTranslateMessageUIDescription[] =
    "Controls whether the Translate Message UI will be shown instead of the "
    "Translate InfoBar.";

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

const char kUseRealColorSpaceForAndroidVideoName[] =
    "Use color space from MediaCodec";
const char kUseRealColorSpaceForAndroidVideoDescription[] =
    "When enabled video will use real color space instead of srgb.";

const char kVideoTutorialsName[] = "Enable video tutorials";
const char kVideoTutorialsDescription[] = "Show video tutorials in Chrome";
const char kVideoTutorialsInstantFetchName[] =
    "Video tutorials fetch on startup";
const char kVideoTutorialsInstantFetchDescription[] =
    "Fetch video tutorials on startup";

const char kAdaptiveButtonInTopToolbarName[] = "Adaptive button in top toolbar";
const char kAdaptiveButtonInTopToolbarDescription[] =
    "Enables showing an adaptive action button in the top toolbar";
const char kAdaptiveButtonInTopToolbarCustomizationName[] =
    "Adaptive button in top toolbar customization";
const char kAdaptiveButtonInTopToolbarCustomizationDescription[] =
    "Enables UI for customizing the adaptive action button in the top toolbar";
const char kVoiceButtonInTopToolbarName[] = "Voice button in top toolbar";
const char kVoiceButtonInTopToolbarDescription[] =
    "Enables showing the voice search button in the top toolbar. Enabling "
    "Adaptive Button overrides this.";

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

const char kXsurfaceMetricsReportingName[] = "Xsurface Metrics Reporting";
const char kXsurfaceMetricsReportingDescription[] =
    "Allows metrics reporting state to be passed to Xsurface";

const char kWebNotesPublishName[] = "WebNotes Publish";
const char kWebNotesPublishDescription[] =
    "Allows users to save their created notes.";

const char kWebNotesDynamicTemplatesName[] = "Dynamic Templates";
const char kWebNotesDynamicTemplatesDescription[] =
    "Allows templates to be modified remotely on short notice.";

const char kPasswordEditDialogWithDetailsName[] =
    "Password edit dialog with details UI";
const char kPasswordEditDialogWithDetailsDescription[] =
    "Enables UI which shows the dialog after clicking on save/update password"
    " with the functionality to choose user account and edit the password.";

const char kEnableAndroidGamepadVibrationName[] = "Gamepad vibration";
const char kEnableAndroidGamepadVibrationDescription[] =
    "Enables the ability to play vibration effects on supported gamepads.";

const char kCormorantName[] = "Cormorant";
const char kCormorantDescription[] = "Enables the experimental Cormorant UI.";

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

extern const char kBatterySaverModeAvailableName[] =
    "Enable the battery saver mode feature in the settings";
extern const char kBatterySaverModeAvailableDescription[] =
    "When enabled, shows the battery section in the performance settings page";

const char kCopyLinkToTextName[] = "Copy Link To Text";
const char kCopyLinkToTextDescription[] =
    "Adds an item to the context menu to allow a user to copy a link to the "
    "page with the selected text highlighted.";

const char kDesktopPartialTranslateName[] = "Desktop Partial Translate";
const char kDesktopPartialTranslateDescription[] =
    "Enables the Partial Translate feature on Desktop, which allows users to "
    "translate text selections on the page through the right click context "
    "menu.";

const char kEnableAccessibilityLiveCaptionName[] = "Live Caption";
const char kEnableAccessibilityLiveCaptionDescription[] =
    "Enables the live caption feature which generates captions for "
    "media playing in Chrome. Turn the feature on in "
    "chrome://settings/accessibility.";

const char kReadAnythingName[] = "Read Anything";
const char kReadAnythingDescription[] =
    "Enables the Read Anything feature which generates a reader-friendly view "
    "of web pages. Open the side panel and select Read Anything to try the "
    "feature.";

const char kReadAnythingWithScreen2xName[] = "Read Anything with Screen2x";
const char kReadAnythingWithScreen2xDescription[] =
    "Have Read Anything use a local machine learning model for web page"
    "distillation.";

const char kEnableUserCloudSigninRestrictionPolicyName[] =
    "Cloud User level Signin Restrictions Policy";
const char kEnableUserCloudSigninRestrictionPolicyDescription[] =
    "Enable the ManagedAccountsSigninRestrictions policy to be set at a cloud "
    "user level";

const char kEnableWebHidOnExtensionServiceWorkerName[] =
    "Enable WebHID on extension service workers";
const char kEnableWebHidOnExtensionServiceWorkerDescription[] =
    "When enabled, WebHID API is available on extension service workers.";

const char kGlobalMediaControlsCastStartStopName[] =
    "Global media controls control Cast start/stop";
const char kGlobalMediaControlsCastStartStopDescription[] =
    "Allows global media controls to control when a Cast session is started "
    "or stopped instead of relying on the Cast dialog.";

extern const char kHighEfficiencyModeAvailableName[] =
    "Enable the high efficiency mode feature in the settings";
extern const char kHighEfficiencyModeAvailableDescription[] =
    "When enabled, shows the performance section in the performance settings "
    "page";

const char kMuteNotificationSnoozeActionName[] =
    "Snooze action for mute notifications";
const char kMuteNotificationSnoozeActionDescription[] =
    "Adds a Snooze action to mute notifications shown while sharing a screen.";

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

const char kNtpComprehensiveThemingName[] = "NTP Comprehensive Theming";
const char kNtpComprehensiveThemingDescription[] =
    "Applies theme based colors across various NTP elements";

const char kNtpDesktopLensName[] = "NTP Desktop Lens Entrypoint";
const char kNtpDesktopLensDescription[] =
    "Shows a Lens entrypoint and upload dialog in desktop NTP when enabled.";

const char kNtpDriveModuleName[] = "NTP Drive Module";
const char kNtpDriveModuleDescription[] =
    "Shows the Google Drive module on the New Tab Page";

#if !defined(OFFICIAL_BUILD)
const char kNtpDummyModulesName[] = "NTP Dummy Modules";
const char kNtpDummyModulesDescription[] =
    "Adds dummy modules to New Tab Page when 'NTP Modules Redesigned' is "
    "enabled.";
#endif

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

const char kNtpModulesRedesignedLayoutName[] = "Ntp Modules Redesigned Layout";
const char kNtpModulesRedesignedLayoutDescription[] =
    "Changes the layout of modules on New Tab Page";

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

const char kNtpRealboxMatchOmniboxThemeName[] =
    "NTP Realbox Matches Omnibox Theme";
const char kNtpRealboxMatchOmniboxThemeDescription[] =
    "NTP Realbox matches the Omnibox theme when enabled.";

const char kNtpRealboxMatchSearchboxThemeName[] =
    "NTP Realbox Matches Searchbox Theme";
const char kNtpRealboxMatchSearchboxThemeDescription[] =
    "Makes NTP Realbox drop shadow match that of the Searchbox when enabled.";

const char kNtpRealboxPedalsName[] = "NTP Realbox Pedals";
const char kNtpRealboxPedalsDescription[] =
    "Shows pedals in the NTP Realbox when enabled.";

const char kNtpRealboxRoundedCornersName[] = "NTP Realbox Rounded Corners";
const char kNtpRealboxRoundedCornersDescription[] =
    "Make NTP realbox dropdown corners match that of the Searchbox.";

const char kNtpRealboxUseGoogleGIconName[] = "NTP Realbox Google G Icon";
const char kNtpRealboxUseGoogleGIconDescription[] =
    "Shows Google G icon "
    "instead of Search Loupe in realbox when enabled";

const char kNtpRecipeTasksModuleName[] = "NTP Recipe Tasks Module";
const char kNtpRecipeTasksModuleDescription[] =
    "Shows the recipe tasks module on the New Tab Page.";

const char kNtpSafeBrowsingModuleName[] = "NTP Safe Browsing Module";
const char kNtpSafeBrowsingModuleDescription[] =
    "Shows the safe browsing module on the New Tab Page.";

const char kEnableReaderModeName[] = "Enable Reader Mode";
const char kEnableReaderModeDescription[] =
    "Allows viewing of simplified web pages by selecting 'Customize and "
    "control Chrome'>'Distill page'";

const char kHappinessTrackingSurveysForDesktopDemoName[] =
    "Happiness Tracking Surveys Demo";
const char kHappinessTrackingSurveysForDesktopDemoDescription[] =
    "Enable showing Happiness Tracking Surveys Demo to users on Desktop";

const char kLayoutExtractionName[] = "Layout Extraction";
const char kLayoutExtractionDescription[] =
    "Enables Layout Extraction local machine intelligence library to use "
    "screen snapshots to add metadata for accessibility tools.";

const char kOmniboxDriveSuggestionsName[] =
    "Omnibox Google Drive Document suggestions";
const char kOmniboxDriveSuggestionsDescription[] =
    "Display suggestions for Google Drive documents in the omnibox when Google "
    "is the default search engine.";

const char kOmniboxExperimentalKeywordModeName[] =
    "Omnibox Experimental Keyword Mode";
const char kOmniboxExperimentalKeywordModeDescription[] =
    "Enables various experimental features related to keyword mode, its "
    "suggestions and layout.";

const char kPasswordsGroupingName[] = "Passwords grouping";
const char kPasswordsGroupingDescription[] =
    "Enables displaying passwords grouped together using information from "
    "the affiliation service.";

const char kPasswordManagerRedesignName[] = "Password Manager new UI";
const char kPasswordManagerRedesignDescription[] =
    "Enables new Password Manager UI on Desktop";

const char kSCTAuditingName[] = "SCT auditing";
const char kSCTAuditingDescription[] =
    "Enables SCT auditing for users who have opted in to Safe Browsing "
    "Extended Reporting.";

const char kSharingDesktopScreenshotsEditName[] =
    "Desktop Screenshots Edit Mode";
const char kSharingDesktopScreenshotsEditDescription[] =
    "Enables an edit flow for users who create screenshots on desktop";

const char kSharingDesktopSharePreviewName[] = "Desktop share hub preview";
const char kSharingDesktopSharePreviewDescription[] =
    "Adds a preview section to the desktop sharing hub to make it clearer what "
    "is about to be shared.";

const char kSharingDesktopShareToGoogleCollectionsName[] =
    "Share to Google Collections";
const char kSharingDesktopShareToGoogleCollectionsDescription[] =
    "Adds an item to the sharing hub to allow sharing to Google Collections.";

const char kWebAppManifestImmediateUpdatingName[] =
    "Web App Manifest Immediate Updating";
const char kWebAppManifestImmediateUpdatingDescription[] =
    "Enables web app manifest updates to apply to running web app windows as "
    "soon as a change has been detected instead of waiting for all app windows "
    "to be closed.";

const char kWebAuthenticationPermitEnterpriseAttestationName[] =
    "Web Authentication Enterprise Attestation";
const char kWebAuthenticationPermitEnterpriseAttestationDescription[] =
    "Permit a set of origins to request a uniquely identifying enterprise "
    "attestation statement from a security key when creating a Web "
    "Authentication credential.";

const char kDevToolsTabTargetLiteralName[] = "DevTools using Tab Target";
const char kDevToolsTabTargetLiteralDescription[] =
    "Makes DevTools use an experimental CDP Tab target.";

const char kAccessCodeCastTabSwitchingUIName[] =
    "Access Code Cast Tab Switching UI";
const char kAccessCodeCastTabSwitchingUIDescription[] =
    "Enables tab switching during an Access Code Casting session.";

#endif  // BUILDFLAG(IS_ANDROID)

// Windows ---------------------------------------------------------------------

#if BUILDFLAG(IS_WIN)

const char kCalculateNativeWinOcclusionName[] =
    "Calculate window occlusion on Windows";
const char kCalculateNativeWinOcclusionDescription[] =
    "Calculate window occlusion on Windows will be used in the future "
    "to throttle and potentially unload foreground tabs in occluded windows";

const char kCloudApAuthName[] = "CloudAP authentication";
const char kCloudApAuthDescription[] =
    "Enables ambient authentication into supported identity providers using "
    "the CloudAP framework. Credentials are retrieved from accounts signed "
    "into the operating system.";

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

const char kMediaFoundationClearName[] = "MediaFoundation for Clear";
const char kMediaFoundationClearDescription[] =
    "Enable/Disable the use of MediaFoundation for non-protected content "
    "playback on supported systems.";

const char kRawAudioCaptureName[] = "Raw audio capture";
const char kRawAudioCaptureDescription[] =
    "Enable/Disable the usage of WASAPI raw audio capture. When enabled, the "
    "audio stream is a 'raw' stream that bypasses all signal processing except "
    "for endpoint specific, always-on processing in the Audio Processing Object"
    " (APO), driver, and hardware.";

const char kFakeAudioCaptureTimestamps[] = "Fake audio capture timestamps";
const char kFakeAudioCaptureTimestampsDescription[] =
    "Emulates audio capture timestamps instead of using timestamps from the "
    "actual audio device. Can be used as a temporary workaround in situations "
    "where the selected audio device generates invalid timestamp sequences.";

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
extern const char kReadPrinterCapabilitiesWithXpsDescription[] =
    "When enabled, utilize XPS interface to read printer capabilities.";

const char kUseXpsForPrintingName[] = "Use XPS for printing";
const char kUseXpsForPrintingDescription[] =
    "When enabled, use XPS printing API instead of the GDI print API.";

const char kUseXpsForPrintingFromPdfName[] = "Use XPS for printing from PDF";
const char kUseXpsForPrintingFromPdfDescription[] =
    "When enabled, use XPS printing API instead of the GDI print API when "
    "printing PDF documents.";
#endif  // BUILDFLAG(ENABLE_PRINTING)

const char kWin10TabSearchCaptionButtonName[] =
    "Windows 10 Tab Search Caption Button";
const char kWin10TabSearchCaptionButtonDescription[] =
    "Move the Tab Search entrypoint besides the window caption buttons on "
    "Windows 10 platforms.";
#endif  // BUILDFLAG(IS_WIN)

// Mac -------------------------------------------------------------------------

#if BUILDFLAG(IS_MAC)

const char kBiometricAuthenticationInSettingsName[] =
    "Biometric authentication in settings";
const char kBiometricAuthenticationInSettingsDescription[] =
    "Enables biometric authentication in settings to view/edit/copy a password";

#if BUILDFLAG(ENABLE_PRINTING)
const char kCupsIppPrintingBackendName[] = "CUPS IPP Printing Backend";
const char kCupsIppPrintingBackendDescription[] =
    "Use the CUPS IPP printing backend instead of the original CUPS backend "
    "that calls the PPD API.";
#endif  // BUILDFLAG(ENABLE_PRINTING)

const char kEnableUniversalLinksName[] = "Universal Links";
const char kEnableUniversalLinksDescription[] =
    "Include Universal Links in the intent picker.";

const char kImmersiveFullscreenName[] = "Immersive Fullscreen Toolbar";
const char kImmersiveFullscreenDescription[] =
    "Automatically hide and show the toolbar in fullscreen.";

const char kMacSyscallSandboxName[] = "Mac Syscall Filtering Sandbox";
const char kMacSyscallSandboxDescription[] =
    "Controls whether the macOS sandbox filters syscalls.";

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

const char kSystemColorChooserName[] = "System Color Chooser";
const char kSystemColorChooserDescription[] =
    "Enables a button that launches the macOS native color chooser.";

#endif

// Windows and Mac -------------------------------------------------------------

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

const char kUseAngleName[] = "Choose ANGLE graphics backend";
const char kUseAngleDefault[] = "Default";
const char kUseAngleGL[] = "OpenGL";

const char kBiometricAuthenticationForFillingName[] =
    "Biometric authentication reauth before filling";
const char kBiometricAuthenticationForFillingDescription[] =
    "Enables biometric authentication before filling form data";
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

const char kAllowRepeatedUpdatesName[] =
    "Continue checking for updates before reboot and after initial update.";
const char kAllowRepeatedUpdatesDescription[] =
    "Continues checking to see if there is a more recent update, even if user"
    "has not rebooted to apply the previous update.";

const char kAllowScrollSettingsName[] =
    "Allow changes to scroll acceleration/sensitivity for mice/touchpads.";
const char kAllowScrollSettingsDescription[] =
    "Shows settings to enable/disable scroll acceleration and to adjust the "
    "sensitivity for scrolling.";

const char kAlwaysEnableHdcpName[] = "Always enable HDCP for external displays";
const char kAlwaysEnableHdcpDescription[] =
    "Enables the specified type for HDCP whenever an external display is "
    "connected. By default, HDCP is only enabled when required.";
const char kAlwaysEnableHdcpDefault[] = "Default";
const char kAlwaysEnableHdcpType0[] = "Type 0";
const char kAlwaysEnableHdcpType1[] = "Type 1";

const char kAmbientSubpageUIChangeName[] =
    "Show the revised UI for ambient mode subpage in Personalization Hub";
const char kAmbientSubpageUIChangeDescription[] =
    "Implement the new UI design for the ambient mode subpage in "
    "Personalization Hub.";

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

extern const char kArcFixupWindowName[] =
    "Enable ARC Ghost Window for ARC App fixup";
extern const char kArcFixupWindowDescription[] =
    "Popup ARC Ghost Window app launching before fixup finishes.";

extern const char kArcGhostWindowNewStyleName[] =
    "Enable new UI for ARC Ghost Window";
extern const char kArcGhostWindowNewStyleDescription[] =
    "Allow use new UI for ARC Ghost Window if corresponding launch source "
    "request it.";

const char kArcGameModeName[] = "Enable Game Mode for ARC";
const char kArcGameModeDescription[] =
    "ARC Fullscreen Games will request accomodation from ChromeOS for "
    "sustained performance.";

extern const char kArcInstantResponseWindowOpenName[] =
    "Enable Instance Response for ARC app window open";
extern const char kArcInstantResponseWindowOpenDescription[] =
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

const char kArcUpdateO4CListViaA2C2Name[] = "Update the O4C list via A2C2";
const char kArcUpdateO4CListViaA2C2Desc[] =
    "When enabled, the O4C list is allowed to be updated via A2C2.";

const char kArcEnableAAudioMMAPName[] = "Enable ARCVM AAudio MMAP";
const char kArcEnableAAudioMMAPDescription[] =
    "Enable AAudio MMAP support for ARCVM which provides low latency audio "
    "for supported apps.";

const char kArcEnableUsapName[] =
    "Enable ARC Unspecialized Application Processes";
const char kArcEnableUsapDesc[] =
    "Enable ARC Unspecialized Application Processes when applicable for "
    "high-memory devices.";

const char kArcEnableVirtioBlkForDataName[] =
    "Enable virtio-blk for ARCVM /data";
const char kArcEnableVirtioBlkForDataDesc[] =
    "If enabled, ARCVM uses virtio-blk for /data in Android storage.";

const char kAshEnableUnifiedDesktopName[] = "Unified desktop mode";
const char kAshEnableUnifiedDesktopDescription[] =
    "Enable unified desktop mode which allows a window to span multiple "
    "displays.";

const char kAudioFlexibleLoopbackName[] =
    "ChromeOS flexible loopback API support";
const char kAudioFlexibleLoopbackDescription[] =
    "Enable flexible loopback API support in ChromeOS.";

const char kAudioHFPMicSRName[] =
    "Audio super-resolution Bluetooth HFP microphone";
const char kAudioHFPMicSRDescription[] =
    "Enable super-resolution Bluetooth HFP microphone recording.";

const char kAudioSettingsPageName[] = "Audio Settings Page";
const char kAudioSettingsPageDescription[] =
    "Enables the dedicated Audio Settings Page in system settings, which "
    "allows for greater audio configuration.";

const char kAudioUrlName[] = "Enable chrome://audio";
const char kAudioUrlDescription[] =
    "Enable chrome://audio that is designed for debugging ChromeOS audio "
    "issues";

const char kAutoFramingOverrideName[] = "Auto-framing control override";
const char kAutoFramingOverrideDescription[] =
    "Overrides the default to forcibly enable or disable the auto-framing "
    "feature";

const char kAutocompleteExtendedSuggestionsName[] =
    "Extended suggestions for CrOS autocomplete";
const char kAutocompleteExtendedSuggestionsDescription[] =
    "Enables extended autocomplete suggestions experiment on ChromeOS.";

const char kAutocorrectByDefaultName[] = "CrOS autocorrect by default";
const char kAutocorrectByDefaultDescription[] =
    "Enables autocorrect by default experiment on ChromeOS";

const char kAutocorrectParamsTuningName[] = "CrOS autocorrect params tuning";
const char kAutocorrectParamsTuningDescription[] =
    "Enables params tuning experiment for autocorrect on ChromeOS.";

const char kAutocorrectToggleName[] = "CrOS autocorrect toggle";
const char kAutocorrectToggleDescription[] =
    "Enables using a toggle for enabling autocorrect on ChromeOS.";

const char kAvatarsCloudMigrationName[] =
    "Loading CrOS avatar images from the cloud";
const char kAvatarsCloudMigrationDescription[] =
    "Enables loading avatar images from the cloud on ChromeOS.";

const char kBluetoothFixA2dpPacketSizeName[] = "Bluetooth fix A2DP packet size";
const char kBluetoothFixA2dpPacketSizeDescription[] =
    "Fixes Bluetooth A2DP packet size to a smaller default value to improve "
    "audio quality and may fix audio stutter.";

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

const char kBluetoothUseLLPrivacyName[] = "Enable LL Privacy in BlueZ";
const char kBluetoothUseLLPrivacyDescription[] =
    "Enable address resolution offloading to Bluetooth Controller if "
    "supported. Modifying this flag will cause Bluetooth Controller to reset.";

const char kCalendarJellyName[] = "Enable Calendar Jelly features";
const char kCalendarJellyDescription[] =
    "Enables Jelly changes for the sys tray Calendar views.";

const char kCalendarViewName[] =
    "Productivity experiment: Monthly Calendar View";
const char kCalendarViewDescription[] =
    "Show Monthly Calendar View with Google Calendar events to increase "
    "productivity by helping users view their schedules more quickly.";

const char kCaptureModeDemoToolsName[] =
    "Enable demo tools feature in screen capture";
const char kCaptureModeDemoToolsDescription[] =
    "Enables the ability to show clicks and keys during video recordings to "
    "enhance demo experience.";

const char kCalendarModelDebugModeName[] = "Monthly Calendar Model Debug Mode";
const char kCalendarModelDebugModeDescription[] =
    "Debug mode for Monthly Calendar Model. This helps a lot in diagnosing any "
    "bugs in the calendar's event fetching/caching functionality. WARNING: DO "
    "NOT enable this flag unless you're OK with information about your "
    "calendar events, such as start/end times and summaries, being dumped to "
    "the system logs, where they are potentially visible to all users of the "
    "device.";

const char kDefaultLinkCapturingInBrowserName[] =
    "Default link capturing in the browser";
const char kDefaultLinkCapturingInBrowserDescription[] =
    "When enabled, newly installed apps will not capture links clicked in the "
    "browser.";

extern const char kDesks16Name[] = "Enable up to 16 virtual desks";
extern const char kDesks16Description[] =
    "When enabled, up to 16 virtual desks are allowed.";

const char kDesksSaveAndRecallName[] = "Desks Save and Recall";
const char kDesksSaveAndRecallDescription[] =
    "Save a desk and its applications so that they can be recalled at a later "
    "time.";

const char kDesksTemplatesName[] = "Desk Templates";
const char kDesksTemplatesDescription[] =
    "Streamline workflows by saving a group of applications and windows as a "
    "launchable template in a new desk";

extern const char kDnsOverHttpsWithIdentifiersReuseOldPolicyName[] =
    "Experiment: Allows using identifiers in the DoH template URI";
extern const char kDnsOverHttpsWithIdentifiersReuseOldPolicyDescription[] =
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

const char kHdrNetOverrideName[] = "HDRnet control override";
const char kHdrNetOverrideDescription[] =
    "Overrides the default to forcibly enable or disable the HDRnet feature";

const char kCellularBypassESimInstallationConnectivityCheckName[] =
    "Bypass eSIM installation connectivity check";
const char kCellularBypassESimInstallationConnectivityCheckDescription[] =
    "Bypass the non-cellular internet connectivity check during eSIM "
    "installation.";

const char kCellularUseSecondEuiccName[] = "Use second Euicc";
const char kCellularUseSecondEuiccDescription[] =
    "When enabled Cellular Setup and Settings UI will use the second available "
    "Euicc that's exposed by Hermes.";

const char kClipboardHistoryReorderName[] =
    "Keep the most recently pasted item at the top of clipboard history";
const char kClipboardHistoryReorderDescription[] =
    "Enables an experimental behavior change where each time a clipboard "
    "history item is pasted, that item shifts to the top of the list.";

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

const char kCrostiniImeSupportName[] = "Crostini IME support";
const char kCrostiniImeSupportDescription[] =
    "Experimental support for IMEs (excluding VK) on Crostini.";

const char kCrostiniVirtualKeyboardSupportName[] =
    "Crostini Virtual Keyboard Support";
const char kCrostiniVirtualKeyboardSupportDescription[] =
    "Experimental support for the Virtual Keyboard on Crostini.";

const char kBruschettaName[] = "Enable the third party VMs feature";
const char kBruschettaDescription[] =
    "Enables UI support for third party/generic VMs";

const char kBruschettaAlphaMigrateName[] = "Migration for Bruschetta Alpha";
const char kBruschettaAlphaMigrateDescription[] =
    "Enable this flag to migrate a Bruschetta installed during the alpha. "
    "Requires the bruschetta flag to be enabled.";

extern const char kCameraAppLowStorageWarningName[] =
    "Camera App Low Storage Warning";
extern const char kCameraAppLowStorageWarningDescription[] =
    "Enable this flag to show warning and handle the video recording when "
    "device storage is running low";

const char kCryptAuthV2DedupDeviceLastActivityTimeName[] =
    "Dedup devices by last activity time";
const char kCryptAuthV2DedupDeviceLastActivityTimeDescription[] =
    "Deduplicates phones in multi-device setup drop-down list by last "
    "activity time";

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

const char kFastPairName[] = "Enable Fast Pair";
const char kFastPairDescription[] =
    "Enables Google Fast Pair service which uses BLE to discover supported "
    "nearby Bluetooth devices and surfaces a notification for quick pairing.";

const char kFastPairHandshakeRefactorName[] =
    "Enable Fast Pair Handshake Refactor";
const char kFastPairHandshakeRefactorDescription[] =
    "Enables refactored handshake logic for Google Fast Pair service.";

const char kFastPairLowPowerName[] = "Enable Fast Pair Low Power mode";
const char kFastPairLowPowerDescription[] =
    "Enables Fast Pair Low Power mode, which doesn't scan for devices "
    "continously. This results in lower power usage, but also higher latency "
    "for device discovery.";

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

const char kDriveFsChromeNetworkingName[] =
    "Enable the DriveFS / Chrome Network Service bridge";
const char kDriveFsChromeNetworkingDescription[] =
    "Enable the bridge bewteen DriveFS and the Chrome Network Service for "
    "communication with the Drive backend.";

const char kEnableAppReinstallZeroStateName[] =
    "Enable Zero State App Reinstall Suggestions.";
const char kEnableAppReinstallZeroStateDescription[] =
    "Enable Zero State App Reinstall Suggestions feature in launcher, which "
    "will show app reinstall recommendations at end of zero state list.";

const char kEnableArcHostVpnName[] =
    "Enable ArcHostVpn for builtin VPN clients";
const char kEnableArcHostVpnDescription[] =
    "When a builtin VPN client is started, also start the ArcHostVpn service "
    "to reflect the builtin VPN config and VPN network inside ARC.";

const char kEnableAssistantRoutinesName[] = "Assistant Routines";
const char kEnableAssistantRoutinesDescription[] = "Enable Assistant Routines.";

const char kEnableBackgroundBlurName[] = "Enable background blur.";
const char kEnableBackgroundBlurDescription[] =
    "Enables background blur for the Launcher, Shelf, Unified System Tray etc.";

const char kEnhancedClipboardNudgeSessionResetName[] =
    "Enable resetting enhanced clipboard nudge data";
const char kEnhancedClipboardNudgeSessionResetDescription[] =
    "When enabled, this will reset the clipboard nudge shown data on every new "
    "user session, allowing the nudge to be shown again.";

const char kEnableCrOSActionRecorderName[] = "Enable CrOS action recorder";
const char kEnableCrOSActionRecorderDescription[] =
    "When enabled, each app launching, file opening, setting change, and url "
    "visiting will be logged locally into an encrypted file. Should not be "
    "enabled. Be aware that hash option only provides a thin layer of privacy.";

const char kDisableDnsProxyName[] = "Disable DNS proxy service for ChromeOS";
const char kDisableDnsProxyDescription[] =
    "Turns off DNS proxying and SecureDNS for ChromeOS (only). Does not impact "
    "Chrome browser.";

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

const char kEnableHeuristicStylusPalmRejectionName[] =
    "Enable Heuristic for Stylus/Palm Rejection.";
const char kEnableHeuristicStylusPalmRejectionDescription[] =
    "Enable additional heuristic palm rejection logic when interacting with "
    "stylus usage. Not intended for all devices.";

const char kEnableInputEventLoggingName[] = "Enable input event logging";
const char kEnableInputEventLoggingDescription[] =
    "Enable detailed logging of input events from touchscreens, touchpads, and "
    "mice. These events include the locations of all touches as well as "
    "relative pointer movements, and so may disclose sensitive data. They "
    "will be included in feedback reports and system logs, so DO NOT ENTER "
    "SENSITIVE INFORMATION with this flag enabled.";

const char kEnableInputInDiagnosticsAppName[] =
    "Enable input device cards in the Diagnostics App";
const char kEnableInputInDiagnosticsAppDescription[] =
    "Enable input device cards in the Diagnostics App";

const char kEnableKeyboardBacklightToggleName[] =
    "Enable Keyboard Backlight Toggle.";
const char kEnableKeyboardBacklightToggleDescription[] =
    "Enable toggling of the keyboard backlight. By "
    "default, this flag is enabled.";

const char kEnableLogControllerForDiagnosticsAppName[] =
    "Enable DiagnosticsLogController for Diagnostics App";
const char kEnableLogControllerForDiagnosticsAppDescription[] =
    "Uses DiagnosticsLogController to manage the lifetime of Diagnostics App "
    "logs.  Enables creation of combined diagnostics log after Diagnostics "
    "App is closed.";

const char kEnableNeuralPalmAdaptiveHoldName[] = "Palm Rejection Adaptive Hold";
const char kEnableNeuralPalmAdaptiveHoldDescription[] =
    "Enable adaptive hold in palm rejection.  Not compatible with all devices.";

const char kEnableNeuralStylusPalmRejectionName[] =
    "Enable Neural Palm Detection";
const char kEnableNeuralStylusPalmRejectionDescription[] =
    "Experimental: Enable Neural Palm detection. Not compatible with all "
    "devices.";

const char kEnableOsFeedbackName[] = "Enable updated Feedback Tool App";
const char kEnableOsFeedbackDescription[] =
    "Enable the feedback tool with new UX design that helps users mitigate "
    "the issues while writing feedback and makes the UI easier to use.";

const char kEnableNewShortcutMappingName[] = "Enable New Shortcut Mapping";
const char kEnableNewShortcutMappingDescription[] =
    "Enables experimental new shortcut mapping";

const char kEnablePalmOnMaxTouchMajorName[] =
    "Enable Palm when Touch is Maximum";
const char kEnablePalmOnMaxTouchMajorDescription[] =
    "Experimental: Enable Palm detection when the touchscreen reports max "
    "size. Not compatible with all devices.";

const char kEnablePalmOnToolTypePalmName[] =
    "Enable Palm when Tool Type is Palm";
const char kEnablePalmOnToolTypePalmDescription[] =
    "Experimental: Enable palm detection when touchscreen reports "
    "TOOL_TYPE_PALM. Not compatible with all devices.";

const char kEnablePalmSuppressionName[] =
    "Enable Palm Suppression with Stylus.";
const char kEnablePalmSuppressionDescription[] =
    "If enabled, suppresses touch when a stylus is on a touchscreen.";

const char kEnablePerDeskZOrderName[] =
    "Enable per-desk Z-order for all-desk windows.";
const char kEnablePerDeskZOrderDescription[] =
    "The Z-order of all-desk windows is maintained on a per-desk basis. This "
    "means that all-desk windows will not keep popping to the front when "
    "switching desks.";

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

const char kEnableZramWriteback[] = "Enable Zram Writeback";
const char kEnableZramWritebackDescription[] =
    "If enabled zram swap will be able to write back to disk increasing "
    "overall swap capacity";

const char kEnableVariableRefreshRateName[] = "Enable Variable Refresh Rate";
const char kEnableVariableRefreshRateDescription[] =
    "Enable the variable refresh rate (Adaptive Sync) setting for capable "
    "displays.";

const char kEnableViewPpdName[] = "Enable View PPD link.";
const char kEnableViewPpdDescription[] =
    "Enable View PPD link in the edit printer dialog allowing the "
    "user to request to view the PPD for a specific printer.";

const char kEapGtcWifiAuthenticationName[] = "EAP-GTC WiFi Authentication";
const char kEapGtcWifiAuthenticationDescription[] =
    "Allows configuration of WiFi networks using EAP-GTC authentication";

const char kAudioPeripheralVolumeGranularityName[] =
    "Audio Peripheral Volume Granularity";
const char kAudioPeripheralVolumeGranularityDescription[] =
    "Change audio peripheral volume granularity as close as peripheral "
    "reported. "
    "If the reported volume granularity are too small or too big. use 25 steps "
    "instead.";

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

const char kEnableOAuthIppName[] =
    "Enable OAuth when printing via the IPP protocol";
const char kEnableOAuthIppDescription[] =
    "Enable OAuth when printing via the IPP protocol";

const char kEnforceAshExtensionKeeplistName[] =
    "Enforce Ash extension keeplist";
const char kEnforceAshExtensionKeeplistDescription[] =
    "Enforce the Ash extension keeplist. Only the extensions and Chrome apps on"
    " the keeplist are enabled in Ash.";

const char kExoGamepadVibrationName[] = "Gamepad Vibration for Exo Clients";
const char kExoGamepadVibrationDescription[] =
    "Allow Exo clients like Android to request vibration events for gamepads "
    "that support it.";

const char kExoOrdinalMotionName[] =
    "Raw (unaccelerated) motion for Linux applications";
const char kExoOrdinalMotionDescription[] =
    "Send unaccelerated values as raw motion events to linux applications.";

const char kExperimentalAccessibilityDictationWithPumpkinName[] =
    "Experimental accessibility dictation using the pumpkin semantic parser.";
const char kExperimentalAccessibilityDictationWithPumpkinDescription[] =
    "Enables the pumpkin semantic parser for the accessibility dictation "
    "feature.";

const char kExperimentalAccessibilityDictationMoreCommandsName[] =
    "Additional experimental accessibility dictation commands.";
const char kExperimentalAccessibilityDictationMoreCommandsDescription[] =
    "Enables experimental dictation commands.";

const char kExperimentalAccessibilityDictationContextCheckingName[] =
    "Experimental accessibility dictation using context checking.";
const char kExperimentalAccessibilityDictationContextCheckingDescription[] =
    "Enables experimental dictation context checking.";

const char kExperimentalAccessibilitySelectToSpeakVoiceSwitchingName[] =
    "Experimental accessibility Select-to-Speak voice switching.";
const char kExperimentalAccessibilitySelectToSpeakVoiceSwitchingDescription[] =
    "Enables Select-to-Speak voice switching, which will change the "
    "text-to-speech voice depending on the language it encounters.";

const char kExperimentalAccessibilityGoogleTtsLanguagePacksName[] =
    "Experimental accessibility Google TTS Langauge Packs.";
const char kExperimentalAccessibilityGoogleTtsLanguagePacksDescription[] =
    "Enables downloading Google TTS voices using Langauge Packs.";

const char kExperimentalAccessibilitySwitchAccessTextName[] =
    "Enable enhanced Switch Access text input.";
const char kExperimentalAccessibilitySwitchAccessTextDescription[] =
    "Enable experimental or in-progress Switch Access features for improved "
    "text input";

const char kFileTransferEnterpriseConnectorName[] =
    "Enable Files Transfer Enterprise Connector.";
const char kFileTransferEnterpriseConnectorDescription[] =
    "Enable the File Transfer Enterprise Connector.";

const char kFilesAppExperimentalName[] =
    "Experimental UI features for Files app";
const char kFilesAppExperimentalDescription[] =
    "Enable experimental UI features for Files app. Experimental features are "
    "expected to be non functional to end users.";

const char kFilesConflictDialogName[] = "Files app conflict dialog";
const char kFilesConflictDialogDescription[] =
    "When enabled, the conflict dialog will be shown during file transfers "
    "if a file entry in the transfer exists at the destination.";

const char kFilesExtractArchiveName[] = "Extract archive in Files app";
const char kFilesExtractArchiveDescription[] =
    "Enable the simplified archive extraction feature in Files app";

const char kFilesInlineSyncStatusName[] =
    "Enable inline sync status in Files app.";
const char kFilesInlineSyncStatusDescription[] =
    "Enable displaying the sync status of each file next to its name in Files "
    "app.";

const char kFilesSearchV2Name[] = "V2 generation of file search.";
const char kFilesSearchV2Description[] = "Enable V2 generation of file search.";

const char kFilesSinglePartitionFormatName[] =
    "Enable Partitioning of Removable Disks.";
const char kFilesSinglePartitionFormatDescription[] =
    "Enable partitioning of removable disks into single partition.";

const char kFilesTrashName[] = "Enable Files Trash.";
const char kFilesTrashDescription[] =
    "Enable trash for My files volume in Files App.";

const char kFloatWindow[] = "Float current active window";
const char kFloatWindowDescription[] =
    "Enables the accelerator (Command + Alt + F) to float current active "
    "window. You can also access this from Multitask Menu, by "
    "hovering/long-pressing on the maximize button on the window frame.";

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

const char kFullRestoreForLacrosName[] = "Full restore lacros support";
const char kFullRestoreForLacrosDescription[] =
    "ChromeOS full restore lacros support";

const char kFuseBoxDebugName[] = "Debugging UI for ChromeOS FuseBox service";
const char kFuseBoxDebugDescription[] =
    "Show additional debugging UI for ChromeOS FuseBox service.";

const char kHelpAppBackgroundPageName[] = "Help App Background Page";
const char kHelpAppBackgroundPageDescription[] =
    "Enables the Background page in the help app. The background page is used "
    "to initialize the Help App Launcher search index and show the Discover "
    "tab notification.";

const char kHelpAppLauncherSearchName[] = "Help App launcher search";
const char kHelpAppLauncherSearchDescription[] =
    "Enables showing search results from the help app in the launcher.";

const char kDiacriticsOnPhysicalKeyboardLongpressName[] =
    "Enable diacritics and variant character selection on PK longpress.";
const char kDiacriticsOnPhysicalKeyboardLongpressDescription[] =
    "Enable diacritics and other varient character selection on physical "
    "keyboard longpress.";

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

const char kImeAssistPersonalInfoName[] = "Enable assistive personal info";
const char kImeAssistPersonalInfoDescription[] =
    "Enable auto-complete suggestions on personal infomation for native IME.";

const char kImeTrayHideVoiceButtonName[] =
    "Hides redudant voice button in IME tray";
const char kImeTrayHideVoiceButtonDescription[] =
    "Hides voice button in IME tray when mic icon is shown in the shelf";

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

const char kImeSystemEmojiPickerSearchExtensionName[] =
    "System emoji picker search extension";
const char kImeSystemEmojiPickerSearchExtensionDescription[] =
    "Emoji picker search extension enhances current emoji search by "
    "introducing multi-word prefix search.";

const char kImeStylusHandwritingName[] = "Stylus Handwriting";
const char kImeStylusHandwritingDescription[] =
    "Enable VK UI for stylus in text fields";

const char kLacrosAvailabilityIgnoreName[] =
    "Ignore lacros-availability policy";
const char kLacrosAvailabilityIgnoreDescription[] =
    "Makes the lacros-availability policy have no effect. Instead Lacros "
    "availability will be controlled by experiment and/or user flags.";

const char kLacrosOnlyName[] = "Lacros is the only browser";
const char kLacrosOnlyDescription[] =
    "Use Lacros-chrome as the only web browser on ChromeOS. "
    "This flag is ignored if Lacros support or primary is disabled.";

const char kLacrosPrimaryName[] = "Lacros as the primary browser";
const char kLacrosPrimaryDescription[] =
    "Use Lacros-chrome as the primary web browser on ChromeOS. "
    "This flag is ignored if Lacros support is disabled.";

const char kLacrosStabilityName[] = "Lacros stability";
const char kLacrosStabilityDescription[] = "Lacros update channel.";

const char kLacrosSelectionName[] = "Lacros selection";
const char kLacrosSelectionDescription[] =
    "Choosing between rootfs or stateful Lacros.";

const char kLacrosSelectionRootfsDescription[] = "Rootfs";
const char kLacrosSelectionStatefulDescription[] = "Stateful";

const char kLacrosSupportName[] = "Lacros support";
const char kLacrosSupportDescription[] =
    "Support for the experimental lacros-chrome browser. Please note that the "
    "first restart can take some time to setup lacros-chrome. Please DO NOT "
    "attempt to turn off the device during the restart.";

const char kLacrosWaylandLoggingName[] = "Lacros wayland logging";
const char kLacrosWaylandLoggingDescription[] =
    "Enables wayland logging for Lacros. This generates a significant amount "
    "of logs on disk. Logs are cleared after two restarts.";

const char kLacrosProfileMigrationForAnyUserName[] =
    "Lacros profile migration for any user";
const char kLacrosProfileMigrationForAnyUserDescription[] =
    "Enables lacros profile migration that are currently only enabled for "
    "certain users. Please enable with CAUTION. Enabling profile migration "
    "means that any pre-existing lacros data will be wiped and replaced with "
    "data migrated from ash. It also has a side effect that lacros will be "
    "disbled until profile migration is completed.";

const char kLacrosMoveProfileMigrationName[] = "Enforce profile move migration";
const char kLacrosMoveProfileMigrationDescription[] =
    "Enforce Lacros profile move migration which moves files from Ash profile "
    "directory to Lacros profile directory instead of copying. Please note "
    "that disabling Lacros and falling back to Ash after move migration is not "
    "supported.";

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

const char kNearbySharingOnePageOnboardingName[] =
    "Nearby Sharing one-page Onboarding.";
const char kNearbySharingOnePageOnboardingDescription[] =
    "Enable new One-page onboarding workflow for Nearby Share.";

const char kNearbySharingSelfShareAutoAcceptName[] =
    "Nearby Sharing Self Share Auto-Accept";
const char kNearbySharingSelfShareAutoAcceptDescription[] =
    "Enables auto-accept functionality when sharing between a user's own "
    "devices.";

const char kNearbySharingSelfShareUIName[] = "Nearby Sharing Self Share UI";
const char kNearbySharingSelfShareUIDescription[] =
    "Enables UI features for Self Share to allow seamless sharing between a "
    "user's own devices.";

const char kOobeHidDetectionRevampName[] = "OOBE HID Detection Revamp";
const char kOobeHidDetectionRevampDescription[] =
    "Enables the ChromeOS HID Detection Revamp, which updates OOBE HID "
    "detection screen UI and related infrastructure.";

const char kOsSettingsAppBadgingToggleName[] =
    "ChromeOS Settings App Badging Toggle";
const char kOsSettingsAppBadgingToggleDescription[] =
    "Enables app badging toggle to be displayed in app notification page in"
    "ChromeOS Settings.";

const char kOsSettingsSearchFeedbackName[] =
    "ChromeOS Settings Search Feedback";
const char kOsSettingsSearchFeedbackDescription[] =
    "Enables search result feedback in ChromeOS Settings when no search "
    "results are returned.";

const char kPartialSplit[] = "Partial Split";
const char kPartialSplitDescription[] =
    "Enables the option to snap two windows into 2/3 and 1/3 for split view.";

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

const char kPhoneHubCameraRollName[] = "Camera Roll in Phone Hub";
const char kPhoneHubCameraRollDescription[] =
    "Enables the Camera Roll feature in Phone Hub, which allows users to "
    "access recent photos and videos taken on a connected Android device.";

const char kPhoneHubFeatureSetupErrorHandlingName[] =
    "Error handling during Phone Hub feature setup";
const char kPhoneHubFeatureSetupErrorHandlingDescription[] =
    "Enable the error handling during Phone Hub feature setup. It allows"
    "users to see result on Chromebook when the setup failed on connected"
    "remote phone device.";

const char kPhoneHubNudgeName[] = "Phone Hub nudge";
const char kPhoneHubNudgeDescription[] =
    "Enables a nudge to direct eligible users to open Phone Hub.";

const char kPreferDcheckName[] = "Prefer DCHECK-enabled build";
const char kPreferDcheckDescription[] =
    "Use a DCHECK-enabled build when available.";

const char kPrintingPpdChannelName[] = "Printing PPD channel";
const char kPrintingPpdChannelDescription[] =
    "The channel from which PPD index "
    "is loaded when matching PPD files during printer setup.";

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

const char kQuickActionShowBubbleLauncherName[] =
    "Swipe to show bubble launcher";
const char kQuickActionShowBubbleLauncherDescription[] =
    "Enables a fling gesture or mouse scroll from the shelf to show the bubble "
    "launcher.";

const char kAppListDragAndDropRefactorName[] =
    "Drag and drop refactor in launcher";
const char kAppListDragAndDropRefactorDescription[] =
    "Enables app list drag and drop refactor to use views drag and drop APIs. "
    "No user visible changes are expected.";

const char kProjectorName[] = "Enable Projector";
const char kProjectorDescription[] =
    "Enables Projects SWA and associated recording tools";

const char kProjectorAnnotatorName[] = "Enable Projector annotator";
const char kProjectorAnnotatorDescription[] =
    "Turns on annotator tools when recording a screen capture using Projector";

const char kProjectorExcludeTranscriptName[] =
    "Enable Projector exclude transcript feature";
const char kProjectorExcludeTranscriptDescription[] =
    "Supports excluding segment of Projector recording by excluding transcript";

const char kProjectorLocalPlaybackName[] = "Enable Projector local playback";
const char kProjectorLocalPlaybackDescription[] =
    "Supports streaming screencast videos from DriveFS instead of Drive before "
    "they have finished transcoding";

const char kProjectorAppDebugName[] = "Enable Projector app debug";
const char kProjectorAppDebugDescription[] =
    "Adds more informative error messages to the Projector app for debugging";

const char kQsRevampName[] = "Quick Settings Revamp";
const char kQsRevampDescription[] =
    "Enable QS UI revamp with separated message center.";

const char kReleaseNotesNotificationAllChannelsName[] =
    "Release Notes Notification All Channels";
const char kReleaseNotesNotificationAllChannelsDescription[] =
    "Enables the release notes notification for all ChromeOS channels";

const char kReleaseTrackUiName[] = "Release track system tray UI";
const char kReleaseTrackUiDescription[] =
    "Enables the release track UI in the system tray";

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

const char kShelfAutoHideSeparationName[] =
    "Enable separate shelf auto-hide preferences.";
const char kShelfAutoHideSeparationDescription[] =
    "Allows for the shelf's auto-hide preference to be specified separately "
    "for clamshell and tablet mode.";

const char kShimlessRMAFlowName[] = "Enable shimless RMA flow";
const char kShimlessRMAFlowDescription[] = "Enable shimless RMA flow";

const char kShimlessRMAOsUpdateName[] = "Enable OS updates in shimless RMA";
const char kShimlessRMAOsUpdateDescription[] =
    "Turns on OS updating in Shimless RMA";

const char kShimlessRMADisableDarkModeName[] =
    "Disable dark mode in Shimless RMA";
const char kShimlessRMADisableDarkModeDescription[] =
    "Disable dark mode and only allow light mode in Shimless RMA";

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

const char kCaptivePortalErrorPageName[] =
    "Enables Captive Portal error page suggestions.";
const char kCaptivePortalErrorPageDescription[] =
    "Enables Captive Portal error page suggestions when a page fails to load "
    "and ChromeOS has detected a captive portal.";

const char kTerminalAlternativeEmulatorName[] = "Terminal alternative emulator";
const char kTerminalAlternativeEmulatorDescription[] =
    "Enable the alternative emulator for the Terminal app. You will also get "
    "an option in Terminal settings to change the default emulator.";

const char kTerminalDevName[] = "Terminal dev";
const char kTerminalDevDescription[] =
    "Enables Terminal System App to load from Downloads for developer testing. "
    "Only works in dev and canary channels.";

const char kTerminalMultiProfileName[] = "Terminal multi-profiles for settings";
const char kTerminalMultiProfileDescription[] =
    "Enables Terminal System App to set multiple profiles in the settings page "
    "and configure which profile to use for each Linux or SSH connection.";

const char kTerminalSftpName[] = "Terminal SFTP / mount";
const char kTerminalSftpDescription[] =
    "Enables Terminal System App to do SFTP and mount remote hosts.";

const char kTerminalTmuxIntegrationName[] = "Terminal tmux integration";
const char kTerminalTmuxIntegrationDescription[] =
    "Enables integration with tmux control mode (tmux -CC) in the Terminal "
    "System App.";

const char kTetherName[] = "Instant Tethering";
const char kTetherDescription[] =
    "Enables Instant Tethering. Instant Tethering allows your nearby Google "
    "phone to share its Internet connection with this device.";

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

const char kUseFakeDeviceForMediaStreamName[] = "Use fake video capture device";
const char kUseFakeDeviceForMediaStreamDescription[] =
    "Forces Chrome to use a fake video capture device (a rolling pacman with a "
    "timestamp) instead of the system audio/video devices, for debugging "
    "purposes.";

const char kUXStudy1Name[] = "UX Study 1";
const char kUXStudy1Description[] = "Opt into a group for UX Study";

const char kUiDevToolsName[] = "Enable native UI inspection";
const char kUiDevToolsDescription[] =
    "Enables inspection of native UI elements. For local inspection use "
    "chrome://inspect#other";

const char kUiSlowAnimationsName[] = "Slow UI animations";
const char kUiSlowAnimationsDescription[] = "Makes all UI animations slow.";

const char kVaapiJpegImageDecodeAccelerationName[] =
    "VA-API JPEG decode acceleration for images";
const char kVaapiJpegImageDecodeAccelerationDescription[] =
    "Enable or disable decode acceleration of JPEG images (as opposed to camera"
    " captures) using the VA-API.";

const char kVaapiWebPImageDecodeAccelerationName[] =
    "VA-API WebP decode acceleration for images";
const char kVaapiWebPImageDecodeAccelerationDescription[] =
    "Enable or disable decode acceleration of WebP images using the VA-API.";

const char kVirtualKeyboardName[] = "Virtual Keyboard";
const char kVirtualKeyboardDescription[] =
    "Always show virtual keyboard regardless of having a physical keyboard "
    "present";

const char kVirtualKeyboardDisabledName[] = "Disable Virtual Keyboard";
const char kVirtualKeyboardDisabledDescription[] =
    "Always disable virtual keyboard regardless of device mode. Workaround for "
    "virtual keyboard showing with some external keyboards.";

const char kVirtualKeyboardMultitouchName[] = "Virtual Keyboard Multitouch";
const char kVirtualKeyboardMultitouchDescription[] =
    "Enables multitouch on the virtual keyboard.";

const char kVirtualKeyboardRoundCornersName[] =
    "Virtual Keyboard Round Corners";
const char kVirtualKeyboardRoundCornersDescription[] =
    "Enables round corners on the virtual keyboard.";

const char kWakeOnWifiAllowedName[] = "Allow enabling wake on WiFi features";
const char kWakeOnWifiAllowedDescription[] =
    "Allows wake on WiFi features in shill to be enabled.";

const char kWebAppsCrosapiName[] = "Web Apps Crosapi";
const char kWebAppsCrosapiDescription[] =
    "Support web apps publishing from Lacros browser.";

const char kWelcomeScreenName[] =
    "Productivity Experiment: Reorient with welcome screen";
const char kWelcomeScreenDescription[] =
    "When a user is about to start a new task, provide them with relevant "
    "information and tools to help them orient and get started.";

const char kWifiConnectMacAddressRandomizationName[] =
    "MAC address randomization";
const char kWifiConnectMacAddressRandomizationDescription[] =
    "Randomize MAC address when connecting to unmanaged (non-enterprise) "
    "WiFi networks.";

const char kLauncherGameSearchName[] = "Enable launcher game search";
const char kLauncherGameSearchDescription[] =
    "Enables cloud game search results in the launcher.";

const char kLauncherKeywordExtractionScoring[] =
    "Query keyword extraction and scoring in launcher";
const char kLauncherKeywordExtractionScoringDescription[] =
    "Enables extraction of keywords from query then calculate score from "
    "extracted keyword in the launcher.";

const char kLauncherFuzzyMatchForOmniboxName[] =
    "Omnibox Results Fuzzy match experiment";
const char kLauncherFuzzyMatchForOmniboxDescription[] =
    "To evaluate the viability of a Fuzzy match on Omnibox results to "
    "downweight search sugestions";

const char kLauncherSystemInfoAnswerCardsName[] =
    "System Info Answer Cards in launcher";
const char kLauncherSystemInfoAnswerCardsDescription[] =
    "Enables System info answer cards in the launcher to provide system "
    "performance metrics";

const char kMacAddressRandomizationName[] = "MAC address randomization";
const char kMacAddressRandomizationDescription[] =
    "Feature to allow MAC address randomization to be enabled for WiFi "
    "networks.";

const char kOobeJellyName[] = "Jelly design for OOBE";
const char kOobeJellyDescription[] =
    "Feature to enable the Jelly design in out of box experience.";

const char kLibAssistantV2MigrationName[] = "LibAssistant V2 migration";
const char kLibAssistantV2MigrationDescription[] =
    "Enables loading LibAssistant V2 for Assistant on ChromeOS.";

// Prefer keeping this section sorted to adding new definitions down here.

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
const char kGetDisplayMediaSetName[] = "GetDisplayMediaSet API";
const char kGetDisplayMediaSetDescription[] =
    "When enabled, the getDisplayMediaSet API for capturing multiple surfaces "
    "at once is available.";

const char kGetDisplayMediaSetAutoSelectAllScreensName[] =
    "autoSelectAllScreens attribute for GetDisplayMediaSet";
const char kGetDisplayMediaSetAutoSelectAllScreensDescription[] =
    "When enabled, the autoSelectAllScreens attribute is available for usage "
    "with the GetDisplayMediaSet API.";

#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
const char kDesktopCaptureLacrosV2Name[] = "Enable Lacros Desktop Capture V2";
const char kDesktopCaptureLacrosV2Description[] =
    "Enables the improved desktop/window capturer for doing screen/window "
    "sharing on Lacros";

const char kLacrosAuraCaptureName[] = "Enable Lacros Aura Capture";
const char kLacrosAuraCaptureDescription[] =
    "Enables LaCrOS to perform an in-process capture of it's own windows "
    "rather than needing to delegate the capture to Ash";

const char kLacrosMergeIcuDataFileName[] =
    "Enable merging of icudtl.dat in Lacros";
const char kLacrosMergeIcuDataFileDescription[] =
    "Enables sharing common areas of icudtl.dat between Ash and Lacros.";

const char kLacrosScreenCoordinatesEnabledName[] =
    "Enable screen coordinates system in lacros-chrome";
const char kLacrosScreenCoordinatesEnabledDescription[] =
    "Enabling this will allow lacros to control the window position in screen "
    "coordinates. This is required for features such as the Javascript APIs "
    "moveBy, moveTo or session restore.";
#endif  // #if BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS)
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

const char kCrosPrivacyHubName[] = "Enable ChromeOS Privacy Hub";
const char kCrosPrivacyHubDescription[] = "Enables ChromeOS Privacy Hub.";

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

const char kIntentChipSkipsPickerName[] =
    "Link capturing intent chip skips the intent picker bubble";
const char kIntentChipSkipsPickerDescription[] =
    "When enabled, clicking the intent chip in the Omnibox will skip the "
    "intent picker bubble and launch directly into the app. If multiple apps "
    "are installed for a URL, the intent picker will still be shown for "
    "disambigation.";

const char kIntentChipAppIconName[] =
    "Show app icons in the link capturing intent chip";
const char kIntentChipAppIconDescription[] =
    "When enabled, the intent chip in the Omnibox will show the app icon for "
    "the app which can handle the current URL.";

const char kKioskEnableAppServiceName[] = "Enable App Service in Kiosk.";
const char kKioskEnableAppServiceDescription[] =
    "Uses App Service to install web apps and launch both Chrome apps and web "
    "apps in Kiosk sessions.";

const char kLacrosColorManagementName[] = "Enable Chrome Color Management.";
const char kLacrosColorManagementDescription[] =
    "Uses chrome-color-management wayland protocol to manage color spaces "
    "for lacros.";

const char kLinkCapturingAutoDisplayIntentPickerName[] =
    "Enable auto-display of intent picker bubble";
const char kLinkCapturingAutoDisplayIntentPickerDescription[] =
    "When enabled, the intent picker bubble will automatically display when "
    "clicking a link which can be opened in installed apps. Only applies when "
    "'Enable updated link capturing UI' is enabled.";

const char kLinkCapturingInfoBarName[] = "Enable link capturing info bar";
const char kLinkCapturingInfoBarDescription[] =
    "Enables an info bar which appears when launching a web app through the "
    "Omnibox intent chip, prompting to update the link capturing setting for "
    "that app.";

const char kLinkCapturingUiUpdateName[] = "Enable updated link capturing UI";
const char kLinkCapturingUiUpdateDescription[] =
    "Enables updated UI for link capturing flows from the browser to apps, "
    "including the intent picker and an in-app link capturing prompt.";

const char kMessagesPreinstallName[] = "Preinstall  Messages PWA";
const char kMessagesPreinstallDescription[] =
    "Enables preinstallation of the Messages for Web PWA for unmanaged users.";

const char kMicrosoftOfficeWebAppExperimentName[] =
    "Microsoft Office web app experiment";
const char kMicrosoftOfficeWebAppExperimentDescription[] =
    "A ChromeOS experiment on the Microsoft Office web app that overrides the "
    "scope and theme color. Not intended for public release; this is testing "
    "capabilites that will be available to websites at a later date.";

const char kOneGroupPerRendererName[] =
    "Use one cgroup for each foreground renderer";
const char kOneGroupPerRendererDescription[] =
    "Places each Chrome foreground renderer into its own cgroup";

const char kDisableQuickAnswersV2TranslationName[] =
    "Disable Quick Answers Translation";
const char kDisableQuickAnswersV2TranslationDescription[] =
    "Disable translation services of the Quick Answers.";

const char kSyncChromeOSExplicitPassphraseSharingName[] =
    "Sync passphrase sharing";
const char kSyncChromeOSExplicitPassphraseSharingDescription[] =
    "Allows sharing custom sync passphrase between OS and Browser on ChromeOS";

const char kChromeOSSyncedSessionSharingName[] =
    "Synced session sharing on ChromeOS";
const char kChromeOSSyncedSessionSharingDescription[] =
    "Allows sharing synced sessions between OS and Browser on ChromeOS";

const char kTouchTextEditingRedesignName[] = "Touch Text Editing Redesign";
const char kTouchTextEditingRedesignDescription[] =
    "Enables new touch text editing features.";
#endif  // BUILDFLAG(IS_CHROMEOS)

#if defined(ARCH_CPU_X86_FAMILY) && BUILDFLAG(IS_CHROMEOS)
const char kVaapiVP9kSVCEncoderName[] =
    "VA-API encode acceleration for k-SVC VP9";
const char kVaapiVP9kSVCEncoderDescription[] =
    "Enable or disable k-SVC VP9 encode acceleration using VA-API.";
#endif  // defined(ARCH_CPU_X86_FAMILY) && BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
const char kChromeOSDirectVideoDecoderName[] = "ChromeOS Direct Video Decoder";
const char kChromeOSDirectVideoDecoderDescription[] =
    "Enables the hardware-accelerated ChromeOS direct media::VideoDecoder "
    "implementation. Note that this might be entirely disallowed by the "
    "--platform-disallows-chromeos-direct-video-decoder command line switch "
    "which is added for platforms where said direct VideoDecoder does not work "
    "or is not well tested (see the disable_cros_video_decoder USE flag in "
    "ChromeOS)";
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

const char kRevampedPasswordManagementBubbleName[] =
    "Revamped Password Managment Bubble (Desktop)";
const char kRevampedPasswordManagementBubbleDescription[] =
    "Enable the new password managment bubble triggered by clicking the key "
    "icon in the omnibox.";

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

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_ANDROID)
const char kLocalWebApprovalsName[] = "Local web approvals";
const char kLocalWebApprovalsDescription[] =
    "Enable local web approvals for Family Link users. Web filter interstitial "
    "refresh needs to also be enabled.";
#endif

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
extern const char kChromeWideEchoCancellationName[] =
    "Chrome-wide echo cancellation";
extern const char kChromeWideEchoCancellationDescription[] =
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
extern const char kEnableCardboardName[] = "Enable Cardboard VR WebXR Runtime";
extern const char kEnableCardboardDescription[] =
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

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP) && BUILDFLAG(IS_CHROMEOS_ASH)
const char kWebUITabStripTabDragIntegrationName[] =
    "ChromeOS drag-drop extensions for WebUI tab strip";
const char kWebUITabStripTabDragIntegrationDescription[] =
    "Enables special handling in ash for WebUI tab strip tab drags. Allows "
    "dragging tabs out to new windows.";
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP) && BUILDFLAG(IS_CHROMEOS_ASH)

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

const char kDesktopDetailedLanguageSettingsName[] =
    "Detailed Language Settings (Desktop)";
const char kDesktopDetailedLanguageSettingsDescription[] =
    "Enable the new detailed language settings page";

const char kSyncPromoAfterSigninInterceptName[] =
    "Sync Promo After Sign-in Intercept";
const char kSyncPromoAfterSigninInterceptDescription[] =
    "Enable updates in the first run experience for a new profile in the "
    "Sign-in Intercept, such as a Sync Promo step and an updated Profile "
    "Customization UI, which is also used for local profile creation.";

const char kSigninInterceptBubbleV2Name[] = "Sign-in Intercept Bubble V2";
const char kSigninInterceptBubbleV2Description[] =
    "Enable visual updates to the sign-in intercept bubble, such as updated "
    "illustration, strings and an enterprise disclaimer.";
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

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// ============================================================================
// Don't just add flags to the end, put them in the right section in
// alphabetical order just like the header file.
// ============================================================================

}  // namespace flag_descriptions
