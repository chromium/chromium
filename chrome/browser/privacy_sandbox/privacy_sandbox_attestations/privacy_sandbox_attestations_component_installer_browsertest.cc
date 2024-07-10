// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/privacy_sandbox_attestations_component_installer.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_path_override.h"
#include "base/test/with_feature_override.h"
#include "base/threading/thread_restrictions.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/privacy_sandbox_attestations_component_installer_test_util.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations_histograms.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/proto/privacy_sandbox_attestations.pb.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "content/public/test/browser_test.h"

namespace privacy_sandbox {
class PrivacySandboxAttestationsBrowserTest
    : public MixinBasedInProcessBrowserTest {
 public:

  void SetUp() override {
    MixinBasedInProcessBrowserTest::SetUp();
    ASSERT_TRUE(DeleteInstalledComponent());
  }

  void TearDown() override {
    MixinBasedInProcessBrowserTest::TearDown();
    ASSERT_TRUE(DeleteInstalledComponent());
  }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
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

  base::HistogramTester histogram_tester_;
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

  // There is a pre-installed attestations component. Choose a version number
  // that is sure to be higher than the pre-installed one. This makes sure that
  // the component installer will choose the attestations file in the user-wide
  // component directory.
  base::Version version("12345.0.0.0");

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

  histogram_tester().ExpectTotalCount(kAttestationsFileSource, 1);
  histogram_tester().ExpectBucketCount(kAttestationsFileSource,
                                       FileSource::kDownloaded, 1);
}

// Depending on whether the component installer has checked the attestations
// file or not, the attestation check status should be recorded in different
// histogram buckets.
IN_PROC_BROWSER_TEST_F(PrivacySandboxAttestationsBrowserTest,
                       DifferentHistogramAfterAttestationsFileCheck) {
  // Allow blocking for file IO.
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Override the pre-install component directory and its alternative directory
  // so that the component update will not find the pre-installed attestations
  // file.
  base::ScopedPathOverride preinstalled_dir_override(
      component_updater::DIR_COMPONENT_PREINSTALLED);
  base::ScopedPathOverride preinstalled_alt_dir_override(
      component_updater::DIR_COMPONENT_PREINSTALLED_ALT);

  std::string site = "https://example.com";
  EXPECT_FALSE(PrivacySandboxSettingsImpl::IsAllowed(
      PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
          net::SchemefulSite(GURL(site)),
          PrivacySandboxAttestationsGatedAPI::kTopics)));

  // The attestation component has not yet checked the attestations file.
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 1);
  histogram_tester().ExpectBucketCount(
      kAttestationStatusUMA,
      PrivacySandboxSettingsImpl::Status::kAttestationsFileNotYetChecked, 1);

  base::RunLoop run_loop;
  PrivacySandboxAttestations::GetInstance()
      ->SetComponentRegistrationCallbackForTesting(run_loop.QuitClosure());

  // Register the privacy sandbox attestations component.
  RegisterPrivacySandboxAttestationsComponent(
      g_browser_process->component_updater());

  // Wait until the point where attestations component has checked the
  // attestations file but could not find it on disk.
  run_loop.Run();

  // Check attestation again.
  EXPECT_FALSE(PrivacySandboxSettingsImpl::IsAllowed(
      PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
          net::SchemefulSite(GURL(site)),
          PrivacySandboxAttestationsGatedAPI::kTopics)));

  // It should record in a different histogram bucket because the file check has
  // completed but no file was found.
  histogram_tester().ExpectTotalCount(kAttestationStatusUMA, 2);
  histogram_tester().ExpectBucketCount(
      kAttestationStatusUMA,
      PrivacySandboxSettingsImpl::Status::kAttestationsFileNotPresent, 1);
}

class PrivacySandboxAttestationPreInstallBrowserTest
    : public PrivacySandboxAttestationsBrowserTest,
      public base::test::WithFeatureOverride {
 public:
  PrivacySandboxAttestationPreInstallBrowserTest()
      : base::test::WithFeatureOverride(
            kPrivacySandboxAttestationsLoadPreInstalledComponent) {}
};

// If there is no attestation list in user directory and feature
// "PrivacySandboxAttestationsLoadPreInstalledComponent" is enabled, the
// pre-installed version should be used.
IN_PROC_BROWSER_TEST_P(PrivacySandboxAttestationPreInstallBrowserTest,
                       PreinstalledAttestationListPresent) {
  // Allow blocking for file IO.
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Override the user-wide component directory to make sure there is no
  // downloaded attestation list.
  base::ScopedPathOverride user_dir_override(
      component_updater::DIR_COMPONENT_USER);

  base::RunLoop run_loop;
  PrivacySandboxAttestations::GetInstance()
      ->SetLoadAttestationsDoneCallbackForTesting(run_loop.QuitClosure());

  // Register the privacy sandbox attestations component, which should parse
  // the pre-installed attestations file on disk if feature
  // "PrivacySandboxAttestationsLoadPreInstalledComponent" is enabled.
  RegisterPrivacySandboxAttestationsComponent(
      g_browser_process->component_updater());

  if (IsParamFeatureEnabled()) {
    // Wait until the attestations parsing is done.
    run_loop.Run();

    EXPECT_TRUE(PrivacySandboxAttestations::GetInstance()
                    ->GetVersionForTesting()
                    .IsValid());

    // Make an attestation check to verify the data point is recorded to the
    // correct histogram bucket.
    PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
        net::SchemefulSite(GURL("https://example.com")),
        PrivacySandboxAttestationsGatedAPI::kTopics);
    histogram_tester().ExpectTotalCount(kAttestationsFileSource, 1);
    histogram_tester().ExpectBucketCount(kAttestationsFileSource,
                                         FileSource::kPreInstalled, 1);
  } else {
    // If feature off, the attestation component should not parse the
    // pre-installed file.
    run_loop.RunUntilIdle();

    ASSERT_FALSE(PrivacySandboxAttestations::GetInstance()
                     ->GetVersionForTesting()
                     .IsValid());
    PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
        net::SchemefulSite(GURL("https://example.com")),
        PrivacySandboxAttestationsGatedAPI::kTopics);
    histogram_tester().ExpectTotalCount(kAttestationsFileSource, 0);
  }
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    PrivacySandboxAttestationPreInstallBrowserTest);

}  // namespace privacy_sandbox
