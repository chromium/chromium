// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tab_annotation_manager.h"

#include <utility>

#include "base/functional/bind.h"
#include "components/shared_highlighting/core/common/text_fragment.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace actor {

WEB_CONTENTS_USER_DATA_KEY_IMPL(TabAnnotationManager);

TabAnnotationManager::TabAnnotationManager(content::WebContents* web_contents)
    : content::WebContentsUserData<TabAnnotationManager>(*web_contents),
      content::WebContentsObserver(web_contents) {}

TabAnnotationManager::~TabAnnotationManager() {
  Reset();
}

void TabAnnotationManager::HighlightText(const std::string& query,
                                         HighlightCallback callback) {
  if (query.empty()) {
    std::move(callback).Run(false);
    return;
  }

  content::RenderFrameHost* rfh = web_contents()->GetPrimaryMainFrame();
  CHECK(rfh);

  if (!current_document_.AsRenderFrameHostIfValid()) {
    current_document_ = rfh->GetWeakDocumentPtr();
    rfh->GetRemoteInterfaces()->GetInterface(
        annotation_container_.BindNewPipeAndPassReceiver());
  }

  Reset();
  CHECK(!pending_highlight_callback_);
  pending_highlight_callback_ = std::move(callback);

  auto selector = blink::mojom::Selector::NewSerializedSelector(
      shared_highlighting::TextFragment(query).ToEscapedString(
          shared_highlighting::TextFragment::EscapedStringFormat::
              kWithoutTextDirective));

  annotation_container_->CreateAgent(
      agent_host_receiver_.BindNewPipeAndPassRemote(),
      annotation_agent_.BindNewPipeAndPassReceiver(),
      blink::mojom::AnnotationType::kGlic, std::move(selector),
      /*search_range_start_node_id=*/std::nullopt);

  annotation_agent_.set_disconnect_handler(base::BindOnce(
      &TabAnnotationManager::OnAgentDisconnected, base::Unretained(this)));
}

void TabAnnotationManager::ClearHighlight() {
  Reset();
}

bool TabAnnotationManager::HasActiveHighlight() const {
  return annotation_agent_.is_bound();
}

void TabAnnotationManager::PrimaryPageChanged(content::Page& page) {
  Reset();
  current_document_ = {};
  annotation_container_.reset();
}

void TabAnnotationManager::DidFinishAttachment(
    const gfx::Rect& document_relative_rect,
    blink::mojom::AttachmentResult attachment_result) {
  if (attachment_result != blink::mojom::AttachmentResult::kSuccess) {
    Reset();
    return;
  }

  if (annotation_agent_.is_bound()) {
    annotation_agent_->ScrollIntoView(/*applies_focus=*/true);
  }

  if (pending_highlight_callback_) {
    std::move(pending_highlight_callback_).Run(true);
  }
}

void TabAnnotationManager::OnAgentDisconnected() {
  Reset();
}

void TabAnnotationManager::Reset() {
  agent_host_receiver_.reset();
  annotation_agent_.reset();
  if (pending_highlight_callback_) {
    std::move(pending_highlight_callback_).Run(false);
  }
}

}  // namespace actor
