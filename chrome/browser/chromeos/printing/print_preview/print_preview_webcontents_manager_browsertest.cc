// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/print_preview/print_preview_webcontents_manager.h"

#include <memory>
#include <utility>

#include "base/callback_list.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "chrome/browser/chromeos/printing/print_preview/test/mock_print_preview_crosapi.h"
#include "chrome/browser/chromeos/printing/print_preview/test/scoped_print_preview_webcontents_manager_for_testing.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/print_preview_cros.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace {

constexpr char kUrl[] = "data:text/html,<p>test</p>";

}  // namespace

namespace chromeos::printing {

class PrintPreviewWebContentsManagerBrowserTest : public InProcessBrowserTest {
 public:
  PrintPreviewWebContentsManagerBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kPrintPreviewCrosPrimary);
  }
  ~PrintPreviewWebContentsManagerBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    if (!IsServiceAvailable()) {
      return;
    }
#endif

    webcontents_manager_.Initialize();
    scoped_print_preview_web_contents_manager_ =
        std::make_unique<ScopedPrintPreviewWebContentsManagerForTesting>(
            &webcontents_manager_);

// Replace the production PrintPreviewCrosDelegate with a mock for testing
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    webcontents_manager_.BindPrintPreviewCrosDelegateForTesting(
        receiver_.BindNewPipeAndPassRemote());
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
    webcontents_manager_.SetPrintPreviewCrosDelegateForTesting(
        &browser_delegate_);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  bool IsServiceAvailable() const {
    chromeos::LacrosService* lacros_service = chromeos::LacrosService::Get();
    return lacros_service &&
           lacros_service
               ->IsAvailable<crosapi::mojom::PrintPreviewCrosDelegate>();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Create a new tab in the browser, which also creates a new webcontent.
  void CreateNewTab() {
    EXPECT_TRUE(
        AddTabAtIndex(/*index=*/0, GURL(kUrl), ui::PAGE_TRANSITION_TYPED));
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  testing::StrictMock<MockPrintPreviewCrosapi> browser_delegate_;
  mojo::Receiver<crosapi::mojom::PrintPreviewCrosDelegate> receiver_{
      &browser_delegate_};

  PrintPreviewWebcontentsManager webcontents_manager_;
  std::unique_ptr<ScopedPrintPreviewWebContentsManagerForTesting>
      scoped_print_preview_web_contents_manager_;
};

IN_PROC_BROWSER_TEST_F(PrintPreviewWebContentsManagerBrowserTest,
                       RequestAndClosePreview) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If `PrintPreviewCrosDelegate` interface is not available on ash-chrome,
  // this test suite will no-op.
  if (!IsServiceAvailable()) {
    GTEST_SKIP();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  const auto expected_token = base::UnguessableToken::Create();
  base::RunLoop run_loop1;
  // Expects that `PrintPreviewCrosDelegate` should receive request.
  EXPECT_CALL(browser_delegate_, RequestPrintPreview)
      .WillOnce([&run_loop1, &expected_token](
                    const base::UnguessableToken& token,
                    ::printing::mojom::RequestPrintPreviewParamsPtr params,
                    base::OnceCallback<void(bool)> callback) {
        EXPECT_EQ(expected_token, token);
        EXPECT_TRUE(params->is_modifiable);
        EXPECT_TRUE(params->has_selection);
        std::move(callback).Run(/*success=*/true);
        run_loop1.Quit();
      });

  // Create a new tab and grab its webcontents.
  CreateNewTab();
  content::WebContents* webcontents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ::printing::mojom::RequestPrintPreviewParamsPtr params_ptr =
      ::printing::mojom::RequestPrintPreviewParams::New();
  params_ptr->is_modifiable = true;
  params_ptr->has_selection = true;
  webcontents_manager_.RequestPrintPreview(expected_token, webcontents,
                                           std::move(params_ptr));
  run_loop1.Run();

  // Now simulate closing print preview.
  base::RunLoop run_loop2;
  // Expects that `PrintPreviewCrosDelegate` should receive request.
  EXPECT_CALL(browser_delegate_, PrintPreviewDone)
      .WillOnce([&run_loop2, &expected_token](
                    const base::UnguessableToken& token,
                    base::OnceCallback<void(bool)> callback) {
        EXPECT_EQ(expected_token, token);
        std::move(callback).Run(/*success=*/true);
        run_loop2.Quit();
      });

  webcontents_manager_.PrintPreviewDone(expected_token);
  run_loop2.Run();
}

}  // namespace chromeos::printing
