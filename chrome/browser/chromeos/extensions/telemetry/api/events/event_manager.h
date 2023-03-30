// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_EVENT_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_EVENT_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"

namespace chromeos {

class EventManager : public extensions::BrowserContextKeyedAPI {
 public:
  // extensions::BrowserContextKeyedAPI:
  static extensions::BrowserContextKeyedAPIFactory<EventManager>*
  GetFactoryInstance();

  // Convenience method to get the EventManager for a content::BrowserContext.
  static EventManager* Get(content::BrowserContext* browser_context);

  explicit EventManager(content::BrowserContext* context);

  EventManager(const EventManager&) = delete;
  EventManager& operator=(const EventManager&) = delete;

  ~EventManager() override;

 private:
  friend class extensions::BrowserContextKeyedAPIFactory<EventManager>;

  // extensions::BrowserContextKeyedAPI:
  static const char* service_name() { return "TelemetryEventManager"; }
  static const bool kServiceIsCreatedInGuestMode = false;
  static const bool kServiceRedirectedInIncognito = true;

  const raw_ptr<content::BrowserContext> browser_context_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_EVENT_MANAGER_H_
