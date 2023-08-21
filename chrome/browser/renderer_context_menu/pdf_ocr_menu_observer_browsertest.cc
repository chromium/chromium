// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/pdf_ocr_menu_observer.h"

#include "base/test/metrics/histogram_tester.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/mock_render_view_context_menu.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#else
#include "content/public/test/scoped_accessibility_mode_override.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

// A test class for the PDF OCR item in the Context Menu. This test should be
// a browser test as it accesses resources.
class PdfOcrMenuObserverTest : public InProcessBrowserTest {
 public:
  PdfOcrMenuObserverTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kPdfOcr);
  }

  void SetUpOnMainThread() override { Reset(false); }
  void TearDownOnMainThread() override {
    // TODO(crbug.com/1401757): Clear an observer from menu before resetting.
    // That way, we can prevent from having a dangling pointer to the reset
    // observer.
    observer_.reset();
    menu_.reset();
  }

  void Reset(bool incognito) {
    // TODO(crbug.com/1401757): Clear an observer from menu before resetting.
    // That way, we can prevent from having a dangling pointer to observer.
    observer_.reset();
    menu_ = std::make_unique<MockRenderViewContextMenu>(incognito);
    observer_ = std::make_unique<PdfOcrMenuObserver>(menu_.get());
    menu_->SetObserver(observer_.get());
  }

  void InitMenu() {
    content::ContextMenuParams params;
    observer_->InitMenu(params);
  }

  PdfOcrMenuObserverTest(const PdfOcrMenuObserverTest&) = delete;
  PdfOcrMenuObserverTest& operator=(const PdfOcrMenuObserverTest&) = delete;
  ~PdfOcrMenuObserverTest() override;
  MockRenderViewContextMenu* menu() { return menu_.get(); }
  PdfOcrMenuObserver* observer() { return observer_.get(); }

 private:
  std::unique_ptr<PdfOcrMenuObserver> observer_;
  std::unique_ptr<MockRenderViewContextMenu> menu_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

PdfOcrMenuObserverTest::~PdfOcrMenuObserverTest() = default;

}  // namespace

// Tests that opening a context menu does not show the menu option if a
// screen reader is not enabled, regardless of the PDF OCR setting.
IN_PROC_BROWSER_TEST_F(PdfOcrMenuObserverTest,
                       PdfOcrItemNotShownWithoutScreenReader) {
  menu()->GetPrefs()->SetBoolean(prefs::kAccessibilityPdfOcrAlwaysActive,
                                 false);
  InitMenu();
  EXPECT_EQ(0u, menu()->GetMenuSize());

  menu()->GetPrefs()->SetBoolean(prefs::kAccessibilityPdfOcrAlwaysActive, true);
  InitMenu();
  EXPECT_EQ(0u, menu()->GetMenuSize());
}

IN_PROC_BROWSER_TEST_F(PdfOcrMenuObserverTest,
                       PdfOcrItemShownWithScreenReaderEnabled) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Enable Chromevox.
  ash::AccessibilityManager::Get()->EnableSpokenFeedback(true);
#else
  // Spoof a screen reader.
  content::ScopedAccessibilityModeOverride scoped_accessibility_mode(
      ui::AXMode::kScreenReader);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  menu()->GetPrefs()->SetBoolean(prefs::kAccessibilityPdfOcrAlwaysActive,
                                 false);
  InitMenu();

  // Shows but is not checked.
  ASSERT_EQ(3u, menu()->GetMenuSize());
  MockRenderViewContextMenu::MockMenuItem item;
  menu()->GetMenuItem(0, &item);
  EXPECT_EQ(IDC_CONTENT_CONTEXT_PDF_OCR, item.command_id);
  EXPECT_TRUE(item.enabled);
  EXPECT_FALSE(item.checked);
  EXPECT_FALSE(item.hidden);

  // The submenu items exist.
  menu()->GetMenuItem(1, &item);
  EXPECT_EQ(IDC_CONTENT_CONTEXT_PDF_OCR_ALWAYS, item.command_id);
  EXPECT_TRUE(item.enabled);
  EXPECT_FALSE(item.checked);
  EXPECT_FALSE(item.hidden);
  menu()->GetMenuItem(2, &item);
  EXPECT_EQ(IDC_CONTENT_CONTEXT_PDF_OCR_ONCE, item.command_id);
  EXPECT_TRUE(item.enabled);
  EXPECT_FALSE(item.checked);
  EXPECT_FALSE(item.hidden);

  Reset(false);
  // Shows and is checked when a screen reader and the setting are both on.
  menu()->GetPrefs()->SetBoolean(prefs::kAccessibilityPdfOcrAlwaysActive, true);
  InitMenu();

  ASSERT_EQ(1u, menu()->GetMenuSize());
  menu()->GetMenuItem(0, &item);
  EXPECT_EQ(IDC_CONTENT_CONTEXT_PDF_OCR, item.command_id);
  EXPECT_TRUE(item.enabled);
  EXPECT_TRUE(item.checked);
  EXPECT_FALSE(item.hidden);
}

