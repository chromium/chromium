// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_IME_INFO_H_
#define ASH_PUBLIC_CPP_IME_INFO_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// Metadata about an installed input method.
struct ASH_PUBLIC_EXPORT ImeInfo {
  ImeInfo();
  ImeInfo(const ImeInfo& other);
  ~ImeInfo();

  // True if the IME is a third-party extension.
  bool third_party = false;

  // ID that identifies the IME (e.g., "t:latn-post", "pinyin", "hangul").
  std::string id;

  // Long name of the IME, which is used as the user-visible name.
  std::u16string name;

  // UI indicator for the IME (e.g., "US"). If the IME has no indicator, uses
  // the first two characters in its preferred keyboard layout or language code
  // (e.g., "ko", "ja", "en-US").
  std::u16string short_name;
};

// A menu item that sets an IME configuration property.
struct ASH_PUBLIC_EXPORT ImeMenuItem {
  ImeMenuItem();
  ImeMenuItem(const ImeMenuItem& other);
  ~ImeMenuItem();

  // True if the item is selected / enabled.
  bool checked = false;

  // The key which identifies the property controlled by the menu item, e.g.
  // "InputMode.HalfWidthKatakana".
  std::string key;

  // The item label, e.g. "Switch to full punctuation mode" or "Hiragana".
  std::u16string label;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_IME_INFO_H_
