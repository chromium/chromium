// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_hints/performance_hints_observer.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/performance_hints/performance_hints_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"

using optimization_guide::OptimizationGuideDecision;
using optimization_guide::proto::PerformanceHint;
using testing::_;
using testing::DoAll;
using testing::Eq;
using testing::NotNull;
using testing::Return;
using testing::SetArgPointee;

namespace performance_hints {
namespace {
const char kPageUrl[] = "http://www.example.com/";
}  // namespace

class MockOptimizationGuideKeyedService : public OptimizationGuideKeyedService {
 public:
  explicit MockOptimizationGuideKeyedService(
      content::BrowserContext* browser_context)
      : OptimizationGuideKeyedService(browser_context) {}
  ~MockOptimizationGuideKeyedService() override = default;

  MOCK_METHOD1(
      RegisterOptimizationTypes,
      void(const std::vector<optimization_guide::proto::OptimizationType>&));
  MOCK_METHOD3(CanApplyOptimization,
               optimization_guide::OptimizationGuideDecision(
                   const GURL& gurl,
                   optimization_guide::proto::OptimizationType,
                   optimization_guide::OptimizationMetadata* metadata));
};

class PerformanceHintsObserverTest : public ChromeRenderViewHostTestHarness {
 public:
  PerformanceHintsObserverTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kPerformanceHintsObserver,
         // Need to enable kOptimizationHints or GetForProfile will return
         // nullptr.
         optimization_guide::features::kOptimizationHints},

        {// Need to disable model downloading for these tests.
         optimization_guide::features::kOptimizationGuideModelDownloading});
  }
  ~PerformanceHintsObserverTest() override = default;

  virtual void SetUpCommandLine() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        optimization_guide::switches::
            kDisableCheckingUserPermissionsForTesting);
  }

  void SetUp() override {
    SetUpCommandLine();

    ChromeRenderViewHostTestHarness::SetUp();
    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();

    mock_optimization_guide_keyed_service_ =
        static_cast<MockOptimizationGuideKeyedService*>(
            OptimizationGuideKeyedServiceFactory::GetInstance()
                ->SetTestingFactoryAndUse(
                    profile(),
                    base::BindRepeating([](content::BrowserContext* context)
                                            -> std::unique_ptr<KeyedService> {
                      return std::make_unique<
                          MockOptimizationGuideKeyedService>(context);
                    })));

    mock_otr_optimization_guide_keyed_service_ =
        static_cast<MockOptimizationGuideKeyedService*>(
            OptimizationGuideKeyedServiceFactory::GetInstance()
                ->SetTestingFactoryAndUse(
                    profile()->GetPrimaryOTRProfile(),
                    base::BindRepeating([](content::BrowserContext* context)
                                            -> std::unique_ptr<KeyedService> {
                      return std::make_unique<
                          MockOptimizationGuideKeyedService>(context);
                    })));

    // By default, all sources will return no hints.
    ON_CALL(*mock_optimization_guide_keyed_service_,
            CanApplyOptimization(
                _, optimization_guide::proto::PERFORMANCE_HINTS, _))
        .WillByDefault(
            Return(optimization_guide::OptimizationGuideDecision::kFalse));
    ON_CALL(
        *mock_optimization_guide_keyed_service_,
        CanApplyOptimization(_, optimization_guide::proto::LINK_PERFORMANCE, _))
        .WillByDefault(
            Return(optimization_guide::OptimizationGuideDecision::kFalse));
    ON_CALL(
        *mock_optimization_guide_keyed_service_,
        CanApplyOptimization(_, optimization_guide::proto::FAST_HOST_HINTS, _))
        .WillByDefault(
            Return(optimization_guide::OptimizationGuideDecision::kFalse));

    test_handle_ = std::make_unique<content::MockNavigationHandle>(
        GURL(kPageUrl), main_rfh());
    std::vector<GURL> redirect_chain;
    redirect_chain.emplace_back(GURL(kPageUrl));
    test_handle_->set_redirect_chain(redirect_chain);
    test_handle_->set_has_committed(true);
    test_handle_->set_is_same_document(false);
    test_handle_->set_is_error_page(false);
  }

  void CallDidFinishNavigation(content::WebContents* web_contents) {
    PerformanceHintsObserver* observer =
        PerformanceHintsObserver::FromWebContents(web_contents);
    observer->DidFinishNavigation(test_handle_.get());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<content::MockNavigationHandle> test_handle_;
  MockOptimizationGuideKeyedService* mock_optimization_guide_keyed_service_ =
      nullptr;
  MockOptimizationGuideKeyedService*
      mock_otr_optimization_guide_keyed_service_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PerformanceHintsObserverTest);
};

