// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SECURITY_EVENTS_SECURITY_EVENT_RECORDER_IMPL_H_
#define CHROME_BROWSER_SECURITY_EVENTS_SECURITY_EVENT_RECORDER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/time/clock.h"
#include "chrome/browser/security_events/security_event_recorder.h"
#include "chrome/browser/security_events/security_event_sync_bridge.h"

class SecurityEventRecorderImpl : public SecurityEventRecorder {
 public:
  SecurityEventRecorderImpl(
      std::unique_ptr<SecurityEventSyncBridge> security_event_sync_bridge,
      base::Clock* clock);
  ~SecurityEventRecorderImpl() override;

  void RecordGaiaPasswordReuse(
      const sync_pb::GaiaPasswordReuse& event) override;

  base::WeakPtr<syncer::ModelTypeControllerDelegate> GetControllerDelegate()
      override;

  // KeyedService (through SecurityEventRecorder) implementation.
  void Shutdown() override;

 private:
  std::unique_ptr<SecurityEventSyncBridge> security_event_sync_bridge_;
  base::Clock* clock_;

  DISALLOW_COPY_AND_ASSIGN(SecurityEventRecorderImpl);
};

#endif  // CHROME_BROWSER_SECURITY_EVENTS_SECURITY_EVENT_RECORDER_IMPL_H_
