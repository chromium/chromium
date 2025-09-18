// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import org.chromium.base.BaseFeatures;
import org.chromium.base.FeatureMap;
import org.chromium.base.MutableBooleanParamWithSafeDefault;
import org.chromium.base.MutableFlagWithSafeDefault;
import org.chromium.base.MutableIntParamWithSafeDefault;
import org.chromium.base.MutableParamWithSafeDefault;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.cached_flags.BooleanCachedFeatureParam;
import org.chromium.components.cached_flags.CachedFeatureParam;
import org.chromium.components.cached_flags.CachedFlag;
import org.chromium.components.cached_flags.DoubleCachedFeatureParam;
import org.chromium.components.cached_flags.IntCachedFeatureParam;
import org.chromium.components.cached_flags.StringCachedFeatureParam;

import java.util.List;
import java.util.Map;

/**
 * A list of feature flags exposed to Java.
 *
 * <p>This class lists flags exposed to Java as String constants. The String value of each feature
 * name must exactly match the corresponding C++ feature name string |kFeaturesExposedToJava| in
 * chrome/browser/flags/android/chrome_feature_list.cc.
 *
 * <p>This class also provides convenience methods to access values of flags and their field trial
 * parameters through {@link ChromeFeatureMap}.
 *
 * <p>Chrome-layer {@link CachedFlag}s and {@link MutableFlagWithSafeDefault}s are instantiated
 * here, as well as {@link CachedFeatureParam}s and {@link MutableParamWithSafeDefault}s.
 */
