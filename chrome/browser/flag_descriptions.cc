// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/flag_descriptions.h"

// Keep in identical order as the header file, see the comment at the top
// for formatting rules.

namespace flag_descriptions {

const char kAccelerated2dCanvasName[] = "Accelerated 2D canvas";
const char kAccelerated2dCanvasDescription[] =
    "Enables the use of the GPU to perform 2d canvas rendering instead of "
    "using software rendering.";

const char kAcceleratedVideoDecodeName[] = "Hardware-accelerated video decode";
const char kAcceleratedVideoDecodeDescription[] =
    "Hardware-accelerated video decode where available.";

const char kAcceleratedVideoEncodeName[] = "Hardware-accelerated video encode";
const char kAcceleratedVideoEncodeDescription[] =
    "Hardware-accelerated video encode where available.";

const char kAccessibilityExposeARIAAnnotationsName[] =
    "Expose ARIA Annotation roles";
const char kAccessibilityExposeARIAAnnotationsDescription[] =
    "Expose annotation- prefixed roles from ARIA Annotations draft "
    "specification at https://w3c.github.io/annotation-aria/.";

const char kAccessibilityExposeDisplayNoneName[] =
    "Expose 'display: none' nodes for accessibility";
const char kAccessibilityExposeDisplayNoneDescription[] =
    "Expose 'display: none' nodes that have an HTML ID to the browser process "
    "accessibility tree.";

const char kAccessibilityInternalsPageImprovementsName[] =
    "Accessibility internals page improvements";
const char kAccessibilityInternalsPageImprovementsDescription[] =
    "Improvements to the chrome://accessibility page.";

const char kAllowInsecureLocalhostName[] =
    "Allow invalid certificates for resources loaded from localhost.";
const char kAllowInsecureLocalhostDescription[] =
    "Allows requests to localhost over HTTPS even when an invalid certificate "
    "is presented.";

const char kAllowPopupsDuringPageUnloadName[] =
    "Allows a page to show popups during its unloading";
const char kAllowPopupsDuringPageUnloadDescription[] =
    "When the flag is set to enabled, pages are allowed to show popups while "
    "they are being unloaded.";

const char kAllowSignedHTTPExchangeCertsWithoutExtensionName[] =
    "Allow Signed HTTP Exchange certificates without extension";
const char kAllowSignedHTTPExchangeCertsWithoutExtensionDescription[] =
    "Accepts Origin-Signed HTTP Exchanges to be signed with certificates "
    "that do not have CanSignHttpExchangesDraft extension. Warning: Enabling "
    "this may pose a security risk.";

const char kAllowSyncXHRInPageDismissalName[] =
    "Allows synchronous XHR requests in page dismissal";
const char kAllowSyncXHRInPageDismissalDescription[] =
    "Allows synchronous XHR requests during page dismissal when the page is "
    "being navigated away or closed by the user.";

const char kEnableClipboardProviderTextSuggestionsName[] =
    "Omnibox clipboard text search suggestions";
const char kEnableClipboardProviderTextSuggestionsDescription[] =
    "Enables search suggestions in omnibox";

const char kEnableFtpName[] = "Enable support for FTP URLs";
const char kEnableFtpDescription[] =
    "When enabled, the browser will handle navigations to ftp:// URLs by "
    "either showing a directory listing or downloading the resource over FTP. "
    "When disabled, the browser has no special handling for ftp:// URLs and "
    "by default defer handling of the URL to the underlying platform.";

const char kEnableSignedExchangeSubresourcePrefetchName[] =
    "Enable Signed Exchange subresource prefetching";
const char kEnableSignedExchangeSubresourcePrefetchDescription[] =
    "When enabled, the distributors of signed exchanges can let Chrome know "
    "alternative signed exchange subresources by setting \"alternate\" link "
    "header. Chrome will prefetch the alternate signed exchange subresources "
    "and will load them if the publisher of the main signed exchange has set "
    "\"allowed-alt-sxg\" link header in the signed inner response of the "
    "main signed exchange.";

const char kEnableSignedExchangePrefetchCacheForNavigationsName[] =
    "Enable Signed Exchange prefetch cache for navigations";
const char kEnableSignedExchangePrefetchCacheForNavigationsDescription[] =
    "When enabled, the prefetched signed exchanges is stored to a prefetch "
    "cache attached to the frame. The body of the inner response is stored as "
    "a blob and the verification process of the signed exchange is skipped for "
    "the succeeding navigation.";

const char kAudioWorkletRealtimeThreadName[] =
    "Use realtime priority thread for Audio Worklet";
const char kAudioWorkletRealtimeThreadDescription[] =
    "Run Audio Worklet operation on a realtime priority thread for better "
    "audio stream stability.";

const char kUpdatedCellularActivationUiName[] =
    "Updated Cellular Activation UI";
const char kUpdatedCellularActivationUiDescription[] =
    "Enables the updated cellular activation UI.";

const char kUseMessagesGoogleComDomainName[] = "Use messages.google.com domain";
const char kUseMessagesGoogleComDomainDescription[] =
    "Use the messages.google.com domain as part of the \"Messages\" "
    "feature under \"Connected Devices\" settings.";

const char kUseMessagesStagingUrlName[] = "Use Messages staging URL";
const char kUseMessagesStagingUrlDescription[] =
    "Use the staging server as part of the \"Messages\" feature under "
    "\"Connected Devices\" settings.";

const char kEnableMessagesWebPushName[] =
    "Web push in Android Messages integration";
const char kEnableMessagesWebPushDescription[] =
    "Use web push for background notificatons in Chrome OS integration "
    "with Android Messages for Web";

const char kAndroidPictureInPictureAPIName[] =
    "Picture-in-Picture Web API for Android";
const char kAndroidPictureInPictureAPIDescription[] =
    "Enable Picture-in-Picture Web API for Android";

const char kAndroidSiteSettingsUIRefreshName[] =
    "Android Site Settings UI changes.";
const char kAndroidSiteSettingsUIRefreshDescription[] =
    "Enable the new UI "
    "changes in Site Settings in Android.";

const char kAutomaticPasswordGenerationName[] = "Automatic password generation";
const char kAutomaticPasswordGenerationDescription[] =
    "Allow Chrome to offer to generate passwords when it detects account "
    "creation pages.";

const char kDnsOverHttpsName[] = "Secure DNS lookups";
const char kDnsOverHttpsDescription[] =
    "Enables DNS over HTTPS. When this feature is enabled, your browser may "
    "try to use a secure HTTPS connection to look up the addresses of websites "
    "and other web resources.";

const char kDrawVerticallyEdgeToEdgeName[] =
    "Draw contents vertically from edge to edge.";
const char kDrawVerticallyEdgeToEdgeDescription[] =
    "Draw contents vertically from edge to edge.";

const char kAutofillAlwaysReturnCloudTokenizedCardName[] =
    "Return cloud token details for server credit cards when possible";
const char kAutofillAlwaysReturnCloudTokenizedCardDescription[] =
    "When enabled and where available, forms filled using Google Payments "
    "server cards are populated with cloud token details, including CPAN "
    "(cloud tokenized version of the Primary Account Number) and dCVV (dynamic "
    "CVV).";

extern const char kAutofillAlwaysShowServerCardsInSyncTransportName[] =
    "AlwaysShowServerCardsInSyncTransport";
extern const char kAutofillAlwaysShowServerCardsInSyncTransportDescription[] =
    "Always show server cards when in sync transport mode for wallet data";

extern const char kAutofillAssistantChromeEntryName[] =
    "AutofillAssistantChromeEntry";
extern const char kAutofillAssistantChromeEntryDescription[] =
    "Initiate autofill assistant from within Chrome.";

extern const char kAutofillAssistantDirectActionsName[] =
    "Autofill Assistant direct actions";
extern const char kAutofillAssistantDirectActionsDescription[] =
    "When enabled, expose direct actions from the Autofill Assistant.";

const char kAutofillCacheQueryResponsesName[] =
    "Cache Autofill Query Responses";
const char kAutofillCacheQueryResponsesDescription[] =
    "When enabled, autofill will cache the responses it receives from the "
    "crowd-sourced field type prediction server.";

const char kAutofillEnableCompanyNameName[] =
    "Enable Autofill Company Name field";
const char kAutofillEnableCompanyNameDescription[] =
    "When enabled, Company Name fields will be auto filled";

const char kAutofillEnableLocalCardMigrationForNonSyncUserName[] =
    "Enable local card migration flow for non-syncing users";
const char kAutofillEnableLocalCardMigrationForNonSyncUserDescription[] =
    "When enabled, the local card migration flow will be enabled for users who "
    "have signed in but not enabled Chrome Sync.";

const char kAutofillEnableToolbarStatusChipName[] =
    "Move Autofill omnibox icons next to the profile avatar icon";
const char kAutofillEnableToolbarStatusChipDescription[] =
    "When enabled, Autofill data related icon will be shown in the status "
    "chip next to the profile avatar icon in the toolbar.";

const char kAutofillEnforceMinRequiredFieldsForHeuristicsName[] =
    "Autofill Enforce Min Required Fields For Heuristics";
const char kAutofillEnforceMinRequiredFieldsForHeuristicsDescription[] =
    "When enabled, autofill will generally require a form to have at least 3 "
    "fields before allowing heuristic field-type prediction to occur.";

const char kAutofillEnforceMinRequiredFieldsForQueryName[] =
    "Autofill Enforce Min Required Fields For Query";
const char kAutofillEnforceMinRequiredFieldsForQueryDescription[] =
    "When enabled, autofill will generally require a form to have at least 3 "
    "fields before querying the autofill server for field-type predictions.";

const char kAutofillEnforceMinRequiredFieldsForUploadName[] =
    "Autofill Enforce Min Required Fields For Upload";
const char kAutofillEnforceMinRequiredFieldsForUploadDescription[] =
    "When enabled, autofill will generally require a form to have at least 3 "
    "fillable fields before uploading field-type votes for that form.";

const char kAutofillNoLocalSaveOnUnmaskSuccessName[] =
    "Remove the option to save local copies of unmasked server cards";
const char kAutofillNoLocalSaveOnUnmaskSuccessDescription[] =
    "When enabled, the server card unmask prompt will not include the checkbox "
    "to also save the card locally on the current device upon success.";

const char kAutofillNoLocalSaveOnUploadSuccessName[] =
    "Disable saving local copy of uploaded card when credit card upload "
    "succeeds";
const char kAutofillNoLocalSaveOnUploadSuccessDescription[] =
    "When enabled, no local copy of server card will be saved when credit card "
    "upload succeeds.";

const char kAutofillOffNoServerDataName[] = "Autofill Off No Server Data";
const char kAutofillOffNoServerDataDescription[] =
    "Disables Autofill for fields with autocomplete off that have no "
    "crowd-sourced evidence that Autofill would be helpful.";

const char kAutofillProfileClientValidationName[] =
    "Autofill Validates Profiles By Client";
const char kAutofillProfileClientValidationDescription[] =
    "Allows autofill to validate profiles on the client side";

const char kAutofillProfileServerValidationName[] =
    "Autofill Uses Server Validation";
const char kAutofillProfileServerValidationDescription[] =
    "Allows autofill to use server side validation";

const char kAutofillPruneSuggestionsName[] = "Autofill Prune Suggestions";
const char kAutofillPruneSuggestionsDescription[] =
    "Further limits the number of suggestions in the Autofill dropdown.";

const char kAutofillRejectCompanyBirthyearName[] =
    "Autofill Rejects Invalid Company Names";
const char kAutofillRejectCompanyBirthyearDescription[] =
    "Autofill rejects using non-verified company names that are in the "
    "format of a birthyear.";

const char kAutofillRestrictUnownedFieldsToFormlessCheckoutName[] =
    "Restrict formless form extraction";
const char kAutofillRestrictUnownedFieldsToFormlessCheckoutDescription[] =
    "Restrict extraction of formless forms to checkout flows";

const char kAutofillRichMetadataQueriesName[] =
    "Autofill - Rich metadata queries (Canary/Dev only)";
const char kAutofillRichMetadataQueriesDescription[] =
    "Transmit rich form/field metadata when querying the autofill server. "
    "This feature only works on the Canary and Dev channels.";

extern const char kAutofillSaveAndFillVPAName[] =
    "Offer save and autofill of UPI/VPA values";
extern const char kAutofillSaveAndFillVPADescription[] =
    "If enabled, when autofill recognizes a UPI/VPA value in a payment form, "
    "it will offer to save it. If saved, it will be offered for filling in "
    "fields which expect a VPA.";

const char kAutofillSettingsSplitByCardTypeName[] =
    "Autofill settings split by card type";
const char kAutofillSettingsSplitByCardTypeDescription[] =
    "When enabled, the cards in the payments settings will be split into two "
    "lists based on where they are stored.";

const char kAutofillUseImprovedLabelDisambiguationName[] =
    "Autofill Uses Improved Label Disambiguation";
const char kAutofillUseImprovedLabelDisambiguationDescription[] =
    "When enabled, the Autofill dropdown's suggestions' labels are displayed "
    "using the improved disambiguation format.";

const char kAutoScreenBrightnessName[] = "Auto Screen Brightness model";
const char kAutoScreenBrightnessDescription[] =
    "Uses Auto Screen Brightness model to adjust screen brightness based on "
    "ambient light";

const char kBackForwardCacheName[] = "Back-forward cache";
const char kBackForwardCacheDescription[] =
    "Enables back-forward cache. NOTE: this feature is highly experimental and "
    "will lead to various breakages, up to and including user data loss. "
    "Do not enable unless you work on this feature";

const char kBrowserTaskSchedulerName[] = "Task Scheduler";
const char kBrowserTaskSchedulerDescription[] =
    "Enables redirection of some task posting APIs to the task scheduler.";

const char kBundledConnectionHelpName[] = "Bundled Connection Help";
const char kBundledConnectionHelpDescription[] =
    "Enables or disables redirection to local help content for users who get "
    "an interstitial after clicking the 'Learn More' link on a previous "
    "interstitial.";

const char kBypassAppBannerEngagementChecksName[] =
    "Bypass user engagement checks";
const char kBypassAppBannerEngagementChecksDescription[] =
    "Bypasses user engagement checks for displaying app banners, such as "
    "requiring that users have visited the site before and that the banner "
    "hasn't been shown recently. This allows developers to test that other "
    "eligibility requirements for showing app banners, such as having a "
    "manifest, are met.";

const char kCaptionSettingsName[] = "Caption Settings";
const char kCaptionSettingsDescription[] =
    "Enable the ability to customize captions.";

const char kContextMenuSearchWithGoogleLensName[] =
    "Google Lens powered image search in the context menu.";
const char kContextMenuSearchWithGoogleLensDescription[] =
    "Replaces default image search with an intent to Google Lens when "
    "supported.";

const char kClickToOpenPDFName[] = "Click to open embedded PDFs";
const char kClickToOpenPDFDescription[] =
    "When the PDF plugin is unavailable, show a click-to-open placeholder for "
    "embedded PDFs.";

const char kCloudPrinterHandlerName[] = "Enable Cloud Printer Handler";
const char kCloudPrinterHandlerDescription[] =
    "Use the new cloud printer handler for communicating with the cloud "
    "print server, instead of the cloud print interface in the Print "
    "Preview WebUI.";

const char kDecodeJpeg420ImagesToYUVName[] = "YUV decoding for JPEG";
const char kDecodeJpeg420ImagesToYUVDescription[] =
    "Decode and render 4:2:0 formatted jpeg images from YUV instead of RGB."
    "This feature requires GPU or OOP rasterization to also be enabled.";

const char kDecodeLossyWebPImagesToYUVName[] = "YUV Decoding for WebP";
const char kDecodeLossyWebPImagesToYUVDescription[] =
    "Decode and render lossy WebP images from YUV instead of RGB. "
    "You must also have GPU rasterization or OOP rasterization.";

const char kEnablePasswordsAccountStorageName[] =
    "Enable the account data storage for passwords";
const char kEnablePasswordsAccountStorageDescription[] =
    "Enables storing passwords in a second, Gaia-account-scoped storage for "
    "signed-in but not syncing users";

const char kFocusMode[] = "Focus Mode";
const char kFocusModeDescription[] =
    "If enabled, allows the user to switch to Focus Mode";

const char kFontSrcLocalMatchingName[] =
    "Match @font-face { src: local(<name>) } names by PostScript and full font "
    "name.";
const char kFontSrcLocalMatchingDescription[] =
    "Match local() src attributes in @font-face declarations precisely by "
    "PostScript name and full font name instead of the previous behavior of "
    "matching those unspecifically as family names.";

const char kForceColorProfileSRGB[] = "sRGB";
const char kForceColorProfileP3[] = "Display P3 D65";
const char kForceColorProfileColorSpin[] = "Color spin with gamma 2.4";
const char kForceColorProfileSCRGBLinear[] =
    "scRGB linear (HDR where available)";
const char kForceColorProfileHDR10[] = "HDR10 (HDR where available)";

const char kForceColorProfileName[] = "Force color profile";
const char kForceColorProfileDescription[] =
    "Forces Chrome to use a specific color profile instead of the color "
    "of the window's current monitor, as specified by the operating system.";

const char kCompositedLayerBordersName[] = "Composited render layer borders";
const char kCompositedLayerBordersDescription[] =
    "Renders a border around composited Render Layers to help debug and study "
    "layer compositing.";

const char kCookieDeprecationMessagesName[] = "Cookie deprecation messages";
const char kCookieDeprecationMessagesDescription[] =
    "Show messages in the DevTools console about upcoming deprecations that "
    "would affect sent/received cookies.";

const char kCookiesWithoutSameSiteMustBeSecureName[] =
    "Cookies without SameSite must be secure";
const char kCookiesWithoutSameSiteMustBeSecureDescription[] =
    "If enabled, cookies without SameSite restrictions must also be Secure. If "
    "a cookie without SameSite restrictions is set without the Secure "
    "attribute, it will be rejected. This flag only has an effect if "
    "\"SameSite by default cookies\" is also enabled.";

const char kCooperativeSchedulingName[] = "Cooperative Scheduling";
const char kCooperativeSchedulingDescription[] =
    "Enables cooperative scheduling in Blink.";

const char kCreditCardAssistName[] = "Credit Card Assisted Filling";
const char kCreditCardAssistDescription[] =
    "Enable assisted credit card filling on certain sites.";

const char kDarkenWebsitesCheckboxInThemesSettingName[] =
    "Darken websites checkbox in themes setting";
const char kDarkenWebsitesCheckboxInThemesSettingDescription[] =
    "Show a darken websites checkbox in themes settings when system default or "
    "dark is selected. The checkbox can toggle the auto-darkening web contents "
    "feature";

const char kDataSaverServerPreviewsName[] = "Data Saver Server Previews";
const char kDataSaverServerPreviewsDescription[] =
    "Allow the Data Reduction Proxy to serve previews.";

const char kDebugPackedAppName[] = "Debugging for packed apps";
const char kDebugPackedAppDescription[] =
    "Enables debugging context menu options such as Inspect Element for packed "
    "applications.";

const char kDebugShortcutsName[] = "Debugging keyboard shortcuts";
const char kDebugShortcutsDescription[] =
    "Enables additional keyboard shortcuts that are useful for debugging Ash.";

const char kDeviceDiscoveryNotificationsName[] =
    "Device Discovery Notifications";
const char kDeviceDiscoveryNotificationsDescription[] =
    "Device discovery notifications on local network.";

const char kDevtoolsExperimentsName[] = "Developer Tools experiments";
const char kDevtoolsExperimentsDescription[] =
    "Enables Developer Tools experiments. Use Settings panel in Developer "
    "Tools to toggle individual experiments.";

const char kDisableAudioForDesktopShareName[] =
    "Disable Audio For Desktop Share";
const char kDisableAudioForDesktopShareDescription[] =
    "With this flag on, desktop share picker window will not let the user "
    "choose whether to share audio.";

const char kDisableBestEffortTasksName[] = "Skip best effort tasks";
const char kDisableBestEffortTasksDescription[] =
    "With this flag on, tasks of the lowest priority will not be executed "
    "until shutdown. The queue of low priority tasks can increase memory usage."
    "Also, while it should be possible to use Chrome almost normally with this "
    "flag, it is expected that some non-visible operations such as writing "
    "user data to disk, cleaning caches, reporting metrics or updating "
    "components won't be performed until shutdown.";

const char kDisableIpcFloodingProtectionName[] =
    "Disable IPC flooding protection";
const char kDisableIpcFloodingProtectionDescription[] =
    "Some javascript code can flood the inter process communication system. "
    "This protection limits the rate (calls/seconds) at which theses function "
    "can be used. This flag disables the protection. This flag is deprecated "
    "and will be removed in Chrome 76. Use the switch "
    "--disable-ipc-flooding-protection instead.";

const char kDisablePushStateThrottleName[] = "Disable pushState throttling";
const char kDisablePushStateThrottleDescription[] =
    "Disables throttling of history.pushState and history.replaceState method "
    "calls. This flag is deprecated and will be removed in Chrome 76. Use the "
    "switch --disable-ipc-flooding-protection instead.";

const char kDisallowDocWrittenScriptsUiName[] =
    "Block scripts loaded via document.write";
const char kDisallowDocWrittenScriptsUiDescription[] =
    "Disallows fetches for third-party parser-blocking scripts inserted into "
    "the main frame via document.write.";

const char kDisallowUnsafeHttpDownloadsName[] =
    "Block unsafe downloads over insecure connections";
const char kDisallowUnsafeHttpDownloadsNameDescription[] =
    "Disallows downloads of unsafe files (files that can potentially execute "
    "code), where the final download origin or any origin in the redirect "
    "chain is insecure.";

const char kDownloadResumptionWithoutStrongValidatorsName[] =
    "Allow download resumption without strong validators";
const char kDownloadResumptionWithoutStrongValidatorsDescription[] =
    "Allows download to resume instead of restarting from the begining if "
    "strong validators are not present.";

const char kEnableAccessibilityImageDescriptionsName[] =
    "Accessibility Image Descriptions";
const char kEnableAccessibilityImageDescriptionsDescription[] =
    "Enables screen reader users to request computer-generated descriptions "
    "of unlabeled images using the page context menu.";

const char kEnableAccessibilityObjectModelName[] =
    "Accessibility Object Model v0 (deprecated)";
const char kEnableAccessibilityObjectModelDescription[] =
    "Enables experimental support for an earlier version of Accessibility"
    "Object Model APIs that are now deprecated.";

const char kEnableAmbientAuthenticationInIncognitoName[] =
    "Enable Ambient Authentication in Incognito mode";
const char kEnableAmbientAuthenticationInIncognitoDescription[] =
    "Enables ambient authentication in Incognito mode. This flag may be "
    "overriden by policies.";

const char kEnableAmbientAuthenticationInGuestSessionName[] =
    "Enable Ambient Authentication in Guest session.";
const char kEnableAmbientAuthenticationInGuestSessionDescription[] =
    "Enables ambient authentication in Guest session. This flag may be "
    "overriden by policies.";

const char kEnableAudioFocusEnforcementName[] = "Audio Focus Enforcement";
const char kEnableAudioFocusEnforcementDescription[] =
    "Enables enforcement of a single media session having audio focus at "
    "any one time. Requires #enable-media-session-service to be enabled too.";

const char kEnableAutocompleteDataRetentionPolicyName[] =
    "Enable automatic cleanup of expired Autocomplete entries.";
const char kEnableAutocompleteDataRetentionPolicyDescription[] =
    "If enabled, will clean-up Autocomplete entries whose last use date is "
    "older than the current retention policy. These entries will be "
    "permanently deleted from the client on startup, and will be unlinked "
    "from sync.";

const char kEnableAutofillAccountWalletStorageName[] =
    "Enable the account data storage for autofill";
const char kEnableAutofillAccountWalletStorageDescription[] =
    "Enable the ephemeral storage for account data for autofill.";

const char kEnableAutofillCreditCardAblationExperimentDisplayName[] =
    "Credit card autofill ablation experiment.";
const char kEnableAutofillCreditCardAblationExperimentDescription[] =
    "If enabled, credit card autofill suggestions will not display.";

const char kEnableAutofillCreditCardAuthenticationName[] =
    "Allow using platform authenticators to retrieve server cards";
const char kEnableAutofillCreditCardAuthenticationDescription[] =
    "When enabled, users will be given the option to use a platform "
    "authenticator (if available) to verify card ownership when retrieving "
    "credit cards from Google Payments.";

const char kEnableAutofillCreditCardLastUsedDateDisplayName[] =
    "Display the last used date of a credit card in autofill.";
const char kEnableAutofillCreditCardLastUsedDateDisplayDescription[] =
    "If enabled, display the last used date of a credit card in autofill.";

const char kEnableAutofillCreditCardUploadEditableCardholderNameName[] =
    "Make cardholder name editable in dialog during credit card upload";
const char kEnableAutofillCreditCardUploadEditableCardholderNameDescription[] =
    "If enabled, in certain situations when offering credit card upload to "
    "Google Payments, the cardholder name can be edited within the "
    "offer-to-save dialog, which is prefilled with the name from the signed-in "
    "Google Account.";

const char kEnableAutofillCreditCardUploadEditableExpirationDateName[] =
    "Make expiration date editable in dialog during credit card upload";
const char kEnableAutofillCreditCardUploadEditableExpirationDateDescription[] =
    "If enabled, if a credit card's expiration date was not detected when "
    "offering card upload to Google Payments, the offer-to-save dialog "
    "displays an expiration date selector.";

const char kEnableAutofillCreditCardUploadFeedbackName[] =
    "Enable feedback for credit card upload flow";
const char kEnableAutofillCreditCardUploadFeedbackDescription[] =
    "When enabled, if credit card upload succeeds, the avatar button will "
    "show a highlight, otherwise the icon will be updated and if it is "
    "clicked, the save card failure bubble will be shown.";

const char kEnableAutofillDoNotMigrateUnsupportedLocalCardsName[] =
    "Prevents local card migration on local cards from unsupported networks";
const char kEnableAutofillDoNotMigrateUnsupportedLocalCardsDescription[] =
    "If enabled, local cards from unsupported networks will not be offered "
    "local card migration.";

const char kEnableAutofillNativeDropdownViewsName[] =
    "Display Autofill Dropdown Using Views";
const char kEnableAutofillNativeDropdownViewsDescription[] =
    "If enabled, the Autofill Dropdown will be built natively using Views, "
    "rather than painted directly to a canvas.";

const char kEnableAutofillUpdatedCardUnmaskPromptUiName[] =
    "Enable new card unmask prompt UI";
const char kEnableAutofillUpdatedCardUnmaskPromptUiDescription[] =
    "If enabled, shows the updated card unmask prompt when performing CVC "
    "verification.";

const char kEnableDeferAllScriptName[] = "DeferAllScript previews";
const char kEnableDeferAllScriptDescription[] =
    "Enable deferring synchronous script on slow pages.";

const char kEnableDeferAllScriptWithoutOptimizationHintsName[] =
    "Skip checking optimization hints for Defer Script previews";
const char kEnableDeferAllScriptWithoutOptimizationHintsDescription[] =
    "Skips checking optimization hints for Defer Script previews and assumes "
    "that the ECT trigger threshold is set to 4G (which is otherwise provided "
    "by the optimization hints). Rest of the checks are still executed.";

extern const char kEnableEduCoexistenceName[] =
    "Enable Family Link managed accounts and EDU accounts coexistence";
extern const char kEnableEduCoexistenceDescription[] =
    "Allows Family Link managed users to add secondary EDU accounts.";

const char kEnableSaveDataName[] = "Enables save data feature";
const char kEnableSaveDataDescription[] =
    "Enables save data feature. May cause user's traffic to be proxied via "
    "Google's data reduction proxy.";

const char kEnableNoScriptPreviewsName[] = "NoScript previews";

const char kEnableNoScriptPreviewsDescription[] =
    "Enable disabling JavaScript on some pages on slow networks.";

const char kEnableRemovingAllThirdPartyCookiesName[] =
    "Enable removing SameSite=None cookies";
const char kEnableRemovingAllThirdPartyCookiesDescription[] =
    "Enables UI on chrome://settings/siteData to remove all third-party "
    "cookies and site data.";

const char kDataReductionProxyServerAlternative1[] = "Use alt. server config 1";
const char kDataReductionProxyServerAlternative2[] = "Use alt. server config 2";
const char kDataReductionProxyServerAlternative3[] = "Use alt. server config 3";
const char kDataReductionProxyServerAlternative4[] = "Use alt. server config 4";
const char kDataReductionProxyServerAlternative5[] = "Use alt. server config 5";
const char kDataReductionProxyServerAlternative6[] = "Use alt. server config 6";
const char kDataReductionProxyServerAlternative7[] = "Use alt. server config 7";
const char kDataReductionProxyServerAlternative8[] = "Use alt. server config 8";
const char kDataReductionProxyServerAlternative9[] = "Use alt. server config 9";
const char kDataReductionProxyServerAlternative10[] =
    "Use alt. server config 10";
const char kEnableDataReductionProxyNetworkServiceName[] =
    "Data reduction proxy with network service";
const char kEnableDataReductionProxyNetworkServiceDescription[] =
    "Enable data reduction proxy when network service is enabled";
const char kEnableDataReductionProxyServerExperimentName[] =
    "Use an alternative Data Saver back end configuration.";
const char kEnableDataReductionProxyServerExperimentDescription[] =
    "Enable a different approach to saving data by configuring the back end "
    "server";

const char kDesktopMinimalUIName[] = "Desktop PWAs support minimal-ui";
const char kDesktopMinimalUIDescription[] =
    "PWAs with display mode minimal-ui open in a window with Back and Refresh "
    "buttons.";

const char kEnableDesktopPWAsName[] = "Desktop PWAs";
const char kEnableDesktopPWAsDescription[] =
    "Experimental windowing and install banner treatment for Progressive Web "
    "Apps on desktop platforms. Implies #enable-experimental-app-banners.";

extern const char kDesktopPWAsLocalUpdatingName[] =
    "Desktop PWAs local updating";
extern const char kDesktopPWAsLocalUpdatingDescription[] =
    "Enable installed PWAs to update their app manifest data when the site "
    "manifest data has changed.";

const char kDesktopPWAsOmniboxInstallName[] =
    "Desktop PWAs installable from the omnibox";
const char kDesktopPWAsOmniboxInstallDescription[] =
    "When on a site that passes PWA installation requirements show a button in "
    "the omnibox for installing it.";

const char kEnableSystemWebAppsName[] = "System Web Apps";
const char kEnableSystemWebAppsDescription[] =
    "Experimental system for using the Desktop PWA framework for running System"
    "Apps (e.g Settings, Discover).";

const char kEnableTLS13EarlyDataName[] = "TLS 1.3 Early Data";
const char kEnableTLS13EarlyDataDescription[] =
    "This option enables TLS 1.3 Early Data, allowing GET requests to be sent "
    "during the handshake when resuming a connection to a compatible TLS 1.3 "
    "server.";

const char kWinrtGeolocationImplementationName[] =
    "WinRT Geolocation Implementation";
const char kWinrtGeolocationImplementationDescription[] =
    "Enables usage of the Windows.Devices.Geolocation WinRT APIs on Windows "
    "for geolocation";

const char kWinrtSensorsImplementationName[] = "WinRT Sensor Implementation";
const char kWinrtSensorsImplementationDescription[] =
    "Enables usage of the Windows.Devices.Sensors WinRT APIs on Windows for "
    "sensors";

const char kEnableGenericSensorExtraClassesName[] =
    "Generic Sensor Extra Classes";
const char kEnableGenericSensorExtraClassesDescription[] =
    "Enables an extra set of sensor classes based on Generic Sensor API, which "
    "expose previously unavailable platform features, i.e. AmbientLightSensor "
    "and Magnetometer interfaces.";

const char kEnableGpuServiceLoggingName[] = "Enable gpu service logging";
const char kEnableGpuServiceLoggingDescription[] =
    "Enable printing the actual GL driver calls.";

const char kEnableHistoryFaviconsGoogleServerQueryName[] =
    "Enable History Favicons Google Server Query";
const char kEnableHistoryFaviconsGoogleServerQueryDescription[] =
    "Allow retrieving favicons of non-local entries in the history WebUIs and "
    "the recent tabs menu using a Google server instead of Sync.";

const char kEnableImplicitRootScrollerName[] = "Implicit Root Scroller";
const char kEnableImplicitRootScrollerDescription[] =
    "Enables implicitly choosing which scroller on a page is the 'root "
    "scroller'. i.e. The one that gets special features like URL bar movement, "
    "overscroll glow, rotation anchoring, etc.";

const char kEnableCSSOMViewScrollCoordinatesName[] =
    "CSSOM View Scroll Coordinates";
const char kEnableCSSOMViewScrollCoordinatesDescription[] =
    "Enables CSSOM View Scroll Coordinates, this affects to box scroll "
    "coordinates in scrollTop / scrollLeft / scrollTo' when ScrollOrigin isn't "
    "at the left top corner. i.e. For leftwards overflow direction box "
    "the X coordinate will start from 0 to negative value. For upwards box the "
    "Y coordinate will start from 0 to negative value. And for other directions"
    "(rightwards and downwards) the value will start from 0 to positive";

const char kEnableLitePageServerPreviewsName[] = "Lite Page Server Previews";
const char kEnableLitePageServerPreviewsDescription[] =
    "Enable showing Lite Page Previews served from a Previews Server."
    "This feature will cause Chrome to redirect eligible navigations "
    "to a Google-owned domain that serves a pre-rendered version of the "
    "original page. Also known as Lite Page Redirect Previews.";

const char kEnablePreviewsCoinFlipName[] = "Enable Previews Coin Flip";
const char kEnablePreviewsCoinFlipDescription[] =
    "Enable coin flip experimentation of Previews.";

const char kBuiltInModuleAllName[] = "All experimental built-in modules";
const char kBuiltInModuleAllDescription[] =
    "Enable all experimental built-in modules, as well as built-in module "
    "infrastructure and import maps. The syntax and the APIs exposed are "
    "experimental and will change over time.";

const char kBuiltInModuleInfraName[] = "Built-in module infra and import maps";
const char kBuiltInModuleInfraDescription[] =
    "Enable built-in module infrastructure and import maps. Individual "
    "built-in modules should be enabled by other flags. The syntax and the "
    "APIs exposed are experimental and will change over time.";

const char kBuiltInModuleKvStorageName[] = "kv-storage built-in module";
const char kBuiltInModuleKvStorageDescription[] =
    "Enable kv-storage built-in module, as well as built-in module "
    "infrastructure and import maps. The syntax and the APIs exposed are "
    "experimental and will change over time.";

const char kDownloadAutoResumptionNativeName[] =
    "Enable download auto-resumption in native";
const char kDownloadAutoResumptionNativeDescription[] =
    "Enables download auto-resumption in native";

const char kEnableDisplayLockingName[] = "Enable Display Locking";
const char kEnableDisplayLockingDescription[] =
    "Enable Display Locking JavaScript API. The syntax and the APIs exposed "
    "are experimental and may change.";

const char kEnableLayoutNGName[] = "Enable LayoutNG";
const char kEnableLayoutNGDescription[] =
    "Enable Blink's next generation layout engine.";

const char kEnableLazyFrameLoadingName[] = "Enable lazy frame loading";
const char kEnableLazyFrameLoadingDescription[] =
    "Defers the loading of iframes marked with the attribute 'loading=lazy' "
    "until the page is scrolled down near them.";

const char kEnableLazyImageLoadingName[] = "Enable lazy image loading";
const char kEnableLazyImageLoadingDescription[] =
    "Defers the loading of images marked with the attribute 'loading=lazy' "
    "until the page is scrolled down near them.";

const char kEnableMacMaterialDesignDownloadShelfName[] =
    "Enable Material Design download shelf";

const char kEnableMacMaterialDesignDownloadShelfDescription[] =
    "If enabled, the download shelf uses Material Design.";

const char kEnableMediaSessionServiceName[] = "Media Session Service";
const char kEnableMediaSessionServiceDescription[] =
    "Enables the media session mojo service and internal media session "
    "support.";

const char kEnableNavigationTracingName[] = "Enable navigation tracing";
const char kEnableNavigationTracingDescription[] =
    "This is to be used in conjunction with the trace-upload-url flag. "
    "WARNING: When enabled, Chrome will record performance data for every "
    "navigation and upload it to the URL specified by the trace-upload-url "
    "flag. The trace may include personally identifiable information (PII) "
    "such as the titles and URLs of websites you visit.";

const char kEnableNetworkLoggingToFileName[] = "Enable network logging to file";
const char kEnableNetworkLoggingToFileDescription[] =
    "Enables network logging to a file named netlog.json in the user data "
    "directory. The file can be imported into chrome://net-internals.";

const char kEnableNetworkServiceInProcessName[] =
    "Runs network service in-process";
const char kEnableNetworkServiceInProcessDescription[] =
    "Runs the network service in the browser process.";

const char kEnableNewDownloadBackendName[] = "Enable new download backend";
const char kEnableNewDownloadBackendDescription[] =
    "Enables the new download backend that uses offline content provider";

const char kEnablePortalsName[] = "Enable Portals.";
const char kEnablePortalsDescription[] =
    "Portals are an experimental web platform feature that allows embedding"
    " and seamless transitions between pages."
    " See https://github.com/WICG/portals and https://wicg.github.io/portals/";

const char kEnableNotificationScrollBarName[] =
    "Enable notification list scroll bar";
const char kEnableNotificationScrollBarDescription[] =
    "Enable the scroll bar of the notification list in Unified System Tray.";

const char kEnableNotificationExpansionAnimationName[] =
    "Enable notification expansion animations";
const char kEnableNotificationExpansionAnimationDescription[] =
    "Enable notification animations whenever the expansion state is toggled.";

const char kEnableOutOfBlinkCorsName[] = "Out of blink CORS";
const char kEnableOutOfBlinkCorsDescription[] =
    "CORS handling logic is moved out of blink.";

const char kCrossOriginIsolationName[] = "Cross Origin Isolation";
const char kCrossOriginIsolationDescription[] =
    "Enable Cross Origin Opener Policy and Cross Origin Embedder Policy.";

const char kDisableKeepaliveFetchName[] = "Disable fetch with keepalive set";
const char kDisableKeepaliveFetchDescription[] =
    "Disable fetch with keepalive set "
    "(https://fetch.spec.whatwg.org/#request-keepalive-flag).";

const char kExperimentalAccessibilityFeaturesName[] =
    "Experimental accessibility features";
const char kExperimentalAccessibilityFeaturesDescription[] =
    "Enable additional accessibility features in the Settings page.";

const char kExperimentalAccessibilityAutoclickName[] =
    "Experimental automatic click features";
const char kExperimentalAccessibilityAutoclickDescription[] =
    "Enable additional features for automatic clicks.";

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

const char kVizDisplayCompositorName[] = "Viz Display Compositor (OOP-D)";
const char kVizDisplayCompositorDescription[] =
    "If enabled, the display compositor runs as part of the viz service in the"
    "GPU process.";

const char kVizHitTestName[] = "Viz Hit-test SurfaceLayer";
const char kVizHitTestDescription[] =
    "If enabled, event targeting uses hit-test data computed from the "
    "SurfaceLayer.";

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
    "Default value for the interval between samples is 100000 (100KB). "
    "This results in low noise for large and/or frequent allocations "
    "[size * frequency >> 100KB]. This means that aggregate numbers [e.g. "
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
const char kMemlogStackModeMixed[] = "Mixed";
const char kMemlogStackModeNative[] = "Native";
const char kMemlogStackModeNativeWithThreadNames[] = "Native with thread names";
const char kMemlogStackModePseudo[] = "Trace events";

const char kEnablePixelCanvasRecordingName[] = "Enable pixel canvas recording";
const char kEnablePixelCanvasRecordingDescription[] =
    "Pixel canvas recording allows the compositor to raster contents aligned "
    "with the pixel and improves text rendering. This should be enabled when a "
    "device is using fractional scale factor.";

const char kEnableResamplingInputEventsName[] =
    "Enable resampling input events";
const char kEnableResamplingInputEventsDescription[] =
    "Predicts mouse and touch inputs position at rAF time based on previous "
    "input";
const char kEnableResamplingScrollEventsName[] =
    "Enable resampling scroll events";
const char kEnableResamplingScrollEventsDescription[] =
    "Predicts the scroll amount at vsync time based on previous input";

const char kEnableResourceLoadingHintsName[] = "Enable resource loading hints";
const char kEnableResourceLoadingHintsDescription[] =
    "Enable using server-provided resource loading hints to provide a preview "
    "over slow network connections.";

const char kEnableSyncUSSNigoriName[] = "Enable USS for sync encryption keys";
const char kEnableSyncUSSNigoriDescription[] =
    "Enables the new, experimental implementation of sync encryption keys";

const char kEnableSyncUSSPasswordsName[] = "Enable USS for passwords sync";
const char kEnableSyncUSSPasswordsDescription[] =
    "Enables the new, experimental implementation of passwords sync";

const char kEnableSyncUSSSessionsName[] = "Enable USS for sessions sync";
const char kEnableSyncUSSSessionsDescription[] =
    "Enables the new, experimental implementation of session sync (aka tab "
    "sync).";

const char kEnableTextFragmentAnchorName[] = "Enable Text Fragment Anchor.";
const char kEnableTextFragmentAnchorDescription[] =
    "Enables scrolling to text specified in URL's fragment.";

const char kEnableUseZoomForDsfName[] =
    "Use Blink's zoom for device scale factor.";
const char kEnableUseZoomForDsfDescription[] =
    "If enabled, Blink uses its zooming mechanism to scale content for device "
    "scale factor.";
const char kEnableUseZoomForDsfChoiceDefault[] = "Default";
const char kEnableUseZoomForDsfChoiceEnabled[] = "Enabled";
const char kEnableUseZoomForDsfChoiceDisabled[] = "Disabled";

const char kEnableScrollAnchorSerializationName[] =
    "Scroll Anchor Serialization";
const char kEnableScrollAnchorSerializationDescription[] =
    "Save the scroll anchor and use it to restore the scroll position when "
    "navigating.";

const char kEnableSharedArrayBufferName[] =
    "Experimental enabled SharedArrayBuffer support in JavaScript.";
const char kEnableSharedArrayBufferDescription[] =
    "Enable SharedArrayBuffer support in JavaScript.";

const char kEnableSubresourceRedirectName[] =
    "Enable Render Level Data Use Optimization";
const char kEnableSubresourceRedirectDescription[] =
    "Allow Render Level Data Use Optimization";

const char kEnableWasmName[] = "WebAssembly structured cloning support.";
const char kEnableWasmDescription[] =
    "Enable web pages to use WebAssembly structured cloning.";

const char kEnableWebAuthenticationCableV2SupportName[] =
    "Web Authentication caBLE v2 support";
const char kEnableWebAuthenticationCableV2SupportDescription[] =
    "Enable the QR-based pairingless BLE protocol for use with the Web "
    "Authentication API. (This will also enable the cloud-based protocol "
    "on platforms where it is not already enabled.)";

const char kExperimentalWebAssemblyFeaturesName[] = "Experimental WebAssembly";
const char kExperimentalWebAssemblyFeaturesDescription[] =
    "Enable web pages to use experimental WebAssembly features.";

const char kEnableWasmBaselineName[] = "WebAssembly baseline compiler";
const char kEnableWasmBaselineDescription[] =
    "Enables WebAssembly baseline compilation and tier up.";

const char kEnableWasmCodeCacheName[] = "WebAssembly compiled module cache";
const char kEnableWasmCodeCacheDescription[] =
    "Enables caching of compiled WebAssembly modules.";

const char kEnableWasmCodeGCName[] = "WebAssembly code garbage collection";
const char kEnableWasmCodeGCDescription[] =
    "Enables garbage collection of WebAssembly code.";

const char kEnableWasmSimdName[] = "WebAssembly SIMD support.";
const char kEnableWasmSimdDescription[] =
    "Enables support for the WebAssembly SIMD proposal.";

const char kEnableWasmThreadsName[] = "WebAssembly threads support";
const char kEnableWasmThreadsDescription[] =
    "Enables support for the WebAssembly Threads proposal. Implies "
    "#shared-array-buffer and #enable-webassembly.";

const char kEvDetailsInPageInfoName[] = "EV certificate details in Page Info.";
const char kEvDetailsInPageInfoDescription[] =
    "Shows the EV certificate details in the Page Info bubble.";

const char kExpensiveBackgroundTimerThrottlingName[] =
    "Throttle expensive background timers";
const char kExpensiveBackgroundTimerThrottlingDescription[] =
    "Enables intervention to limit CPU usage of background timers to 1%.";

const char kExperimentalCanvasFeaturesName[] = "Experimental canvas features";
const char kExperimentalCanvasFeaturesDescription[] =
    "Enables the use of experimental canvas features which are still in "
    "development.";

const char kExperimentalExtensionApisName[] = "Experimental Extension APIs";
const char kExperimentalExtensionApisDescription[] =
    "Enables experimental extension APIs. Note that the extension gallery "
    "doesn't allow you to upload extensions that use experimental APIs.";

const char kExperimentalProductivityFeaturesName[] =
    "Experimental Productivity Features";
const char kExperimentalProductivityFeaturesDescription[] =
    "Enable support for experimental developer productivity features, such as "
    "built-in modules and policies for avoiding slow rendering.";

const char kExperimentalSecurityFeaturesName[] =
    "Potentially annoying security features";
const char kExperimentalSecurityFeaturesDescription[] =
    "Enables several security features that will likely break one or more "
    "pages that you visit on a daily basis. Strict mixed content checking, for "
    "example. And locking powerful features to secure contexts. This flag will "
    "probably annoy you.";

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

const char kExtensionsToolbarMenuName[] = "Extensions Toolbar Menu";
const char kExtensionsToolbarMenuDescription[] =
    "Enable a separate toolbar button and menu for extensions";

const char kExtensionsOnChromeUrlsName[] = "Extensions on chrome:// URLs";
const char kExtensionsOnChromeUrlsDescription[] =
    "Enables running extensions on chrome:// URLs, where extensions explicitly "
    "request this permission.";

const char kFeaturePolicyName[] = "Feature Policy";
const char kFeaturePolicyDescription[] =
    "Enables granting and removing access to features through the "
    "Feature-Policy HTTP header.";

const char kFilteringScrollPredictionName[] = "Filtering scroll prediction";
const char kFilteringScrollPredictionDescription[] =
    "Enable filtering of predicted scroll events";

const char kFractionalScrollOffsetsName[] = "Fractional Scroll Offsets";
const char kFractionalScrollOffsetsDescription[] =
    "Enables fractional scroll offsets inside Blink, exposing non-integer "
    "offsets to web APIs.";

const char kForceEffectiveConnectionTypeName[] =
    "Override effective connection type";
const char kForceEffectiveConnectionTypeDescription[] =
    "Overrides the effective connection type of the current connection "
    "returned by the network quality estimator. Slow 2G on Cellular returns "
    "Slow 2G when connected to a cellular network, and the actual estimate "
    "effective connection type when not on a cellular network. Previews are "
    "usually served on 2G networks.";
const char kEffectiveConnectionTypeUnknownDescription[] = "Unknown";
const char kEffectiveConnectionTypeOfflineDescription[] = "Offline";
const char kEffectiveConnectionTypeSlow2GDescription[] = "Slow 2G";
const char kEffectiveConnectionTypeSlow2GOnCellularDescription[] =
    "Slow 2G On Cellular";
const char kEffectiveConnectionType2GDescription[] = "2G";
const char kEffectiveConnectionType3GDescription[] = "3G";
const char kEffectiveConnectionType4GDescription[] = "4G";

const char kFileHandlingAPIName[] = "File Handling API";
const char kFileHandlingAPIDescription[] =
    "Enables the file handling API, allowing websites to register as file "
    "handlers. This depends on native-file-system";

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

const char kFormControlsRefreshName[] = "Web Platform Controls updated UI";
const char kFormControlsRefreshDescription[] =
    "If enabled, HTML forms elements will be rendered using an updated style.";

const char kGlobalMediaControlsName[] = "Global Media Controls";
const char kGlobalMediaControlsDescription[] =
    "Enables the Global Media Controls UI in the toolbar.";

const char kGpuRasterizationName[] = "GPU rasterization";
const char kGpuRasterizationDescription[] =
    "Use GPU to rasterize web content. Requires impl-side painting.";
const char kForceGpuRasterization[] = "Force-enabled for all layers";

const char kGooglePasswordManagerName[] = "Google Password Manager UI";
const char kGooglePasswordManagerDescription[] =
    "Enables access to the Google Password Manager UI from Chrome.";

const char kGoogleProfileInfoName[] = "Google profile name and icon";
const char kGoogleProfileInfoDescription[] =
    "Enables using Google information to populate the profile name and icon in "
    "the avatar menu.";

const char kHandwritingGestureName[] = "Handwriting Gestures";
const char kHandwritingGestureDescription[] =
    "Enables handwriting gestures within the virtual keyboard";

const char kHardwareMediaKeyHandling[] = "Hardware Media Key Handling";
const char kHardwareMediaKeyHandlingDescription[] =
    "Enables using media keys to control the active media session. This "
    "requires MediaSessionService to be enabled too";

const char kHarfBuzzPDFSubsetterName[] = "HarfBuzz PDF Subsetter";
const char kHarfBuzzPDFSubsetterDescription[] =
    "Changes the PDF subsetter from sftnly to HarfBuzz.";

const char kHeavyAdPrivacyMitigationsOptOutName[] =
    "Disable heavy ad privacy mitigations";
const char kHeavyAdPrivacyMitigationsOptOutDescription[] =
    "Disables privacy mitigations for the heavy ad intervention. This makes "
    "the intervention deterministic."
    "This is intended to be used for debugging only.";

const char kHeavyAdInterventionName[] = "Heavy Ad Intervention";
const char kHeavyAdInterventionDescription[] =
    "Unloads ads that use too many device resources.";

const char kHideActiveAppsFromShelfName[] =
    "Hide running apps (that are not pinned) from the shelf";
const char kHideActiveAppsFromShelfDescription[] =
    "Save space in the shelf by hiding running apps (that are not pinned).";

const char kHorizontalTabSwitcherAndroidName[] =
    "Enable horizontal tab switcher";
const char kHorizontalTabSwitcherAndroidDescription[] =
    "Changes the layout of the Android tab switcher so tabs scroll "
    "horizontally instead of vertically.";

const char kTabSwitcherOnReturnName[] = "Enable tab switcher on return";
const char kTabSwitcherOnReturnDescription[] =
    "Enable tab switcher on return after specified time has elapsed";

const char kHostedAppQuitNotificationName[] =
    "Quit notification for hosted apps";
const char kHostedAppQuitNotificationDescription[] =
    "Display a notification when quitting Chrome if hosted apps are currently "
    "running.";

const char kHostedAppShimCreationName[] =
    "Creation of app shims for hosted apps on Mac";
const char kHostedAppShimCreationDescription[] =
    "Create app shims on Mac when creating a hosted app.";

const char kHTTPAuthCommittedInterstitialsName[] =
    "Enable Committed Interstitials for HTTP Auth";
const char kHTTPAuthCommittedInterstitialsDescription[] =
    "Use committed error pages instead of transient navigation entries "
    "for HTTP auth interstitial pages.";

const char kIconNtpName[] = "Large icons on the New Tab page";
const char kIconNtpDescription[] =
    "Enable the experimental New Tab page using large icons.";

const char kIgnoreGpuBlacklistName[] = "Override software rendering list";
const char kIgnoreGpuBlacklistDescription[] =
    "Overrides the built-in software rendering list and enables "
    "GPU-acceleration on unsupported system configurations.";

const char kIgnorePreviewsBlacklistName[] = "Ignore Previews Blocklist";
const char kIgnorePreviewsBlacklistDescription[] =
    "Ignore decisions made by the PreviewsBlockList";

const char kIgnoreLitePageRedirectHintsBlacklistName[] =
    "Ignore Lite Page Redirect Preview Optimization Hints Blacklist";
const char kIgnoreLitePageRedirectHintsBlacklistDescription[] =
    "Ignore blacklist decisions made by Optimization Hints for Lite Page "
    "Redirect previews";

const char kInProductHelpDemoModeChoiceName[] = "In-Product Help Demo Mode";
const char kInProductHelpDemoModeChoiceDescription[] =
    "Selects the In-Product Help demo mode.";

const char kJavascriptHarmonyName[] = "Experimental JavaScript";
const char kJavascriptHarmonyDescription[] =
    "Enable web pages to use experimental JavaScript features.";

const char kJavascriptHarmonyShippingName[] =
    "Latest stable JavaScript features";
const char kJavascriptHarmonyShippingDescription[] =
    "Some web pages use legacy or non-standard JavaScript extensions that may "
    "conflict with the latest JavaScript features. This flag allows disabling "
    "support of those features for compatibility with such pages.";

const char kKeepAliveRendererForKeepaliveRequestsName[] =
    "Keep a renderer alive for keepalive fetch requests";
const char kKeepAliveRendererForKeepaliveRequestsDescription[] =
    "Keep a render process alive when the process has a pending fetch request "
    "with `keepalive' specified.";

const char kLegacyTLSWarningsName[] =
    "Show security warnings for sites using legacy TLS versions";
const char kLegacyTLSWarningsDescription[] =
    "Show security warnings for sites that use legacy TLS versions (TLS 1.0 "
    "and TLS 1.1), which are deprecated and will be removed in the future.";

const char kLoadMediaRouterComponentExtensionName[] =
    "Load Media Router Component Extension";
const char kLoadMediaRouterComponentExtensionDescription[] =
    "Loads the Media Router component extension at startup.";

const char kLogJsConsoleMessagesName[] =
    "Log JS console messages in system logs";
const char kLogJsConsoleMessagesDescription[] =
    "Enable logging JS console messages in system logs, please note that they "
    "may contain PII.";

const char kLookalikeUrlNavigationSuggestionsName[] =
    "Navigation suggestions for lookalike URLs";
const char kLookalikeUrlNavigationSuggestionsDescription[] =
    "Enable navigation suggestions for URLs that are visually similar to "
    "popular domains or to domains with a site engagement score.";

const char kMarkHttpAsName[] = "Mark non-secure origins as non-secure";
const char kMarkHttpAsDescription[] = "Change the UI treatment for HTTP pages";

extern const char kMediaInspectorLoggingName[] =
    "Enable media log in developer tools";
extern const char kMediaInspectorLoggingDescription[] =
    "Move media logging from chrome://media-internals into the developer tools "
    "project; #enable-devtools-experiments must also be enabled as well on "
    "desktop platforms";

const char kMediaRouterCastAllowAllIPsName[] =
    "Connect to Cast devices on all IP addresses";
const char kMediaRouterCastAllowAllIPsDescription[] =
    "Have the Media Router connect to Cast devices on all IP addresses, not "
    "just RFC1918/RFC4193 private addresses.";

const char kMimeHandlerViewInCrossProcessFrameName[] =
    "MimeHandlerView in cross-process frame";
const char kMimeHandlerViewInCrossProcessFrameDescription[] =
    "Loads the MimeHandlerView (the extension viewer for certain MIME types "
    "such as PDF) in a cross-process frame as opposed to a BrowserPlugin.";

const char kMixBrowserTypeTabsName[] = "Mix browser type tabs";
const char kMixBrowserTypeTabsDescription[] =
    "Allows tabs to be dragged between any browsers that support tabs, "
    "including apps";

const char kMixedContentSiteSettingName[] =
    "Blockable mixed content switch as site setting";
const char kMixedContentSiteSettingDescription[] =
    "Removes the blockable mixed content shield, and adds an 'Insecure "
    "Content' site setting to allow blockable mixed content.";

const char kMobileIdentityConsistencyName[] = "Mobile identity consistency";
const char kMobileIdentityConsistencyDescription[] =
    "Enables stronger identity consistency on mobile";

const char kMouseSubframeNoImplicitCaptureName[] =
    "Disable mouse implicit capture for iframe";
const char kMouseSubframeNoImplicitCaptureDescription[] =
    "When enable, mouse down does not implicit capture for iframe.";

const char kNativeFileSystemAPIName[] = "Native File System API";
const char kNativeFileSystemAPIDescription[] =
    "Enables the experimental Native File System API, giving websites access "
    "to the native file system";

const char kNewAudioRenderingMixingStrategyName[] =
    "New audio rendering mixing strategy";
const char kNewAudioRenderingMixingStrategyDescription[] =
    "Use the new audio rendering mixing strategy.";

const char kNewBookmarkAppsName[] = "The new bookmark app system";
const char kNewBookmarkAppsDescription[] =
    "Enables the new system for creating bookmark apps.";

const char kNewPasswordFormParsingName[] =
    "New password form parsing for filling passwords";
const char kNewPasswordFormParsingDescription[] =
    "Replaces existing form parsing for filling in password manager with a new "
    "version, currently under development. WARNING: when enabled, Password "
    "Manager might stop working";

const char kNewPasswordFormParsingForSavingName[] =
    "New password form parsing for saving passwords";
const char kNewPasswordFormParsingForSavingDescription[] =
    "Replaces existing form parsing for saving in password manager with a new "
    "version, currently under development. WARNING: when enabled, Password "
    "Manager might stop working";

const char kUseSurfaceLayerForVideoName[] =
    "Enable the use of SurfaceLayer objects for videos.";
const char kUseSurfaceLayerForVideoDescription[] =
    "Enable compositing onto a Surface instead of a VideoLayer "
    "for videos.";

const char kNewUsbBackendName[] = "Enable new USB backend";
const char kNewUsbBackendDescription[] =
    "Enables the new experimental USB backend for Windows.";

const char kNewblueName[] = "Newblue";
const char kNewblueDescription[] =
    "Enables the use of Bluetooth dispatcher daemon.";

const char kNewTabstripAnimationName[] = "New tabstrip animations";
const char kNewTabstripAnimationDescription[] =
    "New implementation of tabstrip animations.";

const char kNotificationIndicatorName[] = "Notification Indicators";
const char kNotificationIndicatorDescription[] =
    "Enable notification indicators, which appear on app icons when a "
    "notification is active. This will also enable notifications in context "
    "menus.";

const char kNotificationSchedulerDebugOptionName[] =
    "Notification scheduler debug options";
const char kNotificationSchedulerDebugOptionDescription[] =
    "Enable debugging mode to override certain behavior of notification "
    "scheduler system for easier manual testing.";
const char kNotificationSchedulerImmediateBackgroundTaskDescription[] =
    "Show scheduled notification right away.";

const char kNotificationsNativeFlagName[] = "Enable native notifications.";
const char kNotificationsNativeFlagDescription[] =
    "Enable support for using the native notification toasts and notification "
    "center on platforms where these are available.";

const char kUpdateHoverAtBeginFrameName[] = "Update hover at the begin frame";
const char kUpdateHoverAtBeginFrameDescription[] =
    "Recompute hover state at BeginFrame for layout and scroll based mouse "
    "moves, rather than old timing-based mechanism.";

const char kUseMultiloginEndpointName[] = "Use Multilogin endpoint.";
const char kUseMultiloginEndpointDescription[] =
    "Use Gaia OAuth multilogin for identity consistency.";

const char kOfferStoreUnmaskedWalletCardsName[] =
    "Google Payments card saving checkbox";
const char kOfferStoreUnmaskedWalletCardsDescription[] =
    "Show the checkbox to offer local saving of a credit card downloaded from "
    "the server.";

const char kOmniboxAutocompleteTitlesName[] = "Omnibox Autocomplete Titles";
const char kOmniboxAutocompleteTitlesDescription[] =
    "Allows autocompleting bookmark, history, and document suggestions when the"
    " user input is a prefix of their titles, as opposed to their URLs.";

const char kOmniboxDisplayTitleForCurrentUrlName[] =
    "Include title for the current URL in the omnibox";
const char kOmniboxDisplayTitleForCurrentUrlDescription[] =
    "In the event that the omnibox provides suggestions on-focus, the URL of "
    "the current page is provided as the first suggestion without a title. "
    "Enabling this flag causes the title to be displayed.";

const char kOmniboxDisableInstantExtendedLimitName[] =
    "Disable the 'instant extended' limit on search suggestions";
const char kOmniboxDisableInstantExtendedLimitDescription[] =
    "Effectively doubles the max number of Google-provided search suggestions "
    "on Android by disabling the 'Instant Extended' check.";

const char kOmniboxExperimentalSuggestScoringName[] =
    "Omnibox Experimental Suggest Scoring";
const char kOmniboxExperimentalSuggestScoringDescription[] =
    "Enables an experimental scoring mode for suggestions when Google is the "
    "default search engine.";

const char kOmniboxGroupSuggestionsBySearchVsUrlName[] =
    "Omnibox Group Suggestions By Search vs URL";
const char kOmniboxGroupSuggestionsBySearchVsUrlDescription[] =
    "Group suggestions by major type, search then navigation, except for "
    "the default match which must be first.";

const char kOmniboxLocalEntitySuggestionsName[] =
    "Omnibox Local Entity Suggestions";
const char kOmniboxLocalEntitySuggestionsDescription[] =
    "Enables location specific suggestions displayed with images and enhanced "
    "layout, similar to #omnibox-rich-entity-suggestions. Enabling this feature"
    "will also enable #omnibox-rich-entity-suggestions.";

const char kOmniboxMaterialDesignWeatherIconsName[] =
    "Omnibox Material Design Weather Icons";
const char kOmniboxMaterialDesignWeatherIconsDescription[] =
    "Use material design weather icons in the omnibox when displaying weather "
    "answers.";

const char kOmniboxOnFocusSuggestionsName[] = "Omnibox on-focus suggestions";
const char kOmniboxOnFocusSuggestionsDescription[] =
    "Configures Omnibox on-focus suggestions - suggestions displayed on-focus "
    "before the user has typed any input";

const char kOmniboxRemoveSuggestionsFromClipboardName[] =
    "Omnibox remove suggestions from clipboard";
const char kOmniboxRemoveSuggestionsFromClipboardDescription[] =
    "Allow users to remove suggestions from clipboard.";

const char kOmniboxRichEntitySuggestionsName[] =
    "Omnibox rich entity suggestions";
const char kOmniboxRichEntitySuggestionsDescription[] =
    "Display entity suggestions using images and an enhanced layout; showing "
    "more context and descriptive text about the entity.";

const char kOmniboxSearchEngineLogoName[] = "Omnibox search engine logo";
const char kOmniboxSearchEngineLogoDescription[] =
    "Display the current default search engine's logo in the omnibox";

const char kOmniboxSpareRendererName[] =
    "Start spare renderer on omnibox focus";
const char kOmniboxSpareRendererDescription[] =
    "When the omnibox is focused, start an empty spare renderer. This can "
    "speed up the load of the navigation from the omnibox.";

const char kOmniboxUIHideSteadyStateUrlSchemeName[] =
    "Omnibox UI Hide Steady-State URL Scheme";
const char kOmniboxUIHideSteadyStateUrlSchemeDescription[] =
    "In the omnibox, hide the scheme from steady state displayed URLs. It is "
    "restored during editing.";

const char kOmniboxUIOneClickUnelideName[] = "Omnibox UI One Click Unelide";
const char kOmniboxUIOneClickUnelideDescription[] =
    "In the omnibox, undo all unelisions with a single click or focus action.";

const char kOmniboxUIHideSteadyStateUrlTrivialSubdomainsName[] =
    "Omnibox UI Hide Steady-State URL Trivial Subdomains";
const char kOmniboxUIHideSteadyStateUrlTrivialSubdomainsDescription[] =
    "In the omnibox, hide trivial subdomains from steady state displayed URLs. "
    "Hidden portions are restored during editing.";

const char kOmniboxUIHideSteadyStateUrlPathQueryAndRefName[] =
    "Omnibox UI Hide Steady-State URL Path, Query, and Ref";
const char kOmniboxUIHideSteadyStateUrlPathQueryAndRefDescription[] =
    "In the omnibox, hide the path, query and ref from steady state displayed "
    "URLs. Hidden portions are restored during editing.";

const char kOmniboxUIMaxAutocompleteMatchesName[] =
    "Omnibox UI Max Autocomplete Matches";
const char kOmniboxUIMaxAutocompleteMatchesDescription[] =
    "Changes the maximum number of autocomplete matches displayed in the "
    "Omnibox UI.";

const char kOmniboxMaxURLMatchesName[] = "Omnibox Max URL Matches";
const char kOmniboxMaxURLMatchesDescription[] =
    "The maximum number of URL matches to show, unless there are no "
    "replacements.";

const char kOmniboxOnDeviceHeadSuggestionsName[] =
    "Omnibox on device head suggestions";
const char kOmniboxOnDeviceHeadSuggestionsDescription[] =
    "Google head non personalized search suggestions provided by a compact on "
    "device model";

const char kOmniboxPreserveDefaultMatchAgainstAsyncUpdateName[] =
    "Omnibox Preserve Default Match Against Async Update";
const char kOmniboxPreserveDefaultMatchAgainstAsyncUpdateDescription[] =
    "Preserves the default match against change when providers return results "
    "asynchronously. This prevents the default match from changing after the "
    "user finishes typing. Without this feature, if the default match is "
    "updated right when the user presses Enter, the user may go to a "
    "surprising destination.";

const char kOmniboxUIShowSuggestionFaviconsName[] =
    "Omnibox UI Show Suggestion Favicons";
const char kOmniboxUIShowSuggestionFaviconsDescription[] =
    "Shows favicons instead of generic vector icons for URL suggestions in the "
    "Omnibox dropdown.";

const char kOmniboxUISwapTitleAndUrlName[] = "Omnibox UI Swap Title and URL";
const char kOmniboxUISwapTitleAndUrlDescription[] =
    "In the omnibox dropdown, shows titles before URLs when both are "
    "available.";

const char kOmniboxZeroSuggestionsOnNTPName[] =
    "Omnibox Zero Suggestions on New Tab Page";
const char kOmniboxZeroSuggestionsOnNTPDescription[] =
    "Offer suggestions when URL bar (omnibox) is focused.";

const char kOmniboxZeroSuggestionsOnNTPRealboxName[] =
    "Zero Suggestions in real search box on New Tab Page";
const char kOmniboxZeroSuggestionsOnNTPRealboxDescription[] =
    "Offer suggestions when the real search box on New Tab Page is focused.";

const char kOmniboxZeroSuggestionsOnSERPName[] =
    "Omnibox Zero Suggestions on SERP / On-Focus Query Refinement";
const char kOmniboxZeroSuggestionsOnSERPDescription[] =
    "Offer query refinement suggestions when the URL bar (omnibox) is focused "
    "on the default search provider's search results page (SERP).";

const char kOnlyNewPasswordFormParsingName[] =
    "Use only new password form parsing";
const char kOnlyNewPasswordFormParsingDescription[] =
    "The old password form parsing is disabled";

const char kOnTheFlyMhtmlHashComputationName[] =
    "On-The-Fly MHTML Hash Computation";
const char kOnTheFlyMhtmlHashComputationDescription[] =
    "Save MHTML files to the target location and calculate their content "
    "digests in one step.";

const char kOopRasterizationName[] = "Out of process rasterization";
const char kOopRasterizationDescription[] =
    "Perform Ganesh raster in the GPU Process instead of the renderer.  "
    "Must also enable GPU rasterization";

const char kEnableDeJellyName[] = "Experimental de-jelly effect";
const char kEnableDeJellyDescription[] =
    "Enables an experimental effect which attempts to mitigate "
    "\"jelly-scrolling\". This is an experimental implementation with known "
    "bugs, visual artifacts, and performance cost. This implementation may be "
    "removed at any time.";

const char kOverlayNewLayoutName[] = "Overlay new layout";
const char kOverlayNewLayoutDescription[] =
    "Enables a new layout for the "
    "Overlay panels including Contextual Search and Preview Tab.";

const char kOverlayScrollbarsName[] = "Overlay Scrollbars";
const char kOverlayScrollbarsDescription[] =
    "Enable the experimental overlay scrollbars implementation. You must also "
    "enable threaded compositing to have the scrollbars animate.";

const char kOverlayScrollbarsFlashAfterAnyScrollUpdateName[] =
    "Flash Overlay Scrollbars After Any Scroll Update";
const char kOverlayScrollbarsFlashAfterAnyScrollUpdateDescription[] =
    "Flash Overlay Scrollbars After any scroll update happends in page. You"
    " must also enable Overlay Scrollbars.";

const char kOverlayScrollbarsFlashWhenMouseEnterName[] =
    "Flash Overlay Scrollbars When Mouse Enter";
const char kOverlayScrollbarsFlashWhenMouseEnterDescription[] =
    "Flash Overlay Scrollbars When Mouse Enter a scrollable area. You must also"
    " enable Overlay Scrollbars.";

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

const char kUseNewAcceptLanguageHeaderName[] = "Use new Accept-Language header";
const char kUseNewAcceptLanguageHeaderDescription[] =
    "Adds the base language code after other corresponding language+region "
    "codes. This ensures that users receive content in their preferred "
    "language.";

const char kOverscrollHistoryNavigationName[] = "Overscroll history navigation";
const char kOverscrollHistoryNavigationDescription[] =
    "History navigation in response to horizontal overscroll.";

const char kTouchpadOverscrollHistoryNavigationName[] =
    "Overscroll history navigation on Touchpad";
const char kTouchpadOverscrollHistoryNavigationDescription[] =
    "Allows swipe left/right from touchpad change browser navigation.";

const char kParallelDownloadingName[] = "Parallel downloading";
const char kParallelDownloadingDescription[] =
    "Enable parallel downloading to accelerate download speed.";

const char kPassiveEventListenerDefaultName[] =
    "Passive Event Listener Override";
const char kPassiveEventListenerDefaultDescription[] =
    "Forces touchstart, touchmove, mousewheel and wheel event listeners (which "
    "haven't requested otherwise) to be treated as passive. This will break "
    "touch/wheel behavior on some websites but is useful for demonstrating the "
    "potential performance benefits of adopting passive event listeners.";
const char kPassiveEventListenerTrue[] = "True (when unspecified)";
const char kPassiveEventListenerForceAllTrue[] = "Force All True";

const char kPassiveEventListenersDueToFlingName[] =
    "Touch Event Listeners Passive Default During Fling";
const char kPassiveEventListenersDueToFlingDescription[] =
    "Forces touchstart, and first touchmove per scroll event listeners during "
    "fling to be treated as passive.";

const char kPassiveDocumentEventListenersName[] =
    "Document Level Event Listeners Passive Default";
const char kPassiveDocumentEventListenersDescription[] =
    "Forces touchstart, and touchmove event listeners on document level "
    "targets (which haven't requested otherwise) to be treated as passive.";

const char kPassiveDocumentWheelEventListenersName[] =
    "Document Level Wheel Event Listeners Passive Default";
const char kPassiveDocumentWheelEventListenersDescription[] =
    "Forces wheel, and mousewheel event listeners on document level targets "
    "(which haven't requested otherwise) to be treated as passive.";

const char kPasswordEditingAndroidName[] = "Password editing for Android";
const char kPasswordEditingAndroidDescription[] =
    "Adds the editing option for saved passwords.";

const char kPasswordImportName[] = "Password import";
const char kPasswordImportDescription[] =
    "Import functionality in password settings.";

const char kPasswordLeakDetectionName[] = "Password Leak Detection";
const char kPasswordLeakDetectionDescription[] =
    "Enables the detection of leaked passwords.";

const char kForceWebContentsDarkModeName[] = "Force Dark Mode for Web Contents";
const char kForceWebContentsDarkModeDescription[] =
    "Automatically render all web contents using a dark theme.";

const char kForcedColorsName[] = "Forced Colors";
const char kForcedColorsDescription[] =
    "Enables forced colors mode for web content.";

const char kPercentBasedScrollingName[] = "Percent-based Scrolling";
const char kPercentBasedScrollingDescription[] =
    "If enabled, mousewheel and keyboard scrolls will scroll by a percentage "
    "of the scroller size.";

const char kPeriodicBackgroundSyncName[] = "Periodic Background Sync";
const char kPeriodicBackgroundSyncDescription[] =
    "If enabled, web apps can periodically sync content in the background.";

const char kPerMethodCanMakePaymentQuotaName[] =
    "Per-method canMakePayment() quota.";
const char kPerMethodCanMakePaymentQuotaDescription[] =
    "Allow calling canMakePayment() for different payment methods, as long as "
    "method-specific parameters remain unchanged.";

const char kPointerLockOptionsName[] = "Enables pointer lock options";
const char kPointerLockOptionsDescription[] =
    "Enables pointer lock unadjustedMovement. When unadjustedMovement is set "
    "to true, pointer movements wil not be affected by the underlying platform "
    "modications such as mouse accelaration.";

const char kPolicyAtomicGroupsEnabledName[] = "Policy Atomic Groups Enabled";
const char kPolicyAtomicGroupsEnabledDescription[] =
    "Enables the concept of policy atomic groups that makes policies of an "
    "atomic group that do not share the highest priority source from that group"
    "ignored.";

const char kPreviewsAllowedName[] = "Previews Allowed";
const char kPreviewsAllowedDescription[] =
    "Allows previews to be shown subject to specific preview types being "
    "enabled and the client experiencing specific triggering conditions. "
    "May be used as a kill-switch to turn off all potential preview types.";

const char kPrintPdfAsImageName[] = "Print Pdf as Image";
const char kPrintPdfAsImageDescription[] =
    "If enabled, an option to print PDF files as images will be available in "
    "print preview.";

const char kPrintPreviewRegisterPromosName[] =
    "Print Preview Registration Promos";
const char kPrintPreviewRegisterPromosDescription[] =
    "Enable registering unregistered cloud printers from print preview.";

const char kPrivacySettingsRedesignName[] = "Privacy Settings Redesign";
const char kPrivacySettingsRedesignDescription[] =
    "Redesign of the privacy settings card to make it more prominent and "
    "and easier to use.";

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

const char kQueryInOmniboxName[] = "Query in Omnibox";
const char kQueryInOmniboxDescription[] =
    "Only display query terms in the omnibox when viewing a search results "
    "page.";

const char kQuicName[] = "Experimental QUIC protocol";
const char kQuicDescription[] = "Enable experimental QUIC protocol support.";

const char kQuietNotificationPromptsName[] =
    "Quieter notification permission prompts";
const char kQuietNotificationPromptsDescription[] =
    "Enables quieter permission prompts for notification permission requests. "
    "When a site wishes to show notifications, the usual modal dialog is "
    "replaced with a quieter version.";

const char kReducedReferrerGranularityName[] =
    "Reduce default 'referer' header granularity.";
const char kReducedReferrerGranularityDescription[] =
    "If a page hasn't set an explicit referrer policy, setting this flag will "
    "reduce the amount of information in the 'referer' header for cross-origin "
    "requests.";

const char kRewriteLevelDBOnDeletionName[] =
    "Rewrite LevelDB instances after full deletions";
const char kRewriteLevelDBOnDeletionDescription[] =
    "Rewrite LevelDB instances to remove traces of deleted data from disk.";

const char kRendererSideResourceSchedulerName[] =
    "Renderer side ResourceScheduler";
const char kRendererSideResourceSchedulerDescription[] =
    "Migrate some ResourceScheduler functionalities to renderer";

const char kReorderBookmarksName[] = "Reorder bookmarks";
const char kReorderBookmarksDescription[] =
    "Allows the user to reorder their bookmarks from their Android device. "
    "The bookmark ordering will be synced across devices.";

const char kRequestUnbufferedDispatchName[] = "Use RequestUnbufferedDispatch";
const char kRequestUnbufferedDispatchDescription[] =
    "Calls RequestUnbufferedDispatch in the Android API to disable buffering "
    "of input by the OS.";

const char kRequestTabletSiteName[] =
    "Request tablet site option in the settings menu";
const char kRequestTabletSiteDescription[] =
    "Allows the user to request tablet site. Web content is often optimized "
    "for tablet devices. When this option is selected the user agent string is "
    "changed to indicate a tablet device. Web content optimized for tablets is "
    "received there after for the current tab.";

const char kPrefetchPrivacyChangesName[] =
    "Prefetch request properties are updated to be privacy-preserving";
const char kPrefetchPrivacyChangesDescription[] =
    "Prefetch requests will not follow redirects, not send a Referer header, "
    "not send credentials for cross-origin requests, and do not pass through "
    "service workers.";

const char kPrefetchMainResourceNetworkIsolationKeyName[] =
    "Prefetch requests for cross-origin main resources are fetched with a "
    "special NetworkIsolationKey";
const char kPrefetchMainResourceNetworkIsolationKeyDescription[] =
    "Prefetch requests for cross-origin main resources can be reused by next "
    "top-level navigations when HTTP cache is double-keyed.";

const char kSafetyTipName[] =
    "Show Safety Tip UI when visiting low-reputation websites";
const char kSafetyTipDescription[] =
    "If enabled, a Safety Tip UI may be displayed when visiting or interacting "
    "with a site Chrome believes may be suspicious.";

const char kSameSiteByDefaultCookiesName[] = "SameSite by default cookies";
const char kSameSiteByDefaultCookiesDescription[] =
    "Treat cookies that don't specify a SameSite attribute as if they were "
    "SameSite=Lax. Sites must specify SameSite=None in order to enable "
    "third-party usage.";

const char kSaveasMenuLabelExperimentName[] =
    "Switch 'Save as' menu labels to 'Download'";
const char kSaveasMenuLabelExperimentDescription[] =
    "Enables an experiment to switch menu labels that use 'Save as...' to "
    "'Download'.";

const char kScalableAppListName[] =
    "App list UI configuration dependant on display size";
const char kScalableAppListDescription[] =
    "Adapts app list item sizing and spacing for smaller screen sizes, "
    "instead of using single app list configuration, that is optionally "
    "scaled down, for all screens.";

const char kScrollableTabStripName[] = "Scrollable TabStrip";
const char kScrollableTabStripDescription[] =
    "Allows users to access tabs by scrolling when they no longer fit in the "
    "tabstrip.";

const char kSecurityInterstitialsDarkModeName[] =
    "Security interstitials dark mode";
const char kSecurityInterstitialsDarkModeDescription[] =
    "Allows security intersitials to take on a dark theme when the OS is "
    "switched to dark mode.";

const char kSendTabToSelfName[] = "Send tab to self";
const char kSendTabToSelfDescription[] =
    "Allows users to receive tabs from other synced devices, in order to "
    "easily transition those tabs to this device. This enables the sync "
    "infrastructure for this feature.";

const char kSendTabToSelfBroadcastName[] = "Send tab to self broadcast";
const char kSendTabToSelfBroadcastDescription[] =
    "Allows users to broadcast the tab they send to all of their devices "
    "instead of targetting only one device.";

const char kSendTabToSelfWhenSignedInName[] =
    "Send tab to self: enable use when signed-in regardless of sync state";
const char kSendTabToSelfWhenSignedInDescription[] =
    "Allows use of the send-tab-to-self feature for users who are signed-in "
    "but not necessarily syncing. The tab-share data is thus ephemeral, "
    "rather than persistent sync data.";

const char kServiceWorkerImportedScriptUpdateCheckName[] =
    "Enable update check for service worker importScripts() resources";
const char kServiceWorkerImportedScriptUpdateCheckDescription[] =
    "Extend byte-for-byte update check for scripts that are imported by the "
    "service worker script via importScripts().";

const char kServiceWorkerLongRunningMessageName[] =
    "Service worker long running message dispatch.";
const char kServiceWorkerLongRunningMessageDescription[] =
    "Enables long running message dispatch method for service workers. "
    "Messages sent with this method do not timeout, allowing the service "
    "worker to run indefinitely.";

const char kServiceWorkerOnUIName[] = "Service worker on UI thread";
const char kServiceWorkerOnUIDescription[] =
    "Enables browser process logic related to service workers to run on the UI "
    "thread rather than the IO thread.";

const char kSettingsWindowName[] = "Show settings in a window";
const char kSettingsWindowDescription[] =
    "Settings will be shown in a dedicated window instead of as a browser tab.";

const char kSharedClipboardReceiverName[] =
    "Enable receiver device to handle shared clipboard feature";
const char kSharedClipboardReceiverDescription[] =
    "Enables receiver device to handle shared clipboard feature by showing a "
    "notification to receive the clipboard to share.";

const char kSharedClipboardUIName[] =
    "Enable shared clipboard feature signals to be handled";
const char kSharedClipboardUIDescription[] =
    "Enables shared clipboard feature signals to be handled by showing "
    "a list of user's available devices to share the clipboard.";

const char kSharingDeviceRegistrationName[] =
    "Enable device registration for Sharing features";
const char kSharingDeviceRegistrationDescription[] =
    "Enables device registration with Sharing infrastructure. Required to use "
    "cross-device Sharing features.";

const char kSharingPeerConnectionReceiverName[] =
    "Enable receiver device to handle peer connection requests.";
const char kSharingPeerConnectionReceiverDescription[] =
    "Enables receiver device to connect and share data using a peer to peer "
    "connection.";

const char kSharingPeerConnectionSenderName[] =
    "Enable sender device to initiate peer connection requests.";
const char kSharingPeerConnectionSenderDescription[] =
    "Enables the sender devices to connect with chosen device using a peer to "
    "peer connection for transferring data.";

const char kSharingRenameDevicesName[] =
    "Enable device renaming in Sharing features.";
const char kSharingRenameDevicesDescription[] =
    "Enables renaming devices using HardwareInfo when populating device list "
    "and sender device info.";

const char kSharingUseDeviceInfoName[] =
    "Enable Sharing device registration in DeviceInfo";
const char kSharingUseDeviceInfoDescription[] =
    "Enables Sharing infrastructure to register devices in DeviceInfo.";

const char kSharingDeriveVapidKeyName[] =
    "Enable Sharing derive VAPID key from sync";
const char kSharingDeriveVapidKeyDescription[] =
    "Enables Sharing infrastructure to derive VAPID key from sync.";

const char kShelfHotseatName[] = "Enable a modular design for the shelf.";
const char kShelfHotseatDescription[] =
    "Shows a modular design for the shelf where the apps are shown separately "
    "in a 'hotseat' interface when in tablet mode, and where various pieces "
    "are separate and behave independently. Also reduces the size of the"
    "shelf and its app when in laptop mode.";

const char kShelfHoverPreviewsName[] =
    "Show previews of running apps when hovering over the shelf.";
const char kShelfHoverPreviewsDescription[] =
    "Shows previews of the open windows for a given running app when hovering "
    "over the shelf.";

const char kShelfScrollableName[] =
    "Enable a scrollable list of apps on the shelf";
const char kShelfScrollableDescription[] =
    "Shows a list of applications that is scrollable by default on tablets.";

const char kShowAndroidFilesInFilesAppName[] =
    "Show Android files in Files app";
const char kShowAndroidFilesInFilesAppDescription[] =
    "Show Android files in Files app if Android is enabled on the device.";

const char kShowAutofillSignaturesName[] = "Show autofill signatures.";
const char kShowAutofillSignaturesDescription[] =
    "Annotates web forms with Autofill signatures as HTML attributes. Also "
    "marks password fields suitable for password generation.";

const char kShowAutofillTypePredictionsName[] = "Show Autofill predictions";
const char kShowAutofillTypePredictionsDescription[] =
    "Annotates web forms with Autofill field type predictions as placeholder "
    "text.";

const char kSkiaRendererName[] = "Skia API for OOP-D compositing";
const char kSkiaRendererDescription[] =
    "If enabled, the display compositor will use Skia as the graphics API "
    "instead of OpenGL ES. Requires Viz Display Compositor (OOP-D).";

const char kHistoryManipulationIntervention[] =
    "History Manipulation Intervention";
const char kHistoryManipulationInterventionDescription[] =
    "If a page does a client side redirect or adds to the history without a "
    "user gesture, then skip it on back/forward UI.";

const char kSilentDebuggerExtensionApiName[] = "Silent Debugging";
const char kSilentDebuggerExtensionApiDescription[] =
    "Do not show the infobar when an extension attaches to a page via "
    "chrome.debugger API. This is required to debug extension background "
    "pages.";

const char kSimplifyHttpsIndicatorName[] = "Simplify HTTPS indicator UI";
const char kSimplifyHttpsIndicatorDescription[] =
    "Change the UI treatment for HTTPS pages.";

const char kIsolateOriginsName[] = "Isolate additional origins";
const char kIsolateOriginsDescription[] =
    "Requires dedicated processes for an additional set of origins, "
    "specified as a comma-separated list.";

const char kKidsManagementUrlClassificationName[] =
    "KidsMangement Url Classification";
const char kKidsManagementUrlClassificationDescription[] =
    "Uses KidsManagementService to classify URLs for Kid Accounts.";

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

const char kSpeculativeServiceWorkerStartOnQueryInputName[] =
    "Enable speculative start of a service worker when a search is predicted.";
const char kSpeculativeServiceWorkerStartOnQueryInputDescription[] =
    "If enabled, when the user enters text in the omnibox that looks like a "
    "a query, any service worker associated with the search engine the query "
    "will be sent to is started early.";

extern const char kStrictOriginIsolationName[] = "Strict-Origin-Isolation";
extern const char kStrictOriginIsolationDescription[] =
    "Experimental security mode that strengthens the site isolation policy. "
    "Controls whether site isolation should use origins instead of scheme and "
    "eTLD+1.";

const char kStopInBackgroundName[] = "Stop in background";
const char kStopInBackgroundDescription[] =
    "Stop scheduler task queues, in the background, "
    " after a grace period.";

const char kStopNonTimersInBackgroundName[] =
    "Stop non-timer task queues background";
const char kStopNonTimersInBackgroundDescription[] =
    "Stop non-timer task queues, in the background, "
    "after a grace period.";

const char kStorageAccessAPIName[] = "Storage Access API";
const char kStorageAccessAPIDescription[] =
    "Enables the Storage Access API, allowing websites to request storage "
    "access when it would otherwise be restricted.";

const char kStoragePressureUIName[] = "Enable storage pressure UI";
const char kStoragePressureUIDescription[] =
    "If enabled, Chrome will trigger system notifications to warn about "
    "storage pressure.";

const char kSuggestionsWithSubStringMatchName[] =
    "Substring matching for Autofill suggestions";
const char kSuggestionsWithSubStringMatchDescription[] =
    "Match Autofill suggestions based on substrings (token prefixes) rather "
    "than just prefixes.";

const char kSyncDeviceInfoInTransportModeName[] =
    "Enable syncing DeviceInfo in transport-only sync mode.";
const char kSyncDeviceInfoInTransportModeDescription[] =
    "When enabled, allows syncing DeviceInfo datatype for users who are "
    "signed-in but not necessary sync-ing.";

const char kSyncSandboxName[] = "Use Chrome Sync sandbox";
const char kSyncSandboxDescription[] =
    "Connects to the testing server for Chrome Sync.";

const char kSystemKeyboardLockName[] = "Experimental system keyboard lock";
const char kSystemKeyboardLockDescription[] =
    "Enables websites to use the keyboard.lock() API to intercept system "
    "keyboard shortcuts and have the events routed directly to the website "
    "when in fullscreen mode.";

const char kTabEngagementReportingName[] = "Tab Engagement Metrics";
const char kTabEngagementReportingDescription[] =
    "Tracks tab engagement and lifetime metrics.";

const char kTabGridLayoutAndroidName[] = "Tab Grid Layout";
const char kTabGridLayoutAndroidDescription[] =
    "Allows users to see their tabs in a grid layout in the tab switcher.";

const char kTabGroupsAndroidName[] = "Tab Groups";
const char kTabGroupsAndroidDescription[] =
    "Allows users to create groups to better organize their tabs.";

const char kTabGroupsContinuationAndroidName[] = "Tab Groups Continuation";
const char kTabGroupsContinuationAndroidDescription[] =
    "Allows users to access continuation features in Tab Group.";

const char kTabGroupsUiImprovementsAndroidName[] = "Tab Groups UI Improvements";
const char kTabGroupsUiImprovementsAndroidDescription[] =
    "Allows users to access new features in Tab Group UI.";

const char kTabToGTSAnimationAndroidName[] = "Enable Tab-to-GTS Animation";
const char kTabToGTSAnimationAndroidDescription[] =
    "Allows users to see an animation when entering or leaving the "
    "Grid Tab Switcher.";

const char kTabGroupsName[] = "Tab Groups";
const char kTabGroupsDescription[] =
    "Allows users to organize tabs into visually distinct groups, e.g. to "
    "separate tabs associated with different tasks.";

const char kTabHoverCardsName[] = "Tab Hover Cards";
const char kTabHoverCardsDescription[] =
    "Enables a popup containing tab information to be visible when hovering "
    "over a tab. This will replace tooltips for tabs.";

const char kTabHoverCardImagesName[] = "Tab Hover Card Images";
const char kTabHoverCardImagesDescription[] =
    "Shows a preview image in tab hover cards, if tab hover cards are enabled.";

const char kTabOutlinesInLowContrastThemesName[] =
    "Tab Outlines in Low Contrast Themes";
const char kTabOutlinesInLowContrastThemesDescription[] =
    "Expands the range of situations in which tab outline strokes are "
    "displayed, improving accessiblity in dark and incognito mode.";

const char kTabsInCbdName[] = "Enable tabs for the Clear Browsing Data dialog.";
const char kTabsInCbdDescription[] =
    "Enables a basic and an advanced tab for the Clear Browsing Data dialog.";

const char kTintGlCompositedContentName[] = "Tint GL-composited content";
const char kTintGlCompositedContentDescription[] =
    "Tint contents composited using GL with a shade of red to help debug and "
    "study overlay support.";

const char kTLS13HardeningForLocalAnchorsName[] =
    "TLS 1.3 hardening for local anchors";
const char kTLS13HardeningForLocalAnchorsDescription[] =
    "This option enables the TLS 1.3 downgrade hardening mechanism for "
    "connections authenticated by local trust anchors. This improves security "
    "for connections to TLS-1.3-capable servers while remaining compatible "
    "with older servers. Firewalls and proxies that do not function when this "
    "is enabled do not implement TLS 1.2 correctly or securely and must be "
    "updated.";

const char kTopChromeTouchUiName[] = "Touch UI Layout";
const char kTopChromeTouchUiDescription[] =
    "Enables touch UI layout in the browser's top chrome.";

const char kThreadedScrollingName[] = "Threaded scrolling";
const char kThreadedScrollingDescription[] =
    "Threaded handling of scroll-related input events. Disabling this will "
    "force all such scroll events to be handled on the main thread. Note that "
    "this can dramatically hurt scrolling performance of most websites and is "
    "intended for testing purposes only.";

const char kTouchAdjustmentName[] = "Touch adjustment";
const char kTouchAdjustmentDescription[] =
    "Refine the position of a touch gesture in order to compensate for touches "
    "having poor resolution compared to a mouse.";

const char kTouchDragDropName[] = "Touch initiated drag and drop";
const char kTouchDragDropDescription[] =
    "Touch drag and drop can be initiated through long press on a draggable "
    "element.";

const char kTouchEventsName[] = "Touch Events API";
const char kTouchEventsDescription[] =
    "Force Touch Events API feature detection to always be enabled or "
    "disabled, or to be enabled when a touchscreen is detected on startup "
    "(Automatic).";

const char kTouchSelectionStrategyName[] = "Touch text selection strategy";
const char kTouchSelectionStrategyDescription[] =
    "Controls how text selection granularity changes when touch text selection "
    "handles are dragged. Non-default behavior is experimental.";
const char kTouchSelectionStrategyCharacter[] = "Character";
const char kTouchSelectionStrategyDirection[] = "Direction";

const char kTraceUploadUrlName[] = "Trace label for navigation tracing";
const char kTraceUploadUrlDescription[] =
    "This is to be used in conjunction with the enable-navigation-tracing "
    "flag. Please select the label that best describes the recorded traces. "
    "This will choose the destination the traces are uploaded to. If you are "
    "not sure, select other. If left empty, no traces will be uploaded.";
const char kTraceUploadUrlChoiceOther[] = "Other";
const char kTraceUploadUrlChoiceEmloading[] = "emloading";
const char kTraceUploadUrlChoiceQa[] = "QA";
const char kTraceUploadUrlChoiceTesting[] = "Testing";

const char kTranslateForceTriggerOnEnglishName[] =
    "Select which language model to use to trigger translate on English "
    "content";
const char kTranslateForceTriggerOnEnglishDescription[] =
    "Force the Translate Triggering on English pages experiment to be enabled "
    "with the selected language model active.";

const char kTranslateBubbleUIName[] =
    "Select which UI to use for translate bubble";
const char kTranslateBubbleUIDescription[] =
    "Three bubble options to choose. Existing UI is selected by default";

const char kTreatInsecureOriginAsSecureName[] =
    "Insecure origins treated as secure";
const char kTreatInsecureOriginAsSecureDescription[] =
    "Treat given (insecure) origins as secure origins. Multiple origins can be "
    "supplied as a comma-separated list. Origins must have their protocol "
    "specified e.g. \"http://example.com\". For the definition of secure "
    "contexts, see https://w3c.github.io/webappsec-secure-contexts/";

const char kTreatUnsafeDownloadsAsActiveName[] =
    "Treat risky downloads over insecure connections as active mixed content";
const char kTreatUnsafeDownloadsAsActiveDescription[] =
    "Disallows downloads of unsafe files (files that can potentially execute "
    "code), where the final download origin or any origin in the redirect "
    "chain is insecure if the originating page is secure.";

const char kTrySupportedChannelLayoutsName[] =
    "Causes audio output streams to check if channel layouts other than the "
    "default hardware layout are available.";
const char kTrySupportedChannelLayoutsDescription[] =
    "Causes audio output streams to check if channel layouts other than the "
    "default hardware layout are available. Turning this on will allow the OS "
    "to do stereo to surround expansion if supported. May expose third party "
    "driver bugs, use with caution.";

const char kTurnOffStreamingMediaCachingName[] =
    "Turn off caching of streaming media to disk.";
const char kTurnOffStreamingMediaCachingDescription[] =
    "Reduces disk activity during media playback, which can result in "
    "power savings.";

const char kUnexpireFlagsM78Name[] = "Temporarily unexpire M78 flags.";
const char kUnexpireFlagsM78Description[] =
    "Temporarily unexpire flags that are expired as of M78. These flags will "
    "be removed soon.";
const char kUnexpireFlagsM80Name[] = "Temporarily unexpire M80 flags.";
const char kUnexpireFlagsM80Description[] =
    "Temporarily unexpire flags that are expired as of M80. These flags will "
    "be removed soon.";

const char kUnsafeWebGPUName[] = "Unsafe WebGPU";
const char kUnsafeWebGPUDescription[] =
    "Enables access to the experimental WebGPU API. Warning: As GPU sanboxing "
    "isn't implemented yet for the WebGPU API, it is possible to read GPU data "
    "for other processes.";

const char kUiPartialSwapName[] = "Partial swap";
const char kUiPartialSwapDescription[] = "Sets partial swap behavior.";

const char kUsernameFirstFlowName[] = "Username first flow";
const char kUsernameFirstFlowDescription[] =
    "Support of username saving and filling on username first flow i.e. login "
    "flows where a user has to type username first on one page and then "
    "password on another page";

const char kUseSearchClickForRightClickName[] =
    "Use Search+Click for right click";
const char kUseSearchClickForRightClickDescription[] =
    "When enabled search+click will be remapped to right click, allowing "
    "webpages and apps to consume alt+click. When disabled the legacy "
    "behavior of remapping alt+click to right click will remain unchanged.";

const char kV8VmFutureName[] = "Future V8 VM features";
const char kV8VmFutureDescription[] =
    "This enables upcoming and experimental V8 VM features. "
    "This flag does not enable experimental JavaScript features.";

const char kWalletServiceUseSandboxName[] =
    "Use Google Payments sandbox servers";
const char kWalletServiceUseSandboxDescription[] =
    "For developers: use the sandbox service for Google Payments API calls.";

const char kWebBundlesName[] = "Web Bundles";
const char kWebBundlesDescription[] =
    "Enables experimental supports for Web Bundles (Bundled HTTP Exchanges) "
    "navigation.";

const char kWebglDraftExtensionsName[] = "WebGL Draft Extensions";
const char kWebglDraftExtensionsDescription[] =
    "Enabling this option allows web applications to access the WebGL "
    "Extensions that are still in draft status.";

const char kWebMidiName[] = "Web MIDI API";
const char kWebMidiDescription[] = "Enable Web MIDI API experimental support.";

const char kWebPaymentsExperimentalFeaturesName[] =
    "Experimental Web Payments API features";
const char kWebPaymentsExperimentalFeaturesDescription[] =
    "Enable experimental Web Payments API features";

const char kWebrtcHideLocalIpsWithMdnsName[] =
    "Anonymize local IPs exposed by WebRTC.";
const char kWebrtcHideLocalIpsWithMdnsDecription[] =
    "Conceal local IP addresses with mDNS hostnames.";

const char kWebrtcHybridAgcName[] = "WebRTC hybrid Agc2/Agc1.";
const char kWebrtcHybridAgcDescription[] =
    "WebRTC Agc2 digital adaptation with Agc1 analog adaptation.";

const char kWebrtcHwDecodingName[] = "WebRTC hardware video decoding";
const char kWebrtcHwDecodingDescription[] =
    "Support in WebRTC for decoding video streams using platform hardware.";

const char kWebrtcHwEncodingName[] = "WebRTC hardware video encoding";
const char kWebrtcHwEncodingDescription[] =
    "Support in WebRTC for encoding video streams using platform hardware.";

const char kWebrtcMultiChannelApmName[] =
    "WebRTC multi-channel audio processing.";
const char kWebrtcMultiChannelApmDescription[] =
    "Support in WebRTC for processing playout and capture audio in multi "
    "channel without downmixing.";

const char kWebrtcNewEncodeCpuLoadEstimatorName[] =
    "WebRTC new encode cpu load estimator";
const char kWebrtcNewEncodeCpuLoadEstimatorDescription[] =
    "Enable new estimator for the encoder cpu load, for evaluation and "
    "testing. Intended to improve accuracy when screen casting.";

const char kWebRtcRemoteEventLogName[] = "WebRTC remote-bound event logging";
const char kWebRtcRemoteEventLogDescription[] =
    "Allow collecting WebRTC event logs and uploading them to Crash. "
    "Please note that, even if enabled, this will still require "
    "a policy to be set, for it to have an effect.";

const char kWebrtcSrtpAesGcmName[] =
    "Negotiation with GCM cipher suites for SRTP in WebRTC";
const char kWebrtcSrtpAesGcmDescription[] =
    "When enabled, WebRTC will try to negotiate GCM cipher suites for SRTP.";

const char kWebrtcStunOriginName[] = "WebRTC Stun origin header";
const char kWebrtcStunOriginDescription[] =
    "When enabled, Stun messages generated by WebRTC will contain the Origin "
    "header.";

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
const char kWebUITabStripName[] = "WebUI tab strip";
const char kWebUITabStripDescription[] =
    "When enabled makes use of a WebUI-based tab strip.";

const char kWebUITabStripDemoOptionsName[] = "WebUI tab strip demo options";
const char kWebUITabStripDemoOptionsDescription[] =
    "When enabled, displays a set of options to demo and test various features "
    "and behaviors of the WebUI tab strip. The WebUI tab strip must also be "
    "enabled.";
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)

