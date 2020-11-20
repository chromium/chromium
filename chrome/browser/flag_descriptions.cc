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

#if BUILDFLAG(ENABLE_PLUGINS)
const char kAccessiblePDFFormName[] = "Accessible PDF Forms";
const char kAccessiblePDFFormDescription[] =
    "Enables accessibility support for PDF forms.";
#endif  // BUILDFLAG(ENABLE_PLUGINS)

const char kAccountIdMigrationName[] = "Account ID migration";
const char kAccountIdMigrationDescription[] =
    "Migrate to use Gaia ID instead of the email as the account identifer for "
    "the Identity Manager.";

const char kAlignFontDisplayAutoTimeoutWithLCPGoalName[] =
    "Align 'font-display: auto' timeout with LCP goal";
const char kAlignFontDisplayAutoTimeoutWithLCPGoalDescription[] =
    "Make pending 'display: auto' web fonts enter the swap or failure period "
    "immediately before reaching the LCP time limit (~2500ms), so that web "
    "fonts do not become a source of bad LCP (Largest Contentful Paint).";

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

const char kConditionalTabStripAndroidName[] = "Conditional Tab Strip";
const char kConditionalTabStripAndroidDescription[] =
    "Allows users to access conditional tab strip.";

const char kConversionMeasurementApiName[] = "Conversion Measurement API";
const char kConversionMeasurementApiDescription[] =
    "Enables usage of the Conversion Measurement API. Requires "
    "#enable-experimental-web-platform-features to be enabled.";

const char kConversionMeasurementDebugModeName[] =
    "Conversion Measurement Debug Mode";
const char kConversionMeasurementDebugModeDescription[] =
    "Enables debug mode for the Conversion Measurement API. This removes all "
    "reporting delays and noise. Only works if the Conversion Measurement API "
    "is already enabled.";

const char kEditPasswordsInSettingsName[] = "Edit passwords in settings";
const char kEditPasswordsInSettingsDescription[] =
    "Enables password editing in settings.";

const char kEnableBloomName[] = "Enable Bloom Integration";
const char kEnableBloomDescription[] =
    "Enables native support for bloom, an experimental vertical knowledge "
    "search feature.";

const char kEnableBluetoothSerialPortProfileInSerialApiName[] =
    "Enable Bluetooth Serial Port Profile in Serial API";
const char kEnableBluetoothSerialPortProfileInSerialApiDescription[] =
    "When enabled, Bluetooth Serial Port Profile devices will be enumerated "
    "for use with the Serial API.";

const char kEnableClipboardProviderImageSuggestionsName[] =
    "Omnibox clipboard image search suggestions";
const char kEnableClipboardProviderImageSuggestionsDescription[] =
    "Enables image search suggestions in the Omnibox.";

const char kEnableFtpName[] = "Enable support for FTP URLs";
const char kEnableFtpDescription[] =
    "When enabled, the browser will handle navigations to ftp:// URLs by "
    "either showing a directory listing or downloading the resource over FTP. "
    "When disabled, the browser has no special handling for ftp:// URLs and "
    "by default defer handling of the URL to the underlying platform.";

const char kShelfAppScalingName[] = "Enable shelf app scaling";
const char kShelfAppScalingDescription[] =
    "Reduce the app icon size from standard to dense when the user exceeds the "
    "available space to add an additional app icon in tablet mode";

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

const char kUseLookalikesForNavigationSuggestionsName[] =
    "Use lookalike URL suggestions for navigation suggestions";
const char kUseLookalikesForNavigationSuggestionsDescription[] =
    "Use lookalike URL suggestions to suggest navigations to users who "
    "face domain not found error.";

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

const char kAndroidPictureInPictureAPIName[] =
    "Picture-in-Picture Web API for Android";
const char kAndroidPictureInPictureAPIDescription[] =
    "Enable Picture-in-Picture Web API for Android";

const char kAppCacheName[] = "AppCache web API and browser backend";
const char kAppCacheDescription[] =
    "When disabled, turns off all AppCache code so that developers "
    "can test that their code works properly in the future when AppCache "
    "has been removed.  If disabled, this will also delete any AppCache data "
    "from profile directories.";

const char kDnsHttpssvcName[] = "Support for HTTPSSVC records in DNS.";
const char kDnsHttpssvcDescription[] =
    "When enabled, Chrome may query a configured DoH server for HTTPSSVC "
    "records. If any HTTPSSVC records are returned, Chrome may upgrade the URL "
    "to HTTPS. If the records indicate support for QUIC, Chrome may attempt "
    "QUIC on the first connection.";

const char kDnsOverHttpsName[] = "Secure DNS lookups";
const char kDnsOverHttpsDescription[] =
    "Enables DNS over HTTPS. When this feature is enabled, your browser may "
    "try to use a secure HTTPS connection to look up the addresses of websites "
    "and other web resources.";

const char kAutofillAlwaysReturnCloudTokenizedCardName[] =
    "Return cloud token details for server credit cards when possible";
const char kAutofillAlwaysReturnCloudTokenizedCardDescription[] =
    "When enabled and where available, forms filled using Google Payments "
    "server cards are populated with cloud token details, including CPAN "
    "(cloud tokenized version of the Primary Account Number) and dCVV (dynamic "
    "CVV).";

const char kAutofillAssistantChromeEntryName[] = "AutofillAssistantChromeEntry";
const char kAutofillAssistantChromeEntryDescription[] =
    "Initiate autofill assistant from within Chrome.";

const char kAutofillCacheQueryResponsesName[] =
    "Cache Autofill Query Responses";
const char kAutofillCacheQueryResponsesDescription[] =
    "When enabled, autofill will cache the responses it receives from the "
    "crowd-sourced field type prediction server.";

const char kAutofillEnableCardNicknameManagementName[] =
    "Enable Autofill card nickname management";
const char kAutofillEnableCardNicknameManagementDescription[] =
    "When enabled, nicknames for credit cards will be able to be modified "
    "locally.";

const char kAutofillEnableCardNicknameUpstreamName[] =
    "Enable Autofill card nickname upstream";
const char kAutofillEnableCardNicknameUpstreamDescription[] =
    "When enabled, nicknames for credit cards stored locally will be able to "
    "be uploaded to Payments.";

const char kAutofillEnableGoogleIssuedCardName[] =
    "Enable Autofill Google-issued card";
const char kAutofillEnableGoogleIssuedCardDescription[] =
    "When enabled, Google-issued cards will be available in the autofill "
    "suggestions.";

const char kAutofillEnableOffersInDownstreamName[] =
    "Enable Autofill offers in downstream";
const char kAutofillEnableOffersInDownstreamDescription[] =
    "When enabled, offer data will be retrieved during downstream and shown in "
    "the dropdown list.";

const char kAutofillEnableStickyPaymentsBubbleName[] =
    "Enable Autofill payments sticky bubbles";
const char kAutofillEnableStickyPaymentsBubbleDescription[] =
    "When enabled, all Autofill payments bubbles will not be dismissed upon "
    "navigation.";

const char kAutofillEnableToolbarStatusChipName[] =
    "Move Autofill omnibox icons next to the profile avatar icon";
const char kAutofillEnableToolbarStatusChipDescription[] =
    "When enabled, Autofill data related icon will be shown in the status "
    "chip next to the profile avatar icon in the toolbar.";

const char kAutofillEnableVirtualCardName[] =
    "Offer to use cloud token virtual card in Autofill";
const char kAutofillEnableVirtualCardDescription[] =
    "When enabled, if all requirements are met, Autofill will offer to use "
    "virtual credit cards in form filling.";

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

const char kAutofillRestrictUnownedFieldsToFormlessCheckoutName[] =
    "Restrict formless form extraction";
const char kAutofillRestrictUnownedFieldsToFormlessCheckoutDescription[] =
    "Restrict extraction of formless forms to checkout flows";

const char kAutofillRichMetadataQueriesName[] =
    "Autofill - Rich metadata queries (Canary/Dev only)";
const char kAutofillRichMetadataQueriesDescription[] =
    "Transmit rich form/field metadata when querying the autofill server. "
    "This feature only works on the Canary and Dev channels.";

const char kAutofillSaveAndFillVPAName[] =
    "Offer save and autofill of UPI/VPA values";
const char kAutofillSaveAndFillVPADescription[] =
    "If enabled, when autofill recognizes a UPI/VPA value in a payment form, "
    "it will offer to save it. If saved, it will be offered for filling in "
    "fields which expect a VPA.";

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

const char kAvatarToolbarButtonName[] = "Avatar Toolbar Button";
const char kAvatarToolbarButtonDescription[] =
    "Enables the avatar toolbar button and the associated menu";

const char kBackForwardCacheName[] = "Back-forward cache";
const char kBackForwardCacheDescription[] =
    "If enabled, caches eligible pages after cross-site navigations."
    "To enable caching pages on same-site navigations too, choose 'enabled "
    "same-site support'.";

const char kBypassAppBannerEngagementChecksName[] =
    "Bypass user engagement checks";
const char kBypassAppBannerEngagementChecksDescription[] =
    "Bypasses user engagement checks for displaying app banners, such as "
    "requiring that users have visited the site before and that the banner "
    "hasn't been shown recently. This allows developers to test that other "
    "eligibility requirements for showing app banners, such as having a "
    "manifest, are met.";

const char kContextMenuGoogleLensChipName[] =
    "Google Lens powered image search for surfaced as a chip below the context "
    "menu.";
const char kContextMenuGoogleLensChipDescription[] =
    "Enable a chip for a Shopping intent into Google Lens when supported. ";

const char kContextMenuSearchWithGoogleLensName[] =
    "Google Lens powered image search in the context menu.";
const char kContextMenuSearchWithGoogleLensDescription[] =
    "Replaces default image search with an intent to Google Lens when "
    "supported.";

const char kContextMenuShopWithGoogleLensName[] =
    "Google Lens powered image search for shoppable images in the context "
    "menu.";
const char kContextMenuShopWithGoogleLensDescription[] =
    "Enable a menu item for a Shopping intent into Google Lens when supported. "
    "By default replaces the Search with Google Lens option.";

const char kContextMenuSearchAndShopWithGoogleLensName[] =
    "Additional menu item for Google Lens image search for shoppable images in "
    "the context menu.";
const char kContextMenuSearchAndShopWithGoogleLensDescription[] =
    "Display an additional menu item for a Shopping intent to Google Lens "
    "below Search with Google Lens when Lens shopping feature is enabled";

const char kClickToOpenPDFName[] = "Click to open embedded PDFs";
const char kClickToOpenPDFDescription[] =
    "When the PDF plugin is unavailable, show a click-to-open placeholder for "
    "embedded PDFs.";

const char kClientStorageAccessContextAuditingName[] =
    "Access contexts for client-side storage";
const char kClientStorageAccessContextAuditingDescription[] =
    "Record the first-party contexts in which client-side storage was accessed";

const char kDecodeJpeg420ImagesToYUVName[] = "YUV decoding for JPEG";
const char kDecodeJpeg420ImagesToYUVDescription[] =
    "Decode and render 4:2:0 formatted jpeg images from YUV instead of RGB."
    "This feature requires GPU or OOP rasterization to also be enabled.";

const char kDecodeLossyWebPImagesToYUVName[] = "YUV Decoding for WebP";
const char kDecodeLossyWebPImagesToYUVDescription[] =
    "Decode and render lossy WebP images from YUV instead of RGB. "
    "You must also have GPU rasterization or OOP rasterization.";

const char kDoubleBufferCompositingName[] = "Double buffered compositing";
const char kDoubleBufferCompositingDescription[] =
    "Use double buffer for compositing (instead of triple-buffering). "
    "Latency should be reduced in some cases. On the other hand, more skipped "
    "frames are expected.";

const char kEnablePasswordsAccountStorageName[] =
    "Enable the account data storage for passwords";
const char kEnablePasswordsAccountStorageDescription[] =
    "Enables storing passwords in a second, Gaia-account-scoped storage for "
    "signed-in but not syncing users";

const char kEnablePasswordsAccountStorageIPHName[] =
    "Enable IPH for the account data storage for passwords";
const char kEnablePasswordsAccountStorageIPHDescription[] =
    "Enables in-product help bubbles about storing passwords in a second, "
    "Gaia-account-scoped storage for signed-in but not syncing users";

const char kFocusMode[] = "Focus Mode";
const char kFocusModeDescription[] =
    "If enabled, allows the user to switch to Focus Mode";

const char kFontAccessAPIName[] = "Font Access APIs";
const char kFontAccessAPIDescription[] =
    "Enables the experimental Font Access APIs, giving websites access "
    "to enumerate local fonts and access their table data.";

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

const char kForcePreferredIntervalForVideoName[] =
    "Force preferred interval for video";
const char kForcePreferredIntervalForVideoDescription[] =
    "When enabled, the composition rate is reduced to match the video playback "
    "rate irrespective of the update frequency of other page content";

const char kDynamicColorGamutName[] = "Dynamic color gamut";
const char kDynamicColorGamutDescription[] =
    "Displays in wide color when the content is wide. When the content is "
    "not wide, displays sRGB";

const char kCompositedLayerBordersName[] = "Composited render layer borders";
const char kCompositedLayerBordersDescription[] =
    "Renders a border around composited Render Layers to help debug and study "
    "layer compositing.";

const char kCookieDeprecationMessagesName[] = "Cookie deprecation messages";
const char kCookieDeprecationMessagesDescription[] =
    "Show messages in DevTools about upcoming deprecations that would affect "
    "sent/received cookies.";

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

const char kDetectTargetEmbeddingLookalikesName[] =
    "Detect target embedding domains as lookalikes.";
const char kDetectTargetEmbeddingLookalikesDescription[] =
    "Shows a lookalike interstitial when navigating to target embedding domains"
    "(e.g. google.com.example.com).";

const char kDeviceDiscoveryNotificationsName[] =
    "Device Discovery Notifications";
const char kDeviceDiscoveryNotificationsDescription[] =
    "Device discovery notifications on local network.";

const char kForceEnableDevicesPageName[] = "Force enable chrome://devices page";
const char kForceEnableDevicesPageDescription[] =
    "With this flag on, the chrome://devices page will show its previous "
    "content, instead of a warning message. Note: chrome://devices is "
    "no longer supported, and this flag will be permanently removed in M88.";

const char kDisableBestEffortTasksName[] = "Skip best effort tasks";
const char kDisableBestEffortTasksDescription[] =
    "With this flag on, tasks of the lowest priority will not be executed "
    "until shutdown. The queue of low priority tasks can increase memory usage."
    "Also, while it should be possible to use Chrome almost normally with this "
    "flag, it is expected that some non-visible operations such as writing "
    "user data to disk, cleaning caches, reporting metrics or updating "
    "components won't be performed until shutdown.";

const char kDisallowDocWrittenScriptsUiName[] =
    "Block scripts loaded via document.write";
const char kDisallowDocWrittenScriptsUiDescription[] =
    "Disallows fetches for third-party parser-blocking scripts inserted into "
    "the main frame via document.write.";

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

const char kEnableAutofillAccountWalletStorageName[] =
    "Enable the account data storage for autofill";
const char kEnableAutofillAccountWalletStorageDescription[] =
    "Enable the ephemeral storage for account data for autofill.";

const char kEnableAutofillCacheServerCardInfoName[] =
    "Enable Autofill to cache unmasked server card info";
const char kEnableAutofillCacheServerCardInfoDescription[] =
    "If enabled, when a server card is unmasked, its info will be cached until "
    "page navigation to simplify consecutive fills on the same page.";

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

const char kEnableAutofillCreditCardCvcPromptGoogleLogoName[] =
    "Enable Google Pay branding on CVC prompt on Android";
const char kEnableAutofillCreditCardCvcPromptGoogleLogoDescription[] =
    "If enabled, show the Google Pay logo on CVC prompt on Android.";

const char kEnableAutofillCreditCardUploadFeedbackName[] =
    "Enable feedback for credit card upload flow";
const char kEnableAutofillCreditCardUploadFeedbackDescription[] =
    "When enabled, if credit card upload succeeds, the avatar button will "
    "show a highlight, otherwise the icon will be updated and if it is "
    "clicked, the save card failure bubble will be shown.";

const char kEnableExperimentalCookieFeaturesName[] =
    "Enable experimental cookie features";
const char kEnableExperimentalCookieFeaturesDescription[] =
    "Enable new features that affect setting, sending, and managing cookies. "
    "The enabled features are subject to change at any time.";

const char kEnableDeferAllScriptName[] = "DeferAllScript previews";
const char kEnableDeferAllScriptDescription[] =
    "Enable deferring synchronous script on slow pages.";

const char kEnableDeferAllScriptWithoutOptimizationHintsName[] =
    "Skip checking optimization hints for Defer Script previews";
const char kEnableDeferAllScriptWithoutOptimizationHintsDescription[] =
    "Skips checking optimization hints for Defer Script previews and assumes "
    "that the ECT trigger threshold is set to 4G (which is otherwise provided "
    "by the optimization hints). Rest of the checks are still executed.";

const char kEnableSaveDataName[] = "Enables save data feature";
const char kEnableSaveDataDescription[] =
    "Enables save data feature. May cause user's traffic to be proxied via "
    "Google's data reduction proxy.";

const char kEnableNavigationPredictorName[] = "Enables navigation predictor";
const char kEnableNavigationPredictorDescription[] =
    "Enables navigation predictor feature that predicts the next likely "
    "navigation using a set of heuristics.";

const char kEnablePreconnectToSearchName[] =
    "Enables preconnections to default search engine";
const char kEnablePreconnectToSearchDescription[] =
    "Enables the feature that preconnects to the user's default search engine.";

const char kEnableNoScriptPreviewsName[] = "NoScript previews";

const char kEnableNoScriptPreviewsDescription[] =
    "Enable disabling JavaScript on some pages on slow networks.";

const char kEnableRemovingAllThirdPartyCookiesName[] =
    "Enable removing SameSite=None cookies";
const char kEnableRemovingAllThirdPartyCookiesDescription[] =
    "Enables UI on chrome://settings/siteData to remove all third-party "
    "cookies and site data.";

const char kColorProviderRedirectionName[] = "Color Provider Redirection";
const char kColorProviderRedirectionDescription[] =
    "Redirects color requests to the color provider where possible.";

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
const char kEnableDataReductionProxyServerExperimentName[] =
    "Use an alternative Data Saver back end configuration.";
