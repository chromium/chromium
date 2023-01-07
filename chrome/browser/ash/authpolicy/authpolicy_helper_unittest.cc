// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/authpolicy/authpolicy_helper.h"

#include "base/functional/bind.h"
#include "chromeos/ash/components/dbus/authpolicy/fake_authpolicy_client.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_install_attributes_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

constexpr char kDMToken[] = "dm_token";

class MockAuthPolicyClient : public FakeAuthPolicyClient {
 public:
  MockAuthPolicyClient() { SetStarted(true); }

  MockAuthPolicyClient(const MockAuthPolicyClient&) = delete;
  MockAuthPolicyClient& operator=(const MockAuthPolicyClient&) = delete;

  ~MockAuthPolicyClient() override = default;

  void JoinAdDomain(const authpolicy::JoinDomainRequest& request,
                    int password_fd,
                    JoinCallback callback) override {
    EXPECT_FALSE(join_ad_domain_called_);
    EXPECT_FALSE(refresh_device_policy_called_);
    join_ad_domain_called_ = true;
    dm_token_ = request.dm_token();
    std::move(callback).Run(authpolicy::ERROR_NONE, std::string());
  }

  void RefreshDevicePolicy(RefreshPolicyCallback callback) override {
    EXPECT_TRUE(join_ad_domain_called_);
    EXPECT_FALSE(refresh_device_policy_called_);
    refresh_device_policy_called_ = true;
    std::move(callback).Run(
        authpolicy::ERROR_DEVICE_POLICY_CACHED_BUT_NOT_SENT);
  }

  void CheckExpectations() {
    EXPECT_TRUE(join_ad_domain_called_);
    EXPECT_TRUE(refresh_device_policy_called_);
    EXPECT_EQ(dm_token_, kDMToken);
  }

 private:
  bool join_ad_domain_called_ = false;
  bool refresh_device_policy_called_ = false;
  std::string dm_token_;
};

}  // namespace

// Check that helper calls RefreshDevicePolicy after JoinAdDomain.
TEST(AuthPolicyHelper, JoinFollowedByRefreshDevicePolicy) {
  ScopedStubInstallAttributes scoped_stub_install_attributes;

  auto* mock_client = new MockAuthPolicyClient;
  InstallAttributesClient::InitializeFake();

  AuthPolicyHelper helper;
  helper.set_dm_token(kDMToken);
  helper.JoinAdDomain(std::string(), std::string(),
                      authpolicy::KerberosEncryptionTypes(), std::string(),
                      std::string(),
                      base::BindOnce([](authpolicy::ErrorType error,
                                        const std::string& domain) {
                        EXPECT_EQ(authpolicy::ERROR_NONE, error);
                        EXPECT_TRUE(domain.empty());
                      }));
  mock_client->CheckExpectations();

  InstallAttributesClient::Shutdown();
  AuthPolicyClient::Shutdown();
}

}  // namespace ash
