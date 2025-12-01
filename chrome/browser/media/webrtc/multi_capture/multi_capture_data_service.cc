// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/multi_capture/multi_capture_data_service.h"

#include "base/barrier_closure.h"
#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/media/webrtc/capture_policy_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/isolated_web_app_install_command_helper.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "components/webapps/isolated_web_apps/iwa_key_distribution_info_provider.h"

namespace multi_capture {

MultiCaptureDataService::MultiCaptureDataService(
    web_app::WebAppProvider* provider,
    PrefService* prefs)
    : info_provider_(web_app::IwaKeyDistributionInfoProvider::GetInstance()),
      provider_(provider),
      prefs_(prefs) {
  CHECK(provider_);
  CHECK(prefs_);
}

MultiCaptureDataService::~MultiCaptureDataService() {
  observers_.Notify(&Observer::MultiCaptureDataServiceDestroyed);
}

std::unique_ptr<MultiCaptureDataService> MultiCaptureDataService::Create(
    web_app::WebAppProvider* provider,
    PrefService* prefs) {
  if (!provider || !prefs) {
    return nullptr;
  }
  auto service = base::WrapUnique(new MultiCaptureDataService(provider, prefs));
  service->Init();
  return service;
}

const std::map<webapps::AppId, std::string>&
MultiCaptureDataService::GetCaptureAppsWithNotification() const {
  return capture_apps_with_notification_;
}

const std::map<webapps::AppId, std::string>&
MultiCaptureDataService::GetCaptureAppsWithoutNotification() const {
  return capture_apps_without_notification_;
}

gfx::ImageSkia MultiCaptureDataService::GetAppIcon(
    const webapps::AppId& app_id) const {
  if (app_icons_.contains(app_id)) {
    return app_icons_.at(app_id);
  }
  return gfx::ImageSkia();
}

bool MultiCaptureDataService::IsMultiCaptureAllowed(const GURL& url) const {
  return std::ranges::any_of(
      multi_screen_capture_allowlist_on_login_,
      [url](const base::Value& value) {
        ContentSettingsPattern pattern =
            ContentSettingsPattern::FromString(value.GetString());
        // Despite |url| being a GURL, the path is ignored when matching.
        return pattern.IsValid() && pattern.Matches(url);
      });
}

bool MultiCaptureDataService::IsMultiCaptureAllowedForAnyApp() const {
  return multi_screen_capture_allowlist_on_login_.size();
}

void MultiCaptureDataService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);

  if (is_initialized_) {
    observer->MultiCaptureDataChanged();
  }
}

void MultiCaptureDataService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MultiCaptureDataService::OnWebAppInstalled(const webapps::AppId& app_id) {
  if (MaybeAddAppToCaptureAppLists(app_id)) {
    observers_.Notify(&Observer::MultiCaptureDataChanged);
  }
}

void MultiCaptureDataService::OnWebAppUninstalled(
    const webapps::AppId& app_id,
    webapps::WebappUninstallSource uninstall_source) {
  if (app_icons_.contains(app_id)) {
    app_icons_.erase(app_id);
  }

  if (capture_apps_with_notification_.contains(app_id)) {
    capture_apps_with_notification_.erase(app_id);
    observers_.Notify(&Observer::MultiCaptureDataChanged);
    return;
  }

  if (capture_apps_without_notification_.contains(app_id)) {
    capture_apps_without_notification_.erase(app_id);
    observers_.Notify(&Observer::MultiCaptureDataChanged);
    return;
  }
}

void MultiCaptureDataService::OnWebAppInstallManagerDestroyed() {
  install_manager_observation_.Reset();
}

void MultiCaptureDataService::Init() {
  auto initialized_components_barrier = base::BarrierClosure(
      2u, base::BindOnce(&MultiCaptureDataService::LoadData,
                         weak_ptr_factory_.GetWeakPtr()));
  provider_->on_registry_ready().Post(FROM_HERE,
                                      initialized_components_barrier);
  info_provider_->OnBestEffortRuntimeDataReady().Post(
      FROM_HERE, initialized_components_barrier);
}

