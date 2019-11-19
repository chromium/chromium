// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_PACKAGE_OPERATION_STATUS_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_PACKAGE_OPERATION_STATUS_H_

#include <ostream>

namespace crostini {

// Status of an operation in CrostiniPackageService &
// CrostiniPackageNotification.
enum class PackageOperationStatus {
  QUEUED,
  SUCCEEDED,
  FAILED,
  RUNNING,
  WAITING_FOR_APP_REGISTRY_UPDATE
};

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_PACKAGE_OPERATION_STATUS_H_
