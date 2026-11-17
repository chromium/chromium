// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_guest_page_load_metrics_observer.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer_delegate.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace glic {
namespace {

using ObservedMark = std::pair<std::string, base::TimeTicks>;

class GlicGuestPageLoadMetricsObserverTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void DispatchCustomMarks(
      std::vector<page_load_metrics::mojom::CustomUserTimingMarkPtr> timings) {
    auto* observer =
        page_load_metrics::MetricsWebContentsObserver::FromWebContents(
            web_contents());
    CHECK(observer);
    auto timing = page_load_metrics::CreatePageLoadTiming();
    timing->navigation_start = base::Time::Now();
    observer->OnTimingUpdated(
        main_rfh(), std::move(timing),
        page_load_metrics::mojom::FrameMetadataPtr(std::in_place), {}, {},
        page_load_metrics::mojom::FrameRenderDataUpdatePtr(std::in_place),
        page_load_metrics::mojom::CpuTimingPtr(std::in_place), {}, std::nullopt,
        {}, {}, std::move(timings),
        page_load_metrics::mojom::FontLoadingMetricsPtr());
  }
};

TEST_F(GlicGuestPageLoadMetricsObserverTest,
       ForwardsBatchedCustomMarksAcrossNavigations) {
  std::vector<ObservedMark> observed_marks;
  ObserveGlicGuestPageLoadMetrics(
      *web_contents(),
      base::BindRepeating(
          [](std::vector<ObservedMark>* out, std::string_view name,
             base::TimeTicks t) { out->emplace_back(std::string(name), t); },
          &observed_marks));

  auto nav1 = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://gemini.google.com/app"), web_contents());
  nav1->Commit();
  const base::TimeTicks nav1_start =
      page_load_metrics::MetricsWebContentsObserver::FromWebContents(
          web_contents())
          ->GetDelegateForCommittedLoad()
          .GetNavigationStart();

  std::vector<page_load_metrics::mojom::CustomUserTimingMarkPtr> batch1;
  batch1.push_back(page_load_metrics::mojom::CustomUserTimingMark::New(
      "gemini.interactive", base::Milliseconds(15)));
  batch1.push_back(page_load_metrics::mojom::CustomUserTimingMark::New(
      "gemini.submittable", base::Milliseconds(40)));
  batch1.push_back(page_load_metrics::mojom::CustomUserTimingMark::New(
      "gemini.custom_future_milestone", base::Milliseconds(65)));
  DispatchCustomMarks(std::move(batch1));

  EXPECT_THAT(
      observed_marks,
      testing::ElementsAre(ObservedMark("gemini.interactive",
                                        nav1_start + base::Milliseconds(15)),
                           ObservedMark("gemini.submittable",
                                        nav1_start + base::Milliseconds(40)),
                           ObservedMark("gemini.custom_future_milestone",
                                        nav1_start + base::Milliseconds(65))));

  // Repeated marks are forwarded directly so callers own per-document gating.
  std::vector<page_load_metrics::mojom::CustomUserTimingMarkPtr> batch2;
  batch2.push_back(page_load_metrics::mojom::CustomUserTimingMark::New(
      "gemini.interactive", base::Milliseconds(70)));
  DispatchCustomMarks(std::move(batch2));

  ASSERT_EQ(observed_marks.size(), 4u);
  EXPECT_EQ(
      observed_marks[3],
      ObservedMark("gemini.interactive", nav1_start + base::Milliseconds(70)));

  // A new primary main-frame navigation resolves marks against the new
  // navigation start timestamp.
  auto nav2 = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://gemini.google.com/app?reload=1"), web_contents());
  nav2->Commit();
  const base::TimeTicks nav2_start =
      page_load_metrics::MetricsWebContentsObserver::FromWebContents(
          web_contents())
          ->GetDelegateForCommittedLoad()
          .GetNavigationStart();

  std::vector<page_load_metrics::mojom::CustomUserTimingMarkPtr> batch3;
  batch3.push_back(page_load_metrics::mojom::CustomUserTimingMark::New(
      "gemini.submittable", base::Milliseconds(12)));
  DispatchCustomMarks(std::move(batch3));

  ASSERT_EQ(observed_marks.size(), 5u);
  EXPECT_EQ(
      observed_marks[4],
      ObservedMark("gemini.submittable", nav2_start + base::Milliseconds(12)));
}

