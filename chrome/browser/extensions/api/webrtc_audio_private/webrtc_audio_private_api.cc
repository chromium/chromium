// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/webrtc_audio_private/webrtc_audio_private_api.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/system_connector.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/permissions/permissions_data.h"
#include "media/audio/audio_system.h"
#include "services/audio/public/cpp/audio_system_factory.h"
#include "services/service_manager/public/cpp/connector.h"
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
    const std::string& extension_id = extension->id();
    if (router->ExtensionHasEventListener(extension_id, kEventName) &&
        extension->permissions_data()->HasAPIPermission("webrtcAudioPrivate")) {
      std::unique_ptr<Event> event = std::make_unique<Event>(
          events::WEBRTC_AUDIO_PRIVATE_ON_SINKS_CHANGED, kEventName,
          std::make_unique<base::ListValue>());
      router->DispatchEventToExtension(extension_id, std::move(event));
    }
  }
}

WebrtcAudioPrivateFunction::WebrtcAudioPrivateFunction() {}

WebrtcAudioPrivateFunction::~WebrtcAudioPrivateFunction() {}

std::string WebrtcAudioPrivateFunction::CalculateHMAC(
    const std::string& raw_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // We don't hash the default device description, and we always return
  // "default" for the default device. There is code in SetActiveSink
  // that transforms "default" to the empty string, and code in
  // GetActiveSink that ensures we return "default" if we get the
  // empty string as the current device ID.
  if (media::AudioDeviceDescription::IsDefaultDevice(raw_id))
    return media::AudioDeviceDescription::kDefaultDeviceId;

  url::Origin security_origin = url::Origin::Create(source_url().GetOrigin());
  return content::GetHMACForMediaDeviceID(device_id_salt(), security_origin,
                                          raw_id);
}

void WebrtcAudioPrivateFunction::InitDeviceIDSalt() {
  device_id_salt_ = GetProfile()->GetMediaDeviceIDSalt();
}

std::string WebrtcAudioPrivateFunction::device_id_salt() const {
  return device_id_salt_;
}

media::AudioSystem* WebrtcAudioPrivateFunction::GetAudioSystem() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!audio_system_) {
    audio_system_ =
        audio::CreateAudioSystem(content::GetSystemConnector()->Clone());
  }
  return audio_system_.get();
}

bool WebrtcAudioPrivateGetSinksFunction::RunAsync() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  InitDeviceIDSalt();
  GetAudioSystem()->GetDeviceDescriptions(
      false,
      base::BindOnce(
          &WebrtcAudioPrivateGetSinksFunction::ReceiveOutputDeviceDescriptions,
          this));
  return true;
}

void WebrtcAudioPrivateGetSinksFunction::ReceiveOutputDeviceDescriptions(
    media::AudioDeviceDescriptions sink_devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto results = std::make_unique<SinkInfoVector>();
  for (const media::AudioDeviceDescription& description : sink_devices) {
    wap::SinkInfo info;
    info.sink_id = CalculateHMAC(description.unique_id);
    info.sink_label = description.device_name;
    // TODO(joi): Add other parameters.
    results->push_back(std::move(info));
  }
  results_ = wap::GetSinks::Results::Create(*results);
  SendResponse(true);
}

WebrtcAudioPrivateGetAssociatedSinkFunction::
    WebrtcAudioPrivateGetAssociatedSinkFunction() {}

WebrtcAudioPrivateGetAssociatedSinkFunction::
    ~WebrtcAudioPrivateGetAssociatedSinkFunction() {}

bool WebrtcAudioPrivateGetAssociatedSinkFunction::RunAsync() {
  params_ = wap::GetAssociatedSink::Params::Create(*args_);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  InitDeviceIDSalt();

  GetAudioSystem()->GetDeviceDescriptions(
      true, base::BindOnce(&WebrtcAudioPrivateGetAssociatedSinkFunction::
                               ReceiveInputDeviceDescriptions,
                           this));
  return true;
}

void WebrtcAudioPrivateGetAssociatedSinkFunction::
    ReceiveInputDeviceDescriptions(
        media::AudioDeviceDescriptions source_devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  url::Origin security_origin =
      url::Origin::Create(GURL(params_->security_origin));
  std::string source_id_in_origin(params_->source_id_in_origin);

  // Find the raw source ID for source_id_in_origin.
  std::string raw_source_id;
  for (const auto& device : source_devices) {
    if (content::DoesMediaDeviceIDMatchHMAC(device_id_salt(), security_origin,
                                            source_id_in_origin,
                                            device.unique_id)) {
      raw_source_id = device.unique_id;
      DVLOG(2) << "Found raw ID " << raw_source_id
               << " for source ID in origin " << source_id_in_origin;
      break;
    }
  }
  if (raw_source_id.empty()) {
    CalculateHMACAndReply(base::nullopt);
    return;
  }
  GetAudioSystem()->GetAssociatedOutputDeviceID(
      raw_source_id,
      base::BindOnce(
          &WebrtcAudioPrivateGetAssociatedSinkFunction::CalculateHMACAndReply,
          this));
}

void WebrtcAudioPrivateGetAssociatedSinkFunction::CalculateHMACAndReply(
    const base::Optional<std::string>& raw_sink_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!raw_sink_id || !raw_sink_id->empty());
  // If no |raw_sink_id| is provided, the default device is used.
  Reply(CalculateHMAC(raw_sink_id.value_or(std::string())));
}

void WebrtcAudioPrivateGetAssociatedSinkFunction::Reply(
    const std::string& associated_sink_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (associated_sink_id == media::AudioDeviceDescription::kDefaultDeviceId) {
    DVLOG(2) << "Got default ID, replacing with empty ID.";
    results_ = wap::GetAssociatedSink::Results::Create("");
  } else {
    results_ = wap::GetAssociatedSink::Results::Create(associated_sink_id);
  }
  SendResponse(true);
}

}  // namespace extensions
