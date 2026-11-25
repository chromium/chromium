// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_PROFILE_CONTEXT_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_PROFILE_CONTEXT_DELEGATE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/enterprise/client_certificates/core/context_delegate.h"

class Profile;

namespace device_signals {
class UserPermissionService;
}  // namespace device_signals

namespace enterprise {
class ProfileIdService;
}  // namespace enterprise

namespace client_certificates {

class ProfileContextDelegate : public ContextDelegate {
 public:
  // `profile` and `profile_id_service` must be non-null and outlive `this`.
  // `user_permission_service` is null on ChromeOS (where `computer_name` is
  // unsupported) and in unit tests where `DeviceTrustConnectorServiceFactory`
  // returns null via `ServiceIsNULLWhileTesting()`; when null, `computer_name`
  // is not used and `GetExpectedCommonName()` falls back to the profile ID.
  ProfileContextDelegate(
      Profile* profile,
      enterprise::ProfileIdService* profile_id_service,
      device_signals::UserPermissionService* user_permission_service);
  ~ProfileContextDelegate() override;

  // ContextDelegate:
  void OnClientCertificateDeleted(
      scoped_refptr<net::X509Certificate> certificate) override;
  std::string GetExpectedCommonName() const override;
  std::string GetIdentityName() override;
  std::string GetTemporaryIdentityName() override;
  std::string GetPolicyPref() override;
  std::string GetConfigPolicyPref() const override;
  std::string GetLoggingContext() override;

 private:
  // Evaluates whether the profile user has permission to include
  // `computer_name` in the certificate Common Name, recording the permission
  // metric and fallback log at most once per profile session.
  bool CanUseComputerName() const;

  const raw_ptr<Profile> profile_;
  const raw_ptr<enterprise::ProfileIdService> profile_id_service_;
  const raw_ptr<device_signals::UserPermissionService> user_permission_service_;
  mutable bool has_recorded_computer_name_permission_ = false;
};

}  // namespace client_certificates

#endif  // CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_PROFILE_CONTEXT_DELEGATE_H_
