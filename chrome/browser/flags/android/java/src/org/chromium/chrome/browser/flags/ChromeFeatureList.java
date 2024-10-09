// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import androidx.annotation.NonNull;

import org.chromium.base.BaseFeatures;
import org.chromium.base.FeatureMap;
import org.chromium.base.MutableBooleanParamWithSafeDefault;
import org.chromium.base.MutableFlagWithSafeDefault;
import org.chromium.base.MutableIntParamWithSafeDefault;
import org.chromium.components.cached_flags.AllCachedFieldTrialParameters;
import org.chromium.components.cached_flags.BooleanCachedFieldTrialParameter;
import org.chromium.components.cached_flags.CachedFlag;
import org.chromium.components.cached_flags.DoubleCachedFieldTrialParameter;
import org.chromium.components.cached_flags.IntCachedFieldTrialParameter;
import org.chromium.components.cached_flags.StringCachedFieldTrialParameter;

import java.util.List;
import java.util.Map;

/**
 * A list of feature flags exposed to Java.
 *
 * <p>This class lists flags exposed to Java as String constants. They should match
 * |kFeaturesExposedToJava| in chrome/browser/flags/android/chrome_feature_list.cc.
 *
 * <p>This class also provides convenience methods to access values of flags and their field trial
 * parameters through {@link ChromeFeatureMap}.
 *
 * <p>Chrome-layer {@link CachedFlag}s are instantiated here as well.
 */
public abstract class ChromeFeatureList {

    /** Prevent instantiation. */
    private ChromeFeatureList() {}

    /**
     * Convenience method to check Chrome-layer feature flags, see {@link
     * FeatureMap#isEnabledInNative(String)}.
     *
     * <p>Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in chrome/browser/flags/android/chrome_feature_list.cc
     */
    public static boolean isEnabled(String featureName) {
        return ChromeFeatureMap.isEnabled(featureName);
    }

    /**
     * Convenience method to get Chrome-layer feature field trial params, see {@link
     * FeatureMap#getFieldTrialParamByFeature(String, String)}.
     *
     * <p>Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in chrome/browser/flags/android/chrome_feature_list.cc
     */
    public static String getFieldTrialParamByFeature(String featureName, String paramName) {
        return ChromeFeatureMap.getInstance().getFieldTrialParamByFeature(featureName, paramName);
    }

    /**
     * Convenience method to get Chrome-layer feature field trial params, see {@link
     * FeatureMap#getFieldTrialParamByFeatureAsBoolean(String, String, boolean)}.
     *
     * <p>Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in chrome/browser/flags/android/chrome_feature_list.cc
     */
    public static boolean getFieldTrialParamByFeatureAsBoolean(
            String featureName, String paramName, boolean defaultValue) {
        return ChromeFeatureMap.getInstance()
                .getFieldTrialParamByFeatureAsBoolean(featureName, paramName, defaultValue);
    }

    /**
     * Convenience method to get Chrome-layer feature field trial params, see {@link
     * FeatureMap#getFieldTrialParamByFeatureAsInt(String, String, int)}.
     *
     * <p>Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in chrome/browser/flags/android/chrome_feature_list.cc
     */
    public static int getFieldTrialParamByFeatureAsInt(
            String featureName, String paramName, int defaultValue) {
        return ChromeFeatureMap.getInstance()
                .getFieldTrialParamByFeatureAsInt(featureName, paramName, defaultValue);
    }

    /**
     * Convenience method to get Chrome-layer feature field trial params, see {@link
     * FeatureMap#getFieldTrialParamByFeatureAsDouble(String, String, double)}.
     *
     * <p>Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in chrome/browser/flags/android/chrome_feature_list.cc
     */
    public static double getFieldTrialParamByFeatureAsDouble(
            String featureName, String paramName, double defaultValue) {
        return ChromeFeatureMap.getInstance()
                .getFieldTrialParamByFeatureAsDouble(featureName, paramName, defaultValue);
    }

    /**
     * Convenience method to get Chrome-layer feature field trial params, see {@link
     * FeatureMap#getFieldTrialParamsForFeature(String)}.
     *
     * <p>Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in chrome/browser/flags/android/chrome_feature_list.cc
     */
    public static Map<String, String> getFieldTrialParamsForFeature(String featureName) {
        return ChromeFeatureMap.getInstance().getFieldTrialParamsForFeature(featureName);
    }

    public static AllCachedFieldTrialParameters newAllCachedFieldTrialParameters(
            String featureName) {
        return new AllCachedFieldTrialParameters(ChromeFeatureMap.getInstance(), featureName);
    }

    public static BooleanCachedFieldTrialParameter newBooleanCachedFieldTrialParameter(
            String featureName, String variationName, boolean defaultValue) {
        return new BooleanCachedFieldTrialParameter(
                ChromeFeatureMap.getInstance(), featureName, variationName, defaultValue);
    }

    public static DoubleCachedFieldTrialParameter newDoubleCachedFieldTrialParameter(
            String featureName, String variationName, double defaultValue) {
        return new DoubleCachedFieldTrialParameter(
                ChromeFeatureMap.getInstance(), featureName, variationName, defaultValue);
    }

    public static IntCachedFieldTrialParameter newIntCachedFieldTrialParameter(
            String featureName, String variationName, int defaultValue) {
        return new IntCachedFieldTrialParameter(
                ChromeFeatureMap.getInstance(), featureName, variationName, defaultValue);
    }

    public static StringCachedFieldTrialParameter newStringCachedFieldTrialParameter(
            String featureName, String variationName, @NonNull String defaultValue) {
        return new StringCachedFieldTrialParameter(
                ChromeFeatureMap.getInstance(), featureName, variationName, defaultValue);
    }

    private static CachedFlag newCachedFlag(String featureName, boolean defaultValue) {
        return newCachedFlag(featureName, defaultValue, defaultValue);
    }

    private static CachedFlag newCachedFlag(
            String featureName, boolean defaultValue, boolean defaultValueInTests) {
        return new CachedFlag(
                ChromeFeatureMap.getInstance(), featureName, defaultValue, defaultValueInTests);
    }

    private static MutableFlagWithSafeDefault newMutableFlagWithSafeDefault(
            String featureName, boolean defaultValue) {
        return ChromeFeatureMap.getInstance().mutableFlagWithSafeDefault(featureName, defaultValue);
    }

