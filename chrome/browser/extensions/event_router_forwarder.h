// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EVENT_ROUTER_FORWARDER_H_
#define CHROME_BROWSER_EXTENSIONS_EVENT_ROUTER_FORWARDER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_event_histogram_value.h"

class GURL;

namespace extensions {

// This class forwards events to EventRouters.
// The advantages of this class over direct usage of EventRouters are:
// - this class is thread-safe, you can call the functions from UI and IO
//   thread.
// - the class can handle if a profile is deleted between the time of sending
//   the event from the IO thread to the UI thread.
// - this class can be used in contexts that are not governed by a profile, e.g.
//   by system URLRequestContexts. In these cases the |restrict_to_profile|
//   parameter remains NULL and events are broadcasted to all profiles.
class EventRouterForwarder
    : public base::RefCountedThreadSafe<EventRouterForwarder> {
 public:
  EventRouterForwarder();

  // Calls
  //   DispatchEventToRenderers(event_name, event_args, profile, event_url)
  // on all (original) profiles' EventRouters.
  // May be called on any thread.
  void BroadcastEventToRenderers(events::HistogramValue histogram_value,
                                 const std::string& event_name,
                                 std::unique_ptr<base::ListValue> event_args,
                                 const GURL& event_url,
                                 bool dispatch_to_off_the_record_profiles);

  // Calls
  //   DispatchEventToRenderers(event_name, event_args,
  //       use_profile_to_restrict_events ? profile : NULL, event_url)
  // on |profile|'s EventRouter. May be called on any thread.
  void DispatchEventToRenderers(events::HistogramValue histogram_value,
                                const std::string& event_name,
                                std::unique_ptr<base::ListValue> event_args,
                                void* profile,
                                bool use_profile_to_restrict_events,
                                const GURL& event_url,
                                bool dispatch_to_off_the_record_profiles);

 protected:
  // Protected for testing.
  virtual ~EventRouterForwarder();

  // Helper function for {Broadcast,Dispatch}EventTo{Extension,Renderers}.
  // Virtual for testing.
  virtual void HandleEvent(const std::string& extension_id,
                           events::HistogramValue histogram_value,
                           const std::string& event_name,
                           std::unique_ptr<base::ListValue> event_args,
                           void* profile,
                           bool use_profile_to_restrict_events,
                           const GURL& event_url,
                           bool dispatch_to_off_the_record_profiles);

  // Calls DispatchEventToRenderers or DispatchEventToExtension (depending on
  // whether extension_id == "" or not) of |profile|'s EventRouter.
  // |profile| may never be NULL.
  // Virtual for testing.
  virtual void CallEventRouter(Profile* profile,
                               const std::string& extension_id,
                               events::HistogramValue histogram_value,
                               const std::string& event_name,
                               std::unique_ptr<base::ListValue> event_args,
                               Profile* restrict_to_profile,
                               const GURL& event_url);

 private:
  friend class base::RefCountedThreadSafe<EventRouterForwarder>;

  DISALLOW_COPY_AND_ASSIGN(EventRouterForwarder);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EVENT_ROUTER_FORWARDER_H_
