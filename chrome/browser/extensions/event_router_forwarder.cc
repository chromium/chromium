// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/event_router_forwarder.h"

#include <stddef.h>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/event_router.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace extensions {

EventRouterForwarder::EventRouterForwarder() {
}

EventRouterForwarder::~EventRouterForwarder() {
}

void EventRouterForwarder::BroadcastEventToRenderers(
    events::HistogramValue histogram_value,
    const std::string& event_name,
    base::Value::List event_args,
    bool dispatch_to_off_the_record_profiles) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&EventRouterForwarder::BroadcastEventToRenderers, this,
                       histogram_value, event_name, std::move(event_args),
                       dispatch_to_off_the_record_profiles));
    return;
  }

  if (!g_browser_process || !g_browser_process->profile_manager())
    return;

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  std::set<Profile*> profiles_to_dispatch_to;
  std::vector<Profile*> on_the_record_profiles =
      profile_manager->GetLoadedProfiles();
  profiles_to_dispatch_to.insert(on_the_record_profiles.begin(),
                                 on_the_record_profiles.end());

  if (dispatch_to_off_the_record_profiles) {
    for (Profile* profile_to_dispatch_to : profiles_to_dispatch_to) {
      if (profile_to_dispatch_to->HasPrimaryOTRProfile())
        profiles_to_dispatch_to.insert(
            profile_to_dispatch_to->GetPrimaryOTRProfile(
                /*create_if_needed=*/true));
    }
  }

  // There should always be at least one profile when running as Chromium.
  // However, some Chromium embedders are known to run without profiles, in
  // which case there's nothing to dispatch to.
  if (profiles_to_dispatch_to.size() == 0u)
    return;

  for (Profile* profile_to_dispatch_to : profiles_to_dispatch_to) {
    CallEventRouter(
        profile_to_dispatch_to, histogram_value, event_name,
        profile_to_dispatch_to != *std::prev(profiles_to_dispatch_to.end())
            ? event_args.Clone()
            : std::move(event_args));
  }
}

void EventRouterForwarder::CallEventRouter(
    Profile* profile,
    events::HistogramValue histogram_value,
    const std::string& event_name,
    base::Value::List event_args) {
  auto* event_router = extensions::EventRouter::Get(profile);
  // Extension does not exist for chromeos login.  This needs to be
  // removed once we have an extension service for login screen.
  // crosbug.com/12856.
  //
  // Extensions are not available on System Profile.
  if (!event_router)
    return;

  auto event = std::make_unique<Event>(histogram_value, event_name,
                                       std::move(event_args), profile);
  event_router->BroadcastEvent(std::move(event));
}

}  // namespace extensions
