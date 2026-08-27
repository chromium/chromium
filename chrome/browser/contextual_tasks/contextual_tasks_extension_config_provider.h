// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_EXTENSION_CONFIG_PROVIDER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_EXTENSION_CONFIG_PROVIDER_H_

#include "extensions/browser/extension_config_map.h"

namespace content {
class BrowserContext;
}

namespace contextual_tasks {

// Provides configuration for the Contextual Tasks component extension.
class ContextualTasksExtensionConfigProvider
    : public extensions::ExtensionConfigProvider {
 public:
  static void Register(content::BrowserContext* browser_context);

  ContextualTasksExtensionConfigProvider();
  ~ContextualTasksExtensionConfigProvider() override;

  base::DictValue GetLoadTimeData(content::BrowserContext& context) override;
  bool IsJsErrorReportingEnabled() const override;
  bool ShouldCrashOnJsErrorInDevelopmentBuild() const override;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_EXTENSION_CONFIG_PROVIDER_H_
