// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_alpha_rpc.h"

#include <memory>
#include <string>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/common/chrome_features.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace indigo {

namespace {

MATCHER_P2(IsAlphaGenerateError, error_type, error_message, "") {
  return ExplainMatchResult(error_type, arg.error_type, result_listener) &&
         ExplainMatchResult(error_message, arg.error_message, result_listener);
}

constexpr char kTestGenerateUrl[] = "https://placeholder.google.com/generate";
constexpr char kTestStatusUrl[] = "https://placeholder.google.com/status";

TEST(IndigoAlphaRpcTest, ParseAlphaGenerateResponse_Success) {
  const char kValidResponse[] = R"()]}'{"":
    [
      [null, [["https://example.com/"]]]
    ]
  })";
  base::expected<GURL, AlphaGenerateError> result =
      ParseAlphaGenerateResponse(kValidResponse);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), GURL("https://example.com/"));
}

TEST(IndigoAlphaRpcTest, ParseAlphaGenerateResponse_Failure) {
  const char kFailureResponse[] = R"()]}'{"":[
    null,
    null,
    [123, "error message"]
  ]})";
  base::expected<GURL, AlphaGenerateError> result =
      ParseAlphaGenerateResponse(kFailureResponse);
  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), IsAlphaGenerateError(123, "error message"));
}

TEST(IndigoAlphaRpcTest, ParseAlphaGenerateResponse_FailureSparse) {
  const char kFailureResponse[] = R"()]}'{"":[
    null,
    { "3": [123, "error message"] }
  ]})";
  base::expected<GURL, AlphaGenerateError> result =
      ParseAlphaGenerateResponse(kFailureResponse);
  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), IsAlphaGenerateError(123, "error message"));
}

TEST(IndigoAlphaRpcTest, ParseAlphaGenerateResponse_FailureNoMessage) {
  const char kFailureResponse[] = R"()]}'{"":[
    null,
    null,
    [456]
  ]})";
  base::expected<GURL, AlphaGenerateError> result =
      ParseAlphaGenerateResponse(kFailureResponse);
  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), IsAlphaGenerateError(456, "No message"));
}

TEST(IndigoAlphaRpcTest, ParseAlphaGenerateResponse_InvalidJson) {
  const char kInvalidJson[] = R"([)";
  base::expected<GURL, AlphaGenerateError> result =
      ParseAlphaGenerateResponse(kInvalidJson);
  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(),
              IsAlphaGenerateError(
                  -1, testing::StartsWith("Invalid JSON in response")));
}

TEST(IndigoAlphaRpcTest, ParseAlphaGenerateResponse_MalformedSuccessResponse) {
  const char kMalformedResponse[] = R"()]}'{"":[
    [null, []]
  ]})";
  base::expected<GURL, AlphaGenerateError> result =
      ParseAlphaGenerateResponse(kMalformedResponse);
  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(),
              IsAlphaGenerateError(-1, "Malformed success response"));
}

TEST(IndigoAlphaRpcTest, ParseAlphaGenerateResponse_MalformedFailureResponse) {
  const char kMalformedResponse[] = R"()]}'{"":[
    null,
    null,
    ["not an int", "message"]
  ]})";
  base::expected<GURL, AlphaGenerateError> result =
      ParseAlphaGenerateResponse(kMalformedResponse);
  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(),
              IsAlphaGenerateError(-1, "Malformed failure response"));
}

TEST(IndigoAlphaRpcTest, ParseAlphaGenerateResponse_MalformedResponse) {
  const char kMalformedResponse[] = R"()]}'{"":[]})";
  base::expected<GURL, AlphaGenerateError> result =
      ParseAlphaGenerateResponse(kMalformedResponse);
  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), IsAlphaGenerateError(-1, "Malformed response"));
}

TEST(IndigoAlphaRpcTest, ParseAlphaStatusResponse_Success) {
  const char kValidResponse[] = R"()]}'{"":[null, null, 3]})";
  auto result = ParseAlphaStatusResponse(kValidResponse);
  EXPECT_TRUE(result.has_value());
}

TEST(IndigoAlphaRpcTest, ParseAlphaStatusResponse_SuccessSparse) {
  const char kValidResponse[] = R"()]}'{"":[{"3": 3}]})";
  auto result = ParseAlphaStatusResponse(kValidResponse);
  EXPECT_TRUE(result.has_value());
}

