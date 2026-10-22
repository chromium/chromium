// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/ime_service_connector.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/services/ime/constants.h"
#include "content/public/test/browser_task_environment.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace input_method {
namespace {

class ImeServiceConnectorTest : public testing::Test {
 public:
  ImeServiceConnectorTest() = default;

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    profile_ = TestingProfile::Builder()
                   .SetPath(temp_dir_.GetPath())
                   .SetSharedURLLoaderFactory(
                       base::MakeRefCounted<
                           network::WeakWrapperSharedURLLoaderFactory>(
                           &test_url_loader_factory_))
                   .Build();
    connector_ = std::make_unique<ImeServiceConnector>(profile_.get());
  }

  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ImeServiceConnector> connector_;
};

TEST_F(ImeServiceConnectorTest, DownloadInvalidURL) {
  base::test::TestFuture<base::FilePath> future;
  base::FilePath valid_path = base::FilePath(ime::kInputMethodsDirName)
                                  .Append(ime::kLanguageDataDirName)
                                  .Append("test.dict");

  // HTTP not allowed
  connector_->DownloadImeFileTo(GURL("http://dl.google.com/test.dict"),
                                valid_path,
                                future.GetCallback<const base::FilePath&>());
  EXPECT_TRUE(future.Get().empty());

  // Invalid domain
  future.Clear();
  connector_->DownloadImeFileTo(GURL("https://malicious.com/test.dict"),
                                valid_path,
                                future.GetCallback<const base::FilePath&>());
  EXPECT_TRUE(future.Get().empty());
}

TEST_F(ImeServiceConnectorTest, DownloadInvalidPath) {
  base::test::TestFuture<base::FilePath> future;
  GURL valid_url("https://dl.google.com/test.dict");

  // Absolute path not allowed
  connector_->DownloadImeFileTo(valid_url, base::FilePath("/tmp/test.dict"),
                                future.GetCallback<const base::FilePath&>());
  EXPECT_TRUE(future.Get().empty());

  // Empty path not allowed
  future.Clear();
  connector_->DownloadImeFileTo(valid_url, base::FilePath(),
                                future.GetCallback<const base::FilePath&>());
  EXPECT_TRUE(future.Get().empty());

  // References parent not allowed
  future.Clear();
  connector_->DownloadImeFileTo(valid_url,
                                base::FilePath(ime::kInputMethodsDirName)
                                    .Append("..")
                                    .Append("test.dict"),
                                future.GetCallback<const base::FilePath&>());
  EXPECT_TRUE(future.Get().empty());

  // Not in correct parent directory
  future.Clear();
  connector_->DownloadImeFileTo(valid_url,
                                base::FilePath("wrong_dir/test.dict"),
                                future.GetCallback<const base::FilePath&>());
  EXPECT_TRUE(future.Get().empty());
}

TEST_F(ImeServiceConnectorTest, DownloadValidRequest) {
  base::test::TestFuture<base::FilePath> future;
  GURL valid_url("https://dl.google.com/test.dict");
  base::FilePath relative_path = base::FilePath(ime::kInputMethodsDirName)
                                     .Append(ime::kLanguageDataDirName)
                                     .Append("test.dict");
  base::FilePath expected_full_path = profile_->GetPath().Append(relative_path);
  ASSERT_TRUE(base::CreateDirectory(expected_full_path.DirName()));

  connector_->DownloadImeFileTo(valid_url, relative_path,
                                future.GetCallback<const base::FilePath&>());

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return test_url_loader_factory_.NumPending() == 1; }));

  // Verify download request was made
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      &test_url_loader_factory_.pending_requests()->back();
  EXPECT_EQ(pending_request->request.url, valid_url);

  // Mock successful download
  test_url_loader_factory_.SimulateResponseForPendingRequest(valid_url.spec(),
                                                             "test contents");

  EXPECT_EQ(future.Get(), expected_full_path);
}

