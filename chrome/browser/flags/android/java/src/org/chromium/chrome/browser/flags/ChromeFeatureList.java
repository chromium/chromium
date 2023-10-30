// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import org.chromium.base.FeatureMap;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;

import java.util.List;
import java.util.Map;

/**
 * A list of feature flags exposed to Java.
 *
 * This class lists flags exposed to Java as String constants. They should match
 * |kFeaturesExposedToJava| in chrome/browser/flags/android/chrome_feature_list.cc.
 *
 * This class also provides convenience methods to access values of flags and their field trial
 * parameters through {@link ChromeFeatureMap}.
 *
 * Chrome-layer {@link CachedFlag}s are instantiated here as well.
 */
public abstract class ChromeFeatureList {
    /** Prevent instantiation. */
    private ChromeFeatureList() {}

    /**
     * Convenience method to check Chrome-layer feature flags, see
     * {@link FeatureMap#isEnabled(String)}}.
     *
     * Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in chrome/browser/flags/android/chrome_feature_list.cc
     */
    public static boolean isEnabled(String featureName) {
        return ChromeFeatureMap.isEnabled(featureName);
    }

    /**
     * Convenience method to get Chrome-layer feature field trial params, see
     * {@link FeatureMap#getFieldTrialParamByFeature(String, String)}}.
     *
     * Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in chrome/browser/flags/android/chrome_feature_list.cc
     */
    public static String getFieldTrialParamByFeature(String featureName, String paramName) {
        return ChromeFeatureMap.getInstance().getFieldTrialParamByFeature(featureName, paramName);
    }

    /**
     * Convenience method to get Chrome-layer feature field trial params, see
     * {@link FeatureMap#getFieldTrialParamByFeatureAsBoolean(String, boolean)}}.
     *
     * Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in chrome/browser/flags/android/chrome_feature_list.cc
     */
    public static boolean getFieldTrialParamByFeatureAsBoolean(
            String featureName, String paramName, boolean defaultValue) {
        return ChromeFeatureMap.getInstance().getFieldTrialParamByFeatureAsBoolean(
                featureName, paramName, defaultValue);
    }

    /**
     * Convenience method to get Chrome-layer feature field trial params, see
     * {@link FeatureMap#getFieldTrialParamByFeatureAsInt(String, String, int)}}.
     *
     * Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in chrome/browser/flags/android/chrome_feature_list.cc
     */
    public static int getFieldTrialParamByFeatureAsInt(
            String featureName, String paramName, int defaultValue) {
        return ChromeFeatureMap.getInstance().getFieldTrialParamByFeatureAsInt(
                featureName, paramName, defaultValue);
    }

    /**
     * Convenience method to get Chrome-layer feature field trial params, see
     * {@link FeatureMap#getFieldTrialParamByFeatureAsDouble(String, String, double)}}.
     *
     * Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in chrome/browser/flags/android/chrome_feature_list.cc
     */
    public static double getFieldTrialParamByFeatureAsDouble(
            String featureName, String paramName, double defaultValue) {
        return ChromeFeatureMap.getInstance().getFieldTrialParamByFeatureAsDouble(
                featureName, paramName, defaultValue);
    }

    /**
     * Convenience method to get Chrome-layer feature field trial params, see
     * {@link FeatureMap#getFieldTrialParamsForFeature(String)}}.
     *
     * Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in chrome/browser/flags/android/chrome_feature_list.cc
     */
    public static Map<String, String> getFieldTrialParamsForFeature(String featureName) {
        return ChromeFeatureMap.getInstance().getFieldTrialParamsForFeature(featureName);
    }

