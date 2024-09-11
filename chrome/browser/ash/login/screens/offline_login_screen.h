// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_OFFLINE_LOGIN_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_OFFLINE_LOGIN_SCREEN_H_

#include <string>

#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ui/webui/ash/login/network_state_informer.h"
#include "chromeos/ash/experiences/idle_detector/idle_detector.h"

namespace ash {

class OfflineLoginView;

// This class represents offline login screen: that handles login in offline
// mode with provided username and password checked against cryptohome.
class OfflineLoginScreen
    : public BaseScreen,
      public NetworkStateInformer::NetworkStateInformerObserver {
 public:
  using TView = OfflineLoginView;

  enum class Result {
    BACK,
    RELOAD_ONLINE_LOGIN,
  };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;
  OfflineLoginScreen(base::WeakPtr<OfflineLoginView> view,
                     const ScreenExitCallback& exit_callback);
  ~OfflineLoginScreen() override;

  // NetworkStateInformer::NetworkStateInformerObserver:
  void OnNetworkReady() override;
  void UpdateState(NetworkError::ErrorReason reason) override;

  void ShowPasswordMismatchMessage();

 private:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  void StartIdleDetection();
  void OnIdle();

  void HandleTryLoadOnlineLogin();
  void HandleCompleteAuth(const std::string& username,
                          const std::string& password);
  void HandleEmailSubmitted(const std::string& username);

  base::WeakPtr<OfflineLoginView> view_;

  // True when network is available.
  bool is_network_available_ = false;

  // Network state informer used to keep signin screen up.
  scoped_refptr<NetworkStateInformer> network_state_informer_;

  std::unique_ptr<base::ScopedObservation<NetworkStateInformer,
                                          NetworkStateInformerObserver>>
      scoped_observer_;

  ScreenExitCallback exit_callback_;

  // Will monitor if the user is idle for a long period of time and we can try
  // to get back to Online Gaia.
  std::unique_ptr<IdleDetector> idle_detector_;

  base::WeakPtrFactory<OfflineLoginScreen> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_OFFLINE_LOGIN_SCREEN_H_
