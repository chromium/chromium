// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PRIVACY_HUB_PRIVACY_HUB_METRICS_H_
#define ASH_SYSTEM_PRIVACY_HUB_PRIVACY_HUB_METRICS_H_

namespace ash::privacy_hub_metrics {

// Report microphone mute events from system and notifications.
void LogMicrophoneEnabledFromSettings(bool enabled);
void LogMicrophoneEnabledFromNotification(bool enabled);

// Report camera mute events from system and notifications.
void LogCameraEnabledFromSettings(bool enabled);
void LogCameraEnabledFromNotification(bool enabled);

}  // namespace ash::privacy_hub_metrics

#endif  // ASH_SYSTEM_PRIVACY_HUB_PRIVACY_HUB_METRICS_H_
