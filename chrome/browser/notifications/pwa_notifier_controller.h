// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_PWA_NOTIFIER_CONTROLLER_H_
#define CHROME_BROWSER_NOTIFICATIONS_PWA_NOTIFIER_CONTROLLER_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/notifications/notifier_controller.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/icon_types.h"

class AppUpdate;
class Profile;

class PwaNotifierController : public NotifierController,
                              public apps::AppRegistryCache::Observer {
 public:
  explicit PwaNotifierController(NotifierController::Observer* observer);

  PwaNotifierController(const PwaNotifierController&) = delete;
  PwaNotifierController& operator=(const PwaNotifierController&) = delete;
  ~PwaNotifierController() override;

  std::vector<ash::NotifierMetadata> GetNotifierList(Profile* profile) override;
  void SetNotifierEnabled(Profile* profile,
                          const message_center::NotifierId& notifier_id,
                          bool enabled) override;

 private:
  void CallLoadIcons();
  void CallLoadIcon(const std::string& app_id, bool allow_placeholder_icon);
  void OnLoadIcon(const std::string& app_id, apps::IconValuePtr icon_value);

  void SetIcon(const std::string& app_id, gfx::ImageSkia image);

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  // Needed to load icons for PWAs.
  raw_ptr<Profile, DanglingUntriaged> observed_profile_ = nullptr;
  const raw_ptr<NotifierController::Observer> observer_;

  // Used to keep track of all PWA start URLs to prevent creation of duplicate
  // notifier metadat.
  std::map<std::string, std::string> package_to_app_ids_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};

  base::WeakPtrFactory<PwaNotifierController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_PWA_NOTIFIER_CONTROLLER_H_
