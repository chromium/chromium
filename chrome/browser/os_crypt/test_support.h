// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OS_CRYPT_TEST_SUPPORT_H_
#define CHROME_BROWSER_OS_CRYPT_TEST_SUPPORT_H_

#include <optional>

#include "base/functional/callback_helpers.h"
#include "chrome/install_static/install_details.h"

class ScopedLogGrabber;

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
  // then be overridden. If `use_old_elevator_interface` is true then the
  // IElevator interface will be used otherwise IElevator2 will be used.
  // Production code uses IElevator2 so IElevator compatibility is only used to
  // verify interface compatibility for tests.
  // TODO(crbug.com/383157189): Remove the test calls to IElevator once
  // IElevator2 has shipped to stable for a few milestones.
  explicit FakeInstallDetails(bool use_old_elevator_interface = false);

  FakeInstallDetails(const FakeInstallDetails&) = delete;
  FakeInstallDetails& operator=(const FakeInstallDetails&) = delete;

 private:
  install_static::InstallConstants constants_;
};

// Install the elevation service corresponding to the set of install details for
// the current process, returns a closure that will uninstall the service when
// it goes out of scope. Logs from the service will be spooled to the passed
// `log_grabber` which should outlive the lifetime of the service. If
// `fake_reencrypt` is true then the elevation service will signal that returned
// data should be re-encrypted by the client if a DecryptData call is made.
[[nodiscard]] std::optional<base::ScopedClosureRunner> InstallService(
    const ScopedLogGrabber& log_grabber,
    bool fake_reencrypt = false);

}  // namespace os_crypt

#endif  // CHROME_BROWSER_OS_CRYPT_TEST_SUPPORT_H_
