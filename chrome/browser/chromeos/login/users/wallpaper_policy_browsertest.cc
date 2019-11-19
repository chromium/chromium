// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/wallpaper_controller_observer.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/device_state_mixin.h"
#include "chrome/browser/chromeos/login/test/fake_gaia_mixin.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/cloud_external_data_manager_base_test_util.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/constants/chromeos_paths.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/constants/dbus_paths.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_test_utils.h"
#include "crypto/rsa_private_key.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

const char kRedImageFileName[] = "chromeos/wallpapers/red.jpg";
const char kGreenImageFileName[] = "chromeos/wallpapers/green.jpg";
const char kBlueImageFileName[] = "chromeos/wallpapers/blue.jpg";

const SkColor kRedImageColor = SkColorSetARGB(255, 199, 6, 7);
const SkColor kGreenImageColor = SkColorSetARGB(255, 38, 196, 15);

policy::CloudPolicyStore* GetStoreForUser(const user_manager::User* user) {
  Profile* profile = ProfileHelper::Get()->GetProfileByUserUnsafe(user);
  if (!profile) {
    ADD_FAILURE();
    return NULL;
  }
  policy::UserCloudPolicyManagerChromeOS* policy_manager =
      profile->GetUserCloudPolicyManagerChromeOS();
  if (!policy_manager) {
    ADD_FAILURE();
    return NULL;
  }
  return policy_manager->core()->store();
}

// Compute the average ARGB color of |bitmap|.
SkColor ComputeAverageColor(const SkBitmap& bitmap) {
  if (bitmap.empty() || bitmap.width() < 1 || bitmap.height() < 1) {
    ADD_FAILURE() << "Empty or invalid bitmap.";
    return SkColorSetARGB(0, 0, 0, 0);
  }
  if (bitmap.isNull()) {
    ADD_FAILURE() << "Bitmap has no pixelref.";
    return SkColorSetARGB(0, 0, 0, 0);
  }
  if (bitmap.colorType() == kUnknown_SkColorType) {
    ADD_FAILURE() << "Bitmap has not been configured.";
    return SkColorSetARGB(0, 0, 0, 0);
  }
  uint64_t a = 0, r = 0, g = 0, b = 0;
  for (int x = 0; x < bitmap.width(); ++x) {
    for (int y = 0; y < bitmap.height(); ++y) {
      const SkColor color = bitmap.getColor(x, y);
      a += SkColorGetA(color);
      r += SkColorGetR(color);
      g += SkColorGetG(color);
      b += SkColorGetB(color);
    }
  }
  uint64_t pixel_number = bitmap.width() * bitmap.height();
  return SkColorSetARGB((a + pixel_number / 2) / pixel_number,
                        (r + pixel_number / 2) / pixel_number,
                        (g + pixel_number / 2) / pixel_number,
                        (b + pixel_number / 2) / pixel_number);
}

// Initialize system salt to calculate wallpaper file names.
void SetSystemSalt() {
  chromeos::SystemSaltGetter::Get()->SetRawSaltForTesting(
      chromeos::SystemSaltGetter::RawSalt({1, 2, 3, 4, 5, 6, 7, 8}));
}

}  // namespace

