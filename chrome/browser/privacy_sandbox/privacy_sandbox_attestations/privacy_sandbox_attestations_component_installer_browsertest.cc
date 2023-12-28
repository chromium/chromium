// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/threading/thread_restrictions.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/privacy_sandbox_attestations_component_installer.h"
#include "chrome/browser/component_updater/privacy_sandbox_attestations_component_installer_test_util.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/proto/privacy_sandbox_attestations.pb.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "content/public/test/browser_test.h"

namespace privacy_sandbox {
class PrivacySandboxAttestationsBrowserTest
    : public MixinBasedInProcessBrowserTest {
 public:
  PrivacySandboxAttestationsBrowserTest() = default;

  void SetUp() override {
    MixinBasedInProcessBrowserTest::SetUp();
    ASSERT_TRUE(DeleteInstalledComponent());
  }

  void TearDown() override {
    MixinBasedInProcessBrowserTest::TearDown();
    ASSERT_TRUE(DeleteInstalledComponent());
  }

 protected:
  using Installer =
      component_updater::PrivacySandboxAttestationsComponentInstallerPolicy;

 private:
  bool DeleteInstalledComponent() {
    // Delete the privacy sandbox attestations installation directory.
    base::FilePath component_updater_dir;
    base::PathService::Get(component_updater::DIR_COMPONENT_USER,
                           &component_updater_dir);

    return base::DeletePathRecursively(
        Installer::GetInstalledDirectory(component_updater_dir));
  }

  PrivacySandboxAttestationsMixin privacy_sandbox_attestations_mixin_{
      &mixin_host_};
};

IN_PROC_BROWSER_TEST_F(
    PrivacySandboxAttestationsBrowserTest,
    CallComponentReadyWhenRegistrationFindsExistingComponent) {
  PrivacySandboxAttestationsProto proto;

  // Create an attestations file that contains the site with attestation for
  // Topics API.
  std::string site = "https://example.com";
  PrivacySandboxAttestationsProto::PrivacySandboxAttestedAPIsProto
      site_attestation;
  site_attestation.add_attested_apis(TOPICS);
  (*proto.mutable_site_attestations())[site] = site_attestation;

  base::Version version("0.0.0.1");

  ASSERT_TRUE(
      component_updater::InstallPrivacySandboxAttestationsComponentForTesting(
          proto, version));

  base::RunLoop run_loop;
  PrivacySandboxAttestations::GetInstance()
      ->SetLoadAttestationsDoneCallbackForTesting(run_loop.QuitClosure());

  // Register the privacy sandbox attestations component, which should detect
  // the existing attestations file on disk and start parsing.
  RegisterPrivacySandboxAttestationsComponent(
      g_browser_process->component_updater());

  // Wait until the attestations parsing is done.
  run_loop.Run();

  ASSERT_TRUE(PrivacySandboxAttestations::GetInstance()
                  ->GetVersionForTesting()
                  .IsValid());
  EXPECT_EQ(PrivacySandboxAttestations::GetInstance()->GetVersionForTesting(),
            version);
  EXPECT_TRUE(PrivacySandboxSettingsImpl::IsAllowed(
      PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
          net::SchemefulSite(GURL(site)),
          PrivacySandboxAttestationsGatedAPI::kTopics)));
}

