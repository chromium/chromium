// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/perf_output.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/task/post_task.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace metrics {

PerfOutputCall::PerfOutputCall(chromeos::DebugDaemonClient* debug_daemon_client,
                               base::TimeDelta duration,
                               const std::vector<std::string>& perf_args,
                               DoneCallback callback)
    : debug_daemon_client_(debug_daemon_client),
      duration_(duration),
      perf_args_(perf_args),
      done_callback_(std::move(callback)),
      pending_stop_(false) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  perf_data_pipe_reader_ =
      std::make_unique<chromeos::PipeReader>(base::CreateTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}));

  base::ScopedFD pipe_write_end =
      perf_data_pipe_reader_->StartIO(base::BindOnce(
          &PerfOutputCall::OnIOComplete, weak_factory_.GetWeakPtr()));
  DCHECK(debug_daemon_client_);
  debug_daemon_client_->GetPerfOutput(
      duration_, perf_args_, pipe_write_end.get(),
      base::BindOnce(&PerfOutputCall::OnGetPerfOutput,
                     weak_factory_.GetWeakPtr()));
}

PerfOutputCall::PerfOutputCall()
    : debug_daemon_client_(nullptr),
      pending_stop_(false),
      weak_factory_(this) {}

PerfOutputCall::~PerfOutputCall() {}

void PerfOutputCall::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!perf_session_id_) {
    // GetPerfOutputFd hasn't returned the session ID yet. Mark that Stop() has
    // been called. StopImpl() will be delayed until we receive the session ID.
    pending_stop_ = true;
    return;
  }

  StopImpl();
}

void PerfOutputCall::OnIOComplete(base::Optional<std::string> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  perf_data_pipe_reader_.reset();
  // Use the r-value variant of base::Optional::value_or() to move |result| to
  // the callback argument. Callback can safely use |result| after |this| is
  // deleted.
  std::move(done_callback_).Run(std::move(result).value_or(std::string()));
  // The callback may delete us, so it's hammertime: Can't touch |this|.
}

void PerfOutputCall::OnGetPerfOutput(base::Optional<uint64_t> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Signal pipe reader to shut down.
  if (!result.has_value() && perf_data_pipe_reader_.get()) {
    perf_data_pipe_reader_.reset();
    std::move(done_callback_).Run(std::string());
  }

  // DBus method GetPerfOutputFd returns a generated session ID back to the
  // caller. The session ID will be used in stopping the existing perf session.
  perf_session_id_ = result;
  if (pending_stop_) {
    // Stop() is called before GetPerfOutputFd returns the session ID. We can
    // invoke the StopPerf DBus method now.
    StopImpl();
  }
}

void PerfOutputCall::StopImpl() {
  DCHECK(perf_session_id_);
  debug_daemon_client_->StopPerf(*perf_session_id_, base::DoNothing());
}

}  // namespace metrics
