// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_NOTIMPLEMENTED_H_
#define BASE_NOTIMPLEMENTED_H_

#include "base/check.h"
#include "base/dcheck_is_on.h"

// The NOTIMPLEMENTED() macro annotates codepaths which have not been
// implemented yet. If output spam is a serious concern,
// NOTIMPLEMENTED_LOG_ONCE() can be used.
#if DCHECK_IS_ON()
#define NOTIMPLEMENTED() \
  ::logging::CheckError::NotImplemented(__PRETTY_FUNCTION__)

// The lambda returns false the first time it is run, and true every other time.
#define NOTIMPLEMENTED_LOG_ONCE()                                \
  LOGGING_CHECK_FUNCTION_IMPL(NOTIMPLEMENTED(), [] {             \
    bool old_value = true;                                       \
    [[maybe_unused]] static const bool call_once = [](bool* b) { \
      *b = false;                                                \
      return true;                                               \
    }(&old_value);                                               \
    return old_value;                                            \
  }())

#else
#define NOTIMPLEMENTED() EAT_CHECK_STREAM_PARAMS()
#define NOTIMPLEMENTED_LOG_ONCE() EAT_CHECK_STREAM_PARAMS()
#endif

#endif  // BASE_NOTIMPLEMENTED_H_
