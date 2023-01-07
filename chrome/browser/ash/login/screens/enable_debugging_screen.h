// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_ENABLE_DEBUGGING_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_ENABLE_DEBUGGING_SCREEN_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ui/webui/ash/login/enable_debugging_screen_handler.h"

namespace ash {

// Representation independent class that controls screen showing enable
// debugging screen to users.
class EnableDebuggingScreen : public BaseScreen {
 public:
  EnableDebuggingScreen(base::WeakPtr<EnableDebuggingScreenView> view,
                        const base::RepeatingClosure& exit_callback);

  EnableDebuggingScreen(const EnableDebuggingScreen&) = delete;
  EnableDebuggingScreen& operator=(const EnableDebuggingScreen&) = delete;

  ~EnableDebuggingScreen() override;

  // Called by EnableDebuggingScreenHandler.
  void HandleSetup(const std::string& password);

 protected:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  base::RepeatingClosure* exit_callback() { return &exit_callback_; }

 private:
  // Handle user actions
  void HandleLearnMore();
  void HandleRemoveRootFSProtection();

  // Wait for cryptohomed before checking debugd. See http://crbug.com/440506
  void WaitForCryptohome();

  // Callback for UserDataAuthClient::WaitForServiceToBeAvailable
  void OnCryptohomeDaemonAvailabilityChecked(bool service_is_available);

  // Callback for DebugDaemonClient::WaitForServiceToBeAvailable
  void OnDebugDaemonServiceAvailabilityChecked(bool service_is_available);

  // Callback for DebugDaemonClient::EnableDebuggingFeatures().
  void OnEnableDebuggingFeatures(bool success);

  // Callback for DebugDaemonClient::QueryDebuggingFeatures().
  void OnQueryDebuggingFeatures(bool success, int features_flag);

  // Callback for DebugDaemonClient::RemoveRootfsVerification().
  void OnRemoveRootfsVerification(bool success);

  void UpdateUIState(EnableDebuggingScreenView::UIState state);

  base::WeakPtr<EnableDebuggingScreenView> view_;
  base::RepeatingClosure exit_callback_;

  base::WeakPtrFactory<EnableDebuggingScreen> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_ENABLE_DEBUGGING_SCREEN_H_
