// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_PRIVATE_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_PRIVATE_EVENT_ROUTER_H_

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/event_router.h"

namespace autofill {
class PersonalDataManager;
}

namespace content {
class BrowserContext;
}

namespace extensions {

// An event router that observes changes to autofill addresses and credit cards
// and notifies listeners to the autofill API events.
class AutofillPrivateEventRouter :
    public KeyedService,
    public EventRouter::Observer,
    public autofill::PersonalDataManagerObserver {
 public:
  static AutofillPrivateEventRouter* Create(
      content::BrowserContext* browser_context);
  AutofillPrivateEventRouter(const AutofillPrivateEventRouter&) = delete;
  AutofillPrivateEventRouter& operator=(const AutofillPrivateEventRouter&) =
      delete;
  ~AutofillPrivateEventRouter() override = default;

 protected:
  explicit AutofillPrivateEventRouter(content::BrowserContext* context);

  // KeyedService overrides:
  void Shutdown() override;

  // PersonalDataManagerObserver implementation.
  void OnPersonalDataChanged() override;
  void OnPersonalDataSyncStateChanged() override;

 private:
  // Triggers an event on the router with current user's data.
  void BroadcastCurrentData();

  raw_ptr<content::BrowserContext> context_;

  raw_ptr<EventRouter> event_router_ = nullptr;

  raw_ptr<autofill::PersonalDataManager> personal_data_ = nullptr;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_PRIVATE_EVENT_ROUTER_H_
