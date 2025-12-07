// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/webapk/webapk_icons_hasher.h"

#include <limits>
#include <set>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/webapps/browser/android/shortcut_info.h"
#include "components/webapps/browser/android/webapp_icon.h"
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
#include "ui/gfx/image/image_unittest_util.h"
#include "url/origin.h"

// Browser tests for WebApkIconHasher.
class WebApkIconHasherBrowserTest : public AndroidBrowserTest {
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
    std::map<GURL, std::unique_ptr<webapps::WebappIcon>> icon) {
  std::move(callback).Run();
}

}  // anonymous namespace

// Checks that WebApkIconHasher fetches the icon cached in the HTTP cache by
// ManifestIconDownloader.
//
// Disabled due to flakiness. https://crbug.com/40709179
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

    webapps::ShortcutInfo shortcut_info(kIconUrl);
    shortcut_info.best_primary_icon_url = kIconUrl;

    base::RunLoop run_loop;
    webapps::WebApkIconsHasher hasher;
    hasher.DownloadAndComputeMurmur2Hash(
        url_loader_factory.get(), web_contents->GetWeakPtr(),
        url::Origin::Create(kIconUrl), shortcut_info,
        gfx::test::CreateBitmap(1, SK_ColorRED),
        base::BindOnce(&OnGotMurmur2Hash, run_loop.QuitClosure()));
    run_loop.Run();
  }

  EXPECT_FALSE(network_requests_.count(kIconUrl));
}
