// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_ACTOR_NUDGE_DELEGATE_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_ACTOR_NUDGE_DELEGATE_H_

#include <string>

namespace glic {

// Interface to be implemented by UI classes that present the actor icon and
// nudge.
class GlicActorNudgeDelegate {
 public:
  virtual ~GlicActorNudgeDelegate();

  // Show the actor icon with no nudge text.
  virtual void ShowGlicActorTaskIcon() = 0;

  // Hide the actor icon.
  virtual void HideGlicActorTaskIcon() = 0;

  // Returns true if the actor icon is showing with nudge text.
  virtual bool GetIsShowingGlicActorTaskIconNudge() = 0;

  // Returns true if the glic button and actor button exist.
  virtual bool IsGlicAdded() = 0;

  // Update the nudge label.
  virtual void SetGlicActorNudgeLabel(const std::u16string& nudge_label) = 0;

  // Show the actor nudge with text.
  virtual void TriggerGlicActorNudge(const std::u16string& nudge_text) = 0;

  // Update the nudge button "pressed state".
  virtual void SetGlicActorNudgePressedState(bool pressed) = 0;

  // Show the task list bubble anchored to the button.
  virtual void ShowActorTaskListBubble() = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_ACTOR_NUDGE_DELEGATE_H_
