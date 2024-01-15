// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEBRTC_AUDIO_PRIVATE_WEBRTC_AUDIO_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEBRTC_AUDIO_PRIVATE_WEBRTC_AUDIO_PRIVATE_API_H_

#include <stddef.h>

#include <string>

#include "base/memory/raw_ptr.h"
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

  raw_ptr<content::BrowserContext> browser_context_;
};

// Common base for WebrtcAudioPrivate functions, that provides a
// couple of optionally-used common implementations.
class WebrtcAudioPrivateFunction : public ExtensionFunction {
 public:
  WebrtcAudioPrivateFunction(const WebrtcAudioPrivateFunction&) = delete;
  WebrtcAudioPrivateFunction& operator=(const WebrtcAudioPrivateFunction&) =
      delete;

 protected:
  WebrtcAudioPrivateFunction();
  ~WebrtcAudioPrivateFunction() override;

  using SaltAndDeviceDescriptionsCallback =
      base::OnceCallback<void(const std::string&,
                              media::AudioDeviceDescriptions)>;
  // Calculates the HMAC for `raw_id` using extension ID as the security origin.
  // `extension_salt` must be the salt for the extension ID.
  std::string CalculateHMAC(const std::string& extension_salt,
                            const std::string& raw_id);

  // Returns the extension ID as an origin.
  url::Origin GetExtensionOrigin() const;

  void GetSalt(const url::Origin&,
               base::OnceCallback<void(const std::string&)> salt_callback);

  // Returns the device ID salt and device descriptions.
  void GetSaltAndDeviceDescriptions(const url::Origin& security_origin,
                                    bool is_input_devices,
                                    SaltAndDeviceDescriptionsCallback callback);

  media::AudioSystem* GetAudioSystem();

 private:
  void GotSaltForDeviceDescriptions(bool is_input_devices,
                                    SaltAndDeviceDescriptionsCallback callback,
                                    const std::string& device_id_salt);
  std::unique_ptr<media::AudioSystem> audio_system_;
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
      const std::string& extension_salt,
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
      const url::Origin& origin,
      const std::string& salt,
      media::AudioDeviceDescriptions source_devices);

  void GotExtensionSalt(const std::string& raw_source_id,
                        const std::string& extension_salt);

  // Calculates HMAC and calls Reply().
  void CalculateHMACAndReply(const std::string& extension_salt,
                             const std::optional<std::string>& raw_sink_id);

  // Receives the associated sink ID as HMAC and sends the response.
  void Reply(const std::string& hmac);

  std::optional<api::webrtc_audio_private::GetAssociatedSink::Params> params_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEBRTC_AUDIO_PRIVATE_WEBRTC_AUDIO_PRIVATE_API_H_
