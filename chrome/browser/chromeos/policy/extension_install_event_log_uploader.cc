// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/extension_install_event_log_uploader.h"

#include <atomic>

#include "base/bind.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/install_event_log_util.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/reporting_util.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_provider.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace em = enterprise_management;

namespace policy {
namespace {

ExtensionInstallEventLogUploader::GetReportQueueConfigCallback
CreateReportQueueConfigGetter(Profile* profile) {
  return base::BindRepeating(
      [](Profile* profile,
         ExtensionInstallEventLogUploader::ReportQueueConfigResultCallback
             complete_cb) {
        auto task = base::BindOnce(
            [](Profile* profile,
               base::OnceCallback<void(
                   ::reporting::StatusOr<std::unique_ptr<
                       ::reporting::ReportQueueConfiguration>>)> complete_cb) {
              auto dm_token = GetDMToken(profile, /*only_affiliated=*/false);
              if (!dm_token.is_valid()) {
                std::move(complete_cb)
                    .Run(::reporting::Status(
                        ::reporting::error::INVALID_ARGUMENT,
                        "DMToken must be valid"));
                return;
              }
              std::move(complete_cb)
                  .Run(::reporting::ReportQueueConfiguration::Create(
                      dm_token.value(), ::reporting::Destination::UPLOAD_EVENTS,
                      base::BindRepeating(
                          []() { return ::reporting::Status::StatusOK(); })));
            },
            profile, std::move(complete_cb));

        content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                                     std::move(task));
      },
      profile);
}

}  // namespace

class ExtensionInstallEventLogUploader::ReportQueueBuilderLeaderTracker
    : public base::RefCountedThreadSafe<ReportQueueBuilderLeaderTracker> {
 public:
  static scoped_refptr<ReportQueueBuilderLeaderTracker> Create() {
    return base::WrapRefCounted(new ReportQueueBuilderLeaderTracker());
  }

  ::reporting::StatusOr<base::OnceCallback<void()>> RequestLeaderPromotion() {
    bool expected = false;
    if (!has_promoted_leader_.compare_exchange_strong(expected, true)) {
      return ::reporting::Status(::reporting::error::RESOURCE_EXHAUSTED,
                                 "Only one leader is allowed at a time.");
    }
    return base::BindOnce(&ReportQueueBuilderLeaderTracker::ReleaseLeader,
                          this);
  }

 private:
  friend class base::RefCountedThreadSafe<ReportQueueBuilderLeaderTracker>;

  ReportQueueBuilderLeaderTracker();
  virtual ~ReportQueueBuilderLeaderTracker();

  void ReleaseLeader() {
    bool expected = true;
    DCHECK(has_promoted_leader_.compare_exchange_strong(expected, false))
        << "Leader wasn't set";
  }

  std::atomic<bool> has_promoted_leader_{false};
};

ExtensionInstallEventLogUploader::Delegate::~Delegate() {}

ExtensionInstallEventLogUploader::ExtensionInstallEventLogUploader(
    Profile* profile)
    : InstallEventLogUploaderBase(profile),
      leader_tracker_(ReportQueueBuilderLeaderTracker::Create()),
      report_queue_builder_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({})),
      get_report_queue_config_cb_(CreateReportQueueConfigGetter(profile)) {}

ExtensionInstallEventLogUploader::~ExtensionInstallEventLogUploader() = default;

void ExtensionInstallEventLogUploader::SetDelegate(Delegate* delegate) {
  if (delegate_)
    CancelUpload();
  delegate_ = delegate;
}

void ExtensionInstallEventLogUploader::SetReportQueue(
    std::unique_ptr<::reporting::ReportQueue> report_queue) {
  if (report_queue_ == nullptr) {
    report_queue_ = std::move(report_queue);
  }
}

void ExtensionInstallEventLogUploader::SetBuildReportQueueConfigurationForTests(
    const std::string& dm_token) {
  get_report_queue_config_cb_ = base::BindRepeating(
      [](const std::string& dm_token,
         ReportQueueConfigResultCallback complete_cb) {
        std::move(complete_cb)
            .Run(::reporting::ReportQueueConfiguration::Create(
                dm_token, ::reporting::Destination::UPLOAD_EVENTS,
                base::BindRepeating(
                    []() { return ::reporting::Status::StatusOK(); })));
      },
      dm_token);
}

