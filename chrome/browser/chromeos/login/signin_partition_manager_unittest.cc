// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signin_partition_manager.h"

#include <memory>

#include "base/bind.h"
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
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/network_isolation_key.h"
#include "net/cookies/cookie_store.h"
#include "net/http/http_auth.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
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

void AddEntryToHttpAuthCache(network::NetworkContext* network_context) {
  net::HttpAuthCache* http_auth_cache = network_context->url_request_context()
                                            ->http_transaction_factory()
                                            ->GetSession()
                                            ->http_auth_cache();
  http_auth_cache->Add(GURL(kEmbedderUrl), net::HttpAuth::AUTH_PROXY, "",
                       net::HttpAuth::AUTH_SCHEME_BASIC,
                       net::NetworkIsolationKey(), "", net::AuthCredentials(),
                       "");
}

void IsEntryInHttpAuthCache(network::NetworkContext* network_context,
                            bool* out_entry_found) {
  net::HttpAuthCache* http_auth_cache = network_context->url_request_context()
                                            ->http_transaction_factory()
                                            ->GetSession()
                                            ->http_auth_cache();
  *out_entry_found =
      http_auth_cache->Lookup(GURL(kEmbedderUrl), net::HttpAuth::AUTH_PROXY, "",
                              net::HttpAuth::AUTH_SCHEME_BASIC,
                              net::NetworkIsolationKey()) != nullptr;
}

}  // namespace

class SigninPartitionManagerTest : public ChromeRenderViewHostTestHarness {
 protected:
  SigninPartitionManagerTest() {}
  ~SigninPartitionManagerTest() override {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    signin_browser_context_ = std::make_unique<TestingProfile>();
    // Wait for the Network Service to initialize on the IO thread.
    content::RunAllPendingInMessageLoop(content::BrowserThread::IO);

    auto network_context = std::make_unique<network::NetworkContext>(
        network::NetworkService::GetNetworkServiceForTesting(),
        signin_network_context_remote_.BindNewPipeAndPassReceiver(),
        network::mojom::NetworkContextParams::New());
    signin_network_context_ = network_context.get();
    TestingProfile::Builder()
        .BuildIncognito(signin_browser_context_.get())
        ->SetNetworkContext(std::move(network_context));

    signin_ui_web_contents_ = content::WebContentsTester::CreateTestWebContents(
        GetSigninProfile(), content::SiteInstance::Create(GetSigninProfile()));

    system_network_context_ = std::make_unique<network::NetworkContext>(
        network::NetworkService::GetNetworkServiceForTesting(),
        system_network_context_remote_.BindNewPipeAndPassReceiver(),
        network::mojom::NetworkContextParams::New());

    GURL url(kEmbedderUrl);
    content::WebContentsTester::For(signin_ui_web_contents())
        ->NavigateAndCommit(url);

    GetSigninPartitionManager()->SetClearStoragePartitionTaskForTesting(
        base::BindRepeating(
            &SigninPartitionManagerTest::ClearStoragePartitionTask,
            base::Unretained(this)));
    GetSigninPartitionManager()->SetGetSystemNetworkContextForTesting(
        base::BindRepeating(
            &SigninPartitionManagerTest::GetSystemNetworkContext,
            base::Unretained(this)));
  }

  void TearDown() override {
    system_network_context_.reset();

    signin_ui_web_contents_.reset();

    signin_browser_context_.reset();

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

  network::mojom::NetworkContext* GetSystemNetworkContext() {
    return system_network_context_.get();
  }

  network::NetworkContext* GetSystemNetworkContextImpl() {
    return system_network_context_.get();
  }

  network::NetworkContext* GetSigninNetworkContextImpl() {
    return signin_network_context_;
  }

 private:
  void ClearStoragePartitionTask(content::StoragePartition* partition,
                                 base::OnceClosure clear_done_closure) {
    pending_clear_tasks_.push_back({partition, std::move(clear_done_closure)});
  }

  mojo::Remote<network::mojom::NetworkContext> system_network_context_remote_;
  std::unique_ptr<network::NetworkContext> system_network_context_;

  std::unique_ptr<TestingProfile> signin_browser_context_;
  mojo::Remote<network::mojom::NetworkContext> signin_network_context_remote_;
  network::NetworkContext* signin_network_context_;

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
  base::PostTaskAndReply(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(AddEntryToHttpAuthCache,
                     base::Unretained(GetSystemNetworkContextImpl())),
      loop_prepare.QuitClosure());
  loop_prepare.Run();

  RunStartSigninSesssion(signin_ui_web_contents());

  bool entry_found = false;
  base::RunLoop loop_check;
  base::PostTaskAndReply(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(IsEntryInHttpAuthCache,
                     base::Unretained(GetSigninNetworkContextImpl()),
                     &entry_found),
      loop_check.QuitClosure());
  loop_check.Run();
  EXPECT_TRUE(entry_found);
}

}  // namespace login
}  // namespace chromeos
