// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/target_handler_android.h"

#include "chrome/browser/android/devtools_manager_delegate_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "url/url_constants.h"

using content::WebContents;

TargetHandlerAndroid::TargetHandlerAndroid(protocol::UberDispatcher* dispatcher,
                                           bool is_trusted,
                                           bool may_read_local_files)
    : is_trusted_(is_trusted), may_read_local_files_(may_read_local_files) {
  protocol::Target::Dispatcher::wire(dispatcher, this);
}

TargetHandlerAndroid::~TargetHandlerAndroid() = default;

protocol::Response TargetHandlerAndroid::SetRemoteLocations(
    std::unique_ptr<protocol::Array<protocol::Target::RemoteLocation>>
        locations) {
  remote_locations_.clear();
  if (!locations) {
    return protocol::Response::Success();
  }

  for (const auto& location : *locations) {
    remote_locations_.insert(
        net::HostPortPair(location->GetHost(), location->GetPort()));
  }

  return protocol::Response::Success();
}

protocol::Response TargetHandlerAndroid::CreateTarget(
    const std::string& url,
    std::optional<int> left,
    std::optional<int> top,
    std::optional<int> width,
    std::optional<int> height,
    std::optional<std::string> window_state,
    std::optional<std::string> browser_context_id,
    std::optional<bool> enable_begin_frame_control,
    std::optional<bool> new_window,
    std::optional<bool> background,
    std::optional<bool> for_tab,
    std::optional<bool> hidden,
    std::optional<bool> focus,
    std::string* out_target_id) {
  const TabModelList::TabModelVector& models = TabModelList::models();
  if (models.empty()) {
    return protocol::Response::ServerError("Could not find TabModelList");
  }

  TabModel* tab_model = models[0];
  CHECK(tab_model);

  GURL gurl(url);
  if (gurl.is_empty()) {
    gurl = GURL(url::kAboutBlankURL);
  }

  GURL inner_url = gurl;
  if (gurl.SchemeIs(content::kViewSourceScheme)) {
    inner_url = GURL(gurl.GetContent());
  }

  if (!is_trusted_ && (inner_url.SchemeIs(content::kChromeUIUntrustedScheme) ||
                       inner_url.SchemeIs(content::kChromeDevToolsScheme))) {
    return protocol::Response::ServerError(
        "Navigating to a URL with a privileged scheme is not allowed");
  }

  if (!may_read_local_files_ && inner_url.SchemeIsFile()) {
    return protocol::Response::ServerError(
        "Creating a target with a local URL is not allowed");
  }

  WebContents* web_contents =
      tab_model->CreateNewTabForDevTools(gurl, new_window.value_or(false));
  if (!web_contents) {
    return protocol::Response::ServerError("Could not create a Tab");
  }

  DevToolsManagerDelegateAndroid::MarkCreatedByDevTools(*web_contents);

  if (for_tab.value_or(false)) {
    *out_target_id =
        content::DevToolsAgentHost::GetOrCreateForTab(web_contents)->GetId();
  } else {
    *out_target_id =
        content::DevToolsAgentHost::GetOrCreateFor(web_contents)->GetId();
  }

  return protocol::Response::Success();
}
