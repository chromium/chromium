// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MACROS_IF_H_
#define BASE_MACROS_IF_H_

#include "base/macros/concat.h"

// Given a `_Cond` that evaluates to exactly 0 or 1, this macro evaluates to
// either the `_Then` or `_Else` args. Unlike a real conditional expression,
// this does not support conditions other than `0` and `1`.
#define BASE_IF(_Cond, _Then, _Else) \
  BASE_CONCAT(BASE_INTERNAL_IF_, _Cond)(_Then, _Else)

// Implementation details: do not use directly.
#define BASE_INTERNAL_IF_1(_Then, _Else) _Then
#define BASE_INTERNAL_IF_0(_Then, _Else) _Else

#endif  // BASE_MACROS_IF_H_