TEST_F(PerformanceHintsObserverTest, IncognitoDoesNotRegisterPerformanceHints) {
  std::unique_ptr<content::WebContents> incognito_web_contents(
      content::WebContentsTester::CreateTestWebContents(
          profile()->GetPrimaryOTRProfile(), nullptr));

  EXPECT_CALL(*mock_otr_optimization_guide_keyed_service_,
              RegisterOptimizationTypes(testing::UnorderedElementsAre(
                  optimization_guide::proto::PERFORMANCE_HINTS,
                  optimization_guide::proto::FAST_HOST_HINTS)))
      .Times(0);

  PerformanceHintsObserver::CreateForWebContents(incognito_web_contents.get());
}

TEST_F(PerformanceHintsObserverTest, RegisterPerformanceHints) {
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              RegisterOptimizationTypes(testing::UnorderedElementsAre(
                  optimization_guide::proto::PERFORMANCE_HINTS,
                  optimization_guide::proto::FAST_HOST_HINTS)));

  PerformanceHintsObserver::CreateForWebContents(web_contents());
}

TEST_F(PerformanceHintsObserverTest, LinkHintFound) {
  optimization_guide::proto::PerformanceHintsMetadata hints_metadata;
  auto* hint = hints_metadata.add_performance_hints();
  hint->set_wildcard_pattern("linkhint.com");
  hint->set_performance_class(optimization_guide::proto::PERFORMANCE_SLOW);
  optimization_guide::OptimizationMetadata metadata;
  metadata.set_performance_hints_metadata(hints_metadata);
  ON_CALL(*mock_optimization_guide_keyed_service_,
          CanApplyOptimization(_, optimization_guide::proto::PERFORMANCE_HINTS,
                               NotNull()))
      .WillByDefault(
          DoAll(SetArgPointee<2>(metadata),
                Return(optimization_guide::OptimizationGuideDecision::kTrue)));

  PerformanceHintsObserver::CreateForWebContents(web_contents());
  CallDidFinishNavigation(web_contents());

  base::HistogramTester histogram_tester;

  EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                  web_contents(), GURL("http://www.linkhint.com"),
                  /*record_metrics=*/true),
              Eq(optimization_guide::proto::PERFORMANCE_SLOW));
  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.HintForURLResult", /*kHintFound*/ 3, 1);
  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.SourceLookupStatus.LinkHint", /*kHintFound*/ 3,
      1);
  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.SourceLookupStatus.PageHint",
      /*kNotQueried*/ 0, 1);
  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.SourceLookupStatus.FastHostHint",
      /*kNotQueried*/ 0, 1);
}

TEST_F(PerformanceHintsObserverTest, MultipleLinkHints) {
  optimization_guide::proto::PerformanceHintsMetadata hints_metadata;
  auto* hint = hints_metadata.add_performance_hints();
  hint->set_wildcard_pattern("test.com");
  hint->set_performance_class(optimization_guide::proto::PERFORMANCE_SLOW);
  hint = hints_metadata.add_performance_hints();
  hint->set_wildcard_pattern("othersite.net");
  hint->set_performance_class(optimization_guide::proto::PERFORMANCE_FAST);
  optimization_guide::OptimizationMetadata metadata;
  metadata.set_performance_hints_metadata(hints_metadata);
  ON_CALL(*mock_optimization_guide_keyed_service_,
          CanApplyOptimization(_, optimization_guide::proto::PERFORMANCE_HINTS,
                               NotNull()))
      .WillByDefault(
          DoAll(SetArgPointee<2>(metadata),
                Return(optimization_guide::OptimizationGuideDecision::kTrue)));

  PerformanceHintsObserver::CreateForWebContents(web_contents());
  CallDidFinishNavigation(web_contents());

  base::HistogramTester histogram_tester;

  EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                  web_contents(), GURL("http://www.test.com"),
                  /*record_metrics=*/true),
              Eq(optimization_guide::proto::PERFORMANCE_SLOW));
  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.HintForURLResult", /*kHintFound*/ 3, 1);
  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.PerformanceClassForURL", /*kSlow*/ 1, 1);
  EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                  web_contents(), GURL("https://www.othersite.net/this/link"),
                  /*record_metrics=*/true),
              Eq(optimization_guide::proto::PERFORMANCE_FAST));
  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.HintForURLResult", /*kHintFound*/ 3, 2);
  histogram_tester.ExpectBucketCount(
      "PerformanceHints.Observer.PerformanceClassForURL", /*kFast*/ 2, 1);
  EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                  web_contents(), GURL("https://www.nohint.com"),
                  /*record_metrics=*/true),
              Eq(optimization_guide::proto::PERFORMANCE_UNKNOWN));
  histogram_tester.ExpectBucketCount(
      "PerformanceHints.Observer.HintForURLResult", /*kHintNotFound*/ 0, 1);
  histogram_tester.ExpectBucketCount(
      "PerformanceHints.Observer.PerformanceClassForURL", /*kUnknown*/ 0, 1);
}

