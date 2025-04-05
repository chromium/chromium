// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/browser_handler_android.h"

#include <set>
#include <vector>

#include "chrome/browser/android/devtools_manager_delegate_android.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"

using protocol::Response;

namespace {
static constexpr char kNotImplemented[] = "Not implemented";
}  // namespace

BrowserHandlerAndroid::BrowserHandlerAndroid(
    protocol::UberDispatcher* dispatcher,
    const std::string& target_id)
    : target_id_(target_id) {
  CHECK(dispatcher);
  protocol::Browser::Dispatcher::wire(dispatcher, this);
}

BrowserHandlerAndroid::~BrowserHandlerAndroid() = default;

Response BrowserHandlerAndroid::GetWindowForTarget(
    std::optional<std::string> target_id,
    int* out_window_id,
    std::unique_ptr<protocol::Browser::Bounds>* out_bounds) {
  auto host =
      content::DevToolsAgentHost::GetForId(target_id.value_or(target_id_));
  if (!host) {
    return Response::ServerError("No matching target");
  }
  content::WebContents* web_contents = host->GetWebContents();
  if (!web_contents) {
    return Response::ServerError("No web contents in the target");
  }

  for (TabModel* model : TabModelList::models()) {
    for (int i = 0; i < model->GetTabCount(); ++i) {
      TabAndroid* tab = model->GetTabAt(i);
      if (tab->web_contents() == web_contents) {
        *out_window_id = tab->GetWindowId().id();
        return Response::Success();
      }
    }
  }

  return Response::ServerError("Browser window not found");
}

Response BrowserHandlerAndroid::GetWindowBounds(
    int window_id,
    std::unique_ptr<protocol::Browser::Bounds>* out_bounds) {
  return Response::MethodNotFound(kNotImplemented);
}

Response BrowserHandlerAndroid::Close() {
  return Response::MethodNotFound(kNotImplemented);
}

Response BrowserHandlerAndroid::SetWindowBounds(
    int window_id,
    std::unique_ptr<protocol::Browser::Bounds> window_bounds) {
  return Response::MethodNotFound(kNotImplemented);
}

protocol::Response BrowserHandlerAndroid::SetDockTile(
    std::optional<std::string> label,
    std::optional<protocol::Binary> image) {
  return Response::MethodNotFound(kNotImplemented);
}

protocol::Response BrowserHandlerAndroid::ExecuteBrowserCommand(
    const protocol::Browser::BrowserCommandId& command_id) {
  return Response::MethodNotFound(kNotImplemented);
}

protocol::Response BrowserHandlerAndroid::AddPrivacySandboxEnrollmentOverride(
    const std::string& in_url) {
  return Response::MethodNotFound(kNotImplemented);
}
