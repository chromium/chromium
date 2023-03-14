// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/promise_apps/promise_apps.h"

namespace apps {

PromiseAppPtr PromiseApp::Clone() const {
  auto promise_app = std::make_unique<PromiseApp>(package_id);
  if (progress.has_value()) {
    promise_app->progress = progress;
  }
  promise_app->status = status;
  return promise_app;
}

}  // namespace apps
