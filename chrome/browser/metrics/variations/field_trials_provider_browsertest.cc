// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <string_view>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/metrics/field_trial.h"
#include "base/path_service.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/pref_service.h"
#include "components/variations/hashing.h"
#include "components/variations/pref_names.h"
#include "components/variations/proto/layer.pb.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/variations_switches.h"
#include "components/variations/variations_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/system_profile.pb.h"
#include "third_party/zlib/google/compression_utils.h"

namespace variations {
namespace {

constexpr char kSeedVersion[] = "20260120-123456.789000";
constexpr std::string_view kStudyName = "AbcXyzStudy";
constexpr std::string_view kLimitedLayerStudyName = "LimitedLayerStudy";
constexpr std::string_view kLowLayerStudyName = "LowLayerStudy";

// Returns a 100-slot layer with the given mode and a single member containing
// all slots. The given ID is used for both the layer ID and the layer member
// ID.
Layer CreateSingleMemberLayer(int id, Layer::EntropyMode mode) {
  Layer layer;
  layer.set_id(id);
  layer.set_num_slots(100);
  layer.set_entropy_mode(mode);
  auto* layer_member = layer.add_members();
  layer_member->set_id(id);
  auto* slot_range = layer_member->add_slots();
  slot_range->set_start(0);
  slot_range->set_end(99);
  return layer;
}

// Returns a two-arm, permanent-consistency, ACTIVATE_ON_STARTUP study named
// `kStudyName` targeting desktop and Android clients on all channels.
Study CreateLayerlessTwoArmStudy() {
  Study study;
  study.set_name(kStudyName);
  study.set_consistency(Study::PERMANENT);
  study.set_activation_type(Study::ACTIVATE_ON_STARTUP);

  auto* filter = study.mutable_filter();
  filter->add_channel(Study::CANARY);
  filter->add_channel(Study::BETA);
  filter->add_channel(Study::DEV);
  filter->add_channel(Study::STABLE);
  filter->add_channel(Study::UNKNOWN);
  filter->add_platform(Study::PLATFORM_WINDOWS);
  filter->add_platform(Study::PLATFORM_MAC);
  filter->add_platform(Study::PLATFORM_LINUX);
  filter->add_platform(Study::PLATFORM_CHROMEOS);
  filter->add_platform(Study::PLATFORM_ANDROID);

  auto* group = study.add_experiment();
  group->set_name("A");
  group->set_probability_weight(1);
  group = study.add_experiment();
  group->set_name("B");
  group->set_probability_weight(1);
  return study;
}

// Returns a seed with the following:
// * a version,
// * a limited layer, and
// * a study that runs in all of the limited layer's slots.
VariationsSeed CreateTestSeedWithLimitedLayer() {
  VariationsSeed seed;
  seed.set_version(kSeedVersion);
  *seed.add_layers() = CreateSingleMemberLayer(/*id=*/1, Layer::LIMITED);

  Study study = CreateLayerlessTwoArmStudy();
  auto* layer_member_reference = study.mutable_layer();
  layer_member_reference->set_layer_id(1);
  layer_member_reference->add_layer_member_ids(1);
  *seed.add_study() = study;

  return seed;
}

// Returns a seed with the following:
// * a version,
// * a LOW layer, and
// * a study that runs in all of the LOW layer's slots.
VariationsSeed CreateTestSeedWithLowLayer() {
  VariationsSeed seed;
  seed.set_version(kSeedVersion);
  *seed.add_layers() = CreateSingleMemberLayer(/*id=*/2, Layer::LOW);

  Study study = CreateLayerlessTwoArmStudy();
  auto* layer_member_reference = study.mutable_layer();
  layer_member_reference->set_layer_id(2);
  layer_member_reference->add_layer_member_ids(2);
  *seed.add_study() = study;

  return seed;
}

// Returns a seed with the following:
// * A version.
// * A LOW layer with a single member containing all 100 slots.
// * A two-arm study named `kLowLayerStudyName` that is constrained to the
//   LOW layer. In addition, this study has one zero-weight group with an
//   experiment ID.
// * A limited layer with a single member containing all 100 slots.
// * A two-arm study (with one group having an experiment ID) constrained to the
//   limited layer.
//
// Note that the study in the LOW layer does not consume entropy because no
// clients are randomized into the zero-weight group and other two groups do not
// have experiment IDs.
VariationsSeed CreateTestSeedWithValidLowAndLimitedLayers() {
  VariationsSeed seed;
  seed.set_version(kSeedVersion);
  *seed.add_layers() = CreateSingleMemberLayer(/*id=*/1, Layer::LIMITED);
  *seed.add_layers() = CreateSingleMemberLayer(/*id=*/2, Layer::LOW);

  Study limited_layer_study = CreateLayerlessTwoArmStudy();
  limited_layer_study.set_name(kLimitedLayerStudyName);
  auto* limited_layer_reference = limited_layer_study.mutable_layer();
  limited_layer_reference->set_layer_id(1);
  limited_layer_reference->add_layer_member_ids(1);
  auto* group = limited_layer_study.mutable_experiment(0);
  group->set_google_web_experiment_id(3333);
  *seed.add_study() = limited_layer_study;

  Study low_layer_study = CreateLayerlessTwoArmStudy();
  low_layer_study.set_name(kLowLayerStudyName);
  auto* low_layer_reference = low_layer_study.mutable_layer();
  low_layer_reference->set_layer_id(2);
  low_layer_reference->add_layer_member_ids(2);
  group = low_layer_study.add_experiment();
  group->set_name("GroupWithExperimentId");
  group->set_probability_weight(0);
  group->set_google_web_experiment_id(2222);
  *seed.add_study() = low_layer_study;
  return seed;
}

// Returns a seed with a version and with studies constrained to LOW and limited
// layers. The LOW layer's study has an experiment ID; i.e., it consumes
// entropy. Such seeds should be dropped.
VariationsSeed CreateTestSeedWithMisconfiguredEntropy() {
  VariationsSeed seed;
  seed.set_version(kSeedVersion);
  *seed.add_layers() = CreateSingleMemberLayer(/*id=*/1, Layer::LIMITED);
  *seed.add_layers() = CreateSingleMemberLayer(/*id=*/2, Layer::LOW);

  Study limited_layer_study = CreateLayerlessTwoArmStudy();
  auto* limited_layer_reference = limited_layer_study.mutable_layer();
  limited_layer_reference->set_layer_id(1);
  limited_layer_reference->add_layer_member_ids(1);
  *seed.add_study() = limited_layer_study;

  Study low_layer_study = CreateLayerlessTwoArmStudy();
  low_layer_study.set_name("LowLayerStudy");
  low_layer_study.set_activation_type(Study::ACTIVATE_ON_STARTUP);
  auto* group = low_layer_study.add_experiment();
  group->set_name("GroupWithExperimentId");
  group->set_probability_weight(1);
  group->set_google_web_experiment_id(2222);
  auto* low_layer_reference = low_layer_study.mutable_layer();
  low_layer_reference->set_layer_id(2);
  low_layer_reference->add_layer_member_ids(2);
  *seed.add_study() = low_layer_study;
  return seed;
}

struct FieldTrialsProviderTestParams {
  std::string test_name;
  VariationsSeed seed;
  std::string seed_version;
  bool seed_has_active_limited_layer = false;
  std::set<uint32_t> study_hashes;
};

class FieldTrialsProviderBrowserTest
    : public PlatformBrowserTest,
      public ::testing::WithParamInterface<FieldTrialsProviderTestParams> {
 public:
  FieldTrialsProviderBrowserTest() = default;
  ~FieldTrialsProviderBrowserTest() override = default;

 protected:
  // BrowserTestBase:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kAcceptEmptySeedSignatureForTesting);
    DisableTestingConfig();
  }

