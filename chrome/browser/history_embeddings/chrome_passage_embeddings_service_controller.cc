// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history_embeddings/chrome_passage_embeddings_service_controller.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/process/process.h"
#include "components/history_embeddings/cpu_histogram_logger.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/common/process_type.h"

namespace history_embeddings {

// static
ChromePassageEmbeddingsServiceController*
ChromePassageEmbeddingsServiceController::Get() {
  static base::NoDestructor<ChromePassageEmbeddingsServiceController> instance;
  return instance.get();
}

ChromePassageEmbeddingsServiceController::
    ChromePassageEmbeddingsServiceController() = default;

ChromePassageEmbeddingsServiceController::
    ~ChromePassageEmbeddingsServiceController() = default;

void ChromePassageEmbeddingsServiceController::LaunchService() {
  // No-op if already launched.
  if (service_remote_) {
    return;
  }

  auto receiver = service_remote_.BindNewPipeAndPassReceiver();
  service_remote_.reset_on_disconnect();
  service_remote_.set_idle_handler(
      base::Minutes(1),
      base::BindRepeating(
          &ChromePassageEmbeddingsServiceController::ResetRemotes,
          base::Unretained(this)));
  content::ServiceProcessHost::Launch<
      passage_embeddings::mojom::PassageEmbeddingsService>(
      std::move(receiver),
      content::ServiceProcessHost::Options()
          .WithDisplayName("Passage Embeddings Service")
          .WithProcessCallback(base::BindOnce(
              &ChromePassageEmbeddingsServiceController::OnServiceLaunched,
              weak_ptr_factory_.GetWeakPtr()))
          .Pass());
}

void ChromePassageEmbeddingsServiceController::OnServiceLaunched(
    const base::Process& process) {
  // `OnServiceLaunched` is triggered by the same observable that
  // `PerformanceManager` uses to register new process hosts, which is necessary
  // before we can create the CPU histogram logger. As such, this has to be a
  // `PostTask` to ensure that `InitializeCpuLogger` is invoked after the
  // service process host is registered.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ChromePassageEmbeddingsServiceController::InitializeCpuLogger,
          weak_ptr_factory_.GetWeakPtr()));
}

void ChromePassageEmbeddingsServiceController::InitializeCpuLogger() {
  content::BrowserChildProcessHostIterator iter(content::PROCESS_TYPE_UTILITY);
  while (!iter.Done()) {
    const content::ChildProcessData& data = iter.GetData();
    if (data.name == u"Passage Embeddings Service") {
      cpu_logger_ = std::make_unique<CpuHistogramLogger>(
          content::BrowserChildProcessHost::FromID(data.id));
      break;
    }
    ++iter;
  }

  CHECK(cpu_logger_);
}

}  // namespace history_embeddings
