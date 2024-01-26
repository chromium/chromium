// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/cleanup_manager.h"

#include <utility>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

CleanupManager::CleanupManager() = default;

CleanupManager::~CleanupManager() = default;

void CleanupManager::Cleanup(CleanupCallback callback) {
  if (is_cleanup_in_progress_) {
    std::move(callback).Run("Cleanup is already in progress");
    return;
  }

  if (cleanup_handlers_.empty())
    InitializeCleanupHandlers();

  callback_ = std::move(callback);
  errors_.clear();
  is_cleanup_in_progress_ = true;

  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      cleanup_handlers_.size(),
      base::BindOnce(&CleanupManager::OnAllCleanupHandlersDone,
                     weak_factory_.GetWeakPtr()));

  start_time_ = base::Time::Now();
  for (auto& kv : cleanup_handlers_) {
    kv.second->Cleanup(base::BindOnce(&CleanupManager::OnCleanupHandlerDone,
                                      weak_factory_.GetWeakPtr(),
                                      barrier_closure, kv.first));
  }
}

void CleanupManager::SetCleanupHandlersForTesting(
    std::map<std::string, std::unique_ptr<CleanupHandler>> cleanup_handlers) {
  cleanup_handlers_.swap(cleanup_handlers);
}

void CleanupManager::ResetCleanupHandlersForTesting() {
  cleanup_handlers_.clear();
  is_cleanup_in_progress_ = false;
  InitializeCleanupHandlers();
}

void CleanupManager::SetIsCleanupInProgressForTesting(
    bool is_cleanup_in_progress) {
  is_cleanup_in_progress_ = is_cleanup_in_progress;
}

void CleanupManager::OnCleanupHandlerDone(
    base::RepeatingClosure barrier_closure,
    const std::string& handler_name,
    const std::optional<std::string>& error) {
  if (error) {
    errors_.push_back(handler_name + ": " + *error);
  }

  RecordHandlerMetrics(handler_name, start_time_, !error);

  std::move(barrier_closure).Run();
}

void CleanupManager::OnAllCleanupHandlersDone() {
  is_cleanup_in_progress_ = false;

  if (errors_.empty()) {
    std::move(callback_).Run(std::nullopt);
    return;
  }

  std::string errors = base::JoinString(errors_, "\n");
  std::move(callback_).Run(errors);
}

void CleanupManager::RecordHandlerMetrics(const std::string& handler_name,
                                          const base::Time& start_time,
                                          bool success) {
  base::TimeDelta delta = base::Time::Now() - start_time;

  std::string histogram_prefix =
      "Enterprise.LoginApiCleanup." + handler_name + ".";

  base::UmaHistogramTimes(histogram_prefix + "Timing", delta);
  base::UmaHistogramBoolean(histogram_prefix + "Success", success);
}

}  // namespace chromeos
