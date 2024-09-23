// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/webrtc_audio_private/webrtc_audio_private_api.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/media/webrtc/media_device_salt_service_factory.h"
#include "components/media_device_salt/media_device_salt_service.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "media/audio/audio_system.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

using content::BrowserThread;
namespace wap = api::webrtc_audio_private;

static base::LazyInstance<BrowserContextKeyedAPIFactory<
    WebrtcAudioPrivateEventService>>::DestructorAtExit
    g_webrtc_audio_private_api_factory = LAZY_INSTANCE_INITIALIZER;

WebrtcAudioPrivateEventService::WebrtcAudioPrivateEventService(
    content::BrowserContext* context)
    : browser_context_(context) {
  // In unit tests, the SystemMonitor may not be created.
  base::SystemMonitor* system_monitor = base::SystemMonitor::Get();
  if (system_monitor)
    system_monitor->AddDevicesChangedObserver(this);
}

WebrtcAudioPrivateEventService::~WebrtcAudioPrivateEventService() {
}

void WebrtcAudioPrivateEventService::Shutdown() {
  // In unit tests, the SystemMonitor may not be created.
  base::SystemMonitor* system_monitor = base::SystemMonitor::Get();
  if (system_monitor)
    system_monitor->RemoveDevicesChangedObserver(this);
}

// static
BrowserContextKeyedAPIFactory<WebrtcAudioPrivateEventService>*
WebrtcAudioPrivateEventService::GetFactoryInstance() {
  return g_webrtc_audio_private_api_factory.Pointer();
}

// static
const char* WebrtcAudioPrivateEventService::service_name() {
  return "WebrtcAudioPrivateEventService";
}

void WebrtcAudioPrivateEventService::OnDevicesChanged(
    base::SystemMonitor::DeviceType device_type) {
  switch (device_type) {
    case base::SystemMonitor::DEVTYPE_AUDIO:
    case base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE:
      SignalEvent();
      break;
    default:
      // No action needed.
      break;
  }
}

void WebrtcAudioPrivateEventService::SignalEvent() {
  using api::webrtc_audio_private::OnSinksChanged::kEventName;

  EventRouter* router = EventRouter::Get(browser_context_);
  if (!router || !router->HasEventListener(kEventName))
    return;

  for (const scoped_refptr<const extensions::Extension>& extension :
       ExtensionRegistry::Get(browser_context_)->enabled_extensions()) {
    const ExtensionId& extension_id = extension->id();
    if (router->ExtensionHasEventListener(extension_id, kEventName) &&
        extension->permissions_data()->HasAPIPermission(
            mojom::APIPermissionID::kWebrtcAudioPrivate)) {
      std::unique_ptr<Event> event =
          std::make_unique<Event>(events::WEBRTC_AUDIO_PRIVATE_ON_SINKS_CHANGED,
                                  kEventName, base::Value::List());
      router->DispatchEventToExtension(extension_id, std::move(event));
    }
  }
}

WebrtcAudioPrivateFunction::WebrtcAudioPrivateFunction() {}

WebrtcAudioPrivateFunction::~WebrtcAudioPrivateFunction() {}

url::Origin WebrtcAudioPrivateFunction::GetExtensionOrigin() const {
  return url::Origin::Create(source_url());
}

std::string WebrtcAudioPrivateFunction::CalculateHMAC(
    const std::string& extension_salt,
    const std::string& raw_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // We don't hash the default device description, and we always return
  // "default" for the default device. There is code in SetActiveSink
  // that transforms "default" to the empty string, and code in
  // GetActiveSink that ensures we return "default" if we get the
  // empty string as the current device ID.
  if (media::AudioDeviceDescription::IsDefaultDevice(raw_id))
    return media::AudioDeviceDescription::kDefaultDeviceId;

  return content::GetHMACForMediaDeviceID(extension_salt, GetExtensionOrigin(),
                                          raw_id);
}

void WebrtcAudioPrivateFunction::GetSalt(
    const url::Origin& origin,
    base::OnceCallback<void(const std::string&)> salt_callback) {
  media_device_salt::MediaDeviceSaltService* salt_service =
      MediaDeviceSaltServiceFactory::GetInstance()->GetForBrowserContext(
          browser_context());
  if (!salt_service) {
    std::move(salt_callback).Run(browser_context()->UniqueId());
    return;
  }

  salt_service->GetSalt(blink::StorageKey::CreateFirstParty(origin),
                        std::move(salt_callback));
}

void WebrtcAudioPrivateFunction::GetSaltAndDeviceDescriptions(
    const url::Origin& origin,
    bool is_input_devices,
    SaltAndDeviceDescriptionsCallback callback) {
  GetSalt(origin, base::BindOnce(
                      &WebrtcAudioPrivateFunction::GotSaltForDeviceDescriptions,
                      this, is_input_devices, std::move(callback)));
}

