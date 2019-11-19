// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/app_install_event_log.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/files/file.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/policy/single_app_install_event_log.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

namespace {
constexpr int64_t kLogFileVersion = 3;
constexpr ssize_t kMaxLogs = 1024;
}  // namespace

AppInstallEventLog::AppInstallEventLog(const base::FilePath& file_name)
    : file_name_(file_name) {
  base::File file(file_name_, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid())
    return;

  int64_t version;
  if (!file.ReadAtCurrentPosAndCheck(
          base::as_writable_bytes(base::make_span(&version, 1)))) {
    LOG(WARNING) << "Corrupted app install log.";
    return;
  }

  if (version != kLogFileVersion) {
    LOG(WARNING) << "Log file version mismatch.";
    return;
  }

  ssize_t entries;
  if (!file.ReadAtCurrentPosAndCheck(
          base::as_writable_bytes(base::make_span(&entries, 1)))) {
    LOG(WARNING) << "Corrupted app install log.";
    return;
  }

  for (int i = 0; i < std::min(entries, kMaxLogs); ++i) {
    std::unique_ptr<SingleAppInstallEventLog> log;
    const bool file_ok = SingleAppInstallEventLog::Load(&file, &log);
    const bool log_ok = log && !log->package().empty() &&
                        logs_.find(log->package()) == logs_.end();
    if (!file_ok || !log_ok) {
      LOG(WARNING) << "Corrupted app install log.";
    }
    if (log_ok) {
      total_size_ += log->size();
      max_size_ = std::max(max_size_, log->size());
      logs_[log->package()] = std::move(log);
    }
    if (!file_ok) {
      return;
    }
  }

  if (entries >= kMaxLogs) {
    LOG(WARNING) << "Corrupted app install log.";
  }
}

AppInstallEventLog::~AppInstallEventLog() = default;

void AppInstallEventLog::Add(const std::string& package,
                             const em::AppInstallReportLogEvent& event) {
  if (logs_.size() == kMaxLogs && logs_.find(package) == logs_.end()) {
    LOG(WARNING) << "App install log overflow.";
    return;
  }

  auto& log = logs_[package];
  if (!log)
    log = std::make_unique<SingleAppInstallEventLog>(package);
  total_size_ -= log->size();
  log->Add(event);
  total_size_ += log->size();
  max_size_ = std::max(max_size_, log->size());
  dirty_ = true;
}

void AppInstallEventLog::Store() {
  if (!dirty_) {
    return;
  }

  base::File file(file_name_,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (!file.IsValid()) {
    LOG(WARNING) << "Unable to store app install log.";
    return;
  }

  if (!file.WriteAtCurrentPosAndCheck(
          base::as_bytes(base::make_span(&kLogFileVersion, 1)))) {
    LOG(WARNING) << "Unable to store app install log.";
    return;
  }

  ssize_t entries = logs_.size();
  if (!file.WriteAtCurrentPosAndCheck(
          base::as_bytes(base::make_span(&entries, 1)))) {
    LOG(WARNING) << "Unable to store app install log.";
    return;
  }

  for (const auto& log : logs_) {
    if (!log.second->Store(&file)) {
      LOG(WARNING) << "Unable to store app install log.";
      return;
    }
  }

  dirty_ = false;
}

void AppInstallEventLog::Serialize(em::AppInstallReportRequest* report) {
  report->Clear();
  for (const auto& log : logs_) {
    em::AppInstallReport* const report_log = report->add_app_install_reports();
    log.second->Serialize(report_log);
  }
}

void AppInstallEventLog::ClearSerialized() {
  int total_size = 0;
  max_size_ = 0;

  auto log = logs_.begin();
  while (log != logs_.end()) {
    log->second->ClearSerialized();
    if (log->second->empty()) {
      log = logs_.erase(log);
    } else {
      total_size += log->second->size();
      max_size_ = std::max(max_size_, log->second->size());
      ++log;
    }
  }

  if (total_size != total_size_) {
    total_size_ = total_size;
    dirty_ = true;
  }
}

}  // namespace policy