TEST_F(PerformanceHintsObserverTest, NoLinkHints) {
  optimization_guide::OptimizationMetadata metadata;
  metadata.set_performance_hints_metadata(
      optimization_guide::proto::PerformanceHintsMetadata());
  ON_CALL(*mock_optimization_guide_keyed_service_,
          CanApplyOptimization(_, optimization_guide::proto::PERFORMANCE_HINTS,
                               NotNull()))
      .WillByDefault(
          DoAll(SetArgPointee<2>(metadata),
                Return(optimization_guide::OptimizationGuideDecision::kTrue)));

  PerformanceHintsObserver::CreateForWebContents(web_contents());
  CallDidFinishNavigation(web_contents());

  base::HistogramTester histogram_tester;

  EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                  web_contents(), GURL("https://www.nohint.com"),
                  /*record_metrics=*/true),
              Eq(optimization_guide::proto::PERFORMANCE_UNKNOWN));

  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.HintForURLResult", /*kHintNotFound*/ 0, 1);
}

class LinkPerformanceHintsEnabledPerformanceHintsObserverTest
    : public PerformanceHintsObserverTest {
 public:
  LinkPerformanceHintsEnabledPerformanceHintsObserverTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPerformanceHintsObserver,
          {{"use_link_performance_hints", "true"}}},
         // Need to enable kOptimizationHints or GetForProfile will return
         // nullptr.
         {optimization_guide::features::kOptimizationHints, {}}},
        {});
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(LinkPerformanceHintsEnabledPerformanceHintsObserverTest,
       RegisterLinkPerformance) {
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              RegisterOptimizationTypes(testing::UnorderedElementsAre(
                  optimization_guide::proto::PERFORMANCE_HINTS,
                  optimization_guide::proto::LINK_PERFORMANCE,
                  optimization_guide::proto::FAST_HOST_HINTS)));

  PerformanceHintsObserver::CreateForWebContents(web_contents());
}

TEST_F(LinkPerformanceHintsEnabledPerformanceHintsObserverTest,
       LinkPerformanceHints) {
  optimization_guide::proto::LinkPerformanceMetadata link_metadata;
  auto* hint = link_metadata.add_link_hints();
  hint->set_wildcard_pattern("test.com");
  hint->set_performance_class(optimization_guide::proto::PERFORMANCE_SLOW);
  hint = link_metadata.add_link_hints();
  hint->set_wildcard_pattern("othersite.net");
  hint->set_performance_class(optimization_guide::proto::PERFORMANCE_FAST);

  optimization_guide::OptimizationMetadata metadata;
  metadata.SetAnyMetadataForTesting(link_metadata);

  ON_CALL(*mock_optimization_guide_keyed_service_,
          CanApplyOptimization(GURL(kPageUrl),
                               optimization_guide::proto::LINK_PERFORMANCE,
                               NotNull()))
      .WillByDefault(
          DoAll(SetArgPointee<2>(metadata),
                Return(optimization_guide::OptimizationGuideDecision::kTrue)));

  PerformanceHintsObserver::CreateForWebContents(web_contents());
  CallDidFinishNavigation(web_contents());

  base::HistogramTester histogram_tester;

  EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                  web_contents(), GURL("http://www.test.com"),
                  /*record_metrics=*/true),
              Eq(optimization_guide::proto::PERFORMANCE_SLOW));
  EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                  web_contents(), GURL("https://www.othersite.net/this/link"),
                  /*record_metrics=*/true),
              Eq(optimization_guide::proto::PERFORMANCE_FAST));
  EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                  web_contents(), GURL("https://www.nohint.com"),
                  /*record_metrics=*/true),
              Eq(optimization_guide::proto::PERFORMANCE_UNKNOWN));
}

TEST_F(PerformanceHintsObserverTest, PageHintFound) {
  PerformanceHintsObserver::CreateForWebContents(web_contents());
  CallDidFinishNavigation(web_contents());

  base::HistogramTester histogram_tester;

  optimization_guide::OptimizationMetadata metadata;
  optimization_guide::proto::PerformanceHintsMetadata hints_metadata;
  hints_metadata.mutable_page_hint()->set_performance_class(
      optimization_guide::proto::PERFORMANCE_NORMAL);
  metadata.set_performance_hints_metadata(hints_metadata);
  ON_CALL(*mock_optimization_guide_keyed_service_,
          CanApplyOptimization(GURL("https://www.pagehint.com"),
                               optimization_guide::proto::PERFORMANCE_HINTS,
                               NotNull()))
      .WillByDefault(
          DoAll(SetArgPointee<2>(metadata),
                Return(optimization_guide::OptimizationGuideDecision::kTrue)));

  EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                  web_contents(), GURL("https://www.pagehint.com"),
                  /*record_metrics=*/true),
              Eq(optimization_guide::proto::PERFORMANCE_NORMAL));
  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.HintForURLResult", /*kHintFound*/ 3, 1);
  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.SourceLookupStatus.LinkHint", /*kNoMatch*/ 2,
      1);
  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.SourceLookupStatus.PageHint", /*kHintFound*/ 3,
      1);
  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.SourceLookupStatus.FastHostHint",
      /*kNotQueried*/ 0, 1);
}

