// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/telemetry_extension_api_browser_context_keyed_service_factories.h"

#include "chrome/browser/chromeos/extensions/telemetry/api/events/event_manager.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/routines/diagnostic_routine_manager.h"

namespace chromeos {

void EnsureBrowserContextKeyedServiceFactoriesBuilt() {
  chromeos::EventManager::GetFactoryInstance();
  chromeos::DiagnosticRoutineManager::GetFactoryInstance();
}

}  // namespace chromeos
