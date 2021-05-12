// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/ukm_background_recorder_service.h"

#include "base/optional.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/test/browser_test.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

// TODO(https://crbug.com/1042727): Fix test GURL scoping and remove this getter
// function.
url::Origin VisitedOrigin() {
  return url::Origin::Create(GURL("https://foobar.com"));
}

void DidGetRecordResult(base::OnceClosure quit_closure,
                        base::Optional<ukm::SourceId>* out_result,
                        base::Optional<ukm::SourceId> result) {
  *out_result = std::move(result);
  std::move(quit_closure).Run();
}

}  // namespace

class UkmBackgroundRecorderBrowserTest : public InProcessBrowserTest {
 public:
  UkmBackgroundRecorderBrowserTest() = default;
  ~UkmBackgroundRecorderBrowserTest() override = default;

  void SetUpOnMainThread() override {
    background_recorder_service_ =
        ukm::UkmBackgroundRecorderFactory::GetForProfile(browser()->profile());
    DCHECK(background_recorder_service_);

    // Adds the URL to the history so that UKM events for this origin are
    // recorded.
    background_recorder_service_->history_service_->AddPage(
        GURL(VisitedOrigin().GetURL().spec() + "baz"), base::Time::Now(),
        history::SOURCE_BROWSED);
  }

 protected:
  base::Optional<ukm::SourceId> GetSourceId(const url::Origin& origin) {
    base::Optional<ukm::SourceId> result;

    base::RunLoop run_loop;
    background_recorder_service_->GetBackgroundSourceIdIfAllowed(
        origin,
        base::BindOnce(&DidGetRecordResult, run_loop.QuitClosure(), &result));
    run_loop.Run();

    return result;
  }

 private:
  ukm::UkmBackgroundRecorderService* background_recorder_service_;

  DISALLOW_COPY_AND_ASSIGN(UkmBackgroundRecorderBrowserTest);
};

IN_PROC_BROWSER_TEST_F(UkmBackgroundRecorderBrowserTest,
                       SourceIdReturnedWhenOriginInHistory) {
  // Check visited origin.
  {
    auto source_id = GetSourceId(VisitedOrigin());
    ASSERT_TRUE(source_id);
    EXPECT_NE(*source_id, ukm::kInvalidSourceId);
    EXPECT_EQ(ukm::GetSourceIdType(*source_id), ukm::SourceIdType::HISTORY_ID);
  }

  // Check unvisited origin.
  {
    auto origin = url::Origin::Create(GURL("https://notvisited.com"));
    auto source_id = GetSourceId(origin);
    EXPECT_FALSE(source_id);
  }
}
