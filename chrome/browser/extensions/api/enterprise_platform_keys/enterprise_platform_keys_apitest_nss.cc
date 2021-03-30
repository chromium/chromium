// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cryptohi.h>
#include <stddef.h>

#include <memory>
#include <string>

#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/chromeos/scoped_test_system_nss_key_slot_mixin.h"
#include "chrome/browser/extensions/api/platform_keys/platform_keys_test_base.h"
#include "chrome/browser/net/nss_context.h"
#include "chrome/browser/policy/extension_force_install_mixin.h"
#include "chrome/common/chrome_paths.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "crypto/nss_util_internal.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/scoped_test_system_nss_key_slot.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/api/test.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/cert/nss_cert_database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

// The ID of the enterprise.platformKeys API test extension. The code location
// of the extension is:
// chrome/test/data/extensions/api_test/enterprise_platform_keys/
constexpr char kExtensionId[] = "aecpbnckhoppanpmefllkdkohionpmig";

// The test extension has a certificate referencing this private key which will
// be stored in the user's token in the test setup.
//
// openssl genrsa > privkey.pem
// openssl pkcs8 -inform pem -in privkey.pem -topk8
//   -outform der -out privkey8.der -nocrypt
// xxd -i privkey8.der
const unsigned char privateKeyPkcs8User[] = {
    0x30, 0x82, 0x01, 0x55, 0x02, 0x01, 0x00, 0x30, 0x0d, 0x06, 0x09, 0x2a,
    0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x04, 0x82,
    0x01, 0x3f, 0x30, 0x82, 0x01, 0x3b, 0x02, 0x01, 0x00, 0x02, 0x41, 0x00,
    0xc7, 0xc1, 0x4d, 0xd5, 0xdc, 0x3a, 0x2e, 0x1f, 0x42, 0x30, 0x3d, 0x21,
    0x1e, 0xa2, 0x1f, 0x60, 0xcb, 0x71, 0x11, 0x53, 0xb0, 0x75, 0xa0, 0x62,
    0xfe, 0x5e, 0x0a, 0xde, 0xb0, 0x0f, 0x48, 0x97, 0x5e, 0x42, 0xa7, 0x3a,
    0xd1, 0xca, 0x4c, 0xe3, 0xdb, 0x5f, 0x31, 0xc2, 0x99, 0x08, 0x89, 0xcd,
    0x6d, 0x20, 0xaa, 0x75, 0xe6, 0x2b, 0x98, 0xd2, 0xf3, 0x7b, 0x4b, 0xe5,
    0x9b, 0xfe, 0xe2, 0x6d, 0x02, 0x03, 0x01, 0x00, 0x01, 0x02, 0x40, 0x4a,
    0xf5, 0x76, 0x10, 0xe7, 0xb8, 0x89, 0x70, 0x3f, 0x75, 0x3c, 0xab, 0x3e,
    0x04, 0x96, 0x83, 0xcb, 0x34, 0x1d, 0xcd, 0x6a, 0xed, 0x69, 0x07, 0x5c,
    0xee, 0xcb, 0x63, 0x6f, 0x6b, 0xfc, 0xcf, 0xee, 0xa2, 0xc4, 0x67, 0x05,
    0x68, 0x4d, 0x21, 0x7e, 0x3e, 0xde, 0x74, 0x72, 0xf8, 0x04, 0x35, 0x66,
    0x1e, 0x6b, 0x1d, 0xef, 0x77, 0xf7, 0x33, 0xf0, 0x35, 0xcf, 0x35, 0x6e,
    0x53, 0x3f, 0x9d, 0x02, 0x21, 0x00, 0xee, 0x48, 0x67, 0x1b, 0x24, 0x6e,
    0x3d, 0x7b, 0xa0, 0xc3, 0xee, 0x8a, 0x2e, 0xc7, 0xd0, 0xa1, 0xdb, 0x25,
    0x31, 0x12, 0x99, 0x43, 0x06, 0x3c, 0xb0, 0x80, 0x35, 0x2b, 0xf4, 0xc5,
    0xa2, 0xd3, 0x02, 0x21, 0x00, 0xd6, 0x9b, 0x8b, 0x75, 0x91, 0x52, 0xd4,
    0xf0, 0x76, 0xcf, 0xa2, 0xbe, 0xa6, 0xaf, 0x72, 0x6c, 0x52, 0xf9, 0xc9,
    0x0e, 0xea, 0x4a, 0x4c, 0xd2, 0xdf, 0x25, 0x70, 0xc6, 0x66, 0x35, 0x9d,
    0xbf, 0x02, 0x21, 0x00, 0xe8, 0x9e, 0x40, 0x21, 0xcc, 0x37, 0xde, 0xc7,
    0xd1, 0x13, 0x55, 0xcd, 0x0a, 0x8c, 0x40, 0xcd, 0xb1, 0xed, 0xa5, 0xf1,
    0x7d, 0x33, 0x64, 0x64, 0x5c, 0xfe, 0x5c, 0x6a, 0x34, 0x03, 0xb8, 0xc7,
    0x02, 0x20, 0x17, 0xe1, 0xb5, 0x52, 0x3e, 0xfa, 0xc5, 0xc1, 0x80, 0xa7,
    0x38, 0x88, 0x18, 0xca, 0x7b, 0x64, 0x3c, 0x93, 0x99, 0x61, 0x34, 0x87,
    0x52, 0x27, 0x41, 0x37, 0xcc, 0x65, 0xf7, 0xa7, 0xcd, 0xc7, 0x02, 0x21,
    0x00, 0x8a, 0x17, 0x7f, 0xf9, 0x45, 0xf3, 0xfd, 0xf7, 0x96, 0x62, 0xf3,
    0x7a, 0x09, 0xfb, 0xe9, 0x9e, 0xc7, 0x7a, 0x1f, 0x53, 0x1a, 0xb8, 0xd5,
    0x88, 0x9d, 0xd4, 0x79, 0x57, 0x88, 0x68, 0x72, 0x6f};

