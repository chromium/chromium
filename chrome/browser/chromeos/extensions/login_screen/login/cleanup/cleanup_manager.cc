// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/cleanup_manager.h"

#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/browsing_data_cleanup_handler.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/clipboard_cleanup_handler.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/extension_cleanup_handler.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/files_cleanup_handler.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/open_windows_cleanup_handler.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/print_jobs_cleanup_handler.h"

#include "base/logging.h"

namespace chromeos {

namespace {

// Must kept in sync with the CleanupHandler variant in
// tools/metrics/histograms/metadata/enterprise/histograms.xml
constexpr char kBrowsingDataCleanupHandlerHistogramName[] = "BrowsingData";
constexpr char kClipboardCleanupHandlerHistogramName[] = "Clipboard";
constexpr char kExtensionCleanupHandlerHistogramName[] = "Extension";
constexpr char kFilesCleanupHandlerHistogramName[] = "Files";
constexpr char kOpenWindowsCleanupHandlerHistogramName[] = "OpenWindows";
constexpr char kPrintJobsCleanupHandlerHistogramName[] = "PrintJobs";

void RecordHandlerMetrics(const std::string& handler_name,
                          const base::Time& start_time,
                          bool success) {
  base::TimeDelta delta = base::Time::Now() - start_time;

  std::string histogram_prefix =
      "Enterprise.LoginApiCleanup." + handler_name + ".";

  base::UmaHistogramTimes(histogram_prefix + "Timing", delta);
  base::UmaHistogramBoolean(histogram_prefix + "Success", success);
}

}  // namespace

// static
CleanupManager* CleanupManager::Get() {
  static base::NoDestructor<CleanupManager> instance;
  return instance.get();
}

CleanupManager::CleanupManager() {
  InitializeCleanupHandlers();
}

CleanupManager::~CleanupManager() = default;

void CleanupManager::Cleanup(CleanupCallback callback) {
  if (is_cleanup_in_progress_) {
    std::move(callback).Run("Cleanup is already in progress");
    return;
  }

  callback_ = std::move(callback);
  errors_.clear();
  is_cleanup_in_progress_ = true;

  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      cleanup_handlers_.size(),
      base::BindOnce(&CleanupManager::OnAllCleanupHandlersDone,
                     base::Unretained(this)));

  start_time_ = base::Time::Now();
  for (auto& kv : cleanup_handlers_) {
    kv.second->Cleanup(base::BindOnce(&CleanupManager::OnCleanupHandlerDone,
                                      base::Unretained(this), barrier_closure,
                                      kv.first));
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

void CleanupManager::InitializeCleanupHandlers() {
  cleanup_handlers_.insert({kBrowsingDataCleanupHandlerHistogramName,
                            std::make_unique<BrowsingDataCleanupHandler>()});
  cleanup_handlers_.insert({kOpenWindowsCleanupHandlerHistogramName,
                            std::make_unique<OpenWindowsCleanupHandler>()});
  cleanup_handlers_.insert({kFilesCleanupHandlerHistogramName,
                            std::make_unique<FilesCleanupHandler>()});
  cleanup_handlers_.insert({kClipboardCleanupHandlerHistogramName,
                            std::make_unique<ClipboardCleanupHandler>()});
  cleanup_handlers_.insert({kPrintJobsCleanupHandlerHistogramName,
                            std::make_unique<PrintJobsCleanupHandler>()});
  cleanup_handlers_.insert({kExtensionCleanupHandlerHistogramName,
                            std::make_unique<ExtensionCleanupHandler>()});
}

void CleanupManager::OnCleanupHandlerDone(
    base::RepeatingClosure barrier_closure,
    const std::string& handler_name,
    const absl::optional<std::string>& error) {
  if (error) {
    errors_.push_back(handler_name + ": " + *error);
  }

  RecordHandlerMetrics(handler_name, start_time_, !error);

  std::move(barrier_closure).Run();
}

void CleanupManager::OnAllCleanupHandlersDone() {
  is_cleanup_in_progress_ = false;

  if (errors_.empty()) {
    std::move(callback_).Run(absl::nullopt);
    return;
  }

  std::string errors = base::JoinString(errors_, "\n");
  std::move(callback_).Run(errors);
}

}  // namespace chromeos
