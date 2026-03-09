// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_script_loader.h"

#include <string>
#include <string_view>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace indigo {

class IndigoScriptLoaderTest : public testing::Test {
 public:
  IndigoScriptLoaderTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        loader_(test_shared_loader_factory_) {}

 protected:
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  IndigoScriptLoader loader_;
};

TEST_F(IndigoScriptLoaderTest, LoadFromNetworkSuccess) {
  const std::string_view kUrl = "https://example.com/script.js";
  const std::string_view kContent = "console.log('hello');";

  test_url_loader_factory_.AddResponse(kUrl, kContent);

  base::test::TestFuture<std::optional<std::string>> future;
  loader_.Load(kUrl, future.GetCallback());
  EXPECT_EQ(future.Get(), kContent);
}

TEST_F(IndigoScriptLoaderTest, LoadFromNetworkFailure) {
  const std::string_view kUrl = "https://example.com/script.js";

  test_url_loader_factory_.AddResponse(kUrl, "", net::HTTP_NOT_FOUND);

  base::test::TestFuture<std::optional<std::string>> future;
  loader_.Load(kUrl, future.GetCallback());
  // SimpleURLLoader::DownloadToString returns nullopt on non-2xx responses by
  // default.
  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(IndigoScriptLoaderTest, LoadFromNetworkPlaintext) {
  const std::string_view kUrl = "http://example.com/script.js";
  const std::string_view kContent = "console.log('hello');";

  test_url_loader_factory_.AddResponse(kUrl, kContent);

  base::test::TestFuture<std::optional<std::string>> future;
  loader_.Load(kUrl, future.GetCallback());
  EXPECT_EQ(future.Get(), kContent);
}

TEST_F(IndigoScriptLoaderTest, LoadFromFileSuccess) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath file_path = temp_dir.GetPath().AppendASCII("script.js");
  const std::string_view kContent = "console.log('file hello');";
  ASSERT_TRUE(base::WriteFile(file_path, kContent));

  base::test::TestFuture<std::optional<std::string>> future;
  loader_.Load(file_path.AsUTF8Unsafe(), future.GetCallback());
  EXPECT_EQ(future.Get(), kContent);
}

TEST_F(IndigoScriptLoaderTest, LoadFromFileFailure) {
  base::test::TestFuture<std::optional<std::string>> future;
  loader_.Load("/non/existent/path.js", future.GetCallback());
  EXPECT_FALSE(future.Get().has_value());
}

}  // namespace indigo
