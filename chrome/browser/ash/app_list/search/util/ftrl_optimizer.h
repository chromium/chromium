// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_UTIL_FTRL_OPTIMIZER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_UTIL_FTRL_OPTIMIZER_H_

#include <string>
#include <vector>

#include "ash/utility/persistent_proto.h"
#include "base/files/file_path.h"
#include "chrome/browser/ash/app_list/search/util/ftrl_optimizer.pb.h"

namespace app_list {

// A class implementing the follow-the-regularized-leader optimization
// algorithm.
//
// ALGORITHM
// =========
//
// This is an ensemble model over experts, keeping a weight for each W[i].
// The Score method takes a vector of items, and requests a score from each
// expert. The final score for an item is simply a weighted average:
//
// Score(item, expert_scores):
//   return sum(W[i] * expert_scores[i])
//
// After the user clicks an item, the weights are then updated to reflect the
// accuracy of each expert, like so:
//
// Train(item):
//   for expert i:
//     if expert i did not rank item in its last scores:
//       loss = 1
//     else
//       loss = (rank of item in expert i's last scores) / scores.size
//
//     w[i] = gamma/num_experts + (1-gamma) exp(-alpha * loss) * w[i]
//
//   normalize weights to sum to 1.
//
// PARAMETERS
// ==========
//
// - Alpha is a positive learning rate. Higher values mean weights change more
//   quickly.
//
// - Gamma is a regularization parameter in [0,1]. It controls the minimum
//   weight of an expert. 0 means no regularization, and 1 means all weights
//   will always be equal.
class FtrlOptimizer {
 public:
  // All user-settable parameters of the FTRL optimizer. The defaults should be
  // customized as-needed.
  struct Params {
    double alpha = 0.1;
    double gamma = 0.1;
    size_t num_experts = 0;
  };

  using Proto = ash::PersistentProto<FtrlOptimizerProto>;

  FtrlOptimizer(FtrlOptimizer::Proto, const Params& params);
  ~FtrlOptimizer();

  FtrlOptimizer(const FtrlOptimizer&) = delete;
  FtrlOptimizer& operator=(const FtrlOptimizer&) = delete;

  void Clear();

  // Score the given |items| using the given |scores| from experts. The outer
  // vector of |scores| must be Params.num_experts long, with inner vectors the
  // same length as |items|.
  std::vector<double> Score(std::vector<std::string>&& items,
                            std::vector<std::vector<double>>&& expert_scores);

  void Train(const std::string& item);

 private:
  double Loss(size_t expert, const std::string& item);

  void OnProtoInit();

  Params params_;

  // For each result id it matched a vector that contains result from
  // different experts. The vector size will equal the number of experts,
  // and the scores are always in the same order of experts.
  std::map<std::string, std::vector<double>> last_expert_scores_;

  ash::PersistentProto<FtrlOptimizerProto> proto_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_UTIL_FTRL_OPTIMIZER_H_
