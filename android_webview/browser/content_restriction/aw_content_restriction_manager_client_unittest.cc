// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/content_restriction/aw_content_restriction_manager_client.h"

#include <memory>
#include <string>

#include "android_webview/common/aw_features.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {
namespace {

constexpr char kTestUrl[] = "https://www.example.test/";
constexpr int64_t kTestNavigationId = 123;
constexpr base::TimeDelta kTestContentClassificationTimeout = base::Seconds(5);

class MockContentRestrictionManagerClientDelegate
    : public AwContentRestrictionManagerClient::Delegate {
 public:
  MockContentRestrictionManagerClientDelegate()
      : AwContentRestrictionManagerClient::Delegate(nullptr) {}
  ~MockContentRestrictionManagerClientDelegate() override = default;

  void RequestContentClassification(
      int64_t navigation_id,
      const std::string& url,
      const std::string& mime_type,
      AwContentRestrictionManagerClient::ContentClassificationCallback callback)
      override {
    last_navigation_id_ = navigation_id;
    last_url_ = url;
    classification_call_count_++;
    last_callback_ = std::move(callback);
  }

  void TriggerClassificationResult(bool is_allowed) {
    if (last_callback_) {
      std::move(last_callback_).Run(is_allowed);
    }
  }

  int64_t last_navigation_id() const { return last_navigation_id_; }
  const std::string& last_url() const { return last_url_; }
  size_t classification_call_count() const {
    return classification_call_count_;
  }
  bool has_pending_callback() const { return !last_callback_.is_null(); }

 private:
  int64_t last_navigation_id_ = 0;
  std::string last_url_;
  size_t classification_call_count_ = 0;
  base::OnceCallback<void(bool)> last_callback_;
};

class AwContentRestrictionManagerClientTest : public testing::Test {
 protected:
  AwContentRestrictionManagerClientTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kWebViewContentRestrictionSupport,
        {{features::kWebViewContentRestrictionTimeout.name,
          base::NumberToString(kTestContentClassificationTimeout.InSeconds()) +
              "s"}});
  }

  void SetUp() override {
    auto delegate =
        std::make_unique<MockContentRestrictionManagerClientDelegate>();
    mock_delegate_ = delegate.get();
    client_ = AwContentRestrictionManagerClient::CreateForTesting(
        std::move(delegate));
  }

  void RequestContentClassification(bool* callback_run, bool* callback_result) {
    size_t initial_call_count = mock_delegate_->classification_call_count();
    network::ResourceRequest request;
    request.url = GURL(kTestUrl);
    client_->RequestContentClassification(
        kTestNavigationId, request,
        base::BindOnce(
            [](bool* callback_run, bool* callback_result, bool allowed) {
              *callback_run = true;
              *callback_result = allowed;
            },
            callback_run, callback_result));

    EXPECT_EQ(mock_delegate_->classification_call_count(),
              initial_call_count + 1);
    EXPECT_EQ(mock_delegate_->last_navigation_id(), kTestNavigationId);
    EXPECT_EQ(mock_delegate_->last_url(), kTestUrl);
    EXPECT_TRUE(mock_delegate_->has_pending_callback());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<AwContentRestrictionManagerClient> client_;
  raw_ptr<MockContentRestrictionManagerClientDelegate> mock_delegate_;
};

TEST_F(AwContentRestrictionManagerClientTest, RequestClassification) {
  bool callback_run = false;
  bool callback_result = false;
  RequestContentClassification(&callback_run, &callback_result);
  ASSERT_FALSE(callback_run);

  mock_delegate_->TriggerClassificationResult(true);
  EXPECT_TRUE(callback_run);
  EXPECT_TRUE(callback_result);
}

TEST_F(AwContentRestrictionManagerClientTest, RequestClassificationTimeout) {
  bool callback_run = false;
  bool callback_result = false;
  RequestContentClassification(&callback_run, &callback_result);
  ASSERT_FALSE(callback_run);

  // Simulate classification time out.
  task_environment_.FastForwardBy(kTestContentClassificationTimeout +
                                  base::Seconds(1));
  EXPECT_TRUE(callback_run);
  EXPECT_TRUE(callback_result);

  // Classification result should be ignored following the timeout.
  callback_run = false;
  mock_delegate_->TriggerClassificationResult(false);
  EXPECT_FALSE(callback_run);
}

TEST_F(AwContentRestrictionManagerClientTest,
       SubsequentClassificationRequests) {
  bool callback1_run = false;
  bool callback1_result = false;
  RequestContentClassification(&callback1_run, &callback1_result);
  ASSERT_FALSE(callback1_run);

  bool callback2_run = false;
  bool callback2_result = false;
  RequestContentClassification(&callback2_run, &callback2_result);
  ASSERT_FALSE(callback2_run);

  // Verify that the latest callback is triggered and the previous one is
  // ignored.
  mock_delegate_->TriggerClassificationResult(true);
  EXPECT_FALSE(callback1_run);
  EXPECT_FALSE(callback1_result);
  EXPECT_TRUE(callback2_run);
  EXPECT_TRUE(callback2_result);
}

TEST_F(AwContentRestrictionManagerClientTest,
       SubsequentClassificationRequestsTimeout) {
  bool callback1_run = false;
  bool callback1_result = false;
  RequestContentClassification(&callback1_run, &callback1_result);
  ASSERT_FALSE(callback1_run);

  bool callback2_run = false;
  bool callback2_result = false;
  RequestContentClassification(&callback2_run, &callback2_result);
  ASSERT_FALSE(callback2_run);

  // Verify that the latest callback is triggered on timeouts and the previous
  // one is ignored.
  task_environment_.FastForwardBy(kTestContentClassificationTimeout +
                                  base::Seconds(1));
  EXPECT_FALSE(callback1_run);
  EXPECT_TRUE(callback2_run);
  EXPECT_TRUE(callback2_result);
}

}  // namespace
}  // namespace android_webview