IN_PROC_BROWSER_TEST_F(PdfOcrMenuObserverTest,
                       CheckUmaWhenTurnOnPdfOcrAlwaysFromContextMenu) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Enable Chromevox.
  ash::AccessibilityManager::Get()->EnableSpokenFeedback(true);
#else
  // Spoof a screen reader.
  content::ScopedAccessibilityModeOverride scoped_accessibility_mode(
      ui::AXMode::kScreenReader);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  menu()->GetPrefs()->SetBoolean(prefs::kAccessibilityPdfOcrAlwaysActive,
                                 false);
  InitMenu();
  ASSERT_EQ(menu()->GetMenuSize(), 3u);

  // Get the PDF OCR always menu item.
  MockRenderViewContextMenu::MockMenuItem item;
  menu()->GetMenuItem(1, &item);
  ASSERT_EQ(IDC_CONTENT_CONTEXT_PDF_OCR_ALWAYS, item.command_id);

  // Turn on PDF OCR always.
  base::HistogramTester histograms;
  menu()->ExecuteCommand(item.command_id, 0);

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectUniqueSample(
      "Accessibility.PdfOcr.UserSelection",
      PdfOcrUserSelection::kTurnOnAlwaysFromContextMenu,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(PdfOcrMenuObserverTest,
                       CheckUmaWhenTurnOffPdfOcrFromContextMenu) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Enable Chromevox.
  ash::AccessibilityManager::Get()->EnableSpokenFeedback(true);
#else
  // Spoof a screen reader.
  content::ScopedAccessibilityModeOverride scoped_accessibility_mode(
      ui::AXMode::kScreenReader);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  menu()->GetPrefs()->SetBoolean(prefs::kAccessibilityPdfOcrAlwaysActive, true);
  InitMenu();
  ASSERT_EQ(menu()->GetMenuSize(), 1u);

  // Get the PDF OCR checked menu item.
  MockRenderViewContextMenu::MockMenuItem item;
  menu()->GetMenuItem(0, &item);
  ASSERT_EQ(IDC_CONTENT_CONTEXT_PDF_OCR, item.command_id);
  ASSERT_TRUE(item.checked);

  // Turn off PDF OCR.
  base::HistogramTester histograms;
  menu()->ExecuteCommand(item.command_id, 0);

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectUniqueSample("Accessibility.PdfOcr.UserSelection",
                                PdfOcrUserSelection::kTurnOffFromContextMenu,
                                /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(PdfOcrMenuObserverTest,
                       CheckUmaWhenTurnOnPdfOcrOnceFromContextMenu) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Enable Chromevox.
  ash::AccessibilityManager::Get()->EnableSpokenFeedback(true);
#else
  // Spoof a screen reader.
  content::ScopedAccessibilityModeOverride scoped_accessibility_mode(
      ui::AXMode::kScreenReader);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  menu()->GetPrefs()->SetBoolean(prefs::kAccessibilityPdfOcrAlwaysActive,
                                 false);
  InitMenu();
  ASSERT_EQ(menu()->GetMenuSize(), 3u);

  // Get the PDF OCR once menu item.
  MockRenderViewContextMenu::MockMenuItem item;
  menu()->GetMenuItem(2, &item);
  ASSERT_EQ(IDC_CONTENT_CONTEXT_PDF_OCR_ONCE, item.command_id);

  // Run PDF OCR once.
  base::HistogramTester histograms;
  menu()->ExecuteCommand(item.command_id, 0);

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectUniqueSample("Accessibility.PdfOcr.UserSelection",
                                PdfOcrUserSelection::kTurnOnOnceFromContextMenu,
                                /*expected_bucket_count=*/1);
}
