// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This header is deprecated. `base::StringPiece` is now `std::string_view`.
// Use it and <string_view> instead.
//
// TODO(crbug.com/691162): Remove uses of this header.

#ifndef BASE_STRINGS_STRING_PIECE_H_
#define BASE_STRINGS_STRING_PIECE_H_

// Many files including this header rely on these being included due to IWYU
// violations. Preserve the includes for now. As code is migrated away from this
// header, we can incrementally fix the IWYU violations.
#include "base/base_export.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/cxx20_is_constant_evaluated.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/utf_ostream_operators.h"
#include "build/build_config.h"

#endif  // BASE_STRINGS_STRING_PIECE_H_
