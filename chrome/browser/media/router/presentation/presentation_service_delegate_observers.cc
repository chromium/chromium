// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/presentation/presentation_service_delegate_observers.h"

#include "base/stl_util.h"

namespace media_router {

PresentationServiceDelegateObservers::PresentationServiceDelegateObservers() {}

PresentationServiceDelegateObservers::~PresentationServiceDelegateObservers() {
  for (auto& observer_pair : observers_)
    observer_pair.second->OnDelegateDestroyed();
}

void PresentationServiceDelegateObservers::AddObserver(
    int render_process_id,
    int render_frame_id,
    content::PresentationServiceDelegate::Observer* observer) {
  DCHECK(observer);

  content::GlobalFrameRoutingId rfh_id(render_process_id, render_frame_id);
  DCHECK(!base::Contains(observers_, rfh_id));
  observers_[rfh_id] = observer;
}

void PresentationServiceDelegateObservers::RemoveObserver(int render_process_id,
                                                          int render_frame_id) {
  observers_.erase(
      content::GlobalFrameRoutingId(render_process_id, render_frame_id));
}

}  // namespace media_router
