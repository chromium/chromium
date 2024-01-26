// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_device_data_updater_impl.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/nearby_sharing/client/nearby_share_client.h"
#include "third_party/nearby/sharing/proto/rpc_resources.pb.h"

namespace {

const char kDeviceIdPrefix[] = "users/me/devices/";
const char kContactsFieldMaskPath[] = "contacts";
const char kCertificatesFieldMaskPath[] = "public_certificates";

void RecordResultMetrics(ash::nearby::NearbyHttpResult result) {
  base::UmaHistogramEnumeration(
      "Nearby.Share.LocalDeviceData.DeviceDataUpdater.HttpResult", result);
}

}  // namespace

// static
NearbyShareDeviceDataUpdaterImpl::Factory*
    NearbyShareDeviceDataUpdaterImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<NearbyShareDeviceDataUpdater>
NearbyShareDeviceDataUpdaterImpl::Factory::Create(
    const std::string& device_id,
    base::TimeDelta timeout,
    NearbyShareClientFactory* client_factory) {
  if (test_factory_)
    return test_factory_->CreateInstance(device_id, timeout, client_factory);

  return base::WrapUnique(
      new NearbyShareDeviceDataUpdaterImpl(device_id, timeout, client_factory));
}

// static
void NearbyShareDeviceDataUpdaterImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

NearbyShareDeviceDataUpdaterImpl::Factory::~Factory() = default;

NearbyShareDeviceDataUpdaterImpl::NearbyShareDeviceDataUpdaterImpl(
    const std::string& device_id,
    base::TimeDelta timeout,
    NearbyShareClientFactory* client_factory)
    : NearbyShareDeviceDataUpdater(device_id),
      timeout_(timeout),
      client_factory_(client_factory) {}

NearbyShareDeviceDataUpdaterImpl::~NearbyShareDeviceDataUpdaterImpl() = default;

void NearbyShareDeviceDataUpdaterImpl::HandleNextRequest() {
  timer_.Start(FROM_HERE, timeout_,
               base::BindOnce(&NearbyShareDeviceDataUpdaterImpl::OnTimeout,
                              base::Unretained(this)));

  nearby::sharing::proto::UpdateDeviceRequest request;
  request.mutable_device()->set_name(kDeviceIdPrefix + device_id_);
  if (pending_requests_.front().contacts) {
    *request.mutable_device()->mutable_contacts() = {
        pending_requests_.front().contacts->begin(),
        pending_requests_.front().contacts->end()};
    request.mutable_update_mask()->add_paths(kContactsFieldMaskPath);
  }
  if (pending_requests_.front().certificates) {
    *request.mutable_device()->mutable_public_certificates() = {
        pending_requests_.front().certificates->begin(),
        pending_requests_.front().certificates->end()};
    request.mutable_update_mask()->add_paths(kCertificatesFieldMaskPath);
  }

  client_ = client_factory_->CreateInstance();
  client_->UpdateDevice(
      request,
      base::BindOnce(&NearbyShareDeviceDataUpdaterImpl::OnRpcSuccess,
                     base::Unretained(this)),
      base::BindOnce(&NearbyShareDeviceDataUpdaterImpl::OnRpcFailure,
                     base::Unretained(this)));
}

void NearbyShareDeviceDataUpdaterImpl::OnRpcSuccess(
    const nearby::sharing::proto::UpdateDeviceResponse& response) {
  timer_.Stop();
  nearby::sharing::proto::UpdateDeviceResponse response_copy(response);
  client_.reset();
  RecordResultMetrics(ash::nearby::NearbyHttpResult::kSuccess);
  FinishAttempt(response_copy);
}

void NearbyShareDeviceDataUpdaterImpl::OnRpcFailure(
    ash::nearby::NearbyHttpError error) {
  timer_.Stop();
  client_.reset();
  RecordResultMetrics(ash::nearby::NearbyHttpErrorToResult(error));
  FinishAttempt(/*response=*/std::nullopt);
}

void NearbyShareDeviceDataUpdaterImpl::OnTimeout() {
  client_.reset();
  RecordResultMetrics(ash::nearby::NearbyHttpResult::kTimeout);
  FinishAttempt(/*response=*/std::nullopt);
}
