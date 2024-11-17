// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains helper functions used by Chromium OS login.

#ifndef CHROME_BROWSER_ASH_LOGIN_HELPER_H_
#define CHROME_BROWSER_ASH_LOGIN_HELPER_H_

#include <string>

#include "ash/public/cpp/login_screen_model.h"
#include "base/memory/ref_counted.h"
#include "chromeos/ash/components/login/auth/public/session_auth_factors.h"
#include "chromeos/ash/components/network/network_handler_callbacks.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/view.h"

class Profile;
class UserContext;

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace content {
class StoragePartition;
}  // namespace content

namespace network {
namespace mojom {
class NetworkContext;
}  // namespace mojom
class SharedURLLoaderFactory;
}  // namespace network

namespace policy {
class DeviceLocalAccountPolicyBroker;
}  // namespace policy

namespace ash {

// Returns bounds of the screen to use for login wizard.
// The rect is centered within the default monitor and sized accordingly if
// `size` is not empty. Otherwise the whole monitor is occupied.
gfx::Rect CalculateScreenBounds(const gfx::Size& size);

// Returns the size of user image required for proper display under current DPI.
int GetCurrentUserImageSize();

// Define the constants in `login` namespace to avoid potential
// conflict with other chromeos components.
namespace login {

// Maximum size of user image, in which it should be saved to be properly
// displayed under all possible DPI values.
const int kMaxUserImageSize = 512;

// Returns true if lock/login should scroll user pods into view itself when
// virtual keyboard is shown and disable vk overscroll.
bool LoginScrollIntoViewEnabled();

// A helper class for easily mocking out Network*Handler calls for tests.
class NetworkStateHelper {
 public:
  NetworkStateHelper();

  NetworkStateHelper(const NetworkStateHelper&) = delete;
  NetworkStateHelper& operator=(const NetworkStateHelper&) = delete;

  virtual ~NetworkStateHelper();

  // Returns name of the currently connected network.
  // If there are no connected networks, returns name of the network
  // that is in the "connecting" state. Otherwise empty string is returned.
  // If there are multiple connected networks, network priority:
  // Ethernet > WiFi > Cellular. Same for connecting network.
  virtual std::u16string GetCurrentNetworkName() const;

  // Returns true if the default network is in connected state.
  virtual bool IsConnected() const;

  // Returns true if the ethernet network is in connected state.
  virtual bool IsConnectedToEthernet() const;

  // Returns true if the default network is in connecting state.
  virtual bool IsConnecting() const;

 private:
  void OnCreateConfiguration(base::OnceClosure success_callback,
                             network_handler::ErrorCallback error_callback,
                             const std::string& service_path,
                             const std::string& guid) const;
};

//
// Webview based login helpers.
//

// Returns the storage partition for the sign-in webview. Note the function
// returns nullptr if the sign-in partition is not available yet, or if sign-in
// webui is torn down.
content::StoragePartition* GetSigninPartition();

// Returns the storage partition for the lock screen webview. Can return nullptr
// if the lock screen partition is not available.
content::StoragePartition* GetLockScreenPartition();

// Returns the network context for the sign-in webview. Note the function
// returns nullptr if the sign-in partition is not available yet, or if sign-in
// webui is torn down.
network::mojom::NetworkContext* GetSigninNetworkContext();

// Returns the URLLoaderFactory that contains sign-in cookies. For old iframe
// based flow, the URLLoaderFactory of the sign-in profile is returned. For
// webview basedflow, the URLLoaderFactory of the sign-in webview storage
// partition is returned.
scoped_refptr<network::SharedURLLoaderFactory> GetSigninURLLoaderFactory();

// Saves sync password hash and salt to profile prefs. These will be used to
// detect Gaia password reuses.
void SaveSyncPasswordDataToProfile(const UserContext& user_context,
                                   Profile* profile);

// Returns time remaining to the next online login. The value can be negative
// which means that online login should have been already happened in the past.
base::TimeDelta TimeToOnlineSignIn(base::Time last_online_signin,
                                   base::TimeDelta offline_signin_limit);

// Checks whether full management disclosure is needed for the public/managed
// session login screen UI. Full disclosure is needed if the session is
// managed and any risky extensions or network certificates are forced
// through the policies.
bool IsFullManagementDisclosureNeeded(
    policy::DeviceLocalAccountPolicyBroker* broker);

// Sets the available auth factors for the user on the login & lock screen.
void SetAuthFactorsForUser(const AccountId& user,
                           const SessionAuthFactors& auth_factors,
                           bool is_pin_disabled_by_policy,
                           LoginScreenModel* login_screen);

}  // namespace login
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_HELPER_H_
