// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_util.h"

#include "chrome/browser/actor/actor_keyed_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

namespace actor {

bool IsActorOperatingOnWebContents(content::BrowserContext* context,
                                   content::WebContents* wc) {
  auto* actor_service = ActorKeyedService::Get(context);
  if (!actor_service) {
    return false;
  }

  const auto* tab_interface = tabs::TabInterface::MaybeGetFromContents(wc);
  return tab_interface && actor_service->IsAnyTaskActingOnTab(*tab_interface);
}

}  // namespace actor