const char kWebXrName[] = "WebXR Device API";
const char kWebXrDescription[] =
    "Enables access to experimental APIs to interact with Virtual Reality (VR) "
    "and Augmented Reality (AR) devices.";

const char kWebXrArModuleName[] = "WebXR AR Module";
const char kWebXrArModuleDescription[] =
    "Enables access to Augmented Reality features defined in the WebXR AR "
    "Module";

const char kWebXrArDOMOverlayName[] = "WebXR AR DOM Overlay";
const char kWebXrArDOMOverlayDescription[] =
    "Enables experimental use of a DOM overlay in WebXR AR sessions";

const char kWebXrAnchorsName[] = "WebXR Anchors";
const char kWebXrAnchorsDescription[] =
    "Enables access to anchors via WebXR API.";

const char kWebXrHitTestName[] = "WebXR Hit Test";
const char kWebXrHitTestDescription[] =
    "Enables access to raycasting against estimated XR scene geometry.";

const char kWebXrPlaneDetectionName[] = "WebXR Plane Detection";
const char kWebXrPlaneDetectionDescription[] =
    "Enables access to planes detected in the user's environment.";

const char kZeroCopyName[] = "Zero-copy rasterizer";
const char kZeroCopyDescription[] =
    "Raster threads write directly to GPU memory associated with tiles.";

