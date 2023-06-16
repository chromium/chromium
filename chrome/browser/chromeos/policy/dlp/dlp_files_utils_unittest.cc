// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/mock_dlp_rules_manager.h"
#include "chrome/browser/enterprise/data_controls/component.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#endif

using testing::_;

namespace policy {

namespace {

constexpr char kExampleSourcePattern1[] = "example1.com";
constexpr char kExampleSourcePattern2[] = "example2.com";

}  // namespace

class DlpFilesUtilsTest
    : public testing::Test,
      public ::testing::WithParamInterface<
          std::tuple<data_controls::Component, ::dlp::DlpComponent>> {
 public:
  DlpFilesUtilsTest(const DlpFilesUtilsTest&) = delete;
  DlpFilesUtilsTest& operator=(const DlpFilesUtilsTest&) = delete;

 protected:
  DlpFilesUtilsTest() = default;
  ~DlpFilesUtilsTest() = default;

  void SetUp() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    scoped_profile_ = std::make_unique<TestingProfile>();
    profile_ = scoped_profile_.get();
    AccountId account_id =
        AccountId::FromUserEmailGaiaId("test@example.com", "12345");
    profile_->SetIsNewProfile(true);
    user_manager::User* user =
        user_manager_->AddUserWithAffiliationAndTypeAndProfile(
            account_id, /*is_affiliated=*/false,
            user_manager::USER_TYPE_REGULAR, profile_);
    user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                /*browser_restart=*/false,
                                /*is_child=*/false);
    user_manager_->SimulateUserProfileLoad(account_id);
#else
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("user", true);
#endif
    policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
        profile_, base::BindRepeating(&DlpFilesUtilsTest::SetDlpRulesManager,
                                      base::Unretained(this)));
    ASSERT_TRUE(policy::DlpRulesManagerFactory::GetForPrimaryProfile());
    ASSERT_TRUE(rules_manager_);
  }

  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context) {
    auto dlp_rules_manager = std::make_unique<MockDlpRulesManager>();
    rules_manager_ = dlp_rules_manager.get();
    return dlp_rules_manager;
  }

  content::BrowserTaskEnvironment task_environment_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<TestingProfile> scoped_profile_;
  raw_ptr<ash::FakeChromeUserManager, ExperimentalAsh> user_manager_{
      new ash::FakeChromeUserManager()};
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_{
      std::make_unique<user_manager::ScopedUserManager>(
          base::WrapUnique(user_manager_.get()))};
#else
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
#endif
  raw_ptr<TestingProfile> profile_;

  raw_ptr<MockDlpRulesManager, ExperimentalAsh> rules_manager_ = nullptr;
};

INSTANTIATE_TEST_SUITE_P(
    DlpFiles,
    DlpFilesUtilsTest,
    ::testing::Values(
        std::make_tuple(data_controls::Component::kUnknownComponent,
                        ::dlp::DlpComponent::UNKNOWN_COMPONENT),
        std::make_tuple(data_controls::Component::kArc,
                        ::dlp::DlpComponent::ARC),
        std::make_tuple(data_controls::Component::kCrostini,
                        ::dlp::DlpComponent::CROSTINI),
        std::make_tuple(data_controls::Component::kPluginVm,
                        ::dlp::DlpComponent::PLUGIN_VM),
        std::make_tuple(data_controls::Component::kUsb,
                        ::dlp::DlpComponent::USB),
        std::make_tuple(data_controls::Component::kDrive,
                        ::dlp::DlpComponent::GOOGLE_DRIVE),
        std::make_tuple(data_controls::Component::kOneDrive,
                        ::dlp::DlpComponent::MICROSOFT_ONEDRIVE)));

TEST_P(DlpFilesUtilsTest, TestConvert) {
  auto [component, proto] = GetParam();
  EXPECT_EQ(proto, dlp::MapPolicyComponentToProto(component));
}

TEST_F(DlpFilesUtilsTest, IsFilesTransferBlocked_NoneBlocked) {
  const std::vector<std::string> sources = {
      kExampleSourcePattern1, kExampleSourcePattern2, std::string()};

  EXPECT_CALL(
      *rules_manager_,
      IsRestrictedComponent(_, data_controls::Component::kOneDrive, _, _, _))
      .WillOnce(testing::Return(DlpRulesManager::Level::kReport))
      .WillOnce(testing::Return(DlpRulesManager::Level::kWarn));

  EXPECT_FALSE(dlp::IsFilesTransferBlocked(
      sources, data_controls::Component::kOneDrive));
}

TEST_F(DlpFilesUtilsTest, IsFilesTransferBlocked_SomeBlocked) {
  const std::vector<std::string> sources = {
      kExampleSourcePattern1, kExampleSourcePattern2, std::string()};

  EXPECT_CALL(
      *rules_manager_,
      IsRestrictedComponent(_, data_controls::Component::kOneDrive, _, _, _))
      .WillOnce(testing::Return(DlpRulesManager::Level::kReport))
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock));

  EXPECT_TRUE(dlp::IsFilesTransferBlocked(sources,
                                          data_controls::Component::kOneDrive));
}

}  // namespace policy