@NullMarked
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

    public static BooleanCachedFeatureParam newBooleanCachedFeatureParam(
            String featureName, String variationName, boolean defaultValue) {
        return new BooleanCachedFeatureParam(
                ChromeFeatureMap.getInstance(), featureName, variationName, defaultValue);
    }

    public static DoubleCachedFeatureParam newDoubleCachedFeatureParam(
            String featureName, String variationName, double defaultValue) {
        return new DoubleCachedFeatureParam(
                ChromeFeatureMap.getInstance(), featureName, variationName, defaultValue);
    }

    public static IntCachedFeatureParam newIntCachedFeatureParam(
            String featureName, String variationName, int defaultValue) {
        return new IntCachedFeatureParam(
                ChromeFeatureMap.getInstance(), featureName, variationName, defaultValue);
    }

    public static StringCachedFeatureParam newStringCachedFeatureParam(
            String featureName, String variationName, String defaultValue) {
        return new StringCachedFeatureParam(
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
    // keep-sorted start group_prefixes=["public static final String"]
    public static final String ACCOUNT_FOR_SUPPRESSED_KEYBOARD_INSETS =
            "AccountForSuppressedKeyboardInsets";
    public static final String ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2 =
            "AdaptiveButtonInTopToolbarCustomizationV2";
    public static final String ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_PAGE_SUMMARY =
            "AdaptiveButtonInTopToolbarPageSummary";
    public static final String ALLOW_TAB_CLOSING_UPON_MINIMIZATION =
            "AllowTabClosingUponMinimization";
    public static final String ANDROID_ANIMATED_PROGRESS_BAR_IN_BROWSER =
            "AndroidAnimatedProgressBarInBrowser";
    public static final String ANDROID_ANIMATED_PROGRESS_BAR_IN_VIZ =
            "AndroidAnimatedProgressBarInViz";
    public static final String ANDROID_APPEARANCE_SETTINGS = "AndroidAppearanceSettings";
    public static final String ANDROID_APP_INTEGRATION = "AndroidAppIntegration";
    public static final String ANDROID_APP_INTEGRATION_MODULE = "AndroidAppIntegrationModule";
    public static final String ANDROID_APP_INTEGRATION_MULTI_DATA_SOURCE =
            "AndroidAppIntegrationMultiDataSource";
    public static final String ANDROID_APP_INTEGRATION_V2 = "AndroidAppIntegrationV2";
    public static final String ANDROID_APP_INTEGRATION_WITH_FAVICON =
            "AndroidAppIntegrationWithFavicon";
    public static final String ANDROID_BOOKMARK_BAR = "AndroidBookmarkBar";
    public static final String ANDROID_BOTTOM_TOOLBAR = "AndroidBottomToolbar";
    public static final String ANDROID_BOTTOM_TOOLBAR_V2 = "AndroidBottomToolbarV2";
    public static final String ANDROID_COMPOSEPLATE = "AndroidComposeplate";
    public static final String ANDROID_DATA_IMPORTER_SERVICE = "AndroidDataImporterService";
    public static final String ANDROID_DESKTOP_DENSITY = "AndroidDesktopDensity";
    public static final String ANDROID_ELEGANT_TEXT_HEIGHT = "AndroidElegantTextHeight";
    public static final String ANDROID_FIRST_RUN_LAUNCH_BOUNDS = "AndroidFirstRunLaunchBounds";
    public static final String ANDROID_MINIMAL_UI_LARGE_SCREEN = "AndroidMinimalUiLargeScreen";
    public static final String ANDROID_NATIVE_PAGES_IN_NEW_TAB = "AndroidNativePagesInNewTab";
    public static final String ANDROID_NO_VISIBLE_HINT_FOR_DIFFERENT_TLD =
            "AndroidNoVisibleHintForDifferentTLD";
    public static final String ANDROID_OMNIBOX_FOCUSED_NEW_TAB_PAGE =
            "AndroidOmniboxFocusedNewTabPage";
    public static final String ANDROID_OPEN_INCOGNITO_AS_WINDOW = "AndroidOpenIncognitoAsWindow";
    public static final String ANDROID_OPEN_PDF_INLINE_BACKPORT = "AndroidOpenPdfInlineBackport";
    public static final String ANDROID_PDF_ASSIST_CONTENT = "AndroidPdfAssistContent";
    public static final String ANDROID_PINNED_TABS = "AndroidPinnedTabs";
    public static final String ANDROID_PROGRESS_BAR_VISUAL_UPDATE =
            "AndroidProgressBarVisualUpdate";
    public static final String ANDROID_SETTINGS_CONTAINMENT = "AndroidSettingsContainment";
    public static final String ANDROID_SHOW_RESTORE_TABS_PROMO_ON_FRE_BYPASSED_KILL_SWITCH =
            "AndroidShowRestoreTabsPromoOnFREBypassedKillSwitch";
    public static final String ANDROID_SURFACE_COLOR_UPDATE = "AndroidSurfaceColorUpdate";
    public static final String ANDROID_TAB_DECLUTTER_ARCHIVE_ALL_BUT_ACTIVE =
            "AndroidTabDeclutterArchiveAllButActiveTab";
    public static final String ANDROID_TAB_DECLUTTER_ARCHIVE_TAB_GROUPS =
            "AndroidTabDeclutterArchiveTabGroups";
    public static final String ANDROID_TAB_DECLUTTER_AUTO_DELETE = "AndroidTabDeclutterAutoDelete";
    public static final String ANDROID_TAB_DECLUTTER_AUTO_DELETE_KILL_SWITCH =
            "AndroidTabDeclutterAutoDeleteKillSwitch";
    public static final String ANDROID_TAB_DECLUTTER_DEDUPE_TAB_IDS_KILL_SWITCH =
            "AndroidTabDeclutterDedupeTabIdsKillSwitch";
    public static final String ANDROID_TAB_DECLUTTER_PERFORMANCE_IMPROVEMENTS =
            "AndroidTabDeclutterPerformanceImprovements";
    public static final String ANDROID_TAB_DECLUTTER_RESCUE_KILLSWITCH =
            "AndroidTabDeclutterRescueKillswitch";
    public static final String ANDROID_TAB_GROUPS_COLOR_UPDATE_GM3 =
            "AndroidTabGroupsColorUpdateGM3";
    public static final String ANDROID_TAB_HIGHLIGHTING = "AndroidTabHighlighting";
    public static final String ANDROID_TAB_SKIP_SAVE_TABS_TASK_KILLSWITCH =
            "AndroidTabSkipSaveTabsTaskKillswitch";
    public static final String ANDROID_THEME_MODULE = "AndroidThemeModule";
    public static final String ANDROID_THEME_RESOURCE_PROVIDER = "AndroidThemeResourceProvider";
    public static final String ANDROID_TIPS_NOTIFICATIONS = "AndroidTipsNotifications";
    public static final String ANDROID_USE_ADMINS_FOR_ENTERPRISE_INFO =
            "AndroidUseAdminsForEnterpriseInfo";
    public static final String ANDROID_WEB_APP_LAUNCH_HANDLER = "AndroidWebAppLaunchHandler";
    public static final String ANDROID_WINDOW_CONTROLS_OVERLAY = "AndroidWindowControlsOverlay";
    public static final String ANDROID_WINDOW_POPUP_LARGE_SCREEN = "AndroidWindowPopupLargeScreen";
    public static final String ANDROID_WINDOW_POPUP_PREDICT_FINAL_BOUNDS =
            "AndroidWindowPopupPredictFinalBounds";
    public static final String ANDROID_WINDOW_POPUP_RESIZE_AFTER_SPAWN =
            "AndroidWindowPopupResizeAfterSpawn";
    public static final String ANDROID_XR_USES_SURFACE_CONTROL =
            "AndroidWindowPopupResizeAfterSpawn";
    public static final String ANIMATED_GIF_REFACTOR = "AnimatedGifRefactor";
    public static final String ANIMATED_IMAGE_DRAG_SHADOW = "AnimatedImageDragShadow";
    public static final String ANNOTATED_PAGE_CONTENTS_VIRTUAL_STRUCTURE =
            "AnnotatedPageContentsVirtualStructure";
    public static final String APP_SPECIFIC_HISTORY = "AppSpecificHistory";
    public static final String APP_SPECIFIC_HISTORY_VIEW_INTENT = "AppSpecificHistoryViewIntent";
    public static final String ASYNC_NOTIFICATION_MANAGER = "AsyncNotificationManager";
    public static final String ASYNC_NOTIFICATION_MANAGER_FOR_DOWNLOAD =
            "AsyncNotificationManagerForDownload";
    public static final String AUTOFILL_ALLOW_NON_HTTP_ACTIVATION =
            "AutofillAllowNonHttpActivation";
    public static final String AUTOFILL_ANDROID_DESKTOP_KEYBOARD_ACCESSORY_REVAMP =
            "AutofillAndroidDesktopKeyboardAccessoryRevamp";
    public static final String AUTOFILL_ANDROID_DESKTOP_SUPPRESS_ACCESSORY_ON_EMPTY =
            "AutofillAndroidDesktopSuppressAccessoryOnEmpty";
    public static final String AUTOFILL_DEEP_LINK_AUTOFILL_OPTIONS =
            "AutofillDeepLinkAutofillOptions";
    public static final String AUTOFILL_ENABLE_CARD_BENEFITS_FOR_AMERICAN_EXPRESS =
            "AutofillEnableCardBenefitsForAmericanExpress";
    public static final String AUTOFILL_ENABLE_CARD_BENEFITS_FOR_BMO =
            "AutofillEnableCardBenefitsForBmo";
    public static final String AUTOFILL_ENABLE_CVC_STORAGE = "AutofillEnableCvcStorageAndFilling";
    public static final String AUTOFILL_ENABLE_FLAT_RATE_CARD_BENEFITS_FROM_CURINOS =
            "AutofillEnableFlatRateCardBenefitsFromCurinos";
    public static final String AUTOFILL_ENABLE_LOCAL_IBAN = "AutofillEnableLocalIban";
    public static final String AUTOFILL_ENABLE_LOYALTY_CARDS_FILLING =
            "AutofillEnableLoyaltyCardsFilling";
    public static final String AUTOFILL_ENABLE_NEW_CARD_BENEFITS_TOGGLE_TEXT =
            "AutofillEnableNewCardBenefitsToggleText";
    public static final String AUTOFILL_ENABLE_RANKING_FORMULA_ADDRESS_PROFILES =
            "AutofillEnableRankingFormulaAddressProfiles";
    public static final String AUTOFILL_ENABLE_RANKING_FORMULA_CREDIT_CARDS =
            "AutofillEnableRankingFormulaCreditCards";
    public static final String AUTOFILL_ENABLE_SECURITY_TOUCH_EVENT_FILTERING_ANDROID =
            "AutofillEnableSecurityTouchEventFilteringAndroid";
    public static final String AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM =
            "AutofillEnableSeparatePixPreferenceItem";
    public static final String AUTOFILL_ENABLE_SERVER_IBAN = "AutofillEnableServerIban";
    public static final String AUTOFILL_ENABLE_SUPPORT_FOR_HOME_AND_WORK =
            "AutofillEnableSupportForHomeAndWork";
    public static final String AUTOFILL_ENABLE_SYNCING_OF_PIX_BANK_ACCOUNTS =
            "AutofillEnableSyncingOfPixBankAccounts";
    public static final String AUTOFILL_ENABLE_VERVE_CARD_SUPPORT =
            "AutofillEnableVerveCardSupport";
    public static final String AUTOFILL_RETRY_IMAGE_FETCH_ON_FAILURE =
            "AutofillRetryImageFetchOnFailure";
    public static final String AUTOFILL_SYNC_EWALLET_ACCOUNTS = "AutofillSyncEwalletAccounts";
    public static final String AUTOFILL_THIRD_PARTY_MODE_CONTENT_PROVIDER =
            "AutofillThirdPartyModeContentProvider";
    public static final String AUTOMOTIVE_BACK_BUTTON_BAR_STREAMLINE =
            "AutomotiveBackButtonBarStreamline";
    public static final String AUTO_PICTURE_IN_PICTURE_ANDROID = "AutoPictureInPictureAndroid";
    public static final String AVOID_RELAYOUT_DURING_FOCUS_ANIMATION =
            "AvoidRelayoutDuringFocusAnimation";
    public static final String BACKGROUND_THREAD_POOL_FIELD_TRIAL =
            "BackgroundThreadPoolFieldTrial";
    public static final String BACK_FORWARD_CACHE = "BackForwardCache";
    public static final String BACK_FORWARD_TRANSITIONS = "BackForwardTransitions";
    public static final String BATCH_TAB_RESTORE = "BatchTabRestore";
    public static final String BCIV_BOTTOM_CONTROLS = "AndroidBcivBottomControls";
    public static final String BIOMETRIC_AUTH_IDENTITY_CHECK = "BiometricAuthIdentityCheck";
    public static final String BLOCK_INTENTS_WHILE_LOCKED = "BlockIntentsWhileLocked";
    public static final String BOARDING_PASS_DETECTOR = "BoardingPassDetector";
    public static final String BOOKMARK_PANE_ANDROID = "BookmarkPaneAndroid";
    public static final String BROWSER_CONTROLS_DEBUGGING = "BrowserControlsDebugging";
    public static final String BROWSER_CONTROLS_EARLY_RESIZE = "BrowserControlsEarlyResize";
    public static final String BROWSER_CONTROLS_IN_VIZ = "AndroidBrowserControlsInViz";
    public static final String BROWSING_DATA_MODEL = "BrowsingDataModel";
    public static final String CACHE_ACTIVITY_TASKID = "CacheActivityTaskID";
    public static final String CACHE_IS_MULTI_INSTANCE_API_31_ENABLED =
            "CacheIsMultiInstanceApi31Enabled";
    public static final String CAPTIVE_PORTAL_CERTIFICATE_LIST = "CaptivePortalCertificateList";
    public static final String CCT_ADAPTIVE_BUTTON = "CCTAdaptiveButton";
    public static final String CCT_ADAPTIVE_BUTTON_TEST_SWITCH = "CCTAdaptiveButtonTestSwitch";
    public static final String CCT_AUTH_TAB = "CCTAuthTab";
    public static final String CCT_AUTH_TAB_DISABLE_ALL_EXTERNAL_INTENTS =
            "CCTAuthTabDisableAllExternalIntents";
    public static final String CCT_AUTH_TAB_ENABLE_HTTPS_REDIRECTS =
            "CCTAuthTabEnableHttpsRedirects";
    public static final String CCT_AUTO_TRANSLATE = "CCTAutoTranslate";
    public static final String CCT_BLOCK_TOUCHES_DURING_ENTER_ANIMATION =
            "CCTBlockTouchesDuringEnterAnimation";
    public static final String CCT_CLIENT_DATA_HEADER = "CCTClientDataHeader";
    public static final String CCT_CONTEXTUAL_MENU_ITEMS = "CCTContextualMenuItems";
    public static final String CCT_DESTROY_TAB_WHEN_MODEL_IS_EMPTY =
            "CCTDestroyTabWhenModelIsEmpty";
    public static final String CCT_EARLY_NAV = "CCTEarlyNav";
    public static final String CCT_EXTEND_TRUSTED_CDN_PUBLISHER = "CCTExtendTrustedCdnPublisher";
    public static final String CCT_FIX_WARMUP = "CCTFixWarmup";
    public static final String CCT_FRE_IN_SAME_TASK = "CCTFreInSameTask";
    public static final String CCT_GOOGLE_BOTTOM_BAR = "CCTGoogleBottomBar";
    public static final String CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS =
            "CCTGoogleBottomBarVariantLayouts";
    public static final String CCT_INCOGNITO_AVAILABLE_TO_THIRD_PARTY =
            "CCTIncognitoAvailableToThirdParty";
    public static final String CCT_MINIMIZED_ENABLED_BY_DEFAULT = "CCTMinimizedEnabledByDefault";
    public static final String CCT_NAVIGATIONAL_PREFETCH = "CCTNavigationalPrefetch";
    public static final String CCT_NAVIGATION_METRICS = "CCTNavigationMetrics";
    public static final String CCT_NESTED_SECURITY_ICON = "CCTNestedSecurityIcon";
    public static final String CCT_OPEN_IN_BROWSER_BUTTON_IF_ALLOWED_BY_EMBEDDER =
            "CCTOpenInBrowserButtonIfAllowedByEmbedder";
    public static final String CCT_OPEN_IN_BROWSER_BUTTON_IF_ENABLED_BY_EMBEDDER =
            "CCTOpenInBrowserButtonIfEnabledByEmbedder";
    public static final String CCT_PREDICTIVE_BACK_GESTURE = "CCTPredictiveBackGesture";
    public static final String CCT_REALTIME_ENGAGEMENT_EVENTS_IN_BACKGROUND =
            "CCTRealtimeEngagementEventsInBackground";
    public static final String CCT_REPORT_PARALLEL_REQUEST_STATUS =
            "CCTReportParallelRequestStatus";
    public static final String CCT_REPORT_PRERENDER_EVENTS = "CCTReportPrerenderEvents";
    public static final String CCT_RESET_TIMEOUT_ENABLED = "CCTResetTimeoutEnabled";
    public static final String CCT_RESIZABLE_FOR_THIRD_PARTIES = "CCTResizableForThirdParties";
    public static final String CCT_REVAMPED_BRANDING = "CCTRevampedBranding";
    public static final String CCT_SHOW_TAB_FIX = "CCTShowTabFix";
    public static final String CCT_TAB_MODAL_DIALOG = "CCTTabModalDialog";
    public static final String CCT_TOOLBAR_REFACTOR = "CCTToolbarRefactor";
    public static final String CHANGE_UNFOCUSED_PRIORITY = "ChangeUnfocusedPriority";
    public static final String CHROME_NATIVE_URL_OVERRIDING = "ChromeNativeUrlOverriding";
    public static final String CHROME_SURVEY_NEXT_ANDROID = "ChromeSurveyNextAndroid";
    public static final String CLAMP_AUTOMOTIVE_SCALING = "ClampAutomotiveScaling";
    public static final String CLANK_STARTUP_LATENCY_INJECTION = "ClankStartupLatencyInjection";
    public static final String CLANK_WHATS_NEW = "ClankWhatsNew";
    public static final String CLEANUP_LEGACY_TABSTATE = "CleanupLegacyTabState";
    public static final String CLEAR_BROWSING_DATA_ANDROID_SURVEY =
            "ClearBrowsingDataAndroidSurvey";
    public static final String CLEAR_INSTANCE_INFO_WHEN_CLOSED_INTENTIONALLY =
            "ClearInstanceInfoWhenClosedIntentionally";
    public static final String COLLECT_ANDROID_FRAME_TIMELINE_METRICS =
            "CollectAndroidFrameTimelineMetrics";
    public static final String COMMAND_LINE_ON_NON_ROOTED = "CommandLineOnNonRooted";
    public static final String COMMERCE_MERCHANT_VIEWER = "CommerceMerchantViewer";
    public static final String CONTEXTUAL_PAGE_ACTIONS = "ContextualPageActions";
    public static final String CONTEXTUAL_PAGE_ACTION_READER_MODE =
            "ContextualPageActionReaderMode";
    public static final String CONTEXTUAL_PAGE_ACTION_TAB_GROUPING =
            "ContextualPageActionTabGrouping";
    public static final String CONTEXTUAL_SEARCH_DISABLE_ONLINE_DETECTION =
            "ContextualSearchDisableOnlineDetection";
    public static final String CONTEXTUAL_SEARCH_SUPPRESS_SHORT_VIEW =
            "ContextualSearchSuppressShortView";
    public static final String CONTEXT_MENU_EMPTY_SPACE = "ContextMenuEmptySpace";
    public static final String CONTEXT_MENU_PICTURE_IN_PICTURE_ANDROID =
            "ContextMenuPictureInPictureAndroid";
    public static final String CONTEXT_MENU_TRANSLATE_WITH_GOOGLE_LENS =
            "ContextMenuTranslateWithGoogleLens";
    public static final String CONTROLS_VISIBILITY_FROM_NAVIGATIONS =
            "ControlsVisibilityFromNavigations";
    public static final String CORMORANT = "Cormorant";
    public static final String CPA_SPEC_UPDATE = "CpaSpecUpdate";
    public static final String CROSS_DEVICE_TAB_PANE_ANDROID = "CrossDeviceTabPaneAndroid";
    public static final String DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING =
            "DarkenWebsitesCheckboxInThemesSetting";
    public static final String DATA_SHARING = "DataSharing";
    public static final String DATA_SHARING_ENABLE_UPDATE_CHROME_UI =
            "DataSharingEnableUpdateChromeUI";
    public static final String DATA_SHARING_JOIN_ONLY = "DataSharingJoinOnly";
    public static final String DATA_SHARING_NON_PRODUCTION_ENVIRONMENT =
            "DataSharingNonProductionEnvironment";
    public static final String DEFAULT_BROWSER_PROMO_ANDROID2 = "DefaultBrowserPromoAndroid2";
    public static final String DESKTOP_UA_ON_CONNECTED_DISPLAY = "DesktopUAOnConnectedDisplay";
    public static final String DETAILED_LANGUAGE_SETTINGS = "DetailedLanguageSettings";
    public static final String DEVICE_AUTHENTICATOR_ANDROIDX = "DeviceAuthenticatorAndroidx";
    public static final String DISABLE_INSTANCE_LIMIT = "DisableInstanceLimit";
    public static final String DISCO_FEED_ENDPOINT = "DiscoFeedEndpoint";
    public static final String DISPLAY_EDGE_TO_EDGE_FULLSCREEN = "DisplayEdgeToEdgeFullscreen";
    public static final String DISPLAY_WILDCARD_CONTENT_SETTINGS =
            "DisplayWildcardInContentSettings";
    public static final String DRAW_CHROME_PAGES_EDGE_TO_EDGE = "DrawChromePagesEdgeToEdge";
    public static final String DRAW_CUTOUT_EDGE_TO_EDGE = "DrawCutoutEdgeToEdge";
    public static final String EDGE_TO_EDGE_BOTTOM_CHIN = "EdgeToEdgeBottomChin";
    public static final String EDGE_TO_EDGE_EVERYWHERE = "EdgeToEdgeEverywhere";
    public static final String EDGE_TO_EDGE_MONITOR_CONFIGURATIONS =
            "EdgeToEdgeMonitorConfigurations";
    public static final String EDGE_TO_EDGE_TABLET = "EdgeToEdgeTablet";
    public static final String EDGE_TO_EDGE_USE_BACKUP_NAVBAR_INSETS =
            "EdgeToEdgeUseBackupNavbarInsets";
    public static final String EDUCATIONAL_TIP_DEFAULT_BROWSER_PROMO_CARD =
            "EducationalTipDefaultBrowserPromoCard";
    public static final String EDUCATIONAL_TIP_MODULE = "EducationalTipModule";
    public static final String EMPTY_TAB_LIST_ANIMATION_KILL_SWITCH =
            "EmptyTabListAnimationKillSwitch";
    public static final String ENABLE_CLIPBOARD_DATA_CONTROLS_ANDROID =
            "EnableClipboardDataControlsAndroid";
    public static final String ENABLE_DISCOUNT_INFO_API = "EnableDiscountInfoApi";
    public static final String ENABLE_EXCLUSIVE_ACCESS_MANAGER = "EnableExclusiveAccessManager";
    public static final String ENABLE_FULLSCREEN_TO_ANY_SCREEN_ANDROID =
            "EnableFullscreenToAnyScreenAndroid";
    public static final String ENABLE_SAVE_PACKAGE_FOR_OFF_THE_RECORD =
            "EnableSavePackageForOffTheRecord";
    public static final String ENABLE_X_AXIS_ACTIVITY_TRANSITION = "EnableXAxisActivityTransition";
    public static final String FACILITATED_PAYMENTS_ENABLE_A2A_PAYMENT =
            "FacilitatedPaymentsEnableA2APayment";
    public static final String FEED_AUDIO_OVERVIEWS = "FeedAudioOverviews";
    public static final String FEED_CONTAINMENT = "FeedContainment";
    public static final String FEED_FOLLOW_UI_UPDATE = "FeedFollowUiUpdate";
    public static final String FEED_HEADER_REMOVAL = "FeedHeaderRemoval";
    public static final String FEED_IMAGE_MEMORY_CACHE_SIZE_PERCENTAGE =
            "FeedImageMemoryCacheSizePercentage";
    public static final String FEED_LOADING_PLACEHOLDER = "FeedLoadingPlaceholder";
    public static final String FEED_SHOW_SIGN_IN_COMMAND = "FeedShowSignInCommand";
    public static final String FILLING_PASSWORDS_FROM_ANY_ORIGIN = "FillingPasswordsFromAnyOrigin";
    public static final String FINGERPRINTING_PROTECTION_UX = "FingerprintingProtectionUx";
    public static final String FLOATING_SNACKBAR = "FloatingSnackbar";
    public static final String FORCE_BROWSER_CONTROLS_UPON_EXITING_FULLSCREEN =
            "ForceBrowserControlsUponExitingFullscreen";
    public static final String FORCE_LIST_TAB_SWITCHER = "ForceListTabSwitcher";
    public static final String FORCE_TRANSLUCENT_NOTIFICATION_TRAMPOLINE =
            "ForceTranslucentNotificationTrampoline";
    public static final String FORCE_WEB_CONTENTS_DARK_MODE = "WebContentsForceDark";
    public static final String FULLSCREEN_INSETS_API_MIGRATION = "FullscreenInsetsApiMigration";
    public static final String FULLSCREEN_INSETS_API_MIGRATION_ON_AUTOMOTIVE =
            "FullscreenInsetsApiMigrationOnAutomotive";
    public static final String GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE =
            "GridTabSwitcherSurfaceColorUpdate";
    public static final String GRID_TAB_SWITCHER_UPDATE = "GridTabSwitcherUpdate";
    public static final String GROUP_NEW_TAB_WITH_PARENT = "GroupNewTabWithParent";
    public static final String GROUP_SUGGESTION_SERVICE = "GroupSuggestionService";
    public static final String HASH_PREFIX_REAL_TIME_LOOKUPS =
            "SafeBrowsingHashPrefixRealTimeLookups";
    public static final String HEADLESS_TAB_MODEL = "HeadlessTabModel";
    public static final String HISTORY_JOURNEYS = "Journeys";
    public static final String HISTORY_PANE_ANDROID = "HistoryPaneAndroid";
    public static final String HOMEPAGE_IS_NEW_TAB_PAGE_POLICY_ANDROID =
            "HomepageIsNewTabPagePolicyAndroid";
    public static final String HTTPS_FIRST_BALANCED_MODE = "HttpsFirstBalancedMode";
    public static final String HUB_BACK_BUTTON = "HubBackButton";
    public static final String HUB_SLIDE_ANIMATION = "HubSlideAnimation";
    public static final String INCOGNITO_SCREENSHOT = "IncognitoScreenshot";
    public static final String INCOGNITO_THEME_OVERLAY_TESTING = "IncognitoThemeOverlayTesting";
    public static final String INSTANCE_SWITCHER_V2 = "InstanceSwitcherV2";
    public static final String IP_PROTECTION_UX = "IpProtectionUx";
    public static final String KEYBOARD_ESC_BACK_NAVIGATION = "KeyboardEscBackNavigation";
    public static final String LEGACY_TAB_STATE_DEPRECATION = "LegacyTabStateDeprecation";
    public static final String LENS_ON_QUICK_ACTION_SEARCH_WIDGET = "LensOnQuickActionSearchWidget";
    public static final String LINKED_SERVICES_SETTING = "LinkedServicesSetting";
    public static final String LOADING_PREDICTOR_LIMIT_PRECONNECT_SOCKET_COUNT =
            "LoadingPredictorLimitPreconnectSocketCount";
    public static final String LOAD_NATIVE_EARLY = "LoadNativeEarly";
    public static final String LOCAL_NETWORK_ACCESS = "LocalNetworkAccessChecks";
    public static final String LOCK_BACK_PRESS_HANDLER_AT_START = "LockBackPressHandlerAtStart";
    public static final String LOCK_TOP_CONTROLS_ON_LARGE_TABLETS = "LockTopControlsOnLargeTablets";
    public static final String LOOKALIKE_NAVIGATION_URL_SUGGESTIONS_UI =
            "LookalikeUrlNavigationSuggestionsUI";
    public static final String MAGIC_STACK_ANDROID = "MagicStackAndroid";
    public static final String MALICIOUS_APK_DOWNLOAD_CHECK = "MaliciousApkDownloadCheck";
    public static final String MAYLAUNCHURL_USES_SEPARATE_STORAGE_PARTITION =
            "MayLaunchUrlUsesSeparateStoragePartition";
    public static final String MEDIA_INDICATORS_ANDROID = "MediaIndicatorsAndroid";
    public static final String MINI_ORIGIN_BAR = "MiniOriginBar";
    public static final String MITIGATE_LEGACY_SEARCH_ENGINE_PROMO_OVERLAP =
            "MitigateLegacySearchEnginePromoOverlap";
    public static final String MOST_VISITED_TILES_CUSTOMIZATION = "MostVisitedTilesCustomization";
    public static final String MOST_VISITED_TILES_RESELECT = "MostVisitedTilesReselect";
    public static final String MULTI_INSTANCE_APPLICATION_STATUS_CLEANUP =
            "MultiInstanceApplicationStatusCleanup";
    public static final String MVC_UPDATE_VIEW_WHEN_MODEL_CHANGED = "MvcUpdateViewWhenModelChanged";
    public static final String NATIVE_PAGE_TRANSITION_HARDWARE_CAPTURE =
            "NativePageTransitionHardwareCapture";
    public static final String NAV_BAR_COLOR_ANIMATION = "NavBarColorAnimation";
    public static final String NEW_TAB_PAGE_CUSTOMIZATION = "NewTabPageCustomization";
    public static final String NEW_TAB_PAGE_CUSTOMIZATION_FOR_MVT = "NewTabPageCustomizationForMvt";
    public static final String NEW_TAB_PAGE_CUSTOMIZATION_TOOLBAR_BUTTON =
            "NewTabPageCustomizationToolbarButton";
    public static final String NEW_TAB_PAGE_CUSTOMIZATION_V2 = "NewTabPageCustomizationV2";
    public static final String NOTIFICATION_ONE_TAP_UNSUBSCRIBE = "NotificationOneTapUnsubscribe";
    public static final String NOTIFICATION_PERMISSION_BOTTOM_SHEET =
            "NotificationPermissionBottomSheet";
    public static final String NOTIFICATION_PERMISSION_VARIANT = "NotificationPermissionVariant";
    public static final String NOTIFICATION_TRAMPOLINE = "NotificationTrampoline";
    public static final String OMAHA_MIN_SDK_VERSION_ANDROID = "OmahaMinSdkVersionAndroid";
    public static final String OMNIBOX_CACHE_SUGGESTION_RESOURCES =
            "OmniboxCacheSuggestionResources";
    public static final String OPTIMIZATION_GUIDE_PUSH_NOTIFICATIONS =
            "OptimizationGuidePushNotifications";
    public static final String PAGE_CONTENT_PROVIDER = "PageContentProvider";
    public static final String PAGE_INFO_ABOUT_THIS_SITE_MORE_LANGS =
            "PageInfoAboutThisSiteMoreLangs";
    public static final String PAINT_PREVIEW_DEMO = "PaintPreviewDemo";
    public static final String PARTNER_CUSTOMIZATIONS_UMA = "PartnerCustomizationsUma";
    public static final String PASSWORD_FORM_GROUPED_AFFILIATIONS =
            "PasswordFormGroupedAffiliations";
    public static final String PCCT_MINIMUM_HEIGHT = "PCCTMinimumHeight";
    public static final String PERMISSION_DEDICATED_CPSS_SETTING_ANDROID =
            "PermissionDedicatedCpssSettingAndroid";
    public static final String PERMISSION_SITE_SETTING_RADIO_BUTTON =
            "PermissionSiteSettingsRadioButton";
    public static final String PLUS_ADDRESSES_ENABLED = "PlusAddressesEnabled";
    public static final String PLUS_ADDRESS_ANDROID_OPEN_GMS_CORE_MANAGEMENT_PAGE =
            "PlusAddressAndroidOpenGmsCoreManagementPage";
    public static final String POST_GET_MEMORY_PRESSURE_TO_BACKGROUND =
            BaseFeatures.POST_GET_MY_MEMORY_STATE_TO_BACKGROUND;
    public static final String POWER_SAVING_MODE_BROADCAST_RECEIVER_IN_BACKGROUND =
            "PowerSavingModeBroadcastReceiverInBackground";
    public static final String PRECONNECT_ON_TAB_CREATION = "PreconnectOnTabCreation";
    public static final String PRERENDER2 = "Prerender2";
    public static final String PRICE_ANNOTATIONS = "PriceAnnotations";
    public static final String PRICE_CHANGE_MODULE = "PriceChangeModule";
    public static final String PRIVACY_SANDBOX_ACTIVITY_TYPE_STORAGE =
            "PrivacySandboxActivityTypeStorage";
    public static final String PRIVACY_SANDBOX_ADS_API_UX_ENHANCEMENTS =
            "PrivacySandboxAdsApiUxEnhancements";
    public static final String PRIVACY_SANDBOX_ADS_NOTICE_CCT = "PrivacySandboxAdsNoticeCCT";
    public static final String PRIVACY_SANDBOX_AD_TOPICS_CONTENT_PARITY =
            "PrivacySandboxAdTopicsContentParity";
    public static final String PRIVACY_SANDBOX_SENTIMENT_SURVEY = "PrivacySandboxSentimentSurvey";
    public static final String PRIVACY_SANDBOX_SETTINGS_4 = "PrivacySandboxSettings4";
    public static final String PROCESS_RANK_POLICY_ANDROID = "ProcessRankPolicyAndroid";
    // Do not access directly, use SupervisedUserService::IsLocallySupervised() or
    // supervised_user::UseLocalSupervision() instead. Exposed only for testing.
    public static final String PROPAGATE_DEVICE_CONTENT_FILTERS_TO_SUPERVISED_USER =
            "PropagateDeviceContentFiltersToSupervisedUser";
    public static final String PUSH_MESSAGING_DISALLOW_SENDER_IDS =
            "PushMessagingDisallowSenderIDs";
    public static final String PWA_RESTORE_UI = "PwaRestoreUi";
    public static final String PWA_RESTORE_UI_AT_STARTUP = "PwaRestoreUiAtStartup";
    public static final String PWA_UPDATE_DIALOG_FOR_ICON = "PwaUpdateDialogForIcon";
    public static final String QUICK_DELETE_ANDROID_SURVEY = "QuickDeleteAndroidSurvey";
    public static final String QUIET_NOTIFICATION_PROMPTS = "QuietNotificationPrompts";
    public static final String READALOUD = "ReadAloud";
    public static final String READALOUD_AUDIO_OVERVIEWS = "ReadAloudAudioOverviews";
    public static final String READALOUD_AUDIO_OVERVIEWS_FEEDBACK =
            "ReadAloudAudioOverviewsFeedback";
    public static final String READALOUD_AUDIO_OVERVIEWS_SKIP_DISCLAIMER_WHEN_POSSIBLE =
            "ReadAloudAudioOverviewsSkipDisclaimerWhenPossible";
    public static final String READALOUD_BACKGROUND_PLAYBACK = "ReadAloudBackgroundPlayback";
    public static final String READALOUD_IN_MULTI_WINDOW = "ReadAloudInMultiWindow";
    public static final String READALOUD_IN_OVERFLOW_MENU_IN_CCT = "ReadAloudInOverflowMenuInCCT";
    public static final String READALOUD_IPH_MENU_BUTTON_HIGHLIGHT_CCT =
            "ReadAloudIPHMenuButtonHighlightCCT";
    public static final String READALOUD_PLAYBACK = "ReadAloudPlayback";
    public static final String READALOUD_TAP_TO_SEEK = "ReadAloudTapToSeek";
    public static final String RECORD_SUPPRESSION_METRICS = "RecordSuppressionMetrics";
    public static final String REENGAGEMENT_NOTIFICATION = "ReengagementNotification";
    public static final String RELATED_SEARCHES_ALL_LANGUAGE = "RelatedSearchesAllLanguage";
    public static final String RELATED_SEARCHES_SWITCH = "RelatedSearchesSwitch";
    public static final String RELATED_WEBSITE_SETS_UI = "RelatedWebsiteSetsUi";
    public static final String REMOVE_TAB_FOCUS_ON_SHOWING_AND_SELECT =
            "RemoveTabFocusOnShowingAndSelect";
    public static final String RENAME_JOURNEYS = "RenameJourneys";
    public static final String REPORT_NOTIFICATION_CONTENT_DETECTION_DATA =
            "ReportNotificationContentDetectionData";
    public static final String RIGHT_EDGE_GOES_FORWARD_GESTURE_NAV =
            "RightEdgeGoesForwardGestureNav";
    public static final String ROBUST_WINDOW_MANAGEMENT = "RobustWindowManagement";
    public static final String ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL =
            "RobustWindowManagementExperimental";
    public static final String SAFETY_HUB = "SafetyHub";
    public static final String SAFETY_HUB_ANDROID_ORGANIC_SURVEY = "SafetyHubAndroidOrganicSurvey";
    public static final String SAFETY_HUB_ANDROID_SURVEY = "SafetyHubAndroidSurvey";
    public static final String SAFETY_HUB_ANDROID_SURVEY_V2 = "SafetyHubAndroidSurveyV2";
    public static final String SAFETY_HUB_DISRUPTIVE_NOTIFICATION_REVOCATION =
            "SafetyHubDisruptiveNotificationRevocation";
    public static final String SAFETY_HUB_FOLLOWUP = "SafetyHubFollowup";
    public static final String SAFETY_HUB_LOCAL_PASSWORDS_MODULE = "SafetyHubLocalPasswordsModule";
    public static final String SAFETY_HUB_MAGIC_STACK = "SafetyHubMagicStack";
    public static final String SAFETY_HUB_UNIFIED_PASSWORDS_MODULE =
            "SafetyHubUnifiedPasswordsModule";
    public static final String SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS =
            "SafetyHubWeakAndReusedPasswords";
    public static final String SAFE_BROWSING_DELAYED_WARNINGS = "SafeBrowsingDelayedWarnings";
    public static final String SAFE_BROWSING_EXTENDED_REPORTING_REMOVE_PREF_DEPENDENCY =
            "ExtendedReportingRemovePrefDependency";
    public static final String SEARCH_IN_CCT = "SearchInCCT";
    public static final String SEARCH_IN_CCT_ALTERNATE_TAP_HANDLING =
            "SearchInCCTAlternateTapHandling";
    public static final String SEARCH_IN_CCT_ALTERNATE_TAP_HANDLING_IF_ENABLED_BY_EMBEDDER =
            "SearchInCCTAlternateTapHandlingIfEnabledByEmbedder";
    public static final String SEARCH_IN_CCT_IF_ENABLED_BY_EMBEDDER =
            "SearchInCCTIfEnabledByEmbedder";
    public static final String SEARCH_RESUMPTION_MODULE_ANDROID = "SearchResumptionModuleAndroid";
    public static final String SEED_ACCOUNTS_REVAMP = "SeedAccountsRevamp";
    public static final String SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER =
            "SegmentationPlatformAndroidHomeModuleRanker";
    public static final String SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER_V2 =
            "SegmentationPlatformAndroidHomeModuleRankerV2";
    public static final String SEGMENTATION_PLATFORM_EPHEMERAL_CARD_RANKER =
            "SegmentationPlatformEphemeralCardRanker";
    public static final String SENSITIVE_CONTENT = "SensitiveContent";
    public static final String SENSITIVE_CONTENT_WHILE_SWITCHING_TABS =
            "SensitiveContentWhileSwitchingTabs";
    public static final String SETTINGS_SINGLE_ACTIVITY = "SettingsSingleActivity";
    public static final String SHARED_DATA_TYPES_KILL_SWITCH = "SharedDataTypesKillSwitch";
    public static final String SHARE_CUSTOM_ACTIONS_IN_CCT = "ShareCustomActionsInCCT";
    public static final String SHOW_CLOSE_ALL_INCOGNITO_TABS_BUTTON =
            "ShowCloseAllIncognitoTabsButton";
    public static final String SHOW_HOME_BUTTON_POLICY_ANDROID = "ShowHomeButtonPolicyAndroid";
    public static final String SHOW_NEW_TAB_ANIMATIONS = "ShowNewTabAnimations";
    public static final String SHOW_TAB_LIST_ANIMATIONS = "ShowTabListAnimations";
    public static final String SHOW_WARNINGS_FOR_SUSPICIOUS_NOTIFICATIONS =
            "ShowWarningsForSuspiciousNotifications";
    public static final String SMALLER_TAB_STRIP_TITLE_LIMIT = "SmallerTabStripTitleLimit";
    public static final String SMART_SUGGESTION_FOR_LARGE_DOWNLOADS =
            "SmartSuggestionForLargeDownloads";
    public static final String SPLIT_CACHE_BY_NETWORK_ISOLATION_KEY =
            "SplitCacheByNetworkIsolationKey";
    public static final String START_SURFACE_RETURN_TIME = "StartSurfaceReturnTime";
    public static final String STOP_APP_INDEXING_REPORT = "StopAppIndexingReport";
    public static final String SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP =
            "SubmenusTabContextMenuLffTabStrip";
    public static final String SUGGESTION_ANSWERS_COLOR_REVERSE = "SuggestionAnswersColorReverse";
    public static final String SUPPRESS_TOOLBAR_CAPTURES_AT_GESTURE_END =
            "SuppressToolbarCapturesAtGestureEnd";
    public static final String SYNC_ENABLE_PASSWORDS_SYNC_ERROR_MESSAGE_ALTERNATIVE =
            "SyncEnablePasswordsSyncErrorMessageAlternative";
    public static final String TABLET_TAB_STRIP_ANIMATION = "TabletTabStripAnimation";
    public static final String TAB_ARCHIVAL_DRAG_DROP_ANDROID = "TabArchivalDragDropAndroid";
    public static final String TAB_CLOSURE_METHOD_REFACTOR = "TabClosureMethodRefactor";
    public static final String TAB_COLLECTION_ANDROID = "TabCollectionAndroid";
    public static final String TAB_FREEZE_ON_UNDOABLE_CLOSURE_KILL_SWITCH =
            "TabFreezeOnUndoableClosureKillSwitch";
    public static final String TAB_GROUP_ANDROID_VISUAL_DATA_CLEANUP =
            "TabGroupAndroidVisualDataCleanup";
    public static final String TAB_GROUP_ENTRY_POINTS_ANDROID = "TabGroupEntryPointsAndroid";
    public static final String TAB_GROUP_PARITY_BOTTOM_SHEET_ANDROID =
            "TabGroupParityBottomSheetAndroid";
    public static final String TAB_MODEL_INIT_FIXES = "TabModelInitFixes";
    public static final String TAB_STORAGE_SQLITE_PROTOTYPE = "TabStorageSqlitePrototype";
    public static final String TAB_STRIP_AUTO_SELECT_ON_CLOSE_CHANGE =
            "TabStripAutoSelectOnCloseChange";
    public static final String TAB_STRIP_DENSITY_CHANGE_ANDROID = "TabStripDensityChangeAndroid";
    public static final String TAB_STRIP_GROUP_DRAG_DROP_ANDROID = "TabStripGroupDragDropAndroid";
    public static final String TAB_STRIP_INCOGNITO_MIGRATION = "TabStripIncognitoMigration";
    public static final String TAB_STRIP_LAYOUT_OPTIMIZATION = "TabStripLayoutOptimization";
    public static final String TAB_STRIP_MOUSE_CLOSE_RESIZE_DELAY = "TabStripMouseCloseResizeDelay";
    public static final String TAB_STRIP_TRANSITION_IN_DESKTOP_WINDOW =
            "TabStripTransitionInDesktopWindow";
    public static final String TAB_SWITCHER_DRAG_DROP_ANDROID = "TabSwitcherDragDropAndroid";
    public static final String TAB_SWITCHER_FOREIGN_FAVICON_SUPPORT =
            "TabSwitcherForeignFaviconSupport";
    public static final String TAB_SWITCHER_GROUP_SUGGESTIONS_ANDROID =
            "TabSwitcherGroupSuggestionsAndroid";
    public static final String TAB_SWITCHER_GROUP_SUGGESTIONS_TEST_MODE_ANDROID =
            "TabSwitcherGroupSuggestionsTestModeAndroid";
    public static final String TAB_WINDOW_MANAGER_REPORT_INDICES_MISMATCH =
            "TabWindowManagerReportIndicesMismatch";
    public static final String TASK_MANAGER_CLANK = "TaskManagerClank";
    public static final String TEST_DEFAULT_DISABLED = "TestDefaultDisabled";
    public static final String TEST_DEFAULT_ENABLED = "TestDefaultEnabled";
    public static final String THIRD_PARTY_DISABLE_CHROME_AUTOFILL_SETTINGS_SCREEN =
            "ThirdPartyDisableChromeAutofillSettingsScreen";
    public static final String TINKER_TANK_BOTTOM_SHEET = "TinkerTankBottomSheet";
    public static final String TOOLBAR_PHONE_ANIMATION_REFACTOR = "ToolbarPhoneAnimationRefactor";
    public static final String TOOLBAR_SCROLL_ABLATION = "AndroidToolbarScrollAblation";
    public static final String TOOLBAR_SNAPSHOT_REFACTOR = "ToolbarSnapshotRefactor";
    public static final String TOOLBAR_TABLET_RESIZE_REFACTOR = "ToolbarTabletResizeRefactor";
    public static final String TOP_CONTROLS_REFACTOR = "TopControlsRefactor";
    public static final String TOUCH_TO_SEARCH_CALLOUT = "TouchToSearchCallout";
    public static final String TRACE_BINDER_IPC = "TraceBinderIpc";
    public static final String TRACKING_PROTECTION_3PCD = "TrackingProtection3pcd";
    public static final String TRACKING_PROTECTION_USER_BYPASS_PWA =
            "TrackingProtectionUserBypassPwa";
    public static final String TRACKING_PROTECTION_USER_BYPASS_PWA_TRIGGER =
            "TrackingProtectionUserBypassPwaTrigger";
    public static final String TRANSLATE_MESSAGE_UI = "TranslateMessageUI";
    public static final String TRANSLATE_TFLITE = "TFLiteLanguageDetectionEnabled";
    public static final String UMA_SESSION_CORRECTNESS_FIXES = "UmaSessionCorrectnessFixes";
    public static final String UNO_PHASE_2_FOLLOW_UP = "UnoPhase2FollowUp";
    public static final String UPDATE_COMPOSTIROR_FOR_SURFACE_CONTROL =
            "UpdateCompositorForSurfaceControl";
    public static final String USE_ACTIVITY_MANAGER_FOR_TAB_ACTIVATION =
            "UseActivityManagerForTabActivation";
    public static final String USE_ALTERNATE_HISTORY_SYNC_ILLUSTRATION =
            "UseAlternateHistorySyncIllustration";
    public static final String USE_CHIME_ANDROID_SDK = "UseChimeAndroidSdk";
    public static final String USE_INITIAL_NETWORK_STATE_AT_STARTUP =
            "UseInitialNetworkStateAtStartup";
    public static final String USE_LIBUNWINDSTACK_NATIVE_UNWINDER_ANDROID =
            "UseLibunwindstackNativeUnwinderAndroid";
    public static final String VISITED_URL_RANKING_SERVICE = "VisitedURLRankingService";
    public static final String WEB_APK_BACKUP_AND_RESTORE_BACKEND = "WebApkBackupAndRestoreBackend";
    public static final String WEB_APK_INSTALL_FAILURE_NOTIFICATION =
            "WebApkInstallFailureNotification";
    public static final String WEB_APK_MIN_SHELL_APK_VERSION = "WebApkMinShellVersion";
    public static final String WEB_FEED_AWARENESS = "WebFeedAwareness";
    public static final String WEB_FEED_ONBOARDING = "WebFeedOnboarding";
    public static final String WEB_FEED_SORT = "WebFeedSort";
    public static final String WEB_OTP_CROSS_DEVICE_SIMPLE_STRING = "WebOtpCrossDeviceSimpleString";
    public static final String XSURFACE_METRICS_REPORTING = "XsurfaceMetricsReporting";
    // keep-sorted end

    // keep-sorted start group_prefixes=["public static final CachedFlag"]
    public static final CachedFlag sAccountForSuppressedKeyboardInsets =
            newCachedFlag(ACCOUNT_FOR_SUPPRESSED_KEYBOARD_INSETS, /* defaultValue= */ true);
    public static final CachedFlag sAllowTabClosingUponMinimization =
            newCachedFlag(
                    ALLOW_TAB_CLOSING_UPON_MINIMIZATION,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sAndroidAnimatedProgressBarInBrowser =
            newCachedFlag(ANDROID_ANIMATED_PROGRESS_BAR_IN_BROWSER, false);
    public static final CachedFlag sAndroidAnimatedProgressBarInViz =
            newCachedFlag(ANDROID_ANIMATED_PROGRESS_BAR_IN_VIZ, false);
    public static final CachedFlag sAndroidAppIntegration =
            newCachedFlag(ANDROID_APP_INTEGRATION, true);
    public static final CachedFlag sAndroidAppIntegrationModule =
            newCachedFlag(ANDROID_APP_INTEGRATION_MODULE, true);
    public static final CachedFlag sAndroidAppIntegrationMultiDataSource =
            newCachedFlag(ANDROID_APP_INTEGRATION_MULTI_DATA_SOURCE, true);
    public static final CachedFlag sAndroidAppIntegrationV2 =
            newCachedFlag(ANDROID_APP_INTEGRATION_V2, true);
    public static final CachedFlag sAndroidAppIntegrationWithFavicon =
            newCachedFlag(ANDROID_APP_INTEGRATION_WITH_FAVICON, true);
    public static final CachedFlag sAndroidBottomToolbar =
            newCachedFlag(ANDROID_BOTTOM_TOOLBAR, true, true);
    public static final CachedFlag sAndroidBottomToolbarV2 =
            newCachedFlag(ANDROID_BOTTOM_TOOLBAR_V2, false, false);
    public static final CachedFlag sAndroidComposeplate =
            newCachedFlag(ANDROID_COMPOSEPLATE, false, true);
    public static final CachedFlag sAndroidDataImporterService =
            newCachedFlag(ANDROID_DATA_IMPORTER_SERVICE, true);
    public static final CachedFlag sAndroidDesktopDensity =
            newCachedFlag(ANDROID_DESKTOP_DENSITY, false);
    public static final CachedFlag sAndroidElegantTextHeight =
            newCachedFlag(ANDROID_ELEGANT_TEXT_HEIGHT, true);
    public static final CachedFlag sAndroidMinimalUiLargeScreen =
            newCachedFlag(ANDROID_MINIMAL_UI_LARGE_SCREEN, true);
    public static final CachedFlag sAndroidOpenIncognitoAsWindow =
            newCachedFlag(ANDROID_OPEN_INCOGNITO_AS_WINDOW, false);
    public static final CachedFlag sAndroidProgressBarVisualUpdate =
            newCachedFlag(
                    ANDROID_PROGRESS_BAR_VISUAL_UPDATE,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sAndroidSettingsContainment =
            newCachedFlag(ANDROID_SETTINGS_CONTAINMENT, false);
    public static final CachedFlag sAndroidSurfaceColorUpdate =
            newCachedFlag(
                    ANDROID_SURFACE_COLOR_UPDATE,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ false);
    public static final CachedFlag sAndroidTabDeclutterDedupeTabIdsKillSwitch =
            newCachedFlag(ANDROID_TAB_DECLUTTER_DEDUPE_TAB_IDS_KILL_SWITCH, true);
    public static final CachedFlag sAndroidTabGroupsColorUpdateGm3 =
            newCachedFlag(
                    ANDROID_TAB_GROUPS_COLOR_UPDATE_GM3, false, /* defaultValueInTests= */ true);
    public static final CachedFlag sAndroidTabSkipSaveTabsKillswitch =
            newCachedFlag(ANDROID_TAB_SKIP_SAVE_TABS_TASK_KILLSWITCH, true, true);
    public static final CachedFlag sAndroidThemeModule =
            newCachedFlag(ANDROID_THEME_MODULE, false, /* defaultValueInTests= */ true);
    public static final CachedFlag sAndroidThemeResourceProvider =
            newCachedFlag(ANDROID_THEME_RESOURCE_PROVIDER, false, /* defaultValueInTests= */ false);
    public static final CachedFlag sAndroidUseAdminsForEnterpriseInfo =
            newCachedFlag(
                    ANDROID_USE_ADMINS_FOR_ENTERPRISE_INFO,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sAndroidWebAppLaunchHandler =
            newCachedFlag(ANDROID_WEB_APP_LAUNCH_HANDLER, false, true);
    public static final CachedFlag sAndroidWindowControlsOverlay =
            newCachedFlag(ANDROID_WINDOW_CONTROLS_OVERLAY, false);
    public static final CachedFlag sAndroidWindowPopupLargeScreen =
            newCachedFlag(ANDROID_WINDOW_POPUP_LARGE_SCREEN, false, true);
    public static final CachedFlag sAndroidWindowPopupPredictFinalBounds =
            newCachedFlag(ANDROID_WINDOW_POPUP_PREDICT_FINAL_BOUNDS, false);
    public static final CachedFlag sAndroidWindowPopupResizeAfterSpawn =
            newCachedFlag(ANDROID_WINDOW_POPUP_RESIZE_AFTER_SPAWN, false);
    public static final CachedFlag sAppSpecificHistory = newCachedFlag(APP_SPECIFIC_HISTORY, true);
    public static final CachedFlag sAppSpecificHistoryViewIntent =
            newCachedFlag(APP_SPECIFIC_HISTORY_VIEW_INTENT, true);
    public static final CachedFlag sAsyncNotificationManager =
            newCachedFlag(ASYNC_NOTIFICATION_MANAGER, false, true);
    public static final CachedFlag sAsyncNotificationManagerForDownload =
            newCachedFlag(ASYNC_NOTIFICATION_MANAGER_FOR_DOWNLOAD, true);
    public static final CachedFlag sAutomotiveBackButtonBarStreamline =
            newCachedFlag(
                    AUTOMOTIVE_BACK_BUTTON_BAR_STREAMLINE,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sBackgroundThreadPoolFieldTrial =
            newCachedFlag(
                    BACKGROUND_THREAD_POOL_FIELD_TRIAL,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sBatchTabRestore = newCachedFlag(BATCH_TAB_RESTORE, true);
    public static final CachedFlag sBlockIntentsWhileLocked =
            newCachedFlag(BLOCK_INTENTS_WHILE_LOCKED, false);
    public static final CachedFlag sBookmarkPaneAndroid =
            newCachedFlag(BOOKMARK_PANE_ANDROID, false);
    public static final CachedFlag sBrowserControlsDebugging =
            newCachedFlag(BROWSER_CONTROLS_DEBUGGING, false);
    public static final CachedFlag sCacheIsMultiInstanceApi31Enabled =
            newCachedFlag(CACHE_IS_MULTI_INSTANCE_API_31_ENABLED, true);
    public static final CachedFlag sCctAdaptiveButton =
            newCachedFlag(
                    CCT_ADAPTIVE_BUTTON,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sCctAuthTab = newCachedFlag(CCT_AUTH_TAB, true);
    public static final CachedFlag sCctAuthTabDisableAllExternalIntents =
            newCachedFlag(CCT_AUTH_TAB_DISABLE_ALL_EXTERNAL_INTENTS, false);
    public static final CachedFlag sCctAuthTabEnableHttpsRedirects =
            newCachedFlag(CCT_AUTH_TAB_ENABLE_HTTPS_REDIRECTS, true);
    public static final CachedFlag sCctAutoTranslate = newCachedFlag(CCT_AUTO_TRANSLATE, true);
    public static final CachedFlag sCctBlockTouchesDuringEnterAnimation =
            newCachedFlag(CCT_BLOCK_TOUCHES_DURING_ENTER_ANIMATION, true);
    public static final CachedFlag sCctContextualMenuItems =
            newCachedFlag(CCT_CONTEXTUAL_MENU_ITEMS, false);
    public static final CachedFlag sCctDestroyTabWhenModelIsEmpty =
            newCachedFlag(CCT_DESTROY_TAB_WHEN_MODEL_IS_EMPTY, true);
    public static final CachedFlag sCctFixWarmup =
            newCachedFlag(CCT_FIX_WARMUP, /* defaultValue= */ true);
    public static final CachedFlag sCctFreInSameTask = newCachedFlag(CCT_FRE_IN_SAME_TASK, true);
    public static final CachedFlag sCctGoogleBottomBar =
            newCachedFlag(
                    CCT_GOOGLE_BOTTOM_BAR,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sCctGoogleBottomBarVariantLayouts =
            newCachedFlag(CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS, false);
    public static final CachedFlag sCctIncognitoAvailableToThirdParty =
            newCachedFlag(CCT_INCOGNITO_AVAILABLE_TO_THIRD_PARTY, false);
    public static final CachedFlag sCctNavigationalPrefetch =
            newCachedFlag(
                    CCT_NAVIGATIONAL_PREFETCH,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sCctNestedSecurityIcon =
            newCachedFlag(CCT_NESTED_SECURITY_ICON, true);
    public static final CachedFlag sCctOpenInBrowserButtonIfAllowedByEmbedder =
            newCachedFlag(CCT_OPEN_IN_BROWSER_BUTTON_IF_ALLOWED_BY_EMBEDDER, false);
    public static final CachedFlag sCctOpenInBrowserButtonIfEnabledByEmbedder =
            newCachedFlag(CCT_OPEN_IN_BROWSER_BUTTON_IF_ENABLED_BY_EMBEDDER, true);
    public static final CachedFlag sCctPredictiveBackGesture =
            newCachedFlag(
                    CCT_PREDICTIVE_BACK_GESTURE,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sCctRealtimeEngagementEventsInBackground =
            newCachedFlag(CCT_REALTIME_ENGAGEMENT_EVENTS_IN_BACKGROUND, true);
    public static final CachedFlag sCctResetTimeoutEnabled =
            newCachedFlag(CCT_RESET_TIMEOUT_ENABLED, false);
    public static final CachedFlag sCctResizableForThirdParties =
            newCachedFlag(CCT_RESIZABLE_FOR_THIRD_PARTIES, true);
    public static final CachedFlag sCctRevampedBranding =
            newCachedFlag(CCT_REVAMPED_BRANDING, true);
    public static final CachedFlag sCctTabModalDialog = newCachedFlag(CCT_TAB_MODAL_DIALOG, true);
    public static final CachedFlag sCctToolbarRefactor =
            newCachedFlag(CCT_TOOLBAR_REFACTOR, false, true);
    public static final CachedFlag sClampAutomotiveScaling =
            newCachedFlag(CLAMP_AUTOMOTIVE_SCALING, true);
    public static final CachedFlag sClankStartupLatencyInjection =
            newCachedFlag(CLANK_STARTUP_LATENCY_INJECTION, false);
    public static final CachedFlag sCleanupLegacyTabState =
            newCachedFlag(
                    CLEANUP_LEGACY_TABSTATE,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sCollectAndroidFrameTimelineMetrics =
            newCachedFlag(
                    COLLECT_ANDROID_FRAME_TIMELINE_METRICS,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sCommandLineOnNonRooted =
            newCachedFlag(COMMAND_LINE_ON_NON_ROOTED, false);
    public static final CachedFlag sContextMenuPictureInPictureAndroid =
            newCachedFlag(CONTEXT_MENU_PICTURE_IN_PICTURE_ANDROID, false);
    public static final CachedFlag sCpaSpecUpdate =
            newCachedFlag(
                    CPA_SPEC_UPDATE, /* defaultValue= */ false, /* defaultValueInTests= */ true);
    public static final CachedFlag sCpaTabGroupingButton =
            newCachedFlag(
                    CONTEXTUAL_PAGE_ACTION_TAB_GROUPING,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sCrossDeviceTabPaneAndroid =
            newCachedFlag(CROSS_DEVICE_TAB_PANE_ANDROID, false);
    public static final CachedFlag sDesktopUAOnConnectedDisplay =
            newCachedFlag(
                    DESKTOP_UA_ON_CONNECTED_DISPLAY,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ false);
    public static final CachedFlag sDisableInstanceLimit =
            newCachedFlag(
                    DISABLE_INSTANCE_LIMIT,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sDisplayEdgeToEdgeFullscreen =
            newCachedFlag(DISPLAY_EDGE_TO_EDGE_FULLSCREEN, false, true);
    public static final CachedFlag sDrawChromePagesEdgeToEdge =
            newCachedFlag(DRAW_CHROME_PAGES_EDGE_TO_EDGE, /* defaultValue= */ true);
    public static final CachedFlag sEdgeToEdgeBottomChin =
            newCachedFlag(EDGE_TO_EDGE_BOTTOM_CHIN, /* defaultValue= */ true);
    public static final CachedFlag sEdgeToEdgeEverywhere =
            newCachedFlag(
                    EDGE_TO_EDGE_EVERYWHERE,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sEdgeToEdgeMonitorConfigurations =
            newCachedFlag(EDGE_TO_EDGE_MONITOR_CONFIGURATIONS, /* defaultValue= */ true);
    public static final CachedFlag sEdgeToEdgeTablet =
            newCachedFlag(
                    EDGE_TO_EDGE_TABLET,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sEdgeToEdgeUseBackupNavbarInsets =
            newCachedFlag(EDGE_TO_EDGE_USE_BACKUP_NAVBAR_INSETS, true);
    public static final CachedFlag sEducationalTipDefaultBrowserPromoCard =
            newCachedFlag(EDUCATIONAL_TIP_DEFAULT_BROWSER_PROMO_CARD, false);
    public static final CachedFlag sEducationalTipModule =
            newCachedFlag(EDUCATIONAL_TIP_MODULE, false, true);
    public static final CachedFlag sEnableExclusiveAccessManager =
            newCachedFlag(ENABLE_EXCLUSIVE_ACCESS_MANAGER, false);
    public static final CachedFlag sEnableFullscreenToAnyScreenAndroid =
            newCachedFlag(ENABLE_FULLSCREEN_TO_ANY_SCREEN_ANDROID, false);
    public static final CachedFlag sEnableXAxisActivityTransition =
            newCachedFlag(ENABLE_X_AXIS_ACTIVITY_TRANSITION, false);
    public static final CachedFlag sFloatingSnackbar = newCachedFlag(FLOATING_SNACKBAR, true);
    public static final CachedFlag sForceTranslucentNotificationTrampoline =
            newCachedFlag(FORCE_TRANSLUCENT_NOTIFICATION_TRAMPOLINE, false);
    public static final CachedFlag sFullscreenInsetsApiMigration =
            newCachedFlag(FULLSCREEN_INSETS_API_MIGRATION, false);
    public static final CachedFlag sFullscreenInsetsApiMigrationOnAutomotive =
            newCachedFlag(FULLSCREEN_INSETS_API_MIGRATION_ON_AUTOMOTIVE, true);
    public static final CachedFlag sGridTabSwitcherSurfaceColorUpdate =
            newCachedFlag(
                    GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ false);
    public static final CachedFlag sGridTabSwitcherUpdate =
            newCachedFlag(GRID_TAB_SWITCHER_UPDATE, false, /* defaultValueInTests= */ true);
    public static final CachedFlag sHistoryPaneAndroid =
            newCachedFlag(
                    HISTORY_PANE_ANDROID,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sHomepageIsNewTabPagePolicyAndroid =
            newCachedFlag(HOMEPAGE_IS_NEW_TAB_PAGE_POLICY_ANDROID, true);
    public static final CachedFlag sIncognitoThemeOverlayTesting =
            newCachedFlag(INCOGNITO_THEME_OVERLAY_TESTING, false);
    public static final CachedFlag sKeyboardEscBackNavigation =
            newCachedFlag(KEYBOARD_ESC_BACK_NAVIGATION, true);
    public static final CachedFlag sLegacyTabStateDeprecation =
            newCachedFlag(
                    LEGACY_TAB_STATE_DEPRECATION,
                    /* defaultValue= */ true,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sLoadNativeEarly =
            newCachedFlag(
                    LOAD_NATIVE_EARLY, /* defaultValue= */ false, /* defaultValueInTests= */ true);
    public static final CachedFlag sLockBackPressHandlerAtStart =
            newCachedFlag(LOCK_BACK_PRESS_HANDLER_AT_START, true);
    public static final CachedFlag sLockTopControlsOnLargeTablets =
            newCachedFlag(LOCK_TOP_CONTROLS_ON_LARGE_TABLETS, false);
    public static final CachedFlag sMagicStackAndroid = newCachedFlag(MAGIC_STACK_ANDROID, true);
    public static final CachedFlag sMaliciousApkDownloadCheck =
            newCachedFlag(
                    MALICIOUS_APK_DOWNLOAD_CHECK,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sMiniOriginBar = newCachedFlag(MINI_ORIGIN_BAR, true, true);
    public static final CachedFlag sMitigateLegacySearchEnginePromoOverlap =
            newCachedFlag(MITIGATE_LEGACY_SEARCH_ENGINE_PROMO_OVERLAP, false, true);
    public static final CachedFlag sMostVisitedTilesCustomization =
            newCachedFlag(
                    MOST_VISITED_TILES_CUSTOMIZATION,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sMostVisitedTilesReselect =
            newCachedFlag(MOST_VISITED_TILES_RESELECT, false);
    public static final CachedFlag sMultiInstanceApplicationStatusCleanup =
            newCachedFlag(MULTI_INSTANCE_APPLICATION_STATUS_CLEANUP, false);
    public static final CachedFlag sMvcUpdateViewWhenModelChanged =
            newCachedFlag(
                    MVC_UPDATE_VIEW_WHEN_MODEL_CHANGED,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sNavBarColorAnimation =
            newCachedFlag(NAV_BAR_COLOR_ANIMATION, /* defaultValue= */ true);
    public static final CachedFlag sNewTabPageCustomization =
            newCachedFlag(NEW_TAB_PAGE_CUSTOMIZATION, true);
    public static final CachedFlag sNewTabPageCustomizationForMvt =
            newCachedFlag(NEW_TAB_PAGE_CUSTOMIZATION_FOR_MVT, false, true);
    public static final CachedFlag sNewTabPageCustomizationToolbarButton =
            newCachedFlag(NEW_TAB_PAGE_CUSTOMIZATION_TOOLBAR_BUTTON, false);
    public static final CachedFlag sNewTabPageCustomizationV2 =
            newCachedFlag(NEW_TAB_PAGE_CUSTOMIZATION_V2, false);
    public static final CachedFlag sNotificationTrampoline =
            newCachedFlag(NOTIFICATION_TRAMPOLINE, false);
    public static final CachedFlag sOptimizationGuidePushNotifications =
            newCachedFlag(OPTIMIZATION_GUIDE_PUSH_NOTIFICATIONS, true);
    public static final CachedFlag sPCctMinimumHeight = newCachedFlag(PCCT_MINIMUM_HEIGHT, true);
    public static final CachedFlag sPaintPreviewDemo = newCachedFlag(PAINT_PREVIEW_DEMO, false);
    public static final CachedFlag sPostGetMyMemoryStateToBackground =
            newCachedFlag(POST_GET_MEMORY_PRESSURE_TO_BACKGROUND, true);
    public static final CachedFlag sPowerSavingModeBroadcastReceiverInBackground =
            newCachedFlag(POWER_SAVING_MODE_BROADCAST_RECEIVER_IN_BACKGROUND, true);
    public static final CachedFlag sPriceChangeModule = newCachedFlag(PRICE_CHANGE_MODULE, true);
    public static final CachedFlag sReportNotificationContentDetectionData =
            newCachedFlag(
                    REPORT_NOTIFICATION_CONTENT_DETECTION_DATA,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sRightEdgeGoesForwardGestureNav =
            newCachedFlag(RIGHT_EDGE_GOES_FORWARD_GESTURE_NAV, false);
    public static final CachedFlag sSafetyHubMagicStack =
            newCachedFlag(SAFETY_HUB_MAGIC_STACK, false);
    public static final CachedFlag sSafetyHubWeakAndReusedPasswords =
            newCachedFlag(SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS, false);
    public static final CachedFlag sSearchInCCT =
            newCachedFlag(
                    SEARCH_IN_CCT, /* defaultValue= */ false, /* defaultValueInTests= */ true);
    public static final CachedFlag sSearchInCCTAlternateTapHandling =
            newCachedFlag(SEARCH_IN_CCT_ALTERNATE_TAP_HANDLING, false);
    public static final CachedFlag sSearchInCCTAlternateTapHandlingIfEnabledByEmbedder =
            newCachedFlag(SEARCH_IN_CCT_ALTERNATE_TAP_HANDLING_IF_ENABLED_BY_EMBEDDER, true);
    public static final CachedFlag sSearchInCCTIfEnabledByEmbedder =
            newCachedFlag(SEARCH_IN_CCT_IF_ENABLED_BY_EMBEDDER, true);
    public static final CachedFlag sSettingsSingleActivity =
            newCachedFlag(
                    SETTINGS_SINGLE_ACTIVITY,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sShowHomeButtonPolicyAndroid =
            newCachedFlag(SHOW_HOME_BUTTON_POLICY_ANDROID, true);
    public static final CachedFlag sSmallerTabStripTitleLimit =
            newCachedFlag(SMALLER_TAB_STRIP_TITLE_LIMIT, true);
    public static final CachedFlag sStartSurfaceReturnTime =
            newCachedFlag(START_SURFACE_RETURN_TIME, true);
    public static final CachedFlag sTabClosureMethodRefactor =
            newCachedFlag(TAB_CLOSURE_METHOD_REFACTOR, false);
    public static final CachedFlag sTabModelInitFixes =
            newCachedFlag(
                    TAB_MODEL_INIT_FIXES,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sTabStripDensityChangeAndroid =
            newCachedFlag(TAB_STRIP_DENSITY_CHANGE_ANDROID, true);
    public static final CachedFlag sTabStripIncognitoMigration =
            newCachedFlag(
                    TAB_STRIP_INCOGNITO_MIGRATION,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ false);
    public static final CachedFlag sTabStripLayoutOptimization =
            newCachedFlag(
                    TAB_STRIP_LAYOUT_OPTIMIZATION,
                    /* defaultValue= */ true,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sTabWindowManagerReportIndicesMismatch =
            newCachedFlag(TAB_WINDOW_MANAGER_REPORT_INDICES_MISMATCH, true);
    public static final CachedFlag sTabletTabStripAnimation =
            newCachedFlag(
                    TABLET_TAB_STRIP_ANIMATION,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sTestDefaultDisabled =
            newCachedFlag(TEST_DEFAULT_DISABLED, false);
    public static final CachedFlag sTestDefaultEnabled = newCachedFlag(TEST_DEFAULT_ENABLED, true);
    public static final CachedFlag sToolbarPhoneAnimationRefactor =
            newCachedFlag(
                    TOOLBAR_PHONE_ANIMATION_REFACTOR,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ false);
    public static final CachedFlag sToolbarSnapshotRefactor =
            newCachedFlag(
                    TOOLBAR_SNAPSHOT_REFACTOR,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sTopControlsRefactor =
            newCachedFlag(
                    TOP_CONTROLS_REFACTOR,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sTouchToSearchCallout =
            newCachedFlag(
                    TOUCH_TO_SEARCH_CALLOUT,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sTraceBinderIpc =
            newCachedFlag(
                    TRACE_BINDER_IPC, /* defaultValue= */ false, /* defaultValueInTests= */ true);
    public static final CachedFlag sUseActivityManagerForTabActivation =
            newCachedFlag(USE_ACTIVITY_MANAGER_FOR_TAB_ACTIVATION, true);
    public static final CachedFlag sUseChimeAndroidSdk =
            newCachedFlag(USE_CHIME_ANDROID_SDK, false);
    public static final CachedFlag sUseInitialNetworkStateAtStartup =
            newCachedFlag(USE_INITIAL_NETWORK_STATE_AT_STARTUP, true);
    public static final CachedFlag sUseLibunwindstackNativeUnwinderAndroid =
            newCachedFlag(USE_LIBUNWINDSTACK_NATIVE_UNWINDER_ANDROID, true);
    public static final CachedFlag sWebApkMinShellApkVersion =
            newCachedFlag(WEB_APK_MIN_SHELL_APK_VERSION, true);
    // keep-sorted end

    public static final List<CachedFlag> sFlagsCachedFullBrowser =
            List.of(
                    // keep-sorted start
                    sAccountForSuppressedKeyboardInsets,
                    sAllowTabClosingUponMinimization,
                    sAndroidAnimatedProgressBarInBrowser,
                    sAndroidAnimatedProgressBarInViz,
                    sAndroidAppIntegration,
                    sAndroidAppIntegrationModule,
                    sAndroidAppIntegrationMultiDataSource,
                    sAndroidAppIntegrationV2,
                    sAndroidAppIntegrationWithFavicon,
                    sAndroidBottomToolbar,
                    sAndroidBottomToolbarV2,
                    sAndroidComposeplate,
                    sAndroidDataImporterService,
                    sAndroidDesktopDensity,
                    sAndroidElegantTextHeight,
                    sAndroidMinimalUiLargeScreen,
                    sAndroidOpenIncognitoAsWindow,
                    sAndroidProgressBarVisualUpdate,
                    sAndroidSettingsContainment,
                    sAndroidSurfaceColorUpdate,
                    sAndroidTabDeclutterDedupeTabIdsKillSwitch,
                    sAndroidTabGroupsColorUpdateGm3,
                    sAndroidTabSkipSaveTabsKillswitch,
                    sAndroidThemeModule,
                    sAndroidThemeResourceProvider,
                    sAndroidUseAdminsForEnterpriseInfo,
                    sAndroidWebAppLaunchHandler,
                    sAndroidWindowControlsOverlay,
                    sAndroidWindowPopupLargeScreen,
                    sAndroidWindowPopupPredictFinalBounds,
                    sAndroidWindowPopupResizeAfterSpawn,
                    sAppSpecificHistory,
                    sAppSpecificHistoryViewIntent,
                    sAsyncNotificationManager,
                    sAutomotiveBackButtonBarStreamline,
                    sBackgroundThreadPoolFieldTrial,
                    sBatchTabRestore,
                    sBlockIntentsWhileLocked,
                    sBookmarkPaneAndroid,
                    sBrowserControlsDebugging,
                    sCacheIsMultiInstanceApi31Enabled,
                    sCctAdaptiveButton,
                    sCctAuthTab,
                    sCctAuthTabDisableAllExternalIntents,
                    sCctAuthTabEnableHttpsRedirects,
                    sCctAutoTranslate,
                    sCctBlockTouchesDuringEnterAnimation,
                    sCctContextualMenuItems,
                    sCctDestroyTabWhenModelIsEmpty,
                    sCctFixWarmup,
                    sCctFreInSameTask,
                    sCctGoogleBottomBar,
                    sCctGoogleBottomBarVariantLayouts,
                    sCctIncognitoAvailableToThirdParty,
                    sCctNavigationalPrefetch,
                    sCctNestedSecurityIcon,
                    sCctOpenInBrowserButtonIfAllowedByEmbedder,
                    sCctOpenInBrowserButtonIfEnabledByEmbedder,
                    sCctPredictiveBackGesture,
                    sCctRealtimeEngagementEventsInBackground,
                    sCctResetTimeoutEnabled,
                    sCctResizableForThirdParties,
                    sCctRevampedBranding,
                    sCctTabModalDialog,
                    sCctToolbarRefactor,
                    sClampAutomotiveScaling,
                    sClankStartupLatencyInjection,
                    sCleanupLegacyTabState,
                    sCollectAndroidFrameTimelineMetrics,
                    sCommandLineOnNonRooted,
                    sContextMenuPictureInPictureAndroid,
                    sCpaSpecUpdate,
                    sCpaTabGroupingButton,
                    sCrossDeviceTabPaneAndroid,
                    sDesktopUAOnConnectedDisplay,
                    sDisableInstanceLimit,
                    sDisplayEdgeToEdgeFullscreen,
                    sDrawChromePagesEdgeToEdge,
                    sEdgeToEdgeBottomChin,
                    sEdgeToEdgeEverywhere,
                    sEdgeToEdgeMonitorConfigurations,
                    sEdgeToEdgeTablet,
                    sEdgeToEdgeUseBackupNavbarInsets,
                    sEducationalTipDefaultBrowserPromoCard,
                    sEducationalTipModule,
                    sEnableExclusiveAccessManager,
                    sEnableFullscreenToAnyScreenAndroid,
                    sEnableXAxisActivityTransition,
                    sFloatingSnackbar,
                    sForceTranslucentNotificationTrampoline,
                    sFullscreenInsetsApiMigration,
                    sFullscreenInsetsApiMigrationOnAutomotive,
                    sGridTabSwitcherSurfaceColorUpdate,
                    sGridTabSwitcherUpdate,
                    sHistoryPaneAndroid,
                    sHomepageIsNewTabPagePolicyAndroid,
                    sIncognitoThemeOverlayTesting,
                    sKeyboardEscBackNavigation,
                    sLegacyTabStateDeprecation,
                    sLoadNativeEarly,
                    sLockBackPressHandlerAtStart,
                    sLockTopControlsOnLargeTablets,
                    sMagicStackAndroid,
                    sMaliciousApkDownloadCheck,
                    sMiniOriginBar,
                    sMitigateLegacySearchEnginePromoOverlap,
                    sMostVisitedTilesCustomization,
                    sMostVisitedTilesReselect,
                    sMultiInstanceApplicationStatusCleanup,
                    sMvcUpdateViewWhenModelChanged,
                    sNavBarColorAnimation,
                    sNewTabPageCustomization,
                    sNewTabPageCustomizationForMvt,
                    sNewTabPageCustomizationToolbarButton,
                    sNewTabPageCustomizationV2,
                    sNotificationTrampoline,
                    sOptimizationGuidePushNotifications,
                    sPCctMinimumHeight,
                    sPaintPreviewDemo,
                    sPostGetMyMemoryStateToBackground,
                    sPowerSavingModeBroadcastReceiverInBackground,
                    sPriceChangeModule,
                    sReportNotificationContentDetectionData,
                    sRightEdgeGoesForwardGestureNav,
                    sSafetyHubMagicStack,
                    sSafetyHubWeakAndReusedPasswords,
                    sSearchInCCT,
                    sSearchInCCTAlternateTapHandling,
                    sSearchInCCTAlternateTapHandlingIfEnabledByEmbedder,
                    sSearchInCCTIfEnabledByEmbedder,
                    sSettingsSingleActivity,
                    sShowHomeButtonPolicyAndroid,
                    sSmallerTabStripTitleLimit,
                    sStartSurfaceReturnTime,
                    sTabClosureMethodRefactor,
                    sTabModelInitFixes,
                    sTabStripDensityChangeAndroid,
                    sTabStripIncognitoMigration,
                    sTabStripLayoutOptimization,
                    sTabWindowManagerReportIndicesMismatch,
                    sTabletTabStripAnimation,
                    sToolbarPhoneAnimationRefactor,
                    sToolbarSnapshotRefactor,
                    sTopControlsRefactor,
                    sTouchToSearchCallout,
                    sTraceBinderIpc,
                    sUseActivityManagerForTabActivation,
                    sUseChimeAndroidSdk,
                    sUseInitialNetworkStateAtStartup,
                    sUseLibunwindstackNativeUnwinderAndroid,
                    sWebApkMinShellApkVersion
                    // keep-sorted end
                    );

    public static final List<CachedFlag> sFlagsCachedInMinimalBrowser =
            List.of(sAsyncNotificationManagerForDownload);

    public static final List<CachedFlag> sTestCachedFlags =
            List.of(sTestDefaultDisabled, sTestDefaultEnabled);

    public static final Map<String, CachedFlag> sAllCachedFlags =
            CachedFlag.createCachedFlagMap(
                    List.of(
                            sFlagsCachedFullBrowser,
                            sFlagsCachedInMinimalBrowser,
                            sTestCachedFlags));

    // MutableFlagWithSafeDefault instances.
    // keep-sorted start group_prefixes=["public static final MutableFlagWithSafeDefault"]
    public static final MutableFlagWithSafeDefault sAdaptiveButtonInTopToolbarCustomizationV2 =
            newMutableFlagWithSafeDefault(ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2, false);
    public static final MutableFlagWithSafeDefault sAndroidAppearanceSettings =
            newMutableFlagWithSafeDefault(ANDROID_APPEARANCE_SETTINGS, false);
    public static final MutableFlagWithSafeDefault sAndroidBookmarkBar =
            newMutableFlagWithSafeDefault(ANDROID_BOOKMARK_BAR, false);
    public static final MutableFlagWithSafeDefault sAndroidNativePagesInNewTab =
            newMutableFlagWithSafeDefault(ANDROID_NATIVE_PAGES_IN_NEW_TAB, true);
    public static final MutableFlagWithSafeDefault sAndroidPinnedTabs =
            newMutableFlagWithSafeDefault(ANDROID_PINNED_TABS, false);
    public static final MutableFlagWithSafeDefault
            sAndroidShowRestoreTabsPromoOnFreBypassedKillSwitch =
                    newMutableFlagWithSafeDefault(
                            ANDROID_SHOW_RESTORE_TABS_PROMO_ON_FRE_BYPASSED_KILL_SWITCH, true);
    public static final MutableFlagWithSafeDefault sAndroidTabDeclutterArchiveAllButActiveTab =
            newMutableFlagWithSafeDefault(ANDROID_TAB_DECLUTTER_ARCHIVE_ALL_BUT_ACTIVE, false);
    public static final MutableFlagWithSafeDefault sAndroidTabDeclutterArchiveTabGroups =
            newMutableFlagWithSafeDefault(ANDROID_TAB_DECLUTTER_ARCHIVE_TAB_GROUPS, false);
    public static final MutableFlagWithSafeDefault sAndroidTabDeclutterAutoDelete =
            newMutableFlagWithSafeDefault(ANDROID_TAB_DECLUTTER_AUTO_DELETE, false);
    public static final MutableFlagWithSafeDefault sAndroidTabDeclutterAutoDeleteKillSwitch =
            newMutableFlagWithSafeDefault(ANDROID_TAB_DECLUTTER_AUTO_DELETE_KILL_SWITCH, true);
    public static final MutableFlagWithSafeDefault sAndroidTabDeclutterPerformanceImprovements =
            newMutableFlagWithSafeDefault(ANDROID_TAB_DECLUTTER_PERFORMANCE_IMPROVEMENTS, true);
    public static final MutableFlagWithSafeDefault sAndroidTabDeclutterRescueKillSwitch =
            newMutableFlagWithSafeDefault(ANDROID_TAB_DECLUTTER_RESCUE_KILLSWITCH, true);
    public static final MutableFlagWithSafeDefault sAndroidTabHighlighting =
            newMutableFlagWithSafeDefault(ANDROID_TAB_HIGHLIGHTING, false);
    public static final MutableFlagWithSafeDefault sAndroidTipsNotifications =
            newMutableFlagWithSafeDefault(ANDROID_TIPS_NOTIFICATIONS, false);
    public static final MutableFlagWithSafeDefault sBcivBottomControls =
            newMutableFlagWithSafeDefault(BCIV_BOTTOM_CONTROLS, true);
    public static final MutableFlagWithSafeDefault sBrowserControlsEarlyResize =
            newMutableFlagWithSafeDefault(BROWSER_CONTROLS_EARLY_RESIZE, false);
    public static final MutableFlagWithSafeDefault sBrowserControlsInViz =
            newMutableFlagWithSafeDefault(BROWSER_CONTROLS_IN_VIZ, true);
    public static final MutableFlagWithSafeDefault sChromeNativeUrlOverriding =
            newMutableFlagWithSafeDefault(CHROME_NATIVE_URL_OVERRIDING, false);
    public static final MutableFlagWithSafeDefault sClearBrowsingDataAndroidSurvey =
            newMutableFlagWithSafeDefault(CLEAR_BROWSING_DATA_ANDROID_SURVEY, false);
    public static final MutableFlagWithSafeDefault sControlsVisibilityFromNavigations =
            newMutableFlagWithSafeDefault(CONTROLS_VISIBILITY_FROM_NAVIGATIONS, true);
    // Defaulted to true in native, but since it is being used as a kill switch set the default
    // value pre-native to false as it is safer if the feature needs to be killed via Finch config.
    public static final MutableFlagWithSafeDefault sEmptyTabListAnimationKillSwitch =
            newMutableFlagWithSafeDefault(EMPTY_TAB_LIST_ANIMATION_KILL_SWITCH, false);
    public static final MutableFlagWithSafeDefault sForceBrowserControlsUponExitingFullscreen =
            newMutableFlagWithSafeDefault(FORCE_BROWSER_CONTROLS_UPON_EXITING_FULLSCREEN, true);
    public static final MutableFlagWithSafeDefault sHubBackButton =
            newMutableFlagWithSafeDefault(HUB_BACK_BUTTON, false);
    public static final MutableFlagWithSafeDefault sHubSlideAnimation =
            newMutableFlagWithSafeDefault(HUB_SLIDE_ANIMATION, true);
    public static final MutableFlagWithSafeDefault sIncognitoScreenshot =
            newMutableFlagWithSafeDefault(INCOGNITO_SCREENSHOT, false);
    public static final MutableFlagWithSafeDefault sMediaIndicatorsAndroid =
            newMutableFlagWithSafeDefault(MEDIA_INDICATORS_ANDROID, false);
    public static final MutableFlagWithSafeDefault sNoVisibleHintForDifferentTLD =
            newMutableFlagWithSafeDefault(ANDROID_NO_VISIBLE_HINT_FOR_DIFFERENT_TLD, true);
    public static final MutableFlagWithSafeDefault sQuickDeleteAndroidSurvey =
            newMutableFlagWithSafeDefault(QUICK_DELETE_ANDROID_SURVEY, false);
    public static final MutableFlagWithSafeDefault sReadAloudTapToSeek =
            newMutableFlagWithSafeDefault(READALOUD_TAP_TO_SEEK, false);
    public static final MutableFlagWithSafeDefault sRecordSuppressionMetrics =
            newMutableFlagWithSafeDefault(RECORD_SUPPRESSION_METRICS, true);
    public static final MutableFlagWithSafeDefault sSafetyHubAndroidOrganicSurvey =
            newMutableFlagWithSafeDefault(SAFETY_HUB_ANDROID_ORGANIC_SURVEY, false);
    public static final MutableFlagWithSafeDefault sSafetyHubAndroidSurvey =
            newMutableFlagWithSafeDefault(SAFETY_HUB_ANDROID_SURVEY, false);
    public static final MutableFlagWithSafeDefault sSafetyHubAndroidSurveyV2 =
            newMutableFlagWithSafeDefault(SAFETY_HUB_ANDROID_SURVEY_V2, false);
    public static final MutableFlagWithSafeDefault sSafetyHubFollowup =
            newMutableFlagWithSafeDefault(SAFETY_HUB_FOLLOWUP, true);
    public static final MutableFlagWithSafeDefault sShowNewTabAnimations =
            newMutableFlagWithSafeDefault(SHOW_NEW_TAB_ANIMATIONS, false);
    public static final MutableFlagWithSafeDefault sShowTabListAnimations =
            newMutableFlagWithSafeDefault(SHOW_TAB_LIST_ANIMATIONS, false);
    public static final MutableFlagWithSafeDefault sSuppressToolbarCapturesAtGestureEnd =
            newMutableFlagWithSafeDefault(SUPPRESS_TOOLBAR_CAPTURES_AT_GESTURE_END, false);
    public static final MutableFlagWithSafeDefault sTabArchivalDragDropAndroid =
            newMutableFlagWithSafeDefault(TAB_ARCHIVAL_DRAG_DROP_ANDROID, true);
    public static final MutableFlagWithSafeDefault sTabCollectionAndroid =
            newMutableFlagWithSafeDefault(TAB_COLLECTION_ANDROID, false);
    // Default value will only ever be reached in tests.
    public static final MutableFlagWithSafeDefault sTabFreezeOnUndoableClosureKillSwitch =
            newMutableFlagWithSafeDefault(TAB_FREEZE_ON_UNDOABLE_CLOSURE_KILL_SWITCH, true);
    public static final MutableFlagWithSafeDefault sTabGroupAndroidVisualDataCleanup =
            newMutableFlagWithSafeDefault(TAB_GROUP_ANDROID_VISUAL_DATA_CLEANUP, false);
    public static final MutableFlagWithSafeDefault sTabGroupEntryPointsAndroid =
            newMutableFlagWithSafeDefault(TAB_GROUP_ENTRY_POINTS_ANDROID, true);
    public static final MutableFlagWithSafeDefault sTabGroupParityBottomSheetAndroid =
            newMutableFlagWithSafeDefault(TAB_GROUP_PARITY_BOTTOM_SHEET_ANDROID, true);
    public static final MutableFlagWithSafeDefault sTabStorageSqlitePrototype =
            newMutableFlagWithSafeDefault(TAB_STORAGE_SQLITE_PROTOTYPE, false);
    public static final MutableFlagWithSafeDefault sTabSwitcherForeignFaviconSupport =
            newMutableFlagWithSafeDefault(TAB_SWITCHER_FOREIGN_FAVICON_SUPPORT, true);
    public static final MutableFlagWithSafeDefault sTabSwitcherGroupSuggestionsAndroid =
            newMutableFlagWithSafeDefault(TAB_SWITCHER_GROUP_SUGGESTIONS_ANDROID, false);
    public static final MutableFlagWithSafeDefault sTabSwitcherGroupSuggestionsTestModeAndroid =
            newMutableFlagWithSafeDefault(TAB_SWITCHER_GROUP_SUGGESTIONS_TEST_MODE_ANDROID, false);
    public static final MutableFlagWithSafeDefault sToolbarScrollAblation =
            newMutableFlagWithSafeDefault(TOOLBAR_SCROLL_ABLATION, false);
    public static final MutableFlagWithSafeDefault sToolbarTabletResizeRefactor =
            newMutableFlagWithSafeDefault(TOOLBAR_TABLET_RESIZE_REFACTOR, false);
    // keep-sorted end

    // CachedFeatureParam instances.
    /* Alphabetical order by feature name, arbitrary order by param name: */
    public static final BooleanCachedFeatureParam sAndroidThemeModuleForceDependencies =
            newBooleanCachedFeatureParam(
                    ANDROID_THEME_MODULE, "force_theme_module_dependencies", false);
    public static final BooleanCachedFeatureParam sAndroidThemeResourceProviderForceLight =
            newBooleanCachedFeatureParam(
                    ANDROID_THEME_RESOURCE_PROVIDER, "force_light_theme", false);
    public static final BooleanCachedFeatureParam sCctAdaptiveButtonEnableOpenInBrowser =
            newBooleanCachedFeatureParam(CCT_ADAPTIVE_BUTTON, "open_in_browser", false);
    public static final BooleanCachedFeatureParam sCctAdaptiveButtonEnableVoice =
            newBooleanCachedFeatureParam(CCT_ADAPTIVE_BUTTON, "voice", false);
    public static final BooleanCachedFeatureParam sCctAdaptiveButtonContextualOnly =
            newBooleanCachedFeatureParam(CCT_ADAPTIVE_BUTTON, "contextual_only", false);
    public static final IntCachedFeatureParam sCctAdaptiveButtonDefaultVariant =
            newIntCachedFeatureParam(CCT_ADAPTIVE_BUTTON, "default_variant", 0);
    public static final IntCachedFeatureParam sAndroidAppIntegrationV2ContentTtlHours =
            newIntCachedFeatureParam(ANDROID_APP_INTEGRATION_V2, "content_ttl_hours", 168);

    public static final IntCachedFeatureParam
            sAndroidAppIntegrationMultiDataSourceHistoryContentTtlHours =
                    newIntCachedFeatureParam(
                            ANDROID_APP_INTEGRATION_MULTI_DATA_SOURCE,
                            "history_content_ttl_hours",
                            24);

    public static final BooleanCachedFeatureParam sAndroidAppIntegrationWithFaviconSkipDeviceCheck =
            newBooleanCachedFeatureParam(
                    ANDROID_APP_INTEGRATION_WITH_FAVICON, "skip_device_check", false);
    public static final BooleanCachedFeatureParam sAndroidAppIntegrationModuleForceCardShow =
            newBooleanCachedFeatureParam(ANDROID_APP_INTEGRATION_MODULE, "force_card_shown", false);

    public static final BooleanCachedFeatureParam sAndroidAppIntegrationModuleShowThirdPartyCard =
            newBooleanCachedFeatureParam(
                    ANDROID_APP_INTEGRATION_MODULE, "show_third_party_card", false);

    public static final IntCachedFeatureParam sAndroidAppIntegrationWithFaviconScheduleDelayTimeMs =
            newIntCachedFeatureParam(
                    ANDROID_APP_INTEGRATION_WITH_FAVICON, "schedule_delay_time_ms", 0);
    public static final BooleanCachedFeatureParam sAndroidAppIntegrationWithFaviconUseLargeFavicon =
            newBooleanCachedFeatureParam(
                    ANDROID_APP_INTEGRATION_WITH_FAVICON, "use_large_favicon", true);
    public static final IntCachedFeatureParam
            sAndroidAppIntegrationWithFaviconZeroStateFaviconNumber =
                    newIntCachedFeatureParam(
                            ANDROID_APP_INTEGRATION_WITH_FAVICON, "zero_state_favicon_number", 5);

    public static final BooleanCachedFeatureParam sAndroidAppIntegrationWithFaviconSkipSchemaCheck =
            newBooleanCachedFeatureParam(
                    ANDROID_APP_INTEGRATION_WITH_FAVICON, "skip_schema_check", false);

    public static final BooleanCachedFeatureParam
            sAndroidAppIntegrationMultiDataSourceSkipSchemaCheck =
                    newBooleanCachedFeatureParam(
                            ANDROID_APP_INTEGRATION_MULTI_DATA_SOURCE,
                            "multi_data_source_skip_schema_check",
                            false);
    public static final BooleanCachedFeatureParam
            sAndroidAppIntegrationMultiDataSourceSkipDeviceCheck =
                    newBooleanCachedFeatureParam(
                            ANDROID_APP_INTEGRATION_MULTI_DATA_SOURCE,
                            "multi_data_source_skip_device_check",
                            false);

    public static final BooleanCachedFeatureParam sAndroidComposeplateSkipLocaleCheck =
            newBooleanCachedFeatureParam(ANDROID_COMPOSEPLATE, "skip_locale_check", false);

    public static final BooleanCachedFeatureParam sAndroidComposeplateHideIncognitoButton =
            newBooleanCachedFeatureParam(ANDROID_COMPOSEPLATE, "hide_incognito_button", false);

    public static final BooleanCachedFeatureParam sAndroidComposeplateV2Enabled =
            newBooleanCachedFeatureParam(ANDROID_COMPOSEPLATE, "v2_enabled", false);
    public static final BooleanCachedFeatureParam sAndroidBottomToolbarDefaultToTop =
            newBooleanCachedFeatureParam(ANDROID_BOTTOM_TOOLBAR, "default_to_top", true);

    public static final IntCachedFeatureParam sBackgroundThreadPoolFieldTrialConfig =
            newIntCachedFeatureParam(BACKGROUND_THREAD_POOL_FIELD_TRIAL, "config", 4);

    public static final IntCachedFeatureParam sBatchTabRestoreBatchSize =
            newIntCachedFeatureParam(BATCH_TAB_RESTORE, "batch_tab_restore_batch_size", 5);

    public static final IntCachedFeatureParam sCctAuthTabEnableHttpsRedirectsVerificationTimeoutMs =
            newIntCachedFeatureParam(
                    CCT_AUTH_TAB_ENABLE_HTTPS_REDIRECTS, "verification_timeout_ms", 10_000);
    public static final IntCachedFeatureParam sClampAutomotiveScalingMaxScalingPercentage =
            newIntCachedFeatureParam(
                    CLAMP_AUTOMOTIVE_SCALING, "max_automotive_scaling_percentage", 150);

    /**
     * Parameter that lists a pipe ("|") separated list of package names from which the {@link
     * EXTRA_AUTO_TRANSLATE_LANGUAGE} should be allowed. This defaults to a single list item
     * consisting of the package name of the Android Google Search App.
     */
    public static final StringCachedFeatureParam sCctAutoTranslatePackageNamesAllowlist =
            newStringCachedFeatureParam(
                    CCT_AUTO_TRANSLATE,
                    "package_names_allowlist",
                    "com.google.android.googlequicksearchbox");

    /**
     * Parameter that, if true, indicates that the {@link EXTRA_AUTO_TRANSLATE_LANGUAGE} should be
     * automatically allowed from any first party package name.
     */
    public static final BooleanCachedFeatureParam sCctAutoTranslateAllowAllFirstParties =
            newBooleanCachedFeatureParam(CCT_AUTO_TRANSLATE, "allow_all_first_parties", false);

    // Devices from this OEM--and potentially others--sometimes crash when we call
    // `Activity#enterPictureInPictureMode` on Android R. So, we disable the feature on those
    // devices. See: https://crbug.com/1519164.
    public static final StringCachedFeatureParam
            sCctMinimizedEnabledByDefaultManufacturerExcludeList =
                    newStringCachedFeatureParam(
                            CCT_MINIMIZED_ENABLED_BY_DEFAULT,
                            "manufacturer_exclude_list",
                            "xiaomi");

    /**
     * A cached parameter used for specifying the height of the Google Bottom Bar in DP, when its
     * variant is NO_VARIANT.
     */
    public static final IntCachedFeatureParam sCctGoogleBottomBarNoVariantHeightDp =
            newIntCachedFeatureParam(
                    CCT_GOOGLE_BOTTOM_BAR, "google_bottom_bar_no_variant_height_dp", 64);

    /**
     * A cached parameter representing the order of Google Bottom Bar buttons based on experiment
     * configuration.
     */
    public static final StringCachedFeatureParam sCctGoogleBottomBarButtonList =
            newStringCachedFeatureParam(CCT_GOOGLE_BOTTOM_BAR, "google_bottom_bar_button_list", "");

    /**
     * A cached parameter used for specifying the height of the Google Bottom Bar in DP, when its
     * variant is SINGLE_DECKER.
     */
    public static final IntCachedFeatureParam
            sCctGoogleBottomBarVariantLayoutsSingleDeckerHeightDp =
                    newIntCachedFeatureParam(
                            CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS,
                            "google_bottom_bar_single_decker_height_dp",
                            62);

    /**
     * A cached parameter representing the Google Bottom Bar layout variants value based on
     * experiment configuration.
     */
    public static final IntCachedFeatureParam sCctGoogleBottomBarVariantLayoutsVariantLayout =
            newIntCachedFeatureParam(
                    CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS,
                    "google_bottom_bar_variant_layout",
                    1 /* GoogleBottomBarVariantLayoutType.DOUBLE_DECKER */);

    /**
     * A cached boolean parameter to decide whether to check if Google is Chrome's default search
     * engine.
     */
    public static final BooleanCachedFeatureParam
            sCctGoogleBottomBarVariantLayoutsGoogleDseCheckEnabled =
                    newBooleanCachedFeatureParam(
                            CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS,
                            "google_bottom_bar_variant_is_google_default_search_engine_check_enabled",
                            false);

    public static final IntCachedFeatureParam sCctResetMinimumTimeoutMinutes =
            newIntCachedFeatureParam(CCT_RESET_TIMEOUT_ENABLED, "minimum_reset_timeout_mins", 1);

    public static final StringCachedFeatureParam sCctResizableForThirdPartiesAllowlistEntries =
            newStringCachedFeatureParam(CCT_RESIZABLE_FOR_THIRD_PARTIES, "allowlist_entries", "");
    public static final StringCachedFeatureParam sCctResizableForThirdPartiesDenylistEntries =
            newStringCachedFeatureParam(CCT_RESIZABLE_FOR_THIRD_PARTIES, "denylist_entries", "");
    public static final StringCachedFeatureParam sCctResizableForThirdPartiesDefaultPolicy =
            newStringCachedFeatureParam(
                    CCT_RESIZABLE_FOR_THIRD_PARTIES, "default_policy", "use-denylist");

    /**
     * A cached parameter representing the amount of latency to inject during Clank startup based on
     * experiment configuration.
     */
    public static final IntCachedFeatureParam sClankStartupLatencyInjectionAmountMs =
            newIntCachedFeatureParam(
                    CLANK_STARTUP_LATENCY_INJECTION, "latency_injection_amount_millis", 0);

    /**
     * The parameter to control how quickly the JankTracker should be enabled. Some JankTracker
     * implementations can be quite heavy (including thread creation and the link). To avoid
     * impacting start up we support delaying the construction of the JankTracker.
     *
     * <p>3000ms is enough to push us past the median start up time without losing to much of feed
     * interactions.
     */
    public static final IntCachedFeatureParam
            sCollectAndroidFrameTimelineMetricsJankTrackerDelayedStartMs =
                    newIntCachedFeatureParam(
                            COLLECT_ANDROID_FRAME_TIMELINE_METRICS, "delayed_start_ms", 3000);

    /** Batch size for number of legacy TabState files that should be deleted in a batch. */
    public static final IntCachedFeatureParam sCleanupLegacyTabStateBatchSize =
            newIntCachedFeatureParam(
                    CLEANUP_LEGACY_TABSTATE, "cleanup_legacy_tab_state_batch_size", 5);

    /** Maximum number of legacy TabState files that can be deleted per session. */
    public static final IntCachedFeatureParam sMaxLegacyTabStateFilesCleanedUpPerSession =
            newIntCachedFeatureParam(
                    CLEANUP_LEGACY_TABSTATE,
                    "max_legacy_tab_state_files_cleaned_up_per_session",
                    100);

    public static final IntCachedFeatureParam sDisableInstanceLimitMemoryThresholdMb =
            newIntCachedFeatureParam(
                    DISABLE_INSTANCE_LIMIT, "max_instance_limit_memory_threshold_mb", 6500);
    public static final IntCachedFeatureParam sDisableInstanceLimitMaxCount =
            newIntCachedFeatureParam(DISABLE_INSTANCE_LIMIT, "max_instance_limit", 20);

    /**
     * Cached param whether we disable animations for color changes to the edge-to-edge bottom chin.
     */
    public static final BooleanCachedFeatureParam
            sNavBarColorAnimationDisableBottomChinColorAnimation =
                    newBooleanCachedFeatureParam(
                            NAV_BAR_COLOR_ANIMATION, "disable_bottom_chin_color_animation", false);

    /**
     * Cached param whether we disable animations for color changes to the navigation bar for the
     * edge-to-edge layout used in edge-to-edge-everywhere.
     */
    public static final BooleanCachedFeatureParam
            sNavBarColorAnimationDisableEdgeToEdgeLayoutColorAnimation =
                    newBooleanCachedFeatureParam(
                            NAV_BAR_COLOR_ANIMATION,
                            "disable_edge_to_edge_layout_color_animation",
                            false);

    /**
     * Param for the OEMs that need an exception for min versions. Its value should be a comma
     * separated list of integers, and its index should match {@link #sEdgeToEdgeBottomChinOemList}.
     */
    public static final StringCachedFeatureParam sEdgeToEdgeBottomChinOemMinVersions =
            newStringCachedFeatureParam(
                    EDGE_TO_EDGE_BOTTOM_CHIN, "e2e_field_trial_oem_min_versions", "34,34");

    public static final StringCachedFeatureParam sEdgeToEdgeEverywhereOemMinVersions =
            newStringCachedFeatureParam(
                    EDGE_TO_EDGE_EVERYWHERE, "e2e_field_trial_oem_min_versions", "");

    public static final StringCachedFeatureParam sEdgeToEdgeUseBackupNavbarInsetsOemMinVersions =
            newStringCachedFeatureParam(
                    EDGE_TO_EDGE_USE_BACKUP_NAVBAR_INSETS,
                    "e2e_backup_navbar_insets_oem_min_versions",
                    "");

    /**
     * Param for the OEMs that need an exception for min versions. Its value should be a comma
     * separated list of manufacturer, and its index should match {@link
     * #sEdgeToEdgeBottomChinOemMinVersions}.
     */
    public static final StringCachedFeatureParam sEdgeToEdgeBottomChinOemList =
            newStringCachedFeatureParam(
                    EDGE_TO_EDGE_BOTTOM_CHIN, "e2e_field_trial_oem_list", "oppo,xiaomi");

    public static final StringCachedFeatureParam sEdgeToEdgeEverywhereOemList =
            newStringCachedFeatureParam(EDGE_TO_EDGE_EVERYWHERE, "e2e_field_trial_oem_list", "");

    public static final StringCachedFeatureParam sEdgeToEdgeUseBackupNavbarInsetsOemList =
            newStringCachedFeatureParam(
                    EDGE_TO_EDGE_USE_BACKUP_NAVBAR_INSETS, "e2e_backup_navbar_insets_oem_list", "");

    public static final BooleanCachedFeatureParam sEdgeToEdgeUseBackupNavbarInsetsUseTappable =
            newBooleanCachedFeatureParam(
                    EDGE_TO_EDGE_USE_BACKUP_NAVBAR_INSETS, "use_tappable_element", true);

    public static final BooleanCachedFeatureParam sEdgeToEdgeUseBackupNavbarInsetsUseGestures =
            newBooleanCachedFeatureParam(
                    EDGE_TO_EDGE_USE_BACKUP_NAVBAR_INSETS, "use_gesture_insets", true);

    public static final IntCachedFeatureParam sEdgeToEdgeTabletInvisibleBottomChinMinWidth =
            newIntCachedFeatureParam(
                    EDGE_TO_EDGE_TABLET, "e2e_tablet_invisible_bottom_chin_min_width", -1);

    public static final IntCachedFeatureParam sEdgeToEdgeTabletMinWidthThreshold =
            newIntCachedFeatureParam(EDGE_TO_EDGE_TABLET, "e2e_tablet_width_threshold", -1);

    public static final BooleanCachedFeatureParam sLoadNativeEarlyConcurrentLoad =
            newBooleanCachedFeatureParam(LOAD_NATIVE_EARLY, "concurrent_load", true);

    public static final BooleanCachedFeatureParam sTabGroupListContainment =
            newBooleanCachedFeatureParam(
                    GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE, "tab_group_list_containment", true);

    public static final BooleanCachedFeatureParam sMagicStackAndroidShowAllModules =
            newBooleanCachedFeatureParam(MAGIC_STACK_ANDROID, "show_all_modules", false);
    public static final BooleanCachedFeatureParam sMaliciousApkDownloadCheckTelemetryOnly =
            newBooleanCachedFeatureParam(MALICIOUS_APK_DOWNLOAD_CHECK, "telemetry_only", false);
    public static final BooleanCachedFeatureParam sMostVisitedTilesReselectLaxSchemeHost =
            newBooleanCachedFeatureParam(MOST_VISITED_TILES_RESELECT, "lax_scheme_host", false);
    public static final BooleanCachedFeatureParam sMostVisitedTilesReselectLaxRef =
            newBooleanCachedFeatureParam(MOST_VISITED_TILES_RESELECT, "lax_ref", false);
    public static final BooleanCachedFeatureParam sMostVisitedTilesReselectLaxQuery =
            newBooleanCachedFeatureParam(MOST_VISITED_TILES_RESELECT, "lax_query", false);
    public static final BooleanCachedFeatureParam sMostVisitedTilesReselectLaxPath =
            newBooleanCachedFeatureParam(MOST_VISITED_TILES_RESELECT, "lax_path", false);
    public static final IntCachedFeatureParam sNotificationTrampolineLongJobDurationMs =
            newIntCachedFeatureParam(NOTIFICATION_TRAMPOLINE, "long_job_duration_millis", 8 * 1000);
    public static final IntCachedFeatureParam sNotificationTrampolineNormalJobDurationMs =
            newIntCachedFeatureParam(
                    NOTIFICATION_TRAMPOLINE, "normal_job_duration_millis", 1 * 1000);
    public static final IntCachedFeatureParam sNotificationTrampolineImmediateJobDurationMs =
            newIntCachedFeatureParam(NOTIFICATION_TRAMPOLINE, "minimum_job_duration_millis", 10);
    public static final IntCachedFeatureParam sNotificationTrampolineTimeoutPriorNativeInitMs =
            newIntCachedFeatureParam(
                    NOTIFICATION_TRAMPOLINE, "timeout_in_millis_prior_native_init", 5 * 1000);
    public static final IntCachedFeatureParam sOmahaMinSdkVersionMinSdkVersion =
            newIntCachedFeatureParam(OMAHA_MIN_SDK_VERSION_ANDROID, "min_sdk_version", -1);

    /** The default cache size in Java for push notification. */
    public static final IntCachedFeatureParam sOptimizationGuidePushNotificationsMaxCacheSize =
            newIntCachedFeatureParam(OPTIMIZATION_GUIDE_PUSH_NOTIFICATIONS, "max_cache_size", 100);

    public static final DoubleCachedFeatureParam sPCctMinimumHeightRatio =
            newDoubleCachedFeatureParam(PCCT_MINIMUM_HEIGHT, "pcct_minimum_height_ratio", 0.3);

    public static final BooleanCachedFeatureParam
            sPriceChangeModuleSkipShoppingPersistedTabDataDelayedInit =
                    newBooleanCachedFeatureParam(
                            PRICE_CHANGE_MODULE,
                            "skip_shopping_persisted_tab_data_delayed_initialization",
                            true);

    public static final IntCachedFeatureParam sReadAloudAudioOverviewsSpeedAdditionPercentage =
            newIntCachedFeatureParam(
                    READALOUD_AUDIO_OVERVIEWS,
                    "read_aloud_audio_overviews_speed_addition_percentage",
                    20);

    public static final BooleanCachedFeatureParam sShouldConsiderLanguageInOverviewReadability =
            newBooleanCachedFeatureParam(
              READALOUD_AUDIO_OVERVIEWS,
              "read_aloud_audio_overviews_should_consider_language_in_overview_readability",
              false
            );

    /** Controls whether Referrer App ID is passed to Search Results Page via client= param. */
    public static final BooleanCachedFeatureParam sSearchinCctApplyReferrerId =
            newBooleanCachedFeatureParam(SEARCH_IN_CCT, "apply_referrer_id", false);

    /** Pipe ("|") separated list of package names allowed to use the interactive Omnibox. */
    // TODO(crbug.com/40239922): remove default value when no longer relevant.
    public static final StringCachedFeatureParam sSearchinCctOmniboxAllowedPackageNames =
            newStringCachedFeatureParam(
                    SEARCH_IN_CCT,
                    "omnibox_allowed_package_names",
                    BuildConfig.ENABLE_DEBUG_LOGS ? "org.chromium.customtabsclient" : "");

    public static final IntCachedFeatureParam sStartSurfaceReturnTimeTabletSecs =
            newIntCachedFeatureParam(
                    START_SURFACE_RETURN_TIME,
                    "start_surface_return_time_on_tablet_seconds",
                    14400); // 4 hours

    public static final StringCachedFeatureParam sTabStripLayoutOptimizationOemAllowlist =
            newStringCachedFeatureParam(
                    TAB_STRIP_LAYOUT_OPTIMIZATION, "custom_headers_oem_allowlist", "");

    public static final StringCachedFeatureParam sTabStripLayoutOptimizationOemDenylist =
            newStringCachedFeatureParam(
                    TAB_STRIP_LAYOUT_OPTIMIZATION, "custom_headers_oem_denylist", "");

    public static final BooleanCachedFeatureParam sTabStripLayoutOptimizationOnExternalDisplay =
            newBooleanCachedFeatureParam(
                    TAB_STRIP_LAYOUT_OPTIMIZATION, "enable_on_external_display", true);

    public static final StringCachedFeatureParam
            sTabStripLayoutOptimizationOnExternalDisplayOemDenylist =
                    newStringCachedFeatureParam(
                            TAB_STRIP_LAYOUT_OPTIMIZATION,
                            "external_display_oem_denylist",
                            "samsung");

    public static final IntCachedFeatureParam
            sTabWindowManagerReportIndicesMismatchTimeDiffThresholdMs =
                    newIntCachedFeatureParam(
                            TAB_WINDOW_MANAGER_REPORT_INDICES_MISMATCH,
                            "activity_creation_timestamp_diff_threshold_ms",
                            1000);

    /** Always register to push notification service. */
    public static final BooleanCachedFeatureParam sUseChimeAndroidSdkAlwaysRegister =
            newBooleanCachedFeatureParam(USE_CHIME_ANDROID_SDK, "always_register", false);

    public static final IntCachedFeatureParam sWebApkMinShellApkVersionValue =
            newIntCachedFeatureParam(WEB_APK_MIN_SHELL_APK_VERSION, "version", 146);

    public static final BooleanCachedFeatureParam sTouchToSearchCalloutIph =
            newBooleanCachedFeatureParam(TOUCH_TO_SEARCH_CALLOUT, "iph", false);

    public static final BooleanCachedFeatureParam sTouchToSearchCalloutSnippetAsSubtitle =
            newBooleanCachedFeatureParam(TOUCH_TO_SEARCH_CALLOUT, "snippet_as_subtitle", false);

    /** All {@link CachedFeatureParam}s of features in this FeatureList */
    public static final List<CachedFeatureParam<?>> sParamsCached =
            List.of(
                    // keep-sorted start
                    sAndroidAppIntegrationModuleForceCardShow,
                    sAndroidAppIntegrationModuleShowThirdPartyCard,
                    sAndroidAppIntegrationMultiDataSourceHistoryContentTtlHours,
                    sAndroidAppIntegrationMultiDataSourceSkipDeviceCheck,
                    sAndroidAppIntegrationMultiDataSourceSkipSchemaCheck,
                    sAndroidAppIntegrationV2ContentTtlHours,
                    sAndroidAppIntegrationWithFaviconScheduleDelayTimeMs,
                    sAndroidAppIntegrationWithFaviconSkipDeviceCheck,
                    sAndroidAppIntegrationWithFaviconSkipSchemaCheck,
                    sAndroidAppIntegrationWithFaviconUseLargeFavicon,
                    sAndroidAppIntegrationWithFaviconZeroStateFaviconNumber,
                    sAndroidBottomToolbarDefaultToTop,
                    sAndroidComposeplateHideIncognitoButton,
                    sAndroidComposeplateSkipLocaleCheck,
                    sAndroidComposeplateV2Enabled,
                    sAndroidThemeModuleForceDependencies,
                    sAndroidThemeResourceProviderForceLight,
                    sBackgroundThreadPoolFieldTrialConfig,
                    sBatchTabRestoreBatchSize,
                    sCctAdaptiveButtonContextualOnly,
                    sCctAdaptiveButtonDefaultVariant,
                    sCctAdaptiveButtonEnableOpenInBrowser,
                    sCctAdaptiveButtonEnableVoice,
                    sCctAuthTabEnableHttpsRedirectsVerificationTimeoutMs,
                    sCctAutoTranslateAllowAllFirstParties,
                    sCctAutoTranslatePackageNamesAllowlist,
                    sCctGoogleBottomBarButtonList,
                    sCctGoogleBottomBarNoVariantHeightDp,
                    sCctGoogleBottomBarVariantLayoutsGoogleDseCheckEnabled,
                    sCctGoogleBottomBarVariantLayoutsSingleDeckerHeightDp,
                    sCctGoogleBottomBarVariantLayoutsVariantLayout,
                    sCctMinimizedEnabledByDefaultManufacturerExcludeList,
                    sCctResetMinimumTimeoutMinutes,
                    sCctResizableForThirdPartiesAllowlistEntries,
                    sCctResizableForThirdPartiesDefaultPolicy,
                    sCctResizableForThirdPartiesDenylistEntries,
                    sClampAutomotiveScalingMaxScalingPercentage,
                    sClankStartupLatencyInjectionAmountMs,
                    sCleanupLegacyTabStateBatchSize,
                    sCollectAndroidFrameTimelineMetricsJankTrackerDelayedStartMs,
                    sDisableInstanceLimitMaxCount,
                    sDisableInstanceLimitMemoryThresholdMb,
                    sEdgeToEdgeBottomChinOemList,
                    sEdgeToEdgeBottomChinOemMinVersions,
                    sEdgeToEdgeEverywhereOemList,
                    sEdgeToEdgeEverywhereOemMinVersions,
                    sEdgeToEdgeTabletInvisibleBottomChinMinWidth,
                    sEdgeToEdgeTabletMinWidthThreshold,
                    sEdgeToEdgeUseBackupNavbarInsetsOemList,
                    sEdgeToEdgeUseBackupNavbarInsetsOemMinVersions,
                    sEdgeToEdgeUseBackupNavbarInsetsUseGestures,
                    sEdgeToEdgeUseBackupNavbarInsetsUseTappable,
                    sLoadNativeEarlyConcurrentLoad,
                    sMagicStackAndroidShowAllModules,
                    sMaliciousApkDownloadCheckTelemetryOnly,
                    sMaxLegacyTabStateFilesCleanedUpPerSession,
                    sMostVisitedTilesReselectLaxPath,
                    sMostVisitedTilesReselectLaxQuery,
                    sMostVisitedTilesReselectLaxRef,
                    sMostVisitedTilesReselectLaxSchemeHost,
                    sNavBarColorAnimationDisableBottomChinColorAnimation,
                    sNavBarColorAnimationDisableEdgeToEdgeLayoutColorAnimation,
                    sNotificationTrampolineImmediateJobDurationMs,
                    sNotificationTrampolineLongJobDurationMs,
                    sNotificationTrampolineNormalJobDurationMs,
                    sNotificationTrampolineTimeoutPriorNativeInitMs,
                    sOmahaMinSdkVersionMinSdkVersion,
                    sOptimizationGuidePushNotificationsMaxCacheSize,
                    sPCctMinimumHeightRatio,
                    sPriceChangeModuleSkipShoppingPersistedTabDataDelayedInit,
                    sReadAloudAudioOverviewsSpeedAdditionPercentage,
                    sSearchinCctApplyReferrerId,
                    sSearchinCctOmniboxAllowedPackageNames,
                    sShouldConsiderLanguageInOverviewReadability,
                    sStartSurfaceReturnTimeTabletSecs,
                    sTabGroupListContainment,
                    sTabStripLayoutOptimizationOemAllowlist,
                    sTabStripLayoutOptimizationOemDenylist,
                    sTabStripLayoutOptimizationOnExternalDisplay,
                    sTabStripLayoutOptimizationOnExternalDisplayOemDenylist,
                    sTabWindowManagerReportIndicesMismatchTimeDiffThresholdMs,
                    sTouchToSearchCalloutIph,
                    sTouchToSearchCalloutSnippetAsSubtitle,
                    sUseChimeAndroidSdkAlwaysRegister,
                    sWebApkMinShellApkVersionValue
                    // keep-sorted end
                    );

    // Mutable*ParamWithSafeDefault instances.
    /* Alphabetical: */
    public static final MutableBooleanParamWithSafeDefault
            sAndroidNativePagesInNewTabBookmarksEnabled =
                    sAndroidNativePagesInNewTab.newBooleanParam(
                            "android_native_pages_in_new_tab_bookmarks_enabled", true);
    public static final MutableBooleanParamWithSafeDefault
            sAndroidNativePagesInNewTabDownloadsEnabled =
                    sAndroidNativePagesInNewTab.newBooleanParam(
                            "android_native_pages_in_new_tab_downloads_enabled", true);
    public static final MutableBooleanParamWithSafeDefault
            sAndroidNativePagesInNewTabHistoryEnabled =
                    sAndroidNativePagesInNewTab.newBooleanParam(
                            "android_native_pages_in_new_tab_history_enabled", true);
    public static final MutableBooleanParamWithSafeDefault
            sAndroidNativePagesInNewTabRecentTabsEnabled =
                    sAndroidNativePagesInNewTab.newBooleanParam(
                            "android_native_pages_in_new_tab_recent_tabs_enabled", true);
    public static final MutableIntParamWithSafeDefault
            sAndroidTabDeclutterAutoDeleteTimeDeltaHours =
                    sAndroidTabDeclutterAutoDelete.newIntParam(
                            "android_tab_declutter_auto_delete_time_delta_hours", 90 * 24);

    public static final MutableBooleanParamWithSafeDefault sAndroidTabDeclutterAutoDeletePromoTest =
            sAndroidTabDeclutterAutoDelete.newBooleanParam(
                    "android_tab_declutter_auto_delete_promo_test", false);

    public static final MutableBooleanParamWithSafeDefault sShowNewTabAnimationsLogs =
            sShowNewTabAnimations.newBooleanParam("logs", false);
    public static final MutableBooleanParamWithSafeDefault sShowNewTabAnimationsListeners =
            sShowNewTabAnimations.newBooleanParam("listeners", false);

    public static final MutableBooleanParamWithSafeDefault sAndroidTabHighlightingForceCtrlClick =
            sAndroidTabHighlighting.newBooleanParam("force_ctrl_click", false);
    public static final MutableBooleanParamWithSafeDefault sAndroidTabHighlightingForceShiftClick =
            sAndroidTabHighlighting.newBooleanParam("force_shift_click", false);
}
