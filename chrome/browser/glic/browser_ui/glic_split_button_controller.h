// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_SPLIT_BUTTON_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_SPLIT_BUTTON_CONTROLLER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/browser/glic/browser_ui/glic_split_button_ui_state.h"
#include "chrome/browser/glic/host/glic.mojom-shared.h"
#include "chrome/browser/glic/public/service/glic_activity_types.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/tab_list/tab_list_interface_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;

namespace glic {

class GlicActivityListBubbleController;
class GlicActorNudgeController;
class GlicButtonController;
class GlicKeyedService;
class GlicNudgeController;
class GlicSplitButtonDelegate;

// Window-scoped controller that arbitrates primary and secondary split button
// actions and coordinates UI updates across platforms.
class GlicSplitButtonController : public TabListInterfaceObserver {
 public:
  DECLARE_USER_DATA(GlicSplitButtonController);

  static GlicSplitButtonController* From(BrowserWindowInterface* browser);

  GlicSplitButtonController(BrowserWindowInterface* browser,
                            GlicKeyedService* glic_service);

  GlicSplitButtonController(const GlicSplitButtonController&) = delete;
  GlicSplitButtonController& operator=(const GlicSplitButtonController&) =
      delete;

  ~GlicSplitButtonController() override;

  // TODO(crbug.com/511309088): Rename these to toolbar and tab strip delegate
  // since they no longer necessarily correspond to vertical tab mode.
  void SetHorizontalTabsDelegate(GlicSplitButtonDelegate* delegate);
  void SetVerticalTabsDelegate(GlicSplitButtonDelegate* delegate);
  base::WeakPtr<GlicSplitButtonController> GetWeakPtr();

  void OnGlicButtonClicked();

  void OnSecondaryButtonClicked();
  void UpdateSplitButtonState();

  const GlicSplitButtonUiState& current_ui_state() const {
    return current_ui_state_;
  }

  // TabListInterfaceObserver:
  void OnActiveTabChanged(TabListInterface& tab_list,
                          tabs::TabInterface* tab) override;
  void OnTabListDestroyed(TabListInterface& tab_list) override;

  virtual GlicSplitButtonDelegate* GetActiveDelegate();
  void CallOnBoth(base::RepeatingCallback<void(GlicSplitButtonDelegate&)> fn);

  virtual void ShowBubble();
  virtual void CloseBubble();

  GlicNudgeController* nudge_controller() {
    return glic_nudge_controller_.get();
  }
  GlicActorNudgeController* actor_nudge_controller() {
    return glic_actor_nudge_controller_.get();
  }
  GlicActivityListBubbleController* activity_list_bubble_controller() {
    return activity_list_bubble_controller_.get();
  }
  void SetActorNudgeControllerForTesting(
      std::unique_ptr<GlicActorNudgeController> controller);

 private:
  void InitActivityHub();
  GlicSplitButtonUiState ComputeSplitButtonUiState() const;
  bool IsToolbarButton() const;
  mojom::InvocationSource GetInvocationSource(
      GlicSplitButtonDelegate& delegate) const;

  void OnActivityNudgeStateChanged(bool show_bubble,
                                   GlicActivityNudgeState state);
  void OnBubbleVisibilityChange(bool is_bubble_open);

  const raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<GlicSplitButtonDelegate> horizontal_tabs_delegate_ = nullptr;
  raw_ptr<GlicSplitButtonDelegate> vertical_tabs_delegate_ = nullptr;
  raw_ptr<GlicKeyedService> glic_service_ = nullptr;

  std::unique_ptr<GlicNudgeController> glic_nudge_controller_;
  std::unique_ptr<GlicButtonController> glic_button_controller_;
  std::unique_ptr<GlicActivityListBubbleController>
      activity_list_bubble_controller_;
  std::unique_ptr<GlicActorNudgeController> glic_actor_nudge_controller_;

  GlicSplitButtonUiState current_ui_state_;

  base::ScopedObservation<TabListInterface, TabListInterfaceObserver>
      tab_list_observation_{this};
  base::CallbackListSubscription activity_subscription_;
  std::vector<base::CallbackListSubscription>
      bubble_visibility_change_subscription_;

  ui::ScopedUnownedUserData<GlicSplitButtonController>
      scoped_unowned_user_data_;

  base::WeakPtrFactory<GlicSplitButtonController> weak_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_SPLIT_BUTTON_CONTROLLER_H_
