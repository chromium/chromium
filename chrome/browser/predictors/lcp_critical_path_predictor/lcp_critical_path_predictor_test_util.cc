// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/lcp_critical_path_predictor/lcp_critical_path_predictor_test_util.h"

#include <cmath>

namespace {

bool AlmostEqual(const double x, const double y) {
  return std::fabs(x - y) <= 1e-6;  // Arbitrary but close enough.
}

}  // namespace

namespace predictors {

LcppData CreateLcppData(const std::string& host, uint64_t last_visit_time) {
  // |host| should not contain the scheme.
  CHECK_EQ(std::string::npos, host.find("://"));
  LcppData data;
  data.set_host(host);
  data.set_last_visit_time(last_visit_time);
  return data;
}

void InitializeLcpElementLocatorBucket(LcppData& lcpp_data,
                                       const std::string& lcp_element_locator,
                                       double frequency) {
  LcpElementLocatorBucket& bucket = *lcpp_data.mutable_lcpp_stat()
                                         ->mutable_lcp_element_locator_stat()
                                         ->add_lcp_element_locator_buckets();
  bucket.set_lcp_element_locator(lcp_element_locator);
  bucket.set_frequency(frequency);
}

void InitializeLcpInfluencerScriptUrlsBucket(LcppData& lcpp_data,
                                             const std::vector<GURL>& urls,
                                             double frequency) {
  for (auto& url : urls) {
    lcpp_data.mutable_lcpp_stat()
        ->mutable_lcp_script_url_stat()
        ->mutable_main_buckets()
        ->insert({url.spec(), frequency});
  }
}

void InitializeFontUrlsBucket(LcppData& lcpp_data,
                              const std::vector<GURL>& urls,
                              double frequency) {
  for (const auto& url : urls) {
    lcpp_data.mutable_lcpp_stat()
        ->mutable_fetched_font_url_stat()
        ->mutable_main_buckets()
        ->insert({url.spec(), frequency});
  }
}

void InitializeSubresourceUrlsBucket(LcppData& lcpp_data,
                                     const std::vector<GURL>& urls,
                                     double frequency) {
  for (const auto& url : urls) {
    lcpp_data.mutable_lcpp_stat()
        ->mutable_fetched_subresource_url_stat()
        ->mutable_main_buckets()
        ->insert({url.spec(), frequency});
  }
}

void InitializeLcpElementLocatorOtherBucket(LcppData& lcpp_data,
                                            double frequency) {
  lcpp_data.mutable_lcpp_stat()
      ->mutable_lcp_element_locator_stat()
      ->set_other_bucket_frequency(frequency);
}

void InitializeLcpInfluencerScriptUrlsOtherBucket(LcppData& lcpp_data,
                                                  double frequency) {
  lcpp_data.mutable_lcpp_stat()
      ->mutable_lcp_script_url_stat()
      ->set_other_bucket_frequency(frequency);
}

void InitializeFontUrlsOtherBucket(LcppData& lcpp_data, double frequency) {
  lcpp_data.mutable_lcpp_stat()
      ->mutable_fetched_font_url_stat()
      ->set_other_bucket_frequency(frequency);
}

void InitializeSubresourceUrlsOtherBucket(LcppData& lcpp_data,
                                          double frequency) {
  lcpp_data.mutable_lcpp_stat()
      ->mutable_fetched_subresource_url_stat()
      ->set_other_bucket_frequency(frequency);
}

std::ostream& operator<<(std::ostream& os,
                         const LcpElementLocatorBucket& bucket) {
  return os << "[" << bucket.lcp_element_locator() << "," << bucket.frequency()
            << "]";
}

std::ostream& operator<<(std::ostream& os, const LcpElementLocatorStat& data) {
  for (const LcpElementLocatorBucket& bucket :
       data.lcp_element_locator_buckets()) {
    os << "\t\t\t\t" << bucket << std::endl;
  }
  os << "\t\t\t\t" << "[<other_bucket>," << data.other_bucket_frequency() << "]"
     << std::endl;
  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const LcppStringFrequencyStatData& data) {
  for (const auto& [url, frequency] : data.main_buckets()) {
    os << "\t\t\t\t[" << url << "," << frequency << "]" << std::endl;
  }
  os << "\t\t\t\t" << "[<other_bucket>," << data.other_bucket_frequency() << "]"
     << std::endl;
  return os;
}

std::ostream& operator<<(
    std::ostream& os,
    const google::protobuf::Map<std::string, int32_t>& data) {
  for (const auto& [url, value] : data) {
    os << "\t\t\t\t[" << url << ", " << value << "]" << std::endl;
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, const LcppData& data) {
  os << "[" << data.host() << "," << data.last_visit_time() << "]" << std::endl;
  os << "lcpp_stat:" << std::endl;
  os << data.lcpp_stat();
  os << "lcpp_key_stat:" << std::endl;
  os << data.lcpp_key_stat();
  return os;
}

std::ostream& operator<<(std::ostream& os, const LcppKeyStat& key_stat) {
  os << "\t" << "lcpp_stat_map:" << std::endl;
  for (const auto& [path, path_key_stat] : key_stat.lcpp_stat_map()) {
    os << "\t\t" << path << std::endl;
    os << path_key_stat << std::endl;
  }
  os << "\t" << "key_frequency_stat:" << std::endl;
  os << key_stat.key_frequency_stat();
  return os;
}

std::ostream& operator<<(std::ostream& os, const LcppStat& stat) {
  // Output lcp_element_locator_stat.
  os << "\t\t" << "lcp_element_locator_stat:" << std::endl;
  os << stat.lcp_element_locator_stat();

  // Output lcp_script_url_stat.
  os << "\t\t" << "lcp_script_url_stat:" << std::endl;
  os << stat.lcp_script_url_stat();

  // Output fetched_font_url_stat.
  os << "\t\t" << "fetched_font_url_stat:" << std::endl;
  os << stat.fetched_font_url_stat();

  // Output fetched_subresource_url_stat.
  os << "\t\t" << "fetched_subresource_url_stat:" << std::endl;
  os << stat.fetched_subresource_url_stat();

  // Output fetched_subresource_url_destination.
  os << "\t\t" << "fetched_subresource_url_destination:" << std::endl;
  os << stat.fetched_subresource_url_destination();

  return os;
}

bool operator==(const LcpElementLocatorBucket& lhs,
                const LcpElementLocatorBucket& rhs) {
  return lhs.lcp_element_locator() == rhs.lcp_element_locator() &&
         AlmostEqual(lhs.frequency(), rhs.frequency());
}

bool operator==(const LcpElementLocatorStat& lhs,
                const LcpElementLocatorStat& rhs) {
  if (lhs.lcp_element_locator_buckets_size() !=
          rhs.lcp_element_locator_buckets_size() ||
      !AlmostEqual(lhs.other_bucket_frequency(),
                   rhs.other_bucket_frequency())) {
    return false;
  }

  // lcp_element_locator_buckets don't care the order.
  std::map<std::string, double> lhs_map;
  for (const auto& it : lhs.lcp_element_locator_buckets()) {
    lhs_map.emplace(it.lcp_element_locator(), it.frequency());
  }

  for (const auto& rhs_it : rhs.lcp_element_locator_buckets()) {
    const auto& lhs_it = lhs_map.find(rhs_it.lcp_element_locator());
    if (lhs_it == lhs_map.end() ||
        !AlmostEqual(lhs_it->second, rhs_it.frequency())) {
      return false;
    }
  }

  return true;
}

bool operator==(const LcppStringFrequencyStatData& lhs,
                const LcppStringFrequencyStatData& rhs) {
  if (lhs.main_buckets_size() != rhs.main_buckets_size() ||
      !AlmostEqual(lhs.other_bucket_frequency(),
                   rhs.other_bucket_frequency())) {
    return false;
  }

  for (const auto& [rhs_entry, rhs_frequency] : rhs.main_buckets()) {
    const auto& lhs_it = lhs.main_buckets().find(rhs_entry);
    if (lhs_it == lhs.main_buckets().end() ||
        !AlmostEqual(lhs_it->second, rhs_frequency)) {
      return false;
    }
  }

  return true;
}

bool operator==(const google::protobuf::Map<std::string, int32_t>& lhs,
                const google::protobuf::Map<std::string, int32_t>& rhs) {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  for (const auto& [url, value] : lhs) {
    const auto it = rhs.find(url);
    if (it == rhs.end()) {
      return false;
    }
    if (value != it->second) {
      return false;
    }
  }
  return true;
}

bool operator==(const LcppStat& lhs, const LcppStat& rhs) {
  return lhs.lcp_element_locator_stat() == rhs.lcp_element_locator_stat() &&
         lhs.lcp_script_url_stat() == rhs.lcp_script_url_stat() &&
         lhs.fetched_font_url_stat() == rhs.fetched_font_url_stat() &&
         lhs.fetched_subresource_url_stat() ==
             rhs.fetched_subresource_url_stat() &&
         lhs.fetched_subresource_url_destination() ==
             rhs.fetched_subresource_url_destination();
}

bool operator==(const LcppData& lhs, const LcppData& rhs) {
  return lhs.host() == rhs.host() && lhs.lcpp_stat() == rhs.lcpp_stat();
}

}  // namespace predictors
