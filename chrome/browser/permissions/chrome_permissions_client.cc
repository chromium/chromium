// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/chrome_permissions_client.h"

#include <optional>
#include <vector>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/ash/shimless_rma/chrome_shimless_rma_delegate.h"
#include "chrome/browser/bluetooth/bluetooth_chooser_context_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/important_sites_util.h"
#include "chrome/browser/metrics/ukm_background_recorder_service.h"
#include "chrome/browser/permissions/adaptive_quiet_notification_permission_ui_enabler.h"
#include "chrome/browser/permissions/contextual_notification_permission_ui_selector.h"
#include "chrome/browser/permissions/origin_keyed_permission_action_service_factory.h"
#include "chrome/browser/permissions/permission_actions_history_factory.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/permissions/permission_revocation_request.h"
#include "chrome/browser/permissions/prediction_based_permission_ui_selector.h"
#include "chrome/browser/permissions/pref_based_quiet_permission_ui_selector.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"
#include "chrome/browser/permissions/system/system_permission_settings.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/content_settings/core/browser/content_settings_type_set.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/google/core/common/google_util.h"
#include "components/permissions/constants.h"
#include "components/permissions/contexts/bluetooth_chooser_context.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_hats_trigger_helper.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/request_type.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/subresource_filter/content/browser/subresource_filter_content_settings_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_profile_context.h"
#include "components/unified_consent/pref_names.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/android/search_permissions/search_permissions_service.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/permissions/permission_blocked_message_delegate_android.h"
#include "chrome/browser/permissions/permission_infobar_delegate_android.h"
#include "chrome/browser/permissions/permission_update_message_controller_android.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/permissions/permission_request_manager.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/permission_bubble/permission_prompt.h"
#include "components/vector_icons/vector_icons.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_data.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/app_mode/kiosk_session_service_lacros.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif

namespace {

#if BUILDFLAG(IS_ANDROID)
bool ShouldUseQuietUI(content::WebContents* web_contents,
                      ContentSettingsType type) {
  auto* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents);
  if (type != ContentSettingsType::NOTIFICATIONS &&
      type != ContentSettingsType::GEOLOCATION) {
    return false;
  }
  return manager->ShouldCurrentRequestUseQuietUI();
}
#endif

}  // namespace

// static
ChromePermissionsClient* ChromePermissionsClient::GetInstance() {
  static base::NoDestructor<ChromePermissionsClient> instance;
  return instance.get();
}

HostContentSettingsMap* ChromePermissionsClient::GetSettingsMap(
    content::BrowserContext* browser_context) {
  return HostContentSettingsMapFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context));
}

scoped_refptr<content_settings::CookieSettings>
ChromePermissionsClient::GetCookieSettings(
    content::BrowserContext* browser_context) {
  return CookieSettingsFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context));
}

privacy_sandbox::TrackingProtectionSettings*
ChromePermissionsClient::GetTrackingProtectionSettings(
    content::BrowserContext* browser_context) {
  return TrackingProtectionSettingsFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context));
}

bool ChromePermissionsClient::IsSubresourceFilterActivated(
    content::BrowserContext* browser_context,
    const GURL& url) {
  return SubresourceFilterProfileContextFactory::GetForProfile(
             Profile::FromBrowserContext(browser_context))
      ->settings_manager()
      ->GetSiteActivationFromMetadata(url);
}

permissions::ObjectPermissionContextBase*
ChromePermissionsClient::GetChooserContext(
    content::BrowserContext* browser_context,
    ContentSettingsType type) {
  switch (type) {
    case ContentSettingsType::USB_CHOOSER_DATA:
      return UsbChooserContextFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));
    case ContentSettingsType::BLUETOOTH_CHOOSER_DATA:
      return BluetoothChooserContextFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

permissions::OriginKeyedPermissionActionService*
ChromePermissionsClient::GetOriginKeyedPermissionActionService(
    content::BrowserContext* browser_context) {
  return OriginKeyedPermissionActionServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context));
}

