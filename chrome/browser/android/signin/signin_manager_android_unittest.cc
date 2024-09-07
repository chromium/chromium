// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "signin_manager_android.h"

#include <memory>
#include <set>

#include "base/android/build_info.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate_factory.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_core_service_impl.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "components/offline_pages/core/stub_offline_page_model.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/split_stores_and_local_upm.h"
#include "content/public/browser/background_tracing_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

namespace {

class MockOfflinePageModel : public offline_pages::StubOfflinePageModel {
 public:
  void DeleteCachedPagesByURLPredicate(
      const offline_pages::UrlPredicate& predicate,
      offline_pages::DeletePageCallback callback) override {
    std::move(callback).Run(DeletePageResult::SUCCESS);
  }
};

std::unique_ptr<KeyedService> BuildOfflinePageModel(SimpleFactoryKey* key) {
  return std::make_unique<MockOfflinePageModel>();
}

}  // namespace

class SigninManagerAndroidTest : public ::testing::Test {
 public:
  SigninManagerAndroidTest() {
    // Override the GMS version to be big enough for local UPM support, so
    // DoNotWipePasswordsIfLocalUpmOn still passes on bots with outdated GMS.
    base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
        base::NumberToString(password_manager::GetLocalUpmMinGmsVersion()));
  }

  SigninManagerAndroidTest(const SigninManagerAndroidTest&) = delete;
  SigninManagerAndroidTest& operator=(const SigninManagerAndroidTest&) = delete;

  ~SigninManagerAndroidTest() override = default;

  void SetUp() override {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactories(
        {TestingProfile::TestingFactory{
             BookmarkModelFactory::GetInstance(),
             BookmarkModelFactory::GetDefaultFactory()},
         TestingProfile::TestingFactory{
             ProfilePasswordStoreFactory::GetInstance(),
             base::BindRepeating(&password_manager::BuildPasswordStore<
                                 content::BrowserContext,
                                 password_manager::TestPasswordStore>)},
         TestingProfile::TestingFactory{
             AccountPasswordStoreFactory::GetInstance(),
             base::BindRepeating(&password_manager::BuildPasswordStoreWithArgs<
                                     content::BrowserContext,
                                     password_manager::TestPasswordStore,
                                     password_manager::IsAccountStore>,
                                 password_manager::IsAccountStore(true))}});
    profile_ = profile_builder.Build();

    background_tracing_manager_ =
        content::BackgroundTracingManager::CreateInstance();

    // Creating a BookmarkModel also a creates a StubOfflinePageModel.
    // We need to replace this with a mock that responds to deletions.
    offline_pages::OfflinePageModelFactory::GetInstance()->SetTestingFactory(
        profile_->GetProfileKey(), base::BindRepeating(&BuildOfflinePageModel));

    // TODO(crbug.com/41335519): Remove requirement for this delegate in
    // unit_tests.
    DownloadCoreServiceFactory::GetForBrowserContext(profile_.get())
        ->SetDownloadManagerDelegateForTesting(
            std::make_unique<ChromeDownloadManagerDelegate>(profile_.get()));
  }

  void TearDown() override { background_tracing_manager_.reset(); }

  TestingProfile* profile() { return profile_.get(); }

  password_manager::TestPasswordStore* profile_password_store() {
    return static_cast<password_manager::TestPasswordStore*>(
        ProfilePasswordStoreFactory::GetForProfile(
            profile_.get(), ServiceAccessType::EXPLICIT_ACCESS)
            .get());
  }

  password_manager::TestPasswordStore* account_password_store() {
    return static_cast<password_manager::TestPasswordStore*>(
        AccountPasswordStoreFactory::GetForProfile(
            profile_.get(), ServiceAccessType::EXPLICIT_ACCESS)
            .get());
  }

  // Adds two testing bookmarks to |profile_|.
  bookmarks::BookmarkModel* AddTestBookmarks() {
    bookmarks::BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForBrowserContext(profile());
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);

    bookmark_model->AddURL(bookmark_model->bookmark_bar_node(), 0, u"Example 1",
                           GURL("https://example.org/1"));
    bookmark_model->AddURL(bookmark_model->bookmark_bar_node(), 1, u"Example 2",
                           GURL("https://example.com/2"));

    return bookmark_model;
  }

  // Calls SigninManager::WipeData(|all_data|) and waits for its completion.
  void WipeData(bool all_data) {
    std::unique_ptr<base::RunLoop> run_loop(new base::RunLoop());
    SigninManagerAndroid::WipeData(profile(), all_data,
                                   run_loop->QuitClosure());
    run_loop->Run();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState local_state_{TestingBrowserProcess::GetGlobal()};
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::BackgroundTracingManager>
      background_tracing_manager_;
};

// Tests that wiping all data also deletes bookmarks.
TEST_F(SigninManagerAndroidTest, DeleteBookmarksWhenWipingAllData) {
  bookmarks::BookmarkModel* bookmark_model = AddTestBookmarks();
  ASSERT_GE(bookmark_model->bookmark_bar_node()->children().size(), 0u);
  WipeData(true);
  EXPECT_EQ(0u, bookmark_model->bookmark_bar_node()->children().size());
}

// Tests that wiping Google service worker caches does not delete bookmarks.
TEST_F(SigninManagerAndroidTest, DontDeleteBookmarksWhenDeletingSWCaches) {
  bookmarks::BookmarkModel* bookmark_model = AddTestBookmarks();
  size_t num_bookmarks = bookmark_model->bookmark_bar_node()->children().size();
  ASSERT_GE(num_bookmarks, 0u);
  WipeData(false);
  EXPECT_EQ(num_bookmarks,
            bookmark_model->bookmark_bar_node()->children().size());
}

TEST_F(SigninManagerAndroidTest, DoNotWipePasswordsIfLocalUpmOn) {
  password_manager::PasswordForm profile_store_form;
  profile_store_form.username_value = u"username";
  profile_store_form.password_value = u"password";
  profile_store_form.signon_realm = "https://local.com";
  password_manager::PasswordForm account_store_form = profile_store_form;
  account_store_form.signon_realm = "htts://account.com";
  profile_password_store()->AddLogin(profile_store_form);
  ASSERT_TRUE(account_password_store());
  account_password_store()->AddLogin(account_store_form);

  WipeData(/*all_data=*/true);

  EXPECT_THAT(
      profile_password_store()->stored_passwords(),
      UnorderedElementsAre(Pair(profile_store_form.signon_realm, SizeIs(1))));
  EXPECT_THAT(
      account_password_store()->stored_passwords(),
      UnorderedElementsAre(Pair(account_store_form.signon_realm, SizeIs(1))));
}

class SigninManagerAndroidWithoutLocalUpmTest
    : public SigninManagerAndroidTest {
 public:
  SigninManagerAndroidWithoutLocalUpmTest() {
    // Fake a user with outdated GmsCore.
    base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test("0");
  }
};

TEST_F(SigninManagerAndroidWithoutLocalUpmTest, WipePasswordsIfLocalUpmOff) {
  password_manager::PasswordForm form;
  form.username_value = u"username";
  form.password_value = u"password";
  form.signon_realm = "https://g.com";
  profile_password_store()->AddLogin(form);
  ASSERT_FALSE(account_password_store());

  WipeData(/*all_data=*/true);

  EXPECT_THAT(profile_password_store()->stored_passwords(), IsEmpty());
}