class WallpaperPolicyTest : public LoginManagerTest,
                            public ash::WallpaperControllerObserver {
 protected:
  WallpaperPolicyTest()
      : LoginManagerTest(true, true),
        owner_key_util_(new ownership::MockOwnerKeyUtil()) {
    testUsers_.push_back(
        AccountId::FromUserEmailGaiaId(FakeGaiaMixin::kEnterpriseUser1,
                                       FakeGaiaMixin::kEnterpriseUser1GaiaId));
    testUsers_.push_back(
        AccountId::FromUserEmailGaiaId(FakeGaiaMixin::kEnterpriseUser2,
                                       FakeGaiaMixin::kEnterpriseUser2GaiaId));
  }

  std::unique_ptr<policy::UserPolicyBuilder> GetUserPolicyBuilder(
      const AccountId& account_id) {
    std::unique_ptr<policy::UserPolicyBuilder> user_policy_builder(
        new policy::UserPolicyBuilder());
    base::FilePath user_keys_dir;
    EXPECT_TRUE(base::PathService::Get(dbus_paths::DIR_USER_POLICY_KEYS,
                                       &user_keys_dir));
    const std::string sanitized_user_id =
        CryptohomeClient::GetStubSanitizedUsername(
            cryptohome::CreateAccountIdentifierFromAccountId(account_id));
    const base::FilePath user_key_file =
        user_keys_dir.AppendASCII(sanitized_user_id).AppendASCII("policy.pub");
    std::string user_key_bits =
        user_policy_builder->GetPublicSigningKeyAsString();
    EXPECT_FALSE(user_key_bits.empty());
    EXPECT_TRUE(base::CreateDirectory(user_key_file.DirName()));
    EXPECT_EQ(base::WriteFile(user_key_file, user_key_bits.data(),
                              user_key_bits.length()),
              base::checked_cast<int>(user_key_bits.length()));
    user_policy_builder->policy_data().set_username(account_id.GetUserEmail());
    user_policy_builder->policy_data().set_gaia_id(account_id.GetGaiaId());
    return user_policy_builder;
  }

  // LoginManagerTest:
  void SetUpInProcessBrowserTestFixture() override {
    device_policy_.Build();
    OwnerSettingsServiceChromeOSFactory::GetInstance()
        ->SetOwnerKeyUtilForTesting(owner_key_util_);
    owner_key_util_->SetPublicKeyFromPrivateKey(
        *device_policy_.GetSigningKey());
    SessionManagerClient::InitializeFakeInMemory();
    FakeSessionManagerClient::Get()->set_device_policy(
        device_policy_.GetBlob());

    LoginManagerTest::SetUpInProcessBrowserTestFixture();
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_));

    // Set some fake state keys to make sure they are not empty.
    std::vector<std::string> state_keys;
    state_keys.push_back("1");
    FakeSessionManagerClient::Get()->set_server_backed_state_keys(state_keys);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Set the same switches as LoginManagerTest, except that kMultiProfiles is
    // only set when GetParam() is true and except that kLoginProfile is set
    // when GetParam() is false.  The latter seems to be required for the sane
    // start-up of user profiles.
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitch(switches::kForceLoginManagerInTests);

    // Allow policy fetches to fail - these tests instead invoke InjectPolicy()
    // to directly inject and modify policy dynamically.
    command_line->AppendSwitch(switches::kAllowFailedPolicyFetchForTest);

    LoginManagerTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    LoginManagerTest::SetUpOnMainThread();
    WallpaperControllerClient::Get()->AddObserver(this);

    // Set up policy signing.
    user_policy_builders_[0] = GetUserPolicyBuilder(testUsers_[0]);
    user_policy_builders_[1] = GetUserPolicyBuilder(testUsers_[1]);
  }

  void TearDownOnMainThread() override {
    WallpaperControllerClient::Get()->RemoveObserver(this);
    LoginManagerTest::TearDownOnMainThread();
  }

  // Obtain wallpaper image and return its average ARGB color.
  SkColor GetAverageWallpaperColor() {
    average_color_.reset();
    auto image = WallpaperControllerClient::Get()->GetWallpaperImage();
    const gfx::ImageSkiaRep& representation = image.GetRepresentation(1.0f);
    if (representation.is_null()) {
      ADD_FAILURE() << "No image representation.";
      average_color_ = SkColorSetARGB(0, 0, 0, 0);
    }
    average_color_ = ComputeAverageColor(representation.GetBitmap());
    return average_color_.value();
  }

  // ash::WallpaperControllerObserver:
  void OnWallpaperChanged() override {
    ++wallpaper_change_count_;
    if (run_loop_)
      run_loop_->Quit();
  }

  // Runs the loop until wallpaper has changed to the expected color.
  void RunUntilWallpaperChangeToColor(const SkColor& expected_color) {
    while (expected_color != GetAverageWallpaperColor()) {
      run_loop_.reset(new base::RunLoop);
      run_loop_->Run();
    }
  }

  std::string ConstructPolicy(const std::string& relative_path) const {
    base::ScopedAllowBlockingForTesting allow_io;
    std::string image_data;
    if (!base::ReadFileToString(test_data_dir_.Append(relative_path),
                                &image_data)) {
      ADD_FAILURE();
    }
    std::string policy;
    base::JSONWriter::Write(*policy::test::ConstructExternalDataReference(
                                embedded_test_server()
                                    ->GetURL(std::string("/") + relative_path)
                                    .spec(),
                                image_data),
                            &policy);
    return policy;
  }

  // Inject |filename| as wallpaper policy for test user |user_number|.  Set
  // empty |filename| to clear policy.
  void InjectPolicy(int user_number, const std::string& filename) {
    ASSERT_TRUE(user_number == 0 || user_number == 1);
    const AccountId& account_id = testUsers_[user_number];
    policy::UserPolicyBuilder* builder =
        user_policy_builders_[user_number].get();
    if (!filename.empty()) {
      builder->payload().mutable_wallpaperimage()->set_value(
          ConstructPolicy(filename));
    } else {
      builder->payload().Clear();
    }
    builder->Build();
    FakeSessionManagerClient::Get()->set_user_policy(
        cryptohome::CreateAccountIdentifierFromAccountId(account_id),
        builder->GetBlob());
    const user_manager::User* user =
        user_manager::UserManager::Get()->FindUser(account_id);
    ASSERT_TRUE(user);
    policy::CloudPolicyStore* store = GetStoreForUser(user);
    ASSERT_TRUE(store);
    store->Load();
    ASSERT_EQ(policy::CloudPolicyStore::STATUS_OK, store->status());
    ASSERT_EQ(policy::CloudPolicyValidatorBase::VALIDATION_OK,
              store->validation_status());
  }

  // Inject |filename| as the device wallpaper policy. Set empty |filename| to
  // clear policy.
  void InjectDevicePolicy(const std::string& filename) {
    if (!filename.empty()) {
      device_policy_.payload()
          .mutable_device_wallpaper_image()
          ->set_device_wallpaper_image(ConstructPolicy(filename));
    } else {
      device_policy_.payload().Clear();
    }
    device_policy_.Build();
    FakeSessionManagerClient::Get()->set_device_policy(
        device_policy_.GetBlob());
    FakeSessionManagerClient::Get()->OnPropertyChangeComplete(
        true /* success */);
  }

  base::FilePath test_data_dir_;
  std::unique_ptr<base::RunLoop> run_loop_;
  int wallpaper_change_count_ = 0;
  std::unique_ptr<policy::UserPolicyBuilder> user_policy_builders_[2];
  policy::DevicePolicyBuilder device_policy_;
  scoped_refptr<ownership::MockOwnerKeyUtil> owner_key_util_;
  std::vector<AccountId> testUsers_;
  FakeGaiaMixin fake_gaia_{&mixin_host_, embedded_test_server()};
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};

 private:
  // The average ARGB color of the current wallpaper.
  base::Optional<SkColor> average_color_;

  base::WeakPtrFactory<WallpaperPolicyTest> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WallpaperPolicyTest);
};