// The test extension has a certificate referencing this private key which will
// be stored in the system token in the test setup.
const unsigned char privateKeyPkcs8System[] = {
    0x30, 0x82, 0x01, 0x54, 0x02, 0x01, 0x00, 0x30, 0x0d, 0x06, 0x09, 0x2a,
    0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x04, 0x82,
    0x01, 0x3e, 0x30, 0x82, 0x01, 0x3a, 0x02, 0x01, 0x00, 0x02, 0x41, 0x00,
    0xe8, 0xb3, 0x04, 0xb1, 0xad, 0xef, 0x6b, 0xe5, 0xbe, 0xc9, 0x05, 0x75,
    0x07, 0x41, 0xf5, 0x70, 0x50, 0xc2, 0xe8, 0xee, 0xeb, 0x09, 0x9d, 0x49,
    0x64, 0x4c, 0x60, 0x61, 0x80, 0xbe, 0xc5, 0x41, 0xf3, 0x8c, 0x57, 0x90,
    0x3a, 0x44, 0x62, 0x6d, 0x51, 0xb8, 0xbb, 0xc6, 0x9a, 0x16, 0xdf, 0xf9,
    0xce, 0xe3, 0xb8, 0x8c, 0x2e, 0xa2, 0x16, 0xc8, 0xed, 0xc7, 0xf8, 0x4f,
    0xbd, 0xd3, 0x6e, 0x63, 0x02, 0x03, 0x01, 0x00, 0x01, 0x02, 0x40, 0x76,
    0xc9, 0x83, 0xf8, 0xeb, 0xd0, 0x8f, 0xa4, 0xdd, 0x4a, 0xa2, 0xe5, 0x85,
    0xc9, 0xee, 0xef, 0xe1, 0xda, 0x4d, 0xac, 0x41, 0x01, 0x4c, 0x70, 0x7d,
    0xa9, 0xdb, 0x7d, 0x8a, 0x8a, 0x58, 0x09, 0x04, 0x45, 0x43, 0xa4, 0xf3,
    0xb4, 0x98, 0xf6, 0x34, 0x68, 0x5f, 0xc1, 0xc2, 0xa7, 0x86, 0x3e, 0xec,
    0x84, 0x0b, 0x18, 0xbc, 0xb1, 0xee, 0x6f, 0x3f, 0xb1, 0x6d, 0xbc, 0x3e,
    0xbf, 0x6d, 0x31, 0x02, 0x21, 0x00, 0xff, 0x9d, 0x90, 0x4f, 0x0e, 0xe8,
    0x7e, 0xf3, 0x38, 0xa7, 0xec, 0x73, 0x80, 0xf9, 0x39, 0x2c, 0xaa, 0x33,
    0x91, 0x72, 0x10, 0x7c, 0x8b, 0xc3, 0x61, 0x6d, 0x40, 0x96, 0xac, 0xb3,
    0x5e, 0xc9, 0x02, 0x21, 0x00, 0xe9, 0x0c, 0xa1, 0x34, 0xf2, 0x43, 0x3c,
    0x74, 0xec, 0x1a, 0xf6, 0x80, 0x8e, 0x50, 0x10, 0x6d, 0x55, 0x64, 0xce,
    0x47, 0x4a, 0x1e, 0x34, 0x27, 0x6c, 0x49, 0x79, 0x6a, 0x23, 0xc6, 0x9d,
    0xcb, 0x02, 0x20, 0x48, 0xda, 0xa8, 0xc1, 0xcf, 0xb6, 0xf6, 0x4f, 0xee,
    0x4a, 0xf6, 0x3a, 0xa9, 0x7c, 0xdf, 0x0d, 0xda, 0xe8, 0xdd, 0xc0, 0x8b,
    0xf0, 0x63, 0x89, 0x69, 0x60, 0x51, 0x33, 0x60, 0xbf, 0xb2, 0xf9, 0x02,
    0x21, 0x00, 0xb4, 0x77, 0x81, 0x46, 0x7c, 0xec, 0x30, 0x1e, 0xe2, 0xcf,
    0x26, 0x5f, 0xfa, 0xd4, 0x69, 0x44, 0x21, 0x42, 0x84, 0xb2, 0x93, 0xe4,
    0xbb, 0xc2, 0x63, 0x8a, 0xaa, 0x28, 0xd5, 0x37, 0x72, 0xed, 0x02, 0x20,
    0x16, 0xde, 0x3d, 0x57, 0xc5, 0xd5, 0x3d, 0x90, 0x8b, 0xfd, 0x90, 0x3b,
    0xd8, 0x71, 0x69, 0x5e, 0x8d, 0xb4, 0x48, 0x1c, 0xa4, 0x01, 0xce, 0xc1,
    0xb5, 0x6f, 0xe9, 0x1b, 0x32, 0x91, 0x34, 0x38};

