// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <map>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/with_feature_override.h"
#include "base/threading/thread_restrictions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/pdf/pdf_extension_test_base.h"
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/ax_inspect_factory.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/context_menu_interceptor.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "pdf/pdf_features.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/screen_ai/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/context_menu_data/untrustworthy_context_menu_params.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_features.mojom-features.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/accessibility/platform/inspect/ax_api_type.h"
#include "ui/accessibility/platform/inspect/ax_inspect_scenario.h"
#include "ui/accessibility/platform/inspect/ax_inspect_test_helper.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Fake ScreenAI library returns empty results for all queries, so testing with
// it is not helpful.
#if BUILDFLAG(ENABLE_SCREEN_AI_BROWSERTESTS) && !BUILDFLAG(USE_FAKE_SCREEN_AI)
#define PDF_OCR_INTEGRATION_TEST_ENABLED
#endif

#if defined(PDF_OCR_INTEGRATION_TEST_ENABLED)
#include "base/scoped_observation.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "chrome/browser/screen_ai/screen_ai_service_router.h"
#include "chrome/browser/screen_ai/screen_ai_service_router_factory.h"
#include "components/strings/grit/components_strings.h"
#include "services/screen_ai/public/cpp/utilities.h"
#include "ui/base/l10n/l10n_util.h"
#endif  // defined(PDF_OCR_INTEGRATION_TEST_ENABLED)

namespace {

using ::content::WebContents;
using ::extensions::MimeHandlerViewGuest;

std::string DumpPdfAccessibilityTree(const ui::AXTreeUpdate& ax_tree,
                                     bool skip_status_subtree) {
  // Create a string representation of the tree starting with the kPdfRoot
  // object.
  std::string ax_tree_dump;
  std::map<int32_t, int> id_to_indentation;
  bool found_pdf_root = false;

  // Status node's subtree is in the form of:
  // pdfRoot -> banner -> status -> staticText.
  // If the subtree should be skipped, this variable keeps the ids of the
  // subtree nodes.
  std::set<ui::AXNodeID> status_subtree_ids;

  for (size_t i = 0; i < ax_tree.nodes.size(); i++) {
    const ui::AXNodeData& node = ax_tree.nodes[i];
    if (node.role == ax::mojom::Role::kPdfRoot) {
      found_pdf_root = true;
      if (skip_status_subtree) {
        if (i + 3 < ax_tree.nodes.size() &&
            ax_tree.nodes[i + 1].role == ax::mojom::Role::kBanner &&
            ax_tree.nodes[i + 2].role == ax::mojom::Role::kStatus &&
            ax_tree.nodes[i + 3].role == ax::mojom::Role::kStaticText) {
          status_subtree_ids = {ax_tree.nodes[i + 1].id,
                                ax_tree.nodes[i + 2].id,
                                ax_tree.nodes[i + 3].id};
        }
      }
    }
    if (!found_pdf_root) {
      continue;
    }

    // Exclude the status subtree from `ax_tree_dump` if they exist in the tree.
    // Tests don't expect them to be included in the dump.
    if (base::Contains(status_subtree_ids, node.id)) {
      continue;
    }

    auto indent_found = id_to_indentation.find(node.id);
    int indent = 0;
    if (indent_found != id_to_indentation.end()) {
      indent = indent_found->second;
    } else if (node.role != ax::mojom::Role::kPdfRoot) {
      // If this node has no indent and isn't the kPdfRoot object, finish dump
      // as this indicates the end of the PDF.
      break;
    }

    ax_tree_dump += std::string(2 * indent, ' ');
    ax_tree_dump += ui::ToString(node.role);

    std::string name =
        node.GetStringAttribute(ax::mojom::StringAttribute::kName);
    base::ReplaceChars(name, "\r\n", "", &name);
    if (node.role == ax::mojom::Role::kStaticText) {
      // OCR may detect and put trailing whitespace in `kStaticText`, so trim
      // it in that case.
      base::TrimWhitespaceASCII(name, base::TrimPositions::TRIM_TRAILING,
                                &name);
    }
    if (!name.empty()) {
      ax_tree_dump += " '" + name + "'";
    }
    ax_tree_dump += "\n";
    for (int32_t id : node.child_ids) {
      id_to_indentation[id] = indent + 1;
    }
  }

  EXPECT_TRUE(found_pdf_root);
  return ax_tree_dump;
}

constexpr char kExpectedPDFAXTree[] =
    "pdfRoot 'PDF document containing 3 pages'\n"
    "  region 'Page 1'\n"
    "    paragraph\n"
    "      staticText '1 First Section'\n"
    "        inlineTextBox '1 '\n"
    "        inlineTextBox 'First Section'\n"
    "    paragraph\n"
    "      staticText 'This is the first section.'\n"
    "        inlineTextBox 'This is the first section.'\n"
    "    paragraph\n"
    "      staticText '1'\n"
    "        inlineTextBox '1'\n"
    "  region 'Page 2'\n"
    "    paragraph\n"
    "      staticText '1.1 First Subsection'\n"
    "        inlineTextBox '1.1 '\n"
    "        inlineTextBox 'First Subsection'\n"
    "    paragraph\n"
    "      staticText 'This is the first subsection.'\n"
    "        inlineTextBox 'This is the first subsection.'\n"
    "    paragraph\n"
    "      staticText '2'\n"
    "        inlineTextBox '2'\n"
    "  region 'Page 3'\n"
    "    paragraph\n"
    "      staticText '2 Second Section'\n"
    "        inlineTextBox '2 '\n"
    "        inlineTextBox 'Second Section'\n"
    "    paragraph\n"
    "      staticText '3'\n"
    "        inlineTextBox '3'\n";

}  // namespace

// Using ASSERT_TRUE deliberately instead of ASSERT_EQ or ASSERT_STREQ
// in order to print a more readable message if the strings differ.
#define ASSERT_MULTILINE_STREQ(expected, actual)                 \
  ASSERT_TRUE(expected == actual) << "Expected:\n"               \
                                  << expected << "\n\nActual:\n" \
                                  << actual

class PDFExtensionAccessibilityTest : public PDFExtensionTestBase {
 public:
  PDFExtensionAccessibilityTest() = default;
  ~PDFExtensionAccessibilityTest() override = default;

 protected:
  std::vector<base::test::FeatureRef> GetDisabledFeatures() const override {
    std::vector<base::test::FeatureRef> disabled =
        PDFExtensionTestBase::GetDisabledFeatures();
    // PDF OCR should not be enabled in `PDFExtensionAccessibilityTest`. If a
    // new test class is derived from this class and needs to test PDF OCR,
    // make sure that `GetDisabledFeatures()` is overridden to exclude
    // `::features::kPdfOcr` from a list of disabled features.
    disabled.push_back(::features::kPdfOcr);
    return disabled;
  }

  ui::AXTreeUpdate GetAccessibilityTreeSnapshotForPdf(
      content::WebContents* web_contents) {
    content::FindAccessibilityNodeCriteria find_criteria;
    find_criteria.role = ax::mojom::Role::kPdfRoot;
    ui::AXPlatformNodeDelegate* pdf_root =
        content::FindAccessibilityNode(web_contents, find_criteria);
    ui::AXTreeID pdf_tree_id = pdf_root->GetTreeData().tree_id;
    EXPECT_NE(pdf_tree_id, ui::AXTreeIDUnknown());
    EXPECT_EQ(pdf_root->GetTreeData().focus_id, pdf_root->GetId());

    return content::GetAccessibilityTreeSnapshotFromId(pdf_tree_id);
  }

