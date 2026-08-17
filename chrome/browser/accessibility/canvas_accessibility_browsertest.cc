// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/logging.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#include "net/dns/mock_host_resolver.h"
#include "services/screen_ai/buildflags/buildflags.h"
#include "services/screen_ai/public/cpp/utilities.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_features.mojom-features.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_update.h"

namespace {

using CanvasAccessibilityTestParams =
    std::tuple<bool /*is_ocr_available*/, features::CanvasAccessibilityMode>;

const ui::AXNodeData* FindFirstNodeWithRole(const ui::AXTreeUpdate& tree_update,
                                            ax::mojom::Role role) {
  for (const auto& node : tree_update.nodes) {
    if (node.role == role) {
      return &node;
    }
  }
  return nullptr;
}

class CanvasAccessibilityBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<CanvasAccessibilityTestParams> {
 public:
  CanvasAccessibilityBrowserTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    enabled_features.push_back({features::kScreenAITestMode, {}});
    enabled_features.push_back({ax::mojom::features::kScreenAIOCREnabled, {}});

    switch (GetCanvasAccessibilityMode()) {
      case features::CanvasAccessibilityMode::kDisabled:
        disabled_features.push_back(features::kAccessibilityCanvas);
        break;
      case features::CanvasAccessibilityMode::kBasic:
        enabled_features.push_back({features::kAccessibilityCanvas,
                                    {{"CanvasAccessibilityMode", "Basic"}}});
        break;
      case features::CanvasAccessibilityMode::kAdvanced:
        enabled_features.push_back({features::kAccessibilityCanvas,
                                    {{"CanvasAccessibilityMode", "Advanced"}}});
        break;
    }

    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
  }
  ~CanvasAccessibilityBrowserTest() override = default;

  bool IsOcrLibraryAvailable() const { return std::get<0>(GetParam()); }
  features::CanvasAccessibilityMode GetCanvasAccessibilityMode() const {
    return std::get<1>(GetParam());
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());

    content::BrowserAccessibilityState::GetInstance()
        ->SetActivationFromPlatformEnabled(true);

    if (IsOcrLibraryAvailable()) {
#if BUILDFLAG(ENABLE_SCREEN_AI_BROWSERTESTS)
      screen_ai::ScreenAIInstallState::GetInstance()->SetComponentFolder(
          screen_ai::GetComponentBinaryPathForTests().DirName());
#else
      NOTREACHED();
#endif
    }
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<content::ScopedAccessibilityModeOverride>
      scoped_accessibility_mode_;
};

IN_PROC_BROWSER_TEST_P(CanvasAccessibilityBrowserTest,
                       FillTextCanvasAnnotation) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ui::AXMode mode = ui::kAXModeComplete;
  mode.set_mode(ui::AXMode::kScreenReader, true);
  scoped_accessibility_mode_ =
      std::make_unique<content::ScopedAccessibilityModeOverride>(web_contents,
                                                                 mode);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/accessibility/canvas_filltext.html")));

  bool expect_filltext_annotation =
      (GetCanvasAccessibilityMode() !=
       features::CanvasAccessibilityMode::kDisabled);

  if (expect_filltext_annotation) {
    content::WaitForAccessibilityTreeToContainNodeWithName(web_contents,
                                                           "Hello Canvas");
  } else {
    ui::AXTreeUpdate tree_update =
        content::GetAccessibilityTreeSnapshot(web_contents);
    const ui::AXNodeData* canvas_node =
        FindFirstNodeWithRole(tree_update, ax::mojom::Role::kCanvas);
    if (canvas_node) {
      std::string annotation = canvas_node->GetStringAttribute(
          ax::mojom::StringAttribute::kCanvasAnnotation);
      EXPECT_TRUE(annotation.empty());
    }
  }
}

