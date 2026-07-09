// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_EVENT_OBSERVATION_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_EVENT_OBSERVATION_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/events/event_router.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/events/events_api_converters.h"
#include "chrome/common/chromeos/extensions/api/events.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"
#include "content/public/browser/browser_context.h"
#include "extensions/common/extension_id.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {

class EventRouter;

class EventObservation : public ash::cros_healthd::mojom::EventObserver {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void OnEvent(const extensions::ExtensionId& extension_id,
                         EventRouter* event_router,
                         ash::cros_healthd::mojom::EventInfoPtr info) = 0;
  };

  // `category` describes the category of events that this instance will be
  // notified of.
  EventObservation(const extensions::ExtensionId& extension_id,
                   api::os_events::EventCategory category,
                   EventRouter* event_router,
                   content::BrowserContext* context);

  EventObservation(const EventObservation&) = delete;
  EventObservation& operator=(const EventObservation&) = delete;

  ~EventObservation() override;

  // ash::cros_healthd::mojom::EventObserver:
  void OnEvent(ash::cros_healthd::mojom::EventInfoPtr info) override;

  // Binds a new pending remote to this implementation.
  mojo::PendingRemote<ash::cros_healthd::mojom::EventObserver> GetRemote();

  // Sets the delegate for testing, assumes ownership.
  void SetDelegateForTesting(Delegate* delegate) { delegate_.reset(delegate); }

 private:
  extensions::ExtensionId extension_id_;
  mojo::Receiver<ash::cros_healthd::mojom::EventObserver> receiver_;
  std::unique_ptr<Delegate> delegate_;
  const raw_ptr<EventRouter> event_router_;
  const raw_ptr<content::BrowserContext, DanglingUntriaged> browser_context_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_EVENT_OBSERVATION_H_