  void EnableScreenReader(bool enabled) {
    // Spoof a screen reader.
    if (enabled) {
      content::BrowserAccessibilityState::GetInstance()
          ->AddAccessibilityModeFlags(ui::AXMode::kScreenReader);
    } else {
      content::BrowserAccessibilityState::GetInstance()
          ->RemoveAccessibilityModeFlags(ui::AXMode::kScreenReader);
    }
  }
};

class PDFExtensionAccessibilityTestWithOopifOverride
    : public base::test::WithFeatureOverride,
      public PDFExtensionAccessibilityTest {
 public:
  PDFExtensionAccessibilityTestWithOopifOverride()
      : base::test::WithFeatureOverride(chrome_pdf::features::kPdfOopif) {}

  bool UseOopif() const override { return GetParam(); }
};

// The test is flaky on Mac: https://crbug.com/334099836.
#if BUILDFLAG(IS_MAC)
#define MAYBE_PdfAccessibility DISABLED_PdfAccessibility
#else
#define MAYBE_PdfAccessibility PdfAccessibility
#endif
IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTestWithOopifOverride,
                       MAYBE_PdfAccessibility) {
  content::ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);

  ASSERT_TRUE(
      LoadPdf(embedded_test_server()->GetURL("/pdf/test-bookmarks.pdf")));

  WaitForAccessibilityTreeToContainNodeWithName(GetActiveWebContents(),
                                                "1 First Section\r\n");
  ui::AXTreeUpdate ax_tree =
      GetAccessibilityTreeSnapshotForPdf(GetActiveWebContents());
  std::string ax_tree_dump =
      DumpPdfAccessibilityTree(ax_tree, /*skip_status_subtree=*/true);

  ASSERT_MULTILINE_STREQ(kExpectedPDFAXTree, ax_tree_dump);
}

// The test is flaky on Mac: https://crbug.com/334099836.
#if BUILDFLAG(IS_MAC)
#define MAYBE_PdfAccessibilityEnableLater DISABLED_PdfAccessibilityEnableLater
#else
#define MAYBE_PdfAccessibilityEnableLater PdfAccessibilityEnableLater
#endif
IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTestWithOopifOverride,
                       MAYBE_PdfAccessibilityEnableLater) {
  // In this test, load the PDF file first, with accessibility off.
  ASSERT_TRUE(
      LoadPdf(embedded_test_server()->GetURL("/pdf/test-bookmarks.pdf")));

  // Now enable accessibility globally, and assert that the PDF
  // accessibility tree loads.
  content::ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);

  WebContents* contents = GetActiveWebContents();
  WaitForAccessibilityTreeToContainNodeWithName(contents,
                                                "1 First Section\r\n");
  ui::AXTreeUpdate ax_tree = GetAccessibilityTreeSnapshotForPdf(contents);
  std::string ax_tree_dump =
      DumpPdfAccessibilityTree(ax_tree, /*skip_status_subtree=*/true);
  ASSERT_MULTILINE_STREQ(kExpectedPDFAXTree, ax_tree_dump);
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTestWithOopifOverride,
                       PdfAccessibilityInIframe) {
  content::ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/pdf/test-iframe.html")));

  WebContents* contents = GetActiveWebContents();
  WaitForAccessibilityTreeToContainNodeWithName(contents,
                                                "1 First Section\r\n");

  ui::AXTreeUpdate ax_tree = GetAccessibilityTreeSnapshotForPdf(contents);
  std::string ax_tree_dump =
      DumpPdfAccessibilityTree(ax_tree, /*skip_status_subtree=*/true);
  ASSERT_MULTILINE_STREQ(kExpectedPDFAXTree, ax_tree_dump);
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTestWithOopifOverride,
                       PdfAccessibilityInOOPIF) {
  content::ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/pdf/test-cross-site-iframe.html")));

  WebContents* contents = GetActiveWebContents();
  WaitForAccessibilityTreeToContainNodeWithName(contents,
                                                "1 First Section\r\n");

  ui::AXTreeUpdate ax_tree = GetAccessibilityTreeSnapshotForPdf(contents);
  std::string ax_tree_dump =
      DumpPdfAccessibilityTree(ax_tree, /*skip_status_subtree=*/true);
  ASSERT_MULTILINE_STREQ(kExpectedPDFAXTree, ax_tree_dump);
}

// Flaky on ChromiumOS MSan. See https://crbug.com/1484869.
// Flaky on Mac: https://crbug.com/334099836.
#if (BUILDFLAG(IS_CHROMEOS) && defined(MEMORY_SANITIZER)) || BUILDFLAG(IS_MAC)
#define MAYBE_PdfAccessibilityWordBoundaries \
  DISABLED_PdfAccessibilityWordBoundaries
#else
#define MAYBE_PdfAccessibilityWordBoundaries PdfAccessibilityWordBoundaries
#endif
IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTestWithOopifOverride,
                       MAYBE_PdfAccessibilityWordBoundaries) {
  content::ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);
  ASSERT_TRUE(
      LoadPdf(embedded_test_server()->GetURL("/pdf/test-bookmarks.pdf")));

  WebContents* contents = GetActiveWebContents();
  WaitForAccessibilityTreeToContainNodeWithName(contents,
                                                "1 First Section\r\n");
  ui::AXTreeUpdate ax_tree = GetAccessibilityTreeSnapshotForPdf(contents);

  bool found = false;
  for (auto& node : ax_tree.nodes) {
    std::string name =
        node.GetStringAttribute(ax::mojom::StringAttribute::kName);
    if (node.role == ax::mojom::Role::kInlineTextBox &&
        name == "First Section\r\n") {
      found = true;
      std::vector<int32_t> word_starts =
          node.GetIntListAttribute(ax::mojom::IntListAttribute::kWordStarts);
      std::vector<int32_t> word_ends =
          node.GetIntListAttribute(ax::mojom::IntListAttribute::kWordEnds);
      ASSERT_EQ(2U, word_starts.size());
      ASSERT_EQ(2U, word_ends.size());
      EXPECT_EQ(0, word_starts[0]);
      EXPECT_EQ(5, word_ends[0]);
      EXPECT_EQ(6, word_starts[1]);
      EXPECT_EQ(13, word_ends[1]);
    }
  }
  ASSERT_TRUE(found);
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTestWithOopifOverride,
                       PdfAccessibilitySelection) {
  // TODO(crbug.com/324636880): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  // TODO(accessibility): Forcing renderer accessibility means the accessibility
  // tree updates in a way unexpected by the test.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceRendererAccessibility)) {
    GTEST_SKIP();
  }

  ASSERT_TRUE(
      LoadPdf(embedded_test_server()->GetURL("/pdf/test-bookmarks.pdf")));

  WebContents* contents = GetActiveWebContents();
  ASSERT_TRUE(
      content::ExecJs(contents,
                      "document.getElementsByTagName('embed')[0].postMessage("
                      "{type: 'selectAll'});"));

  content::ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);
  WaitForAccessibilityTreeToContainNodeWithName(contents,
                                                "1 First Section\r\n");
  ui::AXTreeUpdate ax_tree_update =
      GetAccessibilityTreeSnapshotForPdf(contents);
  ui::AXTree ax_tree(ax_tree_update);

  // Ensure that the selection spans the beginning of the first text
  // node to the end of the last one.
  ui::AXNode* sel_start_node =
      ax_tree.GetFromId(ax_tree.data().sel_anchor_object_id);
  ASSERT_TRUE(sel_start_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, sel_start_node->GetRole());
  std::string start_node_name =
      sel_start_node->GetStringAttribute(ax::mojom::StringAttribute::kName);
  EXPECT_EQ("1 First Section\r\n", start_node_name);
  EXPECT_EQ(0, ax_tree.data().sel_anchor_offset);
  ui::AXNode* para = sel_start_node->parent();
  EXPECT_EQ(ax::mojom::Role::kParagraph, para->GetRole());
  ui::AXNode* region = para->parent();
  EXPECT_EQ(ax::mojom::Role::kRegion, region->GetRole());

  ui::AXNode* sel_end_node =
      ax_tree.GetFromId(ax_tree.data().sel_focus_object_id);
  ASSERT_TRUE(sel_end_node);
  std::string end_node_name =
      sel_end_node->GetStringAttribute(ax::mojom::StringAttribute::kName);
  EXPECT_EQ("3", end_node_name);
  EXPECT_EQ(static_cast<int>(end_node_name.size()),
            ax_tree.data().sel_focus_offset);
  para = sel_end_node->parent();
  EXPECT_EQ(ax::mojom::Role::kParagraph, para->GetRole());
  region = para->parent();
  EXPECT_EQ(ax::mojom::Role::kRegion, region->GetRole());
}

