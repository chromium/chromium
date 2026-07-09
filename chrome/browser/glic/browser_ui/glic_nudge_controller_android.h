// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_NUDGE_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_NUDGE_CONTROLLER_ANDROID_H_

#include "base/scoped_observation.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_controller.h"
#include "chrome/browser/tab_list/tab_list_interface_observer.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;
class TabListInterface;

namespace content {
class WebContents;
}

namespace tabs {
class TabInterface;
}

namespace glic {

class GlicNudgeControllerAndroid : public GlicNudgeController,
                                   public TabListInterfaceObserver {
 public:
  explicit GlicNudgeControllerAndroid(BrowserWindowInterface* browser);
  GlicNudgeControllerAndroid(const GlicNudgeControllerAndroid&) = delete;
  GlicNudgeControllerAndroid& operator=(const GlicNudgeControllerAndroid&) =
      delete;
  ~GlicNudgeControllerAndroid() override;

  // GlicNudgeController:
  void SetTabStripDelegate(GlicSplitButtonDelegate* delegate) override;
  void SetToolbarDelegate(GlicSplitButtonDelegate* delegate) override;

  void UpdateNudgeLabel(content::WebContents* web_contents,
                        const std::string& nudge_label,
                        std::optional<std::string> prompt_suggestion,
                        std::optional<GlicNudgeActivity> activity,
                        GlicNudgeActivityCallback callback) override;
  void OnNudgeActivity(GlicNudgeActivity activity) override;

  std::optional<std::string> GetPromptSuggestion() override;
  void ClearPromptSuggestion() override;

 private:
  // TabListInterfaceObserver:
  void OnActiveTabChanged(TabListInterface& tab_list,
                          tabs::TabInterface* tab) override;
  void OnTabListDestroyed(TabListInterface& tab_list) override;

  TabListInterface* GetTabList();
  base::ScopedObservation<TabListInterface, TabListInterfaceObserver>
      tab_list_observation_{this};
  tabs::TabHandle nudged_tab_handle_;
  raw_ptr<GlicSplitButtonDelegate> tab_strip_delegate_ = nullptr;
  std::optional<std::string> prompt_suggestion_;
  GlicNudgeActivityCallback nudge_activity_callback_;
  std::unique_ptr<GlicSplitButtonDelegate> delegate_;
  raw_ptr<BrowserWindowInterface> browser_ = nullptr;

  ui::ScopedUnownedUserData<GlicNudgeController> scoped_unowned_user_data_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_NUDGE_CONTROLLER_ANDROID_H_
