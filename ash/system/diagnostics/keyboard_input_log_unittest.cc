// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/diagnostics/keyboard_input_log.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ash/system/diagnostics/log_test_helpers.h"
#include "ash/webui/diagnostics_ui/mojom/input_data_provider.mojom.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::diagnostics {
namespace {

mojom::KeyEventPtr CreateKeyEventPtr(uint32_t key_code, uint32_t scan_code) {
  return mojom::KeyEvent::New(/*id=*/1, mojom::KeyEventType::kPress, key_code,
                              scan_code, /*top_row_position=*/-1);
}

const char kTemplate[] = "Key code: %s, Scan code: %s";
const char kLogFileName[] = "keyboard_input_log";

}  // namespace

class KeyboardInputLogTest : public testing::Test {
 public:
  KeyboardInputLogTest() {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    log_path_ = temp_dir_.GetPath().AppendASCII(kLogFileName);
  }

  ~KeyboardInputLogTest() override { base::RunLoop().RunUntilIdle(); }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::ScopedTempDir temp_dir_;
  base::FilePath log_path_;

  std::string GetExpectedKeyPressLogLine(const KeyPressData& key_press) {
    return base::StringPrintf(
        kTemplate, base::NumberToString(key_press.key_code).c_str(),
        base::NumberToString(key_press.scan_code).c_str());
  }
};

TEST_F(KeyboardInputLogTest, Empty) {
  KeyboardInputLog log(log_path_);

  task_environment_.RunUntilIdle();

  EXPECT_FALSE(base::PathExists(log_path_));
  EXPECT_TRUE(log.GetLogContents().empty());
}

TEST_F(KeyboardInputLogTest, Basic) {
  KeyboardInputLog log(log_path_);
  const uint32_t keyboard_id = 1;
  const std::string keyboard_name = "fancy keyboard";
  const auto key_press_1 = KeyPressData(uint32_t{1}, uint32_t{2});
  const auto key_press_2 = KeyPressData(uint32_t{3}, uint32_t{4});

  log.AddKeyboard(keyboard_id, keyboard_name);
  log.RecordKeyPressForKeyboard(
      keyboard_id,
      CreateKeyEventPtr(key_press_1.key_code, key_press_1.scan_code));
  log.RecordKeyPressForKeyboard(
      keyboard_id,
      CreateKeyEventPtr(key_press_2.key_code, key_press_2.scan_code));

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(log.KeyboardHasBeenAdded(keyboard_id));

  log.CreateLogAndRemoveKeyboard(keyboard_id);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(base::PathExists(log_path_));

  const std::vector<std::string> log_lines = GetLogLines(log.GetLogContents());
  ASSERT_EQ(3u, log_lines.size());
  EXPECT_TRUE(std::string_view(log_lines[0]).find(keyboard_name));
  EXPECT_EQ(GetExpectedKeyPressLogLine(key_press_1), log_lines[1]);
  EXPECT_EQ(GetExpectedKeyPressLogLine(key_press_2), log_lines[2]);
  EXPECT_FALSE(log.KeyboardHasBeenAdded(keyboard_id));
}

TEST_F(KeyboardInputLogTest, MultipleKeyboardsInLog) {
  KeyboardInputLog log(log_path_);
  const uint32_t keyboard_1_id = 1;
  const uint32_t keyboard_2_id = 2;
  const std::string keyboard_1_name = "fancy keyboard";
  const std::string keyboard_2_name = "fancier keyboard";
  const auto keyboard_1_key_press = KeyPressData(uint32_t{1}, uint32_t{2});
  const auto keyboard_2_key_press = KeyPressData(uint32_t{3}, uint32_t{4});

  log.AddKeyboard(keyboard_1_id, keyboard_1_name);
  log.AddKeyboard(keyboard_2_id, keyboard_2_name);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(log.KeyboardHasBeenAdded(keyboard_1_id));
  EXPECT_TRUE(log.KeyboardHasBeenAdded(keyboard_2_id));

  log.RecordKeyPressForKeyboard(
      keyboard_1_id, CreateKeyEventPtr(keyboard_1_key_press.key_code,
                                       keyboard_1_key_press.scan_code));
  log.RecordKeyPressForKeyboard(
      keyboard_2_id, CreateKeyEventPtr(keyboard_2_key_press.key_code,
                                       keyboard_2_key_press.scan_code));

  log.CreateLogAndRemoveKeyboard(keyboard_1_id);
  log.CreateLogAndRemoveKeyboard(keyboard_2_id);

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(base::PathExists(log_path_));

  const std::vector<std::string> log_lines = GetLogLines(log.GetLogContents());
  ASSERT_EQ(4u, log_lines.size());
  EXPECT_TRUE(std::string_view(log_lines[0]).find(keyboard_1_name));
  EXPECT_EQ(GetExpectedKeyPressLogLine(keyboard_1_key_press), log_lines[1]);
  EXPECT_TRUE(std::string_view(log_lines[2]).find(keyboard_2_name));
  EXPECT_EQ(GetExpectedKeyPressLogLine(keyboard_2_key_press), log_lines[3]);
  EXPECT_FALSE(log.KeyboardHasBeenAdded(keyboard_1_id));
  EXPECT_FALSE(log.KeyboardHasBeenAdded(keyboard_2_id));
}

TEST_F(KeyboardInputLogTest, DuplicateKeyPressesNotStored) {
  KeyboardInputLog log(log_path_);
  const uint32_t keyboard_id = 1;
  const std::string keyboard_name = "fancy keyboard";
  const auto key_press_1 = KeyPressData(uint32_t{1}, uint32_t{2});
  const auto key_press_2 = KeyPressData(uint32_t{1}, uint32_t{2});

  log.AddKeyboard(keyboard_id, keyboard_name);
  log.RecordKeyPressForKeyboard(
      keyboard_id,
      CreateKeyEventPtr(key_press_1.key_code, key_press_1.scan_code));
  log.RecordKeyPressForKeyboard(
      keyboard_id,
      CreateKeyEventPtr(key_press_2.key_code, key_press_2.scan_code));

  log.CreateLogAndRemoveKeyboard(keyboard_id);
  task_environment_.RunUntilIdle();
  const std::vector<std::string> log_lines = GetLogLines(log.GetLogContents());
  ASSERT_EQ(2u, log_lines.size());
  EXPECT_TRUE(std::string_view(log_lines[0]).find(keyboard_name));
  EXPECT_EQ(GetExpectedKeyPressLogLine(key_press_1), log_lines[1]);
  EXPECT_FALSE(log.KeyboardHasBeenAdded(keyboard_id));
}
}  // namespace ash::diagnostics
