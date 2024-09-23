// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SECURITY_EVENTS_SECURITY_EVENT_SYNC_BRIDGE_H_
#define CHROME_BROWSER_SECURITY_EVENTS_SECURITY_EVENT_SYNC_BRIDGE_H_

#include "base/memory/weak_ptr.h"
#include "components/sync/model/data_type_controller_delegate.h"

namespace sync_pb {
class SecurityEventSpecifics;
}

class SecurityEventSyncBridge {
 public:
  SecurityEventSyncBridge() = default;

  SecurityEventSyncBridge(const SecurityEventSyncBridge&) = delete;
  SecurityEventSyncBridge& operator=(const SecurityEventSyncBridge&) = delete;

  virtual ~SecurityEventSyncBridge() = default;

  virtual void RecordSecurityEvent(
      sync_pb::SecurityEventSpecifics specifics) = 0;

  // Returns the delegate for the controller, i.e. sync integration point.
  virtual base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetControllerDelegate() = 0;
};

#endif  // CHROME_BROWSER_SECURITY_EVENTS_SECURITY_EVENT_SYNC_BRIDGE_H_
