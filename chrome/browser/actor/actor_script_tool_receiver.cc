// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_script_tool_receiver.h"

#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace actor {

// static
void ActorScriptToolReceiver::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::ScriptToolHost> receiver) {
  new ActorScriptToolReceiver(*render_frame_host, std::move(receiver));
}

ActorScriptToolReceiver::ActorScriptToolReceiver(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<blink::mojom::ScriptToolHost> receiver)
    : DocumentService(render_frame_host, std::move(receiver)) {}

void ActorScriptToolReceiver::PauseExecution() {
  auto* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  if (!web_contents) {
    return;
  }

  auto* service = ActorKeyedService::Get(web_contents->GetBrowserContext());
  if (!service) {
    return;
  }

  if (const auto* task =
          service->GetActingActorTaskForWebContents(web_contents)) {
    if (auto* mutable_task = service->GetTask(task->id())) {
      mutable_task->Pause(/*from_actor=*/true,
                          /*cancel_existing_action=*/false);
    }
  }
}

}  // namespace actor
