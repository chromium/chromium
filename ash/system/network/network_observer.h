// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_OBSERVER_H_
#define ASH_SYSTEM_NETWORK_NETWORK_OBSERVER_H_

namespace ash {

class NetworkObserver {
 public:
  virtual ~NetworkObserver() {}

  // Called to request toggling Wi-Fi enable/disable, e.g. from an accelerator.
  // NOTE: Toggling is asynchronous and subsequent calls to query the current
  // state may return the old value.
  virtual void RequestToggleWifi() = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_OBSERVER_H_
