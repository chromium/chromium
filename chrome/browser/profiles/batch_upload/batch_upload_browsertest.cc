// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_delegate.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service_factory.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service_test_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/profiles/batch_upload_ui_delegate.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/local_data_description.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

AvatarToolbarButton* GetAvatarToolbarButton(Browser* browser) {
  return BrowserView::GetBrowserViewForBrowser(browser)
      ->toolbar_button_provider()
      ->GetAvatarToolbarButton();
}

}  // namespace

class BatchUploadBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    SetupBatchUploadTestingFactory(Profile::FromBrowserContext(context));
  }

  void SetUpOnMainThread() override {
    BatchUploadService* batch_upload_service =
        BatchUploadServiceFactory::GetForProfile(browser()->profile());
    ASSERT_TRUE(batch_upload_service);
    batch_upload_ = batch_upload_service;
  }

  void TearDownOnMainThread() override { batch_upload_ = nullptr; }

  BatchUploadServiceTestHelper& test_helper() { return test_helper_; }
  BatchUploadService* batch_upload() { return batch_upload_; }

  // Opens the batch upload dialog using the service from the profile in
  // `browser`. Waits for the batch upload url to load if opening the view was
  // successful and `wait_for_url_load`.
  bool OpenBatchUpload(Browser* browser, bool wait_for_url_load = true) {
    content::TestNavigationObserver observer{
        GURL(chrome::kChromeUIBatchUploadURL)};
    observer.StartWatchingNewWebContents();

    base::RunLoop run_loop;
    batch_upload()->OpenBatchUpload(
        browser, BatchUploadService::EntryPoint::kPasswordManagerSettings,
        base::BindOnce(&BatchUploadBrowserTest::OnBatchUploadShownResult,
                       base::Unretained(this), run_loop.QuitClosure()));

    run_loop.Run();
    if (wait_for_url_load && dialog_shown_) {
      observer.Wait();
    }

    return dialog_shown_;
  }

  void SigninWithFullInfo(
      signin::ConsentLevel consent_level = signin::ConsentLevel::kSignin) {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(browser()->profile());
    AccountInfo account_info = signin::MakePrimaryAccountAvailable(
        identity_manager, "test@gmail.com", consent_level);
    ASSERT_FALSE(account_info.IsEmpty());

    account_info = AccountInfo::Builder(account_info)
                       .SetFullName("Joe Testing")
                       .SetGivenName("Joe")
                       .SetHostedDomain(std::string())
                       .SetAvatarUrl("SOME_FAKE_URL")
                       .SetLocale("en")
                       .Build();
    ASSERT_TRUE(account_info.IsValid());
    signin::UpdateAccountInfoForAccount(identity_manager, account_info);
  }

 private:
  void OnBatchUploadShownResult(base::OnceClosure closure, bool shown) {
    dialog_shown_ = shown;
    std::move(closure).Run();
  }

  // May be overridden to in child test suite.
  virtual void SetupBatchUploadTestingFactory(Profile* profile) {
    test_helper_.SetupBatchUploadTestingFactoryInProfile(profile);
  }

  bool dialog_shown_ = false;

  BatchUploadServiceTestHelper test_helper_;
  raw_ptr<BatchUploadService> batch_upload_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(BatchUploadBrowserTest, OpenBatchUpload) {
  SigninWithFullInfo();
  test_helper().SetReturnDescriptions(syncer::DataType::PASSWORDS, 1);

  EXPECT_TRUE(OpenBatchUpload(browser()));
  EXPECT_TRUE(batch_upload()->IsDialogOpened());
}

