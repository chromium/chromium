// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_icon_hasher.h"

#include <string>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/test/browser_task_environment.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

// Murmur2 hash for |icon_url| in Success test.
const char kIconMurmur2Hash[] = "2081059568551351877";

// Runs WebApkIconHasher and blocks till the murmur2 hash is computed.
class WebApkIconHasherRunner {
 public:
  WebApkIconHasherRunner() {}
  ~WebApkIconHasherRunner() {}

  void Run(network::mojom::URLLoaderFactory* url_loader_factory,
           const GURL& icon_url) {
    WebApkIconHasher::DownloadAndComputeMurmur2HashWithTimeout(
        url_loader_factory, url::Origin::Create(icon_url), icon_url, 300,
        base::BindOnce(&WebApkIconHasherRunner::OnCompleted,
                       base::Unretained(this)));

    base::RunLoop run_loop;
    on_completed_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  std::map<std::string, WebApkIconHasher::Icon> RunMultiple(
      network::mojom::URLLoaderFactory* url_loader_factory,
      const std::set<GURL>& icon_urls) {
    std::map<std::string, WebApkIconHasher::Icon> result;
    base::RunLoop run_loop;
    WebApkIconHasher::DownloadAndComputeMurmur2Hash(
        url_loader_factory, url::Origin::Create(*icon_urls.begin()), icon_urls,
        base::BindLambdaForTesting(
            [&](base::Optional<std::map<std::string, WebApkIconHasher::Icon>>
                    hashes) {
              ASSERT_TRUE(hashes);
              result = std::move(*hashes);
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

  const WebApkIconHasher::Icon& icon() { return icon_; }

 private:
  void OnCompleted(WebApkIconHasher::Icon icon) {
    icon_ = std::move(icon);
    std::move(on_completed_callback_).Run();
  }

  // Fake factory that can be primed to return fake data.
  network::TestURLLoaderFactory test_url_loader_factory_;

  // Called once the Murmur2 hash is taken.
  base::OnceClosure on_completed_callback_;

  // Computed Murmur2 hash.
  WebApkIconHasher::Icon icon_;

  DISALLOW_COPY_AND_ASSIGN(WebApkIconHasherRunner);
};

}  // anonymous namespace

class WebApkIconHasherTest : public ::testing::Test {
 public:
  WebApkIconHasherTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}
  ~WebApkIconHasherTest() override {}

 protected:
  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebApkIconHasherTest);
};

TEST_F(WebApkIconHasherTest, Success) {
  std::string icon_url =
      "http://www.google.com/chrome/test/data/android/google.png";
  base::FilePath source_path;
  base::FilePath icon_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &source_path));
  icon_path = source_path.AppendASCII("chrome")
                  .AppendASCII("test")
                  .AppendASCII("data")
                  .AppendASCII("android")
                  .AppendASCII("google.png");
  std::string icon_data;
  ASSERT_TRUE(base::ReadFileToString(icon_path, &icon_data));
  test_url_loader_factory()->AddResponse(icon_url, icon_data);

  WebApkIconHasherRunner runner;
  runner.Run(test_url_loader_factory(), GURL(icon_url));
  EXPECT_EQ(kIconMurmur2Hash, runner.icon().hash);
  EXPECT_FALSE(runner.icon().unsafe_data.empty());
}

TEST_F(WebApkIconHasherTest, DataUri) {
  GURL icon_url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUA"
      "AAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO"
      "9TXL0Y4OHwAAAABJRU5ErkJggg==");
  WebApkIconHasherRunner runner;
  runner.Run(test_url_loader_factory(), icon_url);
  EXPECT_EQ("536500236142107998", runner.icon().hash);
  EXPECT_FALSE(runner.icon().unsafe_data.empty());
}

TEST_F(WebApkIconHasherTest, MultipleIconUrls) {
  std::string icon_url1_string =
      "http://www.google.com/chrome/test/data/android/google.png";
  base::FilePath source_path;
  base::FilePath icon_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &source_path));
  icon_path = source_path.AppendASCII("chrome")
                  .AppendASCII("test")
                  .AppendASCII("data")
                  .AppendASCII("android")
                  .AppendASCII("google.png");
  std::string icon_data;
  ASSERT_TRUE(base::ReadFileToString(icon_path, &icon_data));
  test_url_loader_factory()->AddResponse(icon_url1_string, icon_data);

  GURL icon_url1(icon_url1_string);
  GURL icon_url2(
      "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUA"
      "AAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO"
      "9TXL0Y4OHwAAAABJRU5ErkJggg==");

  WebApkIconHasherRunner runner;
  {
    auto result = runner.RunMultiple(test_url_loader_factory(), {icon_url1});
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[icon_url1.spec()].hash, kIconMurmur2Hash);
    EXPECT_FALSE(result[icon_url1.spec()].unsafe_data.empty());
  }

  {
    auto result =
        runner.RunMultiple(test_url_loader_factory(), {icon_url1, icon_url2});
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[icon_url1.spec()].hash, kIconMurmur2Hash);
    EXPECT_FALSE(result[icon_url1.spec()].unsafe_data.empty());

    EXPECT_EQ(result[icon_url2.spec()].hash, "536500236142107998");
    EXPECT_FALSE(result[icon_url2.spec()].unsafe_data.empty());
  }
}

TEST_F(WebApkIconHasherTest, DataUriInvalid) {
  GURL icon_url("data:image/png;base64");
  WebApkIconHasherRunner runner;
  runner.Run(test_url_loader_factory(), icon_url);
  EXPECT_TRUE(runner.icon().hash.empty());
  EXPECT_TRUE(runner.icon().unsafe_data.empty());
}

TEST_F(WebApkIconHasherTest, InvalidUrl) {
  GURL icon_url("http::google.com");
  WebApkIconHasherRunner runner;
  runner.Run(test_url_loader_factory(), icon_url);
  EXPECT_TRUE(runner.icon().hash.empty());
  EXPECT_TRUE(runner.icon().unsafe_data.empty());
}

TEST_F(WebApkIconHasherTest, DownloadTimedOut) {
  std::string icon_url = "http://www.google.com/timeout";
  WebApkIconHasherRunner runner;
  runner.Run(test_url_loader_factory(), GURL(icon_url));
  EXPECT_TRUE(runner.icon().hash.empty());
  EXPECT_TRUE(runner.icon().unsafe_data.empty());
}

// Test that the hash callback is called with an empty string if an HTTP error
// prevents the icon URL from being fetched.
TEST_F(WebApkIconHasherTest, HTTPError) {
  std::string icon_url = "http://www.google.com/404";
  auto head = network::mojom::URLResponseHead::New();
  std::string headers("HTTP/1.1 404 Not Found\nContent-type: text/html\n\n");
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  head->mime_type = "text/html";
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = 0;
  test_url_loader_factory()->AddResponse(GURL(icon_url), std::move(head), "",
                                         status);

  WebApkIconHasherRunner runner;
  runner.Run(test_url_loader_factory(), GURL(icon_url));
  EXPECT_TRUE(runner.icon().hash.empty());
  EXPECT_TRUE(runner.icon().unsafe_data.empty());
}
