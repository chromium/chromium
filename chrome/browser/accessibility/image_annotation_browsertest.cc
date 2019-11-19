// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/accessibility/accessibility_labels_service.h"
#include "chrome/browser/accessibility/accessibility_labels_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/image_annotation/public/cpp/image_processor.h"
#include "services/image_annotation/public/mojom/image_annotation.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_tree.h"
#include "url/gurl.h"

constexpr base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("chrome/test/data/accessibility");

namespace {

void DescribeNodesWithAnnotations(const ui::AXNode& node,
                                  std::vector<std::string>* descriptions) {
  std::string annotation =
      node.GetStringAttribute(ax::mojom::StringAttribute::kImageAnnotation);
  if (!annotation.empty()) {
    descriptions->push_back(ui::ToString(node.data().role) + std::string(" ") +
                            annotation);
  }
  for (const auto* child : node.children())
    DescribeNodesWithAnnotations(*child, descriptions);
}

std::vector<std::string> DescribeNodesWithAnnotations(
    const ui::AXTreeUpdate& tree_update) {
  ui::AXTree tree(tree_update);
  std::vector<std::string> descriptions;
  DCHECK(tree.root());
  DescribeNodesWithAnnotations(*tree.root(), &descriptions);
  return descriptions;
}

bool HasNodeWithAnnotationStatus(const ui::AXTreeUpdate& tree_update,
                                 ax::mojom::ImageAnnotationStatus status) {
  for (const auto& node_data : tree_update.nodes) {
    if (node_data.GetImageAnnotationStatus() == status)
      return true;
  }
  return false;
}

// A fake implementation of the Annotator mojo interface that
// returns predictable results based on the filename of the image
// it's asked to annotate. Enables us to test the rest of the
// system without using the real annotator that queries a back-end
// API.
class FakeAnnotator : public image_annotation::mojom::Annotator {
 public:
  static void SetReturnOcrResults(bool ocr) { return_ocr_results_ = ocr; }

  static void SetReturnLabelResults(bool label) {
    return_label_results_ = label;
  }

  static void SetReturnErrorCode(
      image_annotation::mojom::AnnotateImageError error_code) {
    return_error_code_ = error_code;
  }

  FakeAnnotator() = default;
  ~FakeAnnotator() override = default;

  void BindReceiver(
      mojo::PendingReceiver<image_annotation::mojom::Annotator> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  void AnnotateImage(
      const std::string& image_id,
      const std::string& description_language_tag,
      mojo::PendingRemote<image_annotation::mojom::ImageProcessor>
          image_processor,
      AnnotateImageCallback callback) override {
    if (return_error_code_) {
      image_annotation::mojom::AnnotateImageResultPtr result =
          image_annotation::mojom::AnnotateImageResult::NewErrorCode(
              *return_error_code_);
      std::move(callback).Run(std::move(result));
      return;
    }

    // Use the filename to create an annotation string.
    // Adds some trailing whitespace and punctuation to check that clean-up
    // happens correctly when combining annotation strings.
    std::string image_filename = GURL(image_id).ExtractFileName();
    image_annotation::mojom::AnnotationPtr ocr_annotation =
        image_annotation::mojom::Annotation::New(
            image_annotation::mojom::AnnotationType::kOcr, 1.0,
            image_filename + " Annotation . ");

    image_annotation::mojom::AnnotationPtr label_annotation =
        image_annotation::mojom::Annotation::New(
            image_annotation::mojom::AnnotationType::kLabel, 1.0,
            image_filename + " '" + description_language_tag + "' Label");

    // Return enabled results as an annotation.
    std::vector<image_annotation::mojom::AnnotationPtr> annotations;
    if (return_ocr_results_)
      annotations.push_back(std::move(ocr_annotation));
    if (return_label_results_)
      annotations.push_back(std::move(label_annotation));

    image_annotation::mojom::AnnotateImageResultPtr result =
        image_annotation::mojom::AnnotateImageResult::NewAnnotations(
            std::move(annotations));
    std::move(callback).Run(std::move(result));
  }

 private:
  mojo::ReceiverSet<image_annotation::mojom::Annotator> receivers_;
  static bool return_ocr_results_;
  static bool return_label_results_;
  static base::Optional<image_annotation::mojom::AnnotateImageError>
      return_error_code_;

  DISALLOW_COPY_AND_ASSIGN(FakeAnnotator);
};

// static
bool FakeAnnotator::return_ocr_results_ = false;
// static
bool FakeAnnotator::return_label_results_ = false;
// static
base::Optional<image_annotation::mojom::AnnotateImageError>
    FakeAnnotator::return_error_code_;

// The fake ImageAnnotationService, which handles mojo calls from the renderer
// process and passes them to FakeAnnotator.
class FakeImageAnnotationService
    : public image_annotation::mojom::ImageAnnotationService {
 public:
  FakeImageAnnotationService() = default;
  ~FakeImageAnnotationService() override = default;

 private:
  // image_annotation::mojom::ImageAnnotationService:
  void BindAnnotator(mojo::PendingReceiver<image_annotation::mojom::Annotator>
                         receiver) override {
    annotator_.BindReceiver(std::move(receiver));
  }

  FakeAnnotator annotator_;

  DISALLOW_COPY_AND_ASSIGN(FakeImageAnnotationService);
};

void BindImageAnnotatorService(
    mojo::PendingReceiver<image_annotation::mojom::ImageAnnotationService>
        receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<FakeImageAnnotationService>(),
                              std::move(receiver));
}

}  // namespace

