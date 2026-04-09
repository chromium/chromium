// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RECORD_REPLAY_CONTENT_ELEMENT_ID_H_
#define CHROME_BROWSER_RECORD_REPLAY_CONTENT_ELEMENT_ID_H_

#include <string>

#include "chrome/browser/record_replay/element_id.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace record_replay {

// Extends ElementId with a blink::LocalFrameToken for use in content-specific
// code.
class ContentElementId : public ElementId {
 public:
  ContentElementId(blink::LocalFrameToken frame_token, DomNodeId dom_node_id);
  ContentElementId(const ContentElementId&);
  ContentElementId& operator=(const ContentElementId&);
  ~ContentElementId() override;

  const blink::LocalFrameToken& frame_token() const { return frame_token_; }

  std::string ToString() const override;

 private:
  blink::LocalFrameToken frame_token_;
};

}  // namespace record_replay

#endif  // CHROME_BROWSER_RECORD_REPLAY_CONTENT_ELEMENT_ID_H_
