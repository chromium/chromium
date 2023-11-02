// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ACCELERATORS_UTIL_H_
#define ASH_PUBLIC_CPP_ACCELERATORS_UTIL_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

// Returns the string of a DomKey for a given KeyboardCode. A keyboard code
// needs to be mapped to a physical key, DomCode, and then the DomCode needs
// to be mapped to a meaning or character of a DomKey based on the
// corresponding keyboard layout.
// `remap_postional_key` is an optional param, by default will attempt to
// convert any positional keys to the corresponding Domkey string.
// This function does take into account of keyboard locale.
ASH_PUBLIC_EXPORT std::u16string KeycodeToKeyString(
    ui::KeyboardCode key_code,
    bool remap_positional_key = true);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ACCELERATORS_UTIL_H_