// TODO(crbug.com/330202391): Fix the flakiness on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_PdfAccessibilityContextMenuAction \
  DISABLED_PdfAccessibilityContextMenuAction
#else
#define MAYBE_PdfAccessibilityContextMenuAction \
  PdfAccessibilityContextMenuAction
#endif  // BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTestWithOopifOverride,
                       MAYBE_PdfAccessibilityContextMenuAction) {
  // TODO(crbug.com/324636880): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  // TODO(accessibility): Forcing renderer accessibility means the accessibility
  // tree updates in a way unexpected by the test.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceRendererAccessibility)) {
    GTEST_SKIP();
  }
  // Validate the context menu arguments for PDF selection when context menu is
  // invoked via accessibility tree.
  const char kExepectedPDFSelection[] =
      "1 First Section\n"
      "This is the first section.\n"
      "1\n"
      "1.1 First Subsection\n"
      "This is the first subsection.\n"
      "2\n"
      "2 Second Section\n"
      "3";

  ASSERT_TRUE(
      LoadPdf(embedded_test_server()->GetURL("/pdf/test-bookmarks.pdf")));

  WebContents* contents = GetActiveWebContents();
  content::RenderFrameHost* content_host =
      pdf_extension_test_util::GetOnlyPdfPluginFrame(contents);
  ASSERT_TRUE(content_host);

  ASSERT_TRUE(
      content::ExecJs(contents,
                      "document.getElementsByTagName('embed')[0].postMessage("
                      "{type: 'selectAll'});"));

  content::ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);
  WaitForAccessibilityTreeToContainNodeWithName(contents,
                                                "1 First Section\r\n");

  // Find pdfRoot node in the accessibility tree.
  content::FindAccessibilityNodeCriteria find_criteria;
  find_criteria.role = ax::mojom::Role::kPdfRoot;
  ui::AXPlatformNodeDelegate* pdf_root =
      content::FindAccessibilityNode(contents, find_criteria);
  ASSERT_TRUE(pdf_root);

  content::ContextMenuInterceptor context_menu_interceptor(content_host);

  ContextMenuWaiter menu_waiter;
  // Invoke kShowContextMenu accessibility action on the node with the kPdfRoot
  // role.
  ui::AXActionData data;
  data.action = ax::mojom::Action::kShowContextMenu;
  pdf_root->AccessibilityPerformAction(data);
  menu_waiter.WaitForMenuOpenAndClose();

  context_menu_interceptor.Wait();
  blink::UntrustworthyContextMenuParams params =
      context_menu_interceptor.get_params();

  // Validate the context menu params for selection.
  EXPECT_EQ(blink::mojom::ContextMenuDataMediaType::kPlugin, params.media_type);
  std::string selected_text = base::UTF16ToUTF8(params.selection_text);
  base::ReplaceChars(selected_text, "\r", "", &selected_text);
  EXPECT_EQ(kExepectedPDFSelection, selected_text);
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTestWithOopifOverride,
                       RecordHasAccessibleTextToUmaWithAccessiblePdf) {
  base::HistogramTester histograms;
  content::ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);
  ASSERT_TRUE(
      LoadPdf(embedded_test_server()->GetURL("/pdf/test-bookmarks.pdf")));

  WebContents* contents = GetActiveWebContents();
  ASSERT_TRUE(contents);

  WaitForAccessibilityTreeToContainNodeWithName(contents,
                                                "1 First Section\r\n");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount("Accessibility.PDF.HasAccessibleText", true,
                               /*expected_count=*/1);
  histograms.ExpectTotalCount("Accessibility.PDF.HasAccessibleText",
                              /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTestWithOopifOverride,
                       RecordInaccessiblePdfUKM) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::test::TestFuture<void> ukm_recorded;
  ukm_recorder.SetOnAddEntryCallback(
      ukm::builders::Accessibility_InaccessiblePDFs::kEntryName,
      ukm_recorded.GetRepeatingCallback());

  content::ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);
  ASSERT_TRUE(LoadPdf(embedded_test_server()->GetURL(
      "/pdf/accessibility/hello-world-in-image.pdf")));

  WebContents* contents = GetActiveWebContents();
  ASSERT_TRUE(contents);

  // This string is defined as `IDS_AX_UNLABELED_IMAGE_ROLE_DESCRIPTION` in
  // blink_accessibility_strings.grd.
#if BUILDFLAG(IS_WIN)
  const char kUnlabeledImageName[] = "Unlabeled graphic";
#else
  const char kUnlabeledImageName[] = "Unlabeled image";
#endif  // BUILDFLAG(IS_WIN)
  WaitForAccessibilityTreeToContainNodeWithName(contents, kUnlabeledImageName);

  ASSERT_TRUE(ukm_recorded.Wait());
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTestWithOopifOverride,
                       RecordHasAccessibleTextToUmaWithInaccessible) {
  base::HistogramTester histograms;
  content::ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);

  ASSERT_TRUE(LoadPdf(embedded_test_server()->GetURL(
      "/pdf/accessibility/hello-world-in-image.pdf")));

  WebContents* contents = GetActiveWebContents();
  ASSERT_TRUE(contents);

  // This string is defined as `IDS_AX_UNLABELED_IMAGE_ROLE_DESCRIPTION` in
  // blink_accessibility_strings.grd.
#if BUILDFLAG(IS_WIN)
  const char kUnlabeledImageName[] = "Unlabeled graphic";
#else
  const char kUnlabeledImageName[] = "Unlabeled image";
#endif  // BUILDFLAG(IS_WIN)
  WaitForAccessibilityTreeToContainNodeWithName(contents, kUnlabeledImageName);

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount("Accessibility.PDF.HasAccessibleText", false,
                               /*expected_count=*/1);
  histograms.ExpectTotalCount("Accessibility.PDF.HasAccessibleText",
                              /*expected_count=*/1);
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Test a particular PDF encountered in the wild that triggered a crash
// when accessibility is enabled.  (http://crbug.com/668724)
IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTestWithOopifOverride,
                       PdfAccessibilityTextRunCrash) {
  content::ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);
  ASSERT_TRUE(LoadPdf(embedded_test_server()->GetURL(
      "/pdf_private/accessibility_crash_2.pdf")));

  WaitForAccessibilityTreeToContainNodeWithName(GetActiveWebContents(),
                                                "Page 1");
}
#endif

