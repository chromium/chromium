// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/mahi/mahi_web_contents_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "base/callback_list.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/mahi/mahi_browser_util.h"
#include "chrome/browser/chromeos/mahi/test/fake_mahi_web_contents_manager.h"
#include "chrome/browser/chromeos/mahi/test/mock_mahi_crosapi.h"
#include "chrome/browser/chromeos/mahi/test/scoped_mahi_web_contents_manager_for_testing.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/components/mahi/public/cpp/mahi_util.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/mahi.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/test/scoped_feature_list.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_init_params.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace mahi {

namespace {

using chromeos::mahi::ButtonType;
using chromeos::mahi::kMahiContentExtractionTriggeringLatency;
using chromeos::mahi::kMahiContextMenuActivated;
using chromeos::mahi::kMahiContextMenuActivatedFailed;

// Fake context menu click action.
constexpr int64_t kDisplayID = 1;
constexpr ButtonType kButtonType = ButtonType::kQA;
constexpr char16_t kQuestion[] = u"dump question";

// Fake web content.
constexpr char kUrl[] = "data:text/html,<p>kittens!</p>";

constexpr char kPDFFilename[] = "paragraphs-and-heading-untagged.pdf";

}  // namespace

class MahiWebContentsManagerBrowserTest : public InProcessBrowserTest {
 public:
  MahiWebContentsManagerBrowserTest() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    scoped_feature_list_.InitWithFeatures({chromeos::features::kMahi}, {});
#endif
  }
  ~MahiWebContentsManagerBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->StartAcceptingConnections();

    InProcessBrowserTest::SetUpOnMainThread();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // If `MahiBrowserDelegate` interface is not available on ash-chrome, this
    // test suite will no-op.
    if (!IsServiceAvailable()) {
      return;
    }
#endif

    fake_mahi_web_contents_manager_ =
        std::make_unique<FakeMahiWebContentsManager>();
    fake_mahi_web_contents_manager_->Initialize();
    scoped_mahi_web_contents_manager_ =
        std::make_unique<ScopedMahiWebContentsManagerForTesting>(
            fake_mahi_web_contents_manager_.get());

