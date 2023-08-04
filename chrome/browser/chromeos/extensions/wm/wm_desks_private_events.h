// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_WM_WM_DESKS_PRIVATE_EVENTS_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_WM_WM_DESKS_PRIVATE_EVENTS_H_

#include <memory>

#include "ash/wm/desks/desks_controller.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/desk.mojom.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"

class Profile;

namespace base {
class Uuid;
}

namespace content {
class BrowserContext;
}

namespace extensions {
class EventRouter;

class WMDesksEventsRouter : public crosapi::mojom::DeskEventObserver {
 public:
  explicit WMDesksEventsRouter(Profile* profile);
  WMDesksEventsRouter(const WMDesksEventsRouter&) = delete;
  WMDesksEventsRouter& operator=(const WMDesksEventsRouter&) = delete;
  ~WMDesksEventsRouter() override;

  // mojom::DeskEventObserver override
  void OnDeskAdded(const base::Uuid& desk_id, bool from_undo = false) override;
  void OnDeskRemoved(const base::Uuid& desk_id) override;
  void OnDeskSwitched(const base::Uuid& activated,
                      const base::Uuid& deactivated) override;

  mojo::PendingRemote<crosapi::mojom::DeskEventObserver> BindDeskClient() {
    return receiver_.BindNewPipeAndPassRemoteWithVersion();
  }
  bool is_receiver_bound() { return receiver_.is_bound(); }

 private:
  raw_ptr<EventRouter> event_router_;
  mojo::Receiver<crosapi::mojom::DeskEventObserver> receiver_{this};
  base::WeakPtrFactory<WMDesksEventsRouter> weak_factory_{this};
};

class WMDesksPrivateEventsAPI : public BrowserContextKeyedAPI,
                                public EventRouter::Observer {
 public:
  static BrowserContextKeyedAPIFactory<WMDesksPrivateEventsAPI>*
  GetFactoryInstance();

  // Convenience method to get the WMDesksPrivateEventsAPI for browser context.
  static WMDesksPrivateEventsAPI* Get(content::BrowserContext* context);

  explicit WMDesksPrivateEventsAPI(content::BrowserContext* context);
  WMDesksPrivateEventsAPI(const WMDesksPrivateEventsAPI&) = delete;
  WMDesksPrivateEventsAPI& operator=(const WMDesksPrivateEventsAPI&) = delete;
  ~WMDesksPrivateEventsAPI() override;

  // KeyedService implementation
  void Shutdown() override;

  // Retrieves instance of WMDesksEventsRouter.
  WMDesksEventsRouter* desks_event_router() {
    return desk_events_router_.get();
  }

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;
  void OnListenerRemoved(const EventListenerInfo& details) override;

 private:
  friend class BrowserContextKeyedAPIFactory<WMDesksPrivateEventsAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "WMDesksPrivateEventsAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceIsCreatedInGuestMode = false;

  bool HasDeskEventsListener();
  raw_ptr<Profile> profile_;
  std::unique_ptr<WMDesksEventsRouter> desk_events_router_;
  base::WeakPtrFactory<WMDesksPrivateEventsAPI> weak_factory_{this};
};

template <>
struct BrowserContextFactoryDependencies<WMDesksPrivateEventsAPI> {
  static void DeclareFactoryDependencies(
      BrowserContextKeyedAPIFactory<WMDesksPrivateEventsAPI>* factory) {
    factory->DependsOn(
        ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
    factory->DependsOn(EventRouterFactory::GetInstance());
  }
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_WM_WM_DESKS_PRIVATE_EVENTS_H_
