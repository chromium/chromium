// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_FM_REGISTRATION_TOKEN_UPLOADER_H_
#define CHROME_BROWSER_POLICY_CLOUD_FM_REGISTRATION_TOKEN_UPLOADER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/policy_invalidation_scope.h"
#include "net/base/backoff_entry.h"

namespace policy {

// Responsible for starting `InvalidationListener` and uploading FM registration
// token (obtained by the listener) to a backend server.
class FmRegistrationTokenUploader
    : public invalidation::RegistrationTokenHandler {
 public:
  FmRegistrationTokenUploader(
      PolicyInvalidationScope scope,
      invalidation::InvalidationListener* invalidation_listener,
      CloudPolicyCore* core);

  ~FmRegistrationTokenUploader() override;

  FmRegistrationTokenUploader(const FmRegistrationTokenUploader&) = delete;
  FmRegistrationTokenUploader& operator=(const FmRegistrationTokenUploader&) =
      delete;

  // invalidation::RegistrationTokenHandler:
  void OnRegistrationTokenReceived(const std::string& registration_token,
                                   base::Time token_end_of_life) override;

 private:
  class CloudPolicyClientRegistrationObserver;
  class CloudPolicyCoreConnectionObserver;
  class UploadJob;

  struct TokenData {
    std::string registration_token;
    base::Time token_end_of_life;
  };

  void DoUploadRegistrationToken(TokenData token_data);
  void DoAsyncUploadRegistrationToken(TokenData token_data,
                                      base::TimeDelta delay);

  void OnRegistrationTokenUploaded(TokenData token_data,
                                   CloudPolicyClient::Result result);

  void StopOngoingUpload();

  const PolicyInvalidationScope scope_;

  raw_ptr<invalidation::InvalidationListener> invalidation_listener_ = nullptr;

  raw_ptr<CloudPolicyCore> core_ = nullptr;

  std::unique_ptr<CloudPolicyCoreConnectionObserver> core_observer_;
  std::unique_ptr<CloudPolicyClientRegistrationObserver> client_observer_;
  std::unique_ptr<UploadJob> upload_job_;
  net::BackoffEntry upload_retry_backoff_;

  SEQUENCE_CHECKER(sequence_checker_);
  // WeakPtrFactory used to create callbacks to this object.
  base::WeakPtrFactory<FmRegistrationTokenUploader> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_FM_REGISTRATION_TOKEN_UPLOADER_H_
