// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.FeatureList;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.annotations.NativeMethods;

import java.util.HashMap;
import java.util.Map;

/**
 * Java accessor for base/feature_list.h state.
 *
 * This class provides methods to access values of feature flags registered in
 * |kFeaturesExposedToJava| in chrome/browser/flags/android/chrome_feature_list.cc and as a constant
 * in this class.
 *
 * This class also provides methods to access values of field trial parameters associated to those
 * flags.
 */
@JNINamespace("chrome::android")
@MainDex
public abstract class ChromeFeatureList {
    /** Prevent instantiation. */
    private ChromeFeatureList() {}

    /**
     * @see FeatureList#setTestCanUseDefaultsForTesting
     */
    // TODO(crbug.com/1060097): Migrate callers to the FeatureList equivalent function.
    @VisibleForTesting
    public static void setTestCanUseDefaultsForTesting() {
        FeatureList.setTestCanUseDefaultsForTesting();
    }

    /**
     * @see FeatureList#resetTestCanUseDefaultsForTesting
     */
    // TODO(crbug.com/1060097): Migrate callers to the FeatureList equivalent function.
    @VisibleForTesting
    public static void resetTestCanUseDefaultsForTesting() {
        FeatureList.resetTestCanUseDefaultsForTesting();
    }

    /**
     * @return Whether the native FeatureList has been initialized. If this method returns false,
     *         none of the methods in this class that require native access should be called (except
     *         in tests if test features have been set).
     */
    // TODO(crbug.com/1060097): Migrate callers to the FeatureList equivalent function.
    @Deprecated
    public static boolean isInitialized() {
        return FeatureList.isInitialized();
    }

    /*
     * Returns whether the specified feature is enabled or not in native.
     *
     * @param featureName The name of the feature to query.
     * @return Whether the feature is enabled or not.
     */
    private static boolean isEnabledInNative(String featureName) {
        assert FeatureList.isNativeInitialized();
        return ChromeFeatureListJni.get().isEnabled(featureName);
    }

    /**
     * Returns whether the specified feature is enabled or not.
     *
     * Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in chrome/browser/flags/android/chrome_feature_list.cc
     *
     * Calling this has the side effect of bucketing this client, which may cause an experiment to
     * be marked as active.
     *
     * Should be called only after native is loaded. If {@link #isInitialized()} return true, this
     * method is safe to call.  In tests, this will return any values set through
     * {@link FeatureList#setTestFeatures(Map)}, even before native is loaded.
     *
     * @param featureName The name of the feature to query.
     * @return Whether the feature is enabled or not.
     */
    public static boolean isEnabled(String featureName) {
        // FeatureFlags set for testing override the native default value.
        Boolean testValue = FeatureList.getTestValueForFeature(featureName);
        if (testValue != null) return testValue;
        return isEnabledInNative(featureName);
    }

    /**
     * Returns a field trial param for the specified feature.
     *
     * Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in chrome/browser/flags/android/chrome_feature_list.cc
     *
     * @param featureName The name of the feature to retrieve a param for.
     * @param paramName The name of the param for which to get as an integer.
     * @return The parameter value as a String. The string is empty if the feature does not exist or
     *   the specified parameter does not exist.
     */
    public static String getFieldTrialParamByFeature(String featureName, String paramName) {
        String testValue = FeatureList.getTestValueForFieldTrialParam(featureName, paramName);
        if (testValue != null) return testValue;
        if (FeatureList.hasTestFeatures()) return "";
        assert FeatureList.isInitialized();
        return ChromeFeatureListJni.get().getFieldTrialParamByFeature(featureName, paramName);
    }

    /**
     * Returns a field trial param as an int for the specified feature.
     *
     * Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in chrome/browser/flags/android/chrome_feature_list.cc
     *
     * @param featureName The name of the feature to retrieve a param for.
     * @param paramName The name of the param for which to get as an integer.
     * @param defaultValue The integer value to use if the param is not available.
     * @return The parameter value as an int. Default value if the feature does not exist or the
     *         specified parameter does not exist or its string value does not represent an int.
     */
    public static int getFieldTrialParamByFeatureAsInt(
            String featureName, String paramName, int defaultValue) {
        String testValue = FeatureList.getTestValueForFieldTrialParam(featureName, paramName);
        if (testValue != null) return Integer.valueOf(testValue);
        if (FeatureList.hasTestFeatures()) return defaultValue;
        assert FeatureList.isInitialized();
        return ChromeFeatureListJni.get().getFieldTrialParamByFeatureAsInt(
                featureName, paramName, defaultValue);
    }

    /**
     * Returns a field trial param as a double for the specified feature.
     *
     * Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in chrome/browser/flags/android/chrome_feature_list.cc
     *
     * @param featureName The name of the feature to retrieve a param for.
     * @param paramName The name of the param for which to get as an integer.
     * @param defaultValue The double value to use if the param is not available.
     * @return The parameter value as a double. Default value if the feature does not exist or the
     *         specified parameter does not exist or its string value does not represent a double.
     */
    public static double getFieldTrialParamByFeatureAsDouble(
            String featureName, String paramName, double defaultValue) {
        String testValue = FeatureList.getTestValueForFieldTrialParam(featureName, paramName);
        if (testValue != null) return Double.valueOf(testValue);
        if (FeatureList.hasTestFeatures()) return defaultValue;
        assert FeatureList.isInitialized();
        return ChromeFeatureListJni.get().getFieldTrialParamByFeatureAsDouble(
                featureName, paramName, defaultValue);
    }

