// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_SCOPED_LIBC_TIMEZONE_OVERRIDE_H_
#define BASE_TEST_SCOPED_LIBC_TIMEZONE_OVERRIDE_H_

#include <optional>
#include <string>

namespace base::test {

// Temporarily sets up libc timezone to use the specified timezone.
// Restores on the destruction of this instance.
// Note that there's similar API ScopedRestoreDefaultTimezone to override
// ICU's timezone config. Both may need to be used together.
class ScopedLibcTimezoneOverride {
 public:
  explicit ScopedLibcTimezoneOverride(const std::string& timezone);
  ScopedLibcTimezoneOverride(const ScopedLibcTimezoneOverride&) = delete;
  ScopedLibcTimezoneOverride& operator=(const ScopedLibcTimezoneOverride&);
  ~ScopedLibcTimezoneOverride();

 private:
  std::optional<std::string> old_value_;
};

}  // namespace base::test

#endif  // BASE_TEST_SCOPED_LIBC_TIMEZONE_OVERRIDE_H_
