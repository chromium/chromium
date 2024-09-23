// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_DEBUGGER_IMPL_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_DEBUGGER_IMPL_H_

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/media_router/browser/media_router_debugger.h"
#include "components/media_router/common/mojom/debugger.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace media_router {

// An implementation for media router debugging and feedback.
class MediaRouterDebuggerImpl : public MediaRouterDebugger,
                                public mojom::Debugger {
 public:
  // Fetches the MediaRouterDebugger from the media router fetched from the
  // |frame_tree_node_id|. Must be called on the UI Thread. May return a
  // nullptr.
  static MediaRouterDebugger* GetForFrameTreeNode(
      content::FrameTreeNodeId frame_tree_node_id);

  explicit MediaRouterDebuggerImpl(content::BrowserContext* context);

  MediaRouterDebuggerImpl(const MediaRouterDebuggerImpl&) = delete;
  MediaRouterDebuggerImpl& operator=(const MediaRouterDebuggerImpl&) = delete;

  ~MediaRouterDebuggerImpl() override;

  // MediaRouterDebugger implementation:
  base::Value::Dict GetMirroringStats() final;
  void AddObserver(MirroringStatsObserver& obs) final;
  void RemoveObserver(MirroringStatsObserver& obs) final;
  void EnableRtcpReports() final;
  void DisableRtcpReports() final;
  bool ShouldFetchMirroringStats() const final;

  // mojom::Debugger overrides:
  void ShouldFetchMirroringStats(
      ShouldFetchMirroringStatsCallback callback) override;
  void OnMirroringStats(const base::Value json_stats) override;
  void BindReceiver(mojo::PendingReceiver<mojom::Debugger> receiver) override;

 protected:
  friend class MediaRouterDebuggerImplTest;
  FRIEND_TEST_ALL_PREFIXES(MediaRouterDebuggerImplTest,
                           ShouldFetchMirroringStatsFeatureDisabled);

  void NotifyGetMirroringStats(const base::Value::Dict& json_logs);
  void LogMirroringStats();

  base::ObserverList<MirroringStatsObserver> observers_;
  bool is_rtcp_reports_enabled_ = false;
  mojo::ReceiverSet<mojom::Debugger> receivers_;
  base::Value::Dict most_recent_mirroring_stats_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<MediaRouterDebuggerImpl> weak_ptr_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_DEBUGGER_IMPL_H_
