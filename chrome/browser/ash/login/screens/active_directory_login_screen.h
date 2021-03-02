// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_ACTIVE_DIRECTORY_LOGIN_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_ACTIVE_DIRECTORY_LOGIN_SCREEN_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ash/authpolicy/authpolicy_helper.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"

namespace chromeos {

class ActiveDirectoryLoginView;
class Key;

// Controller for the active directory login screen.
class ActiveDirectoryLoginScreen
    : public BaseScreen,
      public NetworkStateInformer::NetworkStateInformerObserver {
 public:
  ActiveDirectoryLoginScreen(ActiveDirectoryLoginView* view,
                             ErrorScreen* error_screen,
                             const base::RepeatingClosure& exit_callback);

  ~ActiveDirectoryLoginScreen() override;

  ActiveDirectoryLoginScreen(const ActiveDirectoryLoginScreen&) = delete;
  ActiveDirectoryLoginScreen& operator=(const ActiveDirectoryLoginScreen&) =
      delete;

  // Called when the screen is being destroyed. This should call Unbind() on the
  // associated View if this class is destroyed before that.
  void OnViewDestroyed(ActiveDirectoryLoginView* view);

  void HandleCompleteAuth(const std::string& username,
                          const std::string& password);

  // NetworkStateInformer::NetworkStateInformerObserver implementation:
  void UpdateState(NetworkError::ErrorReason reason) override;

 private:
  void HandleCancel();

  // Callback for AuthPolicyClient.
  void OnAdAuthResult(
      const std::string& username,
      const Key& key,
      authpolicy::ErrorType error,
      const authpolicy::ActiveDirectoryAccountInfo& account_info);

  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const std::string& action_id) override;
  bool HandleAccelerator(ash::LoginAcceleratorAction action) override;

  void ShowOfflineMessage(NetworkStateInformer::State state,
                          NetworkError::ErrorReason reason);
  void HideOfflineMessage();

  // Helper to call AuthPolicyClient and cancel calls if needed. Used to
  // authenticate users against Active Directory server.
  std::unique_ptr<AuthPolicyHelper> authpolicy_login_helper_;

  ActiveDirectoryLoginView* view_ = nullptr;

  scoped_refptr<NetworkStateInformer> network_state_informer_;

  std::unique_ptr<
      ScopedObserver<NetworkStateInformer, NetworkStateInformerObserver>>
      scoped_observer_;

  ErrorScreen* error_screen_ = nullptr;

  // TODO(crbug.com/1154669) Refactor error screen usage
  bool error_screen_visible_ = false;

  base::RepeatingClosure exit_callback_;

  base::WeakPtrFactory<ActiveDirectoryLoginScreen> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_ACTIVE_DIRECTORY_LOGIN_SCREEN_H_
