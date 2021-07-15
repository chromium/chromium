// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_UI_SUGGESTION_DETAILS_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_UI_SUGGESTION_DETAILS_H_

#include <string>

#include "ui/gfx/color_palette.h"

namespace ui {
namespace ime {

constexpr SkColor kDefaultSuggestionTextColor =
    SkColorSetA(gfx::kGoogleGrey700, 0xB3);  // 70% opacity

struct SuggestionDetails {
  std::u16string text;
  size_t confirmed_length = 0;
  bool show_accept_annotation = false;
  bool show_quick_accept_annotation = false;
  bool show_setting_link = false;
  SkColor text_color = kDefaultSuggestionTextColor;

  bool operator==(const SuggestionDetails& other) const {
    return text == other.text && confirmed_length == other.confirmed_length &&
           show_accept_annotation == other.show_accept_annotation &&
           show_quick_accept_annotation == other.show_quick_accept_annotation &&
           show_setting_link == other.show_setting_link &&
           text_color == other.text_color;
  }
};

}  // namespace ime
}  // namespace ui

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_UI_SUGGESTION_DETAILS_H_
