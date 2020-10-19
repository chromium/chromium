// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/chrome_permissions_client.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/bluetooth/bluetooth_chooser_context.h"
#include "chrome/browser/bluetooth/bluetooth_chooser_context_factory.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/important_sites_util.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/metrics/ukm_background_recorder_service.h"
#include "chrome/browser/permissions/abusive_origin_permission_revocation_request.h"
#include "chrome/browser/permissions/adaptive_quiet_notification_permission_ui_enabler.h"
#include "chrome/browser/permissions/contextual_notification_permission_ui_selector.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/subresource_filter/subresource_filter_content_settings_manager.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context_factory.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/google/core/common/google_util.h"
#include "components/permissions/features.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/content/source_url_recorder.h"
#include "extensions/common/constants.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/origin.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/android/search_permissions/search_permissions_service.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/permissions/grouped_permission_infobar_delegate_android.h"
#include "chrome/browser/permissions/permission_update_infobar_delegate_android.h"
#else
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/permission_bubble/permission_prompt.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/app_mode/web_app/web_kiosk_app_data.h"
#include "chrome/browser/chromeos/app_mode/web_app/web_kiosk_app_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#endif

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

permissions::ChooserContextBase* ChromePermissionsClient::GetChooserContext(
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
  return SiteEngagementService::Get(
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
  std::vector<ImportantSitesUtil::ImportantDomainInfo> important_domains =
      ImportantSitesUtil::GetImportantRegisterableDomains(
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
            const ImportantSitesUtil::ImportantDomainInfo& item) {
          return item.registerable_domain == registerable_domain;
        };
    entry.second =
        std::find_if(important_domains.begin(), important_domains.end(),
                     important_domain_search) != important_domains.end();
  }
}

#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
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

permissions::PermissionRequest::IconId
ChromePermissionsClient::GetOverrideIconId(ContentSettingsType type) {
#if defined(OS_CHROMEOS)
  // TODO(xhwang): fix this icon, see crbug.com/446263.
  if (type == ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER)
    return kProductIcon;
#endif
  return PermissionsClient::GetOverrideIconId(type);
}

std::unique_ptr<permissions::NotificationPermissionUiSelector>
ChromePermissionsClient::CreateNotificationPermissionUiSelector(
    content::BrowserContext* browser_context) {
  return std::make_unique<ContextualNotificationPermissionUiSelector>(
      Profile::FromBrowserContext(browser_context));
}

