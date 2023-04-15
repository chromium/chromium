// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/chromeos/parent_access_extension_approvals_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_dialog.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_ui.mojom.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"

namespace {
constexpr char kTestProfileName[] = "child@gmail.com";
constexpr char16_t kTestProfileName16[] = u"child@gmail.com";
constexpr char kTestGaiaId[] = "1234567890";
constexpr char16_t kTestGivenName16[] = u"Tester";
constexpr char kTestExtensionName[] = "extension";
constexpr char16_t kTestExtensionName16[] = u"extension";
}  // namespace

class FakeParentAccessDialogProvider : public ash::ParentAccessDialogProvider {
 public:
  ParentAccessDialogProvider::ShowError Show(
      parent_access_ui::mojom::ParentAccessParamsPtr params,
      ash::ParentAccessDialog::Callback callback) override {
    callback_ = std::move(callback);
    last_params_received_ = std::move(params);
    return next_show_error_;
  }

  void SetNextShowError(ash::ParentAccessDialogProvider::ShowError error) {
    next_show_error_ = error;
  }

  parent_access_ui::mojom::ParentAccessParamsPtr GetLastParamsReceived() {
    return std::move(last_params_received_);
  }

  void TriggerCallbackWithResult(
      std::unique_ptr<ash::ParentAccessDialog::Result> result) {
    std::move(callback_).Run(std::move(result));
  }

 private:
  ash::ParentAccessDialog::Callback callback_;
  parent_access_ui::mojom::ParentAccessParamsPtr last_params_received_;
  ash::ParentAccessDialogProvider::ShowError next_show_error_;
};

class ParentAccessExtensionApprovalsManagerTest : public ::testing::Test {
 public:
  ParentAccessExtensionApprovalsManagerTest() = default;
  ~ParentAccessExtensionApprovalsManagerTest() override = default;

  void SetUp() override {
    CreateSupervisedUser();
    approvals_manager_ =
        std::make_unique<extensions::ParentAccessExtensionApprovalsManager>();
    dialog_provider_ = static_cast<FakeParentAccessDialogProvider*>(
        approvals_manager_->SetDialogProviderForTest(
            std::make_unique<FakeParentAccessDialogProvider>()));
  }

 protected:
  ash::FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  void CreateSupervisedUser() {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile(
        kTestProfileName, {}, kTestProfileName16, 0,
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories(),
        /*is_supervised_profile=*/true);
    supervised_user_test_util::AddCustodians(profile_);
    AddUserToUserManager();
  }

  void AddUserToUserManager() {
    AccountId account_id =
        AccountId::FromUserEmailGaiaId(kTestProfileName, kTestGaiaId);
    GetFakeUserManager()->AddUser(account_id);
    GetFakeUserManager()->LoginUser(account_id);
    GetFakeUserManager()->SwitchActiveUser(account_id);
    GetFakeUserManager()->UpdateUserAccountData(
        account_id, user_manager::UserManager::UserAccountData(
                        /*display_name=*/std::u16string(),
                        /*given_name=*/kTestGivenName16,
                        /*locale=*/std::string()));
  }

  void SetSupervisedUserMayRequestPermissionsPref(bool enable) {
    supervised_user_test_util::
        SetSupervisedUserExtensionsMayRequestPermissionsPref(profile_, enable);
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<extensions::ParentAccessExtensionApprovalsManager>
      approvals_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<FakeParentAccessDialogProvider> dialog_provider_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ParentAccessExtensionApprovalsManagerTest, GetExtensionApprovalParams) {
  SetSupervisedUserMayRequestPermissionsPref(true);
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder(kTestExtensionName).Build();
  approvals_manager_->ShowParentAccessDialog(
      *extension, profile_, extensions::util::GetDefaultExtensionIcon(),
      base::DoNothing());
  parent_access_ui::mojom::ParentAccessParamsPtr params =
      dialog_provider_->GetLastParamsReceived();

  // Check for the correct flow type.
  EXPECT_EQ(
      params->flow_type,
      parent_access_ui::mojom::ParentAccessParams::FlowType::kExtensionAccess);
  // Check that adding extensions is enabled.
  EXPECT_EQ(params->is_disabled, false);
  // Check for the correct child display name.
  EXPECT_EQ(params->flow_type_params->get_extension_approvals_params()
                ->child_display_name,
            kTestGivenName16);
  // Check for the correct extension name.
  EXPECT_EQ(params->flow_type_params->get_extension_approvals_params()
                ->extension_name,
            kTestExtensionName16);
  // Encode the test bitmap and check that it is passed.
  std::vector<uint8_t> icon_bitmap;
  gfx::PNGCodec::FastEncodeBGRASkBitmap(
      *extensions::util::GetDefaultExtensionIcon().bitmap(), false,
      &icon_bitmap);
  EXPECT_EQ(params->flow_type_params->get_extension_approvals_params()
                ->icon_png_bytes,
            icon_bitmap);
}

TEST_F(ParentAccessExtensionApprovalsManagerTest,
       GetExtensionApprovalParamsForExtensionDisabled) {
  SetSupervisedUserMayRequestPermissionsPref(false);
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder(kTestExtensionName).Build();
  approvals_manager_->ShowParentAccessDialog(
      *extension, profile_, extensions::util::GetDefaultExtensionIcon(),
      base::DoNothing());
  parent_access_ui::mojom::ParentAccessParamsPtr params =
      dialog_provider_->GetLastParamsReceived();

  // Check for the correct flow type.
  EXPECT_EQ(
      params->flow_type,
      parent_access_ui::mojom::ParentAccessParams::FlowType::kExtensionAccess);
  // Check that adding extensions is enabled.
  EXPECT_EQ(params->is_disabled, true);
}

TEST_F(ParentAccessExtensionApprovalsManagerTest,
       GetParentAccessApproval_Approved) {
  SetSupervisedUserMayRequestPermissionsPref(true);
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder(kTestExtensionName).Build();
  base::RunLoop run_loop;
  approvals_manager_->ShowParentAccessDialog(
      *extension, profile_, extensions::util::GetDefaultExtensionIcon(),
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             extensions::SupervisedUserExtensionsDelegate::
                 ExtensionApprovalResult result) {
            EXPECT_EQ(result, extensions::SupervisedUserExtensionsDelegate::
                                  ExtensionApprovalResult::kApproved);
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));

