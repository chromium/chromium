// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/privacy_sandbox_attestations_component_installer.h"

#include <optional>
#include <string>
#include <utility>

#include "base/android/apk_assets.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/privacy_sandbox_attestations_component_installer_test_util.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/preload/android_apk_assets.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations_histograms.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/proto/privacy_sandbox_attestations.pb.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_test_util.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {

class PrivacySandboxAttestationsAPKAssetAndroidBrowserTest
    : public AndroidBrowserTest {
 public:
  PrivacySandboxAttestationsAPKAssetAndroidBrowserTest() = default;
  ~PrivacySandboxAttestationsAPKAssetAndroidBrowserTest() override = default;
};

// Check that attestations list exists in Android APK assets. The content of
// the pre-installed attestations list is the same as those delivered via
// component updater. For tests covering the parsing of attestations list, see:
// components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations_parser_unittest.cc.
IN_PROC_BROWSER_TEST_F(PrivacySandboxAttestationsAPKAssetAndroidBrowserTest,
                       APKAssetBundledAttestationsList) {
  // base::MemoryMappedFile requires blocking.
  base::ScopedAllowBlockingForTesting scoped_allow_blocking;

  // Opening the attestations list from APK assets.
  base::MemoryMappedFile::Region region =
      base::MemoryMappedFile::Region::kWholeFile;
  int descriptor = base::android::OpenApkAsset(
      std::string(kAttestationsListAssetPath), &region);
  ASSERT_NE(descriptor, -1);
}

class LoadFromAPKAssetAndroidBrowserTest
    : public PrivacySandboxAttestationsAPKAssetAndroidBrowserTest {
 public:
  LoadFromAPKAssetAndroidBrowserTest() = default;
  ~LoadFromAPKAssetAndroidBrowserTest() override = default;

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      kPrivacySandboxAttestationsLoadFromAPKAsset};

  // Override the user component directories that may have the downloaded
  // attestation file.
  base::ScopedPathOverride user_dir_override_{
      component_updater::DIR_COMPONENT_USER};

  base::HistogramTester histogram_tester_;
};

// If there is available attestations component in user component directory, the
// pre-installed one in APK assets is not loaded.
IN_PROC_BROWSER_TEST_F(
    LoadFromAPKAssetAndroidBrowserTest,
    DoNotLoadAttestationsFromAPKAssetIfUserComponentAvailable) {
  PrivacySandboxAttestationsProto proto;

  // Create an attestations file that contains the site with attestation for
  // Topics API.
  PrivacySandboxAttestationsProto::PrivacySandboxAttestedAPIsProto
      site_attestation;
  site_attestation.add_attested_apis(TOPICS);
  std::string site{"https://example.com"};
  (*proto.mutable_site_attestations())[site] = site_attestation;

  // Install an attestations component in user component directory.
  base::Version version("12345.0.0.0");
  ASSERT_TRUE(
      component_updater::InstallPrivacySandboxAttestationsComponentForTesting(
          proto, version, /*is_pre_installed=*/false));

  base::RunLoop run_loop;
  PrivacySandboxAttestations::GetInstance()
      ->SetLoadAttestationsDoneCallbackForTesting(run_loop.QuitClosure());

  // Register the privacy sandbox attestations component, which should detect
  // the existing attestations file on disk and start parsing.
  RegisterPrivacySandboxAttestationsComponent(
      g_browser_process->component_updater());

  // Wait until the attestations parsing is done.
  run_loop.Run();

  // Load from attestations component in user component directory succeeds.
  ASSERT_TRUE(PrivacySandboxAttestations::GetInstance()
                  ->GetVersionForTesting()
                  .IsValid());
  EXPECT_FALSE(PrivacySandboxAttestations::GetInstance()->is_pre_installed());
  EXPECT_EQ(PrivacySandboxAttestations::GetInstance()->GetVersionForTesting(),
            version);

  // The parsing status histogram should record a successful parsing.
  histogram_tester().ExpectTotalCount(kAttestationsFileParsingStatusUMA, 1);
  histogram_tester().ExpectBucketCount(kAttestationsFileParsingStatusUMA,
                                       ParsingStatus::kSuccess, 1);

  // The attestations component in APK assets should not be loaded.
  histogram_tester().ExpectTotalCount(kAttestationsLoadAPKAssetStatusUMA, 0);

  // Make an attestation check to verify the file source is not pre-installed.
  EXPECT_TRUE(
      privacy_sandbox_test_util::PrivacySandboxSettingsTestPeer::IsAllowed(
          PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
              net::SchemefulSite(GURL(site)),
              PrivacySandboxAttestationsGatedAPI::kTopics)));

  histogram_tester().ExpectTotalCount(kAttestationsFileSource, 1);
  histogram_tester().ExpectBucketCount(kAttestationsFileSource,
                                       FileSource::kDownloaded, 1);
}

// Verify that the pre-installed attestations component in Android APK assets
// is loaded to populate the in-memory attestations map when there is no
// attestations list available in user component directory.
IN_PROC_BROWSER_TEST_F(LoadFromAPKAssetAndroidBrowserTest,
                       LoadAttestationsFromAPKAsset) {
  base::RunLoop run_loop;
  PrivacySandboxAttestations::GetInstance()
      ->SetLoadAttestationsDoneCallbackForTesting(run_loop.QuitClosure());

  // Register the privacy sandbox attestations component. There is no available
  // attestations list in user component directory. The pre-installed
  // attestations component in Android APK assets is loaded.
  RegisterPrivacySandboxAttestationsComponent(
      g_browser_process->component_updater());

  // Wait until the attestations parsing is done.
  run_loop.Run();

  EXPECT_TRUE(PrivacySandboxAttestations::GetInstance()
                  ->GetVersionForTesting()
                  .IsValid());
  ASSERT_EQ(PrivacySandboxAttestations::GetInstance()->GetVersionForTesting(),
            base::Version(kAttestationsListAssetVersion));
  EXPECT_TRUE(PrivacySandboxAttestations::GetInstance()->is_pre_installed());

  // The parsing status histogram should record a successful parsing.
  histogram_tester().ExpectTotalCount(kAttestationsFileParsingStatusUMA, 1);
  histogram_tester().ExpectBucketCount(kAttestationsFileParsingStatusUMA,
                                       ParsingStatus::kSuccess, 1);

  // The loading from APK asset histogram should also record a successful load.
  histogram_tester().ExpectTotalCount(kAttestationsLoadAPKAssetStatusUMA, 1);
  histogram_tester().ExpectBucketCount(kAttestationsLoadAPKAssetStatusUMA,
                                       LoadAPKAssetStatus::kSuccess, 1);

  // Make an attestation check to verify the file source is pre-installed.
  PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
      net::SchemefulSite(GURL("https://example.com")),
      PrivacySandboxAttestationsGatedAPI::kTopics);
  histogram_tester().ExpectTotalCount(kAttestationsFileSource, 1);
  histogram_tester().ExpectBucketCount(kAttestationsFileSource,
                                       FileSource::kPreInstalled, 1);
}

}  // namespace privacy_sandbox
