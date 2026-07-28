// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/target.h"

#include "chrome/browser/dictation/features.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "ui/base/ime/ime_text_span.h"

namespace dictation {

namespace {

std::u16string RemovePrefix(const std::u16string& text,
                            const std::u16string& prefix) {
  size_t prefix_len = 0;
  while (prefix_len < text.length() && prefix_len < prefix.length() &&
         text[prefix_len] == prefix[prefix_len]) {
    prefix_len++;
  }
  return text.substr(prefix_len);
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

Target::Target() = default;

Target::Target(const TargetDetails& target_details)
    : target_details_(target_details) {}

Target::~Target() = default;

void Target::OnFocusChanged(const content::FocusedNodeDetails& details) {
  content::RenderFrameHost* rfh = GetRenderFrameHost();
  if (!last_sent_composition_.empty() &&
      (!rfh ||
       rfh != details.global_dom_node_id.document.AsRenderFrameHostIfValid() ||
       global_dom_node_id().target_element_dom_id !=
           details.global_dom_node_id.target_element_dom_id)) {
    has_lost_focus_during_composition_ = true;
  }
}

content::RenderFrameHost* Target::GetRenderFrameHost() const {
  return global_dom_node_id().document.AsRenderFrameHostIfValid();
}

content::RenderWidgetHost* Target::GetRenderWidgetHost() const {
  content::RenderFrameHost* rfh = GetRenderFrameHost();
  return rfh ? rfh->GetRenderWidgetHost() : nullptr;
}

void Target::SetComposition(const std::u16string& text, bool is_final) {
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
    last_sent_composition_ = u"";
    CommitExternallySourcedComposition(u"");
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

  SetExternallySourcedComposition(text, {text_span});
}

void Target::CommitComposition(const std::u16string& text) {
  if (paste_fallback_required_) {
    PasteIntoNode(text);
    return;
  }

  // If we've lost focus, then some of the previously composed text has already
  // been committed. Determine what has already been sent to avoid duplication.
  // TODO(b/539566748): This will be incorrect if the stream rewrites text. We
  // should instead determine the last composition range and replace the text in
  // that range.
  std::u16string text_to_commit =
      has_lost_focus_during_composition_
          ? RemovePrefix(text, last_sent_composition_)
          : text;

  if (richly_editable() && text_to_commit.find(u'\n') != std::u16string::npos) {
    PasteIntoNode(text_to_commit);
    return;
  }

  CommitExternallySourcedComposition(text_to_commit);
}

void Target::SetExternallySourcedComposition(
    const std::u16string& text,
    const std::vector<ui::ImeTextSpan>& spans) {
  if (content::RenderWidgetHost* rwh = GetRenderWidgetHost()) {
    rwh->SetExternallySourcedComposition(text, spans, global_dom_node_id());
  }
}

void Target::CommitExternallySourcedComposition(const std::u16string& text) {
  if (content::RenderWidgetHost* rwh = GetRenderWidgetHost()) {
    rwh->CommitExternallySourcedComposition(text, global_dom_node_id());
  }
}

void Target::PasteIntoNode(const std::u16string& text) {
  if (content::RenderWidgetHost* rwh = GetRenderWidgetHost()) {
    rwh->PasteIntoNode(text, global_dom_node_id());
  }
}

}  // namespace dictation
