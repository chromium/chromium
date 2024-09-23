// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHORTCUTS_SHORTCUT_ICON_GENERATOR_H_
#define CHROME_BROWSER_SHORTCUTS_SHORTCUT_ICON_GENERATOR_H_

#include <string>

#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace shortcuts {

using SquareSizePx = int;

// Generates a square container icon of |output_size| by drawing the given
// |icon_letter|.
SkBitmap GenerateBitmap(SquareSizePx output_size, char32_t icon_letter);

// Returns the first letter from |app_url| that will be painted on the generated
// icon.
char32_t GenerateIconLetterFromUrl(const GURL& app_url);

// Returns the first letter from |app_name| that will be painted on the
// generated icon.
char32_t GenerateIconLetterFromName(const std::u16string& app_name);

// Converts a codepoint (intended to be the first letter of an app name or URL)
// to a UTF-16 string.
//
// Public for testing.
std::u16string IconLetterToString(char32_t cp);

}  // namespace shortcuts

#endif  // CHROME_BROWSER_SHORTCUTS_SHORTCUT_ICON_GENERATOR_H_
