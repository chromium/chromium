// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SECURITY_EVENTS_SECURITY_EVENT_RECORDER_H_
#define CHROME_BROWSER_SECURITY_EVENTS_SECURITY_EVENT_RECORDER_H_

#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/model/data_type_controller_delegate.h"

namespace sync_pb {
class GaiaPasswordReuse;
}

// The SecurityEventRecorder class allows to record security events via Chrome
// Sync for signed-in users.
class SecurityEventRecorder : public KeyedService {
 public:
  SecurityEventRecorder() = default;

  SecurityEventRecorder(const SecurityEventRecorder&) = delete;
  SecurityEventRecorder& operator=(const SecurityEventRecorder&) = delete;

  ~SecurityEventRecorder() override = default;

  // Records the GaiaPasswordReuse security event for the currently signed-in
  // user. The event is recorded via Chrome Sync.
  virtual void RecordGaiaPasswordReuse(
      const sync_pb::GaiaPasswordReuse& event) = 0;

  // Returns the underlying Sync integration point.
  virtual base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetControllerDelegate() = 0;
};

#endif  // CHROME_BROWSER_SECURITY_EVENTS_SECURITY_EVENT_RECORDER_H_