TEST_F(PerformanceHintsObserverTest, PageHintNotReady) {
  PerformanceHintsObserver::CreateForWebContents(web_contents());
  CallDidFinishNavigation(web_contents());

  base::HistogramTester histogram_tester;

  optimization_guide::OptimizationMetadata metadata;
  optimization_guide::proto::PerformanceHintsMetadata hints_metadata;
  hints_metadata.mutable_page_hint()->set_performance_class(
      optimization_guide::proto::PERFORMANCE_NORMAL);
  metadata.set_performance_hints_metadata(hints_metadata);
  ON_CALL(*mock_optimization_guide_keyed_service_,
          CanApplyOptimization(GURL("https://www.pagehint.com"),
                               optimization_guide::proto::PERFORMANCE_HINTS,
                               NotNull()))
      .WillByDefault(DoAll(
          SetArgPointee<2>(metadata),
          Return(optimization_guide::OptimizationGuideDecision::kUnknown)));

  EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                  web_contents(), GURL("https://www.pagehint.com"),
                  /*record_metrics=*/true),
              Eq(optimization_guide::proto::PERFORMANCE_UNKNOWN));
  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.SourceLookupStatus.PageHint", /*kNotReady*/ 1,
      1);
}

TEST_F(PerformanceHintsObserverTest, FastHostHintFound) {
  PerformanceHintsObserver::CreateForWebContents(web_contents());
  CallDidFinishNavigation(web_contents());

  base::HistogramTester histogram_tester;

  ON_CALL(*mock_optimization_guide_keyed_service_,
          CanApplyOptimization(GURL("https://www.fastfilter.com"),
                               optimization_guide::proto::FAST_HOST_HINTS, _))
      .WillByDefault(
          Return(optimization_guide::OptimizationGuideDecision::kTrue));

  EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                  web_contents(), GURL("https://www.fastfilter.com"),
                  /*record_metrics=*/true),
              Eq(optimization_guide::proto::PERFORMANCE_FAST));
  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.HintForURLResult", /*kHintFound*/ 3, 1);
  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.SourceLookupStatus.LinkHint", /*kNoMatch*/ 2,
      1);
  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.SourceLookupStatus.PageHint", /*kNoMatch*/ 2,
      1);
  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.SourceLookupStatus.FastHostHint",
      /*kHintFound*/ 3, 1);
}

TEST_F(PerformanceHintsObserverTest, FastHostHintNotReady) {
  PerformanceHintsObserver::CreateForWebContents(web_contents());
  CallDidFinishNavigation(web_contents());

  base::HistogramTester histogram_tester;

  ON_CALL(*mock_optimization_guide_keyed_service_,
          CanApplyOptimization(GURL("https://www.fastfilter.com"),
                               optimization_guide::proto::FAST_HOST_HINTS, _))
      .WillByDefault(
          Return(optimization_guide::OptimizationGuideDecision::kUnknown));

  EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                  web_contents(), GURL("https://www.fastfilter.com"),
                  /*record_metrics=*/true),
              Eq(optimization_guide::proto::PERFORMANCE_UNKNOWN));
  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.SourceLookupStatus.FastHostHint",
      /*kNotReady*/ 1, 1);
}

class FastHostHintsDisabledPerformanceHintsObserverTest
    : public PerformanceHintsObserverTest {
 public:
  FastHostHintsDisabledPerformanceHintsObserverTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPerformanceHintsObserver,
          {{"use_fast_host_hints", "false"}}},
         // Need to enable kOptimizationHints or GetForProfile will return
         // nullptr.
         {optimization_guide::features::kOptimizationHints, {}}},
        {});
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(FastHostHintsDisabledPerformanceHintsObserverTest,
       FastHostHintsDisabled) {
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              RegisterOptimizationTypes(testing::UnorderedElementsAre(
                  optimization_guide::proto::PERFORMANCE_HINTS)));

  PerformanceHintsObserver::CreateForWebContents(web_contents());
  CallDidFinishNavigation(web_contents());

  base::HistogramTester histogram_tester;

  EXPECT_CALL(
      *mock_optimization_guide_keyed_service_,
      CanApplyOptimization(GURL(kPageUrl),
                           optimization_guide::proto::PERFORMANCE_HINTS, _));
  EXPECT_CALL(
      *mock_optimization_guide_keyed_service_,
      CanApplyOptimization(GURL("https://www.fastfilter.com"),
                           optimization_guide::proto::PERFORMANCE_HINTS, _));
  EXPECT_CALL(
      *mock_optimization_guide_keyed_service_,
      CanApplyOptimization(GURL("https://www.fastfilter.com"),
                           optimization_guide::proto::FAST_HOST_HINTS, _))
      .Times(0);

  EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                  web_contents(), GURL("https://www.fastfilter.com"),
                  /*record_metrics=*/true),
              Eq(optimization_guide::proto::PERFORMANCE_UNKNOWN));
  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.HintForURLResult", /*kHintNotFound*/ 0, 1);
  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.SourceLookupStatus.LinkHint", /*kNoMatch*/ 2,
      1);
  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.SourceLookupStatus.PageHint", /*kNoMatch*/ 2,
      1);
  histogram_tester.ExpectTotalCount(
      "PerformanceHints.Observer.SourceLookupStatus.FastHostHint", 0);
}

