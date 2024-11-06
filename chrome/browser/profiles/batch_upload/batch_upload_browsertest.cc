// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_delegate.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/profiles/batch_upload_ui_delegate.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/local_data_description.h"
#include "components/sync/test/mock_sync_service.h"
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

syncer::LocalDataItemModel MakeDummyLocalDataModel(size_t id) {
  syncer::LocalDataItemModel model;
  std::string id_string = base::ToString(id);
  model.id = id_string;
  model.title = "title_" + id_string;
  model.subtitle = "subtitle" + id_string;
  return model;
}

}  // namespace

class BatchUploadWithFeatureOffBrowserTest : public InProcessBrowserTest {
 public:
  BatchUploadWithFeatureOffBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(switches::kBatchUploadDesktop);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(BatchUploadWithFeatureOffBrowserTest, BatchUploadNull) {
  BatchUploadService* batch_upload =
      BatchUploadServiceFactory::GetForProfile(browser()->profile());
  EXPECT_FALSE(batch_upload);
}

class BatchUploadBrowserTest : public InProcessBrowserTest {
 public:
  BatchUploadBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        // `switches::kExplicitBrowserSigninUIOnDesktop` is needed for the
        // SigninPending state.
        /*enabled_features=*/{switches::kExplicitBrowserSigninUIOnDesktop,
                              switches::kBatchUploadDesktop},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    BatchUploadServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        browser()->profile(),
        base::BindOnce(&BatchUploadBrowserTest::CreateBatchUploadService,
                       base::Unretained(this)));

    ON_CALL(*GetSyncServiceMock(), GetLocalDataDescriptions)
        .WillByDefault(
            [this](
                syncer::DataTypeSet types,
                base::OnceCallback<void(
                    std::map<syncer::DataType, syncer::LocalDataDescription>)>
                    callback) {
              std::move(callback).Run(returned_descriptions_);
            });
  }

  // Opens the batch upload dialog using `batch_upload_service` in `browser.
  // Waits for the batch upload url to load if opening the view was successful
  // and `wait_for_url_load`.
  bool OpenBatchUpload(BatchUploadService* batch_upload_service,
                       Browser* browser,
                       bool wait_for_url_load = true) {
    content::TestNavigationObserver observer{
        GURL(chrome::kChromeUIBatchUploadURL)};
    observer.StartWatchingNewWebContents();

    base::RunLoop run_loop;
    batch_upload_service->OpenBatchUpload(
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

    account_info.full_name = "Joe Testing";
    account_info.given_name = "Joe";
    account_info.picture_url = "SOME_FAKE_URL";
    account_info.hosted_domain = kNoHostedDomainFound;
    account_info.locale = "en";
    ASSERT_TRUE(account_info.IsValid());
    signin::UpdateAccountInfoForAccount(identity_manager, account_info);
  }

  syncer::MockSyncService* GetSyncServiceMock() { return &mock_sync_service_; }

  // Overrides `SyncService::GetLocalDataDescriptions()` by returned type in the
  // map by constructing dummy models.
  void SetReturnDescriptions(syncer::DataType type, size_t item_count) {
    syncer::LocalDataDescription& description = returned_descriptions_[type];
    description.type = type;
    description.local_data_models.clear();
    for (size_t i = 0; i < item_count; ++i) {
      description.local_data_models.push_back(MakeDummyLocalDataModel(i));
    }
  }

  // Overrides `SyncService::GetLocalDataDescriptions()` by emptying the
  // returned value.
  void ClearReturnDescriptions() { returned_descriptions_.clear(); }

 private:
  void OnBatchUploadShownResult(base::OnceClosure closure, bool shown) {
    dialog_shown_ = shown;
    std::move(closure).Run();
  }

  // May be overridden to in child test suite.
  virtual std::unique_ptr<KeyedService> CreateBatchUploadService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    return std::make_unique<BatchUploadService>(
        IdentityManagerFactory::GetForProfile(profile), GetSyncServiceMock(),
        std::make_unique<BatchUploadUIDelegate>());
  }

  syncer::MockSyncService mock_sync_service_;
  std::map<syncer::DataType, syncer::LocalDataDescription>
      returned_descriptions_;

  bool dialog_shown_ = false;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(BatchUploadBrowserTest, OpenBatchUpload) {
  SigninWithFullInfo();
  SetReturnDescriptions(syncer::DataType::PASSWORDS, 1);

  BatchUploadService* batch_upload =
      BatchUploadServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(batch_upload);

  EXPECT_TRUE(OpenBatchUpload(batch_upload, browser()));
  EXPECT_TRUE(batch_upload->IsDialogOpened());
}