class ImageAnnotationBrowserTest : public InProcessBrowserTest {
 public:
  ImageAnnotationBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    https_server_.AddDefaultHandlers(base::FilePath(kDocRoot));
  }

 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kExperimentalAccessibilityLabels);
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(https_server_.Start());

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    AccessibilityLabelsServiceFactory::GetForProfile(browser()->profile())
        ->OverrideImageAnnotatorBinderForTesting(
            base::BindRepeating(&BindImageAnnotatorService));

    ui::AXMode mode = ui::kAXModeComplete;
    mode.set_mode(ui::AXMode::kLabelImages, true);
    web_contents->SetAccessibilityMode(mode);

    SetAcceptLanguages("en,fr");
  }

  void TearDownOnMainThread() override {
    AccessibilityLabelsServiceFactory::GetForProfile(browser()->profile())
        ->OverrideImageAnnotatorBinderForTesting(base::NullCallback());
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SetAcceptLanguages(const std::string& accept_languages) {
    content::BrowserContext* context =
        static_cast<content::BrowserContext*>(browser()->profile());
    DCHECK(context);

    PrefService* prefs = user_prefs::UserPrefs::Get(context);
    DCHECK(prefs);

    prefs->Set(language::prefs::kAcceptLanguages,
               base::Value(accept_languages));
  }

 protected:
  net::EmbeddedTestServer https_server_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ImageAnnotationBrowserTest);
};

IN_PROC_BROWSER_TEST_F(ImageAnnotationBrowserTest,
                       AnnotateImageInAccessibilityTree) {
  FakeAnnotator::SetReturnOcrResults(true);
  FakeAnnotator::SetReturnLabelResults(true);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/image_annotation.html"));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::WaitForAccessibilityTreeToContainNodeWithName(
      web_contents,
      "Appears to say: red.png Annotation. Appears to be: red.png 'en' Label");
}

IN_PROC_BROWSER_TEST_F(ImageAnnotationBrowserTest, ImagesInLinks) {
  FakeAnnotator::SetReturnOcrResults(true);
  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/image_annotation_link.html"));

  // Block until the accessibility tree has at least 8 annotations. If
  // that never happens, the test will time out.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  while (10 > DescribeNodesWithAnnotations(
                  content::GetAccessibilityTreeSnapshot(web_contents))
                  .size()) {
    content::WaitForAccessibilityTreeToChange(web_contents);
  }

  // All images should be annotated. Only links that contain exactly one image
  // should be annotated.
  ui::AXTreeUpdate ax_tree_update =
      content::GetAccessibilityTreeSnapshot(web_contents);
  EXPECT_THAT(
      DescribeNodesWithAnnotations(ax_tree_update),
      testing::ElementsAre("image Appears to say: red.png Annotation",
                           "link Appears to say: green.png Annotation",
                           "image Appears to say: green.png Annotation",
                           "image Appears to say: red.png Annotation",
                           "image Appears to say: printer.png Annotation",
                           "image Appears to say: red.png Annotation",
                           "link Appears to say: printer.png Annotation",
                           "image Appears to say: printer.png Annotation",
                           "link Appears to say: green.png Annotation",
                           "image Appears to say: green.png Annotation"));
}