ExtensionInstallEventLogUploader::ReportQueueBuilderLeaderTracker::
    ReportQueueBuilderLeaderTracker() = default;
ExtensionInstallEventLogUploader::ReportQueueBuilderLeaderTracker::
    ~ReportQueueBuilderLeaderTracker() = default;

ExtensionInstallEventLogUploader::ReportQueueBuilder::ReportQueueBuilder(
    base::OnceCallback<void(std::unique_ptr<::reporting::ReportQueue>,
                            base::OnceCallback<void()>)> set_report_queue_cb,
    GetReportQueueConfigCallback get_report_queue_config_cb,
    scoped_refptr<ReportQueueBuilderLeaderTracker> leader_tracker,
    base::OnceCallback<void(bool)> completion_cb,
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner)
    : ::reporting::TaskRunnerContext<bool>(std::move(completion_cb),
                                           sequenced_task_runner),
      set_report_queue_cb_(std::move(set_report_queue_cb)),
      get_report_queue_config_cb_(std::move(get_report_queue_config_cb)),
      leader_tracker_(leader_tracker) {}

ExtensionInstallEventLogUploader::ReportQueueBuilder::~ReportQueueBuilder() =
    default;

void ExtensionInstallEventLogUploader::ReportQueueBuilder::OnStart() {
  auto promo_result = leader_tracker_->RequestLeaderPromotion();
  if (!promo_result.ok()) {
    // There is already a ReportQueueBuilder building a ReportQueue - go
    // ahead and exit.
    Complete();
    return;
  }

  release_leader_cb_ = std::move(promo_result.ValueOrDie());
  BuildReportQueueConfig();
}

void ExtensionInstallEventLogUploader::ReportQueueBuilder::
    BuildReportQueueConfig() {
  std::move(get_report_queue_config_cb_)
      .Run(base::BindOnce(&ReportQueueBuilder::OnReportQueueConfigResult,
                          base::Unretained(this)));
}

void ExtensionInstallEventLogUploader::ReportQueueBuilder::
    OnReportQueueConfigResult(
        ReportQueueConfigResult report_queue_config_result) {
  if (!report_queue_config_result.ok()) {
    Complete();
    return;
  }
  Schedule(&ReportQueueBuilder::BuildReportQueue, base::Unretained(this),
           std::move(report_queue_config_result.ValueOrDie()));
}

void ExtensionInstallEventLogUploader::ReportQueueBuilder::BuildReportQueue(
    std::unique_ptr<::reporting::ReportQueueConfiguration>
        report_queue_config) {
  ::reporting::ReportQueueProvider::CreateQueue(
      std::move(report_queue_config),
      base::BindOnce(&ReportQueueBuilder::OnReportQueueResult,
                     base::Unretained(this)));
}

void ExtensionInstallEventLogUploader::ReportQueueBuilder::OnReportQueueResult(
    ::reporting::StatusOr<std::unique_ptr<::reporting::ReportQueue>>
        report_queue_result) {
  if (!report_queue_result.ok()) {
    Complete();
    return;
  }
  Schedule(&ReportQueueBuilder::SetReportQueue, base::Unretained(this),
           std::move(report_queue_result.ValueOrDie()));
}

void ExtensionInstallEventLogUploader::ReportQueueBuilder::SetReportQueue(
    std::unique_ptr<::reporting::ReportQueue> report_queue) {
  DCHECK(report_queue);
  std::move(set_report_queue_cb_)
      .Run(std::move(report_queue),
           base::BindOnce(&ReportQueueBuilder::Complete,
                          base::Unretained(this)));
}

void ExtensionInstallEventLogUploader::ReportQueueBuilder::Complete() {
  Schedule(&ReportQueueBuilder::ReleaseLeader, base::Unretained(this));
}

void ExtensionInstallEventLogUploader::ReportQueueBuilder::ReleaseLeader() {
  if (release_leader_cb_) {
    std::move(release_leader_cb_).Run();
  }
  Response(true);
}

void ExtensionInstallEventLogUploader::CancelClientUpload() {
  weak_factory_.InvalidateWeakPtrs();
}