TEST(IndigoAlphaRpcTest, ParseAlphaStatusResponse_WrongValue) {
  const char kResponse[] = R"()]}'{"":[null, null, 4]})";
  auto result = ParseAlphaStatusResponse(kResponse);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Unexpected value for status field: 4");
}

TEST(IndigoAlphaRpcTest, ParseAlphaStatusResponse_WrongValueSparse) {
  const char kResponse[] = R"()]}'{"":[{"3": 4}]})";
  auto result = ParseAlphaStatusResponse(kResponse);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Unexpected value for status field: 4");
}

TEST(IndigoAlphaRpcTest, ParseAlphaStatusResponse_NoStatus) {
  const char kResponse[] = R"()]}'{"":[]})";
  auto result = ParseAlphaStatusResponse(kResponse);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Status field not found in response.");
}

TEST(IndigoAlphaRpcTest, ParseAlphaStatusResponse_WrongType) {
  const char kResponse[] = R"()]}'{"":[null, null, "hello"]})";
  auto result = ParseAlphaStatusResponse(kResponse);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Unexpected type for status field");
}

TEST(IndigoAlphaRpcTest, ParseAlphaStatusResponse_InvalidJson) {
  const char kInvalidJson[] = R"([)";
  auto result = ParseAlphaStatusResponse(kInvalidJson);
  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), testing::StartsWith("Invalid JSON in response:"));
}

class IndigoAlphaRpcExecuteTest : public testing::Test {
 protected:
  IndigoAlphaRpcExecuteTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIndigo,
        {{features::kIndigoAlphaGenerateUrl.name, kTestGenerateUrl},
         {features::kIndigoAlphaStatusUrl.name, kTestStatusUrl}});
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_);
};

TEST_F(IndigoAlphaRpcExecuteTest, ExecuteAlphaGenerateRpcSuccess) {
  const char kValidResponse[] = R"()]}'{"":
    [
      [null, [["https://example.com/"]]]
    ]
  })";
  test_url_loader_factory_.AddResponse(kTestGenerateUrl, kValidResponse);

  base::test::TestFuture<base::expected<GURL, AlphaGenerateError>> result;
  ExecuteAlphaGenerateRpc(shared_url_loader_factory_.get(),
                          result.GetCallback());

  const auto& res = result.Get();
  ASSERT_TRUE(res.has_value());
  EXPECT_EQ(res.value(), GURL("https://example.com/"));
}

TEST_F(IndigoAlphaRpcExecuteTest, ExecuteAlphaGenerateRpcNetworkError) {
  test_url_loader_factory_.AddResponse(
      GURL(kTestGenerateUrl), network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::ERR_FAILED));

  base::test::TestFuture<base::expected<GURL, AlphaGenerateError>> result;
  ExecuteAlphaGenerateRpc(shared_url_loader_factory_.get(),
                          result.GetCallback());

  const auto& res = result.Get();
  ASSERT_FALSE(res.has_value());
  EXPECT_THAT(res.error(), IsAlphaGenerateError(-1, "Net error"));
}

TEST_F(IndigoAlphaRpcExecuteTest, ExecuteAlphaStatusRpcSuccess) {
  const char kValidResponse[] = R"()]}'{"":[null, null, 3]})";
  test_url_loader_factory_.AddResponse(kTestStatusUrl, kValidResponse);

  base::test::TestFuture<base::expected<void, std::string>> result;
  ExecuteAlphaStatusRpc(shared_url_loader_factory_.get(), result.GetCallback());

  const auto& res = result.Get();
  EXPECT_TRUE(res.has_value());
}

TEST_F(IndigoAlphaRpcExecuteTest, ExecuteAlphaStatusRpcNetworkError) {
  test_url_loader_factory_.AddResponse(
      GURL(kTestStatusUrl), network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::ERR_FAILED));

  base::test::TestFuture<base::expected<void, std::string>> result;
  ExecuteAlphaStatusRpc(shared_url_loader_factory_.get(), result.GetCallback());

  const auto& res = result.Get();
  ASSERT_FALSE(res.has_value());
  EXPECT_EQ(res.error(), "Net error");
}

}  // namespace
}  // namespace indigo
