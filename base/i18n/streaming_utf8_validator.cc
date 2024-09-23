// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

// This implementation doesn't use ICU. The ICU macros are oriented towards
// character-at-a-time processing, whereas byte-at-a-time processing is easier
// with streaming input.

#include "base/i18n/streaming_utf8_validator.h"

#include "base/check_op.h"
#include "base/i18n/utf8_validator_tables.h"

namespace base {
namespace {

uint8_t StateTableLookup(uint8_t offset) {
  DCHECK_LT(offset, internal::kUtf8ValidatorTablesSize);
  return internal::kUtf8ValidatorTables[offset];
}

}  // namespace

StreamingUtf8Validator::State StreamingUtf8Validator::AddBytes(
    base::span<const uint8_t> data) {
  // Copy |state_| into a local variable so that the compiler doesn't have to be
  // careful of aliasing.
  uint8_t state = state_;
  for (const uint8_t ch : data) {
    if ((ch & 0x80) == 0) {
      if (state == 0)
        continue;
      state = internal::I18N_UTF8_VALIDATOR_INVALID_INDEX;
      break;
    }
    const uint8_t shift_amount = StateTableLookup(state);
    const uint8_t shifted_char = (ch & 0x7F) >> shift_amount;
    state = StateTableLookup(state + shifted_char + 1);
    // State may be INVALID here, but this code is optimised for the case of
    // valid UTF-8 and it is more efficient (by about 2%) to not attempt an
    // early loop exit unless we hit an ASCII character.
  }
  state_ = state;
  return state == 0 ? VALID_ENDPOINT
      : state == internal::I18N_UTF8_VALIDATOR_INVALID_INDEX
      ? INVALID
      : VALID_MIDPOINT;
}

void StreamingUtf8Validator::Reset() {
  state_ = 0u;
}

bool StreamingUtf8Validator::Validate(const std::string& string) {
  return StreamingUtf8Validator().AddBytes(base::as_byte_span(string)) ==
         VALID_ENDPOINT;
}

}  // namespace base
