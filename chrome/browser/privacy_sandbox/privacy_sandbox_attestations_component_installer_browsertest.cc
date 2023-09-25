// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/privacy_sandbox_attestations_component_installer.h"
#include "chrome/browser/component_updater/privacy_sandbox_attestations_component_installer_test_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/proto/privacy_sandbox_attestations.pb.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/scoped_privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "content/public/test/browser_test.h"

namespace privacy_sandbox {
class PrivacySandboxAttestationsBrowserTest : public InProcessBrowserTest {
 public:
  PrivacySandboxAttestationsBrowserTest() = default;

  void SetUpOnMainThread() override {
    // `PrivacySandboxAttestations` has a member of type
    // `scoped_refptr<base::SequencedTaskRunner>`, its initialization must be
    // done after a browser process is created.
    scoped_attestations_ = std::make_unique<ScopedPrivacySandboxAttestations>(
        PrivacySandboxAttestations::CreateForTesting());
  }

 protected:
  using Installer =
      component_updater::PrivacySandboxAttestationsComponentInstallerPolicy;

 private:
  std::unique_ptr<ScopedPrivacySandboxAttestations> scoped_attestations_;
  base::test::ScopedFeatureList attestations_feature_;
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

  // Serialize to string.
  std::string serialized_proto;
  proto.SerializeToString(&serialized_proto);

  // Allow blocking for file IO.
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Get component updater installation directory by searching the two possible
  // locations.
  base::FilePath component_updater_dir;
  if (!base::PathService::Get(component_updater::DIR_COMPONENT_PREINSTALLED,
                              &component_updater_dir)) {
    base::PathService::Get(component_updater::DIR_COMPONENT_PREINSTALLED_ALT,
                           &component_updater_dir);
  }

  ASSERT_TRUE(!component_updater_dir.empty());

  // Write the serialized proto to the attestation list file.
  base::FilePath install_dir =
      Installer::GetInstalledDirectory(component_updater_dir);
  EXPECT_TRUE(base::CreateDirectory(install_dir));
  ASSERT_TRUE(component_updater::WritePrivacySandboxAttestationsFileForTesting(
      install_dir, serialized_proto));

  // Write a manifest file. This is needed for component updater to detect any
  // existing component on disk.
  EXPECT_TRUE(
      base::WriteFile(install_dir.Append(FILE_PATH_LITERAL("manifest.json")),
                      R"({
                          "manifest_version": 1,
                          "name": "Privacy Sandbox Attestations",
                          "version": "0.0.0.1"
                        })"));

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
            base::Version("0.0.0.1"));
  EXPECT_TRUE(PrivacySandboxSettingsImpl::IsAllowed(
      PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
          net::SchemefulSite(GURL(site)),
          PrivacySandboxAttestationsGatedAPI::kTopics)));
}

}  // namespace privacy_sandbox
