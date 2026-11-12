// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/discards/tab_discards_dump_android.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/process/process_handle.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_tuning_utils.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/graph_operations.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/TabDiscardState_jni.h"

namespace discards {

namespace {

using performance_manager::FrameNode;
using performance_manager::Graph;
using performance_manager::GraphOperations;
using performance_manager::PageLiveStateDecorator;
using performance_manager::PageNode;
using performance_manager::PageType;
using performance_manager::PerformanceManager;
using performance_manager::ProcessNode;
using performance_manager::user_tuning::GetCannotDiscardReasonsForPageNode;

struct TabCandidate {
  raw_ptr<const PageNode> page_node;
  std::vector<std::string> cannot_discard_reasons;
  bool can_discard;
  bool is_visible;
  bool is_focused;
  base::TimeTicks last_visibility_change_time;

  // Sorts with ascending importance (most discardable first).
  bool operator<(const TabCandidate& rhs) const {
    if (can_discard != rhs.can_discard) {
      return can_discard;
    }
    if (is_focused != rhs.is_focused) {
      return rhs.is_focused;
    }
    if (is_visible != rhs.is_visible) {
      return rhs.is_visible;
    }
    return last_visibility_change_time < rhs.last_visibility_change_time;
  }
};

std::string_view GetTabVisibilityString(const PageNode* page_node) {
  if (page_node->IsVisible()) {
    return "visible";
  }
  if (content::WebContents* contents = page_node->GetWebContents().get()) {
    if (contents->GetVisibility() == content::Visibility::OCCLUDED) {
      return "occluded";
    }
  }
  return "hidden";
}

std::string_view GetLoadingStateString(PageNode::LoadingState loading_state) {
  switch (loading_state) {
    case PageNode::LoadingState::kLoadingNotStarted:
    case PageNode::LoadingState::kLoadingTimedOut:
      return "unloaded";
    case PageNode::LoadingState::kLoading:
      return "loading";
    case PageNode::LoadingState::kLoadedBusy:
    case PageNode::LoadingState::kLoadedIdle:
      return "loaded";
  }
}

std::string_view GetLifecycleStateString(const PageNode* page_node,
                                         std::string_view visibility,
                                         std::string_view loading_state) {
  const auto* live_state =
      PageLiveStateDecorator::Data::FromPageNode(page_node);
  if (live_state && live_state->IsDiscarded()) {
    return "discarded";
  }
  if (page_node->GetLifecycleState() == PageNode::LifecycleState::kFrozen) {
    return "frozen";
  }
  if (loading_state == "unloaded") {
    return "unloaded";
  }
  if (visibility == "visible") {
    return page_node->IsFocused() ? "active" : "passive";
  }
  if (visibility == "occluded") {
    return "passive";
  }
  return "hidden";
}

int GetTabId(const PageNode* page_node) {
  if (content::WebContents* contents = page_node->GetWebContents().get()) {
    if (const TabAndroid* tab = TabAndroid::FromWebContents(contents)) {
      return tab->GetAndroidId();
    }
  }
  return TabAndroid::kInvalidTabId;
}

struct PageRendererPids {
  std::string main_frame_pid = "none";
  std::string subframe_pids;
};

PageRendererPids GetPageRendererPids(const PageNode* page_node) {
  PageRendererPids result;
  base::ProcessId main_frame_pid = base::kNullProcessId;
  if (const FrameNode* main_frame = page_node->GetPrimaryMainFrameNode()) {
    if (const ProcessNode* process_node = main_frame->GetProcessNode()) {
      main_frame_pid = process_node->GetProcessId();
      if (main_frame_pid != base::kNullProcessId) {
        result.main_frame_pid = base::NumberToString(main_frame_pid);
      }
    }
  }

  base::flat_set<base::ProcessId> subframe_pid_set;
  for (const FrameNode* frame_node :
       GraphOperations::GetFrameNodes(page_node)) {
    if (frame_node->IsMainFrame()) {
      continue;
    }
    if (const ProcessNode* process_node = frame_node->GetProcessNode()) {
      base::ProcessId pid = process_node->GetProcessId();
      if (pid != base::kNullProcessId && pid != main_frame_pid) {
        subframe_pid_set.insert(pid);
      }
    }
  }

  std::vector<std::string> subframe_pids;
  subframe_pids.reserve(subframe_pid_set.size());
  for (base::ProcessId pid : subframe_pid_set) {
    subframe_pids.push_back(base::NumberToString(pid));
  }
  result.subframe_pids = base::JoinString(subframe_pids, ", ");
  return result;
}

}  // namespace

std::string DumpTabDiscardsInfo(Graph* graph) {
  if (!graph) {
    return "PerformanceManager graph not available.\n";
  }

  std::vector<TabCandidate> candidates;
  for (const PageNode* page_node : graph->GetAllPageNodes()) {
    if (page_node->GetType() != PageType::kTab) {
      continue;
    }
    std::vector<std::string> reasons =
        GetCannotDiscardReasonsForPageNode(page_node);
    bool can_discard = reasons.empty();
    auto [is_visible, visibility_change_time] = page_node->GetVisibilityState();
    candidates.push_back(TabCandidate{
        .page_node = page_node,
        .cannot_discard_reasons = std::move(reasons),
        .can_discard = can_discard,
        .is_visible = is_visible,
        .is_focused = page_node->IsFocused(),
        .last_visibility_change_time = visibility_change_time,
    });
  }

  // Sorts with ascending importance (most discardable first).
  std::sort(candidates.begin(), candidates.end());

  const base::TimeTicks now = base::TimeTicks::Now();
  std::string output =
      base::StringPrintf("Tab Discards Info (%zu tabs):\n", candidates.size());

  for (const TabCandidate& candidate : candidates) {
    const PageNode* page_node = candidate.page_node;
    int tab_id = GetTabId(page_node);
    std::string_view visibility = GetTabVisibilityString(page_node);
    std::string_view loading_state =
        GetLoadingStateString(page_node->GetLoadingState());
    std::string_view lifecycle_state =
        GetLifecycleStateString(page_node, visibility, loading_state);

    int64_t last_active_seconds = 0;
    if (!candidate.is_visible) {
      base::TimeDelta elapsed = now - candidate.last_visibility_change_time;
      last_active_seconds = std::max<int64_t>(0, elapsed.InSeconds());
    }

    PageRendererPids renderer_pids = GetPageRendererPids(page_node);

    base::StrAppend(
        &output,
        {"  tab_id=", base::NumberToString(tab_id), " visibility=", visibility,
         " loading_state=", loading_state, " lifecycle_state=", lifecycle_state,
         " can_discard=", candidate.can_discard ? "true" : "false",
         " cannot_discard_reasons=[",
         base::JoinString(candidate.cannot_discard_reasons, ", "),
         "] last_active=", base::NumberToString(last_active_seconds),
         "s main_frame_pid=", renderer_pids.main_frame_pid, " subframe_pids=[",
         renderer_pids.subframe_pids, "]\n"});
  }

  return output;
}

static std::string JNI_TabDiscardState_GetTabDiscardsDump(JNIEnv* env) {
  if (!PerformanceManager::IsAvailable()) {
    return "PerformanceManager not available.\n";
  }
  return DumpTabDiscardsInfo(PerformanceManager::GetGraph());
}

}  // namespace discards

DEFINE_JNI(TabDiscardState)
