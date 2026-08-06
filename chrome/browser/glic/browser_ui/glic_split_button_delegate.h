// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_SPLIT_BUTTON_DELEGATE_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_SPLIT_BUTTON_DELEGATE_H_

#include <optional>
#include <string>

#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_controller_delegate.h"

namespace glic {

// Details needed for showing a nudge.
struct NudgeParams {
  ~NudgeParams();
  NudgeParams(NudgeParams&&);
  NudgeParams& operator=(NudgeParams&&);

  explicit NudgeParams(std::string label);
  explicit NudgeParams(std::string label,
                       std::string anchored_message_text,
                       std::optional<std::string> prompt_suggestion);

  // Action label. This string appears on the clickable part of the nudge.
  std::string label;

  // Longer description, shown in the anchored message UI.
  std::string anchored_message_text;

  // Optional prompt to be filled in to Glic if the nudge is clicked.
  std::optional<std::string> prompt_suggestion;
};

// Delegate interface for the UI container that houses GlicButton and
// GlicActorTaskIcon.
class GlicSplitButtonDelegate : public ActorTaskListBubbleControllerDelegate {
 public:
  ~GlicSplitButtonDelegate() override;

  // Methods related to glic nudge.

  // Show the glic nudge.
  virtual void OnTriggerGlicNudgeUI(NudgeParams params);
  // Called when the glic nudge UI needs to be hidden.
  virtual void OnHideGlicNudgeUI();
  // Called when we want to check if the UI is currently showing.
  virtual bool GetIsShowingGlicNudge();

  // Methods related to actor task icon visibility, nudge, highlight, and
  // task list bubble.

  // Show the actor icon with no nudge text.
  virtual void ShowGlicActorTaskIcon();

  // Hide the actor icon.
  virtual void HideGlicActorTaskIcon();

  // Returns true if the actor icon is showing with nudge text.
  virtual bool GetIsShowingGlicActorTaskIconNudge();

  // Update the nudge label.
  virtual void SetGlicActorNudgeLabel(const std::u16string& nudge_label);

  // Show the actor nudge with text.
  virtual void TriggerGlicActorNudge(const std::u16string& nudge_text);

  // Update the nudge button "pressed state".
  virtual void SetGlicActorNudgePressedState(bool pressed);

  // ActorTaskListBubbleControllerDelegate:
  void ShowActorTaskListBubble() override;
  void CloseActorTaskListBubble() override;
  bool IsActorTaskListBubbleShowing() override;

  // Methods related to glic button visibility and glic panel visibility

  // Set the show state of the button
  virtual void SetGlicShowState(bool show);

  // Update the button when glic panel shows or hides.
  virtual void SetGlicPanelIsOpen(bool open);
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_SPLIT_BUTTON_DELEGATE_H_