const char kEnableDataReductionProxyServerExperimentDescription[] =
    "Enable a different approach to saving data by configuring the back end "
    "server";

const char kDesktopPWAsLocalUpdatingName[] = "Desktop PWAs local updating";
const char kDesktopPWAsLocalUpdatingDescription[] =
    "Enable installed PWAs to update their app manifest data when the site "
    "manifest data has changed.";

const char kDesktopPWAsLocalUpdatingThrottlePersistenceName[] =
    "Desktop PWAs local updating throttle persistence";
const char kDesktopPWAsLocalUpdatingThrottlePersistenceDescription[] =
    "Persist the throttling of local PWA manifest update checks across browser "
    "restarts.";

const char kDesktopPWAsMigrationUserDisplayModeCleanUpName[] =
    "Desktop PWAs migration user_display_mode clean up";
const char kDesktopPWAsMigrationUserDisplayModeCleanUpDescription[] =
    "Runs clean up code to fix web apps that had their window/browser tab "
    "launch preference incorrectly migrated. See issue "
    "https://crbug.com/1125020.";

const char kDesktopPWAsAppIconShortcutsMenuName[] =
    "Desktop PWAs app icon shortcuts menu";
const char kDesktopPWAsAppIconShortcutsMenuDescription[] =
    "Enable installed PWAs to include a menu of shortcuts associated with the "
    "app icon in the taskbar on Windows, or the dock on macOS or Linux.";

const char kDesktopPWAsTabStripName[] = "Desktop PWA tab strips";
const char kDesktopPWAsTabStripDescription[] =
    "Experimental UI for exploring what PWA windows would look like with a tab "
    "strip.";

const char kDesktopPWAsTabStripLinkCapturingName[] =
    "Desktop PWA tab strip link capturing";
const char kDesktopPWAsTabStripLinkCapturingDescription[] =
    "Experimental behaviour for \"Desktop PWA tab strips\" to capture link "
    "navigations within the app scope and bring them into the app's tabbed "
    "window.";

const char kDesktopPWAsWithoutExtensionsName[] =
    "Desktop PWAs without Extensions";
const char kDesktopPWAsWithoutExtensionsDescription[] =
    "New platform to manage Web Applications metadata. Warning: this feature "
    "is incomplete and may corrupt your synced Chrome profile. Test accounts "
    "only are advised.";

const char kDesktopPWAsRunOnOsLoginName[] = "Desktop PWAs run on OS login";
const char kDesktopPWAsRunOnOsLoginDescription[] =
    "Enable installed PWAs to be configured to automatically start when the OS "
    "user logs in.";

const char kEnableMigrateDefaultChromeAppToWebAppsGSuiteName[] =
    "Migrate default G Suite Chrome apps to web apps";
const char kEnableMigrateDefaultChromeAppToWebAppsGSuiteDescription[] =
    "Enable the migration of default installed G Suite Chrome apps over to "
    "their corresponding web apps.";

const char kEnableMigrateDefaultChromeAppToWebAppsNonGSuiteName[] =
    "Migrate default non-G Suite Chrome apps to web apps";
const char kEnableMigrateDefaultChromeAppToWebAppsNonGSuiteDescription[] =
    "Enable the migration of default installed non-G Suite Chrome apps over to "
    "their corresponding web apps.";

const char kEnableTLS13EarlyDataName[] = "TLS 1.3 Early Data";
const char kEnableTLS13EarlyDataDescription[] =
    "This option enables TLS 1.3 Early Data, allowing GET requests to be sent "
    "during the handshake when resuming a connection to a compatible TLS 1.3 "
    "server.";

const char kPostQuantumCECPQ2Name[] = "TLS Post-Quantum Confidentiality";
const char kPostQuantumCECPQ2Description[] =
    "This option enables a post-quantum (i.e. resistent to quantum computers) "
    "key exchange algorithm in TLS (CECPQ2).";

const char kMacCoreLocationImplementationName[] =
    "Core Location Implementation";
const char kMacCoreLocationImplementationDescription[] =
    "Enables usage of the Core Location APIs to get location permission on "
    "macOS";

const char kMacCoreLocationBackendName[] = "Core Location Backend";
const char kMacCoreLocationBackendDescription[] =
    "Enables usage of the Core Location APIs as the backend for Geolocation "
    "API";

const char kNewMacNotificationAPIName[] =
    "Determines which notification API to use on macOS devices";
const char kNewMacNotificationAPIDescription[] =
    "Enables the usage of Apple's new notification API which will run on macOS "
    "10.14+";

const char kMaintainShelfStateWhenEnteringOverviewName[] =
    "Allow Home Launcher Shelf In Overview";
const char kMaintainShelfStateWhenEnteringOverviewDescription[] =
    "Maintains shelf state when entering and exiting overview mode.";

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

const char kGuestOsExternalProtocolName[] = "Guest OS External Protocol";
const char kGuestOsExternalProtocolDescription[] =
    "Enable Guest OS external protocol handlers";

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

const char kEnablePreviewsCoinFlipName[] = "Enable Previews Coin Flip";
const char kEnablePreviewsCoinFlipDescription[] =
    "Enable coin flip experimentation of Previews.";

const char kEnableSRPIsolatedPrerendersName[] =
    "Enable Navigation Predictor Isolated Prerenders";
const char kEnableSRPIsolatedPrerendersDescription[] =
    "Enable Navigation Predictions on the Google SRP to be fully isolated.";

const char kEnableSRPIsolatedPrerenderProbingName[] =
    "Enable Probing on Navigation Predictor Isolated Prerenders";
const char kEnableSRPIsolatedPrerenderProbingDescription[] =
    "Enable probing checks for Isolated Prerenders which will block commit.";

const char kEnableSRPIsolatedPrerendersNSPName[] =
    "Enable NoStatePrefetch on Navigation Predictor Isolated Prerenders";
const char kEnableSRPIsolatedPrerendersNSPDescription[] =
    "Enables NoStatePrefetch on Isolated Prerenders.";

const char kDownloadAutoResumptionNativeName[] =
    "Enable download auto-resumption in native";
const char kDownloadAutoResumptionNativeDescription[] =
    "Enables download auto-resumption in native";

const char kDownloadLaterName[] = "Enable download later";
const char kDownloadLaterDescription[] = "Enables download later feature.";

const char kDownloadLaterDebugOnWifiName[] =
    "Show download later dialog on WIFI.";
const char kDownloadLaterDebugOnWifiNameDescription[] =
    "Show download later dialog on WIFI.";

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

const char kEnableMediaSessionServiceName[] = "Media Session Service";
const char kEnableMediaSessionServiceDescription[] =
    "Enables the media session mojo service and internal media session "
    "support.";

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

const char kEnablePortalsCrossOriginName[] = "Enable cross-origin Portals.";
const char kEnablePortalsCrossOriginDescription[] =
    "Allows portals to load cross-origin URLs in addition to same-origin ones."
    " Has no effect if Portals are not enabled.";

const char kEnableTranslateSubFramesName[] = "Translate sub frames";
const char kEnableTranslateSubFramesDescription[] =
    "Enable the translation of sub frames (as well as the main frame)";

const char kEnableWindowsGamingInputDataFetcherName[] =
    "Enable Windows.Gaming.Input";
const char kEnableWindowsGamingInputDataFetcherDescription[] =
    "Enable Windows.Gaming.Input by default to provide game controller "
    "support on Windows 10 desktop.";

const char kBlockInsecurePrivateNetworkRequestsName[] =
    "Block insecure private network requests.";
const char kBlockInsecurePrivateNetworkRequestsDescription[] =
    "Prevents non-secure contexts from making sub-resource requests to "
    "more-private IP addresses. An IP address IP1 is more private than IP2 if "
    "1) IP1 is localhost and IP2 is not, or 2) IP1 is private and IP2 is "
    "public. This is a first step towards full enforcement of CORS-RFC1918: "
    "https://wicg.github.io/cors-rfc1918";

const char kCorsForContentScriptsName[] = "CORS for content scripts";
const char kCorsForContentScriptsDescription[] =
    "Prevent content scripts of Chrome Extensions from bypassing CORS. "
    "For more information about the allowlist and the changes to the "
    "security model of content scripts, please see: "
    "https://www.chromium.org/Home/chromium-security/"
    "extension-content-script-fetches";

const char kForceEmptyCorbAndCorsAllowlistName[] =
    "Force empty CORB and CORS allowlist";
const char kForceEmptyCorbAndCorsAllowlistDescription[] =
    "Removes all Chrome Extensions from the CORB and CORS allowlist. "
    "For more information about the allowlist and the changes to the "
    "security model of content scripts, please see: "
    "https://www.chromium.org/Home/chromium-security/"
    "extension-content-script-fetches";

const char kCrossOriginOpenerPolicyReportingName[] =
    "Cross Origin Opener Policy reporting";
const char kCrossOriginOpenerPolicyReportingDescription[] =
    "Enables Cross Origin Opener Policy reporting.";

const char kCrossOriginOpenerPolicyAccessReportingName[] =
    "Cross Origin Opener Policy access reporting";
const char kCrossOriginOpenerPolicyAccessReportingDescription[] =
    "Enables Cross Origin Opener Policy access reporting.";

const char kCrossOriginIsolatedName[] = "crossOriginIsolated";
const char kCrossOriginIsolatedDescription[] =
    "Marks some BrowsingContext groups as \"crossOriginIsolated\". They can "
    "only host documents using a compatible set of {Origin,COOP,COEP}, "
    "effectively isolating.";

const char kDiagnosticsAppName[] = "Diagnostics app";
const char kDiagnosticsAppDescription[] =
    "Enables the Diagnostics app that allows Chrome OS users to be able to "
    "view their system telemetric information and run diagnostic tests for "
    "their device.";

const char kDisableKeepaliveFetchName[] = "Disable fetch with keepalive set";
const char kDisableKeepaliveFetchDescription[] =
    "Disable fetch with keepalive set "
    "(https://fetch.spec.whatwg.org/#request-keepalive-flag).";

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

#if BUILDFLAG(ENABLE_TAB_SEARCH)
const char kEnableTabSearchName[] = "Enable Tab Search";
const char kEnableTabSearchDescription[] =
    "Enable the Tab Search feature in Top Chrome UI, which will show a popup "
    "bubble that allows users to search over their currently open tabs.";
const char kEnableTabSearchFixedEntrypointName[] =
    "Enable Tab Search fixed "
    "entrypoint";
const char kEnableTabSearchFixedEntrypointDescription[] =
    "Enable the Tab "
    "Search feature in Top Chrome UI having a fixed button position.";
#endif  // BUILDFLAG(ENABLE_TAB_SEARCH)";

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

const char kEnableSubresourceRedirectName[] =
    "Enable Render Level Data Use Optimization";
const char kEnableSubresourceRedirectDescription[] =
    "Allow Render Level Data Use Optimization";

const char kEnableWebAuthenticationCableV2SupportName[] =
    "Web Authentication caBLE v2 support";
const char kEnableWebAuthenticationCableV2SupportDescription[] =
    "Enable the QR-based pairingless BLE protocol for use with the Web "
    "Authentication API. (This will also enable the cloud-based protocol "
    "on platforms where it is not already enabled.)";

const char kEnableWebAuthenticationChromeOSAuthenticatorName[] =
    "ChromeOS platform Web Authentication support";
const char kEnableWebAuthenticationChromeOSAuthenticatorDescription[] =
    "Enable the ChromeOS platform authenticator for the Web Authentication "
    "API.";

const char kExperimentalWebAssemblyFeaturesName[] = "Experimental WebAssembly";
const char kExperimentalWebAssemblyFeaturesDescription[] =
    "Enable web pages to use experimental WebAssembly features.";

const char kEnableWasmBaselineName[] = "WebAssembly baseline compiler";
const char kEnableWasmBaselineDescription[] =
    "Enables WebAssembly baseline compilation and tier up.";

const char kEnableWasmLazyCompilationName[] = "WebAssembly lazy compilation";
const char kEnableWasmLazyCompilationDescription[] =
    "Enables lazy (JIT on first call) compilation of WebAssembly modules.";

const char kEnableWasmSimdName[] = "WebAssembly SIMD support.";
const char kEnableWasmSimdDescription[] =
    "Enables support for the WebAssembly SIMD proposal.";

const char kEnableWasmThreadsName[] = "WebAssembly threads support";
const char kEnableWasmThreadsDescription[] =
    "Enables support for the WebAssembly Threads proposal.";

const char kEnableWasmTieringName[] = "WebAssembly tiering";
const char kEnableWasmTieringDescription[] =
    "Enables tiered compilation of WebAssembly (will tier up to TurboFan if "
    "#enable-webassembly-baseline is enabled).";

const char kEvDetailsInPageInfoName[] = "EV certificate details in Page Info.";
const char kEvDetailsInPageInfoDescription[] =
    "Shows the EV certificate details in the Page Info bubble.";

const char kExpensiveBackgroundTimerThrottlingName[] =
    "Throttle expensive background timers";
const char kExpensiveBackgroundTimerThrottlingDescription[] =
    "Enables intervention to limit CPU usage of background timers to 1%.";

const char kExperimentalExtensionApisName[] = "Experimental Extension APIs";
const char kExperimentalExtensionApisDescription[] =
    "Enables experimental extension APIs. Note that the extension gallery "
    "doesn't allow you to upload extensions that use experimental APIs.";

const char kExperimentalFlingAnimationName[] =
    "Enable experimental fling animation";
const char kExperimentalFlingAnimationDescription[] =
    "Enables the use of a touch fling curve that is based on the behavior of "
    "native apps on Windows.";

const char kExperimentalProductivityFeaturesName[] =
    "Experimental Productivity Features";
const char kExperimentalProductivityFeaturesDescription[] =
    "Enable support for experimental developer productivity features, such as "
    "import maps and policies for avoiding slow rendering.";

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

const char kExtensionsCheckupName[] = "Extensions Checkup";
const char kExtensionsCheckupDescription[] =
    "Enable the extensions checkup experiment";

const char kExtensionsOnChromeUrlsName[] = "Extensions on chrome:// URLs";
const char kExtensionsOnChromeUrlsDescription[] =
    "Enables running extensions on chrome:// URLs, where extensions explicitly "
    "request this permission.";

const char kFilteringScrollPredictionName[] = "Filtering scroll prediction";
const char kFilteringScrollPredictionDescription[] =
    "Enable filtering of predicted scroll events";

const char kFirstScrollLatencyMeasurementName[] =
    "First Scroll Latency Measurement";
const char kFirstScrollLatencyMeasurementDescription[] =
    "Enables measurement of the latency between user input events and visual "
    "updates for the first scroll of a navigation.";

const char kFractionalScrollOffsetsName[] = "Fractional Scroll Offsets";
const char kFractionalScrollOffsetsDescription[] =
    "Enables fractional scroll offsets inside Blink, exposing non-integer "
    "offsets to web APIs.";

const char kFreezeUserAgentName[] = "Freeze User-Agent request header";
const char kFreezeUserAgentDescription[] =
    "Set the User-Agent request header to a static string that conforms to the "
    "current User-Agent string format but only reveals desktop vs Android and "
    "if the 'mobile' flag is set";

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

const char kFormControlsDarkModeName[] = "Web Platform Controls Dark Mode";
const char kFormControlsDarkModeDescription[] =
    "If enabled, forms controls and scrollbars will be rendered with a dark "
    "theme, only on web pages that support dark color schemes, and when the "
    "OS is switched to dark theme.";

const char kFormControlsRefreshName[] = "Web Platform Controls updated UI";
const char kFormControlsRefreshDescription[] =
    "If enabled, HTML forms elements will be rendered using an updated style.";

const char kColorPickerEyeDropperName[] = "Color Picker Eye Dropper Support";
const char kColorPickerEyeDropperDescription[] =
    "If enabled, the color picker will contain an eye dropper control that "
    "can be used to pick colors.";

const char kGlobalMediaControlsName[] = "Global Media Controls";
const char kGlobalMediaControlsDescription[] =
    "Enables the Global Media Controls UI in the toolbar.";

const char kGlobalMediaControlsForCastName[] = "Global Media Controls for Cast";
const char kGlobalMediaControlsForCastDescription[] =
    "Shows Cast sessions in the Global Media Controls UI. Requires "
    "#global-media-controls and #cast-media-route-provider to also be enabled.";

const char kGlobalMediaControlsForChromeOSName[] =
    "Global Media Controls for ChromeOS";
const char kGlobalMediaControlsForChromeOSDescription[] =
    "Enable Global Media Controls UI in shelf and quick settings.";

const char kGlobalMediaControlsPictureInPictureName[] =
    "Global Media Controls Picture-in-Picture";
const char kGlobalMediaControlsPictureInPictureDescription[] =
    "Enables Picture-in-Picture controls in the Global Media Controls UI. "
    "Requires "
    "#global-media-controls to also be enabled.";

const char kGlobalMediaControlsSeamlessTransferName[] =
    "Global Media Controls Seamless Transfer";
const char kGlobalMediaControlsSeamlessTransferDescription[] =
    "Enables selection of audio output device to play media through in "
    "the Global Media Controls UI. Requires #global-media-controls to "
    "also be enabled.";

const char kGlobalMediaControlsModernUIName[] =
    "Global Media Controls Modern UI";

const char kGlobalMediaControlsModernUIDescription[] =
    "Use a redesigned version of the Global Media Controls UI. Requires "
    "#global-media-controls to also be enabled.";

const char kGlobalMediaControlsOverlayControlsName[] =
    "Enable overlay controls for Global Media Controls";
const char kGlobalMediaControlsOverlayControlsDescription[] =
    "Allowing controls to be dragged out from Global Media Controls dialog. "
    "Requires #global-media-controls to also be enabled.";

const char kGpuRasterizationName[] = "GPU rasterization";
const char kGpuRasterizationDescription[] = "Use GPU to rasterize web content.";

const char kH264DecoderBufferIsCompleteFrameName[] =
    "H.264 Decoder Buffer Is Complete Frame";
const char kH264DecoderBufferIsCompleteFrameDescription[] =
    "H.264 decoder will treat each DecoderBuffer as a complete frame. Defaults "
    "to enabled.";

const char kHandwritingGestureName[] = "Handwriting Gestures";
const char kHandwritingGestureDescription[] =
    "Enables handwriting gestures within the virtual keyboard. This will send "
    "handwriting you write inside the virtual keyboard to Google servers for "
    "recognition but will not be associated with your Google account.";