const char kEnableVulkanName[] = "Vulkan";
const char kEnableVulkanDescription[] = "Use vulkan as the graphocs backend.";

// Android ---------------------------------------------------------------------

#if defined(OS_ANDROID)

const char kAiaFetchingName[] = "Intermediate Certificate Fetching";
const char kAiaFetchingDescription[] =
    "Enable intermediate certificate fetching when a server does not provide "
    "sufficient certificates to build a chain to a trusted root.";

const char kAndroidAutofillAccessibilityName[] = "Autofill Accessibility";
const char kAndroidAutofillAccessibilityDescription[] =
    "Enable accessibility for autofill popup.";

const char kAndroidSetupSearchEngineName[] = "Android setup search engine";
const char kAndroidSetupSearchEngineDescription[] =
    "Enables search engine selection at Android setup.";

const char kAndroidSurfaceControl[] = "Use Android SurfaceControl";
const char kAndroidSurfaceControlDescription[] =
    "Use the SurfaceControl API for supporting overlays on Android";

const char kAppNotificationStatusMessagingName[] =
    "App notification status messaging";
const char kAppNotificationStatusMessagingDescription[] =
    "Enables messaging in site permissions UI informing user when "
    "notifications are disabled for the entire app.";

const char kAsyncDnsName[] = "Async DNS resolver";
const char kAsyncDnsDescription[] = "Enables the built-in DNS resolver.";

