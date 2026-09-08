// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/perform_search_tool_request.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_delegate.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "url/gurl.h"

namespace actor {

using ::tabs::TabHandle;

PerformSearchToolRequest::PerformSearchToolRequest(TabHandle tab_handle,
                                                   std::string query)
    : TabToolRequest(tab_handle), query_(std::move(query)) {}

PerformSearchToolRequest::~PerformSearchToolRequest() = default;

// Uses the navigate tool, which is tab scoped, but navigates away from the
// current URL.
bool PerformSearchToolRequest::RequiresUrlCheckInCurrentTab() const {
  return false;
}

void PerformSearchToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

ToolRequest::CreateToolResult PerformSearchToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  if (query_.empty()) {
    return {/*tool=*/nullptr,
            MakeResult(mojom::ActionResultCode::kArgumentsInvalid,
                       /*requires_page_stabilization=*/false,
                       "Search query is empty.")};
  }

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(&tool_delegate.GetProfile());
  if (!template_url_service) {
    return {/*tool=*/nullptr,
            MakeResult(mojom::ActionResultCode::kSearchServiceUnavailable,
                       /*requires_page_stabilization=*/false,
                       "TemplateURLService is unavailable.")};
  }

  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  if (!default_provider) {
    return {/*tool=*/nullptr,
            MakeResult(mojom::ActionResultCode::kDefaultSearchProviderNotSet,
                       /*requires_page_stabilization=*/false,
                       "Default search provider is not set.")};
  }

  GURL search_url = default_provider->GenerateSearchURL(
      template_url_service->search_terms_data(), base::UTF8ToUTF16(query_));

  return NavigateToolRequest(GetTabHandle(), std::move(search_url))
      .CreateTool(task_id, tool_delegate);
}

std::string_view PerformSearchToolRequest::Name() const {
  return kName;
}

}  // namespace actor
