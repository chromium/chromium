// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_RENDER_PROCESS_KEEP_ALIVE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_RENDER_PROCESS_KEEP_ALIVE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace content {
class RenderProcessHost;
}

namespace android_webview {

class AwRenderProcessKeepAlive : public base::SupportsUserData::Data {
 public:
  static AwRenderProcessKeepAlive* GetInstanceForRenderProcessHost(
      content::RenderProcessHost* host);

  AwRenderProcessKeepAlive(const AwRenderProcessKeepAlive&) = delete;
  AwRenderProcessKeepAlive& operator=(const AwRenderProcessKeepAlive&) = delete;

  ~AwRenderProcessKeepAlive() override;

  void AddAwContents();
  void RemoveAwContents();

  bool kept_alive() const { return kept_alive_; }

 private:
  explicit AwRenderProcessKeepAlive(
      content::RenderProcessHost* render_process_host);

  void OnKeepAliveTimerFired();

  raw_ptr<content::RenderProcessHost> render_process_host_;

  int aw_contents_count_ = 0;
  bool kept_alive_ = false;
  base::TimeTicks keep_alive_start_time_;
  base::OneShotTimer keep_alive_timer_;

  base::WeakPtrFactory<AwRenderProcessKeepAlive> weak_factory_{this};
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_RENDER_PROCESS_KEEP_ALIVE_H_