const char kAutofillAccessoryViewName[] =
    "Autofill suggestions as keyboard accessory view";
const char kAutofillAccessoryViewDescription[] =
    "Shows Autofill suggestions on top of the keyboard rather than in a "
    "dropdown.";

const char kAutofillTouchToFillName[] = "Touch To Fill UI for Passwords";
const char kAutofillTouchToFillDescription[] =
    "Adds a Touch To Fill sheet to the keyboard accessory which will be shown "
    "instead of the keyboard when a password can be filled.";

const char kAutofillUseMobileLabelDisambiguationName[] =
    "Autofill Uses Mobile Label Disambiguation";
const char kAutofillUseMobileLabelDisambiguationDescription[] =
    "When enabled, Autofill suggestions' labels are displayed using a "
    "mobile-friendly format.";

const char kBackgroundTaskComponentUpdateName[] =
    "Background Task Component Updates";
const char kBackgroundTaskComponentUpdateDescription[] =
    "Schedule component updates with BackgroundTaskScheduler";

const char kCCTIncognitoName[] = "Chrome Custom Tabs Incognito mode";
const char kCCTIncognitoDescription[] =
    "Enables incognito mode for Chrome Custom Tabs, on Android.";

const char kCCTModuleName[] = "Chrome Custom Tabs Module";
const char kCCTModuleDescription[] =
    "Enables a dynamically loaded module in Chrome Custom Tabs, on Android.";

