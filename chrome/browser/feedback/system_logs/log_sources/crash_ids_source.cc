// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/system_logs/log_sources/crash_ids_source.h"

#include <string>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/crash_upload_list/crash_upload_list.h"
#include "components/feedback/feedback_report.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "content/public/browser/browser_thread.h"
#endif

namespace system_logs {

namespace {

// The maximum number of crashes we retrieve from the crash list.
constexpr size_t kMaxCrashesCountToRetrieve = 10;

// The length of the crash ID string.
constexpr size_t kCrashIdStringSize = 16;

// For recent crashes, which is for all reports, look back one hour.
constexpr base::TimeDelta kOneHourTimeDelta = base::Hours(1);

// For all crashes, which is for only @google.com reports, look back 120 days.
constexpr base::TimeDelta k120DaysTimeDelta = base::Days(120);

}  // namespace

CrashIdsSource::CrashIdsSource()
    : SystemLogsSource("CrashId"),
      crash_upload_list_(CreateCrashUploadList()),
      pending_crash_list_loading_(false) {}

CrashIdsSource::~CrashIdsSource() {}

void CrashIdsSource::Fetch(SysLogsSourceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Unretained since we own this callback.
  pending_requests_.emplace_back(
      base::BindOnce(&CrashIdsSource::RespondWithCrashIds,
                     base::Unretained(this), std::move(callback)));

  if (pending_crash_list_loading_)
    return;

  pending_crash_list_loading_ = true;

  base::OnceClosure list_available_cb = base::BindOnce(
      &CrashIdsSource::OnUploadListAvailable, weak_ptr_factory_.GetWeakPtr());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Upload recent crashes so that they're shown in the report.
  // Non-chromeOS systems upload crashes shortly after they happen. ChromeOS is
  // unique in that it has a separate process (crash_sender) that uploads
  // crashes periodically (by default every 5 minutes).
  ash::DebugDaemonClient* debugd_client = ash::DebugDaemonClient::Get();
  if (debugd_client) {
    debugd_client->UploadCrashes(base::BindOnce(
        [](base::OnceClosure load_crash_list_cb, bool success) {
          if (!success) {
            LOG(ERROR) << "crash_sender failed; proceeding anyway";
          }
          std::move(load_crash_list_cb).Run();
        },
        base::BindOnce(&UploadList::Load, crash_upload_list_,
                       std::move(list_available_cb))));

    // Don't call Load directly; instead let UploadCrashes invoke it via the
    // callback.
    return;
  }
  // Fall through to Load statement below.
  LOG(ERROR) << "Failed to create debugd_client. debugd may be down?";
#endif

  crash_upload_list_->Load(std::move(list_available_cb));
}

void CrashIdsSource::OnUploadListAvailable() {
  pending_crash_list_loading_ = false;

  // We generate two lists of crash IDs. One will be the crashes within the last
  // hour, which is included in all feedback reports. The other is all of the
  // crash IDs from the past 120 days, which is only included in feedback
  // reports sent from @google.com accounts.
  const std::vector<const UploadList::UploadInfo*> crashes =
      crash_upload_list_->GetUploads(kMaxCrashesCountToRetrieve);
  const base::Time now = base::Time::Now();
  crash_ids_list_.clear();
  crash_ids_list_.reserve(kMaxCrashesCountToRetrieve *
                          (kCrashIdStringSize + 2));
  all_crash_ids_list_.clear();
  all_crash_ids_list_.reserve(kMaxCrashesCountToRetrieve *
                              (kCrashIdStringSize + 2));

  // The feedback server expects the crash IDs to be a comma-separated list.
  for (const auto* crash_info : crashes) {
    const base::Time& report_time =
        crash_info->state == UploadList::UploadInfo::State::Uploaded
            ? crash_info->upload_time
            : crash_info->capture_time;
    base::TimeDelta time_diff = now - report_time;
    if (time_diff < k120DaysTimeDelta) {
      const std::string& crash_id = crash_info->upload_id;
      all_crash_ids_list_.append(all_crash_ids_list_.empty() ? crash_id
                                                             : ", " + crash_id);
      if (time_diff < kOneHourTimeDelta) {
        crash_ids_list_.append(crash_ids_list_.empty() ? crash_id
                                                       : ", " + crash_id);
      }
    }
  }

  for (auto& request : pending_requests_)
    std::move(request).Run();

  pending_requests_.clear();
}

void CrashIdsSource::RespondWithCrashIds(SysLogsSourceCallback callback) {
  auto response = std::make_unique<SystemLogsResponse>();
  (*response)[feedback::FeedbackReport::kCrashReportIdsKey] = crash_ids_list_;
  (*response)[feedback::FeedbackReport::kAllCrashReportIdsKey] =
      all_crash_ids_list_;

  // We must respond anyways.
  std::move(callback).Run(std::move(response));
}

}  // namespace system_logs