void ExtensionInstallEventLogUploader::StartSerialization() {
  delegate_->SerializeExtensionLogForUpload(
      base::BindOnce(&ExtensionInstallEventLogUploader::OnSerialized,
                     weak_factory_.GetWeakPtr()));
}

void ExtensionInstallEventLogUploader::CheckDelegateSet() {
  CHECK(delegate_);
}

void ExtensionInstallEventLogUploader::OnUploadSuccess() {
  delegate_->OnExtensionLogUploadSuccess();
}

void ExtensionInstallEventLogUploader::PostTaskForStartSerialization() {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExtensionInstallEventLogUploader::StartSerialization,
                     weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(retry_backoff_ms_));
}

void ExtensionInstallEventLogUploader::OnSerialized(
    const em::ExtensionInstallReportRequest* report) {
  // If report_queue_ hasn't been set, start a ReportQueueBuilder and call
  // OnUploadDone(false). The upload will be attempted again in the future.
  // If another ReportQueueBuilder starts while the previous one is running, it
  // will exit early because the first ReportQueueBuilder will acquire leader
  // role from ReportQueueBuilderLeaderTracker.
  // If the ReportQueueBuilder fails to build, it will release the leader and
  // the next ReportQueueBuilder will acquire the leader role. If a
  // ReportQueueBuilder sets the report_queue_ after it has been checked here,
  // a new ReportQueueBuilder will spawn, but will be unable to set the
  // report_queue_.
  if (report_queue_ == nullptr) {
    auto set_report_queue_cb = base::BindOnce(
        [](base::WeakPtr<ExtensionInstallEventLogUploader> uploader,
           scoped_refptr<base::SingleThreadTaskRunner> task_runner,
           std::unique_ptr<::reporting::ReportQueue> report_queue,
           base::OnceCallback<void()> on_set_cb) {
          auto call_uploader_with_report_queue_cb = base::BindOnce(
              [](base::WeakPtr<ExtensionInstallEventLogUploader> uploader,
                 std::unique_ptr<::reporting::ReportQueue> report_queue,
                 base::OnceCallback<void()> on_set_cb) {
                uploader->SetReportQueue(std::move(report_queue));
                std::move(on_set_cb);
              },
              std::move(uploader), std::move(report_queue),
              std::move(on_set_cb));
          task_runner->PostTask(FROM_HERE,
                                std::move(call_uploader_with_report_queue_cb));
        },
        weak_factory_.GetWeakPtr(), base::ThreadTaskRunnerHandle::Get());

    auto completion_cb = base::BindOnce([](bool success) {
      LOG_IF(WARNING, !success) << "ReportQueueBuilder was unsuccessful";
    });

    ::reporting::Start<ReportQueueBuilder>(
        std::move(set_report_queue_cb), get_report_queue_config_cb_,
        leader_tracker_, std::move(completion_cb),
        report_queue_builder_task_runner_);

    OnUploadDone(false);
    return;
  }
  EnqueueReport(*report);
}

void ExtensionInstallEventLogUploader::EnqueueReport(
    const em::ExtensionInstallReportRequest& report) {
  base::Value context = ::reporting::GetContext(profile_);
  base::Value event_list = ConvertExtensionProtoToValue(&report, context);

  base::Value value_report = RealtimeReportingJobConfiguration::BuildReport(
      std::move(event_list), std::move(context));

  // Uploader must be called on the correct thread, in order to achieve that we
  // pass the appropriate task_runner along with the call.
  auto on_enqueue_done_cb = base::BindOnce(
      [](base::WeakPtr<ExtensionInstallEventLogUploader> uploader,
         scoped_refptr<base::SingleThreadTaskRunner> task_runner,
         ::reporting::Status status) {
        auto call_uploader_with_status = base::BindOnce(
            [](base::WeakPtr<ExtensionInstallEventLogUploader> uploader,
               const ::reporting::Status& status) {
              uploader->OnUploadDone(status.ok());
            },
            uploader, status);

        task_runner->PostTask(FROM_HERE, std::move(call_uploader_with_status));
      },
      weak_factory_.GetWeakPtr(), base::ThreadTaskRunnerHandle::Get());

  report_queue_->Enqueue(std::move(value_report),
                         ::reporting::Priority::SLOW_BATCH,
                         std::move(on_enqueue_done_cb));
}

}  // namespace policy
