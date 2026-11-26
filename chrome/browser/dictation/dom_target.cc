// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/dom_target.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/dictation/dictation_context_fetcher.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/dictation/format_transcription.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_utils.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "ui/base/ime/ime_text_span.h"

namespace dictation {

namespace {

tabs::TabInterface* GetActiveTabFromGlic(content::WebContents* web_contents) {
  if (!glic::IsGlicGuest(web_contents)) {
    return nullptr;
  }

  content::WebContents* outermost_web_contents =
      web_contents->GetOutermostWebContents();
  gfx::NativeWindow native_window =
      outermost_web_contents->GetTopLevelNativeWindow();
  BrowserWindowInterface* browser =
      GlobalBrowserCollection::GetInstance()->FindBrowserWithWindow(
          native_window);
  return browser ? browser->GetActiveTabInterface() : nullptr;
}

tabs::TabInterface* GetTabFromTargetId(
    const content::GlobalDOMNodeId& target_id) {
  content::RenderFrameHost* rfh = target_id.document.AsRenderFrameHostIfValid();
  if (!rfh) {
    return nullptr;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents) {
    return nullptr;
  }

  // Use normal tab lookup first.
  if (auto* tab = tabs::TabInterface::MaybeGetFromContents(web_contents)) {
    return tab;
  }

  // Check if `web_contents` is a WebUI embedded in a tab (e.g. Glic selection
  // overlay).
  if (tabs::TabInterface* embedding_tab =
          webui::GetTabInterface(web_contents)) {
    return embedding_tab;
  }

  // If the Glic side panel is being targeted, then associate the session with
  // the active tab of the window.
  if (tabs::TabInterface* tab = GetActiveTabFromGlic(web_contents)) {
    return tab;
  }

  return nullptr;
}

std::optional<DictationUrlCategory> GetUrlCategoryFromTargetId(
    const content::GlobalDOMNodeId& target_id) {
  content::RenderFrameHost* rfh = target_id.document.AsRenderFrameHostIfValid();
  if (!rfh) {
    return std::nullopt;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents) {
    return std::nullopt;
  }

  if (glic::IsGlicGuest(web_contents) || glic::IsGlicWebUI(web_contents)) {
    return DictationUrlCategory::kGlic;
  }

  if (content::HasWebUIScheme(rfh->GetLastCommittedURL())) {
    return DictationUrlCategory::kWebUI;
  }

  return DictationUrlCategory::kWeb;
}

std::u16string RemovePrefix(const std::u16string& text,
                            const std::u16string& prefix) {
  size_t prefix_len = 0;
  while (prefix_len < text.length() && prefix_len < prefix.length() &&
         text[prefix_len] == prefix[prefix_len]) {
    prefix_len++;
  }
  return text.substr(prefix_len);
}

void CompleteAsync(base::OnceClosure on_complete) {
  if (on_complete) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(on_complete));
  }
}

}  // namespace

TargetDetails::TargetDetails() = default;
TargetDetails::TargetDetails(const content::GlobalDOMNodeId& target_id,
                             bool richly_editable)
    : target_id(target_id), richly_editable(richly_editable) {}
TargetDetails::TargetDetails(const content::GlobalDOMNodeId& target_id)
    : target_id(target_id) {}
TargetDetails::TargetDetails(const TargetDetails&) = default;
TargetDetails& TargetDetails::operator=(const TargetDetails&) = default;
TargetDetails::TargetDetails(TargetDetails&&) = default;
TargetDetails& TargetDetails::operator=(TargetDetails&&) = default;
TargetDetails::~TargetDetails() = default;

DomTarget::DomTarget() = default;

DomTarget::DomTarget(const TargetDetails& target_details)
    : target_details_(target_details) {}

DomTarget::~DomTarget() = default;

tabs::TabInterface* DomTarget::GetHostTab() const {
  return GetTabFromTargetId(target_details_.target_id);
}

std::optional<DictationUrlCategory> DomTarget::GetUrlCategory() const {
  return GetUrlCategoryFromTargetId(target_details_.target_id);
}

std::unique_ptr<DictationContextFetcher> DomTarget::CreateContextFetcher() {
  return std::make_unique<DictationContextFetcher>(*this);
}

void DomTarget::SetSelectedText(std::u16string selected_text) {
  selected_text_ = std::move(selected_text);
}

std::optional<content::GlobalDOMNodeId> DomTarget::GetGlobalDOMNodeId() const {
  return target_details_.target_id;
}

DomTarget* DomTarget::AsDomTarget() {
  return this;
}

const DomTarget* DomTarget::AsDomTarget() const {
  return this;
}

