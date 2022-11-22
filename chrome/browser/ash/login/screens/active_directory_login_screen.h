// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_ACTIVE_DIRECTORY_LOGIN_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_ACTIVE_DIRECTORY_LOGIN_SCREEN_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/ash/authpolicy/authpolicy_helper.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ui/webui/ash/login/network_state_informer.h"

namespace ash {

class ActiveDirectoryLoginView;
class Key;

// Controller for the active directory login screen.
class ActiveDirectoryLoginScreen
    : public BaseScreen,
      public NetworkStateInformer::NetworkStateInformerObserver {
 public:
  ActiveDirectoryLoginScreen(base::WeakPtr<ActiveDirectoryLoginView> view,
                             ErrorScreen* error_screen,
                             const base::RepeatingClosure& exit_callback);

  ~ActiveDirectoryLoginScreen() override;

  ActiveDirectoryLoginScreen(const ActiveDirectoryLoginScreen&) = delete;
  ActiveDirectoryLoginScreen& operator=(const ActiveDirectoryLoginScreen&) =
      delete;

  // NetworkStateInformer::NetworkStateInformerObserver implementation:
  void UpdateState(NetworkError::ErrorReason reason) override;

 private:
  void HandleCancel();
  void HandleCompleteAuth(const std::string& username,
                          const std::string& password);

  // Callback for AuthPolicyClient.
  void OnAdAuthResult(
      const std::string& username,
      const Key& key,
      authpolicy::ErrorType error,
      const authpolicy::ActiveDirectoryAccountInfo& account_info);

  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;
  bool HandleAccelerator(LoginAcceleratorAction action) override;

  void ShowOfflineMessage(NetworkStateInformer::State state,
                          NetworkError::ErrorReason reason);
  void HideOfflineMessage();

  // Helper to call AuthPolicyClient and cancel calls if needed. Used to
  // authenticate users against Active Directory server.
  std::unique_ptr<AuthPolicyHelper> authpolicy_login_helper_;

  base::WeakPtr<ActiveDirectoryLoginView> view_;

  scoped_refptr<NetworkStateInformer> network_state_informer_;

  base::ScopedObservation<NetworkStateInformer, NetworkStateInformerObserver>
      scoped_observation_{this};

  ErrorScreen* error_screen_ = nullptr;

  // TODO(crbug.com/1154669) Refactor error screen usage
  bool error_screen_visible_ = false;

  base::RepeatingClosure exit_callback_;

  base::WeakPtrFactory<ActiveDirectoryLoginScreen> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_ACTIVE_DIRECTORY_LOGIN_SCREEN_H_