// This test suite does a simple text-extraction based on the accessibility
// internals, breaking lines & paragraphs where appropriate.  Unlike
// TreeDumpTests, this allows us to verify the kNextOnLine and kPreviousOnLine
// relationships.
class PDFExtensionAccessibilityTextExtractionTest
    : public PDFExtensionAccessibilityTestWithOopifOverride {
 public:
  PDFExtensionAccessibilityTextExtractionTest() = default;
  ~PDFExtensionAccessibilityTextExtractionTest() override = default;

  void RunTextExtractionTest(const base::FilePath::CharType* pdf_file) {
    base::FilePath test_path = ui_test_utils::GetTestFilePath(
        base::FilePath(FILE_PATH_LITERAL("pdf")),
        base::FilePath(FILE_PATH_LITERAL("accessibility")));
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::PathExists(test_path)) << test_path.LossyDisplayName();
    }
    base::FilePath pdf_path = test_path.Append(pdf_file);

    RunTest(pdf_path, "pdf/accessibility");
  }

 protected:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      const override {
    std::vector<base::test::FeatureRefAndParams> enabled =
        PDFExtensionAccessibilityTestWithOopifOverride::GetEnabledFeatures();
    enabled.push_back({chrome_pdf::features::kAccessiblePDFForm, {}});
    return enabled;
  }

 private:
  void RunTest(const base::FilePath& test_file_path, const char* file_dir) {
    // Load the expectation file.
    ui::AXInspectTestHelper test_helper("content");
    std::optional<base::FilePath> expected_file_path =
        test_helper.GetExpectationFilePath(test_file_path);
    ASSERT_TRUE(expected_file_path) << "No expectation file present.";

    std::optional<std::vector<std::string>> expected_lines =
        test_helper.LoadExpectationFile(*expected_file_path);
    ASSERT_TRUE(expected_lines) << "Couldn't load expectation file.";

    // Enable accessibility and load the test file.
    content::ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);
    const GURL test_file_url = embedded_test_server()->GetURL(base::StrCat(
        {"/", file_dir, "/", test_file_path.BaseName().MaybeAsASCII()}));
    ASSERT_TRUE(LoadPdf(test_file_url));
    WaitForAccessibilityTreeToContainNodeWithName(GetActiveWebContents(),
                                                  "Page 1");

    // Extract the text content.
    ui::AXTreeUpdate ax_tree =
        GetAccessibilityTreeSnapshotForPdf(GetActiveWebContents());
    auto actual_lines = CollectLines(ax_tree);

    // Aborts if CollectLines() had a failure.
    if (HasFailure()) {
      return;
    }

    // Validate the dump against the expectation file.
    EXPECT_TRUE(test_helper.ValidateAgainstExpectation(
        test_file_path, *expected_file_path, actual_lines, *expected_lines));
  }

  std::vector<std::string> CollectLines(
      const ui::AXTreeUpdate& ax_tree_update) {
    std::vector<std::string> lines;

    ui::AXTree tree(ax_tree_update);
    std::vector<ui::AXNode*> pdf_root_objs;
    FindAXNodes(tree.root(), {ax::mojom::Role::kPdfRoot}, &pdf_root_objs);
    // Can't use ASSERT_EQ because CollectLines doesn't return void.
    if (pdf_root_objs.size() != 1u) {
      // Add a non-fatal failure here to make the test fail.
      ADD_FAILURE() << "Multiple PDF root nodes in the tree.";
      return {};
    }
    ui::AXNode* pdf_doc_root = pdf_root_objs[0];

    std::vector<ui::AXNode*> text_nodes;
    FindAXNodes(pdf_doc_root,
                {ax::mojom::Role::kStaticText, ax::mojom::Role::kInlineTextBox},
                &text_nodes);

    int previous_node_id = 0;
    int previous_node_next_id = 0;
    std::string line;
    for (ui::AXNode* node : text_nodes) {
      // StaticText begins a new paragraph.
      if (node->GetRole() == ax::mojom::Role::kStaticText && !line.empty()) {
        lines.push_back(line);
        lines.push_back("\u00b6");  // pilcrow/paragraph mark, Alt+0182
        line.clear();
      }

      // We collect all inline text boxes within the paragraph.
      if (node->GetRole() != ax::mojom::Role::kInlineTextBox) {
        continue;
      }

      std::string name =
          node->GetStringAttribute(ax::mojom::StringAttribute::kName);
      std::string_view trimmed_name =
          base::TrimString(name, "\r\n", base::TRIM_TRAILING);
      int prev_id =
          node->GetIntAttribute(ax::mojom::IntAttribute::kPreviousOnLineId);
      if (previous_node_next_id == node->id()) {
        // Previous node pointed to us, so we are part of the same line.
        EXPECT_EQ(previous_node_id, prev_id)
            << "Expect this node to point to previous node.";
        line.append(trimmed_name);
      } else {
        // Not linked with the previous node; this is a new line.
        EXPECT_EQ(previous_node_next_id, 0)
            << "Previous node pointed to something unexpected.";
        EXPECT_EQ(prev_id, 0)
            << "Our back pointer points to something unexpected.";
        if (!line.empty()) {
          lines.push_back(line);
        }
        line = std::string(trimmed_name);
      }

      previous_node_id = node->id();
      previous_node_next_id =
          node->GetIntAttribute(ax::mojom::IntAttribute::kNextOnLineId);
    }
    if (!line.empty()) {
      lines.push_back(line);
    }

    // Extra newline to match current expectations. TODO: get rid of this
    // and rebase the expectations files.
    if (!lines.empty()) {
      lines.push_back("\u00b6");  // pilcrow/paragraph mark, Alt+0182
    }

    return lines;
  }

  // Searches recursively through |current| and all descendants and
  // populates a vector with all nodes that match any of the roles
  // in |roles|.
  void FindAXNodes(ui::AXNode* current,
                   const base::flat_set<ax::mojom::Role>& roles,
                   std::vector<ui::AXNode*>* results) {
    if (base::Contains(roles, current->GetRole())) {
      results->push_back(current);
    }
    for (ui::AXNode* child : current->children()) {
      FindAXNodes(child, roles, results);
    }
  }
};

// Test that Previous/NextOnLineId attributes are present and properly linked on
// InlineTextBoxes within a line.
IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTextExtractionTest,
                       NextOnLine) {
  RunTextExtractionTest(FILE_PATH_LITERAL("next-on-line.pdf"));
}

// Test that a drop-cap is grouped with the correct line.
IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTextExtractionTest, DropCap) {
  RunTextExtractionTest(FILE_PATH_LITERAL("drop-cap.pdf"));
}

// Test that simulated superscripts and subscripts don't cause a line break.
IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTextExtractionTest,
                       SuperscriptSubscript) {
  RunTextExtractionTest(FILE_PATH_LITERAL("superscript-subscript.pdf"));
}

// Test that simple font and font-size changes in the middle of a line don't
// cause line breaks.
IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTextExtractionTest,
                       FontChange) {
  RunTextExtractionTest(FILE_PATH_LITERAL("font-change.pdf"));
}

// Test one property of pdf_private/accessibility_crash_2.pdf, where a page has
// only whitespace characters.
IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTextExtractionTest,
                       OnlyWhitespaceText) {
  RunTextExtractionTest(FILE_PATH_LITERAL("whitespace.pdf"));
}

// Test data of inline text boxes for PDF with weblinks.
IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTextExtractionTest, WebLinks) {
  RunTextExtractionTest(FILE_PATH_LITERAL("weblinks.pdf"));
}

