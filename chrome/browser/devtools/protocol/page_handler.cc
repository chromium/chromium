// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/page_handler.h"

#include "chrome/browser/installable/installable_manager.h"
#include "chrome/browser/subresource_filter/chrome_subresource_filter_client.h"

PageHandler::PageHandler(content::WebContents* web_contents,
                         protocol::UberDispatcher* dispatcher)
    : content::WebContentsObserver(web_contents) {
  DCHECK(web_contents);
  protocol::Page::Dispatcher::wire(dispatcher, this);
}

PageHandler::~PageHandler() {
  ToggleAdBlocking(false /* enabled */);
}

void PageHandler::ToggleAdBlocking(bool enabled) {
  if (!web_contents())
    return;
  if (auto* client =
          ChromeSubresourceFilterClient::FromWebContents(web_contents())) {
    client->ToggleForceActivationInCurrentWebContents(enabled);
  }
}

protocol::Response PageHandler::Enable() {
  enabled_ = true;
  // Do not mark the command as handled. Let it fall through instead, so that
  // the handler in content gets a chance to process the command.
  return protocol::Response::FallThrough();
}

protocol::Response PageHandler::Disable() {
  enabled_ = false;
  ToggleAdBlocking(false /* enable */);
  // Do not mark the command as handled. Let it fall through instead, so that
  // the handler in content gets a chance to process the command.
  return protocol::Response::FallThrough();
}

protocol::Response PageHandler::SetAdBlockingEnabled(bool enabled) {
  if (!enabled_)
    return protocol::Response::Error("Page domain is disabled.");
  ToggleAdBlocking(enabled);
  return protocol::Response::OK();
}

void PageHandler::GetInstallabilityErrors(
    std::unique_ptr<GetInstallabilityErrorsCallback> callback) {
  auto errors = std::make_unique<protocol::Array<std::string>>();
  InstallableManager* manager =
      web_contents() ? InstallableManager::FromWebContents(web_contents())
                     : nullptr;
  if (!manager) {
    callback->sendFailure(
        protocol::Response::Error("Unable to fetch errors for target"));
    return;
  }
  manager->GetAllErrors(base::BindOnce(&PageHandler::GotInstallabilityErrors,
                                       std::move(callback)));
}

// static
void PageHandler::GotInstallabilityErrors(
    std::unique_ptr<GetInstallabilityErrorsCallback> callback,
    std::vector<std::string> errors) {
  callback->sendSuccess(
      std::make_unique<protocol::Array<std::string>>(std::move(errors)));
}
