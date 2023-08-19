// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_STATE_SESSION_STATE_CHANGED_EVENT_DISPATCHER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_STATE_SESSION_STATE_CHANGED_EVENT_DISPATCHER_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/crosapi/mojom/login_state.mojom.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router_factory.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

class EventRouter;

// |SessionStateChangedEventDispatcher| dispatches changes in the session state
// to extensions listening on the |loginState.onSessionStateChanged| event.
class SessionStateChangedEventDispatcher
    : public crosapi::mojom::SessionStateChangedEventObserver,
      public BrowserContextKeyedAPI {
 public:
  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<SessionStateChangedEventDispatcher>*
  GetFactoryInstance();
  void Shutdown() override;

  explicit SessionStateChangedEventDispatcher(
      content::BrowserContext* browser_context_);

  SessionStateChangedEventDispatcher(
      const SessionStateChangedEventDispatcher&) = delete;
  SessionStateChangedEventDispatcher& operator=(
      const SessionStateChangedEventDispatcher&) = delete;

  ~SessionStateChangedEventDispatcher() override;

  bool IsBoundForTesting();
  void SetEventRouterForTesting(EventRouter* event_router);

  // crosapi::mojom::SessionStateChangedEventObserver:
  void OnSessionStateChanged(crosapi::mojom::SessionState state) override;

 private:
  // Needed for BrowserContextKeyedAPI implementation.
  friend class BrowserContextKeyedAPIFactory<
      SessionStateChangedEventDispatcher>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "SessionStateChangedEventDispatcher";
  }
  static const bool kServiceIsNULLWhileTesting = true;

  raw_ptr<content::BrowserContext, DanglingUntriaged> browser_context_;
  raw_ptr<EventRouter, DanglingUntriaged> event_router_;

  // Receives mojo messages from ash.
  mojo::Receiver<crosapi::mojom::SessionStateChangedEventObserver> receiver_{
      this};
};

template <>
struct BrowserContextFactoryDependencies<SessionStateChangedEventDispatcher> {
  static void DeclareFactoryDependencies(
      BrowserContextKeyedAPIFactory<SessionStateChangedEventDispatcher>*
          factory) {
    factory->DependsOn(EventRouterFactory::GetInstance());
  }
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_STATE_SESSION_STATE_CHANGED_EVENT_DISPATCHER_H_