// If any sources are NotReady and the rest are NotFound, NotReady should be
// returned.
TEST_F(PerformanceHintsObserverTest, SomeSourcesNotReady) {
  PerformanceHintsObserver::CreateForWebContents(web_contents());
  CallDidFinishNavigation(web_contents());

  ON_CALL(
      *mock_optimization_guide_keyed_service_,
      CanApplyOptimization(_, optimization_guide::proto::PERFORMANCE_HINTS, _))
      .WillByDefault(
          Return(optimization_guide::OptimizationGuideDecision::kUnknown));

  base::HistogramTester histogram_tester;
  EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                  web_contents(), GURL("https://www.nohint.com"),
                  /*record_metrics=*/true),
              Eq(optimization_guide::proto::PERFORMANCE_UNKNOWN));
  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.HintForURLResult", /*kHintNotReady*/ 1, 1);
}

TEST_F(PerformanceHintsObserverTest, RewrittenUrl) {
  optimization_guide::proto::PerformanceHintsMetadata hints_metadata;
  auto* hint = hints_metadata.add_performance_hints();
  hint->set_wildcard_pattern("www.google.com");
  hint->set_performance_class(optimization_guide::proto::PERFORMANCE_FAST);
  hint = hints_metadata.add_performance_hints();
  hint->set_wildcard_pattern("www.test.com");
  hint->set_performance_class(optimization_guide::proto::PERFORMANCE_SLOW);
  optimization_guide::OptimizationMetadata metadata;
  metadata.set_performance_hints_metadata(hints_metadata);
  ON_CALL(*mock_optimization_guide_keyed_service_,
          CanApplyOptimization(_, optimization_guide::proto::PERFORMANCE_HINTS,
                               NotNull()))
      .WillByDefault(
          DoAll(SetArgPointee<2>(metadata),
                Return(optimization_guide::OptimizationGuideDecision::kTrue)));

  PerformanceHintsObserver::CreateForWebContents(web_contents());
  CallDidFinishNavigation(web_contents());

  base::HistogramTester histogram_tester;

  // There is a FAST hint that should match all www.google.com URLs, but not
  // those that fit the redirect pattern.
  EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                  web_contents(),
                  GURL("https://www.google.com/url?url=http://www.test.com"),
                  /*record_metrics=*/true),
              Eq(optimization_guide::proto::PERFORMANCE_SLOW));
  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.HintForURLResult",
      /*kHintFound*/ 3, 1);
  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.HintForURLResult.Rewritten",
      /*kHintFound*/ 3, 1);

  EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                  web_contents(), GURL("https://www.google.com/other/link"),
                  /*record_metrics=*/true),
              Eq(optimization_guide::proto::PERFORMANCE_FAST));
  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.HintForURLResult",
      /*kHintFound*/ 3, 2);
  // Still only one sample in .Rewritten.
  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.HintForURLResult.Rewritten",
      /*kHintFound*/ 3, 1);

  EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                  web_contents(),
                  GURL("https://www.google.com/url?url=https://www.nohint.com"),
                  /*record_metrics=*/true),
              Eq(optimization_guide::proto::PERFORMANCE_UNKNOWN));
  histogram_tester.ExpectBucketCount(
      "PerformanceHints.Observer.HintForURLResult",
      /*kHintNotFound*/ 0, 1);
  histogram_tester.ExpectBucketCount(
      "PerformanceHints.Observer.HintForURLResult.Rewritten",
      /*kHintNotFound*/ 0, 1);

  EXPECT_THAT(
      PerformanceHintsObserver::PerformanceClassForURL(
          web_contents(), GURL("https://www.google.com/url?url=notaurl"),
          /*record_metrics=*/true),
      Eq(optimization_guide::proto::PERFORMANCE_UNKNOWN));
  histogram_tester.ExpectBucketCount(
      "PerformanceHints.Observer.HintForURLResult", /*kInvalidURL*/ 2, 1);
  histogram_tester.ExpectBucketCount(
      "PerformanceHints.Observer.HintForURLResult.Rewritten", /*kInvalidURL*/ 2,
      1);
}

