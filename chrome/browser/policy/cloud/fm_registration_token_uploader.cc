// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/fm_registration_token_uploader.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/scoped_observation.h"
#include "components/policy/core/common/cloud/policy_invalidation_scope.h"
#include "components/policy/core/common/cloud/signing_service.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {
namespace {

// After the first failure, retry after 1 minute, then after 2, 4 etc up to a
// maximum of 1 day.
static constexpr net::BackoffEntry::Policy kUploadRetryBackoffPolicy = {
    .num_errors_to_ignore = 0,
    .initial_delay_ms = base::Minutes(1).InMilliseconds(),
    .multiply_factor = 2,
    .jitter_factor = 0.1,
    .maximum_backoff_ms = base::Days(1).InMilliseconds(),
    .always_use_initial_delay = true,
};

std::string ToString(PolicyInvalidationScope scope) {
// The macro replaces a enum entry with a string as is. E.g. Enum::kItem becomes
// "kItem".
#define CASE(_name)                    \
  case PolicyInvalidationScope::_name: \
    return #_name;

  switch (scope) {
    CASE(kUser);
    CASE(kDevice);
    CASE(kDeviceLocalAccount);
    CASE(kCBCM);
  }
#undef CASE
}

em::FmRegistrationTokenUploadRequest::TokenType ScopeToTokenType(
    PolicyInvalidationScope scope) {
  switch (scope) {
    case PolicyInvalidationScope::kUser:
      return em::FmRegistrationTokenUploadRequest::USER;
    case PolicyInvalidationScope::kDevice:
      return em::FmRegistrationTokenUploadRequest::DEVICE;
    case PolicyInvalidationScope::kDeviceLocalAccount:
      NOTREACHED() << "No requests for device local accounts";
    case PolicyInvalidationScope::kCBCM:
      return em::FmRegistrationTokenUploadRequest::BROWSER;
  }
}

}  // namespace

// Observes a cloud policy core connection event and is destroyed afterwards.
class FmRegistrationTokenUploader::CloudPolicyCoreConnectionObserver
    : public CloudPolicyCore::Observer {
 public:
  CloudPolicyCoreConnectionObserver(
      CloudPolicyCore* core,
      base::OnceCallback<void()> on_connected_callback)
      : on_connected_callback_(std::move(on_connected_callback)) {
    observation.Observe(core);
  }

  void OnCoreConnected(CloudPolicyCore* core) override {
    std::move(on_connected_callback_).Run();
  }

  void OnRefreshSchedulerStarted(CloudPolicyCore* core) override {}

  void OnCoreDisconnecting(CloudPolicyCore* core) override {}

 private:
  base::ScopedObservation<CloudPolicyCore, CloudPolicyCoreConnectionObserver>
      observation{this};
  base::OnceCallback<void()> on_connected_callback_;
};

// Observes a cloud policy client registration event and is destroyed
// afterwards.
class FmRegistrationTokenUploader::CloudPolicyClientRegistrationObserver
    : public CloudPolicyClient::Observer {
 public:
  CloudPolicyClientRegistrationObserver(
      CloudPolicyClient* client,
      base::OnceCallback<void()> on_connected_callback)
      : on_connected_callback_(std::move(on_connected_callback)) {
    observation.Observe(client);
  }

  void OnPolicyFetched(CloudPolicyClient* client) override {}

  void OnRegistrationStateChanged(CloudPolicyClient* client) override {
    if (client->is_registered()) {
      std::move(on_connected_callback_).Run();
    }
  }

  void OnClientError(CloudPolicyClient* client) override {}

 private:
  base::ScopedObservation<CloudPolicyClient,
                          CloudPolicyClientRegistrationObserver>
      observation{this};
  base::OnceCallback<void()> on_connected_callback_;
};

class FmRegistrationTokenUploader::UploadJob {
 public:
  using ResultCallback = CloudPolicyClient::ResultCallback;

  UploadJob(CloudPolicyClient* client,
            em::FmRegistrationTokenUploadRequest request,
            ResultCallback on_registered_callback)
      : on_registered_callback_(std::move(on_registered_callback)) {
    client->UploadFmRegistrationToken(
        std::move(request),
        base::BindOnce(&UploadJob::OnRegistrationTokenUploaded,
                       weak_factory_.GetWeakPtr()));
  }

 private:
  void OnRegistrationTokenUploaded(CloudPolicyClient::Result result) {
    std::move(on_registered_callback_).Run(result);
  }

  ResultCallback on_registered_callback_;

  base::WeakPtrFactory<UploadJob> weak_factory_{this};
};

