// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/network_condition_service.h"

#include <cstddef>

#include "base/bind.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace reporting {

NetworkConditionService::NetworkConditionService() = default;

NetworkConditionService::~NetworkConditionService() = default;

uint64_t NetworkConditionService::GetUploadRate() const {
  return upload_rate_;
}

uint64_t NetworkConditionService::ConvertKbpsToBytesPerSec(int32_t kbps) {
  return static_cast<uint64_t>(kbps) * (1024UL / 8UL);
}

NetworkConditionService& NetworkConditionService::SetUploadRateKbps(
    int32_t upload_rate_kbps) {
  upload_rate_ = ConvertKbpsToBytesPerSec(upload_rate_kbps);
  return *this;
}

// NetworkConditionService::NetworkConditionServiceObserver implementation.
NetworkConditionService::NetworkConditionServiceObserver::
    NetworkConditionServiceObserver(
        NetworkConditionService* network_condition_service)
    : network_condition_service_(network_condition_service) {
  // g_browser_process must be accessed from the UI thread
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&NetworkConditionService::NetworkConditionServiceObserver::
                         RegisterRTTAndThroughputEstimatesObserver,
                     // Will always be valid. Can only be destroyed by posting
                     // another task to this task sequence via |Destroy|.
                     base::Unretained(this)));
}
NetworkConditionService::NetworkConditionServiceObserver::UniquePtr
NetworkConditionService::NetworkConditionServiceObserver::MakeUnique(
    NetworkConditionService* network_condition_service) {
  return UniquePtr(
      new NetworkConditionServiceObserver(network_condition_service),
      &CallDestroy);
}

void NetworkConditionService::NetworkConditionServiceObserver::CallDestroy(
    NetworkConditionServiceObserver* observer) {
  observer->Destroy();
}

void NetworkConditionService::NetworkConditionServiceObserver::Destroy() {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(
                     [](NetworkConditionServiceObserver*
                            network_condition_service_observer) {
                       g_browser_process->network_quality_tracker()
                           ->RemoveRTTAndThroughputEstimatesObserver(
                               network_condition_service_observer);
                       delete network_condition_service_observer;
                     },
                     // |Destroy| can only be called once and |this| is only
                     // deleted after the posted task is executed.
                     base::Unretained(this)));
}

void NetworkConditionService::NetworkConditionServiceObserver::
    OnRTTOrThroughputEstimatesComputed(base::TimeDelta http_rtt,
                                       base::TimeDelta transport_rtt,
                                       int32_t downstream_throughput_kbps) {
  // Here we are using download rate to approximate upload rate. While they
  // may be far off in many circumstances, they are generally on the same
  // magnitude at least, which should be sufficient for our purpose.
  network_condition_service_->SetUploadRateKbps(downstream_throughput_kbps);
}

void NetworkConditionService::NetworkConditionServiceObserver::
    RegisterRTTAndThroughputEstimatesObserver() {
  DCHECK(g_browser_process != nullptr);
  auto* network_quality_tracker = g_browser_process->network_quality_tracker();
  // Simulate the notification once before getting on the official list of
  // observers.
  OnRTTOrThroughputEstimatesComputed(
      network_quality_tracker->GetHttpRTT(),
      network_quality_tracker->GetTransportRTT(),
      network_quality_tracker->GetDownstreamThroughputKbps());
  network_quality_tracker->AddRTTAndThroughputEstimatesObserver(this);
}

}  // namespace reporting