void MultiCaptureDataService::LoadData() {
  // Fetch the initial value of the multi screen capture allowlist for later
  // matching to prevent dynamic refresh.
  multi_screen_capture_allowlist_on_login_ =
      prefs_->GetList(capture_policy::kManagedMultiScreenCaptureAllowedForUrls)
          .Clone();

  const std::vector<std::string> app_without_notification_bundle_ids_vector =
      info_provider_->GetSkipMultiCaptureNotificationBundleIds();
  app_without_notification_bundle_ids_ = {
      app_without_notification_bundle_ids_vector.begin(),
      app_without_notification_bundle_ids_vector.end()};

  web_app::WebAppRegistrar& registrar = provider_->registrar_unsafe();

  for (const base::Value& allowlisted_app_value :
       multi_screen_capture_allowlist_on_login_) {
    if (!allowlisted_app_value.is_string()) {
      continue;
    }

    const GURL allowlisted_app_url(allowlisted_app_value.GetString());
    const std::optional<webapps::AppId> app_id =
        registrar.FindBestAppWithUrlInScope(
            allowlisted_app_url, web_app::WebAppFilter::IsIsolatedApp());

    // App isn't installed yet.
    if (!app_id) {
      continue;
    }

    provider_->icon_manager().ReadFavicons(
        *app_id, web_app::IconPurpose::ANY,
        base::BindOnce(&MultiCaptureDataService::OnIconReceived,
                       weak_ptr_factory_.GetWeakPtr(), *app_id));

    const bool can_skip_active_notification = base::Contains(
        app_without_notification_bundle_ids_, allowlisted_app_url.GetHost());
    const std::string app_name = registrar.GetAppShortName(*app_id);
    if (can_skip_active_notification) {
      capture_apps_without_notification_[*app_id] = app_name;
    } else {
      capture_apps_with_notification_[*app_id] = app_name;
    }
  }

  observers_.Notify(&Observer::MultiCaptureDataChanged);

  // Listen for app installation changes after initial processing.
  if (!install_manager_observation_.IsObserving()) {
    install_manager_observation_.Observe(&provider_->install_manager());
  }
  is_initialized_ = true;
}

void MultiCaptureDataService::OnIconReceived(const webapps::AppId& app_id,
                                             gfx::ImageSkia icon) {
  app_icons_[app_id] = std::move(icon);
}

bool MultiCaptureDataService::MaybeAddAppToCaptureAppLists(
    const webapps::AppId& app_id) {
  const web_app::WebAppRegistrar& registrar = provider_->registrar_unsafe();
  ASSIGN_OR_RETURN(const web_app::WebApp& iwa,
                   web_app::GetIsolatedWebAppById(registrar, app_id),
                   [](const std::string& error) { return false; });
  ASSIGN_OR_RETURN(const web_app::IsolatedWebAppUrlInfo& url_info,
                   web_app::IsolatedWebAppUrlInfo::Create(iwa.manifest_id()),
                   [](const std::string& error) { return false; });
  const std::string& bundle_id = url_info.web_bundle_id().id();
  const bool capture_allowed_by_policy =
      std::ranges::any_of(multi_screen_capture_allowlist_on_login_,
                          [&bundle_id](const base::Value& value) {
                            if (!value.is_string()) {
                              return false;
                            }
                            const GURL url(value.GetString());
                            return url.is_valid() && url.GetHost() == bundle_id;
                          });
  if (!capture_allowed_by_policy) {
    return false;
  }

  provider_->icon_manager().ReadFavicons(
      app_id, web_app::IconPurpose::ANY,
      base::BindOnce(&MultiCaptureDataService::OnIconReceived,
                     weak_ptr_factory_.GetWeakPtr(), app_id));

  if (app_without_notification_bundle_ids_.contains(bundle_id)) {
    capture_apps_without_notification_[app_id] =
        registrar.GetAppShortName(app_id);
  } else {
    capture_apps_with_notification_[app_id] = registrar.GetAppShortName(app_id);
  }
  return true;
}

}  // namespace multi_capture
