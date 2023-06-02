// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/webapk/webapk_icon_hasher.h"

#include <limits>
#include <set>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/manifest_icon_downloader.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/features.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"

// Browser tests for WebApkIconHasher.
class WebApkIconHasherBrowserTest : public PlatformBrowserTest {
 public:
  WebApkIconHasherBrowserTest()
      : http_server_(net::EmbeddedTestServer::TYPE_HTTP) {
    scoped_feature_list_.InitAndEnableFeature(
        net::features::kSplitCacheByNetworkIsolationKey);
  }

  WebApkIconHasherBrowserTest(const WebApkIconHasherBrowserTest&) = delete;
  WebApkIconHasherBrowserTest& operator=(const WebApkIconHasherBrowserTest&) =
      delete;

  ~WebApkIconHasherBrowserTest() override = default;

  void SetUpOnMainThread() override {
    http_server_.RegisterRequestHandler(base::BindRepeating(
        &WebApkIconHasherBrowserTest::OnResourceLoad, base::Unretained(this)));
    http_server_.ServeFilesFromSourceDirectory("chrome/test/data/banners");
    ASSERT_TRUE(http_server_.Start());

    ASSERT_TRUE(content::NavigateToURL(
        GetActiveWebContents(),
        http_server_.GetURL("/no_manifest_test_page.html")));

    PlatformBrowserTest::SetUpOnMainThread();
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  std::unique_ptr<net::test_server::HttpResponse> OnResourceLoad(
      const net::test_server::HttpRequest& request) {
    network_requests_.insert(request.GetURL());
    return nullptr;
  }

  net::EmbeddedTestServer http_server_;
  std::set<GURL> network_requests_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

namespace {

void OnDownloadedManifestIcon(base::OnceClosure callback,
                              const SkBitmap& unused_icon) {
  std::move(callback).Run();
}

void OnGotMurmur2Hash(
    base::OnceClosure callback,
    absl::optional<std::map<std::string, webapps::WebApkIconHasher::Icon>>
        hashes) {
  std::move(callback).Run();
}

}  // anonymous namespace

// Checks that WebApkIconHasher fetches the icon cached in the HTTP cache by
// ManifestIconDownloader.
//
// Disabled due to flakiness. https://crbug.com/1111439
IN_PROC_BROWSER_TEST_F(WebApkIconHasherBrowserTest,
                       DISABLED_HasherUsesIconFromCache) {
  const GURL kIconUrl = http_server_.GetURL("/launcher-icon-max-age.png");

  content::WebContents* web_contents = GetActiveWebContents();

  {
    base::RunLoop run_loop;
    content::ManifestIconDownloader::Download(
        web_contents, kIconUrl, 0, 0, std::numeric_limits<int>::max(),
        base::BindOnce(&OnDownloadedManifestIcon, run_loop.QuitClosure()),
        false /* square_only */);
    run_loop.Run();
  }

  ASSERT_TRUE(network_requests_.count(kIconUrl));
  network_requests_.clear();

  {
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
        web_contents->GetBrowserContext()
            ->GetDefaultStoragePartition()
            ->GetURLLoaderFactoryForBrowserProcess();

    base::RunLoop run_loop;
    webapps::WebApkIconHasher::DownloadAndComputeMurmur2Hash(
        url_loader_factory.get(), web_contents->GetWeakPtr(),
        url::Origin::Create(kIconUrl), {webapps::WebappIcon(kIconUrl)},
        base::BindOnce(&OnGotMurmur2Hash, run_loop.QuitClosure()));
    run_loop.Run();
  }

  EXPECT_FALSE(network_requests_.count(kIconUrl));
}