IN_PROC_BROWSER_TEST_F(
    BatchUploadBrowserTest,
    ClosingBrowserWithBatchUploadShouldStillAlowYouToOpenANewOne) {
  SigninWithFullInfo();
  test_helper().SetReturnDescriptions(syncer::DataType::PASSWORDS, 1);

  Profile* profile = browser()->profile();
  Browser* browser_2 = CreateBrowser(profile);

  // Second browser opens dialog.
  EXPECT_TRUE(OpenBatchUpload(browser_2));
  EXPECT_TRUE(batch_upload()->IsDialogOpened());

  // Trying to open a dialog while it is still opened on another browser fails.
  // Only one batch upload dialog should be shown at a time per profile.
  EXPECT_FALSE(OpenBatchUpload(browser()));

  // Closing the browser that is displaying the dialog.
  CloseBrowserSynchronously(browser_2);

  EXPECT_FALSE(batch_upload()->IsDialogOpened());

  // We can now display the dialog on the other browser.
  EXPECT_TRUE(OpenBatchUpload(browser()));
  EXPECT_TRUE(batch_upload()->IsDialogOpened());
}

IN_PROC_BROWSER_TEST_F(BatchUploadBrowserTest, OpenedDialogThenSigninPending) {
  SigninWithFullInfo();
  test_helper().SetReturnDescriptions(syncer::DataType::PASSWORDS, 1);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  CoreAccountInfo primary_account =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  ASSERT_FALSE(primary_account.IsEmpty());

  EXPECT_TRUE(OpenBatchUpload(browser()));
  EXPECT_TRUE(batch_upload()->IsDialogOpened());

  // Trigger Signin Pending.
  signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager);
  EXPECT_FALSE(batch_upload()->IsDialogOpened());

  // Opening the dialog again should fail as we are still in signin pending.
  EXPECT_FALSE(OpenBatchUpload(browser()));
  EXPECT_FALSE(batch_upload()->IsDialogOpened());

  // Resolve the signin pending state.
  signin::SetRefreshTokenForPrimaryAccount(identity_manager);
  // Opening the dialog should now be possible again.
  EXPECT_TRUE(OpenBatchUpload(browser()));
}

IN_PROC_BROWSER_TEST_F(BatchUploadBrowserTest, OpenedDialogThenSignout) {
  SigninWithFullInfo();
  test_helper().SetReturnDescriptions(syncer::DataType::PASSWORDS, 1);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  CoreAccountInfo primary_account =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  ASSERT_FALSE(primary_account.IsEmpty());

  EXPECT_TRUE(OpenBatchUpload(browser()));
  EXPECT_TRUE(batch_upload()->IsDialogOpened());

  // Sign out.
  signin::ClearPrimaryAccount(identity_manager);
  EXPECT_FALSE(batch_upload()->IsDialogOpened());

  // Opening the dialog again should fail as we are still signed out.
  EXPECT_FALSE(OpenBatchUpload(browser()));
  EXPECT_FALSE(batch_upload()->IsDialogOpened());

  // Sign in again.
  SigninWithFullInfo();
  // Opening the dialog should now be possible again.
  EXPECT_TRUE(OpenBatchUpload(browser()));
}

// Used to control the creation of the dialog (not actually creating it), and
// the expected output without having to deal with the real dialog.
class BatchUploadDelegateFake : public BatchUploadDelegate {
 public:
  // No data move requested.
  void SimulateCancel() {
    CHECK(complete_callback_);

    local_data_description_list_.clear();
    std::move(complete_callback_).Run({});
  }

  // All data move requested.
  void SimulateSaveWithAllSelected() {
    CHECK(complete_callback_);

    std::map<syncer::DataType, std::vector<syncer::LocalDataItemModel::DataId>>
        result;
    for (const syncer::LocalDataDescription& description :
         local_data_description_list_) {
      std::vector<syncer::LocalDataItemModel::DataId> data_id_list;
      CHECK(!description.local_data_models.empty());
      std::ranges::transform(
          description.local_data_models, std::back_inserter(data_id_list),
          [](const syncer::LocalDataItemModel& item) { return item.id; });
      result.insert_or_assign(description.type, data_id_list);
    }

    local_data_description_list_.clear();
    std::move(complete_callback_).Run(result);
  }

 private:
  // BatchUploadDelegate:
  void ShowBatchUploadDialog(
      Browser* browser,
      std::vector<syncer::LocalDataDescription> local_data_description_list,
      BatchUploadService::EntryPoint entry_point,
      BatchUploadSelectedDataTypeItemsCallback complete_callback) override {
    local_data_description_list_ = std::move(local_data_description_list);
    complete_callback_ = std::move(complete_callback);
  }