// When parsing fails or crashes, a sentinel file is left in the installation
// directory. This file prevents further parsing attempts.
IN_PROC_BROWSER_TEST_F(PrivacySandboxAttestationsBrowserTest,
                       SentinelFilePreventsSubsequentParsings) {
  std::string site = "https://example.com";

  // Allow blocking for file IO.
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::Version version("0.0.0.1");

  ASSERT_TRUE(
      component_updater::InstallPrivacySandboxAttestationsComponentForTesting(
          "invalid proto", version));

  base::FilePath install_dir =
      component_updater::GetPrivacySandboxAtteststionsComponentInstallDir(
          version);
  ASSERT_FALSE(base::PathExists(install_dir.Append(kSentinelFileName)));

  base::RunLoop parsing_invalid_attestations;
  PrivacySandboxAttestations::GetInstance()
      ->SetLoadAttestationsDoneCallbackForTesting(
          parsing_invalid_attestations.QuitClosure());

  // Register the privacy sandbox attestations component, which should detect
  // the existing attestations file on disk and start parsing.
  RegisterPrivacySandboxAttestationsComponent(
      g_browser_process->component_updater());

  // Wait until the attestations parsing is done.
  parsing_invalid_attestations.Run();

  // Verify the parsing is failed because of the invalid attestations file.
  EXPECT_FALSE(PrivacySandboxAttestations::GetInstance()
                   ->GetVersionForTesting()
                   .IsValid());
  EXPECT_EQ(PrivacySandboxSettingsImpl::Status::kAttestationsFileCorrupt,
            PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
                net::SchemefulSite(GURL(site)),
                PrivacySandboxAttestationsGatedAPI::kTopics));
  ASSERT_TRUE(base::PathExists(install_dir.Append(kSentinelFileName)));

  // Overwrite the attestations file with a serialized proto.
  PrivacySandboxAttestationsProto proto;
  PrivacySandboxAttestationsProto::PrivacySandboxAttestedAPIsProto
      site_attestation;
  site_attestation.add_attested_apis(TOPICS);
  (*proto.mutable_site_attestations())[site] = site_attestation;

  // Serialize proto to string.
  std::string serialized_proto;
  proto.SerializeToString(&serialized_proto);

  ASSERT_TRUE(component_updater::WritePrivacySandboxAttestationsFileForTesting(
      install_dir, serialized_proto));

  // Try to load the attestations file again. This time its content is valid.
  // However, the sentinel file from previous run should prevent it from being
  // parsed.
  base::RunLoop parsing_valid_attestations_with_sentinel;
  PrivacySandboxAttestations::GetInstance()
      ->SetLoadAttestationsDoneCallbackForTesting(
          parsing_valid_attestations_with_sentinel.QuitClosure());

  // Register the privacy sandbox attestations component, which should detect
  // the existing attestations file on disk and start parsing.
  RegisterPrivacySandboxAttestationsComponent(
      g_browser_process->component_updater());

  parsing_valid_attestations_with_sentinel.Run();

  // Sentinel file should prevent parsing. The query result should stay the same
  // as before.
  EXPECT_FALSE(PrivacySandboxAttestations::GetInstance()
                   ->GetVersionForTesting()
                   .IsValid());
  EXPECT_EQ(PrivacySandboxSettingsImpl::Status::kAttestationsFileCorrupt,
            PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
                net::SchemefulSite(GURL(site)),
                PrivacySandboxAttestationsGatedAPI::kTopics));
  EXPECT_TRUE(base::PathExists(install_dir.Append(kSentinelFileName)));

  base::Version newer_version("0.0.0.2");

  // Install newer version attestations.
  ASSERT_TRUE(
      component_updater::InstallPrivacySandboxAttestationsComponentForTesting(
          proto, newer_version));

  // The sentinel file still exists in the old version installation directory.
  EXPECT_TRUE(base::PathExists(install_dir.Append(kSentinelFileName)));

  // Try to load the attestations file again.
  base::RunLoop parsing_new_version;
  PrivacySandboxAttestations::GetInstance()
      ->SetLoadAttestationsDoneCallbackForTesting(
          parsing_new_version.QuitClosure());

  // Register the privacy sandbox attestations component, which should detect
  // the existing attestations file on disk and start parsing.
  RegisterPrivacySandboxAttestationsComponent(
      g_browser_process->component_updater());

  parsing_new_version.Run();

  // Expect the loading to be successful. The sentinel file in the old version
  // directory should not prevent the parsing in the new version directory.
  EXPECT_TRUE(PrivacySandboxAttestations::GetInstance()
                  ->GetVersionForTesting()
                  .IsValid());
  EXPECT_EQ(PrivacySandboxSettingsImpl::Status::kAllowed,
            PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
                net::SchemefulSite(GURL(site)),
                PrivacySandboxAttestationsGatedAPI::kTopics));
  EXPECT_EQ(PrivacySandboxAttestations::GetInstance()->GetVersionForTesting(),
            base::Version("0.0.0.2"));

  // Component updater should delete the old version directory, see
  // `ComponentInstaller::DeleteUnselectedComponentVersions`.
  EXPECT_FALSE(base::PathExists(install_dir));
}

}  // namespace privacy_sandbox