void DomTarget::OnFocusChanged(const content::FocusedNodeDetails& details) {
  content::RenderFrameHost* rfh = GetRenderFrameHost();
  if (!last_sent_composition_.empty() &&
      (!rfh ||
       rfh != details.global_dom_node_id.document.AsRenderFrameHostIfValid() ||
       global_dom_node_id().target_element_dom_id !=
           details.global_dom_node_id.target_element_dom_id)) {
    has_lost_focus_during_composition_ = true;
  }
}

content::RenderFrameHost* DomTarget::GetRenderFrameHost() const {
  return global_dom_node_id().document.AsRenderFrameHostIfValid();
}

content::RenderWidgetHost* DomTarget::GetRenderWidgetHost() const {
  content::RenderFrameHost* rfh = GetRenderFrameHost();
  return rfh ? rfh->GetRenderWidgetHost() : nullptr;
}

blink::mojom::DictationAgent* DomTarget::GetDictationAgent() {
  content::RenderFrameHost* rfh = GetRenderFrameHost();
  if (!rfh) {
    return nullptr;
  }
  if (!dictation_agent_.is_bound()) {
    rfh->GetRemoteInterfaces()->GetInterface(
        dictation_agent_.BindNewPipeAndPassReceiver());
  }
  return dictation_agent_.get();
}

void DomTarget::SetComposition(const std::u16string& text, bool is_final) {
  QueuedOperation op{
      .type = is_final ? QueuedOperation::Type::kSetFinalComposition
                       : QueuedOperation::Type::kSetPartialComposition,
      .text = FormatTextForInsertion(text),
  };
  if (is_waiting_on_operation_completion_) {
    // Intentionally overwrites any other pending operation.
    CHECK(!queued_operation_ || !queued_operation_->on_commit_complete);
    queued_operation_ = std::move(op);
    return;
  }

  ExecuteOperation(std::move(op));
}

void DomTarget::CommitComposition(const std::u16string& text,
                                  base::OnceClosure on_commit_complete) {
  QueuedOperation op{
      .type = QueuedOperation::Type::kCommitComposition,
      .text = FormatTextForInsertion(text),
      .on_commit_complete = std::move(on_commit_complete),
  };
  if (is_waiting_on_operation_completion_) {
    // Intentionally overwrites any other pending operation.
    CHECK(!queued_operation_ || !queued_operation_->on_commit_complete);
    queued_operation_ = std::move(op);
    return;
  }

  ExecuteOperation(std::move(op));
}

void DomTarget::ExecuteSetComposition(const std::u16string& text,
                                      bool is_final,
                                      base::OnceClosure operation_complete) {
  base::ScopedClosureRunner scoped_operation_complete(
      std::move(operation_complete));

  if (!is_final && !kShowPartials.Get()) {
    return;
  }

  if (has_lost_focus_during_composition_ || paste_fallback_required_) {
    // The associated element lost focus, and whatever text we composed was
    // committed. Or we can't compose further due to the need to switch to
    // pasting. Don't begin a new composition in this state, and instead only
    // commit the final text when the stream completes.
    return;
  }

  if (richly_editable() && text.find(u'\n') != std::u16string::npos) {
    // Some sites cannot handle multiline IME composition with their
    // contenteditables. See https://crbug.com/537833858 . We fallback to
    // pasting in this case.
    // TODO(b/540009971): For simplicity, we'll just cancel the existing
    // composition, if any, but we'll need to handle this more robustly if we
    // want to show partial transcripts.
    paste_fallback_required_ = true;
    if (last_sent_composition_.empty()) {
      // Nothing has been composed yet, so there is no composition to cancel.
      // Committing empty text here would delete any selected content as a
      // separate edit, splitting the eventual paste across two undo steps.
      // Instead, leave the selection intact so the paste replaces it in a
      // single edit.
      return;
    }
    last_sent_composition_ = u"";
    gfx::Range replacement_range =
        std::exchange(last_composition_range_, gfx::Range::InvalidRange());
    CommitExternallySourcedComposition(u"", replacement_range,
                                       scoped_operation_complete.Release());
    return;
  }

  // Specify an ImeTextSpan for the entire text to make it look like a user
  // typing without a visual difference for the composition.
  // But if we're showing partials for testing, still include an underline to
  // visually distinguish partials.
  ui::ImeTextSpan text_span;
  text_span.end_offset = text.length();
  text_span.underline_style = is_final ? ui::ImeTextSpan::UnderlineStyle::kNone
                                       : ui::ImeTextSpan::UnderlineStyle::kDot;

  last_sent_composition_ = text;

  SetExternallySourcedComposition(
      text, {text_span}, last_composition_range_,
      base::BindOnce(&DomTarget::OnSetCompositionComplete,
                     weak_factory_.GetWeakPtr(),
                     scoped_operation_complete.Release()));
}

