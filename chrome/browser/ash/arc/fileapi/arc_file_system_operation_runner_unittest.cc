// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/fileapi/arc_file_system_operation_runner.h"

#include <string.h>

#include <memory>
#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/experiences/arc/mojom/file_system.mojom.h"
#include "chromeos/ash/experiences/arc/session/arc_bridge_service.h"
#include "chromeos/ash/experiences/arc/session/arc_service_manager.h"
#include "chromeos/ash/experiences/arc/test/connection_holder_util.h"
#include "chromeos/ash/experiences/arc/test/fake_file_system_instance.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace arc {

namespace {

using File = FakeFileSystemInstance::File;

constexpr char kAuthority[] = "authority";
constexpr char kDocumentId[] = "document_id";
constexpr char kRootId[] = "root_id";
constexpr char kUrl[] = "content://test";
constexpr char kUrlId[] = "url_id";

constexpr char kAccessibleContentUrl[] =
    "content://org.chromium.test/accessible";
constexpr char kInaccessibleContentUrl[] =
    "content://org.chromium.test/inaccessible";
constexpr char kData[] = "abcdef";
constexpr char kMimeType[] = "application/octet-stream";

constexpr size_t kTestAllowlistCacheSize = 10;

}  // namespace

class ArcFileSystemOperationRunnerTest : public testing::Test {
 public:
  ArcFileSystemOperationRunnerTest() = default;

  ArcFileSystemOperationRunnerTest(const ArcFileSystemOperationRunnerTest&) =
      delete;
  ArcFileSystemOperationRunnerTest& operator=(
      const ArcFileSystemOperationRunnerTest&) = delete;

  ~ArcFileSystemOperationRunnerTest() override = default;

  void SetUp() override {
    arc_service_manager_ = std::make_unique<ArcServiceManager>();
    profile_ = std::make_unique<TestingProfile>();
    ArcFileSystemBridge::GetForBrowserContextForTesting(profile_.get());
    runner_ = ArcFileSystemOperationRunner::CreateForTesting(
        profile_.get(), arc_service_manager_->arc_bridge_service(),
        kTestAllowlistCacheSize);
    arc_service_manager_->arc_bridge_service()->file_system()->SetInstance(
        &file_system_instance_);
    WaitForInstanceReady(
        arc_service_manager_->arc_bridge_service()->file_system());

    // Run the message loop until FileSystemInstance::Init() is called.
    ASSERT_TRUE(file_system_instance_.InitCalled());
  }

  void TearDown() override {
    arc_service_manager_->arc_bridge_service()->file_system()->CloseInstance(
        &file_system_instance_);
    // Explicitly calls Shutdown() to detach from services.
    if (runner_)
      runner_->Shutdown();
  }

 protected:
  // Calls private ArcFileSystemOperationRunner::SetShouldDefer().
  void CallSetShouldDefer(bool should_defer) {
    runner_->SetShouldDefer(should_defer);
  }

  bool IsContentUrlAccessible(const GURL& url) {
    base::test::TestFuture<bool> future;
    runner_->IsContentUrlAccessible(url, future.GetCallback());
    return future.Get();
  }

  // Calls all functions implemented by ArcFileSystemOperationRunner.
  void CallAllFunctions(int* counter) {
    // Following functions are deferred.
    runner_->AddWatcher(
        kAuthority, kDocumentId, base::DoNothing(),
        base::BindOnce([](int* counter, int64_t watcher_id) { ++*counter; },
                       counter));
    runner_->GetChildDocuments(
        kAuthority, kDocumentId,
        base::BindOnce(
            [](int* counter,
               std::optional<std::vector<mojom::DocumentPtr>> documents) {
              ++*counter;
            },
            counter));
    runner_->GetDocument(
        kAuthority, kDocumentId,
        base::BindOnce(
            [](int* counter, mojom::DocumentPtr document) { ++*counter; },
            counter));
    runner_->GetFileSize(
        GURL(kUrl),
        base::BindOnce([](int* counter, int64_t size) { ++*counter; },
                       counter));
    runner_->GetMimeType(
        GURL(kUrl),
        base::BindOnce(
            [](int* counter, const std::optional<std::string>& mime_type) {
              ++*counter;
            },
            counter));
    runner_->GetRecentDocuments(
        kAuthority, kDocumentId,
        base::BindOnce(
            [](int* counter,
               std::optional<std::vector<mojom::DocumentPtr>> documents) {
              ++*counter;
            },
            counter));
    runner_->GetRoots(base::BindOnce(
        [](int* counter, std::optional<std::vector<mojom::RootPtr>> roots) {
          ++*counter;
        },
        counter));
    runner_->GetRootSize(
        kAuthority, kRootId,
        base::BindOnce(
            [](int* counter, mojom::RootSizePtr root_size) { ++*counter; },
            counter));
    runner_->OpenFileSessionToWrite(
        GURL(kUrl),
        base::BindOnce([](int* counter,
                          mojom::FileSessionPtr file_session) { ++*counter; },
                       counter));
    runner_->OpenFileSessionToRead(
        GURL(kUrl),
        base::BindOnce([](int* counter,
                          mojom::FileSessionPtr file_session) { ++*counter; },
                       counter));
    runner_->CloseFileSession(kUrlId, /*error_message=*/std::string());

    // RemoveWatcher() is never deferred.
    runner_->RemoveWatcher(
        123, base::BindOnce([](int* counter, bool success) { ++*counter; },
                            counter));
  }

