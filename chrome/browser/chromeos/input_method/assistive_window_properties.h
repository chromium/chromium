// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_ASSISTIVE_WINDOW_PROPERTIES_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_ASSISTIVE_WINDOW_PROPERTIES_H_

#include <string>
#include <vector>
#include "chrome/browser/chromeos/input_method/ui/assistive_delegate.h"

namespace chromeos {
struct AssistiveWindowProperties {
  AssistiveWindowProperties();
  ~AssistiveWindowProperties();

  AssistiveWindowProperties(const AssistiveWindowProperties& other);
  AssistiveWindowProperties& operator=(const AssistiveWindowProperties& other);

  bool operator==(const AssistiveWindowProperties& other) const;

  ui::ime::AssistiveWindowType type = ui::ime::AssistiveWindowType::kNone;
  bool visible = false;
  std::string announce_string;
  std::vector<std::u16string> candidates;
  bool show_indices = false;
  bool show_setting_link = false;
};

}  // namespace chromeos

#endif  //  CHROME_BROWSER_CHROMEOS_INPUT_METHOD_ASSISTIVE_WINDOW_PROPERTIES_H_
