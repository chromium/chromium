// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OS_CRYPT_TEST_SUPPORT_H_
#define CHROME_BROWSER_OS_CRYPT_TEST_SUPPORT_H_

#include <optional>

#include "base/functional/callback_helpers.h"
#include "chrome/install_static/install_details.h"

namespace os_crypt {

namespace switches {

extern const char kAppBoundTestModeEncrypt[];
extern const char kAppBoundTestModeDecrypt[];
extern const char kAppBoundTestInputFilename[];
extern const char kAppBoundTestOutputFilename[];

}  // namespace switches

// This class allows system-level tests to be carried out that do not interfere
// with an existing system-level install.
class FakeInstallDetails : public install_static::PrimaryInstallDetails {
 public:
  // Copy template from first mode from install modes. Some of the values will
  // then be overridden.
  FakeInstallDetails();

  FakeInstallDetails(const FakeInstallDetails&) = delete;
  FakeInstallDetails& operator=(const FakeInstallDetails&) = delete;

 private:
  install_static::InstallConstants constants_;
};

// Install the elevation service corresponding to the set of install details for
// the current process, returns a closure that will uninstall the service when
// it goes out of scope.
[[nodiscard]] std::optional<base::ScopedClosureRunner> InstallService();

}  // namespace os_crypt

#endif  // CHROME_BROWSER_OS_CRYPT_TEST_SUPPORT_H_
