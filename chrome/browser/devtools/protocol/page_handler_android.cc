// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/page_handler.h"

#include "base/check.h"
#include "content/public/browser/web_contents.h"

PageHandler::PageHandler(scoped_refptr<content::DevToolsAgentHost> agent_host,
                         content::WebContents* web_contents,
                         protocol::UberDispatcher* dispatcher,
                         bool is_trusted)
    : agent_host_(agent_host), web_contents_(web_contents->GetWeakPtr()) {
  CHECK(is_trusted, base::NotFatalUntil::M161);
  protocol::Page::Dispatcher::wire(dispatcher, this);
}

PageHandler::~PageHandler() = default;

// Page::Backend requires implementations for every command declared by
// Chrome's embedder-side Page backend. Android only owns PrintToPDF, so the
// remaining commands fall through to Content's Page handler or report that a
// Chrome-only command is unavailable.
protocol::Response PageHandler::Enable(
    std::optional<bool> enable_file_chooser_opened_event) {
  return protocol::Response::FallThrough();
}

protocol::Response PageHandler::Disable() {
  return protocol::Response::FallThrough();
}

protocol::Response PageHandler::SetAdBlockingEnabled(bool enabled) {
  return protocol::Response::FallThrough();
}

protocol::Response PageHandler::SetSPCTransactionMode(
    const protocol::String& mode) {
  return protocol::Response::FallThrough();
}

protocol::Response PageHandler::SetRPHRegistrationMode(
    const protocol::String& mode) {
  return protocol::Response::FallThrough();
}

void PageHandler::GetInstallabilityErrors(
    std::unique_ptr<GetInstallabilityErrorsCallback> callback) {
  callback->fallThrough();
}

void PageHandler::GetManifestIcons(
    std::unique_ptr<GetManifestIconsCallback> callback) {
  callback->fallThrough();
}

void PageHandler::GetAppId(std::unique_ptr<GetAppIdCallback> callback) {
  callback->fallThrough();
}

void PageHandler::GetSubApps(std::unique_ptr<GetSubAppsCallback> callback) {
  callback->fallThrough();
}

void PageHandler::GetSiblingSubApps(
    std::unique_ptr<GetSiblingSubAppsCallback> callback) {
  callback->fallThrough();
}
