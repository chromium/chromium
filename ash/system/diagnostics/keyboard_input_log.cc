// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/diagnostics/keyboard_input_log.h"

#include <sstream>
#include <string>

#include "base/containers/contains.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"

namespace ash::diagnostics {
namespace {
const char kNewline[] = "\n";
const char kDelimiter[] = ", ";
const char kKeyboardNameTemplate[] = "%s - Key press test - %s";
const char kKeyboardInputLogFilename[] = "keyboard_input.log";

void AddKeyboardNameToLog(std::stringstream& output, const std::string& name) {
  const std::string datetime =
      base::UTF16ToUTF8(base::TimeFormatShortDateAndTime(base::Time::Now()));
  output << base::StringPrintf(kKeyboardNameTemplate, datetime.c_str(),
                               name.c_str())
         << kNewline;
}

void AddKeyPressToLog(std::stringstream& output,
                      uint32_t key_code,
                      uint32_t scan_code) {
  output << "Key code: " << key_code << kDelimiter << "Scan code: " << scan_code
         << kNewline;
}
}  // namespace

KeyPressData::KeyPressData(uint32_t key_code, uint32_t scan_code)
    : key_code(key_code), scan_code(scan_code) {}

KeyboardLogData::KeyboardLogData(const std::string& keyboard_name)
    : name(keyboard_name) {}
KeyboardLogData::KeyboardLogData() = default;
KeyboardLogData::KeyboardLogData(KeyboardLogData&&) = default;
KeyboardLogData& KeyboardLogData::operator=(KeyboardLogData&&) = default;
KeyboardLogData::~KeyboardLogData() = default;

KeyboardInputLog::KeyboardInputLog(const base::FilePath& log_base_path)
    : log_(log_base_path.Append(kKeyboardInputLogFilename)) {}

KeyboardInputLog::~KeyboardInputLog() = default;

void KeyboardInputLog::AddKeyboard(uint32_t id, const std::string& name) {
  if (KeyboardHasBeenAdded(id)) {
    LOG(ERROR) << "Keyboard id: " << id << " has already been added";
    return;
  };
  keyboard_log_data_map_[id] = KeyboardLogData(name);
}

void KeyboardInputLog::RecordKeyPressForKeyboard(uint32_t id,
                                                 mojom::KeyEventPtr key_event) {
  if (!KeyboardHasBeenAdded(id)) {
    LOG(ERROR) << "Attempting to record key press for keyboard that hasn't "
                  "been added yet id: "
               << id;
    return;
  };
  keyboard_log_data_map_[id].key_press_data.insert(
      KeyPressData(key_event->key_code, key_event->scan_code));
}

void KeyboardInputLog::CreateLogAndRemoveKeyboard(uint32_t id) {
  if (!KeyboardHasBeenAdded(id)) {
    LOG(ERROR) << "Attempting to create log for keyboard that hasn't been "
                  "added yet id: "
               << id;
    return;
  };

  std::stringstream output;
  AddKeyboardNameToLog(output, keyboard_log_data_map_[id].name);
  for (const auto& key_press : keyboard_log_data_map_[id].key_press_data) {
    AddKeyPressToLog(output, key_press.key_code, key_press.scan_code);
  }
  Append(output.str());
  keyboard_log_data_map_.erase(id);
}

bool KeyboardInputLog::KeyboardHasBeenAdded(uint32_t id) const {
  return base::Contains(keyboard_log_data_map_, id);
}

std::string KeyboardInputLog::GetLogContents() const {
  return log_.GetContents();
}

void KeyboardInputLog::Append(const std::string& text) {
  log_.Append(text);
}
}  // namespace ash::diagnostics
