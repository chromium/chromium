// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/dictation_context_fetcher.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/byte_size.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_split.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/dictation/logging.h"
#include "chrome/browser/dictation/target.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"
#include "chrome/browser/ui/tabs/page_context_eligibility_helper.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/content/browser/page_context_eligibility_observer.h"
#include "components/page_content_annotations/content/page_context_fetcher_options.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
#include "third_party/blink/public/mojom/dictation/dictation_agent.mojom.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace dictation {

namespace {

constexpr base::ByteSize kInnerTextLimit = base::KiB(200);

// Helper to recursively find FrameData by document token in the ContentNode
// tree.
const optimization_guide::proto::FrameData* FindFrameDataInTree(
    const optimization_guide::proto::ContentNode& node,
    const std::string& target_token) {
  if (node.content_attributes().has_iframe_data() &&
      node.content_attributes().iframe_data().has_frame_data()) {
    const optimization_guide::proto::FrameData& frame_data =
        node.content_attributes().iframe_data().frame_data();
    if (frame_data.has_document_identifier() &&
        frame_data.document_identifier().serialized_token() == target_token) {
      return &frame_data;
    }
  }

  for (const auto& child : node.children_nodes()) {
    if (const auto* found = FindFrameDataInTree(child, target_token)) {
      return found;
    }
  }
  return nullptr;
}

// Finds FrameData by document token
const optimization_guide::proto::FrameData* FindFrameData(
    const optimization_guide::proto::AnnotatedPageContent& proto,
    const std::string& target_token) {
  if (proto.has_main_frame_data() &&
      proto.main_frame_data().has_document_identifier() &&
      proto.main_frame_data().document_identifier().serialized_token() ==
          target_token) {
    return &proto.main_frame_data();
  }
  if (proto.has_root_node()) {
    return FindFrameDataInTree(proto.root_node(), target_token);
  }
  return nullptr;
}

std::optional<std::string> GetSelectedText(
    const optimization_guide::proto::AnnotatedPageContent& proto) {
  const optimization_guide::proto::FrameData* target_frame = nullptr;

  if (proto.has_page_interaction_info() &&
      proto.page_interaction_info().has_focused_frame()) {
    target_frame = FindFrameData(
        proto,
        proto.page_interaction_info().focused_frame().serialized_token());
  } else if (proto.has_main_frame_data()) {
    // Fallback to main frame if no focused frame info is available.
    target_frame = &proto.main_frame_data();
  }

  if (target_frame && target_frame->has_frame_interaction_info() &&
      target_frame->frame_interaction_info().has_selection() &&
      target_frame->frame_interaction_info().selection().has_selected_text()) {
    return target_frame->frame_interaction_info().selection().selected_text();
  }
  return std::nullopt;
}

optimization_guide::PageContextEligibilityStatus GetPageContextEligibility(
    content::WebContents* web_contents) {
  auto* tab_interface = tabs::TabInterface::MaybeGetFromContents(web_contents);
  if (!tab_interface) {
    return optimization_guide::PageContextEligibilityStatus::kUnknown;
  }
  auto* helper = tabs::PageContextEligibilityHelper::From(tab_interface);
  if (!helper) {
    return optimization_guide::PageContextEligibilityStatus::kUnknown;
  }
  return helper->IsPageContextEligible();
}

// Returns true if `origin` is https and its host (or a parent domain of it) is
// listed in `kPopulateEditContextHosts`. Uses the origin, not the URL, because
// some editors put their EditContext in an about:blank iframe, which inherits
// its origin from the page.
bool ShouldPopulateEditContext(const url::Origin& origin) {
  if (origin.scheme() != url::kHttpsScheme) {
    return false;
  }
  const std::string hosts = kPopulateEditContextHosts.Get();
  for (std::string_view host : base::SplitStringPiece(
           hosts, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    if (origin.DomainIs(host)) {
      return true;
    }
  }
  return false;
}

}  // namespace

DictationContextFetcher::DictationContextFetcher() = default;
DictationContextFetcher::~DictationContextFetcher() = default;

void DictationContextFetcher::Fetch(Target& target,
                                    GetContextCallback callback) {
  content::RenderFrameHost* rfh = target.GetRenderFrameHost();
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents) {
    DictationContext context;
    std::move(callback).Run(std::move(context));
    return;
  }

