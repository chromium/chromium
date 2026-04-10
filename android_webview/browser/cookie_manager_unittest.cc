// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/cookie_manager.h"

#include <memory>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_path_override.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_monster_store_test.h"
#include "net/cookies/cookie_options.h"
#include "net/log/net_log.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

class CookieManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    path_override_ = std::make_unique<base::ScopedPathOverride>(
        base::DIR_ANDROID_APP_DATA, temp_dir_.GetPath());
    cookie_manager_ = std::make_unique<CookieManager>(nullptr);
  }

  void TearDown() override { cookie_manager_.reset(); }

  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<base::ScopedPathOverride> path_override_;
  std::unique_ptr<CookieManager> cookie_manager_;
};

// Regression test for crbug.com/498517688.
//
// Verifies that OnProvisionalStoreOperationComplete() defers destruction of the
// CookieMonster via PostTask when the last pending provisional store operation
// completes from within CookieMonster::InvokeQueue(). Without the fix,
// OnProvisionalStoreOperationComplete() would synchronously call
// DoCloseProvisionalStoreAndSignalReady() which calls cookie_store_.reset(),
// destroying the CookieMonster while InvokeQueue() is still on the call stack
// -- a use-after-free.
TEST_F(CookieManagerTest, DeferredProvisionalStoreCloseInInvokeQueue) {
  // Create mojo pipes for the handoff. The receivers are held alive so the
  // pipes don't close prematurely when DoCloseProvisionalStoreAndSignalReady
  // runs as a deferred task.
  mojo::PendingRemote<network::mojom::CookieManager> cookie_manager_remote;
  auto cm_receiver = cookie_manager_remote.InitWithNewPipeAndPassReceiver();

  mojo::PendingRemote<network::mojom::CookieStoreReadyCallback> ready_callback;
  auto cb_receiver = ready_callback.InitWithNewPipeAndPassReceiver();

  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  bool cookie_store_alive_after_invoke_queue = false;
  auto main_task_runner = base::SequencedTaskRunner::GetCurrentDefault();

  cookie_manager_->cookie_store_task_runner_->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Replace the real cookie store with one backed by a mock persistent
        // store that defers loading, so we can control exactly when
        // CookieMonster::InvokeQueue() fires.
        auto store = base::MakeRefCounted<net::MockPersistentCookieStore>();
        store->set_store_load_commands(true);
        cookie_manager_->cookie_store_ = std::make_unique<net::CookieMonster>(
            store.get(), net::NetLog::Get());
        cookie_manager_->cookie_store_created_ = true;

        // Queue a cookie operation on the CookieMonster. It won't execute yet
        // because the mock store hasn't completed loading.
        const GURL kUrl("https://www.example.com");
        auto cookie = net::CanonicalCookie::CreateForTesting(
            kUrl, "a=b", base::Time::Now(), net::CookieSourceType::kOther);

        // Track this as a provisional store operation, just as the real
        // SetCookieHelper does.
        cookie_manager_->pending_provisional_store_operations_ = 1;

        cookie_manager_->cookie_store_->SetCanonicalCookieAsync(
            std::move(cookie), kUrl, net::CookieOptions::MakeAllInclusive(),
            base::BindLambdaForTesting([&](net::CookieAccessResult) {
              // This callback fires from within CookieMonster::InvokeQueue().
              // Call the actual OnProvisionalStoreOperationComplete() -- the
              // method under test.
              cookie_manager_->OnProvisionalStoreOperationComplete();
            }));

        // Set up the state as if CloseProvisionalStoreAndSignalReady() was
        // called and deferred because pending_provisional_store_operations_ >
        // 0. This is the state the CookieManager enters when the non-blocking
        // handoff (SetMojoCookieManagerNonBlocking) initiates while cookie
        // operations are still in-flight on the provisional store.
        cookie_manager_->waiting_to_close_provisional_store_ = true;
        cookie_manager_->setting_new_mojo_cookie_manager_ = true;
        cookie_manager_->deferred_cookie_manager_remote_ =
            std::move(cookie_manager_remote);
        cookie_manager_->deferred_ready_callback_ = std::move(ready_callback);

        // Complete the mock store load. This triggers:
        //   CookieMonster::OnLoaded() ->
        //   CookieMonster::InvokeQueue() ->
        //   queued SetCanonicalCookie runs ->
        //   our callback fires ->
        //   OnProvisionalStoreOperationComplete()
        //
        // With the fix, OnProvisionalStoreOperationComplete() posts
        // DoCloseProvisionalStoreAndSignalReady as a separate task instead of
        // calling it synchronously. This means the CookieMonster survives
        // InvokeQueue() and is destroyed safely in the next task.
        ASSERT_GE(store->commands().size(), 1u);
        ASSERT_EQ(store->commands()[0].type, net::CookieStoreCommand::LOAD);
        store->TakeCallbackAt(0).Run(
            std::vector<std::unique_ptr<net::CanonicalCookie>>());

        // KEY ASSERTION: After InvokeQueue() returns, the CookieMonster must
        // still be alive. Without the PostTask fix, cookie_store_.reset() would
        // have been called synchronously from within InvokeQueue(), destroying
        // the CookieMonster while it's still on the call stack.
        cookie_store_alive_after_invoke_queue =
            (cookie_manager_->cookie_store_ != nullptr);

        // The deferred DoCloseProvisionalStoreAndSignalReady task is now queued
        // on this thread. It will bind mojo_cookie_manager_ on this thread.
        // Post a cleanup task AFTER it to unbind mojo_cookie_manager_ on the
        // same thread, avoiding a sequence-checker DCHECK when the
        // CookieManager is destroyed on the main thread in TearDown().
        cookie_manager_->cookie_store_task_runner_->PostTask(
            FROM_HERE, base::BindLambdaForTesting([&]() {
              cookie_manager_->mojo_cookie_manager_.reset();
              // Quit the RunLoop on the main thread. This QuitClosure is
              // posted to the main thread after OnProvisionalStoreClosed has
              // already posted ClearClientHintsCachedPerOriginMapIfNeeded
              // there, so the main thread will process that task first,
              // ensuring the CookieManager outlives all pointers to it.
              main_task_runner->PostTask(FROM_HERE, std::move(quit_closure));
            }));
      }));

  // Pump the main thread until the cookie-thread cleanup task posts the
  // QuitClosure. This also processes ClearClientHintsCachedPerOriginMapIfNeeded
  // (posted to the main thread by OnProvisionalStoreClosed) before quitting,
  // so the CookieManager is still alive when that task runs.
  run_loop.Run();
  EXPECT_TRUE(cookie_store_alive_after_invoke_queue)
      << "CookieMonster was destroyed synchronously from within InvokeQueue(). "
         "OnProvisionalStoreOperationComplete should defer destruction via "
         "PostTask.";
}

}  // namespace android_webview
