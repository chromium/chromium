// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_TARGET_H_
#define CHROME_BROWSER_DICTATION_TARGET_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/global_dom_node_id.h"
#include "ui/base/ime/ime_text_span.h"

namespace content {
struct FocusedNodeDetails;
class RenderFrameHost;
class RenderWidgetHost;
}  // namespace content

namespace dictation {

// Details about an element used to construct a `Target`.
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

// Represents a dictation target into which transcriptions will be written.
class Target {
 public:
  Target();
  explicit Target(const TargetDetails& target_details);
  virtual ~Target();

  // Returns the RenderFrameHost associated with this target, or nullptr if it
  // no longer exists.
  content::RenderFrameHost* GetRenderFrameHost() const;

  const content::GlobalDOMNodeId& global_dom_node_id() const {
    return target_details_.target_id;
  }

  bool richly_editable() const { return target_details_.richly_editable; }

  // Called when focus changes in the page.
  void OnFocusChanged(const content::FocusedNodeDetails& details);

  // Sets the composition text in the target.
  void SetComposition(const std::u16string& text, bool is_final);

  // Commits the text in the target.
  void CommitComposition(const std::u16string& text,
                         base::OnceClosure on_commit_complete);

 protected:
  virtual void SetExternallySourcedComposition(
      const std::u16string& text,
      const std::vector<ui::ImeTextSpan>& spans,
      base::OnceClosure on_complete);
  virtual void CommitExternallySourcedComposition(
      const std::u16string& text,
      base::OnceClosure on_complete);
  virtual void PasteIntoNode(const std::u16string& text);

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
  // Called when the currently executing operations completes.
  void OnOperationComplete(base::OnceClosure on_commit_complete);

  content::RenderWidgetHost* GetRenderWidgetHost() const;

  TargetDetails target_details_;
  std::u16string last_sent_composition_;
  bool has_lost_focus_during_composition_ = false;
  bool paste_fallback_required_ = false;

  bool is_waiting_on_operation_completion_ = false;
  std::optional<QueuedOperation> queued_operation_;

  base::WeakPtrFactory<Target> weak_factory_{this};
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_TARGET_H_
