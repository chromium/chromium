// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pdf/pdf_extension_util.h"

#include <memory>
#include <optional>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "pdf/buildflags.h"
#include "pdf/pdf_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/login/users/scoped_account_id_annotator.h"  // nogncheck
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"  // nogncheck
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"  // nogncheck
#include "chromeos/ash/components/policy/device_local_account/device_local_account_type.h"  // nogncheck
#include "components/account_id/account_id.h"          // nogncheck
#include "components/account_id/account_id_literal.h"  // nogncheck
#include "components/prefs/testing_pref_service.h"     // nogncheck
#include "components/session_manager/test/test_user_session_manager.h"  // nogncheck
#include "components/user_manager/user_manager.h"  // nogncheck
#include "components/user_manager/user_names.h"    // nogncheck
#endif                                             // BUILDFLAG(IS_CHROMEOS)

namespace pdf_extension_util {
namespace {

#if BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)

#if BUILDFLAG(IS_CHROMEOS)
constexpr auto kRegularAccountId =
    AccountId::Literal::FromUserEmailGaiaId("user@gmail.com",
                                            GaiaId::Literal("1234567890"));
constexpr auto kChildAccountId =
    AccountId::Literal::FromUserEmailGaiaId("child@gmail.com",
                                            GaiaId::Literal("0987654321"));
#endif  // BUILDFLAG(IS_CHROMEOS)

class PdfExtensionUtilTest : public testing::Test {
 public:
  PdfExtensionUtilTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
#if BUILDFLAG(IS_CHROMEOS)
    test_user_session_manager_ =
        std::make_unique<ash::test::TestUserSessionManager>(
            TestingBrowserProcess::GetGlobal()->GetTestingLocalState());
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

#if BUILDFLAG(IS_CHROMEOS)
  void TearDown() override {
    profile_manager_.DeleteAllTestingProfiles();
    test_user_session_manager_.reset();
  }

  ash::test::TestUserSessionManager* user_session_manager() {
    return test_user_session_manager_.get();
  }

  TestingProfile* CreateProfileForUser(const AccountId& account_id,
                                       const std::string& profile_name) {
    ash::ScopedAccountIdAnnotator annotator(profile_manager_.profile_manager(),
                                            account_id);
    return profile_manager_.CreateTestingProfile(profile_name);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  TestingProfileManager* profile_manager() { return &profile_manager_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<ash::test::TestUserSessionManager> test_user_session_manager_;
#endif  // BUILDFLAG(IS_CHROMEOS)
};

#if BUILDFLAG(IS_CHROMEOS)

TEST_F(PdfExtensionUtilTest, IsPdfSaveToDriveEnabledRegularUsers) {
  // Add all users before any LogIn calls.
  ASSERT_TRUE(user_session_manager()->AddRegularUser(kRegularAccountId));
  ASSERT_TRUE(user_session_manager()->AddChildUser(kChildAccountId));

  // Test regular user session.
  user_session_manager()->LogIn(kRegularAccountId);
  auto* regular_profile =
      CreateProfileForUser(kRegularAccountId, "regular_profile");

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(chrome_pdf::features::kPdfSaveToDrive);

    content::TestWebContentsFactory factory;
    content::WebContents* web_contents =
        factory.CreateWebContents(regular_profile);
    base::DictValue additional_data = GetAdditionalData(web_contents);
    EXPECT_THAT(additional_data.FindBool("pdfSaveToDrive"),
                testing::Optional(true));
    const std::string* help_center_url =
        additional_data.FindString("pdfSaveToDriveHelpCenterURL");
    ASSERT_TRUE(help_center_url);
    EXPECT_EQ(*help_center_url, chrome::kPdfViewerSaveToDriveHelpCenterURL);

    TestingProfile* otr_profile =
        TestingProfile::Builder().BuildIncognito(regular_profile);
    content::WebContents* otr_web_contents =
        factory.CreateWebContents(otr_profile);
    base::DictValue otr_additional_data = GetAdditionalData(otr_web_contents);
    EXPECT_THAT(otr_additional_data.FindBool("pdfSaveToDrive"),
                testing::Optional(false));
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(chrome_pdf::features::kPdfSaveToDrive);

    content::TestWebContentsFactory factory;
    content::WebContents* web_contents =
        factory.CreateWebContents(regular_profile);
    base::DictValue additional_data = GetAdditionalData(web_contents);
    EXPECT_THAT(additional_data.FindBool("pdfSaveToDrive"),
                testing::Optional(false));
  }

  // Test child user session.
  user_session_manager()->LogIn(kChildAccountId);
  auto* child_profile = CreateProfileForUser(kChildAccountId, "child_profile");

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(chrome_pdf::features::kPdfSaveToDrive);

    content::TestWebContentsFactory factory;
    content::WebContents* web_contents =
        factory.CreateWebContents(child_profile);
    base::DictValue additional_data = GetAdditionalData(web_contents);
    EXPECT_THAT(additional_data.FindBool("pdfSaveToDrive"),
                testing::Optional(true));

    TestingProfile* otr_profile =
        TestingProfile::Builder().BuildIncognito(child_profile);
    content::WebContents* otr_web_contents =
        factory.CreateWebContents(otr_profile);
    base::DictValue otr_additional_data = GetAdditionalData(otr_web_contents);
    EXPECT_THAT(otr_additional_data.FindBool("pdfSaveToDrive"),
                testing::Optional(false));
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(chrome_pdf::features::kPdfSaveToDrive);

    content::TestWebContentsFactory factory;
    content::WebContents* web_contents =
        factory.CreateWebContents(child_profile);
    base::DictValue additional_data = GetAdditionalData(web_contents);
    EXPECT_THAT(additional_data.FindBool("pdfSaveToDrive"),
                testing::Optional(false));
  }
}

TEST_F(PdfExtensionUtilTest, IsPdfSaveToDriveEnabledNonRegularUsers) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(chrome_pdf::features::kPdfSaveToDrive);