FmRegistrationTokenUploader::FmRegistrationTokenUploader(
    PolicyInvalidationScope scope,
    invalidation::InvalidationListener* invalidation_listener,
    CloudPolicyCore* core)
    : scope_(scope),
      invalidation_listener_(invalidation_listener),
      core_(core),
      upload_retry_backoff_(&kUploadRetryBackoffPolicy) {
  CHECK(invalidation_listener_);
  CHECK(core);
  CHECK_NE(scope_, PolicyInvalidationScope::kDeviceLocalAccount)
      << "Registration token is not expected for device local "
         "accounts";
  LOG_POLICY(WARNING, REMOTE_COMMANDS)
      << "Starting FmRegistrationTokenUploader for " << ToString(scope_)
      << " scope";
  invalidation_listener_->Start(this);
}

FmRegistrationTokenUploader::~FmRegistrationTokenUploader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  invalidation_listener_->Shutdown();
}

void FmRegistrationTokenUploader::OnRegistrationTokenReceived(
    const std::string& registration_token,
    base::Time token_end_of_life) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  upload_retry_backoff_.Reset();

  return DoUploadRegistrationToken(TokenData{
      .registration_token = std::move(registration_token),
      .token_end_of_life = token_end_of_life,
  });
}

void FmRegistrationTokenUploader::DoUploadRegistrationToken(
    TokenData token_data) {
  StopOngoingUpload();

  CloudPolicyClient* client = core_->client();

  if (!client) {
    LOG_POLICY(ERROR, REMOTE_COMMANDS)
        << "Client is missing for " << ToString(scope_) << " scope";

    // Async task is required as it will destroy the observer that will call
    // this callback and remove it from the observers list.
    core_observer_ = std::make_unique<CloudPolicyCoreConnectionObserver>(
        core_, base::BindOnce(
                   &FmRegistrationTokenUploader::DoAsyncUploadRegistrationToken,
                   base::Unretained(this), std::move(token_data),
                   /*delay=*/base::TimeDelta()));

    return;
  }

  if (!client->is_registered()) {
    LOG_POLICY(ERROR, REMOTE_COMMANDS)
        << "Client is not registered for " << ToString(scope_) << " scope";

    // Async upload task is required as it will destroy the observer that will
    // call this callback and remove it from the observers list.
    client_observer_ = std::make_unique<CloudPolicyClientRegistrationObserver>(
        client,
        base::BindOnce(
            &FmRegistrationTokenUploader::DoAsyncUploadRegistrationToken,
            base::Unretained(this), std::move(token_data),
            /*delay=*/base::TimeDelta()));
    return;
  }

  em::FmRegistrationTokenUploadRequest request;
  request.set_token(token_data.registration_token);
  request.set_protocol_version(
      invalidation::InvalidationListener::kInvalidationProtocolVersion);
  request.set_token_type(ScopeToTokenType(scope_));
  request.set_expiration_timestamp_ms(
      token_data.token_end_of_life.InMillisecondsSinceUnixEpoch());

  upload_job_ = std::make_unique<UploadJob>(
      client, std::move(request),
      base::BindOnce(&FmRegistrationTokenUploader::OnRegistrationTokenUploaded,
                     base::Unretained(this), std::move(token_data)));
}

void FmRegistrationTokenUploader::DoAsyncUploadRegistrationToken(
    TokenData token_data,
    base::TimeDelta delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FmRegistrationTokenUploader::DoUploadRegistrationToken,
                     weak_factory_.GetWeakPtr(), std::move(token_data)),
      delay);
}

void FmRegistrationTokenUploader::OnRegistrationTokenUploaded(
    TokenData token_data,
    CloudPolicyClient::Result result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  StopOngoingUpload();

  if (!result.IsSuccess()) {
    LOG_POLICY(ERROR, REMOTE_COMMANDS)
        << "Upload failed for " << ToString(scope_)
        << " scope: " << result.GetDMServerError();

    invalidation_listener_->SetRegistrationUploadStatus(
        invalidation::InvalidationListener::RegistrationTokenUploadStatus::
            kFailed);

    // Retry failed upload after timeout.
    DoAsyncUploadRegistrationToken(
        std::move(token_data),
        /*delay=*/upload_retry_backoff_.GetTimeUntilRelease());
    upload_retry_backoff_.InformOfRequest(/*succeeded=*/false);
    return;
  }

  LOG_POLICY(ERROR, REMOTE_COMMANDS)
      << "Registration token uploaded for " << ToString(scope_) << " scope";

  invalidation_listener_->SetRegistrationUploadStatus(
      invalidation::InvalidationListener::RegistrationTokenUploadStatus::
          kSucceeded);
}

void FmRegistrationTokenUploader::StopOngoingUpload() {
  core_observer_.reset();
  client_observer_.reset();
  upload_job_.reset();
  // Drop any posted tasks.
  weak_factory_.InvalidateWeakPtrs();
}

}  // namespace policy
