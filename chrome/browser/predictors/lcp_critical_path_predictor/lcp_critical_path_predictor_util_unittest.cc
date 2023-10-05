// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/lcp_critical_path_predictor/lcp_critical_path_predictor_util.h"

#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace predictors {

namespace {

TEST(IsValidLcppStatTest, Empty) {
  LcppStat lcpp_stat;
  EXPECT_TRUE(IsValidLcppStat(lcpp_stat));
}

TEST(IsValidLcppStatTest, LcpElementLocatorStat) {
  {
    LcppStat lcpp_stat;
    auto* locator_stat = lcpp_stat.mutable_lcp_element_locator_stat();
    locator_stat->set_other_bucket_frequency(0.1);
    auto* bucket = locator_stat->add_lcp_element_locator_buckets();
    bucket->set_lcp_element_locator("fake");
    bucket->set_frequency(0.1);
    EXPECT_TRUE(IsValidLcppStat(lcpp_stat));
  }
  {  // Without the repeated field.
    LcppStat lcpp_stat;
    auto* locator_stat = lcpp_stat.mutable_lcp_element_locator_stat();
    locator_stat->set_other_bucket_frequency(0.1);
    EXPECT_TRUE(IsValidLcppStat(lcpp_stat));
  }
  {  // Negative other frequency is invalid.
    LcppStat lcpp_stat;
    auto* locator_stat = lcpp_stat.mutable_lcp_element_locator_stat();
    locator_stat->set_other_bucket_frequency(-0.1);
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // Nothing in an entry is invalid.
    LcppStat lcpp_stat;
    auto* locator_stat = lcpp_stat.mutable_lcp_element_locator_stat();
    locator_stat->set_other_bucket_frequency(0.1);
    locator_stat->add_lcp_element_locator_buckets();  // allocate a bucket.
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // No frequency in an entry is invalid.
    LcppStat lcpp_stat;
    auto* locator_stat = lcpp_stat.mutable_lcp_element_locator_stat();
    locator_stat->set_other_bucket_frequency(0.1);
    auto* bucket = locator_stat->add_lcp_element_locator_buckets();
    bucket->set_lcp_element_locator("fake");
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // No element locator in an entry is invalid.
    LcppStat lcpp_stat;
    auto* locator_stat = lcpp_stat.mutable_lcp_element_locator_stat();
    locator_stat->set_other_bucket_frequency(0.1);
    auto* bucket = locator_stat->add_lcp_element_locator_buckets();
    bucket->set_frequency(0.1);
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // Negative frequency in an entry is invalid.
    LcppStat lcpp_stat;
    auto* locator_stat = lcpp_stat.mutable_lcp_element_locator_stat();
    locator_stat->set_other_bucket_frequency(0.1);
    auto* bucket = locator_stat->add_lcp_element_locator_buckets();
    bucket->set_lcp_element_locator("fake");
    bucket->set_frequency(-0.1);
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
}

TEST(IsValidLcppStatTest, LcpScriptUrlStat) {
  {
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_lcp_script_url_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert(
        {"https://example.com/script.js", 0.1});
    EXPECT_TRUE(IsValidLcppStat(lcpp_stat));
  }
  {  // Without the map field.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_lcp_script_url_stat();
    stat->set_other_bucket_frequency(0.1);
    EXPECT_TRUE(IsValidLcppStat(lcpp_stat));
  }
  {  // Negative other frequency is invalid.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_lcp_script_url_stat();
    stat->set_other_bucket_frequency(-0.1);
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // Negative frequency in an entry is invalid.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_lcp_script_url_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert(
        {"https://example.com/script.js", -0.1});
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // Empty URL in an entry.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_lcp_script_url_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert({"", 0.1});
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // Invalid URL in an entry.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_lcp_script_url_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert({"invalid url", 0.1});
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // No HTTP/HTTPS URL in an entry.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_lcp_script_url_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert({"wss://example.com/", 0.1});
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // Too long URL in an entry.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_lcp_script_url_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert(
        {"https://example.com/" +
             std::string(ResourcePrefetchPredictorTables::kMaxStringLength,
                         'a'),
         0.1});
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
}

TEST(IsValidLcppStatTest, FetchedFontUrlStat) {
  {
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_fetched_font_url_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert({"https://example.com/a.woff", 0.1});
    EXPECT_TRUE(IsValidLcppStat(lcpp_stat));
  }
  {  // Without the map field.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_fetched_font_url_stat();
    stat->set_other_bucket_frequency(0.1);
    EXPECT_TRUE(IsValidLcppStat(lcpp_stat));
  }
  {  // Negative other frequency is invalid.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_fetched_font_url_stat();
    stat->set_other_bucket_frequency(-0.1);
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // Negative frequency in an entry is invalid.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_fetched_font_url_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert({"https://example.com/a.woff", -0.1});
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // Empty URL in an entry.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_fetched_font_url_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert({"", 0.1});
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // Invalid URL in an entry.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_fetched_font_url_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert({"invalid url", 0.1});
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // No HTTP/HTTPS URL in an entry.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_fetched_font_url_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert({"wss://example.com/", 0.1});
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
  {  // Too long URL in an entry.
    LcppStat lcpp_stat;
    auto* stat = lcpp_stat.mutable_fetched_font_url_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert(
        {"https://example.com/" +
             std::string(ResourcePrefetchPredictorTables::kMaxStringLength,
                         'a'),
         0.1});
    EXPECT_FALSE(IsValidLcppStat(lcpp_stat));
  }
}

TEST(IsValidLcppStatTest, MixedPattern) {
  LcppStat lcpp_stat;
  auto* locator_stat = lcpp_stat.mutable_lcp_element_locator_stat();
  locator_stat->set_other_bucket_frequency(0.1);
  {
    auto* bucket = locator_stat->add_lcp_element_locator_buckets();
    bucket->set_lcp_element_locator("fake");
    bucket->set_frequency(0.1);
  }
  {
    auto* stat = lcpp_stat.mutable_lcp_script_url_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert(
        {"https://example.com/script.js", 0.1});
  }
  {
    auto* stat = lcpp_stat.mutable_fetched_font_url_stat();
    stat->set_other_bucket_frequency(0.1);
    stat->mutable_main_buckets()->insert({"https://example.com/a.woff", 0.1});
  }
  EXPECT_TRUE(IsValidLcppStat(lcpp_stat));
}

}  // namespace

}  // namespace predictors
