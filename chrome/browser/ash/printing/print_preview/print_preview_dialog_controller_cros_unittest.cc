// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_PRINT_PREVIEW_PRINT_PREVIEW_DIALOG_CONTROLLER_CROS_UNITTEST_H_
#define CHROME_BROWSER_ASH_PRINTING_PRINT_PREVIEW_PRINT_PREVIEW_DIALOG_CONTROLLER_CROS_UNITTEST_H_

#include "chrome/browser/ash/printing/print_preview/print_preview_dialog_controller_cros.h"

#include <memory>
#include <utility>

#include "base/unguessable_token.h"
#include "chrome/browser/ui/webui/ash/print_preview_cros/print_preview_cros_dialog.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/dialog_test_browser_window.h"
#include "components/printing/common/print.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/widget_test.h"

using ash::printing::print_preview::PrintPreviewCrosDialog;
using ::printing::mojom::RequestPrintPreviewParams;

namespace ash {

class PrintPreviewDialogControllerCrosTest : public BrowserWithTestWindowTest {
 public:
  class TestObserver
      : public PrintPreviewCrosDialog::PrintPreviewCrosDialogObserver {
   public:
    TestObserver() = default;

    TestObserver(const TestObserver&) = delete;
    TestObserver& operator=(const TestObserver&) = delete;

    ~TestObserver() override = default;

    // PrintPreviewDialogCros::PrintPreviewCrosDialogObserver
    void OnDialogClosed(base::UnguessableToken token) override {
      ++on_dialog_closed_;
    }

    int on_dialog_closed_count() const { return on_dialog_closed_; }

   private:
    int on_dialog_closed_ = 0;
  };

  class TestDialogControllerObserver
      : public PrintPreviewDialogControllerCros::DialogControllerObserver {
   public:
    TestDialogControllerObserver() = default;

    TestDialogControllerObserver(const TestDialogControllerObserver&) = delete;
    TestDialogControllerObserver& operator=(
        const TestDialogControllerObserver&) = delete;

    ~TestDialogControllerObserver() override = default;

    // TestDialogControllerObserver::DialogControllerObserver
    void OnDialogClosed(const base::UnguessableToken& token) override {
      ++on_dialog_closed_;
    }

    int on_dialog_closed_count() const { return on_dialog_closed_; }

   private:
    int on_dialog_closed_ = 0;
  };

  PrintPreviewDialogControllerCrosTest() = default;

  PrintPreviewDialogControllerCrosTest(
      const PrintPreviewDialogControllerCrosTest&) = delete;
  PrintPreviewDialogControllerCrosTest& operator=(
      const PrintPreviewDialogControllerCrosTest&) = delete;

  ~PrintPreviewDialogControllerCrosTest() override = default;

  void SetUp() override {
    dialog_controller_ = std::make_unique<PrintPreviewDialogControllerCros>();
    BrowserWithTestWindowTest::SetUp();
  }

  void TearDown() override {
    dialog_controller_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  // Create a browser window to provide parenting for web contents modal dialog.
  std::unique_ptr<BrowserWindow> CreateBrowserWindow() override {
    return std::make_unique<DialogTestBrowserWindow>();
  }

  std::unique_ptr<PrintPreviewDialogControllerCros> dialog_controller_;
};

TEST_F(PrintPreviewDialogControllerCrosTest, TestObserver) {
  // Generate an Unguessable token to simulate a new webcontent.
  const base::UnguessableToken token(base::UnguessableToken::Create());
  PrintPreviewCrosDialog* original_dialog =
      dialog_controller_->GetOrCreatePrintPreviewDialog(
          token, RequestPrintPreviewParams());
  ASSERT_TRUE(original_dialog);
  EXPECT_TRUE(dialog_controller_->HasDialogForToken(token));

  // Create and add fake test observer.
  auto test_observer = std::make_unique<TestObserver>();
  auto test_dialog_controller_observer =
      std::make_unique<TestDialogControllerObserver>();
  original_dialog->AddObserver(test_observer.get());
  dialog_controller_->AddObserver(test_dialog_controller_observer.get());
  EXPECT_EQ(0, test_observer->on_dialog_closed_count());
  EXPECT_EQ(0, test_dialog_controller_observer->on_dialog_closed_count());

  // Close the dialog.
  views::Widget* parent_widget = views::Widget::GetWidgetForNativeWindow(
      original_dialog->GetDialogWindowForTesting());
  views::test::WidgetDestroyedWaiter waiter(parent_widget);
  original_dialog->Close();
  waiter.Wait();

  EXPECT_FALSE(dialog_controller_->HasDialogForToken(token));
  EXPECT_EQ(1, test_observer->on_dialog_closed_count());
  EXPECT_EQ(1, test_dialog_controller_observer->on_dialog_closed_count());
}

TEST_F(PrintPreviewDialogControllerCrosTest, OpenPrintPreview) {
  // Generate an Unguessable token to simulate a new webcontent.
  const base::UnguessableToken token(base::UnguessableToken::Create());
  PrintPreviewCrosDialog* original_dialog =
      dialog_controller_->GetOrCreatePrintPreviewDialog(
          token, RequestPrintPreviewParams());
  ASSERT_TRUE(original_dialog);
  EXPECT_TRUE(dialog_controller_->HasDialogForToken(token));

  // Attempt to create a new dialog with the same token.
  PrintPreviewCrosDialog* new_dialog =
      dialog_controller_->GetOrCreatePrintPreviewDialog(
          token, RequestPrintPreviewParams());
  ASSERT_TRUE(new_dialog);
  EXPECT_EQ(original_dialog, new_dialog);

  // Close the dialog.
  views::Widget* parent_widget = views::Widget::GetWidgetForNativeWindow(
      original_dialog->GetDialogWindowForTesting());
  views::test::WidgetDestroyedWaiter waiter(parent_widget);
  original_dialog->Close();
  waiter.Wait();

  EXPECT_FALSE(dialog_controller_->HasDialogForToken(token));
}

}  //  namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_PRINT_PREVIEW_PRINT_PREVIEW_DIALOG_CONTROLLER_CROS_UNITTEST_H_
