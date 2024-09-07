// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "chrome/browser/screen_ai/screen_ai_service_router.h"
#include "chrome/browser/screen_ai/screen_ai_service_router_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/mock_host_resolver.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/screen_ai/buildflags/buildflags.h"
#include "services/screen_ai/public/cpp/utilities.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_features.mojom-features.h"

namespace {

#if !BUILDFLAG(USE_FAKE_SCREEN_AI)

const constexpr char* kTestPageRelativeURL =
    "/main_content_extraction/sidebar_and_main_content.html";

// It is expected that the extracted main content from the above url would have
// the text below.
// Since the output is generated using a model that is sensitive to updates and
// rendering detailts, we cannot ensure the entire generated text is a fixed
// string, but at least this longer paragraph is expected to be in it.
const constexpr char* kExpectedText =
    "Sometimes I think the surest sign that intelligent life exists elsewhere "
    "in the universe is that none of it has tried to contact us.";

// Looks in to the given `ids` of the `tree_nodes` and returns true if either of
// them or their children have `kExpectedText`.
bool HasExpectedText(const std::vector<ui::AXNodeData>& tree_nodes,
                     std::vector<ui::AXNodeID> ids) {
  std::map<ui::AXNodeID, uint32_t> id_to_index;
  for (uint32_t i = 0; i < tree_nodes.size(); i++) {
    id_to_index[tree_nodes[i].id] = i;
  }

  while (!ids.empty()) {
    const ui::AXNodeData& node = tree_nodes[id_to_index[ids.back()]];
    ids.pop_back();

    if (node.HasStringAttribute(ax::mojom::StringAttribute::kName)) {
      if (node.GetStringAttribute(ax::mojom::StringAttribute::kName) ==
          kExpectedText) {
        return true;
      }
    } else {
      ids.insert(ids.end(), node.child_ids.begin(), node.child_ids.end());
    }
  }

  return false;
}

#endif

}  // namespace

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

    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
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

  ui::AXTreeUpdate DistillPage(const std::string& relative_url) {
    GURL page = embedded_test_server()->GetURL(relative_url);
    ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), page,
                                                              1);
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(web_contents->GetURL(), page);

    base::test::TestFuture<ui::AXTreeUpdate&> future;
    web_contents->RequestAXTreeSnapshot(
        future.GetCallback(), ui::kAXModeComplete,
        /* max_nodes= */ 0,
        /* timeout= */ {}, content::WebContents::AXTreeSnapshotPolicy::kAll);
    EXPECT_TRUE(future.Wait());
    return future.Get();
  }

  void ExtractMainContent(
      const ui::AXTreeUpdate& ax_tree_update,
      base::OnceCallback<void(const std::vector<int32_t>&)> callback) {
    main_content_extractor_->ExtractMainContent(
        ax_tree_update, ukm::kInvalidSourceId, std::move(callback));
  }

  void ExtractMainContent(const ui::AXTreeUpdate& ax_tree_update,
                          std::vector<ui::AXNodeID>& main_content_ids) {
    base::test::TestFuture<const std::vector<ui::AXNodeID>&> future;
    main_content_extractor_->ExtractMainContent(
        ax_tree_update, ukm::kInvalidSourceId, future.GetCallback());
    ASSERT_TRUE(future.Wait()) << "Main content was not received.";
    main_content_ids = future.Get();
  }

 private:
  mojo::Remote<screen_ai::mojom::Screen2xMainContentExtractor>
      main_content_extractor_;
  base::test::ScopedFeatureList feature_list_;
};

// Tests that calling main content extraction without content gets replied.
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

  std::vector<ui::AXNodeID> main_content_ids;
  ExtractMainContent(empty_tree, main_content_ids);

  ASSERT_EQ(0u, main_content_ids.size());
}

// Fake library always returns empty.
#if !BUILDFLAG(USE_FAKE_SCREEN_AI)

// Tests main content extraction on a simple page with content.
IN_PROC_BROWSER_TEST_F(MainContentExtractionTest, RequestWithContent) {
  base::HistogramTester histograms;

  Connect();

  ui::AXTreeUpdate tree_update = DistillPage(kTestPageRelativeURL);
  ASSERT_FALSE(tree_update.nodes.empty());

  std::vector<ui::AXNodeID> main_content_ids;
  ExtractMainContent(tree_update, main_content_ids);
  ASSERT_FALSE(main_content_ids.empty());

  ASSERT_TRUE(HasExpectedText(tree_update.nodes, main_content_ids));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histograms.ExpectTotalCount(
      "Accessibility.ScreenAI.MainContentExtraction.Successful", 1);
  histograms.ExpectBucketCount(
      "Accessibility.ScreenAI.MainContentExtraction.Successful", true, 1);
}

// Test requesting several extractions without waiting for the previous ones to
// finish.
IN_PROC_BROWSER_TEST_F(MainContentExtractionTest, MultipleRequests) {
  Connect();

  ui::AXTreeUpdate tree_update = DistillPage(kTestPageRelativeURL);
  ASSERT_FALSE(tree_update.nodes.empty());

  constexpr uint32_t kRequestsCount = 3;
  std::vector<ui::AXNodeID> main_content_ids[kRequestsCount];
  base::test::TestFuture<const std::vector<ui::AXNodeID>&>
      futures[kRequestsCount];

  for (uint32_t i = 0; i < kRequestsCount; i++) {
    ExtractMainContent(tree_update, futures[i].GetCallback());
  }

  for (uint32_t i = 0; i < kRequestsCount; i++) {
    ASSERT_TRUE(futures[i].Wait())
        << "Main content was not received for request " << i;

    ASSERT_TRUE(HasExpectedText(tree_update.nodes, futures[i].Get()))
        << "Unexpected result for request " << i;
  }
}
#endif  //! BUILDFLAG(USE_FAKE_SCREEN_AI)

}  // namespace screen_ai
