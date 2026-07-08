// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/events/event_manager.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/app_ui_observer.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/util.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/events/event_router.h"
#include "chrome/common/chromeos/extensions/api/events.h"
#include "chromeos/ash/components/telemetry_extension/events/telemetry_event_service_ash.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest_handlers/externally_connectable.h"

namespace chromeos {

// static
extensions::BrowserContextKeyedAPIFactory<EventManager>*
EventManager::GetFactoryInstance() {
  static base::NoDestructor<
      extensions::BrowserContextKeyedAPIFactory<EventManager>>
      instance;
  return instance.get();
}

// static
EventManager* EventManager::Get(content::BrowserContext* browser_context) {
  return extensions::BrowserContextKeyedAPIFactory<EventManager>::Get(
      browser_context);
}

EventManager::EventManager(content::BrowserContext* context)
    : event_router_(context), browser_context_(context) {
  extensions::ExtensionRegistry::Get(context)->AddObserver(this);
}

EventManager::~EventManager() = default;

void EventManager::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  app_ui_observers_.erase(extension->id());
  event_router_.ResetReceiversForExtension(extension->id());
}

EventManager::RegisterEventResult EventManager::RegisterExtensionForEvent(
    extensions::ExtensionId extension_id,
    chromeos::api::os_events::EventCategory category) {
  if (kCategoriesWithFocusRestriction.contains(category)) {
    // Always check if there is a focused UI for focus-restricted events.
    auto observer =
        CreateAppUiObserver(extension_id, /*focused_ui_required=*/true);
    if (!observer) {
      return kAppUiNotFocused;
    }

    if (auto it = app_ui_observers_.find(extension_id);
        it == app_ui_observers_.end() ||
        it->second->web_contents() != observer->web_contents()) {
      app_ui_observers_.insert_or_assign(extension_id, std::move(observer));
    }

    // The existing observation may be restricted if the previous App UI was
    // unfocused.
    event_router_.UnrestrictReceiversOfExtension(extension_id);

    if (event_router_.IsExtensionObservingForCategory(extension_id, category)) {
      // Same extension and category can only have one observer.
      return kSuccess;
    }
  } else {
    // Existing observation can be reused for regular (non-focus-restricted)
    // events.
    if (event_router_.IsExtensionObservingForCategory(extension_id, category)) {
      // Early return in case the category is already observed by the extension.
      return kSuccess;
    }

    if (app_ui_observers_.find(extension_id) == app_ui_observers_.end()) {
      auto observer =
          CreateAppUiObserver(extension_id, /*focused_ui_required=*/false);
      if (!observer) {
        return kAppUiClosed;
      }
      app_ui_observers_.emplace(extension_id, std::move(observer));
    }
  }

  auto crosapi_category = converters::events::ConvertCrosapi(category);
  GetEventService().AddEventObserver(
      crosapi_category, event_router_.GetPendingRemoteForCategoryAndExtension(
                            category, extension_id));
  return kSuccess;
}

void EventManager::RemoveObservationsForExtensionAndCategory(
    extensions::ExtensionId extension_id,
    chromeos::api::os_events::EventCategory category) {
  event_router_.ResetReceiversOfExtensionByCategory(extension_id, category);
  if (!event_router_.IsExtensionObserving(extension_id)) {
    app_ui_observers_.erase(extension_id);
  }
}

void EventManager::IsEventSupported(
    chromeos::api::os_events::EventCategory category,
    ash::cros_healthd::mojom::CrosHealthdEventService::IsEventSupportedCallback
        callback) {
  ash::cros_healthd::ServiceConnection::GetInstance()
      ->GetEventService()
      ->IsEventSupported(converters::events::Convert(category),
                         std::move(callback));
}

ash::TelemetryEventServiceAsh& EventManager::GetEventService() {
  if (!event_service_) {
    event_service_ = std::make_unique<ash::TelemetryEventServiceAsh>();
  }
  return *event_service_;
}

void EventManager::OnAppUiClosed(extensions::ExtensionId extension_id) {
  // As only the focused UI is tracked, all focus-restricted events' connections
  // should be cut when `OnAppUiClosed` is called.
  for (const auto category : kCategoriesWithFocusRestriction) {
    if (event_router_.IsExtensionObservingForCategory(extension_id, category)) {
      event_router_.ResetReceiversOfExtensionByCategory(extension_id, category);
    }
  }

  // Try to find another open UI.
  auto observer =
      CreateAppUiObserver(extension_id, /*focused_ui_required=*/false);
  if (observer) {
    app_ui_observers_.insert_or_assign(extension_id, std::move(observer));
    return;
  }

  app_ui_observers_.erase(extension_id);
  event_router_.ResetReceiversForExtension(extension_id);
}

void EventManager::OnAppUiFocusChanged(extensions::ExtensionId extension_id,
                                       bool is_focused) {
  if (is_focused) {
    event_router_.UnrestrictReceiversOfExtension(extension_id);
  } else {
    event_router_.RestrictReceiversOfExtension(extension_id);
  }
}

std::unique_ptr<AppUiObserver> EventManager::CreateAppUiObserver(
    extensions::ExtensionId extension_id,
    bool focused_ui_required) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(browser_context_)
          ->GetExtensionById(extension_id,
                             extensions::ExtensionRegistry::EVERYTHING);
  if (!extension) {
    // If the extension has been unloaded from the registry, there
    // won't be any related app UI.
    return nullptr;
  }
  content::WebContents* contents = FindTelemetryExtensionOpenAndSecureAppUi(
      browser_context_, extension, focused_ui_required);
  if (!contents) {
    return nullptr;
  }

  return std::make_unique<AppUiObserver>(
      contents,
      extensions::ExternallyConnectableInfo::Get(extension)->matches.Clone(),
      // Unretained is safe here because `this` will own the observer.
      base::BindOnce(&EventManager::OnAppUiClosed, base::Unretained(this),
                     extension_id),
      base::BindRepeating(&EventManager::OnAppUiFocusChanged,
                          base::Unretained(this), extension_id));
}

}  // namespace chromeos
