// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_UI_SUGGESTION_DETAILS_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_UI_SUGGESTION_DETAILS_H_

#include <string>


namespace ui {
namespace ime {

struct SuggestionDetails {
  std::u16string text;
  size_t confirmed_length = 0;
  bool show_annotation = false;
  bool show_setting_link = false;
};

}  // namespace ime
}  // namespace ui

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_UI_SUGGESTION_DETAILS_H_