// Replace the production Mahi browser delegate with a mock for testing
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    fake_mahi_web_contents_manager_->BindMahiBrowserDelegateForTesting(
        receiver_.BindNewPipeAndPassRemote());
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
    fake_mahi_web_contents_manager_->SetMahiBrowserDelegateForTesting(
        &browser_delegate_);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  }

  // InProcessBrowserTest:
  void TearDownOnMainThread() override {
    scoped_mahi_web_contents_manager_.reset();
    fake_mahi_web_contents_manager_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    crosapi::mojom::BrowserInitParamsPtr init_params =
        chromeos::BrowserInitParams::GetForTests()->Clone();
    init_params->is_mahi_enabled = true;
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
    InProcessBrowserTest::CreatedBrowserMainParts(browser_main_parts);
  }

  bool IsServiceAvailable() const {
    chromeos::LacrosService* lacros_service = chromeos::LacrosService::Get();
    return lacros_service &&
           lacros_service->IsAvailable<crosapi::mojom::MahiBrowserDelegate>();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Simulates opening a new tab with url.
  void CreateWebContent() {
    // Simulates chrome open.
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab/")));
    // Then navigates to the target page.
    EXPECT_TRUE(AddTabAtIndex(0, GURL(kUrl), ui::PAGE_TRANSITION_TYPED));
  }

  void CreateWebContentWithPDF() {
    GURL pdf_url = embedded_test_server()->GetURL(
        base::StrCat({"/pdf/accessibility/", kPDFFilename}));

    // Simulates chrome open.
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab/")));
    EXPECT_TRUE(AddTabAtIndex(0, pdf_url, ui::PAGE_TRANSITION_TYPED));
  }

  void ExpectOnContextMenuClicked(bool success, ButtonType button_type) {
    base::RunLoop run_loop;
    EXPECT_CALL(browser_delegate_, OnContextMenuClicked)
        .WillOnce([&run_loop, success](
                      crosapi::mojom::MahiContextMenuRequestPtr request,
                      base::OnceCallback<void(bool)> callback) {
          std::move(callback).Run(/*success=*/success);
          run_loop.Quit();
        });
    {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      base::RunLoop run_loop_for_remote;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
      fake_mahi_web_contents_manager_->OnContextMenuClicked(
          kDisplayID, button_type,
          /*question=*/kQuestion, /*mahi_menu_bounds=*/gfx::Rect());
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      run_loop_for_remote.RunUntilIdle();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
    }

    run_loop.Run();
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::test::ScopedFeatureList scoped_feature_list_;
#endif

  testing::StrictMock<MockMahiCrosapi> browser_delegate_;
  mojo::Receiver<crosapi::mojom::MahiBrowserDelegate> receiver_{
      &browser_delegate_};

  std::unique_ptr<FakeMahiWebContentsManager> fake_mahi_web_contents_manager_;
  std::unique_ptr<ScopedMahiWebContentsManagerForTesting>
      scoped_mahi_web_contents_manager_;
};

IN_PROC_BROWSER_TEST_F(MahiWebContentsManagerBrowserTest,
                       OnContextMenuClicked) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If `MahiBrowserDelegate` interface is not available on ash-chrome, this
  // test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  base::RunLoop run_loop;
  // Expects that `MahiBrowserDelegate` should receive the context menu click
  // action.
  EXPECT_CALL(browser_delegate_, OnContextMenuClicked)
      .WillOnce([&run_loop](crosapi::mojom::MahiContextMenuRequestPtr request,
                            base::OnceCallback<void(bool)> callback) {
        EXPECT_EQ(kDisplayID, request->display_id);
        EXPECT_EQ(MatchButtonTypeToActionType(kButtonType),
                  request->action_type);
        EXPECT_EQ(kQuestion, request->question);
        std::move(callback).Run(/*success=*/true);
        run_loop.Quit();
      });

  fake_mahi_web_contents_manager_->OnContextMenuClicked(
      kDisplayID, kButtonType, kQuestion, /*mahi_menu_bounds=*/gfx::Rect());
  run_loop.Run();

  EXPECT_EQ(GURL(),
            fake_mahi_web_contents_manager_->focused_web_content_state().url);
  EXPECT_EQ(u"",
            fake_mahi_web_contents_manager_->focused_web_content_state().title);
}

IN_PROC_BROWSER_TEST_F(MahiWebContentsManagerBrowserTest,
                       PDFContentIsDetectedCorrectly) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If `MahiBrowserDelegate` interface is not available on ash-chrome, this
  // test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  base::HistogramTester histogram;

  base::RunLoop run_loop;
  // Expects that `MahiBrowserDelegate` should receive the focused page change.
  EXPECT_CALL(browser_delegate_, OnFocusedPageChanged)
      // When browser opens with `chrome://newtab`, we should be notified to
      // clear the previous focus info.
      .WillOnce([](crosapi::mojom::MahiPageInfoPtr page_info,
                   base::OnceCallback<void(bool)> callback) {
        EXPECT_EQ(GURL(), page_info->url);
        EXPECT_FALSE(page_info->IsDistillable.has_value());
        std::move(callback).Run(/*success=*/true);
      })
      // When a PDF is opened, the `MahiBrowserDelegate` should be
      // notified without the distillability check.
      .WillOnce([&run_loop, &histogram](
                    crosapi::mojom::MahiPageInfoPtr page_info,
                    base::OnceCallback<void(bool)> callback) {
        EXPECT_TRUE(page_info->IsDistillable.has_value());
        EXPECT_EQ(page_info->url.ExtractFileName(), kPDFFilename);
        std::move(callback).Run(/*success=*/true);
        run_loop.Quit();
        // Since there is no distillability check for PDFs, triggering metric is
        // not logged.
        histogram.ExpectTotalCount(kMahiContentExtractionTriggeringLatency, 0);
      });

  CreateWebContentWithPDF();
  run_loop.Run();
  EXPECT_TRUE(fake_mahi_web_contents_manager_->is_pdf_focused_web_contents());
}

