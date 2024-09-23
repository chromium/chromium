// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_CAMERA_APP_UI_CAMERA_APP_EVENTS_SENDER_H_
#define ASH_WEBUI_CAMERA_APP_UI_CAMERA_APP_EVENTS_SENDER_H_

#include "ash/webui/camera_app_ui/events_sender.mojom.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

class CameraAppEventsSender : public camera_app::mojom::EventsSender {
 public:
  explicit CameraAppEventsSender(std::string system_language);
  CameraAppEventsSender(const CameraAppEventsSender&) = delete;
  CameraAppEventsSender& operator=(const CameraAppEventsSender&) = delete;
  ~CameraAppEventsSender() override;

  // Creates the mojo connection, binds the receiver and returns the remote.
  mojo::PendingRemote<camera_app::mojom::EventsSender> CreateConnection();

  // camera_app::mojom::EventsSender implementations.
  void SendStartSessionEvent(
      camera_app::mojom::StartSessionEventParamsPtr params) override;

  void SendCaptureEvent(
      camera_app::mojom::CaptureEventParamsPtr params) override;

  void SendAndroidIntentEvent(
      camera_app::mojom::AndroidIntentEventParamsPtr params) override;

  void SendOpenPTZPanelEvent(
      camera_app::mojom::OpenPTZPanelEventParamsPtr params) override;

  void SendDocScanActionEvent(
      camera_app::mojom::DocScanActionEventParamsPtr params) override;

  void SendDocScanResultEvent(
      camera_app::mojom::DocScanResultEventParamsPtr params) override;

  void SendOpenCameraEvent(
      camera_app::mojom::OpenCameraEventParamsPtr params) override;

  void SendLowStorageActionEvent(
      camera_app::mojom::LowStorageActionEventParamsPtr params) override;

  void SendBarcodeDetectedEvent(
      camera_app::mojom::BarcodeDetectedEventParamsPtr params) override;

  void SendPerfEvent(camera_app::mojom::PerfEventParamsPtr params) override;

  void SendUnsupportedProtocolEvent() override;

  void UpdateMemoryUsageEventParams(
      camera_app::mojom::MemoryUsageEventParamsPtr params) override;

  void SendOcrEvent(camera_app::mojom::OcrEventParamsPtr params) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(CameraAppEventsSenderTest, EndSession);
  FRIEND_TEST_ALL_PREFIXES(CameraAppEventsSenderTest, MemoryUsage);

  friend class CameraAppEventsSenderTest;

  void OnMojoDisconnected();

  std::string system_language_;

  std::optional<base::TimeTicks> start_time_;

  camera_app::mojom::MemoryUsageEventParamsPtr session_memory_usage_;

  mojo::ReceiverSet<camera_app::mojom::EventsSender> receivers_;

  base::WeakPtrFactory<CameraAppEventsSender> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WEBUI_CAMERA_APP_UI_CAMERA_APP_EVENTS_SENDER_H_
