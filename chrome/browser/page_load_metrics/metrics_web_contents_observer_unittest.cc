// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/process/kill.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/page_load_metrics/browser/test_metrics_web_contents_observer_embedder.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/value_builder.h"
#endif

using content::NavigationSimulator;

namespace page_load_metrics {

class MetricsWebContentsObserverTest : public ChromeRenderViewHostTestHarness {
 public:
  MetricsWebContentsObserverTest() = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    AttachObserver();
  }

  const std::vector<mojom::PageLoadFeatures>& observed_features() const {
    return embedder_interface_->observed_features();
  }

 protected:
  MetricsWebContentsObserver* observer() {
    return MetricsWebContentsObserver::FromWebContents(web_contents());
  }

 private:
  void AttachObserver() {
    auto embedder_interface =
        std::make_unique<TestMetricsWebContentsObserverEmbedder>();
    embedder_interface_ = embedder_interface.get();
    MetricsWebContentsObserver* observer =
        MetricsWebContentsObserver::CreateForWebContents(
            web_contents(), std::move(embedder_interface));
    observer->OnVisibilityChanged(content::Visibility::VISIBLE);
  }

  TestMetricsWebContentsObserverEmbedder* embedder_interface_;

  DISALLOW_COPY_AND_ASSIGN(MetricsWebContentsObserverTest);
};

#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(MetricsWebContentsObserverTest,
       RecordFeatureUsageIgnoresChromeExtensionUpdates) {
  // Register our fake extension. The URL we access must be part of the
  // 'web_accessible_resources' for the network commit to work.
  base::DictionaryValue manifest;
  manifest.SetString(extensions::manifest_keys::kVersion, "1.0.0.0");
  manifest.SetString(extensions::manifest_keys::kName, "TestExtension");
  manifest.SetInteger(extensions::manifest_keys::kManifestVersion, 2);
  manifest.Set("web_accessible_resources",
               extensions::ListBuilder().Append("main.html").Build());
  std::string error;
  scoped_refptr<extensions::Extension> extension =
      extensions::Extension::Create(
          base::FilePath(FILE_PATH_LITERAL("//no-such-file")),
          extensions::Manifest::INVALID_LOCATION, manifest,
          extensions::Extension::NO_FLAGS, "mbflcebpggnecokmikipoihdbecnjfoj",
          &error);
  ASSERT_TRUE(error.empty());
  extensions::ExtensionRegistry::Get(web_contents()->GetBrowserContext())
      ->AddEnabled(extension);

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  const GURL chrome_extension_url(
      "chrome-extension://mbflcebpggnecokmikipoihdbecnjfoj/main.html");
  web_contents_tester->NavigateAndCommit(GURL(chrome_extension_url));
  ASSERT_EQ(main_rfh()->GetLastCommittedURL().spec(),
            GURL(chrome_extension_url));

  std::vector<blink::mojom::WebFeature> web_features;
  web_features.push_back(blink::mojom::WebFeature::kHTMLMarqueeElement);
  web_features.push_back(blink::mojom::WebFeature::kFormAttribute);
  mojom::PageLoadFeatures features(web_features, {}, {});
  MetricsWebContentsObserver::RecordFeatureUsage(main_rfh(), features);

  // The features come from an extension source, so shouldn't be counted.
  EXPECT_EQ(observed_features().size(), 0ul);
}
#endif

}  // namespace page_load_metrics
