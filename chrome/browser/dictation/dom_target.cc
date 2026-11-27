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
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/dictation/dictation_context_fetcher.h"
#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/dictation/format_transcription.h"
#include "chrome/browser/dictation/hotkey_manager.h"
#include "chrome/browser/dictation/stream_provider.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/accelerator_table.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_utils.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/ime/ime_text_span.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace dictation {

namespace {

bool MatchesAccelerator(const blink::WebKeyboardEvent& key_event,
                        const ui::Accelerator& accelerator) {
  if (accelerator.IsEmpty()) {
    return false;
  }
  return key_event.windows_key_code == accelerator.key_code() &&
         ui::Accelerator::MaskOutKeyEventFlags(
             ui::WebEventModifiersToEventFlags(key_event.GetModifiers())) ==
             ui::Accelerator::MaskOutKeyEventFlags(accelerator.modifiers());
}

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

DomTarget::DomTarget()
    : key_press_event_callback_(
          base::BindRepeating(&DomTarget::HandleKeyPressEvent,
                              base::Unretained(this))) {}

DomTarget::DomTarget(const TargetDetails& target_details)
    : target_details_(target_details),
      key_press_event_callback_(
          base::BindRepeating(&DomTarget::HandleKeyPressEvent,
                              base::Unretained(this))) {
  Observe(content::WebContents::FromRenderFrameHost(GetRenderFrameHost()));
}

DomTarget::~DomTarget() {
  UnregisterKeyPressCallback();
}

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

void DomTarget::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void DomTarget::SetStreamActive(bool active) {
  is_stream_active_ = active;
  if (active) {
    RegisterKeyPressCallback(GetRenderWidgetHost());
  } else {
    UnregisterKeyPressCallback();
  }
}

DomTarget* DomTarget::AsDomTarget() {
  return this;
}

const DomTarget* DomTarget::AsDomTarget() const {
  return this;
}

void DomTarget::DidGetUserInteraction(const blink::WebInputEvent& event) {
  if (!delegate_) {
    return;
  }
  if (event.GetType() != blink::WebInputEvent::Type::kRawKeyDown &&
      event.GetType() != blink::WebInputEvent::Type::kKeyDown &&
      event.GetType() != blink::WebInputEvent::Type::kChar) {
    return;
  }

  const auto& key_event = static_cast<const blink::WebKeyboardEvent&>(event);
  if (key_event.windows_key_code == ui::VKEY_TAB) {
    // Don't handle tab key presses here. Let stream ending for those be handled
    // by the focus change logic.
    return;
  }

  if (key_event.windows_key_code == ui::VKEY_ESCAPE) {
    if (is_stream_active_) {
      delegate_->OnTargetRequestEndStream(
          DictationStreamEndTrigger::kEscapeKey);
    } else {
      delegate_->OnTargetRequestEndSession(
          DictationStreamEndTrigger::kEscapeKey);
    }
    return;
  }

  // Special case to ignore the accelerator used to focus inactive popups for
  // accessibility so it's not treated as typing that ends the stream.
  ui::Accelerator focus_accelerator;
  if (GetAcceleratorForCommandId(IDC_FOCUS_INACTIVE_POPUP_FOR_ACCESSIBILITY,
                                 &focus_accelerator) &&
      MatchesAccelerator(key_event, focus_accelerator)) {
    return;
  }

  CHECK(web_contents());

  // Ignore the Dictation hotkey itself. On surfaces where the key event reaches
  // the renderer before the accelerator is handled (e.g. the Glic side panel),
  // treating it as typing would end the stream here. ToggleHotkeyHandler()
  // would then find no attached stream to toggle off and start a new one
  // instead, so dictation could never be stopped with the hotkey.
  Profile* const profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  CHECK(profile);
  if (MatchesAccelerator(key_event, GetDictationHotkeyFromPrefs(profile)) ||
      MatchesAccelerator(key_event,
                         GetDictationPressAndHoldHotkeyFromPrefs(profile))) {
    return;
  }

  // While the hotkey hold watcher is active, any non-chord key press is
  // handled by `HotkeyHoldWatcher` (ending with `kHotkeyHoldAbort`), and
  // synthetic IME composition events (`VKEY_PROCESSKEY` on macOS during key
  // auto-repeat while `_hasMarkedText` is true) must not be treated as user
  // typing.
  const DictationKeyedService* const service =
      DictationKeyedService::Get(profile);
  CHECK(service);
  if (!service->is_hold_watcher_active()) {
    // If the user starts typing, end the stream.
    const bool event_has_text = !base::IsUnicodeControl(key_event.text[0]);
    if (is_stream_active_ && web_contents()->IsFocusedElementEditable() &&
        event_has_text) {
      delegate_->OnTargetRequestEndStream(
          DictationStreamEndTrigger::kUserTyping);
    }
  }
}

void DomTarget::OnFocusChangedInPage(
    const content::FocusedNodeDetails& details) {
  content::RenderFrameHost* rfh = GetRenderFrameHost();
  if (!last_sent_composition_.empty() &&
      (!rfh ||
       rfh != details.global_dom_node_id.document.AsRenderFrameHostIfValid() ||
       global_dom_node_id().target_element_dom_id !=
           details.global_dom_node_id.target_element_dom_id)) {
    has_lost_focus_during_composition_ = true;
  }

  if (!delegate_ || !is_stream_active_) {
    return;
  }

  if (details.focus_type == blink::mojom::FocusType::kNone ||
      details.focus_type == blink::mojom::FocusType::kScript) {
    return;
  }

  delegate_->OnTargetRequestEndStream(DictationStreamEndTrigger::kFocusChange);
}

void DomTarget::PrimaryPageChanged(content::Page& page) {
  if (delegate_) {
    delegate_->OnTargetInvalidated();
  }
}

void DomTarget::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  if (delegate_) {
    delegate_->OnTargetInvalidated();
  }
}

void DomTarget::WebContentsDestroyed() {
  UnregisterKeyPressCallback();
  if (delegate_) {
    delegate_->OnTargetInvalidated();
  }
}

void DomTarget::RenderWidgetHostDestroyed(
    content::RenderWidgetHost* widget_host) {
  CHECK_EQ(widget_host, render_widget_host_observation_.GetSource());
  UnregisterKeyPressCallback();
}

void DomTarget::RegisterKeyPressCallback(
    content::RenderWidgetHost* widget_host) {
  UnregisterKeyPressCallback();
  if (!widget_host) {
    return;
  }
  widget_host->AddKeyPressEventCallback(key_press_event_callback_);
  render_widget_host_observation_.Observe(widget_host);
}

void DomTarget::UnregisterKeyPressCallback() {
  if (render_widget_host_observation_.IsObserving()) {
    render_widget_host_observation_.GetSource()->RemoveKeyPressEventCallback(
        key_press_event_callback_);
    render_widget_host_observation_.Reset();
  }
}

bool DomTarget::HandleKeyPressEvent(
    const input::NativeWebKeyboardEvent& event) {
  if (!is_stream_active_ || !delegate_) {
    return false;
  }

  if (event.windows_key_code != ui::VKEY_RETURN ||
      (event.GetModifiers() & blink::WebInputEvent::kKeyModifiers)) {
    return false;
  }

  delegate_->OnTargetRequestEndStream(DictationStreamEndTrigger::kEnterKey);
  return true;
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
