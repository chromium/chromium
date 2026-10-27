// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/perform_search_tool_request.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/expected.h"
#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/tools/navigate_tool_request.h"
#include "chrome/browser/actor/tools/registry/tool_definition.h"
#include "chrome/browser/actor/tools/registry/tool_definition_builder.h"
#include "chrome/browser/actor/tools/registry/tool_schema_builder.h"
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
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace actor {

namespace {

// Default description for the `perform_search` tool.
constexpr std::string_view kPerformSearchToolDescription =
    "Search using the default search engine.";

// Description for the `query` parameter of the `perform_search` tool.
constexpr std::string_view kQueryParamDescription = "The terms to search for.";

base::expected<GURL, mojom::ActionResultPtr> ResolveSearchUrl(
    Profile& profile,
    std::string_view query) {
  const std::string_view trimmed_query =
      base::TrimWhitespaceASCII(query, base::TRIM_ALL);
  if (trimmed_query.empty()) {
    return base::unexpected(MakeResult(
        mojom::ActionResultCode::kArgumentsInvalid,
        /*requires_page_stabilization=*/false, "Search query is empty."));
  }

  const TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(&profile);
  if (!template_url_service) {
    return base::unexpected(
        MakeResult(mojom::ActionResultCode::kSearchServiceUnavailable,
                   /*requires_page_stabilization=*/false,
                   "TemplateURLService is unavailable."));
  }

  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  if (!default_provider) {
    return base::unexpected(
        MakeResult(mojom::ActionResultCode::kDefaultSearchProviderNotSet,
                   /*requires_page_stabilization=*/false,
                   "Default search provider is not set."));
  }

  if (!default_provider->url_ref().SupportsReplacement(
          template_url_service->search_terms_data())) {
    return base::unexpected(
        MakeResult(mojom::ActionResultCode::kSearchUrlGenerationFailed,
                   /*requires_page_stabilization=*/false,
                   "Default search provider does not support search term "
                   "replacement."));
  }

  GURL search_url = default_provider->GenerateSearchURL(
      template_url_service->search_terms_data(),
      base::UTF8ToUTF16(trimmed_query));
  if (!search_url.is_valid()) {
    return base::unexpected(
        MakeResult(mojom::ActionResultCode::kSearchUrlGenerationFailed,
                   /*requires_page_stabilization=*/false,
                   "Default search provider generated an invalid search URL."));
  }

  return search_url;
}

}  // namespace

PerformSearchToolRequest::PerformSearchToolRequest(
    ActorSurfaceHandle actor_surface_handle,
    std::string query)
    : TabToolRequest(actor_surface_handle), query_(std::move(query)) {}

PerformSearchToolRequest::~PerformSearchToolRequest() = default;

// static
std::optional<ToolDefinition> PerformSearchToolRequest::GetToolDefinition() {
  return ToolDefinitionBuilder(ToolId::kPerformSearch, kModelFacingName,
                               kPerformSearchToolDescription)
      .SetToolParameterSchema(ToolSchemaBuilder().AddStringProperty(
          kQueryParam, kQueryParamDescription))
      .Build();
}

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
  base::expected<GURL, mojom::ActionResultPtr> search_url =
      ResolveSearchUrl(tool_delegate.GetProfile(), query_);
  if (!search_url.has_value()) {
    return {/*tool=*/nullptr, std::move(search_url.error())};
  }

  return NavigateToolRequest(GetActorSurfaceHandle(), std::move(*search_url))
      .CreateTool(task_id, tool_delegate);
}

std::optional<url::Origin> PerformSearchToolRequest::AssociatedOriginGrant()
    const {
  const ActorSurface* actor_surface = GetActorSurfaceHandle().Get();
  if (!actor_surface) {
    return std::nullopt;
  }

  const base::expected<GURL, mojom::ActionResultPtr> search_url =
      ResolveSearchUrl(
          *Profile::FromBrowserContext(
              actor_surface->GetWebContents()->GetBrowserContext()),
          query_);
  if (!search_url.has_value()) {
    return std::nullopt;
  }

  url::Origin origin = url::Origin::Create(*search_url);
  if (origin.opaque()) {
    return std::nullopt;
  }
  return origin;
}

std::string_view PerformSearchToolRequest::Name() const {
  return kName;
}

}  // namespace actor
