// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_INTERNALS_PAGE_HANDLER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_INTERNALS_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_internals.mojom.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class OptimizationGuideKeyedService;

namespace contextual_tasks {
class ContextualTasksContextService;
}  // namespace contextual_tasks

class ContextualTasksInternalsPageHandler
    : public contextual_tasks_internals::mojom::
          ContextualTasksInternalsPageHandler,
      public OptimizationGuideLogger::Observer {
 public:
  ContextualTasksInternalsPageHandler(
      contextual_tasks::ContextualTasksContextService* context_service,
      OptimizationGuideKeyedService* optimization_guide_keyed_service,
      mojo::PendingReceiver<contextual_tasks_internals::mojom::
                                ContextualTasksInternalsPageHandler> receiver,
      mojo::PendingRemote<
          contextual_tasks_internals::mojom::ContextualTasksInternalsPage>
          page);
  ~ContextualTasksInternalsPageHandler() override;

  ContextualTasksInternalsPageHandler(
      const ContextualTasksInternalsPageHandler&) = delete;
  ContextualTasksInternalsPageHandler& operator=(
      const ContextualTasksInternalsPageHandler&) = delete;

  // contextual_tasks_internals::mojom::ContextualTasksInternalsPageHandler:
  void GetRelevantContext(
      contextual_tasks_internals::mojom::GetRelevantContextRequestPtr request,
      GetRelevantContextCallback callback) override;

  // OptimizationGuideLogger::Observer:
  void OnLogMessageAdded(base::Time event_time,
                         optimization_guide_common::mojom::LogSource log_source,
                         const std::string& source_file,
                         int source_line,
                         const std::string& message) override;

 private:
  raw_ptr<contextual_tasks::ContextualTasksContextService> context_service_;
  raw_ptr<OptimizationGuideLogger> optimization_guide_logger_;
  mojo::Receiver<
      contextual_tasks_internals::mojom::ContextualTasksInternalsPageHandler>
      receiver_;
  mojo::Remote<contextual_tasks_internals::mojom::ContextualTasksInternalsPage>
      page_;
};

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_INTERNALS_PAGE_HANDLER_H_
