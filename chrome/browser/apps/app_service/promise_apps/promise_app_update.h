// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_UPDATE_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_UPDATE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_apps.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
namespace apps {

class PackageId;

class PromiseAppUpdate {
 public:
  static void Merge(PromiseApp* state, const PromiseApp* delta);

  // At most one of |state| or |delta| may be nullptr.
  PromiseAppUpdate(const PromiseApp* state, const PromiseApp* delta);

  PromiseAppUpdate(const PromiseAppUpdate&) = delete;
  PromiseAppUpdate& operator=(const PromiseAppUpdate&) = delete;

  const PackageId& PackageId() const;

  absl::optional<float> Progress() const;
  bool ProgressChanged() const;

  PromiseStatus Status() const;
  bool StatusChanged() const;

 private:
  raw_ptr<const PromiseApp> state_ = nullptr;
  raw_ptr<const PromiseApp> delta_ = nullptr;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_UPDATE_H_
