// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/crash_upload_list/crash_upload_list_crashpad.h"

#include <stddef.h>

#include <utility>

#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/common/chrome_constants.h"
#include "components/crash/content/app/crashpad.h"

namespace {

UploadList::UploadInfo::State ReportUploadStateToUploadInfoState(
    crash_reporter::ReportUploadState state) {
  switch (state) {
    case crash_reporter::ReportUploadState::NotUploaded:
      return UploadList::UploadInfo::State::NotUploaded;

    case crash_reporter::ReportUploadState::Pending:
      return UploadList::UploadInfo::State::Pending;

    case crash_reporter::ReportUploadState::Pending_UserRequested:
      return UploadList::UploadInfo::State::Pending_UserRequested;

    case crash_reporter::ReportUploadState::Uploaded:
      return UploadList::UploadInfo::State::Uploaded;
  }

  NOTREACHED();
  return UploadList::UploadInfo::State::Uploaded;
}

}  // namespace

CrashUploadListCrashpad::CrashUploadListCrashpad() = default;

CrashUploadListCrashpad::~CrashUploadListCrashpad() = default;

std::vector<UploadList::UploadInfo> CrashUploadListCrashpad::LoadUploadList() {
  std::vector<crash_reporter::Report> reports;
  crash_reporter::GetReports(&reports);

  std::vector<UploadInfo> uploads;
  for (const crash_reporter::Report& report : reports) {
    uploads.push_back(
        UploadInfo(report.remote_id, base::Time::FromTimeT(report.upload_time),
                   report.local_id, base::Time::FromTimeT(report.capture_time),
                   ReportUploadStateToUploadInfoState(report.state)));
  }
  return uploads;
}

void CrashUploadListCrashpad::ClearUploadList(const base::Time& begin,
                                              const base::Time& end) {
  crash_reporter::ClearReportsBetween(begin, end);
}

void CrashUploadListCrashpad::RequestSingleUpload(const std::string& local_id) {
  crash_reporter::RequestSingleCrashUpload(local_id);
}
