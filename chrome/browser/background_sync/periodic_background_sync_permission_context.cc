// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_sync/periodic_background_sync_permission_context.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/webapps/installable/installable_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/shortcut_helper.h"
#else
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#endif

namespace features {

// If enabled, the installability criteria for granting PBS permission is
// dropped and the content setting is checked. This only applies if the
// requesting origin matches that of the browser's default search engine.
BASE_FEATURE(kPeriodicSyncPermissionForDefaultSearchEngine,
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features

PeriodicBackgroundSyncPermissionContext::
    PeriodicBackgroundSyncPermissionContext(
        content::BrowserContext* browser_context)
    : ContentSettingPermissionContextBase(
          browser_context,
          ContentSettingsType::PERIODIC_BACKGROUND_SYNC,
          network::mojom::PermissionsPolicyFeature::kNotFound) {
#if !BUILDFLAG(IS_ANDROID)
  auto* provider = web_app::WebAppProvider::GetForWebApps(
      Profile::FromBrowserContext(browser_context));
  if (provider) {
    install_manager_observation_.Observe(&provider->install_manager());
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

PeriodicBackgroundSyncPermissionContext::
    ~PeriodicBackgroundSyncPermissionContext() = default;

bool PeriodicBackgroundSyncPermissionContext::IsPwaInstalled(
    const GURL& origin) const {
  // Because we're only passed the requesting origin from the permissions
  // infrastructure, we can't match the scope of installed PWAs to the exact URL
  // of the permission request. We instead look for any installed PWA for the
  // requesting origin. With this logic, if there's already a PWA installed for
  // google.com/travel, and a request to register Periodic Background Sync comes
  // in from google.com/maps, this method will return true and registration will
  // succeed, provided other required conditions are met.
  return DoesOriginContainAnyInstalledWebApp(browser_context(), origin);
}

#if BUILDFLAG(IS_ANDROID)
bool PeriodicBackgroundSyncPermissionContext::IsTwaInstalled(
    const GURL& origin) const {
  return ShortcutHelper::DoesOriginContainAnyInstalledTrustedWebActivity(
      origin);
}
#endif

GURL PeriodicBackgroundSyncPermissionContext::GetDefaultSearchEngineUrl()
    const {
  auto* template_url_service = TemplateURLServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context()));
  DCHECK(template_url_service);

  const TemplateURL* default_search_engine =
      template_url_service->GetDefaultSearchProvider();
  return default_search_engine ? default_search_engine->GenerateSearchURL(
                                     template_url_service->search_terms_data())
                               : GURL();
}

ContentSetting
PeriodicBackgroundSyncPermissionContext::GetContentSettingStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

// TODO(crbug.com/397357113): PermissionStatus `change` event not triggered
// when TWA or PWA is installed or uninstalled on Android.
#if BUILDFLAG(IS_ANDROID)
  if (IsTwaInstalled(requesting_origin))
    return CONTENT_SETTING_ALLOW;
#endif

  bool can_bypass_install_requirement =
      base::FeatureList::IsEnabled(
          features::kPeriodicSyncPermissionForDefaultSearchEngine) &&
      url::IsSameOriginWith(GetDefaultSearchEngineUrl(), requesting_origin);

  if (!can_bypass_install_requirement && !IsPwaInstalled(requesting_origin)) {
    return CONTENT_SETTING_BLOCK;
  }

  // |requesting_origin| either has an installed PWA or matches the default
  // search engine's origin. Check one-shot Background Sync content setting.
  // Expected values are CONTENT_SETTING_BLOCK or CONTENT_SETTING_ALLOW.
  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser_context());
  DCHECK(host_content_settings_map);

  auto content_setting = host_content_settings_map->GetContentSetting(
      requesting_origin, embedding_origin,
      ContentSettingsType::BACKGROUND_SYNC);
  DCHECK(content_setting == CONTENT_SETTING_BLOCK ||
         content_setting == CONTENT_SETTING_ALLOW);
  return content_setting;
}

