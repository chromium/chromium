// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_PREDICTORS_LCP_CRITICAL_PATH_PREDICTOR_LCP_CRITICAL_PATH_PREDICTOR_TEST_UTIL_H_
#define CHROME_BROWSER_PREDICTORS_LCP_CRITICAL_PATH_PREDICTOR_LCP_CRITICAL_PATH_PREDICTOR_TEST_UTIL_H_

#include <string>
#include <vector>

#include "chrome/browser/predictors/lcp_critical_path_predictor/lcp_critical_path_predictor.pb.h"
#include "url/gurl.h"

namespace predictors {

LcppData CreateLcppData(const std::string& host, uint64_t last_visit_time = 0);

void InitializeLcpElementLocatorBucket(LcppData& lcpp_data,
                                       const std::string& lcp_element_locator,
                                       double frequency);
void InitializeLcpInfluencerScriptUrlsBucket(LcppData& lcpp_data,
                                             const std::vector<GURL>& urls,
                                             double frequency);
void InitializeFontUrlsBucket(LcppData& lcpp_data,
                              const std::vector<GURL>& urls,
                              double frequency);
void InitializeSubresourceUrlsBucket(LcppData& lcpp_data,
                                     const std::vector<GURL>& urls,
                                     double frequency);
void InitializeLcpElementLocatorOtherBucket(LcppData& lcpp_data,
                                            double frequency);
void InitializeLcpInfluencerScriptUrlsOtherBucket(LcppData& lcpp_data,
                                                  double frequency);
void InitializeFontUrlsOtherBucket(LcppData& lcpp_data, double frequency);
void InitializeSubresourceUrlsOtherBucket(LcppData& lcpp_data,
                                          double frequency);

// For printing failures nicely.
std::ostream& operator<<(std::ostream& os, const LcppData& data);
std::ostream& operator<<(std::ostream& os, const LcppKeyStat& key_stat);
std::ostream& operator<<(std::ostream& os, const LcppStat& stat);
std::ostream& operator<<(std::ostream& os,
                         const LcpElementLocatorBucket& bucket);
std::ostream& operator<<(std::ostream& os,
                         const LcppStringFrequencyStatData& data);

bool operator==(const LcpElementLocatorBucket& lhs,
                const LcpElementLocatorBucket& rhs);
bool operator==(const LcpElementLocatorStat& lhs,
                const LcpElementLocatorStat& rhs);
bool operator==(const LcppData& lhs, const LcppData& rhs);
bool operator==(const LcppStat& lhs, const LcppStat& rhs);
bool operator==(const LcppStringFrequencyStatData& lhs,
                const LcppStringFrequencyStatData& rhs);

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_LCP_CRITICAL_PATH_PREDICTOR_LCP_CRITICAL_PATH_PREDICTOR_TEST_UTIL_H_
