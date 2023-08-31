// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/routines/diagnostic_routine_manager.h"

#include "base/no_destructor.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"

namespace chromeos {

// static
extensions::BrowserContextKeyedAPIFactory<DiagnosticRoutineManager>*
DiagnosticRoutineManager::GetFactoryInstance() {
  static base::NoDestructor<
      extensions::BrowserContextKeyedAPIFactory<DiagnosticRoutineManager>>
      instance;
  return instance.get();
}

// static
DiagnosticRoutineManager* DiagnosticRoutineManager::Get(
    content::BrowserContext* browser_context) {
  return extensions::BrowserContextKeyedAPIFactory<
      DiagnosticRoutineManager>::Get(browser_context);
}

DiagnosticRoutineManager::DiagnosticRoutineManager(
    content::BrowserContext* context)
    : browser_context_(context) {}

DiagnosticRoutineManager::~DiagnosticRoutineManager() = default;

}  // namespace chromeos
