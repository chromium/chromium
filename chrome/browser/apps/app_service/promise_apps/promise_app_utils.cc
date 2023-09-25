// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/promise_apps/promise_app_utils.h"

namespace apps {

bool IsPromiseAppCompleted(apps::PromiseStatus status) {
  return status == PromiseStatus::kSuccess ||
         status == PromiseStatus::kCancelled;
}

}  // namespace apps
