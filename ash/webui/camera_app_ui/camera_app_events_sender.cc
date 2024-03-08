// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/camera_app_ui/camera_app_events_sender.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/metrics_hashes.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/structured_metrics_client.h"

namespace ash {

namespace {

namespace cros_events = metrics::structured::events::v2::cr_os_events;

}  // namespace

CameraAppEventsSender::CameraAppEventsSender(std::string system_language)
    : system_language_(std::move(system_language)) {}

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
  metrics::structured::StructuredMetricsClient::Record(
      std::move(cros_events::CameraApp_StartSession()
                    .SetLaunchType(static_cast<int64_t>(params->launch_type))
                    .SetLanguage(static_cast<int64_t>(language))));
}

bool CameraAppEventsSender::CanSendEvents() {
  return base::FeatureList::IsEnabled(ash::features::kCameraAppCrosEvents);
}

}  // namespace ash
