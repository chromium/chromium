// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/site_based_search_tool_request.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/tools/navigate_tool_request.h"
#include "chrome/browser/actor/tools/site_based_search_tool.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_delegate.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/core/aggregated_journal.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace actor {

SiteBasedSearchToolRequest::SiteBasedSearchToolRequest(
    ActorSurfaceHandle actor_surface_handle,
    std::string query,
    std::string document_identifier)
    : TabToolRequest(actor_surface_handle),
      query_(std::move(query)),
      document_identifier_(std::move(document_identifier)) {}

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
  ActorSurface* actor_surface = GetActorSurfaceHandle().Get();
  if (!actor_surface) {
    return {/*tool=*/nullptr,
            MakeResult(mojom::ActionResultCode::kTabWentAway,
                       /*requires_page_stabilization=*/false,
                       "The target surface is no longer present.")};
  }
  content::WebContents* web_contents = actor_surface->GetWebContents();

  if (query_.empty()) {
    return {/*tool=*/nullptr,
            MakeResult(mojom::ActionResultCode::kArgumentsInvalid,
                       /*requires_page_stabilization=*/false,
                       "Search query is empty.")};
  }

  if (document_identifier_.empty()) {
    return {/*tool=*/nullptr,
            MakeResult(mojom::ActionResultCode::kArgumentsInvalid,
                       /*requires_page_stabilization=*/false,
                       "Document identifier is empty.")};
  }

  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  CHECK(main_frame);

  const std::string current_doc_id =
      optimization_guide::DocumentIdentifierUserData::
          GetOrCreateForCurrentDocument(main_frame)
              ->serialized_token();
  if (current_doc_id != document_identifier_) {
    return {/*tool=*/nullptr,
            MakeResult(mojom::ActionResultCode::kFrameWentAway,
                       /*requires_page_stabilization=*/false,
                       "Document identifier does not match the current page.")};
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
  const GURL& current_url = web_contents->GetLastCommittedURL();
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
        return NavigateToolRequest(GetActorSurfaceHandle(),
                                   std::move(search_url))
            .CreateTool(task_id, tool_delegate);
      }
    }
  }

  return {
      std::make_unique<SiteBasedSearchTool>(
          task_id, tool_delegate, *actor_surface, query_, document_identifier_),
      MakeOkResult()};
}

void SiteBasedSearchToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view SiteBasedSearchToolRequest::Name() const {
  return kName;
}

}  // namespace actor
