// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FULL_RESTORE_FULL_RESTORE_ARC_TASK_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_FULL_RESTORE_FULL_RESTORE_ARC_TASK_HANDLER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace chromeos {
namespace full_restore {

// The FullRestoreArcTaskHandler class observes ArcAppListPrefs, and calls
// FullRestoreSaveHandler to update the ARC app launch info when a task is
// created or destroyed.
//
// FullRestoreArcTaskHandler is an independent KeyedService so that it could be
// created along with ARC system rather than with FullRestoreService.
class FullRestoreArcTaskHandler : public KeyedService,
                                  public ArcAppListPrefs::Observer {
 public:
  static FullRestoreArcTaskHandler* GetForProfile(Profile* profile);

  explicit FullRestoreArcTaskHandler(Profile* profile);
  FullRestoreArcTaskHandler(const FullRestoreArcTaskHandler&) = delete;
  FullRestoreArcTaskHandler& operator=(const FullRestoreArcTaskHandler&) =
      delete;

  ~FullRestoreArcTaskHandler() override;

  // ArcAppListPrefs::Observer.
  void OnTaskCreated(int32_t task_id,
                     const std::string& package_name,
                     const std::string& activity,
                     const std::string& intent,
                     int32_t session_id) override;
  void OnTaskDestroyed(int task_id) override;

 private:
  base::ScopedObservation<ArcAppListPrefs, ArcAppListPrefs::Observer>
      arc_prefs_observer_{this};
};

}  // namespace full_restore
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FULL_RESTORE_FULL_RESTORE_ARC_TASK_HANDLER_H_
