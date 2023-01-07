// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_AUTHPOLICY_AUTHPOLICY_HELPER_H_
#define CHROME_BROWSER_ASH_AUTHPOLICY_AUTHPOLICY_HELPER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/authpolicy/authpolicy_client.h"

namespace ash {

// Helper class to use AuthPolicyClient. For Active Directory domain join and
// authenticate users this class should be used instead of AuthPolicyClient.
// Allows canceling all pending calls and restarting AuthPolicy service. Used
// for enrollment and login UI to proper cancel the flows.
class AuthPolicyHelper {
 public:
  using AuthCallback = AuthPolicyClient::AuthCallback;
  using JoinCallback = AuthPolicyClient::JoinCallback;
  using RefreshPolicyCallback = AuthPolicyClient::RefreshPolicyCallback;
  using OnDecryptedCallback =
      base::OnceCallback<void(std::string decrypted_data)>;

  AuthPolicyHelper();

  AuthPolicyHelper(const AuthPolicyHelper&) = delete;
  AuthPolicyHelper& operator=(const AuthPolicyHelper&) = delete;

  ~AuthPolicyHelper();

  // Tries to get Kerberos TGT. To get TGT and password statuses one should use
  // AuthPolicyClient::GetUserStatus afterwards.
  static void TryAuthenticateUser(const std::string& username,
                                  const std::string& object_guid,
                                  const std::string& password);

  // Restarts AuthPolicy service.
  static void Restart();

  // Decrypts |blob| with |password| on a separate thread. Calls |callback| on
  // the orginal thread. If decryption failed |callback| called with an empty
  // string.
  static void DecryptConfiguration(const std::string& blob,
                                   const std::string& password,
                                   OnDecryptedCallback callback);

  // Packs arguments and calls AuthPolicyClient::JoinAdDomain. Joins machine to
  // Active directory domain. Then it calls RefreshDevicePolicy to cache the
  // policy on the authpolicyd side. |machine_name| is a name for a local
  // machine. If |distinguished_name| is not empty |machine| would be put into
  // that domain or/and organizational unit structure. Otherwise |machine| would
  // be joined to domain of the |username|. |username|, |password| are
  // credentials of the Active directory account which has right to join the
  // machine to the domain. |callback| is called after getting (or failing to
  // get) D-BUS response.
  void JoinAdDomain(const std::string& machine_name,
                    const std::string& distinguished_name,
                    int encryption_types,
                    const std::string& username,
                    const std::string& password,
                    JoinCallback callback);

  // Packs arguments and calls AuthPolicyClient::AuthenticateUser. Authenticates
  // user against Active Directory server. |username|, |password| are
  // credentials of the Active Directory account. |username| should be in the
  // user@example.domain.com format. |object_guid| is the user's LDAP GUID. If
  // specified, it is used instead of |username|. The GUID is guaranteed to be
  // stable, the user's name can change on the server.
  void AuthenticateUser(const std::string& username,
                        const std::string& object_guid,
                        const std::string& password,
                        AuthCallback callback);

  // Refreshes device policy. Waits for authpolicy D-Bus service to start if
  // needed. When Chrome starts it tries to refresh device policy immediately.
  // If authpolicy daemon being started at the same time - device policy fetch
  // could fail. Could happen after reboot only on the login screen. So handle
  // it for device policy only.
  void RefreshDevicePolicy(RefreshPolicyCallback callback);
  // Does not wait for authpolicyd D-Bus service. Added for symmetry.
  void RefreshUserPolicy(const AccountId& account_id,
                         RefreshPolicyCallback callback) const;

  // Cancels pending requests and restarts AuthPolicy service.
  void CancelRequestsAndRestart();

  // Sets the DM token. Will be sent to authpolicy with the domain join call.
  // Authpolicy would set it in the device policy.
  void set_dm_token(const std::string& dm_token) { dm_token_ = dm_token; }

 private:
  void OnServiceAvailable(bool service_is_available);

  // Called from AuthPolicyClient::JoinAdDomain.
  void OnJoinCallback(JoinCallback callback,
                      authpolicy::ErrorType error,
                      const std::string& machine_domain);

  // Called from AuthPolicyClient::RefreshDevicePolicy. This is used only once
  // during device enrollment with the first device policy refresh.
  void OnFirstPolicyRefreshCallback(JoinCallback callback,
                                    const std::string& machine_domain,
                                    authpolicy::ErrorType error);

  // Called from AuthPolicyClient::AuthenticateUser.
  void OnAuthCallback(
      AuthCallback callback,
      authpolicy::ErrorType error,
      const authpolicy::ActiveDirectoryAccountInfo& account_info);

  std::string dm_token_;

  bool service_is_available_ = false;
  RefreshPolicyCallback device_policy_callback_;

  base::WeakPtrFactory<AuthPolicyHelper> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_AUTHPOLICY_AUTHPOLICY_HELPER_H_