  content::BrowserTaskEnvironment task_environment_;
  FakeFileSystemInstance file_system_instance_;

  // Use the same initialization/destruction order as
  // `ChromeBrowserMainPartsAsh`.
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ArcFileSystemOperationRunner> runner_;
};

TEST_F(ArcFileSystemOperationRunnerTest, RunImmediately) {
  int counter = 0;
  CallSetShouldDefer(false);
  CallAllFunctions(&counter);
  EXPECT_TRUE(base::test::RunUntil([&]() { return counter == 11; }));
}

TEST_F(ArcFileSystemOperationRunnerTest, DeferAndRun) {
  int counter = 0;
  CallSetShouldDefer(true);
  CallAllFunctions(&counter);
  EXPECT_TRUE(base::test::RunUntil([&]() { return counter == 1; }));

  CallSetShouldDefer(false);
  EXPECT_TRUE(base::test::RunUntil([&]() { return counter == 11; }));
}

// TODO(nya,hidehiko): Check if we should keep this test.
TEST_F(ArcFileSystemOperationRunnerTest, DeferAndDiscard) {
  int counter = 0;
  CallSetShouldDefer(true);
  CallAllFunctions(&counter);
  EXPECT_TRUE(base::test::RunUntil([&]() { return counter == 1; }));

  runner_->Shutdown();
  runner_.reset();
  // No more callbacks are expected to run since the runner was destroyed.
  EXPECT_EQ(1, counter);
}

TEST_F(ArcFileSystemOperationRunnerTest, FileInstanceUnavailable) {
  arc_service_manager_->arc_bridge_service()->file_system()->CloseInstance(
      &file_system_instance_);

  int counter = 0;
  CallSetShouldDefer(false);
  CallAllFunctions(&counter);
  EXPECT_TRUE(base::test::RunUntil([&]() { return counter == 11; }));
}

TEST_F(ArcFileSystemOperationRunnerTest, ContentUrlAccess) {
  // The instance knows about both URLs, but only one of them has been made
  // accessible to the runner.
  file_system_instance_.AddFile(
      File(kAccessibleContentUrl, kData, kMimeType, File::Seekable::YES));
  file_system_instance_.AddFile(
      File(kInaccessibleContentUrl, kData, kMimeType, File::Seekable::YES));
  runner_->GrantAccessToContentUrl(GURL(kAccessibleContentUrl));

  {
    base::test::TestFuture<int64_t> future;
    runner_->GetFileSize(GURL(kAccessibleContentUrl), future.GetCallback());
    EXPECT_EQ(static_cast<int64_t>(std::string_view(kData).size()),
              future.Get());
  }
  {
    base::test::TestFuture<int64_t> future;
    runner_->GetFileSize(GURL(kInaccessibleContentUrl), future.GetCallback());
    EXPECT_EQ(-1, future.Get());
  }
  {
    base::test::TestFuture<std::optional<std::string>> future;
    runner_->GetMimeType(
        GURL(kAccessibleContentUrl),
        future.GetCallback<const std::optional<std::string>&>());
    EXPECT_EQ(kMimeType, future.Get());
  }
  {
    base::test::TestFuture<std::optional<std::string>> future;
    runner_->GetMimeType(
        GURL(kInaccessibleContentUrl),
        future.GetCallback<const std::optional<std::string>&>());
    EXPECT_EQ(std::nullopt, future.Get());
  }
  {
    base::test::TestFuture<mojom::FileSessionPtr> future;
    runner_->OpenFileSessionToRead(GURL(kAccessibleContentUrl),
                                   future.GetCallback());
    EXPECT_FALSE(future.Get().is_null());
  }
  {
    base::test::TestFuture<mojom::FileSessionPtr> future;
    runner_->OpenFileSessionToRead(GURL(kInaccessibleContentUrl),
                                   future.GetCallback());
    EXPECT_TRUE(future.Get().is_null());
  }
  {
    base::test::TestFuture<mojom::FileSessionPtr> future;
    runner_->OpenFileSessionToWrite(GURL(kAccessibleContentUrl),
                                    future.GetCallback());
    EXPECT_FALSE(future.Get().is_null());
  }
  {
    base::test::TestFuture<mojom::FileSessionPtr> future;
    runner_->OpenFileSessionToWrite(GURL(kInaccessibleContentUrl),
                                    future.GetCallback());
    EXPECT_TRUE(future.Get().is_null());
  }
}

