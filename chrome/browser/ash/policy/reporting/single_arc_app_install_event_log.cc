// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/single_arc_app_install_event_log.h"

#include "base/containers/heap_array.h"
#include "base/files/file.h"

namespace em = enterprise_management;

namespace policy {

SingleArcAppInstallEventLog::SingleArcAppInstallEventLog(
    const std::string& package)
    : SingleInstallEventLog(package) {}

SingleArcAppInstallEventLog::~SingleArcAppInstallEventLog() {}

bool SingleArcAppInstallEventLog::Load(
    base::File* file,
    std::unique_ptr<SingleArcAppInstallEventLog>* log) {
  log->reset();

  ssize_t size;
  base::HeapArray<char> package_buffer;
  if (!ParseIdFromFile(file, &size, &package_buffer))
    return false;

  *log = std::make_unique<SingleArcAppInstallEventLog>(
      std::string(package_buffer.data(), size));

  return LoadEventLogFromFile(file, (*log).get());
}

void SingleArcAppInstallEventLog::Serialize(em::AppInstallReport* report) {
  report->Clear();
  report->set_package(id_);
  report->set_incomplete(incomplete_);
  for (const auto& event : events_) {
    em::AppInstallReportLogEvent* const log_event = report->add_logs();
    *log_event = event;
  }
  serialized_entries_ = events_.size();
}

}  // namespace policy
