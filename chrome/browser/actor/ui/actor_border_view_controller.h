// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_BORDER_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_BORDER_VIEW_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace tabs {
class TabInterface;
}

class BrowserWindowInterface;

// Controller for the border view of the Actor UI.
class ActorBorderViewController {
 public:
  DECLARE_USER_DATA(ActorBorderViewController);
  explicit ActorBorderViewController(
      BrowserWindowInterface* browser_window_interface);
  virtual ~ActorBorderViewController();

  static ActorBorderViewController* From(
      BrowserWindowInterface* browser_window_interface);
  ActorBorderViewController(const ActorBorderViewController&) = delete;
  ActorBorderViewController& operator=(const ActorBorderViewController&) =
      delete;

  // Registers a callback to be invoked when the actor border glow changes.
  using ActorBorderGlowUpdatedCallback =
      base::RepeatingCallback<void(tabs::TabInterface* tab, bool)>;

  base::CallbackListSubscription AddOnActorBorderGlowUpdatedCallback(
      ActorBorderGlowUpdatedCallback callback);

  void SetGlowEnabled(tabs::TabInterface* tab, bool enabled);

 private:
  base::RepeatingCallbackList<void(tabs::TabInterface* tab, bool)>
      on_actor_border_glow_updated_callback_list_;
  ::ui::ScopedUnownedUserData<ActorBorderViewController> scoped_data_holder_;
};

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_BORDER_VIEW_CONTROLLER_H_
