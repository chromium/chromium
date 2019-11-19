// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_fetcher_win.h"

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/net_errors.h"
#include "net/base/network_change_notifier.h"
#include "net/http/http_status_code.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace safe_browsing {
namespace {

class ChromeCleanerFetcherTest : public ::testing::Test {
 public:
  ChromeCleanerFetcherTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  void TearDown() override {
    if (!downloaded_path_.empty()) {
      base::DeleteFile(downloaded_path_, /*recursive=*/false);
      if (base::IsDirectoryEmpty(downloaded_path_.DirName()))
        base::DeleteFile(downloaded_path_.DirName(), /*recursive=*/false);
    }
  }

  void StartFetch() {
    FetchChromeCleaner(
        base::BindOnce(&ChromeCleanerFetcherTest::FetchedCallback,
                       base::Unretained(this)),
        &test_url_loader_factory_);
  }

  void FetchedCallback(base::FilePath downloaded_path,
                       ChromeCleanerFetchStatus fetch_status) {
    callback_called_ = true;
    downloaded_path_ = downloaded_path;
    fetch_status_ = fetch_status;
    run_loop_.Quit();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;

  base::RunLoop run_loop_;

  // Variables set by FetchedCallback().
  bool callback_called_ = false;
  base::FilePath downloaded_path_;
  ChromeCleanerFetchStatus fetch_status_ =
      ChromeCleanerFetchStatus::kOtherFailure;
};

TEST_F(ChromeCleanerFetcherTest, FetchSuccess) {
  const std::string kFileContents("FileContents");
  test_url_loader_factory_.AddResponse(GetSRTDownloadURL().spec(),
                                       kFileContents);

  StartFetch();
  run_loop_.Run();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(downloaded_path_, downloaded_path_);
  EXPECT_EQ(fetch_status_, ChromeCleanerFetchStatus::kSuccess);

  std::string file_contents;
  EXPECT_TRUE(ReadFileToString(downloaded_path_, &file_contents));
  EXPECT_EQ(kFileContents, file_contents);
}

TEST_F(ChromeCleanerFetcherTest, NotFoundOnServer) {
  test_url_loader_factory_.AddResponse(GetSRTDownloadURL().spec(), "",
                                       net::HTTP_NOT_FOUND);

  StartFetch();
  run_loop_.Run();

  EXPECT_TRUE(callback_called_);
  EXPECT_TRUE(downloaded_path_.empty());
  EXPECT_EQ(fetch_status_, ChromeCleanerFetchStatus::kNotFoundOnServer);
}

TEST_F(ChromeCleanerFetcherTest, NetworkError) {
  // For this test, just use any http response code other than net::HTTP_OK
  // and net::HTTP_NOT_FOUND.
  test_url_loader_factory_.AddResponse(
      GetSRTDownloadURL(), network::mojom::URLResponseHead::New(), "contents",
      network::URLLoaderCompletionStatus(net::ERR_ADDRESS_INVALID));

  StartFetch();
  run_loop_.Run();

  EXPECT_TRUE(callback_called_);
  EXPECT_TRUE(downloaded_path_.empty());
  EXPECT_EQ(fetch_status_, ChromeCleanerFetchStatus::kOtherFailure);
}

}  // namespace
}  // namespace safe_browsing