void DomTarget::ExecuteCommitComposition(const std::u16string& text,
                                         base::OnceClosure operation_complete) {
  gfx::Range replacement_range =
      std::exchange(last_composition_range_, gfx::Range::InvalidRange());

  if (paste_fallback_required_) {
    PasteIntoNode(text);
    CompleteAsync(std::move(operation_complete));
    return;
  }

  if (richly_editable() && text.find(u'\n') != std::u16string::npos) {
    // If we've lost focus, then some of the previously composed text has
    // already been committed. Determine what has already been sent to avoid
    // duplication.
    // TODO(b/539566748): This will be incorrect if the stream rewrites text. We
    // should instead determine the last composition range and replace the text
    // in that range.
    std::u16string text_to_paste =
        has_lost_focus_during_composition_
            ? RemovePrefix(text, last_sent_composition_)
            : text;
    if (!text_to_paste.empty()) {
      PasteIntoNode(text_to_paste);
    }
    CompleteAsync(std::move(operation_complete));
    return;
  }

  CommitExternallySourcedComposition(text, replacement_range,
                                     std::move(operation_complete));
}

void DomTarget::ExecuteOperation(QueuedOperation op) {
  is_waiting_on_operation_completion_ = true;

  base::OnceClosure operation_complete = base::BindOnce(
      &DomTarget::OnOperationComplete, weak_factory_.GetWeakPtr(),
      std::move(op.on_commit_complete));

  switch (op.type) {
    case QueuedOperation::Type::kSetPartialComposition:
      ExecuteSetComposition(op.text, /*is_final=*/false,
                            std::move(operation_complete));
      break;
    case QueuedOperation::Type::kSetFinalComposition:
      ExecuteSetComposition(op.text, /*is_final=*/true,
                            std::move(operation_complete));
      break;
    case QueuedOperation::Type::kCommitComposition:
      ExecuteCommitComposition(op.text, std::move(operation_complete));
      break;
  }
}

void DomTarget::OnSetCompositionComplete(base::OnceClosure operation_complete,
                                         const gfx::Range& composition_range) {
  last_composition_range_ = composition_range;
  std::move(operation_complete).Run();
}

void DomTarget::OnOperationComplete(base::OnceClosure on_commit_complete) {
  is_waiting_on_operation_completion_ = false;

  CompleteAsync(std::move(on_commit_complete));

  if (queued_operation_.has_value()) {
    QueuedOperation op = std::move(*queued_operation_);
    queued_operation_.reset();
    ExecuteOperation(std::move(op));
  }
}

void DomTarget::SetExternallySourcedComposition(
    const std::u16string& text,
    const std::vector<ui::ImeTextSpan>& spans,
    const gfx::Range& replacement_range,
    base::OnceCallback<void(const gfx::Range&)> on_complete) {
  if (content::RenderWidgetHost* rwh = GetRenderWidgetHost()) {
    rwh->SetExternallySourcedComposition(text, spans, replacement_range,
                                         global_dom_node_id(),
                                         std::move(on_complete));
  } else if (on_complete) {
    CompleteAsync(
        base::BindOnce(std::move(on_complete), gfx::Range::InvalidRange()));
  }
}

void DomTarget::CommitExternallySourcedComposition(
    const std::u16string& text,
    const gfx::Range& replacement_range,
    base::OnceClosure on_complete) {
  if (content::RenderWidgetHost* rwh = GetRenderWidgetHost()) {
    rwh->CommitExternallySourcedComposition(
        text, replacement_range, global_dom_node_id(), std::move(on_complete));
  } else {
    CompleteAsync(std::move(on_complete));
  }
}

void DomTarget::PasteIntoNode(const std::u16string& text) {
  if (content::RenderWidgetHost* rwh = GetRenderWidgetHost()) {
    rwh->PasteIntoNode(text, global_dom_node_id());
  }
}

std::optional<std::u16string_view> DomTarget::GetTextPrecedingSelection() {
  if (content::RenderWidgetHost* rwh = GetRenderWidgetHost()) {
    return rwh->GetTextPrecedingSelection(global_dom_node_id());
  }
  return std::nullopt;
}

std::u16string DomTarget::FormatTextForInsertion(const std::u16string& text) {
  if (text.empty()) {
    return text;
  }

  // Snapshot the preceding text before the first composition replaces the
  // selection.
  if (!preceding_text_.has_value()) {
    preceding_text_ = std::u16string(GetTextPrecedingSelection().value_or(u""));
  }

  return FormatTranscription(text, *preceding_text_, selected_text_);
}

}  // namespace dictation
