// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/media_router_debugger_impl.h"

#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_router_factory.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_switches.h"
#include "media/cast/constants.h"

namespace media_router {

MediaRouterDebuggerImpl::MediaRouterDebuggerImpl() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}
MediaRouterDebuggerImpl::~MediaRouterDebuggerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static.
MediaRouterDebugger* MediaRouterDebuggerImpl::GetForFrameTreeNode(
    int frame_tree_node_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  if (!web_contents) {
    return nullptr;
  }

  auto* media_router = MediaRouterFactory::GetApiForBrowserContextIfExists(
      web_contents->GetBrowserContext());

  return media_router ? &media_router->GetDebugger() : nullptr;
}

void MediaRouterDebuggerImpl::EnableRtcpReports() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_rtcp_reports_enabled_ = true;
}

void MediaRouterDebuggerImpl::DisableRtcpReports() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_rtcp_reports_enabled_ = false;
}

bool MediaRouterDebuggerImpl::ShouldFetchMirroringStats() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_rtcp_reports_enabled_ ||
         base::FeatureList::IsEnabled(media::kEnableRtcpReporting);
}

void MediaRouterDebuggerImpl::AddObserver(MirroringStatsObserver& obs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(&obs);
}

void MediaRouterDebuggerImpl::RemoveObserver(MirroringStatsObserver& obs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(&obs);
}

void MediaRouterDebuggerImpl::ShouldFetchMirroringStats(
    ShouldFetchMirroringStatsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(ShouldFetchMirroringStats());
}

void MediaRouterDebuggerImpl::OnMirroringStats(const base::Value json_stats) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  json_stats.is_dict() ? NotifyGetMirroringStats(json_stats.GetDict())
                       : NotifyGetMirroringStats(base::Value::Dict());
}

void MediaRouterDebuggerImpl::BindReceiver(
    mojo::PendingReceiver<mojom::Debugger> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receivers_.Add(this, std::move(receiver));
}

void MediaRouterDebuggerImpl::NotifyGetMirroringStats(
    const base::Value::Dict& json_logs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!ShouldFetchMirroringStats()) {
    return;
  }
  for (MirroringStatsObserver& observer : observers_) {
    observer.OnMirroringStatsUpdated(json_logs);
  }
}

}  // namespace media_router
