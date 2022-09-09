// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_COUNTERS_HOSTED_APPS_COUNTER_H_
#define CHROME_BROWSER_BROWSING_DATA_COUNTERS_HOSTED_APPS_COUNTER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"

class Profile;

// A BrowsingDataCounter that returns the number of hosted apps and names
// of up to two of them as examples.
class HostedAppsCounter : public browsing_data::BrowsingDataCounter {
 public:
  class HostedAppsResult : public FinishedResult {
   public:
    HostedAppsResult(const HostedAppsCounter* source,
                     ResultInt num_apps,
                     const std::vector<std::string>& examples);

    HostedAppsResult(const HostedAppsResult&) = delete;
    HostedAppsResult& operator=(const HostedAppsResult&) = delete;

    ~HostedAppsResult() override;

    const std::vector<std::string>& examples() const { return examples_; }

   private:
    const std::vector<std::string> examples_;
  };

  explicit HostedAppsCounter(Profile* profile);

  HostedAppsCounter(const HostedAppsCounter&) = delete;
  HostedAppsCounter& operator=(const HostedAppsCounter&) = delete;

  ~HostedAppsCounter() override;

  const char* GetPrefName() const override;

 private:
  // BrowsingDataCounter:
  void Count() override;

  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_BROWSING_DATA_COUNTERS_HOSTED_APPS_COUNTER_H_
