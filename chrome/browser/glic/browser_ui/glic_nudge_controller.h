// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_NUDGE_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_NUDGE_CONTROLLER_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"

namespace content {
class WebContents;
}  // namespace content

namespace glic {

// Enumerates the various user action for the Glic nudge.
enum class GlicNudgeActivity {
  kNudgeShown,
  kNudgeClicked,
  kNudgeDismissed,
  kNudgeNotShownWebContents,
  kNudgeIgnoredActiveTabChanged,
  kNudgeIgnoredNavigation,
  kNudgeNotShownWindowCallToActionUI,
  kNudgeIgnoredOpenedContextualTasksSidePanel,
  kNudgeIgnoredOmniboxContextMenuInteraction,
};

class GlicNudgeDelegate;

// Interface for the controller that mediates Glic Nudges.
class GlicNudgeController {
 public:
  using GlicNudgeActivityCallback =
      base::RepeatingCallback<void(GlicNudgeActivity)>;

  virtual ~GlicNudgeController();

  virtual void SetTabStripDelegate(GlicNudgeDelegate* delegate) = 0;
  virtual void SetToolbarDelegate(GlicNudgeDelegate* delegate) = 0;

  // Updates the `nudge_label` for `web_contents`, if the WebContents is active.
  // The nudge will be removed from `web_contents` if `nudge_label` is empty.
  // `activity` must be supplied iff. `nudge_label` is empty, to identify the
  // reason of nudge removal.
  virtual void UpdateNudgeLabel(content::WebContents* web_contents,
                                const std::string& nudge_label,
                                std::optional<std::string> prompt_suggestion,
                                const std::string& anchored_message_text,
                                std::optional<GlicNudgeActivity> activity,
                                GlicNudgeActivityCallback callback) = 0;

  virtual void OnNudgeActivity(GlicNudgeActivity activity) = 0;

  virtual std::optional<std::string> GetPromptSuggestion() = 0;
  virtual void ClearPromptSuggestion() = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_NUDGE_CONTROLLER_H_
