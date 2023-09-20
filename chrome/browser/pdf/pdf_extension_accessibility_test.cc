// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <map>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/pdf/pdf_extension_test_base.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/services/screen_ai/buildflags/buildflags.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/ax_inspect_factory.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "pdf/pdf_features.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/context_menu_data/untrustworthy_context_menu_params.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/accessibility/platform/inspect/ax_api_type.h"
#include "ui/accessibility/platform/inspect/ax_inspect_scenario.h"
#include "ui/accessibility/platform/inspect/ax_inspect_test_helper.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include "chrome/browser/renderer_context_menu/pdf_ocr_menu_observer.h"
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

namespace {

using ::content::WebContents;
using ::extensions::MimeHandlerViewGuest;

std::string DumpPdfAccessibilityTree(const ui::AXTreeUpdate& ax_tree) {
  // Create a string representation of the tree starting with the kPdfRoot
  // object.
  std::string ax_tree_dump;
  std::map<int32_t, int> id_to_indentation;
  bool found_pdf_root = false;
  for (const ui::AXNodeData& node : ax_tree.nodes) {
    if (node.role == ax::mojom::Role::kPdfRoot) {
      found_pdf_root = true;
    }
    if (!found_pdf_root) {
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
};

// Flaky, see crbug.com/1477361
IN_PROC_BROWSER_TEST_F(PDFExtensionAccessibilityTest,
                       DISABLED_PdfAccessibility) {
  content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();

  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test-bookmarks.pdf"));
  ASSERT_TRUE(guest);

  WaitForAccessibilityTreeToContainNodeWithName(GetActiveWebContents(),
                                                "1 First Section\r\n");
  ui::AXTreeUpdate ax_tree =
      GetAccessibilityTreeSnapshotForPdf(GetActiveWebContents());
  std::string ax_tree_dump = DumpPdfAccessibilityTree(ax_tree);

  ASSERT_MULTILINE_STREQ(kExpectedPDFAXTree, ax_tree_dump);
}

// Flaky, see crbug.com/1477361
IN_PROC_BROWSER_TEST_F(PDFExtensionAccessibilityTest,
                       DISABLED_PdfAccessibilityEnableLater) {
  // In this test, load the PDF file first, with accessibility off.
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test-bookmarks.pdf"));
  ASSERT_TRUE(guest);

  // Now enable accessibility globally, and assert that the PDF
  // accessibility tree loads.
  content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();

  WebContents* contents = GetActiveWebContents();
  WaitForAccessibilityTreeToContainNodeWithName(contents,
                                                "1 First Section\r\n");
  ui::AXTreeUpdate ax_tree = GetAccessibilityTreeSnapshotForPdf(contents);
  std::string ax_tree_dump = DumpPdfAccessibilityTree(ax_tree);
  ASSERT_MULTILINE_STREQ(kExpectedPDFAXTree, ax_tree_dump);
}

// Flaky, see crbug.com/1477361
IN_PROC_BROWSER_TEST_F(PDFExtensionAccessibilityTest,
                       DISABLED_PdfAccessibilityInIframe) {
  content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/pdf/test-iframe.html")));

  WebContents* contents = GetActiveWebContents();
  WaitForAccessibilityTreeToContainNodeWithName(contents,
                                                "1 First Section\r\n");

  ui::AXTreeUpdate ax_tree = GetAccessibilityTreeSnapshotForPdf(contents);
  std::string ax_tree_dump = DumpPdfAccessibilityTree(ax_tree);
  ASSERT_MULTILINE_STREQ(kExpectedPDFAXTree, ax_tree_dump);
}

IN_PROC_BROWSER_TEST_F(PDFExtensionAccessibilityTest, PdfAccessibilityInOOPIF) {
  content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/pdf/test-cross-site-iframe.html")));

  WebContents* contents = GetActiveWebContents();
  WaitForAccessibilityTreeToContainNodeWithName(contents,
                                                "1 First Section\r\n");

  ui::AXTreeUpdate ax_tree = GetAccessibilityTreeSnapshotForPdf(contents);
  std::string ax_tree_dump = DumpPdfAccessibilityTree(ax_tree);
  ASSERT_MULTILINE_STREQ(kExpectedPDFAXTree, ax_tree_dump);
}

// Flaky on ChromiumOS MSan. See https://crbug.com/1484869
#if BUILDFLAG(IS_CHROMEOS) && defined(MEMORY_SANITIZER)
#define MAYBE_PdfAccessibilityWordBoundaries \
  DISABLED_PdfAccessibilityWordBoundaries
#else
#define MAYBE_PdfAccessibilityWordBoundaries PdfAccessibilityWordBoundaries
#endif
IN_PROC_BROWSER_TEST_F(PDFExtensionAccessibilityTest,
                       MAYBE_PdfAccessibilityWordBoundaries) {
  content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test-bookmarks.pdf"));
  ASSERT_TRUE(guest);

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

// Flaky, see crbug.com/1477361
IN_PROC_BROWSER_TEST_F(PDFExtensionAccessibilityTest,
                       DISABLED_PdfAccessibilitySelection) {
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test-bookmarks.pdf"));
  ASSERT_TRUE(guest);

  WebContents* contents = GetActiveWebContents();
  ASSERT_TRUE(
      content::ExecJs(contents,
                      "document.getElementsByTagName('embed')[0].postMessage("
                      "{type: 'selectAll'});"));

  content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();
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

// Flaky, see crbug.com/1477361
IN_PROC_BROWSER_TEST_F(PDFExtensionAccessibilityTest,
                       DISABLED_PdfAccessibilityContextMenuAction) {
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

  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test-bookmarks.pdf"));
  ASSERT_TRUE(guest);

  WebContents* contents = GetActiveWebContents();
  ASSERT_TRUE(
      content::ExecJs(contents,
                      "document.getElementsByTagName('embed')[0].postMessage("
                      "{type: 'selectAll'});"));

  content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();
  WaitForAccessibilityTreeToContainNodeWithName(contents,
                                                "1 First Section\r\n");

  // Find pdfRoot node in the accessibility tree.
  content::FindAccessibilityNodeCriteria find_criteria;
  find_criteria.role = ax::mojom::Role::kPdfRoot;
  ui::AXPlatformNodeDelegate* pdf_root =
      content::FindAccessibilityNode(contents, find_criteria);
  ASSERT_TRUE(pdf_root);

  content::ContextMenuInterceptor context_menu_interceptor(
      GetPluginFrame(guest));

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

// Flaky, see crbug.com/1477361
IN_PROC_BROWSER_TEST_F(PDFExtensionAccessibilityTest,
                       DISABLED_RecordHasAccessibleTextToUmaWithAccessiblePdf) {
  MimeHandlerViewGuest* guest_view = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test-bookmarks.pdf"));
  ASSERT_TRUE(guest_view);

  WebContents* contents = GetActiveWebContents();
  ASSERT_TRUE(contents);

  base::HistogramTester histograms;
  content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();
  WaitForAccessibilityTreeToContainNodeWithName(contents,
                                                "1 First Section\r\n");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount("Accessibility.PDF.HasAccessibleText", true,
                               /*expected_count=*/1);
  histograms.ExpectTotalCount("Accessibility.PDF.HasAccessibleText",
                              /*expected_count=*/1);
}

// Flaky, see crbug.com/1477361
IN_PROC_BROWSER_TEST_F(PDFExtensionAccessibilityTest,
                       DISABLED_RecordInaccessiblePdfUKM) {
  MimeHandlerViewGuest* guest_view =
      LoadPdfGetMimeHandlerView(embedded_test_server()->GetURL(
          "/pdf/accessibility/hello-world-in-image.pdf"));
  ASSERT_TRUE(guest_view);

  WebContents* contents = GetActiveWebContents();
  ASSERT_TRUE(contents);

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::test::TestFuture<void> ukm_recorded;
  ukm_recorder.SetOnAddEntryCallback(
      ukm::builders::Accessibility_InaccessiblePDFs::kEntryName,
      ukm_recorded.GetRepeatingCallback());

  content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();
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

// Flaky, see crbug.com/1477361
IN_PROC_BROWSER_TEST_F(
    PDFExtensionAccessibilityTest,
    DISABLED_RecordHasAccessibleTextToUmaWithInaccessiblePdf) {
  MimeHandlerViewGuest* guest_view =
      LoadPdfGetMimeHandlerView(embedded_test_server()->GetURL(
          "/pdf/accessibility/hello-world-in-image.pdf"));
  ASSERT_TRUE(guest_view);

  WebContents* contents = GetActiveWebContents();
  ASSERT_TRUE(contents);

  base::HistogramTester histograms;
  content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();
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
IN_PROC_BROWSER_TEST_F(PDFExtensionAccessibilityTest,
                       PdfAccessibilityTextRunCrash) {
  content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf_private/accessibility_crash_2.pdf"));
  ASSERT_TRUE(guest);

  WaitForAccessibilityTreeToContainNodeWithName(GetActiveWebContents(),
                                                "Page 1");
}
#endif

// This test suite does a simple text-extraction based on the accessibility
// internals, breaking lines & paragraphs where appropriate.  Unlike
// TreeDumpTests, this allows us to verify the kNextOnLine and kPreviousOnLine
// relationships.
class PDFExtensionAccessibilityTextExtractionTest
    : public PDFExtensionAccessibilityTest {
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
  std::vector<base::test::FeatureRef> GetEnabledFeatures() const override {
    auto enabled = PDFExtensionAccessibilityTest::GetEnabledFeatures();
    enabled.push_back(chrome_pdf::features::kAccessiblePDFForm);
    return enabled;
  }

 private:
  void RunTest(const base::FilePath& test_file_path, const char* file_dir) {
    // Load the expectation file.
    ui::AXInspectTestHelper test_helper("content");
    absl::optional<base::FilePath> expected_file_path =
        test_helper.GetExpectationFilePath(test_file_path);
    ASSERT_TRUE(expected_file_path) << "No expectation file present.";

    absl::optional<std::vector<std::string>> expected_lines =
        test_helper.LoadExpectationFile(*expected_file_path);
    ASSERT_TRUE(expected_lines) << "Couldn't load expectation file.";

    // Enable accessibility and load the test file.
    content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();
    MimeHandlerViewGuest* guest =
        LoadPdfGetMimeHandlerView(embedded_test_server()->GetURL(
            "/" + std::string(file_dir) + "/" +
            test_file_path.BaseName().MaybeAsASCII()));
    ASSERT_TRUE(guest);
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
      base::StringPiece trimmed_name =
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
                   const base::flat_set<ax::mojom::Role> roles,
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
IN_PROC_BROWSER_TEST_F(PDFExtensionAccessibilityTextExtractionTest,
                       NextOnLine) {
  RunTextExtractionTest(FILE_PATH_LITERAL("next-on-line.pdf"));
}

// Test that a drop-cap is grouped with the correct line.
IN_PROC_BROWSER_TEST_F(PDFExtensionAccessibilityTextExtractionTest, DropCap) {
  RunTextExtractionTest(FILE_PATH_LITERAL("drop-cap.pdf"));
}

// Test that simulated superscripts and subscripts don't cause a line break.
IN_PROC_BROWSER_TEST_F(PDFExtensionAccessibilityTextExtractionTest,
                       SuperscriptSubscript) {
  RunTextExtractionTest(FILE_PATH_LITERAL("superscript-subscript.pdf"));
}

// Test that simple font and font-size changes in the middle of a line don't
// cause line breaks.
IN_PROC_BROWSER_TEST_F(PDFExtensionAccessibilityTextExtractionTest,
                       FontChange) {
  RunTextExtractionTest(FILE_PATH_LITERAL("font-change.pdf"));
}

// Test one property of pdf_private/accessibility_crash_2.pdf, where a page has
// only whitespace characters.
IN_PROC_BROWSER_TEST_F(PDFExtensionAccessibilityTextExtractionTest,
                       OnlyWhitespaceText) {
  RunTextExtractionTest(FILE_PATH_LITERAL("whitespace.pdf"));
}

// Test data of inline text boxes for PDF with weblinks.
IN_PROC_BROWSER_TEST_F(PDFExtensionAccessibilityTextExtractionTest, WebLinks) {
  RunTextExtractionTest(FILE_PATH_LITERAL("weblinks.pdf"));
}

// Test data of inline text boxes for PDF with highlights.
IN_PROC_BROWSER_TEST_F(PDFExtensionAccessibilityTextExtractionTest,
                       Highlights) {
  RunTextExtractionTest(FILE_PATH_LITERAL("highlights.pdf"));
}

// Test data of inline text boxes for PDF with text fields.
IN_PROC_BROWSER_TEST_F(PDFExtensionAccessibilityTextExtractionTest,
                       TextFields) {
  RunTextExtractionTest(FILE_PATH_LITERAL("text_fields.pdf"));
}

// Test data of inline text boxes for PDF with multi-line and various font-sized
// text.
IN_PROC_BROWSER_TEST_F(PDFExtensionAccessibilityTextExtractionTest,
                       ParagraphsAndHeadingUntagged) {
  RunTextExtractionTest(
      FILE_PATH_LITERAL("paragraphs-and-heading-untagged.pdf"));
}

// Test data of inline text boxes for PDF with text, weblinks, images and
// annotation links.
IN_PROC_BROWSER_TEST_F(PDFExtensionAccessibilityTextExtractionTest,
                       LinksImagesAndText) {
  RunTextExtractionTest(FILE_PATH_LITERAL("text-image-link.pdf"));
}

// Test data of inline text boxes for PDF with overlapping annotations.
IN_PROC_BROWSER_TEST_F(PDFExtensionAccessibilityTextExtractionTest,
                       OverlappingAnnots) {
  RunTextExtractionTest(FILE_PATH_LITERAL("overlapping-annots.pdf"));
}

class PDFExtensionAccessibilityTreeDumpTest
    : public PDFExtensionAccessibilityTest,
      public ::testing::WithParamInterface<ui::AXApiType::Type> {
 public:
  PDFExtensionAccessibilityTreeDumpTest() : test_helper_(GetParam()) {}
  ~PDFExtensionAccessibilityTreeDumpTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PDFExtensionAccessibilityTest::SetUpCommandLine(command_line);

    // Each test pass might require custom feature setup
    test_helper_.InitializeFeatureList();
  }

 protected:
  std::vector<base::test::FeatureRef> GetEnabledFeatures() const override {
    auto enabled = PDFExtensionAccessibilityTest::GetEnabledFeatures();
    enabled.push_back(chrome_pdf::features::kAccessiblePDFForm);
    return enabled;
  }

  std::vector<base::test::FeatureRef> GetDisabledFeatures() const override {
    auto disabled = PDFExtensionAccessibilityTest::GetDisabledFeatures();
    // PDF OCR should not modify the dump.
    disabled.push_back(::features::kPdfOcr);
    return disabled;
  }

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
        content::AXInspectFactory::CreateFormatter(GetParam());
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

    absl::optional<std::vector<std::string>> expected_lines =
        test_helper_.LoadExpectationFile(expected_file_path);
    if (!expected_lines) {
      LOG(INFO) << "Skipping this test on this platform.";
      return;
    }

    // Enable accessibility and load the test file.
    content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();
    MimeHandlerViewGuest* guest =
        LoadPdfGetMimeHandlerView(embedded_test_server()->GetURL(
            "/" + std::string(file_dir) + "/" +
            test_file_path.BaseName().MaybeAsASCII()));
    ASSERT_TRUE(guest);
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
      const ::testing::TestParamInfo<ui::AXApiType::Type>& i) const {
    return std::string(i.param);
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         PDFExtensionAccessibilityTreeDumpTest,
                         testing::ValuesIn(GetAXTestValues()),
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

// TODO(https://crbug.com/1172026)
IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest, XfaFields) {
  RunPDFTest(FILE_PATH_LITERAL("xfa_fields.pdf"));
}

// This test suite validates the navigation done using the accessibility client.
using PDFExtensionAccessibilityNavigationTest = PDFExtensionAccessibilityTest;

IN_PROC_BROWSER_TEST_F(PDFExtensionAccessibilityNavigationTest,
                       LinkNavigation) {
  // Enable accessibility and load the test file.
  content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/accessibility/weblinks.pdf"));
  ASSERT_TRUE(guest);
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

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
// This test suite contains simple tests for the PDF OCR feature.
class PDFExtensionAccessibilityPdfOcrTest
    : public PDFExtensionAccessibilityTest {
 public:
  PDFExtensionAccessibilityPdfOcrTest() = default;
  ~PDFExtensionAccessibilityPdfOcrTest() override = default;

 protected:
  std::vector<base::test::FeatureRef> GetEnabledFeatures() const override {
    auto enabled = PDFExtensionAccessibilityTest::GetEnabledFeatures();
    enabled.push_back(::features::kPdfOcr);
    return enabled;
  }

  void ClickPdfOcrToggleButton(MimeHandlerViewGuest* guest_view) {
    content::RenderFrameHost* guest_main_frame =
        guest_view->GetGuestMainFrame();
    ASSERT_TRUE(guest_main_frame);

    ASSERT_TRUE(content::ExecJs(
        guest_main_frame,
        "viewer.shadowRoot.getElementById('toolbar').shadowRoot."
        "getElementById('pdf-ocr-button').click();"));
    ASSERT_TRUE(content::WaitForRenderFrameReady(guest_main_frame));
  }
};

// TODO(b/289010799): Re-enable it when integrating PDF OCR with
// Select-to-Speak.
IN_PROC_BROWSER_TEST_F(PDFExtensionAccessibilityPdfOcrTest,
                       DISABLED_CheckUmaWhenTurnOnPdfOcrFromMoreActions) {
  MimeHandlerViewGuest* guest_view = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(guest_view);

  // Turn on PDF OCR always.
  base::HistogramTester histograms;
  ClickPdfOcrToggleButton(guest_view);

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectUniqueSample(
      "Accessibility.PdfOcr.UserSelection",
      PdfOcrUserSelection::kTurnOnAlwaysFromMoreActions,
      /*expected_bucket_count=*/1);
}

// TODO(b/289010799): Re-enable it when integrating PDF OCR with
// Select-to-Speak.
IN_PROC_BROWSER_TEST_F(PDFExtensionAccessibilityPdfOcrTest,
                       DISABLED_CheckUmaWhenTurnOffPdfOcrFromMoreActions) {
  MimeHandlerViewGuest* guest_view = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(guest_view);

  // Turn on PDF OCR always.
  ClickPdfOcrToggleButton(guest_view);

  // Turn off PDF OCR.
  base::HistogramTester histograms;
  ClickPdfOcrToggleButton(guest_view);

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectUniqueSample("Accessibility.PdfOcr.UserSelection",
                                PdfOcrUserSelection::kTurnOffFromMoreActions,
                                /*expected_bucket_count=*/1);
}
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