IN_PROC_BROWSER_TEST_F(
    BatchUploadBrowserTest,
    ClosingBrowserWithBatchUploadShouldStillAlowYouToOpenANewOne) {
  SigninWithFullInfo();
  SetReturnDescriptions(syncer::DataType::PASSWORDS, 1);

  Profile* profile = browser()->profile();
  Browser* browser_2 = CreateBrowser(profile);

  BatchUploadService* batch_upload =
      BatchUploadServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(batch_upload);

  // Second browser opens dialog.
  EXPECT_TRUE(OpenBatchUpload(batch_upload, browser_2));
  EXPECT_TRUE(batch_upload->IsDialogOpened());

  // Trying to open a dialog while it is still opened on another browser fails.
  // Only one batch upload dialog should be shown at a time per profile.
  EXPECT_FALSE(OpenBatchUpload(batch_upload, browser()));

  // Closing the browser that is displaying the dialog.
  CloseBrowserSynchronously(browser_2);

  EXPECT_FALSE(batch_upload->IsDialogOpened());

  // We can now display the dialog on the other browser.
  EXPECT_TRUE(OpenBatchUpload(batch_upload, browser()));
  EXPECT_TRUE(batch_upload->IsDialogOpened());
}

IN_PROC_BROWSER_TEST_F(BatchUploadBrowserTest, OpenedDialogThenSigninPending) {
  SigninWithFullInfo();
  SetReturnDescriptions(syncer::DataType::PASSWORDS, 1);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  CoreAccountInfo primary_account =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  ASSERT_FALSE(primary_account.IsEmpty());

  BatchUploadService* batch_upload =
      BatchUploadServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(batch_upload);
  EXPECT_TRUE(OpenBatchUpload(batch_upload, browser()));
  EXPECT_TRUE(batch_upload->IsDialogOpened());

  // Trigger Signin Pending.
  signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager);
  EXPECT_FALSE(batch_upload->IsDialogOpened());

  // Opening the dialog again should fail as we are still in signin pending.
  EXPECT_FALSE(OpenBatchUpload(batch_upload, browser()));
  EXPECT_FALSE(batch_upload->IsDialogOpened());

  // Resolve the signin pending state.
  signin::SetRefreshTokenForPrimaryAccount(identity_manager);
  // Opening the dialog should now be possible again.
  EXPECT_TRUE(OpenBatchUpload(batch_upload, browser()));
}

