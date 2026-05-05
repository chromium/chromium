// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_features.h"
#include "net/base/features.h"
#include "net/base/isolation_info.h"
#include "net/base/schemeful_site.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace extensions {

// Browser tests that verify the MIME handler frame and its descendants get
// the extension origin as the partition root for IsolationInfo and
// StorageKey.
class MimeHandlerStoragePartitioningBrowserTest : public ExtensionApiTest {
 public:
  MimeHandlerStoragePartitioningBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{extensions_features::kApiMimeHandler},
        /*disabled_features=*/{});
  }

 protected:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    const base::FilePath test_data_dir =
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA);
    embedded_test_server()->ServeFilesFromDirectory(
        test_data_dir.AppendASCII("pdf"));
    // Also serves /title1.html for the descendant-iframe tests to navigate
    // a non-extension child of the MIME handler frame.
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
    ASSERT_TRUE(StartEmbeddedTestServer());
  }

  // Loads the generic_mime_handler test extension and returns it.
  const Extension* LoadMimeHandlerExtension() {
    return LoadExtension(test_data_dir_.AppendASCII("generic_mime_handler"));
  }

  // Opens /test.pdf and blocks until the MIME handler page fires
  // chrome.test.succeed(). Wraps the parent's `OpenTestURL`.
  bool OpenPdfAndWaitForHandler() {
    return OpenTestURL(embedded_test_server()->GetURL("/test.pdf"),
                       /*open_in_incognito=*/false);
  }

  // Finds the first chrome-extension:// frame in the active tab.
  content::RenderFrameHost* FindExtensionFrame() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::RenderFrameHost* result = nullptr;
    web_contents->ForEachRenderFrameHost([&](content::RenderFrameHost* rfh) {
      if (rfh->GetLastCommittedOrigin().scheme() == kExtensionScheme) {
        result = rfh;
      }
    });
    return result;
  }

  // Appends an iframe under `parent_frame` pointing at `url` and waits for
  // it to load. Returns the descendant `RenderFrameHost`.
  content::RenderFrameHost* CreateDescendantFrame(
      content::RenderFrameHost* parent_frame,
      const GURL& url) {
    constexpr char kAppendFrameScript[] = R"(
        new Promise(resolve => {
          const f = document.createElement('iframe');
          f.src = $1;
          f.onload = () => resolve(true);
          document.body.appendChild(f);
        });
        )";
    EXPECT_EQ(true,
              content::EvalJs(parent_frame,
                              content::JsReplace(kAppendFrameScript, url)));
    content::RenderFrameHost* child = content::ChildFrameAt(parent_frame, 0);
    if (!child) {
      return nullptr;
    }
    EXPECT_EQ(url, child->GetLastCommittedURL());
    return child;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that the extension frame's IsolationInfo treats the extension as
// the top-level site: top_frame_origin matches the extension origin and
// site_for_cookies is non-null (enabling SameSite cookie delivery).
IN_PROC_BROWSER_TEST_F(MimeHandlerStoragePartitioningBrowserTest,
                       IsolationInfoPartitionedByExtension) {
  const Extension* extension = LoadMimeHandlerExtension();
  ASSERT_TRUE(extension);
  ASSERT_TRUE(OpenPdfAndWaitForHandler());

  content::RenderFrameHost* extension_frame = FindExtensionFrame();
  ASSERT_TRUE(extension_frame)
      << "No chrome-extension:// frame found in the frame tree";

  const net::IsolationInfo& isolation_info =
      extension_frame->GetIsolationInfoForSubresources();

  // The extension frame should see itself as top-level for network
  // partitioning purposes, not the embedder page.
  EXPECT_EQ(extension_frame->GetLastCommittedOrigin(),
            isolation_info.top_frame_origin());

  // Non-null site_for_cookies is required for SameSite=Lax/Strict cookies
  // to be sent with subresource requests from the extension frame.
  EXPECT_FALSE(isolation_info.site_for_cookies().IsNull());
}

// Verifies that a non-extension descendant frame inside the MIME handler
// subtree also sees the extension as the effective top-level for
// IsolationInfo. Without this, descendant subresource fetches would hit
// the embedder's site for SameSite cookie purposes instead of the
// extension's, breaking SameSite delivery for hosts that the extension
// has permissions for.
IN_PROC_BROWSER_TEST_F(MimeHandlerStoragePartitioningBrowserTest,
                       DescendantIsolationInfoPartitionedByExtension) {
  const Extension* extension = LoadMimeHandlerExtension();
  ASSERT_TRUE(extension);
  ASSERT_TRUE(OpenPdfAndWaitForHandler());

  content::RenderFrameHost* extension_frame = FindExtensionFrame();
  ASSERT_TRUE(extension_frame);

  GURL descendant_url = embedded_test_server()->GetURL("/title1.html");
  content::RenderFrameHost* descendant =
      CreateDescendantFrame(extension_frame, descendant_url);
  ASSERT_TRUE(descendant);
  ASSERT_NE(kExtensionScheme, descendant->GetLastCommittedOrigin().scheme());

  const net::IsolationInfo& isolation_info =
      descendant->GetIsolationInfoForSubresources();

  EXPECT_EQ(extension_frame->GetLastCommittedOrigin(),
            isolation_info.top_frame_origin());

  EXPECT_FALSE(isolation_info.site_for_cookies().IsNull());
}

// Subfixture that opts in to third-party storage partitioning for the
// descendant StorageKey assertion. Without this feature the descendant
// gets a first-party StorageKey by default and the override is moot, so
// the flag belongs only on this test rather than the parent fixture.
class MimeHandlerStorageKeyPartitioningBrowserTest
    : public MimeHandlerStoragePartitioningBrowserTest {
 public:
  MimeHandlerStorageKeyPartitioningBrowserTest() {
    partitioning_feature_list_.InitAndEnableFeature(
        net::features::kThirdPartyStoragePartitioning);
  }

 private:
  base::test::ScopedFeatureList partitioning_feature_list_;
};

// Verifies that a non-extension descendant frame inside the MIME handler
// subtree gets a StorageKey whose top-level site is the extension. This
// matches the IsolationInfo override and keeps storage partitions
// consistent across the network and storage stacks.
IN_PROC_BROWSER_TEST_F(MimeHandlerStorageKeyPartitioningBrowserTest,
                       DescendantStorageKeyPartitionedByExtension) {
  const Extension* extension = LoadMimeHandlerExtension();
  ASSERT_TRUE(extension);
  ASSERT_TRUE(OpenPdfAndWaitForHandler());

  content::RenderFrameHost* extension_frame = FindExtensionFrame();
  ASSERT_TRUE(extension_frame);

  GURL descendant_url = embedded_test_server()->GetURL("/title1.html");
  content::RenderFrameHost* descendant =
      CreateDescendantFrame(extension_frame, descendant_url);
  ASSERT_TRUE(descendant);
  ASSERT_NE(kExtensionScheme, descendant->GetLastCommittedOrigin().scheme());

  const blink::StorageKey& storage_key = descendant->GetStorageKey();

  EXPECT_EQ(net::SchemefulSite(extension_frame->GetLastCommittedOrigin()),
            storage_key.top_level_site());
  EXPECT_EQ(descendant->GetLastCommittedOrigin(), storage_key.origin());
}

}  // namespace extensions
