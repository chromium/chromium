// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/components/settings/cros_settings_names.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/ash/policy/reporting/user_event_reporter_helper.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/crd_event.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "remoting/host/chromeos/host_event_reporter_impl.h"

namespace remoting {

class HostStatusMonitor;

namespace {

// Production implementation of CRD event reporter for remoting host.
class HostEventReporterDelegateImpl : public HostEventReporterImpl::Delegate {
 public:
  HostEventReporterDelegateImpl()
      : helper_(base::MakeRefCounted<Helper>(
            ::reporting::UserEventReporterHelper::valid_task_runner())) {}

  HostEventReporterDelegateImpl(const HostEventReporterDelegateImpl& other) =
      delete;
  HostEventReporterDelegateImpl& operator=(
      const HostEventReporterDelegateImpl& other) = delete;
  ~HostEventReporterDelegateImpl() override = default;

 private:
  // Helper class - refcounted in order to keep it alive until asynchronous
  // Enqueue task completes.
  class Helper : public base::RefCountedDeleteOnSequence<Helper> {
   public:
    explicit Helper(scoped_refptr<base::SequencedTaskRunner> task_runner)
        : base::RefCountedDeleteOnSequence<Helper>(task_runner),
          task_runner_(task_runner),
          user_event_helper_(::reporting::Destination::CRD_EVENTS,
                             ::reporting::EventType::kUser) {}

    void EnqueueEvent(::ash::reporting::CRDRecord record) {
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&Helper::EnqueueEventSync, base::WrapRefCounted(this),
                         std::move(record)));
    }

   private:
    friend class base::RefCountedDeleteOnSequence<Helper>;
    friend class base::DeleteHelper<Helper>;

    // Private destructor in ref-counted class.
    ~Helper() = default;

    void EnqueueEventSync(::ash::reporting::CRDRecord record) {
      if (!user_event_helper_.ReportingEnabled(::ash::kReportCRDSessions)) {
        return;
      }
      if (!user_event_helper_.ShouldReportUser(
              record.host_user().user_email())) {
        record.clear_host_user();  // anonymize host user.
      }
      user_event_helper_.ReportEvent(&record,
                                     ::reporting::Priority::FAST_BATCH);
    }

    // Task runner for asynchronous actions (including destructor).
    const scoped_refptr<base::SequencedTaskRunner> task_runner_;

    // Helper for posting events.
    ::reporting::UserEventReporterHelper user_event_helper_;
  };

  void EnqueueEvent(::ash::reporting::CRDRecord record) override {
    helper_->EnqueueEvent(std::move(record));
  }

  // Helper for posting events.
  scoped_refptr<Helper> helper_;
};

}  // namespace

// static
std::unique_ptr<HostEventReporter> HostEventReporter::Create(
    scoped_refptr<HostStatusMonitor> monitor) {
  return std::make_unique<HostEventReporterImpl>(
      monitor, std::make_unique<HostEventReporterDelegateImpl>());
}

}  // namespace remoting
