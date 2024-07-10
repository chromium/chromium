// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_AUTH_PANEL_DEBUG_VIEW_H_
#define ASH_LOGIN_UI_AUTH_PANEL_DEBUG_VIEW_H_

#include "ash/ash_export.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/osauth/public/auth_attempt_consumer.h"
#include "components/account_id/account_id.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"

namespace ash {

class ASH_EXPORT AuthPanelDebugView : public views::View,
                                      public AuthAttemptConsumer {
 public:
  // Creates local authentication request view that will enable the user to
  // authenticate with a local authentication.
  AuthPanelDebugView(const AccountId& account_id, bool use_legacy_authpanel);

  AuthPanelDebugView(const AuthPanelDebugView&) = delete;
  AuthPanelDebugView& operator=(const AuthPanelDebugView&) = delete;

  ~AuthPanelDebugView() override;

  // AuthAttemptConsumer:
  void OnUserAuthAttemptRejected() override;
  void OnUserAuthAttemptConfirmed(
      AuthHubConnector* connector,
      raw_ptr<AuthFactorStatusConsumer>& out_consumer) override;
  void OnAccountNotFound() override;
  void OnUserAuthAttemptCancelled() override;
  void OnFactorAttemptFailed(AshAuthFactor factor) override;
  void OnUserAuthSuccess(AshAuthFactor factor,
                         const AuthProofToken& token) override;

 private:
  // Closes the view.
  void OnClose();

  void ChildPreferredSizeChanged(views::View* child) override;

  void OnEndAuthentication();

  void OnAuthPanelPreferredSizeChanged();

  base::WeakPtrFactory<AuthPanelDebugView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_AUTH_PANEL_DEBUG_VIEW_H_