  // PlatformBrowserTest:
  bool SetUpUserDataDirectory() override {
    const base::FilePath user_data_dir =
        base::PathService::CheckedGet(chrome::DIR_USER_DATA);
    const base::FilePath local_state_path =
        user_data_dir.Append(chrome::kLocalStateFilename);
    const base::FilePath seed_file_path =
        user_data_dir.AppendASCII("VariationsSeedV2");
    const std::string serialized_seed = GetParam().seed.SerializeAsString();

    // Write the seed for the seed file experiment's control-group clients.
    std::string compressed_seed;
    compression::GzipCompress(serialized_seed, &compressed_seed);
    base::Value::Dict local_state;
    local_state.SetByDottedPath(prefs::kVariationsCompressedSeed,
                                base::Base64Encode(compressed_seed));
    CHECK(JSONFileValueSerializer(local_state_path).Serialize(local_state));

    // Write the seed for the seed file experiment's treatment-group clients.
    StoredSeedInfo seed_info;
    seed_info.set_data(serialized_seed);
    CHECK(base::WriteFile(seed_file_path,
                          SeedReaderWriter::CompressForSeedFileForTesting(
                              seed_info.SerializeAsString())));
    return true;
  }

  PrefService* local_state() { return g_browser_process->local_state(); }
};

class NoSeedFieldTrialsProviderBrowserTest
    : public FieldTrialsProviderBrowserTest {};

IN_PROC_BROWSER_TEST_P(FieldTrialsProviderBrowserTest, CheckSystemProfile) {
  metrics::SystemProfileProto system_profile;
  g_browser_process->metrics_service()
      ->GetDelegatingProviderForTesting()
      ->ProvideSystemProfileMetricsWithLogCreationTime(base::TimeTicks::Now(),
                                                       &system_profile);

  const FieldTrialsProviderTestParams params = GetParam();

  // Verify the SystemProfileProto.seed_has_active_limited_layer result.
  EXPECT_TRUE(system_profile.has_seed_has_active_limited_layer());
  EXPECT_EQ(system_profile.seed_has_active_limited_layer(),
            params.seed_has_active_limited_layer);

  // Verify the SystemProfileProto.variations_seed_version result.
  EXPECT_TRUE(system_profile.has_variations_seed_version());
  EXPECT_EQ(system_profile.variations_seed_version(), params.seed_version);

  // Verify the SystemProfileProto.field_trial result.
  std::set<uint32_t> trial_ids;
  for (const auto& field_trial : system_profile.field_trial()) {
    trial_ids.insert(static_cast<uint32_t>(field_trial.name_id()));
  }
  EXPECT_THAT(trial_ids, ::testing::IsSupersetOf(params.study_hashes));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    FieldTrialsProviderBrowserTest,
    ::testing::Values(
        FieldTrialsProviderTestParams{
            .test_name = "SeedWithLimitedLayerApplied",
            .seed = CreateTestSeedWithLimitedLayer(),
            .seed_version = kSeedVersion,
            .seed_has_active_limited_layer = true,
            .study_hashes = {HashName(kStudyName)}},
        FieldTrialsProviderTestParams{.test_name = "SeedWithLowLayerApplied",
                                      .seed = CreateTestSeedWithLowLayer(),
                                      .seed_version = kSeedVersion,
                                      .seed_has_active_limited_layer = false,
                                      .study_hashes = {HashName(kStudyName)}},
        FieldTrialsProviderTestParams{
            .test_name = "SeedWithLowAndLimitedLayersApplied",
            .seed = CreateTestSeedWithValidLowAndLimitedLayers(),
            .seed_version = kSeedVersion,
            .seed_has_active_limited_layer = true,
            .study_hashes = {HashName(kLowLayerStudyName),
                             HashName(kLimitedLayerStudyName)}}));

IN_PROC_BROWSER_TEST_P(NoSeedFieldTrialsProviderBrowserTest,
                       CheckSystemProfile) {
  metrics::SystemProfileProto system_profile;
  g_browser_process->metrics_service()
      ->GetDelegatingProviderForTesting()
      ->ProvideSystemProfileMetricsWithLogCreationTime(base::TimeTicks::Now(),
                                                       &system_profile);

  const FieldTrialsProviderTestParams params = GetParam();

  // Verify that the following SystemProfileProto fields are unset:
  // seed_has_active_limited_layer and variations_seed_version.
  EXPECT_FALSE(system_profile.has_seed_has_active_limited_layer());
  EXPECT_FALSE(system_profile.has_variations_seed_version());

  // Verify that the study named `kStudyName` is not in
  // SystemProfileProto.field_trial.
  uint32_t trial_id = HashName(kStudyName);
  EXPECT_TRUE(std::none_of(
      system_profile.field_trial().cbegin(),
      system_profile.field_trial().cend(),
      [trial_id](const metrics::SystemProfileProto::FieldTrial& ft) {
        return ft.name_id() == trial_id;
      }));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    NoSeedFieldTrialsProviderBrowserTest,
    ::testing::Values(FieldTrialsProviderTestParams{
        .test_name = "SeedWithMisconfiguredEntropyDropped",
        .seed = CreateTestSeedWithMisconfiguredEntropy()}));

}  // namespace
}  // namespace variations
