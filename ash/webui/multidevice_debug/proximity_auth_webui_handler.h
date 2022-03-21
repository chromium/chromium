// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_MULTIDEVICE_DEBUG_PROXIMITY_AUTH_WEBUI_HANDLER_H_
#define ASH_WEBUI_MULTIDEVICE_DEBUG_PROXIMITY_AUTH_WEBUI_HANDLER_H_

#include "ash/components/multidevice/logging/log_buffer.h"
#include "ash/components/multidevice/remote_device_ref.h"
#include "ash/services/device_sync/public/cpp/device_sync_client.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace multidevice {

// Handles messages from the chrome://proximity-auth page.
class ProximityAuthWebUIHandler
    : public content::WebUIMessageHandler,
      public multidevice::LogBuffer::Observer,
      public device_sync::DeviceSyncClient::Observer {
 public:
  explicit ProximityAuthWebUIHandler(
      device_sync::DeviceSyncClient* device_sync_client);

  ProximityAuthWebUIHandler(const ProximityAuthWebUIHandler&) = delete;
  ProximityAuthWebUIHandler& operator=(const ProximityAuthWebUIHandler&) =
      delete;

  ~ProximityAuthWebUIHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  // multidevice::LogBuffer::Observer:
  void OnLogMessageAdded(
      const multidevice::LogBuffer::LogMessage& log_message) override;
  void OnLogBufferCleared() override;

  // device_sync::DeviceSyncClient::Observer:
  void OnEnrollmentFinished() override;
  void OnNewDevicesSynced() override;

  // Message handler callbacks.
  void OnWebContentsInitialized(const base::ListValue* args);
  void GetLogMessages(const base::ListValue* args);
  void ClearLogBuffer(const base::ListValue* args);
  void GetLocalState(const base::ListValue* args);
  void ForceEnrollment(const base::ListValue* args);
  void ForceDeviceSync(const base::ListValue* args);

  base::Value::Dict RemoteDeviceToDictionary(
      const multidevice::RemoteDeviceRef& remote_device);

  void OnForceEnrollmentNow(bool success);
  void OnForceSyncNow(bool success);
  void OnSetSoftwareFeatureState(
      const std::string public_key,
      device_sync::mojom::NetworkRequestResult result_code);
  void OnGetDebugInfo(device_sync::mojom::DebugInfoPtr debug_info_ptr);

  void NotifyOnEnrollmentFinished(
      bool success,
      std::unique_ptr<base::DictionaryValue> enrollment_state);
  void NotifyOnSyncFinished(
      bool was_sync_successful,
      bool changed,
      std::unique_ptr<base::DictionaryValue> device_sync_state);
  void NotifyGotLocalState(
      std::unique_ptr<base::Value> truncated_local_device_id,
      std::unique_ptr<base::DictionaryValue> enrollment_state,
      std::unique_ptr<base::DictionaryValue> device_sync_state,
      std::unique_ptr<base::ListValue> synced_devices);

  std::unique_ptr<base::Value> GetTruncatedLocalDeviceId();
  std::unique_ptr<base::ListValue> GetRemoteDevicesList();

  // The delegate used to fetch dependencies. Must outlive this instance.
  device_sync::DeviceSyncClient* device_sync_client_;

  // True if we get a message from the loaded WebContents to know that it is
  // initialized, and we can inject JavaScript.
  bool web_contents_initialized_;

  bool enrollment_update_waiting_for_debug_info_ = false;
  bool sync_update_waiting_for_debug_info_ = false;
  bool get_local_state_update_waiting_for_debug_info_ = false;

  base::WeakPtrFactory<ProximityAuthWebUIHandler> weak_ptr_factory_{this};
};

}  // namespace multidevice

}  // namespace ash

#endif  // ASH_WEBUI_MULTIDEVICE_DEBUG_PROXIMITY_AUTH_WEBUI_HANDLER_H_
