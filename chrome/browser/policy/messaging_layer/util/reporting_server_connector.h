// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_REPORTING_SERVER_CONNECTOR_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_REPORTING_SERVER_CONNECTOR_H_

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/values.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
                                    absl::optional<base::Value::Dict> context,
                                    ResponseCallback callback);

 private:
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

  // Set only in production (on UI task runner), not in tests.
  raw_ptr<::policy::CloudPolicyCore> core_ = nullptr;

  // Used by `UploadEncryptedReport` - must be non-null by then.
  // Set only on UI task runner.
  raw_ptr<::policy::CloudPolicyClient> client_ = nullptr;
};
}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_REPORTING_SERVER_CONNECTOR_H_