const char kHandwritingGestureEditingName[] = "Handwriting Gestures Editing";
const char kHandwritingGestureEditingDescription[] =
    "Enables editing with handwriting gestures within the virtual keyboard.";

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

const char kHideShelfControlsInTabletModeName[] =
    "Hide shelf control buttons in tablet mode.";

const char kHideShelfControlsInTabletModeDescription[] =
    "Hides home, back, and overview button from the shelf while the device is "
    "in tablet mode. Predicated on shelf-hotseat feature being enabled.";

const char kHorizontalTabSwitcherAndroidName[] =
    "Enable horizontal tab switcher";
const char kHorizontalTabSwitcherAndroidDescription[] =
    "Changes the layout of the Android tab switcher so tabs scroll "
    "horizontally instead of vertically.";

const char kTabSwitcherOnReturnName[] = "Tab switcher on return";
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

const char kIgnoreGpuBlocklistName[] = "Override software rendering list";
const char kIgnoreGpuBlocklistDescription[] =
    "Overrides the built-in software rendering list and enables "
    "GPU-acceleration on unsupported system configurations.";

const char kIgnorePreviewsBlocklistName[] = "Ignore Previews Blocklist";
const char kIgnorePreviewsBlocklistDescription[] =
    "Ignore decisions made by the PreviewsBlockList";

const char kImprovedCookieControlsName[] =
    "Enable improved cookie controls UI in incognito mode";
const char kImprovedCookieControlsDescription[] =
    "Improved UI in Incognito mode for third-party cookie blocking.";

const char kImprovedCookieControlsForThirdPartyCookieBlockingName[] =
    "Enable improved UI for third-party cookie blocking";
const char kImprovedCookieControlsForThirdPartyCookieBlockingDescription[] =
    "Enables an improved UI for existing third-party cookie blocking users.";

const char kImpulseScrollAnimationsName[] = "Impulse-style scroll animations";
const char kImpulseScrollAnimationsDescription[] =
    "Replaces the default scroll animation with Impulse-style scroll "
    "animations.";

const char kInProductHelpDemoModeChoiceName[] = "In-Product Help Demo Mode";
const char kInProductHelpDemoModeChoiceDescription[] =
    "Selects the In-Product Help demo mode.";

const char kInstalledAppsInCbdName[] = "Installed Apps in Clear Browsing Data";
const char kInstalledAppsInCbdDescription[] =
    "Adds the installed apps warning dialog to the clear browsing data flow "
    "which allows users to protect installed apps' data from being deleted.";

const char kJavascriptHarmonyName[] = "Experimental JavaScript";
const char kJavascriptHarmonyDescription[] =
    "Enable web pages to use experimental JavaScript features.";

const char kJavascriptHarmonyShippingName[] =
    "Latest stable JavaScript features";
const char kJavascriptHarmonyShippingDescription[] =
    "Some web pages use legacy or non-standard JavaScript extensions that may "
    "conflict with the latest JavaScript features. This flag allows disabling "
    "support of those features for compatibility with such pages.";

const char kKaleidoscopeModuleName[] = "Enforce Kaleidoscope NTP module";
const char kKaleidoscopeModuleDescription[] =
    "Enable the Kaleidoscope NTP module to display Kaleidoscope content on the "
    " New Tab Page.";

const char kLauncherSettingsSearchName[] = "Launcher settings search";
const char kLauncherSettingsSearchDescription[] =
    "Enable search results for OS settings in the launcher.";

const char kLegacyTLSEnforcedName[] =
    "Enforce deprecation of legacy TLS versions";
const char kLegacyTLSEnforcedDescription[] =
    "Enable connection errors and interstitials for sites that use legacy TLS "
    "versions (TLS 1.0 and TLS 1.1), which are deprecated and will be removed "
    " in the future.";

const char kLegacyTLSWarningsName[] =
    "Show security warnings for sites using legacy TLS versions";
const char kLegacyTLSWarningsDescription[] =
    "Show security warnings for sites that use legacy TLS versions (TLS 1.0 "
    "and TLS 1.1), which are deprecated and will be removed in the future.";

const char kLiteVideoName[] = "Enable LiteVideos";
const char kLiteVideoDescription[] =
    "Enable the LiteVideo optimization to throttle media requests to "
    "reduce data usage";

const char kLiteVideoDownlinkBandwidthKbpsName[] =
    "Lite Video: Adjust throttling downlink (in Kbps).";
const char kLiteVideoDownlinkBandwidthKbpsDescription[] =
    "Specify the throttling bandwidth to be used";

const char kLiteVideoForceOverrideDecisionName[] = "Force LiteVideos decision";
const char kLiteVideoForceOverrideDecisionDescription[] =
    "Force the LiteVideo decision to be allowed on every navigation.";

const char kLoadMediaRouterComponentExtensionName[] =
    "Load Media Router Component Extension";
const char kLoadMediaRouterComponentExtensionDescription[] =
    "Loads the Media Router component extension at startup.";

const char kLogJsConsoleMessagesName[] =
    "Log JS console messages in system logs";
const char kLogJsConsoleMessagesDescription[] =
    "Enable logging JS console messages in system logs, please note that they "
    "may contain PII.";

const char kMarkHttpAsName[] = "Mark non-secure origins as non-secure";
const char kMarkHttpAsDescription[] = "Change the UI treatment for HTTP pages";

const char kMediaHistoryName[] = "Enable Media History";
const char kMediaHistoryDescription[] =
    "Enables Media History which records data around media playbacks on "
    "websites.";

const char kMediaRouterCastAllowAllIPsName[] =
    "Connect to Cast devices on all IP addresses";
const char kMediaRouterCastAllowAllIPsDescription[] =
    "Have the Media Router connect to Cast devices on all IP addresses, not "
    "just RFC1918/RFC4193 private addresses.";

const char kMetricsSettingsAndroidName[] = "Metrics Settings on Android";
const char kMetricsSettingsAndroidDescription[] =
    "Enables the new design of metrics settings.";

const char kMixedFormsDisableAutofillName[] =
    "Disable autofill for mixed forms";
const char kMixedFormsDisableAutofillDescription[] =
    "If enabled, autofill is not allowed for mixed forms (forms on HTTPS sites "
    "that submit over HTTP), and a warning bubble will be shown instead. "
    "Autofill for passwords is not affected by this setting.";

const char kMixedFormsInterstitialName[] = "Mixed forms interstitial";
const char kMixedFormsInterstitialDescription[] =
    "When enabled, a full-page interstitial warning is shown when a mixed "
    "content form (a form on an HTTPS site that submits over HTTP) is "
    "submitted.";

const char kMobileIdentityConsistencyName[] = "Mobile identity consistency";
const char kMobileIdentityConsistencyDescription[] =
    "Enables stronger identity consistency on mobile";

const char kMouseSubframeNoImplicitCaptureName[] =
    "Disable mouse implicit capture for iframe";
const char kMouseSubframeNoImplicitCaptureDescription[] =
    "When enable, mouse down does not implicit capture for iframe.";

const char kNearbySharingName[] = "Nearby Sharing";
const char kNearbySharingDescription[] =
    "Enables Nearby Sharing for sharing content between devices.";

const char kNewCanvas2DAPIName[] = "Experimental canvas 2D API features";
const char kNewCanvas2DAPIDescription[] =
    "Enables in-progress features for the canvas 2D API. See "
    "https://github.com/fserb/canvas2d.";

const char kNewProfilePickerName[] = "New profile picker";
const char kNewProfilePickerDescription[] =
    "Enables new profile picker implementation.";

const char kProfilesUIRevampName[] = "Revamp of profiles UI";
const char kProfilesUIRevampDescription[] =
    "Enables several profile-related features including a new signin flow, "
    "profile promo bubbles and a color picker in settings";

const char kNewUsbBackendName[] = "Enable new USB backend";
const char kNewUsbBackendDescription[] =
    "Enables the new experimental USB backends for macOS and Windows";

const char kNewTabstripAnimationName[] = "New tabstrip animations";
const char kNewTabstripAnimationDescription[] =
    "New implementation of tabstrip animations.";

const char kNotificationIndicatorName[] = "Notification Indicators";
const char kNotificationIndicatorDescription[] =
    "Enable notification indicators, which appear on shelf app icons and "
    "launcher apps when a notification is active.";

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

extern const char kOmniboxBookmarkPathsName[] = "Omnibox Bookmark Paths";
extern const char kOmniboxBookmarkPathsDescription[] =
    "Allows inputs to match with bookmark paths. E.g. 'planets jupiter' can "
    "suggest a bookmark titled 'Jupiter' with URL "
    "'en.wikipedia.org/wiki/Jupiter' located in a path containing 'planet.'";

const char kOmniboxClobberTriggersContextualWebZeroSuggestName[] =
    "Omnibox Clobber Triggers Contextual Web ZeroSuggest";
const char kOmniboxClobberTriggersContextualWebZeroSuggestDescription[] =
    "If enabled, when the user clears the whole omnibox text (i.e. via "
    "Backspace), Chrome will request ZeroSuggest suggestions for the OTHER "
    "page classification (contextual web).";

const char kOmniboxCompactSuggestionsName[] = "Omnibox: Compact suggestions";
const char kOmniboxCompactSuggestionsDescription[] =
    "Conserve the space for Omnibox Suggestions by slightly reducing their "
    "size.";

const char kOmniboxDeferredKeyboardPopupName[] =
    "Omnibox: Defer keyboard popup";
const char kOmniboxDeferredKeyboardPopupDescription[] =
    "Defer keyboard popup when user selects the Omnibox to allow presenting "
    "more instant/zero-prefix suggestions. The soft keyboard will only be "
    "shown when user taps the Omnibox again, or if there's no suggestions to "
    "show to the user.";

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

const char kOmniboxHistoryQuickProviderAllowButDoNotScoreMidwordTermsName[] =
    "Omnibox History Quick Provider Allow but Do Not Score Midword Terms";
const char
    kOmniboxHistoryQuickProviderAllowButDoNotScoreMidwordTermsDescription[] =
        "Midword terms are allowed but not scored.";
const char kOmniboxHistoryQuickProviderAllowMidwordContinuationsName[] =
    "Omnibox History Quick Provider Allow Midword Continuations";
const char kOmniboxHistoryQuickProviderAllowMidwordContinuationsDescription[] =
    "Midword matches are allowed and scored when the match begins immediately "
    "after the previous match ends.";

const char kOmniboxLocalZeroSuggestFrecencyRankingName[] =
    "Frecency ranking for local history zero-prefix suggestions";
const char kOmniboxLocalZeroSuggestFrecencyRankingDescription[] =
    "Enable frecency ranking for local history zero-prefix suggestions.";

const char kOmniboxMostVisitedTilesName[] = "Omnibox Most Visited Tiles";
const char kOmniboxMostVisitedTilesDescription[] =
    "Display a list of frquently visited pages from history as a single row "
    "with a carousel instead of one URL per line.";

const char kOmniboxTrendingZeroPrefixSuggestionsOnNTPName[] =
    "Omnibox Trending Zero Prefix Suggestions";
const char kOmniboxTrendingZeroPrefixSuggestionsOnNTPDescription[] =
    "Enables trending zero prefix suggestions for signed-in users with no or "
    "insufficient search history.";

const char kOmniboxRichAutocompletionName[] = "Omnibox Rich Autocompletion";
const char kOmniboxRichAutocompletionDescription[] =
    "Allow autocompletion for titles and non-prefixes. I.e. suggestions whose "
    "titles or URLs contain the user input as a continuous chunk, but not "
    "necessarily a prefix, can be the default suggestion. Typically, only "
    "suggestions whose URLs are prefixed by the user input can be. The "
    "potential variations toggle 4 params: 1) 'Title UI' displays titles, 2) "
    "'2-Line UI' includes titles (and URLs when autocompleting titles) on a "
    "2nd line, 3) 'Title AC' autocompletes titles, and 4) 'Non-Prefix AC' "
    "autocompletes non-prefixes.";
const char kOmniboxRichAutocompletionMinCharName[] =
    "Omnibox Rich Autocompletion Min Characters";
const char kOmniboxRichAutocompletionMinCharDescription[] =
    "Specifies min input character length to trigger rich autocompletion.";
const char kOmniboxRichAutocompletionShowAdditionalTextName[] =
    "Omnibox Rich Autocompletion Show Additional Text";
const char kOmniboxRichAutocompletionShowAdditionalTextDescription[] =
    "Show the suggestion title or URL additional text when the input matches "
    "the URL or title respectively. Defaults to true.";
const char kOmniboxRichAutocompletionSplitName[] =
    "Omnibox Rich Autocompletion Split";
const char kOmniboxRichAutocompletionSplitDescription[] =
    "Allow splitting the user input to intermix with autocompletions; e.g., "
    "the user input 'x z' could be autocompleted as 'x [y ]z'.";
const char kOmniboxRichAutocompletionPromisingName[] =
    "Omnibox Rich Autocompletion Promising Combinations";
const char kOmniboxRichAutocompletionPromisingDescription[] =
    "Allow autocompletion for titles and non-prefixes; see Omnibox Rich "
    "Autocompletion.";

const char kOmniboxRichEntitiesInLauncherName[] =
    "Omnibox rich entities in the launcher";
const char kOmniboxRichEntitiesInLauncherDescription[] =
    "Enable rich entity formatting for Omnibox results in the launcher.";

const char kOmniboxOnFocusSuggestionsName[] = "Omnibox on-focus suggestions";
const char kOmniboxOnFocusSuggestionsDescription[] =
    "Configures Omnibox on-focus suggestions - suggestions displayed on-focus "
    "before the user has typed any input";

const char kOmniboxOnFocusSuggestionsContextualWebName[] =
    "Omnibox on-focus suggestions for the contextual Web";
const char kOmniboxOnFocusSuggestionsContextualWebDescription[] =
    "Enables on-focus suggestions on the Open Web, that are contextual to the "
    "current URL. Will only work if user is signed-in and syncing, or is "
    "otherwise eligible to send the current page URL to the suggest server.";

const char kOmniboxSearchEngineLogoName[] = "Omnibox search engine logo";
const char kOmniboxSearchEngineLogoDescription[] =
    "Display the current default search engine's logo in the omnibox";

const char kOmniboxSearchReadyIncognitoName[] =
    "Search ready omnibox in incognito";
const char kOmniboxSearchReadyIncognitoDescription[] =
    "Show search ready omnibox when browsing incognito.";

const char kOmniboxSpareRendererName[] =
    "Start spare renderer on omnibox focus";
const char kOmniboxSpareRendererDescription[] =
    "When the omnibox is focused, start an empty spare renderer. This can "
    "speed up the load of the navigation from the omnibox.";

const char kOmniboxSuggestionsRecyclerViewName[] =
    "Omnibox Suggestions using a recycler view";
const char kOmniboxSuggestionsRecyclerViewDescription[] =
    "Use an Android RecyclerView to render the Omnibox Suggestions drop down.";

const char kOmniboxSuggestionsWrapAroundName[] =
    "Omnibox Suggestions Wrap-Around";
const char kOmniboxSuggestionsWrapAroundDescription[] =
    "Allow long search suggestions to wrap around to reveal their content";

const char kOmniboxTabSwitchSuggestionsName[] =
    "Omnibox switch to tab suggestions";
const char kOmniboxTabSwitchSuggestionsDescription[] =
    "Enable URL suggestions to optionally take the user to a tab where a "
    "website is already opened.";

const char kOmniboxUIHideSteadyStateUrlSchemeName[] =
    "Omnibox UI Hide Steady-State URL Scheme";
const char kOmniboxUIHideSteadyStateUrlSchemeDescription[] =
    "In the omnibox, hide the scheme from steady state displayed URLs. It is "
    "restored during editing.";

const char kOmniboxUIHideSteadyStateUrlTrivialSubdomainsName[] =
    "Omnibox UI Hide Steady-State URL Trivial Subdomains";
const char kOmniboxUIHideSteadyStateUrlTrivialSubdomainsDescription[] =
    "In the omnibox, hide trivial subdomains from steady state displayed URLs. "
    "Hidden portions are restored during editing.";

const char kOmniboxUIRevealSteadyStateUrlPathQueryAndRefOnHoverName[] =
    "Omnibox UI Reveal Steady-State URL Path, Query, and Ref On Hover";
const char kOmniboxUIRevealSteadyStateUrlPathQueryAndRefOnHoverDescription[] =
    "In the omnibox, reveal the path, query and ref from steady state "
    "displayed URLs on hover.";

const char kOmniboxUIHideSteadyStateUrlPathQueryAndRefOnInteractionName[] =
    "Omnibox UI Hide Steady-State URL Path, Query, and Ref On Interaction";
const char
    kOmniboxUIHideSteadyStateUrlPathQueryAndRefOnInteractionDescription[] =
        "In the omnibox, hide the path, query and ref from steady state "
        "displayed URLs when the user interacts with the page.";

const char kOmniboxUIMaybeElideToRegistrableDomainName[] =
    "Omnibox UI Sometimes Hide Steady-State URL Subdomains Beyond Registrable "
    "Domain";
const char kOmniboxUIMaybeElideToRegistrableDomainDescription[] =
    "In the omnibox, occasionally hide subdomains as well as path, query and "
    "ref from steady state displayed URLs, depending on heuristics. Has no "
    "effect unless at least one of "
    "#omnibox-ui-reveal-steady-state-url-path-query-and-ref-on-hover or "
    "#omnibox-ui-hide-steady-state-url-path-query-and-ref-on-interaction is "
    "enabled.";

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

const char kOmniboxMaxURLMatchesName[] = "Omnibox Max URL Matches";
const char kOmniboxMaxURLMatchesDescription[] =
    "The maximum number of URL matches to show, unless there are no "
    "replacements.";

const char kOmniboxDynamicMaxAutocompleteName[] =
    "Omnibox Dynamic Max Autocomplete";
const char kOmniboxDynamicMaxAutocompleteDescription[] =
    "Configures the maximum number of autocomplete matches displayed in the "
    "Omnibox UI dynamically based on the number of URL matches.";

const char kOmniboxBubbleUrlSuggestionsName[] =
    "Omnibox Bubble URL Suggestions";
const char kOmniboxBubbleUrlSuggestionsDescription[] =
    "Configures the bubbling of URL suggestions after grouping searches "
    "above URLs.";

const char kOmniboxOnDeviceHeadSuggestionsIncognitoName[] =
    "Omnibox on device head suggestions (incognito only)";
