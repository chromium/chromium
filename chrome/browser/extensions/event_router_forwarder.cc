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
    const GURL& event_url,
    bool dispatch_to_off_the_record_profiles) {
  HandleEvent(std::string(), histogram_value, event_name, std::move(event_args),
              nullptr, true, event_url, dispatch_to_off_the_record_profiles);
}

void EventRouterForwarder::DispatchEventToRenderers(
    events::HistogramValue histogram_value,
    const std::string& event_name,
    base::Value::List event_args,
    void* profile,
    bool use_profile_to_restrict_events,
    const GURL& event_url,
    bool dispatch_to_off_the_record_profiles) {
  if (!profile)
    return;
  HandleEvent(std::string(), histogram_value, event_name, std::move(event_args),
              profile, use_profile_to_restrict_events, event_url,
              dispatch_to_off_the_record_profiles);
}

void EventRouterForwarder::HandleEvent(
    const std::string& extension_id,
    events::HistogramValue histogram_value,
    const std::string& event_name,
    base::Value::List event_args,
    void* profile_ptr,
    bool use_profile_to_restrict_events,
    const GURL& event_url,
    bool dispatch_to_off_the_record_profiles) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&EventRouterForwarder::HandleEvent, this, extension_id,
                       histogram_value, event_name, std::move(event_args),
                       profile_ptr, use_profile_to_restrict_events, event_url,
                       dispatch_to_off_the_record_profiles));
    return;
  }

  if (!g_browser_process || !g_browser_process->profile_manager())
    return;

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile = nullptr;
  if (profile_ptr) {
    if (!profile_manager->IsValidProfile(profile_ptr))
      return;
    profile = reinterpret_cast<Profile*>(profile_ptr);
  }
  std::set<Profile*> profiles_to_dispatch_to;
  if (profile) {
    profiles_to_dispatch_to.insert(profile);
  } else {
    std::vector<Profile*> on_the_record_profiles =
        profile_manager->GetLoadedProfiles();
    profiles_to_dispatch_to.insert(on_the_record_profiles.begin(),
                                   on_the_record_profiles.end());
  }

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
        profile_to_dispatch_to, extension_id, histogram_value, event_name,
        profile_to_dispatch_to != *std::prev(profiles_to_dispatch_to.end())
            ? event_args.Clone()
            : std::move(event_args),
        use_profile_to_restrict_events ? profile_to_dispatch_to : nullptr,
        event_url);
  }
}

void EventRouterForwarder::CallEventRouter(
    Profile* profile,
    const std::string& extension_id,
    events::HistogramValue histogram_value,
    const std::string& event_name,
    base::Value::List event_args,
    Profile* restrict_to_profile,
    const GURL& event_url) {
  auto* event_router = extensions::EventRouter::Get(profile);
  // Extension does not exist for chromeos login.  This needs to be
  // removed once we have an extension service for login screen.
  // crosbug.com/12856.
  //
  // Extensions are not available on System Profile.
  if (!event_router)
    return;

  auto event = std::make_unique<Event>(
      histogram_value, event_name, std::move(event_args), restrict_to_profile);
  event->event_url = event_url;
  if (extension_id.empty()) {
    event_router->BroadcastEvent(std::move(event));
  } else {
    event_router->DispatchEventToExtension(extension_id, std::move(event));
  }
}

}  // namespace extensions