TEST_F(ImeServiceConnectorTest, SimultaneousRequestsSameURL) {
  GURL url("https://dl.google.com/test.dict");
  base::FilePath relative_path = base::FilePath(ime::kInputMethodsDirName)
                                     .Append(ime::kLanguageDataDirName)
                                     .Append("test.dict");
  base::FilePath expected_full_path = profile_->GetPath().Append(relative_path);
  ASSERT_TRUE(base::CreateDirectory(expected_full_path.DirName()));

  base::test::TestFuture<base::FilePath> future1;
  base::test::TestFuture<base::FilePath> future2;

  connector_->DownloadImeFileTo(url, relative_path,
                                future1.GetCallback<const base::FilePath&>());
  connector_->DownloadImeFileTo(url, relative_path,
                                future2.GetCallback<const base::FilePath&>());

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return test_url_loader_factory_.NumPending() == 1; }));

  // Only 1 pending request should be triggered
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);

  test_url_loader_factory_.SimulateResponseForPendingRequest(url.spec(),
                                                             "test contents");

  EXPECT_EQ(future1.Get(), expected_full_path);
  EXPECT_EQ(future2.Get(), expected_full_path);
}

TEST_F(ImeServiceConnectorTest, SimultaneousRequestsDifferentURL) {
  GURL url1("https://dl.google.com/test1.dict");
  GURL url2("https://dl.google.com/test2.dict");
  base::FilePath relative_path1 = base::FilePath(ime::kInputMethodsDirName)
                                      .Append(ime::kLanguageDataDirName)
                                      .Append("test1.dict");
  base::FilePath relative_path2 = base::FilePath(ime::kInputMethodsDirName)
                                      .Append(ime::kLanguageDataDirName)
                                      .Append("test2.dict");
  base::FilePath expected_full_path2 =
      profile_->GetPath().Append(relative_path2);
  ASSERT_TRUE(base::CreateDirectory(expected_full_path2.DirName()));

  base::test::TestFuture<base::FilePath> future1;
  base::test::TestFuture<base::FilePath> future1_coalesced;
  base::test::TestFuture<base::FilePath> future2;

  connector_->DownloadImeFileTo(url1, relative_path1,
                                future1.GetCallback<const base::FilePath&>());
  connector_->DownloadImeFileTo(
      url1, relative_path1,
      future1_coalesced.GetCallback<const base::FilePath&>());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return test_url_loader_factory_.IsPending(url1.spec()); }));

  connector_->DownloadImeFileTo(url2, relative_path2,
                                future2.GetCallback<const base::FilePath&>());

  // All listeners on the first request should be superseded by the second
  // request for a different URL and return an empty path immediately.
  EXPECT_TRUE(future1.Get().empty());
  EXPECT_TRUE(future1_coalesced.Get().empty());

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return test_url_loader_factory_.IsPending(url2.spec()) &&
           !test_url_loader_factory_.IsPending(url1.spec());
  }));
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);

  test_url_loader_factory_.SimulateResponseForPendingRequest(url2.spec(),
                                                             "test contents");
  EXPECT_EQ(future2.Get(), expected_full_path2);
}

