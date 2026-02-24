// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/video_conference_common.h"

namespace ash {

TitleChangeInfo::TitleChangeInfo() = default;
TitleChangeInfo::TitleChangeInfo(const TitleChangeInfo&) = default;
TitleChangeInfo& TitleChangeInfo::operator=(const TitleChangeInfo&) = default;
TitleChangeInfo::TitleChangeInfo(TitleChangeInfo&&) noexcept = default;
TitleChangeInfo& TitleChangeInfo::operator=(TitleChangeInfo&&) noexcept =
    default;
TitleChangeInfo::~TitleChangeInfo() = default;

VideoConferenceClientUpdate::VideoConferenceClientUpdate(
    VideoConferenceAppUpdate update_type)
    : added_or_removed_app(update_type) {}

VideoConferenceClientUpdate::VideoConferenceClientUpdate() = default;
VideoConferenceClientUpdate::VideoConferenceClientUpdate(
    const VideoConferenceClientUpdate&) = default;
VideoConferenceClientUpdate& VideoConferenceClientUpdate::operator=(
    const VideoConferenceClientUpdate&) = default;
VideoConferenceClientUpdate::VideoConferenceClientUpdate(
    VideoConferenceClientUpdate&&) noexcept = default;
VideoConferenceClientUpdate& VideoConferenceClientUpdate::operator=(
    VideoConferenceClientUpdate&&) noexcept = default;
VideoConferenceClientUpdate::~VideoConferenceClientUpdate() = default;

bool VideoConferenceMediaState::operator==(
    const VideoConferenceMediaState& other) const = default;

VideoConferenceMediaUsageStatus::VideoConferenceMediaUsageStatus(
    const base::UnguessableToken& client_id)
    : client_id(client_id) {}

VideoConferenceMediaUsageStatus::VideoConferenceMediaUsageStatus(
    const VideoConferenceMediaUsageStatus&) = default;
VideoConferenceMediaUsageStatus& VideoConferenceMediaUsageStatus::operator=(
    const VideoConferenceMediaUsageStatus&) = default;
VideoConferenceMediaUsageStatus::VideoConferenceMediaUsageStatus(
    VideoConferenceMediaUsageStatus&&) noexcept = default;
VideoConferenceMediaUsageStatus& VideoConferenceMediaUsageStatus::operator=(
    VideoConferenceMediaUsageStatus&&) noexcept = default;
VideoConferenceMediaUsageStatus::~VideoConferenceMediaUsageStatus() = default;

bool VideoConferenceMediaUsageStatus::operator==(
    const VideoConferenceMediaUsageStatus& other) const = default;

}  // namespace ash
