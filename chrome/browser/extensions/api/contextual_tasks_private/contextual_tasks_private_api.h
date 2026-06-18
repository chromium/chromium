// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_CONTEXTUAL_TASKS_PRIVATE_CONTEXTUAL_TASKS_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_CONTEXTUAL_TASKS_PRIVATE_CONTEXTUAL_TASKS_PRIVATE_API_H_

#include "extensions/browser/extension_function.h"

namespace extensions {

class ContextualTasksPrivateGetStateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contextualTasksPrivate.getState",
                             CONTEXTUALTASKSPRIVATE_GETSTATE)

  ContextualTasksPrivateGetStateFunction();
  ContextualTasksPrivateGetStateFunction(
      const ContextualTasksPrivateGetStateFunction&) = delete;
  ContextualTasksPrivateGetStateFunction& operator=(
      const ContextualTasksPrivateGetStateFunction&) = delete;

 protected:
  ~ContextualTasksPrivateGetStateFunction() override;

  // Override from ExtensionFunction:
  ResponseAction Run() override;
};

class ContextualTasksPrivateLaunchPanelInNewTabFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contextualTasksPrivate.launchPanelInNewTab",
                             CONTEXTUALTASKSPRIVATE_LAUNCHPANELINNEWTAB)

  ContextualTasksPrivateLaunchPanelInNewTabFunction();
  ContextualTasksPrivateLaunchPanelInNewTabFunction(
      const ContextualTasksPrivateLaunchPanelInNewTabFunction&) = delete;
  ContextualTasksPrivateLaunchPanelInNewTabFunction& operator=(
      const ContextualTasksPrivateLaunchPanelInNewTabFunction&) = delete;

 protected:
  ~ContextualTasksPrivateLaunchPanelInNewTabFunction() override;

  // Override from ExtensionFunction:
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_CONTEXTUAL_TASKS_PRIVATE_CONTEXTUAL_TASKS_PRIVATE_API_H_
