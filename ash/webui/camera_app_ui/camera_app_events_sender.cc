// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/camera_app_ui/camera_app_events_sender.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/metrics_hashes.h"
#include "base/notreached.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/structured_metrics_client.h"

namespace ash {

namespace {

namespace cros_events = metrics::structured::events::v2::cr_os_events;

bool CanSendEvents() {
  return base::FeatureList::IsEnabled(ash::features::kCameraAppCrosEvents);
}

camera_app::mojom::PhotoDetailsPtr* GetPhotoDetails(
    const camera_app::mojom::CaptureEventParamsPtr& params) {
  auto& capture_details = params->capture_details;
  if (capture_details.is_null()) {
    return nullptr;
  }
  if (!capture_details->is_photo_details()) {
    return nullptr;
  }
  return &capture_details->get_photo_details();
}

camera_app::mojom::VideoDetailsPtr* GetVideoDetails(
    const camera_app::mojom::CaptureEventParamsPtr& params) {
  auto& capture_details = params->capture_details;
  if (capture_details.is_null()) {
    return nullptr;
  }
  if (!capture_details->is_video_details()) {
    return nullptr;
  }
  return &capture_details->get_video_details();
}

bool GetIsVideoSnapshot(
    const camera_app::mojom::CaptureEventParamsPtr& params) {
  auto* photo_details = GetPhotoDetails(params);
  if (photo_details == nullptr) {
    return false;
  }
  return (*photo_details)->is_video_snapshot;
}

bool GetIsMuted(const camera_app::mojom::CaptureEventParamsPtr& params) {
  auto* video_details = GetVideoDetails(params);
  if (video_details == nullptr) {
    return false;
  }
  return (*video_details)->is_muted;
}

int GetFps(const camera_app::mojom::CaptureEventParamsPtr& params) {
  auto* video_details = GetVideoDetails(params);
  if (video_details == nullptr) {
    return 0;
  }
  return (*video_details)->fps;
}

bool GetEverPaused(const camera_app::mojom::CaptureEventParamsPtr& params) {
  auto* video_details = GetVideoDetails(params);
  if (video_details == nullptr) {
    return false;
  }
  return (*video_details)->ever_paused;
}

int GetDuration(const camera_app::mojom::CaptureEventParamsPtr& params) {
  auto* video_details = GetVideoDetails(params);
  if (video_details == nullptr) {
    return 0;
  }
  return (*video_details)->duration;
}

camera_app::mojom::RecordType GetRecordType(
    const camera_app::mojom::CaptureEventParamsPtr& params) {
  auto* video_details = GetVideoDetails(params);
  if (video_details == nullptr) {
    return camera_app::mojom::RecordType::kNotRecording;
  }
  auto& record_type_details = (*video_details)->record_type_details;
  if (record_type_details.is_null()) {
    return camera_app::mojom::RecordType::kNotRecording;
  }
  if (record_type_details->is_normal_video_details()) {
    return camera_app::mojom::RecordType::kNormal;
  } else if (record_type_details->is_gif_video_details()) {
    return camera_app::mojom::RecordType::kGif;
  } else if (record_type_details->is_timelapse_video_details()) {
    return camera_app::mojom::RecordType::kTimelapse;
  } else {
    NOTREACHED_NORETURN() << "Unexpected record type";
  }
}

camera_app::mojom::GifResultType GetGifResultType(
    const camera_app::mojom::CaptureEventParamsPtr& params) {
  auto* video_details = GetVideoDetails(params);
  if (video_details == nullptr) {
    return camera_app::mojom::GifResultType::kNotGif;
  }
  auto& record_type_details = (*video_details)->record_type_details;
  if (record_type_details.is_null() ||
      !record_type_details->is_gif_video_details()) {
    return camera_app::mojom::GifResultType::kNotGif;
  }
  auto& gif_video_details = record_type_details->get_gif_video_details();
  return gif_video_details->gif_result_type;
}

int GetTimelapseSpeed(const camera_app::mojom::CaptureEventParamsPtr& params) {
  auto* video_details = GetVideoDetails(params);
  if (video_details == nullptr) {
    return 0;
  }
  auto& record_type_details = (*video_details)->record_type_details;
  if (record_type_details.is_null() ||
      !record_type_details->is_timelapse_video_details()) {
    return 0;
  }
  auto& timelapse_video_details =
      record_type_details->get_timelapse_video_details();
  return timelapse_video_details->timelapse_speed;
}

}  // namespace

CameraAppEventsSender::CameraAppEventsSender(std::string system_language)
    : system_language_(std::move(system_language)) {
  receivers_.set_disconnect_handler(
      base::BindRepeating(&CameraAppEventsSender::OnMojoDisconnected,
                          weak_ptr_factory_.GetWeakPtr()));
}

CameraAppEventsSender::~CameraAppEventsSender() = default;

mojo::PendingRemote<camera_app::mojom::EventsSender>
CameraAppEventsSender::CreateConnection() {
  mojo::PendingRemote<camera_app::mojom::EventsSender> remote;
  receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void CameraAppEventsSender::SendStartSessionEvent(
    camera_app::mojom::StartSessionEventParamsPtr params) {
  if (!CanSendEvents()) {
    return;
  }

  auto language = static_cast<base::HistogramBase::Sample>(
      base::HashMetricName(system_language_));
  metrics::structured::StructuredMetricsClient::Record(std::move(
      cros_events::CameraApp_StartSession()
          .SetLaunchType(static_cast<cros_events::CameraAppLaunchType>(
              params->launch_type))
          .SetLanguage(static_cast<int64_t>(language))));
  start_time_ = base::TimeTicks::Now();
}

void CameraAppEventsSender::SendCaptureEvent(
    camera_app::mojom::CaptureEventParamsPtr params) {
  if (!CanSendEvents()) {
    return;
  }

  metrics::structured::StructuredMetricsClient::Record(std::move(
      cros_events::CameraApp_Capture()
          .SetMode(static_cast<cros_events::CameraAppMode>(params->mode))
          .SetFacing(static_cast<cros_events::CameraAppFacing>(params->facing))
          .SetIsMirrored(static_cast<int64_t>(params->is_mirrored))
          .SetGridType(
              static_cast<cros_events::CameraAppGridType>(params->grid_type))
          .SetTimerType(
              static_cast<cros_events::CameraAppTimerType>(params->timer_type))
          .SetShutterType(static_cast<cros_events::CameraAppShutterType>(
              params->shutter_type))
          .SetAndroidIntentResultType(
              static_cast<cros_events::CameraAppAndroidIntentResultType>(
                  params->android_intent_result_type))
          .SetIsWindowMaximized(
              static_cast<int64_t>(params->is_window_maximized))
          .SetIsWindowPortrait(static_cast<int64_t>(params->is_window_portrait))
          .SetResolutionWidth(static_cast<int64_t>(params->resolution_width))
          .SetResolutionHeight(static_cast<int64_t>(params->resolution_height))
          .SetResolutionLevel(
              static_cast<cros_events::CameraAppResolutionLevel>(
                  params->resolution_level))
          .SetAspectRatioSet(static_cast<cros_events::CameraAppAspectRatioSet>(
              params->aspect_ratio_set))
          .SetIsVideoSnapshot(static_cast<int64_t>(GetIsVideoSnapshot(params)))
          .SetIsMuted(static_cast<int64_t>(GetIsMuted(params)))
          .SetFps(static_cast<int64_t>(GetFps(params)))
          .SetEverPaused(static_cast<int64_t>(GetEverPaused(params)))
          .SetDuration(static_cast<int64_t>(GetDuration(params)))
          .SetRecordType(static_cast<cros_events::CameraAppRecordType>(
              GetRecordType(params)))
          .SetGifResultType(static_cast<cros_events::CameraAppGifResultType>(
              GetGifResultType(params)))
          .SetTimelapseSpeed(static_cast<int64_t>(GetTimelapseSpeed(params)))));
}

void CameraAppEventsSender::SendAndroidIntentEvent(
    camera_app::mojom::AndroidIntentEventParamsPtr params) {
  if (!CanSendEvents()) {
    return;
  }

  metrics::structured::StructuredMetricsClient::Record(std::move(
      cros_events::CameraApp_AndroidIntent()
          .SetMode(static_cast<cros_events::CameraAppMode>(params->mode))
          .SetShouldHandleResult(
              static_cast<int64_t>(params->should_handle_result))
          .SetShouldDownscale(static_cast<int64_t>(params->should_downscale))
          .SetIsSecure(static_cast<int64_t>(params->is_secure))));
}

void CameraAppEventsSender::SendOpenPTZPanelEvent(
    camera_app::mojom::OpenPTZPanelEventParamsPtr params) {
  if (!CanSendEvents()) {
    return;
  }

  metrics::structured::StructuredMetricsClient::Record(std::move(
      cros_events::CameraApp_OpenPTZPanel()
          .SetSupportPan(static_cast<int64_t>(params->support_pan))
          .SetSupportTilt(static_cast<int64_t>(params->support_tilt))
          .SetSupportZoom(static_cast<int64_t>(params->support_zoom))));
}

void CameraAppEventsSender::SendDocScanActionEvent(
    camera_app::mojom::DocScanActionEventParamsPtr params) {
  if (!CanSendEvents()) {
    return;
  }

  metrics::structured::StructuredMetricsClient::Record(
      std::move(cros_events::CameraApp_DocScanAction().SetActionType(
          static_cast<cros_events::CameraAppDocScanActionType>(
              params->action_type))));
}

void CameraAppEventsSender::SendDocScanResultEvent(
    camera_app::mojom::DocScanResultEventParamsPtr params) {
  if (!CanSendEvents()) {
    return;
  }

  metrics::structured::StructuredMetricsClient::Record(std::move(
      cros_events::CameraApp_DocScanResult()
          .SetResultType(static_cast<cros_events::CameraAppDocScanResultType>(
              params->result_type))
          .SetFixTypes(static_cast<int64_t>(params->fix_types_mask))
          .SetFixCount(static_cast<int64_t>(params->fix_count))
          .SetPageCount(static_cast<int64_t>(params->page_count))));
}

void CameraAppEventsSender::SendOpenCameraEvent(
    camera_app::mojom::OpenCameraEventParamsPtr params) {
  if (!CanSendEvents()) {
    return;
  }

  std::string camera_module_id;
  auto& camera_module = params->camera_module;
  if (camera_module->is_mipi_camera()) {
    camera_module_id = "MIPI";
  } else if (camera_module->is_usb_camera()) {
    auto& usb_camera = camera_module->get_usb_camera();
    auto id = usb_camera->id;
    if (id.has_value()) {
      camera_module_id = *id;
    } else {
      camera_module_id = "others";
    }
  } else {
    NOTREACHED_NORETURN() << "Unexpected camera module type";
  }
  metrics::structured::StructuredMetricsClient::Record(std::move(
      cros_events::CameraApp_OpenCamera().SetCameraModuleId(camera_module_id)));
}

void CameraAppEventsSender::SendLowStorageActionEvent(
    camera_app::mojom::LowStorageActionEventParamsPtr params) {
  if (!CanSendEvents()) {
    return;
  }

  metrics::structured::StructuredMetricsClient::Record(
      std::move(cros_events::CameraApp_LowStorageAction().SetActionType(
          static_cast<cros_events::CameraAppLowStorageActionType>(
              params->action_type))));
}

void CameraAppEventsSender::SendBarcodeDetectedEvent(
    camera_app::mojom::BarcodeDetectedEventParamsPtr params) {
  if (!CanSendEvents()) {
    return;
  }

  metrics::structured::StructuredMetricsClient::Record(std::move(
      cros_events::CameraApp_BarcodeDetected()
          .SetContentType(static_cast<cros_events::CameraAppBarcodeContentType>(
              params->content_type))
          .SetWifiSecurityType(
              static_cast<cros_events::CameraAppWifiSecurityType>(
                  params->wifi_security_type))));
}

void CameraAppEventsSender::SendPerfEvent(
    camera_app::mojom::PerfEventParamsPtr params) {
  if (!CanSendEvents()) {
    return;
  }

  metrics::structured::StructuredMetricsClient::Record(std::move(
      cros_events::CameraApp_Perf()
          .SetEventType(static_cast<cros_events::CameraAppPerfEventType>(
              params->event_type))
          .SetDuration(static_cast<int64_t>(params->duration))
          .SetFacing(static_cast<cros_events::CameraAppFacing>(params->facing))
          .SetResolutionWidth(static_cast<int64_t>(params->resolution_width))
          .SetResolutionHeight(
              static_cast<int64_t>(params->resolution_height))));
}

void CameraAppEventsSender::UpdateMemoryUsageEventParams(
    camera_app::mojom::MemoryUsageEventParamsPtr params) {
  if (!CanSendEvents()) {
    return;
  }

  session_memory_usage_ = params.Clone();
}

void CameraAppEventsSender::OnMojoDisconnected() {
  if (!receivers_.empty()) {
    return;
  }
  if (!start_time_.has_value()) {
    return;
  }
  if (session_memory_usage_.is_null()) {
    return;
  }
  int64_t duration = static_cast<int64_t>(
      (base::TimeTicks::Now() - start_time_.value()).InMilliseconds());
  metrics::structured::StructuredMetricsClient::Record(std::move(
      cros_events::CameraApp_EndSession()
          .SetDuration(duration)
          .SetBehaviors(
              static_cast<int64_t>(session_memory_usage_->behaviors_mask))
          .SetMemoryUsage(
              static_cast<int64_t>(session_memory_usage_->memory_usage))));
}

}  // namespace ash
