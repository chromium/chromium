// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/fake_password_manager_lifecycle_helper.h"

#include "base/logging.h"

namespace password_manager {

FakePasswordManagerLifecycleHelper::FakePasswordManagerLifecycleHelper() =
    default;

FakePasswordManagerLifecycleHelper::~FakePasswordManagerLifecycleHelper() {
  DCHECK(!foregrounding_callback_) << "Did not call UnregisterObserver!";
}

void FakePasswordManagerLifecycleHelper::RegisterObserver(
    base::RepeatingClosure foregrounding_callback) {
  foregrounding_callback_ = std::move(foregrounding_callback);
}

void FakePasswordManagerLifecycleHelper::UnregisterObserver() {
  foregrounding_callback_.Reset();
}

void FakePasswordManagerLifecycleHelper::OnForegroundSessionStart() {
  foregrounding_callback_.Run();
}

}  // namespace password_manager
