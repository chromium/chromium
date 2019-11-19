// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_STATE_SESSION_STATE_CHANGED_EVENT_DISPATCHER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_STATE_SESSION_STATE_CHANGED_EVENT_DISPATCHER_H_

#include <memory>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/common/extensions/api/login_state.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

class EventRouter;

// |SessionStateChangedEventDispatcher| observes changes in the session state
// and dispatches them to extensions listening on the
// |loginState.onSessionStateChanged| event.
class SessionStateChangedEventDispatcher
    : public session_manager::SessionManagerObserver,
      public BrowserContextKeyedAPI {
 public:
  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<SessionStateChangedEventDispatcher>*
  GetFactoryInstance();
  void Shutdown() override;

  explicit SessionStateChangedEventDispatcher(
      content::BrowserContext* browser_context_);
  ~SessionStateChangedEventDispatcher() override;

  // SessionManagerObserver implementation.
  void OnSessionStateChanged() override;

  void SetEventRouterForTesting(EventRouter* event_router);

 private:
  // Needed for BrowserContextKeyedAPI implementation.
  friend class BrowserContextKeyedAPIFactory<
      SessionStateChangedEventDispatcher>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "SessionStateChangedEventDispatcher";
  }
  static const bool kServiceIsNULLWhileTesting = true;

  ScopedObserver<session_manager::SessionManager,
                 session_manager::SessionManagerObserver>
      session_manager_observer_;
  content::BrowserContext* browser_context_;
  EventRouter* event_router_;
  api::login_state::SessionState session_state_;

  DISALLOW_COPY_AND_ASSIGN(SessionStateChangedEventDispatcher);
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
