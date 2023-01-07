// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PLATFORM_KEYS_PLATFORM_KEYS_TEST_BASE_H_
#define CHROME_BROWSER_EXTENSIONS_API_PLATFORM_KEYS_PLATFORM_KEYS_TEST_BASE_H_

#include <memory>

#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/extensions/mixin_based_extension_apitest.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "google_apis/gaia/fake_gaia.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace crypto {
class ScopedTestSystemNSSKeySlot;
}

// An ExtensionApiTest which provides additional setup for system token
// availability, device enrollment status, user affiliation and user policy.
// Every test case is supposed to have a PRE_ test case which must call
// PlatformKeysTestBase::RunPreTest.
class PlatformKeysTestBase : public extensions::MixinBasedExtensionApiTest {
 public:
  enum class SystemTokenStatus { EXISTS, DOES_NOT_EXIST };

  enum class EnrollmentStatus { ENROLLED, NOT_ENROLLED };

  enum class UserStatus {
    UNMANAGED,
    MANAGED_AFFILIATED_DOMAIN,
    MANAGED_OTHER_DOMAIN
  };

  PlatformKeysTestBase(SystemTokenStatus system_token_status,
                       EnrollmentStatus enrollment_status,
                       UserStatus user_status);

  PlatformKeysTestBase(const PlatformKeysTestBase&) = delete;
  PlatformKeysTestBase& operator=(const PlatformKeysTestBase&) = delete;

  ~PlatformKeysTestBase() override;

 protected:
  // MixinBasedExtensionApiTest:
  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpInProcessBrowserTestFixture() override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  // Will be called with the system slot on the IO thread, if a system slot is
  // being created.  The subclass can override this to perform its own
  // preparations with the system slot.
  virtual void PrepareTestSystemSlotOnIO(
      crypto::ScopedTestSystemNSSKeySlot* system_slot);

  SystemTokenStatus system_token_status() const { return system_token_status_; }
  EnrollmentStatus enrollment_status() const { return enrollment_status_; }
  UserStatus user_status() const { return user_status_; }

  policy::MockConfigurationPolicyProvider* mock_policy_provider() {
    return &mock_policy_provider_;
  }

  crypto::ScopedTestSystemNSSKeySlot* test_system_slot() {
    return test_system_slot_.get();
  }

  // This must be called from the PRE_ test cases.
  void RunPreTest();

  // Load |page_url| in a new browser in the current profile and wait for PASSED
  // or FAILED notification. The functionality of this function is reduced
  // functionality of RunExtensionTest(), but we don't use it here because it
  // requires function InProcessBrowserTest::browser() to return non-NULL
  // pointer. Unfortunately it returns the value which is set in constructor and
  // can't be modified. Because on login flow there is no browser, the function
  // InProcessBrowserTest::browser() always returns NULL. Besides this we need
  // only very little functionality from RunExtensionTest(). Thus so that
  // don't make RunExtensionTest() too complex we just introduce a new
  // function.
  bool TestExtension(const std::string& page_url);

  // Returns true if called from a PRE_ test.
  bool IsPreTest();

 private:
  // Create test system slot and prepare crypto:: methods to use it when the
  // initialization starts.
  void CreateTestSystemSlot();
  // Destroy test system slot.
  void TearDownTestSystemSlotOnIO();

  const SystemTokenStatus system_token_status_;
  const EnrollmentStatus enrollment_status_;
  const UserStatus user_status_;

  const AccountId account_id_;

  policy::DevicePolicyCrosTestHelper device_policy_test_helper_;
  std::unique_ptr<crypto::ScopedTestSystemNSSKeySlot> test_system_slot_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider>
      mock_policy_provider_;
  FakeGaia fake_gaia_;
  net::EmbeddedTestServer gaia_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  ash::ScopedStubInstallAttributes install_attributes_;
  ash::CryptohomeMixin cryptohome_mixin_{&mixin_host_};
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_PLATFORM_KEYS_PLATFORM_KEYS_TEST_BASE_H_
