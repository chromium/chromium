// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signin_partition_manager.h"

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chromeos {
namespace login {

namespace {
constexpr char kEmbedderUrl[] = "http://www.whatever.com/";

void StorePartitionNameAndQuitLoop(base::RunLoop* loop,
                                   std::string* out_partition_name,
                                   const std::string& partition_name) {
  *out_partition_name = partition_name;
  loop->Quit();
}

void AddEntryToHttpAuthCache(
    net::URLRequestContextGetter* url_request_context_getter) {
  net::HttpAuthCache* http_auth_cache =
      url_request_context_getter->GetURLRequestContext()
          ->http_transaction_factory()
          ->GetSession()
          ->http_auth_cache();
  http_auth_cache->Add(GURL("http://whatever.com/"), "",
                       net::HttpAuth::AUTH_SCHEME_BASIC, "",
                       net::AuthCredentials(), "");
}

void IsEntryInHttpAuthCache(
    net::URLRequestContextGetter* url_request_context_getter,
    bool* out_entry_found) {
  net::HttpAuthCache* http_auth_cache =
      url_request_context_getter->GetURLRequestContext()
          ->http_transaction_factory()
          ->GetSession()
          ->http_auth_cache();
  *out_entry_found =
      http_auth_cache->Lookup(GURL("http://whatever.com/"), "",
                              net::HttpAuth::AUTH_SCHEME_BASIC) != nullptr;
}

}  // namespace

class SigninPartitionManagerTest : public ChromeRenderViewHostTestHarness {
 protected:
  SigninPartitionManagerTest() {}
  ~SigninPartitionManagerTest() override {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    system_request_context_getter_ = new net::TestURLRequestContextGetter(
        base::CreateSingleThreadTaskRunnerWithTraits(
            {content::BrowserThread::IO}));

    signin_browser_context_ = std::make_unique<TestingProfile>();

    signin_ui_web_contents_ = content::WebContentsTester::CreateTestWebContents(
        GetSigninProfile(), content::SiteInstance::Create(GetSigninProfile()));

    GURL url(kEmbedderUrl);
    content::WebContentsTester::For(signin_ui_web_contents())
        ->NavigateAndCommit(url);

    GetSigninPartitionManager()->SetClearStoragePartitionTaskForTesting(
        base::Bind(&SigninPartitionManagerTest::ClearStoragePartitionTask,
                   base::Unretained(this)));
    GetSigninPartitionManager()
        ->SetGetSystemURLRequestContextGetterTaskForTesting(base::BindRepeating(
            &SigninPartitionManagerTest::GetSystemURLRequestContextGetter,
            base::Unretained(this)));
  }

  void TearDown() override {
    signin_ui_web_contents_.reset();

    signin_browser_context_.reset();

    // ChromeRenderViewHostTestHarness::TearDown() simulates shutdown and
    // ~URLRequestContextGetter() assumes BrowserThreads are still up so this
    // must happen first.
    system_request_context_getter_ = nullptr;

    ChromeRenderViewHostTestHarness::TearDown();
  }

  Profile* GetSigninProfile() {
    return signin_browser_context_->GetOffTheRecordProfile();
  }

  SigninPartitionManager* GetSigninPartitionManager() {
    return SigninPartitionManager::Factory::GetForBrowserContext(
        GetSigninProfile());
  }

  content::WebContents* signin_ui_web_contents() {
    return signin_ui_web_contents_.get();
  }

  void ExpectOneClearPartitionTask(
      content::StoragePartition* storage_partition) {
    EXPECT_EQ(1u, pending_clear_tasks_.size());
    if (pending_clear_tasks_.size() > 0) {
      EXPECT_EQ(storage_partition, pending_clear_tasks_[0].first);
    }
  }

  void FinishAllClearPartitionTasks() {
    for (auto& task : pending_clear_tasks_) {
      std::move(task.second).Run();
    }
    pending_clear_tasks_.clear();
  }

  std::string RunStartSigninSesssion(content::WebContents* webcontents) {
    std::string partition_name;
    base::RunLoop loop;
    GetSigninPartitionManager()->StartSigninSession(
        webcontents,
        base::BindOnce(&StorePartitionNameAndQuitLoop, &loop, &partition_name));
    loop.Run();
    return partition_name;
  }

  net::URLRequestContextGetter* GetSystemURLRequestContextGetter() {
    return system_request_context_getter_.get();
  }

 private:
  void ClearStoragePartitionTask(content::StoragePartition* partition,
                                 base::OnceClosure clear_done_closure) {
    pending_clear_tasks_.push_back({partition, std::move(clear_done_closure)});
  }

  scoped_refptr<net::TestURLRequestContextGetter>
      system_request_context_getter_;

  std::unique_ptr<TestingProfile> signin_browser_context_;

  // Web contents of the sign-in UI, embedder of the signin-frame webview.
  std::unique_ptr<content::WebContents> signin_ui_web_contents_;

  std::vector<std::pair<content::StoragePartition*, base::OnceClosure>>
      pending_clear_tasks_;

  DISALLOW_COPY_AND_ASSIGN(SigninPartitionManagerTest);
};

TEST_F(SigninPartitionManagerTest, TestSubsequentAttempts) {
  // First sign-in attempt
  std::string signin_partition_name_1 =
      RunStartSigninSesssion(signin_ui_web_contents());
  auto* signin_partition_1 =
      GetSigninPartitionManager()->GetCurrentStoragePartition();
  EXPECT_FALSE(signin_partition_name_1.empty());
  EXPECT_EQ(signin_partition_name_1,
            GetSigninPartitionManager()->GetCurrentStoragePartitionName());

  // Second sign-in attempt
  std::string signin_partition_name_2 =
      RunStartSigninSesssion(signin_ui_web_contents());
  auto* signin_partition_2 =
      GetSigninPartitionManager()->GetCurrentStoragePartition();
  EXPECT_FALSE(signin_partition_name_2.empty());
  EXPECT_EQ(signin_partition_name_2,
            GetSigninPartitionManager()->GetCurrentStoragePartitionName());

  // Make sure that the StoragePartition has not been re-used.
  EXPECT_NE(signin_partition_name_1, signin_partition_name_2);
  EXPECT_NE(signin_partition_1, signin_partition_2);

  // Make sure that the first StoragePartition has been cleared automatically.
  ExpectOneClearPartitionTask(signin_partition_1);
  FinishAllClearPartitionTasks();

  // Make sure that the second StoragePartition will be cleared when we
  // explicitly close the current sign-in session.
  bool closure_called = false;
  base::RepeatingClosure partition_cleared_closure = base::BindRepeating(
      [](bool* closure_called_ptr) { *closure_called_ptr = true; },
      &closure_called);

  GetSigninPartitionManager()->CloseCurrentSigninSession(
      partition_cleared_closure);
  EXPECT_FALSE(closure_called);

  ExpectOneClearPartitionTask(signin_partition_2);
  FinishAllClearPartitionTasks();

  EXPECT_TRUE(closure_called);
}

TEST_F(SigninPartitionManagerTest, HttpAuthCacheTransferred) {
  base::RunLoop loop_prepare;
  base::PostTaskWithTraitsAndReply(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(AddEntryToHttpAuthCache,
                     base::RetainedRef(GetSystemURLRequestContextGetter())),
      loop_prepare.QuitClosure());
  loop_prepare.Run();

  RunStartSigninSesssion(signin_ui_web_contents());
  net::URLRequestContextGetter* signin_url_request_context_getter =
      GetSigninPartitionManager()
          ->GetCurrentStoragePartition()
          ->GetURLRequestContext();

  bool entry_found = false;
  base::RunLoop loop_check;
  base::PostTaskWithTraitsAndReply(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(IsEntryInHttpAuthCache,
                     base::RetainedRef(signin_url_request_context_getter),
                     &entry_found),
      loop_check.QuitClosure());
  loop_check.Run();
  EXPECT_TRUE(entry_found);
}

}  // namespace login
}  // namespace chromeos