IN_PROC_BROWSER_TEST_F(ImageAnnotationBrowserTest, ImageDoc) {
  FakeAnnotator::SetReturnOcrResults(true);
  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/image_annotation_doc.html"));

  // Block until the accessibility tree has at least 2 annotations. If
  // that never happens, the test will time out.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  while (2 > DescribeNodesWithAnnotations(
                 content::GetAccessibilityTreeSnapshot(web_contents))
                 .size()) {
    content::WaitForAccessibilityTreeToChange(web_contents);
  }

  // When a document contains exactly one image, the document should be
  // annotated with the image's annotation, too.
  ui::AXTreeUpdate ax_tree_update =
      content::GetAccessibilityTreeSnapshot(web_contents);
  EXPECT_THAT(
      DescribeNodesWithAnnotations(ax_tree_update),
      testing::ElementsAre("rootWebArea Appears to say: red.png Annotation",
                           "image Appears to say: red.png Annotation"));
}

IN_PROC_BROWSER_TEST_F(ImageAnnotationBrowserTest, ImageUrl) {
  FakeAnnotator::SetReturnOcrResults(true);
  ui_test_utils::NavigateToURL(browser(), https_server_.GetURL("/red.png"));

  // Block until the accessibility tree has at least 2 annotations. If
  // that never happens, the test will time out.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  while (2 > DescribeNodesWithAnnotations(
                 content::GetAccessibilityTreeSnapshot(web_contents))
                 .size()) {
    content::WaitForAccessibilityTreeToChange(web_contents);
  }

  // When a document contains exactly one image, the document should be
  // annotated with the image's annotation, too.
  ui::AXTreeUpdate ax_tree_update =
      content::GetAccessibilityTreeSnapshot(web_contents);
  EXPECT_THAT(
      DescribeNodesWithAnnotations(ax_tree_update),
      testing::ElementsAre("rootWebArea Appears to say: red.png Annotation",
                           "image Appears to say: red.png Annotation"));
}

IN_PROC_BROWSER_TEST_F(ImageAnnotationBrowserTest, NoAnnotationsAvailable) {
  // Don't return any results.
  FakeAnnotator::SetReturnOcrResults(false);
  FakeAnnotator::SetReturnLabelResults(false);

  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/image_annotation_doc.html"));

  // Block until the annotation status for the root is empty. If that
  // never occurs then the test will time out.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui::AXTreeUpdate snapshot =
      content::GetAccessibilityTreeSnapshot(web_contents);
  while (snapshot.nodes[0].GetImageAnnotationStatus() !=
         ax::mojom::ImageAnnotationStatus::kAnnotationEmpty) {
    content::WaitForAccessibilityTreeToChange(web_contents);
    snapshot = content::GetAccessibilityTreeSnapshot(web_contents);
  }
}

IN_PROC_BROWSER_TEST_F(ImageAnnotationBrowserTest, AnnotationError) {
  // Return an error code.
  FakeAnnotator::SetReturnErrorCode(
      image_annotation::mojom::AnnotateImageError::kFailure);

  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/image_annotation_doc.html"));

  // Block until the annotation status for the root contains an error code. If
  // that never occurs then the test will time out.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui::AXTreeUpdate snapshot =
      content::GetAccessibilityTreeSnapshot(web_contents);
  while (snapshot.nodes[0].GetImageAnnotationStatus() !=
         ax::mojom::ImageAnnotationStatus::kAnnotationProcessFailed) {
    content::WaitForAccessibilityTreeToChange(web_contents);
    snapshot = content::GetAccessibilityTreeSnapshot(web_contents);
  }
}

IN_PROC_BROWSER_TEST_F(ImageAnnotationBrowserTest, ImageWithSrcSet) {
  FakeAnnotator::SetReturnOcrResults(true);
  FakeAnnotator::SetReturnLabelResults(true);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/image_srcset.html"));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WaitForAccessibilityTreeToContainNodeWithName(
      web_contents,
      "Appears to say: red.png Annotation. Appears to be: red.png 'en' Label");
}

// Disabled due to flakiness. http://crbug.com/983404
IN_PROC_BROWSER_TEST_F(ImageAnnotationBrowserTest,
                       DISABLED_AnnotationLanguages) {
  FakeAnnotator::SetReturnOcrResults(true);
  FakeAnnotator::SetReturnLabelResults(true);

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/image_annotation.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WaitForAccessibilityTreeToContainNodeWithName(
      web_contents,
      "Appears to say: red.png Annotation. Appears to be: red.png 'en' Label");

  SetAcceptLanguages("fr,en");
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/image_annotation.html"));
  web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  content::WaitForAccessibilityTreeToContainNodeWithName(
      web_contents,
      "Appears to say: red.png Annotation. Appears to be: red.png 'fr' Label");
}