    /* Alphabetical: */
    public static final String ADAPTIVE_BUTTON_IN_TOP_TOOLBAR = "AdaptiveButtonInTopToolbar";
    public static final String ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_TRANSLATE =
            "AdaptiveButtonInTopToolbarTranslate";
    public static final String ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_ADD_TO_BOOKMARKS =
            "AdaptiveButtonInTopToolbarAddToBookmarks";
    public static final String ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2 =
            "AdaptiveButtonInTopToolbarCustomizationV2";
    public static final String ADD_TO_HOMESCREEN_IPH = "AddToHomescreenIPH";
    public static final String ADVANCED_PERIPHERALS_SUPPORT = "AdvancedPeripheralsSupport";
    public static final String ADVANCED_PERIPHERALS_SUPPORT_TAB_STRIP =
            "AdvancedPeripheralsSupportTabStrip";
    public static final String ALLOW_NEW_INCOGNITO_TAB_INTENTS = "AllowNewIncognitoTabIntents";
    public static final String ANDROID_APP_INTEGRATION = "AndroidAppIntegration";
    public static final String ANDROID_APP_INTEGRATION_SAFE_SEARCH =
            "AndroidAppIntegrationSafeSearch";
    public static final String ANDROID_HATS_REFACTOR = "AndroidHatsRefactor";
    public static final String ANDROID_HUB = "AndroidHub";
    public static final String ANDROID_SEARCH_ENGINE_CHOICE_NOTIFICATION =
            "AndroidSearchEngineChoiceNotification";
    public static final String ANDROID_IMPROVED_BOOKMARKS = "AndroidImprovedBookmarks";
    public static final String ANDROID_VISIBLE_URL_TRUNCATION = "AndroidVisibleUrlTruncation";
    public static final String ANDROID_NO_VISIBLE_HINT_FOR_TABLETS =
            "AndroidNoVisibleHintForTablets";
    public static final String ANIMATED_IMAGE_DRAG_SHADOW = "AnimatedImageDragShadow";
    public static final String APP_MENU_MOBILE_SITE_OPTION = "AppMenuMobileSiteOption";
    public static final String AUTOFILL_ACCOUNT_PROFILE_STORAGE = "AutofillAccountProfileStorage";
    public static final String AUTOFILL_ADDRESS_PROFILE_SAVE_PROMPT_NICKNAME_SUPPORT =
            "AutofillAddressProfileSavePromptNicknameSupport";
    public static final String AUTOFILL_ALLOW_NON_HTTP_ACTIVATION =
            "AutofillAllowNonHttpActivation";
    public static final String AUTOFILL_ENABLE_CARD_ART_IMAGE = "AutofillEnableCardArtImage";
    public static final String AUTOFILL_ENABLE_CARD_PRODUCT_NAME = "AutofillEnableCardProductName";
    public static final String AUTOFILL_ENABLE_MOVING_GPAY_LOGO_TO_THE_RIGHT_ON_CLANK =
            "AutofillEnableMovingGPayLogoToTheRightOnClank";
    public static final String AUTOFILL_ENABLE_NEW_CARD_ART_AND_NETWORK_IMAGES =
            "AutofillEnableNewCardArtAndNetworkImages";
    public static final String AUTOFILL_ENABLE_CARD_ART_SERVER_SIDE_STRETCHING =
            "AutofillEnableCardArtServerSideStretching";
    public static final String AUTOFILL_ENABLE_CVC_STORAGE = "AutofillEnableCvcStorageAndFilling";
    public static final String AUTOFILL_ENABLE_PAYMENTS_MANDATORY_REAUTH =
            "AutofillEnablePaymentsMandatoryReauth";
    public static final String AUTOFILL_ENABLE_RANKING_FORMULA_ADDRESS_PROFILES =
            "AutofillEnableRankingFormulaAddressProfiles";
    public static final String AUTOFILL_ENABLE_RANKING_FORMULA_CREDIT_CARDS =
            "AutofillEnableRankingFormulaCreditCards";
    public static final String AUTOFILL_ENABLE_SUPPORT_FOR_HONORIFIC_PREFIXES =
            "AutofillEnableSupportForHonorificPrefixes";
    public static final String AUTOFILL_ENABLE_UPDATE_VIRTUAL_CARD_ENROLLMENT =
            "AutofillEnableUpdateVirtualCardEnrollment";
    public static final String AUTOFILL_ENABLE_VIRTUAL_CARD_METADATA =
            "AutofillEnableVirtualCardMetadata";
    public static final String AUTOFILL_KEYBOARD_ACCESSORY = "AutofillKeyboardAccessory_LAUNCHED";
    public static final String AUTOFILL_TOUCH_TO_FILL_FOR_CREDIT_CARDS_ANDROID =
            "AutofillTouchToFillForCreditCardsAndroid";
    public static final String AUTOFILL_VIRTUAL_VIEW_STRUCTURE_ANDROID =
            "AutofillVirtualViewStructureAndroid";
    public static final String AVOID_SELECTED_TAB_FOCUS_ON_LAYOUT_DONE_SHOWING =
            "AvoidSelectedTabFocusOnLayoutDoneShowing";
    public static final String BACKGROUND_THREAD_POOL = "BackgroundThreadPool";
    public static final String BACK_FORWARD_CACHE = "BackForwardCache";
    public static final String BACK_FORWARD_TRANSITIONS = "BackForwardTransitions";
    public static final String BACK_GESTURE_ACTIVITY_TAB_PROVIDER =
            "BackGestureActivityTabProvider";
    public static final String BACK_GESTURE_REFACTOR = "BackGestureRefactorAndroid";
    public static final String BACK_GESTURE_REFACTOR_ACTIVITY =
            "BackGestureRefactorActivityAndroid";
    public static final String BASELINE_GM3_SURFACE_COLORS = "BaselineGM3SurfaceColors";
    public static final String BLOCK_INTENTS_WHILE_LOCKED = "BlockIntentsWhileLocked";
    public static final String BOOKMARKS_REFRESH = "BookmarksRefresh";
    public static final String CACHE_ACTIVITY_TASKID = "CacheActivityTaskID";
    public static final String CAPTIVE_PORTAL_CERTIFICATE_LIST = "CaptivePortalCertificateList";
    public static final String CCT_AUTO_TRANSLATE = "CCTAutoTranslate";
    public static final String CCT_BOTTOM_BAR_SWIPE_UP_GESTURE = "CCTBottomBarSwipeUpGesture";
    public static final String CCT_BRAND_TRANSPARENCY = "CCTBrandTransparency";
    public static final String CCT_BRAND_TRANSPARENCY_MEMORY_IMPROVEMENT =
            "CCTBrandTransparencyMemoryImprovement";
    public static final String CCT_CLIENT_DATA_HEADER = "CCTClientDataHeader";
    public static final String CCT_DEPRECATED_APIs = "CCTDeprecatedAPIs";
    public static final String CCT_FEATURE_USAGE = "CCTFeatureUsage";
    public static final String CCT_INCOGNITO_AVAILABLE_TO_THIRD_PARTY =
            "CCTIncognitoAvailableToThirdParty";
    public static final String CCT_MINIMIZED = "CCTMinimized";
    public static final String CCT_INTENT_FEATURE_OVERRIDES = "CCTIntentFeatureOverrides";
    public static final String CCT_NEW_DOWNLOAD_TAB = "CCTNewDownloadTab";
    public static final String CCT_PAGE_INSIGHTS_HUB = "CCTPageInsightsHub";
    public static final String CCT_POST_MESSAGE_API = "CCTPostMessageAPI";
    public static final String CCT_PREFETCH_DELAY_SHOW_ON_START = "CCTPrefetchDelayShowOnStart";
    public static final String CCT_PREVENT_TOUCHES = "CCTPreventTouches";
    public static final String CCT_REAL_TIME_ENGAGEMENT_SIGNALS = "CCTRealTimeEngagementSignals";
    public static final String CCT_REAL_TIME_ENGAGEMENT_SIGNALS_ALTERNATIVE_IMPL =
            "CCTRealTimeEngagementSignalsAlternativeImpl";
    public static final String CCT_REDIRECT_PRECONNECT = "CCTRedirectPreconnect";
    public static final String CCT_REMOVE_REMOTE_VIEW_IDS = "CCTRemoveRemoteViewIds";
    public static final String CCT_REPORT_PARALLEL_REQUEST_STATUS =
            "CCTReportParallelRequestStatus";
    public static final String CCT_RESIZABLE_90_MAXIMUM_HEIGHT = "CCTResizable90MaximumHeight";
    public static final String CCT_RESIZABLE_ALLOW_RESIZE_BY_USER_GESTURE =
            "CCTResizableAllowResizeByUserGesture";
    public static final String CCT_RESIZABLE_FOR_THIRD_PARTIES = "CCTResizableForThirdParties";
    public static final String CCT_RESIZABLE_SIDE_SHEET = "CCTResizableSideSheet";
    public static final String CCT_RESIZABLE_SIDE_SHEET_FOR_THIRD_PARTIES =
            "CCTResizableSideSheetForThirdParties";
    public static final String CCT_RESOURCE_PREFETCH = "CCTResourcePrefetch";
    public static final String CCT_TEXT_FRAGMENT_LOOKUP_API_ENABLED =
            "CCTTextFragmentLookupApiEnabled";
    public static final String CCT_TOOLBAR_CUSTOMIZATIONS = "CCTToolbarCustomizations";
    public static final String CHROME_SHARING_HUB = "ChromeSharingHub";
    public static final String CHROME_SURVEY_NEXT_ANDROID = "ChromeSurveyNextAndroid";
    public static final String CLEAR_OMNIBOX_FOCUS_AFTER_NAVIGATION =
            "ClearOmniboxFocusAfterNavigation";
    public static final String CLOSE_TAB_SUGGESTIONS = "CloseTabSuggestions";
    public static final String CLOSE_TAB_SAVE_TAB_LIST = "CloseTabSaveTabList";
    public static final String COLLECT_ANDROID_FRAME_TIMELINE_METRICS =
            "CollectAndroidFrameTimelineMetrics";
    public static final String COMMAND_LINE_ON_NON_ROOTED = "CommandLineOnNonRooted";
    public static final String COMMERCE_MERCHANT_VIEWER = "CommerceMerchantViewer";
    public static final String COMMERCE_PRICE_TRACKING = "CommercePriceTracking";
    public static final String CONTEXTUAL_PAGE_ACTIONS = "ContextualPageActions";
    public static final String CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING =
            "ContextualPageActionPriceTracking";
    public static final String CONTEXTUAL_PAGE_ACTION_READER_MODE =
            "ContextualPageActionReaderMode";
    public static final String CONTEXTUAL_SEARCH_DISABLE_ONLINE_DETECTION =
            "ContextualSearchDisableOnlineDetection";
    public static final String CONTEXTUAL_SEARCH_FORCE_CAPTION = "ContextualSearchForceCaption";
    public static final String CONTEXTUAL_SEARCH_SUPPRESS_SHORT_VIEW =
            "ContextualSearchSuppressShortView";
    public static final String CONTEXTUAL_SEARCH_THIN_WEB_VIEW_IMPLEMENTATION =
            "ContextualSearchThinWebViewImplementation";
    public static final String CONTEXT_MENU_ENABLE_LENS_SHOPPING_ALLOWLIST =
            "ContextMenuEnableLensShoppingAllowlist";
    public static final String CONTEXT_MENU_GOOGLE_LENS_CHIP = "ContextMenuGoogleLensChip";
    public static final String CONTEXT_MENU_POPUP_FOR_ALL_SCREEN_SIZES =
            "ContextMenuPopupForAllScreenSizes";
    public static final String CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS =
            "ContextMenuSearchWithGoogleLens";
    public static final String CONTEXT_MENU_GOOGLE_LENS_SEARCH_OPTIMIZATIONS =
            "ContextMenuGoogleLensSearchOptimizations";
    public static final String CONTEXT_MENU_TRANSLATE_WITH_GOOGLE_LENS =
            "ContextMenuTranslateWithGoogleLens";
    public static final String CORMORANT = "Cormorant";
    public static final String CREATE_NEW_TAB_INITIALIZE_RENDERER =
            "CreateNewTabInitializeRenderer";
    public static final String DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING =
            "DarkenWebsitesCheckboxInThemesSetting";
    public static final String DEFER_KEEP_SCREEN_ON_DURING_GESTURE =
            "DeferKeepScreenOnDuringGesture";
    public static final String DEFER_NOTIFY_IN_MOTION = "DeferNotifyInMotion";
    public static final String DEFER_TAB_SWITCHER_LAYOUT_CREATION =
            "DeferTabSwitcherLayoutCreation";
    public static final String DELAY_TEMP_STRIP_REMOVAL = "DelayTempStripRemoval";
    public static final String DELAY_TRANSITIONS_FOR_ANIMATION = "DelayTransitionsForAnimation";
    public static final String DETAILED_LANGUAGE_SETTINGS = "DetailedLanguageSettings";
    public static final String DISCO_FEED_ENDPOINT = "DiscoFeedEndpoint";
    public static final String DNS_OVER_HTTPS = "DnsOverHttps";
    public static final String DOWNLOADS_MIGRATE_TO_JOBS_API = "DownloadsMigrateToJobsAPI";
    public static final String DOWNLOAD_NOTIFICATION_SERVICE_UNIFIED_API =
            "DownloadNotificationServiceUnifiedAPI";
    public static final String DOWNLOAD_OFFLINE_CONTENT_PROVIDER =
            "UseDownloadOfflineContentProvider";
    public static final String DRAG_DROP_INTO_OMNIBOX = "DragDropIntoOmnibox";
    public static final String DRAW_CUTOUT_EDGE_TO_EDGE = "DrawCutoutEdgeToEdge";
    public static final String DRAW_EDGE_TO_EDGE = "DrawEdgeToEdge";
    public static final String DRAW_NATIVE_EDGE_TO_EDGE = "DrawNativeEdgeToEdge";
    public static final String DRAW_WEB_EDGE_TO_EDGE = "DrawWebEdgeToEdge";
    public static final String EARLY_INITIALIZE_STARTUP_METRICS = "EarlyInitializeStartupMetrics";
    public static final String ENABLE_PROTO_API_FOR_CLASSIFY_URL = "EnableProtoApiForClassifyUrl";
    public static final String EXPERIMENTS_FOR_AGSA = "ExperimentsForAgsa";
    public static final String EMPTY_STATES = "EmptyStates";
    public static final String FEATURE_NOTIFICATION_GUIDE = "FeatureNotificationGuide";
    public static final String FEED_BACK_TO_TOP = "FeedBackToTop";
    public static final String FEED_DYNAMIC_COLORS = "FeedDynamicColors";
    public static final String FEED_FOLLOW_UI_UPDATE = "FeedFollowUiUpdate";
    public static final String FEED_HEADER_STICK_TO_TOP = "FeedHeaderStickToTop";
    public static final String FEED_IMAGE_MEMORY_CACHE_SIZE_PERCENTAGE =
            "FeedImageMemoryCacheSizePercentage";
    public static final String FEED_LOADING_PLACEHOLDER = "FeedLoadingPlaceholder";
    public static final String FEED_POSITION_ANDROID = "FeedPositionAndroid";
    public static final String FEED_SHOW_SIGN_IN_COMMAND = "FeedShowSignInCommand";
    public static final String FEED_USER_INTERACTION_RELIABILITY_REPORT =
            "FeedUserInteractionReliabilityReport";
    public static final String FILLING_PASSWORDS_FROM_ANY_ORIGIN = "FillingPasswordsFromAnyOrigin";
    public static final String FOCUS_OMNIBOX_IN_INCOGNITO_TAB_INTENTS =
            "FocusOmniboxInIncognitoTabIntents";
    public static final String FORCE_DISABLE_EXTENDED_SYNC_PROMOS =
            "ForceDisableExtendedSyncPromos";
    public static final String FORCE_STARTUP_SIGNIN_PROMO = "ForceStartupSigninPromo";
    public static final String FORCE_WEB_CONTENTS_DARK_MODE = "WebContentsForceDark";
    public static final String FRIENDLIER_SAFE_BROWSING_SETTINGS_ENHANCED_PROTECTION =
            "FriendlierSafeBrowsingSettingsEnhancedProtection";
    public static final String FRIENDLIER_SAFE_BROWSING_SETTINGS_STANDARD_PROTECTION =
            "FriendlierSafeBrowsingSettingsStandardProtection";
    public static final String GRID_TAB_SWITCHER_ANDROID_ANIMATIONS =
            "GridTabSwitcherAndroidAnimations";
    public static final String GRID_TAB_SWITCHER_LANDSCAPE_ASPECT_RATIO_PHONES =
            "GridTabSwitcherLandscapeAspectRatioPhones";
    public static final String HASH_PREFIX_REAL_TIME_LOOKUPS =
            "SafeBrowsingHashPrefixRealTimeLookups";
    public static final String HIDE_TAB_ON_TAB_SWITCHER = "HideTabOnTabSwitcher";
    public static final String HISTORY_JOURNEYS = "Journeys";
    public static final String INCOGNITO_DOWNLOADS_WARNING = "IncognitoDownloadsWarning";
    public static final String INCOGNITO_NTP_REVAMP = "IncognitoNtpRevamp";
    public static final String INCOGNITO_REAUTHENTICATION_FOR_ANDROID =
            "IncognitoReauthenticationForAndroid";
    public static final String INCOGNITO_SCREENSHOT = "IncognitoScreenshot";
    public static final String INFOBAR_SCROLL_OPTIMIZATION = "InfobarScrollOptimization";
    public static final String INSTALLABLE_AMBIENT_BADGE_INFOBAR = "InstallableAmbientBadgeInfoBar";
    public static final String INSTALLABLE_AMBIENT_BADGE_MESSAGE = "InstallableAmbientBadgeMessage";
    public static final String INSTANCE_SWITCHER = "InstanceSwitcher";
    public static final String INSTANT_START = "InstantStart";
    public static final String INTEREST_FEED_V2_AUTOPLAY = "InterestFeedV2Autoplay";
    public static final String INTEREST_FEED_V2_HEARTS = "InterestFeedV2Hearts";
    public static final String KID_FRIENDLY_CONTENT_FEED = "KidFriendlyContentFeed";
    public static final String LENS_CAMERA_ASSISTED_SEARCH = "LensCameraAssistedSearch";
    public static final String LENS_ON_QUICK_ACTION_SEARCH_WIDGET = "LensOnQuickActionSearchWidget";
    public static final String LOOKALIKE_NAVIGATION_URL_SUGGESTIONS_UI =
            "LookalikeUrlNavigationSuggestionsUI";
    public static final String MESSAGES_FOR_ANDROID_ADS_BLOCKED = "MessagesForAndroidAdsBlocked";
    public static final String MESSAGES_FOR_ANDROID_INFRASTRUCTURE =
            "MessagesForAndroidInfrastructure";
    public static final String MESSAGES_FOR_ANDROID_PERMISSION_UPDATE =
            "MessagesForAndroidPermissionUpdate";
    public static final String METRICS_SETTINGS_ANDROID = "MetricsSettingsAndroid";
    public static final String NEW_TAB_SEARCH_ENGINE_URL_ANDROID = "NewTabSearchEngineUrlAndroid";
    public static final String NOTIFICATION_PERMISSION_VARIANT = "NotificationPermissionVariant";
    public static final String NOTIFICATION_PERMISSION_BOTTOM_SHEET =
            "NotificationPermissionBottomSheet";
    public static final String OFFLINE_PAGES_DESCRIPTIVE_FAIL_STATUS =
            "OfflinePagesDescriptiveFailStatus";
    public static final String OFFLINE_PAGES_DESCRIPTIVE_PENDING_STATUS =
            "OfflinePagesDescriptivePendingStatus";
    public static final String OFFLINE_PAGES_LIVE_PAGE_SHARING = "OfflinePagesLivePageSharing";
    public static final String OMAHA_MIN_SDK_VERSION_ANDROID = "OmahaMinSdkVersionAndroid";
    public static final String OMNIBOX_CACHE_SUGGESTION_RESOURCES =
            "OmniboxCacheSuggestionResources";
    public static final String OMNIBOX_HISTORY_CLUSTER_PROVIDER =
            "JourneysOmniboxHistoryClusterProvider";

