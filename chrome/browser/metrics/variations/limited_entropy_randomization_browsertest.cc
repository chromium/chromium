// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/metrics/field_trial.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/variations/pref_names.h"
#include "components/variations/proto/layer.pb.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/variations_switches.h"
#include "components/variations/variations_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

namespace variations {
namespace {

constexpr std::string_view kGwsVisibleStudyName = "GwsVisibleStudy";
constexpr std::string_view kNonGwsVisibleStudyName = "NonGwsVisibleStudy";

// Returns a seed with the following:
// * A 100-slot limited layer with a single layer member containing all slots.
// * A GWS-visible A/B study constrained to the limited layer.
// * A non-GWS-visible A/B study constrained to the limited layer.
//
// Each study has 50% of clients in Group1 and 50% of clients in Group2. Because
// the studies have permanent consistency, have the same group ordering, and
// specify the same randomization seed, they are randomized in the same way.
// Therefore, the Group1 clients in one study are also the Group1 clients in the
// other study. Ditto for Group2.
VariationsSeed CreateTestSeedWithLimitedEntropyLayer() {
  VariationsSeed seed;

  auto* layer = seed.add_layers();
  layer->set_id(123);
  layer->set_num_slots(100);
  layer->set_entropy_mode(Layer::LIMITED);

  auto* layer_member = layer->add_members();
  layer_member->set_id(1);
  auto* slot_range = layer_member->add_slots();
  slot_range->set_start(0);
  slot_range->set_end(99);

  Study base_study;
  base_study.set_randomization_seed(500);
  base_study.set_consistency(Study::PERMANENT);
  base_study.set_activation_type(Study::ACTIVATE_ON_STARTUP);
  auto* filter = base_study.mutable_filter();
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
  auto* layer_member_reference = base_study.mutable_layer();
  layer_member_reference->set_layer_id(123);
  layer_member_reference->add_layer_member_ids(1);

  Study gws_study = base_study;
  gws_study.set_name(kGwsVisibleStudyName);
  auto* group = gws_study.add_experiment();
  group->set_name("Group1");
  group->set_probability_weight(1);
  group->set_google_web_experiment_id(44);
  group = gws_study.add_experiment();
  group->set_name("Group2");
  group->set_probability_weight(1);
  group->set_google_web_experiment_id(55);

  Study non_gws_study = base_study;
  non_gws_study.set_name(kNonGwsVisibleStudyName);
  group = non_gws_study.add_experiment();
  group->set_name("Group1");
  group->set_probability_weight(1);
  group = non_gws_study.add_experiment();
  group->set_name("Group2");
  group->set_probability_weight(1);

  *seed.add_study() = gws_study;
  *seed.add_study() = non_gws_study;

  return seed;
}

class LimitedEntropyRandomizationBrowserTest : public PlatformBrowserTest {
 public:
  LimitedEntropyRandomizationBrowserTest() = default;
  ~LimitedEntropyRandomizationBrowserTest() override = default;

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
    const base::FilePath seed_file_path =
        user_data_dir.AppendASCII("VariationsSeedV1");
    const base::FilePath local_state_path =
        user_data_dir.Append(chrome::kLocalStateFilename);

    std::string serialized_seed =
        CreateTestSeedWithLimitedEntropyLayer().SerializeAsString();
    std::string compressed_seed;
    compression::GzipCompress(serialized_seed, &compressed_seed);

    // Write the seed for the seed file experiment's treatment-group clients.
    CHECK(base::WriteFile(seed_file_path, compressed_seed));

    // Write the seed for the seed file experiment's control-group clients.
    base::Value::Dict local_state;
    local_state.SetByDottedPath(prefs::kVariationsCompressedSeed,
                                base::Base64Encode(compressed_seed));
    CHECK(JSONFileValueSerializer(local_state_path).Serialize(local_state));
    return true;
  }

  PrefService* local_state() { return g_browser_process->local_state(); }
};

// Verifies the following:
// * The client generates a limited entropy randomization source value.
// * FieldTrials are created from the limited-layer-constrained studies in the
//   seed produced by `CreateTestSeedWithLimitedEntropyLayer()`.
// * Randomization is the same for limited-layer-constrained studies that are
//   identical except for (A) unused study names (unused because the studies
//   set randomization seed) and (B) the presence or absence of Google
//   experiment IDs in their respective groups.
IN_PROC_BROWSER_TEST_F(LimitedEntropyRandomizationBrowserTest,
                       UseLimitedEntropyRandomization) {
  EXPECT_FALSE(
      local_state()
          ->FindPreference(
              metrics::prefs::kMetricsLimitedEntropyRandomizationSource)
          ->IsDefaultValue());
  EXPECT_TRUE(base::FieldTrialList::TrialExists(kGwsVisibleStudyName));
  EXPECT_TRUE(base::FieldTrialList::TrialExists(kNonGwsVisibleStudyName));

  // As a reminder, `CreateTestSeedWithLimitedEntropyLayer()` returns a seed
  // with two two-arm studies whose groups have the same names, order, and
  // weight. The expectation is that all clients in the first group of one study
  // are the same clients in the first group of the other study. Ditto for the
  // second group.
  //
  // Verify that the studies were randomized in the same way for this client by
  // checking that the client is in either the first group of each study or the
  // second.
  EXPECT_EQ(base::FieldTrialList::FindFullName(kGwsVisibleStudyName),
            base::FieldTrialList::FindFullName(kNonGwsVisibleStudyName));
}

}  // namespace
}  // namespace variations