const char kCCTModuleCacheName[] = "Chrome Custom Tabs Module Cache";
const char kCCTModuleCacheDescription[] =
    "Enables a cache for dynamically loaded modules in Chrome Custom Tabs. "
    "Under mild memory pressure the cache may be retained for some time";

const char kCCTModuleCustomHeaderName[] =
    "Chrome Custom Tabs Module Custom Header";
const char kCCTModuleCustomHeaderDescription[] =
    "Enables header customization by dynamically loaded modules in "
    "Chrome Custom Tabs.";

const char kCCTModuleCustomRequestHeaderName[] =
    "Chrome Custom Tabs Module Custom Request Header";
const char kCCTModuleCustomRequestHeaderDescription[] =
    "Enables a custom request header for URLs managed by dynamically loaded "
    "modules in Chrome Custom Tabs.";

const char kCCTModuleDexLoadingName[] = "Chrome Custom Tabs Module Dex Loading";
const char kCCTModuleDexLoadingDescription[] =
    "Enables loading Chrome Custom Tabs module code from a dex file "
    "provided by the module.";

const char kCCTModulePostMessageName[] =
    "Chrome Custom Tabs Module postMessage API";
const char kCCTModulePostMessageDescription[] =
    "Enables the postMessage API exposed to dynamically loaded modules in "
    "Chrome Custom Tabs.";

const char kCCTModuleUseIntentExtrasName[] =
    "Chrome Custom Tabs Module Intent Extras Usage";
const char kCCTModuleUseIntentExtrasDescription[] =
    "Enables usage of Intent's extras in Chrome Custom Tabs Module";

const char kCCTTargetTranslateLanguageName[] =
    "Chrome Custom Tabs Target Translate Language";
const char kCCTTargetTranslateLanguageDescription[] =
    "Enables specify target language the page should be translated to "
    "in Chrome Custom Tabs.";

const char kChromeDuetName[] = "Chrome Duet";
const char kChromeDuetDescription[] =
    "Enables Chrome Duet, split toolbar Chrome Home, on Android.";

const char kChromeDuetLabelsName[] = "Chrome Duet Labels";
const char kChromeDuetLabelsDescription[] =
    "Enables Chrome Duet (split toolbar) labels.";

const char kChromeSharingHubName[] = "Chrome Sharing Hub";
const char kChromeSharingHubDescription[] =
    "Enables the Chrome Sharing Hub/custom share sheet.";

const char kClearOldBrowsingDataName[] = "Clear older browsing data";
const char kClearOldBrowsingDataDescription[] =
    "Enables clearing of browsing data which is older than a given time "
    "period.";

const char kClickToCallOpenDialerDirectlyName[] =
    "Open the dialer directly for Click to Call";
const char kClickToCallOpenDialerDirectlyDescription[] =
    "Enables opening the dialer directly instead of displaying a notification. "
    "Only available on Android P- and when the screen is on and unlocked.";
const char kCloseTabSuggestionsName[] = "Suggest to close Tabs";
const char kCloseTabSuggestionsDescription[] =
    "Suggests to the user to close Tabs that haven't been used beyond a "
    "configurable threshold or where duplicates of Tabs exist. "
    "The threshold is configurable.";

const char kClickToCallReceiverName[] =
    "Enable receiver device to handle click to call feature";
const char kClickToCallReceiverDescription[] =
    "Enables receiver device to handle click to call feature by showing a "
    "notification to call the phone number clicked on the desktop.";

const char kContextualSearchDefinitionsName[] = "Contextual Search definitions";
const char kContextualSearchDefinitionsDescription[] =
    "Enables tap-activated contextual definitions of words on a page to be "
    "presented in the caption of the Tap to Search Bar.";

const char kContextualSearchLongpressResolveName[] =
    "Contextual Search long-press Resolves";
const char kContextualSearchLongpressResolveDescription[] =
    "Enables communicating with Google servers when a long-press gesture is "
    "recognized under some privacy-limited conditions.  The page context data "
    " sent to Google is potentially privacy sensitive!";

const char kContextualSearchMlTapSuppressionName[] =
    "Contextual Search ML tap suppression";
const char kContextualSearchMlTapSuppressionDescription[] =
    "Enables tap gestures to be suppressed to improve CTR by applying machine "
    "learning.  The \"Contextual Search Ranker prediction\" flag must also be "
    "enabled!";

const char kContextualSearchRankerQueryName[] =
    "Contextual Search Ranker prediction";
const char kContextualSearchRankerQueryDescription[] =
    "Enables prediction of tap gestures using Assist-Ranker machine learning.";

const char kContextualSearchSimplifiedServerName[] =
    "Contextual Search simplified server logic";
const char kContextualSearchSimplifiedServerDescription[] =
    "Enables simpler server-side logic for determining what data to return and "
    "show in the Contextual Search UI.  Option to allow all cards CoCa "
    "returns.";

const char kContextualSearchSecondTapName[] =
    "Contextual Search second tap triggering";