void PeriodicBackgroundSyncPermissionContext::DecidePermission(
    std::unique_ptr<permissions::PermissionRequestData> request_data,
    permissions::BrowserPermissionCallback callback) {
  // The user should never be prompted to authorize Periodic Background Sync
  // from PeriodicBackgroundSyncPermissionContext.
  NOTREACHED();
}

void PeriodicBackgroundSyncPermissionContext::NotifyPermissionSet(
    const permissions::PermissionRequestData& request_data,
    permissions::BrowserPermissionCallback callback,
    bool persist,
    PermissionDecision decision,
    bool is_final_decision) {
  DCHECK(!persist);
  DCHECK(is_final_decision);

  permissions::ContentSettingPermissionContextBase::NotifyPermissionSet(
      request_data, std::move(callback), persist, decision, is_final_decision);
}

void PeriodicBackgroundSyncPermissionContext::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  if (!content_type_set.Contains(ContentSettingsType::BACKGROUND_SYNC) &&
      !content_type_set.Contains(
          ContentSettingsType::PERIODIC_BACKGROUND_SYNC)) {
    return;
  }
  permissions::ContentSettingPermissionContextBase::OnContentSettingChanged(
      primary_pattern, secondary_pattern,
      ContentSettingsTypeSet(ContentSettingsType::PERIODIC_BACKGROUND_SYNC));
}

#if !BUILDFLAG(IS_ANDROID)
void PeriodicBackgroundSyncPermissionContext::OnWebAppInstalled(
    const webapps::AppId& app_id) {
  auto* provider = web_app::WebAppProvider::GetForWebApps(
      Profile::FromBrowserContext(browser_context()));
  if (provider) {
    const auto& registrar = provider->registrar_unsafe();
    // TODO(crbug.com/340952100): Evaluate call sites of IsInstallState for
    // correctness.
    if (registrar.GetInstallState(app_id) !=
        web_app::proto::InstallState::INSTALLED_WITH_OS_INTEGRATION) {
      return;
    }
    auto gurl = registrar.GetAppScope(app_id);
    if (!gurl.is_valid()) {
      return;
    }
    OnContentSettingChanged(
        ContentSettingsPattern::FromURL(gurl),
        ContentSettingsPattern::Wildcard(),
        ContentSettingsTypeSet(ContentSettingsType::PERIODIC_BACKGROUND_SYNC));
  }
}

void PeriodicBackgroundSyncPermissionContext::OnWebAppInstalledWithOsHooks(
    const webapps::AppId& app_id) {
  // TODO(crbug.com/340952100): Remove the method after the InstallState is
  // saved in the database & available from OnWebAppInstalled.
  OnWebAppInstalled(app_id);
}

void PeriodicBackgroundSyncPermissionContext::OnWebAppWillBeUninstalled(
    const webapps::AppId& app_id) {
  auto* provider = web_app::WebAppProvider::GetForWebApps(
      Profile::FromBrowserContext(browser_context()));
  if (provider) {
    const auto& registrar = provider->registrar_unsafe();
    auto gurl = registrar.GetAppScope(app_id);
    if (!gurl.is_valid()) {
      return;
    }
    app_id_origin_map_[app_id] = gurl;
  }
}

void PeriodicBackgroundSyncPermissionContext::OnWebAppUninstalled(
    const webapps::AppId& app_id,
    webapps::WebappUninstallSource uninstall_source) {
  if (app_id_origin_map_.find(app_id) != app_id_origin_map_.end()) {
    auto gurl = app_id_origin_map_[app_id];
    app_id_origin_map_.erase(app_id);
    OnContentSettingChanged(
        ContentSettingsPattern::FromURL(gurl),
        ContentSettingsPattern::Wildcard(),
        ContentSettingsTypeSet(ContentSettingsType::PERIODIC_BACKGROUND_SYNC));
  }
}

void PeriodicBackgroundSyncPermissionContext::
    OnWebAppInstallManagerDestroyed() {
  install_manager_observation_.Reset();
  app_id_origin_map_.clear();
}
#endif  // !BUILDFLAG(IS_ANDROID)
