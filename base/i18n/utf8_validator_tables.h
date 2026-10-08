// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_UTF8_VALIDATOR_TABLES_H_
#define BASE_I18N_UTF8_VALIDATOR_TABLES_H_

#include <stddef.h>
#include <stdint.h>

#include <array>

namespace base::internal {

// The tables for all states; a list of entries of the form (right_shift,
// next_state, next_state, ....). The right_shifts are used to reduce the
// overall size of the table. The table only covers bytes in the range
// [0x80, 0xFF] to save space.
extern const std::array<uint8_t, 168> kUtf8ValidatorTables;

// The offset of the INVALID state in kUtf8ValidatorTables.
enum { I18N_UTF8_VALIDATOR_INVALID_INDEX = 129 };

}  // namespace base::internal

#endif  // BASE_I18N_UTF8_VALIDATOR_TABLES_H_
