// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_filter/test_ruleset_publisher.h"

#include "base/hash/hash.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {
namespace testing {

namespace {

class RulesetDistributionListener {
 public:
  RulesetDistributionListener()
      : service_(g_browser_process->subresource_filter_ruleset_service()) {
    service_->SetRulesetPublishedCallbackForTesting(run_loop_.QuitClosure());
  }

  ~RulesetDistributionListener() {
    service_->SetRulesetPublishedCallbackForTesting(base::OnceClosure());
  }

  void AwaitDistribution() { run_loop_.Run(); }

 private:
  RulesetService* service_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(RulesetDistributionListener);
};

}  // namespace

TestRulesetPublisher::TestRulesetPublisher() = default;
TestRulesetPublisher::~TestRulesetPublisher() = default;

void TestRulesetPublisher::SetRuleset(const TestRuleset& unindexed_ruleset) {
  const std::string& test_ruleset_content_version(base::NumberToString(
      base::Hash(std::string(unindexed_ruleset.contents.begin(),
                             unindexed_ruleset.contents.end()))));
  subresource_filter::UnindexedRulesetInfo unindexed_ruleset_info;
  unindexed_ruleset_info.content_version = test_ruleset_content_version;
  unindexed_ruleset_info.ruleset_path = unindexed_ruleset.path;
  RulesetDistributionListener listener;
  g_browser_process->subresource_filter_ruleset_service()
      ->IndexAndStoreAndPublishRulesetIfNeeded(unindexed_ruleset_info);
  listener.AwaitDistribution();
}

}  // namespace testing
}  // namespace subresource_filter
