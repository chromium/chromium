// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_DOM_TARGET_H_
#define CHROME_BROWSER_DICTATION_DOM_TARGET_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/dictation/metrics.h"
#include "chrome/browser/dictation/target.h"
#include "content/public/browser/global_dom_node_id.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/dictation/dictation_agent.mojom.h"
#include "ui/base/ime/ime_text_span.h"
#include "ui/gfx/range/range.h"

namespace content {
struct FocusedNodeDetails;
class RenderFrameHost;
class RenderWidgetHost;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace dictation {

// Details about a DOM element used to construct a `DomTarget`.
struct TargetDetails {
  TargetDetails();
  TargetDetails(const content::GlobalDOMNodeId& target_id,
                bool richly_editable);

  // Additional constructor for testing.
  explicit TargetDetails(const content::GlobalDOMNodeId& target_id);

  TargetDetails(const TargetDetails&);
  TargetDetails& operator=(const TargetDetails&);
  TargetDetails(TargetDetails&&);
  TargetDetails& operator=(TargetDetails&&);
  ~TargetDetails();

  content::GlobalDOMNodeId target_id;
  bool richly_editable = false;
};

// Represents a DOM-backed dictation target into which transcriptions are
// written via IME composition or paste fallback.
class DomTarget : public Target {
 public:
  DomTarget();
  explicit DomTarget(const TargetDetails& target_details);
  ~DomTarget() override;

  // Target:
  void SetComposition(const std::u16string& text, bool is_final) override;
  void CommitComposition(const std::u16string& text,
                         base::OnceClosure on_commit_complete) override;
  tabs::TabInterface* GetHostTab() const override;
  std::optional<DictationUrlCategory> GetUrlCategory() const override;
  std::unique_ptr<DictationContextFetcher> CreateContextFetcher() override;
  void SetSelectedText(std::u16string selected_text) override;
  std::optional<content::GlobalDOMNodeId> GetGlobalDOMNodeId() const override;
  DomTarget* AsDomTarget() override;
  const DomTarget* AsDomTarget() const override;

  // Returns the RenderFrameHost associated with this target, or nullptr if it
  // no longer exists.
  content::RenderFrameHost* GetRenderFrameHost() const;

  const content::GlobalDOMNodeId& global_dom_node_id() const {
    return target_details_.target_id;
  }

  bool richly_editable() const { return target_details_.richly_editable; }

  // Called when focus changes in the page.
  void OnFocusChanged(const content::FocusedNodeDetails& details);

  // Returns the DictationAgent in the target's document, connecting to it if
  // needed. Returns nullptr if the target's document is gone.
  blink::mojom::DictationAgent* GetDictationAgent();

 protected:
  virtual void SetExternallySourcedComposition(
      const std::u16string& text,
      const std::vector<ui::ImeTextSpan>& spans,
      const gfx::Range& replacement_range,
      base::OnceCallback<void(const gfx::Range&)> on_complete);
  virtual void CommitExternallySourcedComposition(
      const std::u16string& text,
      const gfx::Range& replacement_range,
      base::OnceClosure on_complete);
  virtual void PasteIntoNode(const std::u16string& text);
  virtual std::optional<std::u16string_view> GetTextPrecedingSelection();

 private:
  // The stream may produce multiple updates while the last composition we sent
  // to the renderer is in progress. We sequence our calls to the renderer so
  // that we only have one in progress at a time. This avoids sending parameters
  // to the renderer that are based on stale information. Only one operation is
  // queued. If multiple updates from the stream come in while waiting for the
  // renderer, the most recent one supersedes the previous ones.
  struct QueuedOperation {
    enum class Type {
      kSetPartialComposition,
      kSetFinalComposition,
      kCommitComposition,
    };

    Type type;
    // The text to compose/commit.
    std::u16string text;
    // To be called when a commit completes.
    base::OnceClosure on_commit_complete;
  };

  // These execute methods perform the actions associated with the public
  // SetComposition and CommitComposition methods, after the action has been
  // sequenced.
  void ExecuteSetComposition(const std::u16string& text,
                             bool is_final,
                             base::OnceClosure operation_complete);
  void ExecuteCommitComposition(const std::u16string& text,
                                base::OnceClosure operation_complete);
  // Dispatches the given operation based on its type.
  void ExecuteOperation(QueuedOperation op);
  // Called when SetExternallySourcedComposition completes.
  void OnSetCompositionComplete(base::OnceClosure operation_complete,
                                const gfx::Range& composition_range);
  // Called when the currently executing operations completes.
  void OnOperationComplete(base::OnceClosure on_commit_complete);

  // Formats transcription text for insertion into the target element.
  std::u16string FormatTextForInsertion(const std::u16string& text);

  content::RenderWidgetHost* GetRenderWidgetHost() const;

  TargetDetails target_details_;

  std::u16string last_sent_composition_;
  // The range of the last composition (or text immediately committed by the
  // page) reported by the renderer. Passed as the replacement range on
  // subsequent composition updates and commits in case the active IME
  // composition was lost.
  gfx::Range last_composition_range_ = gfx::Range::InvalidRange();
  bool has_lost_focus_during_composition_ = false;
  bool paste_fallback_required_ = false;

  std::optional<std::u16string> preceding_text_;
  // The text selected in the target when dictation started. See
  // `SetSelectedText()`.
  std::u16string selected_text_;

  bool is_waiting_on_operation_completion_ = false;
  std::optional<QueuedOperation> queued_operation_;

  // Connection to the DictationAgent in the target's document. Bound lazily.
  mojo::Remote<blink::mojom::DictationAgent> dictation_agent_;

  base::WeakPtrFactory<DomTarget> weak_factory_{this};
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_DOM_TARGET_H_
