// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/dictation_context_fetcher.h"

#include <optional>

#include "base/byte_size.h"
#include "base/functional/bind.h"
#include "chrome/browser/dictation/target.h"
#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/page_content_annotations/content/page_context_fetcher_options.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"

namespace dictation {

namespace {

constexpr base::ByteSize kInnerTextLimit = base::KiBU(200);

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

}  // namespace

DictationContextFetcher::DictationContextFetcher() = default;
DictationContextFetcher::~DictationContextFetcher() = default;

void DictationContextFetcher::Fetch(const Target& target,
                                    GetContextCallback callback) {
  content::RenderFrameHost* rfh = target.GetRenderFrameHost();
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents) {
    DictationContext context;
    std::move(callback).Run(std::move(context));
    return;
  }

  page_content_annotations::FetchPageContextOptions options;
  options.inner_text_bytes_limit = kInnerTextLimit.InBytes();
  options.annotated_page_content_options =
      optimization_guide::DefaultAIPageContentOptions(
          /*on_critical_path=*/true);
  options.annotated_page_content_options->include_same_site_only = true;

  page_content_annotations::FetchPageContext(
      *web_contents, options,
      /*progress_listener=*/nullptr,
      base::BindOnce(&DictationContextFetcher::OnPageContextFetched,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DictationContextFetcher::OnPageContextFetched(
    GetContextCallback callback,
    page_content_annotations::FetchPageContextResultCallbackArg result) {
  DictationContext context;
  // TODO(b/527240600): Handle errors
  // TODO(b/525845074): Implement CSE/IRM protections
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
