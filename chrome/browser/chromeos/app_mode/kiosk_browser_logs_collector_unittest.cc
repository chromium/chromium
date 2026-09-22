// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_browser_logs_collector.h"

#include <memory>

#include "base/test/repeating_test_future.h"
#include "chrome/browser/ash/browser_delegate/browser_controller_impl.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_level_logs_saver.h"
#include "chrome/browser/chromeos/app_mode/kiosk_web_contents_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-shared.h"

namespace chromeos {

namespace {

const auto* kDefaultMessage = u"This is the default message 1.";
const auto* kDefaultMessage2 = u"This is the default message 2.";
const auto* kDefaultMessage3 = u"This is the default message 3.";
const auto* kDefaultSource = u"main.js";
const auto* kDefaultSource2 = u"font.js";
const auto* kDefaultSource3 = u"event.js";
constexpr int kDefaultLineNumber = 0;
constexpr int kDefaultLineNumber2 = 100;
constexpr int kDefaultLineNumber3 = 200;

}  // namespace

class KioskBrowserLogsCollectorTest : public BrowserWithTestWindowTest {
 public:
  void TearDown() override {
    logs_collector_.reset();
    web_contents_list_.clear();
    BrowserWithTestWindowTest::TearDown();
  }

  void CreateLogsCollector(
      KioskWebContentsObserver::LoggerCallback logger_callback) {
    logs_collector_ =
        std::make_unique<KioskBrowserLogsCollector>(std::move(logger_callback));
    for (const auto& web_contents : web_contents_list_) {
      tab_observer()->OnTabInserted(/*browser=*/nullptr, web_contents.get());
    }
  }

  void AddMessageToConsole(
      content::WebContentsTester* web_contents,
      blink::mojom::ConsoleMessageLevel log_level,
      const std::u16string& message,
      int32_t line_no,
      const std::u16string& source_id,
      const std::optional<std::u16string>& untrusted_stack_trace) {
    web_contents->TestDidAddMessageToConsole(log_level, message, line_no,
                                             source_id, untrusted_stack_trace);
  }

  content::WebContentsTester* CreateWebContents() {
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    auto* web_contents_tester =
        content::WebContentsTester::For(web_contents.get());
    if (logs_collector_) {
      tab_observer()->OnTabInserted(/*browser=*/nullptr, web_contents.get());
    }
    web_contents_list_.push_back(std::move(web_contents));
    return web_contents_tester;
  }

 private:
  ash::BrowserController::TabObserver* tab_observer() {
    return logs_collector_.get();
  }