TEST_F(ArcFileSystemOperationRunnerTest, LruCacheAndDatabaseBehavior) {
  base::HistogramTester histogram_tester;

  const GURL url0("content://test/0");
  const GURL url1("content://test/1");
  const GURL urlx("content://test/x");

  // Fill the in-memory cache to its capacity.
  for (size_t i = 0; i < kTestAllowlistCacheSize; ++i) {
    runner_->GrantAccessToContentUrl(
        GURL(base::StringPrintf("content://test/%zu", i)));
  }

  // All URLs should be accessible in the in-memory cache.
  for (size_t i = 0; i < kTestAllowlistCacheSize; ++i) {
    GURL url(base::StringPrintf("content://test/%zu", i));
    EXPECT_TRUE(IsContentUrlAccessible(url));
  }
  histogram_tester.ExpectBucketCount(
      "Arc.FileSystem.ContentUrlAccessCheckResult",
      arc::ArcContentUrlAccessCheckResult::kCacheHit, kTestAllowlistCacheSize);

  // After the above loop, url0 is the least recently used in the cache.
  // Now, granting access to a new URL (urlx) should evict url0 from the
  // in-memory cache to the database.
  runner_->GrantAccessToContentUrl(urlx);

  // urlx itself should go to the in-memory cache, so accessing it should be a
  // primary hit.
  EXPECT_TRUE(IsContentUrlAccessible(urlx));
  histogram_tester.ExpectBucketCount(
      "Arc.FileSystem.ContentUrlAccessCheckResult",
      arc::ArcContentUrlAccessCheckResult::kCacheHit,
      kTestAllowlistCacheSize + 1);

  // url0 is still accessible because it resides in the database.
  // Accessing it will record a kSecondaryHit.
  EXPECT_TRUE(IsContentUrlAccessible(url0));
  histogram_tester.ExpectBucketCount(
      "Arc.FileSystem.ContentUrlAccessCheckResult",
      arc::ArcContentUrlAccessCheckResult::kDatabaseHit, 1);

  // Accessing url0 promoted it back to the in-memory cache, which in turn
  // should evict the next LRU item of the in-memory cache (which is url1).
  EXPECT_TRUE(IsContentUrlAccessible(url0));
  histogram_tester.ExpectBucketCount(
      "Arc.FileSystem.ContentUrlAccessCheckResult",
      arc::ArcContentUrlAccessCheckResult::kCacheHit,
      kTestAllowlistCacheSize + 2);

  // url1 is still accessible because it resides in the database.
  // Accessing it will record a kSecondaryHit.
  EXPECT_TRUE(IsContentUrlAccessible(url1));
  histogram_tester.ExpectBucketCount(
      "Arc.FileSystem.ContentUrlAccessCheckResult",
      arc::ArcContentUrlAccessCheckResult::kDatabaseHit, 2);

  // An ungranted URL should return false and record kMiss.
  const GURL ungranted_url("content://test/ungranted");
  EXPECT_FALSE(IsContentUrlAccessible(ungranted_url));
  histogram_tester.ExpectBucketCount(
      "Arc.FileSystem.ContentUrlAccessCheckResult",
      arc::ArcContentUrlAccessCheckResult::kDenied, 1);
}

}  // namespace arc