IN_PROC_BROWSER_TEST_F(BatchUploadBrowserTest, OpenedDialogThenSignout) {
  SigninWithFullInfo();
  SetReturnDescriptions(syncer::DataType::PASSWORDS, 1);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  CoreAccountInfo primary_account =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  ASSERT_FALSE(primary_account.IsEmpty());

  BatchUploadService* batch_upload =
      BatchUploadServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(batch_upload);
  EXPECT_TRUE(OpenBatchUpload(batch_upload, browser()));
  EXPECT_TRUE(batch_upload->IsDialogOpened());

  // Sign out.
  signin::ClearPrimaryAccount(identity_manager);
  EXPECT_FALSE(batch_upload->IsDialogOpened());

  // Opening the dialog again should fail as we are still signed out.
  EXPECT_FALSE(OpenBatchUpload(batch_upload, browser()));
  EXPECT_FALSE(batch_upload->IsDialogOpened());

  // Sign in again.
  SigninWithFullInfo();
  // Opening the dialog should now be possible again.
  EXPECT_TRUE(OpenBatchUpload(batch_upload, browser()));
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

// This fake service extension is only used to reset the test delegate. The rest
// of the service real implementation is tested.
class BatchUploadServiceFake : public BatchUploadService {
 public:
  BatchUploadServiceFake(signin::IdentityManager* identity_manager,
                         syncer::SyncService* sync_service,
                         std::unique_ptr<BatchUploadDelegate> delegate,
                         base::OnceClosure clear_test_callback)
      : BatchUploadService(identity_manager, sync_service, std::move(delegate)),
        clear_test_callback_(std::move(clear_test_callback)) {}

  // BatchUploadService:
  void Shutdown() override { std::move(clear_test_callback_).Run(); }

 private:
  base::OnceClosure clear_test_callback_;
};

class BatchUploadWithFakeDelegateBrowserTest : public BatchUploadBrowserTest {
 public:
  BatchUploadDelegateFake* GetFakeDelegate() { return delegate_; }

  // The fake delegate will never show the actual content, so we should not wait
  // for the url to load.
  bool OpenBatchUploadWithFakeDelegate(BatchUploadService* batch_upload_service,
                                       Browser* browser) {
    return OpenBatchUpload(batch_upload_service, browser,
                           /*wait_for_url_load=*/false);
  }

 private:
  // Override the creation of the BatchUploadService to use the fake delegate.
  std::unique_ptr<KeyedService> CreateBatchUploadService(
      content::BrowserContext* context) override {
    std::unique_ptr<BatchUploadDelegateFake> fake_delegate =
        std::make_unique<BatchUploadDelegateFake>();
    delegate_ = fake_delegate.get();
    return std::make_unique<BatchUploadServiceFake>(
        IdentityManagerFactory::GetForProfile(
            Profile::FromBrowserContext(context)),
        GetSyncServiceMock(), std::move(fake_delegate),
        base::BindOnce(&BatchUploadWithFakeDelegateBrowserTest::ClearDelegate,
                       base::Unretained(this)));
  }

  void ClearDelegate() { delegate_ = nullptr; }

  raw_ptr<BatchUploadDelegateFake> delegate_;
};

IN_PROC_BROWSER_TEST_F(BatchUploadWithFakeDelegateBrowserTest,
                       CloseDialogWithCancelButton) {
  SigninWithFullInfo();
  SetReturnDescriptions(syncer::DataType::PASSWORDS, 1);
  SetReturnDescriptions(syncer::DataType::CONTACT_INFO, 1);

  AvatarToolbarButton* avatar_button = GetAvatarToolbarButton(browser());
  ASSERT_EQ(avatar_button->GetText(), std::u16string());

  BatchUploadService* batch_upload =
      BatchUploadServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(batch_upload);
  ASSERT_FALSE(batch_upload->IsDialogOpened());

  ASSERT_TRUE(OpenBatchUploadWithFakeDelegate(batch_upload, browser()));
  ASSERT_TRUE(batch_upload->IsDialogOpened());

  BatchUploadDelegateFake* delegate = GetFakeDelegate();
  delegate->SimulateCancel();

  EXPECT_FALSE(batch_upload->IsDialogOpened());
  EXPECT_EQ(avatar_button->GetText(), std::u16string());
}

IN_PROC_BROWSER_TEST_F(BatchUploadWithFakeDelegateBrowserTest,
                       CloseDialogWithSaveButtonAllSelected) {
  SigninWithFullInfo();
  SetReturnDescriptions(syncer::DataType::PASSWORDS, 1);
  SetReturnDescriptions(syncer::DataType::CONTACT_INFO, 1);

  AvatarToolbarButton* avatar_button = GetAvatarToolbarButton(browser());
  ASSERT_EQ(avatar_button->GetText(), std::u16string());

  BatchUploadService* batch_upload =
      BatchUploadServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(batch_upload);
  ASSERT_FALSE(batch_upload->IsDialogOpened());

  ASSERT_TRUE(OpenBatchUploadWithFakeDelegate(batch_upload, browser()));
  ASSERT_TRUE(batch_upload->IsDialogOpened());

  BatchUploadDelegateFake* delegate = GetFakeDelegate();
  delegate->SimulateSaveWithAllSelected();

  EXPECT_FALSE(batch_upload->IsDialogOpened());
  EXPECT_EQ(avatar_button->GetText(),
            l10n_util::GetStringUTF16(
                IDS_BATCH_UPLOAD_AVATAR_BUTTON_SAVING_TO_ACCOUNT));
}