    // Feature names.
    /* Alphabetical: */
    public static final String ACCOUNT_REAUTHENTICATION_RECENT_TIME_WINDOW =
            "AccountReauthenticationRecentTimeWindow";
    public static final String ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_PAGE_SUMMARY =
            "AdaptiveButtonInTopToolbarPageSummary";
    public static final String ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2 =
            "AdaptiveButtonInTopToolbarCustomizationV2";
    public static final String ALLOW_NEW_INCOGNITO_TAB_INTENTS = "AllowNewIncognitoTabIntents";
    public static final String ANDROID_APP_INTEGRATION = "AndroidAppIntegration";
    public static final String ANDROID_APP_INTEGRATION_WITH_FAVICON =
            "AndroidAppIntegrationWithFavicon";
    public static final String ANDROID_BOTTOM_TOOLBAR = "AndroidBottomToolbar";
    public static final String ANDROID_ELEGANT_TEXT_HEIGHT = "AndroidElegantTextHeight";
    public static final String ANDROID_GOOGLE_SANS_TEXT = "AndroidGoogleSansText";
    public static final String ANDROID_HUB_FLOATING_ACTION_BUTTON =
            "AndroidHubFloatingActionButton";
    public static final String ANDROID_HUB_SEARCH = "AndroidHubSearch";
    public static final String ANDROID_HUB_V2 = "AndroidHubV2";
    public static final String ANDROID_NO_VISIBLE_HINT_FOR_DIFFERENT_TLD =
            "AndroidNoVisibleHintForDifferentTLD";
    public static final String ANDROID_TAB_DECLUTTER = "AndroidTabDeclutter";
    public static final String ANDROID_TAB_DECLUTTER_ARCHIVE_ALL_BUT_ACTIVE =
            "AndroidTabDeclutterArchiveAllButActiveTab";
    public static final String ANDROID_TAB_DECLUTTER_DEDUPE_TAB_IDS_KILL_SWITCH =
            "AndroidTabDeclutterDedupeTabIdsKillSwitch";
    public static final String ANDROID_TAB_DECLUTTER_RESCUE_KILLSWITCH =
            "AndroidTabDeclutterRescueKillswitch";
    public static final String ANIMATED_IMAGE_DRAG_SHADOW = "AnimatedImageDragShadow";
    public static final String APP_SPECIFIC_HISTORY = "AppSpecificHistory";
    public static final String ASYNC_NOTIFICATION_MANAGER = "AsyncNotificationManager";
    public static final String AUTOFILL_ALLOW_NON_HTTP_ACTIVATION =
            "AutofillAllowNonHttpActivation";
    public static final String AUTOFILL_ENABLE_CARD_ART_IMAGE = "AutofillEnableCardArtImage";
    public static final String AUTOFILL_ENABLE_CARD_BENEFITS_FOR_AMERICAN_EXPRESS =
            "AutofillEnableCardBenefitsForAmericanExpress";
    public static final String AUTOFILL_ENABLE_CARD_BENEFITS_FOR_CAPITAL_ONE =
            "AutofillEnableCardBenefitsForCapitalOne";
    public static final String AUTOFILL_ENABLE_CARD_PRODUCT_NAME = "AutofillEnableCardProductName";
    public static final String AUTOFILL_ENABLE_LOCAL_IBAN = "AutofillEnableLocalIban";
    public static final String AUTOFILL_ENABLE_MOVING_GPAY_LOGO_TO_THE_RIGHT_ON_CLANK =
            "AutofillEnableMovingGPayLogoToTheRightOnClank";
    public static final String AUTOFILL_ENABLE_NEW_CARD_ART_AND_NETWORK_IMAGES =
            "AutofillEnableNewCardArtAndNetworkImages";
    public static final String AUTOFILL_ENABLE_SERVER_IBAN = "AutofillEnableServerIban";
    public static final String AUTOFILL_ENABLE_CARD_ART_SERVER_SIDE_STRETCHING =
            "AutofillEnableCardArtServerSideStretching";
    public static final String AUTOFILL_ENABLE_CVC_STORAGE = "AutofillEnableCvcStorageAndFilling";
    public static final String AUTOFILL_ENABLE_PAYMENT_SETTINGS_CARD_PROMO_AND_SCAN_CARD =
            "AutofillEnablePaymentSettingsCardPromoAndScanCard";
    public static final String AUTOFILL_ENABLE_PAYMENT_SETTINGS_SERVER_CARD_SAVE =
            "AutofillEnablePaymentSettingsServerCardSave";
    public static final String AUTOFILL_ENABLE_RANKING_FORMULA_ADDRESS_PROFILES =
            "AutofillEnableRankingFormulaAddressProfiles";
    public static final String AUTOFILL_ENABLE_RANKING_FORMULA_CREDIT_CARDS =
            "AutofillEnableRankingFormulaCreditCards";
    public static final String AUTOFILL_ENABLE_SAVE_CARD_LOADING_AND_CONFIRMATION =
            "AutofillEnableSaveCardLoadingAndConfirmation";
    public static final String AUTOFILL_ENABLE_SYNCING_OF_PIX_BANK_ACCOUNTS =
            "AutofillEnableSyncingOfPixBankAccounts";
    public static final String AUTOFILL_ENABLE_VCN_ENROLL_LOADING_AND_CONFIRMATION =
            "AutofillEnableVcnEnrollLoadingAndConfirmation";
    public static final String AUTOFILL_ENABLE_VERVE_CARD_SUPPORT =
            "AutofillEnableVerveCardSupport";
    public static final String AUTOFILL_ENABLE_VIRTUAL_CARD_METADATA =
            "AutofillEnableVirtualCardMetadata";
    public static final String AUTOFILL_VIRTUAL_VIEW_STRUCTURE_ANDROID =
            "AutofillVirtualViewStructureAndroid";
    public static final String AUTOFILL_ENABLE_SECURITY_TOUCH_EVENT_FILTERING_ANDROID =
            "AutofillEnableSecurityTouchEventFilteringAndroid";
    public static final String AUTOMOTIVE_FULLSCREEN_TOOLBAR_IMPROVEMENTS =
            "AutomotiveFullscreenToolbarImprovements";
    public static final String AVOID_SELECTED_TAB_FOCUS_ON_LAYOUT_DONE_SHOWING =
            "AvoidSelectedTabFocusOnLayoutDoneShowing";
    public static final String BACKGROUND_THREAD_POOL = "BackgroundThreadPool";
    public static final String BACK_FORWARD_CACHE = "BackForwardCache";
    public static final String BACK_FORWARD_TRANSITIONS = "BackForwardTransitions";
    public static final String BACK_GESTURE_ACTIVITY_TAB_PROVIDER =
            "BackGestureActivityTabProvider";
    public static final String BACK_GESTURE_MOVE_TO_BACK_DURING_STARTUP =
            "BackGestureMoveToBackDuringStartup";
    public static final String BACK_GESTURE_REFACTOR = "BackGestureRefactorAndroid";
    public static final String BACK_TO_HOME_ANIMATION = "BackToHomeAnimation";
    public static final String BCIV_PHONE_ONLY = "AndroidBcivPhoneOnly";
    public static final String BCIV_WITH_SUPPRESSION = "AndroidBcivWithSuppression";
    public static final String BCIV_ZERO_BROWSER_FRAMES = "AndroidBcivZeroBrowserFrames";
    public static final String BLOCK_INTENTS_WHILE_LOCKED = "BlockIntentsWhileLocked";
    public static final String BOARDING_PASS_DETECTOR = "BoardingPassDetector";
    public static final String BOTTOM_BROWSER_CONTROLS_REFACTOR = "BottomBrowserControlsRefactor";
    public static final String BROWSER_CONTROLS_EARLY_RESIZE = "BrowserControlsEarlyResize";
    public static final String BROWSER_CONTROLS_IN_VIZ = "AndroidBrowserControlsInViz";
    public static final String BROWSING_DATA_MODEL = "BrowsingDataModel";
    public static final String CACHE_ACTIVITY_TASKID = "CacheActivityTaskID";
    public static final String CAPTIVE_PORTAL_CERTIFICATE_LIST = "CaptivePortalCertificateList";
    public static final String CCT_AUTH_TAB = "CCTAuthTab";
    public static final String CCT_AUTH_TAB_DISABLE_ALL_EXTERNAL_INTENTS =
            "CCTAuthTabDisableAllExternalIntents";
    public static final String CCT_AUTO_TRANSLATE = "CCTAutoTranslate";
    public static final String CCT_BEFORE_UNLOAD = "CCTBeforeUnload";
    public static final String CCT_CLIENT_DATA_HEADER = "CCTClientDataHeader";
    public static final String CCT_EPHEMERAL_MODE = "CCTEphemeralMode";
    public static final String CCT_EXTEND_TRUSTED_CDN_PUBLISHER = "CCTExtendTrustedCdnPublisher";
    public static final String CCT_FEATURE_USAGE = "CCTFeatureUsage";
    public static final String CCT_INCOGNITO_AVAILABLE_TO_THIRD_PARTY =
            "CCTIncognitoAvailableToThirdParty";
    public static final String CCT_MINIMIZED = "CCTMinimized";
    public static final String CCT_MINIMIZED_ENABLED_BY_DEFAULT = "CCTMinimizedEnabledByDefault";
    public static final String CCT_NAVIGATIONAL_PREFETCH = "CCTNavigationalPrefetch";
    public static final String CCT_NESTED_SECURITY_ICON = "CCTNestedSecurityIcon";
    public static final String CCT_INTENT_FEATURE_OVERRIDES = "CCTIntentFeatureOverrides";

