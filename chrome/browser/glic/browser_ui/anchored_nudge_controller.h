// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_ANCHORED_NUDGE_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_ANCHORED_NUDGE_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_delegate.h"

class BrowserWindowInterface;

namespace page_actions {
class PageActionController;
}

namespace glic {

// Controller for the "anchored message" variant of the contextual cue nudge.
class AnchoredNudgeController : public GlicNudgeDelegate {
 public:
  explicit AnchoredNudgeController(
      BrowserWindowInterface& browser_window_interface);
  ~AnchoredNudgeController() override;

  // GlicNudgeDelegate:
  void OnTriggerGlicNudgeUI(NudgeParams params) override;
  void OnHideGlicNudgeUI() override;
  bool GetIsShowingGlicNudge() override;

 private:
  page_actions::PageActionController* GetPageActionController();

  raw_ref<BrowserWindowInterface> browser_window_interface_;
  // Tracks the page-action subscription for the anchored contextual cue.
  // Reset when the anchored message is hidden or replaced.
  base::CallbackListSubscription anchored_message_subscription_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_ANCHORED_NUDGE_CONTROLLER_H_
