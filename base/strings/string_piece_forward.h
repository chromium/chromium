// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Forward declaration of StringPiece types from base/strings/string_piece.h.

#ifndef BASE_STRINGS_STRING_PIECE_FORWARD_H_
#define BASE_STRINGS_STRING_PIECE_FORWARD_H_

#include <string>

namespace base {

template <typename CharT, typename Traits = std::char_traits<CharT>>
class BasicStringPiece;
using StringPiece = BasicStringPiece<char>;
using StringPiece16 = BasicStringPiece<char16_t>;
using WStringPiece = BasicStringPiece<wchar_t>;

}  // namespace base

#endif  // BASE_STRINGS_STRING_PIECE_FORWARD_H_