IN_PROC_BROWSER_TEST_P(CanvasAccessibilityBrowserTest, BitmapCanvasAnnotation) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ui::AXMode mode = ui::kAXModeComplete;
  mode.set_mode(ui::AXMode::kScreenReader, true);
  scoped_accessibility_mode_ =
      std::make_unique<content::ScopedAccessibilityModeOverride>(web_contents,
                                                                 mode);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/accessibility/canvas_bitmap.html")));

  content::TitleWatcher title_watcher(web_contents, u"loaded");
  ASSERT_EQ(title_watcher.WaitAndGetTitle(), u"loaded");

  bool expect_ocr_annotation = (GetCanvasAccessibilityMode() ==
                                features::CanvasAccessibilityMode::kAdvanced) &&
                               IsOcrLibraryAvailable();

  if (expect_ocr_annotation) {
    std::string annotation;
    EXPECT_TRUE(base::test::RunUntil([&]() {
      ui::AXTreeUpdate tree_update =
          content::GetAccessibilityTreeSnapshot(web_contents);
      const ui::AXNodeData* canvas_node =
          FindFirstNodeWithRole(tree_update, ax::mojom::Role::kCanvas);
      if (canvas_node) {
        annotation = canvas_node->GetStringAttribute(
            ax::mojom::StringAttribute::kCanvasAnnotation);
        if (!annotation.empty()) {
          return true;
        }
      }
      return false;
    }));
    EXPECT_EQ(annotation, "ABC\nDEF\nGHI\nJKL");
  } else {
    ui::AXTreeUpdate tree_update =
        content::GetAccessibilityTreeSnapshot(web_contents);
    const ui::AXNodeData* canvas_node =
        FindFirstNodeWithRole(tree_update, ax::mojom::Role::kCanvas);
    if (canvas_node) {
      std::string annotation = canvas_node->GetStringAttribute(
          ax::mojom::StringAttribute::kCanvasAnnotation);
      EXPECT_TRUE(annotation.empty());
    }
  }
}

IN_PROC_BROWSER_TEST_P(CanvasAccessibilityBrowserTest,
                       BitmapCanvasAnnotation_ImageUpdate) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ui::AXMode mode = ui::kAXModeComplete;
  mode.set_mode(ui::AXMode::kScreenReader, true);
  scoped_accessibility_mode_ =
      std::make_unique<content::ScopedAccessibilityModeOverride>(web_contents,
                                                                 mode);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/accessibility/canvas_bitmap_update.html")));

  content::TitleWatcher title_watcher(web_contents, u"loaded");
  ASSERT_EQ(title_watcher.WaitAndGetTitle(), u"loaded");

  bool expect_ocr_annotation = (GetCanvasAccessibilityMode() ==
                                features::CanvasAccessibilityMode::kAdvanced) &&
                               IsOcrLibraryAvailable();

  if (expect_ocr_annotation) {
    std::string annotation;
    EXPECT_TRUE(base::test::RunUntil([&]() {
      ui::AXTreeUpdate tree_update =
          content::GetAccessibilityTreeSnapshot(web_contents);
      const ui::AXNodeData* canvas_node =
          FindFirstNodeWithRole(tree_update, ax::mojom::Role::kCanvas);
      if (canvas_node) {
        annotation = canvas_node->GetStringAttribute(
            ax::mojom::StringAttribute::kCanvasAnnotation);
        if (!annotation.empty()) {
          return true;
        }
      }
      return false;
    }));
    EXPECT_EQ(annotation, "ABC\nDEF\nGHI\nJKL");

    content::TitleWatcher update_title_watcher(web_contents, u"updated");
    ASSERT_TRUE(content::ExecJs(web_contents, "changeImage();"));
    ASSERT_EQ(update_title_watcher.WaitAndGetTitle(), u"updated");

    std::string updated_annotation;
    EXPECT_TRUE(base::test::RunUntil([&]() {
      ui::AXTreeUpdate tree_update =
          content::GetAccessibilityTreeSnapshot(web_contents);
      const ui::AXNodeData* canvas_node =
          FindFirstNodeWithRole(tree_update, ax::mojom::Role::kCanvas);
      if (canvas_node) {
        updated_annotation = canvas_node->GetStringAttribute(
            ax::mojom::StringAttribute::kCanvasAnnotation);
        if (updated_annotation == "A") {
          return true;
        }
      }
      return false;
    }));
    EXPECT_EQ(updated_annotation, "A");
  } else {
    ui::AXTreeUpdate tree_update =
        content::GetAccessibilityTreeSnapshot(web_contents);
    const ui::AXNodeData* canvas_node =
        FindFirstNodeWithRole(tree_update, ax::mojom::Role::kCanvas);
    if (canvas_node) {
      std::string annotation = canvas_node->GetStringAttribute(
          ax::mojom::StringAttribute::kCanvasAnnotation);
      EXPECT_TRUE(annotation.empty());
    }
  }
}

