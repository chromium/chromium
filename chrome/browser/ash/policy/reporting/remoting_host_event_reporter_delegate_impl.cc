// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/remoting_host_event_reporter_delegate_impl.h"

#include <memory>

#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/policy/reporting/user_event_reporter_helper.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/crd_event.pb.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "remoting/host/chromeos/host_event_reporter_impl.h"

namespace remoting {

// Production implementation of CRD event reporter for remoting host.
HostEventReporterDelegateImpl::HostEventReporterDelegateImpl(
    std::unique_ptr<::reporting::UserEventReporterHelper> user_event_helper)
    : helper_(base::MakeRefCounted<Helper>(
          ::reporting::UserEventReporterHelper::valid_task_runner(),
          std::move(user_event_helper))) {}

HostEventReporterDelegateImpl::~HostEventReporterDelegateImpl() = default;

// Helper class - refcounted in order to keep it alive until asynchronous
// Enqueue task completes.
class HostEventReporterDelegateImpl::Helper
    : public base::RefCountedDeleteOnSequence<
          HostEventReporterDelegateImpl::Helper> {
 public:
  Helper(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      std::unique_ptr<::reporting::UserEventReporterHelper> user_event_helper)
      : base::RefCountedDeleteOnSequence<Helper>(task_runner),
        task_runner_(task_runner),
        user_event_helper_(std::move(user_event_helper)) {}

  void EnqueueEvent(::ash::reporting::CRDRecord record) {
    task_runner_->PostTask(FROM_HERE, base::BindOnce(&Helper::EnqueueEventSync,
                                                     base::WrapRefCounted(this),
                                                     std::move(record)));
  }

 private:
  friend class base::RefCountedDeleteOnSequence<
      HostEventReporterDelegateImpl::Helper>;
  friend class base::DeleteHelper<HostEventReporterDelegateImpl::Helper>;

  // Private destructor in ref-counted class.
  ~Helper() = default;

  void EnqueueEventSync(::ash::reporting::CRDRecord record) {
    if (!user_event_helper_->ReportingEnabled(::ash::kReportCRDSessions)) {
      return;
    }
    if (!user_event_helper_->ShouldReportUser(
            record.host_user().user_email())) {
      record.clear_host_user();  // anonymize host user.
      if (user_event_helper_->IsKioskUser()) {
        record.set_is_kiosk_session(true);
      }
    }
    user_event_helper_->ReportEvent(
        std::make_unique<::ash::reporting::CRDRecord>(std::move(record)),
        ::reporting::Priority::FAST_BATCH);
  }

  // Task runner for asynchronous actions (including destructor).
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Helper for posting events.
  const std::unique_ptr<::reporting::UserEventReporterHelper>
      user_event_helper_;
};

void HostEventReporterDelegateImpl::EnqueueEvent(
    ::ash::reporting::CRDRecord record) {
  helper_->EnqueueEvent(std::move(record));
}

// static
std::unique_ptr<HostEventReporter> HostEventReporter::Create(
    scoped_refptr<HostStatusMonitor> monitor) {
  return std::make_unique<HostEventReporterImpl>(
      monitor, std::make_unique<HostEventReporterDelegateImpl>());
}
}  // namespace remoting
