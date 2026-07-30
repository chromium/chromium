// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_TARGET_H_
#define CHROME_BROWSER_DICTATION_TARGET_H_

#include <string>
#include <vector>

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
  void CommitComposition(const std::u16string& text);

 protected:
  virtual void SetExternallySourcedComposition(
      const std::u16string& text,
      const std::vector<ui::ImeTextSpan>& spans);
  virtual void CommitExternallySourcedComposition(const std::u16string& text);
  virtual void PasteIntoNode(const std::u16string& text);

 private:
  content::RenderWidgetHost* GetRenderWidgetHost() const;

  TargetDetails target_details_;
  std::u16string last_sent_composition_;
  bool has_lost_focus_during_composition_ = false;
  bool paste_fallback_required_ = false;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_TARGET_H_
