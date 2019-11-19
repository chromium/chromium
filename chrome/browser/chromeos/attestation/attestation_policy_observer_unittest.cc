// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/attestation/attestation_policy_observer.h"
#include "chrome/browser/chromeos/attestation/mock_machine_certificate_uploader.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chromeos/settings/cros_settings_names.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::StrictMock;

namespace chromeos {
namespace attestation {

class AttestationPolicyObserverTest : public ::testing::Test {
 public:
  AttestationPolicyObserverTest() {
    settings_helper_.ReplaceDeviceSettingsProviderWithStub();
  }
  ~AttestationPolicyObserverTest() override {}

 protected:
  void Run() {
    AttestationPolicyObserver observer(&certificate_uploader_);
    base::RunLoop().RunUntilIdle();
  }

  content::BrowserTaskEnvironment task_environment_;
  ScopedCrosSettingsTestHelper settings_helper_;
  StrictMock<MockMachineCertificateUploader> certificate_uploader_;
};

TEST_F(AttestationPolicyObserverTest, FeatureEnabled) {
  EXPECT_CALL(certificate_uploader_, UploadCertificateIfNeeded(_));
  settings_helper_.SetBoolean(kDeviceAttestationEnabled, true);
  Run();
}

TEST_F(AttestationPolicyObserverTest, FeatureDisabled) {
  settings_helper_.SetBoolean(kDeviceAttestationEnabled, false);
  Run();
}

}  // namespace attestation
}  // namespace chromeos
