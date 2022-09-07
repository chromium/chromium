// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_list.h"

#include <utility>

#include "base/callback.h"

namespace base {

CallbackListSubscription::CallbackListSubscription() = default;

CallbackListSubscription::CallbackListSubscription(base::OnceClosure closure)
    : closure_(std::move(closure)) {}

CallbackListSubscription::CallbackListSubscription(
    CallbackListSubscription&& subscription)
    : closure_(std::move(subscription.closure_)) {}

CallbackListSubscription& CallbackListSubscription::operator=(
    CallbackListSubscription&& subscription) {
  // Note: This still works properly for self-assignment.
  Run();
  closure_ = std::move(subscription.closure_);
  return *this;
}

CallbackListSubscription::~CallbackListSubscription() {
  Run();
}

void CallbackListSubscription::Run() {
  if (closure_)
    std::move(closure_).Run();
}

}  // namespace base