IN_PROC_BROWSER_TEST_F(MahiWebContentsManagerBrowserTest,
                       OpenNewPageToChangePageFocus) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If `MahiBrowserDelegate` interface is not available on ash-chrome, this
  // test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  base::HistogramTester histogram;

  // Initially, the focused state's favicon is empty.
  EXPECT_TRUE(fake_mahi_web_contents_manager_->focused_web_content_state()
                  .favicon.isNull());

  base::RunLoop run_loop;
  // Expects that `MahiBrowserDelegate` should receive the focused page change.
  EXPECT_CALL(browser_delegate_, OnFocusedPageChanged)
      // When browser opens with `chrome://newtab`, we should be notified to
      // clear the previous focus info.
      .WillOnce([](crosapi::mojom::MahiPageInfoPtr page_info,
                   base::OnceCallback<void(bool)> callback) {
        EXPECT_EQ(GURL(), page_info->url);
        EXPECT_FALSE(page_info->IsDistillable.has_value());
        std::move(callback).Run(/*success=*/true);
      })
      // When a new page gets focus, the `MahiBrowserDelegate` should be
      // notified without the distillability check.
      .WillOnce([&histogram](crosapi::mojom::MahiPageInfoPtr page_info,
                             base::OnceCallback<void(bool)> callback) {
        EXPECT_EQ(GURL(kUrl), page_info->url);
        EXPECT_FALSE(page_info->IsDistillable.has_value());
        std::move(callback).Run(/*success=*/true);
        // Before distillability check finishes, triggering metric is not
        // logged.
        histogram.ExpectTotalCount(kMahiContentExtractionTriggeringLatency, 0);
      })
      // When the focused page finishes loading, the `MahiBrowserDelegate`
      // should be notified with the distillability check.
      .WillOnce([&run_loop, &histogram](
                    crosapi::mojom::MahiPageInfoPtr page_info,
                    base::OnceCallback<void(bool)> callback) {
        EXPECT_EQ(GURL(kUrl), page_info->url);
        EXPECT_TRUE(page_info->IsDistillable.has_value());
        EXPECT_FALSE(page_info->IsDistillable.value());
        // The favicon is not empty.
        EXPECT_FALSE(page_info->favicon_image.isNull());

        std::move(callback).Run(/*success=*/true);
        run_loop.Quit();

        // When distillability check finishes, triggering metric is logged.
        histogram.ExpectTotalCount(kMahiContentExtractionTriggeringLatency, 1);
      });

  CreateWebContent();
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(MahiWebContentsManagerBrowserTest, GetPageContents) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If `MahiBrowserDelegate` interface is not available on ash-chrome, this
  // test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Initially, the focused state and the requested state should be different.
  base::UnguessableToken focused_page_id =
      fake_mahi_web_contents_manager_->focused_web_content_state().page_id;

  // First create a web page so there is a place to extract the contents from.
  base::RunLoop run_loop;
  EXPECT_CALL(browser_delegate_, OnFocusedPageChanged)
      .WillOnce([](crosapi::mojom::MahiPageInfoPtr page_info,
                   base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(/*success=*/true);
      })
      .WillOnce([](crosapi::mojom::MahiPageInfoPtr page_info,
                   base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(/*success=*/true);
      })
      .WillOnce([&run_loop, &focused_page_id, this](
                    crosapi::mojom::MahiPageInfoPtr page_info,
                    base::OnceCallback<void(bool)> callback) {
        EXPECT_TRUE(page_info->IsDistillable.has_value());
        EXPECT_FALSE(page_info->IsDistillable.value());
        std::move(callback).Run(/*success=*/true);

        // Gets the page id of the newly opened page.
        focused_page_id = page_info->page_id;
        // When distillability check is returned, simulates the content request
        // from the mahi manager.
        fake_mahi_web_contents_manager_->RequestContentFromPage(
            focused_page_id,
            base::BindLambdaForTesting(
                [&](crosapi::mojom::MahiPageContentPtr page_content) {
                  run_loop.Quit();
                }));
      });
  CreateWebContent();
  run_loop.Run();

  EXPECT_EQ(
      focused_page_id,
      fake_mahi_web_contents_manager_->focused_web_content_state().page_id);
  EXPECT_EQ(GURL(kUrl),
            fake_mahi_web_contents_manager_->focused_web_content_state().url);
  EXPECT_EQ(u"data:text/html,<p>kittens!</p>",
            fake_mahi_web_contents_manager_->focused_web_content_state().title);
}

