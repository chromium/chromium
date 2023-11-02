// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_navigation_throttle.h"

#include <memory>
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/content_client.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/identifiability_metrics.h"
#include "extensions/common/value_builder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metrics.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/common/privacy_budget/scoped_identifiability_test_sample_collector.h"
#include "url/gurl.h"

using content::NavigationThrottle;

namespace extensions {

namespace {

const char kAccessible[] = "accessible.html";
const char kPrivate[] = "private.html";
const char kAccessibleDir[] = "accessible_dir/*";
const char kAccessibleDirResource[] = "accessible_dir/foo.html";

class MockBrowserClient : public content::ContentBrowserClient {
 public:
  MockBrowserClient() {}
  ~MockBrowserClient() override {}

  // Only construct an ExtensionNavigationThrottle so that we can test it in
  // isolation.
  std::vector<std::unique_ptr<NavigationThrottle>> CreateThrottlesForNavigation(
      content::NavigationHandle* handle) override {
    std::vector<std::unique_ptr<NavigationThrottle>> throttles;
    throttles.push_back(std::make_unique<ExtensionNavigationThrottle>(handle));
    return throttles;
  }
};

}  // namespace

class ExtensionNavigationThrottleUnitTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ExtensionNavigationThrottleUnitTest() {}

  ExtensionNavigationThrottleUnitTest(
      const ExtensionNavigationThrottleUnitTest&) = delete;
  ExtensionNavigationThrottleUnitTest& operator=(
      const ExtensionNavigationThrottleUnitTest&) = delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    original_client_ = content::SetBrowserClientForTesting(&client_);

    // Constructs an extension with accessible.html and accessible_dir/* as
    // accessible resources.
    DictionaryBuilder manifest;
    manifest.Set("name", "ext")
        .Set("description", "something")
        .Set("version", "0.1")
        .Set("manifest_version", 2)
        .Set("web_accessible_resources",
             ListBuilder().Append(kAccessible).Append(kAccessibleDir).Build());
    extension_ = ExtensionBuilder()
                     .SetManifest(manifest.Build())
                     .SetID(crx_file::id_util::GenerateId("foo"))
                     .Build();
    ASSERT_TRUE(extension_);

    // Simulate installing/adding the extension.
    TestExtensionSystem* extension_system =
        static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile()));
    ExtensionService* extension_service =
        extension_system->CreateExtensionService(
            base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
    extension_service->AddExtension(extension_.get());
  }

  void TearDown() override {
    content::SetBrowserClientForTesting(original_client_);
    ChromeRenderViewHostTestHarness::TearDown();
  }

  // Checks that trying to navigate the given |host| to |extension_url| results
  // in the |expected_will_start_result|, and also that navigating to
  // |extension_url| via http redirect gives the same result.
  void CheckTestCase(
      content::RenderFrameHost* host,
      const GURL& extension_url,
      NavigationThrottle::ThrottleAction expected_will_start_result) {
    // First subtest: direct navigation to |extension_url|.
    content::MockNavigationHandle test_handle(extension_url, host);
    test_handle.set_initiator_origin(host->GetLastCommittedOrigin());
    test_handle.set_starting_site_instance(host->GetSiteInstance());
    auto throttle = std::make_unique<ExtensionNavigationThrottle>(&test_handle);

    {
      blink::test::ScopedIdentifiabilityTestSampleCollector metrics;

      EXPECT_EQ(expected_will_start_result,
                throttle->WillStartRequest().action())
          << extension_url;

      ExpectExtensionAccessResult(expected_will_start_result, extension_url,
                                  test_handle.GetNavigationId(),
                                  metrics.entries());
    }

    // Second subtest: server redirect to
    // |extension_url|.
    {
      blink::test::ScopedIdentifiabilityTestSampleCollector metrics;

      GURL http_url("https://example.com");
      test_handle.set_url(http_url);

      EXPECT_EQ(NavigationThrottle::PROCEED,
                throttle->WillStartRequest().action())
          << http_url;
      EXPECT_EQ(0u, metrics.entries().size());

      test_handle.set_url(extension_url);
      EXPECT_EQ(expected_will_start_result,
                throttle->WillRedirectRequest().action())
          << extension_url;
      ExpectExtensionAccessResult(expected_will_start_result, extension_url,
                                  test_handle.GetNavigationId(),
                                  metrics.entries());
    }
  }

  const Extension* extension() { return extension_.get(); }
  content::WebContentsTester* web_contents_tester() {
    return content::WebContentsTester::For(web_contents());
  }
  content::RenderFrameHostTester* render_frame_host_tester(
      content::RenderFrameHost* host) {
    return content::RenderFrameHostTester::For(host);
  }

  void ExpectExtensionAccessResult(
      NavigationThrottle::ThrottleAction expected_action,
      const GURL& extension_url,
      int64_t navigation_id,
      const std::vector<
          blink::test::ScopedIdentifiabilityTestSampleCollector::Entry>&
          entries) {
    // If throttle doesn't intervene, recording will be done by
    // ExtensionURLLoaderFactory, not the throttle.
    if (expected_action == NavigationThrottle::PROCEED) {
      EXPECT_EQ(0u, entries.size());
      return;
    }

    ExtensionResourceAccessResult expected;
    if (expected_action == NavigationThrottle::BLOCK_REQUEST) {
      expected = ExtensionResourceAccessResult::kFailure;
    } else if (expected_action == NavigationThrottle::CANCEL) {
      expected = ExtensionResourceAccessResult::kCancel;
    } else {
      ADD_FAILURE() << "Unhandled action:" << expected_action;
      return;
    }

    ukm::SourceId source_id = ukm::ConvertToSourceId(
        navigation_id, ukm::SourceIdObj::Type::NAVIGATION_ID);

    ASSERT_EQ(1u, entries.size());
    EXPECT_EQ(source_id, entries[0].source);
    ASSERT_EQ(1u, entries[0].metrics.size());
    EXPECT_EQ(blink::IdentifiableSurface::FromTypeAndToken(
                  blink::IdentifiableSurface::Type::kExtensionFileAccess,
                  base::as_bytes(base::make_span(
                      ExtensionSet::GetExtensionIdByURL(extension_url)))),
              entries[0].metrics[0].surface);
    EXPECT_EQ(blink::IdentifiableToken(expected), entries[0].metrics[0].value);
  }

 private:
  scoped_refptr<const Extension> extension_;
  MockBrowserClient client_;
  raw_ptr<content::ContentBrowserClient> original_client_ = nullptr;
};

