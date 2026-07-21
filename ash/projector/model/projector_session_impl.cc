// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/model/projector_session_impl.h"

#include "ash/projector/projector_metrics.h"
#include "base/files/safe_base_name.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"

namespace ash {

namespace {

// Only call this function on projector session starts.
std::string GenerateScreencastName() {
  base::Time::Exploded exploded;
  base::Time::Now().LocalExplode(&exploded);
  return base::StringPrintf(
      "Screencast %04d-%02d-%02d %02d.%02d.%02d", exploded.year, exploded.month,
      exploded.day_of_month, exploded.hour, exploded.minute, exploded.second);
}

}  // namespace

ProjectorSessionImpl::ProjectorSessionImpl() = default;

ProjectorSessionImpl::~ProjectorSessionImpl() = default;

void ProjectorSessionImpl::Start(const base::SafeBaseName& storage_dir) {
  DCHECK(!active_);

  active_ = true;
  storage_dir_ = storage_dir;
  screencast_name_ = GenerateScreencastName();

  NotifySessionActiveStateChanged(active_);

  RecordCreationFlowMetrics(ProjectorCreationFlow::kSessionStarted);
}

void ProjectorSessionImpl::Stop() {
  DCHECK(active_);

  active_ = false;
  screencast_container_path_.reset();
  screencast_name_ = std::string();
  NotifySessionActiveStateChanged(active_);

  RecordCreationFlowMetrics(ProjectorCreationFlow::kSessionStopped);
}

void ProjectorSessionImpl::AddObserver(ProjectorSessionObserver* observer) {
  observers_.AddObserver(observer);
}

void ProjectorSessionImpl::RemoveObserver(ProjectorSessionObserver* observer) {
  observers_.RemoveObserver(observer);
}

base::FilePath ProjectorSessionImpl::GetScreencastFilePathNoExtension() const {
  DCHECK(screencast_container_path_.has_value());
  DCHECK(!screencast_name_.empty());
  return screencast_container_path_->Append(screencast_name_);
}

void ProjectorSessionImpl::NotifySessionActiveStateChanged(bool active) {
  for (ProjectorSessionObserver& observer : observers_)
    observer.OnProjectorSessionActiveStateChanged(active);
}

}  // namespace ash