class RewritesDisabledPerformanceHintsObserverTest
    : public PerformanceHintsObserverTest {
 public:
  RewritesDisabledPerformanceHintsObserverTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kPerformanceHintsObserver,
         // Need to enable kOptimizationHints or GetForProfile will return
         // nullptr.
         optimization_guide::features::kOptimizationHints},
        {features::kPerformanceHintsHandleRewrites});
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(RewritesDisabledPerformanceHintsObserverTest, RewritesDisabled) {
  optimization_guide::proto::PerformanceHintsMetadata hints_metadata;
  auto* hint = hints_metadata.add_performance_hints();
  hint->set_wildcard_pattern("www.google.com");
  hint->set_performance_class(optimization_guide::proto::PERFORMANCE_FAST);
  hint = hints_metadata.add_performance_hints();
  hint->set_wildcard_pattern("www.test.com");
  hint->set_performance_class(optimization_guide::proto::PERFORMANCE_SLOW);
  optimization_guide::OptimizationMetadata metadata;
  metadata.set_performance_hints_metadata(hints_metadata);
  ON_CALL(*mock_optimization_guide_keyed_service_,
          CanApplyOptimization(_, optimization_guide::proto::PERFORMANCE_HINTS,
                               NotNull()))
      .WillByDefault(
          DoAll(SetArgPointee<2>(metadata),
                Return(optimization_guide::OptimizationGuideDecision::kTrue)));

  PerformanceHintsObserver::CreateForWebContents(web_contents());
  CallDidFinishNavigation(web_contents());

  base::HistogramTester histogram_tester;

  // The rewrite handling should not happen here and the link should be badged
  // as though it were google.com.
  EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                  web_contents(),
                  GURL("https://www.google.com/url?url=http://www.test.com"),
                  /*record_metrics=*/true),
              Eq(optimization_guide::proto::PERFORMANCE_FAST));
  histogram_tester.ExpectTotalCount(
      "PerformanceHints.Observer.HintForURLResult.Rewritten", 0);
}

TEST_F(PerformanceHintsObserverTest, InvalidURL) {
  PerformanceHintsObserver::CreateForWebContents(web_contents());
  CallDidFinishNavigation(web_contents());

  base::HistogramTester histogram_tester;

  EXPECT_THAT(
      PerformanceHintsObserver::PerformanceClassForURL(web_contents(), GURL(""),
                                                       /*record_metrics=*/true),
      Eq(optimization_guide::proto::PERFORMANCE_UNKNOWN));
  EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                  web_contents(), GURL("ftp://notsupported.com"),
                  /*record_metrics=*/true),
              Eq(optimization_guide::proto::PERFORMANCE_UNKNOWN));

  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.HintForURLResult", /*kInvalidUrl*/ 2, 2);
}

TEST_F(PerformanceHintsObserverTest, NoHints) {
  PerformanceHintsObserver::CreateForWebContents(web_contents());
  CallDidFinishNavigation(web_contents());

  base::HistogramTester histogram_tester;

  EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                  web_contents(), GURL("https://www.nohint.com"),
                  /*record_metrics=*/true),
              Eq(optimization_guide::proto::PERFORMANCE_UNKNOWN));

  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.HintForURLResult", /*kHintNotFound*/ 0, 1);
  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.SourceLookupStatus.LinkHint", /*kNoMatch*/ 2,
      1);
  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.SourceLookupStatus.PageHint", /*kNoMatch*/ 2,
      1);
  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.SourceLookupStatus.FastHostHint",
      /*kNoMatch*/ 2, 1);
}

TEST_F(PerformanceHintsObserverTest, DontMatchQueryParams) {
  optimization_guide::proto::PerformanceHintsMetadata hints_metadata;
  auto* hint = hints_metadata.add_performance_hints();
  hint->set_wildcard_pattern("www.test.com");
  hint->set_performance_class(optimization_guide::proto::PERFORMANCE_SLOW);
  hint = hints_metadata.add_performance_hints();
  hint->set_wildcard_pattern("www.google.com");
  hint->set_performance_class(optimization_guide::proto::PERFORMANCE_FAST);
  optimization_guide::OptimizationMetadata metadata;
  metadata.set_performance_hints_metadata(hints_metadata);
  ON_CALL(*mock_optimization_guide_keyed_service_,
          CanApplyOptimization(_, optimization_guide::proto::PERFORMANCE_HINTS,
                               NotNull()))
      .WillByDefault(
          DoAll(SetArgPointee<2>(metadata),
                Return(optimization_guide::OptimizationGuideDecision::kTrue)));

  PerformanceHintsObserver::CreateForWebContents(web_contents());
  CallDidFinishNavigation(web_contents());

  base::HistogramTester histogram_tester;

  EXPECT_THAT(
      PerformanceHintsObserver::PerformanceClassForURL(
          web_contents(), GURL("http://www.google.com/search?q=www.test.com"),
          /*record_metrics=*/true),
      Eq(optimization_guide::proto::PERFORMANCE_FAST));
}

TEST_F(PerformanceHintsObserverTest, LinkHintsNotReady) {
  ON_CALL(*mock_optimization_guide_keyed_service_,
          CanApplyOptimization(_, optimization_guide::proto::PERFORMANCE_HINTS,
                               NotNull()))
      .WillByDefault(
          Return(optimization_guide::OptimizationGuideDecision::kUnknown));

  PerformanceHintsObserver::CreateForWebContents(web_contents());
  CallDidFinishNavigation(web_contents());

  base::HistogramTester histogram_tester;

  EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                  web_contents(), GURL("https://www.nohint.com"),
                  /*record_metrics=*/true),
              Eq(optimization_guide::proto::PERFORMANCE_UNKNOWN));

  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.HintForURLResult", /*kHintNotReady*/ 1, 1);
  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.SourceLookupStatus.LinkHint", /*kNotReady*/ 1,
      1);
}

