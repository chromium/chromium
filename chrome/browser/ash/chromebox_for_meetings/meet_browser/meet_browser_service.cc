// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/meet_browser/meet_browser_service.h"

#include <utility>

#include "base/strings/string_util.h"
#include "chrome/browser/media/webrtc/media_device_salt_service_factory.h"
#include "chromeos/ash/components/dbus/chromebox_for_meetings/cfm_hotline_client.h"
#include "components/media_device_salt/media_device_salt_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/media_device_id.h"

namespace ash::cfm {

namespace {
void TranslateDeviceId(
    const std::string& hashed_device_id,
    base::OnceCallback<void(const std::optional<std::string>&)> callback,
    const url::Origin& security_origin,
    const std::string& salt) {
  auto translate_device_id_callback = base::BindOnce(
      [](const std::string& hashed_device_id,
         base::OnceCallback<void(const std::optional<std::string>&)> callback,
         const url::Origin& security_origin, const std::string& salt) {
        content::GetMediaDeviceIDForHMAC(
            blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, salt,
            security_origin, hashed_device_id,
            content::GetUIThreadTaskRunner({}), std::move(callback));
      },
      std::move(hashed_device_id), std::move(callback),
      std::move(security_origin), std::move(salt));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, std::move(translate_device_id_callback));
}

MeetBrowserService* g_meet_browser_service = nullptr;

}  // namespace

// static
void MeetBrowserService::Initialize() {
  CHECK(!g_meet_browser_service);
  g_meet_browser_service = new MeetBrowserService();
}

// static
void MeetBrowserService::Shutdown() {
  if (!g_meet_browser_service) {
    return;
  }

  delete g_meet_browser_service;
  g_meet_browser_service = nullptr;
}

// static
MeetBrowserService* MeetBrowserService::Get() {
  CHECK(g_meet_browser_service)
      << "MeetBrowserService::Get() called before Initialize()";
  return g_meet_browser_service;
}

// static
bool MeetBrowserService::IsInitialized() {
  return g_meet_browser_service;
}

void MeetBrowserService::SetMeetGlobalRenderFrameToken(
    const content::GlobalRenderFrameHostToken& host_token) {
  host_token_ = std::move(host_token);
}

bool MeetBrowserService::ServiceRequestReceived(
    const std::string& interface_name) {
  if (interface_name != mojom::MeetBrowser::Name_) {
    return false;
  }
  service_adaptor_.BindServiceAdaptor();
  return true;
}

void MeetBrowserService::OnBindService(
    mojo::ScopedMessagePipeHandle receiver_pipe) {
  receivers_.Add(this, mojo::PendingReceiver<mojom::MeetBrowser>(
                           std::move(receiver_pipe)));
}

void MeetBrowserService::OnAdaptorConnect(bool success) {
  if (success) {
    VLOG(2) << "Adaptor is connected.";
  } else {
    LOG(ERROR) << "Adaptor connection failed.";
  }
}

void MeetBrowserService::OnAdaptorDisconnect() {
  LOG(ERROR) << "mojom::MeetBrowser Service Adaptor has been disconnected";
  // CleanUp to follow the lifecycle of the primary CfmServiceContext
  receivers_.Clear();
}

void MeetBrowserService::TranslateVideoDeviceId(
    const std::string& hashed_device_id,
    TranslateVideoDeviceIdCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (host_token_.child_id == content::kInvalidChildProcessUniqueId ||
      hashed_device_id.empty()) {
    VLOG(2) << __func__ << "Render Frame Host undefined or device id invalid.";
    std::move(callback).Run(std::nullopt);
    return;
  }

  content::RenderFrameHost* frame_host =
      content::RenderFrameHost::FromFrameToken(host_token_);

  if (frame_host == nullptr) {
    VLOG(2) << __func__ << " frame_host == nullptr";
    std::move(callback).Run(std::nullopt);
    return;
  }

  content::BrowserContext* browser_context = frame_host->GetBrowserContext();

  if (browser_context == nullptr) {
    VLOG(2) << __func__ << " browser_context == nullptr";
    std::move(callback).Run(std::nullopt);
    return;
  }

  url::Origin security_origin = frame_host->GetLastCommittedOrigin();

  if (media_device_salt::MediaDeviceSaltService* salt_service =
          MediaDeviceSaltServiceFactory::GetInstance()->GetForBrowserContext(
              browser_context)) {
    salt_service->GetSalt(
        frame_host->GetStorageKey(),
        base::BindOnce(&TranslateDeviceId, hashed_device_id,
                       std::move(callback), std::move(security_origin)));
  } else {
    // If the embedder does not provide a salt service, use the browser
    // context's unique ID as salt.
    TranslateDeviceId(hashed_device_id, std::move(callback),
                      std::move(security_origin), browser_context->UniqueId());
  }
}

MeetBrowserService::MeetBrowserService()
    : service_adaptor_(mojom::MeetBrowser::Name_, this) {
  CfmHotlineClient::Get()->AddObserver(this);
}

MeetBrowserService::~MeetBrowserService() {
  CfmHotlineClient::Get()->RemoveObserver(this);
}

}  // namespace ash::cfm
