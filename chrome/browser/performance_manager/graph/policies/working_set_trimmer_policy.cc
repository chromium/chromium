// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/policies/working_set_trimmer_policy.h"

#include "build/build_config.h"

#include "chrome/browser/performance_manager/mechanisms/working_set_trimmer.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/node_attached_data.h"
#include "components/performance_manager/public/graph/process_node.h"
#if defined(OS_WIN)
#include "chrome/browser/performance_manager/graph/policies/working_set_trimmer_policy_win.h"
#elif defined(OS_CHROMEOS)
#include "chrome/browser/performance_manager/graph/policies/working_set_trimmer_policy_chromeos.h"
#endif

namespace performance_manager {
namespace policies {

namespace {

class WorkingSetTrimData
    : public ExternalNodeAttachedDataImpl<WorkingSetTrimData> {
 public:
  explicit WorkingSetTrimData(const ProcessNode* node) {}
  ~WorkingSetTrimData() override = default;

  base::TimeTicks last_trim_;
};

}  // namespace

WorkingSetTrimmerPolicy::WorkingSetTrimmerPolicy() = default;
WorkingSetTrimmerPolicy::~WorkingSetTrimmerPolicy() = default;

void WorkingSetTrimmerPolicy::OnPassedToGraph(Graph* graph) {
  RegisterObservers(graph);
}

void WorkingSetTrimmerPolicy::OnTakenFromGraph(Graph* graph) {
  UnregisterObservers(graph);
}

void WorkingSetTrimmerPolicy::OnAllFramesInProcessFrozen(
    const ProcessNode* process_node) {
  TrimWorkingSet(process_node);
}

void WorkingSetTrimmerPolicy::RegisterObservers(Graph* graph) {
  graph->AddProcessNodeObserver(this);
}

void WorkingSetTrimmerPolicy::UnregisterObservers(Graph* graph) {
  graph->RemoveProcessNodeObserver(this);
}

base::TimeTicks WorkingSetTrimmerPolicy::GetLastTrimTime(
    const ProcessNode* process_node) {
  auto* data = WorkingSetTrimData::GetOrCreate(process_node);
  return data->last_trim_;
}

void WorkingSetTrimmerPolicy::SetLastTrimTimeNow(
    const ProcessNode* process_node) {
  SetLastTrimTime(process_node, base::TimeTicks::Now());
}

void WorkingSetTrimmerPolicy::SetLastTrimTime(const ProcessNode* process_node,
                                              base::TimeTicks time) {
  auto* data = WorkingSetTrimData::GetOrCreate(process_node);
  data->last_trim_ = time;
}

bool WorkingSetTrimmerPolicy::TrimWorkingSet(const ProcessNode* process_node) {
  auto* trimmer = mechanism::WorkingSetTrimmer::GetInstance();
  DCHECK(trimmer);
  if (process_node->GetProcess().IsValid()) {
    SetLastTrimTimeNow(process_node);
    return trimmer->TrimWorkingSet(process_node);
  }

  return false;
}

// static
bool WorkingSetTrimmerPolicy::PlatformSupportsWorkingSetTrim() {
#if defined(OS_WIN)
  return WorkingSetTrimmerPolicyWin::PlatformSupportsWorkingSetTrim();
#elif defined(OS_CHROMEOS)
  return WorkingSetTrimmerPolicyChromeOS::PlatformSupportsWorkingSetTrim();
#else
  return false;
#endif
}

// static
std::unique_ptr<WorkingSetTrimmerPolicy>
WorkingSetTrimmerPolicy::CreatePolicyForPlatform() {
#if defined(OS_WIN)
  return std::make_unique<WorkingSetTrimmerPolicyWin>();
#elif defined(OS_CHROMEOS)
  return std::make_unique<WorkingSetTrimmerPolicyChromeOS>();
#else
  NOTIMPLEMENTED() << "Platform does not support WorkingSetTrim.";
  return nullptr;
#endif
}

}  // namespace policies
}  // namespace performance_manager