  // Add all users before any LogIn calls.
  ASSERT_TRUE(user_session_manager()->AddGuestUser());
  const AccountId guest_id = user_manager::GuestAccountId();

  std::string public_session_id = policy::GenerateDeviceLocalAccountUserId(
      "public_session", policy::DeviceLocalAccountType::kPublicSession);
  ASSERT_TRUE(user_session_manager()->AddPublicAccountUser(public_session_id));
  const AccountId public_account_id =
      AccountId::FromUserEmail(public_session_id);

  std::string kiosk_chrome_id = policy::GenerateDeviceLocalAccountUserId(
      "kiosk_chrome", policy::DeviceLocalAccountType::kKioskApp);
  ASSERT_TRUE(user_session_manager()->AddKioskChromeAppUser(kiosk_chrome_id));
  const AccountId kiosk_chrome_account_id =
      AccountId::FromUserEmail(kiosk_chrome_id);

  std::string kiosk_web_id = policy::GenerateDeviceLocalAccountUserId(
      "kiosk_web", policy::DeviceLocalAccountType::kWebKioskApp);
  ASSERT_TRUE(user_session_manager()->AddKioskWebAppUser(kiosk_web_id));
  const AccountId kiosk_web_account_id = AccountId::FromUserEmail(kiosk_web_id);

  std::string kiosk_iwa_id = policy::GenerateDeviceLocalAccountUserId(
      "kiosk_iwa", policy::DeviceLocalAccountType::kKioskIsolatedWebApp);
  ASSERT_TRUE(user_session_manager()->AddKioskIwaUser(kiosk_iwa_id));
  const AccountId kiosk_iwa_account_id = AccountId::FromUserEmail(kiosk_iwa_id);

  content::TestWebContentsFactory factory;
  auto check_save_to_drive_disabled = [&factory](Profile* profile) {
    content::WebContents* web_contents = factory.CreateWebContents(profile);
    base::DictValue additional_data = GetAdditionalData(web_contents);
    EXPECT_THAT(additional_data.FindBool("pdfSaveToDrive"),
                testing::Optional(false));
  };

  // Guest user.
  user_session_manager()->LogIn(guest_id);
  auto* guest_profile = CreateProfileForUser(guest_id, "guest_profile");
  check_save_to_drive_disabled(guest_profile);

  // Public account (managed guest session).
  user_session_manager()->LogIn(public_account_id);
  auto* public_profile =
      CreateProfileForUser(public_account_id, "public_profile");
  check_save_to_drive_disabled(public_profile);

  // Kiosk Chrome App.
  user_session_manager()->LogIn(kiosk_chrome_account_id);
  auto* kiosk_chrome_profile =
      CreateProfileForUser(kiosk_chrome_account_id, "kiosk_chrome_profile");
  check_save_to_drive_disabled(kiosk_chrome_profile);

  // Kiosk Web App.
  user_session_manager()->LogIn(kiosk_web_account_id);
  auto* kiosk_web_profile =
      CreateProfileForUser(kiosk_web_account_id, "kiosk_web_profile");
  check_save_to_drive_disabled(kiosk_web_profile);

  // Kiosk IWA.
  user_session_manager()->LogIn(kiosk_iwa_account_id);
  auto* kiosk_iwa_profile =
      CreateProfileForUser(kiosk_iwa_account_id, "kiosk_iwa_profile");
  check_save_to_drive_disabled(kiosk_iwa_profile);

  // Non-user profile (e.g. sign-in screen).
  auto* signin_profile = profile_manager()->CreateTestingProfile(
      ash::kSigninBrowserContextBaseName);
  check_save_to_drive_disabled(signin_profile);
}

#else  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(PdfExtensionUtilTest, IsPdfSaveToDriveEnabled) {
  auto* profile = profile_manager()->CreateTestingProfile("test_profile");

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(chrome_pdf::features::kPdfSaveToDrive);

    content::TestWebContentsFactory factory;
    content::WebContents* web_contents = factory.CreateWebContents(profile);
    base::DictValue additional_data = GetAdditionalData(web_contents);
    EXPECT_THAT(additional_data.FindBool("pdfSaveToDrive"),
                testing::Optional(true));
    const std::string* help_center_url =
        additional_data.FindString("pdfSaveToDriveHelpCenterURL");
    ASSERT_TRUE(help_center_url);
    EXPECT_EQ(*help_center_url, chrome::kPdfViewerSaveToDriveHelpCenterURL);

    TestingProfile* otr_profile =
        TestingProfile::Builder().BuildIncognito(profile);
    content::WebContents* otr_web_contents =
        factory.CreateWebContents(otr_profile);
    base::DictValue otr_additional_data = GetAdditionalData(otr_web_contents);
    EXPECT_THAT(otr_additional_data.FindBool("pdfSaveToDrive"),
                testing::Optional(false));
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(chrome_pdf::features::kPdfSaveToDrive);

    content::TestWebContentsFactory factory;
    content::WebContents* web_contents = factory.CreateWebContents(profile);
    base::DictValue additional_data = GetAdditionalData(web_contents);
    EXPECT_THAT(additional_data.FindBool("pdfSaveToDrive"),
                testing::Optional(false));
  }
}

#endif  // BUILDFLAG(IS_CHROMEOS)

#endif  // BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)

}  // namespace
}  // namespace pdf_extension_util
