// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/tpcd_metadata_component_installer.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/notreached.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/tpcd/metadata/parser_test_helper.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {
namespace {
using ::testing::_;

const base::FilePath::CharType kComponentFileName[] =
    FILE_PATH_LITERAL("metadata.pb");

const char* kTpcdMetadataInstallationResult =
    "Navigation.TpcdMitigations.MetadataInstallationResult";
}  // namespace

class TpcdMetadataComponentInstallerTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<bool> {
 public:
  TpcdMetadataComponentInstallerTest() {
    CHECK(install_dir_.CreateUniqueTempDir());
    CHECK(install_dir_.IsValid());
    path_ = install_dir().Append(kComponentFileName);
    CHECK(!path_.empty());
    if (GetParam()) {
      scoped_list_.InitAndEnableFeature(net::features::kTpcdMetadataGrants);
    } else {
      scoped_list_.InitAndDisableFeature(net::features::kTpcdMetadataGrants);
    }
  }

  ~TpcdMetadataComponentInstallerTest() override = default;

 protected:
  const base::FilePath& install_dir() { return install_dir_.GetPath(); }

  const base::FilePath path() { return path_; }

  void ExecFakeComponentInstallation(base::StringPiece contents) {
    CHECK(base::WriteFile(path(), contents));
    CHECK(base::PathExists(path()));
  }

  content::BrowserTaskEnvironment& task_env() { return task_env_; }

  auto* policy() { return policy_.get(); }

 private:
  base::ScopedTempDir install_dir_;
  base::FilePath path_;
  content::BrowserTaskEnvironment task_env_;
  base::test::ScopedFeatureList scoped_list_;
  std::unique_ptr<component_updater::ComponentInstallerPolicy> policy_ =
      std::make_unique<TpcdMetadataComponentInstaller>(base::DoNothing());
};

TEST_P(TpcdMetadataComponentInstallerTest, ComponentRegistered) {
  auto service =
      std::make_unique<component_updater::MockComponentUpdateService>();

  EXPECT_CALL(*service, RegisterComponent(_)).Times(1);
  RegisterTpcdMetadataComponent(service.get());

  task_env().RunUntilIdle();
}

TEST_P(TpcdMetadataComponentInstallerTest,
       VerifyInstallation_InvalidInstallDir) {
  base::HistogramTester histogram_tester;

  EXPECT_FALSE(policy()->VerifyInstallation(
      base::Value::Dict(), install_dir().Append(FILE_PATH_LITERAL("x"))));

  histogram_tester.ExpectBucketCount(
      kTpcdMetadataInstallationResult,
      TpcdMetadataInstallationResult::kMissingMetadataFile, 1);
}

TEST_P(TpcdMetadataComponentInstallerTest,
       VerifyInstallation_RejectsMissingFile) {
  base::HistogramTester histogram_tester;

  EXPECT_FALSE(
      policy()->VerifyInstallation(base::Value::Dict(), install_dir()));

  histogram_tester.ExpectBucketCount(
      kTpcdMetadataInstallationResult,
      TpcdMetadataInstallationResult::kMissingMetadataFile, 1);
}

TEST_P(TpcdMetadataComponentInstallerTest,
       VerifyInstallation_RejectsNotProtoFile) {
  ExecFakeComponentInstallation("clearly not a proto");

  base::HistogramTester histogram_tester;
  EXPECT_FALSE(
      policy()->VerifyInstallation(base::Value::Dict(), install_dir()));
  histogram_tester.ExpectBucketCount(
      kTpcdMetadataInstallationResult,
      TpcdMetadataInstallationResult::kParsingToProtoFailed, 1);
}

TEST_P(TpcdMetadataComponentInstallerTest,
       FeatureEnabled_ComponentReady_ErroneousPrimarySpec) {
  if (!GetParam()) {
    GTEST_SKIP_("Reason: Test parameter instance N/A");
  }

  const std::string primary_pattern_spec = "[*]bar.com";
  const std::string secondary_pattern_spec = "[*.]foo.com";

  std::vector<tpcd::metadata::MetadataPair> metadata_pairs;
  metadata_pairs.emplace_back(primary_pattern_spec, secondary_pattern_spec);
  tpcd::metadata::Metadata metadata =
      tpcd::metadata::MakeMetadataProtoFromVectorOfPair(metadata_pairs);
  ASSERT_EQ(metadata.metadata_entries_size(), 1);

  ExecFakeComponentInstallation(metadata.SerializeAsString());

  base::HistogramTester histogram_tester;
  ASSERT_FALSE(
      policy()->VerifyInstallation(base::Value::Dict(), install_dir()));
  histogram_tester.ExpectBucketCount(
      kTpcdMetadataInstallationResult,
      TpcdMetadataInstallationResult::kErroneousSpec, 1);
}