const char kContextualSearchSecondTapDescription[] =
    "Enables triggering on a second tap gesture even when Ranker would "
    "normally suppress that tap.";

const char kContextualSearchTranslationModelName[] =
    "Contextual Search translation using the Chrome translate model.";
const char kContextualSearchTranslationModelDescription[] =
    "Enables triggering translation in Contextual Search according to the "
    "Chrome translation model semantics.";

const char kDirectActionsName[] = "Direct actions";
const char kDirectActionsDescription[] =
    "Enables direct actions (Android Q and more).";

const char kDownloadRenameName[] = "Enable download rename";
const char kDownloadRenameDescription[] = "Enables rename option for downloads";

const char kAutofillManualFallbackAndroidName[] =
    "Enable Autofill manual fallback for Addresses and Payments (Android)";
const char kAutofillManualFallbackAndroidDescription[] =
    "If enabled, adds toggle for addresses and payments bottom sheet to the "
    "keyboard accessory.";

const char kEnableAutofillRefreshStyleName[] =
    "Enable Autofill refresh style (Android)";
const char kEnableAutofillRefreshStyleDescription[] =
    "Enable modernized style for Autofill on Android";

const char kEnableAndroidSpellcheckerName[] = "Enable spell checking";
const char kEnableAndroidSpellcheckerDescription[] =
    "Enables use of the Android spellchecker.";

const char kEnableCommandLineOnNonRootedName[] =
    "Enable command line on non-rooted devices";
const char kEnableCommandLineOnNoRootedDescription[] =
    "Enable reading command line file on non-rooted devices (DANGEROUS).";

const char kEnableRevampedContextMenuName[] =
    "Enable the revamped context menu";
const char kEnableRevampedContextMenuDescription[] =
    "Enables a revamped context menu when a link, image, or video is long "
    "pressed within Chrome.";

const char kEnableNtpRemoteSuggestionsName[] =
    "Show server-side suggestions on the New Tab page";
const char kEnableNtpRemoteSuggestionsDescription[] =
    "If enabled, the list of content suggestions on the New Tab page will "
    "contain server-side suggestions (e.g., Articles for you). Furthermore, it "
    "allows to override the source used to retrieve these server-side "
    "suggestions.";

const char kEnableOfflinePreviewsName[] = "Offline Page Previews";
const char kEnableOfflinePreviewsDescription[] =
    "Enable showing offline page previews on slow networks.";

const char kEnableWebNfcName[] = "WebNFC";
const char kEnableWebNfcDescription[] = "Enable WebNFC support.";

const char kEphemeralTabName[] = "An ephemeral Preview Tab in an Overlay Panel";
const char kEphemeralTabDescription[] =
    "Enable a 'Preview page/image' at a linked page into an overlay.";

const char kEphemeralTabUsingBottomSheetName[] =
    "An ephemeral Preview Tab in the bottom sheet";
const char kEphemeralTabUsingBottomSheetDescription[] =
    "Enable a 'Preview page/image' at a linked page into the bottom sheet.";

const char kExploreSitesName[] = "Explore websites";
const char kExploreSitesDescription[] =
    "Enables portal from new tab page to explore websites.";

const char kGamesHubName[] = "Games Hub";
const char kGamesHubDescription[] =
    "Enables viewing and usage of the Games Hub.";

const char kHomepageLocationName[] =
    "Enable enterprise policy HomepageLocation";
const char kHomepageLocationDescription[] =
    "Enable enterprice policy HomepageLocation, a rule of configure the home "
    "page URL";

const char kInterestFeedNotificationsName[] = "Interest Feed Notifications";
const char kInterestFeedNotificationsDescription[] =
    "Show notifications for some suggested content from the interest feed. "
    "#interest-feed-content-suggestions should also be enabled.";

const char kInterestFeedContentSuggestionsDescription[] =
    "Use the interest feed to render content suggestions. Currently "
    "content "
    "suggestions are shown on the New Tab Page.";
const char kInterestFeedContentSuggestionsName[] =
    "Interest Feed Content Suggestions";

const char kManualPasswordGenerationAndroidName[] =
    "Manual password generation";
const char kManualPasswordGenerationAndroidDescription[] =
    "Whether Chrome should offer users the option to manually request to "
    "generate passwords on Android.";

const char kNoCreditCardAbort[] = "No Credit Card Abort";
const char kNoCreditCardAbortDescription[] =
    "Whether or not the No Credit Card Abort is enabled.";

const char kOfflineIndicatorAlwaysHttpProbeName[] = "Always http probe";
const char kOfflineIndicatorAlwaysHttpProbeDescription[] =
    "Always do http probe to detect network connectivity for offline indicator "
    "as opposed to just taking the connection state from the system."
    "Used for testing.";

const char kOfflineIndicatorChoiceName[] = "Offline indicator choices";
const char kOfflineIndicatorChoiceDescription[] =
    "Show an offline indicator while offline.";

const char kOfflineHomeName[] = "Offline Home";
const char kOfflineHomeDescription[] = "Show offline home UI when offline.";

const char kOfflineIndicatorV2Name[] = "Offline indicator V2";
const char kOfflineIndicatorV2Description[] =
    "Show a persistent offline indicator when offline.";

const char kOfflinePagesCtName[] = "Enable Offline Pages CT features.";
const char kOfflinePagesCtDescription[] = "Enable Offline Pages CT features.";

const char kOfflinePagesCtV2Name[] = "Enable Offline Pages CT V2 features.";
const char kOfflinePagesCtV2Description[] =
    "V2 features include attributing pages to the app that initiated the "
    "custom tabs, and being able to query for pages by page attribution.";

const char kOfflinePagesCTSuppressNotificationsName[] =
    "Disable download complete notification for whitelisted CCT apps.";
const char kOfflinePagesCTSuppressNotificationsDescription[] =
    "Disable download complete notification for page downloads originating "
    "from a CCT app whitelisted to show their own download complete "
    "notification.";

const char kOfflinePagesDescriptiveFailStatusName[] =
    "Enables descriptive failed download status text.";
const char kOfflinePagesDescriptiveFailStatusDescription[] =
    "Enables failed download status text in notifications and Downloads Home "
    "to state the reason the request failed if the failure is actionable.";

const char kOfflinePagesDescriptivePendingStatusName[] =
    "Enables descriptive pending download status text.";
const char kOfflinePagesDescriptivePendingStatusDescription[] =
    "Enables pending download status text in notifications and Downloads Home "
    "to state the reason the request is pending.";

const char kOfflinePagesInDownloadHomeOpenInCctName[] =
    "Enables offline pages in the downloads home to be opened in CCT.";
const char kOfflinePagesInDownloadHomeOpenInCctDescription[] =
    "When enabled offline pages launched from the Downloads Home will be "
    "opened in Chrome Custom Tabs (CCT) instead of regular tabs.";

const char kOfflinePagesLimitlessPrefetchingName[] =
    "Removes resource usage limits for the prefetching of offline pages.";
const char kOfflinePagesLimitlessPrefetchingDescription[] =
    "Allows the prefetching of suggested offline pages to ignore resource "
    "usage limits. This allows it to completely ignore data usage limitations "
    "and allows downloads to happen with any kind of connection.";

const char kOfflinePagesLoadSignalCollectingName[] =
    "Enables collecting load timing data for offline page snapshots.";
const char kOfflinePagesLoadSignalCollectingDescription[] =
    "Enables loading completeness data collection while writing an offline "
    "page.  This data is collected in the snapshotted offline page to allow "
    "data analysis to improve deciding when to make the offline snapshot.";

const char kOfflinePagesPrefetchingName[] =
    "Enables suggested offline pages to be prefetched.";
const char kOfflinePagesPrefetchingDescription[] =
    "Enables suggested offline pages to be prefetched, so useful content is "
    "available while offline.";

const char kOfflinePagesResourceBasedSnapshotName[] =
    "Enables offline page snapshots to be based on percentage of page loaded.";
const char kOfflinePagesResourceBasedSnapshotDescription[] =
    "Enables offline page snapshots to use a resource percentage based "
    "approach for determining when the page is loaded as opposed to a time "
    "based approach";

const char kOfflinePagesRenovationsName[] = "Enables offline page renovations.";
const char kOfflinePagesRenovationsDescription[] =
    "Enables offline page renovations which correct issues with dynamic "
    "content that occur when offlining pages that use JavaScript.";

const char kOfflinePagesLivePageSharingName[] =
    "Enables live page sharing of offline pages";
const char kOfflinePagesLivePageSharingDescription[] =
    "Enables to share current loaded page as offline page by saving as MHTML "
    "first.";

const char kOfflinePagesShowAlternateDinoPageName[] =
    "Enable alternate dino page with more user capabilities.";
const char kOfflinePagesShowAlternateDinoPageDescription[] =
    "Enables the dino page to show more buttons and offer existing offline "
    "content.";

const char kOffliningRecentPagesName[] =
    "Enable offlining of recently visited pages";
const char kOffliningRecentPagesDescription[] =
    "Enable storing recently visited pages locally for offline use. Requires "
    "Offline Pages to be enabled.";

const char kPasswordManagerOnboardingAndroidName[] =
    "Password manager onboarding experience";
const char kPasswordManagerOnboardingAndroidDescription[] =
    "This flag enables showing the password manager onboarding experience.";

extern const char kProcessSharingWithDefaultSiteInstancesName[] =
    "Process sharing with default site instances";
extern const char kProcessSharingWithDefaultSiteInstancesDescription[] =
    "When site isolation is disabled, this mode changes how sites are lumped "
    "in to shared processes. For sites that do not require isolation, this "
    "feature groups them into a single 'default' site instance (per browsing "
    "instance) instead of creating unique site instances for each one. This "
    "enables resource savings by creating fewer processes for sites that do "
    "not need isolation.";

extern const char kProcessSharingWithStrictSiteInstancesName[] =
    "Process sharing with strict site instances";
extern const char kProcessSharingWithStrictSiteInstancesDescription[] =
    "When site isolation is disabled, this mode changes how sites are lumped "
    "in to a shared process. Process selection is usually controlled with "
    "site instances. With strict site isolation, each site on a page gets its "
    "own site instance and process. With site isolation disabled and without "
    "this mode, all sites that share a process are put into the same site "
    "instance. This mode adds a third way: site instances are strictly "
    "separated like strict site isolation, but process selection puts multiple "
    "site instances in a single process.";

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

const char kSafeBrowsingUseLocalBlacklistsV2Name[] =
    "Use local Safe Browsing blacklists";
const char kSafeBrowsingUseLocalBlacklistsV2Description[] =
    "If enabled, maintain a copy of Safe Browsing blacklists in the browser "
    "process to check the Safe Browsing reputation of URLs without calling "
    "into GmsCore for every URL.";

const char kSetMarketUrlForTestingName[] = "Set market URL for testing";
const char kSetMarketUrlForTestingDescription[] =
    "When enabled, sets the market URL for use in testing the update menu "
    "item.";

const char kShoppingAssistName[] = "Shopping assist exploration";
const char kShoppingAssistDescription[] =
    "Show some shopping assistance when available";

const char kSiteIsolationForPasswordSitesName[] =
    "Site Isolation For Password Sites";
const char kSiteIsolationForPasswordSitesDescription[] =
    "Security mode that enables site isolation for sites based on "
    "password-oriented heuristics, such as a user typing in a password.";

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

extern const char kTabSwitcherLongpressMenuName[] =
    "Enable tab switcher long-press menu";
extern const char kTabSwitcherLongpressMenuDescription[] =
    "Enable showing a popup menu when the tab switcher is long-pressed, which "
    "displays options for 'NewTab', 'New incognito tab' and 'Close tab'.";

const char kStartSurfaceAndroidName[] = "Start Surface";
const char kStartSurfaceAndroidDescription[] =
    "Enable showing the start surface when launching Chrome via the "
    "launcher.";

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
    "the app menu badge and menu item for updates. For Inline Update, the "
    "update available flag is implied. The 'Inline Update: Success' selection "
    "goes through the whole inline update flow to the end with a successful "
    "outcome. The other 'Inline Update' options go through the same flow, but "
    "stop at various stages, see their error type for details.";
const char kUpdateMenuTypeNone[] = "None";
const char kUpdateMenuTypeUpdateAvailable[] = "Update Available";
const char kUpdateMenuTypeUnsupportedOSVersion[] = "Unsupported OS Version";
const char kUpdateMenuTypeInlineUpdateSuccess[] = "Inline Update: Success";
const char kUpdateMenuTypeInlineUpdateDialogCanceled[] =
    "Inline Update Error: Dialog Canceled";
const char kUpdateMenuTypeInlineUpdateDialogFailed[] =
    "Inline Update Error: Dialog Failed";
const char kUpdateMenuTypeInlineUpdateDownloadFailed[] =
    "Inline Update Error: Download Failed";
const char kUpdateMenuTypeInlineUpdateDownloadCanceled[] =
    "Inline Update Error: Download Canceled";
const char kUpdateMenuTypeInlineUpdateInstallFailed[] =
    "Inline Update Error: Install Failed";

const char kUpdateNotificationSchedulingIntegrationName[] =
    "Enable update notification using notification scheduling system";
const char kUpdateNotificationSchedulingIntegrationDescription[] =
    "if enable update notification will hook up to notification scheduling "
    "system in native side";

const char kUsageStatsDescription[] =
    "When set, enables sharing of per-domain usage stats with the Digital "
    "Wellbeing app on Android, and allows Digital Wellbeing to suspend access "
    "to websites in order to enforce user-defined time limits.";
const char kUsageStatsName[] = "Share Usage Stats with Digital Wellbeing";

const char kInlineUpdateFlowName[] = "Enable Google Play inline update flow";
const char kInlineUpdateFlowDescription[] =
    "When this flag is set, instead of taking the user to the Google Play "
    "Store when an update is available, the user is presented with an inline "
    "flow where they do not have to leave Chrome until the update is ready "
    "to install.";

#if BUILDFLAG(ENABLE_ANDROID_NIGHT_MODE)

const char kAndroidNightModeName[] = "Android Chrome UI dark mode";
const char kAndroidNightModeDescription[] =
    "If enabled, user can enable Android Chrome UI dark mode through settings.";

#endif  // BUILDFLAG(ENABLE_ANDROID_NIGHT_MODE)

// Non-Android -----------------------------------------------------------------

#else  // !defined(OS_ANDROID)

const char kForceDiceMigrationName[] = "Force the migration to Dice";
const char kForceDiceMigrationDescription[] =
    "When enabled the account reconcilor will force the migration to dice";

const char kShowSyncPausedReasonCookiesClearedOnExitName[] =
    "Show sync paused reason is the setup of cookie settings.";
const char kShowSyncPausedReasonCookiesClearedOnExitDescription[] =
    "If enabled and the user is in sync paused state because of cookie settings"
    " set to clear cookies on exit, we show the user a message with the reason"
    " in the user menu.";

const char kAppManagementName[] = "Enable App Management page";
const char kAppManagementDescription[] =
    "Shows the new app management page at chrome://apps.";

const char kCastMediaRouteProviderName[] = "Cast Media Route Provider";
const char kCastMediaRouteProviderDescription[] =
    "Enables the native Cast Media Route Provider implementation to be used "
    "instead of the implementation in the Media Router component extension.";

const char kChromeColorsName[] = "Chrome Colors menu";
const char kChromeColorsDescription[] =
    "Show Chrome Colors menu in the NTP customization menu.";

const char kChromeColorsCustomColorPickerName[] =
    "Custom color picker for Chrome Colors menu";
const char kChromeColorsCustomColorPickerDescription[] =
    "Show custom color picker in Chrome Colors menu.";

const char kDialMediaRouteProviderName[] = "DIAL Media Route Provider";
const char kDialMediaRouteProviderDescription[] =
    "Enables the native DIAL Media Route Provider implementation to be used "
    "instead of the implementation in the Media Router component extension.";

const char kNtpConfirmSuggestionRemovalsName[] =
    "Confirm removing \"realbox\" suggestions on New Tab page";
const char kNtpConfirmSuggestionRemovalsDescription[] =
    "Show a confirmation dialog when removing suggestions from the \"realbox\" "
    "on the New Tab page. Requires #ntp-realbox to be enabled.";

const char kNtpCustomizationMenuV2Name[] = "NTP customization menu version 2";
const char kNtpCustomizationMenuV2Description[] =
    "Use the second version of the NTP customization menu.";

const char kNtpDismissPromosName[] = "Dismiss promos on the New Tab Page";
const char kNtpDismissPromosDescription[] =
    "Enables a UI to persistently dismiss [non-emergency] promos on the "
    "bottom/middle of the New Tab Page";

const char kNtpRealboxName[] = "Real search box in New Tab Page";
const char kNtpRealboxDescription[] =
    "Enables a search box in the middle of the NTP that will accept input "
    "directly (i.e. not be a \"fake\" box). Search results will show below the "
    "non-fake input (\"realbox\").";

const char kNtpRealboxMatchOmniboxThemeName[] =
    "Make the New Tab page real search box match the omnibox's theme colors";
const char kNtpRealboxMatchOmniboxThemeDescription[] =
    "Themes the real search box in the middle of the NTP to match the omnibox. "
    "Only has an an effect if #ntp-realbox is enabled.";

const char kImprovedCookieControlsName[] =
    "Enable improved cookie controls UI in incognito mode";
const char kImprovedCookieControlsDescription[] =
    "Improved UI in Incognito mode for third-party cookie blocking.";

const char kImprovedCookieControlsForThirdPartyCookieBlockingName[] =
    "Enable improved UI for third-party cookie blocking";
const char kImprovedCookieControlsForThirdPartyCookieBlockingDescription[] =
    "Enables an improved UI for existing third-party cookie blocking users.";

const char kEnableReaderModeName[] = "Enable Reader Mode";
const char kEnableReaderModeDescription[] =
    "Allows viewing of simplified web pages by selecting 'Customize and "
    "control Chrome'>'Distill page'";

const char kEnableWebAuthenticationBleSupportName[] =
    "Web Authentication API BLE support";
const char kEnableWebAuthenticationBleSupportDescription[] =
    "Enable support for using Web Authentication API via Bluetooth security "
    "keys";

const char kEnableWebAuthenticationTestingAPIName[] =
    "Web Authentication Testing API";
const char kEnableWebAuthenticationTestingAPIDescription[] =
    "Enable Web Authentication Testing API support, which disconnects the API "
    "implementation from the real world, and allows configuring virtual "
    "authenticator devices for testing";

const char kEnterpriseReportingInBrowserName[] =
    "Enterprise cloud reporting in browser";
const char kEnterpriseReportingInBrowserDescription[] =
    "Enable the enterprise cloud reporting in browser without installing the "
    "reporting companion extension. This feautre requires device level cloud "
    "mangement.";

const char kHappinessTrackingSurveysForDesktopName[] =
    "Happiness Tracking Surveys";
const char kHappinessTrackingSurveysForDesktopDescription[] =
    "Enable showing Happiness Tracking Surveys to users on Desktop";

const char kHappinessTrackingSurveysForDesktopDemoName[] =
    "Happiness Tracking Surveys Demo";
const char kHappinessTrackingSurveysForDesktopDemoDescription[] =
    "Enable showing Happiness Tracking Surveys Demo to users on Desktop";

const char kIntentPickerName[] = "Intent picker";
const char kIntentPickerDescription[] =
    "When going to a site that has URL managable by a PWA, show the intent"
    "picker to allow user to open the URL in the app.";

const char kKernelnextVMsName[] = "Enable VMs on experimental kernels.";
const char kKernelnextVMsDescription[] =
    "Enables VM support on devices running experimental kernel versions.";

const char kMirroringServiceName[] = "Mirroring Service";
const char kMirroringServiceDescription[] =
    "Enables the native Mirroring Service for mirroring tabs or desktop to "
    "Chromecast.  Requires AudioServiceAudioStreams to also be enabled.";

const char kOmniboxDriveSuggestionsName[] =
    "Omnibox Google Drive Document suggestions";
const char kOmniboxDriveSuggestionsDescriptions[] =
    "Display suggestions for Google Drive documents in the omnibox when Google "
    "is the default search engine.";

const char kOmniboxExperimentalKeywordModeName[] =
    "Omnibox Experimental Keyword Mode";
const char kOmniboxExperimentalKeywordModeDescription[] =
    "Enables various experimental features related to keyword mode, its "
    "suggestions and layout.";

const char kOmniboxLooseMaxLimitOnDedicatedRowsName[] =
    "Omnibox Loose Max Limit on Dedicated Rows";
const char kOmniboxLooseMaxLimitOnDedicatedRowsDescription[] =
    "Enables not counting submatch suggestions towards total suggestion "
    "count.";

const char kOmniboxPedalSuggestionsName[] = "Omnibox Pedal suggestions";
const char kOmniboxPedalSuggestionsDescription[] =
    "Enable omnibox Pedal suggestions to accelerate actions within Chrome by "
    "detecting user intent and offering direct access to the end goal.";

const char kOmniboxReverseAnswersName[] = "Omnibox reverse answers";
const char kOmniboxReverseAnswersDescription[] =
    "Display answers with rows reversed (swapped); except definitions.";

const char kOmniboxShortBookmarkSuggestionsName[] =
    "Omnibox short bookmark suggestions";
const char kOmniboxShortBookmarkSuggestionsDescription[] =
    "Match very short input words to beginning of words in bookmark "
    "suggestions.";

const char kOmniboxSuggestionTransparencyOptionsName[] =
    "Omnibox Suggestion Transparency Options";
const char kOmniboxSuggestionTransparencyOptionsDescription[] =
    "Improves transparency of and control over omnibox suggestions. This "
    "includes \"Why this Suggestion?\" and user controls to delete "
    "personalized suggestions.";

const char kOmniboxTabSwitchSuggestionsName[] =
    "Omnibox tab switch suggestions";
