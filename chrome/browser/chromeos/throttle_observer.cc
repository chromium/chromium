// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/throttle_observer.h"

namespace chromeos {
namespace {

std::string LevelToString(ThrottleObserver::PriorityLevel level) {
  switch (level) {
    case ThrottleObserver::PriorityLevel::LOW:
      return "PriorityLevel::LOW";
    case ThrottleObserver::PriorityLevel::NORMAL:
      return "PriorityLevel::NORMAL";
    case ThrottleObserver::PriorityLevel::IMPORTANT:
      return "PriorityLevel::IMPORTANT";
    case ThrottleObserver::PriorityLevel::CRITICAL:
      return "PriorityLevel::CRITICAL";
    case ThrottleObserver::PriorityLevel::UNKNOWN:
      return "PriorityLevel::UNKNOWN";
  }
}

}  // namespace

ThrottleObserver::ThrottleObserver(ThrottleObserver::PriorityLevel level,
                                   const std::string& name)
    : level_(level), name_(name) {}

ThrottleObserver::~ThrottleObserver() = default;

void ThrottleObserver::StartObserving(
    content::BrowserContext* context,
    const ObserverStateChangedCallback& callback) {
  DCHECK(!callback_);
  callback_ = callback;
  context_ = context;
}

void ThrottleObserver::StopObserving() {
  callback_.Reset();
  context_ = nullptr;
}

void ThrottleObserver::SetActive(bool active) {
  if (active_ == active)
    return;
  active_ = active;
  if (callback_)
    callback_.Run();
}

std::string ThrottleObserver::GetDebugDescription() const {
  return ("ThrottleObserver(" + name() + ", " + LevelToString(level()) + ", " +
          (active() ? "active" : "inactive") + ")");
}

}  // namespace chromeos
