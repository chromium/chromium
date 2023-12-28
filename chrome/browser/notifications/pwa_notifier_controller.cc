// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/pwa_notifier_controller.h"

#include "ash/public/cpp/notifier_metadata.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/notifications/notifier_dataset.h"
#include "chrome/browser/profiles/profile.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notifier_id.h"

PwaNotifierController::PwaNotifierController(
    NotifierController::Observer* observer)
    : observer_(observer) {}

PwaNotifierController::~PwaNotifierController() = default;

std::vector<ash::NotifierMetadata> PwaNotifierController::GetNotifierList(
    Profile* profile) {
  DCHECK(
      apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile));
  if (observed_profile_ && !observed_profile_->IsSameOrParent(profile))
    weak_ptr_factory_.InvalidateWeakPtrs();
  observed_profile_ = profile;
  auto* cache =
      &apps::AppServiceProxyFactory::GetForProfile(profile)->AppRegistryCache();
  if (!app_registry_cache_observer_.IsObservingSource(cache)) {
    app_registry_cache_observer_.Reset();
    app_registry_cache_observer_.Observe(cache);
  }
  package_to_app_ids_.clear();

  std::vector<NotifierDataset> notifier_dataset;
  cache->ForEachApp(
      [&notifier_dataset](const apps::AppUpdate& update) {
        if (update.AppType() != apps::AppType::kWeb)
          return;

        for (const auto& permission : update.Permissions()) {
          if (permission->permission_type !=
              apps::PermissionType::kNotifications) {
            continue;
          }
          DCHECK(absl::holds_alternative<apps::TriState>(permission->value));
          // Do not include notifier metadata for system apps.
          if (update.InstallReason() == apps::InstallReason::kSystem) {
            return;
          }
          notifier_dataset.push_back(NotifierDataset{
              update.AppId() /*app_id*/, update.Name() /*app_name*/,
              update.PublisherId() /*publisher_id*/,
              permission->IsPermissionEnabled()});
        }
      });
  std::vector<ash::NotifierMetadata> notifiers;

  for (auto& app_data : notifier_dataset) {
    // Handle packages having multiple launcher activities. Do not include
    // notifier metadata for system apps.
    if (package_to_app_ids_.count(app_data.publisher_id)) {
      continue;
    }
    message_center::NotifierId notifier_id(
        message_center::NotifierType::APPLICATION, app_data.app_id);
    notifiers.emplace_back(notifier_id, base::UTF8ToUTF16(app_data.app_name),
                           app_data.enabled, false /* enforced */,
                           gfx::ImageSkia());
    package_to_app_ids_.insert(
        std::make_pair(app_data.publisher_id, app_data.app_id));
  }
  if (!package_to_app_ids_.empty()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&PwaNotifierController::CallLoadIcons,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
  return notifiers;
}

void PwaNotifierController::SetNotifierEnabled(
    Profile* profile,
    const message_center::NotifierId& notifier_id,
    bool enabled) {
  DCHECK(
      apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile));
  // We should not set permissions for a profile we are not currently observing.
  DCHECK(observed_profile_->IsSameOrParent(profile));

  auto permission = std::make_unique<apps::Permission>(
      apps::PermissionType::kNotifications,
      enabled ? apps::TriState::kAllow : apps::TriState::kBlock,
      /*is_managed=*/false);
  apps::AppServiceProxy* service =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  service->SetPermission(notifier_id.id, std::move(permission));
}

void PwaNotifierController::CallLoadIcons() {
  for (const auto& it : package_to_app_ids_) {
    CallLoadIcon(it.second, /*allow_placeholder_icon*/ true);
  }
}

void PwaNotifierController::CallLoadIcon(const std::string& app_id,
                                         bool allow_placeholder_icon) {
  DCHECK(apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
      observed_profile_));

  apps::AppServiceProxyFactory::GetForProfile(observed_profile_)
      ->LoadIcon(app_id, apps::IconType::kStandard,
                 message_center::kQuickSettingIconSizeInDp,
                 allow_placeholder_icon,
                 base::BindOnce(&PwaNotifierController::OnLoadIcon,
                                weak_ptr_factory_.GetWeakPtr(), app_id));
}

void PwaNotifierController::OnLoadIcon(const std::string& app_id,
                                       apps::IconValuePtr icon_value) {
  if (!icon_value || icon_value->icon_type != apps::IconType::kStandard)
    return;

  SetIcon(app_id, icon_value->uncompressed);
  if (icon_value->is_placeholder_icon)
    CallLoadIcon(app_id, /*allow_placeholder_icon*/ false);
}

void PwaNotifierController::SetIcon(const std::string& app_id,
                                    gfx::ImageSkia image) {
  observer_->OnIconImageUpdated(
      message_center::NotifierId(message_center::NotifierType::APPLICATION,
                                 app_id),
      image);
}

void PwaNotifierController::OnAppUpdate(const apps::AppUpdate& update) {
  if (!base::Contains(package_to_app_ids_, update.PublisherId()))
    return;

  if (update.PermissionsChanged()) {
    for (const auto& permission : update.Permissions()) {
      if (permission->permission_type == apps::PermissionType::kNotifications) {
        message_center::NotifierId notifier_id(
            message_center::NotifierType::APPLICATION, update.AppId());
        observer_->OnNotifierEnabledChanged(notifier_id,
                                            permission->IsPermissionEnabled());
      }
    }
  }

  if (update.IconKeyChanged())
    CallLoadIcon(update.AppId(), /*allow_placeholder_icon*/ true);
}

void PwaNotifierController::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observer_.Reset();
}
