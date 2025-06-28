// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/button_click_helper.h"

#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

ButtonClickHelper::ButtonClickHelper(content::WebContents* web_contents,
                                     int dom_node_id,
                                     ClickResult callback)
    : callback_(std::move(callback)) {
  auto invocation = actor::mojom::ToolInvocation::New();
  auto click = actor::mojom::ClickAction::New();
  click->type = actor::mojom::ClickAction::Type::kLeft;
  click->count = actor::mojom::ClickAction::Count::kSingle;
  invocation->action = actor::mojom::ToolAction::NewClick(std::move(click));
  invocation->target = actor::mojom::ToolTarget::NewDomNodeId(dom_node_id);

  web_contents->GetPrimaryMainFrame()
      ->GetRemoteAssociatedInterfaces()
      ->GetInterface(&chrome_render_frame_);
  chrome_render_frame_->InvokeTool(
      std::move(invocation), base::BindOnce(&ButtonClickHelper::OnButtonClicked,
                                            weak_ptr_factory_.GetWeakPtr()));
}

ButtonClickHelper::~ButtonClickHelper() = default;

void ButtonClickHelper::OnButtonClicked(actor::mojom::ActionResultPtr result) {
  CHECK(callback_);
  std::move(callback_).Run(actor::IsOk(*result));
}
