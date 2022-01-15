// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/model/projector_session_impl.h"

#include "ash/projector/projector_metrics.h"

namespace ash {

ProjectorSessionImpl::ProjectorSessionImpl() = default;

ProjectorSessionImpl::~ProjectorSessionImpl() = default;

void ProjectorSessionImpl::Start(const std::string& storage_dir) {
  DCHECK(!active_);

  active_ = true;
  storage_dir_ = storage_dir;
  NotifySessionActiveStateChanged(active_);

  RecordCreationFlowMetrics(ProjectorCreationFlow::kSessionStarted);
}

void ProjectorSessionImpl::Stop() {
  DCHECK(active_);

  active_ = false;
  screencast_container_path_.reset();
  NotifySessionActiveStateChanged(active_);

  RecordCreationFlowMetrics(ProjectorCreationFlow::kSessionStopped);
}

void ProjectorSessionImpl::AddObserver(ProjectorSessionObserver* observer) {
  observers_.AddObserver(observer);
}

void ProjectorSessionImpl::RemoveObserver(ProjectorSessionObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ProjectorSessionImpl::NotifySessionActiveStateChanged(bool active) {
  for (ProjectorSessionObserver& observer : observers_)
    observer.OnProjectorSessionActiveStateChanged(active);
}

}  // namespace ash
