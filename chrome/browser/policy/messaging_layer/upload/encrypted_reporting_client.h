// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_ENCRYPTED_REPORTING_CLIENT_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_ENCRYPTED_REPORTING_CLIENT_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/values.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace policy {
class CloudPolicyClient;
}  // namespace policy

namespace reporting {

// Implements the logic required to talk to the device management service
// for Encrypted Reporting Pipeline records upload.
class EncryptedReportingClient {
 public:
  class Delegate {
   public:
    Delegate() = default;

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    virtual ~Delegate() = default;

    virtual policy::DeviceManagementService* device_management_service() const;
  };

  using ResponseCallback =
      base::OnceCallback<void(absl::optional<base::Value::Dict>)>;

  explicit EncryptedReportingClient(
      std::unique_ptr<Delegate> delegate = std::make_unique<Delegate>());

  EncryptedReportingClient(const EncryptedReportingClient&) = delete;
  EncryptedReportingClient& operator=(const EncryptedReportingClient&) = delete;

  ~EncryptedReportingClient();

  // Uploads a report containing `merging_payload` (merged into the default
  // payload of the job).  The `callback` will be called when the upload is
  // completed.
  void UploadReport(base::Value::Dict merging_payload,
                    absl::optional<base::Value::Dict> context,
                    policy::CloudPolicyClient* cloud_policy_client,
                    ResponseCallback callback);

 private:
  using JobSet =
      base::flat_set<std::unique_ptr<policy::DeviceManagementService::Job>,
                     base::UniquePtrComparator>;

  // Callback for encrypted report upload requests.
  void OnReportUploadCompleted(ResponseCallback callback,
                               policy::DeviceManagementService::Job* job,
                               policy::DeviceManagementStatus status,
                               int response_code,
                               absl::optional<base::Value::Dict> response);

  SEQUENCE_CHECKER(sequence_checker_);

  JobSet request_jobs_ GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<Delegate> delegate_;

  base::WeakPtrFactory<EncryptedReportingClient> weak_ptr_factory_{this};
};
}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_ENCRYPTED_REPORTING_CLIENT_H_
