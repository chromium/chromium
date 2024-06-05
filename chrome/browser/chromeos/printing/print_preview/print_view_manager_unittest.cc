// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_view_manager.h"

#include <cstdint>
#include <optional>

#include "base/containers/util.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/printing/print_preview/print_preview_ui_wrapper.h"
#include "chrome/browser/chromeos/printing/print_preview/print_view_manager_cros.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace chromeos {

namespace {

// Construct a fake print settings with the given ID's.
base::Value::Dict GetFakePrintParams(int preview_id, int request_id) {
  base::Value::Dict settings =
      base::Value::Dict()
          .Set(::printing::kSettingLandscape, false)
          .Set(::printing::kSettingCollate, false)
          .Set(::printing::kSettingColor,
               static_cast<int>(::printing::mojom::ColorModel::kGray))
          .Set(::printing::kSettingPrinterType,
               static_cast<int>(::printing::mojom::PrinterType::kPdf))
          .Set(::printing::kSettingDuplexMode,
               static_cast<int>(::printing::mojom::DuplexMode::kSimplex))
          .Set(::printing::kSettingCopies, 1)
          .Set(::printing::kSettingDeviceName, "dummy")
          .Set(::printing::kSettingDpiHorizontal, 72)
          .Set(::printing::kSettingDpiVertical, 72)
          .Set(::printing::kPreviewUIID, preview_id)
          .Set(::printing::kSettingRasterizePdf, false)
          .Set(::printing::kPreviewRequestID, request_id)
          .Set(::printing::kSettingScaleFactor, 100)
          .Set(::printing::kIsFirstRequest, true)
          .Set(::printing::kSettingMarginsType,
               static_cast<int>(::printing::mojom::MarginType::kDefaultMargins))
          .Set(::printing::kSettingPagesPerSheet, 1)
          .Set(::printing::kSettingPreviewModifiable, true)
          .Set(::printing::kSettingPreviewIsFromArc, false)
          .Set(::printing::kSettingHeaderFooterEnabled, false)
          .Set(::printing::kSettingShouldPrintBackgrounds, false)
          .Set(::printing::kSettingShouldPrintSelectionOnly, false);

  // Using a media size with realistic dimensions for a Letter paper.
  auto media_size =
      base::Value::Dict()
          .Set(::printing::kSettingMediaSizeWidthMicrons, 215900)
          .Set(::printing::kSettingMediaSizeHeightMicrons, 279400)
          .Set(::printing::kSettingsImageableAreaLeftMicrons, 12700)
          .Set(::printing::kSettingsImageableAreaBottomMicrons, 0)
          .Set(::printing::kSettingsImageableAreaRightMicrons, 209550)
          .Set(::printing::kSettingsImageableAreaTopMicrons, 254000);
  settings.Set(::printing::kSettingMediaSize, std::move(media_size));
  return settings;
}

}  // namespace

class PrintViewManagerCrosTest : public BrowserWithTestWindowTest {
 public:
  PrintViewManagerCrosTest() = default;
  PrintViewManagerCrosTest(const PrintViewManagerCrosTest&) = delete;
  PrintViewManagerCrosTest& operator=(const PrintViewManagerCrosTest&) = delete;
  ~PrintViewManagerCrosTest() override {}

  // Overridden from testing::Test
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
            ::features::kPrintPreviewCrosPrimary,
        },
        /*disabled_features=*/{});
    BrowserWithTestWindowTest::SetUp();
  }

  void TearDown() override { BrowserWithTestWindowTest::TearDown(); }

  PrintPreviewUiWrapper* get_ui_wrapper(PrintViewManagerCros* view_manager) {
    CHECK(view_manager);
    return view_manager->ui_wrapper_.get();
  }

  std::optional<uint32_t> get_ui_id(PrintPreviewUiWrapper* ui_wrapper) {
    CHECK(ui_wrapper);
    return ui_wrapper->id_;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PrintViewManagerCrosTest, UseCrosViewManager) {
  chrome::NewTab(browser());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('p'),
                   ui::DomCode::US_P, ui::VKEY_P, /*control=*/true,
                   /*shift=*/false, /*alt=*/false, /*command=*/false);
  ASSERT_TRUE(PrintViewManagerCros::FromWebContents(web_contents));
  ASSERT_FALSE(::printing::PrintViewManager::FromWebContents(web_contents));
}

TEST_F(PrintViewManagerCrosTest, UseBrowserViewManager) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{::features::kPrintPreviewCrosPrimary});
  chrome::NewTab(browser());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('p'),
                   ui::DomCode::US_P, ui::VKEY_P, /*control=*/true,
                   /*shift=*/false, /*alt=*/false, /*command=*/false);
  ASSERT_FALSE(PrintViewManagerCros::FromWebContents(web_contents));
  ASSERT_TRUE(::printing::PrintViewManager::FromWebContents(web_contents));
}

TEST_F(PrintViewManagerCrosTest, PrintPreviewNow) {
  chrome::NewTab(browser());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('p'),
                   ui::DomCode::US_P, ui::VKEY_P, /*control=*/true,
                   /*shift=*/false, /*alt=*/false, /*command=*/false);
  auto* view_manager = PrintViewManagerCros::FromWebContents(web_contents);
  ASSERT_TRUE(view_manager);

  content::RenderFrameHost* rfh = web_contents->GetPrimaryMainFrame();
  ASSERT_TRUE(rfh);
  view_manager->PrintPreviewNow(rfh, /*has_selection=*/true);
  EXPECT_EQ(rfh, view_manager->render_frame_host_for_testing());
  view_manager->PrintPreviewDone();
  // After cleaning up the rfh pointer, assert it is null and not a dangling
  // pointer.
  ASSERT_FALSE(view_manager->render_frame_host_for_testing());
}

TEST_F(PrintViewManagerCrosTest, StartGetPreview) {
  chrome::NewTab(browser());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('p'),
                   ui::DomCode::US_P, ui::VKEY_P, /*control=*/true,
                   /*shift=*/false, /*alt=*/false, /*command=*/false);
  auto* view_manager = PrintViewManagerCros::FromWebContents(web_contents);
  ASSERT_TRUE(view_manager);

  content::RenderFrameHost* rfh = web_contents->GetPrimaryMainFrame();
  ASSERT_TRUE(rfh);
  view_manager->PrintPreviewNow(rfh, /*has_selection=*/true);
  EXPECT_EQ(rfh, view_manager->render_frame_host_for_testing());

  // Confirm UI wrapper exists.
  PrintPreviewUiWrapper* ui_wrapper = get_ui_wrapper(view_manager);
  EXPECT_TRUE(ui_wrapper);

  std::optional<uint32_t> ui_id = get_ui_id(ui_wrapper);
  EXPECT_TRUE(ui_id);

  const int request_id = 0;
  // Technically there is no print preview requested, so it can be cancelled.
  EXPECT_TRUE(PrintPreviewUiWrapper::ShouldCancelRequest(ui_id, request_id));

  // Start the preview generation.
  view_manager->HandleGeneratePrintPreview(
      GetFakePrintParams(static_cast<int>(*ui_id), request_id));

  base::RunLoop().RunUntilIdle();

  // Now that the preview is processed, we shouldn't cancel it.
  EXPECT_FALSE(PrintPreviewUiWrapper::ShouldCancelRequest(ui_id, request_id));

  view_manager->PrintPreviewDone();
  // After cleaning up the rfh pointer, assert it is null and not a dangling
  // pointer.
  ASSERT_FALSE(view_manager->render_frame_host_for_testing());
}

}  //  namespace chromeos
