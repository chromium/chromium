// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_KEYBOARD_KEYBOARD_SWITCHES_H_
#define ASH_PUBLIC_CPP_KEYBOARD_KEYBOARD_SWITCHES_H_

namespace keyboard::switches {

inline constexpr char kEnableVirtualKeyboard[] = "enable-virtual-keyboard";
// TODO(crbug/1154939): Remove this const when we found a solution to
// crbug/1140667
inline constexpr char kDisableVirtualKeyboard[] = "disable-virtual-keyboard";

}  // namespace keyboard::switches

#endif  // ASH_PUBLIC_CPP_KEYBOARD_KEYBOARD_SWITCHES_H_