    /**
     * Returns all the field trial parameters for the specified feature.
     */
    public static Map<String, String> getFieldTrialParamsForFeature(String featureName) {
        assert FeatureList.isInitialized();
        Map<String, String> result = new HashMap<String, String>();
        String[] flattenedParams =
                ChromeFeatureListJni.get().getFlattedFieldTrialParamsForFeature(featureName);
        for (int i = 0; i < flattenedParams.length; i += 2) {
            result.put(flattenedParams[i], flattenedParams[i + 1]);
        }
        return result;
    }

    /**
     * Returns a field trial param as a boolean for the specified feature.
     *
     * Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in chrome/browser/flags/android/chrome_feature_list.cc
     *
     * @param featureName The name of the feature to retrieve a param for.
     * @param paramName The name of the param for which to get as an integer.
     * @param defaultValue The boolean value to use if the param is not available.
     * @return The parameter value as a boolean. Default value if the feature does not exist or the
     *         specified parameter does not exist or its string value is neither "true" nor "false".
     */
    public static boolean getFieldTrialParamByFeatureAsBoolean(
            String featureName, String paramName, boolean defaultValue) {
        String testValue = FeatureList.getTestValueForFieldTrialParam(featureName, paramName);
        if (testValue != null) return Boolean.valueOf(testValue);
        if (FeatureList.hasTestFeatures()) return defaultValue;
        assert FeatureList.isInitialized();
        return ChromeFeatureListJni.get().getFieldTrialParamByFeatureAsBoolean(
                featureName, paramName, defaultValue);
    }

