// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/site_based_search_tool.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/strings/strcat.h"
#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tools/tool_delegate.h"
#include "chrome/browser/actor/tools/type_tool_request.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/optimization_guide/content/browser/page_content_proto_util.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/page_content_annotations/core/page_content_extraction_types.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace actor {

namespace {

bool IsEligibleSearchInput(
    const optimization_guide::proto::ContentAttributes& attrs) {
  const bool contentIsWritableFormControl =
      attrs.has_common_ancestor_dom_node_id() &&
      attrs.has_form_control_data() && !attrs.form_control_data().is_readonly();
  const bool contentIsInteractable =
      !attrs.has_interaction_info() || !attrs.interaction_info().is_disabled();

  return contentIsWritableFormControl && contentIsInteractable &&
         attrs.form_control_data().form_control_type() ==
             optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_SEARCH;
}

std::optional<DomNode> FindSearchInputNode(
    const optimization_guide::proto::ContentNode& root_node,
    std::string_view main_doc_id) {
  std::optional<DomNode> match;
  optimization_guide::VisitContentNodes(
      root_node, main_doc_id,
      [&](const optimization_guide::proto::ContentNode& node,
          std::string_view document_identifier) {
        if (match.has_value() || !node.has_content_attributes()) {
          return;
        }
        const auto& attrs = node.content_attributes();
        if (IsEligibleSearchInput(attrs)) {
          match =
              DomNode{.node_id = attrs.common_ancestor_dom_node_id(),
                      .document_identifier = std::string(document_identifier)};
        }
      });
  return match;
}

}  // namespace

SiteBasedSearchTool::SiteBasedSearchTool(TaskId task_id,
                                         ToolDelegate& tool_delegate,
                                         ActorSurface& actor_surface,
                                         std::string query,
                                         std::string document_identifier)
    : Tool(task_id, tool_delegate),
      actor_surface_handle_(actor_surface.GetHandle()),
      query_(std::move(query)),
      document_identifier_(std::move(document_identifier)) {}

SiteBasedSearchTool::~SiteBasedSearchTool() = default;

void SiteBasedSearchTool::Validate(ToolCallback callback) {
  CHECK(!query_.empty());
  CHECK(!document_identifier_.empty());

  ActorSurface* actor_surface = actor_surface_handle_.Get();
  if (!actor_surface) {
    PostResponseTask(std::move(callback),
                     MakeResult(mojom::ActionResultCode::kTabWentAway));
    return;
  }

  // TODO(b/555814845): Consider caching in, and getting content from,
  // `ActorTabData` before calling into `PageContentExtractionService`.
  const optimization_guide::proto::AnnotatedPageContent* apc = nullptr;
  std::optional<page_content_annotations::ExtractedPageContentResult>
      extracted_content;
  if (auto* extraction_service =
          page_content_annotations::PageContentExtractionServiceFactory::
              GetForProfile(&tool_delegate().GetProfile())) {
    extracted_content =
        extraction_service->GetExtractedPageContentAndEligibilityForPage(
            actor_surface->GetWebContents()->GetPrimaryPage());
    if (extracted_content.has_value() && extracted_content->page_content) {
      apc = &extracted_content->page_content->data;
    }
  }

  if (!apc || !apc->has_root_node() || !apc->has_main_frame_data() ||
      !apc->main_frame_data().has_document_identifier()) {
    PostResponseTask(
        std::move(callback),
        MakeResult(mojom::ActionResultCode::kSiteBasedSearchNotAvailable,
                   /*requires_page_stabilization=*/false,
                   "No search input found on the current page."));
    return;
  }

  const std::string& main_doc_id =
      apc->main_frame_data().document_identifier().serialized_token();
  if (main_doc_id != document_identifier_) {
    PostResponseTask(
        std::move(callback),
        MakeResult(mojom::ActionResultCode::kFrameWentAway,
                   /*requires_page_stabilization=*/false,
                   "Document identifier does not match the current page."));
    return;
  }

  resolved_dom_node_ = FindSearchInputNode(apc->root_node(), main_doc_id);

  if (!resolved_dom_node_.has_value()) {
    PostResponseTask(
        std::move(callback),
        MakeResult(mojom::ActionResultCode::kSiteBasedSearchNotAvailable,
                   /*requires_page_stabilization=*/false,
                   "No search input found on the current page."));
    return;
  }

  PostResponseTask(std::move(callback), MakeOkResult());
}

void SiteBasedSearchTool::Invoke(ToolCallback callback) {
  if (!actor_surface_handle_.Get()) {
    PostResponseTask(std::move(callback),
                     MakeResult(mojom::ActionResultCode::kTabWentAway));
    return;
  }

  CHECK(resolved_dom_node_.has_value());
  tool_delegate().EnqueueFollowupAction(std::make_unique<TypeToolRequest>(
      actor_surface_handle_, PageTarget(*resolved_dom_node_), query_,
      /*follow_by_enter=*/true, TypeToolRequest::Mode::kReplace));

  PostResponseTask(std::move(callback), MakeOkResult());
}

std::string SiteBasedSearchTool::DebugString() const {
  return base::StrCat({"SiteBasedSearchTool[query=\"", query_, "\"]"});
}

std::string SiteBasedSearchTool::JournalEvent() const {
  return "SiteBasedSearch";
}

GURL SiteBasedSearchTool::JournalURL() const {
  if (ActorSurface* actor_surface = actor_surface_handle_.Get()) {
    return actor_surface->GetWebContents()->GetLastCommittedURL();
  }
  return GURL();
}

std::unique_ptr<ObservationDelayController>
SiteBasedSearchTool::GetObservationDelayer(
    ObservationDelayController::PageStabilityConfig page_stability_config) {
  return nullptr;
}

void SiteBasedSearchTool::UpdateTaskBeforeInvoke(ActorTask& task,
                                                 ToolCallback callback) const {
  task.AddActorSurface(actor_surface_handle_, /*stop_task_on_detach=*/true,
                       std::move(callback));
}

ActorSurfaceHandle SiteBasedSearchTool::GetTargetActorSurface() const {
  return actor_surface_handle_;
}

}  // namespace actor
