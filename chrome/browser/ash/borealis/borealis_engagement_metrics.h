// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_ENGAGEMENT_METRICS_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_ENGAGEMENT_METRICS_H_

#include <memory>

class Profile;

namespace guest_os {
class GuestOsEngagementMetrics;
}

namespace borealis {

class BorealisEngagementMetrics {
 public:
  explicit BorealisEngagementMetrics(Profile* profile);
  BorealisEngagementMetrics(const BorealisEngagementMetrics&) = delete;
  BorealisEngagementMetrics& operator=(const BorealisEngagementMetrics&) =
      delete;
  ~BorealisEngagementMetrics();

 private:
  std::unique_ptr<guest_os::GuestOsEngagementMetrics> borealis_metrics_;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_ENGAGEMENT_METRICS_H_
