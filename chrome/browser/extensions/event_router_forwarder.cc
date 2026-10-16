// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/event_router_forwarder.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/event_router.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/event_args.h"
#include "extensions/common/extension_features.h"
#include "url/gurl.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using content::BrowserThread;

namespace extensions {

EventRouterForwarder::EventRouterForwarder() = default;

EventRouterForwarder::~EventRouterForwarder() = default;

void EventRouterForwarder::BroadcastEventToRenderers(
    events::HistogramValue histogram_value,
    const std::string& event_name,
    scoped_refptr<const EventArgs> event_args,
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

  for (Profile* profile_to_dispatch_to : profiles_to_dispatch_to) {
    CallEventRouter(
        profile_to_dispatch_to, histogram_value, event_name,
        base::FeatureList::IsEnabled(
            extensions_features::kShareEventArgsOnDispatch)
            ? event_args
            : base::MakeRefCounted<EventArgs>(event_args->data.Clone()));
  }
}

void EventRouterForwarder::CallEventRouter(
    Profile* profile,
    events::HistogramValue histogram_value,
    const std::string& event_name,
    scoped_refptr<const EventArgs> event_args) {
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
