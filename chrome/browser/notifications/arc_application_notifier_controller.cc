// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/arc_application_notifier_controller.h"

#include <set>

#include "ash/public/cpp/notifier_metadata.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_service_proxy_chromeos.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/ui/webui/app_management/app_management.mojom.h"
#include "chrome/common/chrome_features.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "ui/base/layout.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace arc {

namespace {

constexpr int kArcAppIconSizeInDp = 48;

struct NotifierDataset {
  std::string app_id;
  std::string app_name;
  std::string package_name;
  bool enabled;
  bool is_system_app;
};

}  // namespace

ArcApplicationNotifierController::ArcApplicationNotifierController(
    NotifierController::Observer* observer)
    : observer_(observer) {}

ArcApplicationNotifierController::~ArcApplicationNotifierController() = default;

std::vector<ash::NotifierMetadata>
ArcApplicationNotifierController::GetNotifierList(Profile* profile) {
  DCHECK(
      apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile));

  // In Guest mode, it can be called but there's no ARC apps to return.
  if (profile->IsOffTheRecord())
    return std::vector<ash::NotifierMetadata>();

  last_used_profile_ = profile;
  apps::AppServiceProxy* service =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  Observe(&(service->AppRegistryCache()));

  package_to_app_ids_.clear();
  std::vector<NotifierDataset> notifier_dataset;

  service->AppRegistryCache().ForEachApp([&notifier_dataset](
                                             const apps::AppUpdate& update) {
    if (update.AppType() != apps::mojom::AppType::kArc)
      return;
    for (const auto& permission : update.Permissions()) {
      if (static_cast<app_management::mojom::ArcPermissionType>(
              permission->permission_id) !=
          app_management::mojom::ArcPermissionType::NOTIFICATIONS) {
        continue;
      }
      DCHECK(permission->value_type == apps::mojom::PermissionValueType::kBool);
      notifier_dataset.push_back(
          {update.AppId() /*app_id*/, update.Name() /*app_name*/,
           update.PublisherId() /*package name*/, permission->value /*enabled*/,
           update.InstallSource() ==
               apps::mojom::InstallSource::kSystem /*is_system_app*/});
    }
  });

  std::vector<ash::NotifierMetadata> notifiers;
  for (auto& app_data : notifier_dataset) {
    // Handle packages having multiple launcher activities. Do not include
    // notifier metadata for system apps.
    if (package_to_app_ids_.count(app_data.package_name) ||
        app_data.is_system_app)
      continue;

    message_center::NotifierId notifier_id(
        message_center::NotifierType::ARC_APPLICATION, app_data.app_id);
    notifiers.emplace_back(notifier_id, base::UTF8ToUTF16(app_data.app_name),
                           app_data.enabled, false /* enforced */,
                           gfx::ImageSkia());
    package_to_app_ids_.insert(
        std::make_pair(app_data.package_name, app_data.app_id));
    CallLoadIcon(/*allow_placeholder_icon*/ true, app_data.app_id);
  }
  return notifiers;
}

void ArcApplicationNotifierController::SetNotifierEnabled(
    Profile* profile,
    const message_center::NotifierId& notifier_id,
    bool enabled) {
  DCHECK(
      apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile));

  last_used_profile_ = profile;
  auto permission = apps::mojom::Permission::New();
  permission->permission_id =
      static_cast<int>(app_management::mojom::ArcPermissionType::NOTIFICATIONS);
  permission->value_type = apps::mojom::PermissionValueType::kBool;
  permission->value = enabled;
  permission->is_managed = false;
  apps::AppServiceProxy* service =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  service->SetPermission(notifier_id.id, std::move(permission));
}

void ArcApplicationNotifierController::CallLoadIcon(bool allow_placeholder_icon,
                                                    std::string app_id) {
  DCHECK(apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
      last_used_profile_));

  auto icon_type =
      (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon))
          ? apps::mojom::IconType::kStandard
          : apps::mojom::IconType::kUncompressed;

  apps::AppServiceProxyFactory::GetForProfile(last_used_profile_)
      ->LoadIcon(apps::mojom::AppType::kArc, app_id, icon_type,
                 kArcAppIconSizeInDp, allow_placeholder_icon,
                 base::BindOnce(&ArcApplicationNotifierController::OnLoadIcon,
                                weak_ptr_factory_.GetWeakPtr(), app_id));
}

void ArcApplicationNotifierController::OnLoadIcon(
    std::string app_id,
    apps::mojom::IconValuePtr icon_value) {
  auto icon_type =
      (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon))
          ? apps::mojom::IconType::kStandard
          : apps::mojom::IconType::kUncompressed;
  if (icon_value->icon_type != icon_type)
    return;

  SetIcon(app_id, icon_value->uncompressed);
  if (icon_value->is_placeholder_icon)
    CallLoadIcon(/*allow_placeholder_icon*/ false, app_id);
}

void ArcApplicationNotifierController::SetIcon(std::string app_id,
                                               gfx::ImageSkia image) {
  observer_->OnIconImageUpdated(
      message_center::NotifierId(message_center::NotifierType::ARC_APPLICATION,
                                 app_id),
      image);
}

void ArcApplicationNotifierController::OnAppUpdate(
    const apps::AppUpdate& update) {
  if (!base::Contains(package_to_app_ids_, update.PublisherId()))
    return;

  if (update.PermissionsChanged()) {
    for (const auto& permission : update.Permissions()) {
      if (static_cast<app_management::mojom::ArcPermissionType>(
              permission->permission_id) ==
          app_management::mojom::ArcPermissionType::NOTIFICATIONS) {
        message_center::NotifierId notifier_id(
            message_center::NotifierType::ARC_APPLICATION, update.AppId());
        observer_->OnNotifierEnabledChanged(notifier_id, permission->value);
      }
    }
  }

  if (update.IconKeyChanged()) {
    CallLoadIcon(/*allow_placeholder_icon*/ true, update.AppId());
  }
}

void ArcApplicationNotifierController::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  Observe(nullptr);
}

}  // namespace arc
