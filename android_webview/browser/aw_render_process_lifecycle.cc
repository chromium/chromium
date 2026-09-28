// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_render_process_lifecycle.h"

#include "android_webview/browser/aw_render_process_keep_alive.h"
#include "base/notreached.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

namespace android_webview {

std::string_view AwRenderProcessLifecycleStateToString(
    AwRenderProcessLifecycleState state) {
  switch (state) {
    case AwRenderProcessLifecycleState::kSpareNeverUsed:
      return "SpareNeverUsed";
    case AwRenderProcessLifecycleState::kSpareKeptAlive:
      return "SpareKeptAlive";
    case AwRenderProcessLifecycleState::kActive:
      return "Active";
    case AwRenderProcessLifecycleState::kUnknown:
      return "Unknown";
  }
  NOTREACHED();
}

AwRenderProcessLifecycleState GetAwRenderProcessLifecycleState(
    content::RenderProcessHost* host) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!host) {
    return AwRenderProcessLifecycleState::kUnknown;
  }

  if (host->IsSpare()) {
    return AwRenderProcessLifecycleState::kSpareNeverUsed;
  }

  AwRenderProcessKeepAlive* keep_alive =
      AwRenderProcessKeepAlive::GetInstanceForRenderProcessHost(host);
  if (keep_alive->kept_alive() && !keep_alive->has_aw_contents()) {
    return AwRenderProcessLifecycleState::kSpareKeptAlive;
  }

  return AwRenderProcessLifecycleState::kActive;
}

std::string_view GetAwRenderProcessLifecycleStateString(
    content::RenderProcessHost* host) {
  return AwRenderProcessLifecycleStateToString(
      GetAwRenderProcessLifecycleState(host));
}

}  // namespace android_webview