IN_PROC_BROWSER_TEST_F(WallpaperPolicyTest, PRE_SetResetClear) {
  RegisterUser(testUsers_[0]);
  RegisterUser(testUsers_[1]);
  StartupUtils::MarkOobeCompleted();
}

// Verifies that the wallpaper can be set and re-set through policy and that
// setting policy for a user that is not logged in doesn't affect the current
// user.  Also verifies that after the policy has been cleared, the wallpaper
// reverts to default.
//
// Disabled due to flakiness: https://crbug.com/873908.
IN_PROC_BROWSER_TEST_F(WallpaperPolicyTest, DISABLED_SetResetClear) {
  SetSystemSalt();
  LoginUser(testUsers_[0]);

  // First user: Stores the average color of the default wallpaper (set
  // automatically) to be compared against later.
  const SkColor original_wallpaper_color = GetAverageWallpaperColor();

  // Second user: Set wallpaper policy to blue image.  This should not result in
  // a wallpaper change, which is checked at the very end of this test.
  InjectPolicy(1, kBlueImageFileName);

  // First user: Set wallpaper policy to red image and verify average color.
  InjectPolicy(0, kRedImageFileName);
  RunUntilWallpaperChangeToColor(kRedImageColor);

  // First user: Set wallpaper policy to green image and verify average color.
  InjectPolicy(0, kGreenImageFileName);
  RunUntilWallpaperChangeToColor(kGreenImageColor);

  // First user: Clear wallpaper policy and verify that the default wallpaper is
  // set again.
  InjectPolicy(0, "");
  RunUntilWallpaperChangeToColor(original_wallpaper_color);

  // Check wallpaper change count to ensure that setting the second user's
  // wallpaper didn't have any effect.
  ASSERT_EQ(3, wallpaper_change_count_);
}

IN_PROC_BROWSER_TEST_F(WallpaperPolicyTest, PRE_DevicePolicyTest) {
  SetSystemSalt();
  RegisterUser(testUsers_[0]);
  StartupUtils::MarkOobeCompleted();
}

// Test that if device policy wallpaper and user policy wallpaper are both
// specified, the device policy wallpaper is used in the login screen and the
// user policy wallpaper is used inside of a user session.
IN_PROC_BROWSER_TEST_F(WallpaperPolicyTest, DevicePolicyTest) {
  SetSystemSalt();
  const SkColor original_wallpaper_color = GetAverageWallpaperColor();

  // Set the device wallpaper policy. Test that the device policy controlled
  // wallpaper shows up in the login screen.
  InjectDevicePolicy(kRedImageFileName);
  RunUntilWallpaperChangeToColor(kRedImageColor);

  // Log in a test user. The default wallpaper should be shown to replace the
  // device policy wallpaper.
  LoginUser(testUsers_[0]);
  RunUntilWallpaperChangeToColor(original_wallpaper_color);

  // Now set the user wallpaper policy. The user policy controlled wallpaper
  // should show up in the user session.
  InjectPolicy(0, kGreenImageFileName);
  RunUntilWallpaperChangeToColor(kGreenImageColor);
}

}  // namespace chromeos
