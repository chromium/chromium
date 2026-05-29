// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_NUDGE_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_NUDGE_CONTROLLER_ANDROID_H_

#include "chrome/browser/glic/browser_ui/glic_nudge_controller.h"

namespace glic {

class GlicNudgeControllerAndroid : public GlicNudgeController {
 public:
  GlicNudgeControllerAndroid();
  GlicNudgeControllerAndroid(const GlicNudgeControllerAndroid&) = delete;
  GlicNudgeControllerAndroid& operator=(const GlicNudgeControllerAndroid&) =
      delete;
  ~GlicNudgeControllerAndroid() override;

  // GlicNudgeController:
  void SetTabStripDelegate(GlicNudgeDelegate* delegate) override;
  void SetToolbarDelegate(GlicNudgeDelegate* delegate) override;

  void UpdateNudgeLabel(content::WebContents* web_contents,
                        const std::string& nudge_label,
                        std::optional<std::string> prompt_suggestion,
                        const std::string& anchored_message_text,
                        std::optional<GlicNudgeActivity> activity,
                        GlicNudgeActivityCallback callback) override;
  void OnNudgeActivity(GlicNudgeActivity activity) override;
  std::optional<std::string> GetPromptSuggestion() override;
  void ClearPromptSuggestion() override;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_NUDGE_CONTROLLER_ANDROID_H_
