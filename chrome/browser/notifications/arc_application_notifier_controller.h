// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_ARC_APPLICATION_NOTIFIER_CONTROLLER_H_
#define CHROME_BROWSER_NOTIFICATIONS_ARC_APPLICATION_NOTIFIER_CONTROLLER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/notifications/notifier_controller.h"
#include "chrome/browser/ui/app_list/arc/arc_app_icon.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"

class Profile;

namespace arc {

// TODO(hirono): Observe enabled flag change and notify it to message center.
class ArcApplicationNotifierController : public NotifierController,
                                         public ArcAppIcon::Observer,
                                         public ArcAppListPrefs::Observer {
 public:
  explicit ArcApplicationNotifierController(
      NotifierController::Observer* observer);

  ~ArcApplicationNotifierController() override;

  // TODO(hirono): Rewrite the function with new API to fetch package list.
  std::vector<ash::NotifierMetadata> GetNotifierList(Profile* profile) override;
  void SetNotifierEnabled(Profile* profile,
                          const message_center::NotifierId& notifier_id,
                          bool enabled) override;

 private:
  // Overridden from ArcAppIcon::Observer.
  void OnIconUpdated(ArcAppIcon* icon) override;

  // Overriden from ArcAppListPrefs::Observer.
  void OnNotificationsEnabledChanged(const std::string& package_name,
                                     bool enabled) override;

  void StartObserving();
  void StopObserving();

  NotifierController::Observer* observer_;
  std::vector<std::unique_ptr<ArcAppIcon>> icons_;
  std::map<std::string, std::string> package_to_app_ids_;
  Profile* last_profile_;
  std::unique_ptr<KeyedServiceShutdownNotifier::Subscription>
      shutdown_notifier_;

  DISALLOW_COPY_AND_ASSIGN(ArcApplicationNotifierController);
};

}  // namespace arc

#endif  // CHROME_BROWSER_NOTIFICATIONS_ARC_APPLICATION_NOTIFIER_CONTROLLER_H_