IN_PROC_BROWSER_TEST_F(MahiWebContentsManagerBrowserTest,
                       DISABLED_GetPDFContents) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If `MahiBrowserDelegate` interface is not available on ash-chrome, this
  // test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Initially, the focused state and the requested state should be different.
  base::UnguessableToken focused_page_id =
      fake_mahi_web_contents_manager_->focused_web_content_state().page_id;

  // First create a web page so there is a place to extract the contents from.
  base::RunLoop run_loop;
  EXPECT_CALL(browser_delegate_, OnFocusedPageChanged)
      .WillOnce([](crosapi::mojom::MahiPageInfoPtr page_info,
                   base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(/*success=*/true);
      })
      .WillOnce([&run_loop, &focused_page_id, this](
                    crosapi::mojom::MahiPageInfoPtr page_info,
                    base::OnceCallback<void(bool)> callback) {
        EXPECT_TRUE(page_info->IsDistillable.has_value());
        EXPECT_TRUE(page_info->IsDistillable.value());
        EXPECT_EQ(page_info->url.ExtractFileName(), kPDFFilename);
        std::move(callback).Run(/*success=*/true);

        focused_page_id = page_info->page_id;
        // Simulate a request to extract content from client.
        fake_mahi_web_contents_manager_->RequestContentFromPage(
            focused_page_id,
            base::BindLambdaForTesting(
                [&](crosapi::mojom::MahiPageContentPtr page_content) {
                  run_loop.Quit();
                }));
      });
  CreateWebContentWithPDF();

  EXPECT_EQ(
      focused_page_id,
      fake_mahi_web_contents_manager_->focused_web_content_state().page_id);
  EXPECT_EQ(fake_mahi_web_contents_manager_->focused_web_content_state()
                .url.ExtractFileName(),
            kPDFFilename);
}

