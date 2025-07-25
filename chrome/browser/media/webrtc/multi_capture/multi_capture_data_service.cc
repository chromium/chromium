// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/multi_capture/multi_capture_data_service.h"

#include "base/barrier_closure.h"
#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/media/webrtc/capture_policy_utils.h"
#include "chrome/browser/profiles/profile.h"
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

void MultiCaptureDataService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);

  if (is_initialized_) {
    observer->MultiCaptureDataChanged();
  }
}

void MultiCaptureDataService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MultiCaptureDataService::Init() {
  auto initialized_components_barrier = base::BarrierClosure(
      2u, base::BindOnce(&MultiCaptureDataService::LoadData,
                         weak_ptr_factory_.GetWeakPtr()));
  provider_->on_registry_ready().Post(FROM_HERE,
                                      initialized_components_barrier);
  info_provider_->OnMaybeDownloadedComponentDataReady().Post(
      FROM_HERE, initialized_components_barrier);
}

void MultiCaptureDataService::LoadData() {
  // Fetch the initial value of the multi screen capture allowlist for later
  // matching to prevent dynamic refresh.
  multi_screen_capture_allowlist_on_login_ =
      prefs_->GetList(capture_policy::kManagedMultiScreenCaptureAllowedForUrls)
          .Clone();

  app_without_notification_bundle_ids_ =
      info_provider_->GetSkipMultiCaptureNotificationBundleIds();
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

    const bool can_skip_active_notification = base::Contains(
        app_without_notification_bundle_ids_, allowlisted_app_url.host());
    const std::string app_name = registrar.GetAppShortName(*app_id);
    if (can_skip_active_notification) {
      capture_apps_without_notification_[*app_id] = app_name;
    } else {
      capture_apps_with_notification_[*app_id] = app_name;
    }
  }

  observers_.Notify(&Observer::MultiCaptureDataChanged);
  is_initialized_ = true;
}

}  // namespace multi_capture