void WebrtcAudioPrivateFunction::GotSaltForDeviceDescriptions(
    bool is_input_devices,
    SaltAndDeviceDescriptionsCallback callback,
    const std::string& device_id_salt) {
  GetAudioSystem()->GetDeviceDescriptions(
      is_input_devices, base::BindOnce(std::move(callback), device_id_salt));
}

media::AudioSystem* WebrtcAudioPrivateFunction::GetAudioSystem() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!audio_system_)
    audio_system_ = content::CreateAudioSystemForAudioService();
  return audio_system_.get();
}

ExtensionFunction::ResponseAction WebrtcAudioPrivateGetSinksFunction::Run() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetSaltAndDeviceDescriptions(
      GetExtensionOrigin(),
      /*is_input_devices=*/false,
      base::BindOnce(
          &WebrtcAudioPrivateGetSinksFunction::ReceiveOutputDeviceDescriptions,
          this));
  return RespondLater();
}

void WebrtcAudioPrivateGetSinksFunction::ReceiveOutputDeviceDescriptions(
    const std::string& extension_salt,
    media::AudioDeviceDescriptions sink_devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto results = std::make_unique<SinkInfoVector>();
  for (const media::AudioDeviceDescription& description : sink_devices) {
    wap::SinkInfo info;
    info.sink_id = CalculateHMAC(extension_salt, description.unique_id);
    info.sink_label = description.device_name;
    // TODO(joi): Add other parameters.
    results->push_back(std::move(info));
  }
  Respond(ArgumentList(wap::GetSinks::Results::Create(*results)));
}

WebrtcAudioPrivateGetAssociatedSinkFunction::
    WebrtcAudioPrivateGetAssociatedSinkFunction() = default;

WebrtcAudioPrivateGetAssociatedSinkFunction::
    ~WebrtcAudioPrivateGetAssociatedSinkFunction() = default;

ExtensionFunction::ResponseAction
WebrtcAudioPrivateGetAssociatedSinkFunction::Run() {
  params_ = wap::GetAssociatedSink::Params::Create(args());
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  EXTENSION_FUNCTION_VALIDATE(params_);
  url::Origin origin = url::Origin::Create(GURL(params_->security_origin));
  GetSaltAndDeviceDescriptions(
      origin, /*is_input_devices=*/true,
      base::BindOnce(&WebrtcAudioPrivateGetAssociatedSinkFunction::
                         ReceiveInputDeviceDescriptions,
                     this, origin));
  return RespondLater();
}

void WebrtcAudioPrivateGetAssociatedSinkFunction::
    ReceiveInputDeviceDescriptions(
        const url::Origin& origin,
        const std::string& salt,
        media::AudioDeviceDescriptions source_devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::string source_id_in_origin(params_->source_id_in_origin);

  // Find the raw source ID for source_id_in_origin.
  std::string raw_source_id;
  for (const auto& device : source_devices) {
    if (content::DoesMediaDeviceIDMatchHMAC(salt, origin, source_id_in_origin,
                                            device.unique_id)) {
      raw_source_id = device.unique_id;
      DVLOG(2) << "Found raw ID " << raw_source_id
               << " for source ID in origin " << source_id_in_origin;
      break;
    }
  }
  if (raw_source_id.empty()) {
    Reply(media::AudioDeviceDescription::kDefaultDeviceId);
    return;
  }
  GetSalt(GetExtensionOrigin(),
          base::BindOnce(
              &WebrtcAudioPrivateGetAssociatedSinkFunction::GotExtensionSalt,
              this, raw_source_id));
}

void WebrtcAudioPrivateGetAssociatedSinkFunction::GotExtensionSalt(
    const std::string& raw_source_id,
    const std::string& extension_salt) {
  GetAudioSystem()->GetAssociatedOutputDeviceID(
      raw_source_id,
      base::BindOnce(
          &WebrtcAudioPrivateGetAssociatedSinkFunction::CalculateHMACAndReply,
          this, extension_salt));
}

void WebrtcAudioPrivateGetAssociatedSinkFunction::CalculateHMACAndReply(
    const std::string& extension_salt,
    const std::optional<std::string>& raw_sink_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!raw_sink_id || !raw_sink_id->empty());
  // If no |raw_sink_id| is provided, the default device is used.
  Reply(CalculateHMAC(extension_salt, raw_sink_id.value_or(std::string())));
}

void WebrtcAudioPrivateGetAssociatedSinkFunction::Reply(
    const std::string& associated_sink_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::string sink_id;
  if (associated_sink_id == media::AudioDeviceDescription::kDefaultDeviceId) {
    DVLOG(2) << "Got default ID, replacing with empty ID.";
  } else {
    sink_id = associated_sink_id;
  }
  Respond(WithArguments(sink_id));
}

}  // namespace extensions
