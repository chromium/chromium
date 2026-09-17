// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/request_header_integrity/chrome_companero_host.h"

#include <memory>
#include <string>
#include <utility>

#include "base/base_paths.h"
#include "base/task/task_traits.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/common/request_header_integrity/chrome_companero.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/http_request_headers.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace request_header_integrity {

class ChromeCompaneroHostTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ChromeCompaneroHostTest, BindReceiverAndGetHeaderNameAndValue) {
  auto host = std::make_unique<ChromeCompaneroHost>();

  mojo::Remote<mojom::ChromeCompanero> remote;
  host->BindReceiver(remote.BindNewPipeAndPassReceiver());

  base::test::TestFuture<network::mojom::HttpRequestHeaderKeyValuePairPtr>
      future;
  remote->GetHeaderNameAndValue(future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  EXPECT_FALSE(result->key.empty());
  EXPECT_FALSE(result->value.empty());
}

TEST_F(ChromeCompaneroHostTest, HostDirectGetHeaderNameAndValue) {
  auto host = std::make_unique<ChromeCompaneroHost>();

  base::test::TestFuture<network::mojom::HttpRequestHeaderKeyValuePairPtr>
      future;
  host->GetHeaderNameAndValue(future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result);
  EXPECT_FALSE(result->key.empty());
  EXPECT_FALSE(result->value.empty());
}

TEST_F(ChromeCompaneroHostTest, LibraryAbsentGracefulFailure) {
  // Override DIR_MODULE to an empty directory to simulate a missing dynamic
  // library, ensuring the host gracefully returns nullptr without crashing.
  base::ScopedPathOverride module_override(base::DIR_MODULE);

  auto host = std::make_unique<ChromeCompaneroHost>();

  mojo::Remote<mojom::ChromeCompanero> remote;
  host->BindReceiver(remote.BindNewPipeAndPassReceiver());

  base::test::TestFuture<network::mojom::HttpRequestHeaderKeyValuePairPtr>
      future;
  remote->GetHeaderNameAndValue(future.GetCallback());
  EXPECT_FALSE(future.Get());
}

// The param is read with GetWithoutCache() throughout these tests because
// BASE_FEATURE_ENUM_PARAM memoises the value process-wide on first read, which
// production wants (the host is constructed once) but which would make these
// assertions depend on test ordering.
TEST_F(ChromeCompaneroHostTest, TokenTaskPriorityDefaultsToUserVisible) {
  EXPECT_EQ(base::TaskPriority::USER_VISIBLE,
            kTokenTaskPriority.GetWithoutCache());
}

TEST_F(ChromeCompaneroHostTest, TokenTaskPriorityFallsBackOnUnknownValue) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kRequestHeaderIntegrityTokenPriority,
      {{"TokenTaskPriority", "nonsense"}});
  EXPECT_EQ(base::TaskPriority::USER_VISIBLE,
            kTokenTaskPriority.GetWithoutCache());
}

class ChromeCompaneroHostTaskPriorityTest
    : public ChromeCompaneroHostTest,
      public testing::WithParamInterface<base::TaskPriority> {};

// Every base::TaskPriority the param offers must be settable, and settable
// under base's own spelling for it. Driving both the param value and the test
// name off TaskPriorityToString() means the accepted values cannot drift away
// from base without this failing, and a failure names the offending priority.
TEST_P(ChromeCompaneroHostTaskPriorityTest, TokenTaskPriorityIsConfigurable) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kRequestHeaderIntegrityTokenPriority,
      {{"TokenTaskPriority", base::TaskPriorityToString(GetParam())}});
  EXPECT_EQ(GetParam(), kTokenTaskPriority.GetWithoutCache());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ChromeCompaneroHostTaskPriorityTest,
    testing::Values(base::TaskPriority::BEST_EFFORT,
                    base::TaskPriority::USER_VISIBLE,
                    base::TaskPriority::USER_BLOCKING),
    [](const testing::TestParamInfo<base::TaskPriority>& info) {
      return base::TaskPriorityToString(info.param);
    });

}  // namespace request_header_integrity
