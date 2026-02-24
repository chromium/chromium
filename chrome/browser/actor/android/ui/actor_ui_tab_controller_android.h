// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ANDROID_UI_ACTOR_UI_TAB_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_ACTOR_ANDROID_UI_ACTOR_UI_TAB_CONTROLLER_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace tabs {
class TabInterface;
}

namespace actor {
class ActorKeyedService;
}

namespace actor::ui {

class ActorUiTabControllerAndroid : public ActorUiTabControllerInterface {
 public:
  ActorUiTabControllerAndroid(tabs::TabInterface& tab,
                              ActorKeyedService* actor_keyed_service);
  ~ActorUiTabControllerAndroid() override;
  DECLARE_USER_DATA(ActorUiTabControllerAndroid);

  // ActorUiTabControllerInterface implementation.
  void OnUiTabStateChange(const UiTabState& ui_tab_state,
                          UiResultCallback callback) override;
  void SetActorTaskPaused() override;
  void SetActorTaskResume() override;
  base::WeakPtr<ActorUiTabControllerInterface> GetWeakPtr() override;
  UiTabState GetCurrentUiTabState() const override;

 private:
  const raw_ref<tabs::TabInterface> tab_;
  const raw_ref<ActorKeyedService> actor_keyed_service_;
  UiTabState current_ui_tab_state_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  ::ui::ScopedUnownedUserData<ActorUiTabControllerAndroid>
      scoped_unowned_user_data_;

  base::WeakPtrFactory<ActorUiTabControllerAndroid> weak_factory_{this};
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_ANDROID_UI_ACTOR_UI_TAB_CONTROLLER_ANDROID_H_
