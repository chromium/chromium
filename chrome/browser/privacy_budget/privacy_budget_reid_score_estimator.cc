// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/privacy_budget_reid_score_estimator.h"

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/rand_util.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/privacy_budget/privacy_budget_prefs.h"
#include "chrome/common/privacy_budget/field_trial_param_conversions.h"
#include "chrome/common/privacy_budget/types.h"
#include "components/prefs/pref_service.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metrics.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_sample_collector.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"

namespace {
void ReportHashForReidScore(blink::IdentifiableSurface surface,
                            uint64_t reid_hash) {
  ukm::SourceId ukm_source_id = ukm::NoURLSourceId();
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  blink::IdentifiabilityMetricBuilder(ukm_source_id)
      .Add(surface, reid_hash)
      .Record(ukm_recorder);
  blink::IdentifiabilitySampleCollector::Get()->FlushSource(ukm_recorder,
                                                            ukm_source_id);
}
}  // namespace

PrivacyBudgetReidScoreEstimator::ReidBlockStorage::ReidBlockStorage(
    const IdentifiableSurfaceList& surface_list,
    uint64_t salt_range,
    int number_of_bits,
    double noise_probability)
    : salt_range_(salt_range),
      number_of_bits_(number_of_bits),
      noise_probability_(noise_probability) {
  std::vector<uint64_t> surface_hashes;
  for (const auto& surface : surface_list) {
    surface_hashes.emplace_back(surface.ToUkmMetricHash());
  }
  reid_surface_key_ = blink::IdentifiableSurface::FromTypeAndToken(
      blink::IdentifiableSurface::Type::kReidScoreEstimator,
      base::make_span(surface_hashes));
  std::vector<std::pair<blink::IdentifiableSurface,
                        absl::optional<blink::IdentifiableToken>>>
      surfaces_vector;
  surfaces_vector.reserve(surface_list.size());
  for (blink::IdentifiableSurface surface : surface_list) {
    surfaces_vector.emplace_back(surface, absl::nullopt);
  }
  // Use the constructor which takes an unsorted vector, because inserting the
  // items one by one runs in O(n^2) time.
  surfaces_ = base::flat_map<blink::IdentifiableSurface,
                             absl::optional<blink::IdentifiableToken>>(
      std::move(surfaces_vector));
}

PrivacyBudgetReidScoreEstimator::ReidBlockStorage::~ReidBlockStorage() =
    default;

PrivacyBudgetReidScoreEstimator::ReidBlockStorage::ReidBlockStorage(
    ReidBlockStorage&&) = default;

PrivacyBudgetReidScoreEstimator::ReidBlockStorage&
PrivacyBudgetReidScoreEstimator::ReidBlockStorage::operator=(
    ReidBlockStorage&&) = default;

bool PrivacyBudgetReidScoreEstimator::ReidBlockStorage::Full() const {
  DCHECK_LE(recorded_values_count_, surfaces_.size());
  return recorded_values_count_ == surfaces_.size();
}

void PrivacyBudgetReidScoreEstimator::ReidBlockStorage::Record(
    blink::IdentifiableSurface surface,
    blink::IdentifiableToken value) {
  auto surface_itr = surfaces_.find(surface);
  if (surface_itr != surfaces_.end()) {
    if (!surface_itr->second.has_value()) {
      ++recorded_values_count_;
    }
    surface_itr->second = value;
  }
}

uint64_t
PrivacyBudgetReidScoreEstimator::ReidBlockStorage::ComputeHashForReidScore() {
  std::vector<uint64_t> tokens;
  uint64_t salt = base::RandGenerator(salt_range_);

  tokens.emplace_back(salt);
  for (blink::IdentifiableToken value : GetValues()) {
    tokens.emplace_back(static_cast<uint64_t>(value.ToUkmMetricValue()));
  }
  // Initialize Reid hash with random noise.
  uint64_t reid_hash = base::RandUint64();
  // Calculate the real hash if the random number is greater than the Reid noise
  // probability.
  if (base::RandDouble() >= noise_probability_) {
    // Use the hash function embedded in IdentifiableToken.
    reid_hash = blink::IdentifiabilityDigestOfBytes(
        base::as_bytes(base::make_span(tokens)));
  }
  // Create mask based on reid_bits required.
  constexpr uint64_t kTypedOne = 1;
  uint64_t mask = (kTypedOne << number_of_bits_) - 1;
  uint64_t needed_bits = reid_hash & mask;
  // Return salt in the left 32 bits and Reid b-bits hash in the right 32 bits.
  return ((salt << 32) | needed_bits);
}