  if (glic::IsGlicGuest(web_contents)) {
    // TODO(b/535731618): Straightforward context fetch appears to never return
    // and needs more investigation. For now, just eliding context is better
    // than breaking the feature. (Also, we probably want to include the web
    // page content _in addition to_ the side panel content since that's likely
    // relevant for speech biasing)
    DictationContext context;
    std::move(callback).Run(std::move(context));
    return;
  }

  // If eligibility is kUnknown we'll try again when the fetch completes but if
  // the page is known ineligible just bail now.
  if (GetPageContextEligibility(web_contents) ==
      optimization_guide::PageContextEligibilityStatus::kNotEligible) {
    VT_LOG(web_contents->GetBrowserContext())
        << "Page not eligible for context";
    DictationContext context;
    std::move(callback).Run(std::move(context));
    return;
  }

  // Some pages using EditContext only populate it once they see IME input. On
  // listed sites, ask the agent to populate it first. Page context is fetched
  // over other pipes, so only start it once the agent has replied.
  // TODO(b/568411900): Only do this if the target has an EditContext,
  // e.g. via a new bit in FocusedNodeDetails stored on Target.
  const blink::DOMNodeIdType node_id =
      target.global_dom_node_id().target_element_dom_id;
  if (node_id.is_null() ||
      !ShouldPopulateEditContext(rfh->GetLastCommittedOrigin())) {
    FetchPageContext(web_contents->GetWeakPtr(), std::move(callback));
    return;
  }
  blink::mojom::DictationAgent* agent = target.GetDictationAgent();
  if (!agent) {
    // The target's frame is gone.
    std::move(callback).Run(DictationContext());
    return;
  }
  agent->PopulateEditContext(
      node_id.value(),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&DictationContextFetcher::FetchPageContext,
                         weak_ptr_factory_.GetWeakPtr(),
                         web_contents->GetWeakPtr(), std::move(callback))));
}

void DictationContextFetcher::FetchPageContext(
    base::WeakPtr<content::WebContents> web_contents,
    GetContextCallback callback) {
  if (!web_contents) {
    std::move(callback).Run(DictationContext());
    return;
  }

  page_content_annotations::FetchPageContextOptions options;
  options.inner_text_bytes_limit = kInnerTextLimit.InBytes();
  // Pages using EditContext may keep their text there instead of the DOM.
  options.inner_text_include_edit_context = true;
  options.annotated_page_content_options =
      optimization_guide::DefaultAIPageContentOptions(
          /*on_critical_path=*/true);
  options.annotated_page_content_options->include_same_site_only = true;

  page_content_annotations::FetchPageContext(
      *web_contents, options,
      /*progress_listener=*/nullptr,
      base::BindOnce(&DictationContextFetcher::OnPageContextFetched,
                     weak_ptr_factory_.GetWeakPtr(), web_contents,
                     std::move(callback)));
}

void DictationContextFetcher::OnPageContextFetched(
    base::WeakPtr<content::WebContents> web_contents,
    GetContextCallback callback,
    page_content_annotations::FetchPageContextResultCallbackArg result) {
  DictationContext context;

  // Fail closed if the page is not explicitly eligible.
  if (!web_contents ||
      GetPageContextEligibility(web_contents.get()) !=
          optimization_guide::PageContextEligibilityStatus::kEligible) {
    VT_LOG(web_contents->GetBrowserContext())
        << "Page not eligible for context";
    std::move(callback).Run(std::move(context));
    return;
  }

  // TODO(b/527240600): Handle errors
  if (result.has_value()) {
    auto& fetch_result = *result;
    if (fetch_result->annotated_page_content_result.has_value()) {
      context.annotated_page_content =
          std::move(fetch_result->annotated_page_content_result.value().proto);
      context.editable_content =
          GetSelectedText(*context.annotated_page_content);
    }
    if (fetch_result->inner_text_result.has_value()) {
      context.inner_text =
          std::move(fetch_result->inner_text_result->inner_text);
    }
  }

  std::move(callback).Run(std::move(context));
}

}  // namespace dictation
