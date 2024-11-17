// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/growth/open_url_action_performer.h"

#include <memory>
#include <optional>

#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chromeos/ash/components/growth/campaigns_logger.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

constexpr char kOpenUrlParamTemplate[] = R"(
    {
      "url": "%s",
      "disposition": %d
    }
)";

constexpr char kValidUrl[] = "https://www.google.com";

class TestNewWindowDelegateImpl : public ash::TestNewWindowDelegate {
 public:
  // TestNewWindowDelegate:
  void OpenUrl(const GURL& url,
               OpenUrlFrom from,
               Disposition disposition) override {
    last_opened_url_ = url;
  }

  GURL last_opened_url_;
};

}  // namespace

class OpenUrlActionPerformerTest : public testing::Test {
 public:
  OpenUrlActionPerformerTest() = default;
  OpenUrlActionPerformerTest(const OpenUrlActionPerformerTest&) = delete;
  OpenUrlActionPerformerTest& operator=(const OpenUrlActionPerformerTest&) =
      delete;
  ~OpenUrlActionPerformerTest() override = default;

  void SetUp() override {
    action_ = std::make_unique<OpenUrlActionPerformer>();
  }

  OpenUrlActionPerformer& action() { return *action_; }

  void RunOpenUrlActionPerformerCallback(
      growth::ActionResult result,
      std::optional<growth::ActionResultReason> reason) {
    if (result == growth::ActionResult::kSuccess) {
      std::move(action_success_closure_).Run();
    } else {
      std::move(action_failed_closure_).Run();
    }
  }

  bool VerifyActionResult(bool success) {
    if (success) {
      action_success_run_loop_.Run();
    } else {
      action_failed_run_loop_.Run();
    }
    return true;
  }

  TestNewWindowDelegateImpl& new_window_delegate() {
    return new_window_delegate_;
  }

 private:
  TestNewWindowDelegateImpl new_window_delegate_;

  content::BrowserTaskEnvironment task_environment_;

  base::RunLoop action_success_run_loop_;
  base::RunLoop action_failed_run_loop_;

  base::OnceClosure action_success_closure_ =
      action_success_run_loop_.QuitClosure();
  base::OnceClosure action_failed_closure_ =
      action_failed_run_loop_.QuitClosure();

  std::unique_ptr<OpenUrlActionPerformer> action_;
  growth::CampaignsLogger logger_;
};

TEST_F(OpenUrlActionPerformerTest, TestValidOpenUrlParams) {
  const auto validOpenUrlParam =
      base::StringPrintf(kOpenUrlParamTemplate, kValidUrl, 0);
  auto value = base::JSONReader::Read(validOpenUrlParam);
  ASSERT_TRUE(value.has_value());
  action().Run(
      /*campaign_id=*/1, /*group_id=*/std::nullopt, &value->GetDict(),
      base::BindOnce(
          &OpenUrlActionPerformerTest::RunOpenUrlActionPerformerCallback,
          base::Unretained(this)));

  EXPECT_TRUE(VerifyActionResult(/*success=*/true));
  EXPECT_EQ(new_window_delegate().last_opened_url_, GURL(kValidUrl));
}

TEST_F(OpenUrlActionPerformerTest, TestInvalidOpenUrlParams) {
  auto* const invalidOpenUrlParam = "{}";
  auto value = base::JSONReader::Read(invalidOpenUrlParam);
  ASSERT_TRUE(value.has_value());
  action().Run(
      /*campaign_id=*/1, /*group_id=*/std::nullopt, &value->GetDict(),
      base::BindOnce(
          &OpenUrlActionPerformerTest::RunOpenUrlActionPerformerCallback,
          base::Unretained(this)));

  EXPECT_TRUE(VerifyActionResult(/*success=*/false));
}
