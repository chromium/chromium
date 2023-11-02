// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_KEYBOARD_KEYBOARD_SWITCHES_H_
#define ASH_PUBLIC_CPP_KEYBOARD_KEYBOARD_SWITCHES_H_

#include "ash/public/cpp/ash_public_export.h"

namespace keyboard {
namespace switches {

ASH_PUBLIC_EXPORT extern const char kEnableVirtualKeyboard[];
// TODO(crbug/1154939): Remove this const when we found a solution to
// crbug/1140667
ASH_PUBLIC_EXPORT extern const char kDisableVirtualKeyboard[];

}  // namespace switches
}  // namespace keyboard

#endif  //  ASH_PUBLIC_CPP_KEYBOARD_KEYBOARD_SWITCHES_H_
