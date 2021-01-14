// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search/ntp_features.h"

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_features {

TEST(NTPFeaturesTest, LocalHistoryRepeatableQueriesAgeThresholdDays) {
  base::test::ScopedFeatureList scoped_feature_list_;

  // The default value can be overridden.
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{kNtpRepeatableQueries,
        {{kNtpRepeatableQueriesAgeThresholdDaysParam, "7"}}}},
      {});
  base::Time age_threshold = GetLocalHistoryRepeatableQueriesAgeThreshold();
  EXPECT_EQ(7, base::TimeDelta(base::Time::Now() - age_threshold).InDays());

  // If the age threshold is not parsable to an unsigned integer, the default
  // value is used.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{kNtpRepeatableQueries,
        {{kNtpRepeatableQueriesAgeThresholdDaysParam, "j"}}}},
      {});
  age_threshold = GetLocalHistoryRepeatableQueriesAgeThreshold();
  EXPECT_EQ(180, base::TimeDelta(base::Time::Now() - age_threshold).InDays());
}

TEST(NTPFeaturesTest, LocalHistoryRepeatableQueriesRecencyDecayUnit) {
  base::test::ScopedFeatureList scoped_feature_list_;

  // The default value can be overridden.
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{kNtpRepeatableQueries,
        {{kNtpRepeatableQueriesRecencyHalfLifeSecondsParam,
          "86400" /* One day */}}}},
      {});
  int recency_decay = GetLocalHistoryRepeatableQueriesRecencyHalfLifeSeconds();
  EXPECT_EQ(86400, recency_decay);

  // If the recency decay unit is not parsable to an unsigned integer, the
  // default value is used.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{kNtpRepeatableQueries,
        {{kNtpRepeatableQueriesRecencyHalfLifeSecondsParam, "j"}}}},
      {});
  recency_decay = GetLocalHistoryRepeatableQueriesRecencyHalfLifeSeconds();
  EXPECT_EQ(604800 /* One week */, recency_decay);
}

TEST(NTPFeaturesTest, LocalHistoryRepeatableQueriesFrequencyExponent) {
  base::test::ScopedFeatureList scoped_feature_list_;

  // The default value can be overridden.
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{kNtpRepeatableQueries,
        {{kNtpRepeatableQueriesFrequencyExponentParam, "1.5"}}}},
      {});
  double frequency_exponent =
      GetLocalHistoryRepeatableQueriesFrequencyExponent();
  EXPECT_EQ(1.5, frequency_exponent);

  // If the recency decay unit is not parsable to an unsigned integer, the
  // default value is used.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{kNtpRepeatableQueries,
        {{kNtpRepeatableQueriesFrequencyExponentParam, "j"}}}},
      {});
  frequency_exponent = GetLocalHistoryRepeatableQueriesFrequencyExponent();
  EXPECT_EQ(2, frequency_exponent);
}

TEST(NTPFeaturesTest, ModulesLoadTimeout) {
  base::test::ScopedFeatureList scoped_feature_list_;

  // The default value can be overridden.
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{kModules, {{kNtpModulesLoadTimeoutMillisecondsParam, "123"}}}}, {});
  base::TimeDelta timeout = GetModulesLoadTimeout();
  EXPECT_EQ(123, timeout.InMilliseconds());

  // If the timeout is not parsable to an unsigned integer, the default value is
  // used.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{kModules, {{kNtpModulesLoadTimeoutMillisecondsParam, "j"}}}}, {});
  timeout = GetModulesLoadTimeout();
  EXPECT_EQ(3, timeout.InSeconds());
}

}  // namespace ntp_features