base::FilePath GetExtensionDirName() {
  return base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
      .Append(
          FILE_PATH_LITERAL("extensions/api_test/enterprise_platform_keys/"));
}

base::FilePath GetExtensionPemFileName() {
  return base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
      .Append(FILE_PATH_LITERAL(
          "extensions/api_test/enterprise_platform_keys.pem"));
}

// Returns the profile into which login-screen extensions are force-installed.
Profile* GetOriginalSigninProfile() {
  return chromeos::ProfileHelper::GetSigninProfile()->GetOriginalProfile();
}

enum class TestingMode {
  kUserSessionWithSystemTokenEnabledMode,
  kUserSessionWithSystemTokenDisabledMode,
  kLoginScreenMode
};

// Note: The strings returned by this function must match the strings defined in
// the .js test file (c/t/d/e/api_test/enterprise_platform_keys/background.js)
std::string TestingModeToString(TestingMode mode) {
  switch (mode) {
    case TestingMode::kUserSessionWithSystemTokenEnabledMode:
      return "User session with system token enabled mode.";
    case TestingMode::kUserSessionWithSystemTokenDisabledMode:
      return "User session with system token disabled mode.";
    case TestingMode::kLoginScreenMode:
      return "Login screen mode.";
  }
}

// Sends a message to the test extension to specify the type of the tests to
// run.
void RunTests(Profile* profile, TestingMode mode) {
  api::test::OnMessage::Info info;
  info.data = TestingModeToString(mode);

  auto event = std::make_unique<extensions::Event>(
      extensions::events::FOR_TEST,
      extensions::api::test::OnMessage::kEventName,
      api::test::OnMessage::Create(info), profile);
  extensions::EventRouter::Get(profile)->DispatchEventToExtension(
      kExtensionId, std::move(event));
}

