// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/meet_browser/meet_browser_service.h"

#include <asm-generic/errno.h>
#include <linux/uvcvideo.h>
#include <linux/videodev2.h>

#include <cstdint>
#include <optional>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/chromebox_for_meetings/fake_cfm_hotline_client.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/fake_service_connection.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/fake_service_context.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/service_connection.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/meet_browser.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash::cfm {

class CfmMeetBrowserServiceTest : public testing::Test {
 public:
  CfmMeetBrowserServiceTest() = default;
  CfmMeetBrowserServiceTest(const CfmMeetBrowserServiceTest&) = delete;
  CfmMeetBrowserServiceTest& operator=(const CfmMeetBrowserServiceTest&) =
      delete;

  void SetUp() override {
    CfmHotlineClient::InitializeFake();
    chromeos::cfm::ServiceConnection::UseFakeServiceConnectionForTesting(
        &fake_service_connection_);
    MeetBrowserService::Initialize();
  }

  void TearDown() override {
    MeetBrowserService::Shutdown();
    CfmHotlineClient::Shutdown();
  }

  FakeCfmHotlineClient* GetClient() {
    return static_cast<FakeCfmHotlineClient*>(CfmHotlineClient::Get());
  }

  // Returns a mojo::Remote for the mojom::MeetBrowser by faking the
  // way the cfm mojom binder daemon would request it through chrome.
  const mojo::Remote<mojom::MeetBrowser>& GetMeetBrowserRemote() {
    if (!MeetBrowserService::IsInitialized()) {
      MeetBrowserService::Initialize();
    }
    if (meet_browser_remote_.is_bound()) {
      return meet_browser_remote_;
    }

    // if there is no valid remote create one
    auto* interface_name = mojom::MeetBrowser::Name_;

    base::RunLoop run_loop;

    // Fake out CfmServiceContext
    fake_service_connection_.SetCallback(base::BindLambdaForTesting(
        [&](mojo::PendingReceiver<chromeos::cfm::mojom::CfmServiceContext>
                pending_receiver,
            bool success) {
          ASSERT_TRUE(success);
          context_receiver_set_.Add(&context_, std::move(pending_receiver));
        }));

    context_.SetFakeProvideAdaptorCallback(base::BindLambdaForTesting(
        [&](const std::string& service_id,
            mojo::PendingRemote<chromeos::cfm::mojom::CfmServiceAdaptor>
                pending_adaptor_remote,
            chromeos::cfm::mojom::CfmServiceContext::ProvideAdaptorCallback
                callback) {
          ASSERT_EQ(interface_name, service_id);
          adaptor_remote_.Bind(std::move(pending_adaptor_remote));
          std::move(callback).Run(true);
          run_loop.Quit();
        }));

    EXPECT_TRUE(GetClient()->FakeEmitSignal(interface_name));
    run_loop.Run();

    EXPECT_TRUE(adaptor_remote_.is_connected());

    adaptor_remote_->OnBindService(
        meet_browser_remote_.BindNewPipeAndPassReceiver().PassPipe());
    EXPECT_TRUE(meet_browser_remote_.is_connected());

    return meet_browser_remote_;
  }

 protected:
  chromeos::cfm::FakeCfmServiceContext context_;
  mojo::Remote<mojom::MeetBrowser> meet_browser_remote_;
  mojo::ReceiverSet<chromeos::cfm::mojom::CfmServiceContext>
      context_receiver_set_;
  mojo::Remote<chromeos::cfm::mojom::CfmServiceAdaptor> adaptor_remote_;
  chromeos::cfm::FakeServiceConnectionImpl fake_service_connection_;
  content::BrowserTaskEnvironment task_environment_;
};

// This test ensures that the MeetBrowserService is discoverable by its
// mojom name by sending a signal received by CfmHotlineClient.
TEST_F(CfmMeetBrowserServiceTest, MeetBrowserServiceAvailable) {
  ASSERT_TRUE(GetClient()->FakeEmitSignal(mojom::MeetBrowser::Name_));
}

// This test ensures that the MeetBrowserService correctly registers itself
// for discovery by the cfm mojom binder daemon and correctly returns a
// working mojom remote.
TEST_F(CfmMeetBrowserServiceTest, GetMeetBrowserRemote) {
  ASSERT_TRUE(GetMeetBrowserRemote().is_connected());
}

}  // namespace ash::cfm
