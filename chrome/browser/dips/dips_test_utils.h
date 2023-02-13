// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_TEST_UTILS_H_
#define CHROME_BROWSER_DIPS_DIPS_TEST_UTILS_H_

#include <iosfwd>
#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/dips/dips_redirect_info.h"
#include "chrome/browser/dips/dips_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "url/gurl.h"

namespace testing {
class MatchResultListener;
}

class RedirectChainObserver : public DIPSService::Observer {
 public:
  explicit RedirectChainObserver(DIPSService* service, GURL final_url);
  ~RedirectChainObserver() override;

  void OnChainHandled(const DIPSRedirectChainInfoPtr& chain) override;

  void Wait();

 private:
  GURL final_url_;
  base::RunLoop run_loop_;
  base::ScopedObservation<DIPSService, Observer> obs_{this};
};

// Checks that the URLs associated with the UKM entries with the given name are
// as expected. Sorts the URLs so order doesn't matter.
//
// Example usage:
//
// EXPECT_THAT(ukm_recorder, EntryUrlsAre(entry_name, {url1, url2, url3}));
class EntryUrlsAre {
 public:
  using is_gtest_matcher = void;
  EntryUrlsAre(std::string entry_name, std::vector<std::string> urls);
  EntryUrlsAre(const EntryUrlsAre&);
  EntryUrlsAre(EntryUrlsAre&&);
  ~EntryUrlsAre();

  using UkmEntry = ukm::TestUkmRecorder::HumanReadableUkmEntry;
  bool MatchAndExplain(const ukm::TestUkmRecorder& ukm_recorder,
                       testing::MatchResultListener* result_listener) const;

  void DescribeTo(std::ostream* os) const;
  void DescribeNegationTo(std::ostream* os) const;

 private:
  std::string entry_name_;
  std::vector<std::string> expected_urls_;
};

#endif  // CHROME_BROWSER_DIPS_DIPS_TEST_UTILS_H_