TEST_F(PerformanceHintsObserverTest, CacheLinkHints) {
  PerformanceHintsObserver::CreateForWebContents(web_contents());
  CallDidFinishNavigation(web_contents());

  optimization_guide::proto::PerformanceHintsMetadata hints_metadata;
  auto* hint = hints_metadata.add_performance_hints();
  hint->set_wildcard_pattern("hint.com");
  hint->set_performance_class(optimization_guide::proto::PERFORMANCE_FAST);
  optimization_guide::OptimizationMetadata metadata;
  metadata.set_performance_hints_metadata(hints_metadata);
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              CanApplyOptimization(GURL(kPageUrl),
                                   optimization_guide::proto::PERFORMANCE_HINTS,
                                   NotNull()))
      .WillOnce(
          DoAll(SetArgPointee<2>(metadata),
                Return(optimization_guide::OptimizationGuideDecision::kTrue)));

  base::HistogramTester histogram_tester;

  EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                  web_contents(), GURL("https://www.hint.com"),
                  /*record_metrics=*/true),
              Eq(optimization_guide::proto::PERFORMANCE_FAST));

  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.HintForURLResult",
      /*kHintFound*/ 3, 1);

  // No further calls should be made to OptimizationGuide, just use the cached
  // link hints for the current page.
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              CanApplyOptimization(GURL(kPageUrl),
                                   optimization_guide::proto::PERFORMANCE_HINTS,
                                   NotNull()))
      .Times(0);

  EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                  web_contents(), GURL("https://www.hint.com"),
                  /*record_metrics=*/true),
              Eq(optimization_guide::proto::PERFORMANCE_FAST));

  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.HintForURLResult",
      /*kHintFound*/ 3, 2);
}

TEST_F(PerformanceHintsObserverTest, ResetObserverForNextNavigation) {
  PerformanceHintsObserver::CreateForWebContents(web_contents());
  CallDidFinishNavigation(web_contents());

  {
    base::HistogramTester histogram_tester;

    EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                    web_contents(), GURL("https://www.hint.com"),
                    /*record_metrics=*/true),
                Eq(optimization_guide::proto::PERFORMANCE_UNKNOWN));

    // Hints will not be returned in the default case.
    histogram_tester.ExpectUniqueSample(
        "PerformanceHints.Observer.HintForURLResult", /*kHintNotFound*/ 0, 1);
  }

  // Set up the next call to return hints for the page.
  optimization_guide::proto::PerformanceHintsMetadata hints_metadata;
  auto* hint = hints_metadata.add_performance_hints();
  hint->set_wildcard_pattern("hint.com");
  hint->set_performance_class(optimization_guide::proto::PERFORMANCE_FAST);
  optimization_guide::OptimizationMetadata metadata;
  metadata.set_performance_hints_metadata(hints_metadata);
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              CanApplyOptimization(GURL(kPageUrl),
                                   optimization_guide::proto::PERFORMANCE_HINTS,
                                   NotNull()))
      .WillOnce(
          DoAll(SetArgPointee<2>(metadata),
                Return(optimization_guide::OptimizationGuideDecision::kTrue)));

  {
    base::HistogramTester histogram_tester;

    // Simulate navigation to another page.
    CallDidFinishNavigation(web_contents());

    EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                    web_contents(), GURL("https://www.hint.com"),
                    /*record_metrics=*/true),
                Eq(optimization_guide::proto::PERFORMANCE_FAST));

    histogram_tester.ExpectUniqueSample(
        "PerformanceHints.Observer.HintForURLResult",
        /*kHintFound*/ 3, 1);
  }
}

TEST_F(PerformanceHintsObserverTest, OptimizationGuideDisabled) {
  mock_optimization_guide_keyed_service_ = nullptr;
  OptimizationGuideKeyedServiceFactory::GetInstance()->SetTestingFactory(
      profile(), OptimizationGuideKeyedServiceFactory::TestingFactory());

  PerformanceHintsObserver::CreateForWebContents(web_contents());
  CallDidFinishNavigation(web_contents());

  EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                  web_contents(), GURL("http://www.test.com"),
                  /*record_metrics=*/true),
              Eq(optimization_guide::proto::PERFORMANCE_UNKNOWN));
}

TEST_F(PerformanceHintsObserverTest, NoErrorPageHints) {
  test_handle_->set_is_error_page(true);

  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              CanApplyOptimization(_, _, _))
      .Times(0);

  PerformanceHintsObserver::CreateForWebContents(web_contents());
  CallDidFinishNavigation(web_contents());

  EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                  web_contents(), GURL("http://www.test.com"),
                  /*record_metrics=*/true),
              Eq(optimization_guide::proto::PERFORMANCE_UNKNOWN));
}

