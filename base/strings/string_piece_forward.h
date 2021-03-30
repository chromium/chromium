// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Forward declaration of StringPiece types from base/strings/string_piece.h

#ifndef BASE_STRINGS_STRING_PIECE_FORWARD_H_
#define BASE_STRINGS_STRING_PIECE_FORWARD_H_

namespace base {

template <typename CharT>
class BasicStringPiece;
typedef BasicStringPiece<char> StringPiece;
typedef BasicStringPiece<char16_t> StringPiece16;
typedef BasicStringPiece<wchar_t> WStringPiece;

}  // namespace base

#endif  // BASE_STRINGS_STRING_PIECE_FORWARD_H_
