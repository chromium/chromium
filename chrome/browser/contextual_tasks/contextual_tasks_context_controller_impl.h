// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_CONTROLLER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller.h"

namespace contextual_tasks {
class ContextualTasksService;

class ContextualTasksContextControllerImpl
    : public ContextualTasksContextController {
 public:
  explicit ContextualTasksContextControllerImpl(
      ContextualTasksService* service);
  ~ContextualTasksContextControllerImpl() override;

 private:
  raw_ptr<ContextualTasksService> service_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_CONTROLLER_IMPL_H_
