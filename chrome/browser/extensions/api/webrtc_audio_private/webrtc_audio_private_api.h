// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEBRTC_AUDIO_PRIVATE_WEBRTC_AUDIO_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEBRTC_AUDIO_PRIVATE_WEBRTC_AUDIO_PRIVATE_API_H_

#include <stddef.h>

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/system/system_monitor.h"
#include "chrome/common/extensions/api/webrtc_audio_private.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_context.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"
#include "media/audio/audio_device_description.h"

namespace media {
class AudioSystem;
}

namespace extensions {

// Listens for device changes and forwards as an extension event.
class WebrtcAudioPrivateEventService
    : public BrowserContextKeyedAPI,
      public base::SystemMonitor::DevicesChangedObserver {
 public:
  explicit WebrtcAudioPrivateEventService(content::BrowserContext* context);
  ~WebrtcAudioPrivateEventService() override;

  // BrowserContextKeyedAPI implementation.
  void Shutdown() override;
  static BrowserContextKeyedAPIFactory<WebrtcAudioPrivateEventService>*
      GetFactoryInstance();
  static const char* service_name();

  // base::SystemMonitor::DevicesChangedObserver implementation.
  void OnDevicesChanged(base::SystemMonitor::DeviceType device_type) override;

 private:
  friend class BrowserContextKeyedAPIFactory<WebrtcAudioPrivateEventService>;

  void SignalEvent();

  content::BrowserContext* browser_context_;
};

// Common base for WebrtcAudioPrivate functions, that provides a
// couple of optionally-used common implementations.
class WebrtcAudioPrivateFunction : public ExtensionFunction {
 protected:
  WebrtcAudioPrivateFunction();
  ~WebrtcAudioPrivateFunction() override;

 protected:
  // Calculates a single HMAC, using the extension ID as the security origin.
  std::string CalculateHMAC(const std::string& raw_id);

  // Initializes |device_id_salt_|. Must be called on the UI thread,
  // before any calls to |device_id_salt()|.
  void InitDeviceIDSalt();

  // Callable from any thread. Must previously have called
  // |InitDeviceIDSalt()|.
  std::string device_id_salt() const;

  media::AudioSystem* GetAudioSystem();

 private:
  std::string device_id_salt_;
  std::unique_ptr<media::AudioSystem> audio_system_;

  DISALLOW_COPY_AND_ASSIGN(WebrtcAudioPrivateFunction);
};

class WebrtcAudioPrivateGetSinksFunction : public WebrtcAudioPrivateFunction {
 protected:
  ~WebrtcAudioPrivateGetSinksFunction() override {}

 private:
  using SinkInfoVector = std::vector<api::webrtc_audio_private::SinkInfo>;

  DECLARE_EXTENSION_FUNCTION("webrtcAudioPrivate.getSinks",
                             WEBRTC_AUDIO_PRIVATE_GET_SINKS)

  ResponseAction Run() override;

  // Receives output device descriptions, calculates HMACs for them and sends
  // the response.
  void ReceiveOutputDeviceDescriptions(
      media::AudioDeviceDescriptions sink_devices);
};

class WebrtcAudioPrivateGetAssociatedSinkFunction
    : public WebrtcAudioPrivateFunction {
 public:
  WebrtcAudioPrivateGetAssociatedSinkFunction();

 protected:
  ~WebrtcAudioPrivateGetAssociatedSinkFunction() override;

 private:
  DECLARE_EXTENSION_FUNCTION("webrtcAudioPrivate.getAssociatedSink",
                             WEBRTC_AUDIO_PRIVATE_GET_ASSOCIATED_SINK)

  // UI thread: Entry point, posts GetInputDeviceDescriptions() to IO thread.
  ResponseAction Run() override;

  // Receives the input device descriptions, looks up the raw source device ID
  // basing on |params|, and requests the associated raw sink ID for it.
  void ReceiveInputDeviceDescriptions(
      media::AudioDeviceDescriptions source_devices);

  // Receives the raw sink ID, calculates HMAC and calls Reply().
  void CalculateHMACAndReply(const base::Optional<std::string>& raw_sink_id);

  // Receives the associated sink ID as HMAC and sends the response.
  void Reply(const std::string& hmac);

  std::unique_ptr<api::webrtc_audio_private::GetAssociatedSink::Params> params_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEBRTC_AUDIO_PRIVATE_WEBRTC_AUDIO_PRIVATE_API_H_
