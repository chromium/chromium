// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/enterprise_memory_limit_evaluator.h"

#include "base/bind.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/performance_manager.h"

namespace memory {

EnterpriseMemoryLimitEvaluator::EnterpriseMemoryLimitEvaluator(
    std::unique_ptr<util::MemoryPressureVoter> voter)
    : voter_(std::move(voter)), weak_ptr_factory_(this) {}

EnterpriseMemoryLimitEvaluator::~EnterpriseMemoryLimitEvaluator() {
  DCHECK(!observer_);
}

void EnterpriseMemoryLimitEvaluator::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!observer_);
  auto observer =
      std::make_unique<EnterpriseMemoryLimitEvaluator::GraphObserver>(
          base::BindRepeating(
              &EnterpriseMemoryLimitEvaluator::OnTotalResidentSetKbSample,
              weak_ptr_factory_.GetWeakPtr()),
          base::SequencedTaskRunnerHandle::Get());
  observer_ = observer.get();
  performance_manager::PerformanceManager::PassToGraph(FROM_HERE,
                                                       std::move(observer));
}

std::unique_ptr<EnterpriseMemoryLimitEvaluator::GraphObserver>
EnterpriseMemoryLimitEvaluator::StartForTesting() {
  DCHECK(!observer_);
  auto observer =
      std::make_unique<EnterpriseMemoryLimitEvaluator::GraphObserver>(
          base::BindRepeating(
              &EnterpriseMemoryLimitEvaluator::OnTotalResidentSetKbSample,
              weak_ptr_factory_.GetWeakPtr()),
          base::SequencedTaskRunnerHandle::Get());
  observer_ = observer.get();
  return observer;
}

void EnterpriseMemoryLimitEvaluator::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer_);
  // Start by invalidating all the WeakPtrs that have been served, this will
  // invalidate the callback owned by the observer.
  weak_ptr_factory_.InvalidateWeakPtrs();
  // It's safe to pass |observer_| as Unretained, it's merely used as a key.
  performance_manager::PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindOnce(
                     [](performance_manager::GraphOwned* observer,
                        performance_manager::Graph* graph) {
                       // This will destroy the observer since TakeFromGraph
                       // returns a unique_ptr.
                       graph->TakeFromGraph(observer);
                     },
                     base::Unretained(observer_)));
  observer_ = nullptr;
}

void EnterpriseMemoryLimitEvaluator::StopForTesting() {
  observer_ = nullptr;
}

void EnterpriseMemoryLimitEvaluator::OnTotalResidentSetKbSample(
    uint64_t resident_set_sample_kb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Limit is in mb, must convert to kb.
  bool is_critical = resident_set_sample_kb > (resident_set_limit_mb_ * 1024);
  voter_->SetVote(
      is_critical ? base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL
                  : base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
      /* notify_listeners = */ is_critical);
}

void EnterpriseMemoryLimitEvaluator::SetResidentSetLimitMb(
    uint64_t resident_set_limit_mb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  resident_set_limit_mb_ = resident_set_limit_mb;
}

bool EnterpriseMemoryLimitEvaluator::IsRunning() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return observer_ != nullptr;
}

EnterpriseMemoryLimitEvaluator::GraphObserver::GraphObserver(
    base::RepeatingCallback<void(uint64_t)> on_sample_callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : on_sample_callback_(std::move(on_sample_callback)),
      task_runner_(task_runner) {}

EnterpriseMemoryLimitEvaluator::GraphObserver::~GraphObserver() = default;

void EnterpriseMemoryLimitEvaluator::GraphObserver::OnPassedToGraph(
    performance_manager::Graph* graph) {
  graph->AddSystemNodeObserver(this);
}

void EnterpriseMemoryLimitEvaluator::GraphObserver::OnTakenFromGraph(
    performance_manager::Graph* graph) {
  graph->RemoveSystemNodeObserver(this);
}

void EnterpriseMemoryLimitEvaluator::GraphObserver::
    OnProcessMemoryMetricsAvailable(
        const performance_manager::SystemNode* system_node) {
  uint64_t total_rss_kb = 0U;
  for (const auto* node : system_node->GetGraph()->GetAllProcessNodes())
    total_rss_kb += node->GetResidentSetKb();
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(on_sample_callback_), total_rss_kb));
}

void EnterpriseMemoryLimitEvaluator::GraphObserver::
    OnProcessMemoryMetricsAvailableForTesting(uint64_t total_rss_kb) {
  on_sample_callback_.Run(total_rss_kb);
}

}  // namespace memory
