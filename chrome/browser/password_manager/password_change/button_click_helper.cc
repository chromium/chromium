// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/button_click_helper.h"

#include "base/strings/to_string.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace {

using Logger = password_manager::BrowserSavePasswordProgressLogger;

std::unique_ptr<Logger> GetLoggerIfAvailable(
    password_manager::PasswordManagerClient* client) {
  if (!client) {
    return nullptr;
  }

  autofill::LogManager* log_manager = client->GetCurrentLogManager();
  if (log_manager && log_manager->IsLoggingActive()) {
    return std::make_unique<Logger>(log_manager);
  }

  return nullptr;
}

}  // namespace

ButtonClickHelper::ButtonClickHelper(
    content::WebContents* web_contents,
    password_manager::PasswordManagerClient* client,
    int dom_node_id,
    ClickResult callback)
    : callback_(std::move(callback)), client_(client) {
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
  if (auto logger = GetLoggerIfAvailable(client_)) {
    logger->LogNumber(
        Logger::STRING_AUTOMATED_PASSWORD_CHANGE_DOM_NODE_ID_TO_CLICK,
        dom_node_id);
  }
}

ButtonClickHelper::~ButtonClickHelper() = default;

void ButtonClickHelper::OnButtonClicked(actor::mojom::ActionResultPtr result) {
  if (auto logger = GetLoggerIfAvailable(client_)) {
    logger->LogString(
        Logger::STRING_AUTOMATED_PASSWORD_CHANGE_BUTTON_CLICK_ACTION_RESULT,
        base::ToString(result->code));
  }
  CHECK(callback_);
  std::move(callback_).Run(result->code);
}
