// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "chrome/browser/accessibility/pdf_ocr_controller.h"
#include "chrome/browser/pdf/pdf_extension_test_base.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#else
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

class WebContentsLoadWaiter : public content::WebContentsObserver {
 public:
  // Observe `DidFinishLoad` for the specified |web_contents|.
  explicit WebContentsLoadWaiter(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override {
    run_loop_.Quit();
  }

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
};

class PrefChangeWaiter : public KeyedService {
 public:
  // Observe changes in prefs::kAccessibilityPdfOcrAlwaysActive.
  explicit PrefChangeWaiter(Profile* profile) {
    pref_change_registrar_.Init(profile->GetPrefs());
    pref_change_registrar_.Add(
        prefs::kAccessibilityPdfOcrAlwaysActive,
        base::BindLambdaForTesting([&]() { run_loop_.Quit(); }));
  }

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
  PrefChangeRegistrar pref_change_registrar_;
};

#if !BUILDFLAG(IS_CHROMEOS_ASH)
Profile* CreateProfile(const base::FilePath& basename) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath profile_path =
      profile_manager->user_data_dir().Append(basename);
  return &profiles::testing::CreateProfileSync(profile_manager, profile_path);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

class PdfOcrControllerBrowserTest : public PDFExtensionTestBase {
 public:
  PdfOcrControllerBrowserTest() = default;
  ~PdfOcrControllerBrowserTest() override = default;

  PdfOcrControllerBrowserTest(const PdfOcrControllerBrowserTest&) = delete;
  PdfOcrControllerBrowserTest& operator=(const PdfOcrControllerBrowserTest&) =
      delete;

  // PDFExtensionTestBase overrides:
  void SetUpOnMainThread() override {
    PDFExtensionTestBase::SetUpOnMainThread();
    EnableScreenReader(true);
  }

  // PDFExtensionTestBase overrides:
  void TearDownOnMainThread() override {
    PDFExtensionTestBase::TearDownOnMainThread();
    EnableScreenReader(false);
  }

  void EnableScreenReader(bool enabled) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Enable Chromevox.
    ash::AccessibilityManager::Get()->EnableSpokenFeedback(enabled);
#else
    // Spoof a screen reader.
    if (enabled) {
      content::BrowserAccessibilityState::GetInstance()
          ->AddAccessibilityModeFlags(ui::AXMode::kScreenReader);
    } else {
      content::BrowserAccessibilityState::GetInstance()
          ->RemoveAccessibilityModeFlags(ui::AXMode::kScreenReader);
    }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }
};

// TODO(crbug.com/1443345): Fix flakiness.
// Enabling PDF OCR should affect the accessibility mode of a new WebContents
// of PDF Viewer Mimehandler.
IN_PROC_BROWSER_TEST_F(PdfOcrControllerBrowserTest,
                       DISABLED_OpenPDFAfterTurningOnPdfOcr) {
  ui::AXMode ax_mode =
      content::BrowserAccessibilityState::GetInstance()->GetAccessibilityMode();
  EXPECT_FALSE(ax_mode.has_mode(ui::AXMode::kPDFOcr));

  PrefChangeWaiter pref_waiter(browser()->profile());
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kAccessibilityPdfOcrAlwaysActive, true);
  // Wait until the PDF OCR pref changes accordingly.
  pref_waiter.Wait();

  // Load test PDF.
  content::WebContents* active_web_contents = GetActiveWebContents();
  WebContentsLoadWaiter load_waiter(active_web_contents);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/pdf/test.pdf")));
  load_waiter.Wait();

  std::vector<content::WebContents*> html_web_contents_vector =
      screen_ai::PdfOcrController::GetAllPdfWebContentsesForTesting(
          browser()->profile());
  for (auto* web_contents : html_web_contents_vector) {
    ax_mode = web_contents->GetAccessibilityMode();
    EXPECT_TRUE(ax_mode.has_mode(ui::AXMode::kPDFOcr));
  }
}

