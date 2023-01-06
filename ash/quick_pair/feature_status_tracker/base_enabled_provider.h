// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_BASE_ENABLED_PROVIDER_H_
#define ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_BASE_ENABLED_PROVIDER_H_

#include "base/functional/callback.h"

namespace ash {
namespace quick_pair {

// Common Base class to expose whether or not a feature is enabled.
// Provides a |is_enabled()| method and a callback pattern to observe changes.
//
// Subclasses can focus on implementing the logic to determine the state of
// |is_enabled()|, and this base class provides common functionality for
// querying and observing that state.
//
// Example subclass: BluetoothEnabledProvider - Changes |is_enabled()| based on
// the Bluetooth state on the device.
class BaseEnabledProvider {
 public:
  BaseEnabledProvider();
  BaseEnabledProvider(const BaseEnabledProvider&) = delete;
  BaseEnabledProvider& operator=(const BaseEnabledProvider&) = delete;
  virtual ~BaseEnabledProvider();

  virtual bool is_enabled();
  virtual void SetCallback(base::RepeatingCallback<void(bool)> callback);

 protected:
  void SetEnabledAndInvokeCallback(bool new_value);

 private:
  bool is_enabled_ = false;
  base::RepeatingCallback<void(bool)> callback_;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_BASE_ENABLED_PROVIDER_H_
