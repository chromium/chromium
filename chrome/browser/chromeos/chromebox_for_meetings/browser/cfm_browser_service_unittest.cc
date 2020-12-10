// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/chromebox_for_meetings/browser/cfm_browser_service.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/chromebox_for_meetings/fake_cfm_hotline_client.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/fake_service_connection.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/fake_service_context.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/service_connection.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/cfm_browser.mojom.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/cfm_service_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {
namespace cfm {
namespace {

class CfmBrowserServiceTest : public testing::Test {
 public:
  CfmBrowserServiceTest() = default;
  CfmBrowserServiceTest(const CfmBrowserServiceTest&) = delete;
  CfmBrowserServiceTest& operator=(const CfmBrowserServiceTest&) = delete;

  void SetUp() override {
    CfmHotlineClient::InitializeFake();
    ServiceConnection::UseFakeServiceConnectionForTesting(
        &fake_service_connection_);
    CfmBrowserService::Initialize();
  }

  void TearDown() override {
    CfmBrowserService::Shutdown();
    CfmHotlineClient::Shutdown();
  }

  FakeCfmHotlineClient* GetClient() {
    return static_cast<FakeCfmHotlineClient*>(CfmHotlineClient::Get());
  }

  // Returns a mojo::Remote for the mojom::CfmBrowser by faking the way the cfm
  // mojom binder daemon would request it through chrome.
  mojo::Remote<mojom::CfmBrowser> GetBrowserRemote() {
    base::RunLoop run_loop;

    auto* interface_name = mojom::CfmBrowser::Name_;

    // Fake out CfmServiceContext
    FakeCfmServiceContext context;
    mojo::Receiver<mojom::CfmServiceContext> context_receiver(&context);
    fake_service_connection_.SetCallback(base::BindLambdaForTesting(
        [&](mojo::PendingReceiver<mojom::CfmServiceContext> pending_receiver,
            bool success) {
          ASSERT_TRUE(success);
          context_receiver.Bind(std::move(pending_receiver));
        }));

    mojo::Remote<mojom::CfmServiceAdaptor> adaptor_remote;
    context.SetFakeProvideAdaptorCallback(base::BindLambdaForTesting(
        [&](const std::string& service_id,
            mojo::PendingRemote<mojom::CfmServiceAdaptor>
                adaptor_pending_remote,
            mojom::CfmServiceContext::ProvideAdaptorCallback callback) {
          ASSERT_EQ(interface_name, service_id);
          adaptor_remote.Bind(std::move(adaptor_pending_remote));
          std::move(callback).Run(true);
        }));

    EXPECT_TRUE(GetClient()->FakeEmitSignal(interface_name));
    run_loop.RunUntilIdle();

    EXPECT_TRUE(context_receiver.is_bound());
    EXPECT_TRUE(adaptor_remote.is_connected());

    mojo::Remote<mojom::CfmBrowser> browser_remote;
    adaptor_remote->OnBindService(
        browser_remote.BindNewPipeAndPassReceiver().PassPipe());
    EXPECT_TRUE(browser_remote.is_connected());

    return browser_remote;
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  FakeServiceConnectionImpl fake_service_connection_;
};

// This test ensures that the CfmBrowserService is discoverable by its mojom
// name by sending a signal received by CfmHotlineClient.
TEST_F(CfmBrowserServiceTest, BrowserServiceAvailable) {
  ASSERT_TRUE(GetClient()->FakeEmitSignal(mojom::CfmBrowser::Name_));
}

// This test ensures that the CfmBrowserService correctly registers itself for
// discovery by the cfm mojom binder daemon and correctly returns a working
// mojom remote.
TEST_F(CfmBrowserServiceTest, GetBrowserRemote) {
  ASSERT_TRUE(GetBrowserRemote().is_connected());
}

}  // namespace
}  // namespace cfm
}  // namespace chromeos