  ash::BrowserControllerImpl browser_controller_;
  std::vector<std::unique_ptr<content::WebContents>> web_contents_list_;
  std::unique_ptr<KioskBrowserLogsCollector> logs_collector_;
};

TEST_F(KioskBrowserLogsCollectorTest, ShouldObserveLogsFromMultipleBrowsers) {
  base::test::RepeatingTestFuture<
      const KioskAppLevelLogsSaver::KioskLogMessage&>
      result_future;
  CreateLogsCollector(result_future.GetCallback());

  auto* web_contents1 = CreateWebContents();
  AddMessageToConsole(web_contents1, blink::mojom::ConsoleMessageLevel::kInfo,
                      kDefaultMessage, kDefaultLineNumber, kDefaultSource,
                      std::nullopt);

  auto log = result_future.Take();
  EXPECT_EQ(log.line_no(), kDefaultLineNumber);
  EXPECT_EQ(log.message(), kDefaultMessage);
  EXPECT_EQ(log.source(), kDefaultSource);
  EXPECT_EQ(log.untrusted_stack_trace(), std::nullopt);
  EXPECT_EQ(log.severity(), blink::mojom::ConsoleMessageLevel::kInfo);

  auto* web_contents2 = CreateWebContents();
  AddMessageToConsole(web_contents2, blink::mojom::ConsoleMessageLevel::kError,
                      kDefaultMessage2, kDefaultLineNumber2, kDefaultSource2,
                      std::nullopt);

  auto log2 = result_future.Take();
  EXPECT_EQ(log2.line_no(), kDefaultLineNumber2);
  EXPECT_EQ(log2.message(), kDefaultMessage2);
  EXPECT_EQ(log2.source(), kDefaultSource2);
  EXPECT_EQ(log2.untrusted_stack_trace(), std::nullopt);
  EXPECT_EQ(log2.severity(), blink::mojom::ConsoleMessageLevel::kError);
}

TEST_F(KioskBrowserLogsCollectorTest, ShouldObserveLogsFromExistingBrowsers) {
  auto* web_contents1 = CreateWebContents();
  auto* web_contents2 = CreateWebContents();

  base::test::RepeatingTestFuture<
      const KioskAppLevelLogsSaver::KioskLogMessage&>
      result_future;
  CreateLogsCollector(result_future.GetCallback());

  AddMessageToConsole(web_contents1, blink::mojom::ConsoleMessageLevel::kInfo,
                      kDefaultMessage, kDefaultLineNumber, kDefaultSource,
                      std::nullopt);

  auto log = result_future.Take();
  EXPECT_EQ(log.line_no(), kDefaultLineNumber);
  EXPECT_EQ(log.message(), kDefaultMessage);
  EXPECT_EQ(log.source(), kDefaultSource);
  EXPECT_EQ(log.untrusted_stack_trace(), std::nullopt);
  EXPECT_EQ(log.severity(), blink::mojom::ConsoleMessageLevel::kInfo);

  AddMessageToConsole(web_contents2, blink::mojom::ConsoleMessageLevel::kError,
                      kDefaultMessage2, kDefaultLineNumber2, kDefaultSource2,
                      std::nullopt);

  auto log2 = result_future.Take();
  EXPECT_EQ(log2.line_no(), kDefaultLineNumber2);
  EXPECT_EQ(log2.message(), kDefaultMessage2);
  EXPECT_EQ(log2.source(), kDefaultSource2);
  EXPECT_EQ(log2.untrusted_stack_trace(), std::nullopt);
  EXPECT_EQ(log2.severity(), blink::mojom::ConsoleMessageLevel::kError);
}

TEST_F(KioskBrowserLogsCollectorTest, ShouldObserveLogsFromMultipleTabs) {
  base::test::RepeatingTestFuture<
      const KioskAppLevelLogsSaver::KioskLogMessage&>
      result_future;
  CreateLogsCollector(result_future.GetCallback());

  auto* tab1 = CreateWebContents();
  auto* tab2 = CreateWebContents();
  auto* tab3 = CreateWebContents();

  AddMessageToConsole(tab1, blink::mojom::ConsoleMessageLevel::kInfo,
                      kDefaultMessage, kDefaultLineNumber, kDefaultSource,
                      std::nullopt);

  auto log1 = result_future.Take();
  EXPECT_EQ(log1.line_no(), kDefaultLineNumber);
  EXPECT_EQ(log1.message(), kDefaultMessage);
  EXPECT_EQ(log1.source(), kDefaultSource);
  EXPECT_EQ(log1.untrusted_stack_trace(), std::nullopt);
  EXPECT_EQ(log1.severity(), blink::mojom::ConsoleMessageLevel::kInfo);

  AddMessageToConsole(tab2, blink::mojom::ConsoleMessageLevel::kError,
                      kDefaultMessage2, kDefaultLineNumber2, kDefaultSource2,
                      std::nullopt);

  auto log2 = result_future.Take();
  EXPECT_EQ(log2.line_no(), kDefaultLineNumber2);
  EXPECT_EQ(log2.message(), kDefaultMessage2);
  EXPECT_EQ(log2.source(), kDefaultSource2);
  EXPECT_EQ(log2.untrusted_stack_trace(), std::nullopt);
  EXPECT_EQ(log2.severity(), blink::mojom::ConsoleMessageLevel::kError);

  AddMessageToConsole(tab3, blink::mojom::ConsoleMessageLevel::kWarning,
                      kDefaultMessage3, kDefaultLineNumber3, kDefaultSource3,
                      std::nullopt);

  auto log3 = result_future.Take();
  EXPECT_EQ(log3.line_no(), kDefaultLineNumber3);
  EXPECT_EQ(log3.message(), kDefaultMessage3);
  EXPECT_EQ(log3.source(), kDefaultSource3);
  EXPECT_EQ(log3.untrusted_stack_trace(), std::nullopt);
  EXPECT_EQ(log3.severity(), blink::mojom::ConsoleMessageLevel::kWarning);
}

}  // namespace chromeos