std::vector<blink::IdentifiableToken>
PrivacyBudgetReidScoreEstimator::ReidBlockStorage::GetValues() const {
  DCHECK(Full());
  std::vector<blink::IdentifiableToken> values;
  for (auto& [key, value] : surfaces_) {
    DCHECK(value.has_value());
    values.emplace_back(value.value());
  }
  return values;
}

uint64_t PrivacyBudgetReidScoreEstimator::ReidBlockStorage::salt_range() const {
  DCHECK(Full());
  return salt_range_;
}

int PrivacyBudgetReidScoreEstimator::ReidBlockStorage::number_of_bits() const {
  DCHECK(Full());
  return number_of_bits_;
}

double PrivacyBudgetReidScoreEstimator::ReidBlockStorage::noise_probability()
    const {
  DCHECK(Full());
  return noise_probability_;
}

PrivacyBudgetReidScoreEstimator::PrivacyBudgetReidScoreEstimator(
    const IdentifiabilityStudyGroupSettings* state_settings,
    PrefService* pref_service)
    : settings_(state_settings), pref_service_(pref_service) {}

void PrivacyBudgetReidScoreEstimator::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!settings_->enabled() || !settings_->IsUsingReidScoreEstimator())
    return;

  already_reported_reid_blocks_ =
      DecodeIdentifiabilityFieldTrialParam<IdentifiableSurfaceList>(
          pref_service_->GetString(prefs::kPrivacyBudgetReportedReidBlocks));

  surface_blocks_.clear();
  for (size_t i = 0; i < settings_->reid_blocks().size(); ++i) {
    // All the vectors below have the same size. This is enforced by
    // `IdentifiabilityStudyGroupSettings`.
    ReidBlockStorage block(
        /*surface_list=*/settings_->reid_blocks()[i],
        /*salt_range=*/settings_->reid_blocks_salts_ranges()[i],
        /*number_of_bits=*/settings_->reid_blocks_bits()[i],
        /*noise_probability=*/
        settings_->reid_blocks_noise_probabilities()[i]);
    if (!base::Contains(already_reported_reid_blocks_,
                        block.reid_surface_key())) {
      surface_blocks_.emplace_back(std::move(block));
    }
  }
}

PrivacyBudgetReidScoreEstimator::~PrivacyBudgetReidScoreEstimator() = default;

void PrivacyBudgetReidScoreEstimator::ProcessForReidScore(
    blink::IdentifiableSurface surface,
    blink::IdentifiableToken token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool at_least_one_reported_block = false;
  for (auto block_itr = surface_blocks_.begin();
       block_itr != surface_blocks_.end();) {
    block_itr->Record(surface, token);
    if (block_itr->Full()) {
      // Report new Reid surface if the map is full.

      // Compute the Reid hash for the needed Reid block.
      uint64_t reid_hash = block_itr->ComputeHashForReidScore();

      // Report to UKM in a separate task in order to avoid re-entrancy.
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&ReportHashForReidScore,
                                    block_itr->reid_surface_key(), reid_hash));

      // Remove this block from the map, and store the information in the
      // PrefService, since we want to report a hash for a block only once.
      DCHECK(!base::Contains(already_reported_reid_blocks_,
                             block_itr->reid_surface_key()));
      already_reported_reid_blocks_.push_back(block_itr->reid_surface_key());
      // Note: The returned `block_itr` points to the next element in the
      // vector.
      block_itr = surface_blocks_.erase(block_itr);

      at_least_one_reported_block = true;
    } else {
      ++block_itr;
    }
  }

  if (at_least_one_reported_block)
    WriteReportedReidBlocksToPrefs();
}

void PrivacyBudgetReidScoreEstimator::WriteReportedReidBlocksToPrefs() const {
  DCHECK(!already_reported_reid_blocks_.empty());
  pref_service_->SetString(
      prefs::kPrivacyBudgetReportedReidBlocks,
      EncodeIdentifiabilityFieldTrialParam(already_reported_reid_blocks_));
}

void PrivacyBudgetReidScoreEstimator::ResetPersistedState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->ClearPref(prefs::kPrivacyBudgetReportedReidBlocks);
  already_reported_reid_blocks_.clear();
  surface_blocks_.clear();
  Init();
}
