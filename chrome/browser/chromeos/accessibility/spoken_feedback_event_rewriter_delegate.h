// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SPOKEN_FEEDBACK_EVENT_REWRITER_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SPOKEN_FEEDBACK_EVENT_REWRITER_DELEGATE_H_

#include <memory>

#include "ash/public/cpp/spoken_feedback_event_rewriter_delegate.h"
#include "base/macros.h"
#include "content/public/browser/web_contents_delegate.h"

// Passes key events from Ash's EventRewriter to the ChromeVox extension code.
// Reports ChromeVox's unhandled key events back to Ash for continued dispatch.
// TODO(http://crbug.com/839541): Avoid reposting unhandled events.
class SpokenFeedbackEventRewriterDelegate
    : public ash::SpokenFeedbackEventRewriterDelegate,
      public content::WebContentsDelegate {
 public:
  SpokenFeedbackEventRewriterDelegate();
  ~SpokenFeedbackEventRewriterDelegate() override;

  // ash::SpokenFeedbackEventRewriterDelegate:
  void DispatchKeyEventToChromeVox(std::unique_ptr<ui::Event> event,
                                   bool capture) override;
  void DispatchMouseEventToChromeVox(std::unique_ptr<ui::Event> event) override;

 private:
  // Reports unhandled key events to the EventRewriterController for dispatch.
  void OnUnhandledSpokenFeedbackEvent(std::unique_ptr<ui::Event> event) const;

  // WebContentsDelegate:
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;

  DISALLOW_COPY_AND_ASSIGN(SpokenFeedbackEventRewriterDelegate);
};

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SPOKEN_FEEDBACK_EVENT_REWRITER_DELEGATE_H_
