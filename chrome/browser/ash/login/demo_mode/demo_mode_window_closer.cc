// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_mode_window_closer.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/instance_update.h"
#include "content/public/browser/browser_thread.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr char kAndroidGMSCorePackage[] = "com.google.android.gms";

std::string GetPackageNameFromAppId(const std::string& app_id) {
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(
      ProfileManager::GetPrimaryUserProfile());

  std::string publisher_id;
  proxy->AppRegistryCache().ForOneApp(app_id,
                                      [&](const apps::AppUpdate& update) {
                                        publisher_id = update.PublisherId();
                                      });
  return publisher_id;
}

void CloseAppWithInstanceId(const base::UnguessableToken& instance_id) {
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(
      ProfileManager::GetPrimaryUserProfile());
  proxy->InstanceRegistry().ForOneInstance(
      instance_id, [](const apps::InstanceUpdate& update) {
        auto* widget = views::Widget::GetWidgetForNativeWindow(update.Window());
        widget->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
      });
}

}  // namespace

DemoModeWindowCloser::DemoModeWindowCloser() {
  auto* profile = ProfileManager::GetPrimaryUserProfile();

  // Some test profiles will not have AppServiceProxy.
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    return;
  }

  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  CHECK(proxy);
  scoped_observation_.Observe(&proxy->InstanceRegistry());
}

DemoModeWindowCloser::~DemoModeWindowCloser() = default;

void DemoModeWindowCloser::OnInstanceUpdate(
    const apps::InstanceUpdate& update) {
  if (!gms_core_app_id_.empty()) {
    if (update.AppId() != gms_core_app_id_) {
      return;
    }
  } else if (GetPackageNameFromAppId(update.AppId()) !=
             kAndroidGMSCorePackage) {
    return;
  }

  gms_core_app_id_ = update.AppId();
  // Post the task to close only when the window is being created.
  if (update.IsCreation()) {
    base::UmaHistogramBoolean("DemoMode.GMSCoreDialogShown", true);
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&CloseAppWithInstanceId, update.InstanceId()));
  }
}

void DemoModeWindowCloser::OnInstanceRegistryWillBeDestroyed(
    apps::InstanceRegistry* cache) {
  if (scoped_observation_.GetSource() == cache) {
    scoped_observation_.Reset();
  }
}
