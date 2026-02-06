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
  const url::Origin example = url::Origin::Create(GURL(kExample));
  OriginChecker origin_checker;
  EXPECT_FALSE(origin_checker.IsNavigationAllowed(
      example, url::Origin::Create(GURL(kAnother))));
  EXPECT_FALSE(origin_checker.IsNavigationConfirmedByUser(example));
}

TEST(OriginCheckerTest, AllowNavigationToSingleOrigin) {
  const url::Origin example = url::Origin::Create(GURL(kExample));
  OriginChecker origin_checker;
  origin_checker.AllowNavigationTo(example,
                                   /*is_user_confirmed=*/false);

  const url::Origin another_origin = url::Origin::Create(GURL(kAnother));
  EXPECT_TRUE(origin_checker.IsNavigationAllowed(another_origin, example));
  EXPECT_FALSE(origin_checker.IsNavigationAllowed(
      another_origin, url::Origin::Create(GURL("http://example.com"))));
  EXPECT_FALSE(origin_checker.IsNavigationAllowed(
      another_origin, url::Origin::Create(GURL("https://other.com"))));
}

TEST(OriginCheckerTest, AllowNavigationTo_Opaque) {
  const url::Origin opaque;
  OriginChecker origin_checker;
  origin_checker.AllowNavigationTo(opaque,
                                   /*is_user_confirmed=*/false);

  EXPECT_TRUE(origin_checker.IsNavigationAllowed(std::nullopt, opaque));
  EXPECT_FALSE(origin_checker.IsNavigationAllowed(std::nullopt, url::Origin()));
  EXPECT_FALSE(origin_checker.IsNavigationAllowed(
      opaque, url::Origin::Create(GURL(kExample))));
}

TEST(OriginCheckerTest, AllowNavigationToMultipleOrigins) {
  const url::Origin example = url::Origin::Create(GURL(kExample));
  const url::Origin foo = url::Origin::Create(GURL("https://foo.com"));
  const url::Origin another_origin = url::Origin::Create(GURL(kAnother));
  OriginChecker origin_checker;
  origin_checker.AllowNavigationTo({example, foo});

  EXPECT_TRUE(origin_checker.IsNavigationAllowed(another_origin, example));
  EXPECT_TRUE(origin_checker.IsNavigationAllowed(another_origin, foo));
  EXPECT_FALSE(origin_checker.IsNavigationAllowed(
      another_origin, url::Origin::Create(GURL("https://other.com"))));
}

TEST(OriginCheckerTest, IsNavigationAllowed_SameOrigin) {
  const url::Origin example = url::Origin::Create(GURL(kExample));
  OriginChecker origin_checker;

  EXPECT_TRUE(origin_checker.IsNavigationAllowed(example, example));
}

TEST(OriginCheckerTest, IsNavigationAllowed_SameSite_Disallowed) {
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {{kGlicCrossOriginNavigationGating,
        {{kGlicNavigationGatingUseSiteNotOrigin.name, "false"}}}},
      {});

  OriginChecker origin_checker;
  origin_checker.AllowNavigationTo(
      url::Origin::Create(GURL("https://subdomain.example.com")),
      /*is_user_confirmed=*/false);

  EXPECT_FALSE(
      origin_checker.IsNavigationAllowed(url::Origin::Create(GURL(kAnother)),
                                         url::Origin::Create(GURL(kExample))));
}

TEST(OriginCheckerTest, IsNavigationAllowed_SameSite_AllowedByFeature) {
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {{kGlicCrossOriginNavigationGating,
        {{kGlicNavigationGatingUseSiteNotOrigin.name, "true"}}}},
      {});

  OriginChecker origin_checker;
  origin_checker.AllowNavigationTo(
      url::Origin::Create(GURL("https://subdomain.example.com")),
      /*is_user_confirmed=*/false);

  EXPECT_TRUE(
      origin_checker.IsNavigationAllowed(url::Origin::Create(GURL(kAnother)),
                                         url::Origin::Create(GURL(kExample))));
}

