// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/network_condition_service.h"

#include <cstddef>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace reporting {

NetworkConditionService::NetworkConditionService() = default;

NetworkConditionService::~NetworkConditionService() = default;

uint64_t NetworkConditionService::GetUploadRate() const {
  return impl_->GetUploadRate();
}

// NetworkConditionService::NetworkConditionServiceImpl implementation.
NetworkConditionService::NetworkConditionServiceImpl::
    ~NetworkConditionServiceImpl() {
  // We can access g_browser_process here because the destructor is guaranteed
  // to be called from the UI thread.
  g_browser_process->network_quality_tracker()
      ->RemoveRTTAndThroughputEstimatesObserver(this);
}

NetworkConditionService::NetworkConditionServiceImpl::
    NetworkConditionServiceImpl() {
  // g_browser_process must be accessed from the UI thread
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&NetworkConditionService::NetworkConditionServiceImpl::
                         RegisterRTTAndThroughputEstimatesObserver,
                     // Will always be valid. Can only be destroyed by posting
                     // another task to this task sequence via |Destroy|.
                     base::Unretained(this)));
}

uint64_t
NetworkConditionService::NetworkConditionServiceImpl::ConvertKbpsToBytesPerSec(
    int32_t kbps) {
  return static_cast<uint64_t>(kbps) * (1024UL / 8UL);
}

NetworkConditionService::NetworkConditionServiceImpl::UniquePtr
NetworkConditionService::NetworkConditionServiceImpl::MakeUnique() {
  return UniquePtr(
      new NetworkConditionServiceImpl(),
      base::OnTaskRunnerDeleter(content::GetUIThreadTaskRunner({})));
}

uint64_t NetworkConditionService::NetworkConditionServiceImpl::GetUploadRate()
    const {
  return upload_rate_;
}

NetworkConditionService::NetworkConditionServiceImpl&
NetworkConditionService::NetworkConditionServiceImpl::SetUploadRateKbps(
    int32_t upload_rate_kbps) {
  upload_rate_ = ConvertKbpsToBytesPerSec(upload_rate_kbps);
  return *this;
}

void NetworkConditionService::NetworkConditionServiceImpl::
    OnRTTOrThroughputEstimatesComputed(base::TimeDelta http_rtt,
                                       base::TimeDelta transport_rtt,
                                       int32_t downstream_throughput_kbps) {
  // Here we are using download rate to approximate upload rate. While they
  // may be far off in many circumstances, they are generally on the same
  // magnitude at least, which should be sufficient for our purpose.
  SetUploadRateKbps(downstream_throughput_kbps);
}

void NetworkConditionService::NetworkConditionServiceImpl::
    RegisterRTTAndThroughputEstimatesObserver() {
  CHECK(g_browser_process);
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
