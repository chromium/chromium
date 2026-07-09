// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_TARGET_H_
#define CHROME_BROWSER_DICTATION_TARGET_H_

#include <string>

#include "content/public/browser/global_dom_node_id.h"

namespace content {
class RenderFrameHost;
class RenderWidgetHost;
}  // namespace content

namespace dictation {

// Represents a dictation target into which transcriptions will be written.
class Target {
 public:
  Target();
  explicit Target(const content::GlobalDOMNodeId& target_id);
  virtual ~Target();

  // Returns the RenderFrameHost associated with this target, or nullptr if it
  // no longer exists.
  content::RenderFrameHost* GetRenderFrameHost() const;

  const content::GlobalDOMNodeId& global_dom_node_id() const {
    return target_id_;
  }

  // Sets the composition text in the target.
  void SetComposition(const std::u16string& text, bool is_final);

  // Commits the text in the target.
  void CommitComposition(const std::u16string& text);

 private:
  content::RenderWidgetHost* GetRenderWidgetHost() const;

  content::GlobalDOMNodeId target_id_;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_TARGET_H_