IN_PROC_BROWSER_TEST_P(CanvasAccessibilityBrowserTest,
                       BitmapCanvasAnnotation_NonN32) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ui::AXMode mode = ui::kAXModeComplete;
  mode.set_mode(ui::AXMode::kScreenReader, true);
  scoped_accessibility_mode_ =
      std::make_unique<content::ScopedAccessibilityModeOverride>(web_contents,
                                                                 mode);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/accessibility/canvas_float16.html")));

  content::TitleWatcher title_watcher(web_contents, u"loaded");
  ASSERT_EQ(title_watcher.WaitAndGetTitle(), u"loaded");

  bool expect_ocr_annotation = (GetCanvasAccessibilityMode() ==
                                features::CanvasAccessibilityMode::kAdvanced) &&
                               IsOcrLibraryAvailable();

  if (expect_ocr_annotation) {
    std::string annotation;
    EXPECT_TRUE(base::test::RunUntil([&]() {
      ui::AXTreeUpdate tree_update =
          content::GetAccessibilityTreeSnapshot(web_contents);
      const ui::AXNodeData* canvas_node =
          FindFirstNodeWithRole(tree_update, ax::mojom::Role::kCanvas);
      if (canvas_node) {
        annotation = canvas_node->GetStringAttribute(
            ax::mojom::StringAttribute::kCanvasAnnotation);
        if (!annotation.empty()) {
          return true;
        }
      }
      return false;
    }));
    EXPECT_EQ(annotation, "ABC\nDEF\nGHI\nJKL");
  } else {
    ui::AXTreeUpdate tree_update =
        content::GetAccessibilityTreeSnapshot(web_contents);
    const ui::AXNodeData* canvas_node =
        FindFirstNodeWithRole(tree_update, ax::mojom::Role::kCanvas);
    if (canvas_node) {
      std::string annotation = canvas_node->GetStringAttribute(
          ax::mojom::StringAttribute::kCanvasAnnotation);
      EXPECT_TRUE(annotation.empty());
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CanvasAccessibilityBrowserTest,
    ::testing::Combine(
// ENABLE_SCREEN_AI_BROWSERTESTS ensures the ChromeScreenAI library is available
// for this test configuration. USE_FAKE_SCREEN_AI indicates that the library is
// just a stub without actual implementation (which is used for sanitizer tests)
// and hence does not produce OCR results.
#if BUILDFLAG(ENABLE_SCREEN_AI_BROWSERTESTS) && !BUILDFLAG(USE_FAKE_SCREEN_AI)
        // Test both OCR service available and unavailable states when it is
        // available.
        ::testing::Values(true, false),
#else
        ::testing::Values(false),
#endif
        ::testing::Values(features::CanvasAccessibilityMode::kDisabled,
                          features::CanvasAccessibilityMode::kBasic,
                          features::CanvasAccessibilityMode::kAdvanced)),
    [](const ::testing::TestParamInfo<CanvasAccessibilityTestParams>& info) {
      std::string name;
      name += std::get<0>(info.param) ? "OcrAvailable_" : "OcrUnavailable_";
      switch (std::get<1>(info.param)) {
        case features::CanvasAccessibilityMode::kDisabled:
          name += "Disabled";
          break;
        case features::CanvasAccessibilityMode::kBasic:
          name += "Basic";
          break;
        case features::CanvasAccessibilityMode::kAdvanced:
          name += "Advanced";
          break;
      }
      return name;
    });

}  // namespace