permissions::PermissionActionsHistory*
ChromePermissionsClient::GetPermissionActionsHistory(
    content::BrowserContext* browser_context) {
  return PermissionActionsHistoryFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context));
}

permissions::PermissionDecisionAutoBlocker*
ChromePermissionsClient::GetPermissionDecisionAutoBlocker(
    content::BrowserContext* browser_context) {
  return PermissionDecisionAutoBlockerFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context));
}

double ChromePermissionsClient::GetSiteEngagementScore(
    content::BrowserContext* browser_context,
    const GURL& origin) {
  return site_engagement::SiteEngagementService::Get(
             Profile::FromBrowserContext(browser_context))
      ->GetScore(origin);
}

void ChromePermissionsClient::AreSitesImportant(
    content::BrowserContext* browser_context,
    std::vector<std::pair<url::Origin, bool>>* origins) {
  // We need to limit our size due to the algorithm in ImportantSiteUtil,
  // but we want to be more on the liberal side here as we're not exposing
  // these sites to the user, we're just using them for our 'clear
  // unimportant' feature in ManageSpaceActivity.java.
  const int kMaxImportantSites = 10;
  std::vector<site_engagement::ImportantSitesUtil::ImportantDomainInfo>
      important_domains =
          site_engagement::ImportantSitesUtil::GetImportantRegisterableDomains(
              Profile::FromBrowserContext(browser_context), kMaxImportantSites);

  for (auto& entry : *origins) {
    const url::Origin& origin = entry.first;
    const std::string& host = origin.host();
    std::string registerable_domain =
        net::registry_controlled_domains::GetDomainAndRegistry(
            origin,
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
    if (registerable_domain.empty())
      registerable_domain = host;  // IP address or internal hostname.
    entry.second = base::Contains(important_domains, registerable_domain,
                                  &site_engagement::ImportantSitesUtil::
                                      ImportantDomainInfo::registerable_domain);
  }
}

// Some Google-affiliated domains are not allowed to delete cookies for
// supervised accounts.
bool ChromePermissionsClient::IsCookieDeletionDisabled(
    content::BrowserContext* browser_context,
    const GURL& origin) {
  if (!Profile::FromBrowserContext(browser_context)->IsChild())
    return false;

  return google_util::IsYoutubeDomainUrl(origin, google_util::ALLOW_SUBDOMAIN,
                                         google_util::ALLOW_NON_STANDARD_PORTS);
}

void ChromePermissionsClient::GetUkmSourceId(
    ContentSettingsType permission_type,
    content::BrowserContext* browser_context,
    content::WebContents* web_contents,
    const GURL& requesting_origin,
    GetUkmSourceIdCallback callback) {
  if (web_contents) {
    ukm::SourceId source_id =
        web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();
    std::move(callback).Run(source_id);
  } else if (permission_type == ContentSettingsType::NOTIFICATIONS) {
    ukm::SourceId source_id =
        ukm::UkmRecorder::GetSourceIdForNotificationPermission(
            base::PassKey<ChromePermissionsClient>(), requesting_origin);
    std::move(callback).Run(source_id);
  } else {
    // We only record a permission change if the origin is in the user's
    // history.
    ukm::UkmBackgroundRecorderFactory::GetForProfile(
        Profile::FromBrowserContext(browser_context))
        ->GetBackgroundSourceIdIfAllowed(url::Origin::Create(requesting_origin),
                                         std::move(callback));
  }
}

permissions::IconId ChromePermissionsClient::GetOverrideIconId(
    permissions::RequestType request_type) {
#if BUILDFLAG(IS_CHROMEOS)
  // TODO(xhwang): fix this icon, see crbug.com/446263.
  if (request_type == permissions::RequestType::kProtectedMediaIdentifier)
    return vector_icons::kProductIcon;
#endif
  return PermissionsClient::GetOverrideIconId(request_type);
}

// Triggers the prompt HaTS survey if enabled by field trials for this
// combination of prompt parameters.
void ChromePermissionsClient::TriggerPromptHatsSurveyIfEnabled(
    content::WebContents* web_contents,
    permissions::RequestType request_type,
    std::optional<permissions::PermissionAction> action,
    permissions::PermissionPromptDisposition prompt_disposition,
    permissions::PermissionPromptDispositionReason prompt_disposition_reason,
    permissions::PermissionRequestGestureType gesture_type,
    std::optional<base::TimeDelta> prompt_display_duration,
    bool is_post_prompt,
    const GURL& gurl,
    std::optional<permissions::feature_params::PermissionElementPromptPosition>
        pepc_prompt_position,
    ContentSetting initial_permission_status,
    base::OnceCallback<void()> hats_shown_callback) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  std::optional<GURL> recorded_gurl =
      profile->GetPrefs()->GetBoolean(
          unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled)
          ? std::make_optional(gurl)
          : std::nullopt;

  auto prompt_parameters =
      permissions::PermissionHatsTriggerHelper::PromptParametersForHats(
          request_type, action, prompt_disposition, prompt_disposition_reason,
          gesture_type,
          std::string(version_info::GetChannelString(chrome::GetChannel())),
          is_post_prompt ? permissions::kOnPromptResolved
                         : permissions::kOnPromptAppearing,
          prompt_display_duration,
          permissions::PermissionHatsTriggerHelper::
              GetOneTimePromptsDecidedBucket(profile->GetPrefs()),
          recorded_gurl, pepc_prompt_position, initial_permission_status);

  if (!permissions::PermissionHatsTriggerHelper::
          ArePromptTriggerCriteriaSatisfied(prompt_parameters)) {
    return;
  }

  std::optional<
      permissions::PermissionHatsTriggerHelper::SurveyParametersForHats>
      survey_parameters = permissions::PermissionHatsTriggerHelper::
          GetSurveyParametersForRequestType(request_type);

  auto* hats_service =
      HatsServiceFactory::GetForProfile(profile,
                                        /*create_if_necessary=*/true);
  if (!hats_service || !survey_parameters.has_value()) {
    return;
  }

  auto survey_data = permissions::PermissionHatsTriggerHelper::
      SurveyProductSpecificData::PopulateFrom(prompt_parameters);

  hats_service->LaunchSurveyForWebContents(
      kHatsSurveyTriggerPermissionsPrompt, web_contents,
      survey_data.survey_bits_data, survey_data.survey_string_data,
      std::move(hats_shown_callback), base::DoNothing(),
      survey_parameters->supplied_trigger_id,
      HatsService::SurveyOptions(survey_parameters->custom_survey_invitation,
                                 survey_parameters->message_identifier));
}

