// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_context.h"

#include "android_webview/browser/aw_browser_context_store.h"
#include "android_webview/browser/aw_browser_process.h"
#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/browser/aw_feature_list_creator.h"
#include "android_webview/browser/network_service/aw_network_change_notifier_factory.h"
#include "android_webview/common/aw_features.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "components/visitedlink/browser/partitioned_visitedlink_writer.h"
#include "components/visitedlink/browser/visitedlink_writer.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_content_client_initializer.h"
#include "mojo/core/embedder/embedder.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

class AwBrowserContextTest : public testing::Test {
 protected:
  // Runs before the first test
  static void SetUpTestSuite() {
    net::NetworkChangeNotifier::SetFactory(
        new AwNetworkChangeNotifierFactory());
  }

  void SetUp() override {
    mojo::core::Init();
    test_content_client_initializer_ =
        new content::TestContentClientInitializer();

    AwFeatureListCreator* aw_feature_list_creator = new AwFeatureListCreator();
    aw_feature_list_creator->CreateLocalState();
    AwContentBrowserClient* aw_content_browser_client =
        new AwContentBrowserClient(aw_feature_list_creator);
    browser_process_ = new AwBrowserProcess(aw_content_browser_client);
  }

  void TearDown() override {
    // Flush the network service thread to ensure all pending NetworkContexts
    // are fully destroyed before we delete TestContentClientInitializer
    // (which owns NetworkConnectionTracker).
    if (content::GetNetworkTaskRunner()) {
      base::RunLoop run_loop;
      content::GetNetworkTaskRunner()->PostTaskAndReply(
          FROM_HERE, base::DoNothing(), run_loop.QuitClosure());
      run_loop.Run();
    }
    delete test_content_client_initializer_;
    delete browser_process_;
  }

  visitedlink::VisitedLinkWriter* GetVisitedLinkWriter(
      AwBrowserContext& context) {
    return context.visitedlink_writer_.get();
  }

  visitedlink::PartitionedVisitedLinkWriter* GetPartitionedVisitedLinkWriter(
      AwBrowserContext& context) {
    return context.partitioned_visitedlink_writer_.get();
  }

  // Create the TestBrowserThreads.
  content::BrowserTaskEnvironment task_environment_;
  raw_ptr<content::TestContentClientInitializer>
      test_content_client_initializer_;
  raw_ptr<AwBrowserProcess> browser_process_;
};

TEST_F(AwBrowserContextTest, SetAllowedPrerenderingCount) {
  AwBrowserContext context(
      AwBrowserContextStore::kDefaultContextName,
      base::FilePath(AwBrowserContextStore::kDefaultContextPath),
      /*is_default=*/true);

  network::mojom::NetworkContextParams network_context_params;
  cert_verifier::mojom::CertVerifierCreationParams cert_verifier_params;
  context.ConfigureNetworkContextParams(
      false, base::FilePath(), &network_context_params, &cert_verifier_params);

  // Default value is 2.
  EXPECT_EQ(context.AllowedPrerenderingCount(),
            kDefaultAllowedPrerenderingCount);

  // Set to 1.
  context.SetAllowedPrerenderingCount(nullptr, 1);
  EXPECT_EQ(context.AllowedPrerenderingCount(), 1);

  // Set to 3 (max).
  context.SetAllowedPrerenderingCount(nullptr, 3);
  EXPECT_EQ(context.AllowedPrerenderingCount(), kMaxAllowedPrerenderingCount);

  // Set to 4 (should be capped at 3).
  context.SetAllowedPrerenderingCount(nullptr, 4);
  EXPECT_EQ(context.AllowedPrerenderingCount(), kMaxAllowedPrerenderingCount);

  // Clear the prerenders(should go back to default 2).
  context.ClearAllowedPrerenderingCount(nullptr);
  EXPECT_EQ(context.AllowedPrerenderingCount(),
            kDefaultAllowedPrerenderingCount);
}

TEST_F(AwBrowserContextTest, MigrateVisitedLinksDisabled) {
  // Default is disabled.
  AwBrowserContext context(
      AwBrowserContextStore::kDefaultContextName,
      base::FilePath(AwBrowserContextStore::kDefaultContextPath),
      /*is_default=*/true);

  EXPECT_TRUE(GetVisitedLinkWriter(context));
  EXPECT_FALSE(GetPartitionedVisitedLinkWriter(context));

  // Wait for the initial empty table build to complete.
  base::RunLoop run_loop;
  content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                               run_loop.QuitClosure());
  run_loop.Run();

  GURL url("https://google.com");
  context.AddVisitedURLs({url});

  ASSERT_EQ(GetVisitedLinkWriter(context)->GetUsedCount(), 1);
  ASSERT_TRUE(GetVisitedLinkWriter(context)->IsVisited(url));
}

TEST_F(AwBrowserContextTest, MigrateVisitedLinksEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kWebViewMigrateVisitedLinks);

  AwBrowserContext context(
      AwBrowserContextStore::kDefaultContextName,
      base::FilePath(AwBrowserContextStore::kDefaultContextPath),
      /*is_default=*/true);

  EXPECT_FALSE(GetVisitedLinkWriter(context));
  EXPECT_TRUE(GetPartitionedVisitedLinkWriter(context));

  GURL url("https://google.com");
  context.AddVisitedURLs({url});
  base::RunLoop run_loop;
  content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                               run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_EQ(GetPartitionedVisitedLinkWriter(context)->GetUsedCount(), 1);

  visitedlink::VisitedLinkCommon::Fingerprint expected_fp =
      visitedlink::VisitedLinkCommon::ComputePseudoPartitionedFingerprint(
          url.spec());
  ASSERT_TRUE(GetPartitionedVisitedLinkWriter(context)->IsVisited(expected_fp));
}

}  // namespace android_webview
