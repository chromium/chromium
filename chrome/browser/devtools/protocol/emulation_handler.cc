// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/devtools/protocol/emulation_handler.h"
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "chrome/browser/ui/startup/automation_infobar_delegate.h"

EmulationHandler::EmulationHandler(content::DevToolsAgentHost* agent_host,
                                   protocol::UberDispatcher* dispatcher)
    : agent_host_(agent_host) {
  protocol::Emulation::Dispatcher::wire(dispatcher, this);
}

protocol::Response EmulationHandler::Disable() {
  SetAutomationOverride(false);
  return protocol::Response::FallThrough();
}

protocol::Response EmulationHandler::SetAutomationOverride(bool enabled) {
  // Fallthrough requests when the override is already enabled.
  if (enabled && automation_info_bar_)
    return protocol::Response::FallThrough();

  infobars::ContentInfoBarManager* info_bar_manager = nullptr;
  content::WebContents* web_contents = agent_host_->GetWebContents();
  if (web_contents) {
    info_bar_manager = infobars::ContentInfoBarManager::FromWebContents(
        web_contents->GetOutermostWebContents());
  }
  if (!info_bar_manager) {
    // Implies the web content cannot have an info bar attached. A priori, the
    // automation override doesn't matter on the chrome layer.
    return protocol::Response::FallThrough();
  }

  if (!enabled) {
    if (automation_info_bar_) {
      info_bar_manager->RemoveInfoBar(automation_info_bar_);
      automation_info_bar_ = nullptr;
    }
    return protocol::Response::FallThrough();
  }

  automation_info_bar_ = AutomationInfoBarDelegate::Create(info_bar_manager);
  return protocol::Response::FallThrough();
}