TEST_F(ImeServiceConnectorTest, SynchronousDownloadRequestFromCallback) {
  GURL url1("https://dl.google.com/test1.dict");
  GURL url2("https://dl.google.com/test2.dict");
  GURL url3("https://dl.google.com/test3.dict");
  base::FilePath relative_path1 = base::FilePath(ime::kInputMethodsDirName)
                                      .Append(ime::kLanguageDataDirName)
                                      .Append("test1.dict");
  base::FilePath relative_path2 = base::FilePath(ime::kInputMethodsDirName)
                                      .Append(ime::kLanguageDataDirName)
                                      .Append("test2.dict");
  base::FilePath relative_path3 = base::FilePath(ime::kInputMethodsDirName)
                                      .Append(ime::kLanguageDataDirName)
                                      .Append("test3.dict");
  base::FilePath expected_full_path1 =
      profile_->GetPath().Append(relative_path1);
  base::FilePath expected_full_path3 =
      profile_->GetPath().Append(relative_path3);
  ASSERT_TRUE(base::CreateDirectory(expected_full_path1.DirName()));

  base::FilePath callback1_path;
  base::FilePath callback3_path;
  base::test::TestFuture<base::FilePath> future2;
  base::test::TestFuture<base::FilePath> future1_retry;

  // When `url1` is superseded by `url2`, its callback synchronously requests
  // `url3`; when `url3` completes, its callback synchronously retries `url1`.
  connector_->DownloadImeFileTo(
      url1, relative_path1,
      base::BindLambdaForTesting([&](const base::FilePath& path) {
        callback1_path = path;
        connector_->DownloadImeFileTo(
            url3, relative_path3,
            base::BindLambdaForTesting([&](const base::FilePath& path3) {
              callback3_path = path3;
              connector_->DownloadImeFileTo(
                  url1, relative_path1,
                  future1_retry.GetCallback<const base::FilePath&>());
            }));
      }));

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return test_url_loader_factory_.IsPending(url1.spec()); }));

  // Supersede `url1` with `url2`, which synchronously queues `url3` from
  // `url1`'s cancellation callback and supersedes `url2`.
  connector_->DownloadImeFileTo(url2, relative_path2,
                                future2.GetCallback<const base::FilePath&>());
  EXPECT_TRUE(future2.Get().empty());
  EXPECT_TRUE(callback1_path.empty());

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return test_url_loader_factory_.IsPending(url3.spec()) &&
           !test_url_loader_factory_.IsPending(url1.spec()) &&
           !test_url_loader_factory_.IsPending(url2.spec());
  }));
  test_url_loader_factory_.SimulateResponseForPendingRequest(url3.spec(),
                                                             "test contents 3");

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return test_url_loader_factory_.IsPending(url1.spec()); }));
  EXPECT_EQ(callback3_path, expected_full_path3);

  test_url_loader_factory_.SimulateResponseForPendingRequest(url1.spec(),
                                                             "test contents 1");
  EXPECT_EQ(future1_retry.Get(), expected_full_path1);
}

TEST_F(ImeServiceConnectorTest, DownloadBlocksMaliciousRedirect) {
  base::test::TestFuture<base::FilePath> future;
  GURL valid_url("https://dl.google.com/test.dict");
  GURL malicious_url("https://malicious.com/redirect");
  base::FilePath relative_path = base::FilePath(ime::kInputMethodsDirName)
                                     .Append(ime::kLanguageDataDirName)
                                     .Append("test.dict");

  net::RedirectInfo redirect_info;
  redirect_info.new_url = malicious_url;

  network::TestURLLoaderFactory::Redirects redirects;
  redirects.push_back({redirect_info, network::mojom::URLResponseHead::New()});

  test_url_loader_factory_.AddResponse(
      valid_url, network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::OK), std::move(redirects));

  connector_->DownloadImeFileTo(valid_url, relative_path,
                                future.GetCallback<const base::FilePath&>());

  // Because the redirect is malicious, it should immediately abort and return
  // an empty path.
  EXPECT_TRUE(future.Get().empty());
}

TEST_F(ImeServiceConnectorTest, DownloadAllowsAllowlistedRedirect) {
  base::test::TestFuture<base::FilePath> future;
  GURL valid_url("https://dl.google.com/test.dict");
  GURL redirected_valid_url("https://edgedl.me.gvt1.com/test.dict");
  base::FilePath relative_path = base::FilePath(ime::kInputMethodsDirName)
                                     .Append(ime::kLanguageDataDirName)
                                     .Append("test.dict");
  base::FilePath expected_full_path = profile_->GetPath().Append(relative_path);
  ASSERT_TRUE(base::CreateDirectory(expected_full_path.DirName()));

  net::RedirectInfo redirect_info;
  redirect_info.new_url = redirected_valid_url;

  network::TestURLLoaderFactory::Redirects redirects;
  redirects.push_back({redirect_info, network::mojom::URLResponseHead::New()});

  // Prime the factory for the redirected URL as well.
  test_url_loader_factory_.AddResponse(
      valid_url, network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::OK), std::move(redirects));
  test_url_loader_factory_.AddResponse(redirected_valid_url.spec(),
                                       "test contents");

  connector_->DownloadImeFileTo(valid_url, relative_path,
                                future.GetCallback<const base::FilePath&>());

  // Should succeed because redirected_valid_url is allowlisted.
  EXPECT_EQ(future.Get(), expected_full_path);
}

}  // namespace
}  // namespace input_method
}  // namespace ash