// Test data of inline text boxes for PDF with highlights.
IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTextExtractionTest,
                       Highlights) {
  RunTextExtractionTest(FILE_PATH_LITERAL("highlights.pdf"));
}

// Test data of inline text boxes for PDF with text fields.
IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTextExtractionTest,
                       TextFields) {
  RunTextExtractionTest(FILE_PATH_LITERAL("text_fields.pdf"));
}

// Test data of inline text boxes for PDF with multi-line and various font-sized
// text.
IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTextExtractionTest,
                       ParagraphsAndHeadingUntagged) {
  RunTextExtractionTest(
      FILE_PATH_LITERAL("paragraphs-and-heading-untagged.pdf"));
}

// Test data of inline text boxes for PDF with text, weblinks, images and
// annotation links.
IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTextExtractionTest,
                       LinksImagesAndText) {
  RunTextExtractionTest(FILE_PATH_LITERAL("text-image-link.pdf"));
}

// Test data of inline text boxes for PDF with overlapping annotations.
IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTextExtractionTest,
                       OverlappingAnnots) {
  RunTextExtractionTest(FILE_PATH_LITERAL("overlapping-annots.pdf"));
}

class PDFExtensionAccessibilityTreeDumpTest
    : public PDFExtensionAccessibilityTest,
      public ::testing::WithParamInterface<
          std::tuple<ui::AXApiType::Type, bool>> {
 public:
  PDFExtensionAccessibilityTreeDumpTest() : test_helper_(ax_inspect_type()) {}
  ~PDFExtensionAccessibilityTreeDumpTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PDFExtensionAccessibilityTest::SetUpCommandLine(command_line);

    // Each test pass might require custom feature setup
    test_helper_.InitializeFeatureList();
  }

  ui::AXApiType::Type ax_inspect_type() const {
    return std::get<0>(GetParam());
  }

  bool UseOopif() const override { return std::get<1>(GetParam()); }

  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      const override {
    std::vector<base::test::FeatureRefAndParams> enabled =
        PDFExtensionAccessibilityTest::GetEnabledFeatures();
    enabled.push_back({chrome_pdf::features::kAccessiblePDFForm, {}});
    return enabled;
  }

 protected:
  void RunPDFTest(const base::FilePath::CharType* pdf_file) {
    base::FilePath test_path = ui_test_utils::GetTestFilePath(
        base::FilePath(FILE_PATH_LITERAL("pdf")),
        base::FilePath(FILE_PATH_LITERAL("accessibility")));
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::PathExists(test_path)) << test_path.LossyDisplayName();
    }
    base::FilePath pdf_path = test_path.Append(pdf_file);

    RunTest(pdf_path, "pdf/accessibility");
  }

 private:
  using AXPropertyFilter = ui::AXPropertyFilter;

  //  See chrome/test/data/pdf/accessibility/readme.md for more info.
  ui::AXInspectScenario ParsePdfForExtraDirectives(
      const std::string& pdf_contents) {
    const char kCommentMark = '%';

    std::vector<std::string> lines;
    for (const std::string& line : base::SplitString(
             pdf_contents, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
      if (line.size() > 1 && line[0] == kCommentMark) {
        // Remove first character since it's the comment mark.
        lines.push_back(line.substr(1));
      }
    }

    return test_helper_.ParseScenario(lines, DefaultFilters());
  }

  void RunTest(const base::FilePath& test_file_path, const char* file_dir) {
    std::string pdf_contents;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::ReadFileToString(test_file_path, &pdf_contents));
    }

    // Set up the tree formatter. Parse filters and other directives in the test
    // file.
    ui::AXInspectScenario scenario = ParsePdfForExtraDirectives(pdf_contents);

    std::unique_ptr<ui::AXTreeFormatter> formatter =
        content::AXInspectFactory::CreateFormatter(ax_inspect_type());
    formatter->SetPropertyFilters(scenario.property_filters,
                                  ui::AXTreeFormatter::kFiltersDefaultSet);

    // Exit without running the test if we can't find an expectation file or if
    // the expectation file contains a skip marker.
    // This is used to skip certain tests on certain platforms.
    base::FilePath expected_file_path =
        test_helper_.GetExpectationFilePath(test_file_path);
    if (expected_file_path.empty()) {
      LOG(INFO) << "No expectation file present, ignoring test on this "
                   "platform.";
      return;
    }

    std::optional<std::vector<std::string>> expected_lines =
        test_helper_.LoadExpectationFile(expected_file_path);
    if (!expected_lines) {
      LOG(INFO) << "Skipping this test on this platform.";
      return;
    }

    // Enable accessibility and load the test file.
    content::ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);
    const GURL test_file_url = embedded_test_server()->GetURL(base::StrCat(
        {"/", file_dir, "/", test_file_path.BaseName().MaybeAsASCII()}));
    ASSERT_TRUE(LoadPdf(test_file_url));
    WaitForAccessibilityTreeToContainNodeWithName(GetActiveWebContents(),
                                                  "Page 1");

    // Find the embedded PDF and dump the accessibility tree.
    content::FindAccessibilityNodeCriteria find_criteria;
    find_criteria.role = ax::mojom::Role::kPdfRoot;
    ui::AXPlatformNodeDelegate* pdf_root =
        content::FindAccessibilityNode(GetActiveWebContents(), find_criteria);
    ASSERT_TRUE(pdf_root);

    std::string actual_contents = formatter->Format(pdf_root);

    std::vector<std::string> actual_lines =
        base::SplitString(actual_contents, "\n", base::KEEP_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
    RemoveStatusSubtreeFromFormatOutput(actual_lines, ax_inspect_type());

    // Validate the dump against the expectation file.
    EXPECT_TRUE(test_helper_.ValidateAgainstExpectation(
        test_file_path, expected_file_path, actual_lines, *expected_lines));
  }

  std::vector<AXPropertyFilter> DefaultFilters() const {
    std::vector<AXPropertyFilter> property_filters;

    property_filters.emplace_back("value='*'", AXPropertyFilter::ALLOW);
    // The value attribute on the document object contains the URL of the
    // current page which will not be the same every time the test is run.
    // The PDF plugin uses the 'chrome-extension' protocol, so block that as
    // well.
    property_filters.emplace_back("value='http*'", AXPropertyFilter::DENY);
    property_filters.emplace_back("value='chrome-extension*'",
                                  AXPropertyFilter::DENY);
    // Object attributes.value
    property_filters.emplace_back("layout-guess:*", AXPropertyFilter::ALLOW);

    property_filters.emplace_back("select*", AXPropertyFilter::ALLOW);
    property_filters.emplace_back("descript*", AXPropertyFilter::ALLOW);
    property_filters.emplace_back("check*", AXPropertyFilter::ALLOW);
    property_filters.emplace_back("horizontal", AXPropertyFilter::ALLOW);
    property_filters.emplace_back("multiselectable", AXPropertyFilter::ALLOW);
    property_filters.emplace_back("isPageBreakingObject*",
                                  AXPropertyFilter::ALLOW);

    // Deny most empty values
    property_filters.emplace_back("*=''", AXPropertyFilter::DENY);
    // After denying empty values, because we want to allow name=''
    property_filters.emplace_back("name=*", AXPropertyFilter::ALLOW_EMPTY);

    return property_filters;
  }

  void RemoveStatusSubtreeFromFormatOutput(
      std::vector<std::string>& output_lines,
      const ui::AXApiType::Type& platform_type) {
    // The status subtree is of the form banner -> status -> staticText and will
    // be in the second to fourth lines in the tree output. These nodes are
    // always added to the PDF accessibility tree after the PDF root node and
    // don't get deleted. So, it is safe to assume that they are always there in
    // the format output.
    // Thus, delete the second and third lines from the tree format output.
    ASSERT_GT(output_lines.size(), 3u);
    std::string banner_role;
    std::string status_role;
    std::string static_text_role;
    switch (platform_type) {
      case ui::AXApiType::kBlink:
        banner_role = "banner";
        status_role = "status";
        static_text_role = "static";
        break;
      case ui::AXApiType::kLinux:
        banner_role = "landmark";
        status_role = "statusbar";
        static_text_role = "static";
        break;
      case ui::AXApiType::kMac:
        banner_role = "AXLandmarkBanner";
        status_role = "AXApplicationStatus";
        static_text_role = "AXStaticText";
        break;
      case ui::AXApiType::kWinIA2:
        banner_role = "IA2_ROLE_LANDMARK";
        status_role = "ROLE_SYSTEM_STATUSBAR";
        static_text_role = "ROLE_SYSTEM_STATICTEXT";
        break;
      case ui::AXApiType::kWinUIA:
        banner_role = "Group";
        status_role = "StatusBar";
        static_text_role = "Text";
        break;
      case ui::AXApiType::kNone:
        [[fallthrough]];
      case ui::AXApiType::kAndroid:
        [[fallthrough]];
      case ui::AXApiType::kAndroidExternal:
        [[fallthrough]];
      case ui::AXApiType::kFuchsia:
        return;
    }
    EXPECT_TRUE(base::Contains(output_lines[1], banner_role))
        << output_lines[1];
    EXPECT_TRUE(base::Contains(output_lines[2], status_role))
        << output_lines[2];
    EXPECT_TRUE(base::Contains(output_lines[3], static_text_role))
        << output_lines[3];

    output_lines.erase(output_lines.begin() + 1, output_lines.begin() + 4);
  }

  ui::AXInspectTestHelper test_helper_;
};