// Tests the basic case of an external web page embedding an extension resource.
TEST_F(ExtensionNavigationThrottleUnitTest, ExternalWebPage) {
  web_contents_tester()->NavigateAndCommit(GURL("http://example.com"));
  content::RenderFrameHost* child =
      render_frame_host_tester(main_rfh())->AppendChild("child");

  // Only resources specified in web_accessible_resources should be allowed.
  CheckTestCase(child, extension()->GetResourceURL(kPrivate),
                NavigationThrottle::BLOCK_REQUEST);
  CheckTestCase(child, extension()->GetResourceURL(kAccessible),
                NavigationThrottle::PROCEED);
  CheckTestCase(child, extension()->GetResourceURL(kAccessibleDirResource),
                NavigationThrottle::PROCEED);
}

TEST_F(ExtensionNavigationThrottleUnitTest, CrossSiteFileSystemUrl) {
  web_contents_tester()->NavigateAndCommit(GURL("http://example.com"));

  GURL access_filesystem(base::StringPrintf(
      "filesystem:%s/",
      extension()->GetResourceURL(kAccessible).spec().c_str()));
  CheckTestCase(main_rfh(), access_filesystem, NavigationThrottle::CANCEL);
}

// Tests that the owning extension can access any of its resources.
TEST_F(ExtensionNavigationThrottleUnitTest, SameExtension) {
  web_contents_tester()->NavigateAndCommit(
      extension()->GetResourceURL("trusted.html"));
  content::RenderFrameHost* child =
      render_frame_host_tester(main_rfh())->AppendChild("child");

  // All resources should be allowed.
  CheckTestCase(child, extension()->GetResourceURL(kPrivate),
                NavigationThrottle::PROCEED);
  CheckTestCase(child, extension()->GetResourceURL(kAccessible),
                NavigationThrottle::PROCEED);
  CheckTestCase(child, extension()->GetResourceURL(kAccessibleDirResource),
                NavigationThrottle::PROCEED);
}

