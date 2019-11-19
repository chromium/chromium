// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/security_events/security_event_recorder_impl.h"

#include <memory>
#include <utility>

using sync_pb::SecurityEventSpecifics;

SecurityEventRecorderImpl::SecurityEventRecorderImpl(
    std::unique_ptr<SecurityEventSyncBridge> security_event_sync_bridge,
    base::Clock* clock)
    : security_event_sync_bridge_(std::move(security_event_sync_bridge)),
      clock_(clock) {
  DCHECK(security_event_sync_bridge_);
  DCHECK(clock_);
}

SecurityEventRecorderImpl::~SecurityEventRecorderImpl() {}

void SecurityEventRecorderImpl::RecordGaiaPasswordReuse(
    const sync_pb::GaiaPasswordReuse& event) {
  sync_pb::SecurityEventSpecifics specifics;
  specifics.set_event_time_usec(clock_->Now().since_origin().InMicroseconds());
  sync_pb::GaiaPasswordReuse* gaia_password_reuse_event =
      specifics.mutable_gaia_password_reuse_event();
  gaia_password_reuse_event->CopyFrom(event);

  security_event_sync_bridge_->RecordSecurityEvent(std::move(specifics));
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
SecurityEventRecorderImpl::GetControllerDelegate() {
  if (security_event_sync_bridge_) {
    return security_event_sync_bridge_->GetControllerDelegate();
  }
  return base::WeakPtr<syncer::ModelTypeControllerDelegate>();
}

void SecurityEventRecorderImpl::Shutdown() {}
