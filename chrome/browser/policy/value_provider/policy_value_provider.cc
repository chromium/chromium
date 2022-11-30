// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/value_provider/policy_value_provider.h"

#include <utility>

#include "base/observer_list.h"

namespace policy {

PolicyValueProvider::PolicyValueProvider() = default;

PolicyValueProvider::~PolicyValueProvider() = default;

void PolicyValueProvider::Refresh() {}

void PolicyValueProvider::NotifyValueChange() {
  for (auto& observer : observers_)
    observer.OnPolicyValueChanged();
}

void PolicyValueProvider::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PolicyValueProvider::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace policy
