// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_BASE_OSAUTH_SETUP_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_BASE_OSAUTH_SETUP_SCREEN_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {

class UserContext;
class ScopedSessionRefresher;
// Generic base class for Screens that need to interact with `cryptohomed`
// to properly configure user's authentication on the device.
// See protected methods.

class BaseOSAuthSetupScreen : public BaseScreen {
 public:
  using InspectContextCallback = base::OnceCallback<void(UserContext*)>;
  BaseOSAuthSetupScreen(OobeScreenId screen_id,
                        OobeScreenPriority screen_priority);

  BaseOSAuthSetupScreen(const BaseOSAuthSetupScreen&) = delete;
  BaseOSAuthSetupScreen& operator=(const BaseOSAuthSetupScreen&) = delete;

  ~BaseOSAuthSetupScreen() override;

 protected:
  // Resets AuthSession refresher if it was requested.
  void HideImpl() override;

  // Convenient way to obtain token associated with UserContext.
  AuthProofToken GetToken();

  // Convenient way to obtain context for synchronous inspection/modification,
  // and proceeding once context is stored in Storage.
  // If context is invalidate by the time of request, `inspect_callback` would
  // be called with `nullptr` as parameter, and `continuation` would NOT be
  // called.
  void InspectContextAndContinue(InspectContextCallback inspect_callback,
                                 base::OnceClosure continuation);

  // Obtain and store `ScopedSessionRefresher`. Refresher would be cleared upon
  // screen exit. Does nothing if UseAuthSessionStorage flag is disabled.
  void KeepAliveAuthSession();

  // Inspects UserContext and establishes Session refresher if there is
  // no knowledge factor established yet.
  // TODO(b/271249180): once notification for session expiration is implemented,
  // register for such notification if there is a knowledge factor.
  void EstablishKnowledgeFactorGuard(base::OnceClosure continuation);

 private:
  void InspectContextAndContinueWithContext(
      InspectContextCallback inspect_callback,
      base::OnceClosure continuation,
      std::unique_ptr<UserContext> user_context);

  void CheckForKnowledgeFactorPresence(base::OnceClosure continuation,
                                       UserContext* context);

  std::unique_ptr<ScopedSessionRefresher> session_refresher_;

  base::WeakPtrFactory<BaseOSAuthSetupScreen> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_BASE_OSAUTH_SETUP_SCREEN_H_