#if !BUILDFLAG(IS_ANDROID)
permissions::PermissionIgnoredReason
ChromePermissionsClient::DetermineIgnoreReason(
    content::WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  Browser* browser = chrome::FindLastActiveWithProfile(profile);
  if (browser) {
    if (browser->tab_strip_model()->empty()) {
      return permissions::PermissionIgnoredReason::WINDOW_CLOSED;
    } else if (web_contents->IsBeingDestroyed()) {
      return permissions::PermissionIgnoredReason::TAB_CLOSED;
    } else {
      return permissions::PermissionIgnoredReason::NAVIGATION;
    }
  }
  return permissions::PermissionIgnoredReason::UNKNOWN;
}
#endif

std::vector<std::unique_ptr<permissions::PermissionUiSelector>>
ChromePermissionsClient::CreatePermissionUiSelectors(
    content::BrowserContext* browser_context) {
  std::vector<std::unique_ptr<permissions::PermissionUiSelector>> selectors;
  selectors.emplace_back(
      std::make_unique<ContextualNotificationPermissionUiSelector>());
  selectors.emplace_back(std::make_unique<PrefBasedQuietPermissionUiSelector>(
      Profile::FromBrowserContext(browser_context)));
  selectors.emplace_back(std::make_unique<PredictionBasedPermissionUiSelector>(
      Profile::FromBrowserContext(browser_context)));
  return selectors;
}

