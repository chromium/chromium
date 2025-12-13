// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/typing_helper.h"

#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

TypingHelper::TypingHelper(content::WebContents* web_contents,
                           int dom_node_id,
                           const std::u16string& value,
                           TypingResult callback)
    : dom_node_id_(dom_node_id), callback_(std::move(callback)) {
  auto invocation = actor::mojom::ToolInvocation::New();
  auto type = actor::mojom::TypeAction::New();
  type->text = base::UTF16ToUTF8(value);
  type->mode = actor::mojom::TypeAction::Mode::kDeleteExisting;
  invocation->action = actor::mojom::ToolAction::NewType(std::move(type));
  invocation->target = actor::mojom::ToolTarget::NewDomNodeId(dom_node_id_);

  web_contents->GetPrimaryMainFrame()
      ->GetRemoteAssociatedInterfaces()
      ->GetInterface(&chrome_render_frame_);
  chrome_render_frame_->InvokeTool(
      std::move(invocation),
      base::BindOnce(&TypingHelper::OnTyped, weak_ptr_factory_.GetWeakPtr()));
}

TypingHelper::~TypingHelper() = default;

void TypingHelper::OnTyped(actor::mojom::ActionResultPtr result) {
  CHECK(callback_);
  std::move(callback_).Run(result->code);
}
