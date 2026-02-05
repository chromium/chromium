// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_SCRIPT_TOOL_RECEIVER_H_
#define CHROME_BROWSER_ACTOR_ACTOR_SCRIPT_TOOL_RECEIVER_H_

#include "content/public/browser/document_service.h"
#include "third_party/blink/public/mojom/content_extraction/script_tools.mojom.h"

namespace content {
class RenderFrameHost;
}

namespace actor {

class ActorScriptToolReceiver
    : public content::DocumentService<blink::mojom::ScriptToolHost> {
 public:
  static void Create(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::ScriptToolHost> receiver);

  ActorScriptToolReceiver(const ActorScriptToolReceiver&) = delete;
  ActorScriptToolReceiver& operator=(const ActorScriptToolReceiver&) = delete;
  ActorScriptToolReceiver(ActorScriptToolReceiver&&) = delete;
  ActorScriptToolReceiver& operator=(ActorScriptToolReceiver&&) = delete;

  // blink::mojom::ScriptToolHost implementation.
  void PauseExecution() override;

 private:
  ActorScriptToolReceiver(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<blink::mojom::ScriptToolHost> receiver);
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_SCRIPT_TOOL_RECEIVER_H_
