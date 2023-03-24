// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/promise_apps/promise_apps.h"

#include <iostream>

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

std::ostream& operator<<(std::ostream& out, const PromiseApp& promise_app) {
  out << "Package_id: " << promise_app.package_id.ToString() << std::endl;
  if (promise_app.progress.has_value()) {
    out << "- Progress: " << promise_app.progress.value() << std::endl;
  } else {
    out << "- Progress: N/A" << std::endl;
  }
  out << "- Status: " << EnumToString(promise_app.status) << std::endl;
  return out;
}

}  // namespace apps
