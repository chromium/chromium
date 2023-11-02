// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/deferred_client_wrapper.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/test/base/testing_profile.h"
#include "components/download/public/background_service/test/mock_client.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace download {

class DeferredClientWrapperTest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();

    deferred_wrapper_ = std::make_unique<DeferredClientWrapper>(
        base::BindOnce(&DeferredClientWrapperTest::CreateClient,
                       base::Unretained(this)),
        testing_profile_.GetProfileKey());
  }

  std::unique_ptr<Client> CreateClient(Profile* profile) {
    auto mock_client = std::make_unique<test::MockClient>();
    mock_client_ = mock_client.get();
    return std::move(mock_client);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile testing_profile_;
  std::unique_ptr<DeferredClientWrapper> deferred_wrapper_;
  raw_ptr<test::MockClient> mock_client_;
};

// Tests that DeferredClientWrapper is reentrant (doesn't crash if called into
// while handling another download::Client interface call).
TEST_F(DeferredClientWrapperTest, Reentrancy) {
  DeferredClientWrapper* deferred_wrapper = deferred_wrapper_.get();
  EXPECT_CALL(*mock_client_,
              OnDownloadUpdated(testing::_, testing::_, testing::_))
      .WillOnce([&](const std::string& guid, uint64_t bytes_uploaded,
                    uint64_t bytes_downloaded) {
        deferred_wrapper->GetUploadData(
            guid,
            base::BindOnce([](scoped_refptr<network::ResourceRequestBody>) {}));
      });

  deferred_wrapper_->OnDownloadUpdated("guid", 0, 0);
}

}  // namespace download