void ChromePermissionsClient::OnPromptResolved(
    permissions::RequestType request_type,
    permissions::PermissionAction action,
    const GURL& origin,
    permissions::PermissionPromptDisposition prompt_disposition,
    permissions::PermissionPromptDispositionReason prompt_disposition_reason,
    permissions::PermissionRequestGestureType gesture_type,
    std::optional<QuietUiReason> quiet_ui_reason,
    base::TimeDelta prompt_display_duration,
    std::optional<permissions::feature_params::PermissionElementPromptPosition>
        pepc_prompt_position,
    ContentSetting initial_permission_status,
    content::WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  PermissionActionsHistoryFactory::GetForProfile(profile)->RecordAction(
      action, request_type, prompt_disposition);

  if (request_type == permissions::RequestType::kNotifications) {
    AdaptiveQuietNotificationPermissionUiEnabler::GetForProfile(profile)
        ->PermissionPromptResolved();
    if (action == permissions::PermissionAction::GRANTED &&
        quiet_ui_reason.has_value() &&
        (quiet_ui_reason.value() ==
             QuietUiReason::kTriggeredDueToAbusiveRequests ||
         quiet_ui_reason.value() ==
             QuietUiReason::kTriggeredDueToAbusiveContent ||
         quiet_ui_reason.value() ==
             QuietUiReason::kTriggeredDueToDisruptiveBehavior)) {
      PermissionRevocationRequest::ExemptOriginFromFutureRevocations(profile,
                                                                     origin);
    }
    if (action == permissions::PermissionAction::GRANTED) {
      if (g_browser_process->safe_browsing_service()) {
        g_browser_process->safe_browsing_service()
            ->MaybeSendNotificationsAcceptedReport(
                web_contents->GetPrimaryMainFrame(), profile,
                web_contents->GetLastCommittedURL(),
                web_contents->GetController().GetLastCommittedEntry()->GetURL(),
                origin, prompt_display_duration);
      }
    }
  }

  auto content_setting_type = RequestTypeToContentSettingsType(request_type);
  if (content_setting_type.has_value()) {
    permissions::PermissionHatsTriggerHelper::
        IncrementOneTimePermissionPromptsDecidedIfApplicable(
            content_setting_type.value(), profile->GetPrefs());
  }

  TriggerPromptHatsSurveyIfEnabled(
      web_contents, request_type, std::make_optional(action),
      prompt_disposition, prompt_disposition_reason, gesture_type,
      std::make_optional(prompt_display_duration), /*is_post_prompt=*/true,
      web_contents->GetLastCommittedURL(), pepc_prompt_position,
      initial_permission_status, base::DoNothing());
}

std::optional<bool>
ChromePermissionsClient::HadThreeConsecutiveNotificationPermissionDenies(
    content::BrowserContext* browser_context) {
  if (!QuietNotificationPermissionUiConfig::
          IsAdaptiveActivationDryRunEnabled()) {
    return std::nullopt;
  }
  return Profile::FromBrowserContext(browser_context)
      ->GetPrefs()
      ->GetBoolean(prefs::kHadThreeConsecutiveNotificationPermissionDenies);
}

std::optional<bool> ChromePermissionsClient::HasPreviouslyAutoRevokedPermission(
    content::BrowserContext* browser_context,
    const GURL& origin,
    ContentSettingsType permission) {
  if (permission != ContentSettingsType::NOTIFICATIONS) {
    return std::nullopt;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context);
  return PermissionRevocationRequest::HasPreviouslyRevokedPermission(profile,
                                                                     origin);
}

