// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_UPDATE_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_UPDATE_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
namespace apps {

class PackageId;

class PromiseAppUpdate {
 public:
  static void Merge(PromiseApp* state, const PromiseApp* delta);

  // At most one of |state| or |delta| may be nullptr.
  PromiseAppUpdate(const PromiseApp* state, const PromiseApp* delta);

  PromiseAppUpdate(const PromiseAppUpdate&) = delete;
  PromiseAppUpdate& operator=(const PromiseAppUpdate&) = delete;

  bool operator==(const PromiseAppUpdate&) const;

  const PackageId& PackageId() const;

  // Indicates the app name for the package. If app name is not known or still
  // loading, return std::nullopt.
  std::optional<std::string> Name() const;

  bool NameChanged() const;

  // Indicates the current installation progress percentage. If the package is
  // not actively downloading/ installing then this method returns
  // std::nullopt.
  std::optional<float> Progress() const;

  bool ProgressChanged() const;

  // Indicates the status of the installation.
  PromiseStatus Status() const;

  bool StatusChanged() const;

  // The ID of the app installed from the package.
  // Empty unless promise app installed successfully.
  std::string InstalledAppId() const;

  bool InstalledAppIdChanged() const;

  // Indicates whether the promise app should show in the Launcher/ Shelf.
  bool ShouldShow() const;

  bool ShouldShowChanged() const;

 private:
  raw_ptr<const PromiseApp, DanglingUntriaged> state_ = nullptr;
  raw_ptr<const PromiseApp, DanglingUntriaged> delta_ = nullptr;
};

std::ostream& operator<<(std::ostream& out, const PromiseAppUpdate& update);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_UPDATE_H_
