// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_INTERFACE_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_INTERFACE_H_

#include "base/functional/callback.h"
#include "chrome/browser/actor/ui/ui_event.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/buildflags.h"

namespace actor::ui {
using UiCompleteCallback =
    base::OnceCallback<void(::actor::mojom::ActionResultPtr)>;

class ActorUiStateManagerInterface {
 public:
  virtual ~ActorUiStateManagerInterface() = default;

  // Handles a UiEvent that may be processed asynchronously.
  virtual void OnUiEvent(AsyncUiEvent event, UiCompleteCallback callback) = 0;
  // Handles a UiEvent that must be processed synchronously.
  virtual void OnUiEvent(SyncUiEvent event) = 0;

#if !BUILDFLAG(IS_ANDROID)
  // Lazily initializes the tab strip tracker when the first actor task is
  // created.
  // Since ActorKeyedService is constructed during Profile/TestingProfile
  // instantiation, eager initialization in the constructor would invoke
  // BrowserTabStripTracker::Init() immediately. This triggers crashes in
  // headless environments or non-UI/backend unit tests (e.g., due to a missing
  // DeviceDataManager or X server/$DISPLAY) that instantiate a profile without
  // a complete UI environment. Delaying until the first task ensures the UI
  // environment is fully set up.
  virtual void LazyInitTabTracker() = 0;
#endif

};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_INTERFACE_H_
