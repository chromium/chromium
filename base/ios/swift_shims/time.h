// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_IOS_SWIFT_SHIMS_TIME_H_
#define BASE_IOS_SWIFT_SHIMS_TIME_H_

#include <ctime>
#include <memory>

namespace base {
namespace swift {

class TimeDeltaImpl;

class TimeDelta {
 public:
  static TimeDelta FromTimeSpec(timespec ts);
  int64_t InSeconds() const;
  int64_t InMilliseconds() const;
  int64_t InMicroseconds() const;
  int64_t InNanoseconds() const;

  ~TimeDelta();
  TimeDelta(const TimeDelta& other);
  TimeDelta(TimeDelta&& other);

 private:
  TimeDelta(std::unique_ptr<TimeDeltaImpl>&& impl);
  std::unique_ptr<TimeDeltaImpl> _impl;
};

}  // namespace swift
}  // namespace base

#endif  // BASE_IOS_SWIFT_SHIMS_TIME_H_
