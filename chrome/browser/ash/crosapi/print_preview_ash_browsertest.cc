// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/printing/print_preview/print_preview_webcontents_adapter_ash.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/print_preview_cros.mojom.h"
#include "chromeos/printing/print_settings_test_util.h"
#include "components/printing/common/print.mojom.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "printing/mojom/print.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {
namespace {

class FakePrintPreviewBrowserMojoClient : public mojom::PrintPreviewCrosClient {
 public:
  FakePrintPreviewBrowserMojoClient() = default;
  FakePrintPreviewBrowserMojoClient(const FakePrintPreviewBrowserMojoClient&) =
      delete;
  FakePrintPreviewBrowserMojoClient& operator=(
      const FakePrintPreviewBrowserMojoClient&) = delete;
  ~FakePrintPreviewBrowserMojoClient() override = default;

  // crosapi::mojom::PrintPreviewCros overrides
  void GeneratePrintPreview(const base::UnguessableToken& token,
                            crosapi::mojom::PrintSettingsPtr settings,
                            GeneratePrintPreviewCallback callback) override {
    std::move(callback).Run(/*success=*/true);
  }

  void HandleDialogClosed(const base::UnguessableToken& token,
                          HandleDialogClosedCallback callback) override {
    std::move(callback).Run(/*success=*/true);
    ++handle_dialog_closed_count_;
  }

  int handle_dialog_closed_count() const { return handle_dialog_closed_count_; }

  mojo::Receiver<mojom::PrintPreviewCrosClient> receiver_{this};
  mojo::Remote<mojom::PrintPreviewCrosDelegate> remote_;

 private:
  int handle_dialog_closed_count_ = 0;
};

class FakePrintPreviewBrowserAshClient : public mojom::PrintPreviewCrosClient {
 public:
  FakePrintPreviewBrowserAshClient() = default;
  FakePrintPreviewBrowserAshClient(const FakePrintPreviewBrowserAshClient&) =
      delete;
  FakePrintPreviewBrowserAshClient& operator=(
      const FakePrintPreviewBrowserAshClient&) = delete;
  ~FakePrintPreviewBrowserAshClient() override = default;

  // crosapi::mojom::PrintPreviewCrosClient overrides
  void GeneratePrintPreview(const base::UnguessableToken& token,
                            crosapi::mojom::PrintSettingsPtr settings,
                            GeneratePrintPreviewCallback callback) override {
    std::move(callback).Run(/*success=*/true);
  }

  void HandleDialogClosed(const base::UnguessableToken& token,
                          HandleDialogClosedCallback callback) override {
    std::move(callback).Run(/*success=*/true);
    ++handle_dialog_closed_count_;
  }

  int handle_dialog_closed_count() const { return handle_dialog_closed_count_; }

