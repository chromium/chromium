// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_NUDGE_DELEGATE_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_NUDGE_DELEGATE_H_

#include <optional>
#include <string>

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

class GlicNudgeDelegate {
 public:
  virtual ~GlicNudgeDelegate();

  // Show the glic nudge.
  virtual void OnTriggerGlicNudgeUI(NudgeParams params) = 0;
  // Called when the glic nudge UI needs to be hidden.
  virtual void OnHideGlicNudgeUI() = 0;
  // Called when we want to check if the UI is currently showing.
  virtual bool GetIsShowingGlicNudge() = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_NUDGE_DELEGATE_H_
