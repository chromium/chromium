// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_extra_parts_nacl_deprecation.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/common/ppapi_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {

class ChromeBrowserMainExtraPartsNaclDeprecationTestUnmanagedDeviceDefault
    : public InProcessBrowserTest {};

IN_PROC_BROWSER_TEST_F(
    ChromeBrowserMainExtraPartsNaclDeprecationTestUnmanagedDeviceDefault,
    NaClEnabledByDefault) {
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(IsNaclAllowed());
#else
  EXPECT_FALSE(IsNaclAllowed());
#endif  // BUILDFLAG(IS_CHROMEOS)
}

class
    ChromeBrowserMainExtraPartsNaclDeprecationTestUnmanagedDeviceFeatureDisabled
    : public InProcessBrowserTest {
 public:
  ChromeBrowserMainExtraPartsNaclDeprecationTestUnmanagedDeviceFeatureDisabled() {
    feature_list_.InitAndDisableFeature(kNaclAllow);
  }
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    ChromeBrowserMainExtraPartsNaclDeprecationTestUnmanagedDeviceFeatureDisabled,
    NaClEnabled) {
#if BUILDFLAG(IS_CHROMEOS)
  // On unmanaged devices we consider NaCl enabled until we will implement a
  // device owner setting to emulate the functionality of the device policy.
  // TODO(crbug.com/377443982): Modify after device owner setting is
  // implemented.
  EXPECT_TRUE(IsNaclAllowed());
#else
  EXPECT_FALSE(IsNaclAllowed());
#endif  // BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(IsNaclAllowed());
}

class
    ChromeBrowserMainExtraPartsNaclDeprecationTestUnmanagedDeviceFeatureEnabled
    : public InProcessBrowserTest {
 public:
  ChromeBrowserMainExtraPartsNaclDeprecationTestUnmanagedDeviceFeatureEnabled() {
    feature_list_.InitAndEnableFeature(kNaclAllow);
  }
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    ChromeBrowserMainExtraPartsNaclDeprecationTestUnmanagedDeviceFeatureEnabled,
    NaClEnabled) {
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(IsNaclAllowed());
#else
  EXPECT_FALSE(IsNaclAllowed());
#endif  // BUILDFLAG(IS_CHROMEOS)
}

#if BUILDFLAG(IS_CHROMEOS)

class ChromeBrowserMainExtraPartsNaclDeprecationTestManagedDeviceDefault
    : public policy::PolicyTest {
 public:
  ChromeBrowserMainExtraPartsNaclDeprecationTestManagedDeviceDefault()
      : install_attributes_(ash::StubInstallAttributes::CreateCloudManaged(
            /*domain=*/"test-domain",
            /*device_id=*/"FAKE_DEVICE_ID")) {}

 private:
  ash::ScopedStubInstallAttributes install_attributes_;
};

IN_PROC_BROWSER_TEST_F(
    ChromeBrowserMainExtraPartsNaclDeprecationTestManagedDeviceDefault,
    NaClDisabled) {
  EXPECT_FALSE(IsNaclAllowed());
}

class
    ChromeBrowserMainExtraPartsNaclDeprecationTestManagedDeviceEnabledByFieldTrial
    : public ChromeBrowserMainExtraPartsNaclDeprecationTestManagedDeviceDefault {
 public:
  ChromeBrowserMainExtraPartsNaclDeprecationTestManagedDeviceEnabledByFieldTrial() {
    feature_list_.InitAndEnableFeature(kNaclAllow);
  }
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    ChromeBrowserMainExtraPartsNaclDeprecationTestManagedDeviceEnabledByFieldTrial,
    NaClEnabled) {
  EXPECT_TRUE(IsNaclAllowed());
}

class ChromeBrowserMainExtraPartsNaclDeprecationTestWithPolicy
    : public ChromeBrowserMainExtraPartsNaclDeprecationTestManagedDeviceDefault {
 protected:
  explicit ChromeBrowserMainExtraPartsNaclDeprecationTestWithPolicy(
      bool forceEnable) {
    SetNativeClientForceAllowed(forceEnable);
  }

 private:
  void SetNativeClientForceAllowed(bool value) {
    policy::PolicyMap policies;
    policies.Set(policy::key::kDeviceNativeClientForceAllowed,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                 policy::POLICY_SOURCE_CLOUD, base::Value(value),
                 /*external_data_fetcher=*/nullptr);
    UpdateProviderPolicy(policies);
  }
};

class ChromeBrowserMainExtraPartsNaclDeprecationTestManagedForceAllowedTrue
    : public ChromeBrowserMainExtraPartsNaclDeprecationTestWithPolicy {
 public:
  ChromeBrowserMainExtraPartsNaclDeprecationTestManagedForceAllowedTrue()
      : ChromeBrowserMainExtraPartsNaclDeprecationTestWithPolicy(
            /*forceEnable*/ true) {}
};

// The policy only takes effect after a restart.
IN_PROC_BROWSER_TEST_F(
    ChromeBrowserMainExtraPartsNaclDeprecationTestManagedForceAllowedTrue,
    PRE_PolicyOverridesFieldTrialValue) {}

IN_PROC_BROWSER_TEST_F(
    ChromeBrowserMainExtraPartsNaclDeprecationTestManagedForceAllowedTrue,
    PolicyOverridesFieldTrialValue) {
  EXPECT_TRUE(IsNaclAllowed());
}

class ChromeBrowserMainExtraPartsNaclDeprecationTestManagedForceAllowedFalse
    : public ChromeBrowserMainExtraPartsNaclDeprecationTestWithPolicy {
 public:
  ChromeBrowserMainExtraPartsNaclDeprecationTestManagedForceAllowedFalse()
      : ChromeBrowserMainExtraPartsNaclDeprecationTestWithPolicy(
            /*forceEnable*/ false) {}
};

// The policy only takes effect after a restart.
IN_PROC_BROWSER_TEST_F(
    ChromeBrowserMainExtraPartsNaclDeprecationTestManagedForceAllowedFalse,
    PRE_PolicyUsesFieldTrialValue) {}

IN_PROC_BROWSER_TEST_F(
    ChromeBrowserMainExtraPartsNaclDeprecationTestManagedForceAllowedFalse,
    PolicyUsesFieldTrialValue) {
  EXPECT_FALSE(IsNaclAllowed());
}

#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace
