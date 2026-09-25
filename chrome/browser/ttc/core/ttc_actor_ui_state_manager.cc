// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/ttc_actor_ui_state_manager.h"

#include <utility>

#include "build/build_config.h"
#include "chrome/common/actor/action_result.h"

namespace ttc {

TtcActorUiStateManager::TtcActorUiStateManager() = default;

TtcActorUiStateManager::~TtcActorUiStateManager() = default;

void TtcActorUiStateManager::OnUiEvent(actor::ui::AsyncUiEvent event,
                                       actor::ui::UiCompleteCallback callback) {
  std::move(callback).Run(actor::MakeOkResult());
}

void TtcActorUiStateManager::OnUiEvent(actor::ui::SyncUiEvent event) {}

#if !BUILDFLAG(IS_ANDROID)
void TtcActorUiStateManager::LazyInitTabTracker() {}
#endif

}  // namespace ttc
