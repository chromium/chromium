// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_CONNECTION_OBSERVER_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_CONNECTION_OBSERVER_H_

#include "base/observer_list_types.h"
namespace policy {

// Observer that will be informed when a CRD connection is established, or
// fails to be established.
class CrdConnectionObserver : public base::CheckedObserver {
 public:
  CrdConnectionObserver() = default;
  ~CrdConnectionObserver() override = default;

  // Called when the host user rejects the CRD connection.
  virtual void OnConnectionRejected() = 0;

  // Called when the CRD connection was successfully opened.
  virtual void OnConnectionEstablished() = 0;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_CONNECTION_OBSERVER_H_
