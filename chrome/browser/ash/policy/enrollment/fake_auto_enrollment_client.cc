// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/fake_auto_enrollment_client.h"

#include "chrome/browser/ash/login/oobe_configuration.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_state.h"
#include "chrome/browser/ash/policy/enrollment/psm/rlwe_dmserver_client.h"
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
    std::unique_ptr<psm::RlweDmserverClient> psm_rlwe_dmserver_client,
    ash::OobeConfiguration* oobe_config) {
  std::unique_ptr<FakeAutoEnrollmentClient> fake_client =
      std::make_unique<FakeAutoEnrollmentClient>(progress_callback);
  fake_client_created_callback_.Run(fake_client.get());
  return fake_client;
}

FakeAutoEnrollmentClient::FakeAutoEnrollmentClient(
    const ProgressCallback& progress_callback)
    : progress_callback_(progress_callback) {}

FakeAutoEnrollmentClient::~FakeAutoEnrollmentClient() {}

void FakeAutoEnrollmentClient::Start() {}

void FakeAutoEnrollmentClient::Retry() {}

void FakeAutoEnrollmentClient::SetState(AutoEnrollmentState target_state) {
  progress_callback_.Run(target_state);
}

}  // namespace policy