void ImportPrivateKeyPKCS8ToSlot(const unsigned char* pkcs8_der,
                                 size_t pkcs8_der_size,
                                 PK11SlotInfo* slot) {
  SECItem pki_der_user = {
      siBuffer,
      // NSS requires non-const data even though it is just for input.
      const_cast<unsigned char*>(pkcs8_der), pkcs8_der_size};

  SECKEYPrivateKey* seckey_raw = nullptr;
  ASSERT_EQ(SECSuccess, PK11_ImportDERPrivateKeyInfoAndReturnKey(
                            slot, &pki_der_user,
                            /*nickname=*/nullptr,
                            /*publicValue=*/nullptr,
                            /*isPerm=*/true,
                            /*isPrivate=*/true,
                            /*usage=*/KU_ALL, &seckey_raw, /*wincx=*/nullptr));

  // Make sure that the memory allocated for the key gets freed.
  crypto::ScopedSECKEYPrivateKey seckey(seckey_raw);
}

struct Params {
  Params(PlatformKeysTestBase::SystemTokenStatus system_token_status,
         PlatformKeysTestBase::EnrollmentStatus enrollment_status,
         PlatformKeysTestBase::UserStatus user_status)
      : system_token_status_(system_token_status),
        enrollment_status_(enrollment_status),
        user_status_(user_status) {}

  PlatformKeysTestBase::SystemTokenStatus system_token_status_;
  PlatformKeysTestBase::EnrollmentStatus enrollment_status_;
  PlatformKeysTestBase::UserStatus user_status_;
};

class EnterprisePlatformKeysTest
    : public PlatformKeysTestBase,
      public ::testing::WithParamInterface<Params> {
 public:
  EnterprisePlatformKeysTest()
      : PlatformKeysTestBase(GetParam().system_token_status_,
                             GetParam().enrollment_status_,
                             GetParam().user_status_) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PlatformKeysTestBase::SetUpCommandLine(command_line);

    // Enable the WebCrypto API.
    command_line->AppendSwitch(
        ::switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpOnMainThread() override {
    PlatformKeysTestBase::SetUpOnMainThread();

    extension_force_install_mixin_.InitWithMockPolicyProvider(
        profile(), mock_policy_provider());
  }

  void DidGetCertDatabase(base::OnceClosure done_callback,
                          net::NSSCertDatabase* cert_db) {
    // In order to use a prepared certificate, import a private key to the
    // user's token for which the Javscript test will import the certificate.
    ImportPrivateKeyPKCS8ToSlot(privateKeyPkcs8User,
                                base::size(privateKeyPkcs8User),
                                cert_db->GetPrivateSlot().get());
    std::move(done_callback).Run();
  }

 protected:
  TestingMode GetTestingMode() {
    // Only if the system token exists, and the current user is of the same
    // domain as the device is enrolled to, the system token is available to the
    // extension.
    if (system_token_status() == SystemTokenStatus::EXISTS &&
        enrollment_status() == EnrollmentStatus::ENROLLED &&
        user_status() == UserStatus::MANAGED_AFFILIATED_DOMAIN) {
      return TestingMode::kUserSessionWithSystemTokenEnabledMode;
    }

    return TestingMode::kUserSessionWithSystemTokenDisabledMode;
  }

  ExtensionForceInstallMixin extension_force_install_mixin_{&mixin_host_};

 private:
  void PrepareTestSystemSlotOnIO(
      crypto::ScopedTestSystemNSSKeySlot* system_slot) override {
    // Import a private key to the system slot.  The Javascript part of this
    // test has a prepared certificate for this key.
    ImportPrivateKeyPKCS8ToSlot(privateKeyPkcs8System,
                                base::size(privateKeyPkcs8System),
                                system_slot->slot());
  }

  DISALLOW_COPY_AND_ASSIGN(EnterprisePlatformKeysTest);
};

}  // namespace

IN_PROC_BROWSER_TEST_P(EnterprisePlatformKeysTest, PRE_Basic) {
  RunPreTest();
}

IN_PROC_BROWSER_TEST_P(EnterprisePlatformKeysTest, Basic) {
  {
    base::RunLoop loop;
    GetNSSCertDatabaseForProfile(
        profile(),
        base::BindOnce(&EnterprisePlatformKeysTest::DidGetCertDatabase,
                       base::Unretained(this), loop.QuitClosure()));
    loop.Run();
  }

  extensions::ExtensionId extension_id;
  ASSERT_TRUE(extension_force_install_mixin_.ForceInstallFromSourceDir(
      GetExtensionDirName(), GetExtensionPemFileName(),
      ExtensionForceInstallMixin::WaitMode::kBackgroundPageFirstLoad,
      &extension_id));
  ASSERT_EQ(kExtensionId, extension_id);

  extensions::ResultCatcher catcher;
  RunTests(profile(), GetTestingMode());
  ASSERT_TRUE(catcher.GetNextResult());
}

