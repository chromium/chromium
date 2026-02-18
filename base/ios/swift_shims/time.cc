// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ios/swift_shims/time.h"

#include "base/time/time.h"

namespace base {
namespace swift {

class TimeDeltaImpl : public base::TimeDelta {};

TimeDelta::TimeDelta(const TimeDelta& other)
    : _impl(std::make_unique<TimeDeltaImpl>(*other._impl)) {}

TimeDelta::TimeDelta(TimeDelta&& other) = default;

TimeDelta::TimeDelta(std::unique_ptr<TimeDeltaImpl>&& impl)
    : _impl(std::move(impl)) {}

TimeDelta::~TimeDelta() = default;

TimeDelta TimeDelta::FromTimeSpec(timespec ts) {
  return TimeDelta(
      std::make_unique<TimeDeltaImpl>(base::TimeDelta::FromTimeSpec(ts)));
}

int64_t TimeDelta::InSeconds() const {
  return _impl->InSeconds();
}

int64_t TimeDelta::InMilliseconds() const {
  return _impl->InMilliseconds();
}

int64_t TimeDelta::InMicroseconds() const {
  return _impl->InMicroseconds();
}

int64_t TimeDelta::InNanoseconds() const {
  return _impl->InNanoseconds();
}

}  // namespace swift
}  // namespace base