// TODO(crbug.com/1443345): Fix flakiness.
// Enabling PDF OCR should affect the accessibility mode of an exiting
// WebContents of PDF Viewer Mimehandler.
IN_PROC_BROWSER_TEST_F(PdfOcrControllerBrowserTest,
                       DISABLED_OpenPDFBeforeTurningOnPdfOcr) {
  ui::AXMode ax_mode =
      content::BrowserAccessibilityState::GetInstance()->GetAccessibilityMode();
  EXPECT_FALSE(ax_mode.has_mode(ui::AXMode::kPDFOcr));

  // Load test PDF.
  content::WebContents* active_web_contents = GetActiveWebContents();
  WebContentsLoadWaiter load_waiter(active_web_contents);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/pdf/test.pdf")));
  load_waiter.Wait();

  std::vector<content::WebContents*> html_web_contents_vector =
      screen_ai::PdfOcrController::GetAllPdfWebContentsesForTesting(
          browser()->profile());
  for (auto* web_contents : html_web_contents_vector) {
    ax_mode = web_contents->GetAccessibilityMode();
    EXPECT_FALSE(ax_mode.has_mode(ui::AXMode::kPDFOcr));
  }

  PrefChangeWaiter pref_waiter(browser()->profile());
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kAccessibilityPdfOcrAlwaysActive, true);
  // Wait until the PDF OCR pref changes accordingly.
  pref_waiter.Wait();

  html_web_contents_vector =
      screen_ai::PdfOcrController::GetAllPdfWebContentsesForTesting(
          browser()->profile());
  for (auto* web_contents : html_web_contents_vector) {
    ax_mode = web_contents->GetAccessibilityMode();
    EXPECT_TRUE(ax_mode.has_mode(ui::AXMode::kPDFOcr));
  }
}

// Enabling PDF OCR should not affect the accessibility mode of WebContents if
// it's not related to PDF.
IN_PROC_BROWSER_TEST_F(PdfOcrControllerBrowserTest,
                       PdfOcrNotAffectingNonPdfTab) {
  ui::AXMode ax_mode =
      content::BrowserAccessibilityState::GetInstance()->GetAccessibilityMode();
  EXPECT_FALSE(ax_mode.has_mode(ui::AXMode::kPDFOcr));

  // Open a new tab not associated with PDF.
  chrome::NewTab(browser());
  content::WebContents* web_contents = GetActiveWebContents();
  ax_mode = web_contents->GetAccessibilityMode();
  EXPECT_FALSE(ax_mode.has_mode(ui::AXMode::kPDFOcr));

  PrefChangeWaiter pref_waiter(browser()->profile());
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kAccessibilityPdfOcrAlwaysActive, true);
  // Wait until the PDF OCR pref changes accordingly.
  pref_waiter.Wait();
  // The existing WebContents should not be affected by this pref change.
  ax_mode = web_contents->GetAccessibilityMode();
  EXPECT_FALSE(ax_mode.has_mode(ui::AXMode::kPDFOcr));

  // Open a new tab not associated with PDF.
  chrome::NewTab(browser());
  web_contents = GetActiveWebContents();
  // This new WebContents should not be affected by the pref change.
  ax_mode = web_contents->GetAccessibilityMode();
  EXPECT_FALSE(ax_mode.has_mode(ui::AXMode::kPDFOcr));
}

// Multi-profile is not supported on Ash.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Enabling PDF OCR in one profile should not affect the accessibility mode of
// WebContents in another profile.
IN_PROC_BROWSER_TEST_F(PdfOcrControllerBrowserTest,
                       TurningOnPdfOcrInOneProfileNotAffectingAnotherProfile) {
  ui::AXMode ax_mode =
      content::BrowserAccessibilityState::GetInstance()->GetAccessibilityMode();
  EXPECT_FALSE(ax_mode.has_mode(ui::AXMode::kPDFOcr));

  Profile* profile1 = browser()->profile();
  // Load test PDF on profile1.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/pdf/test.pdf")));

  std::vector<content::WebContents*> html_web_contents_vector1 =
      screen_ai::PdfOcrController::GetAllPdfWebContentsesForTesting(profile1);
  for (auto* web_contents : html_web_contents_vector1) {
    ax_mode = web_contents->GetAccessibilityMode();
    EXPECT_FALSE(ax_mode.has_mode(ui::AXMode::kPDFOcr));
  }

  const base::FilePath kProfileDir2(FILE_PATH_LITERAL("Other"));
  Profile* profile2 = CreateProfile(kProfileDir2);

  // Set the PDF OCR pref for the profile2.
  PrefChangeWaiter pref_waiter(profile2);
  profile2->GetPrefs()->SetBoolean(prefs::kAccessibilityPdfOcrAlwaysActive,
                                   true);
  // Wait until the PDF OCR pref changes accordingly.
  pref_waiter.Wait();

  // Setting the PDF OCR pref for the profile2 should not affect a WebContents
  // of PDF Viewer Mimehandler for the profile1.
  html_web_contents_vector1 =
      screen_ai::PdfOcrController::GetAllPdfWebContentsesForTesting(profile1);
  for (auto* web_contents : html_web_contents_vector1) {
    ax_mode = web_contents->GetAccessibilityMode();
    EXPECT_FALSE(ax_mode.has_mode(ui::AXMode::kPDFOcr));
  }
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
