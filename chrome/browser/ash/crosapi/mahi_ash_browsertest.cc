// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/mahi/mahi_browser_delegate_ash.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/mahi.mojom.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

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
  base::UnguessableToken id_{base::UnguessableToken::Create()};
};

class FakeMahiBrowserCppClient : public mojom::MahiBrowserClient {
 public:
  FakeMahiBrowserCppClient() = default;
  FakeMahiBrowserCppClient(const FakeMahiBrowserCppClient&) = delete;
  FakeMahiBrowserCppClient& operator=(const FakeMahiBrowserCppClient&) = delete;
  ~FakeMahiBrowserCppClient() override = default;

  // crosapi::mojom::MahiBrowserClient overrides
  void GetContent(const base::UnguessableToken& id,
                  GetContentCallback callback) override {
    std::move(callback).Run(crosapi::mojom::MahiPageContent::New());
  }
};

// Calls all crosapi::mojom::Mahi methods over mojo.
void CallMahiBrowserDelegateMethods(FakeMahiBrowserMojoClient& client) {
  base::test::TestFuture<bool> future1;
  crosapi::mojom::MahiPageInfoPtr page_info =
      crosapi::mojom::MahiPageInfo::New();
  page_info->client_id = base::UnguessableToken::Create();
  page_info->page_id = base::UnguessableToken::Create();
  page_info->url = GURL();
  page_info->title = u"";

  client.remote_->OnFocusedPageChanged(std::move(page_info),
                                       future1.GetCallback());
  EXPECT_TRUE(future1.Take());

  base::test::TestFuture<bool> future2;
  crosapi::mojom::MahiContextMenuRequestPtr request =
      crosapi::mojom::MahiContextMenuRequest::New();
  client.remote_->OnContextMenuClicked(std::move(request),
                                       future2.GetCallback());
  EXPECT_TRUE(future2.Take());
}

// Calls all crosapi::mojom::Mahi methods directly.
void CallMahiBrowserDelegateMethods(
    FakeMahiBrowserCppClient& client,
    ash::MahiBrowserDelegateAsh* mahi_browser_delegate) {
  base::test::TestFuture<bool> future1;
  auto page_info = crosapi::mojom::MahiPageInfo::New();
  page_info->client_id = base::UnguessableToken::Create();
  page_info->page_id = base::UnguessableToken::Create();
  page_info->url = GURL();
  page_info->title = u"";

  mahi_browser_delegate->OnFocusedPageChanged(std::move(page_info),
                                              future1.GetCallback());
  EXPECT_TRUE(future1.Take());

  base::test::TestFuture<bool> future2;
  crosapi::mojom::MahiContextMenuRequestPtr request =
      crosapi::mojom::MahiContextMenuRequest::New();
  mahi_browser_delegate->OnContextMenuClicked(std::move(request),
                                              future2.GetCallback());
  EXPECT_TRUE(future2.Take());
}

class MahiAshBrowserTest : public InProcessBrowserTest {
 public:
  MahiAshBrowserTest() = default;

  MahiAshBrowserTest(const MahiAshBrowserTest&) = delete;
  MahiAshBrowserTest& operator=(const MahiAshBrowserTest&) = delete;

  ~MahiAshBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests `MahiBrowserDelegate` api calls don't crash. Tests calls over
// both mojo and cpp clients.
IN_PROC_BROWSER_TEST_F(MahiAshBrowserTest, Basics) {
  ASSERT_TRUE(CrosapiManager::IsInitialized());

  auto* mahi_browser_delegate =
      CrosapiManager::Get()->crosapi_ash()->mahi_browser_delegate_ash();
  {
    FakeMahiBrowserMojoClient mojo_client1;
    mahi_browser_delegate->BindReceiver(
        mojo_client1.remote_.BindNewPipeAndPassReceiver());

    base::test::TestFuture<bool> future1;
    mojo_client1.remote_->RegisterMojoClient(
        mojo_client1.receiver_.BindNewPipeAndPassRemote(), mojo_client1.id_,
        future1.GetCallback());
    EXPECT_TRUE(future1.Take());

    FakeMahiBrowserCppClient cpp_client1;
    mahi_browser_delegate->RegisterCppClient(&cpp_client1, mojo_client1.id_);

    CallMahiBrowserDelegateMethods(mojo_client1);
    CallMahiBrowserDelegateMethods(cpp_client1, mahi_browser_delegate);
  }

  // Disconnect old clients and try again to ensure manager's API doesn't crash
  // after any client disconnects.
  FakeMahiBrowserMojoClient mojo_client2;
  mahi_browser_delegate->BindReceiver(
      mojo_client2.remote_.BindNewPipeAndPassReceiver());

  base::test::TestFuture<bool> future2;
  mojo_client2.remote_->RegisterMojoClient(
      mojo_client2.receiver_.BindNewPipeAndPassRemote(), mojo_client2.id_,
      future2.GetCallback());
  EXPECT_TRUE(future2.Take());

  FakeMahiBrowserCppClient cpp_client2;
  mahi_browser_delegate->RegisterCppClient(&cpp_client2, mojo_client2.id_);

  CallMahiBrowserDelegateMethods(mojo_client2);
  CallMahiBrowserDelegateMethods(cpp_client2, mahi_browser_delegate);
}

}  // namespace
}  // namespace crosapi
