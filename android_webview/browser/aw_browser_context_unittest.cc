// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_browser_context_store.h"
#include "android_webview/browser/aw_browser_process.h"
#include "android_webview/browser/aw_feature_list_creator.h"
#include "android_webview/browser/network_service/aw_network_change_notifier_factory.h"
#include "android_webview/common/aw_features.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/browser_context.h"
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
    browser_process_ = new AwBrowserProcess(aw_feature_list_creator);
  }

  void TearDown() override {
    // Drain the message queue before destroying
    // |test_content_client_initializer_|, otherwise a posted task may call
    // content::GetNetworkConnectionTracker() after
    // TestContentClientInitializer's destructor sets it to null.
    base::RunLoop().RunUntilIdle();
    delete test_content_client_initializer_;
    delete browser_process_;
  }

  // Create the TestBrowserThreads.
  content::BrowserTaskEnvironment task_environment_;
  raw_ptr<content::TestContentClientInitializer>
      test_content_client_initializer_;
  raw_ptr<AwBrowserProcess> browser_process_;
};

// Tests that constraints on trust for Symantec-issued certificates are not
// enforced for the NetworkContext, as it should behave like the Android system.
// TODO(crbug.com/40278955): Fix the flakiness and re-enable.
TEST_F(AwBrowserContextTest, DISABLED_SymantecPoliciesExempted) {
  AwBrowserContext context(
      AwBrowserContextStore::kDefaultContextName,
      base::FilePath(AwBrowserContextStore::kDefaultContextPath),
      /*is_default=*/true);
  network::mojom::NetworkContextParams network_context_params;
  cert_verifier::mojom::CertVerifierCreationParams cert_verifier_params;
  context.ConfigureNetworkContextParams(
      false, base::FilePath(), &network_context_params, &cert_verifier_params);

  ASSERT_TRUE(network_context_params.initial_ssl_config);
  ASSERT_TRUE(
      network_context_params.initial_ssl_config->symantec_enforcement_disabled);
}

// Tests that SHA-1 is still allowed for locally-installed trust anchors,
// including those in application manifests, as it should behave like
// the Android system.
TEST_F(AwBrowserContextTest, SHA1LocalAnchorsAllowed) {
  AwBrowserContext context(
      AwBrowserContextStore::kDefaultContextName,
      base::FilePath(AwBrowserContextStore::kDefaultContextPath),
      /*is_default=*/true);
  network::mojom::NetworkContextParams network_context_params;
  cert_verifier::mojom::CertVerifierCreationParams cert_verifier_params;
  context.ConfigureNetworkContextParams(
      false, base::FilePath(), &network_context_params, &cert_verifier_params);

  ASSERT_TRUE(network_context_params.initial_ssl_config);
  ASSERT_TRUE(
      network_context_params.initial_ssl_config->sha1_local_anchors_enabled);
}

}  // namespace android_webview
