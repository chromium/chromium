// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_NUDGE_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_NUDGE_CONTROLLER_ANDROID_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_controller.h"
#include "chrome/browser/tab_list/tab_list_interface_observer.h"

class TabListInterface;

namespace glic {

class GlicNudgeControllerAndroid : public GlicNudgeController,
                                   public TabListInterfaceObserver {
 public:
  explicit GlicNudgeControllerAndroid(TabListInterface* tab_list);
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

  // TabListInterfaceObserver:
  void OnActiveTabChanged(TabListInterface& tab_list,
                          tabs::TabInterface* tab) override;
  std::optional<std::string> GetPromptSuggestion() override;
  void ClearPromptSuggestion() override;

 private:
  raw_ptr<TabListInterface> tab_list_ = nullptr;
  base::ScopedObservation<TabListInterface, TabListInterfaceObserver>
      tab_list_observation_{this};
  raw_ptr<GlicNudgeDelegate> tab_strip_delegate_ = nullptr;
  std::optional<std::string> prompt_suggestion_;
  GlicNudgeActivityCallback nudge_activity_callback_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_NUDGE_CONTROLLER_ANDROID_H_
