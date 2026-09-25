// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_TTC_ACTOR_UI_STATE_MANAGER_H_
#define CHROME_BROWSER_TTC_CORE_TTC_ACTOR_UI_STATE_MANAGER_H_

#include "build/build_config.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"

namespace ttc {

// TTC specific implementation of ActorUiStateManagerInterface. Actor tasks
// created by TTC use this instead of the profile-wide ActorUiStateManager so
// that TTC can drive its own actor UI.
class TtcActorUiStateManager : public actor::ui::ActorUiStateManagerInterface {
 public:
  TtcActorUiStateManager();
  TtcActorUiStateManager(const TtcActorUiStateManager&) = delete;
  TtcActorUiStateManager& operator=(const TtcActorUiStateManager&) = delete;
  ~TtcActorUiStateManager() override;

  // actor::ui::ActorUiStateManagerInterface:
  void OnUiEvent(actor::ui::AsyncUiEvent event,
                 actor::ui::UiCompleteCallback callback) override;
  void OnUiEvent(actor::ui::SyncUiEvent event) override;
#if !BUILDFLAG(IS_ANDROID)
  void LazyInitTabTracker() override;
#endif
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_TTC_ACTOR_UI_STATE_MANAGER_H_
