// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/util/ftrl_optimizer.h"

#include <cmath>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"

namespace app_list {
namespace {

// A version number to be incremented each time a code change invalidates the
// state stored on-disk in |proto_|.
constexpr int kVersion = 1;

double Total(const google::protobuf::RepeatedField<double>& values) {
  double total = 0.0;
  for (double v : values)
    total += v;
  return total;
}

// Normalize |values| to sum to 1 in-place.
void Normalize(google::protobuf::RepeatedField<double>& values) {
  double total = Total(values);
  if (total == 0.0)
    return;
  for (int i = 0; i < values.size(); ++i)
    values[i] = values[i] / total;
}

}  // namespace

FtrlOptimizer::FtrlOptimizer(FtrlOptimizer::Proto proto, const Params& params)
    : params_(params), proto_(std::move(proto)) {
  DCHECK_GT(params.alpha, 0.0);
  DCHECK_GE(params.gamma, 0.0);
  DCHECK_LE(params.gamma, 1.0);
  DCHECK_GT(params.num_experts, 0u);

  // `proto_` is a class member so it is safe to call `RegisterOnInitUnsafe()`.
  proto_.RegisterOnInitUnsafe(
      base::BindOnce(&FtrlOptimizer::OnProtoInit, base::Unretained(this)));

  proto_.Init();
}

FtrlOptimizer::~FtrlOptimizer() {}

void FtrlOptimizer::Clear() {
  last_expert_scores_.clear();
}

std::vector<double> FtrlOptimizer::Score(
    std::vector<std::string>&& items,
    std::vector<std::vector<double>>&& expert_scores) {
  size_t num_items = items.size();
  size_t num_experts = params_.num_experts;

  std::vector<double> result(num_items, 0.0);
  if (!proto_.initialized())
    return result;

  const auto& weights = proto_->weights();
  DCHECK_EQ(expert_scores.size(), num_experts);
  DCHECK_GE(weights.size(), 0);
  DCHECK_EQ(static_cast<size_t>(weights.size()), num_experts);
  for (size_t i = 0; i < num_items; ++i) {
    last_expert_scores_[items[i]] = {};

    for (size_t j = 0; j < num_experts; ++j) {
      result[i] += weights[j] * expert_scores[j][i];
      last_expert_scores_[items[i]].emplace_back(expert_scores[j][i]);
    }
  }

  return result;
}

void FtrlOptimizer::Train(const std::string& item) {
  // If |last_items_| is empty, experts had no chance at prediction and we
  // should early exit. This could happen if |proto_| finishes initializing
  // after Score but before Train.
  if (!proto_.initialized() || last_expert_scores_.empty()) {
    return;
  }

  // Compute the loss of each expert and update weights.
  auto& weights = *proto_->mutable_weights();
  for (int i = 0; i < weights.size(); ++i) {
    double loss = Loss(i, item);
    double fixed_share = params_.gamma / weights.size();
    double weight_factor = (1.0 - params_.gamma) * exp(-params_.alpha * loss);
    weights[i] = fixed_share + weight_factor * weights[i];
  }

  // Re-normalize the weights.
  Normalize(weights);
  DCHECK_LE(std::abs(Total(proto_->weights()) - 1.0), 1.0e-5);

  proto_.StartWrite();
}

double FtrlOptimizer::Loss(size_t expert, const std::string& item) {
  size_t num_experts = params_.num_experts;
  size_t num_items = last_expert_scores_.size();

  DCHECK_GT(num_items, 0u);
  DCHECK_LT(expert, num_experts);

  // Find the score of the launched item.
  double score = {0.0};

  if (last_expert_scores_.find(item) != last_expert_scores_.end()) {
    DCHECK_EQ(last_expert_scores_[item].size(), num_experts);
    score = last_expert_scores_[item][expert];
  }

  // Find the rank of the item, ie. the number of items with higher score.
  size_t rank = 0;

  for (const auto& scores : last_expert_scores_) {
    if (scores.second[expert] > score) {
      ++rank;
    }
  }

  // The loss is linear in the |rank|. A loss of 1.0 means |item| wasn't
  // included at all.
  DCHECK(!last_expert_scores_.empty());
  return static_cast<double>(rank) / last_expert_scores_.size();
}

void FtrlOptimizer::OnProtoInit() {
  if (!proto_->has_version() || proto_->version() != kVersion ||
      params_.num_experts !=
          base::checked_cast<size_t>(proto_->weights_size())) {
    proto_.Purge();
    proto_->set_version(kVersion);
    for (size_t i = 0; i < params_.num_experts; ++i)
      proto_->add_weights(1.0 / params_.num_experts);
  }
  DCHECK_LE(std::abs(Total(proto_->weights()) - 1.0), 1.0e-5);
}

}  // namespace app_list
