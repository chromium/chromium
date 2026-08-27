// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_extension_config_provider.h"

#include <memory>

#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_config_map.h"
#include "extensions/browser/extension_config_map_factory.h"

namespace contextual_tasks {

// static
void ContextualTasksExtensionConfigProvider::Register(
    content::BrowserContext* browser_context) {
  extensions::ExtensionConfigMapFactory::GetOrCreateForBrowserContext(
      browser_context)
      ->RegisterConfigProvider(
          std::make_unique<ContextualTasksExtensionConfigProvider>());
}

ContextualTasksExtensionConfigProvider::ContextualTasksExtensionConfigProvider()
    : ExtensionConfigProvider(extension_misc::kContextualTasksExtensionId) {}

ContextualTasksExtensionConfigProvider::
    ~ContextualTasksExtensionConfigProvider() = default;

base::DictValue ContextualTasksExtensionConfigProvider::GetLoadTimeData(
    content::BrowserContext& context) {
  return ContextualTasksUI::GetContextualTasksLoadTimeData(
      Profile::FromBrowserContext(&context));
}

bool ContextualTasksExtensionConfigProvider::IsJsErrorReportingEnabled() const {
  return true;
}

bool ContextualTasksExtensionConfigProvider::
    ShouldCrashOnJsErrorInDevelopmentBuild() const {
  return true;
}

}  // namespace contextual_tasks
