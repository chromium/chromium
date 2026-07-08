// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_TARGET_H_
#define CHROME_BROWSER_DICTATION_TARGET_H_

#include <string>

#include "content/public/browser/weak_document_ptr.h"

namespace content {
class RenderFrameHost;
class RenderWidgetHost;
}  // namespace content

namespace dictation {

struct TargetId {
  content::WeakDocumentPtr document;
};

// Represents a dictation target into which transcriptions will be written.
class Target {
 public:
  Target();
  explicit Target(const TargetId& target_id);
  virtual ~Target();

  // Returns the RenderFrameHost associated with this target, or nullptr if it
  // no longer exists.
  content::RenderFrameHost* GetRenderFrameHost() const;

  // Sets the composition text in the target.
  void SetComposition(const std::u16string& text, bool is_final);

  // Commits the text in the target.
  void CommitComposition(const std::u16string& text);

 private:
  content::RenderWidgetHost* GetRenderWidgetHost() const;

  TargetId target_id_;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_TARGET_H_