// Tests that requests to disabled or non-existent extensions are blocked.
TEST_F(ExtensionNavigationThrottleUnitTest, DisabledExtensionChildFrame) {
  web_contents_tester()->NavigateAndCommit(GURL("http://example.com"));
  content::RenderFrameHost* child =
      render_frame_host_tester(main_rfh())->AppendChild("child");

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());
  registry->RemoveEnabled(extension()->id());
  registry->AddDisabled(extension());

  // Since the extension is disabled, all requests should be blocked.
  CheckTestCase(child, extension()->GetResourceURL(kPrivate),
                NavigationThrottle::BLOCK_REQUEST);
  CheckTestCase(child, extension()->GetResourceURL(kAccessible),
                NavigationThrottle::BLOCK_REQUEST);
  CheckTestCase(child, extension()->GetResourceURL(kAccessibleDirResource),
                NavigationThrottle::BLOCK_REQUEST);

  std::string second_id = crx_file::id_util::GenerateId("bar");
  ASSERT_NE(second_id, extension()->id());
  GURL unknown_url(base::StringPrintf("chrome-extension://%s/accessible.html",
                                      second_id.c_str()));
  // Requests to non-existent extensions should be blocked.
  CheckTestCase(child, unknown_url, NavigationThrottle::BLOCK_REQUEST);

  // Test blob and filesystem URLs with disabled/unknown extensions.
  GURL disabled_blob(base::StringPrintf("blob:chrome-extension://%s/SOMEGUID",
                                        extension()->id().c_str()));
  GURL unknown_blob(base::StringPrintf("blob:chrome-extension://%s/SOMEGUID",
                                       second_id.c_str()));
  CheckTestCase(child, disabled_blob, NavigationThrottle::BLOCK_REQUEST);
  CheckTestCase(child, unknown_blob, NavigationThrottle::BLOCK_REQUEST);
  GURL disabled_filesystem(
      base::StringPrintf("filesystem:chrome-extension://%s/temporary/foo.html",
                         extension()->id().c_str()));
  GURL unknown_filesystem(
      base::StringPrintf("filesystem:chrome-extension://%s/temporary/foo.html",
                         second_id.c_str()));
  CheckTestCase(child, disabled_filesystem, NavigationThrottle::BLOCK_REQUEST);
  CheckTestCase(child, unknown_filesystem, NavigationThrottle::BLOCK_REQUEST);
}

// Tests that requests to disabled or non-existent extensions are blocked.
TEST_F(ExtensionNavigationThrottleUnitTest, DisabledExtensionMainFrame) {
  web_contents_tester()->NavigateAndCommit(GURL("http://example.com"));

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());
  registry->RemoveEnabled(extension()->id());
  registry->AddDisabled(extension());

  // Since the extension is disabled, all requests should be blocked.
  CheckTestCase(main_rfh(), extension()->GetResourceURL(kPrivate),
                NavigationThrottle::BLOCK_REQUEST);
  CheckTestCase(main_rfh(), extension()->GetResourceURL(kAccessible),
                NavigationThrottle::BLOCK_REQUEST);
  CheckTestCase(main_rfh(), extension()->GetResourceURL(kAccessibleDirResource),
                NavigationThrottle::BLOCK_REQUEST);

  std::string second_id = crx_file::id_util::GenerateId("bar");

  ASSERT_NE(second_id, extension()->id());
  GURL unknown_url(base::StringPrintf("chrome-extension://%s/accessible.html",
                                      second_id.c_str()));
  // Requests to non-existent extensions should be blocked.
  CheckTestCase(main_rfh(), unknown_url, NavigationThrottle::BLOCK_REQUEST);

  // Test blob and filesystem URLs with disabled/unknown extensions.
  GURL disabled_blob(base::StringPrintf("blob:chrome-extension://%s/SOMEGUID",
                                        extension()->id().c_str()));
  GURL unknown_blob(base::StringPrintf("blob:chrome-extension://%s/SOMEGUID",
                                       second_id.c_str()));
  CheckTestCase(main_rfh(), disabled_blob, NavigationThrottle::BLOCK_REQUEST);
  CheckTestCase(main_rfh(), unknown_blob, NavigationThrottle::BLOCK_REQUEST);
  GURL disabled_filesystem(
      base::StringPrintf("filesystem:chrome-extension://%s/temporary/foo.html",
                         extension()->id().c_str()));
  GURL unknown_filesystem(
      base::StringPrintf("filesystem:chrome-extension://%s/temporary/foo.html",
                         second_id.c_str()));
  CheckTestCase(main_rfh(), disabled_filesystem,
                NavigationThrottle::BLOCK_REQUEST);
  CheckTestCase(main_rfh(), unknown_filesystem,
                NavigationThrottle::BLOCK_REQUEST);
}

}  // namespace extensions
