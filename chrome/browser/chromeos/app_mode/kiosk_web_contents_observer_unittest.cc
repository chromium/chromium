// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_web_contents_observer.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "base/test/repeating_test_future.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_level_logs_saver.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-data-view.h"

namespace chromeos {

namespace {

const auto* kDefaultMessage = u"This is the default message.";
const auto* kDefaultMessage2 = u"This is the second default message";
const auto* kDefaultSource =
    u"chrome-extension://efdahhfldoeikfglgolhibmdidbnpneo/background.js";
const auto* kDefaultSource2 =
    u"chrome-extension://bbbbbbbbbbbbbbbbbbbbbbbbb/background.js";
constexpr int kDefaultLineNumber = 0;
constexpr int kDefaultLineNumber2 = 100;

}  // namespace

class KioskWebContentsObserverTest : public ::testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  }

  void CreateLogsObserver(KioskWebContentsObserver::LoggerCallback callback) {
    observer_ = std::make_unique<KioskWebContentsObserver>(web_contents(),
                                                           std::move(callback));
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

  KioskWebContentsObserver* log_observer() { return observer_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;

  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_;

  std::unique_ptr<KioskWebContentsObserver> observer_;
};

TEST_F(KioskWebContentsObserverTest, ShouldSaveLogWhenMessageAddedToConsole) {
  base::test::RepeatingTestFuture<
      const KioskAppLevelLogsSaver::KioskLogMessage&>
      result_future;

  CreateLogsObserver(result_future.GetCallback());

  AddMessageToConsole(web_contents(), blink::mojom::ConsoleMessageLevel::kInfo,
                      kDefaultMessage, kDefaultLineNumber, kDefaultSource,
                      std::nullopt);

  auto log = result_future.Take();
  EXPECT_EQ(log.line_no(), kDefaultLineNumber);
  EXPECT_EQ(log.message(), kDefaultMessage);
  EXPECT_EQ(log.source(), kDefaultSource);
  EXPECT_EQ(log.untrusted_stack_trace(), std::nullopt);
  EXPECT_EQ(log.severity(), blink::mojom::ConsoleMessageLevel::kInfo);

  AddMessageToConsole(web_contents(), blink::mojom::ConsoleMessageLevel::kError,
                      kDefaultMessage2, kDefaultLineNumber2, kDefaultSource2,
                      std::nullopt);

  auto log2 = result_future.Take();
  EXPECT_EQ(log2.line_no(), kDefaultLineNumber2);
  EXPECT_EQ(log2.message(), kDefaultMessage2);
  EXPECT_EQ(log2.source(), kDefaultSource2);
  EXPECT_EQ(log2.untrusted_stack_trace(), std::nullopt);
  EXPECT_EQ(log2.severity(), blink::mojom::ConsoleMessageLevel::kError);
}

}  // namespace chromeos
