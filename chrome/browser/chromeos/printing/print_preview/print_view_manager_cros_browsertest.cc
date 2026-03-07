// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/print_preview/print_view_manager_cros.h"

#include <vector>

#include "ash/webui/print_preview_cros/url_constants.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/browser_delegate/browser_controller.h"
#include "chrome/browser/ash/browser_delegate/browser_delegate.h"
#include "chrome/browser/ash/printing/print_preview/print_preview_dialog_controller_cros.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/printing/common/print.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

constexpr char kTestUrl[] = "data:text/html,<p>print-preview-test</p>";

std::vector<ash::SystemWebDialogDelegate*> GetPrintPreviewDialogs() {
  std::vector<ash::SystemWebDialogDelegate*> dialogs;
  for (auto* dialog : ash::SystemWebDialogDelegate::GetAllInstances()) {
    if (dialog->GetDialogContentURL() ==
        GURL(ash::kChromeUIPrintPreviewCrosURL)) {
      dialogs.push_back(dialog);
    }
  }
  return dialogs;
}

void CloseAllPrintPreviewDialogs() {
  std::vector<ash::SystemWebDialogDelegate*> dialogs = GetPrintPreviewDialogs();
  for (auto* dialog : dialogs) {
    dialog->Close();
  }

  ASSERT_TRUE(
      base::test::RunUntil([]() { return GetPrintPreviewDialogs().empty(); }));
}

class DialogCloseObserver
    : public ash::PrintPreviewDialogControllerCros::DialogControllerObserver {
 public:
  DialogCloseObserver() = default;
  DialogCloseObserver(const DialogCloseObserver&) = delete;
  DialogCloseObserver& operator=(const DialogCloseObserver&) = delete;
  ~DialogCloseObserver() override = default;

  void OnDialogClosed(const base::UnguessableToken& /*token*/) override {
    ++dialog_closed_count_;
  }

  int dialog_closed_count() const { return dialog_closed_count_; }

 private:
  int dialog_closed_count_ = 0;
};

}  // namespace

class PrintViewManagerCrosBrowserTest : public InProcessBrowserTest {
 public:
  PrintViewManagerCrosBrowserTest() = default;
  PrintViewManagerCrosBrowserTest(const PrintViewManagerCrosBrowserTest&) =
      delete;
  PrintViewManagerCrosBrowserTest& operator=(
      const PrintViewManagerCrosBrowserTest&) = delete;
  ~PrintViewManagerCrosBrowserTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kPrintPreviewCrosPrimary);
    InProcessBrowserTest::SetUp();
  }

  void TearDownOnMainThread() override {
    CloseAllPrintPreviewDialogs();
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  ash::BrowserDelegate* GetLastUsedBrowserDelegate() {
    ash::BrowserController* browser_controller =
        ash::BrowserController::GetInstance();
    EXPECT_TRUE(browser_controller);
    if (!browser_controller) {
      return nullptr;
    }

    ash::BrowserDelegate* browser_delegate =
        browser_controller->GetLastUsedBrowser();
    EXPECT_TRUE(browser_delegate);
    return browser_delegate;
  }

  content::WebContents* GetActiveWebContents() {
    ash::BrowserDelegate* browser_delegate = GetLastUsedBrowserDelegate();
    return browser_delegate ? browser_delegate->GetActiveWebContents()
                            : nullptr;
  }

  PrintViewManagerCros* CreatePrintViewManagerForActiveTab() {
    ash::BrowserDelegate* browser_delegate = GetLastUsedBrowserDelegate();
    if (!browser_delegate) {
      return nullptr;
    }

    browser_delegate->AddTab(GURL(kTestUrl), /*index=*/0u,
                             ash::BrowserDelegate::TabDisposition::kForeground);

    content::WebContents* web_contents =
        browser_delegate->GetActiveWebContents();
    EXPECT_TRUE(web_contents);
    if (!web_contents) {
      return nullptr;
    }

    return PrintViewManagerCros::FromWebContents(web_contents);
  }

  void RequestPrintPreview(PrintViewManagerCros* view_manager) {
    auto params = ::printing::mojom::RequestPrintPreviewParams::New();
    params->is_modifiable = true;
    params->has_selection = false;
    view_manager->RequestPrintPreview(std::move(params));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrintViewManagerCrosBrowserTest,
                       PrintPreviewDoneRoutesThroughDialogController) {
  PrintViewManagerCros* view_manager = CreatePrintViewManagerForActiveTab();
  ASSERT_TRUE(view_manager);

  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  content::RenderFrameHost* rfh = web_contents->GetPrimaryMainFrame();
  ASSERT_TRUE(rfh);
  ASSERT_TRUE(view_manager->PrintPreviewNow(rfh, /*has_selection=*/false));

  auto* dialog_controller =
      ash::PrintPreviewDialogControllerCros::GetInstance();
  DialogCloseObserver observer;
  dialog_controller->AddObserver(&observer);

  RequestPrintPreview(view_manager);
  ASSERT_TRUE(base::test::RunUntil(
      []() { return GetPrintPreviewDialogs().size() == 1; }));

  view_manager->PrintPreviewDone();

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return observer.dialog_closed_count() == 1 &&
           GetPrintPreviewDialogs().empty();
  }));
  EXPECT_EQ(1, observer.dialog_closed_count());
  EXPECT_FALSE(view_manager->render_frame_host_for_testing());
  EXPECT_TRUE(GetPrintPreviewDialogs().empty());

  dialog_controller->RemoveObserver(&observer);
}

IN_PROC_BROWSER_TEST_F(PrintViewManagerCrosBrowserTest,
                       UiDialogCloseCleansUpRenderFrameState) {
  PrintViewManagerCros* view_manager = CreatePrintViewManagerForActiveTab();
  ASSERT_TRUE(view_manager);

  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  content::RenderFrameHost* rfh = web_contents->GetPrimaryMainFrame();
  ASSERT_TRUE(rfh);
  ASSERT_TRUE(view_manager->PrintPreviewNow(rfh, /*has_selection=*/false));

  RequestPrintPreview(view_manager);
  ASSERT_TRUE(base::test::RunUntil(
      []() { return GetPrintPreviewDialogs().size() == 1; }));

  std::vector<ash::SystemWebDialogDelegate*> dialogs = GetPrintPreviewDialogs();
  ASSERT_EQ(1u, dialogs.size());
  dialogs.front()->Close();

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return view_manager->render_frame_host_for_testing() == nullptr;
  }));
  EXPECT_TRUE(GetPrintPreviewDialogs().empty());
}

}  // namespace chromeos