TEST_F(PerformanceHintsObserverTest, DontFetchForSubframe) {
  test_handle_ = std::make_unique<content::MockNavigationHandle>(
      GURL(kPageUrl),
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe"));
  std::vector<GURL> redirect_chain;
  redirect_chain.emplace_back(GURL(kPageUrl));
  test_handle_->set_redirect_chain(redirect_chain);
  test_handle_->set_has_committed(true);
  test_handle_->set_is_same_document(false);
  test_handle_->set_is_error_page(false);

  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              CanApplyOptimization(_, _, _))
      .Times(0);

  PerformanceHintsObserver::CreateForWebContents(web_contents());
  CallDidFinishNavigation(web_contents());

  EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                  web_contents(), GURL(kPageUrl),
                  /*record_metrics=*/true),
              Eq(optimization_guide::proto::PERFORMANCE_UNKNOWN));
}

class OverrideUnknownPerformanceHintsObserverTest
    : public PerformanceHintsObserverTest {
 public:
  OverrideUnknownPerformanceHintsObserverTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kPerformanceHintsObserver,
         features::kPerformanceHintsTreatUnknownAsFast,
         // Need to enable kOptimizationHints or GetForProfile will return
         // nullptr.
         optimization_guide::features::kOptimizationHints},
        {});
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

// When kPerformanceHintsTreatUnknownAsFast is enabled, URLs that have UNKNOWN
// performance class are given the "benefit of the doubt" and are classed as
// FAST.
TEST_F(OverrideUnknownPerformanceHintsObserverTest,
       OverrideUnknownPerformanceToFast) {
  optimization_guide::proto::PerformanceHintsMetadata hints_metadata;
  auto* hint = hints_metadata.add_performance_hints();
  hint->set_wildcard_pattern("test.com");
  hint->set_performance_class(optimization_guide::proto::PERFORMANCE_SLOW);
  hint = hints_metadata.add_performance_hints();
  hint->set_wildcard_pattern("fastsite.org");
  hint->set_performance_class(optimization_guide::proto::PERFORMANCE_FAST);
  hint = hints_metadata.add_performance_hints();
  hint->set_wildcard_pattern("othersite.net");
  hint->set_performance_class(optimization_guide::proto::PERFORMANCE_UNKNOWN);
  optimization_guide::OptimizationMetadata metadata;
  metadata.set_performance_hints_metadata(hints_metadata);
  ON_CALL(*mock_optimization_guide_keyed_service_,
          CanApplyOptimization(_, optimization_guide::proto::PERFORMANCE_HINTS,
                               NotNull()))
      .WillByDefault(
          DoAll(SetArgPointee<2>(metadata),
                Return(optimization_guide::OptimizationGuideDecision::kTrue)));

  PerformanceHintsObserver::CreateForWebContents(web_contents());
  CallDidFinishNavigation(web_contents());

  base::HistogramTester histogram_tester;

  EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                  web_contents(), GURL("http://www.test.com"),
                  /*record_metrics=*/true),
              Eq(optimization_guide::proto::PERFORMANCE_SLOW));
  histogram_tester.ExpectBucketCount(
      "PerformanceHints.Observer.PerformanceClassForURL", /*kSlow*/ 1, 1);
  EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                  web_contents(), GURL("http://www.fastsite.org"),
                  /*record_metrics=*/true),
              Eq(optimization_guide::proto::PERFORMANCE_FAST));
  histogram_tester.ExpectBucketCount(
      "PerformanceHints.Observer.PerformanceClassForURL", /*kFast*/ 2, 1);
  EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                  web_contents(), GURL("https://www.othersite.net/this/link"),
                  /*record_metrics=*/true),
              Eq(optimization_guide::proto::PERFORMANCE_FAST));
  histogram_tester.ExpectBucketCount(
      "PerformanceHints.Observer.PerformanceClassForURL", /*kUnknown*/ 0, 1);
  EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                  web_contents(), GURL("https://www.nohint.com"),
                  /*record_metrics=*/true),
              Eq(optimization_guide::proto::PERFORMANCE_FAST));
  histogram_tester.ExpectBucketCount(
      "PerformanceHints.Observer.PerformanceClassForURL", /*kUnknown*/ 0, 2);
}

class OverrideUnknownPerformanceHintsObserverFetchingNotEnabledTest
    : public OverrideUnknownPerformanceHintsObserverTest {
 public:
  void SetUpCommandLine() override {
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        optimization_guide::switches::
            kDisableCheckingUserPermissionsForTesting);
  }
};

// Uses OverrideUnknownPerformanceHintsObserverTest to ensure
// PERFORMANCE_UNKNOWN is not overridden to FAST when fetching is disabled.
TEST_F(OverrideUnknownPerformanceHintsObserverFetchingNotEnabledTest,
       HintFetchingNotEnabled) {
  PerformanceHintsObserver::CreateForWebContents(web_contents());
  CallDidFinishNavigation(web_contents());

  EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                  web_contents(), GURL("http://www.test.com"),
                  /*record_metrics=*/true),
              Eq(optimization_guide::proto::PERFORMANCE_UNKNOWN));
}

}  // namespace performance_hints