  std::vector<syncer::LocalDataDescription> local_data_description_list_;
  BatchUploadSelectedDataTypeItemsCallback complete_callback_;
};

class BatchUploadWithFakeDelegateBrowserTest : public BatchUploadBrowserTest {
 public:
  BatchUploadDelegateFake* GetFakeDelegate() { return fake_delegate_ptr_; }

  // The fake delegate will never show the actual content, so we should not wait
  // for the url to load.
  bool OpenBatchUploadWithFakeDelegate(Browser* browser) {
    return OpenBatchUpload(browser,
                           /*wait_for_url_load=*/false);
  }

  void TearDownOnMainThread() override {
    BatchUploadBrowserTest::TearDownOnMainThread();
    fake_delegate_ptr_ = nullptr;
  }

 private:
  // Override the creation of the BatchUploadService to use the fake delegate.
  void SetupBatchUploadTestingFactory(Profile* profile) override {
    auto fake_delegate = std::make_unique<BatchUploadDelegateFake>();
    fake_delegate_ptr_ = fake_delegate.get();
    test_helper().SetupBatchUploadTestingFactoryInProfile(
        profile, IdentityManagerFactory::GetForProfile(profile),
        std::move(fake_delegate));
  }

  raw_ptr<BatchUploadDelegateFake> fake_delegate_ptr_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(BatchUploadWithFakeDelegateBrowserTest,
                       CloseDialogWithCancelButton) {
  SigninWithFullInfo();
  test_helper().SetReturnDescriptions(syncer::DataType::PASSWORDS, 1);
  test_helper().SetReturnDescriptions(syncer::DataType::CONTACT_INFO, 1);

  AvatarToolbarButton* avatar_button = GetAvatarToolbarButton(browser());
  const std::u16string original_avatar_button_text(avatar_button->GetText());
  ASSERT_NE(original_avatar_button_text,
            l10n_util::GetStringUTF16(
                IDS_BATCH_UPLOAD_AVATAR_BUTTON_SAVING_TO_ACCOUNT));

  ASSERT_FALSE(batch_upload()->IsDialogOpened());

  ASSERT_TRUE(OpenBatchUploadWithFakeDelegate(browser()));
  ASSERT_TRUE(batch_upload()->IsDialogOpened());

  BatchUploadDelegateFake* delegate = GetFakeDelegate();
  delegate->SimulateCancel();

  EXPECT_FALSE(batch_upload()->IsDialogOpened());
  EXPECT_EQ(avatar_button->GetText(), original_avatar_button_text);
}

IN_PROC_BROWSER_TEST_F(BatchUploadWithFakeDelegateBrowserTest,
                       CloseDialogWithSaveButtonAllSelected) {
  SigninWithFullInfo();
  test_helper().SetReturnDescriptions(syncer::DataType::PASSWORDS, 1);
  test_helper().SetReturnDescriptions(syncer::DataType::CONTACT_INFO, 1);

  AvatarToolbarButton* avatar_button = GetAvatarToolbarButton(browser());
  ASSERT_NE(avatar_button->GetText(),
            l10n_util::GetStringUTF16(
                IDS_BATCH_UPLOAD_AVATAR_BUTTON_SAVING_TO_ACCOUNT));

  ASSERT_FALSE(batch_upload()->IsDialogOpened());

  ASSERT_TRUE(OpenBatchUploadWithFakeDelegate(browser()));
  ASSERT_TRUE(batch_upload()->IsDialogOpened());

  BatchUploadDelegateFake* delegate = GetFakeDelegate();
  delegate->SimulateSaveWithAllSelected();

  EXPECT_FALSE(batch_upload()->IsDialogOpened());
  EXPECT_EQ(avatar_button->GetText(),
            l10n_util::GetStringUTF16(
                IDS_BATCH_UPLOAD_AVATAR_BUTTON_SAVING_TO_ACCOUNT));
}
