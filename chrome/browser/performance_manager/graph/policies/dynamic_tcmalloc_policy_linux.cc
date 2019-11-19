// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/policies/dynamic_tcmalloc_policy_linux.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bits.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_metrics.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/graph/policies/policy_features.h"
#include "chrome/common/performance_manager/mojom/tcmalloc.mojom.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/node_attached_data.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "components/performance_manager/render_process_user_data.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/gurl.h"

namespace performance_manager {
namespace policies {

namespace {
// kMaxOverallThreadCacheSizeMB is the default overall thread cache size for
// tcmalloc and kMinOverallThreadCacheSizeMB is the size that would be used in
// tcmalloc SMALL_BUT_SLOW mode. This policy will adjust the thread cache size
// between these two values.
constexpr uint32_t kMinOverallThreadCacheSizeMB = 4 << 20;  // 4MB
constexpr uint32_t kMaxOverallThreadCacheSizeMB =
    8 * kMinOverallThreadCacheSizeMB;  // 32 MB

class DynamicTcmallocData
    : public ExternalNodeAttachedDataImpl<DynamicTcmallocData> {
 public:
  explicit DynamicTcmallocData(const ProcessNode* node) {}
  ~DynamicTcmallocData() override = default;

  mojo::Remote<tcmalloc::mojom::TcmallocTunables> tcmalloc_tunables;
};

void BindTcmallocTunablesReceiverOnUIThread(
    RenderProcessHostProxy proxy,
    mojo::PendingReceiver<tcmalloc::mojom::TcmallocTunables> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::RenderProcessHost* render_process_host = proxy.Get();
  if (!render_process_host) {
    return;
  }

  render_process_host->BindReceiver(std::move(receiver));
}

}  // namespace

DynamicTcmallocPolicy::DynamicTcmallocPolicy() = default;
DynamicTcmallocPolicy::~DynamicTcmallocPolicy() = default;

void DynamicTcmallocPolicy::OnPassedToGraph(Graph* graph) {
  graph_ = graph;
  timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(features::kDynamicTuningTimeSec.Get()),
      base::BindRepeating(&DynamicTcmallocPolicy::CheckAndUpdateTunables,
                          base::Unretained(this)));
}

void DynamicTcmallocPolicy::OnTakenFromGraph(Graph* graph) {
  timer_.Stop();
  graph_ = nullptr;
}

float DynamicTcmallocPolicy::CalculateFreeMemoryRatio() {
  base::SystemMemoryInfoKB info;
  CHECK(base::GetSystemMemoryInfo(&info));

  float memory_available = static_cast<float>(info.free + info.swap_free) /
                           (info.total + info.swap_total);
  return memory_available;
}

mojo::Remote<tcmalloc::mojom::TcmallocTunables>*
DynamicTcmallocPolicy::EnsureTcmallocTunablesForProcess(
    const ProcessNode* process_node) {
  auto* data = DynamicTcmallocData::Get(process_node);

  // We will lazily create and bind the mojo::Remote.
  if (!data) {
    data = DynamicTcmallocData::GetOrCreate(process_node);

    const RenderProcessHostProxy& proxy =
        process_node->GetRenderProcessHostProxy();

    base::PostTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&BindTcmallocTunablesReceiverOnUIThread, proxy,
                       data->tcmalloc_tunables.BindNewPipeAndPassReceiver()));
  }

  return &data->tcmalloc_tunables;
}

void DynamicTcmallocPolicy::CheckAndUpdateTunables() {
  float memory_available = CalculateFreeMemoryRatio();

  // We start by calculating the percentage of "free" memory as defined by free
  // physical memory and free swap. This percentage is then used to choose a
  // thread cache size between kMinOverallThreadCacheSizeMB and
  // kMaxOverallThreadCacheSizeMB. Using this value, we will additionally
  // examine the main frame and if it has been invisible for more than some
  // amount of time we will scale down its thread cache size further.
  static const size_t kPageSizeBytes = base::GetPageSize();

  // This value can take the range of [(4 << 20), (4 << 20) * 8] which are the
  // values between kMinOverallThreadCacheSizeMB and
  // kMaxOverallThreadCacheSizeMB because memory available has the range [0, 1].
  uint32_t base_size_mb = kMaxOverallThreadCacheSizeMB * memory_available;

  for (const ProcessNode* process_node : graph_->GetAllProcessNodes()) {
    if (process_node->GetProcess().IsValid()) {
      uint32_t node_size_mb = base_size_mb;

      const auto& frame_nodes = process_node->GetFrameNodes();
      if (frame_nodes.empty()) {
        // We skip any ProcessNode which isn't a renderer.
        continue;
      }

      // If we have enabled invisible frame scaling and only if we have more
      // room to reduce we will see if we should scale back more based on the
      // frame visibility state. We can never fall below the minimum so just
      // check before examining all the frames that the invisible scale factor
      // 75% of the base would still be greater than the min.
      constexpr float kInvisibleScaleFactor = 0.75;
      if (features::kDynamicTuningScaleInvisibleTimeSec.Get() >= 0 &&
          (base_size_mb * kInvisibleScaleFactor) >=
              kMinOverallThreadCacheSizeMB) {
        // We will examine the frame nodes to find the main frame, if it hasn't
        // been visible for some time we will also scale down the provisioned
        // thread cache size. Because there can be multiple main frames we need
        // to make sure all of them are invisible for longer than our cutoff
        // time.
        bool all_main_frames_invisible = true;
        for (const auto* frame_node : frame_nodes) {
          if (!frame_node->IsMainFrame())
            continue;

          // We've found the main frame, let's decide if we're going to scale
          // back its thread cache size more if it's invisible.
          auto* page_node = frame_node->GetPageNode();

          // If the node is visible we can just stop now.
          if (page_node->IsVisible()) {
            all_main_frames_invisible = false;
            break;
          }

          base::TimeDelta last_visibility_change =
              page_node->GetTimeSinceLastVisibilityChange();

          // At this point as long as we've been invisible for more than the
          // invisible time cutoff we will reduce the overall thread cache
          // size for that ProcessNode to 75%.
          if (last_visibility_change <
              base::TimeDelta::FromSeconds(
                  features::kDynamicTuningScaleInvisibleTimeSec.Get())) {
            // This frame is invisible but not for long enough so we cannot
            // scale any further.
            all_main_frames_invisible = false;
            break;
          }
        }

        // All main frames were invisible for longer than the cutoff, scale it
        // down to 75% of the systemwide overall thread cache size.
        if (all_main_frames_invisible) {
          node_size_mb *= kInvisibleScaleFactor;
        }
      }

      // Always page align the value that we determined and never let it drop
      // below the minimum.
      node_size_mb = base::bits::Align(
          std::max(node_size_mb, kMinOverallThreadCacheSizeMB), kPageSizeBytes);

      VLOG(1) << "SetMaxTotalThreadCacheBytes=" << node_size_mb;
      (*EnsureTcmallocTunablesForProcess(process_node))
          ->SetMaxTotalThreadCacheBytes(node_size_mb);
    }
  }
}

}  // namespace policies
}  // namespace performance_manager
