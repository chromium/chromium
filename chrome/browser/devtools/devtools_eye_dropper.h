// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_EYE_DROPPER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_EYE_DROPPER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/viz/host/client_frame_sink_video_capturer.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace blink {
class WebMouseEvent;
}

class DevToolsEyeDropper : public content::WebContentsObserver,
                           public viz::mojom::FrameSinkVideoConsumer {
 public:
  typedef base::RepeatingCallback<void(int, int, int, int)> EyeDropperCallback;

  DevToolsEyeDropper(content::WebContents* web_contents,
                     EyeDropperCallback callback);

  DevToolsEyeDropper(const DevToolsEyeDropper&) = delete;
  DevToolsEyeDropper& operator=(const DevToolsEyeDropper&) = delete;

  ~DevToolsEyeDropper() override;

 private:
  void AttachToHost(content::RenderFrameHost* host);
  void DetachFromHost();

  // content::WebContentsObserver.
  void RenderFrameCreated(content::RenderFrameHost* host) override;
  void RenderFrameDeleted(content::RenderFrameHost* host) override;
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;

  void ResetFrame();
  void FrameUpdated(const SkBitmap&);
  bool HandleMouseEvent(const blink::WebMouseEvent& event);
  void UpdateCursor();

  // viz::mojom::FrameSinkVideoConsumer implementation.
  void OnFrameCaptured(
      ::media::mojom::VideoBufferHandlePtr data,
      ::media::mojom::VideoFrameInfoPtr info,
      const gfx::Rect& content_rect,
      mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
          callbacks) override;
  void OnNewSubCaptureTargetVersion(
      uint32_t sub_capture_target_version) override;
  void OnFrameWithEmptyRegionCapture() override;
  void OnStopped() override;
  void OnLog(const std::string& /*message*/) override {}

  EyeDropperCallback callback_;
  SkBitmap frame_;
  int last_cursor_x_ = -1;
  int last_cursor_y_ = -1;
  content::RenderWidgetHost::MouseEventCallback mouse_event_callback_;
  raw_ptr<content::RenderWidgetHost> host_ = nullptr;
  std::unique_ptr<viz::ClientFrameSinkVideoCapturer> video_capturer_;
  base::WeakPtrFactory<DevToolsEyeDropper> weak_factory_{this};
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_EYE_DROPPER_H_
