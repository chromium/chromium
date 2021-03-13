// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/full_restore/full_restore_arc_task_handler.h"

#include "chrome/browser/chromeos/full_restore/full_restore_arc_task_handler_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/full_restore/full_restore_utils.h"

namespace chromeos {
namespace full_restore {

// static
FullRestoreArcTaskHandler* FullRestoreArcTaskHandler::GetForProfile(
    Profile* profile) {
  return FullRestoreArcTaskHandlerFactory::GetForProfile(profile);
}

FullRestoreArcTaskHandler::FullRestoreArcTaskHandler(Profile* profile) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile);
  if (!prefs)
    return;

  arc_prefs_observer_.Observe(prefs);
}

FullRestoreArcTaskHandler::~FullRestoreArcTaskHandler() = default;

void FullRestoreArcTaskHandler::OnTaskCreated(int32_t task_id,
                                              const std::string& package_name,
                                              const std::string& activity,
                                              const std::string& intent,
                                              int32_t session_id) {
  const std::string app_id = ArcAppListPrefs::GetAppId(package_name, activity);
  ::full_restore::OnTaskCreated(app_id, task_id, session_id);
}

void FullRestoreArcTaskHandler::OnTaskDestroyed(int32_t task_id) {
  ::full_restore::OnTaskDestroyed(task_id);
}

}  // namespace full_restore
}  // namespace chromeos
