// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_WINDOW_PROPERTIES_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_WINDOW_PROPERTIES_H_

#include <string>
#include <vector>

#include "chrome/browser/ui/ash/input_method/assistive_delegate.h"

namespace ash {
namespace input_method {

struct AssistiveWindowProperties {
  AssistiveWindowProperties();
  ~AssistiveWindowProperties();

  AssistiveWindowProperties(const AssistiveWindowProperties& other);
  AssistiveWindowProperties& operator=(const AssistiveWindowProperties& other);

  bool operator==(const AssistiveWindowProperties& other) const;

  ash::ime::AssistiveWindowType type = ash::ime::AssistiveWindowType::kNone;
  bool visible = false;
  std::u16string announce_string;
  std::vector<std::u16string> candidates;
  bool show_indices = false;
  bool show_setting_link = false;
};

}  // namespace input_method
}  // namespace ash

#endif  //  CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_WINDOW_PROPERTIES_H_
