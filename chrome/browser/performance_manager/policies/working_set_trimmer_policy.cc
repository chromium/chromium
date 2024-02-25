// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/working_set_trimmer_policy.h"

#include "base/time/time.h"
#include "build/build_config.h"

#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/performance_manager/mechanisms/working_set_trimmer.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/node_attached_data.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/public/graph/process_node.h"
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/performance_manager/policies/working_set_trimmer_policy_chromeos.h"
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

const char kDescriberName[] = "WorkingSetTrimmerPolicy";

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
  graph->GetNodeDataDescriberRegistry()->RegisterDescriber(this,
                                                           kDescriberName);
}

void WorkingSetTrimmerPolicy::UnregisterObservers(Graph* graph) {
  graph->GetNodeDataDescriberRegistry()->UnregisterDescriber(this);
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

void WorkingSetTrimmerPolicy::TrimWorkingSet(const ProcessNode* process_node) {
  auto* trimmer = mechanism::WorkingSetTrimmer::GetInstance();
  DCHECK(trimmer);

  if (process_node->GetProcess().IsValid()) {
    SetLastTrimTimeNow(process_node);
    trimmer->TrimWorkingSet(process_node);
  }
}

base::Value::Dict WorkingSetTrimmerPolicy::DescribeProcessNodeData(
    const ProcessNode* node) const {
  auto* data = WorkingSetTrimData::Get(ProcessNodeImpl::FromNode(node));
  if (data == nullptr)
    return base::Value::Dict();

  base::Value::Dict ret;
  auto last_trim_age = base::TimeTicks::Now() - data->last_trim_;

  ret.Set(
      "last_trim",
      base::Value(base::StrCat(
          {base::NumberToString(last_trim_age.InSeconds()), " seconds ago"})));

  return ret;
}

// static
bool WorkingSetTrimmerPolicy::PlatformSupportsWorkingSetTrim() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return WorkingSetTrimmerPolicyChromeOS::PlatformSupportsWorkingSetTrim();
#else
  return false;
#endif
}

// static
std::unique_ptr<WorkingSetTrimmerPolicy>
WorkingSetTrimmerPolicy::CreatePolicyForPlatform() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return std::make_unique<WorkingSetTrimmerPolicyChromeOS>();
#else
  NOTIMPLEMENTED() << "Platform does not support WorkingSetTrim.";
  return nullptr;
#endif
}

}  // namespace policies
}  // namespace performance_manager