IN_PROC_BROWSER_TEST_F(ImageAnnotationBrowserTest,
                       DoesntAnnotateInternalPages) {
  FakeAnnotator::SetReturnLabelResults(true);
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://version"));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui::AXMode mode = ui::kAXModeComplete;
  mode.set_mode(ui::AXMode::kLabelImages, true);
  web_contents->SetAccessibilityMode(mode);
  std::string svg_image =
      "data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg'><circle "
      "cx='50' cy='50' r='40' fill='yellow' /></svg>";
  const std::string javascript =
      "var image = document.createElement('img');"
      "image.src = \"" +
      svg_image +
      "\";"
      "var outer = document.getElementById('outer');"
      "outer.insertBefore(image, outer.childNodes[0]);";
  EXPECT_TRUE(content::ExecuteScript(web_contents, javascript));

  ui::AXTreeUpdate snapshot =
      content::GetAccessibilityTreeSnapshot(web_contents);
  // Wait for the accessibility tree to contain an error that the image cannot
  // be annotated due to the page url's scheme.
  while (!HasNodeWithAnnotationStatus(
      snapshot,
      ax::mojom::ImageAnnotationStatus::kWillNotAnnotateDueToScheme)) {
    content::WaitForAccessibilityTreeToChange(web_contents);
    snapshot = content::GetAccessibilityTreeSnapshot(web_contents);
  }
}

IN_PROC_BROWSER_TEST_F(ImageAnnotationBrowserTest,
                       TutorMessageOnlyOnFirstImage) {
  // We should not promote the image annotation service on more than one image
  // in the same renderer.

  FakeAnnotator::SetReturnOcrResults(false);
  FakeAnnotator::SetReturnLabelResults(false);

  // The following test page should have at least two images on it.
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/image_annotation.html"));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui::AXMode mode = ui::kAXModeComplete;
  mode.set_mode(ui::AXMode::kLabelImages, false);
  web_contents->SetAccessibilityMode(mode);

  // Block until there are at least two images that have been processed. One of
  // them should get the tutor message and the other shouldn't. The annotation
  // status for the image that didn't get the tutor message should be
  // kSilentlyEligibleForAnnotation whilst the status for the image that did
  // should be kEligibleForAnnotation. If that never occurs then the test will
  // time out.
  ui::AXTreeUpdate snapshot =
      content::GetAccessibilityTreeSnapshot(web_contents);
  while (
      !HasNodeWithAnnotationStatus(
          snapshot,
          ax::mojom::ImageAnnotationStatus::kSilentlyEligibleForAnnotation) ||
      !HasNodeWithAnnotationStatus(
          snapshot, ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation)) {
    content::WaitForAccessibilityTreeToChange(web_contents);
    snapshot = content::GetAccessibilityTreeSnapshot(web_contents);
  }
}

IN_PROC_BROWSER_TEST_F(ImageAnnotationBrowserTest,
                       TutorMessageOnlyOnFirstImageInLinks) {
  // We should not promote the image annotation service on more than one image
  // in the same renderer.

  FakeAnnotator::SetReturnOcrResults(false);
  FakeAnnotator::SetReturnLabelResults(false);

  // The following test page should have at least two images on it.
  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/image_annotation_link.html"));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui::AXMode mode = ui::kAXModeComplete;
  mode.set_mode(ui::AXMode::kLabelImages, false);
  web_contents->SetAccessibilityMode(mode);

  // Block until there are at least two images that have been processed. One of
  // them should get the tutor message and the other shouldn't. The annotation
  // status for the image that didn't get the tutor message should be
  // kSilentlyEligibleForAnnotation whilst the status for the image that did
  // should be kEligibleForAnnotation. If that never occurs then the test will
  // time out.
  ui::AXTreeUpdate snapshot =
      content::GetAccessibilityTreeSnapshot(web_contents);
  while (
      !HasNodeWithAnnotationStatus(
          snapshot,
          ax::mojom::ImageAnnotationStatus::kSilentlyEligibleForAnnotation) ||
      !HasNodeWithAnnotationStatus(
          snapshot, ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation)) {
    content::WaitForAccessibilityTreeToChange(web_contents);
    snapshot = content::GetAccessibilityTreeSnapshot(web_contents);
  }
}
