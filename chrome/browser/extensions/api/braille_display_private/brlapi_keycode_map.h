// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BRAILLE_DISPLAY_PRIVATE_BRLAPI_KEYCODE_MAP_H_
#define CHROME_BROWSER_EXTENSIONS_API_BRAILLE_DISPLAY_PRIVATE_BRLAPI_KEYCODE_MAP_H_

#include <memory>

#include "chrome/common/extensions/api/braille_display_private.h"
#include "library_loaders/libbrlapi.h"

namespace extensions {
namespace api {
namespace braille_display_private {

// Maps a 64 bit BrlAPI keycode to a braille |KeyEvent| object.
std::unique_ptr<KeyEvent> BrlapiKeyCodeToEvent(brlapi_keyCode_t code);

}  // namespace braille_display_private
}  // namespace api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BRAILLE_DISPLAY_PRIVATE_BRLAPI_KEYCODE_MAP_H_
