// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/diagnostics_metrics_message_handler.h"

#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace diagnostics {
namespace metrics {
namespace {

class DiagnosticsMetricsMessageHandlerTest : public testing::Test {
 public:
  DiagnosticsMetricsMessageHandlerTest() : web_ui_() {}
  ~DiagnosticsMetricsMessageHandlerTest() override = default;

  void InitializeHandler(NavigationView view) {
    handler_ = std::make_unique<DiagnosticsMetricsMessageHandler>(view);
    handler_->SetWebUiForTesting(&web_ui_);
    handler_->RegisterMessages();
  }

 protected:
  content::TestWebUI web_ui_;
  std::unique_ptr<DiagnosticsMetricsMessageHandler> handler_;
};
}  // namespace

TEST_F(DiagnosticsMetricsMessageHandlerTest, AbleToRegisterMessages) {
  EXPECT_NO_FATAL_FAILURE(InitializeHandler(NavigationView::kInput));
}

TEST_F(DiagnosticsMetricsMessageHandlerTest, InitialViewSet) {
  NavigationView expected_view = NavigationView::kSystem;
  InitializeHandler(expected_view);

  EXPECT_EQ(expected_view, handler_->GetCurrentViewForTesting());
}
}  // namespace metrics
}  // namespace diagnostics
}  // namespace ash
