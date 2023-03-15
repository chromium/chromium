// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/media_ui_ash.h"

#include "base/unguessable_token.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/media_ui.mojom.h"
#include "components/global_media_controls/public/mojom/device_service.mojom-forward.h"
#include "content/public/test/browser_test.h"

using testing::_;

namespace mojom {
using global_media_controls::mojom::DeviceListClient;
using global_media_controls::mojom::DeviceListHost;
using global_media_controls::mojom::DeviceService;
}  // namespace mojom

namespace crosapi {

namespace {

class MockDeviceListClient : public ::mojom::DeviceListClient {
 public:
  MOCK_METHOD(void,
              OnDevicesUpdated,
              (std::vector<global_media_controls::mojom::DevicePtr> devices));
};

class MockDeviceService : public ::mojom::DeviceService {
 public:
  mojo::PendingRemote<::mojom::DeviceService> TakeRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void ResetReceiver() { receiver_.reset(); }

  MOCK_METHOD(void,
              GetDeviceListHostForSession,
              (const std::string& session_id,
               mojo::PendingReceiver<::mojom::DeviceListHost> host_receiver,
               mojo::PendingRemote<::mojom::DeviceListClient> client_remote));
  MOCK_METHOD(void,
              GetDeviceListHostForPresentation,
              (mojo::PendingReceiver<::mojom::DeviceListHost> host_receiver,
               mojo::PendingRemote<::mojom::DeviceListClient> client_remote));

 private:
  mojo::Receiver<::mojom::DeviceService> receiver_{this};
};

}  // namespace

class MediaUIAshBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    CrosapiManager::Get()->crosapi_ash()->BindMediaUI(
        media_ui_remote_.BindNewPipeAndPassReceiver());
  }

  // Returns the ID of the registered DeviceService.
  base::UnguessableToken RegisterDeviceService() {
    auto id = base::UnguessableToken::Create();
    media_ui_remote_->RegisterDeviceService(id, device_service_.TakeRemote());
    media_ui_remote_.FlushForTesting();
    return id;
  }

  MediaUIAsh* media_ui_ash() {
    return CrosapiManager::Get()->crosapi_ash()->media_ui_ash();
  }

 protected:
  mojo::Remote<mojom::MediaUI> media_ui_remote_;
  MockDeviceService device_service_;
  mojo::Remote<::mojom::DeviceListHost> device_list_host_remote_;
  MockDeviceListClient device_list_client_;
  mojo::Receiver<::mojom::DeviceListClient> device_list_client_receiver_{
      &device_list_client_};
};

IN_PROC_BROWSER_TEST_F(MediaUIAshBrowserTest, GetDeviceService) {
  base::UnguessableToken id = RegisterDeviceService();
  auto* device_service_ptr = media_ui_ash()->GetDeviceService(id);
  ASSERT_NE(nullptr, device_service_ptr);

  const std::string kSessionId = "session_id";
  EXPECT_CALL(device_service_, GetDeviceListHostForSession(kSessionId, _, _));
  device_service_ptr->GetDeviceListHostForSession(
      kSessionId, device_list_host_remote_.BindNewPipeAndPassReceiver(),
      device_list_client_receiver_.BindNewPipeAndPassRemote());
}

IN_PROC_BROWSER_TEST_F(MediaUIAshBrowserTest, GetInvalidDeviceService) {
  RegisterDeviceService();
  auto invalid_id = base::UnguessableToken::Create();
  EXPECT_EQ(nullptr, media_ui_ash()->GetDeviceService(invalid_id));
}

IN_PROC_BROWSER_TEST_F(MediaUIAshBrowserTest, UnregisterDeviceService) {
  base::UnguessableToken id = RegisterDeviceService();
  auto* device_service_ptr = media_ui_ash()->GetDeviceService(id);
  EXPECT_NE(nullptr, device_service_ptr);

  // After disconnecting a DeviceService, MediaUIAsh should return a nullptr
  // when asked for that DeviceService.
  device_service_.ResetReceiver();
  base::RunLoop().RunUntilIdle();
  device_service_ptr = media_ui_ash()->GetDeviceService(id);
  EXPECT_EQ(nullptr, device_service_ptr);
}

}  // namespace crosapi