    public static final String CCT_GOOGLE_BOTTOM_BAR = "CCTGoogleBottomBar";
    public static final String CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS =
            "CCTGoogleBottomBarVariantLayouts";
    // NOTE: Do not query this feature directly, use
    // WarmupManager#isCCTPrewarmTabFeatureEnabled.
    public static final String CCT_PREWARM_TAB = "CCTPrewarmTab";
    public static final String CCT_REPORT_PARALLEL_REQUEST_STATUS =
            "CCTReportParallelRequestStatus";
    public static final String CCT_RESIZABLE_FOR_THIRD_PARTIES = "CCTResizableForThirdParties";
    public static final String CCT_REVAMPED_BRANDING = "CCTRevampedBranding";
    public static final String CCT_TAB_MODAL_DIALOG = "CCTTabModalDialog";
    public static final String CHROME_SURVEY_NEXT_ANDROID = "ChromeSurveyNextAndroid";
    public static final String CHROME_SHARE_PAGE_INFO = "ChromeSharePageInfo";
    public static final String CLANK_STARTUP_LATENCY_INJECTION = "ClankStartupLatencyInjection";
    public static final String COLLECT_ANDROID_FRAME_TIMELINE_METRICS =
            "CollectAndroidFrameTimelineMetrics";
    public static final String COMMAND_LINE_ON_NON_ROOTED = "CommandLineOnNonRooted";
    public static final String COMMERCE_MERCHANT_VIEWER = "CommerceMerchantViewer";
    public static final String COMMERCE_PRICE_TRACKING = "CommercePriceTracking";
    public static final String CONTEXTUAL_PAGE_ACTIONS = "ContextualPageActions";
    public static final String CONTEXTUAL_PAGE_ACTION_READER_MODE =
            "ContextualPageActionReaderMode";
    public static final String CONTEXTUAL_SEARCH_DISABLE_ONLINE_DETECTION =
            "ContextualSearchDisableOnlineDetection";
    public static final String CONTEXTUAL_SEARCH_SUPPRESS_SHORT_VIEW =
            "ContextualSearchSuppressShortView";
    public static final String CONTEXT_MENU_SYS_UI_MATCHES_ACTIVITY =
            "ContextMenuSysUiMatchesActivity";
    public static final String CONTEXT_MENU_TRANSLATE_WITH_GOOGLE_LENS =
            "ContextMenuTranslateWithGoogleLens";
    public static final String COOKIE_DEPRECATION_FACILITATED_TESTING =
            "CookieDeprecationFacilitatedTesting";
    public static final String CORMORANT = "Cormorant";
    public static final String CROSS_DEVICE_TAB_PANE_ANDROID = "CrossDeviceTabPaneAndroid";
    public static final String DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING =
            "DarkenWebsitesCheckboxInThemesSetting";
    public static final String DATA_SHARING = "DataSharing";
    public static final String DATA_SHARING_JOIN_ONLY_FOR_TESTING = "DataSharingJoinOnly";
    public static final String DEFAULT_BROWSER_PROMO_ANDROID = "DefaultBrowserPromoAndroid";
    public static final String DEFAULT_BROWSER_PROMO_ANDROID2 = "DefaultBrowserPromoAndroid2";
    public static final String DELAY_TEMP_STRIP_REMOVAL = "DelayTempStripRemoval";
    public static final String DEVICE_AUTHENTICATOR_ANDROIDX = "DeviceAuthenticatorAndroidx";
    public static final String DETAILED_LANGUAGE_SETTINGS = "DetailedLanguageSettings";
    public static final String DISABLE_INSTANCE_LIMIT = "DisableInstanceLimit";
    public static final String DISCO_FEED_ENDPOINT = "DiscoFeedEndpoint";
    public static final String DOWNLOADS_MIGRATE_TO_JOBS_API = "DownloadsMigrateToJobsAPI";
    public static final String DRAG_DROP_INTO_OMNIBOX = "DragDropIntoOmnibox";
    public static final String DRAG_DROP_TAB_TEARING = "DragDropTabTearing";
    public static final String DRAG_DROP_TAB_TEARING_ENABLE_OEM = "DragDropTabTearingEnableOEM";
    public static final String DRAW_CUTOUT_EDGE_TO_EDGE = "DrawCutoutEdgeToEdge";
    public static final String DRAW_EDGE_TO_EDGE = "DrawEdgeToEdge";
    public static final String DRAW_KEY_NATIVE_EDGE_TO_EDGE = "DrawKeyNativeEdgeToEdge";
    public static final String DRAW_NATIVE_EDGE_TO_EDGE = "DrawNativeEdgeToEdge";
    public static final String EDGE_TO_EDGE_BOTTOM_CHIN = "EdgeToEdgeBottomChin";
    public static final String EDGE_TO_EDGE_EVERYWHERE = "EdgeToEdgeEverywhere";
    public static final String EDGE_TO_EDGE_WEB_OPT_IN = "EdgeToEdgeWebOptIn";
    public static final String EDUCATIONAL_TIP_MODULE = "EducationalTipModule";
    public static final String ENABLE_DISCOUNT_INFO_API = "EnableDiscountInfoApi";
    public static final String ENABLE_PASSWORDS_ACCOUNT_STORAGE_FOR_NON_SYNCING_USERS =
            "EnablePasswordsAccountStorageForNonSyncingUsers";
    public static final String EXPERIMENTS_FOR_AGSA = "ExperimentsForAgsa";
    public static final String FEED_CONTAINMENT = "FeedContainment";
    public static final String FEED_FOLLOW_UI_UPDATE = "FeedFollowUiUpdate";
    public static final String FEED_IMAGE_MEMORY_CACHE_SIZE_PERCENTAGE =
            "FeedImageMemoryCacheSizePercentage";
    public static final String FEED_LOADING_PLACEHOLDER = "FeedLoadingPlaceholder";
    public static final String FEED_LOW_MEMORY_IMPROVEMENT = "FeedLowMemoryImprovement";
    public static final String FEED_POSITION_ANDROID = "FeedPositionAndroid";
    public static final String FEED_SHOW_SIGN_IN_COMMAND = "FeedShowSignInCommand";
    public static final String FILLING_PASSWORDS_FROM_ANY_ORIGIN = "FillingPasswordsFromAnyOrigin";
    public static final String FINGERPRINTING_PROTECTION_UX = "FingerprintingProtectionUx";
    public static final String FINGERPRINTING_PROTECTION_USER_BYPASS =
            "FingerprintingProtectionUserBypass";
    public static final String FOCUS_OMNIBOX_IN_INCOGNITO_TAB_INTENTS =
            "FocusOmniboxInIncognitoTabIntents";
    public static final String FORCE_BROWSER_CONTROLS_UPON_EXITING_FULLSCREEN =
            "ForceBrowserControlsUponExitingFullscreen";
    public static final String FORCE_DISABLE_EXTENDED_SYNC_PROMOS =
            "ForceDisableExtendedSyncPromos";
    public static final String FORCE_LIST_TAB_SWITCHER = "ForceListTabSwitcher";
    public static final String FORCE_STARTUP_SIGNIN_PROMO = "ForceStartupSigninPromo";
    public static final String FORCE_WEB_CONTENTS_DARK_MODE = "WebContentsForceDark";
    public static final String FULLSCREEN_INSETS_API_MIGRATION = "FullscreenInsetsApiMigration";
    public static final String FULLSCREEN_INSETS_API_MIGRATION_ON_AUTOMOTIVE =
            "FullscreenInsetsApiMigrationOnAutomotive";
    public static final String GTS_CLOSE_TAB_ANIMATION_KILL_SWITCH =
            "GtsCloseTabAnimationKillSwitch";
    public static final String LOCK_BACK_PRESS_HANDLER_AT_START = "LockBackPressHandlerAtStart";
    public static final String HASH_PREFIX_REAL_TIME_LOOKUPS =
            "SafeBrowsingHashPrefixRealTimeLookups";
    public static final String HISTORY_JOURNEYS = "Journeys";
    public static final String INCOGNITO_REAUTHENTICATION_FOR_ANDROID =
            "IncognitoReauthenticationForAndroid";
    public static final String INCOGNITO_SCREENSHOT = "IncognitoScreenshot";
    public static final String INTEREST_FEED_V2_HEARTS = "InterestFeedV2Hearts";
    public static final String IP_PROTECTION_V1 = "IpProtectionV1";
    public static final String IP_PROTECTION_UX = "IpProtectionUx";
    public static final String IP_PROTECTION_USER_BYPASS = "IpProtectionUserBypass";
    public static final String KID_FRIENDLY_CONTENT_FEED = "KidFriendlyContentFeed";
    public static final String LENS_ON_QUICK_ACTION_SEARCH_WIDGET = "LensOnQuickActionSearchWidget";
    public static final String LINKED_SERVICES_SETTING = "LinkedServicesSetting";
    public static final String LOADING_PREDICTOR_LIMIT_PRECONNECT_SOCKET_COUNT =
            "LoadingPredictorLimitPreconnectSocketCount";
    public static final String LOGO_POLISH = "LogoPolish";
    public static final String LOGO_POLISH_ANIMATION_KILL_SWITCH = "LogoPolishAnimationKillSwitch";
    public static final String LOOKALIKE_NAVIGATION_URL_SUGGESTIONS_UI =
            "LookalikeUrlNavigationSuggestionsUI";
    public static final String MAGIC_STACK_ANDROID = "MagicStackAndroid";
    public static final String MAYLAUNCHURL_USES_SEPARATE_STORAGE_PARTITION =
            "MayLaunchUrlUsesSeparateStoragePartition";
    public static final String MESSAGES_FOR_ANDROID_ADS_BLOCKED = "MessagesForAndroidAdsBlocked";
    public static final String MOST_VISITED_TILES_RESELECT = "MostVisitedTilesReselect";
    public static final String MUlTI_INSTANCE_APPLICATION_STATUS_CLEANUP =
            "MultiInstanceApplicationStatusCleanup";
    public static final String NAV_BAR_COLOR_MATCHES_TAB_BACKGROUND =
            "NavBarColorMatchesTabBackground";
    public static final String NEW_TAB_SEARCH_ENGINE_URL_ANDROID = "NewTabSearchEngineUrlAndroid";
    public static final String NEW_TAB_PAGE_ANDROID_TRIGGER_FOR_PRERENDER2 =
            "NewTabPageAndroidTriggerForPrerender2";
    public static final String NOTIFICATION_ONE_TAP_UNSUBSCRIBE = "NotificationOneTapUnsubscribe";
    public static final String NOTIFICATION_PERMISSION_VARIANT = "NotificationPermissionVariant";
    public static final String NOTIFICATION_PERMISSION_BOTTOM_SHEET =
            "NotificationPermissionBottomSheet";
    public static final String OMAHA_MIN_SDK_VERSION_ANDROID = "OmahaMinSdkVersionAndroid";
    public static final String OMNIBOX_CACHE_SUGGESTION_RESOURCES =
            "OmniboxCacheSuggestionResources";
    public static final String AVOID_RELAYOUT_DURING_FOCUS_ANIMATION =
            "AvoidRelayoutDuringFocusAnimation";
    public static final String OMNIBOX_UPDATED_CONNECTION_SECURITY_INDICATORS =
            "OmniboxUpdatedConnectionSecurityIndicators";
    public static final String OPTIMIZATION_GUIDE_PUSH_NOTIFICATIONS =
            "OptimizationGuidePushNotifications";
    public static final String PAGE_INFO_ABOUT_THIS_SITE_MORE_LANGS =
            "PageInfoAboutThisSiteMoreLangs";
    public static final String PAINT_PREVIEW_DEMO = "PaintPreviewDemo";
    public static final String PARTNER_CUSTOMIZATIONS_UMA = "PartnerCustomizationsUma";
    public static final String BIOMETRIC_AUTH_IDENTITY_CHECK = "BiometricAuthIdentityCheck";
    public static final String PERMISSION_DEDICATED_CPSS_SETTING_ANDROID =
            "PermissionDedicatedCpssSettingAndroid";
    public static final String PLUS_ADDRESSES_ENABLED = "PlusAddressesEnabled";
    public static final String PLUS_ADDRESS_LOADING_STATES_ANDROID =
            "PlusAddressLoadingStatesAndroid";
    public static final String PLUS_ADDRESS_ANDROID_ENHANCED_LOADING_STATES_ENABLED =
            "PlusAddressAndroidEnhancedLoadingStatesEnabled";
    public static final String PLUS_ADDRESS_ANDROID_SETTINGS_ENTRY =
            "PlusAddressAndroidSettingsEntry";
    public static final String PREFETCH_BROWSER_INITIATED_TRIGGERS =
            "PrefetchBrowserInitiatedTriggers";
    public static final String PRERENDER2 = "Prerender2";
    public static final String PRECONNECT_ON_TAB_CREATION = "PreconnectOnTabCreation";
    public static final String PRICE_CHANGE_MODULE = "PriceChangeModule";
    public static final String PRICE_INSIGHTS = "PriceInsights";
    public static final String PRIVACY_GUIDE_ANDROID_3 = "PrivacyGuideAndroid3";
    public static final String PRIVACY_GUIDE_PRELOAD_ANDROID = "PrivacyGuidePreloadAndroid";
    public static final String PRIVACY_SANDBOX_ACTIVITY_TYPE_STORAGE =
            "PrivacySandboxActivityTypeStorage";
    public static final String PRIVACY_SANDBOX_ADS_NOTICE_CCT = "PrivacySandboxAdsNoticeCCT";
    public static final String PRIVACY_SANDBOX_FPS_UI = "PrivacySandboxFirstPartySetsUI";
    public static final String PRIVACY_SANDBOX_RELATED_WEBSITE_SETS_UI =
            "PrivacySandboxRelatedWebsiteSetsUi";
    public static final String PRIVACY_SANDBOX_SETTINGS_4 = "PrivacySandboxSettings4";
    public static final String PRIVACY_SANDBOX_SENTIMENT_SURVEY = "PrivacySandboxSentimentSurvey";
    public static final String PRIVACY_SANDBOX_PRIVACY_GUIDE_AD_TOPICS =
            "PrivacySandboxPrivacyGuideAdTopics";
    public static final String PRIVACY_SANDBOX_PRIVACY_POLICY = "PrivacySandboxPrivacyPolicy";
    public static final String PRIVACY_SANDBOX_PROACTIVE_TOPICS_BLOCKING =
            "PrivacySandboxProactiveTopicsBlocking";
    public static final String PRIVATE_STATE_TOKENS = "PrivateStateTokens";
    public static final String PUSH_MESSAGING_DISALLOW_SENDER_IDS =
            "PushMessagingDisallowSenderIDs";
    public static final String PWA_UPDATE_DIALOG_FOR_ICON = "PwaUpdateDialogForIcon";
    public static final String PWA_RESTORE_UI = "PwaRestoreUi";
    public static final String PWA_RESTORE_UI_AT_STARTUP = "PwaRestoreUiAtStartup";
    public static final String QUICK_DELETE_FOR_ANDROID = "QuickDeleteForAndroid";
    public static final String QUICK_DELETE_ANDROID_FOLLOWUP = "QuickDeleteAndroidFollowup";
    public static final String QUICK_DELETE_ANDROID_SURVEY = "QuickDeleteAndroidSurvey";
    public static final String QUIET_NOTIFICATION_PROMPTS = "QuietNotificationPrompts";
    public static final String READALOUD = "ReadAloud";
    public static final String READALOUD_BACKGROUND_PLAYBACK = "ReadAloudBackgroundPlayback";
    public static final String READALOUD_IN_OVERFLOW_MENU_IN_CCT = "ReadAloudInOverflowMenuInCCT";
    public static final String READALOUD_IN_MULTI_WINDOW = "ReadAloudInMultiWindow";
    public static final String READALOUD_PLAYBACK = "ReadAloudPlayback";
    public static final String READALOUD_TAP_TO_SEEK = "ReadAloudTapToSeek";
    public static final String READALOUD_IPH_MENU_BUTTON_HIGHLIGHT_CCT =
            "ReadAloudIPHMenuButtonHighlightCCT";
    public static final String READER_MODE_IN_CCT = "ReaderModeInCCT";
    public static final String READING_LIST_ENABLE_SYNC_TRANSPORT_MODE_UPON_SIGNIN =
            "ReadingListEnableSyncTransportModeUponSignIn";
    public static final String RECORD_SUPPRESSION_METRICS = "RecordSuppressionMetrics";
    public static final String REDIRECT_EXPLICIT_CTA_INTENTS_TO_EXISTING_ACTIVITY =
            "RedirectExplicitCTAIntentsToExistingActivity";
    public static final String REENGAGEMENT_NOTIFICATION = "ReengagementNotification";
    public static final String RELATED_SEARCHES_SWITCH = "RelatedSearchesSwitch";
    public static final String RELATED_SEARCHES_ALL_LANGUAGE = "RelatedSearchesAllLanguage";
    public static final String RENAME_JOURNEYS = "RenameJourneys";
    public static final String REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS =
            "ReplaceSyncPromosWithSignInPromos";
    public static final String SAFETY_HUB = "SafetyHub";
    public static final String SAFETY_HUB_ANDROID_SURVEY = "SafetyHubAndroidSurvey";
    public static final String SAFETY_HUB_FOLLOWUP = "SafetyHubFollowup";
    public static final String SAFETY_HUB_MAGIC_STACK = "SafetyHubMagicStack";
    public static final String SAFE_BROWSING_DELAYED_WARNINGS = "SafeBrowsingDelayedWarnings";
    public static final String SAFE_BROWSING_EXTENDED_REPORTING_REMOVE_PREF_DEPENDENCY =
            "ExtendedReportingRemovePrefDependency";
    public static final String SEARCH_IN_CCT = "SearchInCCT";
    public static final String SEARCH_IN_CCT_ALTERNATE_TAP_HANDLING =
            "SearchInCCTAlternateTapHandling";
    public static final String SETTINGS_SINGLE_ACTIVITY = "SettingsSingleActivity";
    public static final String SHARE_CUSTOM_ACTIONS_IN_CCT = "ShareCustomActionsInCCT";
    public static final String SEARCH_RESUMPTION_MODULE_ANDROID = "SearchResumptionModuleAndroid";
    public static final String SEED_ACCOUNTS_REVAMP = "SeedAccountsRevamp";
    public static final String SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER =
            "SegmentationPlatformAndroidHomeModuleRanker";

