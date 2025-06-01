// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHORTCUTS_SHORTCUT_ICON_GENERATOR_H_
#define CHROME_BROWSER_SHORTCUTS_SHORTCUT_ICON_GENERATOR_H_

#include <string>
#include <string_view>

#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

// TODO(https://crbug.com/421202274): This is duplicative of
// ui/gfx/monogram_utils.h which also draws a letter in a shape; figure out how
// to be less redundant.
//
// TODO(https://crbug.com/421202274): This is duplicative of
// components/favicon/core/fallback_url_util.h which also shortens a string to
// an initial letter; figure out how to be less redundant.
namespace shortcuts {

using SquareSizePx = int;

// Generates a square container icon of `output_size` by drawing the given
// `icon_letter`.
SkBitmap GenerateBitmap(SquareSizePx output_size,
                        std::u16string_view icon_letter);

// Returns the first grapheme from `app_url` to be passed to `GenerateBitmap()`,
// above.
std::u16string GenerateIconLetterFromUrl(const GURL& app_url);

// Returns the first grapheme from `app_name` to be passed to
// `GenerateBitmap()`, above.
std::u16string GenerateIconLetterFromName(std::u16string_view app_name);

}  // namespace shortcuts

#endif  // CHROME_BROWSER_SHORTCUTS_SHORTCUT_ICON_GENERATOR_H_