INSTANTIATE_TEST_SUITE_P(
    CheckSystemTokenAvailability,
    EnterprisePlatformKeysTest,
    ::testing::Values(
        Params(PlatformKeysTestBase::SystemTokenStatus::EXISTS,
               PlatformKeysTestBase::EnrollmentStatus::ENROLLED,
               PlatformKeysTestBase::UserStatus::MANAGED_AFFILIATED_DOMAIN),
        Params(PlatformKeysTestBase::SystemTokenStatus::EXISTS,
               PlatformKeysTestBase::EnrollmentStatus::ENROLLED,
               PlatformKeysTestBase::UserStatus::MANAGED_OTHER_DOMAIN),
        Params(PlatformKeysTestBase::SystemTokenStatus::EXISTS,
               PlatformKeysTestBase::EnrollmentStatus::NOT_ENROLLED,
               PlatformKeysTestBase::UserStatus::MANAGED_OTHER_DOMAIN),
        Params(PlatformKeysTestBase::SystemTokenStatus::DOES_NOT_EXIST,
               PlatformKeysTestBase::EnrollmentStatus::ENROLLED,
               PlatformKeysTestBase::UserStatus::MANAGED_AFFILIATED_DOMAIN)));

// Ensure that extensions that are not pre-installed by policy throw an install
// warning if they request the enterprise.platformKeys permission in the
// manifest and that such extensions don't see the
// chrome.enterprise.platformKeys namespace.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest,
                       EnterprisePlatformKeysIsRestrictedToPolicyExtension) {
  ASSERT_TRUE(RunExtensionTest({.name = "enterprise_platform_keys",
                                .page_url = "api_not_available.html"},
                               {.ignore_manifest_warnings = true}));

  base::FilePath extension_path =
      test_data_dir_.AppendASCII("enterprise_platform_keys");
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  const Extension* extension =
      GetExtensionByPath(registry->enabled_extensions(), extension_path);
  ASSERT_FALSE(extension->install_warnings().empty());
  EXPECT_EQ(
      "'enterprise.platformKeys' is not allowed for specified install "
      "location.",
      extension->install_warnings()[0].message);
}

class EnterprisePlatformKeysLoginScreenTest
    : public MixinBasedInProcessBrowserTest {
 public:
  EnterprisePlatformKeysLoginScreenTest() = default;
  EnterprisePlatformKeysLoginScreenTest(
      const EnterprisePlatformKeysLoginScreenTest&) = delete;
  EnterprisePlatformKeysLoginScreenTest& operator=(
      const EnterprisePlatformKeysLoginScreenTest&) = delete;
  ~EnterprisePlatformKeysLoginScreenTest() override = default;

 protected:
  ExtensionForceInstallMixin* extension_force_install_mixin() {
    return &extension_force_install_mixin_;
  }

 private:
  void SetUp() override {
    chromeos::platform_keys::PlatformKeysServiceFactory::GetInstance()
        ->SetTestingMode(true);

    MixinBasedInProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();

    extension_force_install_mixin_.InitWithDeviceStateMixin(
        GetOriginalSigninProfile(), &device_state_mixin_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);

    command_line->AppendSwitch(chromeos::switches::kLoginManager);
    command_line->AppendSwitchASCII(switches::kAllowlistedExtensionID,
                                    kExtensionId);
  }

  chromeos::DeviceStateMixin device_state_mixin_{
      &mixin_host_,
      chromeos::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  chromeos::ScopedTestSystemNSSKeySlotMixin system_nss_key_slot_mixin_{
      &mixin_host_};
  ExtensionForceInstallMixin extension_force_install_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(EnterprisePlatformKeysLoginScreenTest, Basic) {
  extensions::ExtensionId extension_id;
  ASSERT_TRUE(extension_force_install_mixin()->ForceInstallFromSourceDir(
      GetExtensionDirName(), GetExtensionPemFileName(),
      ExtensionForceInstallMixin::WaitMode::kBackgroundPageFirstLoad,
      &extension_id));
  ASSERT_EQ(kExtensionId, extension_id);

  extensions::ResultCatcher catcher;
  RunTests(GetOriginalSigninProfile(), TestingMode::kLoginScreenMode);
  ASSERT_TRUE(catcher.GetNextResult());
}

}  // namespace extensions
