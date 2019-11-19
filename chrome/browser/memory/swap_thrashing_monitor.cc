// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/swap_thrashing_monitor.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task_runner_util.h"
#include "base/time/time.h"

#if defined(OS_WIN)
#include "chrome/browser/memory/swap_thrashing_monitor_delegate_win.h"
#endif

namespace features {

// Feature flag controlling whether or not this monitor should be enabled.
const base::Feature kSwapThrashingMonitor{"SwapThrashingMonitor",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features

namespace memory {
namespace {

constexpr base::TimeDelta kSamplingInterval = base::TimeDelta::FromSeconds(2);

// Enumeration of UMA swap thrashing levels. This needs to be kept in sync with
// tools/metrics/histograms/enums.xml and the swap_thrashing levels defined in
// swap_thrashing_common.h.
//
// These values are persisted to logs, and should therefore never be renumbered
// nor reused
enum SwapThrashingLevelUMA {
  UMA_SWAP_THRASHING_LEVEL_NONE = 0,
  UMA_SWAP_THRASHING_LEVEL_SUSPECTED = 1,
  UMA_SWAP_THRASHING_LEVEL_CONFIRMED = 2,
  // This must be the last value in the enum.
  UMA_SWAP_THRASHING_LEVEL_COUNT,
};

// Enumeration of UMA swap thrashing level changes. This needs to be kept in
// sync with tools/metrics/histograms/enums.xml.
//
// These values are persisted to logs, and should therefore never be renumbered
// nor reused
enum SwapThrashingLevelChangesUMA {
  UMA_SWAP_THRASHING_LEVEL_CHANGE_NONE_TO_SUSPECTED = 0,
  UMA_SWAP_THRASHING_LEVEL_CHANGE_SUSPECTED_TO_CONFIRMED = 1,
  UMA_SWAP_THRASHING_LEVEL_CHANGE_CONFIRMED_TO_SUSPECTED = 2,
  UMA_SWAP_THRASHING_LEVEL_CHANGE_SUSPECTED_TO_NONE = 3,
  // This must be the last value in the enum.
  UMA_SWAP_THRASHING_LEVEL_CHANGE_COUNT,
};

// Converts a swap thrashing level to an UMA enumeration value.
SwapThrashingLevelUMA SwapThrashingLevelToUmaEnumValue(
    SwapThrashingLevel level) {
  switch (level) {
    case SwapThrashingLevel::SWAP_THRASHING_LEVEL_NONE:
      return UMA_SWAP_THRASHING_LEVEL_NONE;
    case SwapThrashingLevel::SWAP_THRASHING_LEVEL_SUSPECTED:
      return UMA_SWAP_THRASHING_LEVEL_SUSPECTED;
    case SwapThrashingLevel::SWAP_THRASHING_LEVEL_CONFIRMED:
      return UMA_SWAP_THRASHING_LEVEL_CONFIRMED;
  }
  NOTREACHED();
  return UMA_SWAP_THRASHING_LEVEL_NONE;
}

std::unique_ptr<SwapThrashingMonitorDelegate> GetPlatformSpecificDelegate() {
#if defined(OS_WIN)
  return std::make_unique<SwapThrashingMonitorDelegateWin>();
#else
  return std::make_unique<SwapThrashingMonitorDelegate>();
#endif
}

}  // namespace

// static
void SwapThrashingMonitor::Initialize() {
  GetInstance();
}

SwapThrashingMonitor* SwapThrashingMonitor::GetInstance() {
  static SwapThrashingMonitor* instance = new SwapThrashingMonitor();
  return instance;
}

SwapThrashingMonitor::SwapThrashingMonitor()
    : delegate_(GetPlatformSpecificDelegate().release(),
                base::OnTaskRunnerDeleter(blocking_task_runner_)),
      current_swap_thrashing_level_(
          SwapThrashingLevel::SWAP_THRASHING_LEVEL_NONE) {
  DCHECK(base::FeatureList::IsEnabled(features::kSwapThrashingMonitor));
  StartObserving();
}

SwapThrashingMonitor::~SwapThrashingMonitor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

SwapThrashingLevel SwapThrashingMonitor::GetCurrentSwapThrashingLevel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return current_swap_thrashing_level_;
}

void SwapThrashingMonitor::CheckSwapThrashingPressureAndRecordStatistics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::BindOnce(
          &SwapThrashingMonitorDelegate::SampleAndCalculateSwapThrashingLevel,
          base::Unretained(delegate_.get())),
      base::BindOnce(&SwapThrashingMonitor::RecordSwapThrashingLevel,
                     weak_factory_.GetWeakPtr()));
}

void SwapThrashingMonitor::StartObserving() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(sebmarchand): Determine if the system is using on-disk swap, abort if
  // it isn't as there won't be any swap-paging to observe (on-disk swap could
  // later become available if the user turn it on but this case is rare that
  // it's safe to ignore it). See crbug.com/779309.
  check_timer_.Start(
      FROM_HERE, kSamplingInterval,
      base::BindRepeating(
          &SwapThrashingMonitor::CheckSwapThrashingPressureAndRecordStatistics,
          base::Unretained(this)));
}

void SwapThrashingMonitor::RecordSwapThrashingLevel(
    SwapThrashingLevel swap_thrashing_level) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Record the state changes.
  if (swap_thrashing_level != current_swap_thrashing_level_) {
    SwapThrashingLevelChangesUMA level_change =
        UMA_SWAP_THRASHING_LEVEL_CHANGE_COUNT;

    switch (current_swap_thrashing_level_) {
      case SwapThrashingLevel::SWAP_THRASHING_LEVEL_NONE: {
        DCHECK_EQ(SwapThrashingLevel::SWAP_THRASHING_LEVEL_SUSPECTED,
                  swap_thrashing_level);
        level_change = UMA_SWAP_THRASHING_LEVEL_CHANGE_NONE_TO_SUSPECTED;
        break;
      }
      case SwapThrashingLevel::SWAP_THRASHING_LEVEL_SUSPECTED: {
        if (swap_thrashing_level ==
            SwapThrashingLevel::SWAP_THRASHING_LEVEL_NONE) {
          level_change = UMA_SWAP_THRASHING_LEVEL_CHANGE_SUSPECTED_TO_NONE;
        } else {
          level_change = UMA_SWAP_THRASHING_LEVEL_CHANGE_SUSPECTED_TO_CONFIRMED;
        }
        break;
      }
      case SwapThrashingLevel::SWAP_THRASHING_LEVEL_CONFIRMED: {
        DCHECK_EQ(SwapThrashingLevel::SWAP_THRASHING_LEVEL_SUSPECTED,
                  swap_thrashing_level);
        level_change = UMA_SWAP_THRASHING_LEVEL_CHANGE_CONFIRMED_TO_SUSPECTED;
        break;
      }
      default:
        break;
    }
    UMA_HISTOGRAM_ENUMERATION("Memory.Experimental.SwapThrashingLevelChanges",
                              level_change,
                              UMA_SWAP_THRASHING_LEVEL_CHANGE_COUNT);
  }

  current_swap_thrashing_level_ = swap_thrashing_level;
  UMA_HISTOGRAM_ENUMERATION(
      "Memory.Experimental.SwapThrashingLevel",
      SwapThrashingLevelToUmaEnumValue(current_swap_thrashing_level_),
      UMA_SWAP_THRASHING_LEVEL_COUNT);
}

}  // namespace memory
