// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/certificate_provider/test_certificate_provider_extension_mixin.h"

#include <memory>

#include "chrome/browser/certificate_provider/test_certificate_provider_extension.h"
#include "chrome/browser/policy/extension_force_install_mixin.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

TestCertificateProviderExtensionMixin::TestCertificateProviderExtensionMixin(
    InProcessBrowserTestMixinHost* host,
    ExtensionForceInstallMixin* extension_force_install_mixin)
    : InProcessBrowserTestMixin(host),
      extension_force_install_mixin_(extension_force_install_mixin) {}

TestCertificateProviderExtensionMixin::
    ~TestCertificateProviderExtensionMixin() = default;

void TestCertificateProviderExtensionMixin::TearDownOnMainThread() {
  certificate_provider_extension_.reset();
}

void TestCertificateProviderExtensionMixin::ForceInstall(
    Profile* profile,
    bool wait_on_extension_loaded,
    bool immediately_provide_certificates) {
  DCHECK(wait_on_extension_loaded || !immediately_provide_certificates)
      << "When wait_on_extension_loaded is unset, "
         "immediately_provide_certificates must also be unset!";
  DCHECK(extension_force_install_mixin_->initialized())
      << "ExtensionForceInstallMixin must be initialized before calling "
         "ForceInstall!";
  DCHECK(!certificate_provider_extension_) << "ForceInstall already called!";
  certificate_provider_extension_ =
      std::make_unique<TestCertificateProviderExtension>(profile);
  ASSERT_TRUE(extension_force_install_mixin_->ForceInstallFromSourceDir(
      TestCertificateProviderExtension::GetExtensionSourcePath(),
      TestCertificateProviderExtension::GetExtensionPemPath(),
      wait_on_extension_loaded
          ? ExtensionForceInstallMixin::WaitMode::kBackgroundPageFirstLoad
          : ExtensionForceInstallMixin::WaitMode::kPrefSet));
  if (wait_on_extension_loaded && immediately_provide_certificates)
    certificate_provider_extension_->TriggerSetCertificates();
}

}  // namespace ash