// Constructs a list of accessibility tests, one for each accessibility tree
// formatter testpasses.
const std::vector<ui::AXApiType::Type> GetAXTestValues() {
  std::vector<ui::AXApiType::Type> passes =
      ui::AXInspectTestHelper::TreeTestPasses();
  return passes;
}

struct PDFExtensionAccessibilityTreeDumpTestPassToString {
  std::string operator()(
      const ::testing::TestParamInfo<std::tuple<ui::AXApiType::Type, bool>>& i)
      const {
    return std::string(std::get<1>(i.param) ? "OOPIF_" : "GUESTVIEW_") +
           std::string(std::get<0>(i.param));
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         PDFExtensionAccessibilityTreeDumpTest,
                         testing::Combine(testing::ValuesIn(GetAXTestValues()),
                                          testing::Bool()),
                         PDFExtensionAccessibilityTreeDumpTestPassToString());

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest, HelloWorld) {
  RunPDFTest(FILE_PATH_LITERAL("hello-world.pdf"));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest,
                       ParagraphsAndHeadingUntagged) {
  RunPDFTest(FILE_PATH_LITERAL("paragraphs-and-heading-untagged.pdf"));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest, MultiPage) {
  RunPDFTest(FILE_PATH_LITERAL("multi-page.pdf"));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest,
                       DirectionalTextRuns) {
  RunPDFTest(FILE_PATH_LITERAL("directional-text-runs.pdf"));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest, TextDirection) {
  RunPDFTest(FILE_PATH_LITERAL("text-direction.pdf"));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest, WebLinks) {
  RunPDFTest(FILE_PATH_LITERAL("weblinks.pdf"));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest,
                       OverlappingLinks) {
  RunPDFTest(FILE_PATH_LITERAL("overlapping-links.pdf"));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest, Highlights) {
  RunPDFTest(FILE_PATH_LITERAL("highlights.pdf"));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest, TextFields) {
  RunPDFTest(FILE_PATH_LITERAL("text_fields.pdf"));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest, Images) {
  RunPDFTest(FILE_PATH_LITERAL("image_alt_text.pdf"));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest,
                       LinksImagesAndText) {
  RunPDFTest(FILE_PATH_LITERAL("text-image-link.pdf"));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest,
                       TextRunStyleHeuristic) {
  RunPDFTest(FILE_PATH_LITERAL("text-run-style-heuristic.pdf"));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest, TextStyle) {
  RunPDFTest(FILE_PATH_LITERAL("text-style.pdf"));
}

// TODO(crbug.com/40745411)
IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest, XfaFields) {
  RunPDFTest(FILE_PATH_LITERAL("xfa_fields.pdf"));
}

// This test suite validates the navigation done using the accessibility client.
using PDFExtensionAccessibilityNavigationTest =
    PDFExtensionAccessibilityTestWithOopifOverride;

// TODO(crbug.com/40934115): Fix the flakiness on ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_LinkNavigation DISABLED_LinkNavigation
#else
#define MAYBE_LinkNavigation LinkNavigation
#endif  // BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityNavigationTest,
                       MAYBE_LinkNavigation) {
  // Enable accessibility and load the test file.
  content::ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);
  ASSERT_TRUE(LoadPdf(
      embedded_test_server()->GetURL("/pdf/accessibility/weblinks.pdf")));

  WaitForAccessibilityTreeToContainNodeWithName(GetActiveWebContents(),
                                                "Page 1");

  // Find the specific link node.
  content::FindAccessibilityNodeCriteria find_criteria;
  find_criteria.role = ax::mojom::Role::kLink;
  find_criteria.name = "http://bing.com";
  ui::AXPlatformNodeDelegate* link_node =
      content::FindAccessibilityNode(GetActiveWebContents(), find_criteria);
  ASSERT_TRUE(link_node);

  // Invoke action on a link and wait for navigation to complete.
  EXPECT_EQ(ax::mojom::DefaultActionVerb::kJump,
            link_node->GetData().GetDefaultActionVerb());
  content::AccessibilityNotificationWaiter event_waiter(
      GetActiveWebContents(), ui::kAXModeComplete,
      ax::mojom::Event::kLoadComplete);
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kDoDefault;
  action_data.target_node_id = link_node->GetData().id;
  link_node->AccessibilityPerformAction(action_data);
  ASSERT_TRUE(event_waiter.WaitForNotification());

  // Test that navigation occurred correctly.
  const GURL& expected_url = GetActiveWebContents()->GetLastCommittedURL();
  EXPECT_EQ("https://bing.com/", expected_url.spec());
}

// TODO(crbug.com/289010799): Revisit using `crosapi` in `PdfOcrUmaTest` for
// Lacros.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
// This test suite contains simple tests for the PDF OCR feature.
class PdfOcrUmaTest : public PDFExtensionAccessibilityTest,
                      public ::testing::WithParamInterface<bool> {
 public:
  PdfOcrUmaTest() = default;
  ~PdfOcrUmaTest() override = default;

  // PDFExtensionAccessibilityTest:
  bool UseOopif() const override { return GetParam(); }

 protected:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      const override {
    std::vector<base::test::FeatureRefAndParams> enabled =
        PDFExtensionAccessibilityTest::GetEnabledFeatures();
    enabled.push_back({::features::kPdfOcr, {}});
    if (UseOopif()) {
      enabled.push_back({chrome_pdf::features::kPdfOopif, {}});
    }
    return enabled;
  }

  std::vector<base::test::FeatureRef> GetDisabledFeatures() const override {
    // `PDFExtensionAccessibilityTest` has
    // `::features::kPdfOcr` in a list of disabled features. Now that
    // `::features::kPdfOcr` is used in this test, don't include it in the
    // disabled list.
    std::vector<base::test::FeatureRef> disabled;
    if (!UseOopif()) {
      disabled.push_back(chrome_pdf::features::kPdfOopif);
    }
    return disabled;
  }
};