    /* Alphabetical: */
    public static final String ABOUT_THIS_SITE_BANNER = "AboutThisSiteBanner";
    public static final String ADAPTIVE_BUTTON_IN_TOP_TOOLBAR = "AdaptiveButtonInTopToolbar";
    public static final String ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2 =
            "AdaptiveButtonInTopToolbarCustomizationV2";
    public static final String ADD_TO_HOMESCREEN_IPH = "AddToHomescreenIPH";
    public static final String ALLOW_NEW_INCOGNITO_TAB_INTENTS = "AllowNewIncognitoTabIntents";
    public static final String ALLOW_SYNC_OFF_FOR_CHILD_ACCOUNTS = "AllowSyncOffForChildAccounts";
    public static final String AUTOFILL_ADDRESS_PROFILE_SAVE_PROMPT_NICKNAME_SUPPORT =
            "AutofillAddressProfileSavePromptNicknameSupport";
    public static final String AUTOFILL_ALLOW_NON_HTTP_ACTIVATION =
            "AutofillAllowNonHttpActivation";
    public static final String AUTOFILL_CREDIT_CARD_AUTHENTICATION =
            "AutofillCreditCardAuthentication";
    public static final String AUTOFILL_ENABLE_RANKING_FORMULA = "AutofillEnableRankingFormula";
    public static final String AUTOFILL_ENABLE_MANUAL_FALLBACK_FOR_VIRTUAL_CARDS =
            "AutofillEnableManualFallbackForVirtualCards";
    public static final String AUTOFILL_ENABLE_SUPPORT_FOR_HONORIFIC_PREFIXES =
            "AutofillEnableSupportForHonorificPrefixes";
    public static final String AUTOFILL_ENABLE_SUPPORT_FOR_MORE_STRUCTURE_IN_ADDRESSES =
            "AutofillEnableSupportForMoreStructureInAddresses";
    public static final String AUTOFILL_ENABLE_SUPPORT_FOR_MORE_STRUCTURE_IN_NAMES =
            "AutofillEnableSupportForMoreStructureInNames";
    public static final String AUTOFILL_ENABLE_UPDATE_VIRTUAL_CARD_ENROLLMENT =
            "AutofillEnableUpdateVirtualCardEnrollment";
    public static final String AUTOFILL_RANKING_FORMULA_USAGE_HALF_LIFE =
            "AutofillRankingFormulaUsageHalfLife";
    public static final String ANDROID_SEARCH_ENGINE_CHOICE_NOTIFICATION =
            "AndroidSearchEngineChoiceNotification";
    public static final String ANONYMOUS_UPDATE_CHECKS = "AnonymousUpdateChecks";
    public static final String APP_LANGUAGE_PROMPT = "AppLanguagePrompt";
    public static final String APP_LANGUAGE_PROMPT_ULP = "AppLanguagePromptULP";
    public static final String ASSISTANT_CONSENT_MODAL = "AssistantConsentModal";
    public static final String ASSISTANT_CONSENT_SIMPLIFIED_TEXT = "AssistantConsentSimplifiedText";
    public static final String ASSISTANT_CONSENT_V2 = "AssistantConsentV2";
    public static final String ASSISTANT_INTENT_EXPERIMENT_ID = "AssistantIntentExperimentId";
    public static final String ASSISTANT_INTENT_PAGE_URL = "AssistantIntentPageUrl";
    public static final String ASSISTANT_INTENT_TRANSLATE_INFO = "AssistantIntentTranslateInfo";
    public static final String AUTOFILL_MANUAL_FALLBACK_ANDROID = "AutofillManualFallbackAndroid";
    public static final String AUTOFILL_REFRESH_STYLE_ANDROID = "AutofillRefreshStyleAndroid";
    public static final String AUTOFILL_KEYBOARD_ACCESSORY = "AutofillKeyboardAccessory";
    public static final String APP_LAUNCHPAD = "AppLaunchpad";
    public static final String APP_MENU_MOBILE_SITE_OPTION = "AppMenuMobileSiteOption";
    public static final String BACKGROUND_THREAD_POOL = "BackgroundThreadPool";
    public static final String BACK_GESTURE_REFACTOR = "BackGestureRefactorAndroid";
    public static final String BIOMETRIC_TOUCH_TO_FILL = "BiometricTouchToFill";
    public static final String BULK_TAB_RESTORE = "BulkTabRestore";
    public static final String CAPTIVE_PORTAL_CERTIFICATE_LIST = "CaptivePortalCertificateList";
    public static final String CCT_BACKGROUND_TAB = "CCTBackgroundTab";
    public static final String CCT_CLIENT_DATA_HEADER = "CCTClientDataHeader";
    public static final String CCT_INCOGNITO = "CCTIncognito";
    public static final String CCT_INCOGNITO_AVAILABLE_TO_THIRD_PARTY =
            "CCTIncognitoAvailableToThirdParty";
    public static final String CCT_EXTERNAL_LINK_HANDLING = "CCTExternalLinkHandling";
    public static final String CCT_NEW_DOWNLOAD_TAB = "CCTNewDownloadTab";
    public static final String CCT_POST_MESSAGE_API = "CCTPostMessageAPI";
    public static final String CCT_REDIRECT_PRECONNECT = "CCTRedirectPreconnect";
    public static final String CCT_REMOVE_REMOTE_VIEW_IDS = "CCTRemoveRemoteViewIds";
    public static final String CCT_RESIZABLE_90_MAXIMUM_HEIGHT = "CCTResizable90MaximumHeight";
    public static final String CCT_RESIZABLE_ALLOW_RESIZE_BY_USER_GESTURE =
            "CCTResizableAllowResizeByUserGesture";
    public static final String CCT_RESIZABLE_FOR_FIRST_PARTIES = "CCTResizableForFirstParties";
    public static final String CCT_RESIZABLE_FOR_THIRD_PARTIES = "CCTResizableForThirdParties";
    public static final String CCT_RESOURCE_PREFETCH = "CCTResourcePrefetch";
    public static final String CCT_REPORT_PARALLEL_REQUEST_STATUS =
            "CCTReportParallelRequestStatus";
    public static final String CCT_TOOLBAR_CUSTOMIZATIONS = "CCTToolbarCustomizations";
    public static final String CLOSE_TAB_SUGGESTIONS = "CloseTabSuggestions";
    public static final String DONT_AUTO_HIDE_BROWSER_CONTROLS = "DontAutoHideBrowserControls";
    public static final String CHROME_NEW_DOWNLOAD_TAB = "ChromeNewDownloadTab";
    public static final String CHROME_SHARE_LONG_SCREENSHOT = "ChromeShareLongScreenshot";
    public static final String CHROME_SHARING_HUB = "ChromeSharingHub";
    public static final String CHROME_SHARING_HUB_LAUNCH_ADJACENT =
            "ChromeSharingHubLaunchAdjacent";
    public static final String CHROME_SURVEY_NEXT_ANDROID = "ChromeSurveyNextAndroid";
    public static final String COMMAND_LINE_ON_NON_ROOTED = "CommandLineOnNonRooted";
    public static final String COMMERCE_MERCHANT_VIEWER = "CommerceMerchantViewer";
    public static final String COMMERCE_PRICE_TRACKING = "CommercePriceTracking";
    public static final String CONDITIONAL_TAB_STRIP_ANDROID = "ConditionalTabStripAndroid";
    public static final String CONTACTS_PICKER_SELECT_ALL = "ContactsPickerSelectAll";
    public static final String CONTEXT_MENU_ENABLE_LENS_SHOPPING_ALLOWLIST =
            "ContextMenuEnableLensShoppingAllowlist";
    public static final String CONTEXT_MENU_GOOGLE_LENS_CHIP = "ContextMenuGoogleLensChip";
    public static final String CONTEXT_MENU_POPUP_STYLE = "ContextMenuPopupStyle";
    public static final String CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS =
            "ContextMenuSearchWithGoogleLens";
    public static final String GOOGLE_LENS_SDK_INTENT = "GoogleLensSdkIntent";
    public static final String CONTEXT_MENU_SHOP_WITH_GOOGLE_LENS = "ContextMenuShopWithGoogleLens";
    public static final String CONTEXT_MENU_SEARCH_AND_SHOP_WITH_GOOGLE_LENS =
            "ContextMenuSearchAndShopWithGoogleLens";
    public static final String CONTEXT_MENU_TRANSLATE_WITH_GOOGLE_LENS =
            "ContextMenuTranslateWithGoogleLens";
    public static final String LENS_CAMERA_ASSISTED_SEARCH = "LensCameraAssistedSearch";
    /** Used only in native code. */
    public static final String CONTEXTUAL_SEARCH_DEBUG = "ContextualSearchDebug";
    public static final String CONTEXTUAL_SEARCH_DELAYED_INTELLIGENCE =
            "ContextualSearchDelayedIntelligence";
    public static final String CONTEXTUAL_SEARCH_FORCE_CAPTION = "ContextualSearchForceCaption";
    public static final String CONTEXTUAL_SEARCH_ML_TAP_SUPPRESSION =
            "ContextualSearchMlTapSuppression";
    public static final String CONTEXTUAL_SEARCH_NEW_SETTINGS = "ContextualSearchNewSettings";
    public static final String CONTEXTUAL_SEARCH_LONGPRESS_RESOLVE =
            "ContextualSearchLongpressResolve";
    public static final String CONTEXTUAL_SEARCH_TAP_DISABLE_OVERRIDE =
            "ContextualSearchTapDisableOverride";
    public static final String CONTEXTUAL_SEARCH_THIN_WEB_VIEW_IMPLEMENTATION =
            "ContextualSearchThinWebViewImplementation";
    public static final String CONTEXTUAL_SEARCH_TRANSLATIONS = "ContextualSearchTranslations";
    public static final String CONTEXTUAL_TRIGGERS_SELECTION_HANDLES =
            "ContextualTriggersSelectionHandles";
    public static final String CONTEXTUAL_TRIGGERS_SELECTION_MENU =
            "ContextualTriggersSelectionMenu";
    public static final String CONTEXTUAL_TRIGGERS_SELECTION_SIZE =
            "ContextualTriggersSelectionSize";
    public static final String SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON =
            "SyncAndroidPromosWithSingleButton";
    public static final String SYNC_ANDROID_PROMOS_WITH_TITLE = "SyncAndroidPromosWithTitle";
    public static final String CONTINUOUS_SEARCH = "ContinuousSearch";
    public static final String CRITICAL_PERSISTED_TAB_DATA = "CriticalPersistedTabData";
    public static final String DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING =
            "DarkenWebsitesCheckboxInThemesSetting";
    public static final String DETAILED_LANGUAGE_SETTINGS = "DetailedLanguageSettings";
    public static final String DIRECT_ACTIONS = "DirectActions";
    public static final String DISABLE_COMPOSITED_PROGRESS_BAR = "DisableCompositedProgressBar";
    public static final String ENABLE_IPH = "EnableIPH";
    public static final String DNS_OVER_HTTPS = "DnsOverHttps";
    public static final String DOWNLOAD_FILE_PROVIDER = "DownloadFileProvider";
    public static final String DOWNLOAD_NOTIFICATION_BADGE = "DownloadNotificationBadge";
    public static final String DOWNLOADS_FOREGROUND = "DownloadsForeground";
    public static final String DOWNLOADS_AUTO_RESUMPTION_NATIVE = "DownloadsAutoResumptionNative";
    public static final String DOWNLOAD_OFFLINE_CONTENT_PROVIDER =
            "UseDownloadOfflineContentProvider";
    public static final String DOWNLOAD_LATER = "DownloadLater";
    public static final String DYNAMIC_COLOR_ANDROID = "DynamicColorAndroid";
    public static final String DYNAMIC_COLOR_BUTTONS_ANDROID = "DynamicColorButtonsAndroid";
    public static final String EARLY_LIBRARY_LOAD = "EarlyLibraryLoad";
    public static final String ELIDE_PRIORITIZATION_OF_PRE_NATIVE_BOOTSTRAP_TASKS =
            "ElidePrioritizationOfPreNativeBootstrapTasks";
    public static final String ELIDE_TAB_PRELOAD_AT_STARTUP = "ElideTabPreloadAtStartup";
    public static final String ENABLE_AUTOMATIC_SNOOZE = "EnableAutomaticSnooze";
    public static final String ELASTIC_OVERSCROLL = "ElasticOverscroll";
    public static final String EXPERIMENTS_FOR_AGSA = "ExperimentsForAgsa";
    public static final String EXPLICIT_LANGUAGE_ASK = "ExplicitLanguageAsk";
    public static final String EXPLORE_SITES = "ExploreSites";
    public static final String FEATURE_NOTIFICATION_GUIDE = "FeatureNotificationGuide";
    public static final String FEED_BACK_TO_TOP = "FeedBackToTop";
    public static final String FEED_CLEAR_IMAGE_MEMORY_CACHE = "FeedClearImageMemoryCache";
    public static final String FEED_IMAGE_MEMORY_CACHE_SIZE_PERCENTAGE =
            "FeedImageMemoryCacheSizePercentage";
    public static final String FEED_INTERACTIVE_REFRESH = "FeedInteractiveRefresh";
    public static final String FEED_LOADING_PLACEHOLDER = "FeedLoadingPlaceholder";
    public static final String FEED_RELIABILITY_LOGGING = "FeedReliabilityLogging";
    public static final String FILLING_PASSWORDS_FROM_ANY_ORIGIN = "FillingPasswordsFromAnyOrigin";
    public static final String FIXED_UMA_SESSION_RESUME_ORDER = "FixedUmaSessionResumeOrder";
    public static final String FOCUS_OMNIBOX_IN_INCOGNITO_TAB_INTENTS =
            "FocusOmniboxInIncognitoTabIntents";
    public static final String FORCE_APP_LANGUAGE_PROMPT = "ForceAppLanguagePrompt";
    public static final String FORCE_WEB_CONTENTS_DARK_MODE = "WebContentsForceDark";
    public static final String FORCE_DISABLE_EXTENDED_SYNC_PROMOS =
            "ForceDisableExtendedSyncPromos";
    public static final String FORCE_STARTUP_SIGNIN_PROMO = "ForceStartupSigninPromo";
    public static final String TANGIBLE_SYNC = "TangibleSync";
    public static final String GIVE_JAVA_UI_THREAD_DEFAULT_TASK_TRAITS_USER_BLOCKING_PRIORITY =
            "GiveJavaUiThreadDefaultTaskTraitsUserBlockingPriority";
    public static final String GRID_TAB_SWITCHER_FOR_TABLETS = "GridTabSwitcherForTablets";
    public static final String HANDLE_MEDIA_INTENTS = "HandleMediaIntents";
    public static final String HTTPS_FIRST_MODE = "HttpsOnlyMode";
    public static final String IMMERSIVE_UI_MODE = "ImmersiveUiMode";
    public static final String IMPROVE_READER_MODE_PROMPT = "ImproveReaderModePrompt";
    public static final String INCOGNITO_DOWNLOADS_WARNING = "IncognitoDownloadsWarning";
    public static final String INCOGNITO_NTP_REVAMP = "IncognitoNtpRevamp";
    public static final String INCOGNITO_REAUTHENTICATION_FOR_ANDROID =
            "IncognitoReauthenticationForAndroid";
    public static final String INCOGNITO_SCREENSHOT = "IncognitoScreenshot";
    public static final String INSTALLABLE_AMBIENT_BADGE_INFOBAR = "InstallableAmbientBadgeInfoBar";
    public static final String INSTALLABLE_AMBIENT_BADGE_MESSAGE = "InstallableAmbientBadgeMessage";
    public static final String INSTANCE_SWITCHER = "InstanceSwitcher";
    public static final String INSTANT_START = "InstantStart";
    public static final String INTEREST_FEEDV1_CLICKS_AND_VIEWS_CONDITIONAL_UPLOAD =
            "InterestFeedV1ClickAndViewActionsConditionalUpload";
    public static final String INTEREST_FEED_CONTENT_SUGGESTIONS = "InterestFeedContentSuggestions";
    public static final String INTEREST_FEED_NOTICE_CARD_AUTO_DISMISS =
            "InterestFeedNoticeCardAutoDismiss";
    public static final String INTEREST_FEED_SPINNER_ALWAYS_ANIMATE =
            "InterestFeedSpinnerAlwaysAnimate";
    public static final String INTEREST_FEED_V2 = "InterestFeedV2";
    public static final String INTEREST_FEED_V2_HEARTS = "InterestFeedV2Hearts";
    public static final String INTEREST_FEED_V2_AUTOPLAY = "InterestFeedV2Autoplay";
    public static final String IS_VOICE_SEARCH_ENABLED_CACHE = "IsVoiceSearchEnabledCache";
    public static final String HISTORY_JOURNEYS = "Journeys";
    public static final String KITKAT_SUPPORTED = "KitKatSupported";
    public static final String LEAK_DETECTION_UNAUTHENTICATED = "LeakDetectionUnauthenticated";
    public static final String LENS_ON_QUICK_ACTION_SEARCH_WIDGET = "LensOnQuickActionSearchWidget";
    public static final String LIGHTWEIGHT_REACTIONS = "LightweightReactions";
    public static final String LOCATION_BAR_MODEL_OPTIMIZATIONS = "LocationBarModelOptimizations";
    public static final String LOOKALIKE_NAVIGATION_URL_SUGGESTIONS_UI =
            "LookalikeUrlNavigationSuggestionsUI";
    public static final String MARK_HTTP_AS = "MarkHttpAs";
    public static final String MESSAGES_FOR_ANDROID_ADS_BLOCKED = "MessagesForAndroidAdsBlocked";
    public static final String MESSAGES_FOR_ANDROID_CHROME_SURVEY =
            "MessagesForAndroidChromeSurvey";
    public static final String MESSAGES_FOR_ANDROID_INFRASTRUCTURE =
            "MessagesForAndroidInfrastructure";
    public static final String MESSAGES_FOR_ANDROID_INSTANT_APPS = "MessagesForAndroidInstantApps";
    public static final String MESSAGES_FOR_ANDROID_PERMISSION_UPDATE =
            "MessagesForAndroidPermissionUpdate";
    public static final String MESSAGES_FOR_ANDROID_READER_MODE = "MessagesForAndroidReaderMode";
    public static final String MESSAGES_FOR_ANDROID_REDUCE_LAYOUT_CHANGES =
            "MessagesForAndroidReduceLayoutChanges";
    public static final String MESSAGES_FOR_ANDROID_SAVE_CARD = "MessagesForAndroidSaveCard";
    public static final String MESSAGES_FOR_ANDROID_STACKING_ANIMATION =
            "MessagesForAndroidStackingAnimation";
    public static final String MESSAGES_FOR_ANDROID_SYNC_ERROR = "MessagesForAndroidSyncError";
    public static final String MODAL_PERMISSION_DIALOG_VIEW = "ModalPermissionDialogView";
    public static final String METRICS_SETTINGS_ANDROID = "MetricsSettingsAndroid";
    public static final String MOST_RECENT_TAB_ON_BACKGROUND_CLOSE_TAB =
            "MostRecentTabOnBackgroundCloseTab";
    public static final String NEW_WINDOW_APP_MENU = "NewWindowAppMenu";
    public static final String NOTIFICATION_PERMISSION_VARIANT = "NotificationPermissionVariant";
    public static final String OFFLINE_INDICATOR = "OfflineIndicator";
    public static final String OFFLINE_PAGES_DESCRIPTIVE_FAIL_STATUS =
            "OfflinePagesDescriptiveFailStatus";
    public static final String OFFLINE_PAGES_DESCRIPTIVE_PENDING_STATUS =
            "OfflinePagesDescriptivePendingStatus";
    public static final String OFFLINE_PAGES_LIVE_PAGE_SHARING = "OfflinePagesLivePageSharing";
    public static final String OFFLINE_PAGES_PREFETCHING = "OfflinePagesPrefetching";
    public static final String OMNIBOX_ANDROID_AUXILIARY_SEARCH =
            "AndroidAuxiliarySearch";
    public static final String OMNIBOX_ASSISTANT_VOICE_SEARCH = "OmniboxAssistantVoiceSearch";
    public static final String OMNIBOX_UPDATED_CONNECTION_SECURITY_INDICATORS =
            "OmniboxUpdatedConnectionSecurityIndicators";
    public static final String OPTIMIZATION_GUIDE_PUSH_NOTIFICATIONS =
            "OptimizationGuidePushNotifications";
    public static final String OVERLAY_NEW_LAYOUT = "OverlayNewLayout";
    public static final String PAGE_ANNOTATIONS_SERVICE = "PageAnnotationsService";
    public static final String PAGE_INFO_ABOUT_THIS_SITE_EN = "PageInfoAboutThisSiteEn";
    public static final String PAGE_INFO_ABOUT_THIS_SITE_MORE_INFO =
            "PageInfoAboutThisSiteMoreInfo";
    public static final String PAGE_INFO_ABOUT_THIS_SITE_NON_EN = "PageInfoAboutThisSiteNonEn";
    public static final String PAGE_INFO_DISCOVERABILITY = "PageInfoDiscoverability";
    public static final String PAINT_PREVIEW_DEMO = "PaintPreviewDemo";
    public static final String PAINT_PREVIEW_SHOW_ON_STARTUP = "PaintPreviewShowOnStartup";
    public static final String PASSWORD_DOMAIN_CAPABILITIES_FETCHING =
            "PasswordDomainCapabilitiesFetching";
    public static final String PASSWORD_EDIT_DIALOG_WITH_DETAILS = "PasswordEditDialogWithDetails";
    public static final String PASSWORD_SCRIPTS_FETCHING = "PasswordScriptsFetching";
    public static final String PERMISSION_DELEGATION = "PermissionDelegation";
    public static final String PERSIST_SHARE_HUB_ON_APP_SWITCH = "PersistShareHubOnAppSwitch";
    public static final String PORTALS = "Portals";
    public static final String PORTALS_CROSS_ORIGIN = "PortalsCrossOrigin";
    public static final String POST_TASK_FOCUS_TAB = "PostTaskFocusTab";
    public static final String PWA_DEFAULT_OFFLINE_PAGE = "AndroidPWAsDefaultOfflinePage";
    public static final String BOOKMARKS_IMPROVED_SAVE_FLOW = "BookmarksImprovedSaveFlow";
    public static final String BOOKMARKS_REFRESH = "BookmarksRefresh";
    public static final String PREEMPTIVE_LINK_TO_TEXT_GENERATION =
            "PreemptiveLinkToTextGeneration";
    public static final String PREDICTIVE_PREFETCHING_ALLOWED_ON_ALL_CONNECTION_TYPES =
            "PredictivePrefetchingAllowedOnAllConnectionTypes";
    public static final String PREFETCH_NOTIFICATION_SCHEDULING_INTEGRATION =
            "PrefetchNotificationSchedulingIntegration";
    public static final String PRERENDER2 = "Prerender2";
    public static final String PRIVACY_REVIEW = "PrivacyGuide";
    public static final String PRIVACY_SANDBOX_SETTINGS_3 = "PrivacySandboxSettings3";
    public static final String PROBABILISTIC_CRYPTID_RENDERER = "ProbabilisticCryptidRenderer";
    public static final String PUSH_MESSAGING_DISALLOW_SENDER_IDS =
            "PushMessagingDisallowSenderIDs";
    public static final String PWA_UPDATE_DIALOG_FOR_ICON = "PwaUpdateDialogForIcon";
    public static final String PWA_UPDATE_DIALOG_FOR_NAME = "PwaUpdateDialogForName";
    public static final String QUERY_TILES = "QueryTiles";
    public static final String QUERY_TILES_IN_NTP = "QueryTilesInNTP";
    public static final String QUERY_TILES_SEGMENTATION = "QueryTilesSegmentation";
    public static final String QUICK_ACTION_SEARCH_WIDGET = "QuickActionSearchWidgetAndroid";
    public static final String QUIET_NOTIFICATION_PROMPTS = "QuietNotificationPrompts";
    public static final String REACHED_CODE_PROFILER = "ReachedCodeProfiler";
    public static final String READ_LATER = "ReadLater";
    public static final String READER_MODE_IN_CCT = "ReaderModeInCCT";
    public static final String RECOVER_FROM_NEVER_SAVE_ANDROID = "RecoverFromNeverSaveAndroid";
    public static final String REENGAGEMENT_NOTIFICATION = "ReengagementNotification";
    public static final String RELATED_SEARCHES = "RelatedSearches";
    public static final String RELATED_SEARCHES_ALTERNATE_UX = "RelatedSearchesAlternateUx";
    public static final String RELATED_SEARCHES_IN_BAR = "RelatedSearchesInBar";
    public static final String RELATED_SEARCHES_SIMPLIFIED_UX = "RelatedSearchesSimplifiedUx";
    public static final String RELATED_SEARCHES_UI = "RelatedSearchesUi";
    public static final String REQUEST_DESKTOP_SITE_FOR_TABLETS = "RequestDesktopSiteForTablets";
    public static final String SAFE_BROWSING_DELAYED_WARNINGS = "SafeBrowsingDelayedWarnings";
    public static final String SEARCH_ENGINE_PROMO_EXISTING_DEVICE =
            "SearchEnginePromo.ExistingDevice";
    public static final String SEARCH_ENGINE_PROMO_EXISTING_DEVICE_V2 =
            "SearchEnginePromo.ExistingDeviceVer2";
    public static final String SEARCH_ENGINE_PROMO_NEW_DEVICE = "SearchEnginePromo.NewDevice";
    public static final String SEARCH_ENGINE_PROMO_NEW_DEVICE_V2 =
            "SearchEnginePromo.NewDeviceVer2";
    public static final String SEND_TAB_TO_SELF_SIGNIN_PROMO = "SendTabToSelfSigninPromo";
    public static final String SEND_TAB_TO_SELF_V2 = "SendTabToSelfV2";
    public static final String SERVICE_MANAGER_FOR_BACKGROUND_PREFETCH =
            "ServiceManagerForBackgroundPrefetch";
    public static final String SERVICE_MANAGER_FOR_DOWNLOAD = "ServiceManagerForDownload";
    public static final String SHARE_BUTTON_IN_TOP_TOOLBAR = "ShareButtonInTopToolbar";
    public static final String SHARE_CROW_BUTTON = "ShareCrowButton";
    public static final String SHARED_CLIPBOARD_UI = "SharedClipboardUI";
    public static final String SHARED_HIGHLIGHTING_AMP = "SharedHighlightingAmp";
    public static final String SHOPPING_LIST = "ShoppingList";
    public static final String SHOW_EXTENDED_PRELOADING_SETTING = "ShowExtendedPreloadingSetting";
    public static final String SHOW_SCROLLABLE_MVT_ON_NTP_ANDROID = "ShowScrollableMVTOnNTPAndroid";
    public static final String SHOW_TRUSTED_PUBLISHER_URL = "ShowTrustedPublisherURL";
    public static final String SMART_SUGGESTION_FOR_LARGE_DOWNLOADS =
            "SmartSuggestionForLargeDownloads";
    public static final String SNOOZABLE_IPH = "IPH_Snooze";
    public static final String SPANNABLE_INLINE_AUTOCOMPLETE = "SpannableInlineAutocomplete";
    public static final String SPLIT_COMPOSITOR_TASK = "SplitCompositorTask";
    public static final String SPLIT_CACHE_BY_NETWORK_ISOLATION_KEY =
            "SplitCacheByNetworkIsolationKey";
    public static final String START_SURFACE_ANDROID = "StartSurfaceAndroid";
    public static final String STORE_HOURS = "StoreHoursAndroid";
    public static final String SUPPRESS_TOOLBAR_CAPTURES = "SuppressToolbarCaptures";
    public static final String SWAP_PIXEL_FORMAT_TO_FIX_CONVERT_FROM_TRANSLUCENT =
            "SwapPixelFormatToFixConvertFromTranslucent";
    public static final String ENABLE_SYNC_IMMEDIATELY_IN_FRE = "EnableSyncImmediatelyInFRE";
    public static final String SYNC_TRUSTED_VAULT_PASSPHRASE_RECOVERY =
            "SyncTrustedVaultPassphraseRecovery";
    public static final String TAB_ENGAGEMENT_REPORTING_ANDROID = "TabEngagementReportingAndroid";
    public static final String TAB_GROUPS_ANDROID = "TabGroupsAndroid";
    public static final String TAB_GROUPS_UI_IMPROVEMENTS_ANDROID =
            "TabGroupsUiImprovementsAndroid";
    public static final String TAB_GROUPS_CONTINUATION_ANDROID = "TabGroupsContinuationAndroid";
    public static final String TAB_GROUPS_FOR_TABLETS = "TabGroupsForTablets";
    public static final String TAB_GRID_LAYOUT_ANDROID = "TabGridLayoutAndroid";
    public static final String TAB_REPARENTING = "TabReparenting";
    public static final String TAB_SWITCHER_ON_RETURN = "TabSwitcherOnReturn";
    public static final String TAB_TO_GTS_ANIMATION = "TabToGTSAnimation";
    public static final String TAB_STRIP_IMPROVEMENTS = "TabStripImprovements";
    public static final String TEST_DEFAULT_DISABLED = "TestDefaultDisabled";
    public static final String TEST_DEFAULT_ENABLED = "TestDefaultEnabled";
    public static final String TOOLBAR_IPH_ANDROID = "ToolbarIphAndroid";
    public static final String TOOLBAR_MIC_IPH_ANDROID = "ToolbarMicIphAndroid";
    public static final String TOOLBAR_USE_HARDWARE_BITMAP_DRAW = "ToolbarUseHardwareBitmapDraw";
    public static final String TOUCH_TO_FILL_PASSWORD_SUBMISSION = "TouchToFillPasswordSubmission";
    public static final String TRANSLATE_ASSIST_CONTENT = "TranslateAssistContent";
    public static final String TRANSLATE_INTENT = "TranslateIntent";
    public static final String TRANSLATE_TFLITE = "TFLiteLanguageDetectionEnabled";
    public static final String TRUSTED_WEB_ACTIVITY_LOCATION_DELEGATION =
            "TrustedWebActivityLocationDelegation";
    public static final String TRUSTED_WEB_ACTIVITY_NEW_DISCLOSURE =
            "TrustedWebActivityNewDisclosure";
    public static final String TRUSTED_WEB_ACTIVITY_NOTIFICATION_PERMISSION_DELEGATION =
            "TrustedWebActivityNotificationPermissionDelegation";
    public static final String TRUSTED_WEB_ACTIVITY_POST_MESSAGE = "TrustedWebActivityPostMessage";
    public static final String TRUSTED_WEB_ACTIVITY_QUALITY_ENFORCEMENT =
            "TrustedWebActivityQualityEnforcement";
    public static final String TRUSTED_WEB_ACTIVITY_QUALITY_ENFORCEMENT_FORCED =
            "TrustedWebActivityQualityEnforcementForced";
    public static final String TRUSTED_WEB_ACTIVITY_QUALITY_ENFORCEMENT_WARNING =
            "TrustedWebActivityQualityEnforcementWarning";
    public static final String VIDEO_TUTORIALS = "VideoTutorials";
    public static final String UNIFIED_CREDENTIAL_MANAGER_DRY_RUN =
            "UnifiedCredentialManagerDryRun";
    public static final String UNIFIED_PASSWORD_MANAGER_ANDROID = "UnifiedPasswordManagerAndroid";
    public static final String UPCOMING_SHARING_FEATURES = "UpcomingSharingFeatures";
    public static final String UPDATE_NOTIFICATION_IMMEDIATE_SHOW_OPTION =
            "UpdateNotificationScheduleServiceImmediateShowOption";
    public static final String UPDATE_HISTORY_ENTRY_POINTS_IN_INCOGNITO =
            "UpdateHistoryEntryPointsInIncognito";
    public static final String USE_CHIME_ANDROID_SDK = "UseChimeAndroidSdk";
    public static final String USE_CLIENT_CONFIG_IPH = "UseClientConfigIPH";
    public static final String VOICE_SEARCH_AUDIO_CAPTURE_POLICY = "VoiceSearchAudioCapturePolicy";
    public static final String VOICE_BUTTON_IN_TOP_TOOLBAR = "VoiceButtonInTopToolbar";
    public static final String VR_BROWSING_FEEDBACK = "VrBrowsingFeedback";
    public static final String WEB_FEED = "WebFeed";
    public static final String WEB_FEED_AWARENESS = "WebFeedAwareness";
    public static final String WEB_FEED_ONBOARDING = "WebFeedOnboarding";
    public static final String WEB_FEED_SORT = "WebFeedSort";
    public static final String WEBNOTES_DYNAMIC_TEMPLATES = "WebNotesDynamicTemplates";
    public static final String WEBNOTES_STYLIZE = "WebNotesStylize";
    public static final String WEB_APK_INSTALL_COMPLETE_NOTIFICATION =
            "WebApkInstallCompleteNotification";
    public static final String WEB_APK_TRAMPOLINE_ON_INITIAL_INTENT =
            "WebApkTrampolineOnInitialIntent";
    public static final String WEB_APK_INSTALL_SERVICE = "WebApkInstallService";
    public static final String XSURFACE_METRICS_REPORTING = "XsurfaceMetricsReporting";
    public static final String WEB_OTP_CROSS_DEVICE_SIMPLE_STRING = "WebOtpCrossDeviceSimpleString";

    @NativeMethods
    interface Natives {
        boolean isEnabled(String featureName);
        String getFieldTrialParamByFeature(String featureName, String paramName);
        int getFieldTrialParamByFeatureAsInt(
                String featureName, String paramName, int defaultValue);
        double getFieldTrialParamByFeatureAsDouble(
                String featureName, String paramName, double defaultValue);
        boolean getFieldTrialParamByFeatureAsBoolean(
                String featureName, String paramName, boolean defaultValue);
        String[] getFlattedFieldTrialParamsForFeature(String featureName);
    }
}
