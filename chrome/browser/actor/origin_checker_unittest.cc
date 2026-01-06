// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/origin_checker.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace actor {
namespace {

constexpr std::string_view kExample = "https://example.com";
constexpr std::string_view kAnother = "https://another.com";

TEST(OriginCheckerTest, InitialState) {
  const GURL example(kExample);
  OriginChecker origin_checker;
  EXPECT_FALSE(origin_checker.IsNavigationAllowed(url::Origin::Create(example),
                                                  GURL(kAnother)));
  EXPECT_FALSE(origin_checker.IsSensitiveUrlConfirmed(example));
}

TEST(OriginCheckerTest, AllowNavigationToSingleOrigin) {
  const GURL example(kExample);
  OriginChecker origin_checker;
  origin_checker.AllowNavigationTo(url::Origin::Create(example));

  const url::Origin another_origin = url::Origin::Create(GURL(kAnother));
  EXPECT_TRUE(origin_checker.IsNavigationAllowed(another_origin, example));
  EXPECT_FALSE(origin_checker.IsNavigationAllowed(another_origin,
                                                  GURL("http://example.com")));
  EXPECT_FALSE(origin_checker.IsNavigationAllowed(another_origin,
                                                  GURL("https://other.com")));
}

TEST(OriginCheckerTest, AllowNavigationToMultipleOrigins) {
  const GURL example(kExample);
  const GURL foo("https://foo.com");
  const url::Origin another_origin = url::Origin::Create(GURL(kAnother));
  OriginChecker origin_checker;
  origin_checker.AllowNavigationTo({
      url::Origin::Create(example),
      url::Origin::Create(foo),
  });

  EXPECT_TRUE(origin_checker.IsNavigationAllowed(another_origin, example));
  EXPECT_TRUE(origin_checker.IsNavigationAllowed(another_origin, foo));
  EXPECT_FALSE(origin_checker.IsNavigationAllowed(another_origin,
                                                  GURL("https://other.com")));
}

TEST(OriginCheckerTest, IsNavigationAllowed_SameOrigin) {
  const GURL example(kExample);
  OriginChecker origin_checker;

  EXPECT_TRUE(origin_checker.IsNavigationAllowed(url::Origin::Create(example),
                                                 example));
}

TEST(OriginCheckerTest, IsNavigationAllowed_SameSite_Disallowed) {
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {{kGlicCrossOriginNavigationGating,
        {{kGlicNavigationGatingUseSiteNotOrigin.name, "false"}}}},
      {});

  OriginChecker origin_checker;
  origin_checker.AllowNavigationTo(
      url::Origin::Create(GURL("https://subdomain.example.com")));

  EXPECT_FALSE(origin_checker.IsNavigationAllowed(
      url::Origin::Create(GURL(kAnother)), GURL(kExample)));
}

TEST(OriginCheckerTest, IsNavigationAllowed_SameSite_AllowedByFeature) {
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {{kGlicCrossOriginNavigationGating,
        {{kGlicNavigationGatingUseSiteNotOrigin.name, "true"}}}},
      {});

  OriginChecker origin_checker;
  origin_checker.AllowNavigationTo(
      url::Origin::Create(GURL("https://subdomain.example.com")));

  EXPECT_TRUE(origin_checker.IsNavigationAllowed(
      url::Origin::Create(GURL(kAnother)), GURL(kExample)));
}

TEST(OriginCheckerTest, IsNavigationAllowed_OpaqueInitiator) {
  const GURL example(kExample);
  OriginChecker origin_checker;
  origin_checker.AllowNavigationTo(url::Origin::Create(example));

  url::Origin opaque;
  EXPECT_TRUE(origin_checker.IsNavigationAllowed(opaque, example));
  EXPECT_FALSE(origin_checker.IsNavigationAllowed(opaque, GURL(kAnother)));
}

TEST(OriginCheckerTest, IsNavigationAllowed_OmittedInitiator) {
  const GURL example(kExample);
  OriginChecker origin_checker;
  origin_checker.AllowNavigationTo(url::Origin::Create(example));

  EXPECT_TRUE(origin_checker.IsNavigationAllowed(std::nullopt, example));
  EXPECT_FALSE(
      origin_checker.IsNavigationAllowed(std::nullopt, GURL(kAnother)));
}

TEST(OriginCheckerTest, ConfirmSensitiveOrigin) {
  const GURL sensitive("https://sensitive.com");

  OriginChecker origin_checker;
  origin_checker.ConfirmSensitiveOrigin(url::Origin::Create(sensitive));

  EXPECT_TRUE(origin_checker.IsSensitiveUrlConfirmed(sensitive));
  EXPECT_FALSE(origin_checker.IsSensitiveUrlConfirmed(GURL(kAnother)));
}

TEST(OriginCheckerTest, RecordsHistograms) {
  base::HistogramTester histograms;

  const url::Origin example = url::Origin::Create(GURL(kExample));
  const url::Origin another = url::Origin::Create(GURL(kAnother));
  OriginChecker origin_checker;
  origin_checker.AllowNavigationTo({example, another});
  origin_checker.ConfirmSensitiveOrigin(example);
  origin_checker.RecordSizeMetrics();

  histograms.ExpectUniqueSample("Actor.NavigationGating.AllowListSize",
                                /*sample=*/2, /*expected_bucket_count=*/1);
  histograms.ExpectUniqueSample("Actor.NavigationGating.ConfirmedListSize",
                                /*sample=*/1, /*expected_bucket_count=*/1);
}

}  // namespace
}  // namespace actor
