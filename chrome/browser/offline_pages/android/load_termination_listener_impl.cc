// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/android/load_termination_listener_impl.h"

#include "base/android/application_status_listener.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/system/sys_info.h"

namespace offline_pages {

LoadTerminationListenerImpl::LoadTerminationListenerImpl() {
  if (base::SysInfo::IsLowEndDeviceOrPartialLowEndModeEnabled()) {
    app_listener_ =
        base::android::ApplicationStatusListener::New(base::BindRepeating(
            &LoadTerminationListenerImpl::OnApplicationStateChange,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

LoadTerminationListenerImpl::~LoadTerminationListenerImpl() {
}

void LoadTerminationListenerImpl::OnApplicationStateChange(
    base::android::ApplicationState application_state) {
  if (offliner_ &&
      application_state ==
          base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES) {
    DVLOG(1) << "App became foreground, canceling current offlining request";
    offliner_->TerminateLoadIfInProgress();
  }
}

}  // namespace offline_pages
