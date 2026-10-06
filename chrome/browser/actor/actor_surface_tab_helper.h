// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_SURFACE_TAB_HELPER_H_
#define CHROME_BROWSER_ACTOR_ACTOR_SURFACE_TAB_HELPER_H_

#include "base/memory/weak_ptr.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace actor {

class ActorSurfaceRegistry;

// Per-tab feature guaranteeing that the tab has an ActorSurface in
// ActorSurfaceRegistry for its lifetime. Owned by TabFeatures.
class ActorSurfaceTabHelper {
 public:
  explicit ActorSurfaceTabHelper(tabs::TabInterface& tab);
  ActorSurfaceTabHelper(const ActorSurfaceTabHelper&) = delete;
  ActorSurfaceTabHelper& operator=(const ActorSurfaceTabHelper&) = delete;
  ~ActorSurfaceTabHelper();

  DECLARE_USER_DATA(ActorSurfaceTabHelper);
  static ActorSurfaceTabHelper* From(tabs::TabInterface* tab);

 private:
  const tabs::TabHandle tab_;
  base::WeakPtr<ActorSurfaceRegistry> registry_;
  ::ui::ScopedUnownedUserData<ActorSurfaceTabHelper> scoped_unowned_user_data_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_SURFACE_TAB_HELPER_H_