void ChromePermissionsClient::OnPromptResolved(
    content::BrowserContext* browser_context,
    permissions::PermissionRequestType request_type,
    permissions::PermissionAction action,
    const GURL& origin,
    base::Optional<QuietUiReason> quiet_ui_reason) {
  if (request_type ==
      permissions::PermissionRequestType::PERMISSION_NOTIFICATIONS) {
    Profile* profile = Profile::FromBrowserContext(browser_context);

    AdaptiveQuietNotificationPermissionUiEnabler::GetForProfile(profile)
        ->RecordPermissionPromptOutcome(action);

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

base::Optional<bool>
ChromePermissionsClient::HadThreeConsecutiveNotificationPermissionDenies(
    content::BrowserContext* browser_context) {
  if (!QuietNotificationPermissionUiConfig::IsAdaptiveActivationDryRunEnabled())
    return base::nullopt;
  return Profile::FromBrowserContext(browser_context)
      ->GetPrefs()
      ->GetBoolean(prefs::kHadThreeConsecutiveNotificationPermissionDenies);
}

base::Optional<bool>
ChromePermissionsClient::HasPreviouslyAutoRevokedPermission(
    content::BrowserContext* browser_context,
    const GURL& origin,
    ContentSettingsType permission) {
  if (permission != ContentSettingsType::NOTIFICATIONS) {
    return base::nullopt;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context);
  return AbusiveOriginPermissionRevocationRequest::
      HasPreviouslyRevokedPermission(profile, origin);
}

base::Optional<url::Origin> ChromePermissionsClient::GetAutoApprovalOrigin() {
#if defined(OS_CHROMEOS)
  // In web kiosk mode, all permission requests are auto-approved for the origin
  // of the main app.
  if (user_manager::UserManager::IsInitialized() &&
      user_manager::UserManager::Get()->IsLoggedInAsWebKioskApp()) {
    const AccountId& account_id =
        user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId();
    DCHECK(chromeos::WebKioskAppManager::IsInitialized());
    const chromeos::WebKioskAppData* app_data =
        chromeos::WebKioskAppManager::Get()->GetAppByAccountId(account_id);
    DCHECK(app_data);
    return url::Origin::Create(app_data->install_url());
  }
#endif
  return base::nullopt;
}

bool ChromePermissionsClient::CanBypassEmbeddingOriginCheck(
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  // The New Tab Page is excluded from origin checks as its effective requesting
  // origin may be the Default Search Engine origin. Extensions are also
  // excluded as currently they can request permission from iframes when
  // embedded in non-secure contexts (https://crbug.com/530507).
  return embedding_origin == GURL(chrome::kChromeUINewTabURL).GetOrigin() ||
         requesting_origin.SchemeIs(extensions::kExtensionScheme);
}

base::Optional<GURL> ChromePermissionsClient::OverrideCanonicalOrigin(
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  if (embedding_origin.GetOrigin() ==
      GURL(chrome::kChromeUINewTabURL).GetOrigin()) {
    if (requesting_origin.GetOrigin() ==
            GURL(chrome::kChromeSearchLocalNtpUrl).GetOrigin() ||
        requesting_origin.GetOrigin() ==
            GURL(chrome::kChromeUINewTabPageURL).GetOrigin()) {
      return GURL(UIThreadSearchTermsData().GoogleBaseURLValue()).GetOrigin();
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

  return base::nullopt;
}

#if defined(OS_ANDROID)
bool ChromePermissionsClient::IsPermissionControlledByDse(
    content::BrowserContext* browser_context,
    ContentSettingsType type,
    const url::Origin& origin) {
  SearchPermissionsService* search_helper =
      SearchPermissionsService::Factory::GetForBrowserContext(browser_context);
  return search_helper &&
         search_helper->IsPermissionControlledByDSE(type, origin);
}

bool ChromePermissionsClient::ResetPermissionIfControlledByDse(
    content::BrowserContext* browser_context,
    ContentSettingsType type,
    const url::Origin& origin) {
  SearchPermissionsService* search_helper =
      SearchPermissionsService::Factory::GetForBrowserContext(browser_context);
  if (search_helper &&
      search_helper->IsPermissionControlledByDSE(type, origin)) {
    search_helper->ResetDSEPermission(type);
    return true;
  }
  return false;
}

infobars::InfoBarManager* ChromePermissionsClient::GetInfoBarManager(
    content::WebContents* web_contents) {
  return InfoBarService::FromWebContents(web_contents);
}

infobars::InfoBar* ChromePermissionsClient::MaybeCreateInfoBar(
    content::WebContents* web_contents,
    ContentSettingsType type,
    base::WeakPtr<permissions::PermissionPromptAndroid> prompt) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  if (infobar_service &&
      GroupedPermissionInfoBarDelegate::ShouldShowMiniInfobar(web_contents,
                                                              type)) {
    return GroupedPermissionInfoBarDelegate::Create(std::move(prompt),
                                                    infobar_service);
  }
  return nullptr;
}

void ChromePermissionsClient::RepromptForAndroidPermissions(
    content::WebContents* web_contents,
    const std::vector<ContentSettingsType>& content_settings_types,
    PermissionsUpdatedCallback callback) {
  PermissionUpdateInfoBarDelegate::Create(web_contents, content_settings_types,
                                          std::move(callback));
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
