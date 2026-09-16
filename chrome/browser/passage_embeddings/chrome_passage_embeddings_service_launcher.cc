// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/passage_embeddings/chrome_passage_embeddings_service_launcher.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/process/process.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/common/process_type.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace passage_embeddings {

ChromePassageEmbeddingsServiceLauncher::
    ChromePassageEmbeddingsServiceLauncher() = default;

ChromePassageEmbeddingsServiceLauncher::
    ~ChromePassageEmbeddingsServiceLauncher() = default;

void ChromePassageEmbeddingsServiceLauncher::LaunchService(
    mojo::PendingReceiver<mojom::PassageEmbeddingsService> receiver) {
  // `service_state_` is only used for invariant checking.
  CHECK_EQ(service_state_, ServiceState::kIdle);

  service_state_ = ServiceState::kLaunching;

  content::ServiceProcessHost::Launch<mojom::PassageEmbeddingsService>(
      std::move(receiver),
      content::ServiceProcessHost::Options()
          .WithDisplayName("Passage Embeddings Service")
          .WithProcessCallback(base::BindOnce(
              &ChromePassageEmbeddingsServiceLauncher::OnServiceLaunched,
              weak_ptr_factory_.GetWeakPtr()))
          .Pass());
}

void ChromePassageEmbeddingsServiceLauncher::OnServiceDisconnected(
    bool is_idle) {
  weak_ptr_factory_.InvalidateWeakPtrs();
  cpu_logger_.StopLoggingAfterNextUpdate();
  service_state_ = ServiceState::kIdle;
}

bool ChromePassageEmbeddingsServiceLauncher::AllowedToLaunch() const {
  return true;
}

void ChromePassageEmbeddingsServiceLauncher::OnServiceLaunched(
    const base::Process& process) {
  // `OnServiceLaunched` is triggered by the same observable that
  // `PerformanceManager` uses to register new process hosts, which is necessary
  // before we can start the CPU histogram logger. As such, this has to be a
  // `PostTask` to ensure that `InitializeCpuLogger` is invoked after the
  // service process host is registered.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ChromePassageEmbeddingsServiceLauncher::InitializeCpuLogger,
          weak_ptr_factory_.GetWeakPtr()));
}

void ChromePassageEmbeddingsServiceLauncher::InitializeCpuLogger() {
  // `service_state_` is only used for invariant checking.
  CHECK_EQ(service_state_, ServiceState::kLaunching);

  content::BrowserChildProcessHostIterator iter(content::PROCESS_TYPE_UTILITY);
  while (!iter.Done()) {
    const content::ChildProcessData& data = iter.GetData();
    if (data.name == u"Passage Embeddings Service") {
      cpu_logger_.StartLogging(
          content::BrowserChildProcessHost::FromID(data.GetChildProcessId()),
          base::BindRepeating(
              &PassageEmbeddingsServiceController::EmbedderRunning,
              base::Unretained(&controller_)));
      service_state_ = ServiceState::kReady;
      return;
    }
    ++iter;
  }
}

}  // namespace passage_embeddings
