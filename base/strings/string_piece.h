// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This header is deprecated. `base::StringPiece` is now `std::string_view`.
// Use it and <string_view> instead.
//
// TODO(crbug.com/40506050): Remove uses of this header.

#ifndef BASE_STRINGS_STRING_PIECE_H_
#define BASE_STRINGS_STRING_PIECE_H_

#include <string_view>

namespace base {

using StringPiece = std::string_view;
using StringPiece16 = std::u16string_view;

}  // namespace base

#endif  // BASE_STRINGS_STRING_PIECE_H_