std::optional<url::Origin> ChromePermissionsClient::GetAutoApprovalOrigin(
    content::BrowserContext* browser_context) {
  // In web kiosk mode, all permission requests are auto-approved for the origin
  // of the main app.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (user_manager::UserManager::IsInitialized() &&
      user_manager::UserManager::Get()->IsLoggedInAsWebKioskApp()) {
    const AccountId& account_id =
        user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId();
    DCHECK(ash::WebKioskAppManager::IsInitialized());
    const ash::WebKioskAppData* app_data =
        ash::WebKioskAppManager::Get()->GetAppByAccountId(account_id);
    DCHECK(app_data);
    return url::Origin::Create(app_data->install_url());
  }

  // In Shimless RMA mode, permission requests are auto-approved during runtime
  // since the app has requested all permissions during install time.
  if (ash::features::IsShimlessRMA3pDiagnosticsAllowPermissionPolicyEnabled() &&
      ash::IsShimlessRmaAppBrowserContext(browser_context)) {
    return ash::shimless_rma::DiagnosticsAppProfileHelperDelegate::
        GetInstalledDiagnosticsAppOrigin();
  }
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  if (profiles::IsWebKioskSession()) {
    return url::Origin::Create(
        KioskSessionServiceLacros::Get()->GetInstallURL());
  }
#endif
  return std::nullopt;
}

std::optional<permissions::PermissionAction>
ChromePermissionsClient::GetAutoApprovalStatus(
    content::BrowserContext* browser_context,
    const GURL& origin) {
  if (base::FeatureList::IsEnabled(
          permissions::features::kAllowMultipleOriginsForWebKioskPermissions)) {
    Profile* profile = Profile::FromBrowserContext(browser_context);
    if (IsWebKioskOriginAllowed(profile->GetPrefs(), origin)) {
      return permissions::PermissionAction::GRANTED;
    }
  }

  std::optional<url::Origin> auto_approval_origin =
      GetAutoApprovalOrigin(browser_context);

  if (!auto_approval_origin.has_value()) {
    return std::nullopt;
  }

  if (url::Origin::Create(origin) == auto_approval_origin.value()) {
    return permissions::PermissionAction::GRANTED;
  }

  return permissions::PermissionAction::IGNORED;
}

bool ChromePermissionsClient::CanBypassEmbeddingOriginCheck(
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Extensions are excluded from origin checks as currently they can request
  // permission from iframes when embedded in non-secure contexts
  // (https://crbug.com/530507).
  if (requesting_origin.SchemeIs(extensions::kExtensionScheme))
    return true;
#endif

  // The New Tab Page is excluded from origin checks as its effective
  // requesting origin may be the Default Search Engine origin.
  return embedding_origin ==
             GURL(chrome::kChromeUINewTabURL).DeprecatedGetOriginAsURL() ||
         embedding_origin ==
             GURL(chrome::kChromeUINewTabPageURL).DeprecatedGetOriginAsURL();
}