    public static final String OMNIBOX_HISTORY_CLUSTER_ACTION_CHIP = "JourneysOmniboxAction";
    public static final String OMNIBOX_MATCH_TOOLBAR_AND_STATUS_BAR_COLOR =
            "OmniboxMatchToolbarAndStatusBarColor";
    public static final String OMNIBOX_MODERNIZE_VISUAL_UPDATE = "OmniboxModernizeVisualUpdate";
    public static final String OMNIBOX_NOOP_EDIT_URL_SUGGESTION_CLICKS =
            "OmniboxNoopEditUrlSuggestionClicks";
    public static final String AVOID_RELAYOUT_DURING_FOCUS_ANIMATION =
            "AvoidRelayoutDuringFocusAnimation";
    public static final String SHORT_CIRCUIT_UNFOCUS_ANIMATION = "ShortCircuitUnfocusAnimation";
    public static final String OMNIBOX_TOUCH_DOWN_TRIGGER_FOR_PREFETCH =
            "OmniboxTouchDownTriggerForPrefetch";
    public static final String OMNIBOX_UPDATED_CONNECTION_SECURITY_INDICATORS =
            "OmniboxUpdatedConnectionSecurityIndicators";
    public static final String OMNIBOX_WARM_RECYCLED_VIEW_POOL = "OmniboxWarmRecycledViewPool";
    public static final String OPTIMIZATION_GUIDE_PUSH_NOTIFICATIONS =
            "OptimizationGuidePushNotifications";
    public static final String PAGE_INFO_ABOUT_THIS_SITE_MORE_LANGS =
            "PageInfoAboutThisSiteMoreLangs";
    public static final String PAINT_PREVIEW_DEMO = "PaintPreviewDemo";
    public static final String PAINT_PREVIEW_NEW_COLD_START_HEURISTIC =
            "PaintPreviewNewColdStartHeuristic";
    public static final String PARTNER_CUSTOMIZATIONS_UMA = "PartnerCustomizationsUma";
    public static final String PASSKEY_MANAGEMENT_USING_ACCOUNT_SETTINGS_ANDROID =
            "PasskeyManagementUsingAccountSettingsAndroid";
    public static final String PASSWORD_GENERATION_BOTTOM_SHEET = "PasswordGenerationBottomSheet";
    public static final String PASSWORD_EDIT_DIALOG_WITH_DETAILS = "PasswordEditDialogWithDetails";
    public static final String PORTALS = "Portals";
    public static final String PORTALS_CROSS_ORIGIN = "PortalsCrossOrigin";
    public static final String PREEMPTIVE_LINK_TO_TEXT_GENERATION =
            "PreemptiveLinkToTextGeneration";
    public static final String PRERENDER2 = "Prerender2";
    public static final String PRECONNECT_ON_TAB_CREATION = "PreconnectOnTabCreation";
    public static final String PRIVACY_GUIDE = "PrivacyGuideAndroid";
    public static final String PRIVACY_GUIDE_ANDROID_3 = "PrivacyGuideAndroid3";
    public static final String PRIVACY_GUIDE_PRELOAD_ANDROID = "PrivacyGuidePreloadAndroid";
    public static final String PRIVACY_GUIDE_POST_MVP = "PrivacyGuideAndroidPostMVP";
    public static final String PRIVACY_SANDBOX_FPS_UI = "PrivacySandboxFirstPartySetsUI";
    public static final String PRIVACY_SANDBOX_SETTINGS_3 = "PrivacySandboxSettings3";
    public static final String PRIVACY_SANDBOX_SETTINGS_4 = "PrivacySandboxSettings4";
    public static final String PRIVACY_SANDBOX_PROACTIVE_TOPICS_BLOCKING =
            "PrivacySandboxProactiveTopicsBlocking";
    public static final String PRIVATE_STATE_TOKENS = "PrivateStateTokens";
    public static final String PROBABILISTIC_CRYPTID_RENDERER = "ProbabilisticCryptidRenderer";
    public static final String PUSH_MESSAGING_DISALLOW_SENDER_IDS =
            "PushMessagingDisallowSenderIDs";
    public static final String PWA_DEFAULT_OFFLINE_PAGE = "PWAsDefaultOfflinePage";
    public static final String PWA_UPDATE_DIALOG_FOR_ICON = "PwaUpdateDialogForIcon";
    public static final String QUERY_TILES = "QueryTiles";
    public static final String QUERY_TILES_IN_NTP = "QueryTilesInNTP";
    public static final String QUERY_TILES_ON_START = "QueryTilesOnStart";
    public static final String QUERY_TILES_SEGMENTATION = "QueryTilesSegmentation";
    public static final String QUICK_DELETE_FOR_ANDROID = "QuickDeleteForAndroid";
    public static final String QUIET_NOTIFICATION_PROMPTS = "QuietNotificationPrompts";
    public static final String REACHED_CODE_PROFILER = "ReachedCodeProfiler";
    public static final String READALOUD = "ReadAloud";
    public static final String READER_MODE_IN_CCT = "ReaderModeInCCT";
    public static final String RECORD_SUPPRESSION_METRICS = "RecordSuppressionMetrics";
    public static final String RECOVER_FROM_NEVER_SAVE_ANDROID =
            "RecoverFromNeverSaveAndroid_LAUNCHED";
    public static final String RED_INTERSTITIAL_FACELIFT = "RedInterstitialFacelift";
    public static final String REENGAGEMENT_NOTIFICATION = "ReengagementNotification";
    public static final String RELATED_SEARCHES = "RelatedSearches";
    public static final String RENAME_JOURNEYS = "RenameJourneys";
    public static final String RESTORE_TABS_ON_FRE = "RestoreTabsOnFRE";
    public static final String REQUEST_DESKTOP_SITE_DEFAULTS = "RequestDesktopSiteDefaults";
    public static final String REQUEST_DESKTOP_SITE_DEFAULTS_CONTROL =
            "RequestDesktopSiteDefaultsControl";
    public static final String REQUEST_DESKTOP_SITE_DEFAULTS_CONTROL_SYNTHETIC =
            "RequestDesktopSiteDefaultsControlSynthetic";
    public static final String REQUEST_DESKTOP_SITE_DEFAULTS_DOWNGRADE =
            "RequestDesktopSiteDefaultsDowngrade";
    public static final String REQUEST_DESKTOP_SITE_DEFAULTS_LOGGING =
            "RequestDesktopSiteDefaultsLogging";
    public static final String REQUEST_DESKTOP_SITE_DEFAULTS_SYNTHETIC =
            "RequestDesktopSiteDefaultsSynthetic";
    public static final String REQUEST_DESKTOP_SITE_OPT_IN_CONTROL_SYNTHETIC =
            "RequestDesktopSiteOptInControlSynthetic";
    public static final String REQUEST_DESKTOP_SITE_OPT_IN_SYNTHETIC =
            "RequestDesktopSiteOptInSynthetic";
    public static final String SAFE_BROWSING_DELAYED_WARNINGS = "SafeBrowsingDelayedWarnings";
    public static final String SAFE_BROWSING_SKIP_SUBRESOURCES = "SafeBrowsingSkipSubResources";
    public static final String SCREENSHOTS_FOR_ANDROID_V2 = "ScreenshotsForAndroidV2";
    public static final String SCROLL_TO_TLD_OPTIMIZATION = "ScrollToTLDOptimization";
    public static final String SEARCH_ENGINES_PROMO_V3 = "SearchEnginesPromoV3";
    public static final String SEARCH_ENGINE_CHOICE = "SearchEngineChoice";
    public static final String SEARCH_READY_OMNIBOX_ALLOW_QUERY_EDIT =
            "SearchReadyOmniboxAllowQueryEdit";
    public static final String SEARCH_RESUMPTION_MODULE_ANDROID = "SearchResumptionModuleAndroid";
    public static final String SEED_ACCOUNTS_REVAMP = "SeedAccountsRevamp";
    public static final String SHOULD_IGNORE_INTENT_SKIP_INTERNAL_CHECK =
            "ShouldIgnoreIntentSkipInternalCheck";
    public static final String SHARE_SHEET_CUSTOM_ACTIONS_POLISH = "ShareSheetCustomActionsPolish";
    public static final String SHARE_SHEET_MIGRATION_ANDROID = "ShareSheetMigrationAndroid";
    public static final String SEND_TAB_TO_SELF_V2 = "SendTabToSelfV2";
    public static final String SHOPPING_LIST = "ShoppingList";
    public static final String SHOW_NTP_AT_STARTUP_ANDROID = "ShowNtpAtStartupAndroid";
    public static final String SHOW_SCROLLABLE_MVT_ON_NTP_ANDROID = "ShowScrollableMVTOnNTPAndroid";
    public static final String SHOW_SCROLLABLE_MVT_ON_NTP_PHONE_ANDROID =
            "ShowScrollableMVTOnNtpPhoneAndroid";
    public static final String SMART_SUGGESTION_FOR_LARGE_DOWNLOADS =
            "SmartSuggestionForLargeDownloads";
    public static final String SPARE_TAB = "SpareTab";
    public static final String SPLIT_CACHE_BY_NETWORK_ISOLATION_KEY =
            "SplitCacheByNetworkIsolationKey";
    public static final String START_SURFACE_ANDROID = "StartSurfaceAndroid";
    public static final String START_SURFACE_DISABLED_FEED_IMPROVEMENT =
            "StartSurfaceDisabledFeedImprovement";
    public static final String START_SURFACE_ON_TABLET = "StartSurfaceOnTablet";
    public static final String START_SURFACE_REFACTOR = "StartSurfaceRefactor";
    public static final String START_SURFACE_RETURN_TIME = "StartSurfaceReturnTime";
    public static final String ACCOUNT_REAUTHENTICATION_RECENT_TIME_WINDOW =
            "AccountReauthenticationRecentTimeWindow";
    public static final String START_SURFACE_WITH_ACCESSIBILITY = "StartSurfaceWithAccessibility";
    public static final String START_SURFACE_SPARE_TAB = "StartSurfaceSpareTab";
    public static final String STORE_HOURS = "StoreHoursAndroid";
    public static final String SUGGESTION_ANSWERS_COLOR_REVERSE = "SuggestionAnswersColorReverse";
    public static final String SUPPRESS_TOOLBAR_CAPTURES = "SuppressToolbarCaptures";
    public static final String SURFACE_POLISH = "SurfacePolish";
    public static final String SYNC_ANDROID_LIMIT_NTP_PROMO_IMPRESSIONS =
            "SyncAndroidLimitNTPPromoImpressions";
    public static final String SYNC_DECOUPLE_ADDRESS_PAYMENT_SETTINGS =
            "SyncDecoupleAddressPaymentSettings";
    public static final String SYNC_ENABLE_CONTACT_INFO_DATA_TYPE_IN_TRANSPORT_MODE =
            "SyncEnableContactInfoDataTypeInTransportMode";
    public static final String TAB_DRAG_DROP_ANDROID = "TabDragDropAndroid";
    public static final String TAB_LINK_DRAG_DROP_ANDROID = "TabAndLinkDragDropAndroid";
    public static final String TAB_ENGAGEMENT_REPORTING_ANDROID = "TabEngagementReportingAndroid";
    public static final String TAB_STRIP_REDESIGN = "TabStripRedesign";
    public static final String TAB_STRIP_STARTUP_REFACTORING = "TabStripStartupRefactoring";
    public static final String TABLET_TOOLBAR_REORDERING = "TabletToolbarReordering";
    public static final String TAB_TO_GTS_ANIMATION = "TabToGTSAnimation";
    public static final String TAB_STATE_FLATBUFFER = "TabStateFlatBuffer";
    public static final String TANGIBLE_SYNC = "TangibleSync";
    public static final String TEST_DEFAULT_DISABLED = "TestDefaultDisabled";
    public static final String TEST_DEFAULT_ENABLED = "TestDefaultEnabled";
    public static final String THUMBNAIL_CACHE_REFACTOR = "ThumbnailCacheRefactor";
    public static final String THUMBNAIL_PLACEHOLDER = "ThumbnailPlaceholder";
    public static final String TOOLBAR_SCROLL_ABLATION_ANDROID = "ToolbarScrollAblationAndroid";
    public static final String TOOLBAR_USE_HARDWARE_BITMAP_DRAW = "ToolbarUseHardwareBitmapDraw";
    public static final String TRACKING_PROTECTION_SETTINGS_PAGE_ROLLBACK_NOTICE =
            "TrackingProtectionSettingsPageRollbackNotice";
    public static final String TRACKING_PROTECTION_ONBOARDING_SKIP_SECURE_PAGE_CHECK =
            "TrackingProtectionOnboardingSkipSecurePageCheck";
    public static final String TRACKING_PROTECTION_3PCD = "TrackingProtection3pcd";
    public static final String TRANSLATE_MESSAGE_UI = "TranslateMessageUI";
    public static final String TRANSLATE_TFLITE = "TFLiteLanguageDetectionEnabled";
    public static final String TRUSTED_WEB_ACTIVITY_POST_MESSAGE = "TrustedWebActivityPostMessage";
    public static final String UNIFIED_PASSWORD_MANAGER_LOCAL_PWD_MIGRATION_WARNING =
            "UnifiedPasswordManagerLocalPasswordsMigrationWarning";
    public static final String USE_CHIME_ANDROID_SDK = "UseChimeAndroidSdk";
    public static final String USE_LIBUNWINDSTACK_NATIVE_UNWINDER_ANDROID =
            "UseLibunwindstackNativeUnwinderAndroid";
    public static final String USER_BYPASS_UI = "UserBypassUI";
    public static final String VOICE_BUTTON_IN_TOP_TOOLBAR = "VoiceButtonInTopToolbar";
    public static final String VOICE_SEARCH_AUDIO_CAPTURE_POLICY = "VoiceSearchAudioCapturePolicy";
    public static final String WEBNOTES_STYLIZE = "WebNotesStylize";
    public static final String WEB_APK_ALLOW_ICON_UPDATE = "WebApkAllowIconUpdate";
    public static final String WEB_APK_ICON_UPDATE_THRESHOLD = "WebApkIconUpdateThreshold";
    public static final String WEB_APK_INSTALL_SERVICE = "WebApkInstallService";
    public static final String WEB_APP_AMBIENT_BADGE_SUPRESS_FIRST_VISIT =
            "AmbientBadgeSuppressFirstVisit";
    public static final String WEB_APK_INSTALL_FAILURE_NOTIFICATION =
            "WebApkInstallFailureNotification";
    public static final String WEB_APK_INSTALL_RETRY = "WebApkInstallFailureRetry";
    public static final String WEB_FEED = "WebFeed";
    public static final String WEB_FEED_AWARENESS = "WebFeedAwareness";
    public static final String WEB_FEED_ONBOARDING = "WebFeedOnboarding";
    public static final String WEB_FEED_SORT = "WebFeedSort";
    public static final String WEB_OTP_CROSS_DEVICE_SIMPLE_STRING = "WebOtpCrossDeviceSimpleString";
    public static final String XSURFACE_METRICS_REPORTING = "XsurfaceMetricsReporting";

