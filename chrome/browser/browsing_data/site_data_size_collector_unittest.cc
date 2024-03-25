// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/site_data_size_collector.h"

#include <memory>
#include <string_view>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browsing_data/content/mock_browsing_data_quota_helper.h"
#include "components/browsing_data/content/mock_cookie_helper.h"
#include "components/browsing_data/content/mock_local_storage_helper.h"
#include "content/public/common/content_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kCookieFileData[] = "cookie_file_data";

class SiteDataSizeCollectorTest : public testing::Test {
 public:
  ~SiteDataSizeCollectorTest() override {
    profile_.reset();
  }

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    auto* storage_partition = profile_->GetDefaultStoragePartition();
    mock_browsing_data_cookie_helper_ =
        base::MakeRefCounted<browsing_data::MockCookieHelper>(
            storage_partition);
    mock_browsing_data_local_storage_helper_ =
        base::MakeRefCounted<browsing_data::MockLocalStorageHelper>(
            storage_partition);
    mock_browsing_data_quota_helper_ =
        base::MakeRefCounted<MockBrowsingDataQuotaHelper>();
    base::WriteFile(
        profile_->GetPath().Append(chrome::kCookieFilename),
        std::string_view(kCookieFileData, std::size(kCookieFileData)));
    fetched_size_ = -1;
  }

  void TearDown() override {
    mock_browsing_data_cookie_helper_ = nullptr;
    mock_browsing_data_local_storage_helper_ = nullptr;
    mock_browsing_data_quota_helper_ = nullptr;
  }

  void FetchCallback(base::OnceClosure done, int64_t size) {
    fetched_size_ = size;
    if (done)
      std::move(done).Run();
  }

 protected:
  int64_t fetched_size_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  scoped_refptr<browsing_data::MockCookieHelper>
      mock_browsing_data_cookie_helper_;
  scoped_refptr<browsing_data::MockLocalStorageHelper>
      mock_browsing_data_local_storage_helper_;
  scoped_refptr<MockBrowsingDataQuotaHelper> mock_browsing_data_quota_helper_;
};

TEST_F(SiteDataSizeCollectorTest, FetchCookie) {
  SiteDataSizeCollector collector(profile_->GetPath(),
                                  mock_browsing_data_cookie_helper_.get(),
                                  nullptr, nullptr);

  base::RunLoop run_loop;
  collector.Fetch(base::BindOnce(&SiteDataSizeCollectorTest::FetchCallback,
                                 base::Unretained(this),
                                 run_loop.QuitClosure()));
  // AddCookieSample() actually doesn't write the cookie to the file, only
  // triggers the condition to take the file into account.
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("http://foo1"), "A=1");
  mock_browsing_data_cookie_helper_->Notify();
  // Wait until reading files on blocking pool finishes.
  run_loop.Run();
  EXPECT_EQ(static_cast<int64_t>(std::size(kCookieFileData)), fetched_size_);
}

TEST_F(SiteDataSizeCollectorTest, FetchCookieWithoutEntry) {
  SiteDataSizeCollector collector(profile_->GetPath(),
                                  mock_browsing_data_cookie_helper_.get(),
                                  nullptr, nullptr);

  // Fetched size should be 0 if there are no cookies.
  collector.Fetch(base::BindOnce(&SiteDataSizeCollectorTest::FetchCallback,
                                 base::Unretained(this), base::OnceClosure()));
  mock_browsing_data_cookie_helper_->Notify();
  EXPECT_EQ(0, fetched_size_);
}

TEST_F(SiteDataSizeCollectorTest, FetchLocalStorage) {
  SiteDataSizeCollector collector(
      profile_->GetPath(), nullptr,
      mock_browsing_data_local_storage_helper_.get(), nullptr);

  collector.Fetch(base::BindOnce(&SiteDataSizeCollectorTest::FetchCallback,
                                 base::Unretained(this), base::OnceClosure()));
  mock_browsing_data_local_storage_helper_->AddLocalStorageSamples();
  mock_browsing_data_local_storage_helper_->Notify();
  EXPECT_EQ(3, fetched_size_);
}

TEST_F(SiteDataSizeCollectorTest, FetchQuota) {
  SiteDataSizeCollector collector(profile_->GetPath(), nullptr, nullptr,
                                  mock_browsing_data_quota_helper_.get());

  collector.Fetch(base::BindOnce(&SiteDataSizeCollectorTest::FetchCallback,
                                 base::Unretained(this), base::OnceClosure()));
  mock_browsing_data_quota_helper_->AddQuotaSamples();
  mock_browsing_data_quota_helper_->Notify();
  EXPECT_EQ(22, fetched_size_);
}

TEST_F(SiteDataSizeCollectorTest, FetchMultiple) {
  SiteDataSizeCollector collector(
      profile_->GetPath(), nullptr,
      mock_browsing_data_local_storage_helper_.get(),
      mock_browsing_data_quota_helper_.get());

  collector.Fetch(base::BindOnce(&SiteDataSizeCollectorTest::FetchCallback,
                                 base::Unretained(this), base::OnceClosure()));

  mock_browsing_data_local_storage_helper_->AddLocalStorageSamples();
  mock_browsing_data_local_storage_helper_->Notify();
  // The callback for Fetch() shouldn't be called at this point.
  EXPECT_EQ(-1, fetched_size_);

  mock_browsing_data_quota_helper_->AddQuotaSamples();
  mock_browsing_data_quota_helper_->Notify();
  EXPECT_EQ(3 + 22, fetched_size_);
}

}  // namespace
