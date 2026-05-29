// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_NUDGE_CONTROLLER_DESKTOP_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_NUDGE_CONTROLLER_DESKTOP_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_controller.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/tab_list/tab_list_interface_observer.h"

class BrowserWindowInterface;
class TabListInterface;
class ScopedCallToActionLock;

namespace glic {

class GlicNudgeDelegate;

class GlicNudgeControllerDesktop : public GlicNudgeController,
                                   public TabListInterfaceObserver {
 public:
  GlicNudgeControllerDesktop(BrowserWindowInterface* browser_window_interface,
                             TabListInterface* tab_list);
  GlicNudgeControllerDesktop(const GlicNudgeControllerDesktop&) = delete;
  GlicNudgeControllerDesktop& operator=(const GlicNudgeControllerDesktop&) =
      delete;
  ~GlicNudgeControllerDesktop() override;

  // GlicNudgeController:
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

  void SetTabStripDelegate(GlicNudgeDelegate* delegate) override;
  void SetToolbarDelegate(GlicNudgeDelegate* delegate) override;

  void SetNudgeActivityCallbackForTesting();

  std::optional<std::string> GetPromptSuggestion() override;
  void ClearPromptSuggestion() override;

 private:
  GlicNudgeDelegate* GetActiveDelegate();

  const raw_ptr<BrowserWindowInterface> browser_window_interface_;
  const raw_ptr<TabListInterface> tab_list_;

  raw_ptr<GlicNudgeDelegate> tab_strip_delegate_ = nullptr;
  raw_ptr<GlicNudgeDelegate> toolbar_delegate_ = nullptr;

  std::unique_ptr<GlicNudgeDelegate> anchored_nudge_controller_;
  std::optional<std::string> prompt_suggestion_;
  GlicNudgeActivityCallback nudge_activity_callback_;

  base::ScopedObservation<TabListInterface, TabListInterfaceObserver>
      tab_list_observation_{this};
  std::unique_ptr<ScopedCallToActionLock> scoped_call_to_action_lock_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_NUDGE_CONTROLLER_DESKTOP_H_
