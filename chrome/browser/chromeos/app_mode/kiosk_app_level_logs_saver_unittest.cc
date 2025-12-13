// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_level_logs_saver.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/app_mode/kiosk_web_app_install_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-data-view.h"

namespace chromeos {

using blink::mojom::ConsoleMessageLevel;

namespace {

constexpr size_t kMaxLogEntriesAllowed = 3000u;
constexpr int kDefaultLineNumber = 100;

const auto* kDefaultMessage = u"This is a log from the extension console.";
const auto* kDefaultSource =
    u"chrome-extension://efdahhfldoeikfglgolhibmdidbnpneo/background.js";
const auto* kDefaultStackTrace = u"method1\nmethod2\nmethod3";

const auto* kDefaultFormattedMessage =
    u"chrome-extension://efdahhfldoeikfglgolhibmdidbnpneo/background.js:100 "
    u"\"This is a log from the extension console.\"";
const auto* kDefaultFormattedMessageWithStackTrace =
    u"chrome-extension://efdahhfldoeikfglgolhibmdidbnpneo/background.js:100 "
    u"\"This is a log from the extension console.\"\nstack_trace: "
    u"method1\nmethod2\nmethod3";
}  // namespace

class KioskAppLevelLogsSaverTest : public testing::Test {
 public:
  KioskAppLevelLogsSaverTest() = default;

  KioskAppLevelLogsSaverTest(const KioskAppLevelLogsSaverTest&) = delete;
  KioskAppLevelLogsSaverTest& operator=(const KioskAppLevelLogsSaverTest&) =
      delete;

  void SetUp() override {
    logs_saver_ = std::make_unique<KioskAppLevelLogsSaver>(
        base::BindRepeating(&KioskAppLevelLogsSaverTest::SaveLogsCallback,
                            weak_factory_.GetWeakPtr()));
  }

  KioskAppLevelLogsSaver& logs_saver() {
    return CHECK_DEREF(logs_saver_.get());
  }

  size_t total_logs_saved() { return total_logs_saved_; }

  std::u16string& last_saved_log() { return last_saved_log_; }

 private:
  void SaveLogsCallback(const std::u16string& log,
                        blink::mojom::ConsoleMessageLevel severity) {
    total_logs_saved_++;
    last_saved_log_ = log;
  }

  std::unique_ptr<KioskAppLevelLogsSaver> logs_saver_;
  size_t total_logs_saved_ = 0u;
  std::u16string last_saved_log_;

  base::WeakPtrFactory<KioskAppLevelLogsSaverTest> weak_factory_{this};
};

TEST_F(KioskAppLevelLogsSaverTest, ShouldNotSaveLogAfterMaximumLimit) {
  KioskAppLevelLogsSaver::KioskLogMessage log(
      kDefaultMessage, ConsoleMessageLevel::kInfo, kDefaultLineNumber,
      kDefaultSource, /*untrusted_stack_trace=*/std::nullopt);
  CHECK_EQ(total_logs_saved(), 0u);

  for (size_t i = 0u; i < kMaxLogEntriesAllowed; i++) {
    CHECK_EQ(total_logs_saved(), i);

    logs_saver().SaveLog(log);
  }
  EXPECT_EQ(total_logs_saved(), kMaxLogEntriesAllowed);

  logs_saver().SaveLog(log);

  // Total logs saved should not increase by the maximum value.
  EXPECT_EQ(total_logs_saved(), kMaxLogEntriesAllowed);
}

TEST_F(KioskAppLevelLogsSaverTest, ShouldLogWithProperFormat) {
  KioskAppLevelLogsSaver::KioskLogMessage log(
      kDefaultMessage, ConsoleMessageLevel::kInfo, kDefaultLineNumber,
      kDefaultSource, /*untrusted_stack_trace=*/std::nullopt);

  logs_saver().SaveLog(log);

  EXPECT_EQ(last_saved_log(), kDefaultFormattedMessage);
}

TEST_F(KioskAppLevelLogsSaverTest, ShouldLogWithStackTraceWithProperFormat) {
  KioskAppLevelLogsSaver::KioskLogMessage log(
      kDefaultMessage, ConsoleMessageLevel::kInfo, kDefaultLineNumber,
      kDefaultSource, kDefaultStackTrace);

  logs_saver().SaveLog(log);

  EXPECT_EQ(last_saved_log(), kDefaultFormattedMessageWithStackTrace);
}

TEST_F(KioskAppLevelLogsSaverTest, ShouldLogMessageWithoutLogSource) {
  KioskAppLevelLogsSaver::KioskLogMessage log(kDefaultMessage,
                                              ConsoleMessageLevel::kInfo);

  logs_saver().SaveLog(log);

  EXPECT_EQ(last_saved_log(), kDefaultMessage);
}

}  // namespace chromeos