const char kOmniboxOnDeviceHeadSuggestionsIncognitoDescription[] =
    "Google head non personalized search suggestions provided by a compact on "
    "device model for incognito";

const char kOmniboxOnDeviceHeadSuggestionsNonIncognitoName[] =
    "Omnibox on device head suggestions (non-incognito only)";
const char kOmniboxOnDeviceHeadSuggestionsNonIncognitoDescription[] =
    "Google head non personalized search suggestions provided by a compact on "
    "device model for non-incognito";

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

const char kOnTheFlyMhtmlHashComputationName[] =
    "On-The-Fly MHTML Hash Computation";
const char kOnTheFlyMhtmlHashComputationDescription[] =
    "Save MHTML files to the target location and calculate their content "
    "digests in one step.";

const char kOopRasterizationName[] = "Out of process rasterization";
const char kOopRasterizationDescription[] =
    "Perform Ganesh raster in the GPU Process instead of the renderer.  "
    "Must also enable GPU rasterization";

const char kOopRasterizationDDLName[] =
    "Out of process rasterization using DDLs";
const char kOopRasterizationDDLDescription[] =
    "Use Skia Deferred Display Lists when performing rasterization in the GPU "
    "process  "
    "Must also enable OOP rasterization";

const char kEnableDeJellyName[] = "Experimental de-jelly effect";
const char kEnableDeJellyDescription[] =
    "Enables an experimental effect which attempts to mitigate "
    "\"jelly-scrolling\". This is an experimental implementation with known "
    "bugs, visual artifacts, and performance cost. This implementation may be "
    "removed at any time.";

const char kOsSettingsDeepLinkingName[] = "CrOS Settings Deep Linking";
const char kOsSettingsDeepLinkingDescription[] =
    "Enables a unique URL for each path in CrOS settings. "
    "This allows deep linking to individual settings, i.e. in settings search.";

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

const char kOverrideLanguagePrefsForHrefTranslateName[] =
    "Override user-blocklisted languages for hrefTranslate";
const char kOverrideLanguagePrefsForHrefTranslateDescription[] =
    "When using hrefTranslate, ignore the user's blocklist of languages that "
    "shouldn't be translated.";
const char kOverrideSitePrefsForHrefTranslateName[] =
    "Override user-blocklisted sites for hrefTranslate";
const char kOverrideSitePrefsForHrefTranslateDescription[] =
    "When using hrefTranslate, ignore the user's blocklist of websites that "
    "shouldn't be translated.";

const char kOverscrollHistoryNavigationName[] = "Overscroll history navigation";
const char kOverscrollHistoryNavigationDescription[] =
    "History navigation in response to horizontal overscroll.";

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

const char kPasswordChangeInSettingsName[] =
    "Rework password change flow from settings";
const char kPasswordChangeInSettingsDescription[] =
    "Change password when bulk leak check detected an issue.";

const char kPasswordChangeName[] = "Rework password change flow";
const char kPasswordChangeDescription[] =
    "Change password when password leak is detected.";

const char kPasswordImportName[] = "Password import";
const char kPasswordImportDescription[] =
    "Import functionality in password settings.";

const char kPasswordScriptsFetchingName[] = "Fetch password scripts";
const char kPasswordScriptsFetchingDescription[] =
    "Fetches scripts for password change flows.";

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

const char kPermissionChipName[] = "Permissions Chip Experiment";
const char kPermissionChipDescription[] =
    "Enables an experimental permission prompt that uses a chip in the location"
    " bar.";

const char kPointerLockOptionsName[] = "Enables pointer lock options";
const char kPointerLockOptionsDescription[] =
    "Enables pointer lock unadjustedMovement. When unadjustedMovement is set "
    "to true, pointer movements wil not be affected by the underlying platform "
    "modications such as mouse accelaration.";

const char kPreviewsAllowedName[] = "Previews Allowed";
const char kPreviewsAllowedDescription[] =
    "Allows previews to be shown subject to specific preview types being "
    "enabled and the client experiencing specific triggering conditions. "
    "May be used as a kill-switch to turn off all potential preview types.";

const char kPrintJobManagementAppName[] = "Print Management App";
const char kPrintJobManagementAppDescription[] =
    "Enables the print management app that allows Chrome OS users to view "
    "and manage their native print jobs.";

const char kPrintSaveToDriveName[] = "Print Save to Drive locally";
const char kPrintSaveToDriveDescription[] =
    "Modifies Print Preview Save to Drive to use locally mounted Drive";

const char kPrivacyElevatedAndroidName[] =
    "Elevate Privacy in Settings on Android";
const char kPrivacyElevatedAndroidDescription[] =
    "Moves the Privacy section up to Basics and moves the Homepage one to "
    " Advanced.";

const char kPrivacyReorderedAndroidName[] =
    "Reorder settings in Privacy on Android";
const char kPrivacyReorderedAndroidDescription[] =
    "Reorder settings in the Privacy settings page for better user experience.";

const char kPrivacySettingsRedesignName[] = "Privacy Settings Redesign";
const char kPrivacySettingsRedesignDescription[] =
    "Redesign of the privacy settings card to make it more prominent and "
    "and easier to use.";

const char kSafetyCheckAndroidName[] = "Safety Check on Android";
const char kSafetyCheckAndroidDescription[] =
    "A settings component for checking multiple areas of browser safety. "
    "Requires #password-check to also be enabled.";

const char kProminentDarkModeActiveTabTitleName[] =
    "Prominent Dark Mode Active Tab Titles";
const char kProminentDarkModeActiveTabTitleDescription[] =
    "Makes the active tab title in dark mode bolder so the active tab is "
    "easier "
    "to identify.";

const char kPromoBrowserCommandsName[] = "NTP Promo Browser Commands";
const char kPromoBrowserCommandsDescription[] =
    "Enables executing the browser commands sent by the NTP promos";

const char kPullToRefreshName[] = "Pull-to-refresh gesture";
const char kPullToRefreshDescription[] =
    "Pull-to-refresh gesture in response to vertical overscroll.";
const char kPullToRefreshEnabledTouchscreen[] = "Enabled for touchscreen only";

const char kQuicName[] = "Experimental QUIC protocol";
const char kQuicDescription[] = "Enable experimental QUIC protocol support.";

const char kQuietNotificationPromptsName[] =
    "Quieter notification permission prompts";
const char kQuietNotificationPromptsDescription[] =
    "Enables quieter permission prompts for notification permission requests. "
    "When a site wishes to show notifications, the usual modal dialog is "
    "replaced with a quieter version.";

const char kAbusiveNotificationPermissionRevocationName[] =
    "Abusive notification permission revocation";
const char kAbusiveNotificationPermissionRevocationDescription[] =
    "Enables notification permission revocation for abusive origins. "
    "Prior to dispatching a push message to the service worker, the origin is "
    "verified through Safe Browsing. Origins with abusive notification "
    "permission requests or content will have the notification permission "
    "revoked.";

const char kContentSettingsRedesignName[] = "Content settings page redesign";
const char kContentSettingsRedesignDescription[] =
    "Enables a new content settings page UI.";

const char kRawClipboardName[] = "Raw Clipboard";
const char kRawClipboardDescription[] =
    "Allows raw / unsanitized clipboard content to be read and written. "
    "See https://github.com/WICG/raw-clipboard-access.";

const char kReadLaterName[] = "Read Later";
const char kReadLaterDescription[] =
    "Allow users to save tabs for later. Enables a new button and menu for "
    "accessing tabs saved for later.";

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

const char kRequestUnbufferedDispatchName[] = "Use RequestUnbufferedDispatch";
const char kRequestUnbufferedDispatchDescription[] =
    "Calls RequestUnbufferedDispatch in the Android API to disable buffering "
    "of input by the OS.";

const char kRestrictGamepadAccessName[] = "Restrict gamepad access";
const char kRestrictGamepadAccessDescription[] =
    "Enables Feature Policy and Secure Context restrictions on the Gamepad API";

const char kDelayAsyncScriptExecutionName[] = "Delay Async Script Execution";
const char kDelayAsyncScriptExecutionDescription[] =
    "The execution of async scripts will be delayed.";

const char kDelayCompetingLowPriorityRequestsName[] =
    "Delay Competing Low Priority Requests";
const char kDelayCompetingLowPriorityRequestsDescription[] =
    "Low priority requests will be delayed if they would otherwise compete "
    "with a high priority request.";

const char kIntensiveWakeUpThrottlingName[] =
    "Throttle Javascript timers in background.";
const char kIntensiveWakeUpThrottlingDescription[] =
    "When enabled, wake ups from DOM Timers are limited to 1 per minute in a "
    "page that has been hidden for 5 minutes. For additional details, see "
    "https://www.chromestatus.com/feature/4718288976216064.";

const char kPrinterStatusName[] = "Show printer Status";
const char kPrinterStatusDescription[] =
    "Enables printer status icons and labels for saved and nearby printers";

const char kPrinterStatusDialogName[] =
    "Show printer status on destination dialog";
const char kPrinterStatusDialogDescription[] =
    "Enables printer status icons and labels for saved printers on the Print "
    "Preview destination dialog";

const char kSafeBrowsingEnhancedProtectionMessageInInterstitialsName[] =
    "Show enhanced protection message in security interstitials.";

const char kSafeBrowsingEnhancedProtectionMessageInInterstitialsDescription[] =
    "If enabled, the Safe Browsing interstitials and the SSL interstitials "
    "will include an enhanced protection message when users are not in "
    "enhanced protection mode.";

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

const char kSchemefulSameSiteName[] = "Schemeful Same-Site";
const char kSchemefulSameSiteDescription[] =
    "Modify the same-site computation such that origins with the same "
    "registrable domain but different schemes are considered cross-site. This "
    "change only applies to cookies with the 'SameSite' attribute.";

const char kScrollableTabStripName[] = "Scrollable TabStrip";
const char kScrollableTabStripDescription[] =
    "Allows users to access tabs by scrolling when they no longer fit in the "
    "tabstrip.";

const char kScrollUnificationName[] = "Scroll Unification";
const char kScrollUnificationDescription[] =
    "Refactoring project that eliminates scroll handling code from Blink. "
    "Does not affect behavior or performance.";

const char kSecurePaymentConfirmationDebugName[] =
    "Secure Payment Confirmation Debug Mode";
const char kSecurePaymentConfirmationDebugDescription[] =
    "This flag removes the restriction that PaymentCredential in WebAuthn and "
    "secure payment confirmation in PaymentRequest API must use user verifying "
    "platform authenticators.";

const char kSendTabToSelfOmniboxSendingAnimationName[] =
    "Send tab to self omnibox sending animation";
const char kSendTabToSelfOmniboxSendingAnimationDescription[] =
    "If enabled, shows Sending... animation in omnibox instead of Desktop OS "
    "notifications for contextual menu entry points.";

const char kServiceWorkerOnUIName[] = "Service worker on UI thread";
const char kServiceWorkerOnUIDescription[] =
    "Enables browser process logic related to service workers to run on the UI "
    "thread rather than the IO thread.";

const char kSharedClipboardUIName[] =
    "Enable shared clipboard feature signals to be handled";
const char kSharedClipboardUIDescription[] =
    "Enables shared clipboard feature signals to be handled by showing "
    "a list of user's available devices to share the clipboard.";

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

const char kSharingPreferVapidName[] =
    "Prefer sending Sharing message via VAPID";
const char kSharingPreferVapidDescription[] =
    "Prefer sending Sharing message via FCM WebPush authenticated using VAPID.";

const char kSharingQRCodeGeneratorName[] = "Enable sharing page via QR Code";
const char kSharingQRCodeGeneratorDescription[] =
    "Enables right-click UI to share the page's URL via a generated QR Code.";

const char kSharingSendViaSyncName[] =
    "Enable sending Sharing message via Sync";
const char kSharingSendViaSyncDescription[] =
    "Enables sending Sharing message via commiting to Chrome Sync's "
    "SHARING_MESSAGE data type";

const char kSharingDeviceExpirationName[] =
    "Configures sharing device expiration";
const char kSharingDeviceExpirationDescription[] =
    "Configures how long after a device was last active that it is eligible "
    "for use in sharing features.";

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

const char kShowOverdrawFeedbackName[] = "Show overdraw feedback";
const char kShowOverdrawFeedbackDescription[] =
    "Visualize overdraw by color-coding elements based on if they have other "
    "elements drawn underneath.";

const char kSkiaRendererName[] = "Skia API for compositing";
const char kSkiaRendererDescription[] =
    "If enabled, the display compositor will use Skia as the graphics API "
    "instead of OpenGL ES.";

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

const char kIsolateOriginsName[] = "Isolate additional origins";
const char kIsolateOriginsDescription[] =
    "Requires dedicated processes for an additional set of origins, "
    "specified as a comma-separated list.";

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

const char kSmsReceiverCrossDeviceName[] = "SMS Receiver Cross Device";
const char kSmsReceiverCrossDeviceDescription[] =
    "Enable the SMS Receiver API to work across devices";

const char kSpeculativeServiceWorkerStartOnQueryInputName[] =
    "Enable speculative start of a service worker when a search is predicted.";
const char kSpeculativeServiceWorkerStartOnQueryInputDescription[] =
    "If enabled, when the user enters text in the omnibox that looks like a "
    "a query, any service worker associated with the search engine the query "
    "will be sent to is started early.";

const char kStrictOriginIsolationName[] = "Strict-Origin-Isolation";
const char kStrictOriginIsolationDescription[] =
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

const char kStoragePressureEventName[] = "Enable storage pressure Event";
const char kStoragePressureEventDescription[] =
    "If enabled, Chrome will dispatch a DOM event, informing applications "
    "about storage pressure (low disk space)";

const char kStoragePressureUIName[] = "Enable storage pressure UI";
const char kStoragePressureUIDescription[] =
    "If enabled, Chrome will trigger system notifications to warn about "
    "storage pressure.";

const char kSuggestedContentToggleName[] = "Enable Suggested Content toggle";
const char kSuggestedContentToggleDescription[] =
    "Enables a settings UI toggle that controls Suggested Content status. Also"
    "enables a corresponding notice in the Launcher about Suggested Content.";

const char kSuggestionsWithSubStringMatchName[] =
    "Substring matching for Autofill suggestions";
const char kSuggestionsWithSubStringMatchDescription[] =
    "Match Autofill suggestions based on substrings (token prefixes) rather "
    "than just prefixes.";

const char kSyncAutofillWalletOfferDataName[] =
    "Enable syncing autofill offer data";
const char kSyncAutofillWalletOfferDataDescription[] =
    "When enabled, allows syncing autofill wallet offer data type.";

const char kSyncSandboxName[] = "Use Chrome Sync sandbox";
const char kSyncSandboxDescription[] =
    "Connects to the testing server for Chrome Sync.";

const char kSystemKeyboardLockName[] = "Experimental system keyboard lock";
const char kSystemKeyboardLockDescription[] =
    "Enables websites to use the keyboard.lock() API to intercept system "
    "keyboard shortcuts and have the events routed directly to the website "
    "when in fullscreen mode.";

const char kSystemTrayMicGainName[] = "Modify mic gain in the system tray";
const char kSystemTrayMicGainDescription[] =
    "Enables mic gain settings in the system tray audio "
    "settings.";

const char kTabEngagementReportingName[] = "Tab Engagement Metrics";
const char kTabEngagementReportingDescription[] =
    "Tracks tab engagement and lifetime metrics.";

const char kTabGridLayoutAndroidName[] = "Tab Grid Layout";
const char kTabGridLayoutAndroidDescription[] =
    "Allows users to see their tabs in a grid layout in the tab switcher on "
    "phones.";

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

const char kTabGroupsName[] = "Tab Groups";
const char kTabGroupsDescription[] =
    "Allows users to organize tabs into visually distinct groups, e.g. to "
    "separate tabs associated with different tasks.";

const char kTabGroupsAutoCreateName[] = "Tab Groups Auto Create";
const char kTabGroupsAutoCreateDescription[] =
    "Automatically creates groups for users, if tab groups are enabled.";

const char kTabGroupsCollapseName[] = "Tab Groups Collapse";
const char kTabGroupsCollapseDescription[] =
    "Allows a tab group to be collapsible and expandable, if tab groups are "
    "enabled.";

const char kTabGroupsCollapseFreezingName[] = "Tab Groups Collapse Freezing";
const char kTabGroupsCollapseFreezingDescription[] =
    "Experimental tab freezing upon collapsing a tab group.";

const char kTabGroupsFeedbackName[] = "Tab Groups Feedback";
const char kTabGroupsFeedbackDescription[] =
    "Enables the feedback app to appear in the tab group editor bubble, if tab "
    "groups are enabled.";

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

const char kTouchpadOverscrollHistoryNavigationName[] =
    "Overscroll history navigation on Touchpad";
const char kTouchpadOverscrollHistoryNavigationDescription[] =
    "Allows swipe left/right from touchpad change browser navigation.";

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

const char kTrustTokensName[] = "Enable Trust Tokens";
const char kTrustTokensDescription[] =
    "Enables the prototype Trust Token API "
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

const char kUnsafeWebGPUName[] = "Unsafe WebGPU";
const char kUnsafeWebGPUDescription[] =
    "Enables access to the experimental WebGPU API. Warning: As GPU sandboxing "
    "isn't implemented yet for the WebGPU API, it is possible to read GPU data "
    "for other processes.";

const char kUnsafeFastJSCallsName[] = "Unsafe fast JS calls";
const char kUnsafeFastJSCallsDescription[] =
    "Enables experimental fast API between Blink and V8."
    "Warning: type checking, few POD types and array types "
    "are not supported yet, so crashes are possible.";

const char kUiPartialSwapName[] = "Partial swap";
const char kUiPartialSwapDescription[] = "Sets partial swap behavior.";

const char kUsernameFirstFlowName[] = "Username first flow";
const char kUsernameFirstFlowDescription[] =
    "Support of username saving and filling on username first flow i.e. login "
    "flows where a user has to type username first on one page and then "
    "password on another page";

const char kUsePreferredIntervalForVideoName[] =
    "Use preferred interval for video";
const char kUsePreferredIntervalForVideoDescription[] =
    "When enabled, the composition rate will be changed based on the videos' "
    "actual fps.";

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

const char kWebBluetoothNewPermissionsBackendName[] =
    "Use the new permissions backend for Web Bluetooth";
const char kWebBluetoothNewPermissionsBackendDescription[] =
    "Enables the new permissions backend for Web Bluetooth. This will enable "
    "persistent storage of device permissions.";

