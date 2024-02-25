// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_EVENT_ROUTER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_EVENT_ROUTER_H_

#include <memory>
#include <vector>

#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_map.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/events/event_observation_crosapi.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "content/public/browser/browser_context.h"
#include "extensions/common/extension_id.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace chromeos {

namespace {

inline constexpr auto kCategoriesWithFocusRestriction =
    base::MakeFixedFlatSet<crosapi::mojom::TelemetryEventCategoryEnum>({
        crosapi::mojom::TelemetryEventCategoryEnum::kTouchpadButton,
        crosapi::mojom::TelemetryEventCategoryEnum::kTouchpadTouch,
        crosapi::mojom::TelemetryEventCategoryEnum::kTouchpadConnected,
        crosapi::mojom::TelemetryEventCategoryEnum::kStylusTouch,
        crosapi::mojom::TelemetryEventCategoryEnum::kStylusConnected,
        crosapi::mojom::TelemetryEventCategoryEnum::kTouchscreenTouch,
        crosapi::mojom::TelemetryEventCategoryEnum::kTouchscreenConnected,
    });

}  // namespace

class EventObservationCrosapi;

class EventRouter {
 public:
  explicit EventRouter(content::BrowserContext* context);

  EventRouter(const EventRouter&) = delete;
  EventRouter& operator=(const EventRouter&) = delete;

  ~EventRouter();

  // Binds a `PendingRemote` for a certain category with an associated
  // `ExtensionId`. For one category there can always be a number of receivers.
  // The `ExtensionId` is needed to dispatch an actual event to a specific
  // extension.
  mojo::PendingRemote<crosapi::mojom::TelemetryEventObserver>
  GetPendingRemoteForCategoryAndExtension(
      crosapi::mojom::TelemetryEventCategoryEnum category,
      extensions::ExtensionId extension_id);

  // Cuts the mojom pipe to all connected remotes for a certain extension.
  void ResetReceiversForExtension(extensions::ExtensionId extension_id);

  // Cuts the mojom pipe to all connected remotes for a certain extension and
  // category.
  void ResetReceiversOfExtensionByCategory(
      extensions::ExtensionId extension_id,
      crosapi::mojom::TelemetryEventCategoryEnum category);

  // Prevent the mojom pipe from sending focus-restricted events to all
  // connected remotes for a certain extension.
  void RestrictReceiversOfExtension(extensions::ExtensionId extension_id);

  // Allow the mojom pipe from sending focus-restricted events to all
  // connected remotes for a certain extension.
  void UnrestrictReceiversOfExtension(extensions::ExtensionId extension_id);

  // Checks whether an extension is observing any event.
  bool IsExtensionObserving(extensions::ExtensionId extension_id);

  // Checks whether an extension is observing a certain category of event.
  bool IsExtensionObservingForCategory(
      extensions::ExtensionId extension_id,
      crosapi::mojom::TelemetryEventCategoryEnum category);

  // Checks whether an extension is blocked from focus-restricted events.
  bool IsExtensionRestricted(extensions::ExtensionId extension_id);

  // Checks whether an extension is allowed (i.e., not restricted) for a certain
  // category of event.
  bool IsExtensionAllowedForCategory(
      extensions::ExtensionId extension_id,
      crosapi::mojom::TelemetryEventCategoryEnum category);

 private:
  // Observers grouped by category and extension.
  base::flat_map<extensions::ExtensionId,
                 base::flat_map<crosapi::mojom::TelemetryEventCategoryEnum,
                                std::unique_ptr<EventObservationCrosapi>>>
      observers_;

  // Extensions in the restricted state (i.e., blocked from focus-restricted
  // events).
  base::flat_set<extensions::ExtensionId> restricted_extensions_;

  const raw_ptr<content::BrowserContext, DanglingUntriaged> browser_context_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_EVENT_ROUTER_H_
