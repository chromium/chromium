// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/process/kill.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/page_load_metrics/browser/test_metrics_web_contents_observer_embedder.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_constants.h"
#endif

using content::NavigationSimulator;

namespace page_load_metrics {

class MetricsWebContentsObserverTest : public ChromeRenderViewHostTestHarness {
 public:
  MetricsWebContentsObserverTest() = default;

  MetricsWebContentsObserverTest(const MetricsWebContentsObserverTest&) =
      delete;
  MetricsWebContentsObserverTest& operator=(
      const MetricsWebContentsObserverTest&) = delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    AttachObserver();
  }

  const std::vector<blink::UseCounterFeature>& observed_features() const {
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

  raw_ptr<TestMetricsWebContentsObserverEmbedder, DanglingUntriaged>
      embedder_interface_ = nullptr;
};

#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(MetricsWebContentsObserverTest,
       RecordFeatureUsageIgnoresChromeExtensionUpdates) {
  // Register our fake extension. The URL we access must be part of the
  // 'web_accessible_resources' for the network commit to work.
  auto manifest = base::Value::Dict()
                      .Set(extensions::manifest_keys::kVersion, "1.0.0.0")
                      .Set(extensions::manifest_keys::kName, "TestExtension")
                      .Set(extensions::manifest_keys::kManifestVersion, 2)
                      .Set("web_accessible_resources",
                           base::Value::List().Append("main.html"));
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder()
          .SetManifest(std::move(manifest))
          .SetID("mbflcebpggnecokmikipoihdbecnjfoj")
          .Build();
  ASSERT_TRUE(extension);

  extensions::TestExtensionSystem* extension_system =
      static_cast<extensions::TestExtensionSystem*>(
          extensions::ExtensionSystem::Get(profile()));
  extensions::ExtensionService* extension_service =
      extension_system->CreateExtensionService(
          base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
  extension_service->AddExtension(extension.get());

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  const GURL chrome_extension_url(
      "chrome-extension://mbflcebpggnecokmikipoihdbecnjfoj/main.html");
  web_contents_tester->NavigateAndCommit(GURL(chrome_extension_url));
  ASSERT_EQ(main_rfh()->GetLastCommittedURL().spec(),
            GURL(chrome_extension_url));

  MetricsWebContentsObserver::RecordFeatureUsage(
      main_rfh(), {blink::mojom::WebFeature::kHTMLMarqueeElement,
                   blink::mojom::WebFeature::kFormAttribute});

  // The features come from an extension source, so shouldn't be counted.
  EXPECT_EQ(observed_features().size(), 0ul);
}
#endif

}  // namespace page_load_metrics
