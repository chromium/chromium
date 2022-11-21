// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_REPORTING_SERVER_CONNECTOR_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_REPORTING_SERVER_CONNECTOR_H_

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

// Singleton wrapper of a client used for uploading events to the reporting
// server. Enables safe access to the client with an ability to detect when it
// is disconnected. Currently implemented with ::policy::CloudPolicyClient;
// later on we will switch it to a dedicated reporting client.
class ReportingServerConnector : public ::policy::CloudPolicyCore::Observer {
 public:
  using ResponseCallback =
      base::OnceCallback<void(StatusOr<base::Value::Dict>)>;

  friend struct base::DefaultSingletonTraits<ReportingServerConnector>;

  // RAII class for testing ReportingServerConnector - substitutes cloud policy
  // client instead of getting it from the cloud policy core. Resets client when
  // destructed.
  class TestEnvironment;

  ReportingServerConnector(const ReportingServerConnector& other) = delete;
  ReportingServerConnector& operator=(const ReportingServerConnector& other) =
      delete;
  ~ReportingServerConnector() override;

  // Accesses singleton ReportingServerConnector instance.
  static ReportingServerConnector* GetInstance();

  // Uploads a report containing `merging_payload` (merged into the default
  // payload of the job). The client must be in a registered state (otherwise
  // the upload fails). The `callback` will be called when the operation
  // completes or fails.
  static void UploadEncryptedReport(base::Value::Dict merging_payload,
                                    ResponseCallback callback);

 private:
  // Manages reporting accumulated payload sizes per hour via UMA.
  class PayloadSizePerHourUmaReporter {
   public:
    PayloadSizePerHourUmaReporter();
    ~PayloadSizePerHourUmaReporter();
    PayloadSizePerHourUmaReporter(const PayloadSizePerHourUmaReporter&) =
        delete;
    PayloadSizePerHourUmaReporter& operator=(
        const PayloadSizePerHourUmaReporter&) = delete;

    // Adds request paylaod size to the accumulated request payload size.
    void RecordRequestPayloadSize(int payload_size);

    // Adds response paylaod size to the accumulated response payload size.
    void RecordResponsePayloadSize(int payload_size);

    // Gets the weak pointer.
    base::WeakPtr<PayloadSizePerHourUmaReporter> GetWeakPtr();

   private:
    // Reporting interval.
    static constexpr base::TimeDelta kReportingInterval = base::Hours(1);

    // Converts bytes to KiB.
    static int ConvertBytesToKiB(int bytes);

    // Reports the data to UMA.
    void Report();

    // Accumulated request payload size since last report.
    int request_payload_size_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

    // Accumulated response payload size since last report.
    int response_payload_size_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

    // Timer that controls when network usage is reported.
    base::RepeatingTimer timer_;

    SEQUENCE_CHECKER(sequence_checker_);

    base::WeakPtrFactory<PayloadSizePerHourUmaReporter> weak_factory_{this};
  };

  // Constructor to be used by singleton only.
  ReportingServerConnector();

  // Returns OK if `CloudPolicyCore` instance is usable, or error status
  // otherwise. If successful, caches the core and registers `this` as its
  // observer.
  Status EnsureUsableCore();

  // Returns OK if `CloudPolicyClient` is already usable or made usable, or
  // error status otherwise. If successful, caches the client.
  Status EnsureUsableClient();

  // ::policy::CloudPolicyCore::Observer implementation
  void OnCoreConnected(::policy::CloudPolicyCore* core) override;
  void OnRefreshSchedulerStarted(::policy::CloudPolicyCore* core) override;
  void OnCoreDisconnecting(::policy::CloudPolicyCore* core) override;
  void OnCoreDestruction(::policy::CloudPolicyCore* core) override;

  // Gets `payload_size_per_hour_uma_reporter_`'s weak pointer.
  base::WeakPtr<PayloadSizePerHourUmaReporter>
  GetPayloadSizePerHourUmaReporter();

  // Manages reporting accumulated payload sizes per hour via UMA.
  PayloadSizePerHourUmaReporter payload_size_per_hour_uma_reporter_;

  // Set only in production (on UI task runner), not in tests.
  raw_ptr<::policy::CloudPolicyCore> core_ = nullptr;

  // Used by `UploadEncryptedReport` - must be non-null by then.
  // Set only on UI task runner.
  raw_ptr<::policy::CloudPolicyClient> client_ = nullptr;
};
}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_REPORTING_SERVER_CONNECTOR_H_
