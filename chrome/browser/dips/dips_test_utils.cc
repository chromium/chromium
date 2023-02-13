// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

RedirectChainObserver::RedirectChainObserver(DIPSService* service,
                                             GURL final_url)
    : final_url_(std::move(final_url)) {
  obs_.Observe(service);
}

RedirectChainObserver::~RedirectChainObserver() = default;

void RedirectChainObserver::OnChainHandled(
    const DIPSRedirectChainInfoPtr& chain) {
  if (chain->final_url == final_url_) {
    run_loop_.Quit();
  }
}

void RedirectChainObserver::Wait() {
  run_loop_.Run();
}

EntryUrlsAre::EntryUrlsAre(std::string entry_name,
                           std::vector<std::string> urls)
    : entry_name_(std::move(entry_name)), expected_urls_(std::move(urls)) {
  // Sort the URLs before comparing, so order doesn't matter. (DIPSDatabase
  // currently sorts its results, but that could change and these tests
  // shouldn't care.)
  std::sort(expected_urls_.begin(), expected_urls_.end());
}

EntryUrlsAre::EntryUrlsAre(const EntryUrlsAre&) = default;
EntryUrlsAre::EntryUrlsAre(EntryUrlsAre&&) = default;
EntryUrlsAre::~EntryUrlsAre() = default;

bool EntryUrlsAre::MatchAndExplain(
    const ukm::TestUkmRecorder& ukm_recorder,
    testing::MatchResultListener* result_listener) const {
  std::vector<std::string> actual_urls;
  for (const auto* entry : ukm_recorder.GetEntriesByName(entry_name_)) {
    GURL url = ukm_recorder.GetSourceForSourceId(entry->source_id)->url();
    actual_urls.push_back(url.spec());
  }
  std::sort(actual_urls.begin(), actual_urls.end());

  // ExplainMatchResult() won't print out the full contents of `actual_urls`,
  // so for more helpful error messages, we do it ourselves.
  *result_listener << "whose entries for '" << entry_name_
                   << "' contain the URLs "
                   << testing::PrintToString(actual_urls) << ", ";

  // Use ContainerEq() instead of e.g. ElementsAreArray() because the error
  // messages are much more useful.
  return ExplainMatchResult(testing::ContainerEq(expected_urls_), actual_urls,
                            result_listener);
}

void EntryUrlsAre::DescribeTo(std::ostream* os) const {
  *os << "has entries for '" << entry_name_ << "' whose URLs are "
      << testing::PrintToString(expected_urls_);
}

void EntryUrlsAre::DescribeNegationTo(std::ostream* os) const {
  *os << "does not have entries for '" << entry_name_ << "' whose URLs are "
      << testing::PrintToString(expected_urls_);
}
