// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_ARC_APPLICATION_NOTIFIER_CONTROLLER_H_
#define CHROME_BROWSER_NOTIFICATIONS_ARC_APPLICATION_NOTIFIER_CONTROLLER_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/notifications/notifier_controller.h"
#include "chrome/browser/ui/app_list/arc/arc_app_icon.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"

class AppUpdate;
class Profile;

namespace arc {

// TODO(hirono): Observe enabled flag change and notify it to message center.
class ArcApplicationNotifierController
    : public NotifierController,
      public apps::AppRegistryCache::Observer {
 public:
  explicit ArcApplicationNotifierController(
      NotifierController::Observer* observer);

  ArcApplicationNotifierController(const ArcApplicationNotifierController&) =
      delete;
  ArcApplicationNotifierController& operator=(
      const ArcApplicationNotifierController&) = delete;
  ~ArcApplicationNotifierController() override;

  std::vector<ash::NotifierMetadata> GetNotifierList(Profile* profile) override;
  void SetNotifierEnabled(Profile* profile,
                          const message_center::NotifierId& notifier_id,
                          bool enabled) override;

 private:
  void SetIcon(std::string app_id, gfx::ImageSkia image);
  void CallLoadIcon(bool allow_placeholder_icon, std::string app_id);
  void OnLoadIcon(std::string app_id, apps::mojom::IconValuePtr icon_value);

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  Profile* last_used_profile_ = nullptr;
  NotifierController::Observer* observer_;
  std::map<std::string, std::string> package_to_app_ids_;
  base::WeakPtrFactory<ArcApplicationNotifierController> weak_ptr_factory_{
      this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_NOTIFICATIONS_ARC_APPLICATION_NOTIFIER_CONTROLLER_H_
