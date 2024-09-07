// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EVENT_ROUTER_FORWARDER_H_
#define CHROME_BROWSER_EXTENSIONS_EVENT_ROUTER_FORWARDER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_event_histogram_value.h"

namespace extensions {

// This class forwards events to EventRouters.
// The advantages of this class over direct usage of EventRouters are:
// - this class is thread-safe.
// - the class can handle if a profile is deleted between the time of sending
//   the event from a different thread to the UI thread.
// - this class can send events to the set of all active profiles. This is
//   useful for system-wide settings that may change.
class EventRouterForwarder
    : public base::RefCountedThreadSafe<EventRouterForwarder> {
 public:
  EventRouterForwarder();

  EventRouterForwarder(const EventRouterForwarder&) = delete;
  EventRouterForwarder& operator=(const EventRouterForwarder&) = delete;

  // Dispatches an event to all active on-the-record EventRouters.
  // Safe to call on any thread.
  void BroadcastEventToRenderers(events::HistogramValue histogram_value,
                                 const std::string& event_name,
                                 base::Value::List event_args,
                                 bool dispatch_to_off_the_record_profiles);

 protected:
  // Protected for testing.
  virtual ~EventRouterForwarder();

  // Broadcasts the event to listeners associated with `profile`'s EventRouter.
  // Virtual for testing.
  virtual void CallEventRouter(Profile* profile,
                               events::HistogramValue histogram_value,
                               const std::string& event_name,
                               base::Value::List event_args);

 private:
  friend class base::RefCountedThreadSafe<EventRouterForwarder>;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EVENT_ROUTER_FORWARDER_H_
