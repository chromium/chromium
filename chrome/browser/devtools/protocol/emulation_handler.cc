// Copyright 2022 The Chromium Authors
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
EmulationHandler::~EmulationHandler() {
  SetAutomationOverride(false);
}

protocol::Response EmulationHandler::Disable() {
  SetAutomationOverride(false);
  return protocol::Response::FallThrough();
}

protocol::Response EmulationHandler::SetAutomationOverride(bool enabled) {
  if (!enabled) {
    if (automation_info_bar_) {
      automation_info_bar_->RemoveSelf();
    }
    return protocol::Response::FallThrough();
  }
  if (automation_info_bar_) {
    return protocol::Response::FallThrough();
  }

  infobars::ContentInfoBarManager* info_bar_manager =
      GetContentInfoBarManager();
  if (!info_bar_manager) {
    // Implies the web content cannot have an info bar attached. A priori, the
    // automation override doesn't matter on the chrome layer.
    return protocol::Response::FallThrough();
  }

  // Note since the observer removes itself when the info bar is removed, the
  // observer is added at most once because of the info bar nullity check
  // above.
  automation_info_bar_ = AutomationInfoBarDelegate::Create(info_bar_manager);
  if (automation_info_bar_) {
    info_bar_manager->AddObserver(this);
  }
  return protocol::Response::FallThrough();
}

infobars::ContentInfoBarManager* EmulationHandler::GetContentInfoBarManager() {
  content::WebContents* web_contents = agent_host_->GetWebContents();
  if (!web_contents) {
    return nullptr;
  }
  return infobars::ContentInfoBarManager::FromWebContents(
      web_contents->GetOutermostWebContents());
}

void EmulationHandler::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                        bool animate) {
  if (automation_info_bar_ == infobar) {
    infobar->owner()->RemoveObserver(this);
    automation_info_bar_ = nullptr;
  }
}
