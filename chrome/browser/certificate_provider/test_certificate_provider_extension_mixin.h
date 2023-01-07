// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CERTIFICATE_PROVIDER_TEST_CERTIFICATE_PROVIDER_EXTENSION_MIXIN_H_
#define CHROME_BROWSER_CERTIFICATE_PROVIDER_TEST_CERTIFICATE_PROVIDER_EXTENSION_MIXIN_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"

class ExtensionForceInstallMixin;
class Profile;

namespace ash {

class TestCertificateProviderExtension;

// Mixin to automatically set up a TestCertificateProviderExtension.
class TestCertificateProviderExtensionMixin final
    : public InProcessBrowserTestMixin {
 public:
  explicit TestCertificateProviderExtensionMixin(
      InProcessBrowserTestMixinHost* host,
      ExtensionForceInstallMixin* extension_force_install_mixin);
  TestCertificateProviderExtensionMixin(
      const TestCertificateProviderExtensionMixin&) = delete;
  TestCertificateProviderExtensionMixin& operator=(
      const TestCertificateProviderExtensionMixin&) = delete;
  ~TestCertificateProviderExtensionMixin() override;

  // InProcessBrowserTestMixin:
  void TearDownOnMainThread() override;

  // Sets up the extension.
  // Must be called after the `extension_force_install_mixin_` is initialized.
  // Should only be called once.
  // This asserts that the extension is installed correctly. To stop execution
  // of a test on a failed assertion use the ASSERT_NO_FATAL_FAILURE macro.
  // `wait_on_extension_loaded`: Waits until the extension is ready to provide
  // certificates.
  // `immediately_provide_certificates`: Causes the extension to provide
  // certificates once loaded. Applies only when `wait_on_extension_loaded` is
  // set.
  void ForceInstall(Profile* profile,
                    bool wait_on_extension_loaded = true,
                    bool immediately_provide_certificates = true);

  TestCertificateProviderExtension* extension() {
    return certificate_provider_extension_.get();
  }

  const TestCertificateProviderExtension* extension() const {
    return certificate_provider_extension_.get();
  }

 private:
  const raw_ptr<ExtensionForceInstallMixin> extension_force_install_mixin_;
  std::unique_ptr<TestCertificateProviderExtension>
      certificate_provider_extension_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_CERTIFICATE_PROVIDER_TEST_CERTIFICATE_PROVIDER_EXTENSION_MIXIN_H_
