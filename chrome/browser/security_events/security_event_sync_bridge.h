// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SECURITY_EVENTS_SECURITY_EVENT_SYNC_BRIDGE_H_
#define CHROME_BROWSER_SECURITY_EVENTS_SECURITY_EVENT_SYNC_BRIDGE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/sync/model/model_type_controller_delegate.h"
#include "components/sync/protocol/sync.pb.h"

class SecurityEventSyncBridge {
 public:
  SecurityEventSyncBridge() = default;
  virtual ~SecurityEventSyncBridge() = default;

  virtual void RecordSecurityEvent(
      sync_pb::SecurityEventSpecifics specifics) = 0;

  // Returns the delegate for the controller, i.e. sync integration point.
  virtual base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetControllerDelegate() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SecurityEventSyncBridge);
};

#endif  // CHROME_BROWSER_SECURITY_EVENTS_SECURITY_EVENT_SYNC_BRIDGE_H_