const char kWebBundlesName[] = "Web Bundles";
const char kWebBundlesDescription[] =
    "Enables experimental supports for Web Bundles (Bundled HTTP Exchanges) "
    "navigation.";

const char kWebOtpBackendName[] = "Web OTP";
const char kWebOtpBackendDescription[] =
    "Enables Web OTP API that uses the specified backend.";
const char kWebOtpBackendSmsVerification[] =
    "SMS Verification API (requires app-hash)";
const char kWebOtpBackendUserConsent[] = "User Consent API";

const char kWebglDraftExtensionsName[] = "WebGL Draft Extensions";
const char kWebglDraftExtensionsDescription[] =
    "Enabling this option allows web applications to access the WebGL "
    "Extensions that are still in draft status.";

const char kWebPaymentsExperimentalFeaturesName[] =
    "Experimental Web Payments API features";
const char kWebPaymentsExperimentalFeaturesDescription[] =
    "Enable experimental Web Payments API features";

const char kWebPaymentsMinimalUIName[] = "Web Payments Minimal UI";
const char kWebPaymentsMinimalUIDescription[] =
    "Allow Payment Request API to open a minimal UI to replace the Payment "
    "Request UI when appropriate.";

const char kAppStoreBillingDebugName[] =
    "Web Payments App Store Billing Debug Mode";
const char kAppStoreBillingDebugDescription[] =
    "App-store purchases (e.g., Google Play Store) within a TWA can be "
    "requested using the Payment Request API. This flag removes the "
    "restriction that the TWA has to be installed from the app-store.";

const char kWebrtcCaptureMultiChannelApmName[] =
    "WebRTC multi-channel capture audio processing.";
const char kWebrtcCaptureMultiChannelApmDescription[] =
    "Support in WebRTC for processing capture audio in multi channel without "
    "downmixing when running APM in the render process.";

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

const char kWebrtcUseMinMaxVEADimensionsName[] =
    "WebRTC Min/Max Video Encode Accelerator dimensions";
const char kWebrtcUseMinMaxVEADimensionsDescription[] =
    "When enabled, WebRTC will only use the Video Encode Accelerator for "
    "video resolutions inside those published as supported.";

const char kWebXrForceRuntimeName[] = "Force WebXr Runtime";
const char kWebXrForceRuntimeDescription[] =
    "Force the browser to use a particular runtime, even if it would not "
    "usually be enabled or would otherwise not be selected based on the "
    "attached hardware.";

const char kWebXrRuntimeChoiceNone[] = "No Runtime";
const char kWebXrRuntimeChoiceOpenXR[] = "OpenXR";
const char kWebXrRuntimeChoiceWindowsMixedReality[] = "Windows Mixed Reality";

const char kWebXrIncubationsName[] = "WebXR Incubations";
const char kWebXrIncubationsDescription[] =
    "Enables experimental features for WebXR.";

const char kWebXrMultiGpuName[] = "WebXR Multi GPU Support";
const char kWebXrMultiGpuDescription[] =
    "Improves WebXR support for computers with more than one GPU.";

const char kWellKnownChangePasswordName[] =
    "Support for .well-known/change-password";

const char kWellKnownChangePasswordDescription[] =
    "If enabled the 'change password' button in password checkup redirects to "
    "the .well-known/change-password path. The path is supposed to point to "
    "the password change form of the site. When the site doesn't support "
    ".well-known/change-password it is checked if a fallback url is available. "
    "Otherwise the user is redirected to the origin.";

const char kWindowNamingName[] = "Window Naming";
const char kWindowNamingDescription[] =
    "Whether the window naming UI is enabled.";

const char kZeroCopyName[] = "Zero-copy rasterizer";
const char kZeroCopyDescription[] =
    "Raster threads write directly to GPU memory associated with tiles.";

const char kEnableVulkanName[] = "Vulkan";
const char kEnableVulkanDescription[] = "Use vulkan as the graphics backend.";

// Android ---------------------------------------------------------------------

#if defined(OS_ANDROID)

const char kAImageReaderName[] = "Android ImageReader";
const char kAImageReaderDescription[] =
    " Enables MediaPlayer and MediaCodec to use AImageReader on Android. "
    " This feature is only available for android P+ devices. Disabling it also "
    " disables SurfaceControl.";

const char kAndroidAutofillAccessibilityName[] = "Autofill Accessibility";
const char kAndroidAutofillAccessibilityDescription[] =
    "Enable accessibility for autofill popup.";

const char kAndroidDefaultBrowserPromoName[] = "Default Browser Promo";
const char kAndroidDefaultBrowserPromoDescription[] =
    "Shows a promo dialog to set Chrome as the default browser";

const char kAndroidDetailedLanguageSettingsName[] =
    "Detailed Language Settings";
const char kAndroidDetailedLanguageSettingsDescription[] =
    "Enable the new detailed language settings page";

const char kAndroidManagedByMenuItemName[] = "Managed by menu item";
const char kAndroidManagedByMenuItemDescription[] =
    "whether policies have been applied to this browser at the profile or "
    "machine level.";

const char kAndroidMultipleDisplayName[] = "Multiple Display";
const char kAndroidMultipleDisplayDescription[] =
    "When enabled, tabs can be moved to the secondary display.";

const char kAndroidSurfaceControlName[] = "Android SurfaceControl";
const char kAndroidSurfaceControlDescription[] =
    " Enables SurfaceControl to manage the buffer queue for the "
    " DisplayCompositor on Android. This feature is only available on "
    " android Q+ devices";

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

const char kAutofillAssistantDirectActionsName[] =
    "Autofill Assistant direct actions";
const char kAutofillAssistantDirectActionsDescription[] =
    "When enabled, expose direct actions from the Autofill Assistant.";

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

const char kBentoOfflineName[] =
    "Enables an experiment for Offline Bento content on Android";
const char kBentoOfflineDescription[] =
    "Enables displaying Bento content on the offline page for Android.";

const char kCCTIncognitoName[] = "Chrome Custom Tabs Incognito mode";
const char kCCTIncognitoDescription[] =
    "Enables incognito mode for Chrome Custom Tabs, on Android.";

const char kCCTTargetTranslateLanguageName[] =
    "Chrome Custom Tabs Target Translate Language";
const char kCCTTargetTranslateLanguageDescription[] =
    "Enables specify target language the page should be translated to "
    "in Chrome Custom Tabs.";

const char kChromeShareHighlightsAndroidName[] =
    "Chrome Share text highlights on Android";
const char kChromeShareHighlightsAndroidDescription[] =
    "Enables UI to generate and share link to text highlights on Android";

const char kChromeShareQRCodeName[] = "Chrome Share QRCodes";
const char kChromeShareQRCodeDescription[] =
    "Enables UI to generate and scan QR Codes on Android";

const char kChromeShareScreenshotName[] = "Chrome Share Screenshots";
const char kChromeShareScreenshotDescription[] =
    "Enables UI to edit and share screenshots";

const char kChromeSharingHubName[] = "Chrome Sharing Hub";
const char kChromeSharingHubDescription[] =
    "Enables the Chrome Sharing Hub/custom share sheet.";

const char kChromeSharingHubV15Name[] = "Chrome Sharing Hub V1.5";
const char kChromeSharingHubV15Description[] =
    "Enables v1.5 of the Chrome Sharing Hub.";

const char kClearOldBrowsingDataName[] = "Clear older browsing data";
const char kClearOldBrowsingDataDescription[] =
    "Enables clearing of browsing data which is older than a given time "
    "period.";

const char kCloseTabSuggestionsName[] = "Suggest to close Tabs";
const char kCloseTabSuggestionsDescription[] =
    "Suggests to the user to close Tabs that haven't been used beyond a "
    "configurable threshold or where duplicates of Tabs exist. "
    "The threshold is configurable.";

const char kCriticalPersistedTabDataName[] = "Enable CriticalPersistedTabData";
const char kCriticalPersistedTabDataDescription[] =
    "A new method of persisting Tab data across restarts has been devised "
    "and implemented. This actives the new approach.";

const char kContextMenuPerformanceInfoAndRemoteHintFetchingName[] =
    "Context menu performance info and remote hint fetching";
const char kContextMenuPerformanceInfoAndRemoteHintFetchingDescription[] =
    "Enables showing link performance information in the context menu and "
    "allows communicating with Google servers to fetch performance information "
    "for the main frame URL.";

const char kContextualSearchDebugName[] = "Contextual Search debug";
const char kContextualSearchDebugDescription[] =
    "Enables internal debugging of Contextual Search behavior on the client "
    "and server.";

const char kContextualSearchDefinitionsName[] = "Contextual Search definitions";
const char kContextualSearchDefinitionsDescription[] =
    "Enables touch-activated contextual definitions of words on a page to be "
    "presented in the caption of the Touch to Search Bar.";

const char kContextualSearchLongpressResolveName[] =
    "Contextual Search long-press Resolves";
const char kContextualSearchLongpressResolveDescription[] =
    "Enables communicating with Google servers when a long-press gesture is "
    "recognized under some privacy-limited conditions, including having Touch "
    "to Search enabled in preferences. The page context data sent to Google is "
    "potentially privacy sensitive!  This disables the tap gesture from "
    "triggering Touch to Search unless that experiment arm is enabled.";

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

const char kContextualSearchSecondTapName[] =
    "Contextual Search second tap triggering";
const char kContextualSearchSecondTapDescription[] =
    "Enables triggering on a second tap gesture even when Ranker would "
    "normally suppress that tap.";

const char kContextualSearchTranslationsName[] =
    "Contextual Search translations";
const char kContextualSearchTranslationsDescription[] =
    "Enables automatic translations of words on a page to be presented in the "
    "caption of the bottom bar.";

const char kCpuAffinityRestrictToLittleCoresName[] = "Restrict to LITTLE cores";
const char kCpuAffinityRestrictToLittleCoresDescription[] =
    "Restricts Chrome threads to LITTLE cores on devices with big.LITTLE or "
    "similar CPU architectures.";

const char kDecoupleSyncFromAndroidAutoSyncName[] =
    "Enable Chrome Sync decoupling from Android auto-sync";
const char kDecoupleSyncFromAndroidAutoSyncDescription[] =
    "Causes Chrome to disappear from the list of auto-sync apps in Android "
    "settings. Sync will no longer be disabled when master sync is disabled, "
    "provided that Chrome is run at least once with master sync enabled.";

const char kDirectActionsName[] = "Direct actions";
const char kDirectActionsDescription[] =
    "Enables direct actions (Android Q and more).";

const char kDuetTabStripIntegrationAndroidName[] = "Duet-TabStrip Integration";
const char kDuetTabStripIntegrationAndroidDescription[] =
    "Allows users to access integration of Duet and TabStrip.";

const char kAutofillManualFallbackAndroidName[] =
    "Enable Autofill manual fallback for Addresses and Payments (Android)";
const char kAutofillManualFallbackAndroidDescription[] =
    "If enabled, adds toggle for addresses and payments bottom sheet to the "
    "keyboard accessory.";

const char kEnableAutofillRefreshStyleName[] =
    "Enable Autofill refresh style (Android)";
const char kEnableAutofillRefreshStyleDescription[] =
    "Enable modernized style for Autofill on Android";

const char kEnableAndroidSpellcheckerDescription[] =
    "Enables use of the Android spellchecker.";

const char kEnableCommandLineOnNonRootedName[] =
    "Enable command line on non-rooted devices";
const char kEnableCommandLineOnNoRootedDescription[] =
    "Enable reading command line file on non-rooted devices (DANGEROUS).";

const char kEnableOfflinePreviewsName[] = "Offline Page Previews";
const char kEnableOfflinePreviewsDescription[] =
    "Enable showing offline page previews on slow networks.";

const char kEnableUseAaudioDriverName[] = "Use AAudio Driver";
const char kEnableUseAaudioDriverDescription[] =
    "Enable the use of AAudio, if supported by the current Android version.";

const char kEphemeralTabUsingBottomSheetName[] =
    "An ephemeral Preview Tab using the bottom sheet";
const char kEphemeralTabUsingBottomSheetDescription[] =
    "Enable a 'Preview page/image' at a linked page into the bottom sheet. "
    "No other flags are needed for this feature.";

const char kExploreSitesName[] = "Explore websites";
const char kExploreSitesDescription[] =
    "Enables portal from new tab page to explore websites.";

const char kFillingPasswordsFromAnyOriginName[] =
    "Filling passwords from any origin";
const char kFillingPasswordsFromAnyOriginDescription[] =
    "Enabling this flag adds a button to the password fallback sheet. The "
    "button opens a different sheet that allows filling a password from any "
    "origin.";

const char kGamesHubName[] = "Games Hub";
const char kGamesHubDescription[] =
    "Enables viewing and usage of the Games Hub.";

const char kHomepageLocationName[] =
    "Enable enterprise policy HomepageLocation";
const char kHomepageLocationDescription[] =
    "Enable enterprice policy HomepageLocation, a rule of configure the home "
    "page URL";

const char kHomepagePromoCardName[] =
    "Enable homepage promo card on the New Tab Page";
const char kHomepagePromoCardDescription[] =
    "Enable homepage promo card that will be shown to users with partner "
    "configured homepage.";

const char kHomepageSettingsUIConversionName[] =
    "Enable new homepage setting UI";
const char kHomepageSettingsUIConversionDescription[] =
    "Enable new homepage setting UI, which allows user set up customized "
    "homepage URL through a radio button group, and add promo box on NTP "
    "if homepage is not enabled.";

const char kInstantStartName[] = "Instant start";
const char kInstantStartDescription[] =
    "Show start surface before native library is loaded.";

const char kIntentBlockExternalFormRedirectsNoGestureName[] =
    "Block intents from form submissions without user gesture";
const char kIntentBlockExternalFormRedirectsNoGestureDescription[] =
    "Require a user gesture that triggered a form submission in order to "
    "allow for redirecting to an external intent.";

const char kInterestFeedContentSuggestionsDescription[] =
    "Use the interest feed to render content suggestions. Currently "
    "content "
    "suggestions are shown on the New Tab Page.";
const char kInterestFeedContentSuggestionsName[] =
    "Interest Feed Content Suggestions";

const char kInterestFeedV2Name[] = "Interest Feed v2";
const char kInterestFeedV2Description[] =
    "Show content suggestions on the New Tab Page and Start Surface using the "
    "new Feed Component.";

const char kInterestFeedFeedbackDescription[] =
    "Allow the user to provide feedback from a feed card.";
const char kInterestFeedFeedbackName[] = "Interest Feed Feedback";

const char InterestFeedV1ClickAndViewActionsConditionalUploadName[] =
    "Interest Feed V1 clicks/views conditional upload";
const char InterestFeedV1ClickAndViewActionsConditionalUploadDescription[] =
    "Only enable the upload of clicks/views in Feed V1 after reaching "
    "conditions.";

const char InterestFeedV2ClickAndViewActionsConditionalUploadName[] =
    "Interest Feed V2 clicks/views conditional upload";
const char InterestFeedV2ClickAndViewActionsConditionalUploadDescription[] =
    "Only enable the upload of clicks/views in Feed V2 after reaching "
    "conditions.";

const char kMessagesForAndroidInfrastructureName[] = "Messages infrastructure";
const char kMessagesForAndroidInfrastructureDescription[] =
    "When enabled, will initialize Messages UI infrastructure";
const char kMessagesForAndroidPasswordsName[] = "Passwords Messages UI";
const char kMessagesForAndroidPasswordsDescription[] =
    "When enabled, password infobars will use the new Messages UI.";

const char kOfflineIndicatorAlwaysHttpProbeName[] = "Always http probe";
const char kOfflineIndicatorAlwaysHttpProbeDescription[] =
    "Always do http probe to detect network connectivity for offline indicator "
    "as opposed to just taking the connection state from the system."
    "Used for testing.";

const char kOfflineIndicatorChoiceName[] = "Offline indicator choices";
const char kOfflineIndicatorChoiceDescription[] =
    "Show an offline indicator while offline.";

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

const char kAndroidPartnerCustomizationPhenotypeName[] =
    "Use homepage and bookmarks from partner customization";
const char kAndroidPartnerCustomizationPhenotypeDescription[] =
    "This flag loads a new configuration source of the default homepage and "
    "bookmarks.";

const char kPageInfoPerformanceHintsName[] = "Page info performance hints";
const char kPageInfoPerformanceHintsDescription[] =
    "Show site performance information in the page info menu.";

const char kPageInfoV2Name[] = "Page info version two";
const char kPageInfoV2Description[] =
    "Enable the second version of the page info menu.";

const char kPasswordCheckName[] = "Bulk Password Check";
const char kPasswordCheckDescription[] =
    "Enables the Bulk Password Check feature for signed-in users.";

const char kPhotoPickerVideoSupportName[] = "Photo Picker Video Support";
const char kPhotoPickerVideoSupportDescription[] =
    "Enables video files to be shown in the Photo Picker dialog";

const char kProcessSharingWithDefaultSiteInstancesName[] =
    "Process sharing with default site instances";
const char kProcessSharingWithDefaultSiteInstancesDescription[] =
    "When site isolation is disabled, this mode changes how sites are lumped "
    "in to shared processes. For sites that do not require isolation, this "
    "feature groups them into a single 'default' site instance (per browsing "
    "instance) instead of creating unique site instances for each one. This "
    "enables resource savings by creating fewer processes for sites that do "
    "not need isolation.";

const char kProcessSharingWithStrictSiteInstancesName[] =
    "Process sharing with strict site instances";
const char kProcessSharingWithStrictSiteInstancesDescription[] =
    "When site isolation is disabled, this mode changes how sites are lumped "
    "in to a shared process. Process selection is usually controlled with "
    "site instances. With strict site isolation, each site on a page gets its "
    "own site instance and process. With site isolation disabled and without "
    "this mode, all sites that share a process are put into the same site "
    "instance. This mode adds a third way: site instances are strictly "
    "separated like strict site isolation, but process selection puts multiple "
    "site instances in a single process.";

const char kQueryTilesName[] = "Show query tiles";
const char kQueryTilesDescription[] = "Shows query tiles in Chrome";
const char kQueryTilesNTPName[] = "Show query tiles in NTP";
const char kQueryTilesNTPDescription[] = "Shows query tiles in NTP";
const char kQueryTilesOmniboxName[] = "Show query tiles in omnibox";
const char kQueryTilesOmniboxDescription[] = "Shows query tiles in omnibox";
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
const char kQueryTilesLocalOrderingName[] =
    "Query Tiles - Enable local ordering";
