// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEMORY_ENTERPRISE_MEMORY_LIMIT_EVALUATOR_H_
#define CHROME_BROWSER_MEMORY_ENTERPRISE_MEMORY_LIMIT_EVALUATOR_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/memory_pressure/memory_pressure_voter.h"
#include "components/performance_manager/public/decorators/process_metrics_decorator.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/system_node.h"

namespace memory {

// Periodically receives updates on Chrome's current total resident set,
// compares that usage to the limit set via an Enterprise policy, and uses a
// MemoryLimitVoter to send its vote on the MemoryPressureLevel to the
// MultiSourceMemoryPressureMonitor.
class EnterpriseMemoryLimitEvaluator {
 protected:
  class GraphObserver;

 public:
  explicit EnterpriseMemoryLimitEvaluator(
      std::unique_ptr<memory_pressure::MemoryPressureVoter> voter);

  EnterpriseMemoryLimitEvaluator(const EnterpriseMemoryLimitEvaluator&) =
      delete;
  EnterpriseMemoryLimitEvaluator& operator=(
      const EnterpriseMemoryLimitEvaluator&) = delete;

  ~EnterpriseMemoryLimitEvaluator();

  // Starts/stops observing the resident set of Chrome processes and notifying
  // its MemoryPressureVoter when it is above the limit.
  void Start();
  void Stop();

  std::unique_ptr<GraphObserver> StartForTesting();
  void StopForTesting();

  // Sets the limit against which the total resident set will be compared.
  void SetResidentSetLimitMb(uint64_t resident_set_limit_mb);

  // Indicates if this evaluator is currently running.
  bool IsRunning() const;

 private:
  // Called on the sequence that owns this object every time a new total RSS
  // sample is available.
  void OnTotalResidentSetKbSample(uint64_t resident_set_sample_kb);

  // Raw pointer to the GraphObserver used by this object to monitor the total
  // RSS. This is only meant to be used as a key to remove the observer once
  // it's not necessary anymore, do not call functions directly from this
  // pointer.
  raw_ptr<GraphObserver> observer_ = nullptr;

  uint64_t resident_set_limit_mb_ = 0;

  const std::unique_ptr<memory_pressure::MemoryPressureVoter> voter_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<EnterpriseMemoryLimitEvaluator> weak_ptr_factory_;
};

// Instances of this class are constructed and destructed on the main thread.
// They are then passed to the Performance Manager's Graph and a task gets
// posted to the proper task runner when new data is available.
class EnterpriseMemoryLimitEvaluator::GraphObserver
    : public performance_manager::SystemNode::ObserverDefaultImpl,
      public performance_manager::GraphOwned {
 public:
  // The constructor of this class takes 2 parameters: the callback to run each
  // time a new sample is available and the task runner on which this callback
  // should run.
  GraphObserver(base::RepeatingCallback<void(uint64_t)> on_sample_callback,
                scoped_refptr<base::SequencedTaskRunner> task_runner);

  ~GraphObserver() override;

  // GraphOwned implementation, called on the PM sequence:
  void OnPassedToGraph(performance_manager::Graph* graph) override;
  void OnTakenFromGraph(performance_manager::Graph* graph) override;

  // SystemNode::ObserverDefaultImpl, called on the PM sequence:
  void OnProcessMemoryMetricsAvailable(
      const performance_manager::SystemNode* system_node) override;

  void OnProcessMemoryMetricsAvailableForTesting(uint64_t total_rss_kb);

 private:
  const base::RepeatingCallback<void(uint64_t)> on_sample_callback_;

  std::unique_ptr<
      performance_manager::ProcessMetricsDecorator::ScopedMetricsInterestToken>
      metrics_interest_token_;

  // The task runner on which |on_sample_callback_| should be invoked.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace memory

#endif  // CHROME_BROWSER_MEMORY_ENTERPRISE_MEMORY_LIMIT_EVALUATOR_H_
