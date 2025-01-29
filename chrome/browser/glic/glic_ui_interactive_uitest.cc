// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_ui.h"
#include "chrome/browser/glic/interactive_glic_test.h"
#include "chrome/common/chrome_features.h"
#include "content/public/test/browser_test.h"

namespace glic {

namespace {

// Specifies artificial parameters for how network and loading should behave for
// tests in this file.
struct TestParams {
  TestParams() = default;
  explicit TestParams(bool connected) : start_connected(connected) {}
  TestParams(size_t time_before_loading_page_ms_,
             size_t min_loading_page_duration_ms_,
             size_t max_loading_page_duration_ms_)
      : time_before_loading_page_ms(time_before_loading_page_ms_),
        min_loading_page_duration_ms(min_loading_page_duration_ms_),
        max_loading_page_duration_ms(max_loading_page_duration_ms_) {}
  ~TestParams() = default;

  // Time before loading page shows.
  std::optional<size_t> time_before_loading_page_ms;
  // Minimum time loading page shows.
  std::optional<size_t> min_loading_page_duration_ms;
  // Maximum time loading page shows before error.
  std::optional<size_t> max_loading_page_duration_ms;
  // Whether the page believes it has network at startup.
  bool start_connected = true;

  base::FieldTrialParams GetFieldTrialParams() const {
    base::FieldTrialParams params;
    if (time_before_loading_page_ms) {
      params.emplace(features::kGlicPreLoadingTimeMs.name,
                     base::StringPrintf("%ums", *time_before_loading_page_ms));
    }
    if (min_loading_page_duration_ms) {
      params.emplace(features::kGlicMinLoadingTimeMs.name,
                     base::StringPrintf("%ums", *min_loading_page_duration_ms));
    }
    if (max_loading_page_duration_ms) {
      params.emplace(features::kGlicMaxLoadingTimeMs.name,
                     base::StringPrintf("%ums", *max_loading_page_duration_ms));
    }
    return params;
  }
};
}  // namespace

// Base class that sets up network connection mode and timeouts based on
// `TestParams` (see above).
class GlicUiInteractiveUiTestBase : public test::InteractiveGlicTest {
 public:
  explicit GlicUiInteractiveUiTestBase(const TestParams& params)
      : InteractiveGlicTestT(params.GetFieldTrialParams()) {
    if (!params.start_connected) {
      GlicUI::simulate_no_connection_for_testing();
    }
  }
  ~GlicUiInteractiveUiTestBase() override = default;

  auto CheckElementHidden(const DeepQuery& where, bool hidden) {
    MultiStep steps;
    if (!hidden) {
      steps = Steps(
          InAnyContext(WaitForElementVisible(test::kGlicHostElementId, where)));
    }
    steps.emplace_back(InAnyContext(CheckJsResultAt(test::kGlicHostElementId,
                                                    where, "(el) => el.hidden",
                                                    testing::Eq(hidden))));
    AddDescriptionPrefix(steps, "CheckElementHidden");
    return steps;
  }

  const DeepQuery kOfflinePanel = {"#offlinePanel"};
};

// Tests the network being connected at startup (as normal).
class GlicUiConnectedUiTest : public GlicUiInteractiveUiTestBase {
 public:
  GlicUiConnectedUiTest()
      : GlicUiInteractiveUiTestBase(TestParams(/*connected=*/true)) {}
  ~GlicUiConnectedUiTest() override = default;
};

IN_PROC_BROWSER_TEST_F(GlicUiConnectedUiTest, DisconnectedPanelHidden) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kAttached, GlicInstrumentMode::kHostOnly),
      CheckElementHidden(kOfflinePanel, true));
}

// Tests the network being unavailable at startup.
class GlicUiDisconnectedUiTest : public GlicUiInteractiveUiTestBase {
 public:
  GlicUiDisconnectedUiTest()
      : GlicUiInteractiveUiTestBase(TestParams(/*connected=*/false)) {}
  ~GlicUiDisconnectedUiTest() override = default;
};

IN_PROC_BROWSER_TEST_F(GlicUiDisconnectedUiTest, DisconnectedPanelShown) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kAttached, GlicInstrumentMode::kHostOnly),
      CheckElementHidden(kOfflinePanel, false));
}

}  // namespace glic
