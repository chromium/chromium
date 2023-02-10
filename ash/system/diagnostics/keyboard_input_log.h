// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_DIAGNOSTICS_KEYBOARD_INPUT_LOG_H_
#define ASH_SYSTEM_DIAGNOSTICS_KEYBOARD_INPUT_LOG_H_

#include <cstdint>
#include <string>

#include "ash/ash_export.h"
#include "ash/system/diagnostics/async_log.h"
#include "ash/system/diagnostics/mojom/input.mojom.h"
#include "ash/webui/diagnostics_ui/mojom/input_data_provider.mojom.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"

namespace ash::diagnostics {
// Contains information related to a key press.
struct ASH_EXPORT KeyPressData {
  KeyPressData(uint32_t key_code, uint32_t scan_code);

  uint32_t key_code;
  uint32_t scan_code;

  // We're overriding this operator so that KeyPressData can be used within a
  // set.
  bool operator<(const KeyPressData& rhs) const {
    if (key_code < rhs.key_code) {
      return true;
    };

    return scan_code < rhs.scan_code;
  }
};

// For a given keyboard, KeyboardLogData stores the keyboard name along with a
// list of each key that was pressed while testing the keyboard.
struct KeyboardLogData {
  explicit KeyboardLogData(const std::string& keyboard_name);
  KeyboardLogData();
  KeyboardLogData(KeyboardLogData&&);
  KeyboardLogData& operator=(KeyboardLogData&&);
  ~KeyboardLogData();

  std::string name;
  // Contains the key code and scan code for a pressed key.
  base::flat_set<KeyPressData> key_press_data;
};

// KeyboardInputLog is used to record information about each key that is
// pressed while a keyboard is being tested in the Diagnostics App.
class ASH_EXPORT KeyboardInputLog {
 public:
  explicit KeyboardInputLog(const base::FilePath& log_base_path);
  ~KeyboardInputLog();
  KeyboardInputLog(const KeyboardInputLog&) = delete;
  KeyboardInputLog& operator=(const KeyboardInputLog&) = delete;

  // Creates an entry in |keyboard_log_data_map_| for |id|.
  void AddKeyboard(uint32_t id, const std::string& name);

  // Stores the key code and scan code of the pressed key in
  // |key_press_data_map|. Note: we're only interested in recording the first
  // time a key is pressed. Subsequent presses for a key are ignored.
  void RecordKeyPressForKeyboard(uint32_t id, mojom::KeyEventPtr key_event);
  // Formats the pressed keys for the matching keyboard |id| and appends the
  // formatted content to |log_|. Additionally, we remove |id| from
  // |keyboard_log_data_map_| as this method will be called when we're no
  // longer observing input events for the associated keyboard.
  void CreateLogAndRemoveKeyboard(uint32_t id);
  // Checks if |id| is present in |keyboard_log_data_map|.
  bool KeyboardHasBeenAdded(uint32_t id) const;
  // Returns stored log contents as a string.
  std::string GetLogContents() const;

 private:
  void Append(const std::string& text);

  AsyncLog log_;
  // Maps a keyboard id to a struct containing the keyboard name and info
  // about the keys that were pressed during testing.
  base::flat_map<uint32_t, KeyboardLogData> keyboard_log_data_map_;
};

}  // namespace ash::diagnostics

#endif  // ASH_SYSTEM_DIAGNOSTICS_KEYBOARD_INPUT_LOG_H_
