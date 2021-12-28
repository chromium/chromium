// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/chrome_permissions_client.h"

#include <vector>

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/bluetooth/bluetooth_chooser_context_factory.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/important_sites_util.h"
#include "chrome/browser/metrics/ukm_background_recorder_service.h"
#include "chrome/browser/permissions/abusive_origin_permission_revocation_request.h"
#include "chrome/browser/permissions/adaptive_quiet_notification_permission_ui_enabler.h"
#include "chrome/browser/permissions/contextual_notification_permission_ui_selector.h"
#include "chrome/browser/permissions/permission_actions_history_factory.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/permissions/prediction_based_permission_ui_selector.h"
#include "chrome/browser/permissions/pref_notification_permission_ui_selector.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context_factory.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/google/core/common/google_util.h"
#include "components/permissions/contexts/bluetooth_chooser_context.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/request_type.h"
#include "components/prefs/pref_service.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/subresource_filter/content/browser/subresource_filter_content_settings_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_profile_context.h"
#include "components/ukm/content/source_url_recorder.h"
#include "extensions/common/constants.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/origin.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/android/search_permissions/search_permissions_service.h"
#include "chrome/browser/permissions/grouped_permission_infobar_delegate_android.h"
#include "chrome/browser/permissions/notification_blocked_message_delegate_android.h"
#include "chrome/browser/permissions/permission_update_infobar_delegate_android.h"
#include "chrome/browser/permissions/permission_update_message_controller_android.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/messages/android/messages_feature.h"
#include "components/permissions/permission_request_manager.h"
#else
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/permission_bubble/permission_prompt.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_data.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#endif

namespace {

#if defined(OS_ANDROID)
bool ShouldUseQuietUI(content::WebContents* web_contents,
                      ContentSettingsType type) {
  auto* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents);
  return type == ContentSettingsType::NOTIFICATIONS &&
         manager->ShouldCurrentRequestUseQuietUI();
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
      NOTREACHED();
      return nullptr;
  }
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

permissions::PermissionManager* ChromePermissionsClient::GetPermissionManager(
    content::BrowserContext* browser_context) {
  return PermissionManagerFactory::GetForProfile(
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
    auto important_domain_search =
        [&registerable_domain](
            const site_engagement::ImportantSitesUtil::ImportantDomainInfo&
                item) {
          return item.registerable_domain == registerable_domain;
        };
    entry.second =
        std::find_if(important_domains.begin(), important_domains.end(),
                     important_domain_search) != important_domains.end();
  }
}

#if defined(OS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
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
#endif

void ChromePermissionsClient::GetUkmSourceId(
    content::BrowserContext* browser_context,
    const content::WebContents* web_contents,
    const GURL& requesting_origin,
    GetUkmSourceIdCallback callback) {
  if (web_contents) {
    ukm::SourceId source_id =
        ukm::GetSourceIdForWebContentsDocument(web_contents);
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(xhwang): fix this icon, see crbug.com/446263.
  if (request_type == permissions::RequestType::kProtectedMediaIdentifier)
    return kProductIcon;
#endif
  return PermissionsClient::GetOverrideIconId(request_type);
}

std::vector<std::unique_ptr<permissions::PermissionUiSelector>>
ChromePermissionsClient::CreatePermissionUiSelectors(
    content::BrowserContext* browser_context) {
  std::vector<std::unique_ptr<permissions::PermissionUiSelector>> selectors;
  selectors.emplace_back(
      std::make_unique<ContextualNotificationPermissionUiSelector>());
  selectors.emplace_back(std::make_unique<PrefNotificationPermissionUiSelector>(
      Profile::FromBrowserContext(browser_context)));
  selectors.emplace_back(std::make_unique<PredictionBasedPermissionUiSelector>(
      Profile::FromBrowserContext(browser_context)));
  return selectors;
}

void ChromePermissionsClient::OnPromptResolved(
    content::BrowserContext* browser_context,
    permissions::RequestType request_type,
    permissions::PermissionAction action,
    const GURL& origin,
    permissions::PermissionPromptDisposition prompt_disposition,
    absl::optional<QuietUiReason> quiet_ui_reason) {
  Profile* profile = Profile::FromBrowserContext(browser_context);

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
             QuietUiReason::kTriggeredDueToAbusiveContent)) {
      AbusiveOriginPermissionRevocationRequest::
          ExemptOriginFromFutureRevocations(profile, origin);
    }
  }
}

