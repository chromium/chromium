// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/wm/wm_desks_private_events.h"

#include "base/check_is_test.h"
#include "base/lazy_instance.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/wm_desks_private.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_system.h"

namespace extensions {

WMDesksEventsRouter::WMDesksEventsRouter(Profile* profile)
    : event_router_(EventRouter::Get(profile)) {}

WMDesksEventsRouter::~WMDesksEventsRouter() = default;

void WMDesksEventsRouter::OnDeskAdded(const base::Uuid& desk_id,
                                      bool from_undo) {
  if (!event_router_) {
    CHECK_IS_TEST();
    return;
  }

  auto event = std::make_unique<Event>(
      from_undo ? events::DESK_EVENTS_ON_DESK_REMOVAL_UNDONE
                : events::DESK_EVENTS_ON_DESK_ADDED,
      api::wm_desks_private::OnDeskAdded::kEventName,
      base::Value::List()
          .Append(desk_id.AsLowercaseString())
          .Append(from_undo));
  event_router_->BroadcastEvent(std::move(event));
}

void WMDesksEventsRouter::OnDeskRemoved(const base::Uuid& desk_id) {
  if (!event_router_) {
    CHECK_IS_TEST();
    return;
  }

  auto event = std::make_unique<Event>(
      events::DESK_EVENTS_ON_DESK_REMOVED,
      api::wm_desks_private::OnDeskRemoved::kEventName,
      base::Value::List().Append(desk_id.AsLowercaseString()));
  event_router_->BroadcastEvent(std::move(event));
}

void WMDesksEventsRouter::OnDeskSwitched(const base::Uuid& deactivated,
                                         const base::Uuid& activated) {
  if (!event_router_) {
    CHECK_IS_TEST();
    return;
  }

  auto event =
      std::make_unique<Event>(events::DESK_EVENTS_ON_DESK_SWITCHED,
                              api::wm_desks_private::OnDeskSwitched::kEventName,
                              base::Value::List()
                                  .Append(deactivated.AsLowercaseString())
                                  .Append(activated.AsLowercaseString()));
  event_router_->BroadcastEvent(std::move(event));
}

WMDesksPrivateEventsAPI::WMDesksPrivateEventsAPI(
    content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)) {
  // Register for listener added and removed events.
  auto* router = EventRouter::Get(profile_);
  router->RegisterObserver(this,
                           api::wm_desks_private::OnDeskAdded::kEventName);
  router->RegisterObserver(this,
                           api::wm_desks_private::OnDeskRemoved::kEventName);
  router->RegisterObserver(this,
                           api::wm_desks_private::OnDeskSwitched::kEventName);
}

WMDesksPrivateEventsAPI::~WMDesksPrivateEventsAPI() = default;

// static
BrowserContextKeyedAPIFactory<WMDesksPrivateEventsAPI>*
WMDesksPrivateEventsAPI::GetFactoryInstance() {
  static base::NoDestructor<
      BrowserContextKeyedAPIFactory<WMDesksPrivateEventsAPI>>
      instance;
  return instance.get();
}

WMDesksPrivateEventsAPI* WMDesksPrivateEventsAPI::Get(
    content::BrowserContext* context) {
  return GetFactoryInstance()->Get(context);
}

void WMDesksPrivateEventsAPI::Shutdown() {
  if (auto* router = EventRouter::Get(profile_)) {
    router->UnregisterObserver(this);
  }
}

void WMDesksPrivateEventsAPI::OnListenerAdded(
    const EventListenerInfo& details) {
  if (!desk_events_router_ && HasDeskEventsListener()) {
    desk_events_router_ = std::make_unique<WMDesksEventsRouter>(profile_);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // Only register once
    chromeos::LacrosService* service = chromeos::LacrosService::Get();
    if (!service->IsAvailable<crosapi::mojom::Desk>() ||
        service->GetInterfaceVersion<crosapi::mojom::Desk>() <
            static_cast<int>(crosapi::mojom::Desk::MethodMinVersions::
                                 kAddDeskEventObserverMinVersion)) {
      return;
    }
    service->GetRemote<crosapi::mojom::Desk>()->AddDeskEventObserver(
        desk_events_router_->BindDeskClient());
#endif
  }
}

void WMDesksPrivateEventsAPI::OnListenerRemoved(
    const EventListenerInfo& details) {
  if (!HasDeskEventsListener()) {
    desk_events_router_.reset();
  }
}

bool WMDesksPrivateEventsAPI::HasDeskEventsListener() {
  auto* router = EventRouter::Get(profile_);
  return router->HasEventListener(
             api::wm_desks_private::OnDeskAdded::kEventName) ||
         router->HasEventListener(
             api::wm_desks_private::OnDeskRemoved::kEventName) ||
         router->HasEventListener(
             api::wm_desks_private::OnDeskSwitched::kEventName);
}

}  // namespace extensions
