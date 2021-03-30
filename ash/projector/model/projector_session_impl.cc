// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/model/projector_session_impl.h"

namespace ash {

ProjectorSessionImpl::ProjectorSessionImpl() = default;

ProjectorSessionImpl::~ProjectorSessionImpl() = default;

void ProjectorSessionImpl::Start(SourceType preset_source_type) {
  DCHECK(!active_);

  preset_source_type_ = preset_source_type;
  active_ = true;
  NotifySessionActiveStateChanged(active_);
}

void ProjectorSessionImpl::Stop() {
  DCHECK(active_);

  preset_source_type_ = SourceType::kUnset;
  active_ = false;
  NotifySessionActiveStateChanged(active_);
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