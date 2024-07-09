// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_PERF_PERF_OUTPUT_H_
#define CHROME_BROWSER_METRICS_PERF_PERF_OUTPUT_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chromeos/dbus/common/dbus_callback.h"
#include "chromeos/dbus/common/pipe_reader.h"

namespace ash {
class DebugDaemonClient;
}

namespace metrics {

// Class for handling getting output from perf over DBus. Manages the
// asynchronous DBus call and retrieving data from quipper over a pipe.
class PerfOutputCall {
 public:
  // Called once GetPerfOutput() is complete, or an error occurred.
  // The callback may delete this object.
  // The argument is one of:
  // - Output from "perf record", in PerfDataProto format, OR
  // - The empty string if there was an error.
  // The output is transferred to |perf_stdout|.
  using DoneCallback = base::OnceCallback<void(std::string perf_stdout)>;

  PerfOutputCall(ash::DebugDaemonClient* debug_daemon_client,
                 const std::vector<std::string>& quipper_args,
                 bool disable_cpu_idle,
                 DoneCallback callback);

  PerfOutputCall(const PerfOutputCall&) = delete;
  PerfOutputCall& operator=(const PerfOutputCall&) = delete;

  virtual ~PerfOutputCall();

  // Stop() is made virtual for mocks in testing.
  virtual void Stop();

 protected:
  // Exposed for mocking in unit test.
  PerfOutputCall();

 private:
  // Internal callbacks.
  void OnIOComplete(std::optional<std::string> data);
  void OnGetPerfOutput(std::optional<uint64_t> result);

  void StopImpl();

  // A non-retaining pointer to the DebugDaemonClient instance.
  raw_ptr<ash::DebugDaemonClient> debug_daemon_client_;

  // Used to capture perf data written to a pipe.
  std::unique_ptr<chromeos::PipeReader> perf_data_pipe_reader_;

  // Saved arguments.
  std::vector<std::string> quipper_args_;
  bool disable_cpu_idle_;
  DoneCallback done_callback_;

  // Whether Stop() is called before OnGetPerfOutput() has returned the session
  // ID. If true (meaning Stop() is called very soon after we request perf
  // output), the stop request will be sent out after we have the session ID to
  // stop the perf session.
  bool pending_stop_;
  std::optional<uint64_t> perf_session_id_;

  SEQUENCE_CHECKER(sequence_checker_);

  // To pass around the "this" pointer across threads safely.
  base::WeakPtrFactory<PerfOutputCall> weak_factory_{this};
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_PERF_PERF_OUTPUT_H_
