// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_EVENT_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_EVENT_MANAGER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/events/event_router.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/events/remote_event_service_strategy.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_extension_exception.mojom.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

class AppUiObserver;

class EventManager : public extensions::BrowserContextKeyedAPI,
                     public extensions::ExtensionRegistryObserver {
 public:
  enum RegisterEventResult {
    kSuccess,
    kAppUiClosed,
    kAppUiNotFocused,
  };

  // extensions::BrowserContextKeyedAPI:
  static extensions::BrowserContextKeyedAPIFactory<EventManager>*
  GetFactoryInstance();

  // Convenience method to get the EventManager for a content::BrowserContext.
  static EventManager* Get(content::BrowserContext* browser_context);

  explicit EventManager(content::BrowserContext* context);

  EventManager(const EventManager&) = delete;
  EventManager& operator=(const EventManager&) = delete;

  ~EventManager() override;

  // `ExtensionRegistryObserver`:
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;

  // Registers an extension for a certain event category. This results in a
  // subscription with cros_healthd which is cut when either:
  // 1. The app UI associated with the extension is closed.
  // 2. The connection gets cut manually.
  RegisterEventResult RegisterExtensionForEvent(
      extensions::ExtensionId extension_id,
      crosapi::mojom::TelemetryEventCategoryEnum category);

  // Removes an observation for a certain extension and category.
  // This results in a cut of the mojom pipe to cros_healthd.
  void RemoveObservationsForExtensionAndCategory(
      extensions::ExtensionId extension_id,
      crosapi::mojom::TelemetryEventCategoryEnum category);

  // Checks whether a certain event category is supported.
  void IsEventSupported(
      crosapi::mojom::TelemetryEventCategoryEnum category,
      crosapi::mojom::TelemetryEventService::IsEventSupportedCallback callback);

 private:
  friend class extensions::BrowserContextKeyedAPIFactory<EventManager>;
  friend class TelemetryExtensionEventManagerTest;

  // extensions::BrowserContextKeyedAPI:
  static const char* service_name() { return "TelemetryEventManager"; }
  static const bool kServiceIsCreatedInGuestMode = false;
  static const bool kServiceRedirectedInIncognito = true;

  mojo::Remote<crosapi::mojom::TelemetryEventService>& GetRemoteService();

  void OnAppUiClosed(extensions::ExtensionId extension_id);
  void OnAppUiFocusChanged(extensions::ExtensionId extension_id,
                           bool is_focused);

  std::unique_ptr<AppUiObserver> CreateAppUiObserver(
      extensions::ExtensionId extension_id,
      bool focused_ui_required);

  base::flat_map<extensions::ExtensionId, std::unique_ptr<AppUiObserver>>
      app_ui_observers_;
  EventRouter event_router_;
  std::unique_ptr<RemoteEventServiceStrategy> remote_event_service_strategy_;

  const raw_ptr<content::BrowserContext> browser_context_;
};

}  // namespace chromeos

namespace extensions {

template <>
struct BrowserContextFactoryDependencies<chromeos::EventManager> {
  static void DeclareFactoryDependencies(
      extensions::BrowserContextKeyedAPIFactory<chromeos::EventManager>*
          factory) {
    factory->DependsOn(ExtensionRegistryFactory::GetInstance());
  }
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_EVENT_MANAGER_H_
