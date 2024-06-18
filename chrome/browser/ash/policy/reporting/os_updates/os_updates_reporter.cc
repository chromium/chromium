// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/os_updates/os_updates_reporter.h"

#include <memory>
#include <optional>
#include <string>

#include "base/memory/ptr_util.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/reporting/event_based_logs/event_based_log_uploader.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/os_events.pb.h"
#include "chrome/browser/upgrade_detector/build_state.h"
#include "chrome/common/channel_info.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/version/version_loader.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/user_manager/user_manager.h"
#include "components/version_info/version_info.h"

namespace reporting {

namespace {

bool ShouldReportStatus(const update_engine::StatusResult& status) {
  // If the status change is for an installation, this means that DLCs are being
  // installed and has nothing to with the OS. Ignore this status change.
  if (status.is_install()) {
    return false;
  }

  // We are only interested in the following operations: ERROR,
  // REPORTING_ERROR_EVENT and UPDATED_NEED_REBOOT. All the other
  // ones will be ignored.
  if (status.current_operation() != update_engine::Operation::ERROR &&
      status.current_operation() !=
          update_engine::Operation::REPORTING_ERROR_EVENT &&
      status.current_operation() !=
          update_engine::Operation::UPDATED_NEED_REBOOT) {
    return false;
  }

  // We don't report the rollback success here as the process still needs to
  // boot into the new image, powerwash, and recover the data.
  if (status.current_operation() ==
          update_engine::Operation::UPDATED_NEED_REBOOT &&
      status.is_enterprise_rollback()) {
    return false;
  }
  return true;
}

}  // namespace

// static
std::unique_ptr<OsUpdatesReporter> OsUpdatesReporter::Create() {
  return base::WrapUnique(new OsUpdatesReporter(
      std::make_unique<UserEventReporterHelper>(Destination::OS_EVENTS)));
}

// static
std::unique_ptr<OsUpdatesReporter> OsUpdatesReporter::CreateForTesting(
    std::unique_ptr<UserEventReporterHelper> helper) {
  return base::WrapUnique(new OsUpdatesReporter(std::move(helper)));
}

OsUpdatesReporter::~OsUpdatesReporter() {
  if (ash::SessionManagerClient::Get()) {
    ash::SessionManagerClient::Get()->RemoveObserver(this);
  }
  if (ash::UpdateEngineClient::Get()) {
    ash::UpdateEngineClient::Get()->RemoveObserver(this);
  }
}

void OsUpdatesReporter::MaybeReportEvent(
    ash::reporting::OsEventsRecord record) {
  if (!helper_->ReportingEnabled(ash::kReportOsUpdateStatus)) {
    return;
  }

  std::optional<std::string> os_version = chromeos::version_loader::GetVersion(
      chromeos::version_loader::VERSION_SHORT);
  record.set_current_os_version(os_version.value_or("0.0.0.0"));

  record.set_current_channel(
      std::string(version_info::GetChannelString(chrome::GetChannel())));

  record.set_event_timestamp_sec(base::Time::Now().ToTimeT());

  auto log_upload_id = NotifyOsUpdateFailed();
  if (log_upload_id.has_value()) {
    record.set_log_upload_id(log_upload_id.value());
  }

  helper_->ReportEvent(
      std::make_unique<ash::reporting::OsEventsRecord>(std::move(record)),
      ::reporting::Priority::SECURITY);
}

void OsUpdatesReporter::UpdateStatusChanged(
    const update_engine::StatusResult& status) {
  if (!ShouldReportStatus(status)) {
    return;
  }

  ::ash::reporting::OsEventsRecord record;
  // Report that an update was successfully applied, just waiting to reboot.
  if (status.current_operation() ==
      update_engine::Operation::UPDATED_NEED_REBOOT) {
    record.mutable_update_event();
    record.set_os_operation_type(ash::reporting::OsOperationType::SUCCESS);
  }

  //  Report a failure downloading or updating the rollback/update image.
  if (status.current_operation() == update_engine::Operation::ERROR ||
      status.current_operation() ==
          update_engine::Operation::REPORTING_ERROR_EVENT) {
    if (status.is_enterprise_rollback()) {
      record.mutable_rollback_event();
    } else {
      record.mutable_update_event();
    }
    record.set_os_operation_type(ash::reporting::OsOperationType::FAILURE);
  }

  record.set_target_os_version(status.new_version());
  MaybeReportEvent(std::move(record));
}

void OsUpdatesReporter::PowerwashRequested(bool remote_request) {
  base::Time time_now = base::Time::Now();

  ::ash::reporting::OsEventsRecord record;
  record.mutable_powerwash_event()->set_remote_request(remote_request);
  record.set_os_operation_type(ash::reporting::OsOperationType::INITIATED);
  // If the powerwash is remote requested we need to do additional checks
  // but if its user requested we always want to report it.
  if (remote_request) {
    // Powerwashes initiated by remote commands trigger call this method twice a
    // few seconds apart, adding this conditions to verify that we only report
    // the powerwash once.
    if (time_now - last_powerwash_attempt_time_ < base::Seconds(10)) {
      return;
    }
    last_powerwash_attempt_time_ = time_now;
  }
  MaybeReportEvent(std::move(record));
}

void OsUpdatesReporter::AddObserver(OsUpdateEventBasedLogObserver* observer) {
  observers_.AddObserver(observer);
}

void OsUpdatesReporter::RemoveObserver(
    const OsUpdateEventBasedLogObserver* observer) {
  observers_.RemoveObserver(observer);
}

std::optional<std::string> OsUpdatesReporter::NotifyOsUpdateFailed() {
  // We won't generate an upload ID if there's no observers to trigger the log
  // upload.
  if (observers_.empty()) {
    return std::nullopt;
  }
  std::string upload_id = policy::EventBasedLogUploader::GenerateUploadId();
  for (auto& observer : observers_) {
    observer.OnOsUpdateFailed(upload_id);
  }
  return upload_id;
}

OsUpdatesReporter::OsUpdatesReporter(
    std::unique_ptr<UserEventReporterHelper> helper)
    : helper_(std::move(helper)) {
  if (ash::UpdateEngineClient::Get()) {
    ash::UpdateEngineClient::Get()->AddObserver(this);
  }
  if (ash::SessionManagerClient::Get()) {
    ash::SessionManagerClient::Get()->AddObserver(this);
  }
}
}  // namespace reporting