TEST(OriginCheckerTest, IsNavigationAllowed_OpaqueInitiator) {
  const url::Origin example = url::Origin::Create(GURL(kExample));
  OriginChecker origin_checker;
  origin_checker.AllowNavigationTo(example,
                                   /*is_user_confirmed=*/false);

  url::Origin opaque;
  EXPECT_TRUE(origin_checker.IsNavigationAllowed(opaque, example));
  EXPECT_FALSE(origin_checker.IsNavigationAllowed(
      opaque, url::Origin::Create(GURL(kAnother))));
}

TEST(OriginCheckerTest, IsNavigationAllowed_OmittedInitiator) {
  const url::Origin example = url::Origin::Create(GURL(kExample));
  OriginChecker origin_checker;
  origin_checker.AllowNavigationTo(example, /*is_user_confirmed=*/false);

  EXPECT_TRUE(origin_checker.IsNavigationAllowed(std::nullopt, example));
  EXPECT_FALSE(origin_checker.IsNavigationAllowed(
      std::nullopt, url::Origin::Create(GURL(kAnother))));
}

TEST(OriginCheckerTest, ConfirmOrigin_Query) {
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  OriginChecker origin_checker;
  origin_checker.AllowNavigationTo(origin,
                                   /*is_user_confirmed=*/true);

  EXPECT_TRUE(origin_checker.IsNavigationConfirmedByUser(origin));
  EXPECT_FALSE(origin_checker.IsNavigationConfirmedByUser(
      url::Origin::Create(GURL(kAnother))));
}

TEST(OriginCheckerTest, ConfirmOrigin_AllowsNavigation) {
  const url::Origin example = url::Origin::Create(GURL(kExample));

  OriginChecker origin_checker;
  origin_checker.AllowNavigationTo(example,
                                   /*is_user_confirmed=*/true);

  EXPECT_TRUE(origin_checker.IsNavigationAllowed(std::nullopt, example));
}

TEST(OriginCheckerTest, ConfirmOrigin_Opaque) {
  const url::Origin example = url::Origin::Create(GURL(kExample));
  const url::Origin opaque;

  OriginChecker origin_checker;
  origin_checker.AllowNavigationTo(opaque,
                                   /*is_user_confirmed=*/true);

  EXPECT_TRUE(origin_checker.IsNavigationConfirmedByUser(opaque));
  EXPECT_FALSE(origin_checker.IsNavigationConfirmedByUser(url::Origin()));
}

TEST(OriginCheckerTest, ConfirmOrigin_AllowsNavigation_RemembersConfirmation) {
  const url::Origin example = url::Origin::Create(GURL("https://example.com"));

  OriginChecker origin_checker;
  origin_checker.AllowNavigationTo(example,
                                   /*is_user_confirmed=*/false);
  origin_checker.AllowNavigationTo(example,
                                   /*is_user_confirmed=*/true);
  origin_checker.AllowNavigationTo(example,
                                   /*is_user_confirmed=*/false);

  EXPECT_TRUE(origin_checker.IsNavigationConfirmedByUser(example));
}

TEST(OriginCheckerTest, RecordsHistograms) {
  base::HistogramTester histograms;

  const url::Origin example = url::Origin::Create(GURL(kExample));
  const url::Origin another = url::Origin::Create(GURL(kAnother));
  OriginChecker origin_checker;
  origin_checker.AllowNavigationTo(example, /*is_user_confirmed=*/true);
  origin_checker.AllowNavigationTo(another, /*is_user_confirmed=*/false);
  origin_checker.RecordSizeMetrics();

  histograms.ExpectUniqueSample("Actor.NavigationGating.AllowListSize",
                                /*sample=*/2, /*expected_bucket_count=*/1);
  histograms.ExpectUniqueSample("Actor.NavigationGating.ConfirmedListSize2",
                                /*sample=*/1, /*expected_bucket_count=*/1);
}

}  // namespace
}  // namespace actor
