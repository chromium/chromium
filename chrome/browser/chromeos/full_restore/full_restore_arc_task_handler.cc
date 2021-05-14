// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/full_restore/full_restore_arc_task_handler.h"

#include "chrome/browser/chromeos/full_restore/arc_window_utils.h"
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

#if BUILDFLAG(ENABLE_WAYLAND_SERVER)
  if (IsArcGhostWindowEnabled())
    window_handler_ = std::make_unique<ArcWindowHandler>();
#endif
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

void FullRestoreArcTaskHandler::OnTaskDescriptionChanged(
    int32_t task_id,
    const std::string& label,
    const arc::mojom::RawIconPngData& icon,
    uint32_t primary_color,
    uint32_t status_bar_color) {
  ::full_restore::OnTaskThemeColorUpdated(task_id, primary_color,
                                          status_bar_color);
}

void FullRestoreArcTaskHandler::OnAppConnectionReady() {
#if BUILDFLAG(ENABLE_WAYLAND_SERVER)
  if (window_handler_)
    window_handler_->OnAppInstanceConnected();
#endif
}

}  // namespace full_restore
}  // namespace chromeos