const char kOmniboxTabSwitchSuggestionsDescription[] =
    "Enable suggestions for switching to open tabs within the Omnibox.";

const char kOmniboxTabSwitchSuggestionsDedicatedRowName[] =
    "Omnibox dedicated row tab switch suggestions";
const char kOmniboxTabSwitchSuggestionsDedicatedRowDescription[] =
    "Put each tab switch suggestion in a separate suggestion, immediately "
    "following the original suggestion.";

const char kTabFreezeName[] = "Tab Freeze";
const char kTabFreezeDescription[] =
    "Enables freezing eligible tabs when they have been backgrounded for 5 "
    "minutes.";

const char kWebUIA11yEnhancementsName[] =
    "Enable accessibility enhancements in WebUI";
const char kWebUIA11yEnhancementsDescription[] =
    "This flag covers a wide variety of accessibility enhancements in WebUI "
    "and is used to demonstrate the enhancements for review and testing. When "
    "a enhancement is ready to be released, the enhancement will be taken out "
    "from behind this feature flag. This flag will remain disabled and ready "
    "to be used for other enhancements.";

const char kGoogleBrandedContextMenuName[] =
    "Google branding in the context menu";
const char kGoogleBrandedContextMenuDescription[] =
    "Shows a Google icon next to context menu items powered by Google "
    "services.";

#endif  // !defined(OS_ANDROID)

// Windows ---------------------------------------------------------------------

#if defined(OS_WIN)

const char kCalculateNativeWinOcclusionName[] =
    "Calculate window occlusion on Windows";
const char kCalculateNativeWinOcclusionDescription[] =
    "Calculate window occlusion on Windows will be used in the future "
    "to throttle and potentially unload foreground tabs in occluded windows";

const char kCloudPrintXpsName[] = "XPS in Google Cloud Print";
const char kCloudPrintXpsDescription[] =
    "XPS enables advanced options for classic printers connected to the Cloud "
    "Print with Chrome. Printers must be re-connected after changing this "
    "flag.";

const char kD3D11VideoDecoderName[] = "D3D11 Video Decoder";
const char kD3D11VideoDecoderDescription[] =
    "Enables D3D11VideoDecoder for hardware accelerated video decoding.";

const char kEnableAuraTooltipsOnWindowsName[] =
    "Enable aura tooltips on Windows";
const char kEnableAuraTooltipsOnWindowsDescription[] =
    "Enables aura tooltips instead of the native comctl32 tooltips on Windows.";

const char kEnableGpuAppcontainerName[] = "Enable GPU AppContainer Lockdown.";
const char kEnableGpuAppcontainerDescription[] =
    "Enables the use of an AppContainer for the GPU sandboxed processes to "
    "improve security.";

const char kExperimentalFlingAnimationName[] =
    "Enable experimental fling animation";
const char kExperimentalFlingAnimationDescription[] =
    "Enables the use of a touch fling curve that is based on the behavior of "
    "native apps on Windows.";

const char kGdiTextPrinting[] = "GDI Text Printing";
const char kGdiTextPrintingDescription[] =
    "Use GDI to print text as simply text";

const char kUseAngleName[] = "Choose ANGLE graphics backend";
const char kUseAngleDescription[] =
    "Choose the graphics backend for ANGLE. D3D11 is used on most Windows "
    "computers by default. Using the OpenGL driver as the graphics backend may "
    "result in higher performance in some graphics-heavy applications, "
    "particularly on NVIDIA GPUs. It can increase battery and memory usage of "
    "video playback.";

const char kUseAngleDefault[] = "Default";
const char kUseAngleGL[] = "OpenGL";
const char kUseAngleD3D11[] = "D3D11";
const char kUseAngleD3D9[] = "D3D9";
const char kUseAngleD3D11on12[] = "D3D11on12";

const char kUseWinrtMidiApiName[] = "Use Windows Runtime MIDI API";
const char kUseWinrtMidiApiDescription[] =
    "Use Windows Runtime MIDI API for WebMIDI (effective only on Windows 10 or "
    "later).";

#if BUILDFLAG(ENABLE_SPELLCHECK)
const char kWinUseBrowserSpellCheckerName[] = "Use the Windows OS spellchecker";
const char kWinUseBrowserSpellCheckerDescription[] =
    "Use the Windows OS spellchecker to find spelling mistakes and provide "
    "spelling suggestions instead of using the Hunspell engine.";
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

#endif  // defined(OS_WIN)

// Mac -------------------------------------------------------------------------

#if defined(OS_MACOSX)

const char kImmersiveFullscreenName[] = "Immersive Fullscreen Toolbar";
const char kImmersiveFullscreenDescription[] =
    "Automatically hide and show the toolbar in fullscreen.";

const char kEnableCustomMacPaperSizesName[] = "Enable custom paper sizes";
const char kEnableCustomMacPaperSizesDescription[] =
    "Allow use of custom paper sizes in Print Preview.";

const char kMacTouchBarName[] = "Hardware Touch Bar";
const char kMacTouchBarDescription[] = "Control the use of the Touch Bar.";

const char kMacSyscallSandboxName[] = "Mac Syscall Filtering Sandbox";
const char kMacSyscallSandboxDescription[] =
    "Controls whether the macOS sandbox filters syscalls.";

const char kMacV2GPUSandboxName[] = "Mac V2 GPU Sandbox";
const char kMacV2GPUSandboxDescription[] =
    "Controls whether the GPU process on macOS uses the V1 or V2 sandbox.";

const char kMacViewsTaskManagerName[] = "Toolkit-Views Task Manager.";
const char kMacViewsTaskManagerDescription[] =
    "Controls whether to use the Toolkit-Views based Task Manager.";

const char kMacSystemMediaPermissionsInfoUiName[] =
    "System media permissions info UI";
const char kMacSystemMediaPermissionsInfoUiDescription[] =
    "In case a website is trying to use the camera/microphone, but Chrome "
    "itself is blocked on the system level to access these, show an icon in "
    "the Omnibox, which, when clicked, displays a bubble with information on "
    "how to toggle Chrome's system-level media permissions.";

const char kMetalName[] = "Metal";
const char kMetalDescription[] =
    "Use Metal instead of OpenGL for rasterization (if out-of-process "
    "rasterization is enabled) and display (if the Skia renderer is enabled)";

#endif

// Chrome OS -------------------------------------------------------------------

#if defined(OS_CHROMEOS)

const char kAcceleratedMjpegDecodeName[] =
    "Hardware-accelerated mjpeg decode for captured frame";
const char kAcceleratedMjpegDecodeDescription[] =
    "Enable hardware-accelerated mjpeg decode for captured frame where "
    "available.";

const char kAggregatedMlAppRankingName[] = "Rank suggested apps with ML.";
const char kAggregatedMlAppRankingDescription[] =
    "Use the aggregated ML model to rank the suggested apps.";

const char kAggregatedMlSearchRankingName[] = "Rank search results with ML.";
const char kAggregatedMlSearchRankingDescription[] =
    "Use the aggregated ML model to rank the non-app search results for "
    "non-empty queries.";

const char kAllowAmbientEQName[] = "Show Ambient EQ UI";
const char kAllowAmbientEQDescription[] =
    "Shows the UI to enable Ambient EQ on devices that support it.";

const char kAllowDisableMouseAccelerationName[] =
    "Allow disabling mouse acceleration";
const char kAllowDisableMouseAccelerationDescription[] =
    "Shows a setting to disable mouse acceleration.";

const char kAppServiceAshName[] = "App Service Ash";
const char kAppServiceAshDescription[] =
    "Use the App Service to provide data to the Ash UI, such as the app list.";

const char kAppServiceInstanceRegistryName[] = "App Service Instance Registry";
const char kAppServiceInstanceRegistryDescription[] =
    "Use the App Service to provide app instance information, such as the "
    "app window running status.";

const char kAppServiceIntentHandlingName[] = "App Service Intent Handling";
const char kAppServiceIntentHandlingDescription[] =
    "Use the App Service to provide data for intent handling.";

const char kAppServiceShelfName[] = "App Service Shelf";
const char kAppServiceShelfDescription[] =
    "Use the App Service to provide data to the UI Shelf";

const char kArcApplicationZoomName[] = "Allow zooming of Android apps";
const char kArcApplicationZoomDescription[] =
    "Allow Android apps to be zoomed in/out using ctrl+/-.";

const char kArcBootCompleted[] = "Load Android apps automatically";
const char kArcBootCompletedDescription[] =
    "Allow Android apps to start automatically after signing in.";

const char kArcCupsApiName[] = "ARC CUPS API";
const char kArcCupsApiDescription[] =
    "Enables support of libcups APIs from ARC";

const char kArcCustomTabsExperimentName[] =
    "Enable Custom Tabs experiment for ARC";
const char kArcCustomTabsExperimentDescription[] =
    "Allow Android apps to use Custom Tabs."
    "This feature only works on the Canary and Dev channels.";

const char kArcDocumentsProviderName[] = "ARC DocumentsProvider integration";
const char kArcDocumentsProviderDescription[] =
    "Enables DocumentsProvider integration in Chrome OS Files app.";

const char kArcFilePickerExperimentName[] =
    "Enable file picker experiment for ARC";
const char kArcFilePickerExperimentDescription[] =
    "Enables using Chrome OS file picker in ARC.";

const char kArcNativeBridgeToggleName[] =
    "Toggle between native bridge implementations for ARC";
const char kArcNativeBridgeToggleDescription[] =
    "Toggle between native bridge implementations for ARC.";

const char kArcPrintSpoolerExperimentName[] =
    "Enable print spooler experiment for ARC";
const char kArcPrintSpoolerExperimentDescription[] =
    "Enables using Chrome OS print system and print preview in ARC."
    "This feature only works on the Canary and Dev channels.";

const char kArcUsbHostName[] = "Enable ARC USB host integration";
const char kArcUsbHostDescription[] =
    "Allow Android apps to use USB host feature on ChromeOS devices.";

const char kArcUsbStorageUIName[] = "Enable ARC USB Storage UI";
const char kArcUsbStorageUIDescription[] =
    "Enable experimental UI for controlling ARC access to USB storage devices.";

const char kArcVpnName[] = "Enable ARC VPN integration";
const char kArcVpnDescription[] =
    "Allow Android VPN clients to tunnel Chrome traffic.";

const char kAshDragWindowFromShelfName[] =
    "Enable dragging a window from shelf to overview or home screen.";
const char kAshDragWindowFromShelfDescription[] =
    "Enable dragging a window from shelf to overview or home screen.";

const char kAshEnableDisplayMoveWindowAccelsName[] =
    "Enable shortcuts for moving window between displays.";
const char kAshEnableDisplayMoveWindowAccelsDescription[] =
    "Enable shortcuts for moving window between displays.";

const char kAshEnableOverviewRoundedCornersName[] =
    "Enable rounded corners in overview mode.";
const char kAshEnableOverviewRoundedCornersDescription[] =
    "Enables rounded corners on overview windows.";

const char kAshEnablePersistentWindowBoundsName[] =
    "Enable persistent window bounds in multi-displays scenario.";
const char kAshEnablePersistentWindowBoundsDescription[] =
    "Enable persistent window bounds in multi-displays scenario.";

const char kAshEnablePipRoundedCornersName[] =
    "Enable Picture-in-Picture rounded corners.";
const char kAshEnablePipRoundedCornersDescription[] =
    "Enable rounded corners on the Picture-in-Picture window.";

const char kAshEnableUnifiedDesktopName[] = "Unified desktop mode";
const char kAshEnableUnifiedDesktopDescription[] =
    "Enable unified desktop mode which allows a window to span multiple "
    "displays.";

extern const char kAshNotificationStackingBarRedesignName[] =
    "Redesigned notification stacking bar";
extern const char kAshNotificationStackingBarRedesignDescription[] =
    "Enables the redesigned notification stacking bar UI with a \"Clear all\" "
    "button.";

const char kAshSwapSideVolumeButtonsForOrientationName[] =
    "Swap side volume buttons to match screen orientation.";
const char kAshSwapSideVolumeButtonsForOrientationDescription[] =
    "Make the side volume button that's closer to the top/right always "
    "increase the volume and the button that's closer to the bottom/left "
    "always decrease the volume.";

const char kAshSwipingFromLeftEdgeToGoBackName[] =
    "Swping from the left edge of the display to go back to the previous page.";
const char kAshSwipingFromLeftEdgeToGoBackDescription[] =
    "Swiping from the restricted left area of the display with enough drag "
    "distance or fling velocity could go back to the previous page while in "
    "tablet mode.";

const char kBluetoothAggressiveAppearanceFilterName[] =
    "Aggressive Bluetooth device filtering";
const char kBluetoothAggressiveAppearanceFilterDescription[] =
    "Enables a more aggressive Bluetooth filter in the UI to hide devices that "
    "likely cannot be connected to.";

const char kCameraSystemWebAppName[] = "Camera System Web App";
const char kCameraSystemWebAppDescription[] =
    "Run the Chrome Camera App as a System Web App.";

const char kCrOSContainerName[] = "Chrome OS Container";
const char kCrOSContainerDescription[] =
    "Enable the use of Chrome OS Container utility.";

const char kCrosRegionsModeName[] = "Cros-regions load mode";
const char kCrosRegionsModeDescription[] =
    "This flag controls cros-regions load mode";
const char kCrosRegionsModeDefault[] = "Default";
const char kCrosRegionsModeOverride[] = "Override VPD values.";
const char kCrosRegionsModeHide[] = "Hide VPD values.";

const char kCrostiniAppSearchName[] = "Crostini App Search";
const char kCrostiniAppSearchDescription[] =
    "Enable search and installation of Crostini apps in the launcher.";

const char kCrostiniBackupName[] = "Crostini Backup";
const char kCrostiniBackupDescription[] = "Enable Crostini export and import.";

const char kCrostiniUseBusterImageName[] = "New Crostini containers use Buster";
const char kCrostiniUseBusterImageDescription[] =
    "New Crostini containers use Debian Buster images instead of Debian "
    "Stretch.";

const char kCrostiniGpuSupportName[] = "Crostini GPU Support";
const char kCrostiniGpuSupportDescription[] = "Enable Crostini GPU support.";

const char kCrostiniUsbAllowUnsupportedName[] =
    "Crostini Usb Allow Unsupported";
const char kCrostiniUsbAllowUnsupportedDescription[] =
    "Allow mounting unsupported Usb devices in Crostini. At your own risk. ";

const char kCrostiniWebUIInstallerName[] = "Crostini WebUI Installer";
const char kCrostiniWebUIInstallerDescription[] =
    "Enable the new WebUI Crostini Installer.";

const char kCrostiniWebUIUpgraderName[] = "Crostini WebUI Upgrader";
const char kCrostiniWebUIUpgraderDescription[] =
    "Enable the new WebUI Crostini Upgrader Dialog.";

const char kCryptAuthV1DeviceSyncDeprecateName[] =
    "Deprecate CryptAuth v1 DeviceSync";
const char kCryptAuthV1DeviceSyncDeprecateDescription[] =
    "Deprecate the CryptAuth v1 DeviceSync protocol. The v2 DeviceSync flag "
    "should be enabled before this flag is flipped.";

const char kCryptAuthV2DeviceActivityStatusName[] =
    "CryptAuth Device Activity Status";
const char kCryptAuthV2DeviceActivityStatusDescription[] =
    "Use the CryptAuth GetDevicesActivityStatus API to sort devices.";

const char kCryptAuthV2DeviceSyncName[] = "CryptAuth v2 DeviceSync";
const char kCryptAuthV2DeviceSyncDescription[] =
    "Use the CryptAuth v2 DeviceSync protocol. Note: v1 DeviceSync will "
    "continue to run until the deprecation flag is flipped.";

const char kCryptAuthV2EnrollmentName[] = "CryptAuth v2 Enrollment";
const char kCryptAuthV2EnrollmentDescription[] =
    "Use the CryptAuth v2 Enrollment protocol.";

const char kCupsPrintersUiOverhaulName[] = "Cups Printers UI overhaul";
const char kCupsPrintersUiOverhaulDescription[] =
    "Enables the new native printing UI in settings.";

const char kDisableCancelAllTouchesName[] = "Disable CancelAllTouches()";
const char kDisableCancelAllTouchesDescription[] =
    "If enabled, a canceled touch will not force all other touches to be "
    "canceled.";

const char kDisableExplicitDmaFencesName[] = "Disable explicit dma-fences";
const char kDisableExplicitDmaFencesDescription[] =
    "Always rely on implicit syncrhonization between GPU and display "
    "controller instead of using dma-fences explcitily when available.";

const char kEnableUseHDRTransferFunctionName[] =
    "Enable using HDR transfer function";
const char kEnableUseHDRTransferFunctionDescription[] =
    "Allows using an HDR transfer functions if any connected monitor supports "
    "it";

const char kDisableOfficeEditingComponentAppName[] =
    "Disable Office Editing for Docs, Sheets & Slides";
const char kDisableOfficeEditingComponentAppDescription[] =
    "Disables Office Editing for Docs, Sheets & Slides component app so "
    "handlers won't be registered, making it possible to install another "
    "version for testing.";

const char kDisableTabletAutohideTitlebarsName[] =
    "Disable autohide titlebars in tablet mode";
const char kDisableTabletAutohideTitlebarsDescription[] =
    "Disable tablet mode autohide titlebars functionality. The user will be "
    "able to see the titlebar in tablet mode.";

const char kDoubleTapToZoomInTabletModeName[] =
    "Double-tap to zoom in tablet mode";
const char kDoubleTapToZoomInTabletModeDescription[] =
    "If Enabled, double tapping in webpages while in tablet mode will zoom the "
    "page.";

const char kEnableAdvancedPpdAttributesName[] =
    "Enable advanced PPD attributes";
const char kEnableAdvancedPpdAttributesDescription[] =
    "Enable advanced settings on CUPS printers";

const char kEnableAppDataSearchName[] = "Enable app data search in launcher";
const char kEnableAppDataSearchDescription[] =
    "Allow launcher search to access data available through Firebase App "
    "Indexing";

const char kEnableAppReinstallZeroStateName[] =
    "Enable Zero State App Reinstall Suggestions.";
const char kEnableAppReinstallZeroStateDescription[] =
    "Enable Zero State App Reinstall Suggestions feature in launcher, which "
    "will show app reinstall recommendations at end of zero state list.";

const char kEnableAppGridGhostName[] = "App Grid Ghosting";
const char kEnableAppGridGhostDescription[] =
    "Enables ghosting during an item drag in launcher.";

const char kEnableSearchBoxSelectionName[] = "Search Box Selection";
const char kEnableSearchBoxSelectionDescription[] =
    "Enables the ResultSelectionController in the Search Box. This alters "
    "perceived focus traversal.";

const char kEnableAppListSearchAutocompleteName[] =
    "App List Search Autocomplete";
const char kEnableAppListSearchAutocompleteDescription[] =
    "Allow App List search box to autocomplete queries for Google searches and "
    "apps.";

const char kEnableArcUnifiedAudioFocusName[] =
    "Enable unified audio focus on ARC";
const char kEnableArcUnifiedAudioFocusDescription[] =
    "If audio focus is enabled in Chrome then this will delegate audio focus "
    "control in Android apps to Chrome.";

const char kEnableAssistantAppSupportName[] = "Enable Assistant App Support";
const char kEnableAssistantAppSupportDescription[] =
    "Enable the Assistant App Support feature";

const char kEnableAssistantLauncherIntegrationName[] =
    "Assistant & Launcher integration";
const char kEnableAssistantLauncherIntegrationDescription[] =
    "Combine Launcher search with the power of Assistant to provide the most "
    "useful answer for each query. Requires Assistant to be enabled.";

const char kEnableAssistantLauncherUIName[] = "Assistant Launcher UI";
const char kEnableAssistantLauncherUIDescription[] =
    "Enables the embedded Assistant UI in the app list. Requires Assistant to "
    "be enabled.";

const char kEnableAssistantMediaSessionIntegrationName[] =
    "Assistant Media Session integration";
const char kEnableAssistantMediaSessionIntegrationDescription[] =
    "Enable Assistant Media Session Integration.";

const char kEnableAssistantRoutinesName[] = "Assistant Routines";
const char kEnableAssistantRoutinesDescription[] = "Enable Assistant Routines.";

const char kEnableBackgroundBlurName[] = "Enable background blur.";
const char kEnableBackgroundBlurDescription[] =
    "Enables background blur for the Launcher, Shelf, Unified System Tray etc.";

const char kEnableCrOSActionRecorderName[] = "Enable CrOS action recorder";
const char kEnableCrOSActionRecorderDescription[] =
    "When enabled, each app launching, file opening, setting change, and url "
    "visiting will be logged locally into an encrypted file. Should not be "
    "enabled. Be aware that hash option only provides a thin layer of privacy.";

const char kEnableDiscoverAppName[] = "Enable Discover App";
const char kEnableDiscoverAppDescription[] =
    "Enable Discover App icon in launcher.";

const char kEnableEncryptionMigrationName[] =
    "Enable encryption migration of user data";
const char kEnableEncryptionMigrationDescription[] =
    "If enabled and the device supports ARC, the user will be asked to update "
    "the encryption of user data when the user signs in.";

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

const char kEnableHomeLauncherName[] = "Enable home launcher";
const char kEnableHomeLauncherDescription[] =
    "Enable home launcher in tablet mode.";

const char kEnableNeuralStylusPalmRejectionName[] =
    "Enable Neural Palm Detection";
const char kEnableNeuralStylusPalmRejectionDescription[] =
    "Experimental: Enable Neural Palm detection. Not compatible with all "
    "devices.";

const char kEnableParentalControlsSettingsName[] =
    "Enable Parental controls settings";
const char kEnableParentalControlsSettingsDescription[] =
    "Enable Parental Control options in settings.";

const char kEnablePlayStoreSearchName[] = "Enable Play Store search";
const char kEnablePlayStoreSearchDescription[] =
    "Enable Play Store search in launcher.";

