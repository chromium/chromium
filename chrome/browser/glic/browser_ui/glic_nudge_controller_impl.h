// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_NUDGE_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_NUDGE_CONTROLLER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_controller.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/tab_list/tab_list_interface_observer.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;
class TabListInterface;
class ScopedCallToActionLock;

namespace glic {

class GlicSplitButtonController;
class GlicSplitButtonDelegate;

class GlicNudgeControllerImpl : public GlicNudgeController,
                                public TabListInterfaceObserver {
 public:
  explicit GlicNudgeControllerImpl(
      BrowserWindowInterface* browser_window_interface,
      GlicSplitButtonController* split_button_controller);
  GlicNudgeControllerImpl(const GlicNudgeControllerImpl&) = delete;
  GlicNudgeControllerImpl& operator=(const GlicNudgeControllerImpl&) = delete;
  ~GlicNudgeControllerImpl() override;

  // GlicNudgeController:
  void UpdateNudgeLabel(content::WebContents* web_contents,
                        const std::string& nudge_label,
                        std::optional<std::string> prompt_suggestion,
                        std::optional<GlicNudgeActivity> activity,
                        GlicNudgeActivityCallback callback) override;
  void OnNudgeActivity(GlicNudgeActivity activity) override;

  // TabListInterfaceObserver:
  void OnActiveTabChanged(TabListInterface& tab_list,
                          tabs::TabInterface* tab) override;
  void OnTabListActiveChanged(TabListInterface& tab_list,
                              bool is_active) override;
  void OnTabListDestroyed(TabListInterface& tab_list) override;

  // TODO(crbug.com/511309088): Remove and have callers do this directly on the
  // split button controller.
  void SetHorizontalTabsDelegate(GlicSplitButtonDelegate* delegate) override;
  void SetVerticalTabsDelegate(GlicSplitButtonDelegate* delegate) override;

  void SetNudgeActivityCallbackForTesting();

  std::optional<std::string> GetPromptSuggestion() override;
  void ClearPromptSuggestion() override;

  base::WeakPtr<GlicNudgeController> GetWeakPtr() override;

 private:
  void HideNudge(GlicNudgeActivity activity);
  TabListInterface* GetTabList();

  const raw_ptr<BrowserWindowInterface> browser_window_interface_;
  const raw_ptr<GlicSplitButtonController> split_button_controller_;
  tabs::TabHandle nudged_tab_handle_;

  std::optional<std::string> prompt_suggestion_;
  GlicNudgeActivityCallback nudge_activity_callback_;

  base::ScopedObservation<TabListInterface, TabListInterfaceObserver>
      tab_list_observation_{this};
  std::unique_ptr<ScopedCallToActionLock> scoped_call_to_action_lock_;

  ui::ScopedUnownedUserData<GlicNudgeController> scoped_unowned_user_data_;
  base::WeakPtrFactory<GlicNudgeController> weak_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_NUDGE_CONTROLLER_IMPL_H_