IN_PROC_BROWSER_TEST_P(PdfOcrUmaTest, CheckOpenedWithScreenReader) {
  // TODO(crbug.com/289010799): Remove this once the metrics are added for OOPIF
  // PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ::ash::AccessibilityManager::Get()->EnableSpokenFeedback(true);
#else
  EnableScreenReader(true);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  base::HistogramTester histograms;
  histograms.ExpectUniqueSample(
      "Accessibility.PDF.OpenedWithScreenReader.PdfOcr", true,
      /*expected_bucket_count=*/0);

  ASSERT_TRUE(LoadPdf(embedded_test_server()->GetURL("/pdf/test.pdf")));

  WebContents* contents = GetActiveWebContents();
  content::RenderFrameHost* extension_host =
      pdf_extension_test_util::GetOnlyPdfExtensionHost(contents);
  ASSERT_TRUE(extension_host);

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectUniqueSample(
      "Accessibility.PDF.OpenedWithScreenReader.PdfOcr", true,
      /*expected_bucket_count=*/1);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_P(PdfOcrUmaTest, CheckOpenedWithSelectToSpeak) {
  // TODO(crbug.com/289010799): Remove this once the metrics are added for OOPIF
  // PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  ::ash::AccessibilityManager::Get()->SetSelectToSpeakEnabled(true);

  base::HistogramTester histograms;
  histograms.ExpectUniqueSample(
      "Accessibility.PDF.OpenedWithSelectToSpeak.PdfOcr", true,
      /*expected_bucket_count=*/0);

  ASSERT_TRUE(LoadPdf(embedded_test_server()->GetURL("/pdf/test.pdf")));

  WebContents* contents = GetActiveWebContents();
  content::RenderFrameHost* extension_host =
      pdf_extension_test_util::GetOnlyPdfExtensionHost(contents);
  ASSERT_TRUE(extension_host);

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectUniqueSample(
      "Accessibility.PDF.OpenedWithSelectToSpeak.PdfOcr", true,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_P(PdfOcrUmaTest,
                       CheckSelectToSpeakPagesOcredWithAccessiblePdf) {
  // TODO(crbug.com/289010799): Remove this once the metrics are added for OOPIF
  // PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  ::ash::AccessibilityManager::Get()->SetSelectToSpeakEnabled(true);

  base::HistogramTester histograms;
  histograms.ExpectTotalCount(
      "Accessibility.PdfOcr.CrosSelectToSpeak.PagesOcred",
      /*expected_count=*/0);

  ASSERT_TRUE(LoadPdf(embedded_test_server()->GetURL("/pdf/test.pdf")));

  WebContents* contents = GetActiveWebContents();
  content::RenderFrameHost* extension_host =
      pdf_extension_test_util::GetOnlyPdfExtensionHost(contents);
  ASSERT_TRUE(extension_host);

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // The metric should record nothing for accessible PDFs.
  histograms.ExpectTotalCount(
      "Accessibility.PdfOcr.CrosSelectToSpeak.PagesOcred",
      /*expected_count=*/0);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

INSTANTIATE_TEST_SUITE_P(All,
                         PdfOcrUmaTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return base::StringPrintf(
                               "OOPIF_%s", info.param ? "Enabled" : "Disabled");
                         });
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

// TODO(crbug.com/40268279): Stop testing both modes after OOPIF PDF viewer
// launches.
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    PDFExtensionAccessibilityTestWithOopifOverride);
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    PDFExtensionAccessibilityTextExtractionTest);
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    PDFExtensionAccessibilityNavigationTest);

#if defined(PDF_OCR_INTEGRATION_TEST_ENABLED)

