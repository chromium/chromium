// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/common/actor.mojom.h"
#include "components/actor/core/actor_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "pdf/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"

#if BUILDFLAG(ENABLE_PDF)
#include "components/pdf/browser/pdf_document_helper.h"
#include "pdf/pdf_features.h"
#endif  // BUILDFLAG(ENABLE_PDF)

using base::test::TestFuture;

namespace actor {
namespace {

#if BUILDFLAG(ENABLE_PDF) && !BUILDFLAG(IS_CHROMEOS)
class ActorClickToolPDFBrowserTest
    : public ActorToolsTest,
      public ::testing::WithParamInterface<bool> {
 public:
  ActorClickToolPDFBrowserTest() {
    if (BypassTOUValidationForGuestView()) {
      feature_list_.InitWithFeatures({kActorBypassTOUValidationForGuestView},
                                     {chrome_pdf::features::kPdfOopif});
    } else {
      feature_list_.InitWithFeatures({},
                                     {chrome_pdf::features::kPdfOopif,
                                      kActorBypassTOUValidationForGuestView});
    }
  }

  ~ActorClickToolPDFBrowserTest() override = default;

  bool BypassTOUValidationForGuestView() { return GetParam(); }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ActorToolsTest::SetUpCommandLine(command_line);
    // PDF rendering under interactive_ui_tests can trigger low-level transfer
    // cache alignment or serialization bugs in GPU rasterization depending on
    // the platform or virtualized driver. Disabling the GPU forces software GL
    // rendering which bypasses these crashes.
    command_line->AppendSwitch(switches::kDisableGpu);
  }

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    return info.param ? "BypassGuestViewTOU" : "CheckGuestViewTOU";
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(ActorClickToolPDFBrowserTest, Click) {
  const GURL url = embedded_test_server()->GetURL("/pdf/test.pdf");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  ASSERT_TRUE(base::test::RunUntil([this]() {
    auto* pdf_helper =
        pdf::PDFDocumentHelper::MaybeGetForWebContents(web_contents());
    if (!pdf_helper) {
      return false;
    }
    return pdf_helper->IsDocumentLoadComplete() &&
           web_contents()->IsDocumentOnLoadCompletedInPrimaryMainFrame();
  })) << "Timeout waiting for condition: PDF Loaded";

  while (true) {
    GetPageApc();
    std::unique_ptr<ToolRequest> action =
        MakeClickRequest(*active_tab(), gfx::Point(650, 25));
    ActResultFuture future;
    actor_task().Act(ToRequestList(action), future.GetCallback());
    const auto& action_results = future.Get();
    ASSERT_EQ(action_results.size(), 1u);
    const auto& result = *action_results[0].result;

    if (BypassTOUValidationForGuestView()) {
      if (IsOk(result)) {
        break;
      }
      // Under virtualized environments or slow test runs, optimization guide
      // may take some time to map subframe document identifiers. Retry after
      // a tiny wait.
      if (result.code ==
          mojom::ActionResultCode::kFrameLocationChangedSinceObservation) {
        TinyWait();
        continue;
      }
      ExpectOkResult(future);
      break;
    } else {
      // Sometimes it might be allowed, but it will fail eventually. Keep
      // looping until we fail.
      if (result.code ==
          mojom::ActionResultCode::kFrameLocationChangedSinceObservation) {
        break;
      }
    }
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         ActorClickToolPDFBrowserTest,
                         ::testing::Bool(),
                         &ActorClickToolPDFBrowserTest::DescribeParams);

#endif  // BUILDFLAG(ENABLE_PDF) && !BUILDFLAG(IS_CHROMEOS)

}  // namespace
}  // namespace actor