const char kQueryTilesLocalOrderingDescription[] =
    "Enables ordering query tiles locally based on user interactions.";
const char kQueryTilesInstantFetchName[] = "Query tile instant fetch";
const char kQueryTilesInstantFetchDescription[] =
    "Immediately schedule background task to fetch query tiles";

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
    "Enables displaying related searches in a UI surface for experimentation "
    "purposes.  This may have privacy implications so do not enable unless "
    "you have no privacy worries about sending page content and URLs to "
    "Google!";

const char kReportFeedUserActionsName[] =
    "Enable the report of click actions on cards in Chrome's Feed on Android.";
const char kReportFeedUserActionsDescription[] =
    "Enable the report of click actions on cards in Chrome's Feed on Android."
    " Also enable the feed header menu to manage the feed.";

const char kSafeBrowsingClientSideDetectionAndroidName[] =
    "Safe Browsing Client Side Detection on Android";
const char kSafeBrowsingClientSideDetectionAndroidDescription[] =
    "Enable DOM feature collection on Safe Browsing pings on Android";

const char kSafeBrowsingEnhancedProtectionAndroidName[] =
    "Safe Browsing Enhanced Protection on Android";
const char kSafeBrowsingEnhancedProtectionAndroidDescription[] =
    "A new Safe Browsing mode for faster, proactive protection against "
    "dangerous websites, downloads, and extensions. To enable enhanced "
    "protection from the settings page, please enable "
    "safe-browsing-security-section-ui-android and this flag together.";

const char kEnhancedProtectionPromoAndroidName[] =
    "Enable enhanced protection promo card on Android on the New Tab Page";
const char kEnhancedProtectionPromoAndroidDescription[] =
    "Enable enhanced protection promo card for users that have not signed up "
    "for enhanced protection.";

const char kSafeBrowsingSecuritySectionUiAndroidName[] =
    "Security Section on Android";
const char kSafeBrowsingSecuritySectionUiAndroidDescription[] =
    "Add a settings page for configuring security settings. The new security "
    "page is under Settings > Privacy and security";

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

const char kShareButtonInTopToolbarName[] = "Share Button In Top Toolbar";
const char kShareButtonInTopToolbarDescription[] =
    "Enables UI to initiate sharing from the top toolbar";

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

const char kPrefetchNotificationSchedulingIntegrationName[] =
    "Enable prefetch notification using notification scheduling system";
const char kPrefetchNotificationSchedulingIntegrationDescription[] =
    "if enable prefetch notification service and background task will hook up "
    "to notification scheduling system in native side";

const char kUpdateNotificationServiceImmediateShowOptionName[] =
    "Update notification service debug options";
const char kUpdateNotificationServiceImmediateShowOptionDescription[] =
    "Show update notification right immediately";

const char kUsageStatsDescription[] =
    "When set, enables sharing of per-domain usage stats with the Digital "
    "Wellbeing app on Android, and allows Digital Wellbeing to suspend access "
    "to websites in order to enforce user-defined time limits.";
const char kUsageStatsName[] = "Share Usage Stats with Digital Wellbeing";

const char kUseChimeAndroidSdkDescription[] =
    "Enable Chime SDK to receive push notification.";
const char kUseChimeAndroidSdkName[] = "Use Chime SDK";

const char kVideoTutorialsName[] = "Enable video tutorials";
const char kVideoTutorialsDescription[] = "Show video tutorials in Chrome";

const char kInlineUpdateFlowName[] = "Enable Google Play inline update flow";
const char kInlineUpdateFlowDescription[] =
    "When this flag is set, instead of taking the user to the Google Play "
    "Store when an update is available, the user is presented with an inline "
    "flow where they do not have to leave Chrome until the update is ready "
    "to install.";

const char kAndroidDarkSearchName[] = "Show darkened search pages on Android";
const char kAndroidDarkSearchDescription[] =
    "If enabled, users will see a darkened search page if Chrome is in "
    "nightmode as well.";

const char kAndroidNightModeTabReparentingName[] =
    "Android Chrome UI dark mode tab reparenting";
const char kAndroidNightModeTabReparentingDescription[] =
    "If enabled, when the user changes themes the current tabs are reparented "
    "instead of reloaded.";

const char kTabbedAppOverflowMenuIconsName[] =
    "Android tabbed app overflow menu icons";
const char kTabbedAppOverflowMenuIconsDescription[] =
    "If enabled, shows icon in front of each overflow menu item.";

const char kTabbedAppOverflowMenuRegroupName[] =
    "Android tabbed app overflow menu Regroup";
const char kTabbedAppOverflowMenuRegroupDescription[] =
    "If enabled, regroup overflow menu items.";

const char kTabbedAppOverflowMenuThreeButtonActionbarName[] =
    "Android tabbed app overflow menu three buttons actionbar";
const char kTabbedAppOverflowMenuThreeButtonActionbarDescription[] =
    "If enabled, the actionbar in the overflow menu will have 3 buttons.";

// Non-Android -----------------------------------------------------------------

#else  // !defined(OS_ANDROID)

extern const char kAllowAllSitesToInitiateMirroringName[] =
    "Allow all sites to initiate mirroring";
extern const char kAllowAllSitesToInitiateMirroringDescription[] =
    "When enabled, allows all websites to request to initiate tab mirroring "
    "via Presentation API. Requires #cast-media-route-provider to also be "
    "enabled";

const char kEnableAccessibilityLiveCaptionsName[] = "Live Captions";
const char kEnableAccessibilityLiveCaptionsDescription[] =
    "Enables the live captions feature which generates captions for "
    "media playing in Chrome. Turn the feature on in "
    "chrome://settings/accessibility.";

const char kCastMediaRouteProviderName[] = "Cast Media Route Provider";
const char kCastMediaRouteProviderDescription[] =
    "Enables the native Cast Media Route Provider implementation to be used "
    "instead of the implementation in the Media Router component extension.";

const char kCopyLinkToTextName[] = "Copy Link To Text";
const char kCopyLinkToTextDescription[] =
    "Adds an item to the context menu to allow a user to copy a link to the "
    "page with the selected text highlighted.";

const char kGlobalMediaControlsCastStartStopName[] =
    "Global media controls control Cast start/stop";
const char kGlobalMediaControlsCastStartStopDescription[] =
    "Allows global media controls to control when a Cast session is started "
    "or stopped instead of relying on the Cast dialog.";

const char kMuteNotificationsDuringScreenShareName[] =
    "Mute notifications during screen share";
const char kMuteNotificationsDuringScreenShareDescription[] =
    "Prevents new notifications from being shown while a screen is being "
    "shared.";

const char kNtpDismissPromosName[] = "Dismiss promos on the New Tab Page";
const char kNtpDismissPromosDescription[] =
    "Enables a UI to persistently dismiss [non-emergency] promos on the "
    "bottom/middle of the New Tab Page";

const char kNtpIframeOneGoogleBarName[] = "Load OneGoogleBar in an iframe";
const char kNtpIframeOneGoogleBarDescription[] =
    "Enables loading the OneGoogleBar in an iframe. Otherwise, the "
    "OneGoogleBar is loaded inline on chrome://new-tab-page.";

const char kNtpOneGoogleBarModalOverlaysName[] =
    "When OneGoogleBar is loaded in an iframe, overlays are modal";
const char kNtpOneGoogleBarModalOverlaysDescription[] =
    "Enables overlays being modal, when the OneGoogleBar is loaded as iframe."
    "Otherwise, a clip-path definition is used to clip away parts of the"
    "OneGoogleBar that do not have visible elements.";

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

const char kNtpRepeatableQueriesName[] =
    "Repeatable queries on the New Tab Page";
const char kNtpRepeatableQueriesDescription[] =
    "Enables surfacing repeatable queries as most visited tiles on the "
    "New Tab Page.";

const char kNtpWebUIName[] =
    "Use the WebUI new tab page when opening a new tab";
const char kNtpWebUIDescription[] =
    "The WebUI new tab page is a replacement for the local NTP.";

const char kNtpModulesName[] = "NTP Modules";
const char kNtpModulesDescription[] = "Shows modules on the New Tab Page.";

const char kNtpShoppingTasksModuleName[] = "NTP Shopping Tasks Module";
const char kNtpShoppingTasksModuleDescription[] =
    "Shows the shopping tasks module on the New Tab Page.";

const char kEnableReaderModeName[] = "Enable Reader Mode";
const char kEnableReaderModeDescription[] =
    "Allows viewing of simplified web pages by selecting 'Customize and "
    "control Chrome'>'Distill page'";

const char kHappinessTrackingSurveysForDesktopName[] =
    "Happiness Tracking Surveys";
const char kHappinessTrackingSurveysForDesktopDescription[] =
    "Enable showing Happiness Tracking Surveys to users on Desktop";

const char kHappinessTrackingSurveysForDesktopDemoName[] =
    "Happiness Tracking Surveys Demo";
const char kHappinessTrackingSurveysForDesktopDemoDescription[] =
    "Enable showing Happiness Tracking Surveys Demo to users on Desktop";

const char kHappinessTrackingSurveysForDesktopSettingsName[] =
    "Happiness Tracking Surveys for Settings";
const char kHappinessTrackingSurveysForDesktopSettingsDescription[] =
    "Enable showing Happiness Tracking Surveys for Settings to users on "
    "Desktop";

const char kHappinessTrackingSurveysForDesktopSettingsPrivacyName[] =
    "Happiness Tracking Surveys for Privacy Settings";
const char kHappinessTrackingSurveysForDesktopSettingsPrivacyDescription[] =
    "Enable showing Happiness Tracking Surveys for Privacy Settings to users "
    "on Desktop";

const char kHappinessTrackingSurveysForDesktopMigrationName[] =
    "Happiness Tracking Surveys migration";
const char kHappinessTrackingSurveysForDesktopMigrationDescription[] =
    "Enable the latest version of Happiness Tracking Surveys.";

const char kKernelnextVMsName[] = "Enable VMs on experimental kernels.";
const char kKernelnextVMsDescription[] =
    "Enables VM support on devices running experimental kernel versions.";

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

const char kOmniboxSuggestionButtonRowName[] = "Omnibox suggestion button row";
const char kOmniboxSuggestionButtonRowDescription[] =
    "Enable a button row on omnibox suggestions to present actionable items "
    "such as keyword search, tab-switch buttons, and Pedals.";

const char kOmniboxPedalSuggestionsName[] = "Omnibox Pedal suggestions";
const char kOmniboxPedalSuggestionsDescription[] =
    "Enable omnibox Pedal suggestions to accelerate actions within Chrome by "
    "detecting user intent and offering direct access to the end goal. This "
    "flag has no effect unless \"Omnibox suggestion button row\" is also "
    "enabled.";

const char kOmniboxKeywordSearchButtonName[] = "Omnibox keyword search button";
const char kOmniboxKeywordSearchButtonDescription[] =
    "Enable the omnibox keyword search button which offers a way to search "
    "on specific sites from the omnibox. This flag has no effect unless "
    "\"Omnibox suggestion button row\" is also enabled.";

const char kOmniboxRefinedFocusStateName[] = "Omnibox refined focus state UI";
const char kOmniboxRefinedFocusStateDescription[] =
    "Enables new changes to the UI indicating focus and hover states.";

const char kOmniboxShortBookmarkSuggestionsName[] =
    "Omnibox short bookmark suggestions";
const char kOmniboxShortBookmarkSuggestionsDescription[] =
    "Match very short input words to beginning of words in bookmark "
    "suggestions.";

const char kPasswordsWeaknessCheckName[] = "Passwords weakness check";
const char kPasswordsWeaknessCheckDescription[] =
    "Enables checking passwords for weakness in Password Check";

const char kSCTAuditingName[] = "SCT auditing";
const char kSCTAuditingDescription[] =
    "Enables SCT auditing for users who have opted in to Safe Browsing "
    "Extended Reporting.";

const char kTabFreezeName[] = "Tab Freeze";
const char kTabFreezeDescription[] =
    "Enables freezing eligible tabs when they have been backgrounded for 5 "
    "minutes.";

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

const char kElasticOverscrollWinName[] = "Elastic Overscroll for Windows";
const char kElasticOverscrollWinDescription[] =
    "Enables Elastic Overscrolling for Windows on touchscreens and precision "
    "touchpads.";

extern const char kEnableIncognitoShortcutOnDesktopName[] =
    "Enable Incognito Desktop Shortcut";
extern const char kEnableIncognitoShortcutOnDesktopDescription[] =
    "Enables users to create a desktop shortcut for incognito mode.";

const char kEnableMediaFoundationVideoCaptureName[] =
    "MediaFoundation Video Capture";
const char kEnableMediaFoundationVideoCaptureDescription[] =
    "Enable/Disable the usage of MediaFoundation for video capture. Fall back "
    "to DirectShow if disabled.";

const char kRunVideoCaptureServiceInBrowserProcessName[] =
    "Run video capture service in browser";
const char kRunVideoCaptureServiceInBrowserProcessDescription[] =
    "Run the video capture service in the browser process.";

const char kSafetyCheckChromeCleanerChildName[] =
    "Chrome Cleanup Tool in safety check";
const char kSafetyCheckChromeCleanerChildDescription[] =
    "Enables the Chrome Cleanup Tool child in safety check.";

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

const char kUseSerialBusEnumeratorName[] = "Use system serial port enumerator";
const char kUseSerialBusEnumeratorDescription[] =
    "Use the system-supplied enumerator for serial port devices instead of "
    "enumerating devices implementing the COM port interface.";

const char kUseWinrtMidiApiName[] = "Use Windows Runtime MIDI API";
const char kUseWinrtMidiApiDescription[] =
    "Use Windows Runtime MIDI API for WebMIDI (effective only on Windows 10 or "
    "later).";

#if BUILDFLAG(ENABLE_PRINTING)
const char kGdiTextPrinting[] = "GDI Text Printing";
const char kGdiTextPrintingDescription[] =
    "Use GDI to print text as simply text";

const char kPrintWithReducedRasterizationName[] =
    "Print with reduced rasterization";
const char kPrintWithReducedRasterizationDescription[] =
    "When using GDI printing, avoid rasterization if possible.";

const char kUseXpsForPrintingName[] = "Use XPS for printing";
const char kUseXpsForPrintingDescription[] =
    "When enabled, use XPS printing API instead of the GDI print API.";

const char kUseXpsForPrintingFromPdfName[] = "Use XPS for printing from PDF";
const char kUseXpsForPrintingFromPdfDescription[] =
    "When enabled, use XPS printing API instead of the GDI print API when "
    "printing PDF documents.";
#endif  // BUILDFLAG(ENABLE_PRINTING)

#if BUILDFLAG(ENABLE_SPELLCHECK)
const char kWinUseBrowserSpellCheckerName[] =
    "Use the Windows OS spell checker";
const char kWinUseBrowserSpellCheckerDescription[] =
    "For supported languages, use the Windows OS spell checker to find "
    "spelling mistakes and provide spelling suggestions. Additional languages "
    "can be installed in the Windows OS settings to improve Windows spell "
    "check support.";
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

#endif  // defined(OS_WIN)

// Mac -------------------------------------------------------------------------

#if defined(OS_MAC)

#if BUILDFLAG(ENABLE_PRINTING)
const char kCupsIppPrintingBackendName[] = "CUPS IPP Printing Backend";
const char kCupsIppPrintingBackendDescription[] =
    "Use the CUPS IPP printing backend instead of the original CUPS backend "
    "that calls the PPD API.";
#endif  // BUILDFLAG(ENABLE_PRINTING)

const char kImmersiveFullscreenName[] = "Immersive Fullscreen Toolbar";
const char kImmersiveFullscreenDescription[] =
    "Automatically hide and show the toolbar in fullscreen.";

const char kMacSyscallSandboxName[] = "Mac Syscall Filtering Sandbox";
const char kMacSyscallSandboxDescription[] =
    "Controls whether the macOS sandbox filters syscalls.";

const char kMacV2GPUSandboxName[] = "Mac V2 GPU Sandbox";
const char kMacV2GPUSandboxDescription[] =
    "Controls whether the GPU process on macOS uses the V1 or V2 sandbox.";

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

const char kVideoToolboxVp9DecodingName[] = "VideoToolbox VP9 Decoding";
const char kVideoToolboxVp9DecodingDescription[] =
    "Controls usage of VideoToolbox for hardware decoding of VP9 video.";

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

const char kAllowDisableMouseAccelerationName[] =
    "Allow disabling mouse acceleration";
const char kAllowDisableMouseAccelerationDescription[] =
    "Shows a setting to disable mouse acceleration.";

const char kAllowScrollSettingsName[] =
    "Allow changes to scroll acceleration/sensitivity for mice/touchpads.";
const char kAllowScrollSettingsDescription[] =
    "Shows settings to enable/disable scroll acceleration and to adjust the "
    "sensitivity for scrolling.";

const char kAppServiceExternalProtocolName[] = "App Service External Protocol";
const char kAppServiceExternalProtocolDescription[] =
    "Use the App Service to provide data for external protocol dialog.";

const char kAppServiceIntentHandlingName[] = "App Service Intent Handling";
const char kAppServiceIntentHandlingDescription[] =
    "Use the App Service to provide data for intent handling.";

const char kAppServiceAdaptiveIconName[] = "App Service Adaptive Icons";
const char kAppServiceAdaptiveIconDescription[] =
    "Provide adaptive icons through the App Service";

const char kArcApplicationZoomName[] = "Allow zooming of Android apps";
const char kArcApplicationZoomDescription[] =
    "Allow Android apps to be zoomed in/out using ctrl+/-.";

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

const char kArcNativeBridge64BitSupportExperimentName[] =
    "Enable experimental 64-bit native bridge support for ARC";
const char kArcNativeBridge64BitSupportExperimentDescription[] =
    "Enable experimental 64-bit native bridge support for ARC where available.";

const char kArcUsbHostName[] = "Enable ARC USB host integration";
const char kArcUsbHostDescription[] =
    "Allow Android apps to use USB host feature on ChromeOS devices.";

const char kArcUsbStorageUIName[] = "Enable ARC USB Storage UI";
const char kArcUsbStorageUIDescription[] =
    "Enable experimental UI for controlling ARC access to USB storage devices.";

const char kAshEnablePipRoundedCornersName[] =
    "Enable Picture-in-Picture rounded corners.";