class PdfOcrIntegrationTest
    : public PDFExtensionAccessibilityTest,
      public screen_ai::ScreenAIInstallState::Observer,
      public ::testing::WithParamInterface<std::tuple<bool, bool, bool, bool>> {
 public:
  PdfOcrIntegrationTest() = default;
  ~PdfOcrIntegrationTest() override = default;

  bool IsOcrServiceEnabled() const { return std::get<0>(GetParam()); }
  bool IsLibraryAvailable() const { return std::get<1>(GetParam()); }
  bool IsSearchifyEnabled() const { return std::get<2>(GetParam()); }

  // PDFExtensionAccessibilityTest:
  bool UseOopif() const override { return std::get<3>(GetParam()); }

  bool IsOcrAvailable() const {
    return IsOcrServiceEnabled() && IsLibraryAvailable();
  }

  // PDFExtensionAccessibilityTest:
  void SetUpOnMainThread() override {
    PDFExtensionAccessibilityTest::SetUpOnMainThread();

    if (IsLibraryAvailable()) {
      screen_ai::ScreenAIInstallState::GetInstance()->SetComponentFolder(
          screen_ai::GetComponentBinaryPathForTests().DirName());
    } else {
      // Set an observer to mark download as failed when requested.
      component_download_observer_.Observe(
          screen_ai::ScreenAIInstallState::GetInstance());
    }

    mode_override_.emplace(ui::kAXModeComplete);
    EnableScreenReader(true);
  }

  void TearDownOnMainThread() override {
    component_download_observer_.Reset();
    EnableScreenReader(false);
    mode_override_.reset();
    PDFExtensionAccessibilityTest::TearDownOnMainThread();
  }

  void WaitForTreeStatus(int status_message_id) {
    WebContents* contents = GetActiveWebContents();
    ASSERT_TRUE(contents);
    const std::string expected_message =
        l10n_util::GetStringUTF8(status_message_id);
    WaitForAccessibilityTreeToContainNodeWithName(contents, expected_message);
  }

  // ScreenAIInstallState::Observer:
  void StateChanged(screen_ai::ScreenAIInstallState::State state) override {
    if (state == screen_ai::ScreenAIInstallState::State::kDownloading &&
        !IsLibraryAvailable()) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce([]() {
            screen_ai::ScreenAIInstallState::GetInstance()->SetState(
                screen_ai::ScreenAIInstallState::State::kDownloadFailed);
          }));
    }
  }

  int GetExpectedStatus(bool has_content) {
    // TODO(crbug.com/360803943): Update `PdfAccessibilityTree` to send the same
    // notifications when searchify is enabled.
    if (IsSearchifyEnabled()) {
      return IDS_PDF_LOADED_TO_A11Y_TREE;
    }

    if (!IsOcrAvailable()) {
      return IDS_PDF_OCR_FEATURE_ALERT;
    }

    return has_content ? IDS_PDF_OCR_COMPLETED : IDS_PDF_OCR_NO_RESULT;
  }

 protected:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      const override {
    auto enabled = PDFExtensionAccessibilityTest::GetEnabledFeatures();
    enabled.push_back({::features::kPdfOcr, {}});
    enabled.push_back({::features::kScreenAITestMode, {}});
    if (IsOcrServiceEnabled()) {
      enabled.push_back({ax::mojom::features::kScreenAIOCREnabled, {}});
    }
    if (IsSearchifyEnabled()) {
      enabled.push_back({chrome_pdf::features::kPdfSearchify, {}});
    }
    if (UseOopif()) {
      enabled.push_back({chrome_pdf::features::kPdfOopif, {}});
    }
    return enabled;
  }

  std::vector<base::test::FeatureRef> GetDisabledFeatures() const override {
    // `PDFExtensionAccessibilityTest` has `::features::kPdfOcr` in a list of
    // disabled features. Now that `::features::kPdfOcr` is used in this test,
    // parent disabled features should not be used.
    std::vector<base::test::FeatureRef> disabled;
    if (!IsOcrServiceEnabled()) {
      disabled.push_back(ax::mojom::features::kScreenAIOCREnabled);
    }
    if (!IsSearchifyEnabled()) {
      disabled.push_back(chrome_pdf::features::kPdfSearchify);
    }
    if (!UseOopif()) {
      disabled.push_back(chrome_pdf::features::kPdfOopif);
    }
    return disabled;
  }

  void RunPDFAXTreeDumpTest(const char* pdf_file, int status_message_id) {
    base::FilePath test_path = ui_test_utils::GetTestFilePath(
        base::FilePath(FILE_PATH_LITERAL("pdf")),
        base::FilePath(FILE_PATH_LITERAL("accessibility")));
    base::FilePath test_pdf_path = test_path.AppendASCII(pdf_file);

    const GURL test_file_url = embedded_test_server()->GetURL(
        base::StrCat({"/pdf/accessibility/", pdf_file}));
    ASSERT_TRUE(LoadPdf(test_file_url));

    WaitForTreeStatus(status_message_id);

    ui::AXTreeUpdate ax_tree =
        GetAccessibilityTreeSnapshotForPdf(GetActiveWebContents());
    std::string ax_tree_dump =
        DumpPdfAccessibilityTree(ax_tree, /*skip_status_subtree=*/false);
    std::string expected_tree_dump =
        GetExpectedAXTreeDumpForPdf(test_pdf_path, IsOcrAvailable());
    ASSERT_NE("", expected_tree_dump);

    // TODO(crbug.com/360803943): Update `PdfAccessibilityTree` to add header
    // and footer to the searchify extracted text, similar to the enabled OCR
    // case.
    if (IsSearchifyEnabled()) {
      return;
    }

    ASSERT_MULTILINE_STREQ(expected_tree_dump, ax_tree_dump);
  }

 private:
  std::string GetExpectedAXTreeDumpForPdf(const base::FilePath& pdf_path,
                                          bool is_ocr_available) {
    // If the given `pdf_path` contains a filename, "test.pdf", an expected
    // file path will have a filename, "test-expected-with-pdfocr.txt", when
    // PDF OCR is on. `expected_file_suffix` will be created based on whether
    // PDF OCR is on and whether it has a separate output for Windows.
    base::FilePath::StringType expected_file_suffix =
        is_ocr_available ? FILE_PATH_LITERAL("-expected-with-pdfocr")
                         : FILE_PATH_LITERAL("-expected-without-pdfocr");
#if BUILDFLAG(IS_WIN)
    // When OCR is unavailable, each test input has a separate expected output
    // for Windows. Otherwise, only "blank_image.pdf" has a separate expected
    // output for Windows.
    if (!is_ocr_available ||
        pdf_path.BaseName().value() == FILE_PATH_LITERAL("blank_image.pdf")) {
      expected_file_suffix += FILE_PATH_LITERAL("-win");
    }
#endif  // BUILDFLAG(IS_WIN)
    expected_file_suffix += FILE_PATH_LITERAL(".txt");

    // Replace the extension of `pdf_path` with `expected_file_suffix`. However
    // `base::FilePath::ReplaceExtension()` won't work here as it appends '.'
    // to the beginning of the new extension given to the function if the new
    // extension doesn't start with '.'.
    base::FilePath expected_file_path = pdf_path.DirName().Append(
        pdf_path.BaseName().RemoveExtension().value() + expected_file_suffix);
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      if (!base::PathExists(expected_file_path)) {
        return std::string();
      }
    }

    return LoadExpectationFile(expected_file_path);
  }

  std::string LoadExpectationFile(const base::FilePath& expected_file) {
    base::ScopedAllowBlockingForTesting allow_blocking;

    std::string expected_contents_raw;
    if (!base::ReadFileToString(expected_file, &expected_contents_raw)) {
      ADD_FAILURE() << "Unable to read an expected file at " << expected_file;
    }

    // Tolerate Windows-style line endings (\r\n) in the expected file:
    // normalize by deleting all \r from the file (if any) to leave only \n.
    std::string expected_contents;
    base::RemoveChars(expected_contents_raw, "\r", &expected_contents);

    return expected_contents;
  }

  std::optional<content::ScopedAccessibilityModeOverride> mode_override_;
  base::ScopedObservation<screen_ai::ScreenAIInstallState,
                          screen_ai::ScreenAIInstallState::Observer>
      component_download_observer_{this};
};

IN_PROC_BROWSER_TEST_P(PdfOcrIntegrationTest, EnsureScreenAIInitializes) {
  // Since screen reader is on, library download is triggered and if it is
  // successful, initialization of Screen AI OCR service will be successful.

  // Wait for Screen AI OCR service to either get ready or fail.
  base::test::TestFuture<bool> future;
  auto* router = screen_ai::ScreenAIServiceRouterFactory::GetForBrowserContext(
      browser()->profile());
  router->GetServiceStateAsync(screen_ai::ScreenAIServiceRouter::Service::kOCR,
                               future.GetCallback());
  ASSERT_TRUE(future.Wait());
  ASSERT_EQ(future.Get(), IsOcrAvailable());

  // Library download state should not depend on OcrService availability.
  screen_ai::ScreenAIInstallState::State expected_state =
      IsLibraryAvailable()
          ? screen_ai::ScreenAIInstallState::State::kDownloaded
          : screen_ai::ScreenAIInstallState::State::kDownloadFailed;
  EXPECT_EQ(expected_state,
            screen_ai::ScreenAIInstallState::GetInstance()->get_state());
}

IN_PROC_BROWSER_TEST_P(PdfOcrIntegrationTest, HelloWorld) {
  RunPDFAXTreeDumpTest("hello-world-in-image.pdf",
                       GetExpectedStatus(/*has_content=*/true));
}

IN_PROC_BROWSER_TEST_P(PdfOcrIntegrationTest, ThreePagePDF) {
  RunPDFAXTreeDumpTest("inaccessible-text-in-three-page.pdf",
                       GetExpectedStatus(/*has_content=*/true));
}

IN_PROC_BROWSER_TEST_P(PdfOcrIntegrationTest, TestBatchingWithTwentyPagePDF) {
  RunPDFAXTreeDumpTest("inaccessible-text-in-twenty-page.pdf",
                       GetExpectedStatus(/*has_content=*/true));
}

IN_PROC_BROWSER_TEST_P(PdfOcrIntegrationTest, NoOcrResultOnBlankImagePdf) {
  RunPDFAXTreeDumpTest("blank_image.pdf",
                       GetExpectedStatus(/*has_content=*/false));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PdfOcrIntegrationTest,
    ::testing::Combine(testing::Bool(),
                       testing::Bool(),
                       testing::Bool(),
                       testing::Bool()),
    [](const testing::TestParamInfo<std::tuple<bool, bool, bool, bool>>& info) {
      return base::StringPrintf(
          "OcrService_%s_Library_%s_Searchify_%s_%s",
          std::get<0>(info.param) ? "Enabled" : "Disabled",
          std::get<1>(info.param) ? "Available" : "Unavailable",
          std::get<2>(info.param) ? "Enabled" : "Disabled",
          std::get<3>(info.param) ? "OOPIF" : "GuestView");
    });

#endif  // defined(PDF_OCR_INTEGRATION_TEST_ENABLED)
