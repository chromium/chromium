// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_INTERNALS_PAGE_HANDLER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_INTERNALS_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_internals.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace contextual_tasks {
class ContextualTasksContextService;
}  // namespace contextual_tasks

class ContextualTasksInternalsPageHandler
    : public contextual_tasks::mojom::ContextualTasksInternalsPageHandler {
 public:
  ContextualTasksInternalsPageHandler(
      contextual_tasks::ContextualTasksContextService* context_service,
      mojo::PendingReceiver<
          contextual_tasks::mojom::ContextualTasksInternalsPageHandler>
          receiver);
  ~ContextualTasksInternalsPageHandler() override;

  ContextualTasksInternalsPageHandler(
      const ContextualTasksInternalsPageHandler&) = delete;
  ContextualTasksInternalsPageHandler& operator=(
      const ContextualTasksInternalsPageHandler&) = delete;

  // contextual_tasks::mojom::ContextualTasksInternalsPageHandler:
  void GetRelevantContext(
      contextual_tasks::mojom::GetRelevantContextRequestPtr request,
      GetRelevantContextCallback callback) override;

 private:
  raw_ptr<contextual_tasks::ContextualTasksContextService> context_service_;
  mojo::Receiver<contextual_tasks::mojom::ContextualTasksInternalsPageHandler>
      receiver_;
};

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_INTERNALS_PAGE_HANDLER_H_
