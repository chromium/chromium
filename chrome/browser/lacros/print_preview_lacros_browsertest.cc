// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/unguessable_token.h"
#include "chrome/browser/chromeos/printing/print_preview/print_preview_webcontents_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/print_preview_cros.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/printing/print_settings_test_util.h"
#include "components/printing/common/print.mojom.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
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
  }

  mojo::Receiver<mojom::PrintPreviewCrosClient> receiver_{this};
  mojo::Remote<mojom::PrintPreviewCrosDelegate> remote_;
};

// Calls all crosapi::mojom::PrintPreviewCrosDelegate methods over mojo.
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

class PrintPreviewLacrosBrowserTest : public InProcessBrowserTest {
 public:
  PrintPreviewLacrosBrowserTest() = default;

  PrintPreviewLacrosBrowserTest(const PrintPreviewLacrosBrowserTest&) = delete;
  PrintPreviewLacrosBrowserTest& operator=(
      const PrintPreviewLacrosBrowserTest&) = delete;

  ~PrintPreviewLacrosBrowserTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kPrintPreviewCrosPrimary);
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests `PrintPreviewCros` api calls don't crash.
IN_PROC_BROWSER_TEST_F(PrintPreviewLacrosBrowserTest, ApiCalls) {
  auto* lacros_service = chromeos::LacrosService::Get();
  ASSERT_TRUE(lacros_service);
  ASSERT_TRUE(
      lacros_service->IsRegistered<crosapi::mojom::PrintPreviewCrosDelegate>());

  if (!lacros_service
           ->IsAvailable<crosapi::mojom::PrintPreviewCrosDelegate>()) {
    GTEST_SKIP();
  }

  // Reset remote to allow binding for tests.
  chromeos::PrintPreviewWebcontentsManager::Get()->ResetRemoteForTesting();

  FakePrintPreviewBrowserMojoClient mojo_client;
  lacros_service->BindPrintPreviewCrosDelegate(
      mojo_client.remote_.BindNewPipeAndPassReceiver());

  base::test::TestFuture<bool> future;
  mojo_client.remote_->RegisterMojoClient(
      mojo_client.receiver_.BindNewPipeAndPassRemote(), future.GetCallback());
  EXPECT_TRUE(future.Get());

  // No crashes.
  CallPrintPreviewBrowserDelegateMethods(mojo_client);

  base::UnguessableToken token = base::UnguessableToken::Create();
  base::test::TestFuture<bool> future2;
  mojo_client.GeneratePrintPreview(
      token, chromeos::CreatePrintSettings(/*preview_id=*/0),
      future2.GetCallback());
  EXPECT_TRUE(future2.Wait());

  base::test::TestFuture<bool> future3;
  mojo_client.HandleDialogClosed(token, future3.GetCallback());
  EXPECT_TRUE(future3.Wait());
}

}  // namespace
}  // namespace crosapi
