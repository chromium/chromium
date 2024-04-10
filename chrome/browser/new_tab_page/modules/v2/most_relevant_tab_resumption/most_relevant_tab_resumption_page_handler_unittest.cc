// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/most_relevant_tab_resumption/most_relevant_tab_resumption_page_handler.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/mojom/history_types.mojom.h"
#include "components/search/ntp_features.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MostRelevantTabResumptionPageHandlerTest
    : public BrowserWithTestWindowTest {
 public:
  MostRelevantTabResumptionPageHandlerTest() = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile()));
    handler_ = std::make_unique<MostRelevantTabResumptionPageHandler>(
        web_contents_.get());
  }

  void TearDown() override {
    handler_.reset();
    web_contents_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  MostRelevantTabResumptionPageHandler& handler() { return *handler_; }

 private:
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<MostRelevantTabResumptionPageHandler> handler_;
};
}  // namespace
