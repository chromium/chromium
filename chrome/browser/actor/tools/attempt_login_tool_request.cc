// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/attempt_login_tool_request.h"

#include <optional>

#include "chrome/browser/actor/tools/attempt_login_tool.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor/action_result.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/common/content_features.h"

namespace actor {

AttemptLoginToolRequest::AttemptLoginToolRequest(
    tabs::TabHandle tab_handle,
    std::optional<PageTarget> password_button,
    std::optional<PageTarget> sign_in_with_google_button)
    : TabToolRequest(tab_handle),
      password_button_(password_button),
      sign_in_with_google_button_(sign_in_with_google_button) {}

AttemptLoginToolRequest::~AttemptLoginToolRequest() = default;

AttemptLoginToolRequest::AttemptLoginToolRequest(
    const AttemptLoginToolRequest&) = default;
AttemptLoginToolRequest& AttemptLoginToolRequest::operator=(
    const AttemptLoginToolRequest&) = default;

ToolRequest::CreateToolResult AttemptLoginToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  tabs::TabInterface* tab = GetTabHandle().Get();
  if (!tab) {
    return {/*tool=*/nullptr, MakeResult(mojom::ActionResultCode::kTabWentAway,
                                         /*requires_page_stabilization=*/false,
                                         "The tab is no longer present.")};
  }

  return {std::make_unique<AttemptLoginTool>(task_id, tool_delegate, *tab,
                                             password_button_,
                                             sign_in_with_google_button_),
          MakeOkResult()};
}

void AttemptLoginToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view AttemptLoginToolRequest::Name() const {
  return kName;
}

bool AttemptLoginToolRequest::RequiresOpeningWebContents() const {
  return base::FeatureList::IsEnabled(
             password_manager::features::kActorLoginFederatedLoginSupport) &&
         base::FeatureList::IsEnabled(features::kFedCmEmbedderInitiatedLogin) &&
         base::FeatureList::IsEnabled(features::kFedCmNavigationInterception);
}

}  // namespace actor
