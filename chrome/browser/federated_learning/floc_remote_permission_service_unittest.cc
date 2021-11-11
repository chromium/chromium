// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/federated_learning/floc_remote_permission_service.h"

#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace federated_learning {

namespace {

// A testing floc remote permission service that does extra checks and creates a
// TestRequest instead of a normal request.
class TestingFlocRemotePermissionService : public FlocRemotePermissionService {
 public:
  explicit TestingFlocRemotePermissionService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : FlocRemotePermissionService(url_loader_factory),
        expected_url_(GURL()),
        expected_floc_permission_(false) {}

  TestingFlocRemotePermissionService(
      const TestingFlocRemotePermissionService&) = delete;
  TestingFlocRemotePermissionService& operator=(
      const TestingFlocRemotePermissionService&) = delete;

  ~TestingFlocRemotePermissionService() override = default;

  std::unique_ptr<FlocRemotePermissionService::Request> CreateRequest(
      const GURL& url,
      CreateRequestCallback callback,
      const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation)
      override;

  const std::string& GetExpectedPostData(
      FlocRemotePermissionService::Request* request);

  void QueryFlocPermissionCallback(bool success);

  void SetExpectedURL(const GURL& expected_url) {
    expected_url_ = expected_url;
  }

  void SetExpectedFlocPermission(bool expected_value) {
    expected_floc_permission_ = expected_value;
  }

  void SetResponseCodeOverride(int response_code_override) {
    response_code_override_ = response_code_override;
  }

  void SetResponseBodyOverride(const std::string& response_body_override) {
    response_body_override_ = response_body_override;
  }

 private:
  GURL expected_url_;
  bool expected_floc_permission_;
  absl::optional<int> response_code_override_;
  absl::optional<std::string> response_body_override_;
};

// A testing request class that allows expected values to be filled in.
class TestRequest : public FlocRemotePermissionService::Request {
 public:
  TestRequest(const GURL& url,
              FlocRemotePermissionService::CreateRequestCallback callback,
              int response_code,
              const std::string& response_body)
      : url_(url),
        callback_(std::move(callback)),
        response_code_(response_code),
        response_body_(response_body) {}

  TestRequest(const TestRequest&) = delete;
  TestRequest& operator=(const TestRequest&) = delete;

  ~TestRequest() override = default;

  // FlocRemotePermissionService::Request overrides
  int GetResponseCode() override { return response_code_; }
  const std::string& GetResponseBody() override { return response_body_; }

  void Start() override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&TestRequest::MimicReturnFromFetch,
                                  base::Unretained(this)));
  }

  void MimicReturnFromFetch() {
    // Mimic a successful fetch and return. We don't actually send out a request
    // in unittests.
    std::move(callback_).Run(this);
  }

  void SetResponseCode(int response_code) { response_code_ = response_code; }

  void SetResponseBody(const std::string& response_body) {
    response_body_ = response_body;
  }

 private:
  GURL url_;
  FlocRemotePermissionService::CreateRequestCallback callback_;
  int response_code_;
  std::string response_body_;
};

std::unique_ptr<FlocRemotePermissionService::Request>
TestingFlocRemotePermissionService::CreateRequest(
    const GURL& url,
    CreateRequestCallback callback,
    const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation) {
  EXPECT_EQ(expected_url_, url);
  auto request =
      std::make_unique<TestRequest>(url, std::move(callback), net::HTTP_OK, "");

  if (response_code_override_) {
    request->SetResponseCode(response_code_override_.value());
  }

  if (response_body_override_) {
    request->SetResponseBody(response_body_override_.value());
  }

  return std::move(request);
}

void TestingFlocRemotePermissionService::QueryFlocPermissionCallback(
    bool success) {
  EXPECT_EQ(expected_floc_permission_, success);
}

}  // namespace

// A test class used for testing the FlocRemotePermissionService class.
class FlocRemotePermissionServiceTest : public testing::Test {
 public:
  FlocRemotePermissionServiceTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        floc_remote_permission_service_(test_shared_loader_factory_) {}

  FlocRemotePermissionServiceTest(const FlocRemotePermissionServiceTest&) =
      delete;
  FlocRemotePermissionServiceTest& operator=(
      const FlocRemotePermissionServiceTest&) = delete;

  ~FlocRemotePermissionServiceTest() override = default;

  void TearDown() override {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  run_loop.QuitClosure());
    run_loop.Run();
  }

  TestingFlocRemotePermissionService* floc_remote_permission_service() {
    return &floc_remote_permission_service_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  TestingFlocRemotePermissionService floc_remote_permission_service_;
};

TEST_F(FlocRemotePermissionServiceTest, QueryFlocPermission) {
  floc_remote_permission_service()->SetExpectedURL(GURL(
      "https://adservice.google.com/settings/do_ad_settings_allow_floc_poc"));

  /* Success cases */
  floc_remote_permission_service()->SetExpectedFlocPermission(true);

  floc_remote_permission_service()->SetResponseCodeOverride(net::HTTP_OK);
  floc_remote_permission_service()->SetResponseBodyOverride(
      "[true, true, true]");

  floc_remote_permission_service()->QueryFlocPermission(
      base::BindOnce(
          &TestingFlocRemotePermissionService::QueryFlocPermissionCallback,
          base::Unretained(floc_remote_permission_service())),
      PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS);

  base::RunLoop().RunUntilIdle();

  /* Failure cases */
  floc_remote_permission_service()->SetExpectedFlocPermission(false);

  // Failure: disabled permission setting
  floc_remote_permission_service()->SetResponseBodyOverride(
      "[true, false, true]");
  floc_remote_permission_service()->QueryFlocPermission(
      base::BindOnce(
          &TestingFlocRemotePermissionService::QueryFlocPermissionCallback,
          base::Unretained(floc_remote_permission_service())),
      PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS);

  base::RunLoop().RunUntilIdle();

  // Failure: unexpected response format
  floc_remote_permission_service()->SetResponseBodyOverride("[1, 1, 1]");

  floc_remote_permission_service()->QueryFlocPermission(
      base::BindOnce(
          &TestingFlocRemotePermissionService::QueryFlocPermissionCallback,
          base::Unretained(floc_remote_permission_service())),
      PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS);

  base::RunLoop().RunUntilIdle();

  // Failure: 404 Not Found
  floc_remote_permission_service()->SetResponseBodyOverride(
      "[true, true, true]");
  floc_remote_permission_service()->SetResponseCodeOverride(
      net::HTTP_NOT_FOUND);

  floc_remote_permission_service()->QueryFlocPermission(
      base::BindOnce(
          &TestingFlocRemotePermissionService::QueryFlocPermissionCallback,
          base::Unretained(floc_remote_permission_service())),
      PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS);

  base::RunLoop().RunUntilIdle();
}

}  // namespace federated_learning