  auto dialog_result = std::make_unique<ash::ParentAccessDialog::Result>();
  dialog_result->status = ash::ParentAccessDialog::Result::Status::kApproved;
  dialog_provider_->TriggerCallbackWithResult(std::move(dialog_result));
  run_loop.Run();
}

TEST_F(ParentAccessExtensionApprovalsManagerTest,
       GetParentAccessApproval_Declined) {
  SetSupervisedUserMayRequestPermissionsPref(true);
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder(kTestExtensionName).Build();
  base::RunLoop run_loop;
  approvals_manager_->ShowParentAccessDialog(
      *extension, profile_, extensions::util::GetDefaultExtensionIcon(),
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             extensions::SupervisedUserExtensionsDelegate::
                 ExtensionApprovalResult result) {
            EXPECT_EQ(result, extensions::SupervisedUserExtensionsDelegate::
                                  ExtensionApprovalResult::kCanceled);
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));

  auto dialog_result = std::make_unique<ash::ParentAccessDialog::Result>();
  dialog_result->status = ash::ParentAccessDialog::Result::Status::kDeclined;
  dialog_provider_->TriggerCallbackWithResult(std::move(dialog_result));
  run_loop.Run();
}

TEST_F(ParentAccessExtensionApprovalsManagerTest,
       GetParentAccessApproval_Canceled) {
  SetSupervisedUserMayRequestPermissionsPref(true);
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder(kTestExtensionName).Build();
  base::RunLoop run_loop;
  approvals_manager_->ShowParentAccessDialog(
      *extension, profile_, extensions::util::GetDefaultExtensionIcon(),
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             extensions::SupervisedUserExtensionsDelegate::
                 ExtensionApprovalResult result) {
            EXPECT_EQ(result, extensions::SupervisedUserExtensionsDelegate::
                                  ExtensionApprovalResult::kCanceled);
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));

  auto dialog_result = std::make_unique<ash::ParentAccessDialog::Result>();
  dialog_result->status = ash::ParentAccessDialog::Result::Status::kCanceled;
  dialog_provider_->TriggerCallbackWithResult(std::move(dialog_result));
  run_loop.Run();
}

TEST_F(ParentAccessExtensionApprovalsManagerTest,
       GetParentAccessApproval_Disabled) {
  SetSupervisedUserMayRequestPermissionsPref(true);
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder(kTestExtensionName).Build();
  base::RunLoop run_loop;
  approvals_manager_->ShowParentAccessDialog(
      *extension, profile_, extensions::util::GetDefaultExtensionIcon(),
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             extensions::SupervisedUserExtensionsDelegate::
                 ExtensionApprovalResult result) {
            EXPECT_EQ(result, extensions::SupervisedUserExtensionsDelegate::
                                  ExtensionApprovalResult::kBlocked);
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));

  auto dialog_result = std::make_unique<ash::ParentAccessDialog::Result>();
  dialog_result->status = ash::ParentAccessDialog::Result::Status::kDisabled;
  dialog_provider_->TriggerCallbackWithResult(std::move(dialog_result));
  run_loop.Run();
}

TEST_F(ParentAccessExtensionApprovalsManagerTest,
       GetParentAccessApproval_Error) {
  SetSupervisedUserMayRequestPermissionsPref(true);
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder(kTestExtensionName).Build();
  base::RunLoop run_loop;
  approvals_manager_->ShowParentAccessDialog(
      *extension, profile_, extensions::util::GetDefaultExtensionIcon(),
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             extensions::SupervisedUserExtensionsDelegate::
                 ExtensionApprovalResult result) {
            EXPECT_EQ(result, extensions::SupervisedUserExtensionsDelegate::
                                  ExtensionApprovalResult::kFailed);
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));

  auto dialog_result = std::make_unique<ash::ParentAccessDialog::Result>();
  dialog_result->status = ash::ParentAccessDialog::Result::Status::kError;
  dialog_provider_->TriggerCallbackWithResult(std::move(dialog_result));
  run_loop.Run();
}

TEST_F(ParentAccessExtensionApprovalsManagerTest,
       GetParentAccessApproval_ShowError) {
  dialog_provider_->SetNextShowError(
      ash::ParentAccessDialogProvider::ShowError::kDialogAlreadyVisible);
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder(kTestExtensionName).Build();
  base::RunLoop run_loop;
  approvals_manager_->ShowParentAccessDialog(
      *extension, profile_, extensions::util::GetDefaultExtensionIcon(),
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             extensions::SupervisedUserExtensionsDelegate::
                 ExtensionApprovalResult result) {
            EXPECT_EQ(result, extensions::SupervisedUserExtensionsDelegate::
                                  ExtensionApprovalResult::kFailed);
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}