IN_PROC_BROWSER_TEST_F(MahiWebContentsManagerBrowserTest, ContextMenuMetrics) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If `MahiBrowserDelegate` interface is not available on ash-chrome, this
  // test suite will no-op.
  if (!IsServiceAvailable()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  base::HistogramTester histogram;

  histogram.ExpectBucketCount(kMahiContextMenuActivated, ButtonType::kSettings,
                              0);
  histogram.ExpectBucketCount(kMahiContextMenuActivated, ButtonType::kOutline,
                              0);
  histogram.ExpectBucketCount(kMahiContextMenuActivated, ButtonType::kSummary,
                              0);
  histogram.ExpectBucketCount(kMahiContextMenuActivated, ButtonType::kQA, 0);
  histogram.ExpectBucketCount(kMahiContextMenuActivatedFailed,
                              ButtonType::kSettings, 0);
  histogram.ExpectBucketCount(kMahiContextMenuActivatedFailed,
                              ButtonType::kOutline, 0);
  histogram.ExpectBucketCount(kMahiContextMenuActivatedFailed,
                              ButtonType::kSummary, 0);
  histogram.ExpectBucketCount(kMahiContextMenuActivatedFailed, ButtonType::kQA,
                              0);

  // QA section.
  // With a successful click.
  ExpectOnContextMenuClicked(true, ButtonType::kQA);
  histogram.ExpectBucketCount(kMahiContextMenuActivated, ButtonType::kQA, 1);
  histogram.ExpectBucketCount(kMahiContextMenuActivatedFailed, ButtonType::kQA,
                              0);
  histogram.ExpectTotalCount(kMahiContextMenuActivated, 1);
  histogram.ExpectTotalCount(kMahiContextMenuActivatedFailed, 0);
  testing::Mock::VerifyAndClearExpectations(this);
  // Clicking failed.
  ExpectOnContextMenuClicked(false, ButtonType::kQA);
  histogram.ExpectBucketCount(kMahiContextMenuActivated, ButtonType::kQA, 2);
  histogram.ExpectBucketCount(kMahiContextMenuActivatedFailed, ButtonType::kQA,
                              1);
  histogram.ExpectTotalCount(kMahiContextMenuActivated, 2);
  histogram.ExpectTotalCount(kMahiContextMenuActivatedFailed, 1);
  testing::Mock::VerifyAndClearExpectations(this);

  // Outline.
  // With a successful click.
  ExpectOnContextMenuClicked(true, ButtonType::kOutline);
  histogram.ExpectBucketCount(kMahiContextMenuActivated, ButtonType::kOutline,
                              1);
  histogram.ExpectBucketCount(kMahiContextMenuActivatedFailed,
                              ButtonType::kOutline, 0);
  histogram.ExpectTotalCount(kMahiContextMenuActivated, 3);
  histogram.ExpectTotalCount(kMahiContextMenuActivatedFailed, 1);
  testing::Mock::VerifyAndClearExpectations(this);
  // Clicking failed.
  ExpectOnContextMenuClicked(false, ButtonType::kOutline);
  histogram.ExpectBucketCount(kMahiContextMenuActivated, ButtonType::kOutline,
                              2);
  histogram.ExpectBucketCount(kMahiContextMenuActivatedFailed,
                              ButtonType::kOutline, 1);
  histogram.ExpectTotalCount(kMahiContextMenuActivated, 4);
  histogram.ExpectTotalCount(kMahiContextMenuActivatedFailed, 2);
  testing::Mock::VerifyAndClearExpectations(this);

  // Summary button.
  // With a successful click.
  ExpectOnContextMenuClicked(true, ButtonType::kSummary);
  histogram.ExpectBucketCount(kMahiContextMenuActivated, ButtonType::kSummary,
                              1);
  histogram.ExpectBucketCount(kMahiContextMenuActivatedFailed,
                              ButtonType::kSummary, 0);
  histogram.ExpectTotalCount(kMahiContextMenuActivated, 5);
  histogram.ExpectTotalCount(kMahiContextMenuActivatedFailed, 2);
  testing::Mock::VerifyAndClearExpectations(this);
  // Clicking failed.
  ExpectOnContextMenuClicked(false, ButtonType::kSummary);
  histogram.ExpectBucketCount(kMahiContextMenuActivated, ButtonType::kSummary,
                              2);
  histogram.ExpectBucketCount(kMahiContextMenuActivatedFailed,
                              ButtonType::kSummary, 1);
  histogram.ExpectTotalCount(kMahiContextMenuActivated, 6);
  histogram.ExpectTotalCount(kMahiContextMenuActivatedFailed, 3);
  testing::Mock::VerifyAndClearExpectations(this);

  // Settings button.
  // With a successful click.
  ExpectOnContextMenuClicked(true, ButtonType::kSettings);
  histogram.ExpectBucketCount(kMahiContextMenuActivated, ButtonType::kSettings,
                              1);
  histogram.ExpectBucketCount(kMahiContextMenuActivatedFailed,
                              ButtonType::kSettings, 0);
  histogram.ExpectTotalCount(kMahiContextMenuActivated, 7);
  histogram.ExpectTotalCount(kMahiContextMenuActivatedFailed, 3);
  testing::Mock::VerifyAndClearExpectations(this);
  // Clicking failed.
  ExpectOnContextMenuClicked(false, ButtonType::kSettings);
  histogram.ExpectBucketCount(kMahiContextMenuActivated, ButtonType::kSettings,
                              2);
  histogram.ExpectBucketCount(kMahiContextMenuActivatedFailed,
                              ButtonType::kSettings, 1);
  histogram.ExpectTotalCount(kMahiContextMenuActivated, 8);
  histogram.ExpectTotalCount(kMahiContextMenuActivatedFailed, 4);
  testing::Mock::VerifyAndClearExpectations(this);
}

}  // namespace mahi
