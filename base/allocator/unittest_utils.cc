// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

// The unittests need a this in order to link up without pulling in tons
// of other libraries

#include <config.h>
#include <stddef.h>

inline int snprintf(char* buffer, size_t count, const char* format, ...) {
  int result;
  va_list args;
  va_start(args, format);
  result = _vsnprintf(buffer, count, format, args);
  va_end(args);
  return result;
}
