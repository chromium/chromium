// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/ash_tracing_handler.h"

#include <sys/mman.h>

#include <algorithm>

#include "ash/hud_display/ash_tracing_request.h"
#include "ash/shell.h"
#include "base/files/file.h"
#include "base/files/platform_file.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "services/tracing/public/cpp/perfetto/perfetto_config.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"

namespace ash {
namespace hud_display {
namespace {

std::unique_ptr<perfetto::TracingSession> (*testing_perfetto_session_creator)(
    void) = nullptr;

}  // anonymous namespace

AshTracingHandler::AshTracingHandler() {
  // Bind sequence checker.
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
}

AshTracingHandler::~AshTracingHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
}

void AshTracingHandler::Start(AshTracingRequest* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK(!request_);
  DCHECK(!tracing_session_);

  request_ = request;
  perfetto::TraceConfig perfetto_config = tracing::GetDefaultPerfettoConfig(
      base::trace_event::TraceConfig(),
      /*privacy_filtering_enabled=*/false,
      /*convert_to_legacy_json=*/false,
      perfetto::protos::gen::ChromeConfig::USER_INITIATED);

  perfetto_config.set_write_into_file(true);
  tracing_session_ = testing_perfetto_session_creator
                         ? testing_perfetto_session_creator()
                         : perfetto::Tracing::NewTrace();
  tracing_session_->Setup(perfetto_config, request->GetPlatformFile());
  auto runner = base::SequencedTaskRunner::GetCurrentDefault();
  base::WeakPtr<AshTracingHandler> weak_ptr = weak_factory_.GetWeakPtr();
  tracing_session_->SetOnStartCallback([runner, weak_ptr]() {
    runner->PostTask(
        FROM_HERE,
        base::BindOnce(&AshTracingHandler::OnTracingStarted, weak_ptr));
  });
  tracing_session_->SetOnStopCallback([runner, weak_ptr]() {
    runner->PostTask(
        FROM_HERE,
        base::BindOnce(&AshTracingHandler::OnTracingFinished, weak_ptr));
  });
  tracing_session_->Start();
}

void AshTracingHandler::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK(tracing_session_);
  tracing_session_->Stop();
}

bool AshTracingHandler::IsStarted() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  return static_cast<bool>(tracing_session_);
}

// static
void AshTracingHandler::SetPerfettoTracingSessionCreatorForTesting(
    std::unique_ptr<perfetto::TracingSession> (*creator)(void)) {
  DCHECK(!testing_perfetto_session_creator);
  testing_perfetto_session_creator = creator;
}

// static
void AshTracingHandler::ResetPerfettoTracingSessionCreatorForTesting() {
  DCHECK(testing_perfetto_session_creator);
  testing_perfetto_session_creator = nullptr;
}

void AshTracingHandler::OnTracingStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  request_->OnTracingStarted();
}

void AshTracingHandler::OnTracingFinished() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  tracing_session_.reset();
  request_->OnTracingFinished();
}

}  // namespace hud_display
}  // namespace ash
