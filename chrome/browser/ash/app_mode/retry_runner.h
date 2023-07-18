// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_RETRY_RUNNER_H_
#define CHROME_BROWSER_ASH_APP_MODE_RETRY_RUNNER_H_

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_mode/cancellable_job.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace internal {

// Computes the delay for `attempt_count` with exponential backoff.
base::TimeDelta DelayForAttempt(int attempt_count);

// Runs `task` with the given `delay` in the current sequence.
void PostDelayedTask(base::OnceClosure task, base::TimeDelta delay);

}  // namespace internal

// Helper to retry tasks that can fail.
template <typename Result>
class RetryRunner : public CancellableJob {
 public:
  using ResultCallback =
      base::OnceCallback<void(absl::optional<Result> result)>;
  using Job = base::RepeatingCallback<void(ResultCallback on_result)>;

  [[nodiscard]] static std::unique_ptr<CancellableJob>
  Run(int max_attempts, Job job, ResultCallback on_done) {
    auto handle = base::WrapUnique(
        new RetryRunner<Result>(max_attempts, job, std::move(on_done)));
    handle->RunAndRetryOnFailure();
    return handle;
  }

  RetryRunner(const RetryRunner&) = delete;
  RetryRunner& operator=(const RetryRunner&) = delete;
  ~RetryRunner() override = default;

 private:
  RetryRunner(int max_attempts, Job job, ResultCallback on_done)
      : max_attempts_(max_attempts), job_(job), on_done_(std::move(on_done)) {}

  void RunAndRetryOnFailure(int attempt_count = 1) {
    job_.Run(base::BindOnce(
        [](base::WeakPtr<RetryRunner> self, int attempt_count,
           absl::optional<Result> result) {
          if (!self) {
            return;
          }
          if (result.has_value() || attempt_count >= self->max_attempts_) {
            return std::move(self->on_done_).Run(std::move(result));
          }

          internal::PostDelayedTask(
              base::BindOnce(&RetryRunner::RunAndRetryOnFailure, self,
                             attempt_count + 1),
              internal::DelayForAttempt(attempt_count));
        },
        weak_ptr_factory_.GetWeakPtr(), attempt_count));
  }

  int max_attempts_;
  Job job_;
  ResultCallback on_done_;
  base::WeakPtrFactory<RetryRunner> weak_ptr_factory_{this};
};

// Runs the given `job` up to `n` times.
//
// `on_done` will be called if `job` returns a value, or if `job` returns no
// value after the `n` attempts.
//
// Attempts at `job` are delayed with exponential backoff after failure.
//
// Destroying the returned `std::unique_ptr` cancels this task. In that case
// `on_done` will not be called.
template <typename Result>
[[nodiscard]] std::unique_ptr<CancellableJob> RunUpToNTimes(
    int n,
    typename RetryRunner<Result>::Job job,
    typename RetryRunner<Result>::ResultCallback on_done) {
  return RetryRunner<Result>::Run(/*max_attempts=*/n, job, std::move(on_done));
}

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_RETRY_RUNNER_H_
