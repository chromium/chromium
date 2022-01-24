// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_LO_H_
#define ASH_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_LO_H_

namespace lo {

// The id of this IME/keyboard.
extern const char* kId;

// Whether this keyboard layout is a 102 or 101 keyboard.
extern bool kIs102;

// The key mapping definitions under various modifier states.
extern const char** kKeyMap[8];

}  // namespace lo

#endif  // ASH_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_LO_H_
