// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_browser_logs_collector.h"

#include <memory>

#include "base/test/repeating_test_future.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_level_logs_saver.h"
#include "chrome/browser/chromeos/app_mode/kiosk_web_contents_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-data-view.h"

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
    CloseAllTabs();
    browsers_.clear();
    BrowserWithTestWindowTest::TearDown();
  }

  void CreateLogsCollector(
      KioskWebContentsObserver::LoggerCallback logger_callback) {
    logs_collector_ =
        std::make_unique<KioskBrowserLogsCollector>(std::move(logger_callback));
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

  content::WebContentsTester* AddWebContentsToBrowser(Browser* browser) {
    CHECK(browser);
    std::unique_ptr<content::WebContents> web_contents(
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
    auto* web_contents_tester =
        content::WebContentsTester::For(web_contents.get());
    browser->tab_strip_model()->AppendWebContents(std::move(web_contents),
                                                  true);
    return web_contents_tester;
  }

  Browser* CreateTestBrowser() {
    browsers_.push_back(CreateBrowser(profile(), Browser::Type::TYPE_NORMAL,
                                      /*hosted_app=*/false));
    return browsers_.back().get();
  }

  void CloseAllTabs() {
    for (auto& browser : browsers_) {
      browser->tab_strip_model()->CloseAllTabs();
    }
  }

 private:
  std::vector<std::unique_ptr<Browser>> browsers_;
  std::unique_ptr<KioskBrowserLogsCollector> logs_collector_;
};

TEST_F(KioskBrowserLogsCollectorTest, ShouldObserveLogsFromMultipleBrowsers) {
  base::test::RepeatingTestFuture<
      const KioskAppLevelLogsSaver::KioskLogMessage&>
      result_future;
  CreateLogsCollector(result_future.GetCallback());

  Browser* browser1 = CreateTestBrowser();
  auto* web_contents1 = AddWebContentsToBrowser(browser1);
  AddMessageToConsole(web_contents1, blink::mojom::ConsoleMessageLevel::kInfo,
                      kDefaultMessage, kDefaultLineNumber, kDefaultSource,
                      std::nullopt);

  auto log = result_future.Take();
  EXPECT_EQ(log.line_no(), kDefaultLineNumber);
  EXPECT_EQ(log.message(), kDefaultMessage);
  EXPECT_EQ(log.source(), kDefaultSource);
  EXPECT_EQ(log.untrusted_stack_trace(), std::nullopt);
  EXPECT_EQ(log.severity(), blink::mojom::ConsoleMessageLevel::kInfo);

  Browser* browser2 = CreateTestBrowser();
  auto* web_contents2 = AddWebContentsToBrowser(browser2);
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
  Browser* browser1 = CreateTestBrowser();
  auto* web_contents1 = AddWebContentsToBrowser(browser1);
  Browser* browser2 = CreateTestBrowser();
  auto* web_contents2 = AddWebContentsToBrowser(browser2);

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

  Browser* browser1 = CreateTestBrowser();
  auto* tab1 = AddWebContentsToBrowser(browser1);
  auto* tab2 = AddWebContentsToBrowser(browser1);
  auto* tab3 = AddWebContentsToBrowser(browser1);

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