const char kAshEnablePipRoundedCornersDescription[] =
    "Enable rounded corners on the Picture-in-Picture window.";

const char kAshEnableUnifiedDesktopName[] = "Unified desktop mode";
const char kAshEnableUnifiedDesktopDescription[] =
    "Enable unified desktop mode which allows a window to span multiple "
    "displays.";

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

const char kBluetoothFixA2dpPacketSizeName[] = "Bluetooth fix A2DP packet size";
const char kBluetoothFixA2dpPacketSizeDescription[] =
    "Fixes Bluetooth A2DP packet size to a smaller default value to improve "
    "audio quality and may fix audio stutter.";

const char kBluetoothNextHandsfreeProfileName[] =
    "Enable next version of Bluetooth Handsfree profile";
const char kBluetoothNextHandsfreeProfileDescription[] =
    "Enable next version of Bluetooth Handsfree profile 1.7 which includes "
    "WBS(wideband speech) feature";

const char kCameraSystemWebAppName[] = "Camera System Web App";
const char kCameraSystemWebAppDescription[] =
    "Run the Chrome Camera App as a System Web App.";

#if defined(OS_CHROMEOS) && BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
const char kChromeOSDirectVideoDecoderName[] = "ChromeOS Direct Video Decoder";
const char kChromeOSDirectVideoDecoderDescription[] =
    "Enables the hardware-accelerated ChromeOS direct media::VideoDecoder "
    "implementation. Note that this might be entirely disallowed by the "
    "--platform-disallows-chromeos-direct-video-decoder command line switch "
    "which is added for platforms where said direct VideoDecoder does not work "
    "or is not well tested (see the disable_cros_video_decoder USE flag in "
    "Chrome OS)";
#endif  // defined(OS_CHROMEOS) && BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

const char kCdmFactoryDaemonName[] = "CDM Factory Daemon";
const char kCdmFactoryDaemonDescription[] =
    "Use the CDM daemon instead of the library CDM";

const char kContextualNudgesName[] =
    "Contextual nudges for user gesture education";
const char kContextualNudgesDescription[] =
    "Enables contextual nudges, periodically showing the user a label "
    "explaining how to interact with a particular UI element using gestures.";

const char kCrosLanguageSettingsUpdateName[] = "Language Settings Update";
const char kCrosLanguageSettingsUpdateDescription[] =
    "Enable this flag to see the new language settings update.";

const char kCrosRegionsModeName[] = "Cros-regions load mode";
const char kCrosRegionsModeDescription[] =
    "This flag controls cros-regions load mode";
const char kCrosRegionsModeDefault[] = "Default";
const char kCrosRegionsModeOverride[] = "Override VPD values.";
const char kCrosRegionsModeHide[] = "Hide VPD values.";

const char kCrostiniDiskResizingName[] = "Allow resizing Crostini disks";
const char kCrostiniDiskResizingDescription[] =
    "Use preallocated user-resizeable disks for Crostini instead of sparse "
    "automatically sized disks.";

const char kCrostiniUseBusterImageName[] = "New Crostini containers use Buster";
const char kCrostiniUseBusterImageDescription[] =
    "New Crostini containers use Debian Buster images instead of Debian "
    "Stretch.";

const char kCrostiniGpuSupportName[] = "Crostini GPU Support";
const char kCrostiniGpuSupportDescription[] = "Enable Crostini GPU support.";

const char kCrostiniWebUIUpgraderName[] = "Crostini WebUI Upgrader";
const char kCrostiniWebUIUpgraderDescription[] =
    "Enable the new WebUI Crostini Upgrader Dialog.";

const char kCrostiniUseDlcName[] = "Crostini Use DLC";
const char kCrostiniUseDlcDescription[] =
    "Download the termina VM using the new DLC service instead of the old "
    "component updater.";

const char kCrostiniEnableDlcName[] = "Crostini Enable DLC";
const char kCrostiniEnableDlcDescription[] =
    "Signal to Crostini that the DLC service is available for use.";

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

const char kDisableCameraFrameRotationAtSourceName[] =
    "Disable camera frame rotation at source";
const char kDisableCameraFrameRotationAtSourceDescription[] =
    "Disable camera frame rotation to the upright display orientation in the "
    "video capture device";

const char kDisableCancelAllTouchesName[] = "Disable CancelAllTouches()";
const char kDisableCancelAllTouchesDescription[] =
    "If enabled, a canceled touch will not force all other touches to be "
    "canceled.";

const char kDisableCryptAuthV1DeviceSyncName[] =
    "Disable CryptAuth v1 DeviceSync";
const char kDisableCryptAuthV1DeviceSyncDescription[] =
    "Disable the CryptAuth v1 DeviceSync protocol. The v2 DeviceSync flag "
    "should be enabled before this flag is flipped.";

const char kDisableIdleSocketsCloseOnMemoryPressureName[] =
    "Disable closing idle sockets on memory pressure";
const char kDisableIdleSocketsCloseOnMemoryPressureDescription[] =
    "If enabled, idle sockets will not be closed when chrome detects memory "
    "pressure. This applies to web pages only and not to internal requests.";

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

const char kDisplayIdentificationName[] =
    "Enable display identification highlight";
const char kDisplayIdentificationDescription[] =
    "Shows a blue highlight around the edges of the display that is selected "
    "in the Displays Settings page. Only shown when the Displays Settings page "
    "is open.";

const char kUseHDRTransferFunctionName[] =
    "Monitor/Display HDR transfer function";
const char kUseHDRTransferFunctionDescription[] =
    "Allows using the HDR transfer functions of any connected monitor that "
    "supports it";

const char kDisableOfficeEditingComponentAppName[] =
    "Disable Office Editing for Docs, Sheets & Slides";
const char kDisableOfficeEditingComponentAppDescription[] =
    "Disables Office Editing for Docs, Sheets & Slides component app so "
    "handlers won't be registered, making it possible to install another "
    "version for testing.";

const char kDoubleTapToZoomInTabletModeName[] =
    "Double-tap to zoom in tablet mode";
const char kDoubleTapToZoomInTabletModeDescription[] =
    "If Enabled, double tapping in webpages while in tablet mode will zoom the "
    "page.";

const char kDriveFsBidirectionalNativeMessagingName[] =
    "Enable bidirectional native messaging for DriveFS";
const char kDriveFsBidirectionalNativeMessagingDescription[] =
    "Enable enhanced native messaging host to communicate with DriveFS.";

const char kMovablePartialScreenshotName[] =
    "Movable/resizable partial screenshot region";
const char kMovablePartialScreenshotDescription[] =
    "Allow partial screenshot region to be moved/resized via mouse/touch.";

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

const char kEnableAmbientModeName[] = "Enable Ambient Mode";
const char kEnableAmbientModeDescription[] =
    "Enable Ambient Mode to show an enhanced lock screen with more functions";

const char kEnableAssistantAppSupportName[] = "Enable Assistant App Support";
const char kEnableAssistantAppSupportDescription[] =
    "Enable the Assistant App Support feature";

const char kEnableAssistantBetterOnboardingName[] =
    "Enable Assistant Better Onboarding";
const char kEnableAssistantBetterOnboardingDescription[] =
    "Enables the Assistant better onboarding experience.";

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

const char kEnableAssistantTimersV2Name[] = "Enable Assistant Timers V2";
const char kEnableAssistantTimersV2Description[] =
    "Enables v2 of Assistant timers.";

const char kEnableAutoSelectName[] = "Auto Select";
const char kEnableAutoSelectDescription[] =
    "Automatically select the word under cursor on contextual menu click.";

const char kEnableBackgroundBlurName[] = "Enable background blur.";
const char kEnableBackgroundBlurDescription[] =
    "Enables background blur for the Launcher, Shelf, Unified System Tray etc.";

const char kDragToSnapInClamshellModeName[] = "Enable clamshell split view.";
const char kDragToSnapInClamshellModeDescription[] =
    "You can drag a window from clamshell overview and snap it. If you will be "
    "using multiple displays, you should use this flag only in conjunction "
    "with multi-display-overview-and-split-view (otherwise you are likely to "
    "get a crash).";

const char kEnhancedClipboardName[] =
    "Productivity Experiment: Enable Enhanced Clipboard";
const char kEnhancedClipboardDescription[] =
    "Enables an experimental clipboard history which aims to reduce context "
    "switching. After copying to the clipboard, press search + v to show the "
    "history. Selecting something from the menu will result in a paste to the "
    "active window.";

const char kEnhancedClipboardSimpleRenderName[] =
    "Only renders html in the Enhanced Clipboard if there are img or table "
    "tags";
const char kEnhancedClipboardSimpleRenderDescription[] =
    "This will result in less HTML rendering in the Enhanced Clipboard menu.";

const char kMultiDisplayOverviewAndSplitViewName[] =
    "Enable multi-display overview and split view.";
const char kMultiDisplayOverviewAndSplitViewDescription[] =
    "You can drag a window from one overview grid and drop into another "
    "overview grid.";

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

const char kNewDragSpecInLauncherName[] = "Enable Launcher App Paging";
const char kNewDragSpecInLauncherDescription[] =
    "Show visual affordance of launcher app pages and enable page previews "
    "when dragging apps.";

const char kEnableNeuralStylusPalmRejectionName[] =
    "Enable Neural Palm Detection";
const char kEnableNeuralStylusPalmRejectionDescription[] =
    "Experimental: Enable Neural Palm detection. Not compatible with all "
    "devices.";

const char kEnableNewShortcutMappingName[] = "Enable New Shortcut Mapping";
const char kEnableNewShortcutMappingDescription[] =
    "Enables experimental new shortcut mapping";

const char kEnableOnDeviceAssistantName[] = "Enable On Device Assistant";
const char kEnableOnDeviceAssistantDescription[] =
    "Enable On Device Assistant.";

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

const char kEnableParentalControlsSettingsName[] =
    "Enable Parental controls settings";
const char kEnableParentalControlsSettingsDescription[] =
    "Enable Parental Control options in settings.";

const char kEnablePlayStoreSearchName[] = "Enable Play Store search";
const char kEnablePlayStoreSearchDescription[] =
    "Enable Play Store search in launcher.";

const char kEnableQuickAnswersName[] = "Enable Quick Answers";
const char kEnableQuickAnswersDescription[] =
    "Enable the Quick Answers feature";

const char kEnableQuickAnswersRichUiName[] = "Enable Quick Answers Rich UI";
const char kEnableQuickAnswersRichUiDescription[] =
    "Enable the Quick Answers rich UI.";

const char kEnableQuickAnswersTextAnnotatorName[] =
    "Enable Quick Answers text annotator";
const char kEnableQuickAnswersTextAnnotatorDescription[] =
    "Enable Quick Answers text annotator.";

const char kEnableQuickAnswersTranslationName[] =
    "Enable Quick Answers translation";
const char kEnableQuickAnswersTranslationDescription[] =
    "Enable Quick Answers translation.";

const char kEnableQuickAnswersTranslationCloudAPIName[] =
    "Enable Quick Answers translation using the Cloud API";
const char kEnableQuickAnswersTranslationCloudAPIDescription[] =
    "Enable Quick Answers translation using the Cloud API.";

const char kTrimOnFreezeName[] = "Trim Working Set on freeze";
const char kTrimOnFreezeDescription[] = "Trim Working Set on all frames frozen";

const char kTrimOnMemoryPressureName[] = "Trim Working Set on memory pressure";
const char kTrimOnMemoryPressureDescription[] =
    "Trim Working Set periodically on memory pressure";

const char kEnableZeroStateSuggestionsName[] = "Enable Zero State Suggetions";
const char kEnableZeroStateSuggestionsDescription[] =
    "Enable Zero State Suggestions feature in Launcher, which will show "
    "suggestions when launcher search box is active with an empty query";

const char kEnableSuggestedFilesName[] = "Enable Suggested Files";
const char kEnableSuggestedFilesDescription[] =
    "Enable Suggested Files feature in Launcher, which will show file "
    "suggestions in the suggestion chips when the launcher is opened";

const char kEnhancedDeskAnimationsName[] =
    "Enable Enhanced Virtual Desks Animations";
const char kEnhancedDeskAnimationsDescription[] =
    "Allows pressing multiple keyboard shortcuts to switch multiple desks, and "
    "to have touchpad swipes continuously move desks.";

const char kEnterpriseReportingInChromeOSName[] =
    "Enterprise cloud reporting in Chrome OS";
const char kEnterpriseReportingInChromeOSDescription[] =
    "Enable the enterprise cloud reporting in Chrome OS. This feature requires "
    "user level cloud management.";

const char kExoGamepadVibrationName[] = "Gamepad Vibration for Exo Clients";
const char kExoGamepadVibrationDescription[] =
    "Allow Exo clients like Android to request vibration events for gamepads "
    "that support it.";

const char kExoOrdinalMotionName[] =
    "Raw (unaccelerated) motion for Linux applications";
const char kExoOrdinalMotionDescription[] =
    "Send unaccelerated values as raw motion events to linux applications.";

const char kExoPointerLockName[] = "Pointer lock for Linux applications";
const char kExoPointerLockDescription[] =
    "Allow Linux applications to request a pointer lock, i.e. exclusive use of "
    "the mouse pointer.";

const char kExperimentalAccessibilityChromeVoxAnnotationsName[] =
    "Enable experimental ChromeVox annotations feature.";
const char kExperimentalAccessibilityChromeVoxAnnotationsDescription[] =
    "Allows users to create custom annotations for elements using ChromeVox.";

const char kExperimentalAccessibilityChromeVoxTutorialName[] =
    "Enable experimental ChromeVox interactive tutorial.";
const char kExperimentalAccessibilityChromeVoxTutorialDescription[] =
    "A comprehensive and interactive tutorial for the ChromeVox screen reader.";

const char kExperimentalAccessibilitySwitchAccessTextName[] =
    "Enable enhanced Switch Access text input.";
const char kExperimentalAccessibilitySwitchAccessTextDescription[] =
    "Enable experimental or in-progress Switch Access features for improved "
    "text input";

const char kSwitchAccessPointScanningName[] =
    "Enable point scanning with Switch Access.";
const char kSwitchAccessPointScanningDescription[] =
    "Enable an in-process feature to select points onscreen with Switch "
    "Access.";

const char kExperimentalAccessibilityCursorColorsName[] =
    "Enable cursor colors.";
const char kExperimentalAccessibilityCursorColorsDescription[] =
    "Enable experimental feature which allows setting cursor color in "
    "Accessibility settings.";

const char kExperimentalAccessibilityMagnifierNewFocusFollowingName[] =
    "Enable new focus following in Magnifier";
const char kExperimentalAccessibilityMagnifierNewFocusFollowingDescription[] =
    "Enable experimental feature which allows more comprehensive focus "
    "following in Magnifier.";

const char kFilesAppCopyImageName[] = "Enable Copy Images from Files App";
const char kFilesAppCopyImageDescription[] =
    "Enables the Files App to copy images selected to the system clipboard";

const char kFilesCameraFolderName[] =
    "Enable Camera folder handling in Files App";
const char kFilesCameraFolderDescription[] =
    "Enables the Files App to display Camera folder with i18n name and make it "
    "nonmodifiable";

const char kFilesNGName[] = "Enable Files App. NG.";
const char kFilesNGDescription[] =
    "Enable the next generation UI style of the file manager.";

const char kFilesSinglePartitionFormatName[] =
    "Enable Partitioning of Removable Disks.";
const char kFilesSinglePartitionFormatDescription[] =
    "Enable partitioning of removable disks into single partition.";

const char kFilesSWAName[] = "Enable Files App SWA.";
const char kFilesSWADescription[] =
    "Enable the SWA version of the file manager.";

const char kFilesTransferDetailsName[] = "Enable Files Transfer Details.";
const char kFilesTransferDetailsDescription[] =
    "Enable transfer details like remaining time in the progress center panel.";

const char kFilesZipMountName[] = "New ZIP mounting in Files App";
const char kFilesZipMountDescription[] =
    "Enable new ZIP archive mounting system in File Manager.";

const char kFilesZipPackName[] = "New ZIP packing in Files App";
const char kFilesZipPackDescription[] =
    "Enable new ZIP archive creation system in File Manager.";

const char kFilesZipUnpackName[] = "New ZIP unpacking in Files App";
const char kFilesZipUnpackDescription[] =
    "Enable new ZIP archive extraction system in File Manager.";

extern const char kFiltersInRecentsName[] = "Enable filters in Recents";
extern const char kFiltersInRecentsDescription[] =
    "Enable file-type filters (Audio, Images, Videos) in Files App Recents "
    "view.";

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

const char kFsNosymfollowName[] =
    "Prevent symlink traversal on user-supplied filesystems.";
const char kFsNosymfollowDescription[] =
    "Causes user-supplied filesystems to be mounted with the 'nosymfollow'"
    " option, so the chromuimos LSM denies symlink traversal on the"
    " filesystem.";

const char kHelpAppReleaseNotesName[] = "Help App Release Notes";
const char kHelpAppReleaseNotesDescription[] =
    "Enables release notes in the help app";

const char kHelpAppSearchServiceIntegrationName[] =
    "Help App search service integration";
const char kHelpAppSearchServiceIntegrationDescription[] =
    "Enables the integration between the help app and the local search"
    " service. Includes using the search service for in app search.";

const char kHideArcMediaNotificationsName[] = "Hide ARC media notifications";
const char kHideArcMediaNotificationsDescription[] =
    "Hides media notifications for ARC apps. Requires "
    "#enable-media-session-notifications to be enabled.";

const char kHoldingSpaceName[] =
    "Quick access to screenshots, downloads, and files test";
const char kHoldingSpaceDescription[] =
    "Enables quick access to screenshots, downloads, and important files which "
    "aims to increase productivity by saving time. When enabled, access recent "
    "screenshots and downloads from the shelf. Pin important files with the "
    "Files App context menu to keep them one click away.";

const char kImeAssistAutocorrectName[] = "Enable assistive autocorrect";
const char kImeAssistAutocorrectDescription[] =
    "Enable assistive auto-correct features for native IME";

const char kImeAssistPersonalInfoName[] = "Enable assistive personal info";
const char kImeAssistPersonalInfoDescription[] =
    "Enable auto-complete suggestions on personal infomation for native IME.";

const char kImeEmojiSuggestAdditionName[] =
    "Enable emoji suggestion (addition)";
