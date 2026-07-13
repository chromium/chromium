// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sensor/chrome_sensor_delegate.h"

#include "base/feature_list.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "content/public/browser/render_frame_host.h"

ChromeSensorDelegate::ChromeSensorDelegate() = default;
ChromeSensorDelegate::~ChromeSensorDelegate() = default;

void ChromeSensorDelegate::SetRequestedSensorIsAvailable(
    content::RenderFrameHost* render_frame_host,
    bool is_available) {
  CHECK(render_frame_host);
  auto* settings = content_settings::PageSpecificContentSettings::GetForFrame(
      render_frame_host);
  if (settings) {
    settings->SetRequestedSensorIsAvailable(is_available);
  }
}

void ChromeSensorDelegate::OnSensorStarted(
    content::RenderFrameHost* render_frame_host) {
  CHECK(render_frame_host);
  auto* settings = content_settings::PageSpecificContentSettings::GetForFrame(
      render_frame_host);
  if (settings) {
    settings->OnContentAllowed(ContentSettingsType::SENSORS);
    settings->OnSensorStarted();
  }
}

void ChromeSensorDelegate::OnSensorStopped(
    content::RenderFrameHost* render_frame_host) {
  CHECK(render_frame_host);
  auto* settings = content_settings::PageSpecificContentSettings::GetForFrame(
      render_frame_host);
  if (settings) {
    settings->OnSensorStopped();
  }
}

void ChromeSensorDelegate::OnSensorBlocked(
    content::RenderFrameHost* render_frame_host) {
  CHECK(render_frame_host);
  auto* settings = content_settings::PageSpecificContentSettings::GetForFrame(
      render_frame_host);
  if (settings) {
    settings->OnContentBlocked(ContentSettingsType::SENSORS);
  }
}
