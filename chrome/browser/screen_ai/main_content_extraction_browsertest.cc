// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "chrome/browser/screen_ai/screen_ai_service_router.h"
#include "chrome/browser/screen_ai/screen_ai_service_router_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/screen_ai/public/cpp/utilities.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_features.mojom-features.h"

namespace screen_ai {

class MainContentExtractionTest : public InProcessBrowserTest {
 public:
  MainContentExtractionTest() {
    feature_list_.InitWithFeatures(
        {
            features::kScreenAITestMode,
            features::kReadAnythingWithScreen2x,
            ax::mojom::features::kScreenAIMainContentExtractionEnabled,
        },
        {});
  }

  ~MainContentExtractionTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    ScreenAIInstallState::GetInstance()->SetComponentFolder(
        GetComponentBinaryPathForTests().DirName());
  }

  void Connect() {
    base::test::TestFuture<bool> future;
    ScreenAIServiceRouterFactory::GetForBrowserContext(browser()->profile())
        ->GetServiceStateAsync(
            ScreenAIServiceRouter::Service::kMainContentExtraction,
            future.GetCallback());
    ASSERT_TRUE(future.Wait()) << "Service state callback not called.";
    ASSERT_TRUE(future.Get<bool>()) << "Service initialization failed.";

    ScreenAIServiceRouterFactory::GetForBrowserContext(browser()->profile())
        ->BindMainContentExtractor(
            main_content_extractor_.BindNewPipeAndPassReceiver());
  }

  void ExtractMainContent(const ui::AXTreeUpdate& ax_tree_update,
                          std::vector<ui::AXNodeID>& main_content) {
    base::test::TestFuture<const std::vector<ui::AXNodeID>&> future;
    main_content_extractor_->ExtractMainContent(
        ax_tree_update, ukm::kInvalidSourceId, future.GetCallback());
    ASSERT_TRUE(future.Wait()) << "Main content was not received.";
    main_content = future.Get();
  }

 private:
  mojo::Remote<screen_ai::mojom::Screen2xMainContentExtractor>
      main_content_extractor_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(MainContentExtractionTest, EmptyInput) {
  Connect();

  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.AddBoolAttribute(ax::mojom::BoolAttribute::kNonAtomicTextFieldRoot,
                        true);
  root.relative_bounds.bounds = gfx::RectF(0, 0, 640, 480);

  ui::AXTreeUpdate empty_tree;
  empty_tree.root_id = root.id;
  empty_tree.nodes = {root};

  std::vector<ui::AXNodeID> main_content;
  ExtractMainContent(empty_tree, main_content);

  ASSERT_EQ(0u, main_content.size());
}

// TODO(crbug.com/41489544): Add tests with non-empty inputs and multiple calls.

}  // namespace screen_ai
