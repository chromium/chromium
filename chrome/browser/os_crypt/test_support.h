// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OS_CRYPT_TEST_SUPPORT_H_
#define CHROME_BROWSER_OS_CRYPT_TEST_SUPPORT_H_

#include "chrome/install_static/install_details.h"

namespace os_crypt {

namespace switches {

// Encrypt the data in input-filename and place the result in output-filename.
inline constexpr char kAppBoundTestModeEncrypt[] = "app-bound-test-encrypt";
// Decrypt the data in input-filename and place the result in output-filename.
inline constexpr char kAppBoundTestModeDecrypt[] = "app-bound-test-decrypt";
// The input file for encryption or decryption.
inline constexpr char kAppBoundTestInputFilename[] =
    "app-bound-test-input-filename";
// The output file for encryption or decryption.
inline constexpr char kAppBoundTestOutputFilename[] =
    "app-bound-test-output-filename";
// The protection level to use.
inline constexpr char kAppBoundTestProtectionLevel[] =
    "app-bound-test-protection-level";
// The output file for any re-encryption ciphertext that came back from the
// service. Optional.
inline constexpr char kAppBoundTestReencryptionOutputFilename[] =
    "app-bound-test-reencryption-output-filename";

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

}  // namespace os_crypt

#endif  // CHROME_BROWSER_OS_CRYPT_TEST_SUPPORT_H_
