// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNAL_REPORTER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNAL_REPORTER_H_

#include "base/threading/sequenced_task_runner_handle.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/reporting/client/report_queue_provider.h"

namespace enterprise_connectors {

class DeviceTrustSignalReporter {
 public:
  DeviceTrustSignalReporter();
  virtual ~DeviceTrustSignalReporter();

  // Before sending each message, |policy_check| is used to verify that
  // the specific ReportQueue is still allowed. Because the creation of
  // ReportQueue is posted as an asynchronous task, |done_cb| is always
  // called when ReportQueue initialization is finished, but this class
  // is only usable after |done_cb| is called back with true.
  void Init(base::RepeatingCallback<bool(void)> policy_check,
            base::OnceCallback<void(bool)> done_cb);

  // Init() must have completed and |done_cb| above must have been called
  // without error before calling SendReport(), otherwise browser will crash.
  // ReportQueue::Enqueue with |sent_cb|.
  virtual void SendReport(base::Value value,
                          base::OnceCallback<void(bool)> sent_cb);

 protected:
  void OnCreateReportQueueResponse(
      base::OnceCallback<void(bool)> create_queue_cb,
      reporting::ReportQueueProvider::CreateReportQueueResponse
          report_queue_result);

  // Helper methods made virtual to be overwritten in unit tests.
  virtual policy::DMToken GetDmToken() const;
  virtual void PostCreateReportQueueTask(
      reporting::ReportQueueProvider::CreateReportQueueCallback create_queue_cb,
      std::unique_ptr<reporting::ReportQueueConfiguration> config);

 private:
  std::unique_ptr<reporting::ReportQueue> report_queue_;

  enum class CreateQueueStatus { NOT_STARTED, IN_PROGRESS, DONE };
  CreateQueueStatus create_queue_status_{CreateQueueStatus::NOT_STARTED};

  base::WeakPtrFactory<DeviceTrustSignalReporter> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNAL_REPORTER_H_
