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
#include "base/test/run_until.h"
#include "base/test/scoped_path_override.h"
#include "base/test/with_feature_override.h"
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
class PrivacySandboxAttestationsBrowserTestBase
    : public MixinBasedInProcessBrowserTest {
 public:
  PrivacySandboxAttestationsBrowserTestBase() = default;

  ~PrivacySandboxAttestationsBrowserTestBase() override = default;

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

 private:
  PrivacySandboxAttestationsMixin privacy_sandbox_attestations_mixin_{
      &mixin_host_};

  // Override the user component directories that may have the downloaded
  // attestation file.
  base::ScopedPathOverride user_dir_override_{
      component_updater::DIR_COMPONENT_USER};

  base::HistogramTester histogram_tester_;
};

class PrivacySandboxAttestationsBrowserTest
    : public PrivacySandboxAttestationsBrowserTestBase {
 public:
  PrivacySandboxAttestationsBrowserTest() = default;

  ~PrivacySandboxAttestationsBrowserTest() override = default;

 private:
  // Override the pre-install component directories that may have the
  // pre-installed attestation file.
  base::ScopedPathOverride preinstalled_dir_override_{
      component_updater::DIR_COMPONENT_PREINSTALLED};
  base::ScopedPathOverride preinstalled_alt_dir_override_{
      component_updater::DIR_COMPONENT_PREINSTALLED_ALT};
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

  // There is an existing pre-installed attestations component that is shipped
  // with Chromium. Choose a version number that is sure to be higher. This
  // makes sure that the component installer will ignore the shipped
  // pre-installed attestations component.
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
    : public PrivacySandboxAttestationsBrowserTestBase,
      public base::test::WithFeatureOverride {
 public:
  PrivacySandboxAttestationPreInstallBrowserTest()
      : base::test::WithFeatureOverride(
            kPrivacySandboxAttestationsLoadPreInstalledComponent) {}

  ~PrivacySandboxAttestationPreInstallBrowserTest() override = default;
};

// If there is no attestation list in user directory and feature
// "PrivacySandboxAttestationsLoadPreInstalledComponent" is enabled, the
// pre-installed version should be used. This test verifies there is a
// pre-installed attestation list shipped with Chromium.
IN_PROC_BROWSER_TEST_P(PrivacySandboxAttestationPreInstallBrowserTest,
                       PreinstalledAttestationListPresent) {
  base::RunLoop run_loop;
  PrivacySandboxAttestations::GetInstance()
      ->SetLoadAttestationsDoneCallbackForTesting(run_loop.QuitClosure());

  // Register the privacy sandbox attestations component, which should parse
  // the pre-installed attestations file on disk if feature
  // "PrivacySandboxAttestationsLoadPreInstalledComponent" is enabled. This
  // pre-installed file is shipped with Chromium.
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
    // pre-installed file. Since there is no downloaded attestation file, the
    // attestations component ends up with no attestation map.
    ASSERT_TRUE(base::test::RunUntil([]() {
      return PrivacySandboxAttestations::GetInstance()
          ->attestations_file_checked();
    }));

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
