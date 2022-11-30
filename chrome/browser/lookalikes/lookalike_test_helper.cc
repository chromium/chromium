// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lookalikes/lookalike_test_helper.h"

#include "components/lookalikes/core/lookalike_url_util.h"
#include "components/reputation/core/safety_tip_test_utils.h"
#include "components/reputation/core/safety_tips_config.h"
#include "components/url_formatter/spoof_checks/idn_spoof_checker.h"
#include "components/url_formatter/spoof_checks/top_domains/test_top500_domains.h"

namespace test {
#include "components/url_formatter/spoof_checks/top_domains/browsertest_domains-trie-inc.cc"
}

void SetUpLookalikeTestParams() {
  // Use test top domain lists instead of the actual list.
  url_formatter::IDNSpoofChecker::HuffmanTrieParams trie_params{
      test::kTopDomainsHuffmanTree, sizeof(test::kTopDomainsHuffmanTree),
      test::kTopDomainsTrie, test::kTopDomainsTrieBits,
      test::kTopDomainsRootPosition};
  url_formatter::IDNSpoofChecker::SetTrieParamsForTesting(trie_params);

  // Use test top 500 domain skeletons instead of the actual list.
  Top500DomainsParams top500_params{
      test_top500_domains::kTop500EditDistanceSkeletons,
      test_top500_domains::kNumTop500EditDistanceSkeletons};
  SetTop500DomainsParamsForTesting(top500_params);

  reputation::InitializeSafetyTipConfig();
}

void TearDownLookalikeTestParams() {
  url_formatter::IDNSpoofChecker::RestoreTrieParamsForTesting();
  ResetTop500DomainsParamsForTesting();
}
