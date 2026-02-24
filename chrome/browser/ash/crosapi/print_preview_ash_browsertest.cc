// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/printing/print_preview/print_preview_webcontents_adapter_ash.h"
#include "chrome/browser/chromeos/printing/print_preview/print_preview_cros_client.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/printing/common/print.mojom.h"
#include "content/public/test/browser_test.h"
#include "printing/mojom/print.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {
namespace {

class FakePrintPreviewBrowserAshClient
    : public chromeos::PrintPreviewCrosClient {
 public:
  FakePrintPreviewBrowserAshClient() = default;
  FakePrintPreviewBrowserAshClient(const FakePrintPreviewBrowserAshClient&) =
      delete;
  FakePrintPreviewBrowserAshClient& operator=(
      const FakePrintPreviewBrowserAshClient&) = delete;
  ~FakePrintPreviewBrowserAshClient() override = default;

  // chromeos::PrintPreviewCrosClient overrides:
  void HandleDialogClosed(const base::UnguessableToken& /*token*/,
                          HandleDialogClosedCallback callback) override {
    std::move(callback).Run(/*success=*/true);
    ++handle_dialog_closed_count_;
  }

  int handle_dialog_closed_count() const { return handle_dialog_closed_count_; }

 private:
  int handle_dialog_closed_count_ = 0;
};

// Calls all chromeos::PrintPreviewCrosDelegate methods directly.
void CallPrintPreviewBrowserDelegateMethods(
    ash::printing::PrintPreviewWebcontentsAdapterAsh* adapter) {
  const auto token = base::UnguessableToken::Create();
  printing::mojom::RequestPrintPreviewParamsPtr params_ptr =
      printing::mojom::RequestPrintPreviewParams::New();

  base::test::TestFuture<bool> future1;
  adapter->RequestPrintPreview(token, std::move(params_ptr),
                               future1.GetCallback());
  EXPECT_TRUE(future1.Get());

  base::test::TestFuture<bool> future2;
  adapter->PrintPreviewDone(token, future2.GetCallback());
  EXPECT_TRUE(future2.Get());
}

class PrintPreviewAshBrowserTest : public InProcessBrowserTest {
 public:
  PrintPreviewAshBrowserTest() = default;

  PrintPreviewAshBrowserTest(const PrintPreviewAshBrowserTest&) = delete;
  PrintPreviewAshBrowserTest& operator=(const PrintPreviewAshBrowserTest&) =
      delete;

  ~PrintPreviewAshBrowserTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kPrintPreviewCrosPrimary);
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(CrosapiManager::IsInitialized());

    print_preview_cros_adapter_ = CrosapiManager::Get()
                                      ->crosapi_ash()
                                      ->print_preview_webcontents_adapter_ash();

    print_preview_cros_adapter_->RegisterAshClient(&ash_client_);
  }

  void TearDownOnMainThread() override {
    if (print_preview_cros_adapter_) {
      print_preview_cros_adapter_->RegisterAshClient(nullptr);
      print_preview_cros_adapter_ = nullptr;
    }
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  raw_ptr<ash::printing::PrintPreviewWebcontentsAdapterAsh>
      print_preview_cros_adapter_ = nullptr;
  FakePrintPreviewBrowserAshClient ash_client_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests `PrintPreviewCros` api calls don't crash.
IN_PROC_BROWSER_TEST_F(PrintPreviewAshBrowserTest, ApiCalls) {
  // No crashes.
  CallPrintPreviewBrowserDelegateMethods(print_preview_cros_adapter_);
}

IN_PROC_BROWSER_TEST_F(PrintPreviewAshBrowserTest, HandleDialogClosed) {
  EXPECT_EQ(0, ash_client_.handle_dialog_closed_count());
  print_preview_cros_adapter_->OnDialogClosed(base::UnguessableToken::Create());
  EXPECT_EQ(1, ash_client_.handle_dialog_closed_count());
}

}  // namespace
}  // namespace crosapi
