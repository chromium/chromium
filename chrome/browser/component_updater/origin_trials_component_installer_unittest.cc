// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/chrome_origin_trials_component_installer.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/embedder_support/origin_trials/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

// Mirror the constants used in the component installer. Do not share the
// constants, as want to catch inadvertent changes in the tests. The keys will
// will be generated server-side, so any changes need to be intentional and
// coordinated.
constexpr char kManifestOriginTrialsKey[] = "origin-trials";
constexpr char kTestUpdateVersion[] = "1.0";
constexpr char kExistingPublicKey[] = "existing public key";

}  // namespace

namespace component_updater {

class OriginTrialsComponentInstallerTest : public PlatformTest {
 public:
  OriginTrialsComponentInstallerTest()
      : testing_local_state_(TestingBrowserProcess::GetGlobal()) {}

  OriginTrialsComponentInstallerTest(
      const OriginTrialsComponentInstallerTest&) = delete;
  OriginTrialsComponentInstallerTest& operator=(
      const OriginTrialsComponentInstallerTest&) = delete;

  void SetUp() override {
    PlatformTest::SetUp();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    policy_ = std::make_unique<ChromeOriginTrialsComponentInstallerPolicy>();
  }

  void LoadUpdates(base::Value::Dict manifest) {
    if (manifest.empty()) {
      manifest.Set(kManifestOriginTrialsKey, base::Value());
    }
    ASSERT_TRUE(policy_->VerifyInstallation(manifest, temp_dir_.GetPath()));
    const base::Version expected_version(kTestUpdateVersion);
    policy_->ComponentReady(expected_version, temp_dir_.GetPath(),
                            std::move(manifest));
  }

  PrefService* local_state() { return g_browser_process->local_state(); }

 protected:
  base::ScopedTempDir temp_dir_;
  ScopedTestingLocalState testing_local_state_;
  std::unique_ptr<ComponentInstallerPolicy> policy_;
};

TEST_F(OriginTrialsComponentInstallerTest,
       PublicKeyResetToDefaultWhenOverrideMissing) {
  local_state()->SetString(embedder_support::prefs::kOriginTrialPublicKey,
                           kExistingPublicKey);
  ASSERT_EQ(
      kExistingPublicKey,
      local_state()->GetString(embedder_support::prefs::kOriginTrialPublicKey));

  // Load with empty section in manifest
  LoadUpdates(base::Value::Dict());

  EXPECT_FALSE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialPublicKey));
}

}  // namespace component_updater
