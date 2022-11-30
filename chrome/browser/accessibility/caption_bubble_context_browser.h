// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_CAPTION_BUBBLE_CONTEXT_BROWSER_H_
#define CHROME_BROWSER_ACCESSIBILITY_CAPTION_BUBBLE_CONTEXT_BROWSER_H_

#include <memory>

#include "components/live_caption/caption_bubble_context.h"

namespace content {
class WebContents;
}

namespace captions {

///////////////////////////////////////////////////////////////////////////////
// Caption Bubble Context Implementation for Browser
//
//  Browser implementation of Caption Bubble Context which takes a WebContents
//  as a constructor parameter.
//
class CaptionBubbleContextBrowser : public CaptionBubbleContext {
 public:
  explicit CaptionBubbleContextBrowser(content::WebContents* web_contents) {}
  ~CaptionBubbleContextBrowser() override = default;
  CaptionBubbleContextBrowser(const CaptionBubbleContextBrowser&) = delete;
  CaptionBubbleContextBrowser& operator=(const CaptionBubbleContextBrowser&) =
      delete;

  static std::unique_ptr<CaptionBubbleContextBrowser> Create(
      content::WebContents* web_contents);
};

}  // namespace captions

#endif  // CHROME_BROWSER_ACCESSIBILITY_CAPTION_BUBBLE_CONTEXT_BROWSER_H_