    /* Alphabetical: */
    public static final CachedFlag sAndroidAppIntegration =
            new CachedFlag(ANDROID_APP_INTEGRATION, false);
    public static final CachedFlag sAndroidHub = new CachedFlag(ANDROID_HUB, false);
    public static final CachedFlag sAppMenuMobileSiteOption =
            new CachedFlag(APP_MENU_MOBILE_SITE_OPTION, false);
    public static final CachedFlag sBackGestureActivityTabProvider =
            new CachedFlag(BACK_GESTURE_ACTIVITY_TAB_PROVIDER, false);
    public static final CachedFlag sBackGestureRefactorActivityAndroid =
            new CachedFlag(BACK_GESTURE_REFACTOR_ACTIVITY, false);
    public static final CachedFlag sBackGestureRefactorAndroid =
            new CachedFlag(BACK_GESTURE_REFACTOR, false);
    public static final CachedFlag sBaselineGm3SurfaceColors =
            new CachedFlag(BASELINE_GM3_SURFACE_COLORS, true);
    public static final CachedFlag sBlockIntentsWhileLocked =
            new CachedFlag(BLOCK_INTENTS_WHILE_LOCKED, false);
    public static final CachedFlag sCctAutoTranslate = new CachedFlag(CCT_AUTO_TRANSLATE, true);
    public static final CachedFlag sCctBottomBarSwipeUpGesture =
            new CachedFlag(CCT_BOTTOM_BAR_SWIPE_UP_GESTURE, true);
    public static final CachedFlag sCctBrandTransparencyMemoryImprovement =
            new CachedFlag(CCT_BRAND_TRANSPARENCY_MEMORY_IMPROVEMENT, true);
    public static final CachedFlag sCctFeatureUsage = new CachedFlag(CCT_FEATURE_USAGE, false);
    public static final CachedFlag sCctIncognitoAvailableToThirdParty =
            new CachedFlag(CCT_INCOGNITO_AVAILABLE_TO_THIRD_PARTY, false);
    public static final CachedFlag sCctIntentFeatureOverrides =
            new CachedFlag(CCT_INTENT_FEATURE_OVERRIDES, true);
    public static final CachedFlag sCctMinimized = new CachedFlag(CCT_MINIMIZED, false);
    public static final CachedFlag sCctPageInsightsHub =
            new CachedFlag(CCT_PAGE_INSIGHTS_HUB, false);
    public static final CachedFlag sCctRemoveRemoteViewIds =
            new CachedFlag(CCT_REMOVE_REMOTE_VIEW_IDS, true);
    public static final CachedFlag sCctResizable90MaximumHeight =
            new CachedFlag(CCT_RESIZABLE_90_MAXIMUM_HEIGHT, false);
    public static final CachedFlag sCctResizableForThirdParties =
            new CachedFlag(CCT_RESIZABLE_FOR_THIRD_PARTIES, true);
    public static final CachedFlag sCctResizableSideSheet =
            new CachedFlag(CCT_RESIZABLE_SIDE_SHEET, true);
    public static final CachedFlag sCctResizableSideSheetForThirdParties =
            new CachedFlag(CCT_RESIZABLE_SIDE_SHEET_FOR_THIRD_PARTIES, false);
    public static final CachedFlag sCctToolbarCustomizations =
            new CachedFlag(CCT_TOOLBAR_CUSTOMIZATIONS, true);
    public static final CachedFlag sCloseTabSuggestions =
            new CachedFlag(CLOSE_TAB_SUGGESTIONS, false);
    public static final CachedFlag sCloseTabSaveTabList =
            new CachedFlag(CLOSE_TAB_SAVE_TAB_LIST, true);
    public static final CachedFlag sCollectAndroidFrameTimelineMetrics =
            new CachedFlag(COLLECT_ANDROID_FRAME_TIMELINE_METRICS, false);
    public static final CachedFlag sCommandLineOnNonRooted =
            new CachedFlag(COMMAND_LINE_ON_NON_ROOTED,
                    ChromePreferenceKeys.FLAGS_CACHED_COMMAND_LINE_ON_NON_ROOTED_ENABLED, false);
    public static final CachedFlag sDeferTabSwitcherLayoutCreation =
            new CachedFlag(DEFER_TAB_SWITCHER_LAYOUT_CREATION, false);
    public static final CachedFlag sDelayTempStripRemoval =
            new CachedFlag(DELAY_TEMP_STRIP_REMOVAL, true);
    public static final CachedFlag sDragDropIntoOmnibox =
            new CachedFlag(DRAG_DROP_INTO_OMNIBOX, false);
    public static final CachedFlag sDownloadsMigrateToJobsAPI =
            new CachedFlag(DOWNLOADS_MIGRATE_TO_JOBS_API, false);
    public static final CachedFlag sDrawEdgeToEdge = new CachedFlag(DRAW_EDGE_TO_EDGE, false);
    public static final CachedFlag sDrawNativeEdgeToEdge =
            new CachedFlag(DRAW_NATIVE_EDGE_TO_EDGE, false);
    public static final CachedFlag sDrawWebEdgeToEdge =
            new CachedFlag(DRAW_WEB_EDGE_TO_EDGE, false);
    public static final CachedFlag sEarlyInitializeStartupMetrics =
            new CachedFlag(EARLY_INITIALIZE_STARTUP_METRICS, false);
    public static final CachedFlag sEmptyStates = new CachedFlag(EMPTY_STATES, true);
    public static final CachedFlag sExperimentsForAgsa = new CachedFlag(EXPERIMENTS_FOR_AGSA, true);
    public static final CachedFlag sFeedLoadingPlaceholder =
            new CachedFlag(FEED_LOADING_PLACEHOLDER, false);
    public static final CachedFlag sFriendlierSafeBrowsingSettingsEnhancedProtection =
            new CachedFlag(FRIENDLIER_SAFE_BROWSING_SETTINGS_ENHANCED_PROTECTION, false);
    public static final CachedFlag sFriendlierSafeBrowsingSettingsStandardProtection =
            new CachedFlag(FRIENDLIER_SAFE_BROWSING_SETTINGS_STANDARD_PROTECTION, false);
    public static final CachedFlag sGridTabSwitcherAndroidAnimations =
            new CachedFlag(GRID_TAB_SWITCHER_ANDROID_ANIMATIONS, false);
    public static final CachedFlag sGridTabSwitcherLandscapeAspectRatioPhones =
            new CachedFlag(GRID_TAB_SWITCHER_LANDSCAPE_ASPECT_RATIO_PHONES, true);
    public static final CachedFlag sIncognitoReauthenticationForAndroid =
            new CachedFlag(INCOGNITO_REAUTHENTICATION_FOR_ANDROID, false);
    public static final CachedFlag sInstanceSwitcher = new CachedFlag(INSTANCE_SWITCHER, true);
    public static final CachedFlag sInstantStart = new CachedFlag(INSTANT_START, false);
    public static final CachedFlag sHideTabOnTabSwitcher =
            new CachedFlag(HIDE_TAB_ON_TAB_SWITCHER, false);
    public static final CachedFlag sNewTabSearchEngineUrlAndroid =
            new CachedFlag(NEW_TAB_SEARCH_ENGINE_URL_ANDROID, false);
    public static final CachedFlag sPrivacyGuideAndroid3 =
            new CachedFlag(PRIVACY_GUIDE_ANDROID_3, false);
    public static final CachedFlag sPrivacyGuidePreloadAndroid =
            new CachedFlag(PRIVACY_GUIDE_PRELOAD_ANDROID, false);
    public static final CachedFlag sPrivacyGuidePostMVP =
            new CachedFlag(PRIVACY_GUIDE_POST_MVP, false);
    public static final CachedFlag sOmniboxMatchToolbarAndStatusBarColor =
            new CachedFlag(OMNIBOX_MATCH_TOOLBAR_AND_STATUS_BAR_COLOR, false);
    public static final CachedFlag sOmniboxModernizeVisualUpdate =
            new CachedFlag(OMNIBOX_MODERNIZE_VISUAL_UPDATE, true);
    public static final CachedFlag sOptimizationGuidePushNotifications =
            new CachedFlag(OPTIMIZATION_GUIDE_PUSH_NOTIFICATIONS, false);
    public static final CachedFlag sPaintPreviewDemo = new CachedFlag(PAINT_PREVIEW_DEMO, false);
    public static final CachedFlag sPaintPreviewNewColdStartHeuristic =
            new CachedFlag(PAINT_PREVIEW_NEW_COLD_START_HEURISTIC, true);
    public static final CachedFlag sQueryTiles = new CachedFlag(QUERY_TILES, false);
    public static final CachedFlag sQueryTilesOnStart = new CachedFlag(QUERY_TILES_ON_START, false);
    public static final CachedFlag sSearchEngineChoice =
            new CachedFlag(SEARCH_ENGINE_CHOICE, false);
    public static final CachedFlag sShouldIgnoreIntentSkipInternalCheck =
            new CachedFlag(SHOULD_IGNORE_INTENT_SKIP_INTERNAL_CHECK, true);
    public static final CachedFlag sShowNtpAtStartupAndroid =
            new CachedFlag(SHOW_NTP_AT_STARTUP_ANDROID, false);
    public static final CachedFlag sSpareTab = new CachedFlag(SPARE_TAB, false);
    public static final CachedFlag sStartSurfaceAndroid = new CachedFlag(
            START_SURFACE_ANDROID, ChromePreferenceKeys.FLAGS_CACHED_START_SURFACE_ENABLED, true);
    public static final CachedFlag sStartSurfaceDisabledFeedImprovement =
            new CachedFlag(START_SURFACE_DISABLED_FEED_IMPROVEMENT, false);
    public static final CachedFlag sStartSurfaceOnTablet =
            new CachedFlag(START_SURFACE_ON_TABLET, true);
    public static final CachedFlag sStartSurfaceRefactor =
            new CachedFlag(START_SURFACE_REFACTOR, false);
    public static final CachedFlag sStartSurfaceReturnTime =
            new CachedFlag(START_SURFACE_RETURN_TIME, false);
    public static final CachedFlag sAccountReauthenticationRecentTimeWindow =
            new CachedFlag(ACCOUNT_REAUTHENTICATION_RECENT_TIME_WINDOW, true);
    public static final CachedFlag sStartSurfaceWithAccessibility =
            new CachedFlag(START_SURFACE_WITH_ACCESSIBILITY, true);
    public static final CachedFlag sStoreHoursAndroid = new CachedFlag(STORE_HOURS, false);
    public static final CachedFlag sSurfacePolish = new CachedFlag(SURFACE_POLISH, false);
    public static final CachedFlag sTabDragDropAsWindowAndroid =
            new CachedFlag(TAB_DRAG_DROP_ANDROID, false);
    public static final CachedFlag sTabLinkDragDropAndroid =
            new CachedFlag(TAB_LINK_DRAG_DROP_ANDROID, false);
    public static final CachedFlag sTabStripRedesign = new CachedFlag(TAB_STRIP_REDESIGN, true);
    public static final CachedFlag sTabStateFlatBuffer =
            new CachedFlag(TAB_STATE_FLATBUFFER, false);
    public static final CachedFlag sTabStripStartupRefactoring =
            new CachedFlag(TAB_STRIP_STARTUP_REFACTORING, false);
    public static final CachedFlag sTabletToolbarReordering =
            new CachedFlag(TABLET_TOOLBAR_REORDERING, false);
    public static final CachedFlag sTabToGTSAnimation = new CachedFlag(TAB_TO_GTS_ANIMATION, true);
    public static final CachedFlag sTestDefaultDisabled =
            new CachedFlag(TEST_DEFAULT_DISABLED, false);
    public static final CachedFlag sTestDefaultEnabled = new CachedFlag(TEST_DEFAULT_ENABLED, true);
    public static final CachedFlag sToolbarUseHardwareBitmapDraw =
            new CachedFlag(TOOLBAR_USE_HARDWARE_BITMAP_DRAW, false);
    public static final CachedFlag sUseChimeAndroidSdk =
            new CachedFlag(USE_CHIME_ANDROID_SDK, false);
    public static final CachedFlag sUseLibunwindstackNativeUnwinderAndroid =
            new CachedFlag(USE_LIBUNWINDSTACK_NATIVE_UNWINDER_ANDROID, true);

