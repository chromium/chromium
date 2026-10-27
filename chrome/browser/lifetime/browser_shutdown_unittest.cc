// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/browser_shutdown.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/test/test_cookie_manager.h"
#include "services/network/test/test_network_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace browser_shutdown {
namespace {

class FlushCountingCookieManager : public network::TestCookieManager {
 public:
  explicit FlushCountingCookieManager(base::OnceClosure on_flush)
      : on_flush_(std::move(on_flush)) {}

  void FlushCookieStore(FlushCookieStoreCallback callback) override {
    ++flush_count_;
    if (callback) {
      std::move(callback).Run();
    }
    if (on_flush_) {
      std::move(on_flush_).Run();
    }
  }

  int flush_count() const { return flush_count_; }

 private:
  int flush_count_ = 0;
  base::OnceClosure on_flush_;
};

class CookieManagerTestNetworkContext : public network::TestNetworkContext {
 public:
  explicit CookieManagerTestNetworkContext(
      network::mojom::CookieManager* cookie_manager)
      : cookie_manager_receiver_(cookie_manager) {}

  void GetCookieManager(
      mojo::PendingReceiver<network::mojom::CookieManager> receiver) override {
    cookie_manager_receiver_.Bind(std::move(receiver));
  }

 private:
  mojo::Receiver<network::mojom::CookieManager> cookie_manager_receiver_;
};

TEST(BrowserShutdownTest, OnShutdownStartingFlushesLoadedStoragePartitions) {
  content::BrowserTaskEnvironment task_environment;
  ResetShutdownGlobalsForTesting();
  absl::Cleanup cleanup = [] { ResetShutdownGlobalsForTesting(); };

  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());
  TestingProfile* profile = profile_manager.CreateTestingProfile("user");
  ASSERT_NE(profile, nullptr);

  content::StoragePartition* default_partition =
      profile->GetDefaultStoragePartition();
  ASSERT_NE(default_partition, nullptr);

  base::RunLoop flush_loop;
  FlushCountingCookieManager cookie_manager(flush_loop.QuitClosure());
  CookieManagerTestNetworkContext network_context(&cookie_manager);
  mojo::Receiver<network::mojom::NetworkContext> network_context_receiver(
      &network_context);
  default_partition->SetNetworkContextForTesting(
      network_context_receiver.BindNewPipeAndPassRemote());

  OnShutdownStarting(ShutdownType::kEndSession);
  EXPECT_TRUE(HasShutdownStarted());
  flush_loop.Run();
  EXPECT_EQ(cookie_manager.flush_count(), 1);
}

}  // namespace
}  // namespace browser_shutdown
