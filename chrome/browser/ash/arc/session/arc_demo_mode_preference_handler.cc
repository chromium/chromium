// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/session/arc_demo_mode_preference_handler.h"

#include <utility>

#include "ash/components/arc/arc_util.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace arc {

// static
std::unique_ptr<ArcDemoModePreferenceHandler>
ArcDemoModePreferenceHandler::Create(ArcSessionManager* arc_session_manager) {
  return base::WrapUnique(new ArcDemoModePreferenceHandler(
      base::BindOnce(&ArcSessionManager::StopMiniArcIfNecessary,
                     base::Unretained(arc_session_manager)),
      g_browser_process->local_state()));
}

ArcDemoModePreferenceHandler::ArcDemoModePreferenceHandler(
    base::OnceClosure preference_changed_callback,
    PrefService* pref_service)
    : preference_changed_callback_(std::move(preference_changed_callback)),
      pref_service_(pref_service) {
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kDemoModeConfig,
      base::BindRepeating(&ArcDemoModePreferenceHandler::OnPreferenceChanged,
                          base::Unretained(this)));
}

ArcDemoModePreferenceHandler::~ArcDemoModePreferenceHandler() = default;

void ArcDemoModePreferenceHandler::OnPreferenceChanged() {
  // On ARC++, the demo session apps image is directly mounted into the
  // container namespace at upgrade time, so this isn't needed.
  if (!IsArcVmEnabled())
    return;

  auto config = static_cast<ash::DemoSession::DemoModeConfig>(
      pref_service_->GetInteger(prefs::kDemoModeConfig));
  switch (config) {
    case ash::DemoSession::DemoModeConfig::kNone:
    case ash::DemoSession::DemoModeConfig::kOfflineDeprecated:
      return;
    case ash::DemoSession::DemoModeConfig::kOnline:
      break;
  }

  VLOG(1) << "Demo Mode enabled; requesting ARCVM stop";
  DCHECK(preference_changed_callback_);
  std::move(preference_changed_callback_).Run();

  pref_change_registrar_.RemoveAll();
}

}  // namespace arc