    public static final List<CachedFlag> sFlagsCachedFullBrowser =
            List.of(
                    sAndroidAppIntegration,
                    sAndroidHub,
                    sAppMenuMobileSiteOption,
                    sBackGestureActivityTabProvider,
                    sBackGestureRefactorActivityAndroid,
                    sBackGestureRefactorAndroid,
                    sBaselineGm3SurfaceColors,
                    sBlockIntentsWhileLocked,
                    sCctAutoTranslate,
                    sCctBottomBarSwipeUpGesture,
                    sCctBrandTransparencyMemoryImprovement,
                    sCctFeatureUsage,
                    sCctIncognitoAvailableToThirdParty,
                    sCctIntentFeatureOverrides,
                    sCctMinimized,
                    sCctPageInsightsHub,
                    sCctRemoveRemoteViewIds,
                    sCctResizable90MaximumHeight,
                    sCctResizableForThirdParties,
                    sCctResizableSideSheet,
                    sCctResizableSideSheetForThirdParties,
                    sCctToolbarCustomizations,
                    sCloseTabSuggestions,
                    sCloseTabSaveTabList,
                    sCollectAndroidFrameTimelineMetrics,
                    sCommandLineOnNonRooted,
                    sDeferTabSwitcherLayoutCreation,
                    sDelayTempStripRemoval,
                    sDragDropIntoOmnibox,
                    sDownloadsMigrateToJobsAPI,
                    sDrawEdgeToEdge,
                    sDrawNativeEdgeToEdge,
                    sDrawWebEdgeToEdge,
                    sEarlyInitializeStartupMetrics,
                    sEmptyStates,
                    sFeedLoadingPlaceholder,
                    sFriendlierSafeBrowsingSettingsEnhancedProtection,
                    sFriendlierSafeBrowsingSettingsStandardProtection,
                    sGridTabSwitcherAndroidAnimations,
                    sGridTabSwitcherLandscapeAspectRatioPhones,
                    sIncognitoReauthenticationForAndroid,
                    sInstanceSwitcher,
                    sInstantStart,
                    sHideTabOnTabSwitcher,
                    sNewTabSearchEngineUrlAndroid,
                    sPaintPreviewNewColdStartHeuristic,
                    sPrivacyGuideAndroid3,
                    sPrivacyGuidePreloadAndroid,
                    sPrivacyGuidePostMVP,
                    sOmniboxMatchToolbarAndStatusBarColor,
                    sOmniboxModernizeVisualUpdate,
                    sOptimizationGuidePushNotifications,
                    sPaintPreviewDemo,
                    sQueryTiles,
                    sQueryTilesOnStart,
                    sSearchEngineChoice,
                    sShouldIgnoreIntentSkipInternalCheck,
                    sShowNtpAtStartupAndroid,
                    sSpareTab,
                    sStartSurfaceAndroid,
                    sStartSurfaceDisabledFeedImprovement,
                    sStartSurfaceOnTablet,
                    sStartSurfaceRefactor,
                    sStartSurfaceReturnTime,
                    sAccountReauthenticationRecentTimeWindow,
                    sStartSurfaceWithAccessibility,
                    sStoreHoursAndroid,
                    sSurfacePolish,
                    sTabDragDropAsWindowAndroid,
                    sTabLinkDragDropAndroid,
                    sTabStateFlatBuffer,
                    sTabStripRedesign,
                    sTabStripStartupRefactoring,
                    sTabletToolbarReordering,
                    sTabToGTSAnimation,
                    sToolbarUseHardwareBitmapDraw,
                    sUseChimeAndroidSdk,
                    sUseLibunwindstackNativeUnwinderAndroid);

    public static final List<CachedFlag> sFlagsCachedInMinimalBrowser =
            List.of(sExperimentsForAgsa);

    public static final List<CachedFlag> sTestCachedFlags =
            List.of(sTestDefaultDisabled, sTestDefaultEnabled);

    public static final Map<String, CachedFlag> sAllCachedFlags = CachedFlag.createCachedFlagMap(
            List.of(sFlagsCachedFullBrowser, sFlagsCachedInMinimalBrowser, sTestCachedFlags));
}
