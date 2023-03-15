// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/promise_apps/promise_apps.h"
#include "components/services/app_service/public/cpp/macros.h"

namespace apps {

APP_ENUM_TO_STRING(PromiseStatus, kUnknown, kPending, kDownloading, kInstalling)

PromiseAppPtr PromiseApp::Clone() const {
  auto promise_app = std::make_unique<PromiseApp>(package_id);
  if (progress.has_value()) {
    promise_app->progress = progress;
  }
  promise_app->status = status;
  return promise_app;
}

}  // namespace apps