    public static final String SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER_V2 =
            "SegmentationPlatformAndroidHomeModuleRankerV2";
    public static final String SEND_TAB_TO_SELF_V2 = "SendTabToSelfV2";
    public static final String SMALLER_TAB_STRIP_TITLE_LIMIT = "SmallerTabStripTitleLimit";
    public static final String SMART_SUGGESTION_FOR_LARGE_DOWNLOADS =
            "SmartSuggestionForLargeDownloads";
    public static final String SPLIT_CACHE_BY_NETWORK_ISOLATION_KEY =
            "SplitCacheByNetworkIsolationKey";
    public static final String START_SURFACE_RETURN_TIME = "StartSurfaceReturnTime";
    public static final String STOP_APP_INDEXING_REPORT = "StopAppIndexingReport";
    public static final String SUGGESTION_ANSWERS_COLOR_REVERSE = "SuggestionAnswersColorReverse";
    public static final String SUPPRESS_TOOLBAR_CAPTURES = "SuppressToolbarCaptures";
    public static final String SUPPRESS_TOOLBAR_CAPTURES_AT_GESTURE_END =
            "SuppressToolbarCapturesAtGestureEnd";
    public static final String ENABLE_BATCH_UPLOAD_FROM_SETTINGS = "EnableBatchUploadFromSettings";
    public static final String SYNC_ENABLE_CONTACT_INFO_DATA_TYPE_IN_TRANSPORT_MODE =
            "SyncEnableContactInfoDataTypeInTransportMode";
    public static final String TAB_DRAG_DROP_ANDROID = "TabDragDropAndroid";
    public static final String TAB_GROUP_CREATION_DIALOG_ANDROID = "TabGroupCreationDialogAndroid";
    public static final String TAB_GROUP_PANE_ANDROID = "TabGroupPaneAndroid";
    public static final String TAB_GROUP_PARITY_ANDROID = "TabGroupParityAndroid";
    public static final String TAB_GROUP_SYNC_ANDROID = "TabGroupSyncAndroid";
    public static final String TAB_GROUP_SYNC_AUTO_OPEN_KILL_SWITCH =
            "TabGroupSyncAutoOpenKillSwitch";
    public static final String TAB_RESUMPTION_MODULE_ANDROID = "TabResumptionModuleAndroid";
    public static final String TAB_STRIP_GROUP_COLLAPSE = "TabStripGroupCollapseAndroid";
    public static final String TAB_STRIP_GROUP_CONTEXT_MENU = "TabStripGroupContextMenuAndroid";
    public static final String TAB_STRIP_INCOGNITO_MIGRATION = "TabStripIncognitoMigration";
    public static final String TAB_STRIP_LAYOUT_OPTIMIZATION = "TabStripLayoutOptimization";
    public static final String TAB_STRIP_STARTUP_REFACTORING = "TabStripStartupRefactoring";
    public static final String TAB_STRIP_TRANSITION_IN_DESKTOP_WINDOW =
            "TabStripTransitionInDesktopWindow";
    public static final String TABLET_TAB_SWITCHER_LONG_PRESS_MENU =
            "TabletTabSwitcherLongPressMenu";
    public static final String TABLET_TOOLBAR_REORDERING = "TabletToolbarReordering";
    public static final String TAB_STATE_FLAT_BUFFER = "TabStateFlatBuffer";
    public static final String TAB_WINDOW_MANAGER_INDEX_REASSIGNMENT_ACTIVITY_FINISHING =
            "TabWindowManagerIndexReassignmentActivityFinishing";
    public static final String TAB_WINDOW_MANAGER_INDEX_REASSIGNMENT_ACTIVITY_IN_SAME_TASK =
            "TabWindowManagerIndexReassignmentActivityInSameTask";
    public static final String TAB_WINDOW_MANAGER_INDEX_REASSIGNMENT_ACTIVITY_NOT_IN_APP_TASKS =
            "TabWindowManagerIndexReassignmentActivityNotInAppTasks";
    public static final String TAB_WINDOW_MANAGER_REPORT_INDICES_MISMATCH =
            "TabWindowManagerReportIndicesMismatch";
    public static final String TEST_DEFAULT_DISABLED = "TestDefaultDisabled";
    public static final String TEST_DEFAULT_ENABLED = "TestDefaultEnabled";
    public static final String TINKER_TANK_BOTTOM_SHEET = "TinkerTankBottomSheet";
    public static final String TOOLBAR_PHONE_CLEANUP = "ToolbarPhoneCleanup";
    public static final String TOOLBAR_SCROLL_ABLATION = "AndroidToolbarScrollAblation";
    public static final String TRACE_BINDER_IPC = "TraceBinderIpc";
    public static final String TRACKING_PROTECTION_3PCD = "TrackingProtection3pcd";
    public static final String TRACKING_PROTECTION_3PCD_UX = "TrackingProtection3pcdUx";
    public static final String TRACKING_PROTECTION_USER_BYPASS_PWA =
            "TrackingProtectionUserBypassPwa";
    public static final String TRACKING_PROTECTION_USER_BYPASS_PWA_TRIGGER =
            "TrackingProtectionUserBypassPwaTrigger";
    public static final String TRANSLATE_MESSAGE_UI = "TranslateMessageUI";
    public static final String TRANSLATE_TFLITE = "TFLiteLanguageDetectionEnabled";
    public static final String
            UNIFIED_PASSWORD_MANAGER_LOCAL_PASSWORDS_ANDROID_ACCESS_LOSS_WARNING =
                    "UnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarning";
    public static final String UNIFIED_PASSWORD_MANAGER_LOCAL_PWD_MIGRATION_WARNING =
            "UnifiedPasswordManagerLocalPasswordsMigrationWarning";
    public static final String UNO_PHASE_2_FOLLOW_UP = "UnoPhase2FollowUp";
    public static final String USE_ALTERNATE_HISTORY_SYNC_ILLUSTRATION =
            "UseAlternateHistorySyncIllustration";
    public static final String USE_CHIME_ANDROID_SDK = "UseChimeAndroidSdk";
    public static final String USE_LIBUNWINDSTACK_NATIVE_UNWINDER_ANDROID =
            "UseLibunwindstackNativeUnwinderAndroid";
    public static final String VISITED_URL_RANKING_SERVICE = "VisitedURLRankingService";
    public static final String VOICE_SEARCH_AUDIO_CAPTURE_POLICY = "VoiceSearchAudioCapturePolicy";
    public static final String WEB_APK_ALLOW_ICON_UPDATE = "WebApkAllowIconUpdate";
    public static final String WEB_APK_BACKUP_AND_RESTORE_BACKEND = "WebApkBackupAndRestoreBackend";
    public static final String WEB_APK_INSTALL_FAILURE_NOTIFICATION =
            "WebApkInstallFailureNotification";
    public static final String WEB_APK_MIN_SHELL_APK_VERSION = "WebApkMinShellVersion";
    public static final String WEB_AUTHN_ENABLE_CABLE_AUTHENTICATOR =
            "WebAuthenticationEnableAndroidCableAuthenticator";
    public static final String WEB_FEED_AWARENESS = "WebFeedAwareness";
    public static final String WEB_FEED_ONBOARDING = "WebFeedOnboarding";
    public static final String WEB_FEED_SORT = "WebFeedSort";
    public static final String WEB_OTP_CROSS_DEVICE_SIMPLE_STRING = "WebOtpCrossDeviceSimpleString";
    public static final String XSURFACE_METRICS_REPORTING = "XsurfaceMetricsReporting";
    public static final String POST_GET_MEMORY_PRESSURE_TO_BACKGROUND =
            BaseFeatures.POST_GET_MY_MEMORY_STATE_TO_BACKGROUND;