std::optional<GURL> ChromePermissionsClient::OverrideCanonicalOrigin(
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  if (embedding_origin.DeprecatedGetOriginAsURL() ==
      GURL(chrome::kChromeUINewTabURL).DeprecatedGetOriginAsURL()) {
    if (requesting_origin.DeprecatedGetOriginAsURL() ==
        GURL(chrome::kChromeUINewTabPageURL).DeprecatedGetOriginAsURL()) {
      return GURL(UIThreadSearchTermsData().GoogleBaseURLValue())
          .DeprecatedGetOriginAsURL();
    }
    return requesting_origin;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Note that currently chrome extensions are allowed to use permissions even
  // when in embedded in non-secure contexts. This is unfortunate and we
  // should remove this at some point, but for now always use the requesting
  // origin for embedded extensions. https://crbug.com/530507.
  if (requesting_origin.SchemeIs(extensions::kExtensionScheme)) {
    return requesting_origin;
  }
#endif

  return std::nullopt;
}

bool ChromePermissionsClient::DoURLsMatchNewTabPage(
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  return embedding_origin ==
             GURL(chrome::kChromeUINewTabURL).DeprecatedGetOriginAsURL() &&
         requesting_origin ==
             GURL(chrome::kChromeUINewTabPageURL).DeprecatedGetOriginAsURL();
}

#if BUILDFLAG(IS_ANDROID)
bool ChromePermissionsClient::IsDseOrigin(
    content::BrowserContext* browser_context,
    const url::Origin& origin) {
  SearchPermissionsService* search_helper =
      SearchPermissionsService::Factory::GetForBrowserContext(browser_context);
  return search_helper && search_helper->IsDseOrigin(origin);
}

infobars::InfoBarManager* ChromePermissionsClient::GetInfoBarManager(
    content::WebContents* web_contents) {
  return infobars::ContentInfoBarManager::FromWebContents(web_contents);
}

infobars::InfoBar* ChromePermissionsClient::MaybeCreateInfoBar(
    content::WebContents* web_contents,
    ContentSettingsType type,
    base::WeakPtr<permissions::PermissionPromptAndroid> prompt) {
  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents);
  if (infobar_manager && ShouldUseQuietUI(web_contents, type)) {
    return PermissionInfoBarDelegate::Create(std::move(prompt),
                                             infobar_manager);
  }
  return nullptr;
}

std::unique_ptr<ChromePermissionsClient::PermissionMessageDelegate>
ChromePermissionsClient::MaybeCreateMessageUI(
    content::WebContents* web_contents,
    ContentSettingsType type,
    base::WeakPtr<permissions::PermissionPromptAndroid> prompt) {
  if (ShouldUseQuietUI(web_contents, type)) {
    auto delegate =
        std::make_unique<PermissionBlockedMessageDelegate::Delegate>(
            std::move(prompt));
    return std::make_unique<PermissionBlockedMessageDelegate>(
        web_contents, std::move(delegate));
  }

  return {};
}

void ChromePermissionsClient::RepromptForAndroidPermissions(
    content::WebContents* web_contents,
    const std::vector<ContentSettingsType>& content_settings_types,
    const std::vector<ContentSettingsType>& filtered_content_settings_types,
    const std::vector<std::string>& required_permissions,
    const std::vector<std::string>& optional_permissions,
    PermissionsUpdatedCallback callback) {
    PermissionUpdateMessageController::CreateForWebContents(web_contents);
    PermissionUpdateMessageController::FromWebContents(web_contents)
        ->ShowMessage(content_settings_types, filtered_content_settings_types,
                      required_permissions, optional_permissions,
                      std::move(callback));
}

int ChromePermissionsClient::MapToJavaDrawableId(int resource_id) {
  return ResourceMapper::MapToJavaDrawableId(resource_id);
}

favicon::FaviconService* ChromePermissionsClient::GetFaviconService(
    content::BrowserContext* browser_context) {
  return FaviconServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context),
      ServiceAccessType::EXPLICIT_ACCESS);
}

#else
std::unique_ptr<permissions::PermissionPrompt>
ChromePermissionsClient::CreatePrompt(
    content::WebContents* web_contents,
    permissions::PermissionPrompt::Delegate* delegate) {
  return CreatePermissionPrompt(web_contents, delegate);
}
#endif

bool ChromePermissionsClient::HasDevicePermission(
    ContentSettingsType type) const {
#if BUILDFLAG(IS_MAC)
  return system_permission_settings::IsAllowed(type);
#else
  return PermissionsClient::HasDevicePermission(type);
#endif
}

bool ChromePermissionsClient::CanRequestDevicePermission(
    ContentSettingsType type) const {
#if BUILDFLAG(IS_MAC)
  return system_permission_settings::CanPrompt(type);
#else
  return PermissionsClient::CanRequestDevicePermission(type);
#endif
}
