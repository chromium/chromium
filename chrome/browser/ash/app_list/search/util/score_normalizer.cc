// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/util/score_normalizer.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "base/functional/bind.h"
#include "base/logging.h"

namespace app_list {
namespace {

using Bin = ScoreNormalizerProto::Bin;
using Bins = google::protobuf::RepeatedPtrField<ScoreNormalizerProto::Bin>;

// This should be incremented whenever a change to the algorithm is made that
// is incompatible with on-disk state. On reading, any state is wiped if its
// version doesn't match.
constexpr int32_t kModelVersion = 1;

// A default uninformative score to be returned when we don't have enough
// information to provide a proper normalized score.
constexpr double kDefaultScore = 0.5;

// Return the index of the bin in |bins| that contains |score|.
size_t BinIndexFor(const Bins& bins, double score) {
  DCHECK(bins[0].lower_divider() == -INFINITY);
  for (int i = 0; i < bins.size(); ++i) {
    if (bins.at(i).lower_divider() > score) {
      return i - 1;
    }
  }
  return bins.size() - 1;
}

// Computes x * log(x) for use in entropy calculations. Inputs will be
// probabilities, and so an input of 0.0 is special-cased consistent with a
// zero-probability's contribution to entropy.
inline double xlogx(double x) {
  DCHECK(0.0 <= x && x <= 1.0);
  return x == 0.0 ? 0 : x * log(x);
}

// Inserts a bin into |bins| with the given |lower_divider| and |count|. Ensures
// the bins are in the correct order by sorting |bins|.
inline void InsertBin(Bins& bins, double lower_divider, double count) {
  Bin& bin = *bins.Add();
  bin.set_lower_divider(lower_divider);
  bin.set_count(count);

  std::sort(bins.begin(), bins.end(), [](const auto& a, const auto& b) {
    return a.lower_divider() < b.lower_divider();
  });
}

}  // namespace

ScoreNormalizer::ScoreNormalizer(ScoreNormalizer::Proto proto,
                                 const Params& params)
    : proto_(std::move(proto)), params_(params) {
  // `proto_` is a class member so it is safe to call `RegisterOnInitUnsafe()`.
  proto_.RegisterOnInitUnsafe(
      base::BindOnce(&ScoreNormalizer::OnProtoInit, base::Unretained(this)));

  proto_.Init();
}

ScoreNormalizer::~ScoreNormalizer() {}

double ScoreNormalizer::Normalize(const std::string& name, double score) const {
  // If we haven't finished initializing, return a default score.
  if (!proto_) {
    return kDefaultScore;
  }

  // If we don't have any data for |name|, return a default score.
  const auto& normalizers = proto_->normalizers();
  const auto it = normalizers.find(name);
  if (it == normalizers.end()) {
    return kDefaultScore;
  }

  const Bins& bins = it->second.bins();
  size_t size = bins.size();

  // If we only have zero or one bins, there's no reasonable normalized score to
  // return, so use the default.
  if (size < 2) {
    return kDefaultScore;
  }

  // If all bins expect the first have the same lower divider, the normalized
  // score is meaningless, so use the default.
  if (size > 2 && bins[1].lower_divider() == bins[size - 1].lower_divider()) {
    return kDefaultScore;
  }

  size_t index = BinIndexFor(bins, score);
  double offset;
  if (index == 0) {
    // The leftmost bin has a finite right boundary but a -infinite left
    // boundary. Offset with a hyperbolic decay function bounded to (0,1].
    offset = 1 / (bins[1].lower_divider() - score + 1);
  } else if (index == size - 1) {
    // The rightmost bin has a finite left boundary but an infinite right
    // boundary. Offset with a hyperbolic decay function bounded to [0,1).
    offset = 1 - 1 / (score - bins[index].lower_divider() + 1);
  } else {
    // If this is an 'internal' bin with finite left and right boundaries,
    // offset linearly between the two boundaries.
    double left = bins[index].lower_divider();
    double right = bins[index + 1].lower_divider();
    offset = (left == right) ? 0.0 : (score - left) / (right - left);
  }

  return (index + offset) / static_cast<double>(size);
}

void ScoreNormalizer::Update(const std::string& name, double score) {
  // If we haven't finished initializing, ignore the update.
  if (!proto_) {
    return;
  }

  auto& normalizer = (*proto_->mutable_normalizers())[name];
  auto& bins = *normalizer.mutable_bins();

  double total = normalizer.total() + 1;
  normalizer.set_total(total);

  // If the normalizer for |name| has no bins, we need to initialize an empty
  // first bin covering [-infinity, infinity].
  if (bins.empty()) {
    InsertBin(bins, -INFINITY, 0.0);
  }

  // If we haven't reached the target number of bins yet, insert an existing bin
  // at |score|. If we do this, we can early exit the rest of the algorithm.
  if (bins.size() < params_.max_bins) {
    InsertBin(bins, score, 1.0);
    proto_.QueueWrite();
    return;
  }

  // Update the count in the relevant bin for |score|.
  size_t split_index = BinIndexFor(bins, score);
  Bin& split_bin = bins[split_index];
  split_bin.set_count(split_bin.count() + 1.0);

  // Calculate the size of the updated bin in the case that it is split. These
  // are currently equal but may not be in future.
  double split_l_count = split_bin.count() / 2.0;
  double split_r_count = split_bin.count() / 2.0;

  // Select a contiguous pair of bins as candidates to merge. For simplicity, we
  // don't allow the merge to overlap with the split.
  size_t merge_index = std::numeric_limits<size_t>::max();
  double merge_l_count = INFINITY, merge_r_count = INFINITY;
  for (size_t i = 0; i + 1 < static_cast<size_t>(bins.size()); ++i) {
    if (i == split_index - 1 || i == split_index)
      continue;
    double l_count = bins[i].count();
    double r_count = bins[i + 1].count();
    if (l_count + r_count < merge_l_count + merge_r_count) {
      merge_index = i;
      merge_l_count = l_count;
      merge_r_count = r_count;
    }
  }

  // If we don't have enough bins to perform a merge, early exit.
  if (merge_index == std::numeric_limits<size_t>::max()) {
    proto_.QueueWrite();
    return;
  }

  // Compute the difference in entropy if |split_index| bin is split and the
  // |merge_index| bins are merged. Most terms cancel out here, see
  // score_normalizer.md for details.
  //
  // These calculations drop the negatives so are actually computing the
  // difference in negentropy.
  double p_split_l = split_l_count / total;
  double p_split_r = split_r_count / total;
  double p_merge_l = merge_l_count / total;
  double p_merge_r = merge_r_count / total;

  double new_h =
      xlogx(p_merge_l + p_merge_r) + xlogx(p_split_l) + xlogx(p_split_r);
  double old_h =
      xlogx(p_merge_l) + xlogx(p_merge_r) + xlogx(p_split_l + p_split_r);

  // Only do the proposed split/merge operation if it increases entropy.
  if (old_h <= new_h) {
    proto_.QueueWrite();
    return;
  }

  // Terminology. we have five bins of interest:
  // - split-combo, the to-be-split bin at |split_index|.
  // - split-left and split-right, the post-split bins
  // - merge-left and merge-right, the to-be-merged bins

  if (split_index < merge_index) {
    // Merge merge-left into merge-right, leaving merge-left removable.
    bins[merge_index + 1].set_lower_divider(bins[merge_index].lower_divider());
    bins[merge_index + 1].set_count(merge_l_count + merge_r_count);

    // Shuffle bins in (split-combo, merge-left] right by one. This wraps
    // around, leaving merge-left at |split_index| + 1, which will become
    // split-right.
    auto start = bins.begin() + split_index + 1;
    auto end = bins.begin() + merge_index + 1;
    std::rotate(start, end - 1, end);

    // Create split-right by overwriting merge-left, and convert split-combo
    // into split-left
    bins[split_index + 1].set_lower_divider(score);
    bins[split_index + 1].set_count(split_r_count);
    bins[split_index].set_count(split_l_count);
  } else {
    // The merge and split are non-intersecting.
    DCHECK(merge_index < split_index - 1);

    // Merge merge-right into merge-left, leaving merge-right removable.
    bins[merge_index].set_count(merge_l_count + merge_r_count);

    // Shuffle bins in [merge-right, split-combo) left by one. This wraps
    // around, leaving merge-right at |split_index| - 1, which will become
    // split-left.
    auto start = bins.begin() + merge_index + 1;
    auto end = bins.begin() + split_index;
    std::rotate(start, start + 1, end);

    // Create split-left by overwriting merge-right and convert split-combo into
    // split-right.
    bins[split_index - 1].set_lower_divider(bins[split_index].lower_divider());
    bins[split_index - 1].set_count(split_l_count);
    bins[split_index].set_lower_divider(score);
    bins[split_index].set_count(split_r_count);
  }

  proto_.QueueWrite();
}

void ScoreNormalizer::OnProtoInit() {
  DCHECK(proto_.initialized());

  if ((proto_->has_model_version() &&
       proto_->model_version() != kModelVersion) ||
      (proto_->has_parameter_version() &&
       proto_->parameter_version() != params_.version)) {
    proto_.Purge();
  }

  proto_->set_model_version(kModelVersion);
  proto_->set_parameter_version(params_.version);
  proto_.QueueWrite();
}

}  // namespace app_list
