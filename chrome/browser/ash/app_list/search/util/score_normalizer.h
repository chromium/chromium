// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_UTIL_SCORE_NORMALIZER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_UTIL_SCORE_NORMALIZER_H_

#include "ash/utility/persistent_proto.h"
#include "chrome/browser/ash/app_list/search/util/score_normalizer.pb.h"

namespace app_list {

namespace test {
class ScoreNormalizerTest;
}

// The score normalizer is a heuristic model that attempts to map a incoming
// stream of numbers drawn from a fixed distribution into a uniform
// distribution.
//
// For more information on the algorithm, see score_normalizer.md.
//
// The class is intended to learn and normalize many distributions at once.
// Each distribution is denoted by a |name| passed to the public methods of
// this class. Distributions are learned independently, and are only bundled
// into one object for convenience and to avoid writing too many files to disk.
class ScoreNormalizer {
 public:
  // All user-settable parameters of the score normalizer. The struct has some
  // reasonable defaults, but this should be customized per use-case.
  struct Params {
    // A version for this set of parameters. This is saved alongside on-disk
    // state for the ScoreNormalizer and, if it doesn't match on reading, the
    // on-disk state is cleared.
    int32_t version = 1;

    // The maximum number of bins to discretize each distribution into.
    int32_t max_bins = 5;
  };

  using Proto = ash::PersistentProto<ScoreNormalizerProto>;

  ScoreNormalizer(ScoreNormalizer::Proto proto, const Params& params);
  ~ScoreNormalizer();

  ScoreNormalizer(const ScoreNormalizer&) = delete;
  ScoreNormalizer& operator=(const ScoreNormalizer&) = delete;

  // Normalizes the provided score with the normalizer for |name|.
  double Normalize(const std::string& name, double score) const;

  // Train the |name| normalizer on |score|.
  void Update(const std::string& name, double score);

 private:
  friend class test::ScoreNormalizerTest;

  void OnProtoInit();

  ash::PersistentProto<ScoreNormalizerProto> proto_;
  Params params_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_UTIL_SCORE_NORMALIZER_H_
