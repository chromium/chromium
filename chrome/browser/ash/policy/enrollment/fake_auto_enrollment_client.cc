// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/fake_auto_enrollment_client.h"

#include "chrome/browser/ash/policy/enrollment/private_membership/psm_rlwe_dmserver_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {

FakeAutoEnrollmentClient::FactoryImpl::FactoryImpl(
    const base::RepeatingCallback<void(FakeAutoEnrollmentClient*)>&
        fake_client_created_callback)
    : fake_client_created_callback_(fake_client_created_callback) {}

FakeAutoEnrollmentClient::FactoryImpl::~FactoryImpl() {}

std::unique_ptr<AutoEnrollmentClient>
FakeAutoEnrollmentClient::FactoryImpl::CreateForFRE(
    const AutoEnrollmentClient::ProgressCallback& progress_callback,
    DeviceManagementService* device_management_service,
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& server_backed_state_key,
    int power_initial,
    int power_limit) {
  std::unique_ptr<FakeAutoEnrollmentClient> fake_client =
      std::make_unique<FakeAutoEnrollmentClient>(progress_callback);
  fake_client_created_callback_.Run(fake_client.get());
  return fake_client;
}

std::unique_ptr<AutoEnrollmentClient>
FakeAutoEnrollmentClient::FactoryImpl::CreateForInitialEnrollment(
    const AutoEnrollmentClient::ProgressCallback& progress_callback,
    DeviceManagementService* device_management_service,
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& device_serial_number,
    const std::string& device_brand_code,
    int power_initial,
    int power_limit,
    std::unique_ptr<PsmRlweDmserverClient> psm_rlwe_dmserver_client) {
  std::unique_ptr<FakeAutoEnrollmentClient> fake_client =
      std::make_unique<FakeAutoEnrollmentClient>(progress_callback);
  fake_client_created_callback_.Run(fake_client.get());
  return fake_client;
}

FakeAutoEnrollmentClient::FakeAutoEnrollmentClient(
    const ProgressCallback& progress_callback)
    : progress_callback_(progress_callback),
      state_(AUTO_ENROLLMENT_STATE_IDLE) {}

FakeAutoEnrollmentClient::~FakeAutoEnrollmentClient() {}

void FakeAutoEnrollmentClient::Start() {
  SetState(AUTO_ENROLLMENT_STATE_PENDING);
}

void FakeAutoEnrollmentClient::Retry() {}

void FakeAutoEnrollmentClient::CancelAndDeleteSoon() {
  delete this;
}

void FakeAutoEnrollmentClient::SetState(AutoEnrollmentState target_state) {
  state_ = target_state;
  progress_callback_.Run(state_);
}

}  // namespace policy