    /* Alphabetical: */
    public static final CachedFlag sAccountReauthenticationRecentTimeWindow =
            newCachedFlag(ACCOUNT_REAUTHENTICATION_RECENT_TIME_WINDOW, true);
    public static final CachedFlag sAndroidAppIntegration =
            newCachedFlag(ANDROID_APP_INTEGRATION, false);
    public static final CachedFlag sAndroidAppIntegrationWithFavicon =
            newCachedFlag(ANDROID_APP_INTEGRATION_WITH_FAVICON, false);
    public static final CachedFlag sAndroidBottomToolbar =
            newCachedFlag(ANDROID_BOTTOM_TOOLBAR, false);
    public static final CachedFlag sAndroidElegantTextHeight =
            newCachedFlag(ANDROID_ELEGANT_TEXT_HEIGHT, false);
    public static final CachedFlag sAndroidGoogleSansText =
            newCachedFlag(ANDROID_GOOGLE_SANS_TEXT, true);
    public static final CachedFlag sAndroidHubFloatingActionButton =
            newCachedFlag(
                    ANDROID_HUB_FLOATING_ACTION_BUTTON,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sAndroidHubSearch =
            newCachedFlag(
                    ANDROID_HUB_SEARCH,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ false);
    public static final CachedFlag sAndroidHubV2 = newCachedFlag(ANDROID_HUB_V2, false);
    public static final CachedFlag sAndroidTabDeclutterDedupeTabIdsKillSwitch =
            newCachedFlag(ANDROID_TAB_DECLUTTER_DEDUPE_TAB_IDS_KILL_SWITCH, true);
    public static final CachedFlag sAppSpecificHistory = newCachedFlag(APP_SPECIFIC_HISTORY, true);
    public static final CachedFlag sAsyncNotificationManager =
            newCachedFlag(ASYNC_NOTIFICATION_MANAGER, false);
    public static final CachedFlag sBackGestureActivityTabProvider =
            newCachedFlag(BACK_GESTURE_ACTIVITY_TAB_PROVIDER, false);
    public static final CachedFlag sBackGestureMoveToBackDuringStartup =
            newCachedFlag(BACK_GESTURE_MOVE_TO_BACK_DURING_STARTUP, true);
    public static final CachedFlag sBackGestureRefactorAndroid =
            newCachedFlag(BACK_GESTURE_REFACTOR, true);
    public static final CachedFlag sBackToHomeAnimation =
            newCachedFlag(BACK_TO_HOME_ANIMATION, true);
    public static final CachedFlag sBlockIntentsWhileLocked =
            newCachedFlag(BLOCK_INTENTS_WHILE_LOCKED, false);
    public static final CachedFlag sCctAuthTab = newCachedFlag(CCT_AUTH_TAB, false);
    public static final CachedFlag sCctAuthTabDisableAllExternalIntents =
            newCachedFlag(CCT_AUTH_TAB_DISABLE_ALL_EXTERNAL_INTENTS, false);
    public static final CachedFlag sCctAutoTranslate = newCachedFlag(CCT_AUTO_TRANSLATE, true);
    public static final CachedFlag sCctFeatureUsage = newCachedFlag(CCT_FEATURE_USAGE, false);
    public static final CachedFlag sCctEphemeralMode = newCachedFlag(CCT_EPHEMERAL_MODE, false);
    public static final CachedFlag sCctIncognitoAvailableToThirdParty =
            newCachedFlag(CCT_INCOGNITO_AVAILABLE_TO_THIRD_PARTY, false);
    public static final CachedFlag sCctIntentFeatureOverrides =
            newCachedFlag(CCT_INTENT_FEATURE_OVERRIDES, true);
    public static final CachedFlag sCctMinimized = newCachedFlag(CCT_MINIMIZED, true);
    public static final CachedFlag sCctNavigationalPrefetch =
            newCachedFlag(CCT_NAVIGATIONAL_PREFETCH, false);
    public static final CachedFlag sCctGoogleBottomBar =
            newCachedFlag(CCT_GOOGLE_BOTTOM_BAR, false);
    public static final CachedFlag sCctGoogleBottomBarVariantLayouts =
            newCachedFlag(CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS, false);
    public static final CachedFlag sCctResizableForThirdParties =
            newCachedFlag(CCT_RESIZABLE_FOR_THIRD_PARTIES, true);
    public static final CachedFlag sCctRevampedBranding =
            newCachedFlag(CCT_REVAMPED_BRANDING, false);
    public static final CachedFlag sCctNestedSecurityIcon =
            newCachedFlag(CCT_NESTED_SECURITY_ICON, false);
    public static final CachedFlag sCctTabModalDialog = newCachedFlag(CCT_TAB_MODAL_DIALOG, true);
    public static final CachedFlag sClankStartupLatencyInjection =
            newCachedFlag(CLANK_STARTUP_LATENCY_INJECTION, false);
    public static final CachedFlag sCollectAndroidFrameTimelineMetrics =
            newCachedFlag(COLLECT_ANDROID_FRAME_TIMELINE_METRICS, false);
    public static final CachedFlag sCommandLineOnNonRooted =
            newCachedFlag(COMMAND_LINE_ON_NON_ROOTED, false);
    public static final CachedFlag sCrossDeviceTabPaneAndroid =
            newCachedFlag(CROSS_DEVICE_TAB_PANE_ANDROID, false);
    public static final CachedFlag sDelayTempStripRemoval =
            newCachedFlag(DELAY_TEMP_STRIP_REMOVAL, false);
    public static final CachedFlag sDisableInstanceLimit =
            newCachedFlag(DISABLE_INSTANCE_LIMIT, false);
    public static final CachedFlag sDragDropIntoOmnibox =
            newCachedFlag(DRAG_DROP_INTO_OMNIBOX, false);
    public static final CachedFlag sDownloadsMigrateToJobsAPI =
            newCachedFlag(DOWNLOADS_MIGRATE_TO_JOBS_API, false);
    public static final CachedFlag sDrawEdgeToEdge = newCachedFlag(DRAW_EDGE_TO_EDGE, false);
    public static final CachedFlag sDrawKeyNativeEdgeToEdge =
            newCachedFlag(DRAW_KEY_NATIVE_EDGE_TO_EDGE, false);
    public static final CachedFlag sDrawNativeEdgeToEdge =
            newCachedFlag(DRAW_NATIVE_EDGE_TO_EDGE, false);
    public static final CachedFlag sEdgeToEdgeBottomChin =
            newCachedFlag(EDGE_TO_EDGE_BOTTOM_CHIN, false);
    public static final CachedFlag sEdgeToEdgeEverywhere =
            newCachedFlag(EDGE_TO_EDGE_EVERYWHERE, false);
    public static final CachedFlag sEdgeToEdgeWebOptIn =
            newCachedFlag(EDGE_TO_EDGE_WEB_OPT_IN, false);
    public static final CachedFlag sEducationalTipModule =
            newCachedFlag(EDUCATIONAL_TIP_MODULE, false);
    public static final CachedFlag sEnableDiscountInfoApi =
            newCachedFlag(ENABLE_DISCOUNT_INFO_API, false);
    public static final CachedFlag sExperimentsForAgsa = newCachedFlag(EXPERIMENTS_FOR_AGSA, true);
    public static final CachedFlag sForceListTabSwitcher =
            newCachedFlag(FORCE_LIST_TAB_SWITCHER, false);
    public static final CachedFlag sFullscreenInsetsApiMigration =
            newCachedFlag(FULLSCREEN_INSETS_API_MIGRATION, false);
    public static final CachedFlag sFullscreenInsetsApiMigrationOnAutomotive =
            newCachedFlag(FULLSCREEN_INSETS_API_MIGRATION_ON_AUTOMOTIVE, true);
    public static final CachedFlag sLockBackPressHandlerAtStart =
            newCachedFlag(LOCK_BACK_PRESS_HANDLER_AT_START, true);
    public static final CachedFlag sIncognitoReauthenticationForAndroid =
            newCachedFlag(INCOGNITO_REAUTHENTICATION_FOR_ANDROID, true);
    public static final CachedFlag sLogoPolish = newCachedFlag(LOGO_POLISH, true);
    public static final CachedFlag sLogoPolishAnimationKillSwitch =
            newCachedFlag(LOGO_POLISH_ANIMATION_KILL_SWITCH, true);
    public static final CachedFlag sMagicStackAndroid = newCachedFlag(MAGIC_STACK_ANDROID, true);
    public static final CachedFlag sMostVisitedTilesReselect =
            newCachedFlag(MOST_VISITED_TILES_RESELECT, false);
    public static final CachedFlag sMultiInstanceApplicationStatusCleanup =
            newCachedFlag(MUlTI_INSTANCE_APPLICATION_STATUS_CLEANUP, false);
    public static final CachedFlag sNavBarColorMatchesTabBackground =
            newCachedFlag(NAV_BAR_COLOR_MATCHES_TAB_BACKGROUND, true);
    public static final CachedFlag sNewTabPageAndroidTriggerForPrerender2 =
            newCachedFlag(NEW_TAB_PAGE_ANDROID_TRIGGER_FOR_PRERENDER2, false);
    public static final CachedFlag sPriceChangeModule = newCachedFlag(PRICE_CHANGE_MODULE, true);
    public static final CachedFlag sPriceInsights = newCachedFlag(PRICE_INSIGHTS, false);
    public static final CachedFlag sPrivacyGuideAndroid3 =
            newCachedFlag(PRIVACY_GUIDE_ANDROID_3, false);
    public static final CachedFlag sPrivacyGuidePreloadAndroid =
            newCachedFlag(PRIVACY_GUIDE_PRELOAD_ANDROID, false);
    public static final CachedFlag sOptimizationGuidePushNotifications =
            newCachedFlag(OPTIMIZATION_GUIDE_PUSH_NOTIFICATIONS, false);
    public static final CachedFlag sPaintPreviewDemo = newCachedFlag(PAINT_PREVIEW_DEMO, false);
    public static final CachedFlag sPostGetMyMemoryStateToBackground =
            newCachedFlag(POST_GET_MEMORY_PRESSURE_TO_BACKGROUND, false);
    public static final CachedFlag sPrefetchBrowserInitiatedTriggers =
            newCachedFlag(PREFETCH_BROWSER_INITIATED_TRIGGERS, false);
    public static final CachedFlag sRedirectExplicitCTAIntentsToExistingActivity =
            newCachedFlag(REDIRECT_EXPLICIT_CTA_INTENTS_TO_EXISTING_ACTIVITY, true);
    public static final CachedFlag sSafetyHubMagicStack =
            newCachedFlag(SAFETY_HUB_MAGIC_STACK, false);
    public static final CachedFlag sSearchInCCT = newCachedFlag(SEARCH_IN_CCT, false);
    public static final CachedFlag sSearchInCCTAlternateTapHandling =
            newCachedFlag(SEARCH_IN_CCT_ALTERNATE_TAP_HANDLING, false);
    public static final CachedFlag sSettingsSingleActivity =
            newCachedFlag(SETTINGS_SINGLE_ACTIVITY, true);
    public static final CachedFlag sSmallerTabStripTitleLimit =
            newCachedFlag(SMALLER_TAB_STRIP_TITLE_LIMIT, true);
    public static final CachedFlag sStartSurfaceReturnTime =
            newCachedFlag(START_SURFACE_RETURN_TIME, true);
    public static final CachedFlag sTabDragDropAsWindowAndroid =
            newCachedFlag(TAB_DRAG_DROP_ANDROID, false);
    public static final CachedFlag sTabGroupCreationDialogAndroid =
            newCachedFlag(TAB_GROUP_CREATION_DIALOG_ANDROID, false);
    public static final CachedFlag sTabGroupPaneAndroid =
            newCachedFlag(TAB_GROUP_PANE_ANDROID, false);
    public static final CachedFlag sTabGroupParityAndroid =
            newCachedFlag(TAB_GROUP_PARITY_ANDROID, true);
    public static final CachedFlag sTabResumptionModuleAndroid =
            newCachedFlag(TAB_RESUMPTION_MODULE_ANDROID, false);
    public static final CachedFlag sTabStateFlatBuffer =
            newCachedFlag(TAB_STATE_FLAT_BUFFER, false);
    public static final CachedFlag sTabStripIncognitoMigration =
            newCachedFlag(TAB_STRIP_INCOGNITO_MIGRATION, false);
    public static final CachedFlag sTabStripLayoutOptimization =
            newCachedFlag(TAB_STRIP_LAYOUT_OPTIMIZATION, false);
    public static final CachedFlag sTabStripStartupRefactoring =
            newCachedFlag(TAB_STRIP_STARTUP_REFACTORING, true);
    public static final CachedFlag sTabletToolbarReordering =
            newCachedFlag(TABLET_TOOLBAR_REORDERING, false);
    public static final CachedFlag sTabStripGroupCollapse =
            newCachedFlag(TAB_STRIP_GROUP_COLLAPSE, false);
    public static final CachedFlag sTabWindowManagerIndexReassignmentActivityFinishing =
            newCachedFlag(TAB_WINDOW_MANAGER_INDEX_REASSIGNMENT_ACTIVITY_FINISHING, true);
    public static final CachedFlag sTabWindowManagerIndexReassignmentActivityInSameTask =
            newCachedFlag(TAB_WINDOW_MANAGER_INDEX_REASSIGNMENT_ACTIVITY_IN_SAME_TASK, true);
    public static final CachedFlag sTabWindowManagerIndexReassignmentActivityNotInAppTasks =
            newCachedFlag(TAB_WINDOW_MANAGER_INDEX_REASSIGNMENT_ACTIVITY_NOT_IN_APP_TASKS, true);
    public static final CachedFlag sTabWindowManagerReportIndicesMismatch =
            newCachedFlag(TAB_WINDOW_MANAGER_REPORT_INDICES_MISMATCH, true);
    public static final CachedFlag sTestDefaultDisabled =
            newCachedFlag(TEST_DEFAULT_DISABLED, false);
    public static final CachedFlag sTestDefaultEnabled = newCachedFlag(TEST_DEFAULT_ENABLED, true);
    public static final CachedFlag sTraceBinderIpc =
            newCachedFlag(
                    TRACE_BINDER_IPC, /* defaultValue= */ false, /* defaultValueInTests= */ true);
    public static final CachedFlag sUseChimeAndroidSdk =
            newCachedFlag(USE_CHIME_ANDROID_SDK, false);
    public static final CachedFlag sUseLibunwindstackNativeUnwinderAndroid =
            newCachedFlag(USE_LIBUNWINDSTACK_NATIVE_UNWINDER_ANDROID, true);
    public static final CachedFlag sWebApkMinShellApkVersion =
            newCachedFlag(WEB_APK_MIN_SHELL_APK_VERSION, true);

    public static final List<CachedFlag> sFlagsCachedFullBrowser =
            List.of(
                    sAccountReauthenticationRecentTimeWindow,
                    sAndroidAppIntegration,
                    sAndroidAppIntegrationWithFavicon,
                    sAndroidBottomToolbar,
                    sAndroidElegantTextHeight,
                    sAndroidGoogleSansText,
                    sAndroidHubFloatingActionButton,
                    sAndroidHubSearch,
                    sAndroidHubV2,
                    sAndroidTabDeclutterDedupeTabIdsKillSwitch,
                    sAppSpecificHistory,
                    sAsyncNotificationManager,
                    sBackGestureActivityTabProvider,
                    sBackGestureMoveToBackDuringStartup,
                    sBackGestureRefactorAndroid,
                    sBackToHomeAnimation,
                    sBlockIntentsWhileLocked,
                    sCctAuthTab,
                    sCctAuthTabDisableAllExternalIntents,
                    sCctAutoTranslate,
                    sCctEphemeralMode,
                    sCctFeatureUsage,
                    sCctIncognitoAvailableToThirdParty,
                    sCctIntentFeatureOverrides,
                    sCctMinimized,
                    sCctNavigationalPrefetch,
                    sCctGoogleBottomBar,
                    sCctGoogleBottomBarVariantLayouts,
                    sCctResizableForThirdParties,
                    sCctRevampedBranding,
                    sCctNestedSecurityIcon,
                    sCctTabModalDialog,
                    sClankStartupLatencyInjection,
                    sCollectAndroidFrameTimelineMetrics,
                    sCommandLineOnNonRooted,
                    sCrossDeviceTabPaneAndroid,
                    sDelayTempStripRemoval,
                    sDisableInstanceLimit,
                    sDragDropIntoOmnibox,
                    sDownloadsMigrateToJobsAPI,
                    sDrawEdgeToEdge,
                    sDrawKeyNativeEdgeToEdge,
                    sDrawNativeEdgeToEdge,
                    sEdgeToEdgeBottomChin,
                    sEdgeToEdgeEverywhere,
                    sEdgeToEdgeWebOptIn,
                    sEducationalTipModule,
                    sEnableDiscountInfoApi,
                    sForceListTabSwitcher,
                    sFullscreenInsetsApiMigration,
                    sFullscreenInsetsApiMigrationOnAutomotive,
                    sIncognitoReauthenticationForAndroid,
                    sLockBackPressHandlerAtStart,
                    sLogoPolish,
                    sLogoPolishAnimationKillSwitch,
                    sMagicStackAndroid,
                    sMostVisitedTilesReselect,
                    sMultiInstanceApplicationStatusCleanup,
                    sNavBarColorMatchesTabBackground,
                    sNewTabPageAndroidTriggerForPrerender2,
                    sPriceChangeModule,
                    sPriceInsights,
                    sPrivacyGuideAndroid3,
                    sPrivacyGuidePreloadAndroid,
                    sOptimizationGuidePushNotifications,
                    sPaintPreviewDemo,
                    sPostGetMyMemoryStateToBackground,
                    sPrefetchBrowserInitiatedTriggers,
                    sRedirectExplicitCTAIntentsToExistingActivity,
                    sSafetyHubMagicStack,
                    sSearchInCCT,
                    sSearchInCCTAlternateTapHandling,
                    sSettingsSingleActivity,
                    sSmallerTabStripTitleLimit,
                    sStartSurfaceReturnTime,
                    sTabDragDropAsWindowAndroid,
                    sTabGroupCreationDialogAndroid,
                    sTabGroupPaneAndroid,
                    sTabGroupParityAndroid,
                    sTabResumptionModuleAndroid,
                    sTabStateFlatBuffer,
                    sTabStripGroupCollapse,
                    sTabStripIncognitoMigration,
                    sTabStripLayoutOptimization,
                    sTabStripStartupRefactoring,
                    sTabletToolbarReordering,
                    sTabWindowManagerIndexReassignmentActivityFinishing,
                    sTabWindowManagerIndexReassignmentActivityInSameTask,
                    sTabWindowManagerIndexReassignmentActivityNotInAppTasks,
                    sTabWindowManagerReportIndicesMismatch,
                    sTraceBinderIpc,
                    sUseChimeAndroidSdk,
                    sUseLibunwindstackNativeUnwinderAndroid,
                    sWebApkMinShellApkVersion);

    public static final List<CachedFlag> sFlagsCachedInMinimalBrowser =
            List.of(sExperimentsForAgsa);

    public static final List<CachedFlag> sTestCachedFlags =
            List.of(sTestDefaultDisabled, sTestDefaultEnabled);

    public static final Map<String, CachedFlag> sAllCachedFlags =
            CachedFlag.createCachedFlagMap(
                    List.of(
                            sFlagsCachedFullBrowser,
                            sFlagsCachedInMinimalBrowser,
                            sTestCachedFlags));

    // MutableFlagWithSafeDefault instances.
    /* Alphabetical: */
    public static final MutableFlagWithSafeDefault sAdaptiveButtonInTopToolbarCustomizationV2 =
            newMutableFlagWithSafeDefault(ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2, false);
    public static final MutableFlagWithSafeDefault sAndroidTabDeclutter =
            newMutableFlagWithSafeDefault(ANDROID_TAB_DECLUTTER, false);
    public static final MutableFlagWithSafeDefault sAndroidTabDeclutterArchiveAllButActiveTab =
            newMutableFlagWithSafeDefault(ANDROID_TAB_DECLUTTER_ARCHIVE_ALL_BUT_ACTIVE, false);
    public static final MutableFlagWithSafeDefault sAndroidTabDeclutterRescueKillSwitch =
            newMutableFlagWithSafeDefault(ANDROID_TAB_DECLUTTER_RESCUE_KILLSWITCH, true);
    public static final MutableFlagWithSafeDefault sBottomBrowserControlsRefactor =
            newMutableFlagWithSafeDefault(BOTTOM_BROWSER_CONTROLS_REFACTOR, true);
    public static final MutableFlagWithSafeDefault sBcivPhoneOnly =
            newMutableFlagWithSafeDefault(BCIV_PHONE_ONLY, false);
    public static final MutableFlagWithSafeDefault sBcivWithSuppression =
            newMutableFlagWithSafeDefault(BCIV_WITH_SUPPRESSION, false);
    public static final MutableFlagWithSafeDefault sBcivZeroBrowserFrames =
            newMutableFlagWithSafeDefault(BCIV_ZERO_BROWSER_FRAMES, false);
    public static final MutableFlagWithSafeDefault sBrowserControlsInViz =
            newMutableFlagWithSafeDefault(BROWSER_CONTROLS_IN_VIZ, true);
    public static final MutableFlagWithSafeDefault sBrowserControlsEarlyResize =
            newMutableFlagWithSafeDefault(BROWSER_CONTROLS_EARLY_RESIZE, false);
    public static final MutableFlagWithSafeDefault sForceBrowserControlsUponExitingFullscreen =
            newMutableFlagWithSafeDefault(FORCE_BROWSER_CONTROLS_UPON_EXITING_FULLSCREEN, true);
    // Defaulted to true in native, but since it is being used as a kill switch set the default
    // value pre-native to false as it is safer if the feature needs to be killed via Finch config.
    public static final MutableFlagWithSafeDefault sGtsCloseTabAnimationKillSwitch =
            newMutableFlagWithSafeDefault(GTS_CLOSE_TAB_ANIMATION_KILL_SWITCH, false);
    public static final MutableFlagWithSafeDefault sIncognitoScreenshot =
            newMutableFlagWithSafeDefault(INCOGNITO_SCREENSHOT, false);
    public static final MutableFlagWithSafeDefault sNoVisibleHintForDifferentTLD =
            newMutableFlagWithSafeDefault(ANDROID_NO_VISIBLE_HINT_FOR_DIFFERENT_TLD, true);
    public static final MutableFlagWithSafeDefault sQuickDeleteForAndroid =
            newMutableFlagWithSafeDefault(QUICK_DELETE_FOR_ANDROID, true);
    public static final MutableFlagWithSafeDefault sQuickDeleteAndroidFollowup =
            newMutableFlagWithSafeDefault(QUICK_DELETE_ANDROID_FOLLOWUP, false);
    public static final MutableFlagWithSafeDefault sQuickDeleteAndroidSurvey =
            newMutableFlagWithSafeDefault(QUICK_DELETE_ANDROID_SURVEY, false);
    public static final MutableFlagWithSafeDefault sReadAloudTapToSeek =
            newMutableFlagWithSafeDefault(READALOUD_TAP_TO_SEEK, false);
    public static final MutableFlagWithSafeDefault sReaderModeCct =
            newMutableFlagWithSafeDefault(READER_MODE_IN_CCT, false);
    public static final MutableFlagWithSafeDefault sRecordSuppressionMetrics =
            newMutableFlagWithSafeDefault(RECORD_SUPPRESSION_METRICS, true);
    public static final MutableFlagWithSafeDefault sSafetyHub =
            newMutableFlagWithSafeDefault(SAFETY_HUB, false);
    public static final MutableFlagWithSafeDefault sSafetyHubAndroidSurvey =
            newMutableFlagWithSafeDefault(SAFETY_HUB_ANDROID_SURVEY, false);
    public static final MutableFlagWithSafeDefault sSafetyHubFollowup =
            newMutableFlagWithSafeDefault(SAFETY_HUB_FOLLOWUP, false);
    public static final MutableFlagWithSafeDefault sSuppressionToolbarCaptures =
            newMutableFlagWithSafeDefault(SUPPRESS_TOOLBAR_CAPTURES, false);
    public static final MutableFlagWithSafeDefault sSuppressToolbarCapturesAtGestureEnd =
            newMutableFlagWithSafeDefault(SUPPRESS_TOOLBAR_CAPTURES_AT_GESTURE_END, false);
    public static final MutableFlagWithSafeDefault sToolbarScrollAblation =
            newMutableFlagWithSafeDefault(TOOLBAR_SCROLL_ABLATION, false);
    public static final MutableFlagWithSafeDefault sVoiceSearchAudioCapturePolicy =
            newMutableFlagWithSafeDefault(VOICE_SEARCH_AUDIO_CAPTURE_POLICY, false);

    // Mutable*ParamWithSafeDefault instances.
    /* Alphabetical: */
    public static final MutableBooleanParamWithSafeDefault sShouldBlockCapturesForFullscreenParam =
            sSuppressionToolbarCaptures.newBooleanParam("block_for_fullscreen", false);
    public static final MutableBooleanParamWithSafeDefault sAndroidTabDeclutterArchiveEnabled =
            sAndroidTabDeclutter.newBooleanParam("android_tab_declutter_archive_enabled", true);
    public static final MutableIntParamWithSafeDefault sAndroidTabDeclutterArchiveTimeDeltaHours =
            sAndroidTabDeclutter.newIntParam(
                    "android_tab_declutter_archive_time_delta_hours", 7 * 24);
    public static final MutableBooleanParamWithSafeDefault sAndroidTabDeclutterAutoDeleteEnabled =
            sAndroidTabDeclutter.newBooleanParam(
                    "android_tab_declutter_auto_delete_enabled", false);
    public static final MutableIntParamWithSafeDefault
            sAndroidTabDeclutterAutoDeleteTimeDeltaHours =
                    sAndroidTabDeclutter.newIntParam(
                            "android_tab_declutter_auto_delete_time_delta_hours", 60 * 24);
    public static final MutableIntParamWithSafeDefault sAndroidTabDeclutterIntervalTimeDeltaHours =
            sAndroidTabDeclutter.newIntParam(
                    "android_tab_declutter_interval_time_delta_hours", 7 * 24);
    public static final MutableIntParamWithSafeDefault
            sAndroidTabDeclutterIphMessageDismissThreshold =
                    sAndroidTabDeclutter.newIntParam(
                            "android_tab_declutter_iph_message_dismiss_threshold", 3);

    public static final MutableBooleanParamWithSafeDefault
            sDisableBottomControlsStackerYOffsetDispatching =
                    sBottomBrowserControlsRefactor.newBooleanParam(
                            "disable_bottom_controls_stacker_y_offset", true);
}