absl::optional<bool>
ChromePermissionsClient::HadThreeConsecutiveNotificationPermissionDenies(
    content::BrowserContext* browser_context) {
  if (!QuietNotificationPermissionUiConfig::IsAdaptiveActivationDryRunEnabled())
    return absl::nullopt;
  return Profile::FromBrowserContext(browser_context)
      ->GetPrefs()
      ->GetBoolean(prefs::kHadThreeConsecutiveNotificationPermissionDenies);
}

absl::optional<bool>
ChromePermissionsClient::HasPreviouslyAutoRevokedPermission(
    content::BrowserContext* browser_context,
    const GURL& origin,
    ContentSettingsType permission) {
  if (permission != ContentSettingsType::NOTIFICATIONS) {
    return absl::nullopt;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context);
  return AbusiveOriginPermissionRevocationRequest::
      HasPreviouslyRevokedPermission(profile, origin);
}

absl::optional<url::Origin> ChromePermissionsClient::GetAutoApprovalOrigin() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // In web kiosk mode, all permission requests are auto-approved for the origin
  // of the main app.
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
#endif
  return absl::nullopt;
}

bool ChromePermissionsClient::CanBypassEmbeddingOriginCheck(
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  // The New Tab Page is excluded from origin checks as its effective requesting
  // origin may be the Default Search Engine origin. Extensions are also
  // excluded as currently they can request permission from iframes when
  // embedded in non-secure contexts (https://crbug.com/530507).
  return embedding_origin ==
             GURL(chrome::kChromeUINewTabURL).DeprecatedGetOriginAsURL() ||
         embedding_origin ==
             GURL(chrome::kChromeUINewTabPageURL).DeprecatedGetOriginAsURL() ||
         requesting_origin.SchemeIs(extensions::kExtensionScheme);
}

absl::optional<GURL> ChromePermissionsClient::OverrideCanonicalOrigin(
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

  // Note that currently chrome extensions are allowed to use permissions even
  // when in embedded in non-secure contexts. This is unfortunate and we
  // should remove this at some point, but for now always use the requesting
  // origin for embedded extensions. https://crbug.com/530507.
  if (requesting_origin.SchemeIs(extensions::kExtensionScheme)) {
    return requesting_origin;
  }

  return absl::nullopt;
}

bool ChromePermissionsClient::DoOriginsMatchNewTabPage(
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  return embedding_origin ==
             GURL(chrome::kChromeUINewTabURL).DeprecatedGetOriginAsURL() &&
         requesting_origin ==
             GURL(chrome::kChromeUINewTabPageURL).DeprecatedGetOriginAsURL();
}

#if defined(OS_ANDROID)
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
    return GroupedPermissionInfoBarDelegate::Create(std::move(prompt),
                                                    infobar_manager);
  }
  return nullptr;
}

std::unique_ptr<ChromePermissionsClient::PermissionMessageDelegate>
ChromePermissionsClient::MaybeCreateMessageUI(
    content::WebContents* web_contents,
    ContentSettingsType type,
    base::WeakPtr<permissions::PermissionPromptAndroid> prompt) {
  if (messages::IsNotificationBlockedMessagesUiEnabled() &&
      ShouldUseQuietUI(web_contents, type)) {
    auto delegate =
        std::make_unique<NotificationBlockedMessageDelegate::Delegate>(
            std::move(prompt));
    return std::make_unique<NotificationBlockedMessageDelegate>(
        web_contents, std::move(delegate));
  }

  return {};
}

void ChromePermissionsClient::RepromptForAndroidPermissions(
    content::WebContents* web_contents,
    const std::vector<ContentSettingsType>& content_settings_types,
    PermissionsUpdatedCallback callback) {
  if (messages::IsPermissionUpdateMessagesUiEnabled()) {
    PermissionUpdateMessageController::CreateForWebContents(web_contents);
    PermissionUpdateMessageController::FromWebContents(web_contents)
        ->ShowMessage(content_settings_types, std::move(callback));
  } else {
    PermissionUpdateInfoBarDelegate::Create(
        web_contents, content_settings_types, std::move(callback));
  }
}

int ChromePermissionsClient::MapToJavaDrawableId(int resource_id) {
  return ResourceMapper::MapToJavaDrawableId(resource_id);
}
#else
std::unique_ptr<permissions::PermissionPrompt>
ChromePermissionsClient::CreatePrompt(
    content::WebContents* web_contents,
    permissions::PermissionPrompt::Delegate* delegate) {
  return CreatePermissionPrompt(web_contents, delegate);
}
#endif
