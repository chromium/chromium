// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/request_header_integrity/chrome_companero_host.h"

#include <memory>
#include <string>
#include <utility>

#include "base/base_paths.h"
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

}  // namespace request_header_integrity
