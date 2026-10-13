// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/site_based_search_tool_request.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/actor/tools/navigate_tool_request.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_delegate.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/core/aggregated_journal.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace actor {

SiteBasedSearchToolRequest::SiteBasedSearchToolRequest(
    tabs::TabHandle tab_handle,
    std::string query)
    : TabToolRequest(tab_handle), query_(std::move(query)) {}

SiteBasedSearchToolRequest::~SiteBasedSearchToolRequest() = default;

SiteBasedSearchToolRequest::SiteBasedSearchToolRequest(
    const SiteBasedSearchToolRequest&) = default;
SiteBasedSearchToolRequest& SiteBasedSearchToolRequest::operator=(
    const SiteBasedSearchToolRequest&) = default;

bool SiteBasedSearchToolRequest::RequiresUrlCheckInCurrentTab() const {
  return true;
}

ToolRequest::CreateToolResult SiteBasedSearchToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  tabs::TabInterface* tab = GetTabHandle().Get();
  if (!tab || !tab->GetContents()) {
    return {/*tool=*/nullptr, MakeResult(mojom::ActionResultCode::kTabWentAway,
                                         /*requires_page_stabilization=*/false,
                                         "The tab is no longer present.")};
  }

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

  // If a search URL with replaceable search terms exists, then this is now a
  // navigation action.
  const GURL& current_url = tab->GetContents()->GetLastCommittedURL();
  if (current_url.is_valid() && current_url.SchemeIsHTTPOrHTTPS()) {
    const std::u16string keyword = TemplateURL::GenerateKeyword(current_url);
    const TemplateURL* turl =
        keyword.empty()
            ? nullptr
            : template_url_service->GetTemplateURLForKeyword(keyword);
    if (!turl) {
      turl = template_url_service->GetTemplateURLForHost(current_url.GetHost());
    }
    if (!turl) {
      tool_delegate.GetJournal().Log(
          GetURLForJournal(), task_id, "SiteBasedSearchTemplateURLNotFound",
          JournalDetailsBuilder().Add("host", current_url.GetHost()).Build());
    }

    const bool turl_supports_search_term_replacement =
        turl && turl->type() == TemplateURL::NORMAL &&
        turl->url_ref().SupportsReplacement(
            template_url_service->search_terms_data());
    if (turl_supports_search_term_replacement) {
      GURL search_url = turl->GenerateSearchURL(
          template_url_service->search_terms_data(), base::UTF8ToUTF16(query_));
      CHECK(search_url.GetHost() == current_url.GetHost() ||
            (!keyword.empty() && turl->keyword() == keyword));
      if (search_url.is_valid() && search_url.SchemeIsHTTPOrHTTPS()) {
        return NavigateToolRequest(GetTabHandle(), std::move(search_url))
            .CreateTool(task_id, tool_delegate);
      }
    }
  }

  NOTIMPLEMENTED();
  return {/*tool=*/nullptr, MakeResult(mojom::ActionResultCode::kNotImplemented,
                                       /*requires_page_stabilization=*/false,
                                       "Not implemented.")};
}

void SiteBasedSearchToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view SiteBasedSearchToolRequest::Name() const {
  return kName;
}

}  // namespace actor
