// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SECURITY_EVENTS_SECURITY_EVENT_RECORDER_IMPL_H_
#define CHROME_BROWSER_SECURITY_EVENTS_SECURITY_EVENT_RECORDER_IMPL_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/clock.h"
#include "chrome/browser/security_events/security_event_recorder.h"
#include "chrome/browser/security_events/security_event_sync_bridge.h"

class SecurityEventRecorderImpl : public SecurityEventRecorder {
 public:
  SecurityEventRecorderImpl(
      std::unique_ptr<SecurityEventSyncBridge> security_event_sync_bridge,
      base::Clock* clock);

  SecurityEventRecorderImpl(const SecurityEventRecorderImpl&) = delete;
  SecurityEventRecorderImpl& operator=(const SecurityEventRecorderImpl&) =
      delete;

  ~SecurityEventRecorderImpl() override;

  void RecordGaiaPasswordReuse(
      const sync_pb::GaiaPasswordReuse& event) override;

  base::WeakPtr<syncer::DataTypeControllerDelegate> GetControllerDelegate()
      override;

  // KeyedService (through SecurityEventRecorder) implementation.
  void Shutdown() override;

 private:
  std::unique_ptr<SecurityEventSyncBridge> security_event_sync_bridge_;
  raw_ptr<base::Clock> clock_;
};

#endif  // CHROME_BROWSER_SECURITY_EVENTS_SECURITY_EVENT_RECORDER_IMPL_H_