const char kImeEmojiSuggestAdditionDescription[] =
    "Enable emoji suggestion as addition to the text written for native IME.";

const char kImeInputLogicFstName[] = "Enable FST Input Logic on IME";
const char kImeInputLogicFstDescription[] =
    "Enable FST Input Logic to replace the IME legacy input logic on NaCl";

const char kImeInputLogicHmmName[] = "Enable HMM Input Logic on IME";
const char kImeInputLogicHmmDescription[] =
    "Enable HMM Input Logic to replace the IME legacy input logic on NaCl";

const char kImeInputLogicMozcName[] = "Enable MOZC Input Logic on IME";
const char kImeInputLogicMozcDescription[] =
    "Enable MOZC Input Logic to replace the IME legacy input logic on NaCl";

const char kImeMozcProtoName[] = "Enable protobuf on Japanese IME";
const char kImeMozcProtoDescription[] =
    "Enable Japanese IME to use protobuf as interactive message format to "
    "replace JSON";

const char kImeServiceSandboxName[] = "Use CrOS 'ime' sandbox";
const char kImeServiceSandboxDescription[] =
    "Controls whether the IME service on CrOS uses the 'ime' sandbox.";

const char kIntentHandlingSharingName[] = "Intent handling for sharing";
const char kIntentHandlingSharingDescription[] =
    "Support sharing in Chrome OS intent handling.";

const char kIntentPickerPWAPersistenceName[] = "Intent picker PWA Persistence";
const char kIntentPickerPWAPersistenceDescription[] =
    "Allow user to always open with PWA in intent picker.";

const char kInteractiveWindowCycleList[] =
    "Enable Alt-Tab interactivity improvements.";
const char kInteractiveWindowCycleListDescription[] =
    "Adds mouse behavior, three-finger touchpad swipe, left/right "
    "arrow navigation, and space/enter confirmation to Alt-Tab.";

const char kLacrosSupportName[] = "LaCrOS support";
const char kLacrosSupportDescription[] =
    "Support for the experimental lacros-chrome browser.";

const char kLimitAltTabToActiveDeskName[] =
    "Limit Alt-Tab windows to active desk";
const char kLimitAltTabToActiveDeskDescription[] =
    "Limits the windows listed in Alt-Tab to the ones in the currently active "
    "virtual desk";

const char kLimitShelfItemsToActiveDeskName[] =
    "Limit Shelf items to active desk";
const char kLimitShelfItemsToActiveDeskDescription[] =
    "Limits items on the shelf to the ones associated with windows on the "
    "active desk";

const char kListAllDisplayModesName[] = "List all display modes";
const char kListAllDisplayModesDescription[] =
    "Enables listing all external displays' modes in the display settings.";

const char kLockScreenMediaControlsName[] = "Lock screen media controls";
const char kLockScreenMediaControlsDescription[] =
    "Enable media controls on the lock screen.";

const char kLockScreenNotificationName[] = "Lock screen notification";
const char kLockScreenNotificationDescription[] =
    "Enable notifications on the lock screen.";

const char kMediaAppName[] = "Media App";
const char kMediaAppDescription[] =
    "Enables the chrome://media-app System Web App (SWA)";

const char kMediaNotificationsCounterName[] = "Media Notifications Counter";
const char kMediaNotificationsCounterDescription[] =
    "Remove media notifications from the notification counter in the status "
    "area";

const char kMediaSessionNotificationsName[] = "Media session notifications";
const char kMediaSessionNotificationsDescription[] =
    "Shows notifications for media sessions showing the currently playing "
    "media and providing playback controls";

const char kMeteredShowToggleName[] = "Show Metered Toggle";
const char kMeteredShowToggleDescription[] =
    "Shows a Metered toggle in the Network settings UI for WiFI and Cellular. "
    "The toggle allows users to set whether a network should be considered "
    "metered for purposes of bandwith usage (e.g. for automatic updates).";

const char kOsSettingsPolymer3Name[] = "Enable OS Settings Polymer3";
const char kOsSettingsPolymer3Description[] =
    "Flips chrome://os-settings to show Polymer3 version.";

const char kPhoneHubName[] = "Enable Phone Hub";
const char kPhoneHubDescription[] =
    "Provides a UI for users to view information about their Android phone"
    "and perform phone-side actions within Chrome OS.";

const char kReduceDisplayNotificationsName[] = "Reduce display notifications";
const char kReduceDisplayNotificationsDescription[] =
    "If enabled, notifications for display rotation, display removed, display "
    "mirroring, and display extending will be suppressed.";

const char kReleaseNotesNotificationName[] = "Release Notes Notification";
const char kReleaseNotesNotificationDescription[] =
    "Enables the release notes notification and suggestion chip";

const char kReleaseNotesNotificationAllChannelsName[] =
    "Release Notes Notification All Channels";
const char kReleaseNotesNotificationAllChannelsDescription[] =
    "Enables the release notes notification for all Chrome OS channels";

const char kScanningUIName[] = "Scanning UI";
const char kScanningUIDescription[] =
    "An experimental UI that allows users to interact with a connected "
    "scanner.";

const char kSchedulerConfigurationName[] = "Scheduler Configuration";
const char kSchedulerConfigurationDescription[] =
    "Instructs the OS to use a specific scheduler configuration setting.";
const char kSchedulerConfigurationConservative[] =
    "Disables Hyper-Threading on relevant CPUs.";
const char kSchedulerConfigurationPerformance[] =
    "Enables Hyper-Threading on relevant CPUs.";

const char kSeparatePointingStickSettingsName[] =
    "Separate pointing stick settings";
const char kSeparatePointingStickSettingsDescription[] =
    "Enables a separate group of settings (speed, button swap, and "
    "acceleration) for pointing sticks (such as TrackPoints).";

const char kSharesheetName[] = "Sharesheet";
const char kSharesheetDescription[] = "Chrome OS sharesheet.";

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

const char kSmartDimNewMlAgentName[] = "Smart Dim new machine learning agent";
const char kSmartDimNewMlAgentDescription[] =
    "Uses a new machine learning agent to switch between builtin and dynamic "
    "ML models.";

const char kSmbfsFileSharesName[] = "Smbfs file shares";
const char kSmbfsFileSharesDescription[] =
    "Use smbfs for accessing network file shares.";

const char kSplitSettingsSyncName[] = "Split OS and browser sync";
const char kSplitSettingsSyncDescription[] =
    "Allows OS sync to be configured separately from browser sync. Changes the "
    "OS settings UI to provide controls for OS data types. Requires "
    "#split-settings to be enabled.";

const char kSyncWifiConfigurationsName[] = "Sync Wi-Fi network configurations";
const char kSyncWifiConfigurationsDescription[] =
    "Enables the option to sync Wi-Fi network configurations with Chrome Sync.";

const char kSystemLatinPhysicalTypingName[] =
    "Use system IME for latin-script typing";
const char kSystemLatinPhysicalTypingDescription[] =
    "Use the system input engine instead of the Chrome extension for physical "
    "typing in languages based on latin script.";

const char kPluginVmShowCameraPermissionsName[] =
    "Show Plugin VM camera permissions";
const char kPluginVmShowCameraPermissionsDescription[] =
    "Displays camera permissions for Plugin VM in the app settings.";

const char kPluginVmShowMicrophonePermissionsName[] =
    "Show Plugin VM microphone permissions";
const char kPluginVmShowMicrophonePermissionsDescription[] =
    "Displays microphone permissions for Plugin VM in the app settings.";

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

const char kUnifiedMediaViewName[] = "Unified media view in Files App";
const char kUnifiedMediaViewDescription[] =
    "Enable unified media view to browse recently-modified media files from"
    " local disk, Google Drive, and Android.";

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

const char kVmCameraMicIndicatorsAndNotificationsName[] =
    "VM camera/mic indicators/notifications";
const char kVmCameraMicIndicatorsAndNotificationsDescription[] =
    "Show VM camera/mic indicators/notifications";

const char kWifiSyncAndroidName[] =
    "Sync Wi-Fi network configurations with Android";
const char kWifiSyncAndroidDescription[] =
    "Enables the option to sync Wi-Fi network configurations between Chrome OS "
    "devices and a connected Android phone";

const char kZeroCopyVideoCaptureName[] = "Enable Zero-Copy Video Capture";
const char kZeroCopyVideoCaptureDescription[] =
    "Camera produces a gpu friendly buffer on capture and, if there is, "
    "hardware accelerated video encoder consumes the buffer";

// Prefer keeping this section sorted to adding new definitions down here.

#endif  // defined(OS_CHROMEOS)

// All views-based platforms --------------------------------------------------

#if defined(TOOLKIT_VIEWS)

const char kDesktopInProductHelpSnoozeName[] =
    "Allow snooze on supported in-product help promos";
const char kDesktopInProductHelpSnoozeDescription[] =
    "Snoozing an in-product help promo closes it and schedules it to be shown "
    "later. When enabled, this functionality is allowed on supported promos.";

const char kEnableMDRoundedCornersOnDialogsName[] =
    "MD corners on secondary UI";
const char kEnableMDRoundedCornersOnDialogsDescription[] =
    "Increases corner radius on secondary UI.";

const char kInstallableInkDropName[] = "Use InstallableInkDrop where supported";
const char kInstallableInkDropDescription[] =
    "InstallableInkDrop is part of an InkDrop refactoring effort. This enables "
    "the pilot implementation where available.";

const char kTextfieldFocusOnTapUpName[] = "Focus UI text fields on touch-up";
const char kTextfieldFocusOnTapUpDescription[] =
    "When enabled, Views-based text fields take focus on touch-up instead of "
    "touch-down. This includes the Omnibox.";

const char kEnableNewBadgeOnMenuItemsName[] =
    "Enable 'New' badge on menu items";
const char kEnableNewBadgeOnMenuItemsDescription[] =
    "When enabled, allows 'New' badge to help users identify menu items which "
    "access new functionality.";

#endif  // defined(TOOLKIT_VIEWS)

// Random platform combinations -----------------------------------------------

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS)

const char kWebGL2ComputeContextName[] = "WebGL 2.0 Compute";
const char kWebGL2ComputeContextDescription[] =
    "Enable the use of WebGL 2.0 Compute API.";

#endif  // defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS)

#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)

const char kEnableMediaFeedsName[] = "Enables Media Feeds";
const char kEnableMediaFeedsDescription[] =
    "Enables the Media Feeds feature which allows us to fetch feeds of media "
    "items from websites that support the feature. ";

const char kEnableMediaFeedsBackgroundFetchName[] =
    "Enables Media Feeds Background Fetch";
const char kEnableMediaFeedsBackgroundFetchDescription[] =
    "Enables the Media Feeds background fetch feature which allows feeds to be "
    "fetched in the background. Requires #enable-media-feeds to be enabled. ";

const char kEnableOopPrintDriversName[] =
    "Enables Out-of-Process Printer Drivers";
const char kEnableOopPrintDriversDescription[] =
    "Enables printing interactions with the operating system to be performed "
    "out-of-process.";

const char kRemoteCopyReceiverName[] =
    "Enables the remote copy feature to receive messages";
const char kRemoteCopyReceiverDescription[] =
    "Enables the remote copy feature to handle messages by writing content to "
    "the clipboard and showing a notification to the user.";

const char kRemoteCopyImageNotificationName[] =
    "Enables image notifications for the remote copy feature";
const char kRemoteCopyImageNotificationDescription[] =
    "Enables image notifications to be shown for the remote copy feature "
    "when receiving a message.";

const char kRemoteCopyPersistentNotificationName[] =
    "Enables persistent notifications for the remote copy feature";
const char kRemoteCopyPersistentNotificationDescription[] =
    "Enables persistent notifications to be shown for the remote copy feature "
    "when receiving a message.";

const char kRemoteCopyProgressNotificationName[] =
    "Enables progress notifications for the remote copy feature";
const char kRemoteCopyProgressNotificationDescription[] =
    "Enables progress notifications to be shown for the remote copy feature "
    "when receiving a message.";

#endif  // defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) ||
        // defined(OS_CHROMEOS)

#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)

const char kDirectManipulationStylusName[] = "Direct Manipulation Stylus";
const char kDirectManipulationStylusDescription[] =
    "If enabled, Chrome will scroll web pages on stylus drag.";

#endif  // defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) ||
        // defined(OS_CHROMEOS)

#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_CHROMEOS)

const char kWebContentsOcclusionName[] = "Enable occlusion of web contents";
const char kWebContentsOcclusionDescription[] =
    "If enabled, web contents will behave as hidden when it is occluded by "
    "other windows.";

#endif  // defined(OS_WIN) || defined(OS_MAC) || defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
#if BUILDFLAG(USE_TCMALLOC)
const char kDynamicTcmallocName[] = "Dynamic Tcmalloc Tuning";
const char kDynamicTcmallocDescription[] =
    "Allows tcmalloc to dynamically adjust tunables based on system resource "
    "utilization.";
#endif  // BUILDFLAG(USE_TCMALLOC)
#endif  // #if defined(OS_CHROMEOS) || defined(OS_LINUX)

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
const char kUserDataSnapshotName[] = "Enable user data snapshots";
const char kUserDataSnapshotDescription[] =
    "Enables taking snapshots of the user data directory after a Chrome "
    "update and restoring them after a version rollback.";
#endif  // !defined(OS_ANDROID) && !defined(OS_CHROMEOS)

#if defined(OS_WIN) || defined(OS_CHROMEOS)
const char kWebShareName[] = "Web Share";
const char kWebShareDescription[] =
    "Enables the Web Share (navigator.share) APIs on experimentally supported "
    "platforms.";
#endif  // defined(OS_WIN) || defined(OS_CHROMEOS)

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MAC)
const char kEnableEphemeralGuestProfilesOnDesktopName[] =
    "Enable ephemeral Guest profiles on Desktop";
const char kEnableEphemeralGuestProfilesOnDesktopDescription[] =
    "Enables ephemeral Guest profiles on Windows, Linux, and Mac.";
#endif  // defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MAC)

// Feature flags --------------------------------------------------------------

#if defined(DCHECK_IS_CONFIGURABLE)
const char kDcheckIsFatalName[] = "DCHECKs are fatal";
const char kDcheckIsFatalDescription[] =
    "By default Chrome will evaluate in this build, but only log failures, "
    "rather than crashing. If enabled, DCHECKs will crash the calling process.";
#endif  // defined(DCHECK_IS_CONFIGURABLE)

#if BUILDFLAG(ENABLE_AV1_DECODER)
const char kEnableAVIFName[] = "Enable AVIF image format";
const char kEnableAVIFDescription[] =
    "Adds image decoding support for the AVIF image format.";
#endif  // BUILDFLAG(ENABLE_AV1_DECODER)

#if BUILDFLAG(ENABLE_CLICK_TO_CALL)
const char kClickToCallUIName[] =
    "Enable click to call feature signals to be handled on desktop";
const char kClickToCallUIDescription[] =
    "Enables click to call feature signals to be handled on desktop by showing "
    "a list of user's available devices with telephony functionality.";
#endif  // BUILDFLAG(ENABLE_CLICK_TO_CALL)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
const char kDiceWebSigninInterceptionName[] = "Dice Web-Signin Interception";
const char kDiceWebSigninInterceptionDescription[] =
    "If enabled, Chrome may promote profile creation after signin on the web.";
#endif

#if BUILDFLAG(ENABLE_NACL)
const char kNaclName[] = "Native Client";
const char kNaclDescription[] =
    "Support Native Client for all web applications, even those that were not "
    "installed from the Chrome Web Store.";
#endif  // ENABLE_NACL

#if BUILDFLAG(ENABLE_PAINT_PREVIEW) && defined(OS_ANDROID)
const char kPaintPreviewDemoName[] = "Paint Preview Demo";
const char kPaintPreviewDemoDescription[] =
    "If enabled a menu item is added to the Android main menu to demo paint "
    "previews.";
const char kPaintPreviewStartupName[] = "Paint Preview Startup";
const char kPaintPreviewStartupDescription[] =
    "If enabled, paint previews for each tab are captured when a tab is hidden "
    "and are deleted when a tab is closed. If a paint preview was captured for "
    "the tab to be restored on startup, the paint preview will be shown "
    "instead.";
#endif  // ENABLE_PAINT_PREVIEW && defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_PLUGINS)

const char kPdfFormSaveName[] = "Save PDF Forms";
const char kPdfFormSaveDescription[] =
    "Enable saving PDFs with filled form data.";

const char kPdfHonorJsContentSettingsName[] =
    "Honor JavaScript content settings in PDFs";
const char kPdfHonorJsContentSettingsDescription[] =
    "Enable whether an origin's JavaScript content settings are honored in "
    "PDFs opened from that origin.";

const char kPdfViewerUpdateName[] = "PDF Viewer Update";
const char kPdfViewerUpdateDescription[] =
    "When enabled, the PDF viewer will display an updated UI with new "
    "options and features.";
#endif  // BUILDFLAG(ENABLE_PLUGINS)

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
const char kWebUITabStripName[] = "WebUI tab strip";
const char kWebUITabStripDescription[] =
    "When enabled makes use of a WebUI-based tab strip.";
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP) && defined(OS_CHROMEOS)
const char kWebUITabStripTabDragIntegrationName[] =
    "ChromeOS drag-drop extensions for WebUI tab strip";
const char kWebUITabStripTabDragIntegrationDescription[] =
    "Enables special handling in ash for WebUI tab strip tab drags. Allows "
    "dragging tabs out to new windows.";
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP) && defined(OS_CHROMEOS)

#if defined(TOOLKIT_VIEWS) || defined(OS_ANDROID)

const char kAutofillCreditCardUploadName[] =
    "Enable offering upload of Autofilled credit cards";
const char kAutofillCreditCardUploadDescription[] =
    "Enables a new option to upload credit cards to Google Payments for sync "
    "to all Chrome devices.";

#endif  // defined(TOOLKIT_VIEWS) || defined(OS_ANDROID)

#if defined(WEBRTC_USE_PIPEWIRE)
const char kWebrtcPipeWireCapturerName[] = "WebRTC PipeWire support";
const char kWebrtcPipeWireCapturerDescription[] =
    "When enabled the WebRTC will use the PipeWire multimedia server for "
    "capturing the desktop content on the Wayland display server.";
#endif  // #if defined(WEBRTC_USE_PIPEWIRE)

// ============================================================================
// Don't just add flags to the end, put them in the right section in
// alphabetical order just like the header file.
// ============================================================================

}  // namespace flag_descriptions
