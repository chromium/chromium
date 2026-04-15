// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_util.h"

#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

namespace actor {

bool HaveActiveTaskForContents(content::WebContents* source_contents) {
  if (!source_contents) {
    return false;
  }

  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(source_contents->GetBrowserContext());
  if (!actor_service) {
    return false;
  }

  return actor_service->GetActingActorTaskForWebContents(source_contents);
}

bool IsRunningBackgroundActorTask(content::WebContents& source_contents) {
  if (!HaveActiveTaskForContents(&source_contents)) {
    return false;
  }

  tabs::TabInterface* task_tab =
      tabs::TabInterface::GetFromContents(&source_contents);
  if (task_tab->IsActivated()) {
    // The tab in which the task is running is active, so the task isn't in the
    // background.
    return false;
  }

  // Determine whether the active tab is showing the conversation instance of
  // the actor task. If the conversation is showing, consider the task to be in
  // the foreground.

  glic::GlicKeyedService* glic_service =
      glic::GlicKeyedService::Get(source_contents.GetBrowserContext());
  if (!glic_service) {
    // Without Glic, there'd be no conversation to provide context to the user
    // while the tab isn't active, so consider the task backgrounded.
    return true;
  }

  const glic::GlicInstance* task_instance =
      glic_service->GetInstanceForTab(task_tab);
  if (!task_instance) {
    // There's no Glic instance for the tab running the task (e.g. something
    // else is invoking the actor framework). As above, there's no conversation,
    // so consider the task backgrounded.
    return true;
  }
  BrowserWindowInterface* task_window = task_tab->GetBrowserWindowInterface();
  const glic::GlicInstance* active_instance =
      glic_service->GetInstanceForActiveTab(task_window);
  if (task_instance != active_instance) {
    // The active tab has no Glic instance or an unrelated instance. The user
    // doesn't see the conversation related to the task, so consider the task
    // backgrounded.
    return true;
  }

  return !task_instance->IsShowing();
}

}  // namespace actor