TEST_F(GlicGuestPageLoadMetricsObserverTest,
       ObservesReverseMarkOrderInSingleBatch) {
  std::vector<ObservedMark> observed_marks;
  ObserveGlicGuestPageLoadMetrics(
      *web_contents(),
      base::BindRepeating(
          [](std::vector<ObservedMark>* out, std::string_view name,
             base::TimeTicks t) { out->emplace_back(std::string(name), t); },
          &observed_marks));

  content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://gemini.google.com/app"), web_contents())
      ->Commit();
  const base::TimeTicks nav_start =
      page_load_metrics::MetricsWebContentsObserver::FromWebContents(
          web_contents())
          ->GetDelegateForCommittedLoad()
          .GetNavigationStart();

  std::vector<page_load_metrics::mojom::CustomUserTimingMarkPtr> batch;
  batch.push_back(page_load_metrics::mojom::CustomUserTimingMark::New(
      "gemini.submittable", base::Milliseconds(10)));
  batch.push_back(page_load_metrics::mojom::CustomUserTimingMark::New(
      "gemini.interactive", base::Milliseconds(25)));
  DispatchCustomMarks(std::move(batch));

  EXPECT_THAT(
      observed_marks,
      testing::ElementsAre(ObservedMark("gemini.submittable",
                                        nav_start + base::Milliseconds(10)),
                           ObservedMark("gemini.interactive",
                                        nav_start + base::Milliseconds(25))));
}

TEST_F(GlicGuestPageLoadMetricsObserverTest,
       NoOpWhenMetricsWebContentsObserverAlreadyAttached) {
  int first_count = 0;
  int second_count = 0;
  ObserveGlicGuestPageLoadMetrics(
      *web_contents(), base::BindRepeating([](int* count, std::string_view,
                                              base::TimeTicks) { (*count)++; },
                                           &first_count));
  ObserveGlicGuestPageLoadMetrics(
      *web_contents(), base::BindRepeating([](int* count, std::string_view,
                                              base::TimeTicks) { (*count)++; },
                                           &second_count));

  content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://gemini.google.com/app"), web_contents())
      ->Commit();

  std::vector<page_load_metrics::mojom::CustomUserTimingMarkPtr> batch;
  batch.push_back(page_load_metrics::mojom::CustomUserTimingMark::New(
      "gemini.interactive", base::Milliseconds(10)));
  DispatchCustomMarks(std::move(batch));

  EXPECT_EQ(first_count, 1);
  EXPECT_EQ(second_count, 0);
}

TEST_F(GlicGuestPageLoadMetricsObserverTest,
       SameDocumentNavigationKeepsInitialNavigationStartAnchor) {
  std::vector<ObservedMark> observed_marks;
  ObserveGlicGuestPageLoadMetrics(
      *web_contents(),
      base::BindRepeating(
          [](std::vector<ObservedMark>* out, std::string_view name,
             base::TimeTicks t) { out->emplace_back(std::string(name), t); },
          &observed_marks));

  content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://gemini.google.com/app"), web_contents())
      ->Commit();
  const base::TimeTicks nav_start =
      page_load_metrics::MetricsWebContentsObserver::FromWebContents(
          web_contents())
          ->GetDelegateForCommittedLoad()
          .GetNavigationStart();

  std::vector<page_load_metrics::mojom::CustomUserTimingMarkPtr> batch1;
  batch1.push_back(page_load_metrics::mojom::CustomUserTimingMark::New(
      "gemini.interactive", base::Milliseconds(12)));
  DispatchCustomMarks(std::move(batch1));
  EXPECT_THAT(observed_marks,
              testing::ElementsAre(ObservedMark(
                  "gemini.interactive", nav_start + base::Milliseconds(12))));

  content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://gemini.google.com/app/c/123"), main_rfh())
      ->CommitSameDocument();

  std::vector<page_load_metrics::mojom::CustomUserTimingMarkPtr> batch2;
  batch2.push_back(page_load_metrics::mojom::CustomUserTimingMark::New(
      "gemini.interactive", base::Milliseconds(30)));
  DispatchCustomMarks(std::move(batch2));
  EXPECT_THAT(
      observed_marks,
      testing::ElementsAre(ObservedMark("gemini.interactive",
                                        nav_start + base::Milliseconds(12)),
                           ObservedMark("gemini.interactive",
                                        nav_start + base::Milliseconds(30))));
}

}  // namespace
}  // namespace glic
