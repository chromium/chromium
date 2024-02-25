// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/mahi.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {
namespace {

class FakeMahiBrowserMojoClient : public mojom::MahiBrowserClient {
 public:
  FakeMahiBrowserMojoClient() = default;
  FakeMahiBrowserMojoClient(const FakeMahiBrowserMojoClient&) = delete;
  FakeMahiBrowserMojoClient& operator=(const FakeMahiBrowserMojoClient&) =
      delete;
  ~FakeMahiBrowserMojoClient() override = default;

  // crosapi::mojom::MahiBrowserClient overrides
  void GetContent(const base::UnguessableToken& id,
                  GetContentCallback callback) override {
    std::move(callback).Run(crosapi::mojom::MahiPageContent::New());
  }

  mojo::Receiver<mojom::MahiBrowserClient> receiver_{this};
  mojo::Remote<mojom::MahiBrowserDelegate> remote_;
};

// Calls all crosapi::mojom::Mahi methods over mojo.
void CallMahiBrowserDelegateMethods(FakeMahiBrowserMojoClient& client) {
  base::RunLoop run_loop1;
  crosapi::mojom::MahiPageInfoPtr page_info =
      crosapi::mojom::MahiPageInfo::New();
  page_info->client_id = base::UnguessableToken::Create();
  page_info->page_id = base::UnguessableToken::Create();
  page_info->url = GURL();
  page_info->title = u"";

  client.remote_->OnFocusedPageChanged(
      std::move(page_info), base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        run_loop1.Quit();
      }));
  run_loop1.Run();

  base::RunLoop run_loop2;
  crosapi::mojom::MahiContextMenuRequestPtr request =
      crosapi::mojom::MahiContextMenuRequest::New();
  client.remote_->OnContextMenuClicked(
      std::move(request), base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        run_loop2.Quit();
      }));
  run_loop2.Run();
}

class MahiLacrosBrowserTest : public InProcessBrowserTest {
 public:
  MahiLacrosBrowserTest() = default;

  MahiLacrosBrowserTest(const MahiLacrosBrowserTest&) = delete;
  MahiLacrosBrowserTest& operator=(const MahiLacrosBrowserTest&) = delete;

  ~MahiLacrosBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests `MahiBrowserDelegate` api calls over mojo don't crash.
IN_PROC_BROWSER_TEST_F(MahiLacrosBrowserTest, Basics) {
  auto* lacros_service = chromeos::LacrosService::Get();
  ASSERT_TRUE(lacros_service);
  ASSERT_TRUE(
      lacros_service->IsRegistered<crosapi::mojom::MahiBrowserDelegate>());

  if (!lacros_service->IsAvailable<crosapi::mojom::MahiBrowserDelegate>()) {
    GTEST_SKIP();
  }

  FakeMahiBrowserMojoClient mojo_client1;
  lacros_service->BindMahiBrowserDelegate(
      mojo_client1.remote_.BindNewPipeAndPassReceiver());

  {
    FakeMahiBrowserMojoClient mojo_client2;
    lacros_service->BindMahiBrowserDelegate(
        mojo_client2.remote_.BindNewPipeAndPassReceiver());

    // Calls and verifies that `MahiBrowserDelegate` methods don't
    // crash.
    CallMahiBrowserDelegateMethods(mojo_client1);
    CallMahiBrowserDelegateMethods(mojo_client2);
  }

  // Calls and verifies that `MahiBrowserDelegate` methods don't crash
  // after a client has disconnected.
  FakeMahiBrowserMojoClient mojo_client3;
  lacros_service->BindMahiBrowserDelegate(
      mojo_client3.remote_.BindNewPipeAndPassReceiver());
  CallMahiBrowserDelegateMethods(mojo_client3);
}

}  // namespace
}  // namespace crosapi
