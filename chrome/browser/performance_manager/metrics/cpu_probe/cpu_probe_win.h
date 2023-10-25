// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_CPU_PROBE_CPU_PROBE_WIN_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_CPU_PROBE_CPU_PROBE_WIN_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"
#include "chrome/browser/performance_manager/metrics/cpu_probe/cpu_probe.h"

namespace performance_manager::metrics {

class CpuProbeWin : public CpuProbe {
 public:
  // Factory method for production instances.
  static std::unique_ptr<CpuProbeWin> Create();

  ~CpuProbeWin() override;

  CpuProbeWin(const CpuProbeWin&) = delete;
  CpuProbeWin& operator=(const CpuProbeWin&) = delete;

 protected:
  CpuProbeWin();

  // CpuProbe implementation.
  void Update(SampleCallback callback) override;
  base::WeakPtr<CpuProbe> GetWeakPtr() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(CpuProbeWinTest, ProductionDataNoCrash);

  class BlockingTaskRunnerHelper;

  base::SequenceBound<BlockingTaskRunnerHelper> helper_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<CpuProbeWin> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace performance_manager::metrics

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_CPU_PROBE_CPU_PROBE_WIN_H_
