// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/network_quality_observer.h"

#include <cstdint>
#include <limits>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/sequence_checker.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "components/component_updater/component_updater_paths.h"
#include "services/network/public/cpp/network_quality_tracker.h"

namespace component_updater {
namespace {
constexpr uint64_t kBytesPerMegabyte = 1000 * 1000;
constexpr uint64_t kBytesPerGigabyte = 1000 * kBytesPerMegabyte;

// The minimum available storage in the install directory for recording network
// quality.
constexpr int64_t kMinBytesForReporting = 20 * kBytesPerGigabyte;
}  // namespace

// Listens for changes to the network quality and manages metric collection.
class NetworkQualityObserver
    : public network::NetworkQualityTracker::RTTAndThroughputEstimatesObserver {
 public:
  NetworkQualityObserver()
      : background_sequence_(
            base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    g_browser_process->network_quality_tracker()
        ->AddRTTAndThroughputEstimatesObserver(this);
  }

  ~NetworkQualityObserver() override = default;

 private:
  // Overrides for net::RTTAndThroughputEstimatesObserver.
  void OnRTTOrThroughputEstimatesComputed(
      base::TimeDelta http_rtt,
      base::TimeDelta transport_rtt,
      int32_t downstream_throughput_kbps) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (downstream_throughput_kbps > 0 &&
        downstream_throughput_kbps != std::numeric_limits<int32_t>::max()) {
      background_sequence_->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](int32_t downstream_throughput_kbps) {
                base::FilePath components_dir;
                if (!base::PathService::Get(DIR_COMPONENT_USER,
                                            &components_dir)) {
                  return;
                }
                int64_t free_space =
                    base::SysInfo::AmountOfFreeDiskSpace(components_dir);
                if (free_space < 0) {
                  return;
                }

                base::UmaHistogramMemoryLargeMB(
                    "ComponentUpdater.ClientHealth.AvailableStorage",
                    free_space / kBytesPerMegabyte);
                if (free_space >= kMinBytesForReporting) {
                  base::UmaHistogramMemoryKB(
                      "ComponentUpdater.ClientHealth.DownstreamThroughput",
                      downstream_throughput_kbps);
                }
              },
              downstream_throughput_kbps));
    }
  }

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<base::SequencedTaskRunner> background_sequence_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

void EnsureNetworkQualityObserver() {
  static base::NoDestructor<std::unique_ptr<NetworkQualityObserver>>
      network_quality_observer(std::make_unique<NetworkQualityObserver>());
}

}  // namespace component_updater