TEST_P(TpcdMetadataComponentInstallerTest,
       FeatureEnabled_ComponentReady_ErroneousSecondarySpec) {
  if (!GetParam()) {
    GTEST_SKIP_("Reason: Test parameter instance N/A");
  }

  const std::string primary_pattern_spec = "[*.]bar.com";
  const std::string secondary_pattern_spec = "[*]foo.com";

  std::vector<tpcd::metadata::MetadataPair> metadata_pairs;
  metadata_pairs.emplace_back(primary_pattern_spec, secondary_pattern_spec);
  tpcd::metadata::Metadata metadata =
      tpcd::metadata::MakeMetadataProtoFromVectorOfPair(metadata_pairs);
  ASSERT_EQ(metadata.metadata_entries_size(), 1);

  ExecFakeComponentInstallation(metadata.SerializeAsString());

  base::HistogramTester histogram_tester;
  ASSERT_FALSE(
      policy()->VerifyInstallation(base::Value::Dict(), install_dir()));
  histogram_tester.ExpectBucketCount(
      kTpcdMetadataInstallationResult,
      TpcdMetadataInstallationResult::kErroneousSpec, 1);
}

TEST_P(TpcdMetadataComponentInstallerTest,
       FeatureEnabled_ComponentReady_FiresCallback) {
  if (!GetParam()) {
    GTEST_SKIP_("Reason: Test parameter instance N/A");
  }

  const std::string primary_pattern_spec = "[*.]bar.com";
  const std::string secondary_pattern_spec = "[*.]foo.com";

  std::vector<tpcd::metadata::MetadataPair> metadata_pairs;
  metadata_pairs.emplace_back(primary_pattern_spec, secondary_pattern_spec);
  tpcd::metadata::Metadata metadata =
      tpcd::metadata::MakeMetadataProtoFromVectorOfPair(metadata_pairs);
  ASSERT_EQ(metadata.metadata_entries_size(), 1);

  ExecFakeComponentInstallation(metadata.SerializeAsString());

  base::RunLoop run_loop;

  std::unique_ptr<component_updater::ComponentInstallerPolicy> policy =
      std::make_unique<TpcdMetadataComponentInstaller>(
          base::BindLambdaForTesting([&](std::string raw_metadata) {
            EXPECT_EQ(raw_metadata, metadata.SerializeAsString());
            run_loop.Quit();
          }));

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(policy->VerifyInstallation(base::Value::Dict(), install_dir()));
  histogram_tester.ExpectBucketCount(
      kTpcdMetadataInstallationResult,
      TpcdMetadataInstallationResult::kSuccessful, 1);

  policy->ComponentReady(base::Version(), install_dir(), base::Value::Dict());

  run_loop.Run();
}

TEST_P(TpcdMetadataComponentInstallerTest,
       FeatureDisabled_ComponentReady_DoesNotFireCallback) {
  if (GetParam()) {
    GTEST_SKIP_("Reason: Test parameter instance N/A");
  }

  const std::string primary_pattern_spec = "[*.]bar.com";
  const std::string secondary_pattern_spec = "[*.]foo.com";

  std::vector<tpcd::metadata::MetadataPair> metadata_pairs;
  metadata_pairs.emplace_back(primary_pattern_spec, secondary_pattern_spec);
  tpcd::metadata::Metadata metadata =
      tpcd::metadata::MakeMetadataProtoFromVectorOfPair(metadata_pairs);
  ASSERT_EQ(metadata.metadata_entries_size(), 1);

  ExecFakeComponentInstallation(metadata.SerializeAsString());

  base::RunLoop run_loop;

  std::unique_ptr<component_updater::ComponentInstallerPolicy> policy =
      std::make_unique<TpcdMetadataComponentInstaller>(
          base::BindLambdaForTesting(
              [&](std::string raw_metadata) { NOTREACHED_NORETURN(); }));

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(policy->VerifyInstallation(base::Value::Dict(), install_dir()));
  histogram_tester.ExpectBucketCount(
      kTpcdMetadataInstallationResult,
      TpcdMetadataInstallationResult::kSuccessful, 1);

  policy->ComponentReady(base::Version(), install_dir(), base::Value::Dict());

  run_loop.RunUntilIdle();
}

INSTANTIATE_TEST_SUITE_P(
    /* no label */,
    TpcdMetadataComponentInstallerTest,
    ::testing::Bool());

}  // namespace component_updater
