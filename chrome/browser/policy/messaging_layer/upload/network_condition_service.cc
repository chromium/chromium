// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/network_condition_service.h"

#include <cstddef>

#include "base/memory/singleton.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace reporting {

uint64_t NetworkConditionService::GetUploadRate() const {
  return upload_rate_;
}

NetworkConditionService::NetworkConditionService() {
  // g_browser_process must be accessed from the UI thread
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &NetworkConditionService::RegisterRTTAndThroughputEstimatesObserver,
          // this is a singleton. Will always be valid.
          base::Unretained(this)));
}

NetworkConditionService* NetworkConditionService::GetInstance() {
  return base::Singleton<NetworkConditionService>::get();
}

NetworkConditionService& NetworkConditionService::SetUploadRateKbps(
    int32_t upload_rate_kbps) {
  upload_rate_ = ConvertKbpsToBytesPerSec(upload_rate_kbps);
  return *this;
}

uint64_t NetworkConditionService::ConvertKbpsToBytesPerSec(int32_t kbps) {
  return static_cast<uint64_t>(kbps) * (1024UL / 8UL);
}

void NetworkConditionService::OnRTTOrThroughputEstimatesComputed(
    base::TimeDelta http_rtt,
    base::TimeDelta transport_rtt,
    int32_t downstream_throughput_kbps) {
  // Here we are using download rate to approximate upload rate. While they may
  // be far off in many circumstances, they are generally on the same magnitude
  // at least, which should be sufficient for our purpose.
  SetUploadRateKbps(downstream_throughput_kbps);
}

void NetworkConditionService::RegisterRTTAndThroughputEstimatesObserver() {
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