const char kEnableVideoPlayerNativeControlsName[] =
    "Enable native controls in video player app";
const char kEnableVideoPlayerNativeControlsDescription[] =
    "Enable native controls in video player app";

const char kEnableVirtualDesksName[] = "Enable Virtual Desks";
const char kEnableVirtualDesksDescription[] =
    "A preview of the upcoming Virtual Desks features on Chrome OS devices.";

const char kTrimOnFreezeName[] = "Trim Working Set on freeze";
const char kTrimOnFreezeDescription[] = "Trim Working Set on all frames frozen";

const char kTrimOnMemoryPressureName[] = "Trim Working Set on memory pressure";
const char kTrimOnMemoryPressureDescription[] =
    "Trim Working Set periodically on memory pressure";

const char kEnableZeroStateSuggestionsName[] = "Enable Zero State Suggetions";
const char kEnableZeroStateSuggestionsDescription[] =
    "Enable Zero State Suggestions feature in Launcher, which will show "
    "suggestions when launcher search box is active with an empty query";

const char kExoPointerLockName[] = "Pointer lock for Linux applications";
const char kExoPointerLockDescription[] =
    "Allow Linux applications to request a pointer lock, i.e. exclusive use of "
    "the mouse pointer.";

const char kExperimentalAccessibilityChromeVoxLanguageSwitchingName[] =
    "Enable experimental ChromeVox language switching.";
const char kExperimentalAccessibilityChromeVoxLanguageSwitchingDescription[] =
    "Enable ChromeVox language switching, which changes ChromeVox's "
    "output language upon detection of new language.";

const char kExperimentalAccessibilityChromeVoxSubNodeLanguageSwitchingName[] =
    "Enable experimental ChromeVox sub-node (word-level) language switching.";
const char
    kExperimentalAccessibilityChromeVoxSubNodeLanguageSwitchingDescription[] =
        "Enable ChromeVox language switching at the sub-node level, "
        "which changes ChromeVox's output language upon detection of a new "
        "language. This type of switching is more granular than the ChromeVox"
        "node-level language switching.";

const char kExperimentalAccessibilitySwitchAccessName[] =
    "Experimental feature Switch Access";
const char kExperimentalAccessibilitySwitchAccessDescription[] =
    "Add a setting to enable the prototype of Switch Access";

const char kExperimentalAccessibilitySwitchAccessTextName[] =
    "Enable enhanced Switch Access text input.";
const char kExperimentalAccessibilitySwitchAccessTextDescription[] =
    "Enable experimental or in-progress Switch Access features for improved "
    "text input";

const char kFileManagerFeedbackPanelDescription[] =
    "Enable feedback panel in the Files app.";
const char kFileManagerFeedbackPanelName[] = "Files App. feedback panel";

const char kFileManagerPiexWasmName[] = "Enable FilesApp piex-wasm module";
const char kFileManagerPiexWasmDescription[] =
    "Enable the FilesApp piex-wasm raw image extractor module.";

const char kFileManagerTouchModeName[] = "Files App. touch mode";
const char kFileManagerTouchModeDescription[] =
    "Touchscreen-specific interactions of the Files app.";

const char kFilesNGName[] = "Enable Files App. NG.";
const char kFilesNGDescription[] =
    "Enable the next generation UI style of the file manager.";

const char kFirstRunUiTransitionsName[] =
    "Animated transitions in the first-run tutorial";
const char kFirstRunUiTransitionsDescription[] =
    "Transitions during first-run tutorial are animated.";

const char kFsNosymfollowName[] =
    "Prevent symlink traversal on user-supplied filesystems.";
const char kFsNosymfollowDescription[] =
    "Causes user-supplied filesystems to be mounted with the 'nosymfollow'"
    " option, so the chromuimos LSM denies symlink traversal on the"
    " filesystem.";

const char kFuzzyAppSearchName[] = "Fuzzy app search algorithm in launcher.";
const char kFuzzyAppSearchDescription[] =
    "Uses fuzzy search algorithm for app search in launcher.";

const char kGaiaActionButtonsName[] =
    "Enable action buttons on Gaia login screen";
const char kGaiaActionButtonsDescription[] =
    "Enable primary/secondary action button on Gaia login screen.";

const char kHideArcMediaNotificationsName[] = "Hide ARC media notifications";
const char kHideArcMediaNotificationsDescription[] =
    "Hides media notifications for ARC apps. Requires "
    "#enable-media-session-notifications to be enabled.";

const char kImeInputLogicFstName[] = "Enable FST Input Logic on IME";
const char kImeInputLogicFstDescription[] =
    "Enable FST Input Logic to replace the IME legacy input logic on NaCl";

const char kImeNativeDecoderName[] = "Enable native decoders in IME Service";
const char kImeNativeDecoderDescription[] =
    "Enable native decoders in IME service to deprecate NaCl decoders";

const char kListAllDisplayModesName[] = "List all display modes";
const char kListAllDisplayModesDescription[] =
    "Enables listing all external displays' modes in the display settings.";

const char kLockScreenMediaControlsName[] = "Lock screen media controls";
const char kLockScreenMediaControlsDescription[] =
    "Enable media controls on the lock screen.";

const char kLockScreenNotificationName[] = "Lock screen notification";
const char kLockScreenNotificationDescription[] =
    "Enable notifications on the lock screen.";

extern const char kMediaAppName[] = "Media App";
extern const char kMediaAppDescription[] =
    "Enables the chrome://media-app System Web App (SWA)";

const char kMediaSessionNotificationsName[] = "Media session notifications";
const char kMediaSessionNotificationsDescription[] =
    "Shows notifications for media sessions showing the currently playing "
    "media and providing playback controls";

const char kNetworkPortalNotificationName[] =
    "Notifications about captive portals";
const char kNetworkPortalNotificationDescription[] =
    "If enabled, notification is displayed when device is connected to a "
    "network behind captive portal.";

const char kNewOverviewTabletLayoutName[] = "New overview tablet mode layout";
const char kNewOverviewTabletLayoutDescription[] =
    "If enabled, in tablet mode, overview items will be laid out in two rows "
    "with six items showing at a time. Users can scroll through to see hidden "
    "items.";

const char kNewZipUnpackerName[] = "ZIP Archiver (unpacking)";
const char kNewZipUnpackerDescription[] =
    "Use the ZIP Archiver for mounting/unpacking ZIP files";

const char kPrinterProviderSearchAppName[] =
    "Chrome Web Store Gallery app for printer drivers";
const char kPrinterProviderSearchAppDescription[] =
    "Enables Chrome Web Store Gallery app for printer drivers. The app "
    "searches Chrome Web Store for extensions that support printing to a USB "
    "printer with specific USB ID.";

const char kPrintServerUiName[] = "Print Server UI";
const char kPrintServerUiDescription[] =
    "Enables users to add their own print server in the printer settings page.";

const char kReduceDisplayNotificationsName[] = "Reduce display notifications";
const char kReduceDisplayNotificationsDescription[] =
    "If enabled, notifications for display rotation, display removed, display "
    "mirroring, and display extending will be suppressed.";

const char kReleaseNotesName[] = "CrOS Release Notes.";
const char kReleaseNotesDescription[] =
    "Creates release notes app in settings menu that shows a webview "
    "describing new OS features.";

const char kReleaseNotesNotificationName[] = "CrOS Release Notes Notification.";
const char kReleaseNotesNotificationDescription[] =
    "Instructs OS to show notificationlogin after update that release notes "
    "are now available.";

const char kSchedulerConfigurationName[] = "Scheduler Configuration";
const char kSchedulerConfigurationDescription[] =
    "Instructs the OS to use a specific scheduler configuration setting.";
const char kSchedulerConfigurationConservative[] =
    "Disables Hyper-Threading on relevant CPUs.";
const char kSchedulerConfigurationPerformance[] =
    "Enables Hyper-Threading on relevant CPUs.";

const char kShowBluetoothDebugLogToggleName[] =
    "Show Bluetooth debug log toggle";
const char kShowBluetoothDebugLogToggleDescription[] =
    "Enables a toggle which can enable debug (i.e., verbose) logs for "
    "Bluetooth";

const char kShowBluetoothDeviceBatteryName[] = "Show Bluetooth device battery";
const char kShowBluetoothDeviceBatteryDescription[] =
    "Enables showing the battery level of connected and supported Bluetooth "
    "devices in the System Tray and Settings UI.";

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

const char kSmartDimModelV3Name[] = "Smart Dim updated model";
const char kSmartDimModelV3Description[] =
    "Uses an updated model for user activity prediction (Smart Dim).";

const char kSmartTextSelectionName[] = "Smart Text Selection";
const char kSmartTextSelectionDescription[] =
    "Shows quick actions for text "
    "selections in the context menu.";

const char kSplitSettingsName[] = "Split OS and browser settings";
const char kSplitSettingsDescription[] =
    "Show separate settings for the OS and browser";

const char kSplitSettingsSyncName[] = "Split OS and browser sync";
const char kSplitSettingsSyncDescription[] =
    "Allows OS sync to be configured separately from browser sync. Changes the "
    "OS settings UI to provide controls for OS data types. Requires "
    "#split-settings to be enabled.";

const char kStreamlinedUsbPrinterSetupName[] =
    "Streamlined USB Printer Setup Flow";
const char kStreamlinedUsbPrinterSetupDescription[] =
    "Automatically sets up capable USB printers when plugged in. Shows a "
    "notification with the setup result.";

const char kSyncWifiConfigurationsName[] = "Sync Wi-Fi network configurations";
const char kSyncWifiConfigurationsDescription[] =
    "Enables the option to sync Wi-Fi network configurations with Chrome Sync.";

const char kMessageCenterRedesignName[] = "Enable Message Center Redesign";
const char kMessageCenterRedesignDescription[] =
    "Enables split message center, stacked notification icons and system tray "
    "pagination";

const char kTerminalSystemAppName[] = "Terminal System App";
const char kTerminalSystemAppDescription[] =
    "Enables the Terminal System App at chrome://terminal which is used for "
    "the Chrome OS Linux terminal.";

const char kTerminalSystemAppSplitsName[] = "Terminal System App Splits";
const char kTerminalSystemAppSplitsDescription[] =
    "Enables splits for the Terminal System App.";

const char kTetherName[] = "Instant Tethering";
const char kTetherDescription[] =
    "Enables Instant Tethering. Instant Tethering allows your nearby Google "
    "phone to share its Internet connection with this device.";

const char kTouchscreenCalibrationName[] =
    "Enable/disable touchscreen calibration option in material design settings";
const char kTouchscreenCalibrationDescription[] =
    "If enabled, the user can calibrate the touch screen displays in "
    "chrome://settings/display.";

const char kUseFakeDeviceForMediaStreamName[] = "Use fake video capture device";
const char kUseFakeDeviceForMediaStreamDescription[] =
    "Forces Chrome to use a fake video capture device (a rolling pacman with a "
    "timestamp) instead of the system audio/video devices, for debugging "
    "purposes.";

const char kUiDevToolsName[] = "Enable native UI inspection";
const char kUiDevToolsDescription[] =
    "Enables inspection of native UI elements. For local inspection use "
    "chrome://inspect#other";

const char kUiShowCompositedLayerBordersName[] =
    "Show UI composited layer borders";
const char kUiShowCompositedLayerBordersDescription[] =
    "Show border around composited layers created by UI.";
const char kUiShowCompositedLayerBordersRenderPass[] = "RenderPass";
const char kUiShowCompositedLayerBordersSurface[] = "Surface";
const char kUiShowCompositedLayerBordersLayer[] = "Layer";
const char kUiShowCompositedLayerBordersAll[] = "All";

const char kUiSlowAnimationsName[] = "Slow UI animations";
const char kUiSlowAnimationsDescription[] = "Makes all UI animations slow.";

const char kUsbguardName[] = "Block new USB devices at the lock screen.";
const char kUsbguardDescription[] =
    "Prevents newly connected USB devices from operating at the lock screen"
    " until Chrome OS is unlocked to protect against malicious USB devices."
    " Already connected USB devices will continue to function.";

const char kVaapiJpegImageDecodeAccelerationName[] =
    "VA-API JPEG decode acceleration for images";
const char kVaapiJpegImageDecodeAccelerationDescription[] =
    "Enable or disable decode acceleration of JPEG images (as opposed to camera"
    " captures) using the VA-API.";

const char kVaapiWebPImageDecodeAccelerationName[] =
    "VA-API WebP decode acceleration for images";
const char kVaapiWebPImageDecodeAccelerationDescription[] =
    "Enable or disable decode acceleration of WebP images using the VA-API.";

const char kVirtualKeyboardBorderedKeyName[] = "Virtual Keyboard Bordered Key";
const char kVirtualKeyboardBorderedKeyDescription[] =
    "Show virtual keyboard with bordered key";

const char kVirtualKeyboardName[] = "Virtual Keyboard";
const char kVirtualKeyboardDescription[] =
    "Always show virtual keyboard regardless of having a physical keyboard "
    "present";

const char kWakeOnPacketsName[] = "Wake On Packets";
const char kWakeOnPacketsDescription[] =
    "Enables waking the device based on the receipt of some network packets.";

const char kZeroStateFilesName[] = "Enable Launcher Zero State Files";
const char kZeroStateFilesDescription[] =
    "Enables zero state file recommendations in the Launcher, which appear when"
    "the search box is active and no query has been entered.";

// Prefer keeping this section sorted to adding new definitions down here.

#endif  // defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS) || defined(OS_LINUX)

#if BUILDFLAG(USE_TCMALLOC)
const char kDynamicTcmallocName[] = "Dynamic Tcmalloc Tuning";
const char kDynamicTcmallocDescription[] =
    "Allows tcmalloc to dynamically adjust tunables based on system resource "
    "utilization.";
#endif  // BUILDFLAG(USE_TCMALLOC)

#endif  // #if defined(OS_CHROMEOS) || defined(OS_LINUX)

// All views-based platforms --------------------------------------------------

#if defined(TOOLKIT_VIEWS)

const char kEnableMDRoundedCornersOnDialogsName[] =
    "MD corners on secondary UI";
const char kEnableMDRoundedCornersOnDialogsDescription[] =
    "Increases corner radius on secondary UI.";

const char kInstallableInkDropName[] = "Use InstallableInkDrop where supported";
const char kInstallableInkDropDescription[] =
    "InstallableInkDrop is part of an InkDrop refactoring effort. This enables "
    "the pilot implementation where available.";

const char kReopenTabInProductHelpName[] = "Reopen tab in-product help";
const char kReopenTabInProductHelpDescription[] =
    "Enable in-product help that guides a user to reopen a tab if it looks "
    "like they accidentally closed it.";

#endif  // defined(TOOLKIT_VIEWS)

// Random platform combinations -----------------------------------------------

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS)

const char kWebGL2ComputeContextName[] = "WebGL 2.0 Compute";
const char kWebGL2ComputeContextDescription[] =
    "Enable the use of WebGL 2.0 Compute API.";

#endif  // defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_CLICK_TO_CALL)

const char kClickToCallContextMenuForSelectedTextName[] =
    "Enable click to call feature on desktop when a phone number is selected";
const char kClickToCallContextMenuForSelectedTextDescription[] =
    "Enables click to call feature to be shown in context menu when the user "
    "right clicks after selecting a phone number.";

const char kClickToCallUIName[] =
    "Enable click to call feature signals to be handled on desktop";
const char kClickToCallUIDescription[] =
    "Enables click to call feature signals to be handled on desktop by showing "
    "a list of user's available devices with telephony functionality.";

const char kClickToCallDetectionV2Name[] = "Click to call detection V2";
const char kClickToCallDetectionV2Description[] =
    "Improves the detection of phone numbers in the context menu for the click "
    "to call feature.";

#endif  // BUILDFLAG(ENABLE_CLICK_TO_CALL)

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)

const char kRemoteCopyReceiverName[] =
    "Enables the remote copy feature to receive messages";
const char kRemoteCopyReceiverDescription[] =
    "Enables the remote copy feature to handle messages by writing content to "
    "the clipboard and showing a notification to the user.";

#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) ||
        // defined(OS_CHROMEOS)

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)

const char kDirectManipulationStylusName[] = "Direct Manipulation Stylus";
const char kDirectManipulationStylusDescription[] =
    "If enabled, Chrome will scroll web pages on stylus drag.";

const char kAnimatedAvatarButtonName[] = "Animated avatar button";
const char kAnimatedAvatarButtonDescription[] =
    "If enabled, Chrome will animate a pill with identity information around "
    "the avatar button on start-up and on sign-in.";

const char kProfileMenuRevampName[] = "Profile menu revamp";
const char kProfileMenuRevampDescription[] =
    "Enables the new version of the profile menu (aka user menu).";

const char kSyncClipboardServiceName[] = "Sync Clipboard Service";
const char kSyncClipboardServiceDescription[] =
    "Enables clipboard syncing via Chrome Sync.";

#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)

#if defined(OS_MACOSX) || defined(OS_CHROMEOS)

const char kForceEnableSystemAecName[] = "Force enable system AEC";
const char kForceEnableSystemAecDescription[] =
    "Use system echo canceller instead of WebRTC echo canceller. If there is "
    "no system echo canceller available, getUserMedia with echo cancellation "
    "enabled will fail.";

#endif  // defined(OS_MACOSX) || defined(OS_CHROMEOS)

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)

const char kWebContentsOcclusionName[] = "Enable occlusion of web contents";
const char kWebContentsOcclusionDescription[] =
    "If enabled, web contents will behave as hidden when it is occluded by "
    "other windows.";

#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)

// Feature flags --------------------------------------------------------------

#if defined(DCHECK_IS_CONFIGURABLE)
const char kDcheckIsFatalName[] = "DCHECKs are fatal";
const char kDcheckIsFatalDescription[] =
    "By default Chrome will evaluate in this build, but only log failures, "
    "rather than crashing. If enabled, DCHECKs will crash the calling process.";
#endif  // defined(DCHECK_IS_CONFIGURABLE)

#if BUILDFLAG(ENABLE_VR)

const char kWebXrOrientationSensorDeviceName[] = "Orientation sensors for XR";
const char kWebXrOrientationSensorDeviceDescription[] =
    "Allow use of orientation sensors for XR if present";

#if BUILDFLAG(ENABLE_OCULUS_VR)
const char kOculusVRName[] = "Oculus hardware support";
const char kOculusVRDescription[] =
    "If enabled, Chrome will use Oculus devices for VR (supported only on "
    "Windows 10 or later).";
#endif  // ENABLE_OCULUS_VR

#if BUILDFLAG(ENABLE_OPENVR)
const char kOpenVRName[] = "OpenVR hardware support";
const char kOpenVRDescription[] =
    "If enabled, Chrome will use OpenVR devices for VR (supported only on "
    "Windows 10 or later).";
#endif  // ENABLE_OPENVR

#if BUILDFLAG(ENABLE_WINDOWS_MR)
const char kWindowsMixedRealityName[] = "Windows Mixed Reality support";
const char kWindowsMixedRealityDescription[] =
    "If enabled, Chrome will use Windows Mixed Reality devices for VR"
    " (supported only on Windows 10 or later).";
#endif  // ENABLE_WINDOWS_MR

#if BUILDFLAG(ENABLE_OPENXR)
const char kOpenXRName[] = "OpenXR support";
const char kOpenXRDescription[] =
    "If enabled, Chrome will use OpenXR Backend for VR.";
#endif  // ENABLE_OPENXR

#if !defined(OS_ANDROID)
const char kXRSandboxName[] = "XR device sandboxing";
const char kXRSandboxDescription[] =
    "If enabled, Chrome will host VR APIs in a restricted process on desktop.";
#endif  // !defined(OS_ANDROID)

#endif  // ENABLE_VR

#if BUILDFLAG(ENABLE_NACL)
const char kNaclName[] = "Native Client";
const char kNaclDescription[] =
    "Support Native Client for all web applications, even those that were not "
    "installed from the Chrome Web Store.";
#endif  // ENABLE_NACL

#if BUILDFLAG(ENABLE_PLUGINS)

#if defined(OS_CHROMEOS)
const char kPdfAnnotations[] = "PDF Annotations";
const char kPdfAnnotationsDescription[] = "Enable annotating PDF documents.";
#endif  // defined(OS_CHROMEOS)

const char kPdfFormSaveName[] = "Save PDF Forms";
const char kPdfFormSaveDescription[] =
    "Enable saving PDFs with filled form data.";

#endif  // BUILDFLAG(ENABLE_PLUGINS)

#if defined(TOOLKIT_VIEWS) || defined(OS_ANDROID)

const char kAutofillCreditCardUploadName[] =
    "Enable offering upload of Autofilled credit cards";
const char kAutofillCreditCardUploadDescription[] =
    "Enables a new option to upload credit cards to Google Payments for sync "
    "to all Chrome devices.";

#endif  // defined(TOOLKIT_VIEWS) || defined(OS_ANDROID)

const char kPaintHoldingName[] =
    "Delay the commit to screen for same-origin navigations";
const char kPaintHoldingDescription[] =
    "Enables a delay before commiting the page to screen when navigating "
    "between pages in the same origin. This may help avoid a flash of unstyled "
    "content for same-origin navigations";

#if defined(WEBRTC_USE_PIPEWIRE)

extern const char kWebrtcPipeWireCapturerName[] = "WebRTC PipeWire support";
extern const char kWebrtcPipeWireCapturerDescription[] =
    "When enabled the WebRTC will use the PipeWire multimedia server for "
    "capturing the desktop content on the Wayland display server.";

#endif  // #if defined(WEBRTC_USE_PIPEWIRE)

// ============================================================================
// Don't just add flags to the end, put them in the right section in
// alphabetical order just like the header file.
// ============================================================================

}  // namespace flag_descriptions
