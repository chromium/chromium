// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_SPLIT_BUTTON_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_SPLIT_BUTTON_CONTROLLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class ActorTaskListBubbleController;
class BrowserWindowInterface;

namespace glic {

class GlicActorNudgeController;
class GlicButtonController;
class GlicKeyedService;
class GlicNudgeController;
class GlicSplitButtonDelegate;

class GlicSplitButtonController {
 public:
  DECLARE_USER_DATA(GlicSplitButtonController);

  static GlicSplitButtonController* From(BrowserWindowInterface* browser);

  GlicSplitButtonController(BrowserWindowInterface* browser,
                            GlicKeyedService* glic_service);

  GlicSplitButtonController(const GlicSplitButtonController&) = delete;
  GlicSplitButtonController& operator=(const GlicSplitButtonController&) =
      delete;

  ~GlicSplitButtonController();

  void SetHorizontalTabsDelegate(GlicSplitButtonDelegate* delegate);
  void SetVerticalTabsDelegate(GlicSplitButtonDelegate* delegate);
  base::WeakPtr<GlicSplitButtonController> GetWeakPtr();

  GlicNudgeController* nudge_controller() {
    return glic_nudge_controller_.get();
  }
#if !BUILDFLAG(IS_ANDROID)
  GlicButtonController* button_controller() {
    return glic_button_controller_.get();
  }
  GlicActorNudgeController* actor_nudge_controller() {
    return glic_actor_nudge_controller_.get();
  }
#endif

 private:
  std::unique_ptr<GlicNudgeController> glic_nudge_controller_;
#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<GlicButtonController> glic_button_controller_;
  std::unique_ptr<ActorTaskListBubbleController>
      actor_task_list_bubble_controller_;
  std::unique_ptr<GlicActorNudgeController> glic_actor_nudge_controller_;
#endif

  ui::ScopedUnownedUserData<GlicSplitButtonController>
      scoped_unowned_user_data_;

  base::WeakPtrFactory<GlicSplitButtonController> weak_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_SPLIT_BUTTON_CONTROLLER_H_