 private:
  int handle_dialog_closed_count_ = 0;
};

// Calls all crosapi::mojom::PrintPreviewCrosDelegate methods over mojo.
// Intended for the mojo client impl.
void CallPrintPreviewBrowserDelegateMethods(
    FakePrintPreviewBrowserMojoClient& client) {
  const auto token = base::UnguessableToken::Create();
  printing::mojom::RequestPrintPreviewParamsPtr params_ptr =
      printing::mojom::RequestPrintPreviewParams::New();

  base::test::TestFuture<bool> future1;
  client.remote_->RequestPrintPreview(token, std::move(params_ptr),
                                      future1.GetCallback());
  EXPECT_TRUE(future1.Wait());

  base::test::TestFuture<bool> future2;
  client.remote_->PrintPreviewDone(token, future2.GetCallback());
  EXPECT_TRUE(future2.Wait());
}

// Calls all crosapi::mojom::PrintPreviewCrosDelegate methods directly.
void CallPrintPreviewBrowserDelegateMethods(
    ash::printing::PrintPreviewWebcontentsAdapterAsh* adapter) {
  const auto token = base::UnguessableToken::Create();
  printing::mojom::RequestPrintPreviewParamsPtr params_ptr =
      printing::mojom::RequestPrintPreviewParams::New();

  base::test::TestFuture<bool> future1;
  adapter->RequestPrintPreview(token, std::move(params_ptr),
                               future1.GetCallback());
  EXPECT_TRUE(future1.Wait());

  base::test::TestFuture<bool> future2;
  adapter->PrintPreviewDone(token, future2.GetCallback());
  EXPECT_TRUE(future2.Wait());
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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests `PrintPreviewCros` api calls don't crash. Tests calls over
// both mojo and ash clients.
IN_PROC_BROWSER_TEST_F(PrintPreviewAshBrowserTest, ApiCalls) {
  ASSERT_TRUE(CrosapiManager::IsInitialized());

  auto* print_preview_cros_adapter =
      CrosapiManager::Get()
          ->crosapi_ash()
          ->print_preview_webcontents_adapter_ash();
  {
    // Ash client.
    FakePrintPreviewBrowserAshClient ash_client;
    print_preview_cros_adapter->RegisterAshClient(&ash_client);

    // No crashes.
    CallPrintPreviewBrowserDelegateMethods(print_preview_cros_adapter);

    // Via ash client directly.
    base::test::TestFuture<bool> future;
    print_preview_cros_adapter->StartGetPreview(
        base::UnguessableToken::Create(),
        chromeos::CreatePrintSettings(/*preview_id=*/0), future.GetCallback());
    EXPECT_TRUE(future.Wait());
  }

  {
    // Mojo client.
    FakePrintPreviewBrowserMojoClient mojo_client;
    print_preview_cros_adapter->BindReceiver(
        mojo_client.remote_.BindNewPipeAndPassReceiver());

    base::test::TestFuture<bool> future2;
    mojo_client.remote_->RegisterMojoClient(
        mojo_client.receiver_.BindNewPipeAndPassRemote(),
        future2.GetCallback());
    EXPECT_TRUE(future2.Get());

    // No crashes.
    CallPrintPreviewBrowserDelegateMethods(mojo_client);

    base::test::TestFuture<bool> future3;
    print_preview_cros_adapter->StartGetPreview(
        base::UnguessableToken::Create(),
        chromeos::CreatePrintSettings(/*preview_id=*/1), future3.GetCallback());
    EXPECT_TRUE(future3.Wait());
  }
}

IN_PROC_BROWSER_TEST_F(PrintPreviewAshBrowserTest, HandleDialogClosed) {
  ASSERT_TRUE(CrosapiManager::IsInitialized());

  auto* print_preview_cros_adapter =
      CrosapiManager::Get()
          ->crosapi_ash()
          ->print_preview_webcontents_adapter_ash();
  {
    // Ash client.
    FakePrintPreviewBrowserAshClient ash_client;
    print_preview_cros_adapter->RegisterAshClient(&ash_client);

    EXPECT_EQ(0, ash_client.handle_dialog_closed_count());
    print_preview_cros_adapter->OnDialogClosed(
        base::UnguessableToken::Create());
    EXPECT_EQ(1, ash_client.handle_dialog_closed_count());
  }

  {
    // Mojo client.
    FakePrintPreviewBrowserMojoClient mojo_client;
    print_preview_cros_adapter->BindReceiver(
        mojo_client.remote_.BindNewPipeAndPassReceiver());

    base::RunLoop run_loop;
    mojo_client.remote_->RegisterMojoClient(
        mojo_client.receiver_.BindNewPipeAndPassRemote(),
        base::BindLambdaForTesting([&](bool success) {
          EXPECT_TRUE(success);
          run_loop.Quit();
        }));
    run_loop.Run();

    EXPECT_EQ(0, mojo_client.handle_dialog_closed_count());
    base::RunLoop run_loop2;
    print_preview_cros_adapter->OnDialogClosed(
        base::UnguessableToken::Create());
    run_loop2.RunUntilIdle();
    EXPECT_EQ(1, mojo_client.handle_dialog_closed_count());
  }
}

}  // namespace
}  // namespace crosapi
