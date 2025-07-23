// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_web_contents_logs_collector.h"

#include <memory>
#include <optional>
#include <string>

#include "base/test/repeating_test_future.h"
#include "base/test/task_environment.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-data-view.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

const std::u16string kDefaultMessage = u"This is the default message.";
const std::u16string kDefaultSource =
    u"chrome-extension://efdahhfldoeikfglgolhibmdidbnpneo/background.js";
constexpr int kDefaultLineNumber = 0;
const std::u16string kWebContent1Message =
    u"This message is from web contents1";
const std::u16string kWebContent2Message =
    u"This message is from web contents2";

}  // namespace

class KioskWebContentsLogsCollectorTest : public ::testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  }

  void CreateLogCollector(
      KioskWebContentsLogsCollector::LoggerCallback callback) {
    log_collector_ = std::make_unique<KioskWebContentsLogsCollector>(
        profile(), std::move(callback));
  }

  void AddMessageToConsole(
      content::WebContents* web_contents,
      blink::mojom::ConsoleMessageLevel log_level,
      const std::u16string& message,
      int32_t line_no,
      const std::u16string& source_id,
      const std::optional<std::u16string>& untrusted_stack_trace) {
    auto* tester = content::WebContentsTester::For(web_contents);
    tester->TestDidAddMessageToConsole(log_level, message, line_no, source_id,
                                       untrusted_stack_trace);
  }

  TestingProfile* profile() { return &profile_; }

  content::WebContents* web_contents() { return web_contents_.get(); }

  KioskWebContentsLogsCollector* log_collector() {
    return log_collector_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;

  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_;

  std::unique_ptr<KioskWebContentsLogsCollector> log_collector_;
};

TEST_F(KioskWebContentsLogsCollectorTest,
       ShouldSaveLogWhenMessageAddedToConsole) {
  base::test::RepeatingTestFuture<
      const KioskAppLevelLogsSaver::KioskLogMessage&>
      result_future;

  CreateLogCollector(result_future.GetCallback());
  log_collector()->AddWebContentsToObserve(web_contents());

  AddMessageToConsole(web_contents(), blink::mojom::ConsoleMessageLevel::kInfo,
                      kDefaultMessage, kDefaultLineNumber, kDefaultSource,
                      std::nullopt);

  auto log = result_future.Take();
  EXPECT_EQ(log.line_no(), kDefaultLineNumber);
  EXPECT_EQ(log.message(), kDefaultMessage);
  EXPECT_EQ(log.source(), kDefaultSource);
  EXPECT_EQ(log.untrusted_stack_trace(), std::nullopt);
  EXPECT_EQ(log.severity(), blink::mojom::ConsoleMessageLevel::kInfo);
}

TEST_F(KioskWebContentsLogsCollectorTest,
       ShouldSaveLogsFromDifferentWebContents) {
  base::test::RepeatingTestFuture<
      const KioskAppLevelLogsSaver::KioskLogMessage&>
      result_future;
  std::unique_ptr<content::WebContents> web_contents1 =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  std::unique_ptr<content::WebContents> web_contents2 =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);

  CreateLogCollector(result_future.GetCallback());
  log_collector()->AddWebContentsToObserve(web_contents1.get());
  log_collector()->AddWebContentsToObserve(web_contents2.get());

  AddMessageToConsole(
      web_contents1.get(), blink::mojom::ConsoleMessageLevel::kInfo,
      kWebContent1Message, kDefaultLineNumber, kDefaultSource, std::nullopt);

  auto log1 = result_future.Take();
  EXPECT_EQ(log1.message(), kWebContent1Message);

  AddMessageToConsole(
      web_contents2.get(), blink::mojom::ConsoleMessageLevel::kInfo,
      kWebContent2Message, kDefaultLineNumber, kDefaultSource, std::nullopt);

  auto log2 = result_future.Take();
  EXPECT_EQ(log2.message(), kWebContent2Message);
}

}  // namespace chromeos
